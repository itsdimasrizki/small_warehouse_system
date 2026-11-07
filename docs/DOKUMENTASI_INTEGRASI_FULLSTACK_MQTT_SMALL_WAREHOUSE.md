# PANDUAN INTEGRASI MQTT & DOKUMENTASI SISTEM SMALL WAREHOUSE IoT
> **Dokumen Panduan Pengembang (Frontend / Backend / Fullstack Developer)**  
> Versi Sistem: 1.0 (Stable) | Framework: ESP32 Arduino Core | Format Data: JSON over MQTT

---

## 1. Ringkasan Eksekutif & Arsitektur Sistem

Sistem **Small Warehouse IoT** adalah sistem pemantauan rak penyimpanan gudang 3 lantai berbasis mikrokontroler NodeMCU ESP32. Sistem ini memiliki 3 fungsi inti:
1. **Deteksi Okupansi & Estimasi Jumlah Barang 3 Lantai:** Menggunakan 6 sensor ultrasonik (HC-SR04) yang dipasang berhadapan (Kiri & Kanan) di setiap lantai untuk mengukur jarak, panjang ruang terisi, dan estimasi unit barang.
2. **Monitoring Keamanan Lingkungan:** Menggunakan sensor suhu/kelembapan DHT11 dan sensor gas/asap MQ-2 untuk mendeteksi dini bahaya kebakaran atau kebocoran gas.
3. **Komunikasi Dua Arah Real-time (MQTT):**
   * **Hardware $\rightarrow$ Web (Publish Telemetri):** Mengirimkan data sensor, status FSM, dan aktuator secara non-blocking setiap 2 detik dalam format JSON.
   * **Web $\rightarrow$ Hardware (Subscribe Kontrol):** Menerima perintah dari operator/dashboard untuk mendiamkan alarm (*MUTE*), uji coba manual (*ON*), atau mode otomatis (*AUTO*).

---

## 2. Parameter Fisik Rak & Konvensi Perhitungan

Informasi ini penting dipahami oleh tim web untuk rendering visual rak di dashboard:

| Lantai | Tinggi Fisik ($T$) | Panjang Fisik ($P$) | Threshold Deteksi ($P/2$) | Sisi Sensor |
| :---: | :---: | :---: | :---: | :---: |
| **Lantai 1** | $20.0\text{ cm}$ | $48.0\text{ cm}$ | **$24.0\text{ cm}$** | Kiri ($d_L$) & Kanan ($d_R$) |
| **Lantai 2** | $20.0\text{ cm}$ | $48.0\text{ cm}$ | **$24.0\text{ cm}$** | Kiri ($d_L$) & Kanan ($d_R$) |
| **Lantai 3** | $25.0\text{ cm}$ | $48.0\text{ cm}$ | **$24.0\text{ cm}$** | Kiri ($d_L$) & Kanan ($d_R$) |

### Rumus Matematis di Mikrokontroler:
* **Rentang Panjang Terisi (`occupied_span_cm`):**
  $$\text{occupied\_span} = 48.0 - (d_L + d_R)$$
  *(Jika rak kosong, span = $0.0\text{ cm}$. Jika terisi, span di-clamp antara $12.0\text{ cm}$ hingga $48.0\text{ cm}$)*.
* **Estimasi Jumlah Barang (`item_count`):**
  Menggunakan lebar referensi kardus standar $W = 12.0\text{ cm}$:
  $$\text{item\_count} = \text{round}\left(\frac{\text{occupied\_span}}{12.0}\right)$$
* **SOP Pergudangan:** Barang dimasukkan secara berurutan (*Sequential Stacking / FIFO*) menempel dari satu sisi (kiri ke kanan atau kanan ke kiri) untuk menjamin akurasi 100% tanpa celah buta (*zero blind spot*).

---

## 3. Spesifikasi Broker & Jaringan MQTT

Sistem saat ini dikonfigurasi ke public broker gratis (EMQX). Untuk production, fullstack dapat menggantinya dengan broker privat (Mosquitto, HiveMQ, atau EMQX Cloud).

