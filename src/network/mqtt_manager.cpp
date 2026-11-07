#include "network/mqtt_manager.h"
#include "network/wifi_portal.h"
MqttManager* MqttManager::instance = nullptr;

MqttManager::MqttManager(SafetyFSM &safetyFsm, WifiPortal* wifiPortal)
    : safety(safetyFsm),
      portal(wifiPortal),
      mqttClient(espClient),
      lastReconnectAttempt(0),
      lastWifiCheck(0)
{
    instance = this;
}

void MqttManager::begin() {
    Serial.println("\n[MQTT] Menginisialisasi MQTT Client...");

    const char* broker = (portal && portal->getBroker().length() > 0) ? portal->getBroker().c_str() : MQTT_BROKER;
    uint16_t port = (portal && portal->getPort() > 0) ? portal->getPort() : MQTT_PORT;

    // Konfigurasi PubSubClient
    mqttClient.setServer(broker, port);
    mqttClient.setCallback(mqttCallback);
    // Perbesar buffer paket agar JSON lengkap tidak terpotong (default PubSubClient hanya 128/256 bytes)
    mqttClient.setBufferSize(1024);

    Serial.printf("[MQTT] Broker dikonfigurasi ke: %s:%d (Buffer: 1024 bytes)\n", broker, port);
}

void MqttManager::loop() {
    ensureWifiConnected();

    if (WiFi.status() == WL_CONNECTED) {
        if (!mqttClient.connected()) {
            unsigned long now = millis();
            if (now - lastReconnectAttempt >= INTERVAL_MQTT_RECONNECT_MS) {
                lastReconnectAttempt = now;
                if (reconnectMqtt()) {
                    lastReconnectAttempt = 0;
                }
            }
        } else {
            mqttClient.loop();
        }
    }
}

void MqttManager::ensureWifiConnected() {
    if (portal && portal->isPortalActive()) {
        return; // Jangan coba reconnect STA jika sedang dalam mode Web Config Portal SoftAP
    }

    unsigned long now = millis();
    if (now - lastWifiCheck >= INTERVAL_MQTT_RECONNECT_MS) {
        lastWifiCheck = now;
        if (WiFi.status() != WL_CONNECTED) {
            static bool reported = false;
            if (!reported) {
                const char* ssid = (portal && portal->getSSID().length() > 0) ? portal->getSSID().c_str() : WIFI_SSID;
                Serial.printf("[WIFI] Status: Belum terhubung ke '%s' (mencoba di latar belakang...)\n", ssid);
                reported = true;
            }
            WiFi.reconnect();
        } else {
            static bool reportedConnected = false;
            if (!reportedConnected) {
                Serial.printf("[WIFI] Berhasil terhubung! IP Address: %s\n", WiFi.localIP().toString().c_str());
                reportedConnected = true;
            }
        }
    }
}

bool MqttManager::reconnectMqtt() {
    const char* broker = (portal && portal->getBroker().length() > 0) ? portal->getBroker().c_str() : MQTT_BROKER;
    uint16_t port = (portal && portal->getPort() > 0) ? portal->getPort() : MQTT_PORT;
    mqttClient.setServer(broker, port);

    Serial.printf("[MQTT] Menghubungkan ke broker %s:%d...", broker, port);

    // Buat Client ID unik berbasis MAC address ESP32
    uint64_t chipMac = ESP.getEfuseMac();
    char clientId[48];
    snprintf(clientId, sizeof(clientId), "%s_%04X", MQTT_CLIENT_ID, (uint16_t)(chipMac & 0xFFFF));

    bool connected = false;
    if (strlen(MQTT_USER) > 0) {
        connected = mqttClient.connect(clientId, MQTT_USER, MQTT_PASSWORD);
    } else {
        connected = mqttClient.connect(clientId);
    }

    if (connected) {
        Serial.printf(" BERHASIL! (ID: %s)\n", clientId);
        // Subscribe ke topik kontrol alarm buzzer
        if (mqttClient.subscribe(TOPIC_BUZZER_CTRL)) {
            Serial.printf("[MQTT] Berhasil subscribe ke topik kontrol: %s\n", TOPIC_BUZZER_CTRL);
        } else {
            Serial.printf("[MQTT] Gagal subscribe ke topik kontrol: %s\n", TOPIC_BUZZER_CTRL);
        }
        return true;
    } else {
        Serial.printf(" GAGAL, rc=%d. Coba lagi dalam 5 detik.\n", mqttClient.state());
        return false;
    }
}

