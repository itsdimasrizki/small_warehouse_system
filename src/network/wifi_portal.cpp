#include "network/wifi_portal.h"

static const IPAddress apIP(192, 168, 4, 1);
static const IPAddress netMask(255, 255, 255, 0);

WifiPortal::WifiPortal()
    : server(80),
      portalActive(false),
      storedSSID(""),
      storedPass(""),
      storedBroker(MQTT_BROKER),
      storedPort(MQTT_PORT)
{
}

void WifiPortal::loadCredentials() {
    preferences.begin("warehouse", true); // Mode read-only
    storedSSID = preferences.getString("ssid", "");
    storedPass = preferences.getString("pass", "");
    storedBroker = preferences.getString("broker", MQTT_BROKER);
    storedPort = preferences.getUShort("port", MQTT_PORT);
    preferences.end();
}

void WifiPortal::saveCredentials(const String &ssid, const String &pass, const String &broker, uint16_t port) {
    preferences.begin("warehouse", false); // Mode read-write
    preferences.putString("ssid", ssid);
    preferences.putString("pass", pass);
    preferences.putString("broker", broker);
    preferences.putUShort("port", port);
    preferences.end();

    storedSSID = ssid;
    storedPass = pass;
    storedBroker = broker;
    storedPort = port;
    Serial.println("[PREFERENCES] Kredensial Wi-Fi & MQTT berhasil disimpan ke NVS Flash ESP32.");
}

void WifiPortal::resetSettings() {
    preferences.begin("warehouse", false);
    preferences.clear();
    preferences.end();
    Serial.println("[PREFERENCES] Seluruh pengaturan Wi-Fi di NVS telah direset ke default.");
    loadCredentials();
}

bool WifiPortal::begin(uint8_t timeoutSec) {
    loadCredentials();

    Serial.println("\n==================================================");
    Serial.println("[WIFI PORTAL] Memulai Manajemen Jaringan ESP32...");
    Serial.println("==================================================");

    // Jika ada SSID yang tersimpan di NVS Flash dari konfigurasi via Web Portal
    if (storedSSID.length() > 0 && storedSSID != "YOUR_WIFI_SSID") {
        Serial.printf("[WIFI] Mencoba koneksi ke SSID tersimpan: '%s' (Batas waktu: %d detik)...\n",
                      storedSSID.c_str(), timeoutSec);
        WiFi.persistent(false);
        WiFi.setAutoReconnect(false);
        WiFi.mode(WIFI_STA);
        WiFi.setTxPower(WIFI_POWER_11dBm);
        WiFi.begin(storedSSID.c_str(), storedPass.c_str());

        unsigned long startAttempt = millis();
        while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < (timeoutSec * 1000UL)) {
            delay(500);
            Serial.print(".");
        }

        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\n[WIFI] BERHASIL TERHUBUNG!");
            Serial.printf("[WIFI] IP Address     : %s\n", WiFi.localIP().toString().c_str());
            Serial.printf("[WIFI] Gateway        : %s\n", WiFi.gatewayIP().toString().c_str());
            Serial.printf("[WIFI] Kekuatan (RSSI): %d dBm\n", WiFi.RSSI());
            portalActive = false;
            return true;
        } else {
            Serial.println("\n[WIFI] Gagal terhubung ke Wi-Fi tersimpan (Timeout / Sandi Salah / Router Mati).");
            // Bersihkan koneksi STA agar tidak ada background reconnect & channel hopping saat portal dibuka
            WiFi.disconnect(true, true);
            delay(100);
        }
    } else {
        Serial.println("[WIFI] Mode Non-Hardcode: Belum ada kredensial Wi-Fi tersimpan di NVS Flash.");
    }

    // Jika belum ada kredensial atau koneksi gagal, otomatis aktifkan SoftAP Captive Portal
    Serial.println("[WIFI] Mengaktifkan SoftAP Captive Portal agar dapat dikonfigurasi via HP...");
    startPortal();
    return false;
}
void WifiPortal::startPortal() {
    if (portalActive) {
        stopPortal();
    }
    portalActive = true;

    // 1. Putuskan dan nonaktifkan STA sepenuhnya agar radio tidak melakukan background reconnect
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(true, true);
    delay(100);

    // 2. Set mode ke WIFI_AP (Dedicated SoftAP agar radio terkunci di Channel 1 tanpa channel-hopping)
    WiFi.mode(WIFI_AP);
    delay(100);

    char apName[32];
    uint16_t chipId = (uint16_t)(ESP.getEfuseMac() & 0xFFFF);
    snprintf(apName, sizeof(apName), "Warehouse-Rak-%04X", chipId);

    // 3. Konfigurasi SoftAP IP & pastikan DNS server (apIP) disebarkan oleh DHCP server ke klien HP via Option 6
    WiFi.softAPConfig(apIP, apIP, netMask, IPAddress(192, 168, 4, 2), apIP);
    bool apSuccess = WiFi.softAP(apName);
    if (!apSuccess) {
        Serial.println("[WARN] Gagal menyalakan SoftAP, mencoba inisialisasi ulang...");
        delay(100);
        WiFi.softAPConfig(apIP, apIP, netMask, IPAddress(192, 168, 4, 2), apIP);
        apSuccess = WiFi.softAP(apName);
    }
    WiFi.setTxPower(WIFI_POWER_11dBm);
    Serial.println("\n--------------------------------------------------");
    Serial.println("[CAPTIVE PORTAL AKTIF] Silakan sambungkan HP / Laptop Anda:");
    Serial.printf(" * Nama Wi-Fi (SSID) : %s\n", apName);
    Serial.println(" * Sandi Wi-Fi       : (Tanpa Sandi / Open)");
    Serial.printf(" * Alamat Web Portal : http://%s/\n", apIP.toString().c_str());
    Serial.println("--------------------------------------------------");

    // 4. DNS Server untuk mengarahkan seluruh domain (Captive Portal Redirect) ke IP ESP32
    dnsServer.stop();
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    if (dnsServer.start(53, "*", apIP)) {
        Serial.println("[DNS SERVER] DNS Server Captive Portal aktif di Port 53.");
    } else {
        Serial.println("[WARN] DNS Server gagal mengikat port 53!");
    }

    // 5. Inisialisasi Web Server
    setupWebServer();
    server.begin();
    Serial.println("[HTTP SERVER] Web Server Captive Portal telah aktif di Port 80.");
}
void WifiPortal::stopPortal() {
    if (portalActive) {
        dnsServer.stop();
        server.stop();
        WiFi.softAPdisconnect(true);
        portalActive = false;
        Serial.println("[WIFI PORTAL] SoftAP Captive Portal dimatikan.");
    }
}

