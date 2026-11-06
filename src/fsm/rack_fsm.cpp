#include "fsm/rack_fsm.h"

RackFSM::RackFSM(RgbIndicator &rgbIndicator, const float* customThresholds)
    : rgb(rgbIndicator),
      thresholds(customThresholds ? customThresholds : RACK_THRESHOLDS_CM)
{
}

void RackFSM::begin() {
    // Inisialisasi awal FSM Rak
}

void RackFSM::update(SystemState &state) {
    int occupiedCount = 0;

    for (uint8_t i = 0; i < FLOOR_COUNT; i++) {
        float dL = state.floors[i].distance_left_cm;
        float dR = state.floors[i].distance_right_cm;
        float currentThreshold = thresholds[i];

        bool leftDetected  = (dL > 0.0f && dL < currentThreshold);
        bool rightDetected = (dR > 0.0f && dR < currentThreshold);

        int detectedSides = (leftDetected ? 1 : 0) + (rightDetected ? 1 : 0);
        state.floors[i].detected_sides = detectedSides;
        state.floors[i].is_occupied = (detectedSides > 0);

        if (state.floors[i].is_occupied) {
            occupiedCount++;
        }

        // Logika 3 Status Warna per Rak (Instant evaluation):
        // 0 sisi deteksi = Kosong          -> HIJAU (0, 255, 0)
        // 1 sisi deteksi = Terisi Sebagian -> BIRU  (0, 0, 255)
        // 2 sisi deteksi = Terisi Penuh    -> MERAH (255, 0, 0)
        if (detectedSides == 0) {
            state.floors[i].rack_state = STATE_RACK_EMPTY;
            rgb.setColorAndSync(i, 0, 255, 0, state);
        } else if (detectedSides == 1) {
            state.floors[i].rack_state = STATE_RACK_PARTIAL;
            rgb.setColorAndSync(i, 0, 0, 255, state);
        } else {
            state.floors[i].rack_state = STATE_RACK_FULL;
            rgb.setColorAndSync(i, 255, 0, 0, state);
        }
    }

    state.occupied_floor_count = occupiedCount;

    // Agregasi status kapasitas global gudang
    if (occupiedCount == 0) {
        state.rack_state = STATE_RACK_EMPTY;
    } else if (occupiedCount == FLOOR_COUNT) {
        state.rack_state = STATE_RACK_FULL;
    } else {
        state.rack_state = STATE_RACK_PARTIAL;
    }
}

float RackFSM::getThreshold(uint8_t floorIndex) const {
    if (floorIndex < FLOOR_COUNT) {
        return thresholds[floorIndex];
    }
    return 0.0f;
}

const char* RackFSM::getStateName(RackState state) {
    switch (state) {
        case STATE_RACK_EMPTY:   return "EMPTY (KOSONG)";
        case STATE_RACK_PARTIAL: return "PARTIAL (SEBAGIAN)";
        case STATE_RACK_FULL:    return "FULL (PENUH)";
        default:                 return "UNKNOWN";
    }
}
