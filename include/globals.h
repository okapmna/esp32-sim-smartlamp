#ifndef GLOBALS_H
#define GLOBALS_H

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include <ArduinoOTA.h>
#include <esp_system.h>

extern const char* current_version;
extern const char* device_type;

extern const int LAMP1_PIN;
extern const int LAMP2_PIN;

extern bool updateAvailable;
extern String pendingUpdateUrl;
extern String latest_version;

extern WiFiClientSecure espClient;
extern PubSubClient client;
extern Preferences pref;

extern String storage_ssid, storage_pass, storage_mqtt_host, storage_dev_id, storage_ota_server;
extern int storage_mqtt_port;
extern bool lamp1State, lamp2State;

void setupWiFi();
void setupOTA();
void callback(char* topic, byte* payload, unsigned int length);
void reconnect();
void checkOTAUpdate();
void executeOTAUpdate();

#endif
