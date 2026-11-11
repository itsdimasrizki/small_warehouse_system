#pragma once
#include <Arduino.h>
#include <DHT.h>
#include "types/system_state.h"
#include "config/pins.h"
#include "config/thresholds.h"

class EnvSensors {
public:
    EnvSensors();
    void begin();

    // Membaca suhu, kelembapan, dan ADC asap MQ2, lalu mengupdate SystemState
    bool readSensors(SystemState &state);

    float readTemperature();
    float readHumidity();
    int readSmokeRaw();

private:
    DHT dht;
    bool simInitialized;
    float simTempC;
    float simHumidityPct;

    void updateSimulatedValues(int smokeAdc);
};
