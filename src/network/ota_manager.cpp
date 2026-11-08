#include "network/ota_manager.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <ArduinoOTA.h>

OtaManager::OtaManager()
    : updating(false), statusCallback(nullptr) {
}

void OtaManager::begin(const char* hostname) {
    // 1. Konfigurasi ArduinoOTA (Local Wi-Fi Flashing dari PlatformIO / Arduino IDE)
    ArduinoOTA.setHostname(hostname);

    ArduinoOTA.onStart([this]() {
        updating = true;
        String type;
        if (ArduinoOTA.getCommand() == U_FLASH) {
            type = "sketch";
        } else {
            type = "filesystem";
        }
        Serial.printf("[OTA-LAN] Memulai flashing %s lokal...\n", type.c_str());
        reportStatus("START_LAN", 0, "Memulai OTA via LAN");
    });

    ArduinoOTA.onEnd([this]() {
        updating = false;
        Serial.println("\n[OTA-LAN] Flashing selesai! ESP32 akan reboot...");
        reportStatus("SUCCESS_LAN", 100, "OTA LAN Selesai, rebooting...");
    });

    ArduinoOTA.onProgress([this](unsigned int progress, unsigned int total) {
        int percent = (int)(progress / (total / 100));
        static int lastPercent = -1;
        if (percent != lastPercent && percent % 10 == 0) {
            lastPercent = percent;
            Serial.printf("[OTA-LAN] Progress: %u%%\n", percent);
            reportStatus("PROGRESS_LAN", percent, "Mengunggah firmware...");
        }
    });

    ArduinoOTA.onError([this](ota_error_t error) {
        updating = false;
        const char* errStr = "Unknown";
        if (error == OTA_AUTH_ERROR) errStr = "Auth Failed";
        else if (error == OTA_BEGIN_ERROR) errStr = "Begin Failed";
        else if (error == OTA_CONNECT_ERROR) errStr = "Connect Failed";
        else if (error == OTA_RECEIVE_ERROR) errStr = "Receive Failed";
        else if (error == OTA_END_ERROR) errStr = "End Failed";
        Serial.printf("[OTA-LAN] Error[%u]: %s\n", error, errStr);
        reportStatus("ERROR_LAN", 0, errStr);
    });

    ArduinoOTA.begin();
    Serial.printf("[OTA] ArduinoOTA aktif. Hostname: %s.local\n", hostname);
}

void OtaManager::loop() {
    if (WiFi.status() == WL_CONNECTED && !updating) {
        ArduinoOTA.handle();
    }
}

void OtaManager::reportStatus(const char* status, int progress, const char* message) {
    if (statusCallback) {
        statusCallback(status, progress, message);
    }
}

bool OtaManager::startHttpUpdate(const String &firmwareUrl, const String &targetVersion) {
    if (firmwareUrl.length() == 0) {
        Serial.println("[OTA-HTTP] URL firmware kosong!");
        reportStatus("FAILED", 0, "URL firmware kosong");
        return false;
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[OTA-HTTP] Gagal: Wi-Fi belum terhubung!");
        reportStatus("FAILED", 0, "Wi-Fi tidak terhubung");
        return false;
    }

    updating = true;
    Serial.println("\n==================================================");
    Serial.println("  [OTA-HTTP] MEMULAI UPDATE FIRMWARE OTOMATIS");
    Serial.printf("  Versi Saat Ini : %s\n", FIRMWARE_VERSION);
    Serial.printf("  Target Versi   : %s\n", targetVersion.length() > 0 ? targetVersion.c_str() : "Terbaru");
    Serial.printf("  URL Unduhan    : %s\n", firmwareUrl.c_str());
    Serial.println("==================================================");

    reportStatus("DOWNLOADING", 0, "Mengunduh firmware baru dari server...");

    WiFiClientSecure client;
    // Gunakan setInsecure agar kompatibel dengan redirect GitHub Release / CDN tanpa perlu pinning Root CA DigiCert
    client.setInsecure();

    // Penting untuk GitHub Releases: GitHub mengarahkan link via 302 Redirect ke AWS/objects.githubusercontent.com
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    httpUpdate.rebootOnUpdate(false); // Kita kendalikan reboot manual agar bisa kirim notifikasi sukses dulu

    httpUpdate.onProgress([this](int current, int total) {
        if (total > 0) {
            int percent = (int)((current * 100) / total);
            static int lastPercent = -1;
            if (percent != lastPercent && percent % 10 == 0) {
                lastPercent = percent;
                Serial.printf("[OTA-HTTP] Download & Flash: %d%% (%d / %d bytes)\n", percent, current, total);
                reportStatus("BURNING", percent, "Menulis ke partisi flash...");
            }
        }
    });

    t_httpUpdate_return ret = httpUpdate.update(client, firmwareUrl);

    switch (ret) {
        case HTTP_UPDATE_FAILED: {
            updating = false;
            int errCode = httpUpdate.getLastError();
            String errStr = httpUpdate.getLastErrorString();
            Serial.printf("[OTA-HTTP] GAGAL: (%d) %s\n", errCode, errStr.c_str());
            reportStatus("FAILED", 0, errStr.c_str());
            return false;
        }

        case HTTP_UPDATE_NO_UPDATES: {
            updating = false;
            Serial.println("[OTA-HTTP] Tidak ada pembaruan firmware.");
            reportStatus("NO_UPDATE", 100, "Firmware sudah versi terbaru");
            return false;
        }

        case HTTP_UPDATE_OK: {
            Serial.println("\n[OTA-HTTP] SUKSES! Firmware berhasil di-flash!");
            reportStatus("SUCCESS", 100, "Update sukses! Memulai restart...");
            delay(1000);
            Serial.println("[OTA-HTTP] Merestart ESP32...");
            ESP.restart();
            return true;
        }
    }

    updating = false;
    return false;
}
