/**
 * ============================================================================
 * PROGRAM UJI KHUSUS LAMPU LED RGB PER LANTAI (RAK GUDANG)
 * Lokasi: tests/test_led_floors.cpp
 * ============================================================================
 * Alokasi Pin Hardware:
 *   - Lantai 1 (Floor 1): Red = GPIO 1,  Green = GPIO 2,  Blue = GPIO 42
 *   - Lantai 2 (Floor 2): Red = GPIO 39, Green = GPIO 38, Blue = GPIO 37
 *   - Lantai 3 (Floor 3): Red = GPIO 45, Green = GPIO 48, Blue = GPIO 47
 *
 * Fitur:
 *   - Setiap lantai punya kendali independen (state terisolasi).
 *   - Input manual:
 *       'r' : Menyala Merah (Red)
 *       'g' : Menyala Hijau (Green)
 *       'b' : Menyala Biru  (Blue)
 *       'x' : Menyala Bergantian (Cycle R -> G -> B non-blocking)
 *       '0' / 'off' : Mematikan LED lantai tersebut
 *   - Format Perintah:
 *       1. Langsung: 1r, 1g, 1b, 1x, 1off, 2r, 3x, dsb.
 *       2. Interaktif: Ketik "1", lalu ketik "r", "x", dsb.
 *       3. Massal: all r, all g, all b, all x, all off
 *   - Konfigurasi:
 *       'cc' : Common Cathode / Katoda (-) -> Sinyal HIGH = ON, LOW = OFF [Default]
 *       'ca' : Common Anode (+)            -> Sinyal LOW = ON, HIGH = OFF
 *       'speed <ms>' : Mengatur jeda gantian mode 'x' (default 500 ms)
 *       'status' / 's' : Menampilkan tabel status pin & mode
 *       'help' / '?'   : Menampilkan panduan perintah
 * ============================================================================
 */

#include <Arduino.h>

// ============================================================================
// 1. DEFINISI PIN LED RGB PER LANTAI
// ============================================================================

// Lantai 1 (Floor 1)
#define PIN_F1_RED    1
#define PIN_F1_GREEN  2
#define PIN_F1_BLUE   42

// Lantai 2 (Floor 2)
#define PIN_F2_RED    39
#define PIN_F2_GREEN  38
#define PIN_F2_BLUE   37

// Lantai 3 (Floor 3)
#define PIN_F3_RED    45
#define PIN_F3_GREEN  48
#define PIN_F3_BLUE   47

// ============================================================================
// 2. STRUKTUR DATA & STATE INDIKATOR
// ============================================================================

enum LedColorMode : uint8_t {
    MODE_OFF = 0,
    MODE_RED,
    MODE_GREEN,
    MODE_BLUE,
    MODE_CYCLE // Mode 'x' (menyala bergantian R -> G -> B)
};

struct FloorLedChannel {
    uint8_t floorNum;          // 1, 2, atau 3
    uint8_t pinR;
    uint8_t pinG;
    uint8_t pinB;
    LedColorMode mode;         // Mode warna saat ini
    uint8_t cycleStep;         // Langkah siklus mode 'x' (0=R, 1=G, 2=B)
    unsigned long lastStepMs;  // Timestamp millis pergantian warna terakhir
    uint8_t currentLevelR;     // Status logika pin saat ini (HIGH / LOW)
    uint8_t currentLevelG;
    uint8_t currentLevelB;
};

// Prototipe Fungsi (Mencegah error deklarasi pada parser Arduino IDE)
void writeFloorPins(uint8_t floorIdx, bool rOn, bool gOn, bool bOn);
void updateFloorOutput(uint8_t floorIdx);
void setFloorMode(uint8_t floorIdx, LedColorMode newMode);
void printFloorState(uint8_t floorIdx);
void printStatus();
void printHelp();
void handleCommand(String rawCmd);
void processLedCycle();

// Inisialisasi konfigurasi pin masing-masing lantai
static FloorLedChannel floors[3] = {
    { 1, PIN_F1_RED, PIN_F1_GREEN, PIN_F1_BLUE, MODE_OFF, 0, 0, LOW, LOW, LOW },
    { 2, PIN_F2_RED, PIN_F2_GREEN, PIN_F2_BLUE, MODE_OFF, 0, 0, LOW, LOW, LOW },
    { 3, PIN_F3_RED, PIN_F3_GREEN, PIN_F3_BLUE, MODE_OFF, 0, 0, LOW, LOW, LOW }
};

