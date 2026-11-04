#include "devices/alarm_buzzer.h"

AlarmBuzzer::AlarmBuzzer()
    : currentState(false)
{
}

void AlarmBuzzer::begin() {
    pinMode(PIN_BUZZER, OUTPUT);
    off();
}

void AlarmBuzzer::setState(bool active) {
    currentState = active;
    digitalWrite(PIN_BUZZER, active ? HIGH : LOW);
}

void AlarmBuzzer::setStateAndSync(bool active, SystemState &state) {
    setState(active);
    state.buzzer_active = active;
}

void AlarmBuzzer::on() {
    setState(true);
}

void AlarmBuzzer::off() {
    setState(false);
}

void AlarmBuzzer::beep(uint16_t durationMs) {
    on();
    delay(durationMs);
    off();
}
