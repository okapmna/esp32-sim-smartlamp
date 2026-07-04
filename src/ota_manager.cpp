#include "globals.h"
#include <Update.h>
#include <mbedtls/sha256.h>

static String bytesToHex(const uint8_t* data, size_t len) {
  String hex = "";
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10) hex += "0";
    hex += String(data[i], HEX);
  }
  return hex;
}

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
      pendingUpdateChecksum = doc["checksum"].as<String>();
      latest_version = doc["version"].as<String>();

      Serial.println("*************************************************");
      Serial.print("UPDATE TERSEDIA: "); Serial.println(doc["version"].as<String>());
      Serial.print("Checksum SHA-256: "); Serial.println(pendingUpdateChecksum);
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
  if (!updateAvailable || pendingUpdateUrl.isEmpty()) {
    Serial.println("[OTA] Tidak ada update yang tersedia. Jalankan 'check' dulu.");
    return;
  }

  if (pendingUpdateChecksum.isEmpty()) {
    Serial.println("[OTA] Error: Tidak ada checksum dari server. Update dibatalkan.");
    return;
  }

  Serial.println("\n[OTA] Memulai download & update dengan verifikasi SHA-256...");
  Serial.print("[OTA] URL: "); Serial.println(pendingUpdateUrl);

  WiFiClientSecure client;
  client.setInsecure();

  if (!client.connect(storage_ota_server.c_str(), 443)) {
    Serial.println("[OTA] Gagal konek ke server OTA.");
    return;
  }

  String path = pendingUpdateUrl.substring(pendingUpdateUrl.indexOf('/', 8));
  if (path.isEmpty()) path = "/";

  client.print("GET " + path + " HTTP/1.1\r\n");
  client.print("Host: " + storage_ota_server + "\r\n");
  client.print("Connection: close\r\n");
  client.print("\r\n");

  unsigned long timeout = millis();
  while (client.available() == 0) {
    if (millis() - timeout > 10000) {
      Serial.println("[OTA] Timeout saat menunggu response dari server.");
      client.stop();
      return;
    }
  }

  String headerLine;
  int contentLength = -1;
  bool httpStatusOk = false;

  while (client.connected()) {
    headerLine = client.readStringUntil('\n');
    headerLine.trim();
    if (headerLine.isEmpty()) break;

    if (headerLine.startsWith("HTTP/1.")) {
      httpStatusOk = headerLine.indexOf("200") >= 0;
    } else if (headerLine.startsWith("Content-Length:") || headerLine.startsWith("content-length:")) {
      contentLength = headerLine.substring(headerLine.indexOf(':') + 1).toInt();
    }
  }

  if (!httpStatusOk || contentLength <= 0) {
    Serial.printf("[OTA] Response server tidak valid. Content-Length: %d\n", contentLength);
    client.stop();
    return;
  }

  if (!Update.begin(contentLength)) {
    Serial.printf("[OTA] Gagal memulai OTA. Ukuran: %d, Error: %s\n", contentLength, Update.errorString());
    client.stop();
    return;
  }

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);

  uint8_t buffer[1024];
  int bytesRead = 0;
  unsigned long lastReport = 0;

  Serial.printf("[OTA] Mendownload %d bytes...\n", contentLength);

  while (bytesRead < contentLength && client.connected()) {
    int len = client.read(buffer, sizeof(buffer));
    if (len <= 0) continue;

    if (Update.write(buffer, len) != len) {
      Serial.printf("[OTA] Gagal menulis ke flash. Error: %s\n", Update.errorString());
      break;
    }

    mbedtls_sha256_update(&sha, buffer, len);
    bytesRead += len;

    unsigned long now = millis();
    if (now - lastReport > 2000) {
      int pct = (bytesRead * 100) / contentLength;
      Serial.printf("[OTA] Progress: %d%% (%d/%d bytes)\r", pct, bytesRead, contentLength);
      lastReport = now;
    }
  }

  Serial.printf("\n[OTA] Download selesai: %d bytes diterima.\n", bytesRead);

  uint8_t hash[32];
  mbedtls_sha256_finish(&sha, hash);
  mbedtls_sha256_free(&sha);

  String computedChecksum = bytesToHex(hash, 32);
  computedChecksum.toLowerCase();
  String expectedChecksum = pendingUpdateChecksum;
  expectedChecksum.toLowerCase();

  Serial.print("[OTA] SHA-256 computed: "); Serial.println(computedChecksum);
  Serial.print("[OTA] SHA-256 expected: "); Serial.println(expectedChecksum);

  if (computedChecksum != expectedChecksum) {
    Serial.println("[OTA] Checksum MISMATCH! Update dibatalkan.");
    Update.abort();
    updateAvailable = false;
    pendingUpdateUrl = "";
    pendingUpdateChecksum = "";
    client.stop();
    return;
  }

  Serial.println("[OTA] Checksum valid!");

  if (!Update.end(true)) {
    Serial.printf("[OTA] Gagal finalisasi update. Error: %s\n", Update.errorString());
    client.stop();
    return;
  }

  Serial.println("[OTA] Update BERHASIL! Rebooting dalam 3 detik...");
  client.stop();
  delay(3000);
  ESP.restart();
}
