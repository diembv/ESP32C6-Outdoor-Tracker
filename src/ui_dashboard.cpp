/**
 * @file ui_dashboard.cpp
 * @brief LVGL 3-page Dashboard — Milestone 4 Implementation
 */

#include "ui_dashboard.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// ═══════════════════════════════════════════════════════════════════════════════
//  LVGL Object Handles
// ═══════════════════════════════════════════════════════════════════════════════

static lv_obj_t *s_tileview = nullptr;
static lv_obj_t *s_tile[3]  = {nullptr, nullptr, nullptr};

// ── Tile 1: Outdoor Sensors (Mặc định - Chính giữa) ───────────────────────────
static lv_obj_t *t1_lbl_header;    
static lv_obj_t *t1_lbl_speed;     
static lv_obj_t *t1_lbl_coords;    
static lv_obj_t *t1_lbl_env;       
static lv_obj_t *t1_dot_fix;       

// ── Tile 2: Navigation (Bên phải) ────────────────────────────────────────────
static lv_obj_t *t2_lbl_direction; 
static lv_obj_t *t2_lbl_note;     

// ── Tile 0: System (Bên trái) ────────────────────────────────────────────────
static lv_obj_t *t0_lbl_gps_stats; 
static lv_obj_t *t0_lbl_uptime;    
static lv_obj_t *t0_lbl_battery;   
static lv_obj_t *t0_lbl_sd;        
static lv_obj_t *t0_dot_gps;       
static lv_obj_t *t0_lbl_datetime;  

static lv_obj_t *t0_btn_log;
static lv_obj_t *t0_lbl_btn_log;
static lv_obj_t *t0_dd_mode;
static lv_obj_t *t0_btn_calib;

// ═══════════════════════════════════════════════════════════════════════════════
//  Helpers
// ═══════════════════════════════════════════════════════════════════════════════
static lv_obj_t *make_hline(lv_obj_t *parent, lv_coord_t y)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, SCREEN_W - 16, 1);
    lv_obj_set_pos(line, 8, y);
    lv_obj_set_style_bg_color(line, CLR_BORDER, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    return line;
}

static lv_obj_t *make_key_label(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(lbl, CLR_DIMTEXT, 0);
    lv_obj_set_pos(lbl, x, y);
    return lbl;
}

static lv_obj_t *make_val_label(lv_obj_t *parent, const char *init_text, lv_color_t color, const lv_font_t *font, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, init_text);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    lv_obj_set_pos(lbl, x, y);
    return lbl;
}

static lv_obj_t *make_dot(lv_obj_t *parent, lv_color_t color, lv_coord_t x, lv_coord_t y, lv_coord_t size = 10)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, size, size);
    lv_obj_set_pos(dot, x, y);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    return dot;
}

static void no_scrollbar_draw_cb(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc && (dsc->type == LV_OBJ_DRAW_PART_SCROLLBAR || dsc->part == LV_PART_SCROLLBAR)) {
        if (dsc->rect_dsc) {
            dsc->rect_dsc->bg_opa = LV_OPA_TRANSP;
            dsc->rect_dsc->border_opa = LV_OPA_TRANSP;
            dsc->rect_dsc->shadow_opa = LV_OPA_TRANSP;
            dsc->rect_dsc->outline_opa = LV_OPA_TRANSP;
        }
        if (dsc->draw_area) {
            dsc->draw_area->x2 = dsc->draw_area->x1 - 1;
            dsc->draw_area->y2 = dsc->draw_area->y1 - 1;
        }
    }
}

static void strip_scrollbar(lv_obj_t *obj)
{
    if (!obj) return;
    lv_obj_remove_style(obj, NULL, (lv_style_selector_t)LV_PART_SCROLLBAR | (lv_style_selector_t)LV_STATE_ANY);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_width(obj, 0, (lv_style_selector_t)LV_PART_SCROLLBAR | (lv_style_selector_t)LV_STATE_ANY);
    lv_obj_set_style_pad_all(obj, 0, (lv_style_selector_t)LV_PART_SCROLLBAR | (lv_style_selector_t)LV_STATE_ANY);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, (lv_style_selector_t)LV_PART_SCROLLBAR | (lv_style_selector_t)LV_STATE_ANY);
    lv_obj_set_style_border_opa(obj, LV_OPA_TRANSP, (lv_style_selector_t)LV_PART_SCROLLBAR | (lv_style_selector_t)LV_STATE_ANY);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_TRANSP, (lv_style_selector_t)LV_PART_SCROLLBAR | (lv_style_selector_t)LV_STATE_ANY);
    lv_obj_add_event_cb(obj, no_scrollbar_draw_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
}

