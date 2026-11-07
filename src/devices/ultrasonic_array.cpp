#include "devices/ultrasonic_array.h"

UltrasonicArray::UltrasonicArray()
    : currentIndex(0),
      sensors{
          NewPing(PIN_TRIG_F1_LEFT, PIN_ECHO_F1_LEFT, ULTRASONIC_MAX_DISTANCE_CM),
          NewPing(PIN_TRIG_F1_RIGHT, PIN_ECHO_F1_RIGHT, ULTRASONIC_MAX_DISTANCE_CM),
          NewPing(PIN_TRIG_F2_LEFT, PIN_ECHO_F2_LEFT, ULTRASONIC_MAX_DISTANCE_CM),
          NewPing(PIN_TRIG_F2_RIGHT, PIN_ECHO_F2_RIGHT, ULTRASONIC_MAX_DISTANCE_CM),
          NewPing(PIN_TRIG_F3_LEFT, PIN_ECHO_F3_LEFT, ULTRASONIC_MAX_DISTANCE_CM),
          NewPing(PIN_TRIG_F3_RIGHT, PIN_ECHO_F3_RIGHT, ULTRASONIC_MAX_DISTANCE_CM)
      }
{
}

void UltrasonicArray::begin() {
    // NewPing mengatur pinMode secara otomatis saat ping dijalankan.
    currentIndex = 0;
}

uint8_t UltrasonicArray::pollNextSensor(SystemState &state) {
    uint8_t readIndex = currentIndex;
    
    // Baca jarak dalam centimeter (0 jika tidak ada echo / di luar jangkauan)
    unsigned int distance = sensors[readIndex].ping_cm();
    float distance_cm = static_cast<float>(distance);

    // Simpan ke struct SystemState sesuai indeks lantai dan orientasi
    switch (readIndex) {
        case 0: state.floors[0].distance_left_cm  = distance_cm; break;
        case 1: state.floors[0].distance_right_cm = distance_cm; break;
        case 2: state.floors[1].distance_left_cm  = distance_cm; break;
        case 3: state.floors[1].distance_right_cm = distance_cm; break;
        case 4: state.floors[2].distance_left_cm  = distance_cm; break;
        case 5: state.floors[2].distance_right_cm = distance_cm; break;
        default: break;
    }

    // Geser ke sensor berikutnya secara sekuensial (0 s.d. 5)
    currentIndex = (currentIndex + 1) % SENSOR_COUNT;
    return readIndex;
}

float UltrasonicArray::readDirect(uint8_t sensorIndex) {
    if (sensorIndex >= SENSOR_COUNT) return 0.0f;
    return static_cast<float>(sensors[sensorIndex].ping_cm());
}

const char* UltrasonicArray::getSensorName(uint8_t sensorIndex) const {
    switch (sensorIndex) {
        case 0: return "Lantai 1 - Kiri  (TRIG 27, ECHO 35)";
        case 1: return "Lantai 1 - Kanan (TRIG 27, ECHO 34)";
        case 2: return "Lantai 2 - Kiri  (TRIG 27, ECHO 33)";
        case 3: return "Lantai 2 - Kanan (TRIG 27, ECHO 32)";
        case 4: return "Lantai 3 - Kiri  (TRIG 27, ECHO 26)";
        case 5: return "Lantai 3 - Kanan (TRIG 27, ECHO 25)";
        default: return "Sensor Tidak Dikenal";
    }
}
