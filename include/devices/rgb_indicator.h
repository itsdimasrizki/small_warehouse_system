#pragma once
#include <Arduino.h>
#include "types/system_state.h"
#include "config/pins.h"

struct RgbPins {
    uint8_t pinR;
    uint8_t pinG;
    uint8_t pinB;
};

class RgbIndicator {
public:
    static const uint8_t RACK_COUNT = 3;

    // Default true untuk Common Anode (+)
    RgbIndicator(bool commonAnode = true);

    // Inisialisasi seluruh pin PWM 3x LED RGB
    void begin();

    // Atur warna 1 rak tertentu (floorIndex: 0 = Rak 1, 1 = Rak 2, 2 = Rak 3)
    void setColor(uint8_t floorIndex, uint8_t red, uint8_t green, uint8_t blue);

    // Atur warna 1 rak dan sinkronkan ke SystemState
    void setColorAndSync(uint8_t floorIndex, uint8_t red, uint8_t green, uint8_t blue, SystemState &state);

    // Pintasan warna standar per rak
    void setGreen(uint8_t floorIndex);
    void setBlue(uint8_t floorIndex);
    void setRed(uint8_t floorIndex);
    void off(uint8_t floorIndex);

    // Kontrol seluruh rak serentak
    void setAllColor(uint8_t red, uint8_t green, uint8_t blue);
    void offAll();

    // Ubah tipe LED (Common Cathode vs Common Anode) jika warna terbalik
    void setCommonAnode(bool commonAnode) { isCommonAnode = commonAnode; }
    bool getCommonAnode() const { return isCommonAnode; }

private:
    bool isCommonAnode;
    RgbPins rackPins[RACK_COUNT];
    void writePwm(uint8_t pin, uint8_t value);
};
