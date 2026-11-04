# Small Warehouse Monitoring & Safety System (IoT)

Sistem pemantauan kapasitas rak gudang 3 lantai berbasis IoT secara *real-time* dan sistem proteksi dini keselamatan gudang terhadap risiko kebakaran atau suhu berlebih. Dibangun menggunakan mikrokontroler **NodeMCU ESP32** dengan framework **Arduino** pada environment **PlatformIO**.

---

## 📋 Daftar Isi
1. [Fitur Utama](#-fitur-utama)
2. [Spesifikasi Perangkat Keras & Pinout](#-spesifikasi-perangkat-keras--pinout)
3. [Struktur Direktori Proyek](#-struktur-direktori-proyek)
4. [Panduan Command PlatformIO (Lengkap)](#-panduan-command-platformio-lengkap)
5. [Program Pengujian Sensor (Bring-Up & Diagnostics)](#-program-pengujian-sensor-bring-up--diagnostics)
6. [Sketch Pengujian Mandiri (Examples)](#-sketch-pengujian-mandiri-examples)
7. [Troubleshooting & Solusi Masalah Hardware](#-troubleshooting--solusi-masalah-hardware)

---

## 🚀 Fitur Utama

1. **Deteksi Kapasitas Rak Presisi Tinggi:**
   * Memakai 6 sensor ultrasonik HC-SR04 (2 sensor saling berhadapan di setiap lantai: Kiri & Kanan) untuk meminimalkan *blind spot*.
   * Menggunakan mekanisme *Sequential Polling Non-Blocking* (jeda $\ge 40\text{ ms}$ antar sensor) untuk mencegah interferensi pantulan (*cross-talk*).
2. **Indikator Visual Kapasitas (LED RGB Utama):**
   * **HIJAU** (`0, 255, 0`): Seluruh rak kosong ($0\%$).
   * **BIRU** (`0, 0, 255`): Rak terisi sebagian ($33\% - 66\%$).
   * **MERAH** (`255, 0, 0`): Seluruh lantai rak penuh ($100\%$).
   * Mendukung mode *Common Cathode* (-) dan *Common Anode* (+).
3. **Sistem Proteksi Keselamatan Ruangan (Safety Alarm):**
   * Pemantauan suhu & kelembapan ruangan via DHT11 (Ambang bahaya: $T \ge 38.0^\circ\text{C}$).
   * Pemantauan kepekatan asap & gas via MQ2 pada jalur **ADC1** (Ambang bahaya: $\text{ADC} \ge 1800$).
   * Aktuasi Active Buzzer alarm secara otomatis jika bahaya terdeteksi.

---

## 🔌 Spesifikasi Perangkat Keras & Pinout

> [!IMPORTANT]
> - **Aturan ADC1 vs Wi-Fi:** Pin `GPIO 34` wajib digunakan untuk MQ2 karena berada di jalur **ADC1**. Jalur ADC2 tidak bisa membaca analog saat Wi-Fi ESP32 aktif.
> - **Pin Input-Only:** Pin `GPIO 34, 35, 36, 39` adalah pin khusus input (tanpa internal pull-up/pull-down).
> - **Proteksi Tegangan (Voltage Divider):** Apabila HC-SR04 diberi daya $5\text{V}$, gunakan pembagi tegangan (resistor $1\text{ k}\Omega$ & $2\text{ k}\Omega$) pada pin `ECHO` sebelum masuk ke GPIO ESP32 ($3.3\text{V}$).

| Komponen | Pin Modul | Pin GPIO ESP32 | Tipe I/O | Keterangan |
| :--- | :--- | :--- | :--- | :--- |
| **Ultrasonik Lantai 1 Kiri** | TRIG / ECHO | `GPIO 13` / `GPIO 36` | Output / Input | Pin 36 (VP) Input-Only |
| **Ultrasonik Lantai 1 Kanan** | TRIG / ECHO | `GPIO 14` / `GPIO 39` | Output / Input | Pin 39 (VN) Input-Only |
| **Ultrasonik Lantai 2 Kiri** | TRIG / ECHO | `GPIO 27` / `GPIO 35` | Output / Input | Pin 35 Input-Only |
| **Ultrasonik Lantai 2 Kanan** | TRIG / ECHO | `GPIO 26` / `GPIO 32` | Output / Input | I/O Digital Standar |
| **Ultrasonik Lantai 3 Kiri** | TRIG / ECHO | `GPIO 25` / `GPIO 33` | Output / Input | I/O Digital Standar |
| **Ultrasonik Lantai 3 Kanan** | TRIG / ECHO | `GPIO 17` / `GPIO 16` | Output / Input | GPIO Bebas Pakai |
| **Sensor Suhu DHT11** | DATA | `GPIO 4` | Digital I/O | Wajib pull-up $10\text{ k}\Omega$ ke $3.3\text{V}$ |
| **Sensor Asap MQ2** | A0 (Analog) | `GPIO 34` | Analog Input | ADC1 Channel 6 |
| **LED RGB Utama** | R / G / B | `GPIO 21` / `22` / `23` | PWM Output | Beri resistor seri $220\text{ }\Omega$ |
| **Active Buzzer** | Positif (+) | `GPIO 18` | Digital Output | Active High ($3.3\text{V}/5\text{V}$) |

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

### 2. Upload Firmware ke ESP32
Mengompilasi dan mengunggah binary firmware ke board ESP32 yang terhubung:
```bash
pio run --target upload
```
*Atau versi singkat:*
```bash
pio run -t upload
```

### 3. Membuka Serial Monitor
Membuka monitor serial untuk membaca log output dan dashboard data:
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
   * LED RGB menyala bergantian: **MERAH** $\rightarrow$ **HIJAU** $\rightarrow$ **BIRU** $\rightarrow$ **MATI**.
2. **Pemantauan Real-time (Looping Non-Blocking):**
   * Membaca 1 sensor ultrasonik setiap $40\text{ ms}$ secara bergantian.
   * Membaca sensor suhu/kelembapan DHT11 dan gas MQ2 setiap $2\text{ detik}$.
   * Memperbarui warna LED RGB sesuai jumlah lantai yang terdeteksi terisi barang ($< 35\text{ cm}$).
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
| **Warna LED RGB Terbalik (misal Hijau jadi Ungu)** | Perbedaan tipe LED Common Cathode (-) vs Common Anode (+) | Ketik `c` di Serial Monitor, atau ubah parameter constructor di kode: `RgbIndicator rgbIndicator(true);`. |
| **Jarak Ultrasonik Selalu 0 cm** | Pin ECHO/TRIG terbalik atau tegangan 5V merusak pin ESP32 | Pastikan pin TRIG dan ECHO terhubung sesuai tabel pinout. Gunakan voltage divider resistor 1k & 2k pada jalur ECHO. |
| **DHT11 Selalu NaN / Error** | Resistor pull-up belum terpasang | Pasang resistor $10\text{ k}\Omega$ antara pin DATA DHT11 (`GPIO 4`) dan jalur $3.3\text{V}$. |
| **MQ2 Error atau Mengganggu Wi-Fi** | MQ2 dicolok ke pin ADC2 | Pastikan pin analog MQ2 selalu dicolok ke **GPIO 34** (ADC1). |
