# ESP32-C6 Outdoor GPS Tracker — Kế Hoạch Phát Triển

> **Board:** Waveshare ESP32-C6-Touch-AMOLED-1.64
> **Framework:** PlatformIO + Arduino
> **Ngày bắt đầu:** 2026-09-26

---

## Thông Số Phần Cứng Xác Nhận (Thực tế)

| Thành phần | Thông số |
|---|---|
| CPU | ESP32-C6 RISC-V single-core @ 160 MHz |
| SRAM | **512 KB on-chip — KHÔNG CÓ PSRAM ngoài** |
| Flash | 16 MB |
| Màn hình | AMOLED 1.64", **280 × 456 px**, giao tiếp QSPI |
| Touch | FT3168 (I2C, địa chỉ `0x38` vật lý / `0x7E` register) |
| IMU | QMI8658 (I2C, địa chỉ `0x6B`, tích hợp sẵn) |
| GPS | ATGM336H (ngoài, UART1) |
| Áp suất | BMP580 (ngoài, I2C) |
| Storage | Khe Micro SD tích hợp |
| Nguồn | Mạch sạc pin Li-Po 3.7V tích hợp |

---

## Sơ Đồ Chân (Pin Map) Đã Xác Nhận

### QSPI Display (Driver SH8601)

| Tín hiệu | GPIO |
|---|---|
| CS | 10 |
| CLK | 11 |
| D0 | 4 |
| D1 | 5 |
| D2 | 7 |
| D3 | 19 |
| RST | **20** |

### I2C Bus (SDA=18, SCL=8)

### GPS UART1 — ATGM336H

| Tín hiệu | GPIO | Ghi chú |
|---|---|---|
| RX (← nhận từ GPS TX) | **GPIO 2** | Dây từ ATGM336H TX |
| TX (→ gửi tới GPS RX) | **GPIO 3** | Thường không cần dùng |

> ⚠️ **CẢNH BÁO QUAN TRỌNG — GPIO16 / GPIO17 BỊ CẤM:**
> Hai chân này được ESP32-C6 dùng nội bộ cho LP_UART (Low-Power UART).
> Khi gán UART1 vào GPIO16/17, hàm `uart_driver_install()` xung đột ngay lúc boot
> → **WDT crash loop, board reboot liên tục**. Đây là lỗi đã được tái hiện và xác nhận.

---

## Địa Chỉ I2C Thực Tế (Bus Scan xác nhận)

| Địa chỉ | Thiết bị | Ghi chú |
|---|---|---|
| `0x38` | FT3168 Touch controller | Địa chỉ vật lý thực; driver còn map `0x7E` |
| `0x47` | BMP580 Barometer | SDO nối VCC → địa chỉ `0x47` (không phải `0x46`) |
| `0x6B` | QMI8658 IMU built-in | 6-axis: accelerometer + gyroscope |
| `0x7E` | FT3168 / PMIC phụ | Có thể là PMIC hoặc touch register phụ |

---

## Ghi Chú LVGL — Giới Hạn SRAM ESP32-C6

> **KHÔNG CÓ PSRAM** — toàn bộ LVGL buffer phải nằm trong 512 KB SRAM on-chip.

- **Draw buffer tối đa:** ~1/10 màn hình = **280 × 45 px** (~49 KB/buffer, dùng double-buffer)
- **Nền `#000000` (đen tuyệt đối):** tối ưu cho AMOLED — pixel đen = pixel tắt = tiết kiệm điện
- **Khai báo font:** Phải bật ở CẢ HAI nơi:
  - `lv_conf.h`: `#define LV_FONT_MONTSERRAT_xx 1`
  - `platformio.ini`: `-DLV_FONT_MONTSERRAT_xx=1`
  - Thiếu một trong hai → linker error `undefined reference to 'lv_font_montserrat_xx'`
