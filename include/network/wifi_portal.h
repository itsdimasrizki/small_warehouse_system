#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include "config/credentials.h"

class WifiPortal {
public:
    WifiPortal();

    // Memulai proses Wi-Fi:
    bool begin(uint8_t timeoutSec = 15);

    // Pemeliharaan loop server Captive Portal (wajib dipanggil di loop main.cpp)
    void loop();

    // Membuka mode AP portal secara manual (misal dari Serial Monitor atau tombol)
    void startPortal();

    // Menghentikan portal AP dan beralih ke mode STA normal
    void stopPortal();

    // Menghapus data kredensial di NVS (Preferences) dan kembali ke default credentials.h
    void resetSettings();

    // Status
    bool isPortalActive() const { return portalActive; }
    bool isConnected() const { return WiFi.status() == WL_CONNECTED; }

    // Kredensial aktif yang tersimpan
    String getSSID() const { return storedSSID; }
    String getPassword() const { return storedPass; }
    String getBroker() const { return storedBroker; }
    uint16_t getPort() const { return storedPort; }

private:
    WebServer server;
    DNSServer dnsServer;
    Preferences preferences;

    bool portalActive;
    String storedSSID;
    String storedPass;
    String storedBroker;
    uint16_t storedPort;

    void loadCredentials();
    void saveCredentials(const String &ssid, const String &pass, const String &broker, uint16_t port);
    void setupWebServer();

    // Request Handlers
    void handleRoot();
    void handleSave();
    void handleScan();
    void handleRestart();
    void handleNotFound();
};