// Konfigurasi sistem
static bool isCommonAnode = false;         // Default Common Cathode / Katoda (Active HIGH)
static uint8_t selectedFloor = 0;           // Lantai aktif untuk mode input bertahap (0 = Lantai 1)
static unsigned long cycleIntervalMs = 500; // Kecepatan siklus mode 'x' (ms)
static String serialInputBuffer = "";

// ============================================================================
// 3. FUNGSI KONTROL PIN HARDWARE
// ============================================================================

void writeFloorPins(uint8_t floorIdx, bool rOn, bool gOn, bool bOn) {
    if (floorIdx >= 3) return;
    FloorLedChannel &f = floors[floorIdx];

    // Common Anode: ON = LOW, OFF = HIGH
    // Common Cathode: ON = HIGH, OFF = LOW
    uint8_t levelR = isCommonAnode ? (rOn ? LOW : HIGH) : (rOn ? HIGH : LOW);
    uint8_t levelG = isCommonAnode ? (gOn ? LOW : HIGH) : (gOn ? HIGH : LOW);
    uint8_t levelB = isCommonAnode ? (bOn ? LOW : HIGH) : (bOn ? HIGH : LOW);

    f.currentLevelR = levelR;
    f.currentLevelG = levelG;
    f.currentLevelB = levelB;

    digitalWrite(f.pinR, levelR);
    digitalWrite(f.pinG, levelG);
    digitalWrite(f.pinB, levelB);
}

void updateFloorOutput(uint8_t floorIdx) {
    if (floorIdx >= 3) return;
    FloorLedChannel &f = floors[floorIdx];

    switch (f.mode) {
        case MODE_OFF:
            writeFloorPins(floorIdx, false, false, false);
            break;
        case MODE_RED:
            writeFloorPins(floorIdx, true, false, false);
            break;
        case MODE_GREEN:
            writeFloorPins(floorIdx, false, true, false);
            break;
        case MODE_BLUE:
            writeFloorPins(floorIdx, false, false, true);
            break;
        case MODE_CYCLE:
            if (f.cycleStep == 0) {
                writeFloorPins(floorIdx, true, false, false);  // Merah
            } else if (f.cycleStep == 1) {
                writeFloorPins(floorIdx, false, true, false);  // Hijau
            } else {
                writeFloorPins(floorIdx, false, false, true);  // Biru
            }
            break;
    }
}

void setFloorMode(uint8_t floorIdx, LedColorMode newMode) {
    if (floorIdx >= 3) return;
    floors[floorIdx].mode = newMode;
    if (newMode == MODE_CYCLE) {
        floors[floorIdx].cycleStep = 0;
        floors[floorIdx].lastStepMs = millis();
    }
    updateFloorOutput(floorIdx);
}

// ============================================================================
// 4. FUNGSI TAMPILAN MONITOR SERIAL
// ============================================================================

void printFloorState(uint8_t floorIdx) {
    if (floorIdx >= 3) return;
    const FloorLedChannel &f = floors[floorIdx];
    Serial.print(F("-> [Lantai "));
    Serial.print(f.floorNum);
    Serial.print(F("] "));
    switch (f.mode) {
        case MODE_OFF:   Serial.println(F("MATI (OFF)")); break;
        case MODE_RED:   Serial.println(F("Menyala MERAH (Red)")); break;
        case MODE_GREEN: Serial.println(F("Menyala HIJAU (Green)")); break;
        case MODE_BLUE:  Serial.println(F("Menyala BIRU (Blue)")); break;
        case MODE_CYCLE: Serial.println(F("Menyala GANTIAN [x] (Merah -> Hijau -> Biru)")); break;
    }
}