- **Thread safety:** Mọi thao tác LVGL từ ngoài LVGL task phải bọc trong `example_lvgl_lock(-1)` / `example_lvgl_unlock()`
- **`lv_label_set_text_fmt` + `%f`** bị vô hiệu hóa trong build Arduino LVGL để tiết kiệm RAM → dùng `snprintf()` + `lv_label_set_text()` thay thế

---

## Thư Viện Sử Dụng

| Thư viện | Phiên bản | Mục đích |
|---|---|---|
| `TinyGPSPlus` | latest | Parse NMEA từ ATGM336H |
| `Adafruit BMP5xx Library` | `^1.0.2` | Driver BMP580 — thay thế Adafruit_BMP3XX |
| `Adafruit BusIO` | `^1.16.1` | I2C/SPI HAL |
| `Adafruit Unified Sensor` | `^1.1.14` | Sensor abstraction |
| `lewisxhe/SensorLib` | `^0.5.0` | Driver QMI8658 (`SensorQMI8658.hpp`) |
| `lvgl/lvgl` | `^8.4.0` | Graphics UI framework |
| Waveshare AMOLED BSP | local `lib/` | QSPI display + LVGL glue (`lcd_bsp.h`) |

> **Lưu ý:** `Adafruit_BMP3XX` KHÔNG tương thích BMP580 — lib đó chỉ nhận chip ID `0x50`/`0x60` (BMP388/390).
> Hàm `bmp.pressure` của BMP5xx trả về **hPa trực tiếp** — không cần chia 100.

---

## platformio.ini Key Settings

```ini
[env:esp32c6_tracker]
platform = espressif32
board = esp32-c6-devkitc-1
framework = arduino
monitor_dtr = 0    ; QUAN TRỌNG: ngăn Serial Monitor reset board khi mở
monitor_rts = 0
board_build.flash_size = 16MB

build_flags =
    -DGPS_RX_PIN=2
    -DGPS_TX_PIN=3
    -DGPS_BAUD=9600
    -DI2C_SDA_PIN=18
    -DI2C_SCL_PIN=8
    -DBMP580_I2C_ADDR=0x47
    -DLV_CONF_INCLUDE_SIMPLE
    -DLV_COLOR_DEPTH=16
    -DLV_FONT_MONTSERRAT_20=1
    -DLV_FONT_MONTSERRAT_28=1
    -DLV_FONT_MONTSERRAT_32=1

lib_deps =
    mikalhart/TinyGPSPlus
    adafruit/Adafruit Unified Sensor @ ^1.1.14
    adafruit/Adafruit BusIO          @ ^1.16.1
    adafruit/Adafruit BMP5xx Library @ ^1.0.2
    lewisxhe/SensorLib               @ ^0.5.0
    lvgl/lvgl                        @ ^8.4.0
```

---

## Dependency Graph

```mermaid
flowchart LR
    M1["M1: GPS UART"] --> M3["M3: Display QSPI"]
    M2["M2: I2C Sensors"] --> M3
    M3 --> M4["M4: LVGL Multi-Screen Dashboard"]
    M4 --> M5["M5: Power & Storage"]
```

---

## [x] Milestone 1 — GPS ATGM336H qua UART1 ✅ HOÀN THÀNH

**Kết quả thực tế:**
- UART1 chạy ổn định trên GPIO2(RX)/GPIO3(TX) ở 9600 baud
- TinyGPSPlus parse thành công hàng chục nghìn NMEA chars (chars > 30,000, fixes = 0 do test trong nhà)
- Không crash, không lỗi checksum
- **Fix quan trọng:** Chuyển từ GPIO16/17 → GPIO2/GPIO3 để khắc phục WDT crash do LP_UART conflict

**Output thực tế:**
```
  [GPS] ATGM336H
    Vi tri     : Chua fix (dang tim ve tinh...)
    Ve tinh    : 0
    NMEA       : chars=30452  fixes=0  err=0
```

---

## [x] Milestone 2 — I2C Sensors (BMP580 + QMI8658) ✅ HOÀN THÀNH

