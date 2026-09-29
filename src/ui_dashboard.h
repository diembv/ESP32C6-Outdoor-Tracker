/**
 * @file ui_dashboard.h
 * @brief LVGL 3-page Dashboard — ESP32-C6 Outdoor Tracker (Milestone 4)
 *
 * Board  : Waveshare ESP32-C6-Touch-AMOLED-1.64
 * Screen : SH8601 AMOLED, 280 × 456 px, QSPI
 * MCU    : ESP32-C6, 512 KB SRAM, no PSRAM
 *
 * TileView layout (swipe ngang — FT3168 touch):
 *   Tile 0 — Outdoor Sensors : GPS Lat/Lon/Speed/Sats/Alt, BMP580 Temp/Pressure
 *   Tile 1 — Navigation      : GPS Course heading + IMU Pitch/Roll
 *   Tile 2 — System          : Uptime, GPS stats, Battery placeholder, SD placeholder
 *
 * Quy tắc quan trọng (tuân theo PLAN.md):
 *   - Mọi lv_* call PHẢI bọc trong example_lvgl_lock / example_lvgl_unlock
 *   - KHÔNG dùng lv_label_set_text_fmt với %f — dùng snprintf + lv_label_set_text
 *   - Font dùng: Montserrat 12, 14, 20, 28, 32 (đã bật trong lv_conf.h)
 *   - Nền #000000 tối ưu AMOLED (pixel đen = pixel tắt)
 */

#pragma once
#include <lvgl.h>
#include <stdint.h>
#include <stdbool.h>

// ── Screen resolution ─────────────────────────────────────────────────────────
#define SCREEN_W   280
#define SCREEN_H   456

// ── AMOLED Color palette ──────────────────────────────────────────────────────
#define CLR_BG        lv_color_hex(0x000000)   // Nền đen tuyệt đối (AMOLED)
#define CLR_SURFACE   lv_color_hex(0x0D1117)   // Card surface
#define CLR_BORDER    lv_color_hex(0x21262D)   // Card border
#define CLR_ACCENT    lv_color_hex(0x00FFFF)   // Cyan — header, accent (từ build hiện tại)
#define CLR_GREEN     lv_color_hex(0x00FF87)   // GPS fix OK, battery OK
#define CLR_YELLOW    lv_color_hex(0xFFFF00)   // Coords (từ build hiện tại)
#define CLR_ORANGE    lv_color_hex(0xFF8800)   // Course/compass (từ build hiện tại)
#define CLR_WHITE     lv_color_hex(0xFFFFFF)   // Speed (từ build hiện tại)
#define CLR_GRAY      lv_color_hex(0xAAAAAA)   // IMU tilt (từ build hiện tại)
#define CLR_RED       lv_color_hex(0xFF3B30)   // No fix, error
#define CLR_DIMTEXT   lv_color_hex(0x6B7280)   // Dim label keys
#define CLR_ENVGREEN  lv_color_hex(0x00FF00)   // Env data (từ build hiện tại)

// ── Sensor data structs ───────────────────────────────────────────────────────

struct GpsData {
    double   latitude;      // độ thập phân (N+, S-)
    double   longitude;     // độ thập phân (E+, W-)
    float    speed_kmh;     // tốc độ km/h
    float    altitude_m;    // cao độ GPS (m)
    float    course_deg;    // hướng di chuyển 0–359.9°
    uint32_t satellites;    // số vệ tinh lock
    bool     fix_valid;     // true = GPS fix hợp lệ
    
    // GPS Time & Date
    bool     time_valid;
    bool     date_valid;
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;

    uint32_t chars_proc;    // NMEA chars processed
    uint32_t fixes;         // sentences with fix
    uint32_t checksum_err;  // failed checksum
};

struct BaroData {
    float pressure_hpa;     // áp suất hPa (BMP580.pressure trả về thẳng hPa)
    float temperature_c;    // nhiệt độ °C
    float altitude_m;       // cao độ baro (m) — tính từ áp suất
    bool  valid;            // sensor đọc được
};

struct ImuData {
    float pitch_deg;        // Pitch (°) — từ accelerometer
    float roll_deg;         // Roll  (°) — từ accelerometer
    // Note: QMI8658 không có magnetometer → không có Yaw thực
    bool  valid;
};

struct SysData {
    uint32_t uptime_s;      // uptime (giây)
    float    battery_v;     // điện áp pin (V) — placeholder
    uint8_t  battery_pct;   // % pin — placeholder
    bool     sd_present;    // SD có mặt
    bool     sd_ok;         // SD OK
    uint32_t sd_free_mb;    // SD free MB
    bool     is_logging;    // Trạng thái đang ghi log
};

struct SensorSnapshot {
    GpsData  gps;
    BaroData baro;
    ImuData  imu;
    SysData  sys;
};

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * @brief Khởi tạo Dashboard UI (gọi bên trong example_lvgl_lock)
 * @note  Phải gọi TRONG vùng example_lvgl_lock(-1) / example_lvgl_unlock()
 */
void ui_dashboard_init(void);

/**
 * @brief Cập nhật toàn bộ UI với dữ liệu mới (gọi bên trong example_lvgl_lock)
 * @param snap Pointer đến SensorSnapshot mới nhất
 * @note  Phải gọi TRONG vùng example_lvgl_lock(-1) / example_lvgl_unlock()
 */
void ui_dashboard_update(const SensorSnapshot *snap);
