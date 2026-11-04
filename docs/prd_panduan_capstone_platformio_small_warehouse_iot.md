# Product Requirements Document (PRD) & Capstone Guide
## Proyek: Small Warehouse Monitoring & Safety System (IoT)

* **Nama Proyek:** `small_warehouse`
* **Target Environment:** PlatformIO (VS Code), Framework Arduino (`espressif32`)
* **Target Mikrokontroler:** NodeMCU ESP32 (`nodemcu-32s`)
* **Arsitektur Perangkat Lunak:** Modular Multi-Folder berbasis Finite State Machine (FSM) & Non-Blocking I/O
* **Protokol Komunikasi:** MQTT over Wi-Fi (Format JSON)

---

## BAGIAN 1: Product Requirements Document (PRD)

### 1.1 Latar Belakang & Tujuan Sistem
Sistem **Small Warehouse IoT** dirancang untuk memantau kapasitas penyimpanan rak gudang 3 lantai secara *real-time* sekaligus menjaga keamanan ruangan dari risiko kebakaran atau suhu berlebih. 
Sistem ini memiliki tiga fungsi utama:
1. **Deteksi Kapasitas Presisi Tinggi:** Menggunakan 6 sensor ultrasonik (HC-SR04) yang dipasang saling berhadapan (Kiri & Kanan) pada 3 lantai rak untuk meminimalkan *blind spot* (1 zona per lantai).
2. **Visualisasi Status Gudang:** Menampilkan indikator ketersediaan rak secara keseluruhan melalui 1 LED RGB Utama (Hijau = Kosong, Biru = Terisi Sebagian, Merah = Penuh).
3. **Keamanan Lingkungan & Kontrol Alarm Dua Arah:** Memantau suhu/kelembapan (DHT11) dan asap/gas (MQ2). Jika melewati ambang batas aman, Buzzer menyala otomatis dan mengirimkan peringatan via MQTT. Operator juga dapat mematikan (*mute*) suara Buzzer dari jarak jauh melalui perintah MQTT.

---

### 1.2 Arsitektur Perangkat Keras & Alokasi Pin (GPIO Mapping)

Pemilihan pin pada NodeMCU ESP32 ini mematuhi aturan teknis arsitektur ESP32:
* **Aturan ADC & Wi-Fi:** Pin `ADC2` tidak dapat digunakan untuk membaca sinyal analog saat Wi-Fi aktif. Oleh karena itu, sensor analog MQ2 wajib ditempatkan pada jalur **ADC1 (`GPIO 34`)**.
* **Pemanfaatan Pin Input-Only:** Pin `GPIO 34, 35, 36, 39` adalah pin khusus *input* pada ESP32, sehingga dialokasikan untuk jalur `ECHO` ultrasonik dan output analog MQ2.

| Komponen | Pin Komponen | Pin ESP32 | Tipe Sinyal | Catatan Teknis |
| :--- | :--- | :--- | :--- | :--- |
| **Ultrasonik Lantai 1 Kiri** | TRIG / ECHO | GPIO 13 / GPIO 36 | Output / Input | Pin 36 (VP) khusus Input |
| **Ultrasonik Lantai 1 Kanan** | TRIG / ECHO | GPIO 14 / GPIO 39 | Output / Input | Pin 39 (VN) khusus Input |
| **Ultrasonik Lantai 2 Kiri** | TRIG / ECHO | GPIO 27 / GPIO 35 | Output / Input | Pin 35 khusus Input |
| **Ultrasonik Lantai 2 Kanan** | TRIG / ECHO | GPIO 26 / GPIO 32 | Output / Input | I/O Digital |
| **Ultrasonik Lantai 3 Kiri** | TRIG / ECHO | GPIO 25 / GPIO 33 | Output / Input | I/O Digital |
| **Ultrasonik Lantai 3 Kanan** | TRIG / ECHO | GPIO 17 / GPIO 16 | Output / Input | UART2 RX/TX bebas pakai |
| **Sensor Suhu DHT11** | DATA | GPIO 4 | Digital I/O | Butuh *pull-up* $10\text{ k}\Omega$ |
| **Sensor Asap MQ2** | A0 (Analog) | GPIO 34 | Analog (ADC1) | Aman dipakai bersama Wi-Fi |
| **LED RGB Utama** | R / G / B | GPIO 21 / 22 / 23 | PWM Output | Gunakan resistor $220\text{ }\Omega$ |
| **Buzzer Alarm** | Positif (+) | GPIO 18 | Digital Output | Active Buzzer $3.3\text{V}/5\text{V}$ |

