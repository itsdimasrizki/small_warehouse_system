#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "types/system_state.h"
#include "config/credentials.h"
#include "config/thresholds.h"
#include "fsm/safety_fsm.h"
#include "fsm/rack_fsm.h"

class WifiPortal;

class MqttManager {
public:
    // Konstruktor menerima referensi SafetyFSM dan opsional pointer ke WifiPortal
    explicit MqttManager(SafetyFSM &safetyFsm, WifiPortal* wifiPortal = nullptr);
    // Inisialisasi Wi-Fi dan MQTT client
    void begin();

    // Pemeliharaan loop non-blocking (wajib dipanggil setiap siklus loop di main.cpp)
    void loop();

    // Mengemas dan mem-publish data telemetri ke topik MQTT dalam format JSON
    bool publishTelemetry(const SystemState &state);

    // Status konektivitas
    bool isConnected();
    bool isWifiConnected();
    bool isMqttConnected();

private:
    SafetyFSM &safety;
    WifiPortal* portal;
    WiFiClient espClient;
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
