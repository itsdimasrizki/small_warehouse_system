#pragma once

// Konfigurasi Wi-Fi Default (Dikosongkan agar selalu menggunakan Wi-Fi Portal via HP)
#define WIFI_SSID           ""
#define WIFI_PASSWORD       ""

// Konfigurasi MQTT Broker
#define MQTT_BROKER         "broker.emqx.io"
#define MQTT_PORT           1883
#define MQTT_CLIENT_ID      "esp32_small_warehouse_01"
#define MQTT_USER           ""
#define MQTT_PASSWORD       ""

// Topik MQTT
#define TOPIC_TELEMETRY     "warehouse/small_warehouse/telemetry"
#define TOPIC_BUZZER_CTRL   "warehouse/small_warehouse/control/buzzer"
