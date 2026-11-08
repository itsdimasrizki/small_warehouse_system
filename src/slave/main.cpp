#include <Arduino.h>
#include <ArduinoJson.h>
#include "esp_private/brownout.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "config/pins.h"

// Timeout ultrasonik: 30 ms (~514 cm, jauh melampaui lebar rak 48 cm)
static const unsigned long ULTRASONIC_TIMEOUT_MS   = 30;
// Jeda antar sisi (Kiri <-> Kanan) agar gelombang pantulan akustik benar-benar reda
static const unsigned long DELAY_BETWEEN_SIDES_MS  = 45;
// Baud rate komunikasi UART antar ESP
static const unsigned long SLAVE_UART_BAUD         = 115200;

enum UltrasonicPhase {
    PHASE_TRIGGER_LEFT,
    PHASE_WAIT_LEFT,
    PHASE_DELAY_AFTER_LEFT,
    PHASE_TRIGGER_RIGHT,
    PHASE_WAIT_RIGHT,
    PHASE_DELAY_AFTER_RIGHT,
    PHASE_SEND_DATA
};

static UltrasonicPhase currentPhase = PHASE_TRIGGER_LEFT;
static unsigned long phaseStartTime = 0;

// Indeks sensor:
// 0: Lantai 1 Kiri,  1: Lantai 1 Kanan
// 2: Lantai 2 Kiri,  3: Lantai 2 Kanan
// 4: Lantai 3 Kiri,  5: Lantai 3 Kanan
volatile uint32_t echoStartUs[6]     = {0};
volatile uint32_t echoDurationUs[6]  = {0};
volatile bool     echoFinished[6]    = {false};

float currentDistances[6]            = {0.0f};
unsigned long packetCount            = 0;
unsigned long lastDebugPrint         = 0;

// ISR Handler dengan IRAM_ATTR untuk presisi microsecond
void IRAM_ATTR isrEchoF1L() {
    uint32_t now = micros();
    if (digitalRead(PIN_SLAVE_ECHO_F1_L) == HIGH) {
        echoStartUs[0] = now;
        echoFinished[0] = false;
    } else if (echoStartUs[0] > 0) {
        echoDurationUs[0] = now - echoStartUs[0];
        echoFinished[0] = true;
    }
}

void IRAM_ATTR isrEchoF1R() {
    uint32_t now = micros();
    if (digitalRead(PIN_SLAVE_ECHO_F1_R) == HIGH) {
        echoStartUs[1] = now;
        echoFinished[1] = false;
    } else if (echoStartUs[1] > 0) {
        echoDurationUs[1] = now - echoStartUs[1];
        echoFinished[1] = true;
    }
}

void IRAM_ATTR isrEchoF2L() {
    uint32_t now = micros();
    if (digitalRead(PIN_SLAVE_ECHO_F2_L) == HIGH) {
        echoStartUs[2] = now;
        echoFinished[2] = false;
    } else if (echoStartUs[2] > 0) {
        echoDurationUs[2] = now - echoStartUs[2];
        echoFinished[2] = true;
    }
}

void IRAM_ATTR isrEchoF2R() {
    uint32_t now = micros();
    if (digitalRead(PIN_SLAVE_ECHO_F2_R) == HIGH) {
        echoStartUs[3] = now;
        echoFinished[3] = false;
    } else if (echoStartUs[3] > 0) {
        echoDurationUs[3] = now - echoStartUs[3];
        echoFinished[3] = true;
    }
}

void IRAM_ATTR isrEchoF3L() {
    uint32_t now = micros();
    if (digitalRead(PIN_SLAVE_ECHO_F3_L) == HIGH) {
        echoStartUs[4] = now;
        echoFinished[4] = false;
    } else if (echoStartUs[4] > 0) {
        echoDurationUs[4] = now - echoStartUs[4];
        echoFinished[4] = true;
    }
}

void IRAM_ATTR isrEchoF3R() {
    uint32_t now = micros();
    if (digitalRead(PIN_SLAVE_ECHO_F3_R) == HIGH) {
        echoStartUs[5] = now;
        echoFinished[5] = false;
    } else if (echoStartUs[5] > 0) {
        echoDurationUs[5] = now - echoStartUs[5];
        echoFinished[5] = true;
    }
}

