#pragma once

// ==============================================================================
// Thresholds & Timing Parameters
// Proyek: Small Warehouse Monitoring & Safety System
// ==============================================================================

// --- Batas Deteksi Kapasitas Rak ---
#define RACK_DETECT_THRESHOLD_CM    35.0f   // Ambang batas jarak (cm): jika < 35 cm dianggap terisi
#define ULTRASONIC_MAX_DISTANCE_CM  200     // Jarak maksimum pembacaan NewPing (cm)

// --- Batas Bahaya Lingkungan & Keselamatan ---
#define TEMP_HAZARD_THRESHOLD_C     38.0f   // Ambang batas suhu (°C) memicu alarm bahaya
#define MQ2_HAZARD_THRESHOLD_ADC    1800    // Ambang batas ADC sensor asap MQ2 (0 - 4095)

// --- Interval Penjadwalan Non-Blocking (millis) ---
#define INTERVAL_ULTRASONIC_POLL_MS 40      // Jeda pembacaan antar sensor ultrasonik (mencegah echo cross-talk)
#define INTERVAL_ENV_READ_MS        2000    // Interval pembacaan DHT11 dan MQ2
#define INTERVAL_TELEMETRY_MS       2000    // Interval pengiriman data telemetry via MQTT
#define INTERVAL_MQTT_RECONNECT_MS  5000    // Jeda coba ulang koneksi Wi-Fi & MQTT jika terputus