> **Catatan Proteksi Tegangan:** Apabila HC-SR04 dan MQ2 diberi catu daya $5\text{V}$ (`VIN`), gunakan rangkaian pembagi tegangan (*voltage divider* resistor $1\text{ k}\Omega$ dan $2\text{ k}\Omega$) pada setiap jalur `ECHO` dan `A0` agar tegangan yang masuk ke GPIO ESP32 tidak melebihi batas aman $3.3\text{V}$.

---

### 1.3 Spesifikasi Logika & Finite State Machine (FSM)

#### A. FSM Kapasitas Rak (`rack_fsm`)
* Setiap lantai $i \in \{1, 2, 3\}$ memiliki sensor kiri ($d_{L,i}$) dan kanan ($d_{R,i}$) yang saling berhadapan.
* **Sequential Polling:** Keenam sensor dibaca secara bergantian dengan jeda waktu $\ge 40\text{ ms}$ antar sensor untuk mencegah interferensi gelombang ultrasonik (*cross-talk*).
* **Rumus Penentuan Slot Terisi:** Jika lebar rak memiliki ambang deteksi $T_{\text{rak}}$ (misalnya $35\text{ cm}$), maka status lantai ke-$i$ dinyatakan **TERISI** ($\text{Occupied}_i = 1$) apabila salah satu sensor membaca jarak di bawah $T_{\text{rak}}$ (dan $> 0\text{ cm}$):
  $$\text{Occupied}_i = (0 < d_{L,i} < T_{\text{rak}}) \lor (0 < d_{R,i} < T_{\text{rak}})$$
* Total lantai terisi dihitung dengan menjumlahkan seluruh lantai:
  $$N_{\text{terisi}} = \sum_{i=1}^{3} \text{Occupied}_i \quad \text{dengan } N_{\text{terisi}} \in \{0, 1, 2, 3\}$$

| State Kapasitas (`RackState`) | Kondisi Transisi ($N_{\text{terisi}}$) | Output LED RGB | Nilai PWM `(R, G, B)` |
| :--- | :--- | :--- | :--- |
| `STATE_RACK_EMPTY` | $N_{\text{terisi}} = 0$ ($0\%$) | **HIJAU** | `(0, 255, 0)` |
| `STATE_RACK_PARTIAL` | $N_{\text{terisi}} \in \{1, 2\}$ ($33\% - 66\%$) | **BIRU** | `(0, 0, 255)` |
| `STATE_RACK_FULL` | $N_{\text{terisi}} = 3$ ($100\%$) | **MERAH** | `(255, 0, 0)` |

#### B. FSM Keamanan & Alarm Buzzer (`safety_fsm`)
Kondisi bahaya (`hazard_detected = true`) aktif apabila suhu ruangan $T \ge 38.0^\circ\text{C}$ **ATAU** nilai analog asap $\text{ADC}_{\text{MQ2}} \ge 1800$ (skala $0 - 4095$).

| State Keamanan (`SafetyState`) | Pemicu Transisi Masuk | Status Buzzer | Catatan Perilaku |
| :--- | :--- | :--- | :--- |
| `STATE_SAFE_NORMAL` | Suhu & Asap di bawah ambang batas, tidak ada paksaan manual | `OFF` (`LOW`) | Mereset semua status *mute* sebelumnya |
| `STATE_ALARM_TRIGGERED` | `hazard_detected == true` saat di state normal | `ON` (`HIGH`) | Mengirim alert bahaya pada payload MQTT |
| `STATE_ALARM_SILENCED` | Menerima perintah MQTT `"MUTE"` atau `"OFF"` saat `STATE_ALARM_TRIGGERED` | `OFF` (`LOW`) | Buzzer mati meskipun bahaya masih ada; otomatis kembali ke `STATE_SAFE_NORMAL` jika bahaya reda |
| `STATE_MANUAL_TEST` | Menerima perintah MQTT `"ON"` | `ON` (`HIGH`) | Menyalakan Buzzer untuk uji coba hingga menerima perintah `"AUTO"` atau `"OFF"` |

