#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include "secret.h"

// CONFIG
const char* current_version = "1.0.5";
const char* device_type     = "esp32-smartlamp";
const char* ota_server      = "otaup.okamna.my.id";

const int LAMP1_PIN = 26;
const int LAMP2_PIN = 27;

// OTA DEBUG VARIABLES
bool updateAvailable = false;
String pendingUpdateUrl = "";
unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 60000; 

// GLOBAL OBJECTS   
WiFiClientSecure espClient;
PubSubClient client(espClient);
Preferences pref;

// SHARED VARIABLES 
String storage_ssid, storage_pass, storage_mqtt_host, storage_dev_id;
int storage_mqtt_port;
bool lamp1State = false, lamp2State = false;

// FUNCTION PROTOTYPES 
void setupWiFi();
void setupOTA();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void checkOTAUpdate();
void executeOTAUpdate();

void setup() {
  Serial.begin(9600);
  pinMode(LAMP1_PIN, OUTPUT);
  pinMode(LAMP2_PIN, OUTPUT);

  pref.begin("config", false); 
  if (!pref.isKey("ssid")) {
    pref.putString("ssid", ssid);
    pref.putString("pass", password);
    pref.putString("mqtt_host", mqtt_server);
    pref.putInt("mqtt_port", mqtt_port);
    pref.putString("dev_id", device_id);
  }

  storage_ssid = pref.getString("ssid");
  storage_pass = pref.getString("pass");
  storage_mqtt_host = pref.getString("mqtt_host");
  storage_mqtt_port = pref.getInt("mqtt_port");
  storage_dev_id = pref.getString("dev_id");
  
  lamp1State = pref.getBool("l1", false);
  lamp2State = pref.getBool("l2", false);
  digitalWrite(LAMP1_PIN, lamp1State ? HIGH : LOW);
  digitalWrite(LAMP2_PIN, lamp2State ? HIGH : LOW);

  setupWiFi();
  setupOTA(); 

  espClient.setInsecure();
  client.setServer(storage_mqtt_host.c_str(), storage_mqtt_port);
  client.setCallback(callback);

  Serial.println("\n--- DEBUG MODE ACTIVE ---");
  Serial.println("Ketik 'check' untuk cek update manual");
  Serial.println("Ketik 'update' untuk eksekusi jika update tersedia");
  Serial.println("--------------------------");
}

void loop() {
  ArduinoOTA.handle();
  if (!client.connected()) reconnect();
  client.loop();

  if (millis() - lastCheckTime > checkInterval) {
    lastCheckTime = millis();
    checkOTAUpdate();
  }

  if (Serial.available() > 0) {
    String input = Serial.readStringUntil('\n');
    input.trim();
    
    if (input.equalsIgnoreCase("check")) {
      checkOTAUpdate();
    } 
    else if (input.equalsIgnoreCase("update")) {
      if (updateAvailable) {
        executeOTAUpdate();
      } else {
        Serial.println("[DEBUG] Tidak ada update yang siap. Ketik 'check' dulu.");
      }
    }
  }
}
