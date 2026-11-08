#pragma once
#include <Arduino.h>
#include <functional>
#include "config/credentials.h"

// Tipe callback untuk mempublikasikan status OTA ke MQTT atau Serial
typedef std::function<void(const char* status, int progress, const char* message)> OtaStatusCallback;

class OtaManager {
public:
    OtaManager();

    // Memulai ArduinoOTA untuk flashing via Wi-Fi lokal (LAN)
    void begin(const char* hostname = "esp32-warehouse-master");

    // Dipanggil berkala di loop() untuk melayani ArduinoOTA
    void loop();

    // Memulai proses update firmware dari URL HTTPS (GitHub Release / CDN / Server)
    bool startHttpUpdate(const String &firmwareUrl, const String &targetVersion = "");

    // Mendaftarkan callback status progress
    void setStatusCallback(OtaStatusCallback cb) { statusCallback = cb; }

    // Status apakah sedang berlangsung proses update
    bool isUpdating() const { return updating; }

    // Dapatkan versi firmware saat ini
    const char* getVersion() const { return FIRMWARE_VERSION; }

private:
    bool updating;
    OtaStatusCallback statusCallback;
    void reportStatus(const char* status, int progress, const char* message);
};
