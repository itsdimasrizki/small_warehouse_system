#include <Arduino.h>
#include <ArduinoJson.h>
#include "esp_private/brownout.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "config/pins.h"
#include "config/thresholds.h"
#include "types/system_state.h"
#include "devices/env_sensors.h"
#include "devices/rgb_indicator.h"
#include "devices/alarm_buzzer.h"
#include "fsm/rack_fsm.h"
#include "fsm/safety_fsm.h"
#include "network/wifi_portal.h"
#include "network/mqtt_manager.h"

// ============================================================================
// ESP32 MASTER FIRMWARE: IOT CONTROLLER, FSM MANAGER & ACTUATOR DRIVER
// Tugas:
// 1. Menerima data jarak 6 sensor ultrasonik dari ESP32 Slave via UART (Serial2).
// 2. Mengeksekusi RackFSM (penghitungan kapasitas rak & visualisasi 3x LED RGB).
// 3. Membaca sensor lingkungan (DHT11 & MQ-2).
// 4. Mengeksekusi SafetyFSM & mengontrol Buzzer alarm serta proteksi bahaya.
// 5. Mengelola konektivitas Wi-Fi (Captive Portal) dan komunikasi MQTT (telemetri/perintah).
// ============================================================================

// Inisialisasi Driver Perangkat Keras
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
unsigned long lastEnvRead = 0;
unsigned long lastDashboardPrint = 0;
unsigned long lastTelemetryPublish = 0;

// Status & Buffer Komunikasi UART Slave
static char slaveBuffer[256];
static size_t slaveBufferIndex = 0;
static unsigned long lastSlavePacketMs = 0;
static unsigned long slavePacketCount = 0;
static unsigned long rawBytesCount = 0;
static unsigned long parseErrorCount = 0;
static bool slaveConnected = false;

bool isCommonAnode = true;

void runStartupSelfTest();
void printDashboard();
void handleSerialCommands();
void handleSlaveUart();
void parseSlavePacket(const char* json);

void setup() {
    // 1. Nonaktifkan Brownout Detector agar lonjakan transmisi RF Wi-Fi tidak mereset ESP32
    esp_brownout_disable();
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        ; // Tunggu koneksi Serial Monitor USB
    }

    Serial.println("\n");
    Serial.println("========================================================");
    Serial.println("  ESP32 MASTER: WAREHOUSE CONTROLLER & FSM INITIALIZING ");
    Serial.println("========================================================");

    // 2. Inisialisasi UART Serial2 untuk menerima data dari Slave
    Serial2.begin(115200, SERIAL_8N1, PIN_MASTER_UART_RX, PIN_MASTER_UART_TX);
    Serial.printf("[INFO] UART Slave diaktifkan: RX=%d, TX=%d @ 115200 baud\n",
                  PIN_MASTER_UART_RX, PIN_MASTER_UART_TX);

    // 3. Inisialisasi driver aktuator & sensor lokal
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

    // Inisialisasi FSM & Jaringan
    rackFsm.begin();
    safetyFsm.begin(&systemState);
    wifiPortal.begin(15);
    mqttManager.begin();

    Serial.println("[INFO] Inisialisasi driver selesai. Menjalankan Self-Test Aktuator...");
    runStartupSelfTest();

    Serial.println("\n[SIAP] ESP32 Master siap menerima stream ultrasonik dari Slave dan mengelola IoT!");
    Serial.println("[TIPS] Ketik 'h' lalu tekan Enter untuk melihat perintah interaktif.\n");
}

void loop() {
    unsigned long currentMillis = millis();

    // 0. Layani Web Server Captive Portal jika mode AP sedang aktif
    wifiPortal.loop();

    // 1. Baca stream data jarak ultrasonik dari ESP Slave via UART Serial2
    handleSlaveUart();

    // if (slaveConnected && (currentMillis - lastSlavePacketMs > 3000)) {
    //     slaveConnected = false;
    //     Serial.println("\n[PERINGATAN UART] Kehilangan komunikasi dengan ESP32 Slave (>3 detik)!\n");
    // }

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

    // 6. Menampilkan Laporan Diagnostik ke Serial Monitor dengan jeda berkala
    if (wifiPortal.isPortalActive()) {
        if (currentMillis - lastDashboardPrint >= 8000) {
            lastDashboardPrint = currentMillis;
            Serial.println("\n==================================================");
            Serial.println("  [PORTAL SETUP AKTIF] Menunggu konfigurasi dari HP");
            Serial.println("  1. Sambungkan Wi-Fi HP ke : Warehouse-Rak-XXXX");
            Serial.println("  2. Buka Browser HP ke     : http://192.168.4.1/");
            Serial.println("==================================================\n");
        }
    } else {
        if (currentMillis - lastDashboardPrint >= 5000) {
            lastDashboardPrint = currentMillis;
            printDashboard();
        }
    }

    // 7. Penanganan Perintah Masuk via Serial Monitor
    handleSerialCommands();
}

void handleSlaveUart() {
    while (Serial2.available()) {
        char c = (char)Serial2.read();
        rawBytesCount++;
        if (c == '\n') {
            slaveBuffer[slaveBufferIndex] = '\0';
            if (slaveBufferIndex > 0) {
                parseSlavePacket(slaveBuffer);
            }
            slaveBufferIndex = 0;
        } else if (c != '\r' && slaveBufferIndex < sizeof(slaveBuffer) - 1) {
            slaveBuffer[slaveBufferIndex++] = c;
        }
    }
}

