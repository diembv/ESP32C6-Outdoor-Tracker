/**
 * @file layout_portrait.cpp
 * @brief Adaptive Flex-Column Portrait/Square UI Profile
 *        Supports: 280x456 (C6-1.64), 368x448 (S3-1.8), 410x502 (S3-2.06), 480x480 (S3-2.16)
 */

#include "ui/ui_layout.h"
#include "ui/ui_theme.h"
#include "ui/ui_viewmodel.h"

extern SensorSnapshot g_snap;

static lv_obj_t *make_flex_row(lv_obj_t *parent, lv_flex_align_t main_align, lv_coord_t col_gap)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, col_gap, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, main_align, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    ui_theme_strip_scrollbar(row);
    return row;
}

static lv_obj_t *make_flex_col(lv_obj_t *parent, lv_coord_t row_gap)
{
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_set_size(col, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_style_pad_row(col, row_gap, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    ui_theme_strip_scrollbar(col);
    return col;
}

static void build_tile0_nav(lv_obj_t *tile, UiWidgets *w, lv_coord_t pad, lv_coord_t gap)
{
    const UiThemeFonts *f = ui_theme_fonts();
    ui_theme_style_tile(tile);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(tile, pad, 0);
    lv_obj_set_style_pad_ver(tile, pad, 0);
    lv_obj_set_style_pad_row(tile, gap, 0);

    // 1. Header: Route Name & Zoom Button
    lv_obj_t *hdr = make_flex_row(tile, LV_FLEX_ALIGN_SPACE_BETWEEN, 8);
    w->t0_lbl_route_name = ui_theme_make_val_label(hdr, "ROUTE", CLR_ACCENT, f->md);
    lv_obj_set_flex_grow(w->t0_lbl_route_name, 1);
    lv_label_set_long_mode(w->t0_lbl_route_name, LV_LABEL_LONG_DOT);

    w->t0_btn_zoom = lv_btn_create(hdr);
    lv_obj_set_size(w->t0_btn_zoom, (SCREEN_W >= 360) ? 88 : 74, 30);
    lv_obj_set_style_bg_color(w->t0_btn_zoom, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_bg_color(w->t0_btn_zoom, lv_color_hex(0x374151), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(w->t0_btn_zoom, CLR_BORDER, 0);
    lv_obj_set_style_border_width(w->t0_btn_zoom, 1, 0);
    lv_obj_set_style_radius(w->t0_btn_zoom, 6, 0);
    lv_obj_clear_flag(w->t0_btn_zoom, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(w->t0_btn_zoom, ui_vm_btn_zoom_cb, LV_EVENT_CLICKED, w);

    w->t0_lbl_zoom = lv_label_create(w->t0_btn_zoom);
    lv_label_set_text(w->t0_lbl_zoom, "250m");
    lv_obj_set_style_text_font(w->t0_lbl_zoom, f->xs, 0);
    lv_obj_set_style_text_color(w->t0_lbl_zoom, CLR_WHITE, 0);
    lv_obj_clear_flag(w->t0_lbl_zoom, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(w->t0_lbl_zoom);

    ui_theme_make_divider(tile);

    // 2. Map Canvas Area (grows to fill available vertical space)
    w->t0_map_area = lv_obj_create(tile);
    lv_obj_set_width(w->t0_map_area, lv_pct(100));
    lv_obj_set_flex_grow(w->t0_map_area, 1);
    lv_obj_set_style_bg_color(w->t0_map_area, CLR_BG, 0);
    lv_obj_set_style_bg_opa(w->t0_map_area, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(w->t0_map_area, 0, 0);
    lv_obj_set_style_pad_all(w->t0_map_area, 0, 0);
    lv_obj_clear_flag(w->t0_map_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(w->t0_map_area, ui_vm_map_draw_cb, LV_EVENT_DRAW_MAIN, w);
    lv_obj_add_event_cb(w->t0_map_area, ui_vm_btn_zoom_cb, LV_EVENT_CLICKED, w);
    ui_theme_strip_scrollbar(w->t0_map_area);

    ui_theme_make_divider(tile);

    // 3. Bottom Info Panel
    lv_obj_t *row1 = make_flex_row(tile, LV_FLEX_ALIGN_SPACE_BETWEEN, 6);
    w->t0_lbl_dist_rem = ui_theme_make_val_label(row1, "Rem: -- km", CLR_WHITE, f->md);
    w->t0_lbl_xte      = ui_theme_make_val_label(row1, "ON ROUTE", CLR_GREEN, f->md);

    ui_theme_make_divider(tile);

    lv_obj_t *row2 = make_flex_row(tile, LV_FLEX_ALIGN_SPACE_BETWEEN, 6);
    w->t0_lbl_course    = ui_theme_make_val_label(row2, "HDG: 000 deg N", CLR_ORANGE, f->sm);
    w->t0_lbl_speed_alt = ui_theme_make_val_label(row2, "0.0 km/h | --m", CLR_DIMTEXT, f->sm);

    w->t0_lbl_map_note = ui_theme_make_val_label(tile, "Tap map/btn to zoom (100m-2.5km)", CLR_DIMTEXT, f->xs);
}

static void build_tile1_sensors(lv_obj_t *tile, UiWidgets *w, lv_coord_t pad, lv_coord_t gap)
{
    const UiThemeFonts *f = ui_theme_fonts();
    ui_theme_style_tile(tile);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(tile, pad, 0);
    lv_obj_set_style_pad_ver(tile, pad, 0);
    lv_obj_set_style_pad_row(tile, gap + 2, 0);

    // Header
    lv_obj_t *hdr = make_flex_row(tile, LV_FLEX_ALIGN_START, 8);
    w->t1_dot_fix    = ui_theme_make_dot(hdr, CLR_RED, 10);
    w->t1_lbl_header = ui_theme_make_val_label(hdr, "Sats: 0 | --:-- | 100%", CLR_ACCENT, f->md);
    lv_obj_set_flex_grow(w->t1_lbl_header, 1);

    ui_theme_make_divider(tile);

    // Speed
    lv_obj_t *spd_box = make_flex_col(tile, 4);
    lv_obj_set_flex_align(spd_box, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    ui_theme_make_key_label(spd_box, "Speed");
    w->t1_lbl_speed = ui_theme_make_val_label(spd_box, "0.0 km/h", CLR_WHITE, f->xl);
    lv_obj_set_style_text_align(w->t1_lbl_speed, LV_TEXT_ALIGN_CENTER, 0);

    ui_theme_make_divider(tile);

    // GPS Position & Alt
    lv_obj_t *pos_box = make_flex_col(tile, 4);
    ui_theme_make_key_label(pos_box, "GPS Position & Alt");
    w->t1_lbl_coords = ui_theme_make_val_label(pos_box, "--.------ N\n--.------ E\nAlt: -- m (GPS)", CLR_YELLOW, f->md);

    ui_theme_make_divider(tile);

    // Environment & IMU
    lv_obj_t *env_box = make_flex_col(tile, 4);
    ui_theme_make_key_label(env_box, "Environment & IMU");
    w->t1_lbl_env = ui_theme_make_val_label(env_box, "-- C\n-- hPa  |  Baro: -- m\nP: --  |  R: --", CLR_ENVGREEN, f->md);
}

static void build_tile2_system(lv_obj_t *tile, UiWidgets *w, lv_coord_t pad, lv_coord_t gap)
{
    const UiThemeFonts *f = ui_theme_fonts();
    const lv_font_t *val_font = (SCREEN_W >= 360) ? f->md : f->sm;
    lv_coord_t btn_h = (SCREEN_H >= 480) ? 54 : 46;

    ui_theme_style_tile(tile);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_hor(tile, pad, 0);
    lv_obj_set_style_pad_ver(tile, pad, 0);
    lv_obj_set_style_pad_row(tile, gap, 0);

    ui_theme_make_val_label(tile, "SYSTEM", CLR_ACCENT, f->md);
    ui_theme_make_divider(tile);

    // GPS Stats
    lv_obj_t *gps_col = make_flex_col(tile, 2);
    lv_obj_t *gps_hdr = make_flex_row(gps_col, LV_FLEX_ALIGN_START, 8);
    w->t2_dot_gps = ui_theme_make_dot(gps_hdr, CLR_RED, 10);
    ui_theme_make_key_label(gps_hdr, "GPS Status");
    w->t2_lbl_gps_stats = ui_theme_make_val_label(gps_col, "chars=0.0k  fixes=0  err=0", CLR_WHITE, val_font);

    ui_theme_make_divider(tile);

    // Date / Time
    lv_obj_t *dt_col = make_flex_col(tile, 2);
    ui_theme_make_key_label(dt_col, "Date / Time");
    w->t2_lbl_datetime = ui_theme_make_val_label(dt_col, "--/--/----  --:--:--", CLR_WHITE, val_font);

    ui_theme_make_divider(tile);

    // Uptime
    lv_obj_t *up_col = make_flex_col(tile, 2);
    ui_theme_make_key_label(up_col, "Uptime");
    w->t2_lbl_uptime = ui_theme_make_val_label(up_col, "00:00:00", CLR_WHITE, val_font);

    ui_theme_make_divider(tile);

    // Battery
    lv_obj_t *bat_col = make_flex_col(tile, 2);
    ui_theme_make_key_label(bat_col, "Battery");
    w->t2_lbl_battery = ui_theme_make_val_label(bat_col, "-- V  |  -- %", CLR_YELLOW, val_font);

    ui_theme_make_divider(tile);

    // SD Card
    lv_obj_t *sd_col = make_flex_col(tile, 2);
    ui_theme_make_key_label(sd_col, "SD Card");
    w->t2_lbl_sd = ui_theme_make_val_label(sd_col, "Checking...", CLR_ACCENT, val_font);

    // Flexible spacer before bottom buttons
    lv_obj_t *spacer = lv_obj_create(tile);
    lv_obj_set_size(spacer, lv_pct(100), 1);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);
    lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);

    // Utility Row: [CALIB] + [WIFI] + [BLE]
    lv_obj_t *util_row = make_flex_row(tile, LV_FLEX_ALIGN_SPACE_BETWEEN, 6);

    w->t2_btn_calib = lv_btn_create(util_row);
    lv_obj_set_height(w->t2_btn_calib, 40);
    lv_obj_set_flex_grow(w->t2_btn_calib, 2);
    lv_obj_clear_flag(w->t2_btn_calib, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_bg_color(w->t2_btn_calib, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_bg_color(w->t2_btn_calib, lv_color_hex(0x374151), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(w->t2_btn_calib, CLR_ACCENT, 0);
    lv_obj_set_style_border_width(w->t2_btn_calib, 1, 0);
    lv_obj_set_style_radius(w->t2_btn_calib, 8, 0);
    lv_obj_add_event_cb(w->t2_btn_calib, ui_vm_btn_calib_cb, LV_EVENT_CLICKED, w);

    lv_obj_t *lbl_calib = lv_label_create(w->t2_btn_calib);
    lv_label_set_text(lbl_calib, LV_SYMBOL_REFRESH " CALIB");
    lv_obj_set_style_text_font(lbl_calib, f->xs, 0);
    lv_obj_set_style_text_color(lbl_calib, CLR_ACCENT, 0);
    lv_obj_clear_flag(lbl_calib, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(lbl_calib);

    w->t2_btn_wifi = lv_btn_create(util_row);
    lv_obj_set_height(w->t2_btn_wifi, 40);
    lv_obj_set_flex_grow(w->t2_btn_wifi, 1);
    lv_obj_clear_flag(w->t2_btn_wifi, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_bg_color(w->t2_btn_wifi, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_border_color(w->t2_btn_wifi, CLR_BORDER, 0);
    lv_obj_set_style_border_width(w->t2_btn_wifi, 1, 0);
    lv_obj_set_style_radius(w->t2_btn_wifi, 8, 0);
    lv_obj_add_event_cb(w->t2_btn_wifi, ui_vm_btn_wifi_cb, LV_EVENT_CLICKED, w);

    w->t2_lbl_btn_wifi = lv_label_create(w->t2_btn_wifi);
    lv_label_set_text(w->t2_lbl_btn_wifi, LV_SYMBOL_WIFI " WIFI");
    lv_obj_set_style_text_font(w->t2_lbl_btn_wifi, f->xs, 0);
    lv_obj_set_style_text_color(w->t2_lbl_btn_wifi, CLR_WHITE, 0);
    lv_obj_clear_flag(w->t2_lbl_btn_wifi, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(w->t2_lbl_btn_wifi);

    w->t2_btn_ble = lv_btn_create(util_row);
    lv_obj_set_height(w->t2_btn_ble, 40);
    lv_obj_set_flex_grow(w->t2_btn_ble, 1);
    lv_obj_clear_flag(w->t2_btn_ble, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_bg_color(w->t2_btn_ble, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_border_color(w->t2_btn_ble, CLR_BORDER, 0);
    lv_obj_set_style_border_width(w->t2_btn_ble, 1, 0);
    lv_obj_set_style_radius(w->t2_btn_ble, 8, 0);
    lv_obj_add_event_cb(w->t2_btn_ble, ui_vm_btn_ble_cb, LV_EVENT_CLICKED, w);

    w->t2_lbl_btn_ble = lv_label_create(w->t2_btn_ble);
    lv_label_set_text(w->t2_lbl_btn_ble, LV_SYMBOL_BLUETOOTH " BLE");
    lv_obj_set_style_text_font(w->t2_lbl_btn_ble, f->xs, 0);
    lv_obj_set_style_text_color(w->t2_lbl_btn_ble, CLR_WHITE, 0);
    lv_obj_clear_flag(w->t2_lbl_btn_ble, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(w->t2_lbl_btn_ble);

    // Bottom Row: [Mode Dropdown] + [Start/Stop Log Button]
    lv_obj_t *bot_row = make_flex_row(tile, LV_FLEX_ALIGN_SPACE_BETWEEN, 8);

    w->t2_dd_mode = lv_dropdown_create(bot_row);
    lv_dropdown_set_options(w->t2_dd_mode, "Hiking\nCycling");
    lv_dropdown_set_symbol(w->t2_dd_mode, NULL);
    lv_dropdown_set_dir(w->t2_dd_mode, LV_DIR_TOP);
    lv_obj_set_size(w->t2_dd_mode, (SCREEN_W >= 360) ? 120 : 96, btn_h);
    lv_obj_clear_flag(w->t2_dd_mode, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_ext_click_area(w->t2_dd_mode, 10);
    lv_obj_set_style_bg_color(w->t2_dd_mode, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_color(w->t2_dd_mode, CLR_WHITE, 0);
    lv_obj_set_style_text_font(w->t2_dd_mode, f->sm, 0);
    lv_obj_set_style_text_align(w->t2_dd_mode, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_border_color(w->t2_dd_mode, CLR_BORDER, 0);
    lv_obj_set_style_border_width(w->t2_dd_mode, 1, 0);
    lv_obj_set_style_radius(w->t2_dd_mode, 8, 0);
    lv_coord_t vpad = (btn_h - 16) / 2;
    lv_obj_set_style_pad_top(w->t2_dd_mode, vpad, 0);
    lv_obj_set_style_pad_bottom(w->t2_dd_mode, vpad, 0);
    lv_obj_set_style_pad_left(w->t2_dd_mode, 0, 0);
    lv_obj_set_style_pad_right(w->t2_dd_mode, 0, 0);
    lv_dropdown_set_selected(w->t2_dd_mode, (g_snap.sys.activity_mode == ACTIVITY_MODE_BIKE) ? 1 : 0);
    lv_obj_add_event_cb(w->t2_dd_mode, ui_vm_dd_mode_cb, LV_EVENT_VALUE_CHANGED, w);

    lv_obj_t *list = lv_dropdown_get_list(w->t2_dd_mode);
    lv_obj_set_style_bg_color(list, lv_color_hex(0x0D1117), 0);
    lv_obj_set_style_text_color(list, CLR_WHITE, 0);
    lv_obj_set_style_border_color(list, CLR_ACCENT, 0);
    lv_obj_set_style_border_width(list, 2, 0);
    lv_obj_set_style_radius(list, 8, 0);
    lv_obj_set_style_text_font(list, f->sm, 0);
    lv_obj_set_style_text_align(list, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_all(list, 8, 0);

    w->t2_btn_log = lv_btn_create(bot_row);
    lv_obj_set_height(w->t2_btn_log, btn_h);
    lv_obj_set_flex_grow(w->t2_btn_log, 1);
    lv_obj_clear_flag(w->t2_btn_log, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_ext_click_area(w->t2_btn_log, 10);
    lv_obj_set_style_bg_color(w->t2_btn_log, lv_color_hex(0x2A3B4C), 0);
    lv_obj_set_style_bg_color(w->t2_btn_log, lv_color_hex(0x007ACC), LV_STATE_PRESSED);
    lv_obj_set_style_radius(w->t2_btn_log, 8, 0);
    lv_obj_add_event_cb(w->t2_btn_log, ui_vm_btn_log_cb, LV_EVENT_CLICKED, w);

    w->t2_lbl_btn_log = lv_label_create(w->t2_btn_log);
    lv_label_set_text(w->t2_lbl_btn_log, "START LOG");
    lv_obj_set_style_text_font(w->t2_lbl_btn_log, f->sm, 0);
    lv_obj_clear_flag(w->t2_lbl_btn_log, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(w->t2_lbl_btn_log);
}

void ui_layout_build_portrait(lv_obj_t *scr, UiWidgets *w)
{
    lv_coord_t pad = (SCREEN_W >= 360) ? 12 : 8;
    lv_coord_t gap = (SCREEN_H >= 480) ? 6 : 4;

    w->tileview = lv_tileview_create(scr);
    lv_obj_set_size(w->tileview, SCREEN_W, SCREEN_H);
    lv_obj_set_pos(w->tileview, 0, 0);
    lv_obj_set_style_bg_color(w->tileview, CLR_BG, 0);
    lv_obj_set_style_bg_opa(w->tileview, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(w->tileview, 0, 0);
    ui_theme_strip_scrollbar(w->tileview);

    w->tile[0] = lv_tileview_add_tile(w->tileview, 0, 0, LV_DIR_RIGHT);
    w->tile[1] = lv_tileview_add_tile(w->tileview, 1, 0, LV_DIR_HOR);
    w->tile[2] = lv_tileview_add_tile(w->tileview, 2, 0, LV_DIR_LEFT);

    build_tile0_nav(w->tile[0], w, pad, gap);
    build_tile1_sensors(w->tile[1], w, pad, gap);
    build_tile2_system(w->tile[2], w, pad, gap);

    for (int i = 0; i < 3; i++) {
        if (w->tile[i]) ui_theme_strip_scrollbar(w->tile[i]);
    }

    lv_obj_set_tile(w->tileview, w->tile[1], LV_ANIM_OFF);
    ui_theme_strip_scrollbar(w->tileview);
}
