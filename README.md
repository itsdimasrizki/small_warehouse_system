# Small Warehouse Monitoring & Safety System (IoT)

Sistem pemantauan kapasitas rak gudang 3 lantai berbasis IoT secara *real-time* dan sistem proteksi dini keselamatan gudang terhadap risiko kebakaran atau suhu berlebih. 

Proyek ini menggunakan arsitektur **Dual ESP32 (Master & Slave)** yang terhubung melalui komunikasi **UART Hardware Serial (Serial2)** untuk menjamin performa maksimal tanpa perebutan sumber daya atau pin I/O.

---

## 📋 Arsitektur Sistem Dual ESP32

1. **ESP32 SLAVE (Dedicated Ultrasonic Array Reader):**
   * Mengontrol penuh 6 sensor ultrasonik HC-SR04 (Lantai 1, 2, 3 - Kiri & Kanan) dengan 12 pin GPIO independen (tidak ada lagi pin sharing).
   * **Anti-Crosstalk Polling:** Menggunakan delay transisi antara sisi KIRI dan KANAN sehingga gelombang pantulan akustik reda sepenuhnya.
   * **Simultaneous Vertical Ping:** Menembakkan ketiga sensor ultrasonik secara bersamaan untuk sisi yang aktif (Lantai 1, 2, dan 3 serentak), diukur presisi via GPIO Interrupts (`IRAM_ATTR`).
   * Mengirimkan paket data jarak 6 sensor berformat JSON ke Master melalui UART Serial2 (`115200` baud).

2. **ESP32 MASTER (Warehouse Controller & IoT Gateway):**
   * Menerima stream jarak sensor dari Slave via UART Serial2.
   * Menjalankan **RackFSM** untuk menghitung estimasi jumlah barang, rentang terisi, serta mengontrol **3x LED RGB** indikator rak (Hijau = Kosong, Biru = Terisi Sebagian, Merah = Penuh).
   * Membaca sensor lingkungan lokal: Suhu & Kelembapan (**DHT11**) dan Gas/Asap (**MQ-2**).
   * Menjalankan **SafetyFSM** dan mengontrol **Active Buzzer** alarm saat terjadi bahaya suhu atau gas.
   * Menghubungkan sistem ke **Wi-Fi** (dengan Captive Web Portal) dan mengirim telemetri serta menerima perintah kendali via **MQTT**.

---

## 🔌 Diagram Pengkabelan & Pinout Antar ESP

### 1. Interkoneksi UART (Master <--> Slave)
| ESP32 Master | ESP32 Slave | Keterangan |
| :--- | :--- | :--- |
| **GPIO 26 (RX)** | **GPIO 17 (TX)** | Jalur Data Slave -> Master (WAJIB COCOK) |
| **GPIO 27 (TX)** | **GPIO 16 (RX)** | Jalur Perintah Master -> Slave |
| **GND** | **GND** | **Wajib Terhubung (*Common Ground*)** |

### 2. Pinout ESP32 Slave (Sensor Ultrasonik HC-SR04)
| Komponen | Pin Modul | Pin ESP32 Slave | Tipe I/O |
| :--- | :--- | :--- | :--- |
| **Ultrasonik Lantai 1 Kiri** | TRIG / ECHO | `GPIO 14` / `GPIO 35` | Output / Input |
| **Ultrasonik Lantai 1 Kanan** | TRIG / ECHO | `GPIO 27` / `GPIO 34` | Output / Input |
| **Ultrasonik Lantai 2 Kiri** | TRIG / ECHO | `GPIO 4` / `GPIO 33`  | Output / Input |
| **Ultrasonik Lantai 2 Kanan** | TRIG / ECHO | `GPIO 13` / `GPIO 32` | Output / Input |
| **Ultrasonik Lantai 3 Kiri** | TRIG / ECHO | `GPIO 18` / `GPIO 26` | Output / Input |
| **Ultrasonik Lantai 3 Kanan** | TRIG / ECHO | `GPIO 5` / `GPIO 25`  | Output / Input |