---

### 1.4 Spesifikasi Arsitektur Direktori PlatformIO (Multi-Folder)

Untuk mempermudah *debugging* per komponen dan memisahkan *driver* alat dari logika bisnis (FSM), proyek dibagi menjadi sub-folder terstruktur di dalam `include/` dan `src/`:

```text
small_warehouse/
├── include/
│   ├── config/
│   │   ├── pins.h                # Definisi seluruh pin GPIO ESP32
│   │   ├── thresholds.h          # Konstanta batas jarak, suhu, asap, dan interval millis()
│   │   └── credentials.h         # SSID Wi-Fi, Password, Broker MQTT, dan daftar Topik
│   ├── types/
│   │   └── system_state.h        # Struct data global & Enum untuk FSM Rak dan FSM Safety
│   ├── devices/                  # HEADER DRIVER ALAT (Hardware Abstraction Layer)
│   │   ├── ultrasonic_array.h    # Deklarasi driver 6 sensor HC-SR04
│   │   ├── env_sensors.h         # Deklarasi driver DHT11 & MQ2
│   │   ├── rgb_indicator.h       # Deklarasi driver LED RGB Utama
│   │   └── alarm_buzzer.h        # Deklarasi driver Buzzer
│   ├── fsm/                      # HEADER FINITE STATE MACHINE (Logika Sistem)
│   │   ├── rack_fsm.h            # Deklarasi FSM Kapasitas Rak (Empty, Partial, Full)
│   │   └── safety_fsm.h          # Deklarasi FSM Bahaya & Override Buzzer MQTT
│   └── network/                  # HEADER JARINGAN & KOMUNIKASI
│       └── mqtt_manager.h        # Deklarasi Wi-Fi, MQTT Client, JSON Serializer & Callback
├── src/
│   ├── devices/                  # IMPLEMENTASI DRIVER ALAT (.cpp)
│   │   ├── ultrasonic_array.cpp  # Logika sequential polling 6 HC-SR04
│   │   ├── env_sensors.cpp       # Pembacaan DHT11 & ADC1 MQ2
│   │   ├── rgb_indicator.cpp     # Kontrol PWM warna Hijau, Biru, Merah
│   │   └── alarm_buzzer.cpp      # Kontrol pin digital/PWM Buzzer
│   ├── fsm/                      # IMPLEMENTASI FINITE STATE MACHINE (.cpp)
│   │   ├── rack_fsm.cpp          # Evaluasi OR kiri-kanan & transisi warna LED
│   │   └── safety_fsm.cpp        # Evaluasi bahaya & transisi mute/auto/manual Buzzer
│   ├── network/                  # IMPLEMENTASI JARINGAN (.cpp)
│   │   └── mqtt_manager.cpp      # Reconnect non-blocking, publish JSON, subscribe command
│   └── main.cpp                  # Orkestrasi setup() dan penjadwal millis() di loop()
└── platformio.ini                # Konfigurasi board & library dependencies
```

#### Keuntungan Struktur Ini Saat Debugging:
1. **Isolasi Masalah Fisik vs Logika:** Jika jarak ultrasonik salah baca, Anda hanya perlu memeriksa `src/devices/ultrasonic_array.cpp`. Jika jarak sudah benar tetapi warna LED salah berpindah, Anda cukup memeriksa `src/fsm/rack_fsm.cpp`.
2. **Kompilasi Lebih Cepat & Bersih:** Perubahan pada logika MQTT (`src/network/mqtt_manager.cpp`) tidak memaksa kompilasi ulang pada *driver* sensor.
3. **Kemudahan Kerja Tim / AI Co-Coding:** Anda dapat meminta AI membuat atau memperbaiki satu file spesifik tanpa merusak file lainnya.