**Kết quả thực tế:**
- I2C Bus scan phát hiện đủ 4 thiết bị: `0x38`, `0x47`, `0x6B`, `0x7E`
- BMP580 tại `0x47` (SDO=VCC): khởi tạo thành công, đọc ổn định
- QMI8658 tại `0x6B`: phát hiện, khởi tạo thành công với SensorLib
- **Fix quan trọng:** Thay `Adafruit_BMP3XX` bằng `Adafruit BMP5xx Library`

**Output thực tế:**
```
  [BMP] BMP580
    Nhiet do   : 31.49 °C
    Ap suat    : 1009.61 hPa
    Cao do Baro: 30.4 m
```

---

## [x] Milestone 3 — AMOLED Display Bring-up (QSPI SH8601) ✅ HOÀN THÀNH

**Kết quả thực tế:**
- Tích hợp Waveshare BSP: `lcd_bsp.c`, `esp_lcd_sh8601`, `lcd_config.h`
- Chỉnh RST pin = **GPIO20** (theo schematic board thực tế)
- LVGL khởi tạo thành công — màn hình sáng, hiển thị text màu xanh lá
- LVGL chạy trong FreeRTOS task riêng (`example_lvgl_port_task`)
- Expose `example_lvgl_lock` / `example_lvgl_unlock` ra `lcd_bsp.h` với `extern "C"`
- Dashboard cơ bản hiển thị: Sats, Speed, Lat/Lng, Temp/Pressure, Tilt

---

### Khe Thẻ Nhớ Micro SD (Tích hợp trên board)
Board tích hợp sẵn khe cắm thẻ nhớ TF (MicroSD) dùng chung bus SPI2 với màn hình AMOLED:

| Tín hiệu | GPIO | Ghi chú |
|---|---|---|
| SD_CS | **GPIO 15** | Chip Select riêng cho thẻ nhớ |
| SD_CLK (SCLK) | **GPIO 11** | Chung chân PCLK màn hình |
| SD_MOSI (CMD) | **GPIO 4** | Chung chân DATA0 màn hình |
| SD_MISO (DAT0) | **GPIO 5** | Chung chân DATA1 màn hình |
| Host | **SPI2_HOST** | Giao tiếp qua SPI bus chung |

---

## [x] Milestone 4 — LVGL Dashboard Multi-Screen (3 Trang) ✅ HOÀN THÀNH

**Mục tiêu:** Giao diện đa trang dùng `lv_tileview` — người dùng vuốt trái/phải để chuyển trang, vuốt lên về trang chủ (Sensors).

### Kiến trúc: `lv_tileview` 3 trang ngang
```
[ Trang 0: Navigation ] ←→ [ Trang 1: Sensors (Mặc định) ] ←→ [ Trang 2: System ]
```
- **Trang 0 (Bên trái):** Navigation (GPS Course, Heading, La bàn số).
- **Trang 1 (Ở giữa - Mặc định):** 
  - GPS Position (kinh độ/vĩ độ định dạng `N / E`).
  - Cao độ GPS: `Alt: ... m (GPS)` lấy từ vệ tinh.
  - Tốc độ (`km/h`), Số vệ tinh GPS.
  - Box Environment & IMU: Dòng 1: Nhiệt độ (`°C`), Dòng 2: Áp suất (`hPa`) & Cao độ phong vũ biểu (`Baro: ... m`), Dòng 3: Pitch & Roll (`P: ... | R: ...`).
- **Trang 2 (Bên phải):** System (GPS Stats, Uptime, Điện áp pin `V` và `%`, trạng thái thẻ nhớ SD).

### Thao tác cử chỉ (Gestures):
- Vuốt sang trái → Trang Navigation.
- Vuốt sang phải → Trang System.
- **Vuốt từ dưới lên trên (Swipe Up):** Sự kiện `LV_DIR_TOP` đưa ngay lập tức về trang trung tâm (Sensors) như phím Home điện thoại.
- Đã ẩn thanh scrollbars (không còn 3 chấm dưới đáy).

