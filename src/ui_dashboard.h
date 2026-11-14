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
#include "app_types.h"
#include "boards/board_select.h"

// ── Screen resolution ─────────────────────────────────────────────────────────
#define SCREEN_W   BOARD_DISP_WIDTH
#define SCREEN_H   BOARD_DISP_HEIGHT

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

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * @brief Khởi tạo Dashboard UI (gọi bên trong example_lvgl_lock)
 * @note  Phải gọi TRONG vùng example_lvgl_lock(-1) / example_lvgl_unlock()
 */
void ui_dashboard_init(void);

/**
 * @brief Yêu cầu hiệu chuẩn lại cao độ khí áp kế bằng cao độ GPS hiện tại
 */
void baro_request_recalibration(void);

/**
 * @brief Cập nhật toàn bộ UI với dữ liệu mới (gọi bên trong example_lvgl_lock)
 * @param snap Pointer đến SensorSnapshot mới nhất
 * @note  Phải gọi TRONG vùng example_lvgl_lock(-1) / example_lvgl_unlock()
 */
void ui_dashboard_update(const SensorSnapshot *snap);
