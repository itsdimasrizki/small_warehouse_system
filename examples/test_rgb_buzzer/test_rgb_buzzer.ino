// ==============================================================================
// Standalone Test: LED RGB Utama & Buzzer Alarm
// Menguji urutan warna RGB, deteksi Common Anode / Common Cathode, dan bunyi Buzzer.
// ==============================================================================

#include <Arduino.h>

#define PIN_RGB_RED   21
#define PIN_RGB_GREEN 22
#define PIN_RGB_BLUE  23
#define PIN_BUZZER    18

// Ubah ke true jika LED RGB yang digunakan bertipe Common Anode (+), false jika Common Cathode (-)
bool isCommonAnode = false;

void setRgb(uint8_t r, uint8_t g, uint8_t b) {
    uint8_t valR = isCommonAnode ? (255 - r) : r;
    uint8_t valG = isCommonAnode ? (255 - g) : g;
    uint8_t valB = isCommonAnode ? (255 - b) : b;
    analogWrite(PIN_RGB_RED, valR);
    analogWrite(PIN_RGB_GREEN, valG);
    analogWrite(PIN_RGB_BLUE, valB);
}

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== TES MANDIRI LED RGB & BUZZER ALARM ===");

    pinMode(PIN_RGB_RED, OUTPUT);
    pinMode(PIN_RGB_GREEN, OUTPUT);
    pinMode(PIN_RGB_BLUE, OUTPUT);
    pinMode(PIN_BUZZER, OUTPUT);

    setRgb(0, 0, 0);
    digitalWrite(PIN_BUZZER, LOW);
}

void loop() {
    Serial.println("-> Buzzer Beep (GPIO 18)...");
    digitalWrite(PIN_BUZZER, HIGH);
    delay(150);
    digitalWrite(PIN_BUZZER, LOW);
    delay(300);

    Serial.println("-> LED HIJAU (Status Rak: Kosong)...");
    setRgb(0, 255, 0);
    delay(1500);

    Serial.println("-> LED BIRU  (Status Rak: Terisi Sebagian)...");
    setRgb(0, 0, 255);
    delay(1500);

    Serial.println("-> LED MERAH (Status Rak: Penuh)...");
    setRgb(255, 0, 0);
    delay(1500);

    Serial.println("-> LED MATI...");
    setRgb(0, 0, 0);
    delay(1000);
}