| Parameter | Pengaturan Hardware (TCP) | Pengaturan Web / Browser (WebSocket) |
| :--- | :--- | :--- |
| **Broker Host** | `broker.emqx.io` | `broker.emqx.io` |
| **Protokol** | MQTT over TCP (Raw) | **MQTT over WebSockets (WS / WSS)** |
| **Port** | `1883` | **`8083` (WS non-SSL) / `8084` (WSS SSL)** |
| **WebSocket Path** | *-* | `/mqtt` |
| **Autentikasi** | Anonim / Tanpa User-Pass *(default)* | Anonim / Tanpa User-Pass *(default)* |
| **Topik Telemetri** | `warehouse/small_warehouse/telemetry` | *(Web melakukan **SUBSCRIBE**)* |
| **Topik Kontrol** | `warehouse/small_warehouse/control/buzzer` | *(Web melakukan **PUBLISH**)* |

> ⚠️ **CATATAN KRUSIAL UNTUK WEB DEVELOPER:**  
> Browser JavaScript **TIDAK BISA** menggunakan port TCP `1883`. Browser **WAJIB** menggunakan jalur **WebSocket** (`ws://broker.emqx.io:8083/mqtt` atau `wss://broker.emqx.io:8084/mqtt`).

---

## 4. Spesifikasi Topik 1: Telemetri Hardware $\rightarrow$ Web (Publish)

* **Topik:** `warehouse/small_warehouse/telemetry`
* **Frekuensi:** Dikirim otomatis setiap **2000 ms (2 detik)** secara non-blocking.
* **Format:** JSON String (UTF-8, max 1024 bytes).

### Contoh Payload JSON Aktual:
```json
{
  "floors": [
    {
      "floor": 1,
      "distance_left_cm": 12.0,
      "distance_right_cm": 15.2,
      "occupied_span_cm": 20.8,
      "item_count": 2,
      "is_occupied": true,
      "detected_sides": 2,
      "status": "FULL",
      "led_color": "GREEN"
    },
    {
      "floor": 2,
      "distance_left_cm": 18.0,
      "distance_right_cm": 48.0,
      "occupied_span_cm": 12.0,
      "item_count": 1,
      "is_occupied": true,
      "detected_sides": 1,
      "status": "PARTIAL",
      "led_color": "BLUE"
    },
    {
      "floor": 3,
      "distance_left_cm": 48.0,
      "distance_right_cm": 48.0,
      "occupied_span_cm": 0.0,
      "item_count": 0,
      "is_occupied": false,
      "detected_sides": 0,
      "status": "EMPTY",
      "led_color": "RED"
    }
  ],
  "summary": {
    "total_items": 3,
    "occupied_floors": 2,
    "warehouse_status": "PARTIAL"
  },
  "environment": {
    "temperature_c": 29.5,
    "humidity_pct": 65.0,
    "gas_raw": 680,
    "gas_pct": 16.6,
    "hazard_detected": false,
    "safety_state": "SAFE_NORMAL"
  },
  "actuators": {
    "buzzer": false
  }
}
```

### Kamus Data (Field Dictionary):

#### A. Array `floors` (Data per Lantai Rak, Indeks 0 s.d. 2)
| Key | Tipe Data | Satuan / Nilai | Penjelasan untuk Frontend |
| :--- | :---: | :---: | :--- |
| `floor` | `int` | `1`, `2`, `3` | Nomor lantai rak gudang. |
| `distance_left_cm` | `float` | `cm` (0.0 – 200.0) | Jarak pantulan sensor ultrasonik Kiri ke objek terdekat. |
| `distance_right_cm`| `float` | `cm` (0.0 – 200.0) | Jarak pantulan sensor ultrasonik Kanan ke objek terdekat. |
| `occupied_span_cm` | `float` | `cm` (0.0 – 48.0) | Estimasi panjang fisik rak yang sedang terpakai oleh barang. Cocok untuk progress bar kapasitas rak (misal: `24 cm / 48 cm`). |
| `item_count` | `int` | `unit` ($\ge 0$) | Estimasi jumlah unit paket/kardus di lantai tersebut (Opsi B matematis). |
| `is_occupied` | `bool` | `true` / `false` | Status apakah lantai memiliki barang (`true` jika ada deteksi $< 24\text{ cm}$). |
| `detected_sides` | `int` | `0`, `1`, `2` | Jumlah sisi sensor yang mendeteksi objek. |
| `status` | `string` | `"EMPTY"`, `"PARTIAL"`, `"FULL"` | State FSM kapasitas lantai: <br>• `EMPTY`: Kosong (0 deteksi)<br>• `PARTIAL`: Terisi Sebagian (1 sisi deteksi)<br>• `FULL`: Terisi Penuh (2 sisi deteksi). |
| `led_color` | `string` | `"GREEN"`, `"BLUE"`, `"RED"`, `"RED_BLINKING"` | Warna indikator LED fisik di rak saat ini: <br>• `GREEN`: Penuh (Full)<br>• `BLUE`: Terisi Sebagian (Partial)<br>• `RED`: Kosong (Empty)<br>• `RED_BLINKING`: Bahaya Suhu/Gas Melebihi Threshold. Frontend dapat mewarnai badge rak sesuai key ini. |