void triggerSimultaneous(uint8_t pin1, uint8_t pin2, uint8_t pin3) {
    digitalWrite(pin1, LOW);
    digitalWrite(pin2, LOW);
    digitalWrite(pin3, LOW);
    delayMicroseconds(4);
    digitalWrite(pin1, HIGH);
    digitalWrite(pin2, HIGH);
    digitalWrite(pin3, HIGH);
    delayMicroseconds(10);
    digitalWrite(pin1, LOW);
    digitalWrite(pin2, LOW);
    digitalWrite(pin3, LOW);
}

void setup() {
    // Nonaktifkan brownout agar lonjakan arus tidak mereset board
    esp_brownout_disable();
    WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

    // USB Serial untuk debugging monitor PC
    Serial.begin(115200);

    // Hardware Serial2 untuk komunikasi ke Master
    Serial2.begin(SLAVE_UART_BAUD, SERIAL_8N1, PIN_SLAVE_UART_RX, PIN_SLAVE_UART_TX);

    Serial.println("\n========================================================");
    Serial.println("   ESP32 SLAVE: DEDICATED ULTRASONIC SCANNER RUNNING");
    Serial.println("========================================================");
    Serial.printf("[INFO] UART to Master: TX=%d, RX=%d @ %lu baud\n",
                  PIN_SLAVE_UART_TX, PIN_SLAVE_UART_RX, SLAVE_UART_BAUD);

    // Inisialisasi Pin TRIG (Semua OUTPUT, set LOW)
    const uint8_t trigPins[6] = {
        PIN_SLAVE_TRIG_F1_L, PIN_SLAVE_TRIG_F1_R,
        PIN_SLAVE_TRIG_F2_L, PIN_SLAVE_TRIG_F2_R,
        PIN_SLAVE_TRIG_F3_L, PIN_SLAVE_TRIG_F3_R
    };
    for (int i = 0; i < 6; i++) {
        pinMode(trigPins[i], OUTPUT);
        digitalWrite(trigPins[i], LOW);
    }

    // Inisialisasi Pin ECHO (Semua INPUT) & Pasang ISR Edge Change
    pinMode(PIN_SLAVE_ECHO_F1_L, INPUT);
    pinMode(PIN_SLAVE_ECHO_F1_R, INPUT);
    pinMode(PIN_SLAVE_ECHO_F2_L, INPUT);
    pinMode(PIN_SLAVE_ECHO_F2_R, INPUT);
    pinMode(PIN_SLAVE_ECHO_F3_L, INPUT);
    pinMode(PIN_SLAVE_ECHO_F3_R, INPUT);

    attachInterrupt(digitalPinToInterrupt(PIN_SLAVE_ECHO_F1_L), isrEchoF1L, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_SLAVE_ECHO_F1_R), isrEchoF1R, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_SLAVE_ECHO_F2_L), isrEchoF2L, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_SLAVE_ECHO_F2_R), isrEchoF2R, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_SLAVE_ECHO_F3_L), isrEchoF3L, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_SLAVE_ECHO_F3_R), isrEchoF3R, CHANGE);

    Serial.println("[INFO] Semua 6 pin TRIG & ECHO independen berhasil dikonfigurasi.");
    Serial.println("[SIAP] Memulai pemindaian ultrasonik bergantian KIRI <-> KANAN...\n");

    currentPhase = PHASE_TRIGGER_LEFT;
    phaseStartTime = millis();
}

