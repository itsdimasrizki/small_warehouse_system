// ==============================================================================
// Standalone Test: DHT11 (Suhu & Kelembapan) + MQ2 (Gas/Asap)
// Gunakan sketch ini jika hanya ingin menguji DHT11 & MQ2.
// ==============================================================================

#include <Arduino.h>
#include <DHT.h>

#define PIN_DHT_DATA   4
#define PIN_MQ2_ANALOG 34 // ADC1 ESP32

DHT dht(PIN_DHT_DATA, DHT11);

void setup() {
    Serial.begin(115200);
    delay(1000);
    Serial.println("\n=== TES MANDIRI SENSOR LINGKUNGAN (DHT11 & MQ2) ===");
    dht.begin();
    pinMode(PIN_MQ2_ANALOG, INPUT);
    analogReadResolution(12); // Resolusi 12-bit (0 - 4095)
    Serial.println("[INFO] Sensor diinisialisasi. Menunggu pemanasan MQ2 (disarankan 20-30 detik)...\n");
}

void loop() {
    float temp = dht.readTemperature();
    float hum  = dht.readHumidity();
    int smokeAdc = analogRead(PIN_MQ2_ANALOG);

    Serial.println("----------------------------------------");
    if (isnan(temp) || isnan(hum)) {
        Serial.println("[DHT11 ERROR] Gagal membaca data DHT11! Periksa kabel GPIO 4 dan resistor pull-up 10k.");
    } else {
        Serial.printf("Suhu Ruangan       : %.1f °C %s\n", temp, (temp >= 38.0) ? "-> [PANAS / MELEBIHI 38°C!]" : "[AMAN]");
        Serial.printf("Kelembapan Ruangan : %.1f %%\n", hum);
    }

    Serial.printf("Sensor Asap MQ2    : %d ADC (0-4095) %s\n", smokeAdc, (smokeAdc >= 1800) ? "-> [BAHAYA / ASAP TERDETEKSI!]" : "[AMAN]");
    Serial.println("----------------------------------------\n");

    delay(2000); // DHT11 butuh interval minimal 1-2 detik
}