### Các khắc phục & Tối ưu quan trọng trong Milestone 4:
1. **Khắc phục xung đột I2C (driver_ng conflict):** Gỡ bỏ `SensorLib`, viết driver trực tiếp qua `Wire` cho QMI8658 với cơ chế tự động dò địa chỉ fallback `0x6A` → `0x6B`.
2. **Khắc phục hiện tượng Ghost Touch (chạm ảo tự nhảy trang):** Bổ sung kiểm tra lỗi I2C và lọc số điểm chạm hợp lệ (1-2) trong driver `FT3168.cpp`.
3. **Khắc phục lỗi `[259] ESP_ERR_INVALID_STATE`:** Bao bọc mọi thao tác I2C của task chính bằng Mutex `example_lvgl_lock(-1)` để tránh đụng độ với task đọc cảm ứng ngầm của LVGL.
4. **Tắt log hệ thống ESP32:** Cấu hình `-DCORE_DEBUG_LEVEL=0` trong `platformio.ini` để loại bỏ hoàn toàn các log cảnh báo HAL rác.
5. **Cân chỉnh tần số quét hiển thị:** Đọc cảm biến ở 5Hz, cập nhật màn hình LVGL ở 4Hz (250ms) giúp số liệu Pitch/Roll phản hồi nhạy và êm mắt.
6. **Đo dung lượng Pin Li-Po:** Đọc ADC từ GPIO0 (bộ chia áp tích hợp của board Waveshare) tính ra điện áp (V) và % dung lượng.
7. **Khắc phục vệt trắng bo tròn (White Pill Box) dưới đáy màn hình khi boot:**
   - **Nguyên nhân:** Thanh cuộn ngang (`LV_PART_SCROLLBAR`) của `lv_tileview` do `LV_THEME_DEFAULT_DARK 0` (mặc định là Light Mode có màu sáng). Với 3 tile, thanh cuộn có chiều rộng bằng 1/3 màn hình (~93px) và nằm chính giữa đáy.
   - **Xử lý:** 
     + Chuyển `#define LV_THEME_DEFAULT_DARK 1` trong `include/lv_conf.h` (chế độ Dark theme tối ưu AMOLED).
     + Tắt triệt để cờ cuộn & scrollbar trên `scr`, `s_tileview` và cả 3 tile (`LV_SCROLLBAR_MODE_OFF`).
     + Đặt độ mờ của `LV_PART_SCROLLBAR` thành trong suốt hoàn toàn (`LV_OPA_TRANSP`).
     + Khóa Mutex `example_lvgl_lock(-1)` khi chạy `sd_logger_init()` tránh xung đột bus `SPI2_HOST` lúc boot.

---

## [x] Milestone 5 — Power & Data Logging (Đã hoàn thiện & Tối ưu hóa sâu)

### 1. Quản lý Nguồn & Lưu trữ MicroSD
- [x] Đọc ADC pin Li-Po (GPIO0), hiển thị điện áp (V) và % trên màn hình System & Header.
- [x] Khởi tạo MicroSD an toàn qua `esp_vfs_fat_sdspi_mount` (SPI2_HOST, CS=GPIO15, Bus chia sẻ với AMOLED).
- [x] Nút bấm Start/Stop Logging trên giao diện System kèm hiển thị thống kê điểm ghi và dung lượng RAM buffer.

### 2. Tối ưu hóa Module Ghi Log Compact CSV Siêu Nhẹ (Tonight - 2026-10-05)
- [x] **Cấu trúc dữ liệu & Đặt tên file:**
  - Tự động tạo file theo định dạng `/sdcard/logs/YYYYMMDD_HHMMSS.csv` (fallback: `/sdcard/logs/track_001.csv` khi chưa có GPS).
  - Tiêu đề ngắn gọn: `ts,lat,lon,alt\n`.
  - Bản ghi siêu rút gọn (~32–35 bytes/dòng): dùng Unix Epoch Timestamp UTC (`ts`), tọa độ 6 số thập phân, cao độ hiệu chuẩn (`alt`), loại bỏ trường `speed` (ứng dụng như Strava sẽ tự nội suy vận tốc).
  - Tiết kiệm hơn **80–85% dung lượng** so với định dạng GPX/XML ban đầu.
