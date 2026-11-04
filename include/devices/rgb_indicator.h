#pragma once
#include <Arduino.h>
#include "types/system_state.h"
#include "config/pins.h"

// ==============================================================================
// Driver: LED RGB Indikator Status Gudang
// Mendukung tipe Common Cathode (default) dan Common Anode
// ==============================================================================

class RgbIndicator {
public:
    RgbIndicator(bool commonAnode = false);

    // Inisialisasi pin PWM LED RGB
    void begin();

    // Atur warna langsung berdasarkan nilai RGB (0 - 255)
    void setColor(uint8_t red, uint8_t green, uint8_t blue);

    // Set warna dan simpan ke SystemState
    void setColorAndSync(uint8_t red, uint8_t green, uint8_t blue, SystemState &state);

    // Pintasan warna standar
    void setGreen();
    void setBlue();
    void setRed();
    void off();

    // Ubah tipe LED (Common Cathode vs Common Anode) jika warna terbalik
    void setCommonAnode(bool commonAnode) { isCommonAnode = commonAnode; }

private:
    bool isCommonAnode;
    void writePwm(uint8_t pin, uint8_t value);
};
