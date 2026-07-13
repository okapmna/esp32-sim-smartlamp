#include "globals.h"
#include <Update.h>
#include <mbedtls/sha256.h>

// ---------------------------------------------------------------------------
// Helper: byte array → lowercase hex string
// ---------------------------------------------------------------------------
static String bytesToHex(const uint8_t* data, size_t len) {
  String hex = "";
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x10) hex += "0";
    hex += String(data[i], HEX);
  }
  return hex;
}

// ---------------------------------------------------------------------------
// Validasi format hex SHA-256 (64 karakter, semua hex digit)
// ---------------------------------------------------------------------------
static bool isValidSHA256Hex(const String& s) {
  if (s.length() != 64) return false;
  for (size_t i = 0; i < s.length(); i++) {
    if (!isxdigit((unsigned char)s[i])) return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// checkOTAUpdate()
//   GET /api/ota/check?version=<ver>&device=<type>
//   Response: {"status":"update_available","version":"2.0.0",
//              "url":"/api/ota/download/firmware-esp32-v2.0.0.bin",
//              "checksum":"a1b2c3..."}
// ---------------------------------------------------------------------------
void checkOTAUpdate() {
  Serial.println("\n[OTA] Memeriksa versi terbaru ke server...");

  String serverUrl = "https://" + storage_ota_server
                   + "/api/ota/check?version=" + String(current_version)
                   + "&device=" + String(device_type);

  WiFiClientSecure otaClient;
  otaClient.setInsecure();

  HTTPClient http;
  http.begin(otaClient, serverUrl);
  http.setTimeout(10000);
  int httpCode = http.GET();

  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    Serial.print("[OTA] Response: "); Serial.println(payload);

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);

    if (err) {
      Serial.print("[OTA] JSON parse error: "); Serial.println(err.c_str());
      http.end();
      return;
    }

    if (doc["status"] == "update_available") {
      // URL dari server sudah "/api/ota/download/...", prefix HANYA "https://<host>"
      // Tidak perlu tambah "/api" lagi agar tidak menjadi "/api/api/..."
      String relativeUrl = doc["url"].as<String>();
      pendingUpdateUrl      = "https://" + storage_ota_server + relativeUrl;
      pendingUpdateChecksum = doc["checksum"].as<String>();
      latest_version        = doc["version"].as<String>();

      updateAvailable = true;

      Serial.println("*************************************************");
      Serial.print("UPDATE TERSEDIA  : "); Serial.println(latest_version);
      Serial.print("Download URL     : "); Serial.println(pendingUpdateUrl);
      Serial.print("Checksum SHA-256 : "); Serial.println(pendingUpdateChecksum);
      Serial.println("Ketik 'update' di Serial Monitor untuk mengunduh.");
      Serial.println("*************************************************");

      if (!isValidSHA256Hex(pendingUpdateChecksum)) {
        Serial.println("[OTA] PERINGATAN: Checksum dari server tidak valid (bukan 64-char hex)!");
        Serial.println("[OTA] Update tidak akan dilanjutkan sampai checksum valid.");
        updateAvailable = false;
      }

    } else if (doc["status"] == "up_to_date") {
      updateAvailable = false;
      Serial.println("[OTA] Firmware sudah versi terbaru.");
    } else {
      updateAvailable = false;
      Serial.print("[OTA] Status tidak dikenal: ");
      Serial.println(doc["status"].as<String>());
    }

  } else {
    Serial.printf("[OTA] Gagal konek ke server. HTTP code: %d\n", httpCode);
    if (httpCode > 0) {
      Serial.print("[OTA] Body: "); Serial.println(http.getString());
    }
  }
  http.end();
}

