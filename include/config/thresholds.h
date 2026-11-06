#pragma once

// Batas Deteksi Kapasitas Rak per Lantai (cm)
// Setiap lantai rak dapat memiliki ukuran/ambang batas deteksi yang berbeda
#define RACK1_DETECT_THRESHOLD_CM   35.0f   // Batas jarak Lantai 1 / Rak 1 (cm)
#define RACK2_DETECT_THRESHOLD_CM   35.0f   // Batas jarak Lantai 2 / Rak 2 (cm)
#define RACK3_DETECT_THRESHOLD_CM   35.0f   // Batas jarak Lantai 3 / Rak 3 (cm)

// Backward compatibility
#define RACK_DETECT_THRESHOLD_CM    RACK1_DETECT_THRESHOLD_CM
#define ULTRASONIC_MAX_DISTANCE_CM  200     // Batas jarak maksimum pembacaan (cm)

// Array statis threshold per rak (indeks 0 = Lantai 1, 1 = Lantai 2, 2 = Lantai 3)
static const float RACK_THRESHOLDS_CM[3] = {
    RACK1_DETECT_THRESHOLD_CM,
    RACK2_DETECT_THRESHOLD_CM,
    RACK3_DETECT_THRESHOLD_CM
};

// Batas Sensor Ruangan
#define TEMP_HAZARD_THRESHOLD_C     38.0f   // Ambang batas suhu (°C)
#define MQ2_HAZARD_THRESHOLD_ADC    1800    // Ambang batas ADC MQ2 (0 - 4095)

// Durasi Pulsa Beep Alarm Buzzer Bahaya (Non-blocking)
#define BUZZER_ALARM_PULSE_ON_MS    200     // Durasi ON saat alarm aktif (ms)
#define BUZZER_ALARM_PULSE_OFF_MS   200     // Durasi OFF saat alarm aktif (ms)
// Interval Penjadwalan Non-Blocking (millis)
#define INTERVAL_ULTRASONIC_POLL_MS 40      // Jeda pembacaan antar sensor ultrasonik
#define INTERVAL_ENV_READ_MS        2000    // Interval pembacaan DHT11 dan MQ2
#define INTERVAL_TELEMETRY_MS       2000    // Interval pengiriman data MQTT
#define INTERVAL_MQTT_RECONNECT_MS  5000    // Jeda coba ulang koneksi Wi-Fi & MQTT
