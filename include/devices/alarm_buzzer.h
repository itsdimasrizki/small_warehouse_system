#pragma once
#include <Arduino.h>
#include "types/system_state.h"
#include "config/pins.h"

class AlarmBuzzer {
public:
    AlarmBuzzer();

    void begin();

    // Mengatur status nyala (true) atau mati (false)
    void setState(bool active);

    // Mengatur status dan menyinkronkan dengan SystemState
    void setStateAndSync(bool active, SystemState &state);

    void on();
    void off();
    void beep(uint16_t durationMs = 100);

    bool isActive() const { return currentState; }

private:
    bool currentState;
};
