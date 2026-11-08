#pragma once

// ============================================================================
// ARSITEKTUR DUAL ESP32 (MASTER & SLAVE VIA UART HARDWARE SERIAL)
// ============================================================================

// ============================================================================
// 1. ALOKASI PIN ESP32 MASTER
// Tugas: DHT11, MQ2, 3x LED RGB Rak, Buzzer, Wi-Fi & MQTT, Evaluasi FSM
// ============================================================================

// UART Komunikasi ke Slave (Serial2)
#define PIN_MASTER_UART_RX    26  // Terhubung ke TX Slave (GPIO 17)
#define PIN_MASTER_UART_TX    27  // Terhubung ke RX Slave (GPIO 16)

// Sensor Lingkungan
#define PIN_DHT_DATA          4   // Sensor Suhu & Kelembapan DHT11
#define PIN_MQ2_ANALOG        36  // Sensor Asap/Gas MQ2 (ADC1 VP - aman bersama Wi-Fi)

// Buzzer Alarm
#define PIN_BUZZER            13  // Active Buzzer

// Indikator Rak 1 (Lantai 1) - Common Anode
#define PIN_RGB1_RED          23
#define PIN_RGB1_GREEN        22
#define PIN_RGB1_BLUE         21

// Indikator Rak 2 (Lantai 2) - Common Anode
#define PIN_RGB2_RED          19
#define PIN_RGB2_GREEN        18
#define PIN_RGB2_BLUE         5

// Indikator Rak 3 (Lantai 3) - Common Anode
#define PIN_RGB3_RED          17
#define PIN_RGB3_GREEN        16
#define PIN_RGB3_BLUE         14

// ============================================================================
// 2. ALOKASI PIN ESP32 SLAVE
// Tugas: Full Pembacaan 6x Sensor Ultrasonik HC-SR04 (Semua pin independen)
// ============================================================================

// UART Komunikasi ke Master (Serial2)
#define PIN_SLAVE_UART_TX     17  // Terhubung ke RX Master (GPIO 16)
#define PIN_SLAVE_UART_RX     16  // Terhubung ke TX Master (GPIO 17)

// Lantai 1 (T20 P48) - HC-SR04 Kiri & Kanan
#define PIN_SLAVE_TRIG_F1_L   14
#define PIN_SLAVE_ECHO_F1_L   35  // Pin VP (Input only)
#define PIN_SLAVE_TRIG_F1_R   27
#define PIN_SLAVE_ECHO_F1_R   34  // Pin VN (Input only)

// Lantai 2 (T20 P48) - HC-SR04 Kiri & Kanan
#define PIN_SLAVE_TRIG_F2_L   4
#define PIN_SLAVE_ECHO_F2_L   33  // Pin Input only
#define PIN_SLAVE_TRIG_F2_R   13
#define PIN_SLAVE_ECHO_F2_R   32  // Pin Input only

// Lantai 3 (T25 P48) - HC-SR04 Kiri & Kanan
#define PIN_SLAVE_TRIG_F3_L   18
#define PIN_SLAVE_ECHO_F3_L   26  // Digital I/O
#define PIN_SLAVE_TRIG_F3_R   5
#define PIN_SLAVE_ECHO_F3_R   25  // Digital I/O

// Backward Compatibility Alias
#define PIN_TRIG_F1_LEFT      PIN_SLAVE_TRIG_F1_L
#define PIN_ECHO_F1_LEFT      PIN_SLAVE_ECHO_F1_L
#define PIN_TRIG_F1_RIGHT     PIN_SLAVE_TRIG_F1_R
#define PIN_ECHO_F1_RIGHT     PIN_SLAVE_ECHO_F1_R
#define PIN_TRIG_F2_LEFT      PIN_SLAVE_TRIG_F2_L
#define PIN_ECHO_F2_LEFT      PIN_SLAVE_ECHO_F2_L
#define PIN_TRIG_F2_RIGHT     PIN_SLAVE_TRIG_F2_R
#define PIN_ECHO_F2_RIGHT     PIN_SLAVE_ECHO_F2_R
#define PIN_TRIG_F3_LEFT      PIN_SLAVE_TRIG_F3_L
#define PIN_ECHO_F3_LEFT      PIN_SLAVE_ECHO_F3_L
#define PIN_TRIG_F3_RIGHT     PIN_SLAVE_TRIG_F3_R
#define PIN_ECHO_F3_RIGHT     PIN_SLAVE_ECHO_F3_R
