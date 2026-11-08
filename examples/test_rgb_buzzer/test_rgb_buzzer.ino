#include <Arduino.h>

// Rak 1 (Lantai 1)
#define PIN_RGB1_RED   23
#define PIN_RGB1_GREEN 22
#define PIN_RGB1_BLUE  21

// Rak 2 (Lantai 2)
#define PIN_RGB2_RED   19
#define PIN_RGB2_GREEN 18
#define PIN_RGB2_BLUE  5

// Rak 3 (Lantai 3)
#define PIN_RGB3_RED   17
#define PIN_RGB3_GREEN 16
#define PIN_RGB3_BLUE  14

#define PIN_BUZZER     13

bool isCommonAnode = false; // Default Common Cathode (-) (Kaki panjang ke GND)

const uint8_t PINS_R[3] = {PIN_RGB1_RED, PIN_RGB2_RED, PIN_RGB3_RED};
const uint8_t PINS_G[3] = {PIN_RGB1_GREEN, PIN_RGB2_GREEN, PIN_RGB3_GREEN};
const uint8_t PINS_B[3] = {PIN_RGB1_BLUE, PIN_RGB2_BLUE, PIN_RGB3_BLUE};

void setRackRgb(uint8_t rackIdx, uint8_t r, uint8_t g, uint8_t b) {
    if (rackIdx >= 3) return;
    uint8_t valR = isCommonAnode ? (255 - r) : r;
    uint8_t valG = isCommonAnode ? (255 - g) : g;
    uint8_t valB = isCommonAnode ? (255 - b) : b;
    analogWrite(PINS_R[rackIdx], valR);
    analogWrite(PINS_G[rackIdx], valG);
    analogWrite(PINS_B[rackIdx], valB);
}

void setAllRgb(uint8_t r, uint8_t g, uint8_t b) {
    for (uint8_t i = 0; i < 3; i++) {
        setRackRgb(i, r, g, b);
    }
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== TES MANDIRI 3x LED RGB & BUZZER ALARM ===");

    for (uint8_t i = 0; i < 3; i++) {
        pinMode(PINS_R[i], OUTPUT);
        pinMode(PINS_G[i], OUTPUT);
        pinMode(PINS_B[i], OUTPUT);
    }
    pinMode(PIN_BUZZER, OUTPUT);

    setAllRgb(0, 0, 0);
    digitalWrite(PIN_BUZZER, LOW);
}

void loop() {
    Serial.println("-> Buzzer Beep (GPIO 13)...");
    digitalWrite(PIN_BUZZER, HIGH);
    delay(150);
    digitalWrite(PIN_BUZZER, LOW);
    delay(300);

    Serial.println("-> Semua LED MERAH (Status Rak: Kosong)...");
    setAllRgb(255, 0, 0);
    delay(1500);

    Serial.println("-> Semua LED BIRU  (Status Rak: Terisi Sebagian)...");
    setAllRgb(0, 0, 255);
    delay(1500);

    Serial.println("-> Semua LED HIJAU (Status Rak: Penuh)...");
    setAllRgb(0, 255, 0);
    delay(1500);
    Serial.println("-> Uji Bergantian per Rak:");
    for (uint8_t i = 0; i < 3; i++) {
        Serial.printf("   * Rak %d menyala Hijau...\n", i + 1);
        setAllRgb(0, 0, 0);
        setRackRgb(i, 0, 255, 0);
        delay(600);
    }

    Serial.println("-> Semua LED MATI...\n");
    setAllRgb(0, 0, 0);
    delay(1000);
}
