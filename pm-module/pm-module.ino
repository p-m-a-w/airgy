#include <WiFi.h>
#include <MQTT.h>
#include <esp_wifi.h>

#define DEBUG
#define APP_NAME "pm-module"
#define SSID_DEFAULT "@JumboPlusIoT"
#define PASS_DEFAULT "kvss8l7p"
#define USERNAME_DEFAULT "demo215"
#define PASSWORD_DEFAULT "cpe2152023*"
#define BROKER_DEFAULT "10.10.182.144"
#define TOPIC_DEFAULT "default-pm-module"
#define PORT_DEFAULT 1883

#define LED 12

const char mqtt_client_id[] = "pm-module-demo";
String ssid;
String pass;
String mqtt_broker;
String mqtt_topic;
int mqtt_port;

WiFiClient net;
MQTTClient client;

unsigned long lastMillis = 0;

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
}

void loop() {
  client.loop();
  delay(10);  // <- fixes some issues with WiFi stability

  if (!client.connected()) {
    connect();
  }
}