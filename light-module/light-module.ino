#include <WiFi.h>
#include <MQTT.h>
#include <Preferences.h>
#include <movingAvg.h>
#include <esp_wifi.h>
#include "FFT.h"

#define DEBUG
#define DEFAULT_PARAM
#define APP_NAME "airgy"
// #define SSID_DEFAULT "@JumboPlusIoT"
// #define PASS_DEFAULT "kvss8l7p"
#define SSID_DEFAULT "CatMaw"
#define PASS_DEFAULT "123456789"
#define BROKER_DEFAULT "172.20.10.3"
#define TOPIC_DEFAULT "default-light-module"
#define COMMAND_TOPIC "airgy-command"
#define PORT_DEFAULT 1883

#define SSID_KEY "SSID"
#define PASS_KEY "PASS"
#define BROKER_KEY "BROKER"
#define TOPIC_KEY "TOPIC"
#define PORT_KEY "PORT"

#define SSID_CMD "wifi_ssid"
#define PASS_CMD "wifi_pass"
#define BROKER_CMD "mqtt_host"
#define PORT_CMD "mqtt_port"
#define TOPIC_CMD "mqtt_topic"
#define REBOOT_CMD "reboot"

const int photoresistorPin = 36;
const char mqtt_client_id[] = "airgy-demo";
String ssid;
String pass;
String mqtt_broker;
String mqtt_topic;
int mqtt_port;
int counter = 0;
int sensorData = 0;

WiFiClient net;
MQTTClient client;
Preferences prefs;

unsigned long lastMillis = 0;

void connect() {
#ifdef DEBUG
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.print("\nconnecting...");
  while (!client.connect(mqtt_client_id)) {
    Serial.print(".");
    delay(1000);
  }
  Serial.println("\nconnected!");
#else
  while (WiFi.status() != WL_CONNECTED || !client.connect(mqtt_client_id)) {
    __asm__("nop\n\t");
  }
#endif
  client.subscribe(COMMAND_TOPIC);
}
void processCommand(String &topic, String &payload) {
  if (topic != COMMAND_TOPIC)
    return;
  String key, data;
  if (payload.startsWith(REBOOT_CMD)) {
#ifdef DEBUG
    Serial.println("Reboot called");
#endif
    ESP.restart();
  } else if (payload.startsWith(SSID_CMD)) {
    key = SSID_KEY;
    data = payload.substring(strlen(SSID_CMD) + 1);
#ifdef DEBUG
    Serial.println("SSID set to " + data);
#endif
  } else if (payload.startsWith(PASS_CMD)) {
    key = PASS_KEY;
    data = payload.substring(strlen(PASS_CMD) + 1);
#ifdef DEBUG
    Serial.println("Passphrase set to " + data);
#endif
  }else if (payload.startsWith(TOPIC_CMD)) {
    key = TOPIC_CMD;
    data = payload.substring(strlen(TOPIC_CMD) + 1);
#ifdef DEBUG
    Serial.println("MQTT Broker set to " + data);
#endif
  }else if (payload.startsWith(PORT_CMD)) {
    data = payload.substring(strlen(PORT_CMD) + 1);
#ifdef DEBUG
    Serial.println("MQTT Port set to " + data);
#endif
    prefs.begin(APP_NAME, false);
    prefs.putInt(PORT_KEY, data.toInt());
    prefs.end();
    return;
  }else if (payload.startsWith(TOPIC_CMD)) {
	  key = TOPIC_KEY;
    data = payload.substring(strlen(TOPIC_CMD) + 1);
#ifdef DEBUG
    Serial.println("MQTT Topic set to " + data);
#endif
  }
  prefs.begin(APP_NAME, false);
  prefs.putString(key.c_str(), data.c_str());
  prefs.end();
}

void messageReceived(String &topic, String &payload) {
#ifdef DEBUG
  Serial.println("incoming: " + topic + " - " + payload);
#endif
  processCommand(topic, payload);
}

hw_timer_t *samplingTimer = NULL;
#define SAMPLING_RATE 64
#define SAMPLE_DURATION 4 // * 100 ms
#define TARGET_FREQ 28
#define FFT_N SAMPLING_RATE * SAMPLE_DURATION
bool dataReady = false;

int i = 0;
int sample[FFT_N] = {};
int fftSample[FFT_N] = {};
void IRAM_ATTR onSample() {
  if (dataReady) return;
  if (i >= FFT_N) {
    i = 0;
    dataReady = true;
  }
  sample[i] = analogRead(photoresistorPin);
  i++;
}

float fftInput[FFT_N];
float fftOutput[FFT_N];
fft_config_t *fftPlan;

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  Serial.print("esp32 mac: ");
  Serial.println(WiFi.macAddress());
  uint8_t debug_mac[] = {0x32, 0xAE, 0xA4, 0x07, 0x0D, 0x66};
  esp_base_mac_addr_set(debug_mac);
  Serial.print("debug esp32 mac: ");
  Serial.println(WiFi.macAddress());
#endif
#ifdef DEFAULT_PARAM
  ssid = String(SSID_DEFAULT);
  pass = String(PASS_DEFAULT);
  mqtt_broker = String(BROKER_DEFAULT);
  mqtt_topic = String(TOPIC_DEFAULT);
  mqtt_port = PORT_DEFAULT;
#else
  prefs.begin(APP_NAME, false);
  ssid = prefs.getString(SSID_KEY, SSID_DEFAULT);
  pass = prefs.getString(PASS_KEY, PASS_DEFAULT);
  mqtt_broker = prefs.getString(BROKER_KEY, BROKER_DEFAULT);
  mqtt_topic = prefs.getString(TOPIC_KEY, TOPIC_DEFAULT);
  mqtt_port = prefs.getInt(PORT_KEY, PORT_DEFAULT);
  prefs.end();
#endif

  samplingTimer = timerBegin(0, 80, true);
  timerAttachInterrupt(samplingTimer, &onSample, true);
  timerAlarmWrite(samplingTimer, 250000/SAMPLING_RATE, true);
  timerAlarmEnable(samplingTimer);

  fftPlan = fft_init(
    FFT_N,
    FFT_REAL, 
    FFT_FORWARD, 
    fftInput, 
    fftOutput
  );

  WiFi.begin(ssid.c_str(), pass.c_str());
  client.begin(mqtt_broker.c_str(), mqtt_port, net);
  client.onMessage(messageReceived);

  connect();
}

void loop() {
  client.loop();
  delay(10);  // <- fixes some issues with WiFi stability

  if (!client.connected()) {
    connect();
  }

  if (dataReady) {
    for (int k = 0; k < FFT_N; k++)
      fftPlan->input[k] = (float) sample[k];
    fft_execute(fftPlan);
    int k = TARGET_FREQ * SAMPLE_DURATION;
    float amplitude = sqrt(pow(fftPlan->output[2*k],2) + pow(fftPlan->output[2*k+1],2));
    char buffer[8];
    client.publish(mqtt_topic.c_str(), itoa(amplitude, buffer, 10));
    dataReady = false;
  }
}