void MqttManager::mqttCallback(char* topic, byte* payload, unsigned int length) {
    if (instance) {
        instance->handleIncomingMessage(topic, payload, length);
    }
}

void MqttManager::handleIncomingMessage(char* topic, byte* payload, unsigned int length) {
    String cmd;
    cmd.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        cmd += (char)payload[i];
    }
    cmd.trim();
    cmd.toUpperCase();

    Serial.printf("\n[MQTT KONTROL] Pesan masuk pada topik '%s': %s\n", topic, cmd.c_str());

    // Teruskan perintah ke FSM Keamanan
    if (strcmp(topic, TOPIC_BUZZER_CTRL) == 0) {
        safety.handleMqttCommand(cmd);
    }
}

bool MqttManager::publishTelemetry(const SystemState &state) {
    // Alokasi dokumen JSON dinamis (ArduinoJson v7)
    JsonDocument doc;

    // 1. Data Jarak dan Status per Lantai Rak (6 Sensor Ultrasonik)
    JsonArray floors = doc["floors"].to<JsonArray>();
    for (int i = 0; i < 3; i++) {
        JsonObject f = floors.add<JsonObject>();
        f["floor"] = i + 1;
        f["distance_left_cm"] = state.floors[i].distance_left_cm;
        f["distance_right_cm"] = state.floors[i].distance_right_cm;
        f["occupied_span_cm"] = state.floors[i].occupied_span_cm;
        f["item_count"] = state.floors[i].item_count;
        f["is_occupied"] = state.floors[i].is_occupied;
        f["detected_sides"] = state.floors[i].detected_sides;

        const char* statusStr = (state.floors[i].rack_state == STATE_RACK_EMPTY) ? "EMPTY" :
                                (state.floors[i].rack_state == STATE_RACK_PARTIAL) ? "PARTIAL" : "FULL";
        const char* ledColorStr = (state.hazard_detected) ? "RED_BLINKING" :
                                  (state.floors[i].rack_state == STATE_RACK_EMPTY) ? "RED" :
                                  (state.floors[i].rack_state == STATE_RACK_PARTIAL) ? "BLUE" : "GREEN";
        f["status"] = statusStr;
        f["led_color"] = ledColorStr;
    }

    // 2. Ringkasan Okupansi Gudang & Total Barang
    JsonObject summary = doc["summary"].to<JsonObject>();
    summary["total_items"] = state.total_item_count;
    summary["occupied_floors"] = state.occupied_floor_count;
    summary["warehouse_status"] = (state.rack_state == STATE_RACK_EMPTY) ? "EMPTY" :
                                  (state.rack_state == STATE_RACK_PARTIAL) ? "PARTIAL" : "FULL";

    // 3. Sensor Lingkungan (DHT11 & MQ2 Gas)
    JsonObject env = doc["environment"].to<JsonObject>();
    env["temperature_c"] = state.temperature_c;
    env["humidity_pct"] = state.humidity_pct;
    env["gas_raw"] = state.smoke_adc;
    env["gas_pct"] = state.smoke_pct;
    env["hazard_detected"] = state.hazard_detected;
    env["safety_state"] = SafetyFSM::getStateName(state.safety_state);

    // 4. Status Aktuator
    JsonObject actuators = doc["actuators"].to<JsonObject>();
    actuators["buzzer"] = state.buzzer_active;

    // Serialisasi JSON
    char jsonBuffer[1024];
    size_t len = serializeJson(doc, jsonBuffer, sizeof(jsonBuffer));

    if (mqttClient.connected()) {
        bool ok = mqttClient.publish(TOPIC_TELEMETRY, jsonBuffer);
        if (ok) {
            Serial.printf("[MQTT TELEMETRY] Berhasil dikirim (%u bytes ke '%s'):\n%s\n",
                          len, TOPIC_TELEMETRY, jsonBuffer);
            return true;
        } else {
            Serial.println("[MQTT TELEMETRY] Gagal mem-publish payload (buffer penuh atau koneksi terputus).");
            return false;
        }
    } else {
        Serial.printf("[MQTT TELEMETRY] Offline (MQTT belum terkoneksi). Preview JSON payload (%u bytes):\n%s\n",
                      len, jsonBuffer);
        return false;
    }
}

bool MqttManager::isConnected() {
    return isWifiConnected() && isMqttConnected();
}

bool MqttManager::isWifiConnected() {
    return (WiFi.status() == WL_CONNECTED);
}

bool MqttManager::isMqttConnected() {
    return mqttClient.connected();
}
