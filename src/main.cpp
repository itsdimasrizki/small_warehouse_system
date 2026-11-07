#include <Arduino.h>
#include "config/pins.h"
#include "config/thresholds.h"
#include "types/system_state.h"
#include "devices/ultrasonic_array.h"
#include "devices/env_sensors.h"
#include "devices/rgb_indicator.h"
#include "devices/alarm_buzzer.h"
#include "fsm/rack_fsm.h"
#include "fsm/safety_fsm.h"
#include "network/wifi_portal.h"
#include "network/mqtt_manager.h"
// Inisialisasi Driver Perangkat Keras
UltrasonicArray ultrasonicArray;
EnvSensors envSensors;
RgbIndicator rgbIndicator(true); // Default Common Anode (+)
AlarmBuzzer buzzer;

// Inisialisasi Finite State Machine (FSM)
RackFSM rackFsm(rgbIndicator);
SafetyFSM safetyFsm(buzzer, &rgbIndicator);
WifiPortal wifiPortal;
MqttManager mqttManager(safetyFsm, &wifiPortal);
// State Global Sistem
SystemState systemState;

// Penjadwalan Waktu Non-Blocking (millis)
unsigned long lastUltrasonicPoll = 0;
unsigned long lastEnvRead = 0;
unsigned long lastDashboardPrint = 0;
unsigned long lastTelemetryPublish = 0;

bool isCommonAnode = true;

void runStartupSelfTest();
void printDashboard();
void handleSerialCommands();

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        ; // Tunggu koneksi Serial Monitor
    }

    Serial.println("\n");
    Serial.println("[INFO] Menginisialisasi seluruh driver sensor, aktuator, dan FSM...");

    ultrasonicArray.begin();
    envSensors.begin();
    rgbIndicator.begin();
    buzzer.begin();

    // Inisialisasi struct SystemState
    memset(&systemState, 0, sizeof(SystemState));
    systemState.rack_state = STATE_RACK_EMPTY;
    for (int i = 0; i < 3; i++) {
        systemState.floors[i].rack_state = STATE_RACK_EMPTY;
        systemState.floors[i].rgb_r = 255; // Kosong = Merah
    }
    systemState.safety_state = STATE_SAFE_NORMAL;

    // Inisialisasi FSM
    rackFsm.begin();
    safetyFsm.begin(&systemState);
    wifiPortal.begin(15);
    mqttManager.begin();
    Serial.println("[INFO] Inisialisasi selesai. Menjalankan Self-Test Aktuator...");
    runStartupSelfTest();

    Serial.println("\n[SIAP] Memulai pemantauan sensor dan eksekusi FSM secara realtime...");
    Serial.println("[TIPS] Ketik 'h' lalu tekan Enter untuk melihat perintah interaktif.\n");
}

void loop() {
    unsigned long currentMillis = millis();

    // 0. Layani Web Server Captive Portal jika mode AP sedang aktif
    wifiPortal.loop();
    // 1. Pembacaan Ultrasonik Sekuensial Non-Blocking (1 sensor setiap 40 ms)
    //    dan evaluasi FSM Kapasitas Rak per lantai
    if (currentMillis - lastUltrasonicPoll >= INTERVAL_ULTRASONIC_POLL_MS) {
        lastUltrasonicPoll = currentMillis;
        ultrasonicArray.pollNextSensor(systemState);
        rackFsm.update(systemState);
    }

    // 2. Pembacaan Sensor Lingkungan (DHT11 & MQ2) setiap 2 detik
    if (currentMillis - lastEnvRead >= INTERVAL_ENV_READ_MS) {
        lastEnvRead = currentMillis;
        envSensors.readSensors(systemState);
    }

    // 3. Eksekusi FSM Keamanan setiap siklus loop agar pulsa alarm buzzer non-blocking akurat
    safetyFsm.update(systemState);
    // 4. Pengiriman Telemetri MQTT Berbasis JSON setiap 2 detik
    if (currentMillis - lastTelemetryPublish >= INTERVAL_TELEMETRY_MS) {
        lastTelemetryPublish = currentMillis;
        mqttManager.publishTelemetry(systemState);
    }

    // 5. Pemeliharaan Koneksi Wi-Fi & MQTT Client Non-Blocking
    mqttManager.loop();

    // 6. Menampilkan Laporan Diagnostik ke Serial Monitor setiap 1 detik
    if (currentMillis - lastDashboardPrint >= 1000) {
        lastDashboardPrint = currentMillis;
        printDashboard();
    }

    // 7. Penanganan Perintah Masuk via Serial Monitor
    handleSerialCommands();
}

