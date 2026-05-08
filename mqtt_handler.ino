void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) message += (char)payload[i];
  message.trim(); // Clean space & newline

  Serial.print("[MQTT] Pesan masuk di topik [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);
  
  if (message == "1ON") lamp1State = true;
  else if (message == "1OFF") lamp1State = false;
  else if (message == "2ON") lamp2State = true;
  else if (message == "2OFF") lamp2State = false;
  else if (message == "dev_getupdate") {
    Serial.println("[MQTT] Eksekusi dev_getupdate...");
    checkOTAUpdate();
    String topicData = "lamp/" + storage_dev_id + "/data";
    String payloadStr;
    
    if (updateAvailable) {
      payloadStr = "{\"update\":true,\"version_new\":\"" + latest_version + "\",\"version_current\":\"" + String(current_version) + "\"}";
    } else {
      payloadStr = "{\"update\":false}";
    }
    Serial.print("[MQTT] Mengirim balasan ke ");
    Serial.println(topicData);
    if (client.publish(topicData.c_str(), payloadStr.c_str())) {
      Serial.println("[MQTT] Publish BERHASIL");
    } else {
      Serial.println("[MQTT] Publish GAGAL (Mungkin payload terlalu besar atau koneksi terputus)");
    }
    return;
  }
  else if (message == "dev_getupgrade") {
    Serial.println("[MQTT] Perintah dev_getupgrade diterima. Memulai upgrade...");
    if (updateAvailable) {
      executeOTAUpdate();
    } else {
      Serial.println("[MQTT] Update belum dicek atau tidak tersedia.");
    }
    return;
  }

  digitalWrite(LAMP1_PIN, lamp1State ? HIGH : LOW);
  digitalWrite(LAMP2_PIN, lamp2State ? HIGH : LOW);
  pref.putBool("l1", lamp1State);
  pref.putBool("l2", lamp2State);
}

unsigned long lastReconnectAttempt = 0;

void reconnect() {
  if (client.connected()) return;

  unsigned long now = millis();
  // Coba reconnect setiap 3 detik (non-blocking)
  if (now - lastReconnectAttempt < 3000) return;
  lastReconnectAttempt = now;

  Serial.println("[MQTT] Mencoba reconnect...");
  String clientId = "Lamp-" + storage_dev_id;
  if (client.connect(clientId.c_str())) {
    Serial.println("[MQTT] Terhubung kembali!");
    client.subscribe(("lamp/" + storage_dev_id + "/con").c_str());
  } else {
    Serial.print("[MQTT] Gagal, rc=");
    Serial.println(client.state());
  }
}
