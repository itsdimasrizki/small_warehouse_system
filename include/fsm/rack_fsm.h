#pragma once
#include <Arduino.h>
#include "types/system_state.h"
#include "config/thresholds.h"
#include "devices/rgb_indicator.h"

class RackFSM {
public:
    static const uint8_t FLOOR_COUNT = 3;
    // Konstruktor menerima referensi driver RgbIndicator dan opsional pointer array threshold per lantai
    explicit RackFSM(RgbIndicator &rgbIndicator, const float* customThresholds = nullptr);

    // Inisialisasi awal FSM Rak
    void begin();

    // Evaluasi seluruh lantai rak berdasarkan data sensor di SystemState
    // Memperbarui state per lantai, warna LED RGB per lantai, dan agregasi total sistem
    void update(SystemState &state);

    // Mendapatkan ambang batas deteksi untuk lantai tertentu (0 = Lantai 1, dst)
    float getThreshold(uint8_t floorIndex) const;

    // Helper konversi enum RackState ke string teks
    static const char* getStateName(RackState state);

private:
    RgbIndicator &rgb;
    const float* thresholds;
};
