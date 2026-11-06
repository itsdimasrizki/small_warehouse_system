#pragma once
#include <Arduino.h>

// State FSM Kapasitas Rak
enum RackState {
    STATE_RACK_EMPTY = 0,    // 0 lantai terisi (0%)       -> Indikator LED HIJAU
    STATE_RACK_PARTIAL = 1,  // 1-2 lantai terisi (33-66%) -> Indikator LED BIRU
    STATE_RACK_FULL = 2      // 3 lantai terisi (100%)     -> Indikator LED MERAH
};

// State FSM Ruangan & Alarm Buzzer
enum SafetyState {
    STATE_SAFE_NORMAL = 0,      // Aman, Buzzer OFF
    STATE_ALARM_TRIGGERED = 1,  // Suhu/Asap melebihi threshold, Buzzer ON
    STATE_ALARM_SILENCED = 2,   // Operator melakukan MUTE/OFF dari MQTT, Buzzer OFF
    STATE_MANUAL_TEST = 3       // Buzzer dinyalakan manual via MQTT (ON) untuk pengujian
};

// Data Jarak & Okupansi per Lantai Rak
struct FloorData {
    float distance_left_cm;     // Pembacaan sensor HC-SR04 Kiri (cm)
    float distance_right_cm;    // Pembacaan sensor HC-SR04 Kanan (cm)
    bool is_occupied;           // True jika terisi (jarak < threshold dan > 0)
    int detected_sides;         // 0 = Kosong, 1 = Terisi 1 sisi, 2 = Terisi 2 sisi
    RackState rack_state;       // State FSM Rak ini (EMPTY / PARTIAL / FULL)
    uint8_t rgb_r;              // PWM Merah LED Rak (0 - 255)
    uint8_t rgb_g;              // PWM Hijau LED Rak (0 - 255)
    uint8_t rgb_b;              // PWM Biru LED Rak (0 - 255)
};

// Struct Global Status Sistem
struct SystemState {
    // 3 Lantai Rak (0 = Lantai 1, 1 = Lantai 2, 2 = Lantai 3)
    FloorData floors[3];
    int occupied_floor_count;   // Jumlah lantai terisi (0, 1, 2, atau 3)
    RackState rack_state;       // State FSM Kapasitas

    // Sensor Lingkungan
    float temperature_c;        // Suhu (°C) dari DHT11
    float humidity_pct;         // Kelembapan (%RH) dari DHT11
    int smoke_adc;              // Nilai ADC 12-bit MQ2 (0 - 4095)
    bool hazard_detected;       // True jika suhu >= 38.0C atau smoke_adc >= 1800
    SafetyState safety_state;   // State FSM Keamanan

    // Status Aktuator
    bool buzzer_active;         // Status nyala Buzzer
    uint8_t rgb_r;              // PWM Merah (0 - 255)
    uint8_t rgb_g;              // PWM Hijau (0 - 255)
    uint8_t rgb_b;              // PWM Biru (0 - 255)
};