- [x] **Chiến lược SPI Flush & Giảm hao mòn bộ nhớ Flash:**
  - Bộ đệm tĩnh RAM 1024 bytes gom ~30 điểm Smart Logging trước khi ghi khối.
  - Chỉ kích hoạt ghi khối (`fwrite`) và chốt sector (`fflush` + `fsync`) khi bộ đệm tích lũy $\ge 512$ bytes (chuẩn 1 sector FATFS) hoặc sau 60 giây chờ.
  - Khi bấm STOP: Flush toàn bộ byte còn lại trong RAM buffer và đóng file an toàn (`fsync` + `fclose`).

### 3. Khắc phục các Lỗi Phát sinh (Tonight - 2026-10-05)
- [x] **Sửa lỗi cú pháp XML GPX (`</trkpt À`):**
  - Phát hiện nguyên nhân: `line_buf[128]` bị hụt kích thước khiến `memcpy` đọc tràn byte rác trên stack (`0xC0` $\rightarrow$ `À`). Đã nâng lên 256 bytes và kiểm soát chặt chẽ giá trị trả về của `snprintf`.
  - Bổ sung lệnh `fsync(fileno())` ép dữ liệu ghi thực sự xuống sector vật lý thay vì chỉ nằm ở tầng đệm FATFS.
- [x] **Sửa lỗi ngắt log đột ngột khi cắm nguồn USB:**
  - Phát hiện nguyên nhân: Khi chưa cắm pin Li-Po mà cấp nguồn qua cáp USB, chân ADC GPIO0 bị thả nổi đo được ~3.19V $\rightarrow$ cơ chế tự ngắt khi pin < 3.45V hiểu nhầm là pin cạn nên tự ngắt ngay khi vừa bấm Start.
  - Khắc phục: Gỡ bỏ lệnh ngắt bắt buộc, chuyển thành cảnh báo Serial định kỳ, cho phép hoạt động hoàn hảo cả khi dùng pin, cắm sạc dự phòng hoặc cắm USB máy tính.
- [x] **Sửa lỗi Baro kẹt ở `-36m` & Tối ưu thuật toán Auto-Calibration (Garmin-Style):**
  - Bản chất: BMP580 đo chênh lệch áp suất cực nhạy (từng bước chân), GPS chỉ đóng vai trò "mỏ neo" để gán mốc mực nước biển $P_0$.
  - Khắc phục: Bản cập nhật trước đòi hỏi điều kiện quá ngặt nghèo ($\ge 7$ vệ tinh và $\text{HDOP} \le 1.3$ liên tục 30s không được sụt 1s nào) khiến hệ thống không bao giờ chốt được mốc $P_0$, dẫn tới BMP580 bị kẹt ở mốc chuẩn quốc tế 1013.25 hPa (tính ra $-36\text{m}$).
  - Cải tiến: Nới lỏng điều kiện thực tế ($\ge 5$ vệ tinh, $\text{HDOP} \le 2.5$), tích lũy 8 mẫu hợp lệ (không reset trắng bộ đệm nếu có 1 giây nhiễu), bổ sung hiển thị trực quan HDOP và trạng thái `(DA CALIBRATE)` trên Serial.

### 4. Công cụ Chuyển đổi Tự động (`tools/csv_to_gpx.py`)
- [x] Viết script Python độc lập (chỉ dùng thư viện chuẩn Python 3, không cần cài thư viện ngoài).
- [x] Hỗ trợ kéo-thả file hoặc dòng lệnh CLI, tự động chuyển đổi Unix Epoch Timestamp thành chuẩn ISO 8601 UTC.
- [x] Xuất file GPX 1.1 chuẩn schema quốc tế, tương thích 100% với Strava, Garmin Connect, Google Earth, GPX Studio,...
- [x] Đã xử lý sẵn tương thích lỗi bảng mã console Windows (CP1252 `UnicodeEncodeError`).

