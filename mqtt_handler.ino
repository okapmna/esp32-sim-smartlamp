void callback(char* topic, byte* payload, unsigned int length) {
  String message = "";
  for (int i = 0; i < length; i++) message += (char)payload[i];
  
  if (message == "1ON") lamp1State = true;
  else if (message == "1OFF") lamp1State = false;
  else if (message == "2ON") lamp2State = true;
  else if (message == "2OFF") lamp2State = false;
  else if (message == "dev_update") {
    Serial.println("[MQTT] Perintah update diterima. Mengecek server...");
    checkOTAUpdate();
    return;
  }

  digitalWrite(LAMP1_PIN, lamp1State ? HIGH : LOW);
  digitalWrite(LAMP2_PIN, lamp2State ? HIGH : LOW);
  pref.putBool("l1", lamp1State);
  pref.putBool("l2", lamp2State);
}

void reconnect() {
  while (!client.connected()) {
    String clientId = "Lamp-" + storage_dev_id;
    if (client.connect(clientId.c_str())) {
      client.subscribe(("lamp/" + storage_dev_id + "/con").c_str());
    } else {
      delay(5000);
    }
  }
}
