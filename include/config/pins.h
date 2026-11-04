#pragma once

// ==============================================================================
// Hardware Pin Mapping (GPIO) untuk NodeMCU ESP32 (38-pin / 30-pin)
// Proyek: Small Warehouse Monitoring & Safety System
// ==============================================================================

// --- HC-SR04 Ultrasonic Sensors (Sequential Polling Array) ---
// Catatan: GPIO 34, 35, 36, 39 adalah pin INPUT-ONLY pada ESP32 (tanpa internal pull-up).
// Gunakan voltage divider (1k & 2k ohm) pada ECHO jika HC-SR04 diberi tegangan 5V.

// Lantai 1 (Floor 1)
#define PIN_TRIG_F1_LEFT    13
#define PIN_ECHO_F1_LEFT    36  // VP (Input-only)
#define PIN_TRIG_F1_RIGHT   14
#define PIN_ECHO_F1_RIGHT   39  // VN (Input-only)

// Lantai 2 (Floor 2)
#define PIN_TRIG_F2_LEFT    27
#define PIN_ECHO_F2_LEFT    35  // Input-only
#define PIN_TRIG_F2_RIGHT   26
#define PIN_ECHO_F2_RIGHT   32

// Lantai 3 (Floor 3)
#define PIN_TRIG_F3_LEFT    25
#define PIN_ECHO_F3_LEFT    33
#define PIN_TRIG_F3_RIGHT   17
#define PIN_ECHO_F3_RIGHT   16

// --- Environmental & Safety Sensors ---
// DHT11 Sensor Suhu & Kelembapan (Digital I/O - disarankan pull-up 10k ke 3.3V)
#define PIN_DHT_DATA        4

// MQ2 Gas/Smoke Sensor (Wajib ADC1 agar tidak bentrok dengan Wi-Fi ESP32)
#define PIN_MQ2_ANALOG      34  // ADC1_CH6 (Input-only)

// --- Actuators & Indicators ---
// LED RGB Status Rak (Dapat dikonfigurasi Common Cathode atau Common Anode)
#define PIN_RGB_RED         21
#define PIN_RGB_GREEN       22
#define PIN_RGB_BLUE        23

// Buzzer Alarm (Active Buzzer 3.3V / 5V via Transistor/Direct)
#define PIN_BUZZER          18