### 3. Pinout ESP32 Master (Sensor Lingkungan, Aktuator & IoT)
| Komponen | Pin Modul | Pin ESP32 Master | Tipe I/O | Keterangan |
| :--- | :--- | :--- | :--- | :--- |
| **Sensor Suhu DHT11** | DATA | `GPIO 4` | Digital I/O | Resistor pull-up 10k ke 3.3V |
| **Sensor Asap MQ2** | A0 (Analog) | `GPIO 36` (VP) | Analog Input | ADC1 (Aman dengan Wi-Fi) |
| **Active Buzzer** | Positif (+) | `GPIO 13` | Digital Output | Active High |
| **LED RGB Rak 1 (Lantai 1)** | R / G / B | `GPIO 23` / `22` / `21` | Digital Output | Resistor 220 Ohm (Common Cathode / Kaki panjang ke GND) |
| **LED RGB Rak 2 (Lantai 2)** | R / G / B | `GPIO 19` / `18` / `5`  | Digital Output | Resistor 220 Ohm (Common Cathode / Kaki panjang ke GND) |
| **LED RGB Rak 3 (Lantai 3)** | R / G / B | `GPIO 17` / `16` / `14` | Digital Output | Resistor 220 Ohm (Common Cathode / Kaki panjang ke GND) |
---
## 📁 Struktur Direktori Proyek

```text
small_warehouse/
├── include/
│   ├── config/
│   │   ├── pins.h                # Definisi alokasi pin GPIO ESP32
│   │   ├── thresholds.h          # Ambang batas jarak, suhu, asap, & timer millis
│   │   └── credentials.h         # Konfigurasi Wi-Fi & Topik MQTT
│   ├── types/
│   │   └── system_state.h        # Struct SystemState, FloorData, & Enum FSM
│   ├── devices/                  # HEADER DRIVER HARDWARE (HAL)
│   │   ├── ultrasonic_array.h    # Driver 6x HC-SR04 sequential polling
│   │   ├── env_sensors.h         # Driver DHT11 & MQ2
│   │   ├── rgb_indicator.h       # Driver LED RGB (PWM)
│   │   └── alarm_buzzer.h        # Driver Active Buzzer
│   ├── fsm/                      # Header State Machine (Tahap selanjutnya)
│   └── network/                  # Header Komunikasi MQTT (Tahap selanjutnya)
├── src/
│   ├── devices/                  # IMPLEMENTASI DRIVER HARDWARE (.cpp)
│   │   ├── ultrasonic_array.cpp
│   │   ├── env_sensors.cpp
│   │   ├── rgb_indicator.cpp
│   │   └── alarm_buzzer.cpp
│   └── main.cpp                  # Program Diagnostik & Pengujian Sensor Real-time
├── examples/                     # SKETCH PENGUJIAN MANDIRI TERISOLASI
│   ├── test_ultrasonic/          # Uji khusus 6 sensor ultrasonik
│   ├── test_env_sensors/         # Uji khusus DHT11 & MQ2
│   └── test_rgb_buzzer/          # Uji khusus LED RGB & Buzzer
├── docs/
│   └── prd_panduan_capstone_platformio_small_warehouse_iot.md
├── platformio.ini                # Konfigurasi PlatformIO & dependensi library
└── .gitignore                    # Berkas yang diabaikan oleh Git
```

---

## 🛠 Panduan Command PlatformIO (Lengkap)

Jalankan perintah-perintah berikut di Terminal (VS Code Terminal atau terminal Linux/macOS/Windows):

### 1. Kompilasi Kode (Build / Verify)
Mengompilasi seluruh kode sumber dan memastikan tidak ada error sintaks atau library yang hilang:
```bash
pio run
```

### 2. Upload Firmware ke ESP32 (Pilih Target Environment)
Karena proyek ini memiliki 2 board ESP32 (Master & Slave), tentukan environment target saat upload:

* **Upload ke ESP32 Master:**
  ```bash
  pio run -e esp32-master -t upload
  ```
* **Upload ke ESP32 Slave:**
  ```bash
  pio run -e esp32-slave -t upload
  ```
*(Jika menggunakan port spesifik: tambahkan flag `--upload-port /dev/ttyUSB0` atau `COM3`)*

### 3. Membuka Serial Monitor
```bash
pio device monitor -b 115200
```
> **Catatan Keluar dari Serial Monitor:** Tekan kombinasi tombol `Ctrl + ]` atau `Ctrl + C`.

### 4. Upload & Langsung Buka Serial Monitor (All-in-One)
Perintah praktis sekali jalan untuk upload dan langsung memantau serial:
```bash
pio run -t upload && pio device monitor -b 115200
```

### 5. Deteksi Port Perangkat (Device List)
Melihat daftar port USB / Serial yang terdeteksi di komputer:
```bash
pio device list
```

### 6. Upload ke Port Spesifik (Jika Port Otomatis Salah)
Jika terdapat beberapa perangkat USB yang terhubung:
* **Linux:**
  ```bash
  pio run -t upload --upload-port /dev/ttyUSB0
  ```
