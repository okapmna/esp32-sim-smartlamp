void checkOTAUpdate() {
  Serial.println("\n[OTA] Memeriksa versi terbaru ke server...");
  
  String serverUrl = "https://" + storage_ota_server + "/api/ota/check?version=" + String(current_version) + "&device=" + String(device_type);

  WiFiClientSecure otaClient;
  otaClient.setInsecure();
  
  HTTPClient http;
  http.begin(otaClient, serverUrl);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    JsonDocument doc;
    deserializeJson(doc, payload);

    if (doc["status"] == "update_available") {
      updateAvailable = true;
      pendingUpdateUrl = "https://" + storage_ota_server + "/api" + doc["url"].as<String>();
      latest_version = doc["version"].as<String>();
      
      Serial.println("*************************************************");
      Serial.print("UPDATE TERSEDIA: "); Serial.println(doc["version"].as<String>());
      Serial.println("Ketik 'update' di Serial Monitor untuk mengunduh.");
      Serial.println("*************************************************");
    } else {
      updateAvailable = false;
      Serial.println("[OTA] Firmware sudah versi terbaru.");
    }
  } else {
    Serial.printf("[OTA] Gagal konek ke server. Kode: %d\n", httpCode);
  }
  http.end();
}

void executeOTAUpdate() {
  Serial.println("\n[OTA] Memulai proses download & update...");
  
  WiFiClientSecure otaClient;
  otaClient.setInsecure();

  // Update Progress Indicator
  httpUpdate.onProgress([](int cur, int total) {
    Serial.printf("[OTA] Progress: %d%%\r", (cur * 100) / total);
  });

  t_httpUpdate_return ret = httpUpdate.update(otaClient, pendingUpdateUrl);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("\n[OTA] Update Gagal (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      updateAvailable = false; 
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("\n[OTA] Tidak ada update.");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("\n[OTA] Update Berhasil! Rebooting...");
      break;
  }
}