void WifiPortal::loop() {
    if (portalActive) {
        dnsServer.processNextRequest();
        server.handleClient();
    }
}

void WifiPortal::setupWebServer() {
    static bool routesConfigured = false;
    if (routesConfigured) return;
    routesConfigured = true;

    server.on("/", HTTP_GET, [this]() { handleRoot(); });
    server.on("/save", HTTP_POST, [this]() { handleSave(); });
    server.on("/scan", HTTP_GET, [this]() { handleScan(); });
    server.on("/restart", HTTP_GET, [this]() { handleRestart(); });

    // Rute standar OS Captive Portal Detection
    server.on("/generate_204", HTTP_GET, [this]() { handleRoot(); });      // Android
    server.on("/hotspot-detect.html", HTTP_GET, [this]() { handleRoot(); });// Apple iOS/macOS
    server.on("/canonical.html", HTTP_GET, [this]() { handleRoot(); });
    server.on("/ncsi.txt", HTTP_GET, [this]() { handleRoot(); });          // Windows
    server.on("/connecttest.txt", HTTP_GET, [this]() { handleRoot(); });

    server.onNotFound([this]() { handleNotFound(); });
}

void WifiPortal::handleNotFound() {
    // Redirect otomatis ke root captive portal
    server.sendHeader("Location", String("http://") + apIP.toString() + "/", true);
    server.send(302, "text/plain", "");
}

