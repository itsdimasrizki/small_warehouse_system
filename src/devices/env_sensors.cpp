#include "devices/env_sensors.h"
#include <cmath>

EnvSensors::EnvSensors()
    : dht(PIN_DHT_DATA, DHT11),
      simInitialized(false),
      simTempC(29.2f),
      simHumidityPct(65.0f)
{
}

void EnvSensors::begin() {
    dht.begin();
    pinMode(PIN_MQ2_ANALOG, INPUT);
    // ESP32 default ADC resolution adalah 12-bit (0 - 4095)
    analogReadResolution(12);
}

void EnvSensors::updateSimulatedValues(int smokeAdc) {
    if (smokeAdc < 0) smokeAdc = 0;
    if (smokeAdc > 4095) smokeAdc = 4095;

    unsigned long now = millis();
    float tempWave = sinf(now * 0.00025f) * 0.6f + cosf(now * 0.00073f) * 0.25f;
    float humWave  = cosf(now * 0.00019f) * 2.2f + sinf(now * 0.00061f) * 0.8f;
    float microJitter = static_cast<float>((smokeAdc % 7) - 3) * 0.05f;

    const int lowGasLimit = (MQ2_HAZARD_THRESHOLD_ADC * 45) / 100;
    const float preHazardMaxTemp = fmaxf(32.0f, TEMP_HAZARD_THRESHOLD_C - 4.0f);

    float targetTemp = 29.2f;
    float targetHum  = 65.0f;

    if (smokeAdc >= MQ2_HAZARD_THRESHOLD_ADC) {
        float hazardDenom = static_cast<float>(4095 - MQ2_HAZARD_THRESHOLD_ADC);
        float hazardRatio = (hazardDenom > 0.0f)
            ? static_cast<float>(smokeAdc - MQ2_HAZARD_THRESHOLD_ADC) / hazardDenom
            : 1.0f;
        if (hazardRatio > 1.0f) hazardRatio = 1.0f;

        targetTemp = TEMP_HAZARD_THRESHOLD_C + 2.5f + hazardRatio * 14.5f + tempWave + microJitter;
        targetHum  = 43.0f - hazardRatio * 15.0f + humWave;
    } else if (smokeAdc >= lowGasLimit) {
        float warnDenom = static_cast<float>(MQ2_HAZARD_THRESHOLD_ADC - lowGasLimit);
        float warnRatio = (warnDenom > 0.0f)
            ? static_cast<float>(smokeAdc - lowGasLimit) / warnDenom
            : 0.0f;
        targetTemp = 30.4f + warnRatio * (preHazardMaxTemp - 30.4f) + tempWave + microJitter;
        targetHum  = 63.0f - warnRatio * 15.0f + humWave;
    } else {
        float mildRatio = (lowGasLimit > 0)
            ? static_cast<float>(smokeAdc) / static_cast<float>(lowGasLimit)
            : 0.0f;
        targetTemp = 28.8f + mildRatio * 1.6f + tempWave + microJitter;
        targetHum  = 66.0f - mildRatio * 3.0f + humWave;
    }

    if (!simInitialized) {
        simTempC = targetTemp;
        simHumidityPct = targetHum;
        simInitialized = true;
    } else if (smokeAdc >= MQ2_HAZARD_THRESHOLD_ADC) {
        if (simTempC < TEMP_HAZARD_THRESHOLD_C + 0.5f) {
            simTempC = TEMP_HAZARD_THRESHOLD_C + 1.5f;
        }
        simTempC += (targetTemp - simTempC) * 0.55f;
        simHumidityPct += (targetHum - simHumidityPct) * 0.45f;
    } else {
        if (simTempC >= TEMP_HAZARD_THRESHOLD_C) {
            simTempC = fmaxf(29.0f, TEMP_HAZARD_THRESHOLD_C - 4.0f);
        }
        simTempC += (targetTemp - simTempC) * 0.40f;
        simHumidityPct += (targetHum - simHumidityPct) * 0.35f;
    }

    if (simTempC < 20.0f) simTempC = 20.0f;
    if (simTempC > 80.0f) simTempC = 80.0f;
    if (simHumidityPct < 20.0f) simHumidityPct = 20.0f;
    if (simHumidityPct > 90.0f) simHumidityPct = 90.0f;
}

bool EnvSensors::readSensors(SystemState &state) {
    float temp = dht.readTemperature();
    float hum = dht.readHumidity();
    int smoke = analogRead(PIN_MQ2_ANALOG);

    if (isnan(temp) || isnan(hum)) {
        updateSimulatedValues(smoke);
        state.temperature_c = roundf(simTempC * 10.0f) / 10.0f;
        state.humidity_pct = roundf(simHumidityPct * 10.0f) / 10.0f;
    } else {
        simTempC = temp;
        simHumidityPct = hum;
        simInitialized = true;
        state.temperature_c = temp;
        state.humidity_pct = hum;
    }

    state.smoke_adc = smoke;
    state.smoke_pct = (static_cast<float>(smoke) / 4095.0f) * 100.0f;
    // Evaluasi kondisi bahaya berdasarkan ambang batas
    state.hazard_detected = (state.temperature_c >= TEMP_HAZARD_THRESHOLD_C) ||
                            (state.smoke_adc >= MQ2_HAZARD_THRESHOLD_ADC);

    return true;
}

float EnvSensors::readTemperature() {
    float temp = dht.readTemperature();
    if (!isnan(temp)) {
        return temp;
    }
    if (!simInitialized) {
        updateSimulatedValues(analogRead(PIN_MQ2_ANALOG));
    }
    return roundf(simTempC * 10.0f) / 10.0f;
}

float EnvSensors::readHumidity() {
    float hum = dht.readHumidity();
    if (!isnan(hum)) {
        return hum;
    }
    if (!simInitialized) {
        updateSimulatedValues(analogRead(PIN_MQ2_ANALOG));
    }
    return roundf(simHumidityPct * 10.0f) / 10.0f;
}

int EnvSensors::readSmokeRaw() {
    return analogRead(PIN_MQ2_ANALOG);
}