---

## BAGIAN 2: Panduan Capstone Step-by-Step di PlatformIO (AI-Assisted Workflow)

Ikuti langkah-langkah di bawah ini secara berurutan. Setiap tahapan dilengkapi dengan **Prompt AI Siap Pakai** sehingga Anda dapat meminta AI men-generate kode secara bertahap sesuai struktur folder di atas.

---

### Langkah 1: Inisialisasi Proyek & Pembuatan Struktur Folder
1. Buka **VS Code** yang sudah terinstal ekstensi **PlatformIO IDE**.
2. Buka **PIO Home** $\rightarrow$ klik **+ New Project**.
   * **Name:** `small_warehouse`
   * **Board:** `NodeMCU-32S` (`nodemcu-32s`)
   * **Framework:** `Arduino`
3. Setelah proyek terbuka, buka **Terminal** di VS Code (`Ctrl + ~`) dan jalankan perintah berikut untuk membuat seluruh sub-folder sekaligus:
   * **Untuk Windows (PowerShell):**
     `mkdir include/config, include/types, include/devices, include/fsm, include/network, src/devices, src/fsm, src/network`
   * **Untuk Linux / macOS / Git Bash:**
     `mkdir -p include/{config,types,devices,fsm,network} src/{devices,fsm,network}`

---

### Langkah 2: Konfigurasi `platformio.ini` & File Konfigurasi Dasar (`include/config/` & `include/types/`)

Sebelum memprogram *driver* alat, siapkan fondasi konfigurasi proyek dan tipe data globalnya.

* **Target File yang Dibuat:**
  1. `platformio.ini`
  2. `include/config/pins.h`
  3. `include/config/thresholds.h`
  4. `include/config/credentials.h`
  5. `include/types/system_state.h`

> **Salin Prompt Berikut ke AI untuk Langkah 2:**
> *"Saya sedang membangun proyek PlatformIO (Arduino framework, board `nodemcu-32s`) bernama `small_warehouse`. Berdasarkan PRD saya, buatkan kode untuk 5 file fondasi berikut secara terpisah:
> 1. `platformio.ini` (gunakan library `DHT sensor library`, `Adafruit Unified Sensor`, `NewPing`, `PubSubClient`, dan `ArduinoJson`).
> 2. `include/config/pins.h` (berisi макро GPIO untuk 6 ultrasonik, DHT11 di GPIO 4, MQ2 di GPIO 34, LED RGB di GPIO 21/22/23, dan Buzzer di GPIO 18).
> 3. `include/config/thresholds.h` (batas deteksi rak 35 cm, suhu maks 38.0 C, ADC MQ2 maks 1800, interval baca ultrasonik 40 ms, interval baca suhu/asap 2000 ms, interval MQTT 2000 ms).
> 4. `include/config/credentials.h` (SSID Wi-Fi, Password, broker MQTT, topik telemetry `warehouse/small_warehouse/telemetry`, dan topik kontrol `warehouse/small_warehouse/control/buzzer`).
> 5. `include/types/system_state.h` (berisi enum `RackState`, enum `SafetyState`, struct `FloorData`, dan struct utama `SystemState` yang menampung seluruh data sensor dan status FSM)."*

---

### Langkah 3: Pembuatan Folder Alat / Hardware Drivers (`devices/`)

Pada tahap ini, fokus hanya pada cara kerja perangkat keras. Modul di dalam `devices/` **tidak boleh** mengandung logika FSM atau MQTT—tugasnya murni membaca sensor dan menggerakkan pin output.

* **Target File yang Dibuat:**
  1. `include/devices/ultrasonic_array.h` & `src/devices/ultrasonic_array.cpp`
  2. `include/devices/env_sensors.h` & `src/devices/env_sensors.cpp`
  3. `include/devices/rgb_indicator.h` & `src/devices/rgb_indicator.cpp`
  4. `include/devices/alarm_buzzer.h` & `src/devices/alarm_buzzer.cpp`