void loop() {
    unsigned long now = millis();

    switch (currentPhase) {
        case PHASE_TRIGGER_LEFT: {
            // Reset state echo untuk sisi KIRI (Lantai 1, 2, 3)
            echoStartUs[0] = 0; echoDurationUs[0] = 0; echoFinished[0] = false;
            echoStartUs[2] = 0; echoDurationUs[2] = 0; echoFinished[2] = false;
            echoStartUs[4] = 0; echoDurationUs[4] = 0; echoFinished[4] = false;

            // Tembakkan ketiga sensor Kiri secara bersamaan (simultan)
            triggerSimultaneous(PIN_SLAVE_TRIG_F1_L, PIN_SLAVE_TRIG_F2_L, PIN_SLAVE_TRIG_F3_L);

            phaseStartTime = now;
            currentPhase = PHASE_WAIT_LEFT;
            break;
        }

        case PHASE_WAIT_LEFT: {
            bool leftDone = echoFinished[0] && echoFinished[2] && echoFinished[4];
            if (leftDone || (now - phaseStartTime >= ULTRASONIC_TIMEOUT_MS)) {
                // Konversi durasi pulsa ke jarak cm untuk sisi Kiri
                const int leftIndices[3] = {0, 2, 4};
                for (int idx : leftIndices) {
                    if (echoFinished[idx] && echoDurationUs[idx] > 0) {
                        float dist = echoDurationUs[idx] / 58.2f;
                        currentDistances[idx] = (dist > 0.0f && dist <= 300.0f) ? dist : 0.0f;
                    } else {
                        currentDistances[idx] = 0.0f; // Timeout / di luar jangkauan
                    }
                }
                phaseStartTime = now;
                currentPhase = PHASE_DELAY_AFTER_LEFT;
            }
            break;
        }

        case PHASE_DELAY_AFTER_LEFT: {
            // Jeda agar pantulan suara sisi Kiri benar-benar hilang sebelum Kanan menembak
            if (now - phaseStartTime >= DELAY_BETWEEN_SIDES_MS) {
                currentPhase = PHASE_TRIGGER_RIGHT;
            }
            break;
        }

        case PHASE_TRIGGER_RIGHT: {
            // Reset state echo untuk sisi KANAN (Lantai 1, 2, 3)
            echoStartUs[1] = 0; echoDurationUs[1] = 0; echoFinished[1] = false;
            echoStartUs[3] = 0; echoDurationUs[3] = 0; echoFinished[3] = false;
            echoStartUs[5] = 0; echoDurationUs[5] = 0; echoFinished[5] = false;

            // Tembakkan ketiga sensor Kanan secara bersamaan (simultan)
            triggerSimultaneous(PIN_SLAVE_TRIG_F1_R, PIN_SLAVE_TRIG_F2_R, PIN_SLAVE_TRIG_F3_R);

            phaseStartTime = now;
            currentPhase = PHASE_WAIT_RIGHT;
            break;
        }

        case PHASE_WAIT_RIGHT: {
            bool rightDone = echoFinished[1] && echoFinished[3] && echoFinished[5];
            if (rightDone || (now - phaseStartTime >= ULTRASONIC_TIMEOUT_MS)) {
                // Konversi durasi pulsa ke jarak cm untuk sisi Kanan
                const int rightIndices[3] = {1, 3, 5};
                for (int idx : rightIndices) {
                    if (echoFinished[idx] && echoDurationUs[idx] > 0) {
                        float dist = echoDurationUs[idx] / 58.2f;
                        currentDistances[idx] = (dist > 0.0f && dist <= 300.0f) ? dist : 0.0f;
                    } else {
                        currentDistances[idx] = 0.0f; // Timeout / di luar jangkauan
                    }
                }
                phaseStartTime = now;
                currentPhase = PHASE_DELAY_AFTER_RIGHT;
            }
            break;
        }

        case PHASE_DELAY_AFTER_RIGHT: {
            // Jeda agar pantulan suara sisi Kanan reda sebelum paket dikirim & siklus diulang
            if (now - phaseStartTime >= DELAY_BETWEEN_SIDES_MS) {
                currentPhase = PHASE_SEND_DATA;
            }
            break;
        }

        case PHASE_SEND_DATA: {
            // Serialisasi data 6 sensor ke JSON dan kirim via UART Serial2 ke Master
            JsonDocument doc;
            doc["f1_l"] = roundf(currentDistances[0] * 10.0f) / 10.0f;
            doc["f1_r"] = roundf(currentDistances[1] * 10.0f) / 10.0f;
            doc["f2_l"] = roundf(currentDistances[2] * 10.0f) / 10.0f;
            doc["f2_r"] = roundf(currentDistances[3] * 10.0f) / 10.0f;
            doc["f3_l"] = roundf(currentDistances[4] * 10.0f) / 10.0f;
            doc["f3_r"] = roundf(currentDistances[5] * 10.0f) / 10.0f;

            serializeJson(doc, Serial2);
            Serial2.println(); // Terminator baris baru
            packetCount++;

            // Debug print berkala ke Serial Monitor USB (setiap 1 detik)
            if (now - lastDebugPrint >= 1000) {
                lastDebugPrint = now;
                Serial.printf("[SLAVE TX #%lu] L1=(%.1f, %.1f) | L2=(%.1f, %.1f) | L3=(%.1f, %.1f) cm\n",
                              packetCount,
                              currentDistances[0], currentDistances[1],
                              currentDistances[2], currentDistances[3],
                              currentDistances[4], currentDistances[5]);
            }

            // Langsung mulai siklus pemindaian berikutnya
            currentPhase = PHASE_TRIGGER_LEFT;
            break;
        }
    }
}
