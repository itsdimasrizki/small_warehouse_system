#pragma once

// ==============================================================================
// Network & MQTT Configuration
// Proyek: Small Warehouse Monitoring & Safety System
// ==============================================================================

// --- Konfigurasi Wi-Fi ---
#define WIFI_SSID           "YOUR_WIFI_SSID"
#define WIFI_PASSWORD       "YOUR_WIFI_PASSWORD"

// --- Konfigurasi MQTT Broker ---
#define MQTT_BROKER         "broker.emqx.io"    // Atau broker lokal seperti "192.168.1.100"
#define MQTT_PORT           1883
#define MQTT_CLIENT_ID      "esp32_small_warehouse_01"
#define MQTT_USER           ""
#define MQTT_PASSWORD       ""

// --- Topik MQTT ---
#define TOPIC_TELEMETRY     "warehouse/small_warehouse/telemetry"
#define TOPIC_BUZZER_CTRL   "warehouse/small_warehouse/control/buzzer"
