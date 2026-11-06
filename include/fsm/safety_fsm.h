#pragma once
#include <Arduino.h>
#include "types/system_state.h"
#include "config/thresholds.h"
#include "devices/alarm_buzzer.h"

class SafetyFSM {
public:
    // Konstruktor menerima referensi driver AlarmBuzzer
    explicit SafetyFSM(AlarmBuzzer &alarmBuzzer);

    // Inisialisasi awal FSM Keamanan
    void begin(SystemState *initialState = nullptr);

    // Evaluasi kondisi bahaya (suhu & asap) dan jalankan siklus FSM
    void update(SystemState &state);

    // Memproses perintah kontrol (MUTE, OFF, ON, AUTO) dari MQTT atau Serial
    void handleMqttCommand(const String &cmd, SystemState &state);
    void handleMqttCommand(const String &cmd);

    // Getters status FSM
    SafetyState getState() const { return currentState; }
    static const char* getStateName(SafetyState state);

private:
    AlarmBuzzer &buzzer;
    SafetyState currentState;
    SystemState *lastStatePtr;

    // Timer dan status pulsa non-blocking buzzer saat alarm aktif
    unsigned long lastPulseMillis;
    bool pulseActive;

    void processCommand(String cleanCmd, SystemState *state);
};