void runStartupSelfTest() {
    Serial.println(" -> Menguji Buzzer (2x Beep)...");
    buzzer.beep(80);
    delay(100);
    buzzer.beep(80);
    delay(200);

    Serial.println(" -> Menguji 3x LED RGB (Rak 1, 2, 3): MERAH...");
    rgbIndicator.setAllColor(255, 0, 0);
    delay(400);

    Serial.println(" -> Menguji 3x LED RGB (Rak 1, 2, 3): HIJAU...");
    rgbIndicator.setAllColor(0, 255, 0);
    delay(400);

    Serial.println(" -> Menguji 3x LED RGB (Rak 1, 2, 3): BIRU...");
    rgbIndicator.setAllColor(0, 0, 255);
    delay(400);

    rgbIndicator.offAll();
    Serial.println(" -> Self-Test Aktuator Berhasil!\n");
}

void printDashboard() {
    Serial.println("------------------------------------------------------------------");
    Serial.printf("[DIAGNOSTIK SENSOR & FSM] Waktu Aktif: %lu detik\n", millis() / 1000);
    Serial.println("------------------------------------------------------------------");

    // 1. Data Jarak 6 Sensor Ultrasonik & FSM Rak
    Serial.println("1. Status Kapasitas Rak (HC-SR04 & RackFSM):");
    const char* floorNames[3] = {"Lantai 1 (Rak 1)", "Lantai 2 (Rak 2)", "Lantai 3 (Rak 3)"};
    for (int i = 0; i < 3; i++) {
        float dL = systemState.floors[i].distance_left_cm;
        float dR = systemState.floors[i].distance_right_cm;
        int sides = systemState.floors[i].detected_sides;
        float th = rackFsm.getThreshold(i);
        const char* statusStr = RackFSM::getStateName(systemState.floors[i].rack_state);

        float span = systemState.floors[i].occupied_span_cm;
        int items = systemState.floors[i].item_count;
        Serial.printf("   * %s (Th: %.1f cm) -> Kiri: %5.1f cm | Kanan: %5.1f cm | Terisi: %4.1f cm (%d unit, %d sisi) -> %s\n",
                      floorNames[i], th, dL, dR, span, items, sides, statusStr);
    }
    
    Serial.printf("   >> Total Barang: %d unit | Total Lantai Terisi: %d/3 | FSM Rak Global: %s\n",
                  systemState.total_item_count,
                  systemState.occupied_floor_count,
                  RackFSM::getStateName(systemState.rack_state));

    // 2. DHT11 & MQ2 & FSM Safety
    Serial.println("\n2. Sensor Lingkungan & FSM Keamanan (DHT11, MQ2 & SafetyFSM):");
    if (isnan(systemState.temperature_c) || systemState.temperature_c == 0.0f) {
        Serial.printf("   * Suhu / Kelembapan (DHT11, GPIO %d) : [BELUM TERBACA ATAU PERIKSA KABEL]\n", PIN_DHT_DATA);
    } else {
        Serial.printf("   * Suhu Ruangan (DHT11, GPIO %d)       : %.1f °C  %s\n",
                      PIN_DHT_DATA,
                      systemState.temperature_c,
                      (systemState.temperature_c >= TEMP_HAZARD_THRESHOLD_C) ? "[PERINGATAN: PANAS!]" : "[NORMAL]");
        Serial.printf("   * Kelembapan Ruangan (DHT11, GPIO %d) : %.1f %%\n",
                      PIN_DHT_DATA,
                      systemState.humidity_pct);
    }

    Serial.printf("   * Asap/Gas Analog (MQ2, GPIO %d)     : %d ADC (%.1f%%)  %s\n",
                  PIN_MQ2_ANALOG,
                  systemState.smoke_adc,
                  systemState.smoke_pct,
                  (systemState.smoke_adc >= MQ2_HAZARD_THRESHOLD_ADC) ? "[PERINGATAN: ASAP PEKAT!]" : "[AMAN]");
    Serial.printf("   >> Kondisi Bahaya : %s\n",
                  systemState.hazard_detected ? "YA (BAHAYA AKTIF!)" : "TIDAK (AMAN)");
    Serial.printf("   >> State SafetyFSM: %s\n",
                  SafetyFSM::getStateName(systemState.safety_state));

    // 3. Status Aktuator
    Serial.println("\n3. Status Aktuator:");
    Serial.printf("   * Polaritas LED    : %s\n",
                  isCommonAnode ? "COMMON ANODE (+)" : "COMMON CATHODE (-)");
    for (int i = 0; i < 3; i++) {
        const char* colorName = (systemState.hazard_detected) ? "MERAH KEDIP (Bahaya Suhu/Gas!)" :
                                (systemState.floors[i].rack_state == STATE_RACK_EMPTY) ? "MERAH (Kosong)" :
                                (systemState.floors[i].rack_state == STATE_RACK_PARTIAL) ? "BIRU (Sebagian)" : "HIJAU (Penuh)";
        Serial.printf("   * LED RGB Rak %d    : R=%-3d G=%-3d B=%-3d -> %s\n",
                      i + 1,
                      systemState.floors[i].rgb_r,
                      systemState.floors[i].rgb_g,
                      systemState.floors[i].rgb_b,
                      colorName);
    }
    Serial.printf("   * Buzzer Alarm Pin : %s\n",
                  systemState.buzzer_active ? "MENYALA (ON / PULSED)" : "MATI (OFF)");

    // 4. Status Jaringan & MQTT
    Serial.println("\n4. Status Jaringan & MQTT:");
    if (wifiPortal.isPortalActive()) {
        Serial.println("   * Mode Jaringan    : SOFTAP CAPTIVE PORTAL (Aktif di 192.168.4.1)");
        Serial.println("   * Info Setup       : Sambungkan HP/Laptop ke Wi-Fi 'Warehouse-Rak-XXXX' untuk setup.");
    } else {
        Serial.printf("   * Koneksi Wi-Fi    : %s (SSID: %s)\n",
                      wifiPortal.isConnected() ? "TERHUBUNG (Online)" : "TERPUTUS / MENGHUBUNGKAN...",
                      wifiPortal.getSSID().c_str());
        Serial.printf("   * IP Address       : %s\n", WiFi.localIP().toString().c_str());
        Serial.printf("   * Broker MQTT      : %s (%s:%d)\n",
                      mqttManager.isMqttConnected() ? "TERHUBUNG (Active)" : "OFFLINE / MENCOBA KONEKSI...",
                      wifiPortal.getBroker().c_str(), wifiPortal.getPort());
    }
    Serial.printf("   * Topik Telemetri  : %s\n", TOPIC_TELEMETRY);
    Serial.printf("   * Topik Kontrol    : %s\n", TOPIC_BUZZER_CTRL);
    Serial.println("------------------------------------------------------------------");
}

