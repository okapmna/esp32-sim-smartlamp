#ifndef SECRET_H
#define SECRET_H

// WiFi Credentials
const char* ssid = "NYUDIS";
const char* password = "87654321C";

// MQTT Config Credentials
const char* mqtt_server = "broker.hivemq.com";
const int mqtt_port = 8883;
const char* mqtt_user = "";
const char* mqtt_pass = "";

// Device ID (xx)
const char* device_id = "15";

// MQTT Topics
// mqtt_topic_data = "lamp/14/data"
// mqtt_topic_con  = "lamp/14/con"
const char* mqtt_topic_data = "lamp/14/data";
const char* mqtt_topic_con  = "lamp/14/con";

// OTA Config
const char* ota_server = "otaup.okamna.my.id";

#endif
