#pragma once

// ==============================================================================
// Disesuaikan dengan ukuran fisik aktual rak:
// Lantai 1: Tinggi (T) = 20 cm, Panjang (P) = 48 cm
// Lantai 2: Tinggi (T) = 20 cm, Panjang (P) = 48 cm
// Lantai 3: Tinggi (T) = 25 cm, Panjang (P) = 48 cm
// ==============================================================================
#define RACK1_LENGTH_CM             48.0f   
#define RACK1_HEIGHT_CM             20.0f  

#define RACK2_LENGTH_CM             48.0f   
#define RACK2_HEIGHT_CM             20.0f   

#define RACK3_LENGTH_CM             48.0f   
#define RACK3_HEIGHT_CM             25.0f   

// Batas Deteksi Kapasitas Rak per Lantai (cm)
// ambang batas deteksi ideal adalah separuh panjang rak (P / 2 = 24.0 cm):
// - Jarak >= 24 cm (tidak ada halangan) = Kosong (Merah)
// - Jarak < 24 cm pada 1 sisi = Terisi Sebagian (Biru)
// - Jarak < 24 cm pada 2 sisi = Terisi Penuh (Hijau)
// - Hazard (Suhu/Gas melebihi threshold) = Merah Kedip-Kedip
// Ambang batas deteksi barang (cm):
#define RACK1_DETECT_THRESHOLD_CM   18.0f 
#define RACK2_DETECT_THRESHOLD_CM   18.0f   
#define RACK3_DETECT_THRESHOLD_CM   18.0f   

// Backward compatibility
#define RACK_DETECT_THRESHOLD_CM    RACK1_DETECT_THRESHOLD_CM
#define ULTRASONIC_MAX_DISTANCE_CM  200 

// Array statis threshold per rak (indeks 0 = Lantai 1, 1 = Lantai 2, 2 = Lantai 3)
static const float RACK_THRESHOLDS_CM[3] = {
    RACK1_DETECT_THRESHOLD_CM,
    RACK2_DETECT_THRESHOLD_CM,
    RACK3_DETECT_THRESHOLD_CM
};

// Array statis panjang rak fisik per lantai (cm)
static const float RACK_LENGTHS_CM[3] = {
    RACK1_LENGTH_CM,
    RACK2_LENGTH_CM,
    RACK3_LENGTH_CM
};

// Estimasi Lebar Standar 1 Unit Barang (cm)
#define DEFAULT_ITEM_WIDTH_CM       8.0f 

// Batas Sensor Ruangan
#define TEMP_HAZARD_THRESHOLD_C     50.0f   // suhu (°C)
#define MQ2_HAZARD_THRESHOLD_ADC    1800    // ADC MQ2 (0 - 4095)

// Durasi Alarm Buzzer Bahaya
#define BUZZER_ALARM_PULSE_ON_MS    200     // Durasi ON (ms)
#define BUZZER_ALARM_PULSE_OFF_MS   200     // Durasi OFF (ms)

// Interval Penjadwalan Non-Blocking (millis)
#define INTERVAL_ULTRASONIC_POLL_MS 40      
#define INTERVAL_ENV_READ_MS        2000    
#define INTERVAL_TELEMETRY_MS       2000    
#define INTERVAL_MQTT_RECONNECT_MS  5000    