---

## [ ] Milestone 6 — Navigation với Cửa sổ trượt (Sliding Window GPX) & Breadcrumb Map
- [ ] **Định dạng dữ liệu tuyến đường (Route Binary):**
  - Chuyển đổi file GPX thành dạng bản ghi nhị phân cố định (Fixed-size Record: 8 bytes/điểm - float lat, float lon).
- [ ] **Thuật toán Cửa sổ trượt (Sliding Window):**
  - Cấp phát cố định mảng đệm 100 điểm (~800 bytes RAM) cho các điểm lân cận vị trí hiện tại.
  - Sử dụng `fseek()` đọc nạp cuộn tuần tự từ thẻ nhớ khi người dùng di chuyển, không nạp toàn bộ file vào SRAM.
- [ ] **Giao diện Breadcrumb Navigation (AMOLED Dark Mode):**
  - Tọa độ người dùng làm tâm `(140, 120)` với biểu tượng mũi tên định hướng (kết hợp Gyro + GPS Course).
  - Vẽ vệt lộ trình dự kiến (màu Neon Cyan) và vết đường đã đi (màu xám nhạt).
  - Tính toán và cảnh báo lệch lộ trình (Cross-Track Error) cùng cự ly đến waypoint kế tiếp.
  - Hỗ trợ thao tác Zoom thang đo ($100\text{m} \leftrightarrow 500\text{m} \leftrightarrow 2\text{km}$).

---

## [ ] Milestone 7 — Web App Kết nối & Truyền dữ liệu (BLE / Wi-Fi)
- [ ] **Web BLE Dashboard (PWA):**
  - Kết nối qua Bluetooth Web API từ trình duyệt điện thoại để xem dữ liệu thời gian thực.
  - Tải file nhị phân lộ trình từ điện thoại bắn sang thẻ nhớ ESP32-C6.
- [ ] **Wi-Fi Hotspot Mode (Trích xuất dữ liệu nhanh):**
  - Kích hoạt Web Server tạm thời để tải toàn bộ file CSV từ thẻ MicroSD về điện thoại.

---

## Ý Tưởng Phát Triển Tương Lai

| Tính năng | Mô tả |
|---|---|
| 🗺️ Bản đồ offline | Đọc tiles OpenStreetMap từ Micro SD |
| 📍 Waypoints | Lưu và điều hướng đến điểm đã đánh dấu |
| 📊 Track recording | Ghi route GPS, export file GPX/CSV ra SD |
| 📶 BLE streaming | Stream dữ liệu GPS đến điện thoại |
| 🌐 OTA update | Cập nhật firmware qua WiFi ngoài trời |

## [x] Updates (2026-09-30)
- **SD Card & SPI Fix**: Restored ~SDMMC_HOST_FLAG_DEINIT_ARG flag to prevent SPI de-initialization when mounting SD card, fixing I2C lockups and mount failures.
- **Smart GPX Logging**: Implemented 3-state GPS sampling logic (10s standby, 3s steady, 1s maneuver) in sd_logger.cpp.
- **Power Management (Light Sleep)**: Switched from DFS CPU scaling to esp_light_sleep_start(), preventing UART baudrate corruption. Screen successfully auto-timeouts after 20s.
- **UI Freeze Fixes**: Resolved USB CDC blocking by setting Serial.setTxTimeoutMs(0). Resolved LVGL threading Guru Meditation Error by wrapping all cross-task LVGL calls (lv_disp_trig_activity, lv_disp_get_inactive_time) in example_lvgl_lock(-1).
- **BOOT Button Status**: Edge detection logic and pinMode restoring implemented, but the physical button read is still failing to trigger the manual wake/sleep. Needs investigation in the next session.
