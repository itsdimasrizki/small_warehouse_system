#include "devices/env_sensors.h"

EnvSensors::EnvSensors()
    : dht(PIN_DHT_DATA, DHT11)
{
}

void EnvSensors::begin() {
    dht.begin();
    pinMode(PIN_MQ2_ANALOG, INPUT);
    // ESP32 default ADC resolution adalah 12-bit (0 - 4095)
    analogReadResolution(12);
}

bool EnvSensors::readSensors(SystemState &state) {
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    int smoke = analogRead(PIN_MQ2_ANALOG);

    bool dhtSuccess = true;
    if (isnan(temp) || isnan(hum)) {
        dhtSuccess = false;
        // Pertahankan nilai lama jika ada kegagalan sesaat membaca DHT11
    } else {
        state.temperature_c = temp;
        state.humidity_pct = hum;
    }

    state.smoke_adc = smoke;
    state.smoke_pct = (static_cast<float>(smoke) / 4095.0f) * 100.0f;
    // Evaluasi kondisi bahaya berdasarkan ambang batas
    state.hazard_detected = (state.temperature_c >= TEMP_HAZARD_THRESHOLD_C) ||
                            (state.smoke_adc >= MQ2_HAZARD_THRESHOLD_ADC);

    return dhtSuccess;
}

float EnvSensors::readTemperature() {
    return dht.readTemperature();
}

float EnvSensors::readHumidity() {
    return dht.readHumidity();
}

int EnvSensors::readSmokeRaw() {
    return analogRead(PIN_MQ2_ANALOG);
}
