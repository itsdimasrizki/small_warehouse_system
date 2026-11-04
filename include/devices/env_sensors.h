#pragma once
#include <Arduino.h>
#include <DHT.h>
#include "types/system_state.h"
#include "config/pins.h"
#include "config/thresholds.h"

// ==============================================================================
// Driver: Sensor Lingkungan & Keamanan (DHT11 & MQ2 Smoke/Gas)
// ==============================================================================

class EnvSensors {
public:
    EnvSensors();

    // Inisialisasi pin dan sensor DHT11
    void begin();

    // Membaca suhu, kelembapan, dan ADC asap MQ2, lalu mengupdate SystemState
    // Mengembalikan true jika pembacaan DHT11 berhasil (bukan NaN)
    bool readSensors(SystemState &state);

    // Pembacaan langsung untuk testing
    float readTemperature();
    float readHumidity();
    int readSmokeRaw();

private:
    DHT dht;
};
