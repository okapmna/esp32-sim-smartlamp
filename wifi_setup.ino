void setupWiFi() {
  Serial.printf("\nConnecting WiFi: %s\n", storage_ssid.c_str());
  WiFi.begin(storage_ssid.c_str(), storage_pass.c_str());
  int counter = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500); Serial.print(".");
    if (counter++ > 40) ESP.restart();
  }
  Serial.println("\nWiFi Connected!");
}

void setupOTA() {
  ArduinoOTA.setHostname(("SmartLamp-" + storage_dev_id).c_str());
  ArduinoOTA.begin();
}
