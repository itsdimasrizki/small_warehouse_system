#include "fsm/safety_fsm.h"

SafetyFSM::SafetyFSM(AlarmBuzzer &alarmBuzzer)
    : buzzer(alarmBuzzer),
      currentState(STATE_SAFE_NORMAL),
      lastStatePtr(nullptr),
      lastPulseMillis(0),
      pulseActive(false)
{
}

void SafetyFSM::begin(SystemState *initialState) {
    currentState = STATE_SAFE_NORMAL;
    lastStatePtr = initialState;
    lastPulseMillis = 0;
    pulseActive = false;
    buzzer.off();
    if (initialState) {
        initialState->safety_state = STATE_SAFE_NORMAL;
        initialState->buzzer_active = false;
    }
}

void SafetyFSM::update(SystemState &state) {
    lastStatePtr = &state;

    // Evaluasi ambang batas bahaya (suhu >= 38.0 C atau asap ADC >= 1800)
    bool hazard = state.hazard_detected || (state.temperature_c >= TEMP_HAZARD_THRESHOLD_C) || (state.smoke_adc >= MQ2_HAZARD_THRESHOLD_ADC);
    state.hazard_detected = hazard;

    unsigned long now = millis();

    switch (currentState) {
        case STATE_SAFE_NORMAL:
            if (hazard) {
                // Transisi dari Normal -> Alarm Triggered
                currentState = STATE_ALARM_TRIGGERED;
                pulseActive = true;
                lastPulseMillis = now;
                buzzer.setStateAndSync(true, state);
            } else {
                if (buzzer.isActive()) {
                    buzzer.setStateAndSync(false, state);
                }
            }
            break;

        case STATE_ALARM_TRIGGERED:
            if (!hazard) {
                // Bahaya reda secara alami -> Kembali ke status Normal
                currentState = STATE_SAFE_NORMAL;
                pulseActive = false;
                buzzer.setStateAndSync(false, state);
            } else {
                // Pola Pulsed Beep Non-blocking (200ms ON / 200ms OFF)
                unsigned long interval = pulseActive ? BUZZER_ALARM_PULSE_ON_MS : BUZZER_ALARM_PULSE_OFF_MS;
                if (now - lastPulseMillis >= interval) {
                    lastPulseMillis = now;
                    pulseActive = !pulseActive;
                    buzzer.setStateAndSync(pulseActive, state);
                }
            }
            break;

        case STATE_ALARM_SILENCED:
            // Buzzer wajib nonaktif selama state SILENCED
            pulseActive = false;
            if (buzzer.isActive()) {
                buzzer.setStateAndSync(false, state);
            }
            // Auto-reset: jika bahaya sudah reda, kembali otomatis ke SAFE_NORMAL
            if (!hazard) {
                currentState = STATE_SAFE_NORMAL;
            }
            break;

        case STATE_MANUAL_TEST:
            // Mode pengujian manual: Buzzer dinyalakan konstan
            pulseActive = true;
            if (!buzzer.isActive()) {
                buzzer.setStateAndSync(true, state);
            }
            break;
    }

    state.safety_state = currentState;
    state.buzzer_active = buzzer.isActive();
}

void SafetyFSM::handleMqttCommand(const String &cmd, SystemState &state) {
    lastStatePtr = &state;
    processCommand(cmd, &state);
}

void SafetyFSM::handleMqttCommand(const String &cmd) {
    processCommand(cmd, lastStatePtr);
}

void SafetyFSM::processCommand(String cleanCmd, SystemState *state) {
    cleanCmd.trim();
    cleanCmd.toUpperCase();

    if (cleanCmd == "MUTE" || cleanCmd == "OFF") {
        if (currentState == STATE_ALARM_TRIGGERED) {
            currentState = STATE_ALARM_SILENCED;
            pulseActive = false;
            buzzer.off();
            if (state) {
                state->safety_state = STATE_ALARM_SILENCED;
                state->buzzer_active = false;
            }
        } else if (currentState == STATE_MANUAL_TEST) {
            currentState = STATE_SAFE_NORMAL;
            pulseActive = false;
            buzzer.off();
            if (state) {
                state->safety_state = STATE_SAFE_NORMAL;
                state->buzzer_active = false;
            }
        }
    } else if (cleanCmd == "ON") {
        currentState = STATE_MANUAL_TEST;
        pulseActive = true;
        buzzer.on();
        if (state) {
            state->safety_state = STATE_MANUAL_TEST;
            state->buzzer_active = true;
        }
    } else if (cleanCmd == "AUTO") {
        bool hazard = (state && state->hazard_detected);
        if (hazard) {
            currentState = STATE_ALARM_TRIGGERED;
            pulseActive = true;
            lastPulseMillis = millis();
            buzzer.on();
            if (state) {
                state->safety_state = STATE_ALARM_TRIGGERED;
                state->buzzer_active = true;
            }
        } else {
            currentState = STATE_SAFE_NORMAL;
            pulseActive = false;
            buzzer.off();
            if (state) {
                state->safety_state = STATE_SAFE_NORMAL;
                state->buzzer_active = false;
            }
        }
    }
}

const char* SafetyFSM::getStateName(SafetyState state) {
    switch (state) {
        case STATE_SAFE_NORMAL:      return "SAFE_NORMAL (AMAN)";
        case STATE_ALARM_TRIGGERED:  return "ALARM_TRIGGERED (BAHAYA!)";
        case STATE_ALARM_SILENCED:   return "ALARM_SILENCED (DIMUTE)";
        case STATE_MANUAL_TEST:      return "MANUAL_TEST (UJI COBA)";
        default:                     return "UNKNOWN";
    }
}