void WifiPortal::handleScan() {
    int n = WiFi.scanNetworks();
    String json = "[";
    for (int i = 0; i < n; i++) {
        if (i > 0) json += ",";
        json += "{\"ssid\":\"" + WiFi.SSID(i) + "\",";
        json += "\"rssi\":" + String(WiFi.RSSI(i)) + ",";
        json += "\"secure\":" + String((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "false" : "true") + "}";
    }
    json += "]";
    WiFi.scanDelete();
    server.send(200, "application/json", json);
}
void WifiPortal::handleRestart() {
    server.send(200, "text/html", "<html><body><h3>Memulai Ulang Perangkat...</h3></body></html>");
    delay(1000);
    ESP.restart();
}

void WifiPortal::handleSave() {
    String newSsid = server.arg("ssid");
    String newPass = server.arg("password");
    String newBroker = server.arg("broker");
    String portStr = server.arg("port");

    newSsid.trim();
    newPass.trim();
    newBroker.trim();
    portStr.trim();

    uint16_t newPort = portStr.length() > 0 ? (uint16_t)portStr.toInt() : 1883;
    if (newBroker.length() == 0) newBroker = "broker.emqx.io";

    if (newSsid.length() == 0) {
        server.send(400, "text/html", "<html><body><h3>Error: Nama Wi-Fi (SSID) tidak boleh kosong!</h3><a href='/'>Kembali</a></body></html>");
        return;
    }

    saveCredentials(newSsid, newPass, newBroker, newPort);

    // Respon HTML Sukses dengan UI Tema Warehouse
    String page = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Menyimpan Konfigurasi Rak</title>
    <style>
        body {
            background-color: #0b0f19;
            color: #f8fafc;
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            height: 100vh;
            margin: 0;
            padding: 20px;
            box-sizing: border-box;
        }
        .card {
            background: #131b2e;
            border: 1px solid #1e293b;
            border-radius: 16px;
            padding: 32px;
            text-align: center;
            max-width: 420px;
            width: 100%;
            box-shadow: 0 10px 25px -5px rgba(0, 0, 0, 0.5);
        }
        .icon-box {
            display: inline-flex;
            background: rgba(16, 185, 129, 0.15);
            border: 2px solid #10b981;
            color: #10b981;
            border-radius: 50%;
            padding: 16px;
            margin-bottom: 20px;
        }
        h2 { color: #f59e0b; margin: 0 0 10px 0; font-size: 22px; }
        p { color: #94a3b8; font-size: 14px; line-height: 1.5; margin: 8px 0; }
        .highlight { color: #38bdf8; font-weight: 600; }
        .progress-bar {
            width: 100%;
            height: 8px;
            background: #1e293b;
            border-radius: 4px;
            overflow: hidden;
            margin: 24px 0 12px 0;
        }
        .progress-fill {
            height: 100%;
            background: linear-gradient(90deg, #f59e0b, #10b981);
            width: 0%;
            animation: loadProgress 5s linear forwards;
        }
        @keyframes loadProgress {
            0% { width: 0%; }
            100% { width: 100%; }
        }
    </style>
</head>
<body>
    <div class="card">
        <div class="icon-box">
            <svg width="40" height="40" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
                <path d="M22 11.08V12a10 10 0 1 1-5.93-9.14"></path>
                <polyline points="22 4 12 14.01 9 11.01"></polyline>
            </svg>
        </div>
        <h2>Kredensial Tersimpan!</h2>
        <p>ESP32 sedang menghubungkan ke jaringan:</p>
        <p class="highlight">)rawliteral" + newSsid + R"rawliteral(</p>
        <div class="progress-bar">
            <div class="progress-fill"></div>
        </div>
        <p style="font-size: 12px; color: #64748b;">Perangkat sedang memulai ulang... Silakan hubungkan kembali HP/Laptop Anda ke Wi-Fi gudang tersebut.</p>
    </div>
    <script>
        setTimeout(() => {
            window.location.href = '/';
        }, 6000);
    </script>
</body>
</html>
)rawliteral";

    server.send(200, "text/html", page);
    delay(2000);

    Serial.println("[WIFI PORTAL] Melakukan restart perangkat untuk menerapkan koneksi baru...");
    ESP.restart();
}

void WifiPortal::handleRoot() {
    uint64_t chipMac = ESP.getEfuseMac();
    char macStr[20];
    snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
             (uint8_t)(chipMac >> 40), (uint8_t)(chipMac >> 32),
             (uint8_t)(chipMac >> 24), (uint8_t)(chipMac >> 16),
             (uint8_t)(chipMac >> 8),  (uint8_t)(chipMac));

    String html = R"rawliteral(
<!DOCTYPE html>
<html lang="id">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Smart Warehouse - Konfigurasi Wi-Fi Rak</title>
    <style>
        :root {
            --bg: #0b0f19;
            --surface: #131b2e;
            --surface-hover: #1b2640;
            --border: #1e293b;
            --text-main: #f8fafc;
            --text-muted: #94a3b8;
            --accent-amber: #f59e0b;
            --accent-amber-hover: #d97706;
            --accent-cyan: #06b6d4;
            --accent-green: #10b981;
            --accent-red: #ef4444;
        }
        * { box-sizing: border-box; margin: 0; padding: 0; }
        body {
            background-color: var(--bg);
            color: var(--text-main);
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Helvetica, Arial, sans-serif;
            display: flex;
            justify-content: center;
            align-items: center;
            min-height: 100vh;
            padding: 16px;
        }
        .container {
            width: 100%;
            max-width: 460px;
            background: var(--surface);
            border: 1px solid var(--border);
            border-radius: 20px;
            box-shadow: 0 20px 40px -15px rgba(0, 0, 0, 0.7);
            overflow: hidden;
        }
        /* Header Tema Gudang Rak */
        .header {
            background: linear-gradient(135deg, #1e293b 0%, #0f172a 100%);
            border-bottom: 2px solid var(--accent-amber);
            padding: 24px 20px;
            text-align: center;
            position: relative;
        }
        .rack-logo {
            width: 58px;
            height: 58px;
            margin: 0 auto 12px;
            background: rgba(245, 158, 11, 0.12);
            border: 2px solid var(--accent-amber);
            border-radius: 14px;
            display: flex;
            align-items: center;
            justify-content: center;
            color: var(--accent-amber);
        }
        .title {
            font-size: 20px;
            font-weight: 700;
            letter-spacing: 0.5px;
            color: var(--text-main);
        }
        .subtitle {
            font-size: 13px;
            color: var(--accent-amber);
            margin-top: 4px;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        /* Status Badge */
        .status-row {
            display: flex;
            justify-content: space-between;
            background: rgba(15, 23, 42, 0.6);
            padding: 10px 16px;
            border-bottom: 1px solid var(--border);
            font-size: 11px;
            color: var(--text-muted);
        }
        .badge {
            display: inline-flex;
            align-items: center;
            gap: 5px;
            color: var(--accent-cyan);
            font-family: monospace;
            font-size: 12px;
        }
        .badge-dot {
            width: 7px;
            height: 7px;
            background: var(--accent-green);
            border-radius: 50%;
            box-shadow: 0 0 8px var(--accent-green);
        }
        /* Content Form */
        .content {
            padding: 24px 20px;
        }
        .section-title {
            font-size: 13px;
            font-weight: 600;
            color: var(--text-muted);
            text-transform: uppercase;
            letter-spacing: 0.8px;
            margin-bottom: 14px;
            display: flex;
            align-items: center;
            gap: 8px;
        }
        .section-title svg { stroke: var(--accent-amber); }
        .form-group {
            margin-bottom: 18px;
        }
        label {
            display: block;
            font-size: 13px;
            font-weight: 500;
            color: var(--text-main);
            margin-bottom: 6px;
        }
        .input-wrapper {
            position: relative;
            display: flex;
            align-items: center;
        }
        input[type="text"], input[type="password"], input[type="number"] {
            width: 100%;
            padding: 12px 14px;
            background: #0b0f19;
            border: 1px solid var(--border);
            border-radius: 10px;
            color: var(--text-main);
            font-size: 14px;
            outline: none;
            transition: all 0.2s ease;
        }
        input:focus {
            border-color: var(--accent-amber);
            box-shadow: 0 0 0 3px rgba(245, 158, 11, 0.15);
        }
        .toggle-btn {
            position: absolute;
            right: 12px;
            background: none;
            border: none;
            color: var(--text-muted);
            cursor: pointer;
            padding: 4px;
            display: flex;
            align-items: center;
        }
        .toggle-btn:hover { color: var(--text-main); }
        /* Tombol Scan */
        .btn-scan {
            width: 100%;
            padding: 10px;
            background: rgba(6, 182, 212, 0.1);
            border: 1px dashed var(--accent-cyan);
            border-radius: 10px;
            color: var(--accent-cyan);
            font-size: 13px;
            font-weight: 600;
            cursor: pointer;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
            transition: all 0.2s ease;
            margin-bottom: 16px;
        }
        .btn-scan:hover {
            background: rgba(6, 182, 212, 0.2);
        }
        .scan-list {
            max-height: 160px;
            overflow-y: auto;
            margin-bottom: 16px;
            border: 1px solid var(--border);
            border-radius: 10px;
            display: none;
            background: #0b0f19;
        }
        .scan-item {
            padding: 10px 14px;
            border-bottom: 1px solid var(--border);
            font-size: 13px;
            display: flex;
            justify-content: space-between;
            align-items: center;
            cursor: pointer;
            transition: background 0.15s;
        }
        .scan-item:last-child { border-bottom: none; }
        .scan-item:hover { background: var(--surface-hover); }
        .rssi-badge {
            font-size: 11px;
            color: var(--accent-green);
            background: rgba(16, 185, 129, 0.12);
            padding: 2px 6px;
            border-radius: 4px;
        }
        /* Tombol Utama */
        .btn-submit {
            width: 100%;
            padding: 14px;
            background: linear-gradient(135deg, var(--accent-amber) 0%, var(--accent-amber-hover) 100%);
            border: none;
            border-radius: 10px;
            color: #0b0f19;
            font-size: 15px;
            font-weight: 700;
            letter-spacing: 0.5px;
            cursor: pointer;
            box-shadow: 0 4px 15px rgba(245, 158, 11, 0.35);
            transition: all 0.2s ease;
            display: flex;
            align-items: center;
            justify-content: center;
            gap: 8px;
            margin-top: 10px;
        }
        .btn-submit:hover {
            transform: translateY(-1px);
            box-shadow: 0 6px 20px rgba(245, 158, 11, 0.45);
        }
        .btn-submit:active { transform: translateY(0); }
        .footer {
            text-align: center;
            padding: 14px 20px;
            background: rgba(15, 23, 42, 0.5);
            border-top: 1px solid var(--border);
            font-size: 11px;
            color: #64748b;
        }
        .collapsible-header {
            font-size: 12px;
            color: var(--accent-amber);
            cursor: pointer;
            margin-bottom: 12px;
            display: flex;
            align-items: center;
            justify-content: space-between;
            user-select: none;
            padding: 6px 0;
        }
        .collapsible-content {
            display: none;
            padding-top: 6px;
            border-top: 1px dashed var(--border);
            margin-bottom: 12px;
        }
    </style>
</head>
<body>
    <div class="container">
        <!-- Header -->
        <div class="header">
            <div class="rack-logo">
                <svg width="34" height="34" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                    <rect x="3" y="3" width="18" height="18" rx="2"></rect>
                    <line x1="3" y1="9" x2="21" y2="9"></line>
                    <line x1="3" y1="15" x2="21" y2="15"></line>
                    <line x1="9" y1="9" x2="9" y2="21"></line>
                    <line x1="15" y1="9" x2="15" y2="21"></line>
                </svg>
            </div>
            <div class="title">SMART WAREHOUSE IoT</div>
            <div class="subtitle">Sistem Rak Pintar 3 Lantai</div>
        </div>

        <!-- Status Device -->
        <div class="status-row">
            <span>ESP32 MAC: <strong class="badge">)rawliteral" + String(macStr) + R"rawliteral(</strong></span>
            <span class="badge"><span class="badge-dot"></span> Mode Portal AP</span>
        </div>

        <!-- Form Konten -->
        <div class="content">
            <!-- Tombol Pindai Wi-Fi -->
            <button type="button" class="btn-scan" onclick="scanWifi()">
                <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                    <path d="M5 12.55a11 11 0 0 1 14.08 0"></path>
                    <path d="M1.42 9a16 16 0 0 1 21.16 0"></path>
                    <path d="M8.53 16.11a6 6 0 0 1 6.95 0"></path>
                    <line x1="12" y1="20" x2="12.01" y2="20"></line>
                </svg>
                <span id="scan-label">Pindai Jaringan Wi-Fi Sekitar</span>
            </button>
            <div id="scan-list" class="scan-list"></div>

            <form action="/save" method="POST">
                <div class="section-title">
                    <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
                        <path d="M5 12.55a11 11 0 0 1 14.08 0"></path>
                        <path d="M1.42 9a16 16 0 0 1 21.16 0"></path>
                        <line x1="12" y1="20" x2="12.01" y2="20"></line>
                    </svg>
                    Koneksi Wi-Fi Gudang
                </div>

                <div class="form-group">
                    <label for="ssid">Nama Jaringan (SSID)</label>
                    <div class="input-wrapper">
                        <input type="text" id="ssid" name="ssid" value=")rawliteral" + storedSSID + R"rawliteral(" placeholder="Contoh: Gudang_Utama" required>
                    </div>
                </div>

                <div class="form-group">
                    <label for="password">Kata Sandi Wi-Fi</label>
                    <div class="input-wrapper">
                        <input type="password" id="password" name="password" value=")rawliteral" + storedPass + R"rawliteral(" placeholder="Masukkan kata sandi">
                        <button type="button" class="toggle-btn" onclick="togglePass()">
                            <svg id="eye-icon" width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                                <path d="M1 12s4-8 11-8 11 8 11 8-4 8-11 8-11-8-11-8z"></path>
                                <circle cx="12" cy="12" r="3"></circle>
                            </svg>
                        </button>
                    </div>
                </div>

                <!-- Konfigurasi Broker MQTT (Collapsible) -->
                <div class="collapsible-header" onclick="toggleMqtt()">
                    <span>⚙️ Pengaturan MQTT Broker (Opsional)</span>
                    <span id="mqtt-arrow">▼</span>
                </div>
                <div id="mqtt-box" class="collapsible-content">
                    <div class="form-group">
                        <label for="broker">Host Broker MQTT</label>
                        <input type="text" id="broker" name="broker" value=")rawliteral" + storedBroker + R"rawliteral(" placeholder="broker.emqx.io">
                    </div>
                    <div class="form-group">
                        <label for="port">Port MQTT</label>
                        <input type="number" id="port" name="port" value=")rawliteral" + String(storedPort) + R"rawliteral(" placeholder="1883">
                    </div>
                </div>

                <button type="submit" class="btn-submit">
                    <svg width="18" height="18" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2.5" stroke-linecap="round" stroke-linejoin="round">
                        <path d="M19 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v11a2 2 0 0 1-2 2z"></path>
                        <polyline points="17 21 17 13 7 13 7 21"></polyline>
                        <polyline points="7 3 7 8 15 8"></polyline>
                    </svg>
                    SIMPAN & HUBUNGKAN
                </button>
            </form>
        </div>

        <div class="footer">
            Capstone IoT Small Warehouse System &copy; 2026
        </div>
    </div>

    <script>
        function togglePass() {
            const pass = document.getElementById('password');
            pass.type = (pass.type === 'password') ? 'text' : 'password';
        }

        function toggleMqtt() {
            const box = document.getElementById('mqtt-box');
            const arrow = document.getElementById('mqtt-arrow');
            if (box.style.display === 'block') {
                box.style.display = 'none';
                arrow.textContent = '▼';
            } else {
                box.style.display = 'block';
                arrow.textContent = '▲';
            }
        }

        function scanWifi() {
            const label = document.getElementById('scan-label');
            const list = document.getElementById('scan-list');
            label.textContent = "Sedang memindai...";
            list.style.display = 'block';
            list.innerHTML = "<div style='padding: 12px; font-size: 12px; color: #94a3b8; text-align: center;'>Mencari sinyal Wi-Fi...</div>";

            fetch('/scan')
                .then(res => res.json())
                .then(networks => {
                    label.textContent = "Pindai Ulang Wi-Fi";
                    if (networks.length === 0) {
                        list.innerHTML = "<div style='padding: 12px; font-size: 12px; color: #ef4444; text-align: center;'>Tidak ada Wi-Fi terdeteksi.</div>";
                        return;
                    }
                    let html = "";
                    networks.forEach(net => {
                        html += `<div class="scan-item" onclick="selectWifi('${net.ssid}')">
                                    <span><strong>${net.ssid}</strong> ${net.secure ? '🔒' : ''}</span>
                                    <span class="rssi-badge">${net.rssi} dBm</span>
                                 </div>`;
                    });
                    list.innerHTML = html;
                })
                .catch(err => {
                    label.textContent = "Pindai Gagal, Coba Lagi";
                    list.innerHTML = "<div style='padding: 12px; font-size: 12px; color: #ef4444; text-align: center;'>Gagal memindai jaringan.</div>";
                });
        }

        function selectWifi(ssid) {
            document.getElementById('ssid').value = ssid;
            document.getElementById('password').focus();
        }
    </script>
</body>
</html>
)rawliteral";

    server.send(200, "text/html", html);
}