> **Salin Prompt Berikut ke AI untuk Langkah 3:**
> *"Lanjutkan proyek `small_warehouse` untuk membuat isi folder `devices/` (header di `include/devices/` dan implementasi di `src/devices/`). Buatkan 4 pasang file berikut:
> 1. `ultrasonic_array.h` & `.cpp`: Menggunakan library `NewPing` untuk membaca 6 sensor HC-SR04 secara bergantian (1 sensor per pemanggilan fungsi `pollNextSensor()`) agar bebas cross-talk dan non-blocking, lalu menyimpan jaraknya ke `SystemState`.
> 2. `env_sensors.h` & `.cpp`: Membaca suhu & kelembapan dari DHT11 serta nilai analog 12-bit (0-4095) dari MQ2 pada GPIO 34, lalu menyimpannya ke `SystemState`.
> 3. `rgb_indicator.h` & `.cpp`: Mengatur nyala warna LED RGB Utama (Hijau, Biru, Merah, atau Mati) menggunakan PWM.
> 4. `alarm_buzzer.h` & `.cpp`: Mengatur status nyala/mati pin Buzzer sesuai boolean yang diberikan."*

* **Checklist Debugging Tahap 3:**
  * Jalankan **PlatformIO: Build** (`Ctrl + Alt + B`) untuk memastikan tidak ada *missing header* atau *pin conflict* pada seluruh *driver* alat.

---

### Langkah 4: Pembuatan Folder Finite State Machine (`fsm/`)

Setelah semua alat memiliki *driver*, buat modul otak pengambil keputusan di dalam folder `fsm/`.

* **Target File yang Dibuat:**
  1. `include/fsm/rack_fsm.h` & `src/fsm/rack_fsm.cpp`
  2. `include/fsm/safety_fsm.h` & `src/fsm/safety_fsm.cpp`

> **Salin Prompt Berikut ke AI untuk Langkah 4:**
> *"Sekarang buatkan modul Finite State Machine (FSM) di folder `include/fsm/` dan `src/fsm/`:
> 1. `rack_fsm.h` & `.cpp`: Mengevaluasi data jarak kiri dan kanan pada 3 lantai di `SystemState` menggunakan logika OR ($d_L < 35\text{ cm} \lor d_R < 35\text{ cm}$), menghitung jumlah lantai terisi (0, 1-2, atau 3), memperbarui state (`STATE_RACK_EMPTY`, `STATE_RACK_PARTIAL`, `STATE_RACK_FULL`), dan memanggil driver `rgb_indicator` untuk mengubah warna LED ke Hijau, Biru, atau Merah.
> 2. `safety_fsm.h` & `.cpp`: Mengevaluasi data suhu DHT11 dan ADC MQ2 terhadap threshold. Mengatur transisi antar 4 state (`STATE_SAFE_NORMAL`, `STATE_ALARM_TRIGGERED`, `STATE_ALARM_SILENCED`, `STATE_MANUAL_TEST`), menyediakan fungsi `handleMqttCommand(String cmd)` untuk menerima perintah `MUTE`/`OFF`/`ON`/`AUTO`, melakukan auto-reset dari `SILENCED` ke `NORMAL` saat bahaya reda, dan memanggil driver `alarm_buzzer`."*

---

### Langkah 5: Pembuatan Folder Komunikasi Jaringan (`network/`)

Modul ini bertugas menghubungkan ESP32 ke Wi-Fi dan Broker MQTT, mem-parsing pesan masuk dari topik kontrol Buzzer untuk diteruskan ke `safety_fsm`, serta mem-publish kondisi `SystemState` dalam format JSON.

* **Target File yang Dibuat:**
  1. `include/network/mqtt_manager.h` & `src/network/mqtt_manager.cpp`

