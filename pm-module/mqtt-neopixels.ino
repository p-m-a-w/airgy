// Used with ESP32 board with built-in OLED
// Model name is WeMos LoLin32
// https://www.arduitronics.com/product/1679/esp32-wroom-development-board-with-built-in-oled-wifi-bluetooth-free-pin-header


#include <PubSubClient.h>

#include <ETH.h>
#include <WiFi.h>
#include <WiFiAP.h>
#include <WiFiClient.h>
#include <WiFiGeneric.h>
#include <WiFiMulti.h>
#include <WiFiScan.h>
#include <WiFiServer.h>
#include <WiFiSTA.h>
#include <WiFiType.h>
#include <WiFiUdp.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "Arduino.h"

#include "esp32-hal.h"

#define NR_OF_LEDS   24
#define NR_OF_ALL_BITS 24*NR_OF_LEDS

#define DEBUG
#define APP_NAME "Neo-Pixels Display"
#define SSID_DEFAULT "CatMaw"
#define PASS_DEFAULT "12345678"
#define BROKER_DEFAULT "172.20.10.3"
#define TOPIC_DEFAULT "NATTY"
#define TOPIC_SUBSCRIBE "airgy-PMstatus"
#define COMMAND_TOPIC "airgy-command"
#define PORT_DEFAULT 1883


//
// Note: This example uses Neopixel LED board, 24 LEDs chained one
//      after another, each RGB LED has its 24 bit value 
//      for color configuration (8b for each color)
//
//      Bits encoded as pulses as follows:
//
//      "0":
//         +-------+              +--
//         |       |              |
//         |       |              |
//         |       |              |
//      ---|       |--------------|
//         +       +              +
//         | 0.4us |   0.85 0us   |
//
//      "1":
//         +-------------+       +--
//         |             |       |
//         |             |       |
//         |             |       |
//         |             |       |
//      ---+             +-------+
//         |    0.8us    | 0.4us |

rmt_data_t led_data[NR_OF_ALL_BITS];
rmt_obj_t* rmt_send = NULL;

int pm25=0;
int newMQTTValue=false;

WiFiClient espClient; 
PubSubClient client(espClient);

void setup() 
{
    Serial.begin(115200);
    
    if ((rmt_send = rmtInit(2, true, RMT_MEM_64)) == NULL)
    {
        Serial.println("init sender failed\n");
      
    }

    float realTick = rmtSetTick(rmt_send, 100);
    Serial.printf("real tick set to: %fns\n", realTick);
    
    connectWifi();
    connectMQTT();

}

void connectWifi() {
    //WiFi.begin(ssid, password);

    if (WiFi.status() == WL_CONNECTED) { return; }

    Serial.println("Connecting Wifi");
    while (WiFi.status() != WL_CONNECTED) {
        WiFi.begin(SSID_DEFAULT, PASS_DEFAULT);
        delay(500);
        Serial.print(".");
    }
    
    randomSeed(micros());    
    
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
}

void connectMQTT() {

  client.setServer(BROKER_DEFAULT, PORT_DEFAULT);
  client.setCallback(callback);
  
  reconnectMQTT();

}

void reconnectMQTT() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
//      // Once connected, publish an announcement...
//      client.publish("outTopic", "hello world");
//      // ... and resubscribe
      if (client.subscribe(TOPIC_SUBSCRIBE,0)) {
        Serial.println("Subscribed to airgy-PMstatus");
      } else { 
        Serial.println("Subscribe failed");
      }
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
  
}

void callback(char* topic, byte* payload, unsigned int length) {
 
  String payload_buff;
  int i;

  Serial.print("Message arrived in topic: ");
  Serial.println(topic);

  for (int i=0;i<length;i++) {
    payload_buff = payload_buff+String((char)payload[i]);
  }

 
  Serial.print("Message:"); 
  Serial.println(payload_buff);

  pm25 = payload_buff.toInt();
  newMQTTValue = true;
 
}

