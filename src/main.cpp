#include <Arduino.h>
#include "config/pins.h"
#include "config/thresholds.h"
#include "types/system_state.h"
#include "devices/ultrasonic_array.h"
#include "devices/env_sensors.h"
#include "devices/rgb_indicator.h"
#include "devices/alarm_buzzer.h"

UltrasonicArray ultrasonicArray;
EnvSensors envSensors;
RgbIndicator rgbIndicator(false); 
AlarmBuzzer buzzer;

SystemState systemState;

unsigned long lastUltrasonicPoll = 0;
unsigned long lastEnvRead = 0;
unsigned long lastDashboardPrint = 0;

bool isCommonAnode = false;

void runStartupSelfTest();
void printDashboard();
void handleSerialCommands();
void updateRackEvaluation();

void setup() {
    Serial.begin(115200);
    while (!Serial && millis() < 2000) {
        ; // Tunggu koneksi Serial Monitor
    }

    Serial.println("\n");
    Serial.println("[INFO] Menginisialisasi seluruh driver sensor & aktuator...");

    ultrasonicArray.begin();
    envSensors.begin();
    rgbIndicator.begin();
    buzzer.begin();

    memset(&systemState, 0, sizeof(SystemState));
    systemState.rack_state = STATE_RACK_EMPTY;
    systemState.safety_state = STATE_SAFE_NORMAL;

    Serial.println("[INFO] Inisialisasi selesai. Menjalankan Self-Test Aktuator...");
    runStartupSelfTest();

    Serial.println("\n[SIAP] Memulai pemantauan sensor secara realtime...");
    Serial.println("[TIPS] Ketik 'h' lalu tekan Enter untuk melihat perintah interaktif.\n");
}

void loop() {
    unsigned long currentMillis = millis();

    // 1. Pembacaan Ultrasonik Sekuensial Non-Blocking (1 sensor setiap 40 ms)
    //    Mencegah interferensi gelombang ultrasonik (cross-talk) antara kiri & kanan.
    if (currentMillis - lastUltrasonicPoll >= INTERVAL_ULTRASONIC_POLL_MS) {
        lastUltrasonicPoll = currentMillis;
        ultrasonicArray.pollNextSensor(systemState);
        updateRackEvaluation();
    }

    // 2. DHT11 & MQ2 setiap 2 detik
    if (currentMillis - lastEnvRead >= INTERVAL_ENV_READ_MS) {
        lastEnvRead = currentMillis;
        envSensors.readSensors(systemState);
    }

    // 3. Menampilkan Laporan Diagnostik ke Serial Monitor setiap 1 detik
    if (currentMillis - lastDashboardPrint >= 1000) {
        lastDashboardPrint = currentMillis;
        printDashboard();
    }

    handleSerialCommands();
}

void runStartupSelfTest() {
    Serial.println(" -> Menguji Buzzer (2x Beep)...");
    buzzer.beep(80);
    delay(100);
    buzzer.beep(80);
    delay(200);

    Serial.println(" -> Menguji LED RGB: MERAH...");
    rgbIndicator.setRed();
    delay(400);

    Serial.println(" -> Menguji LED RGB: HIJAU...");
    rgbIndicator.setGreen();
    delay(400);

    Serial.println(" -> Menguji LED RGB: BIRU...");
    rgbIndicator.setBlue();
    delay(400);

    rgbIndicator.off();
    Serial.println(" -> Self-Test Aktuator Berhasil!\n");
}

void updateRackEvaluation() {
    int occupiedCount = 0;

    for (int i = 0; i < 3; i++) {
        float dL = systemState.floors[i].distance_left_cm;
        float dR = systemState.floors[i].distance_right_cm;

        // Suatu lantai dianggap terisi jika sensor kiri atau kanan membaca < 35 cm (> 0 cm)
        bool leftDetected  = (dL > 0.0f && dL < RACK_DETECT_THRESHOLD_CM);
        bool rightDetected = (dR > 0.0f && dR < RACK_DETECT_THRESHOLD_CM);

        systemState.floors[i].is_occupied = (leftDetected || rightDetected);
        if (systemState.floors[i].is_occupied) {
            occupiedCount++;
        }
    }

    systemState.occupied_floor_count = occupiedCount;

    // Visualisasikan status kapasitas ke LED RGB untuk memverifikasi LED berjalan dinamis
    if (occupiedCount == 0) {
        systemState.rack_state = STATE_RACK_EMPTY;
        rgbIndicator.setColorAndSync(0, 255, 0, systemState); // Hijau = Kosong
    } else if (occupiedCount == 3) {
        systemState.rack_state = STATE_RACK_FULL;
        rgbIndicator.setColorAndSync(255, 0, 0, systemState); // Merah = Penuh
    } else {
        systemState.rack_state = STATE_RACK_PARTIAL;
        rgbIndicator.setColorAndSync(0, 0, 255, systemState); // Biru = Terisi Sebagian
    }
}

