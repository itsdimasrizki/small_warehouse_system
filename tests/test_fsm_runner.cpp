#include <cassert>
#include <iostream>
#include "types/system_state.h"
#include "config/thresholds.h"
#include "devices/rgb_indicator.h"
#include "devices/alarm_buzzer.h"
#include "fsm/rack_fsm.h"
#include "fsm/safety_fsm.h"

int main() {
    std::cout << "=== RUNNING FSM INTEGRATION & LOGIC TESTS ===" << std::endl;

    // 1. Inisialisasi Driver & FSM
    RgbIndicator rgb(true);
    AlarmBuzzer buzzer;
    RackFSM rackFsm(rgb);
    SafetyFSM safetyFsm(buzzer);

    SystemState state;
    memset(&state, 0, sizeof(SystemState));

    rackFsm.begin();
    safetyFsm.begin(&state);

    assert(state.rack_state == STATE_RACK_EMPTY);
    assert(state.safety_state == STATE_SAFE_NORMAL);
    assert(!state.buzzer_active);
    std::cout << "[PASS] Inisialisasi awal berhasil." << std::endl;

    // 2. Test RackFSM dengan ukuran threshold rak yang berbeda
    // Di thresholds.h: Lantai 1: 35cm, Lantai 2: 35cm, Lantai 3: 35cm
    std::cout << "Rack Thresholds: F1=" << rackFsm.getThreshold(0)
              << "cm, F2=" << rackFsm.getThreshold(1)
              << "cm, F3=" << rackFsm.getThreshold(2) << "cm" << std::endl;

    // Skenario A: Semua kosong (jarak > threshold)
    state.floors[0].distance_left_cm = 50.0f;
    state.floors[0].distance_right_cm = 50.0f;
    state.floors[1].distance_left_cm = 40.0f;
    state.floors[1].distance_right_cm = 40.0f;
    state.floors[2].distance_left_cm = 60.0f;
    state.floors[2].distance_right_cm = 60.0f;

    rackFsm.update(state);

    assert(state.floors[0].rack_state == STATE_RACK_EMPTY);
    assert(state.floors[0].rgb_g == 255); // Hijau
    assert(state.floors[1].rack_state == STATE_RACK_EMPTY);
    assert(state.floors[2].rack_state == STATE_RACK_EMPTY);
    assert(state.occupied_floor_count == 0);
    assert(state.rack_state == STATE_RACK_EMPTY);
    std::cout << "[PASS] Skenario A: Semua rak kosong -> STATE_RACK_EMPTY (Hijau)." << std::endl;

    // Skenario B: Lantai 1 terisi 1 sisi (Partial -> Biru)
    state.floors[0].distance_left_cm = 20.0f; // < 35cm
    state.floors[0].distance_right_cm = 50.0f;

    rackFsm.update(state);

    assert(state.floors[0].rack_state == STATE_RACK_PARTIAL);
    assert(state.floors[0].detected_sides == 1);
    assert(state.floors[0].rgb_b == 255); // Biru
    assert(state.occupied_floor_count == 1);
    assert(state.rack_state == STATE_RACK_PARTIAL);
    std::cout << "[PASS] Skenario B: Lantai 1 terisi 1 sisi -> STATE_RACK_PARTIAL (Biru)." << std::endl;

    // Skenario C: Lantai 1 terisi 2 sisi (Full -> Merah)
    state.floors[0].distance_right_cm = 15.0f; // < 35cm

    rackFsm.update(state);

    assert(state.floors[0].rack_state == STATE_RACK_FULL);
    assert(state.floors[0].detected_sides == 2);
    assert(state.floors[0].rgb_r == 255); // Merah
    std::cout << "[PASS] Skenario C: Lantai 1 terisi 2 sisi -> STATE_RACK_FULL (Merah)." << std::endl;

    // Skenario D: Semua lantai terisi -> Global STATE_RACK_FULL
    state.floors[1].distance_left_cm = 10.0f;
    state.floors[2].distance_right_cm = 12.0f;

    rackFsm.update(state);

    assert(state.occupied_floor_count == 3);
    assert(state.rack_state == STATE_RACK_FULL);
    std::cout << "[PASS] Skenario D: 3 lantai terisi -> Global STATE_RACK_FULL." << std::endl;
    // Skenario D2: Ukuran rak berbeda di tiap lantai (F1=35cm, F2=25cm, F3=15cm)
    const float differentThresholds[3] = {35.0f, 25.0f, 15.0f};
    RackFSM customRackFsm(rgb, differentThresholds);
    assert(customRackFsm.getThreshold(0) == 35.0f);
    assert(customRackFsm.getThreshold(1) == 25.0f);
    assert(customRackFsm.getThreshold(2) == 15.0f);

    // Beri jarak objek yang sama (20.0 cm) di sisi kiri pada ketiga lantai:
    state.floors[0].distance_left_cm = 20.0f; state.floors[0].distance_right_cm = 50.0f; // 20 < 35 -> Terisi
    state.floors[1].distance_left_cm = 20.0f; state.floors[1].distance_right_cm = 50.0f; // 20 < 25 -> Terisi
    state.floors[2].distance_left_cm = 20.0f; state.floors[2].distance_right_cm = 50.0f; // 20 > 15 -> Kosong!

    customRackFsm.update(state);

    assert(state.floors[0].is_occupied == true);
    assert(state.floors[0].rack_state == STATE_RACK_PARTIAL);
    assert(state.floors[1].is_occupied == true);
    assert(state.floors[1].rack_state == STATE_RACK_PARTIAL);
    assert(state.floors[2].is_occupied == false);
    assert(state.floors[2].rack_state == STATE_RACK_EMPTY);
    assert(state.occupied_floor_count == 2);
    assert(state.rack_state == STATE_RACK_PARTIAL);
    std::cout << "[PASS] Skenario D2: Evaluasi threshold berbeda per rak (F1=35, F2=25, F3=15) berhasil." << std::endl;

    // 3. Test SafetyFSM
    // Skenario E: Normal state
    state.temperature_c = 28.0f;
    state.smoke_adc = 400;
    state.hazard_detected = false;

    safetyFsm.update(state);
    assert(state.safety_state == STATE_SAFE_NORMAL);
    assert(!state.buzzer_active);
    std::cout << "[PASS] Skenario E: Kondisi lingkungan normal -> STATE_SAFE_NORMAL (Buzzer OFF)." << std::endl;

    // Skenario F: Hazard terdeteksi (Suhu tinggi) -> Alarm Triggered & Buzzer Beep Pulsed
    mock_millis = 1000;
    state.temperature_c = 40.5f; // >= 38.0 C
    state.hazard_detected = true;

    safetyFsm.update(state);
    assert(state.safety_state == STATE_ALARM_TRIGGERED);
    assert(state.buzzer_active == true); // Fase pertama ON
    // Uji pulsa: setelah 200ms -> harus OFF
    mock_millis = 1205;
    safetyFsm.update(state);
    assert(state.buzzer_active == false); // Fase kedua OFF

    // Setelah 200ms lagi -> harus kembali ON
    mock_millis = 1410;
    safetyFsm.update(state);
    assert(state.buzzer_active == true); // Fase ketiga ON
    std::cout << "[PASS] Skenario F: Bahaya suhu memicu ALARM_TRIGGERED dengan non-blocking pulse buzzer." << std::endl;

    // Skenario G: Operator mengirim MUTE
    safetyFsm.handleMqttCommand("MUTE", state);
    assert(state.safety_state == STATE_ALARM_SILENCED);
    assert(state.buzzer_active == false);

    // Pastikan di loop berikutnya buzzer tetap mati meskipun suhu masih tinggi
    mock_millis = 2000;
    safetyFsm.update(state);
    assert(state.safety_state == STATE_ALARM_SILENCED);
    assert(state.buzzer_active == false);
    std::cout << "[PASS] Skenario G: Perintah MUTE berhasil mendiamkan buzzer (STATE_ALARM_SILENCED)." << std::endl;

    // Skenario H: Bahaya reda saat di state SILENCED -> Auto-reset ke STATE_SAFE_NORMAL
    state.temperature_c = 27.0f;
    state.smoke_adc = 300;
    state.hazard_detected = false;

    safetyFsm.update(state);
    assert(state.safety_state == STATE_SAFE_NORMAL);
    assert(state.buzzer_active == false);
    std::cout << "[PASS] Skenario H: Auto-reset saat bahaya reda berhasil kembali ke STATE_SAFE_NORMAL." << std::endl;

    // Skenario I: Perintah manual ON dan OFF
    safetyFsm.handleMqttCommand("ON", state);
    assert(state.safety_state == STATE_MANUAL_TEST);
    assert(state.buzzer_active == true);

    safetyFsm.handleMqttCommand("OFF", state);
    assert(state.safety_state == STATE_SAFE_NORMAL);
    assert(state.buzzer_active == false);
    std::cout << "[PASS] Skenario I: Perintah ON (MANUAL_TEST) dan OFF berhasil." << std::endl;

    std::cout << "=== SEMUA PENGUJIAN FSM BERHASIL DENGAN SEMPURNA! ===" << std::endl;
    return 0;
}