void parseSlavePacket(const char* json) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, json);
    if (error) {
        parseErrorCount++;
        return; // Abaikan baris rusak
    }

    if (doc["f1_l"].is<float>() && doc["f1_r"].is<float>()) {
        systemState.floors[0].distance_left_cm  = doc["f1_l"].as<float>();
        systemState.floors[0].distance_right_cm = doc["f1_r"].as<float>();
        systemState.floors[1].distance_left_cm  = doc["f2_l"].as<float>();
        systemState.floors[1].distance_right_cm = doc["f2_r"].as<float>();
        systemState.floors[2].distance_left_cm  = doc["f3_l"].as<float>();
        systemState.floors[2].distance_right_cm = doc["f3_r"].as<float>();

        lastSlavePacketMs = millis();
        slaveConnected = true;
        slavePacketCount++;

        // Evaluasi FSM Kapasitas Rak seketika dengan data jarak terbaru & update warna LED
        rackFsm.update(systemState);
    }
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
    Serial.printf("[DIAGNOSTIK MASTER & FSM] Waktu Aktif: %lu detik\n", millis() / 1000);
    Serial.println("------------------------------------------------------------------");

    // 0. Status UART ESP Slave
    Serial.println("0. Status Komunikasi UART ESP Slave:");
    Serial.printf("   * Pin Konfigurasi : RX Master=GPIO %d | TX Master=GPIO %d\n",
                  PIN_MASTER_UART_RX, PIN_MASTER_UART_TX);
    Serial.printf("   * Raw Bytes Masuk : %lu byte\n", rawBytesCount);
    Serial.printf("   * Paket Valid     : %lu paket (Error Parse: %lu)\n", slavePacketCount, parseErrorCount);
    if (slaveConnected) {
        Serial.printf("   * Status Link     : TERHUBUNG / AKTIF (delay: %lu ms)\n", millis() - lastSlavePacketMs);
    } else {
        Serial.println("   * Status Link     : MENUNGGU DATA SLAVE / TIMEOUT");
        if (rawBytesCount == 0) {
            Serial.println("     [PANDUAN KABEL]:");
            Serial.printf("     1. Sambungkan TX Slave (GPIO %d) -> RX Master (GPIO %d)\n",
                          PIN_SLAVE_UART_TX, PIN_MASTER_UART_RX);
            Serial.printf("     2. Sambungkan RX Slave (GPIO %d) <- TX Master (GPIO %d)\n",
                          PIN_SLAVE_UART_RX, PIN_MASTER_UART_TX);
            Serial.println("     3. Sambungkan GND Slave <-> GND Master (WAJIB Common Ground)!");
        }
    }

    // 1. Data Jarak 6 Sensor Ultrasonik (dari Slave) & FSM Rak
    Serial.println("\n1. Status Kapasitas Rak (Data Slave HC-SR04 & RackFSM):");
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
    Serial.println("------------------------------------------------------------------\n\n");
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
            Serial.println("\n[STATUS ULTRASONIK DARI SLAVE]:");
            Serial.printf("   Lantai 1 -> Kiri: %.1f cm | Kanan: %.1f cm\n",
                          systemState.floors[0].distance_left_cm, systemState.floors[0].distance_right_cm);
            Serial.printf("   Lantai 2 -> Kiri: %.1f cm | Kanan: %.1f cm\n",
                          systemState.floors[1].distance_left_cm, systemState.floors[1].distance_right_cm);
            Serial.printf("   Lantai 3 -> Kiri: %.1f cm | Kanan: %.1f cm\n",
                          systemState.floors[2].distance_left_cm, systemState.floors[2].distance_right_cm);
            Serial.printf("   Status Slave UART: %s (Paket: %lu, Delay: %lu ms)\n\n",
                          slaveConnected ? "ONLINE" : "OFFLINE / TIMEOUT",
                          slavePacketCount, millis() - lastSlavePacketMs);
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
            Serial.println("\n--- MENU BANTUAN SERIAL MONITOR (MASTER) ---");
            Serial.println("  'b'        : Beep Buzzer sekali (300 ms)");
            Serial.println("  'r'        : Tes siklus warna 3x LED RGB (Merah -> Hijau -> Biru)");
            Serial.println("  'c'        : Beralih mode polaritas LED RGB (Anode / Cathode)");
            Serial.println("  'u'        : Tampilkan jarak 6 sensor ultrasonik terbaru dari Slave");
            Serial.println("  'MUTE'/'m' : FSM Safety - MUTE / Diamkan alarm bahaya");
            Serial.println("  'ON'/'o'   : FSM Safety - Nyalakan Buzzer uji manual (MANUAL_TEST)");
            Serial.println("  'OFF'      : FSM Safety - Matikan Buzzer uji manual");
            Serial.println("  'AUTO'/'a' : FSM Safety - Kembalikan ke mode evaluasi otomatis");
            Serial.println("  'w'        : Buka SoftAP Captive Portal Konfigurasi Wi-Fi");
            Serial.println("  'k'        : Reset kredensial Wi-Fi NVS ke default");
            Serial.println("  'h'        : Tampilkan menu bantuan ini");
            Serial.println("--------------------------------------------");
            break;

        default:
            break;
    }
}
