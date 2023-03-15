#include <WiFi.h>
#include <MQTT.h>
#include <esp_wifi.h>
#include <Adafruit_NeoPixel.h>

#define DEBUG
#define APP_NAME "pm-module"
#define SSID_DEFAULT "@JumboPlusIoT"
#define PASS_DEFAULT "kvss8l7p"
#define USERNAME_DEFAULT "demo215"
#define PASSWORD_DEFAULT "cpe2152023*"
#define BROKER_DEFAULT "10.10.182.144"
#define TOPIC_DEFAULT "default-pm-module"
#define PORT_DEFAULT 1883

#define MAX_PM25 270
#define MIN_PM25 0
#define DELAY 30
#define LED_PIN 12
#define LED_COUNT 24


const char mqtt_client_id[] = "pm-module-demo";
String ssid;
String pass;
String mqtt_broker;
String mqtt_topic;
int mqtt_port;
int pm25 = 0;

WiFiClient net;
MQTTClient client;

unsigned long lastMillis = 0;

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

void connect() {
#ifdef DEBUG
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.print("\nconnecting...");
  while (!client.connect(mqtt_client_id, USERNAME_DEFAULT, PASSWORD_DEFAULT)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nconnected!");
#else
  while (WiFi.status() != WL_CONNECTED || !client.connect(mqtt_client_id)) {
    __asm__("nop\n\t");
  }
#endif
  client.subscribe(TOPIC_DEFAULT);
}

void messageReceived(String &topic, String &payload) {
#ifdef DEBUG
  Serial.println("incoming: " + topic + " - " + payload);
#endif
  pm25 = payload.toInt();
}

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  Serial.print("esp32 mac: ");
  Serial.println(WiFi.macAddress());
#endif

  WiFi.begin(SSID_DEFAULT, PASS_DEFAULT);
  client.begin(BROKER_DEFAULT, PORT_DEFAULT, net);
  client.onMessage(messageReceived);

  connect();

  strip.begin();            // INITIALIZE NeoPixel strip object (REQUIRED)
  strip.show();             // Turn OFF all pixels ASAP
  strip.setBrightness(50);  // Set BRIGHTNESS to about 1/5 (max = 255)
}

// PM25 value based on US AQI
uint32_t getColor(int pm25Value) {
  if (pm25Value < 50) {
    return (strip.Color(0, 255, 0));  // green
  } else if (pm25Value < 100) {
    return (strip.Color(255, 255, 0));  // yellow
  } else if (pm25Value < 150) {
    return (strip.Color(255, 64, 0));  // Orange
  } else if (pm25Value < 200) {
    return (strip.Color(255, 0, 0));  // red
  } else if (pm25Value < 300) {
    return (strip.Color(255, 0, 255));  // purple
  } else {
    return (strip.Color(255,248,220));  // blue                                          
  }
}

void setPMStatus() {// set LED depend on PM2.5 value
  int p = pm25;
  uint32_t color = getColor(pm25);
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, p > 0 ? color : 0);
    strip.show();
    delay(DELAY);
    p -= (MAX_PM25 - MIN_PM25) / LED_COUNT;
  }
}

void loop() {
  client.loop();
  delay(10);  // <- fixes some issues with WiFi stability

  if (!client.connected()) {
    connect();
  }
  setPMStatus();
}