void handleSerialCommands() {
    if (!Serial.available()) {
        return;
    }

    String input = Serial.readStringUntil('\n');
    input.trim();
    if (input.length() == 0) return;

    // Cek jika perintah string ditujukan untuk SafetyFSM
    String upper = input;
    upper.toUpperCase();
    if (upper == "MUTE" || upper == "OFF" || upper == "ON" || upper == "AUTO") {
        Serial.printf("\n[FSM SAFETY CMD] Perintah diterima: %s\n", upper.c_str());
        safetyFsm.handleMqttCommand(upper, systemState);
        return;
    }

    // Karakter tunggal
    char cmd = input.charAt(0);
    switch (cmd) {
        case 'b':
        case 'B':
            Serial.println("\n[MANUAL TEST] Menguji Buzzer selama 300 ms...");
            buzzer.beep(300);
            break;

        case 'm':
        case 'M':
            Serial.println("\n[FSM SAFETY CMD] MUTE Alarm...");
            safetyFsm.handleMqttCommand("MUTE", systemState);
            break;

        case 'o':
        case 'O':
            Serial.println("\n[FSM SAFETY CMD] Force ON Buzzer...");
            safetyFsm.handleMqttCommand("ON", systemState);
            break;

        case 'a':
        case 'A':
            Serial.println("\n[FSM SAFETY CMD] Mode AUTO Safety...");
            safetyFsm.handleMqttCommand("AUTO", systemState);
            break;

        case 'r':
        case 'R':
            Serial.println("\n[MANUAL TEST] Menjalankan siklus warna 3x LED RGB...");
            Serial.println("  -> Semua Merah...");
            rgbIndicator.setAllColor(255, 0, 0);
            delay(500);
            Serial.println("  -> Semua Hijau...");
            rgbIndicator.setAllColor(0, 255, 0);
            delay(500);
            Serial.println("  -> Semua Biru...");
            rgbIndicator.setAllColor(0, 0, 255);
            delay(500);
            rgbIndicator.offAll();
            Serial.println("  -> Selesai.");
            break;

        case 'c':
        case 'C':
            isCommonAnode = !isCommonAnode;
            rgbIndicator.setCommonAnode(isCommonAnode);
            Serial.printf("\n[CONFIG] Mode LED RGB diubah ke: %s\n", isCommonAnode ? "COMMON ANODE" : "COMMON CATHODE");
            break;

        case 'u':
        case 'U':
            Serial.println("\n[MANUAL TEST] Pembacaan langsung 6 ultrasonik: ");
            for (uint8_t i = 0; i < 6; i++) {
                float d = ultrasonicArray.readDirect(i);
                Serial.printf("   Sensor [%d] %s: %.1f cm\n", i, ultrasonicArray.getSensorName(i), d);
                delay(40);
            }
            break;

        case 'w':
        case 'W':
            Serial.println("\n[PORTAL] Membuka Web Portal SoftAP Konfigurasi Wi-Fi...");
            wifiPortal.startPortal();
            break;

        case 'k':
        case 'K':
            Serial.println("\n[CONFIG] Mereset kredensial Wi-Fi tersimpan di NVS ke default...");
            wifiPortal.resetSettings();
            break;

        case 'h':
        case 'H':
            Serial.println("\n--- MENU BANTUAN SERIAL MONITOR ---");
            Serial.println("  'b'        : Beep Buzzer sekali (300 ms)");
            Serial.println("  'r'        : Tes siklus warna 3x LED RGB (Merah -> Hijau -> Biru)");
            Serial.println("  'c'        : Beralih mode polaritas LED RGB (Anode / Cathode)");
            Serial.println("  'u'        : Baca langsung 6 sensor ultrasonik");
            Serial.println("  'MUTE'/'m' : FSM Safety - MUTE / Diamkan alarm bahaya");
            Serial.println("  'ON'/'o'   : FSM Safety - Nyalakan Buzzer uji manual (MANUAL_TEST)");
            Serial.println("  'OFF'      : FSM Safety - Matikan Buzzer uji manual");
            Serial.println("  'AUTO'/'a' : FSM Safety - Kembalikan ke mode evaluasi otomatis");
            Serial.println("  'w'        : Buka SoftAP Captive Portal Konfigurasi Wi-Fi");
            Serial.println("  'k'        : Reset kredensial Wi-Fi NVS ke default");
            Serial.println("  'h'        : Tampilkan menu bantuan ini");
            Serial.println("------------------------------------");
            break;

        default:
            break;
    }
}