static void style_tile(lv_obj_t *tile)
{
    lv_obj_set_style_bg_color(tile, CLR_BG, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    strip_scrollbar(tile);
}

// Swipe gesture callback removed: s_tileview handles native horizontal swipes smoothly without interference.

// ═══════════════════════════════════════════════════════════════════════════════
//  Tile 0: Navigation (Bên trái)
// ═══════════════════════════════════════════════════════════════════════════════
static void build_tile0_navigation(lv_obj_t *tile)
{
    style_tile(tile);

    lv_obj_t *hdr = make_val_label(tile, "NAVIGATION", CLR_ACCENT, &lv_font_montserrat_20, 8, 12);
    (void)hdr;
    make_hline(tile, 46);

    make_key_label(tile, "Course / Heading", SCREEN_W/2 - 60, 60);

    t2_lbl_direction = lv_label_create(tile);
    lv_label_set_text(t2_lbl_direction, "N (000 deg)");
    lv_obj_set_style_text_font(t2_lbl_direction, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(t2_lbl_direction, CLR_ORANGE, 0);
    lv_obj_set_style_text_align(t2_lbl_direction, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(t2_lbl_direction, SCREEN_W);
    lv_obj_set_pos(t2_lbl_direction, 0, 80);

    make_hline(tile, 130);

    t2_lbl_note = lv_label_create(tile);
    lv_label_set_text(t2_lbl_note, "Course over ground (v > 0.5 km/h)");
    lv_obj_set_style_text_font(t2_lbl_note, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(t2_lbl_note, CLR_DIMTEXT, 0);
    lv_obj_set_pos(t2_lbl_note, 8, 140);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Tile 1: Sensors (Mặc định - Ở giữa)
// ═══════════════════════════════════════════════════════════════════════════════
static void build_tile1_sensors(lv_obj_t *tile)
{
    style_tile(tile);

    t1_dot_fix = make_dot(tile, CLR_RED, 8, 12);
    t1_lbl_header = make_val_label(tile, "Sats: 0 | Bat: 100%", CLR_ACCENT, &lv_font_montserrat_20, 26, 6);
    make_hline(tile, 42);

    make_key_label(tile, "Speed", SCREEN_W/2 - 18, 50);
    t1_lbl_speed = lv_label_create(tile);
    lv_label_set_text(t1_lbl_speed, "0.0 km/h");
    lv_obj_set_style_text_font(t1_lbl_speed, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(t1_lbl_speed, CLR_WHITE, 0);
    lv_obj_set_style_text_align(t1_lbl_speed, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(t1_lbl_speed, SCREEN_W);
    lv_obj_set_pos(t1_lbl_speed, 0, 68);

    make_hline(tile, 112);

    make_key_label(tile, "GPS Position & Alt", 8, 120);
    t1_lbl_coords = lv_label_create(tile);
    lv_label_set_text(t1_lbl_coords, "--.------ N\n--.------ E\nAlt: -- m (GPS)");
    lv_obj_set_style_text_font(t1_lbl_coords, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(t1_lbl_coords, CLR_YELLOW, 0);
    lv_obj_set_pos(t1_lbl_coords, 8, 140);

    make_hline(tile, 225);

    make_key_label(tile, "Environment & IMU", 8, 235);
    t1_lbl_env = lv_label_create(tile);
    lv_label_set_text(t1_lbl_env, "-- C\n-- hPa  |  Baro: -- m\nP: --  |  R: --");
    lv_obj_set_style_text_font(t1_lbl_env, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(t1_lbl_env, CLR_ENVGREEN, 0);
    lv_obj_set_pos(t1_lbl_env, 8, 255);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Tile 2: System (Bên phải)
// ═══════════════════════════════════════════════════════════════════════════════

extern SensorSnapshot g_snap;
#include "sd_logger.h"

static uint32_t logStartTime = 0;

static void mbox_close_cb(lv_event_t * e) {
    lv_msgbox_close(lv_event_get_current_target(e));
}

static void dd_mode_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_VALUE_CHANGED) {
        lv_obj_t * dropdown = lv_event_get_target(e);
        uint16_t selected = lv_dropdown_get_selected(dropdown);
        ActivityMode mode = (selected == 1) ? ACTIVITY_MODE_BIKE : ACTIVITY_MODE_HIKE;
        g_snap.sys.activity_mode = mode;
        sd_logger_set_activity_mode(mode);
        Serial.printf("[UI] Che do the thao doi sang: %s\n", (mode == ACTIVITY_MODE_BIKE) ? "Cycling" : "Hiking");
    }
}

static void btn_calib_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        if (!g_snap.gps.fix_valid || !g_snap.gps.altitude_valid || g_snap.gps.satellites < 5) {
            static const char * btns[] = {"OK", ""};
            lv_obj_t * mbox = lv_msgbox_create(NULL, "CALIB ALTITUDE", "GPS fix not ready.\nNeed 3D fix (>= 5 sats)\nto calibrate altitude.", btns, false);
            lv_obj_add_event_cb(mbox, mbox_close_cb, LV_EVENT_VALUE_CHANGED, NULL);
            lv_obj_center(mbox);
        } else {
            baro_request_recalibration();
            static const char * btns[] = {"OK", ""};
            char msg[128];
            snprintf(msg, sizeof(msg), "Calibrated to GPS Alt: %.1fm\nSea-level P0 updated.", g_snap.gps.altitude_m);
            lv_obj_t * mbox = lv_msgbox_create(NULL, "CALIB SUCCESS", msg, btns, false);
            lv_obj_add_event_cb(mbox, mbox_close_cb, LV_EVENT_VALUE_CHANGED, NULL);
            lv_obj_center(mbox);
        }
    }
}

static void btn_log_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        if (!g_snap.sys.sd_present || !g_snap.sys.sd_ok) {
            static const char * btns[] = {"OK", ""};
            lv_obj_t * mbox = lv_msgbox_create(NULL, "SD CARD ERROR", "No MicroSD card inserted\nor card not mounted.", btns, false);
            lv_obj_add_event_cb(mbox, mbox_close_cb, LV_EVENT_VALUE_CHANGED, NULL);
            lv_obj_center(mbox);
            return;
        }

        if (g_snap.sys.is_logging_active) {
            sd_logger_toggle(&g_snap);
            
            float dist_m = 0;
            uint32_t dur_s = 0;
            sd_logger_get_summary(&dist_m, &dur_s);
            
            uint32_t m = dur_s / 60;
            uint32_t s = dur_s % 60;
            float dist_km = dist_m / 1000.0f;
            float avg_speed = dur_s > 0 ? (dist_km / (dur_s / 3600.0f)) : 0;
            
            static const char * btns[] = {"OK", ""};
            char summary_text[200];
            snprintf(summary_text, sizeof(summary_text), 
                     "Session saved to MicroSD.\n\n"
                     "Mode: %s\n"
                     "Elapsed Time: %02lu:%02lu\n"
                     "Distance: %.2f km\n"
                     "Avg Speed: %.1f km/h", 
                     (g_snap.sys.activity_mode == ACTIVITY_MODE_BIKE) ? "Cycling" : "Hiking",
                     (unsigned long)m, (unsigned long)s, dist_km, avg_speed);
                     
            lv_obj_t * mbox = lv_msgbox_create(NULL, "ACTIVITY SUMMARY", summary_text, btns, false);
            lv_obj_add_event_cb(mbox, mbox_close_cb, LV_EVENT_VALUE_CHANGED, NULL);
            lv_obj_center(mbox);
        } else {
            logStartTime = millis();
            sd_logger_toggle(&g_snap);
            
            if (!g_snap.gps.fix_valid) {
                static const char * btns[] = {"OK", ""};
                lv_obj_t * mbox = lv_msgbox_create(NULL, "WAITING FOR GPS", "Log armed in standby.\nSystem will auto-record\nonce GPS fix is acquired.", btns, false);
                lv_obj_add_event_cb(mbox, mbox_close_cb, LV_EVENT_VALUE_CHANGED, NULL);
                lv_obj_center(mbox);
            }
        }
    }
}

static void build_tile2_system(lv_obj_t *tile)
{
    style_tile(tile);

    make_val_label(tile, "SYSTEM", CLR_ACCENT, &lv_font_montserrat_20, 8, 12);
    make_hline(tile, 46);

    t0_dot_gps = make_dot(tile, CLR_RED, 8, 62);
    make_key_label(tile, "GPS Status", 26, 56);
    t0_lbl_gps_stats = lv_label_create(tile);
    lv_label_set_text(t0_lbl_gps_stats, "chars=0  fixes=0  err=0");
    lv_obj_set_style_text_font(t0_lbl_gps_stats, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(t0_lbl_gps_stats, CLR_WHITE, 0);
    lv_obj_set_pos(t0_lbl_gps_stats, 8, 78);

    make_hline(tile, 105);

    make_key_label(tile, "Date / Time", 8, 110);
    t0_lbl_datetime = lv_label_create(tile);
    lv_label_set_text(t0_lbl_datetime, "--/--/---- --:--:--");
    lv_obj_set_style_text_font(t0_lbl_datetime, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(t0_lbl_datetime, CLR_WHITE, 0);
    lv_obj_set_pos(t0_lbl_datetime, 8, 128);

    make_hline(tile, 155);

    make_key_label(tile, "Uptime", 8, 160);
    t0_lbl_uptime = lv_label_create(tile);
    lv_label_set_text(t0_lbl_uptime, "00:00:00");
    lv_obj_set_style_text_font(t0_lbl_uptime, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(t0_lbl_uptime, CLR_WHITE, 0);
    lv_obj_set_pos(t0_lbl_uptime, 8, 178);

    make_hline(tile, 205);

    make_key_label(tile, "Battery", 8, 210);
    t0_lbl_battery = lv_label_create(tile);
    lv_label_set_text(t0_lbl_battery, "-- V | -- %");
    lv_obj_set_style_text_font(t0_lbl_battery, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(t0_lbl_battery, CLR_YELLOW, 0);
    lv_obj_set_pos(t0_lbl_battery, 8, 228);

    make_hline(tile, 255);

    make_key_label(tile, "SD Card", 8, 260);
    t0_lbl_sd = lv_label_create(tile);
    lv_label_set_text(t0_lbl_sd, "No card (*placeholder)");
    lv_obj_set_style_text_font(t0_lbl_sd, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(t0_lbl_sd, CLR_ACCENT, 0);
    lv_obj_set_pos(t0_lbl_sd, 8, 278);

    // ── Nút Hiệu chuẩn Cao độ thủ công (GPS Calib Alt) ──
    t0_btn_calib = lv_btn_create(tile);
    lv_obj_set_size(t0_btn_calib, 256, 52);
    lv_obj_align(t0_btn_calib, LV_ALIGN_BOTTOM_MID, 0, -72);
    lv_obj_clear_flag(t0_btn_calib, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_bg_color(t0_btn_calib, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_bg_color(t0_btn_calib, lv_color_hex(0x374151), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(t0_btn_calib, CLR_ACCENT, 0);
    lv_obj_set_style_border_width(t0_btn_calib, 1, 0);
    lv_obj_set_style_radius(t0_btn_calib, 8, 0);
    lv_obj_add_event_cb(t0_btn_calib, btn_calib_event_cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl_calib = lv_label_create(t0_btn_calib);
    lv_label_set_text(lbl_calib, LV_SYMBOL_REFRESH " CALIB ALT (GPS)");
    lv_obj_set_style_text_font(lbl_calib, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(lbl_calib, CLR_ACCENT, 0);
    lv_obj_clear_flag(lbl_calib, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(lbl_calib);

    // ── Hàng điều khiển đáy: [Dropdown Mode] + [Nút Log] (Chiều cao 52px bằng nhau) ──
    // 1. Ô Dropdown chọn chế độ Hiking / Cycling (bên trái - bung danh sách LÊN TRÊN)
    t0_dd_mode = lv_dropdown_create(tile);
    lv_dropdown_set_options(t0_dd_mode, "Hiking\nCycling");
    lv_dropdown_set_symbol(t0_dd_mode, NULL); // BỎ HOÀN TOÀN MŨI TÊN CHỈ XUỐNG
    lv_dropdown_set_dir(t0_dd_mode, LV_DIR_TOP); // LUÔN BUNG LÊN TRÊN
    lv_obj_set_size(t0_dd_mode, 96, 52);
    lv_obj_align(t0_dd_mode, LV_ALIGN_BOTTOM_LEFT, 12, -12);
    lv_obj_clear_flag(t0_dd_mode, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_ext_click_area(t0_dd_mode, 10);

    // Style nút chính: Căn giữa 100%, font montserrat_14 nhỏ gọn, thanh thoát
    lv_obj_set_style_bg_color(t0_dd_mode, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_color(t0_dd_mode, CLR_WHITE, 0);
    lv_obj_set_style_text_font(t0_dd_mode, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(t0_dd_mode, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_border_color(t0_dd_mode, CLR_BORDER, 0);
    lv_obj_set_style_border_width(t0_dd_mode, 1, 0);
    lv_obj_set_style_radius(t0_dd_mode, 8, 0);

    // Đệm chuẩn xác để text Montserrat 14 (cao ~14px) nằm chính giữa tâm ô cao 52px: (52 - 14) / 2 = 19px
    lv_obj_set_style_pad_top(t0_dd_mode, 18, 0);
    lv_obj_set_style_pad_bottom(t0_dd_mode, 18, 0);
    lv_obj_set_style_pad_left(t0_dd_mode, 0, 0);
    lv_obj_set_style_pad_right(t0_dd_mode, 0, 0);

    lv_dropdown_set_selected(t0_dd_mode, (g_snap.sys.activity_mode == ACTIVITY_MODE_BIKE) ? 1 : 0);
    lv_obj_add_event_cb(t0_dd_mode, dd_mode_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Style cho danh sách sổ ra (Popup list mở lên trên)
    lv_obj_t * list = lv_dropdown_get_list(t0_dd_mode);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_text_color(list, CLR_WHITE, 0);
    lv_obj_set_style_border_color(list, CLR_ACCENT, 0);
    lv_obj_set_style_border_width(list, 2, 0);
    lv_obj_set_style_radius(list, 8, 0);
    lv_obj_set_style_text_font(list, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(list, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_all(list, 8, 0);


    // 2. Nút Bật/Tắt Ghi Log (bên phải - chiều cao 52px)
    t0_btn_log = lv_btn_create(tile);
    lv_obj_set_size(t0_btn_log, 150, 52);
    lv_obj_align(t0_btn_log, LV_ALIGN_BOTTOM_RIGHT, -12, -12);
    lv_obj_clear_flag(t0_btn_log, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_ext_click_area(t0_btn_log, 10);
    lv_obj_set_style_bg_color(t0_btn_log, lv_color_hex(0x2A3B4C), 0);
    lv_obj_set_style_bg_color(t0_btn_log, lv_color_hex(0x007ACC), LV_STATE_PRESSED);
    lv_obj_set_style_radius(t0_btn_log, 8, 0);
    lv_obj_add_event_cb(t0_btn_log, btn_log_event_cb, LV_EVENT_CLICKED, NULL);

    t0_lbl_btn_log = lv_label_create(t0_btn_log);
    lv_label_set_text(t0_lbl_btn_log, "START LOG");
    lv_obj_set_style_text_font(t0_lbl_btn_log, &lv_font_montserrat_14, 0);
    lv_obj_clear_flag(t0_lbl_btn_log, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(t0_lbl_btn_log);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  GPS Course → compass direction string
// ═══════════════════════════════════════════════════════════════════════════════
static const char *course_to_dir(float course)
{
    if (course < 22.5f  || course >= 337.5f) return "N";
    if (course < 67.5f)  return "NE";
    if (course < 112.5f) return "E";
    if (course < 157.5f) return "SE";
    if (course < 202.5f) return "S";
    if (course < 247.5f) return "SW";
    if (course < 292.5f) return "W";
    return "NW";
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Public API
// ═══════════════════════════════════════════════════════════════════════════════

void ui_dashboard_init(void)
{
    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, CLR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    strip_scrollbar(scr);


    s_tileview = lv_tileview_create(scr);
    lv_obj_set_size(s_tileview, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(s_tileview, 0, 0);
    lv_obj_set_style_bg_color(s_tileview, CLR_BG, 0);
    lv_obj_set_style_bg_opa(s_tileview, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(s_tileview, 0, 0);
    strip_scrollbar(s_tileview);

    // ── 3 Tiles ngang: Navigation (0) - Sensors (1) - System (2)
    s_tile[0] = lv_tileview_add_tile(s_tileview, 0, 0, LV_DIR_RIGHT);
    s_tile[1] = lv_tileview_add_tile(s_tileview, 1, 0, LV_DIR_HOR);
    s_tile[2] = lv_tileview_add_tile(s_tileview, 2, 0, LV_DIR_LEFT);

    build_tile0_navigation(s_tile[0]);
    build_tile1_sensors(s_tile[1]);
    build_tile2_system(s_tile[2]);

    for (int i = 0; i < 3; i++) {
        if (s_tile[i]) {
            strip_scrollbar(s_tile[i]);
        }
    }

    // Đặt mặc định mở trang ở giữa (Sensors - Tile 1)
    lv_obj_set_tile(s_tileview, s_tile[1], LV_ANIM_OFF);
    strip_scrollbar(s_tileview);
}

void ui_dashboard_update(const SensorSnapshot *snap)
{
    if (!snap) return;
    char buf[128];

    lv_obj_t *act_tile = s_tileview ? lv_tileview_get_tile_act(s_tileview) : NULL;

    // ═══ Tile 1: Sensors (Trang chính ở giữa) ═════════════════════════════════
    if (!act_tile || act_tile == s_tile[1]) {
        lv_obj_set_style_bg_color(t1_dot_fix, snap->gps.fix_valid ? CLR_GREEN : CLR_RED, 0);

        char timeStr[16] = "--:-- --";
        if (snap->gps.time_valid) {
            uint8_t local_h = (snap->gps.hour + 7) % 24;
            uint8_t h12 = local_h % 12;
            if (h12 == 0) h12 = 12;
            const char *ampm = (local_h >= 12) ? "PM" : "AM";
            snprintf(timeStr, sizeof(timeStr), "%02d:%02d %s", h12, snap->gps.minute, ampm);
        }
        snprintf(buf, sizeof(buf), "Sats: %lu | %s | %d%%", 
                 (unsigned long)snap->gps.satellites, timeStr, (int)snap->sys.battery_pct);
        lv_label_set_text(t1_lbl_header, buf);

        snprintf(buf, sizeof(buf), "%.1f km/h", snap->gps.speed_kmh);
        lv_label_set_text(t1_lbl_speed, buf);

        if (snap->gps.fix_valid) {
            double absLat = snap->gps.latitude >= 0 ? snap->gps.latitude : -snap->gps.latitude;
            double absLon = snap->gps.longitude >= 0 ? snap->gps.longitude : -snap->gps.longitude;
            char cLat = snap->gps.latitude >= 0 ? 'N' : 'S';
            char cLon = snap->gps.longitude >= 0 ? 'E' : 'W';

            int32_t latInt = (int32_t)absLat;
            int32_t latDec = (int32_t)((absLat - latInt) * 1000000);
            int32_t lonInt = (int32_t)absLon;
            int32_t lonDec = (int32_t)((absLon - lonInt) * 1000000);

            int32_t gpsAltInt = (int32_t)snap->gps.altitude_m;
            int32_t gpsAltDec = (int32_t)((snap->gps.altitude_m >= 0 ? snap->gps.altitude_m - gpsAltInt : -(snap->gps.altitude_m) - (-gpsAltInt)) * 10);

            snprintf(buf, sizeof(buf), "%ld.%06ld %c\n%ld.%06ld %c\nAlt: %ld.%01ld m (GPS)", 
                     (long)latInt, (long)latDec, cLat,
                     (long)lonInt, (long)lonDec, cLon,
                     (long)gpsAltInt, (long)abs(gpsAltDec));
        } else {
            snprintf(buf, sizeof(buf), "--.------ N\n--.------ E\nAlt: -- m (GPS)");
        }
        lv_label_set_text(t1_lbl_coords, buf);

        // Gộp Alt Baro, P, T, Pitch, Roll
        char baroPart[64] = "-- C\n-- hPa  |  Baro: -- m";
        if (snap->baro.valid) {
            int32_t tInt = (int32_t)snap->baro.temperature_c;
            int32_t tDec = (int32_t)((snap->baro.temperature_c >= 0 ? snap->baro.temperature_c - tInt : -snap->baro.temperature_c + tInt) * 10);
            int32_t pInt = (int32_t)snap->baro.pressure_hpa;
            int32_t pDec = (int32_t)((snap->baro.pressure_hpa - pInt) * 10);
            int32_t altInt = (int32_t)snap->baro.altitude_m;
            int32_t altDec = (int32_t)((snap->baro.altitude_m >= 0 ? snap->baro.altitude_m - altInt : -(snap->baro.altitude_m) - (-altInt)) * 10);

            snprintf(baroPart, sizeof(baroPart), "%ld.%01ld C\n%ld.%01ld hPa  |  Baro: %ld.%01ld m", 
                     (long)tInt, (long)abs(tDec),
                     (long)pInt, (long)abs(pDec), 
                     (long)altInt, (long)abs(altDec));
        }

        char imuPart[64] = "P: --  |  R: --";
        if (snap->imu.valid) {
            float p = snap->imu.pitch_deg;
            float r = snap->imu.roll_deg;
            int32_t pi = (int32_t)p;
            int32_t pd = (int32_t)((p >= 0 ? p - pi : -p + pi) * 10);
            int32_t ri = (int32_t)r;
            int32_t rd = (int32_t)((r >= 0 ? r - ri : -r + ri) * 10);

            char psign = (p < 0) ? '-' : ' ';
            char rsign = (r < 0) ? '-' : ' ';
            snprintf(imuPart, sizeof(imuPart), "P: %c%ld.%01ld  |  R: %c%ld.%01ld", 
                     psign, (long)abs(pi), (long)abs(pd),
                     rsign, (long)abs(ri), (long)abs(rd));
        }

        snprintf(buf, sizeof(buf), "%s\n%s", baroPart, imuPart);
        lv_label_set_text(t1_lbl_env, buf);
    }
    // ═══ Tile 0: Navigation (Trang bên trái) ══════════════════════════════════
    else if (act_tile == s_tile[0]) {
        const char *dir = course_to_dir(snap->gps.course_deg);
        int32_t courseInt = (int32_t)snap->gps.course_deg;
        snprintf(buf, sizeof(buf), "%s (%03ld deg)", dir, (long)courseInt);
        lv_label_set_text(t2_lbl_direction, buf);

        if (snap->gps.fix_valid && snap->gps.speed_kmh > 0.5f) {
            lv_obj_set_style_text_color(t2_lbl_note, CLR_GREEN, 0);
        } else {
            lv_obj_set_style_text_color(t2_lbl_note, CLR_DIMTEXT, 0);
        }
    }
    // ═══ Tile 2: System (Trang bên phải) ══════════════════════════════════════
    else if (act_tile == s_tile[2]) {
        lv_obj_set_style_bg_color(t0_dot_gps, snap->gps.fix_valid ? CLR_GREEN : CLR_RED, 0);

        {
            uint32_t chars_k = snap->gps.chars_proc / 1000;
            uint32_t chars_r = snap->gps.chars_proc % 1000 / 100;
            snprintf(buf, sizeof(buf), "chars=%lu.%luk  fixes=%lu  err=%lu",
                     (unsigned long)chars_k, (unsigned long)chars_r,
                     (unsigned long)snap->gps.fixes, (unsigned long)snap->gps.checksum_err);
            lv_label_set_text(t0_lbl_gps_stats, buf);
        }

        if (snap->gps.date_valid && snap->gps.time_valid) {
            uint8_t local_h = (snap->gps.hour + 7) % 24;
            snprintf(buf, sizeof(buf), "%02u/%02u/%04u  %02u:%02u:%02u",
                     snap->gps.day, snap->gps.month, snap->gps.year,
                     local_h, snap->gps.minute, snap->gps.second);
            lv_label_set_text(t0_lbl_datetime, buf);
        } else {
            lv_label_set_text(t0_lbl_datetime, "--/--/----  --:----");
        }

        {
            uint32_t h = snap->sys.uptime_s / 3600;
            uint32_t m = (snap->sys.uptime_s % 3600) / 60;
            uint32_t s = snap->sys.uptime_s % 60;
            snprintf(buf, sizeof(buf), "%02lu:%02lu:%02lu  #%lu %s", (unsigned long)h, (unsigned long)m, (unsigned long)s,
                     (unsigned long)snap->sys.boot_count,
                     snap->sys.reset_reason ? snap->sys.reset_reason : "");
            lv_label_set_text(t0_lbl_uptime, buf);
        }

        // Battery (Thêm hiển thị Volts)
        int32_t vInt = (int32_t)snap->sys.battery_v;
        int32_t vDec = (int32_t)((snap->sys.battery_v - vInt) * 100);
        snprintf(buf, sizeof(buf), "%ld.%02ld V  |  %d %%", (long)vInt, (long)abs(vDec), (int)snap->sys.battery_pct);
        lv_label_set_text(t0_lbl_battery, buf);

        if (snap->sys.sd_present && snap->sys.sd_ok) {
            if (snap->sys.is_logging_active) {
                uint32_t pts = 0;
                size_t buf_usage = 0;
                sd_logger_get_stats(&pts, &buf_usage);
                snprintf(buf, sizeof(buf), "REC: %lu pts | RAM: %u/1024B", (unsigned long)pts, (unsigned int)buf_usage);
                lv_label_set_text(t0_lbl_sd, buf);
                // Blink text color
                if ((millis() / 500) % 2 == 0) {
                    lv_obj_set_style_text_color(t0_lbl_sd, CLR_YELLOW, 0);
                } else {
                    lv_obj_set_style_text_color(t0_lbl_sd, CLR_GREEN, 0);
                }
            } else {
                snprintf(buf, sizeof(buf), "OK (%lu MB free)", (unsigned long)snap->sys.sd_free_mb);
                lv_label_set_text(t0_lbl_sd, buf);
                lv_obj_set_style_text_color(t0_lbl_sd, CLR_GREEN, 0);
            }
        } else {
            if (snap->sys.sd_free_mb > 0) {
                snprintf(buf, sizeof(buf), "Error 0x%X", (unsigned int)snap->sys.sd_free_mb);
                lv_label_set_text(t0_lbl_sd, buf);
            } else {
                lv_label_set_text(t0_lbl_sd, "No SD card");
            }
            lv_obj_set_style_text_color(t0_lbl_sd, CLR_RED, 0);
        }

        // Cập nhật trạng thái nút ghi log (Chỉ cập nhật khi đổi trạng thái hoặc nhấp nháy REC)
        static bool last_logging_state = false;
        if (snap->sys.is_logging_active != last_logging_state) {
            last_logging_state = snap->sys.is_logging_active;
            if (snap->sys.is_logging_active) {
                lv_label_set_text(t0_lbl_btn_log, "STOP (REC)");
                lv_obj_set_style_bg_color(t0_btn_log, CLR_RED, 0);
                if (t0_dd_mode) lv_obj_add_state(t0_dd_mode, LV_STATE_DISABLED);
            } else {
                lv_label_set_text(t0_lbl_btn_log, "START LOG");
                lv_obj_set_style_bg_color(t0_btn_log, lv_color_hex(0x2A3B4C), 0);
                if (t0_dd_mode) lv_obj_clear_state(t0_dd_mode, LV_STATE_DISABLED);
            }
        } else if (snap->sys.is_logging_active) {
            // Hiệu ứng nhấp nháy Đỏ Sáng / Đỏ Đậm mỗi 500ms khi đang REC
            if ((millis() / 500) % 2 == 0) {
                lv_obj_set_style_bg_color(t0_btn_log, CLR_RED, 0); 
            } else {
                lv_obj_set_style_bg_color(t0_btn_log, lv_color_hex(0x8B0000), 0); // Dark Red
            }
        }
    }
}