// ---------------------------------------------------------------------------
// executeOTAUpdate()
//   Download firmware via HTTPClient (streaming) + verifikasi SHA-256
// ---------------------------------------------------------------------------
void executeOTAUpdate() {
  if (!updateAvailable || pendingUpdateUrl.isEmpty()) {
    Serial.println("[OTA] Tidak ada update yang tersedia. Jalankan 'check' dulu.");
    return;
  }

  if (!isValidSHA256Hex(pendingUpdateChecksum)) {
    Serial.println("[OTA] Error: Checksum tidak valid. Update dibatalkan.");
    return;
  }

  Serial.println("\n[OTA] Memulai download & update dengan verifikasi SHA-256...");
  Serial.print("[OTA] URL: "); Serial.println(pendingUpdateUrl);

  WiFiClientSecure dlClient;
  dlClient.setInsecure();

  HTTPClient http;
  http.begin(dlClient, pendingUpdateUrl);
  http.setTimeout(30000);

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    Serial.printf("[OTA] Gagal download firmware. HTTP code: %d\n", httpCode);
    http.end();
    return;
  }

  int contentLength = http.getSize();  // -1 jika chunked / tidak ada header
  Serial.printf("[OTA] Content-Length: %d bytes\n", contentLength);

  if (contentLength == 0) {
    Serial.println("[OTA] Content-Length = 0. Update dibatalkan.");
    http.end();
    return;
  }

  if (!Update.begin(contentLength > 0 ? (size_t)contentLength : UPDATE_SIZE_UNKNOWN)) {
    Serial.printf("[OTA] Gagal memulai OTA: %s\n", Update.errorString());
    http.end();
    return;
  }

  // Inisialisasi SHA-256
  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);  // 0 = SHA-256 (bukan SHA-224)

  WiFiClient* stream   = http.getStreamPtr();
  uint8_t     buffer[1024];
  int         totalWritten = 0;
  unsigned long lastReport = 0;

  Serial.println("[OTA] Streaming firmware ke flash...");

  while (http.connected() && (contentLength < 0 || totalWritten < contentLength)) {
    int available = stream->available();
    if (available == 0) {
      delay(1);
      continue;
    }

    int toRead = available < (int)sizeof(buffer) ? available : (int)sizeof(buffer);
    int len    = stream->readBytes(buffer, toRead);
    if (len <= 0) break;

    size_t written = Update.write(buffer, len);
    if (written != (size_t)len) {
      Serial.printf("[OTA] Gagal tulis ke flash: %s\n", Update.errorString());
      Update.abort();
      mbedtls_sha256_free(&sha);
      http.end();
      return;
    }

    mbedtls_sha256_update(&sha, buffer, len);
    totalWritten += len;

    unsigned long now = millis();
    if (now - lastReport > 2000) {
      if (contentLength > 0) {
        int pct = (totalWritten * 100) / contentLength;
        Serial.printf("[OTA] Progress: %d%% (%d/%d bytes)\n", pct, totalWritten, contentLength);
      } else {
        Serial.printf("[OTA] Diunduh: %d bytes\n", totalWritten);
      }
      lastReport = now;
    }
  }

  http.end();
  Serial.printf("[OTA] Download selesai: %d bytes diterima.\n", totalWritten);

  // Finalisasi SHA-256
  uint8_t hash[32];
  mbedtls_sha256_finish(&sha, hash);
  mbedtls_sha256_free(&sha);

  String computedChecksum = bytesToHex(hash, 32);
  computedChecksum.toLowerCase();
  String expectedChecksum = pendingUpdateChecksum;
  expectedChecksum.toLowerCase();

  Serial.print("[OTA] SHA-256 computed : "); Serial.println(computedChecksum);
  Serial.print("[OTA] SHA-256 expected : "); Serial.println(expectedChecksum);

  if (computedChecksum != expectedChecksum) {
    Serial.println("[OTA] *** Checksum MISMATCH! Update dibatalkan. ***");
    Update.abort();
    updateAvailable       = false;
    pendingUpdateUrl      = "";
    pendingUpdateChecksum = "";

    // Beritahu backend via MQTT bahwa OTA gagal
    String topicData   = "lamp/" + storage_dev_id + "/data";
    String failPayload = "{\"event\":\"ota_failed\",\"reason\":\"checksum_mismatch\","
                         "\"version\":\"" + latest_version + "\"}";
    client.publish(topicData.c_str(), failPayload.c_str());
    return;
  }

  Serial.println("[OTA] Checksum valid! Memfinalisasi...");

  if (!Update.end(true)) {
    Serial.printf("[OTA] Gagal finalisasi update: %s\n", Update.errorString());

    // Beritahu backend via MQTT
    String topicData   = "lamp/" + storage_dev_id + "/data";
    String failPayload = "{\"event\":\"ota_failed\",\"reason\":\"finalize_error\","
                         "\"version\":\"" + latest_version + "\"}";
    client.publish(topicData.c_str(), failPayload.c_str());
    return;
  }

  // Beritahu backend bahwa OTA berhasil SEBELUM restart
  String topicData = "lamp/" + storage_dev_id + "/data";
  String okPayload = "{\"event\":\"ota_success\",\"version\":\"" + latest_version + "\"}";
  client.publish(topicData.c_str(), okPayload.c_str());
  client.loop();  // flush publish buffer

  Serial.println("[OTA] *** Update BERHASIL! Rebooting dalam 3 detik... ***");
  delay(3000);
  ESP.restart();
}