void setPixelColor(int pixelIndex, uint32_t RGB) {
  int bit, bufferIndex;

  // if index out of range
  if (pixelIndex >= NR_OF_LEDS) { return; }

  // rearrange the RGB buffer.
  // the pulses seem to require GRB instead of RGB
  RGB = ((RGB & 0x00ff00)<<8) | ((RGB & 0xff0000) >> 8) | (RGB & 0x0000ff);

  bufferIndex = pixelIndex*24;

            for (bit=0; bit<24; bit++){
                if ( (RGB & (1<<(24-bit))))  {
                    led_data[bufferIndex+bit].level0 = 1;
                    led_data[bufferIndex+bit].duration0 = 8;
                    led_data[bufferIndex+bit].level1 = 0;
                    led_data[bufferIndex+bit].duration1 = 4;
                } else {
                    led_data[bufferIndex+bit].level0 = 1;
                    led_data[bufferIndex+bit].duration0 = 4;
                    led_data[bufferIndex+bit].level1 = 0;
                    led_data[bufferIndex+bit].duration1 = 8;
                }
            }
  
}

void clearPixels() {
  int i;

  for (i=0;i<NR_OF_LEDS;i++) {
    setPixelColor(i,0);
  }

}

int PixelBrightness = 24;

uint32_t getColor(int R, int G, int B) {
  return( (R<<16) | (G<<8) | B);
}

/// Has to change here ///
// Returns the RGB color given the PM2.5 level, based on US AQI
uint32_t PM_Color(int pm25Value) {

  if (pm25Value < 50) {
    return(getColor(0,PixelBrightness,0)); // green
  } else if (pm25Value < 100) {
    return(getColor(PixelBrightness,PixelBrightness,0));  // yellow
  } else if (pm25Value < 150) {
    return(getColor(PixelBrightness,PixelBrightness/4,0));  // Orange
  } else if (pm25Value < 200) {
    return(getColor(PixelBrightness,0,0));  // red
  } else if (pm25Value < 300) {
    return(getColor(PixelBrightness,0,PixelBrightness));  // purple
  } else  {
    return(getColor(0,0,PixelBrightness));  // brown
  }
  
}


// Returns the number of pixels to turn on based on the PM2.5 value
// Each pm2.5 color level has 4 pixels, but has different pm2.5 ranges

int PixelCount(int pm25Value) {
  int range[6] = {12, 23, 20, 95, 100, 250};
  int pixelDiv[6] = {3,6,5,24,25,63};
  int i=0;
  int offset;
  int pixelCount=0;

  // if over the maximum -> turn on all LEDs
  if (pm25Value >= 500) {
    return(24);
  }

  for (i=0;i<6;i++) {
    if (pm25Value >= range[i]){
      pixelCount+=4;
    } else {
      pixelCount+=(pm25Value/pixelDiv[i]) +1;
      break;
    }
    pm25Value -= range[i];
    if (pm25Value ==0) { break; }
  }

  return(pixelCount);
  
}

#define TIMEOUT 60
uint32_t color =  0x100000;  // RGB value
int led_index = 0;
int timeoutCounter = 0;

void loop() 
{
  
  int prevPixelCount=0;
  int currentPixelCount=0;
  
  
  uint32_t pixelColor;
  
  connectWifi();
  reconnectMQTT();
  client.loop();


//  for (pm25=0;pm25<510;pm25+=3) {
  if(newMQTTValue) {
    newMQTTValue = false; 
  
    pixelColor=PM_Color(pm25);
    currentPixelCount=PixelCount(pm25);

    if (prevPixelCount != currentPixelCount) {
      Serial.print("PM25=");Serial.print(pm25);Serial.print(" pixel count=");Serial.println(currentPixelCount);
      prevPixelCount = currentPixelCount;  
     
      
      for(int i=0; i<currentPixelCount; i++) {
        setPixelColor(i, pixelColor);
      }  

      pixelColor = getColor(0,0,0);
      for(int i=currentPixelCount; i<NR_OF_LEDS; i++) {
        setPixelColor(i, pixelColor);
      }
      
    // Send the data
    rmtWrite(rmt_send, led_data, NR_OF_ALL_BITS);
    
    }

  }
  delay(1000);

  // if no new value have been received within timeout -> turn off all LEDs.
  if (++timeoutCounter > TIMEOUT) {
    pm25=0;       
  }
}
