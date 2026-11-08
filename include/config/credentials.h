#pragma once

// Konfigurasi Wi-Fi Default (Dikosongkan agar selalu menggunakan Wi-Fi Portal via HP)
#define WIFI_SSID           ""
#define WIFI_PASSWORD       ""

// Konfigurasi MQTT Broker
#define MQTT_BROKER         "k4366bfc.ala.asia-southeast1.emqxsl.com"
#define MQTT_PORT           8883
#define MQTT_CLIENT_ID      "esp32_small_warehouse_01"
#define MQTT_USER           "iwakdantempe"
#define MQTT_PASSWORD       "admin123"

// Topik MQTT
#define TOPIC_TELEMETRY     "warehouse/small_warehouse/telemetry"
#define TOPIC_BUZZER_CTRL   "warehouse/small_warehouse/control/buzzer"
#define TOPIC_OTA_UPDATE    "warehouse/small_warehouse/ota/update"
#define TOPIC_OTA_STATUS    "warehouse/small_warehouse/ota/status"

// Versi Firmware Aktif
#define FIRMWARE_VERSION    "1.0.0"
