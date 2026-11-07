#include <Arduino.h>
#include <NewPing.h>

#define MAX_DISTANCE 200

// Mapping Pin Ultrasonic ESP32
const uint8_t TRIG_PINS[6] = {27, 27, 27, 27, 27, 27};
const uint8_t ECHO_PINS[6] = {35, 34, 33, 32, 26, 25};
const char* SENSOR_NAMES[6] = {
    "Lantai 1 - Kiri  (Trig: 27, Echo: 35)",
    "Lantai 1 - Kanan (Trig: 27, Echo: 34)",
    "Lantai 2 - Kiri  (Trig: 27, Echo: 33)",
    "Lantai 2 - Kanan (Trig: 27, Echo: 32)",
    "Lantai 3 - Kiri  (Trig: 27, Echo: 26)",
    "Lantai 3 - Kanan (Trig: 27, Echo: 25)"
};

NewPing sonars[6] = {
    NewPing(TRIG_PINS[0], ECHO_PINS[0], MAX_DISTANCE),
    NewPing(TRIG_PINS[1], ECHO_PINS[1], MAX_DISTANCE),
    NewPing(TRIG_PINS[2], ECHO_PINS[2], MAX_DISTANCE),
    NewPing(TRIG_PINS[3], ECHO_PINS[3], MAX_DISTANCE),
    NewPing(TRIG_PINS[4], ECHO_PINS[4], MAX_DISTANCE),
    NewPing(TRIG_PINS[5], ECHO_PINS[5], MAX_DISTANCE)
};

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== TES MANDIRI 6x SENSOR ULTRASONIK HC-SR04 ===");
    Serial.println("Jeda pembacaan 40 ms per sensor untuk mencegah cross-talk.\n");
}

void loop() {
    Serial.println("--- Pembacaan Siklus Sensor Ultrasonik ---");
    for (int i = 0; i < 6; i++) {
        unsigned int uS = sonars[i].ping();
        float dist_cm = (float)uS / US_ROUNDTRIP_CM;
        
        Serial.print(" * ");
        Serial.print(SENSOR_NAMES[i]);
        Serial.print(": ");
        if (dist_cm == 0) {
            Serial.println("Di luar jangkauan / Tidak ada pantulan");
        } else {
            Serial.print(dist_cm, 1);
            Serial.println(" cm");
        }
        delay(40); // Wajib jeda >= 40 ms antar sensor
    }
    Serial.println();
    delay(1000);
}