#### B. Objek `summary` (Agregasi Seluruh Gudang)
| Key | Tipe Data | Nilai | Penjelasan untuk Frontend |
| :--- | :---: | :---: | :--- |
| `total_items` | `int` | $\ge 0$ | Total estimasi seluruh barang di rak 1 + rak 2 + rak 3. |
| `occupied_floors`| `int` | `0`, `1`, `2`, `3` | Jumlah lantai rak yang berstatus terisi (`is_occupied == true`). |
| `warehouse_status`| `string`| `"EMPTY"`, `"PARTIAL"`, `"FULL"` | Status FSM global gudang: `EMPTY` (0 lantai), `PARTIAL` (1-2 lantai), `FULL` (3 lantai terisi). |

#### C. Objek `environment` (Sensor DHT11 & MQ-2)
| Key | Tipe Data | Nilai | Batas Normal vs Bahaya | Penjelasan untuk Frontend |
| :--- | :---: | :---: | :---: | :--- |
| `temperature_c` | `float` | $^\circ\text{C}$ | $< 38.0^\circ\text{C}$ = Normal<br>$\ge 38.0^\circ\text{C}$ = **BAHAYA** | Suhu ruangan gudang dari sensor DHT11. |
| `humidity_pct` | `float` | $\%$RH ($0 - 100$) | Normal: $40 - 70\%$ | Kelembapan relatif ruangan dari DHT11. |
| `gas_raw` | `int` | $0 - 4095$ | $< 1800$ = Aman<br>$\ge 1800$ = **BAHAYA** | Nilai ADC 12-bit mentah dari sensor asap/gas MQ-2. |
| `gas_pct` | `float` | $\%$ ($0.0 - 100.0$) | $< 44.0\%$ = Aman<br>$\ge 44.0\%$ = **BAHAYA** | Persentase densitas gas ($(\text{ADC}/4095) \times 100\%$). Sangat cocok untuk gauge meter di web. |
| `hazard_detected`| `bool` | `true` / `false` | `true` jika suhu $\ge 38^\circ\text{C}$ ATAU gas $\ge 1800$ | Trigger utama alarm bahaya sistem. |
| `safety_state` | `string` | 4 Nilai Enum | Lihat tabel FSM Keamanan di bawah | Status FSM keamanan ruangan. |

##### Tabel Enum `safety_state`:
1. `"SAFE_NORMAL"`: Ruangan aman, suhu & gas normal, buzzer MATI.
2. `"ALARM_TRIGGERED"`: Kondisi bahaya aktif! Suhu/gas melebihi batas, buzzer berbunyi pulsa cepat non-blocking.
3. `"ALARM_SILENCED"`: Bahaya masih ada, namun operator telah mengirim perintah `"MUTE"` dari web. Buzzer didiamkan agar tidak bising saat evakuasi/penanganan. Sistem otomatis kembali ke `"SAFE_NORMAL"` saat suhu/gas normal kembali.
4. `"MANUAL_TEST"`: Operator menguji buzzer secara paksa via web (`"ON"`).

#### D. Objek `actuators`
| Key | Tipe Data | Nilai | Penjelasan |
| :--- | :---: | :---: | :--- |
| `buzzer` | `bool` | `true` / `false` | Status fisik pin Buzzer saat ini (`true` = sedang bunyi, `false` = mati). |

---

## 5. Spesifikasi Topik 2: Kontrol Web $\rightarrow$ Hardware (Subscribe)

Web dapat mengirimkan string perintah (*plain text string*) ke topik kontrol untuk mengatur status buzzer alarm.

* **Topik:** `warehouse/small_warehouse/control/buzzer`
* **Format:** String UTF-8 sederhana (case-insensitive, otomatis diubah ke uppercase oleh ESP32).

