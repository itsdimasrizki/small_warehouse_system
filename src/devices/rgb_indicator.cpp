#include "devices/rgb_indicator.h"

RgbIndicator::RgbIndicator(bool commonAnode)
    : isCommonAnode(commonAnode)
{
}

void RgbIndicator::begin() {
    pinMode(PIN_RGB_RED, OUTPUT);
    pinMode(PIN_RGB_GREEN, OUTPUT);
    pinMode(PIN_RGB_BLUE, OUTPUT);
    off();
}

void RgbIndicator::writePwm(uint8_t pin, uint8_t value) {
    uint8_t effectiveValue = isCommonAnode ? (255 - value) : value;
    analogWrite(pin, effectiveValue);
}

void RgbIndicator::setColor(uint8_t red, uint8_t green, uint8_t blue) {
    writePwm(PIN_RGB_RED, red);
    writePwm(PIN_RGB_GREEN, green);
    writePwm(PIN_RGB_BLUE, blue);
}

void RgbIndicator::setColorAndSync(uint8_t red, uint8_t green, uint8_t blue, SystemState &state) {
    setColor(red, green, blue);
    state.rgb_r = red;
    state.rgb_g = green;
    state.rgb_b = blue;
}

void RgbIndicator::setGreen() {
    setColor(0, 255, 0);
}

void RgbIndicator::setBlue() {
    setColor(0, 0, 255);
}

void RgbIndicator::setRed() {
    setColor(255, 0, 0);
}

void RgbIndicator::off() {
    setColor(0, 0, 0);
}