void printDashboard() {
    Serial.println("------------------------------------------------------------------");
    Serial.printf("[DIAGNOSTIK SENSOR] Waktu Aktif: %lu detik\n", millis() / 1000);
    Serial.println("------------------------------------------------------------------");

    // 1. Data Jarak 6 Sensor Ultrasonik
    Serial.println("1. Sensor Ultrasonik (HC-SR04):");
    const char* floorNames[3] = {"Lantai 1", "Lantai 2", "Lantai 3"};
    for (int i = 0; i < 3; i++) {
        float dL = systemState.floors[i].distance_left_cm;
        float dR = systemState.floors[i].distance_right_cm;
        bool occ = systemState.floors[i].is_occupied;

        Serial.printf("   * %s -> Kiri: %5.1f cm | Kanan: %5.1f cm | Status: %s\n", floorNames[i], dL, dR, occ ? "[TERISI / OBJEK DETECTED]" : "[KOSONG]");
    }
    
    Serial.printf("   >> Total Lantai Terisi: %d/3 (State: %s)\n",
                  systemState.occupied_floor_count,
                  systemState.rack_state == STATE_RACK_EMPTY ? "KOSONG (HIJAU)" :
                  systemState.rack_state == STATE_RACK_PARTIAL ? "SEBAGIAN (BIRU)" : "PENUH (MERAH)");

    // 2. DHT11 & MQ2
    Serial.println("\n2. Sensor Lingkungan & Bahaya:");
    if (isnan(systemState.temperature_c) || systemState.temperature_c == 0.0f) {
        Serial.println("   * Suhu / Kelembapan (DHT11, GPIO 4) : [BELUM TERBACA ATAU PERIKSA KABEL]");
    } else {
        Serial.printf("   * Suhu Ruangan (DHT11, GPIO 4)       : %.1f °C  %s\n",
                      systemState.temperature_c,
                      (systemState.temperature_c >= TEMP_HAZARD_THRESHOLD_C) ? "[PERINGATAN: PANAS!]" : "[NORMAL]");
        Serial.printf("   * Kelembapan Ruangan (DHT11, GPIO 4) : %.1f %%\n",
                      systemState.humidity_pct);
    }

    Serial.printf("   * Asap/Gas Analog (MQ2, GPIO 34)     : %d ADC (0-4095)  %s\n",
                  systemState.smoke_adc,
                  (systemState.smoke_adc >= MQ2_HAZARD_THRESHOLD_ADC) ? "[PERINGATAN: ASAP PEKAT!]" : "[AMAN]");
    Serial.printf("   >> Status Bahaya: %s\n",
                  systemState.hazard_detected ? "BAHAYA TERDETEKSI!" : "AMAN");

    // 3. Status Aktuator
    Serial.println("\n3. Status Aktuator:");
    Serial.printf("   * LED RGB Status   : R=%d, G=%d, B=%d (%s)\n",
                  systemState.rgb_r, systemState.rgb_g, systemState.rgb_b,
                  isCommonAnode ? "Mode Common Anode" : "Mode Common Cathode");
    Serial.printf("   * Buzzer Alarm Pin : %s\n",
                  systemState.buzzer_active ? "MENYALA (ON)" : "MATI (OFF)");
    Serial.println("------------------------------------------------------------------");
}

void handleSerialCommands() {
    if (!Serial.available()) {
        return;
    }

    char cmd = Serial.read();
    while (Serial.available()) {
        Serial.read();
    }

    switch (cmd) {
        case 'b':
        case 'B':
            Serial.println("\n[MANUAL TEST] Menguji Buzzer selama 300 ms...");
            buzzer.beep(300);
            break;

        case 'r':
        case 'R':
            Serial.println("\n[MANUAL TEST] Menjalankan siklus warna LED RGB...");
            rgbIndicator.setRed();
            delay(500);
            rgbIndicator.setGreen();
            delay(500);
            rgbIndicator.setBlue();
            delay(500);
            rgbIndicator.off();
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

        case 'h':
        case 'H':
            Serial.println("  'b' : Buzzer (300 ms)");
            Serial.println("  'r' : Tes siklus warna LED RGB (Merah -> Hijau -> Biru)");
            Serial.println("  'c' : Beralih mode LED RGB");
            Serial.println("  'u' : 6 sensor ultrasonik");
            Serial.println("  'h' : Tampilkan menu");
            break;

        default:
            break;
    }
}
