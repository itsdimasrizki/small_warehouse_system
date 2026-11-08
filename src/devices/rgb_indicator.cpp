#include "devices/rgb_indicator.h"

RgbIndicator::RgbIndicator(bool commonAnode)
    : isCommonAnode(commonAnode),
      rackPins{
          {PIN_RGB1_RED, PIN_RGB1_GREEN, PIN_RGB1_BLUE},
          {PIN_RGB2_RED, PIN_RGB2_GREEN, PIN_RGB2_BLUE},
          {PIN_RGB3_RED, PIN_RGB3_GREEN, PIN_RGB3_BLUE}
      }
{
}

void RgbIndicator::begin() {
    for (uint8_t i = 0; i < RACK_COUNT; i++) {
        pinMode(rackPins[i].pinR, OUTPUT);
        pinMode(rackPins[i].pinG, OUTPUT);
        pinMode(rackPins[i].pinB, OUTPUT);
    }
    offAll();
}

void RgbIndicator::writePwm(uint8_t pin, uint8_t value) {
    // Gunakan kontrol digital penuh (0V / 3.3V)
    // Common Cathode (-): Kaki panjang ke GND -> HIGH = ON, LOW = OFF
    // Common Anode (+):   Kaki panjang ke 3.3V -> LOW = ON, HIGH = OFF
    if (isCommonAnode) {
        digitalWrite(pin, (value > 127) ? LOW : HIGH);
    } else {
        digitalWrite(pin, (value > 127) ? HIGH : LOW);
    }
}

void RgbIndicator::setColor(uint8_t floorIndex, uint8_t red, uint8_t green, uint8_t blue) {
    if (floorIndex >= RACK_COUNT) return;
    writePwm(rackPins[floorIndex].pinR, red);
    writePwm(rackPins[floorIndex].pinG, green);
    writePwm(rackPins[floorIndex].pinB, blue);
}

void RgbIndicator::setColorAndSync(uint8_t floorIndex, uint8_t red, uint8_t green, uint8_t blue, SystemState &state) {
    setColor(floorIndex, red, green, blue);
    if (floorIndex < RACK_COUNT) {
        state.floors[floorIndex].rgb_r = red;
        state.floors[floorIndex].rgb_g = green;
        state.floors[floorIndex].rgb_b = blue;
    }
}

void RgbIndicator::setGreen(uint8_t floorIndex) {
    setColor(floorIndex, 0, 255, 0);
}

void RgbIndicator::setBlue(uint8_t floorIndex) {
    setColor(floorIndex, 0, 0, 255);
}

void RgbIndicator::setRed(uint8_t floorIndex) {
    setColor(floorIndex, 255, 0, 0);
}

void RgbIndicator::off(uint8_t floorIndex) {
    setColor(floorIndex, 0, 0, 0);
}

void RgbIndicator::setAllColor(uint8_t red, uint8_t green, uint8_t blue) {
    for (uint8_t i = 0; i < RACK_COUNT; i++) {
        setColor(i, red, green, blue);
    }
}

void RgbIndicator::offAll() {
    for (uint8_t i = 0; i < RACK_COUNT; i++) {
        off(i);
    }
}
