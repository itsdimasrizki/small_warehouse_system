#pragma once
#include <Arduino.h>
#include "types/system_state.h"
#include "config/thresholds.h"
#include "devices/rgb_indicator.h"

class RackFSM {
public:
    static const uint8_t FLOOR_COUNT = 3;
    // Konstruktor menerima referensi driver RgbIndicator dan opsional pointer array threshold per lantai
    explicit RackFSM(RgbIndicator &rgbIndicator, const float* customThresholds = nullptr);
    void begin();

    void update(SystemState &state);
    void applyRgbColors(SystemState &state);

    float getThreshold(uint8_t floorIndex) const;

    static const char* getStateName(RackState state);

private:
    RgbIndicator &rgb;
    const float* thresholds;
};