void printStatus() {
    Serial.println(F("\n================= STATUS INDIKATOR LED LANTAI ================="));
    Serial.print(F("Tipe LED Aktif      : "));
    Serial.println(isCommonAnode ? F("Common Anode (+) [Active LOW]") : F("Common Cathode / Katoda (-) [Active HIGH, Default]"));
    Serial.print(F("Lantai Terpilih     : Lantai "));
    Serial.println(selectedFloor + 1);
    Serial.print(F("Kecepatan Siklus 'x': "));
    Serial.print(cycleIntervalMs);
    Serial.println(F(" ms\n"));
    Serial.println(F("Lantai | Pin (R, G, B) | Mode Saat Ini | Status Pin (R, G, B)"));
    Serial.println(F("-------+---------------+---------------+---------------------"));
    for (uint8_t i = 0; i < 3; i++) {
        const FloorLedChannel &f = floors[i];
        Serial.print(F("  "));
        Serial.print(f.floorNum);
        Serial.print(F("    | "));
        
        String pinsStr = String(f.pinR) + ", " + String(f.pinG) + ", " + String(f.pinB);
        while (pinsStr.length() < 13) pinsStr += " ";
        Serial.print(pinsStr);
        Serial.print(F(" | "));

        const char* modeStr = "MATI (OFF) ";
        if (f.mode == MODE_RED)   modeStr = "MERAH (R)  ";
        if (f.mode == MODE_GREEN) modeStr = "HIJAU (G)  ";
        if (f.mode == MODE_BLUE)  modeStr = "BIRU  (B)  ";
        if (f.mode == MODE_CYCLE) modeStr = "GANTIAN [x]";
        Serial.print(modeStr);
        Serial.print(F(" | "));

        Serial.print(F("R:"));
        Serial.print(f.currentLevelR == HIGH ? F("HIGH ") : F("LOW  "));
        Serial.print(F("G:"));
        Serial.print(f.currentLevelG == HIGH ? F("HIGH ") : F("LOW  "));
        Serial.print(F("B:"));
        Serial.println(f.currentLevelB == HIGH ? F("HIGH") : F("LOW"));
    }
    Serial.println(F("===============================================================\n"));
}

void printHelp() {
    Serial.println(F("\n====================== PANDUAN PENGUJIAN LED ======================"));
    Serial.println(F("Alokasi Pin Hardware (R G B):"));
    Serial.println(F("  - Lantai 1: Pin 1,  2, 42"));
    Serial.println(F("  - Lantai 2: Pin 39, 38, 37"));
    Serial.println(F("  - Lantai 3: Pin 45, 48, 47"));
    Serial.println(F("\nCara 1: Perintah Langsung (Lantai + Aksi):"));
    Serial.println(F("  1r  -> Lantai 1 Merah        | 2r  -> Lantai 2 Merah"));
    Serial.println(F("  1g  -> Lantai 1 Hijau        | 2g  -> Lantai 2 Hijau"));
    Serial.println(F("  1b  -> Lantai 1 Biru         | 2b  -> Lantai 2 Biru"));
    Serial.println(F("  1x  -> Lantai 1 Gantian      | 2x  -> Lantai 2 Gantian"));
    Serial.println(F("  1off-> Lantai 1 Mati         | 2off-> Lantai 2 Mati"));
    Serial.println(F("  (Bisa juga dengan spasi, contoh: '1 r', '2 x', '3 b', '3 off')"));
    Serial.println(F("\nCara 2: Pilih Lantai Dahulu (Interaktif):"));
    Serial.println(F("  1   -> Pilih Lantai 1"));
    Serial.println(F("  2   -> Pilih Lantai 2"));
    Serial.println(F("  3   -> Pilih Lantai 3"));
    Serial.println(F("  Setelah lantai dipilih, cukup masukkan: r, g, b, x, atau 0/off"));
    Serial.println(F("\nPerintah Global:"));
    Serial.println(F("  all r / all g / all b : Nyalakan semua lantai warna R / G / B"));
    Serial.println(F("  all x                 : Nyalakan semua lantai bergantian [x]"));
    Serial.println(F("  all off / off         : Matikan semua lantai"));
    Serial.println(F("  status / s            : Tampilkan status & logika pin tiap lantai"));
    Serial.println(F("  cc                    : Set Common Cathode / Katoda (-) [Active HIGH, Default]"));
    Serial.println(F("  ca                    : Set Common Anode (+) [Active LOW]"));
    Serial.println(F("  speed <ms>            : Set jeda gantian mode 'x' (contoh: speed 300)"));
    Serial.println(F("  help / ?              : Menampilkan menu bantuan ini"));
    Serial.println(F("===================================================================\n"));
}