* **Windows:**
  ```bash
  pio run -t upload --upload-port COM3
  ```

### 7. Membersihkan Cache Build (Clean)
Jika mengalami masalah caching atau linker error:
```bash
pio run --target clean
```

### 8. Memperbarui Library / Dependensi
Memeriksa dan memperbarui library yang tertera pada `platformio.ini`:
```bash
pio pkg update
```

---

## 🧪 Program Pengujian Sensor (Bring-Up & Diagnostics)

File `src/main.cpp` telah dikonfigurasi sebagai program pengujian komprehensif tanpa perlu koneksi internet/broker MQTT terlebih dahulu.

### Perilaku Saat ESP32 Dinyalakan:
1. **Self-Test Aktuator (Startup):**
   * Buzzer berbunyi 2x bip pendek.
   * 3x LED RGB menyala serentak: **MERAH** $\rightarrow$ **HIJAU** $\rightarrow$ **BIRU** $\rightarrow$ **MATI**.
2. **Pemantauan Real-time (Looping Non-Blocking):**
   * Membaca 1 sensor ultrasonik setiap $40\text{ ms}$ secara bergantian.
   * Membaca sensor suhu/kelembapan DHT11 dan gas MQ2 setiap $2\text{ detik}$.
   * Memperbarui warna ke-3 LED RGB secara independen per rak sesuai deteksi objek ($< 35\text{ cm}$).
   * Mencetak tabel diagnostik rapi ke Serial Monitor setiap $1\text{ detik}$.
### Perintah Interaktif Serial Monitor
Saat Serial Monitor terbuka (`pio device monitor -b 115200`), ketik huruf berikut lalu tekan **Enter**:
* `b` : Membunyikan Buzzer manual selama 300 ms.
* `r` : Menguji siklus warna LED RGB (Merah $\rightarrow$ Hijau $\rightarrow$ Biru).
* `c` : Membalik mode LED RGB antara **Common Cathode** dan **Common Anode**.
* `u` : Melakukan pembacaan langsung seluruh 6 sensor ultrasonik secara berurutan.
* `h` : Menampilkan menu bantuan perintah interaktif.

---

## 🔬 Sketch Pengujian Mandiri (Examples)

Tersedia sketch terpisah di dalam folder `examples/` jika ingin menguji satu komponen secara terisolasi tanpa merangkai seluruh sensor sekaligus:

1. **`examples/test_ultrasonic/test_ultrasonic.ino`**:
   * Khusus menguji 6 sensor ultrasonik HC-SR04 dengan jeda 40 ms.
2. **`examples/test_env_sensors/test_env_sensors.ino`**:
   * Khusus menguji pembacaan suhu/kelembapan DHT11 dan analog ADC MQ2.
3. **`examples/test_rgb_buzzer/test_rgb_buzzer.ino`**:
   * Khusus menguji aktuator output (LED RGB dan Active Buzzer).

---

## ⚠️ Troubleshooting & Solusi Masalah Hardware

| Masalah | Kemungkinan Penyebab | Solusi |
| :--- | :--- | :--- |
| **Permission Denied `/dev/ttyUSB0` (Linux)** | User belum masuk ke grup `dialout` | Jalankan: `sudo usermod -a -G dialout $USER` lalu logout dan login kembali. |
| **Output Serial Monitor Berantakan / Karakter Aneh** | Baud rate tidak cocok | Pastikan baud rate diset ke `115200` (`pio device monitor -b 115200`). |
| **Warna LED RGB Terbalik / Nyampur** | Perbedaan tipe LED Common Cathode (-) vs Common Anode (+) | Rangkaian menggunakan Common Cathode (kaki panjang ke GND). Default kode sudah diset `RgbIndicator rgbIndicator(false);`. Anda juga dapat menekan 'c' di Serial Monitor untuk beralih mode. |
| **Jarak Ultrasonik Selalu 0 cm** | Pin ECHO/TRIG terbalik atau tegangan 5V merusak pin ESP32 | Pastikan pin TRIG dan ECHO terhubung sesuai tabel pinout. Gunakan voltage divider resistor 1k & 2k pada jalur ECHO. |
| **DHT11 Selalu NaN / Error** | Resistor pull-up belum terpasang | Pasang resistor $10\text{ k}\Omega$ antara pin DATA DHT11 (`GPIO 4`) dan jalur $3.3\text{V}$. |
| **MQ2 Error atau Mengganggu Wi-Fi** | MQ2 dicolok ke pin ADC2 | Pastikan pin analog MQ2 selalu dicolok ke **GPIO 34** (ADC1). |
