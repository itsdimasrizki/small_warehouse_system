#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "types/system_state.h"
#include "config/credentials.h"
#include "config/thresholds.h"
#include "fsm/safety_fsm.h"
#include "fsm/rack_fsm.h"

class WifiPortal;
class OtaManager;

class MqttManager {
public:
    // Konstruktor menerima referensi SafetyFSM, opsional WifiPortal dan OtaManager
    explicit MqttManager(SafetyFSM &safetyFsm, WifiPortal* wifiPortal = nullptr, OtaManager* otaManager = nullptr);
    // Inisialisasi Wi-Fi dan MQTT client
    void begin();

    void loop();

    bool publishTelemetry(const SystemState &state);

    // Mem-publish status OTA ke topik status
    bool publishOtaStatus(const char* status, int progress, const char* message);

    // Set / update OtaManager instance
    void setOtaManager(OtaManager* otaManager) { ota = otaManager; }

    // Status konektivitas
    bool isConnected();
    bool isWifiConnected();
    bool isMqttConnected();

private:
    SafetyFSM &safety;
    WifiPortal* portal;
    OtaManager* ota;
    WiFiClientSecure espClient;
    PubSubClient mqttClient;
    unsigned long lastReconnectAttempt;
    unsigned long lastWifiCheck;

    // Prosedur koneksi non-blocking
    void ensureWifiConnected();
    bool reconnectMqtt();

    // Callback pemroses pesan masuk dari topik MQTT
    void handleIncomingMessage(char* topic, byte* payload, unsigned int length);
    static void mqttCallback(char* topic, byte* payload, unsigned int length);
    static MqttManager* instance;
};