// ============================================================================
// 5. PARSER & PENGENDALI PERINTAH SERIAL
// ============================================================================

void handleCommand(String rawCmd) {
    rawCmd.trim();
    if (rawCmd.length() == 0) return;

    String cmd = rawCmd;
    cmd.toLowerCase();

    // 1. Perintah Bantuan & Status
    if (cmd == "help" || cmd == "?" || cmd == "menu") {
        printHelp();
        return;
    }
    if (cmd == "status" || cmd == "s") {
        printStatus();
        return;
    }

    // 2. Perintah Konfigurasi Tipe LED
    if (cmd == "ca") {
        isCommonAnode = true;
        for (uint8_t i = 0; i < 3; i++) updateFloorOutput(i);
        Serial.println(F("[KONFIGURASI] Tipe LED diubah ke Common Anode (+) [Sinyal LOW = ON]"));
        return;
    }
    if (cmd == "cc") {
        isCommonAnode = false;
        for (uint8_t i = 0; i < 3; i++) updateFloorOutput(i);
        Serial.println(F("[KONFIGURASI] Tipe LED diubah ke Common Cathode (-) [Sinyal HIGH = ON]"));
        return;
    }

    // 3. Perintah Kecepatan Siklus 'x'
    if (cmd.startsWith("speed ") || cmd.startsWith("kecepatan ")) {
        int spaceIdx = cmd.indexOf(' ');
        int val = cmd.substring(spaceIdx + 1).toInt();
        if (val >= 50 && val <= 10000) {
            cycleIntervalMs = (unsigned long)val;
            Serial.print(F("[KONFIGURASI] Kecepatan siklus mode 'x' diatur ke "));
            Serial.print(cycleIntervalMs);
            Serial.println(F(" ms."));
        } else {
            Serial.println(F("[ERROR] Nilai speed harus antara 50 dan 10000 ms."));
        }
        return;
    }

    // 4. Perintah Massal Semua Lantai
    if (cmd == "all off" || cmd == "a off" || cmd == "a0" || cmd == "off" || cmd == "mati") {
        for (uint8_t i = 0; i < 3; i++) setFloorMode(i, MODE_OFF);
        Serial.println(F("-> [SEMUA LANTAI] Lampu dimatikan (OFF)."));
        return;
    }
    if (cmd == "all r" || cmd == "ar" || cmd == "all red" || cmd == "all merah") {
        for (uint8_t i = 0; i < 3; i++) setFloorMode(i, MODE_RED);
        Serial.println(F("-> [SEMUA LANTAI] Menyala MERAH."));
        return;
    }
    if (cmd == "all g" || cmd == "ag" || cmd == "all green" || cmd == "all hijau") {
        for (uint8_t i = 0; i < 3; i++) setFloorMode(i, MODE_GREEN);
        Serial.println(F("-> [SEMUA LANTAI] Menyala HIJAU."));
        return;
    }
    if (cmd == "all b" || cmd == "ab" || cmd == "all blue" || cmd == "all biru") {
        for (uint8_t i = 0; i < 3; i++) setFloorMode(i, MODE_BLUE);
        Serial.println(F("-> [SEMUA LANTAI] Menyala BIRU."));
        return;
    }
    if (cmd == "all x" || cmd == "ax" || cmd == "all cycle" || cmd == "all gantian") {
        for (uint8_t i = 0; i < 3; i++) setFloorMode(i, MODE_CYCLE);
        Serial.println(F("-> [SEMUA LANTAI] Menyala GANTIAN [x] (R -> G -> B)."));
        return;
    }

    // 5. Identifikasi Target Lantai
    int floorIdx = -1;
    String action = "";

    if (cmd.startsWith("f1") || cmd.startsWith("l1")) {
        floorIdx = 0;
        action = cmd.substring(2);
    } else if (cmd.startsWith("f2") || cmd.startsWith("l2")) {
        floorIdx = 1;
        action = cmd.substring(2);
    } else if (cmd.startsWith("f3") || cmd.startsWith("l3")) {
        floorIdx = 2;
        action = cmd.substring(2);
    } else if (cmd.length() >= 1 && (cmd[0] == '1' || cmd[0] == '2' || cmd[0] == '3')) {
        floorIdx = cmd[0] - '1';
        action = cmd.substring(1);
    }

    action.trim();

    // Jika user menginput nomor lantai
    if (floorIdx >= 0) {
        selectedFloor = (uint8_t)floorIdx;

        // Jika hanya memasukkan nomor lantai saja (contoh: "1", "2", atau "3")
        if (action.length() == 0) {
            Serial.print(F("[Lantai "));
            Serial.print(selectedFloor + 1);
            Serial.println(F(" Terpilih] Masukkan aksi: r (Merah), g (Hijau), b (Biru), x (Gantian), 0/off (Mati)"));
            return;
        }
    } else {
        // Jika tidak ada nomor lantai di awal, terapkan ke lantai yang sedang terpilih
        floorIdx = selectedFloor;
        action = cmd;
    }

    // 6. Eksekusi Aksi Warna
    if (action == "r" || action == "red" || action == "merah") {
        setFloorMode((uint8_t)floorIdx, MODE_RED);
        printFloorState((uint8_t)floorIdx);
    } else if (action == "g" || action == "green" || action == "hijau") {
        setFloorMode((uint8_t)floorIdx, MODE_GREEN);
        printFloorState((uint8_t)floorIdx);
    } else if (action == "b" || action == "blue" || action == "biru") {
        setFloorMode((uint8_t)floorIdx, MODE_BLUE);
        printFloorState((uint8_t)floorIdx);
    } else if (action == "x" || action == "cycle" || action == "gantian") {
        setFloorMode((uint8_t)floorIdx, MODE_CYCLE);
        printFloorState((uint8_t)floorIdx);
    } else if (action == "0" || action == "off" || action == "mati") {
        setFloorMode((uint8_t)floorIdx, MODE_OFF);
        printFloorState((uint8_t)floorIdx);
    } else {
        Serial.print(F("[PERINGATAN] Perintah tidak dikenal: '"));
        Serial.print(rawCmd);
        Serial.println(F("'. Ketik 'help' untuk daftar perintah lengkap."));
    }
}

