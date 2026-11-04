#pragma once

// Batas Deteksi Kapasitas Rak
#define RACK_DETECT_THRESHOLD_CM    35.0f   // Batas jarak (cm)
#define ULTRASONIC_MAX_DISTANCE_CM  200     // Batas jarak maksimum pembacaan (cm)

// Batas Sensor Ruangan
#define TEMP_HAZARD_THRESHOLD_C     38.0f   // Ambang batas suhu (°C)
#define MQ2_HAZARD_THRESHOLD_ADC    1800    // Ambang batas ADC MQ2 (0 - 4095)

// Interval Penjadwalan Non-Blocking (millis)
#define INTERVAL_ULTRASONIC_POLL_MS 40      // Jeda pembacaan antar sensor ultrasonik
#define INTERVAL_ENV_READ_MS        2000    // Interval pembacaan DHT11 dan MQ2
#define INTERVAL_TELEMETRY_MS       2000    // Interval pengiriman data MQTT
#define INTERVAL_MQTT_RECONNECT_MS  5000    // Jeda coba ulang koneksi Wi-Fi & MQTT