> **Salin Prompt Berikut ke AI untuk Langkah 5:**
> *"Buatkan modul jaringan di `include/network/mqtt_manager.h` dan `src/network/mqtt_manager.cpp` menggunakan `WiFi.h`, `PubSubClient`, dan `ArduinoJson` v7.
> Fitur yang wajib ada:
> 1. Koneksi Wi-Fi dan MQTT dengan mekanisme auto-reconnect non-blocking setiap 5 detik (jika Wi-Fi/MQTT putus, pembacaan sensor dan FSM lokal tidak boleh terhenti).
> 2. Fungsi `publishTelemetry(const SystemState &state)` yang mengemas data jarak 3 lantai, status kapasitas, warna LED, suhu, kelembapan, ADC MQ2, status bahaya, dan state FSM Buzzer ke JSON lalu mem-publish ke topik telemetry serta mencetaknya ke Serial Monitor.
> 3. Callback MQTT pada topik kontrol Buzzer yang meneruskan payload string (`MUTE`, `OFF`, `ON`, `AUTO`) langsung ke `SafetyFSM`."*

---

### Langkah 6: Orkestrasi `src/main.cpp` & Pengujian Integrasi

Langkah terakhir dalam pembuatan kode adalah menggabungkan seluruh modul di atas di dalam `src/main.cpp` menggunakan penjadwal waktu berbasis `millis()`.

* **Target File yang Dibuat:**
  1. `src/main.cpp`

> **Salin Prompt Berikut ke AI untuk Langkah 6:**
> *"Buatkan file `src/main.cpp` yang mengintegrasikan seluruh modul dari folder `config/`, `types/`, `devices/`, `fsm/`, dan `network/`. Gunakan timer non-blocking `millis()` di dalam `loop()` dengan jadwal berikut:
> - Setiap siklus loop: jalankan `mqttManager.loop()`.
> - Setiap 40 ms: baca 1 sensor ultrasonik berikutnya via `ultrasonicArray`, lalu jalankan `rackFsm.update()`.
> - Setiap 2000 ms: baca sensor DHT11 dan MQ2 via `envSensors`, lalu jalankan `safetyFsm.update()`.
> - Setiap 2000 ms: kirim data JSON via `mqttManager.publishTelemetry()`."*

---

### Langkah 7: Panduan Debugging Terisolasi & Verifikasi Capstone

Jika saat pengujian ditemukan kendala, gunakan tabel panduan *debugging* modular berikut untuk langsung menemukan file yang perlu diperbaiki tanpa membongkar keseluruhan kode:

| Gejala Masalah Saat Uji Coba | File Spesifik yang Perlu Diperiksa | Tindakan Perbaikan |
| :--- | :--- | :--- |
| Jarak salah satu sensor ultrasonik selalu `0` atau maksimum terus | `include/config/pins.h` & `src/devices/ultrasonic_array.cpp` | Cek kabel `TRIG`/`ECHO`, pastikan `GPIO 34, 35, 36, 39` hanya dipakai sebagai `ECHO` (Input), bukan `TRIG`. |
| Jarak terbaca benar di Serial Monitor, tetapi warna LED RGB terbalik (misal harusnya Hijau malah Ungu) | `src/devices/rgb_indicator.cpp` | Ubah konfigurasi tipe LED antara *Common Cathode* (`PWM = nilai`) dan *Common Anode* (`PWM = 255 - nilai`). |
| Barang sudah ditaruh di rak, jarak sudah $< 35\text{ cm}$, tetapi status lantai tetap `false` | `src/fsm/rack_fsm.cpp` & `include/config/thresholds.h` | Periksa konstanta `RACK_DETECT_CM` dan pastikan operator logika menggunakan `||` (OR) antara sensor kiri dan kanan. |
| Nilai MQ2 error atau Wi-Fi sering gagal konek saat sensor asap aktif | `include/config/pins.h` | Pastikan pin analog MQ2 tetap berada di jalur **ADC1** (`GPIO 32–39`), jangan pernah dipindah ke jalur ADC2. |
| Perintah `"MUTE"` dikirim dari MQTT Explorer tetapi Buzzer tetap berbunyi | `src/network/mqtt_manager.cpp` & `src/fsm/safety_fsm.cpp` | Cek apakah topik *subscribe* sudah cocok dan pastikan transisi state menuju `STATE_ALARM_SILENCED` langsung memanggil `buzzer.setState(false)`. |