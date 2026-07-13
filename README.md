# ESP32 Smart Lamp

Firmware ESP32 untuk smart lamp dengan kendali MQTT dan update OTA.

## Fitur

- Kontrol 2 lampu via MQTT
- Update OTA dari server
- Konfigurasi tersimpan di NVS (non-volatile storage)
- Mode debug via Serial (9600 baud)

## Requirements

- PlatformIO
- ESP32 dev board

## Setup

```bash
cp secret.h.example include/secret.h
# Isi kredensial WiFi, MQTT, dan OTA server di include/secret.h
```

## Build & Upload

```bash
pio run -e esp32dev           # build
pio run -e esp32dev --target upload  # build + upload
```
