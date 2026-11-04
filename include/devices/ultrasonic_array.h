#pragma once
#include <Arduino.h>
#include <NewPing.h>
#include "types/system_state.h"
#include "config/pins.h"
#include "config/thresholds.h"

// ==============================================================================
// Driver: Array 6 Sensor Ultrasonik HC-SR04
// Sequential Polling Non-Blocking untuk Menghindari Cross-Talk
// ==============================================================================

class UltrasonicArray {
public:
    static const uint8_t SENSOR_COUNT = 6;

    UltrasonicArray();

    // Inisialisasi pin (jika diperlukan)
    void begin();

    // Membaca 1 sensor berikutnya secara bergiliran dan memperbarui SystemState
    // Mengembalikan indeks sensor yang baru saja dibaca (0 - 5)
    uint8_t pollNextSensor(SystemState &state);

    // Membaca langsung sensor tertentu (0 = F1L, 1 = F1R, 2 = F2L, 3 = F2R, 4 = F3L, 5 = F3R)
    float readDirect(uint8_t sensorIndex);

    // Mendapatkan nama label sensor untuk kemudahan debugging
    const char* getSensorName(uint8_t sensorIndex) const;

    // Mendapatkan indeks sensor saat ini
    uint8_t getCurrentIndex() const { return currentIndex; }

private:
    uint8_t currentIndex;
    NewPing sensors[SENSOR_COUNT];
};
