#include "network/mqtt_manager.h"
#include "network/wifi_portal.h"
#include "network/ota_manager.h"

MqttManager* MqttManager::instance = nullptr;

MqttManager::MqttManager(SafetyFSM &safetyFsm, WifiPortal* wifiPortal, OtaManager* otaManager)
    : safety(safetyFsm),
      portal(wifiPortal),
      ota(otaManager),
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

    // Mengizinkan koneksi TLS/SSL port 8883 tanpa validasi manual sertifikat CA
    espClient.setInsecure();
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
                String ssidStr = (portal && portal->getSSID().length() > 0) ? portal->getSSID() : String(WIFI_SSID);
                const char* ssid = ssidStr.length() > 0 ? ssidStr.c_str() : "(Belum Disetel)";
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

    bool connected = mqttClient.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASSWORD);

    if (connected) {
        Serial.printf(" BERHASIL! (ID: %s)\n", MQTT_CLIENT_ID);
        // Subscribe ke topik kontrol alarm buzzer & topik update OTA
        mqttClient.subscribe(TOPIC_BUZZER_CTRL);
        mqttClient.subscribe(TOPIC_OTA_UPDATE);
        Serial.printf("[MQTT] Subscribed ke topik kontrol (%s) dan OTA (%s)\n",
                      TOPIC_BUZZER_CTRL, TOPIC_OTA_UPDATE);
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
    String rawPayload;
    rawPayload.reserve(length);
    for (unsigned int i = 0; i < length; i++) {
        rawPayload += (char)payload[i];
    }
    rawPayload.trim();

    Serial.printf("\n[MQTT KONTROL] Pesan masuk pada topik '%s': %s\n", topic, rawPayload.c_str());

    // 1. Tangani Topik Pembaruan Firmware OTA
    if (strcmp(topic, TOPIC_OTA_UPDATE) == 0) {
        String targetUrl = "";
        String targetVersion = "";

        // Parse payload (bisa JSON {"url": "...", "version": "..."} atau URL teks langsung)
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, rawPayload);
        if (!err && doc["url"].is<const char*>()) {
            targetUrl = doc["url"].as<String>();
            if (doc["version"].is<const char*>()) {
                targetVersion = doc["version"].as<String>();
            }
        } else {
            targetUrl = rawPayload;
        }

        if (ota) {
            Serial.printf("[OTA] Menjalankan update dari URL: %s (Target Versi: %s)\n",
                          targetUrl.c_str(), targetVersion.c_str());
            ota->startHttpUpdate(targetUrl, targetVersion);
        } else {
            Serial.println("[OTA] Error: OtaManager belum terpasang di MqttManager!");
        }
        return;
    }

    // 2. Teruskan perintah ke FSM Keamanan
    if (strcmp(topic, TOPIC_BUZZER_CTRL) == 0) {
        String cmd = rawPayload;
        cmd.toUpperCase();
        safety.handleMqttCommand(cmd);
    }
}

bool MqttManager::publishOtaStatus(const char* status, int progress, const char* message) {
    if (!mqttClient.connected()) return false;

    JsonDocument doc;
    doc["status"] = status;
    doc["progress"] = progress;
    doc["message"] = message;
    doc["firmware_version"] = FIRMWARE_VERSION;

    char buffer[256];
    serializeJson(doc, buffer, sizeof(buffer));
    return mqttClient.publish(TOPIC_OTA_STATUS, buffer);
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
    summary["firmware_version"] = FIRMWARE_VERSION;
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