### Daftar Perintah yang Didukung:
| Payload String | Aksi yang Dijalankan Hardware | Skenario Penggunaan di Web |
| :--- | :--- | :--- |
| `"MUTE"` atau `"OFF"` | Mematikan suara Buzzer saat bahaya sedang aktif (`STATE_ALARM_SILENCED`). | Tombol *"Diamkan Alarm / Mute"* saat terjadi insiden kebakaran/gas. |
| `"ON"` | Menyalakan Buzzer secara terus-menerus (`STATE_MANUAL_TEST`). | Tombol *"Tes Sirine Gudang"* untuk pengujian sistem alarm. |
| `"AUTO"` | Mengembalikan sistem ke mode pemantauan otomatis (`STATE_SAFE_NORMAL`). | Tombol *"Reset Alarm ke Otomatis"*. |

---

## 6. Tutorial Setup & Integrasi MQTT di Sisi Web (Fullstack Guide)

### Langkah 1: Pengujian Cepat Tanpa Koding (Verifikasi Broker)
Sebelum coding web, Anda bisa memverifikasi bahwa data ESP32 masuk ke broker:
1. Buka web client publik EMQX: **[http://www.emqx.com/en/mqtt/online-mqtt-client](http://www.emqx.com/en/mqtt/online-mqtt-client)** (atau gunakan aplikasi **MQTT Explorer**).
2. Klik **Connect** ke host `broker.emqx.io`, port `8083` (atau `8084` SSL).
3. Di panel **Subscription**, tambahkan topik: `warehouse/small_warehouse/telemetry` $\rightarrow$ Klik **Subscribe**.
4. Anda akan melihat pesan JSON dari ESP32 masuk setiap 2 detik.
5. Di panel **Publish**, kirim topik `warehouse/small_warehouse/control/buzzer` dengan payload `"ON"` atau `"MUTE"` untuk menguji respon alat.

---

### Langkah 2: Contoh Kode Implementasi Web (Node.js / React / Vue / Vanilla JS)

Gunakan library standar industri **`mqtt` (MQTT.js)**:
```bash
npm install mqtt
```

#### Contoh Kode Siap Pakai (JavaScript / TypeScript):

```javascript
import mqtt from 'mqtt';

// 1. Konfigurasi Koneksi WebSocket ke Broker
const BROKER_URL = 'wss://broker.emqx.io:8084/mqtt'; // Port WSS SSL aman
const TOPIC_TELEMETRY = 'warehouse/small_warehouse/telemetry';
const TOPIC_BUZZER_CTRL = 'warehouse/small_warehouse/control/buzzer';

const options = {
    clean: true,
    connectTimeout: 4000,
    clientId: 'web_dashboard_' + Math.random().toString(16).substring(2, 10),
};

console.log('[MQTT] Menghubungkan ke broker via WebSockets...');
const client = mqtt.connect(BROKER_URL, options);

// 2. Event saat Berhasil Terhubung
client.on('connect', () => {
    console.log('[MQTT] Berhasil terhubung ke Broker!');
    
    // Subscribe ke data telemetri gudang
    client.subscribe(TOPIC_TELEMETRY, (err) => {
        if (!err) {
            console.log(`[MQTT] Berhasil subscribe ke topik: ${TOPIC_TELEMETRY}`);
        } else {
            console.error('[MQTT] Gagal subscribe:', err);
        }
    });
});

// 3. Event Penerimaan Data Telemetri dari ESP32
client.on('message', (topic, payload) => {
    if (topic === TOPIC_TELEMETRY) {
        try {
            const data = JSON.parse(payload.toString());
            
            // --- CONTOH KONSUMSI DATA DI STATE DASHBOARD / REACT / VUE ---
            console.log('--- DATA TELEMETRI DITERIMA ---');
            console.log(`Total Barang di Gudang : ${data.summary.total_items} unit`);
            console.log(`Status Okupansi Global : ${data.summary.warehouse_status}`);
            console.log(`Suhu Ruangan           : ${data.environment.temperature_c} °C`);
            console.log(`Kelembapan Ruangan     : ${data.environment.humidity_pct} %`);
            console.log(`Konsentrasi Asap/Gas   : ${data.environment.gas_pct} % (ADC: ${data.environment.gas_raw})`);
            console.log(`Status Bahaya          : ${data.environment.hazard_detected ? 'BAHAYA AKTIF!' : 'AMAN'}`);
            console.log(`FSM Keamanan           : ${data.environment.safety_state}`);

            // Iterasi per lantai rak
            data.floors.forEach((f) => {
                console.log(` * Lantai ${f.floor}: ${f.status} (${f.led_color}) | ` +
                            `Kiri: ${f.distance_left_cm} cm | Kanan: ${f.distance_right_cm} cm | ` +
                            `Terisi: ${f.occupied_span_cm} cm (${f.item_count} barang)`);
            });

            // Update UI/State React/Vue Anda di sini:
            // setWarehouseData(data);

        } catch (error) {
            console.error('[MQTT] Gagal parsing JSON payload:', error);
        }
    }
});

// 4. Fungsi Mengirim Perintah Kontrol Buzzer (Tombol di Web)
export function sendBuzzerCommand(command) {
    // Valid command: "MUTE", "OFF", "ON", "AUTO"
    if (!client.connected) {
        console.warn('[MQTT] Klien belum terhubung ke broker!');
        return;
    }
    client.publish(TOPIC_BUZZER_CTRL, command.toUpperCase(), { qos: 0 }, (err) => {
        if (!err) {
            console.log(`[MQTT KONTROL] Perintah '${command}' berhasil dikirim ke hardware.`);
        } else {
            console.error('[MQTT KONTROL] Gagal mengirim perintah:', err);
        }
    });
}

// 5. Penanganan Error & Reconnect
client.on('error', (err) => {
    console.error('[MQTT ERROR]:', err);
});

client.on('reconnect', () => {
    console.log('[MQTT] Mencoba menghubungkan ulang ke broker...');
});
```

---

### Langkah 3: Contoh Integrasi UI Rekomendasi (Mockup Logic)

1. **Komponen Visualisasi Rak 3 Lantai:**
   * Gunakan `data.floors[i].led_color` untuk mengubah warna card/lampu rak:
     * `"GREEN"` $\rightarrow$ CSS Badge Hijau (`#10b981`), Label: *"Penuh"*
     * `"BLUE"` $\rightarrow$ CSS Badge Biru (`#0284c7`), Label: *"Terisi Sebagian"*
     * `"RED"` $\rightarrow$ CSS Badge Merah (`#ef4444`), Label: *"Kosong (0 barang)"*
     * `"RED_BLINKING"` $\rightarrow$ CSS Animasi Kedip Merah, Label: *"Bahaya Lingkungan!"*
   * Gunakan `data.floors[i].occupied_span_cm` untuk Progress Bar:
     $$\text{Width \%} = \left(\frac{\text{occupied\_span\_cm}}{48.0}\right) \times 100\%$$
2. **Komponen Banner Bahaya (Emergency Alert):**
   * Jika `data.environment.hazard_detected === true`:
     Tampilkan modal/banner merah berkedip:
     > ⚠️ **PERINGATAN BAHAYA GUDANG AKTIF!**  
     > Suhu: `${temperature_c} °C` | Asap: `${gas_pct} %`  
     > `[TOMBOL: MUTE ALARM]` `[TOMBOL: PANIC SIREN ON]`
   * Tombol *MUTE ALARM* memanggil `sendBuzzerCommand('MUTE')`.
   * Tombol *PANIC SIREN ON* memanggil `sendBuzzerCommand('ON')`.

---

## 7. Fitur Tambahan: Setup Wi-Fi Tanpa Hardcode (Captive Portal)

Jika alat ESP32 dibawa ke lokasi baru atau Wi-Fi gudang berganti:
1. Nyalakan ESP32. Jika Wi-Fi lama tidak terdeteksi dalam 15 detik, ESP32 otomatis memancarkan Wi-Fi AP:
   * **SSID:** `Warehouse-Rak-XXXX` *(tanpa sandi)*.
2. Sambungkan HP atau Laptop ke Wi-Fi tersebut. Layar konfigurasi bertema **Warehouse Rak Pintar** otomatis muncul (atau buka browser ke `http://192.168.4.1/`).
3. Pilih Wi-Fi gudang baru, masukkan password, lalu klik **"Simpan & Hubungkan"**.
4. ESP32 otomatis restart dan langsung terhubung ke Wi-Fi baru serta broker MQTT.

---

## 8. Ringkasan Topik & Payload (Cheat Sheet)

```
[PUBLISH]   Topik : warehouse/small_warehouse/telemetry
            Jadwal: Setiap 2000 ms
            Format: JSON (Lihat Bagian 4)

[SUBSCRIBE] Topik : warehouse/small_warehouse/control/buzzer
            Format: Plain String ("MUTE", "OFF", "ON", "AUTO")
```

Dokumentasi ini siap diserahkan langsung ke tim Fullstack / Frontend Developer. Jika tim web membutuhkan penyesuaian nama field (*key rename*), sistem hardware siap dikonfigurasi ulang dengan cepat.