// ============================================================================
// 6. LOOP SIKLUS NON-BLOCKING (MODE 'x' GANTIAN)
// ============================================================================

void processLedCycle() {
    unsigned long now = millis();
    for (uint8_t i = 0; i < 3; i++) {
        if (floors[i].mode == MODE_CYCLE) {
            if (now - floors[i].lastStepMs >= cycleIntervalMs) {
                floors[i].lastStepMs = now;
                floors[i].cycleStep = (floors[i].cycleStep + 1) % 3;
                updateFloorOutput(i);
            }
        }
    }
}

// ============================================================================
// 7. SETUP & LOOP ARDUINO RUNTIME (ESP32)
// ============================================================================

void setup() {
    Serial.begin(115200);
    delay(1000);

    // Inisialisasi seluruh pin sebagai OUTPUT dan matikan di awal
    for (uint8_t i = 0; i < 3; i++) {
        pinMode(floors[i].pinR, OUTPUT);
        pinMode(floors[i].pinG, OUTPUT);
        pinMode(floors[i].pinB, OUTPUT);
        updateFloorOutput(i);
    }

    Serial.println(F("\n======================================================="));
    Serial.println(F("   PROGRAM UJI MANDIRI LAMPU LED RGB PER LANTAI"));
    Serial.println(F("   Proyek Small Warehouse IoT - NodeMCU ESP32"));
    Serial.println(F("======================================================="));
    printHelp();
    printStatus();
    Serial.println(F("Ketik perintah lalu tekan Enter:"));
}

void loop() {
    // 1. Jalankan siklus animasi warna mode 'x' secara non-blocking
    processLedCycle();

    // 2. Baca masukan pengguna dari Serial Monitor
    while (Serial.available() > 0) {
        char c = (char)Serial.read();
        if (c == '\r') {
            continue; // Lewati karakter carriage return
        }
        if (c == '\n') {
            serialInputBuffer.trim();
            if (serialInputBuffer.length() > 0) {
                Serial.print(F("Input > "));
                Serial.println(serialInputBuffer);
                handleCommand(serialInputBuffer);
                serialInputBuffer = "";
            }
        } else {
            if (serialInputBuffer.length() < 64) {
                serialInputBuffer += c;
            }
        }
    }
}
