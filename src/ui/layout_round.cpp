/**
 * @file layout_round.cpp
 * @brief Circular Safe-Area Adaptive UI Profile for Round AMOLED (e.g. S3-1.75C 466x466)
 */

#include "ui/ui_layout.h"
#include "ui/ui_theme.h"
#include "ui/ui_viewmodel.h"

extern SensorSnapshot g_snap;

static lv_obj_t *make_centered_row(lv_obj_t *parent, lv_coord_t width_pct, lv_coord_t col_gap)
{
    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(width_pct), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, col_gap, 0);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    ui_theme_strip_scrollbar(row);
    return row;
}

static lv_obj_t *make_centered_col(lv_obj_t *parent, lv_coord_t width_pct, lv_coord_t row_gap)
{
    lv_obj_t *col = lv_obj_create(parent);
    lv_obj_set_size(col, lv_pct(width_pct), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_style_pad_row(col, row_gap, 0);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    ui_theme_strip_scrollbar(col);
    return col;
}

static lv_obj_t *make_round_divider(lv_obj_t *parent, lv_coord_t width_pct)
{
    lv_obj_t *line = ui_theme_make_divider(parent);
    lv_obj_set_width(line, lv_pct(width_pct));
    return line;
}

static void build_round_tile0_nav(lv_obj_t *tile, UiWidgets *w)
{
    const UiThemeFonts *f = ui_theme_fonts();
    ui_theme_style_tile(tile);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(tile, 22, 0);
    lv_obj_set_style_pad_bottom(tile, 22, 0);
    lv_obj_set_style_pad_row(tile, 4, 0);

    // Top Safe-Area Header (~64% width)
    lv_obj_t *hdr = make_centered_row(tile, 64, 8);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    w->t0_lbl_route_name = ui_theme_make_val_label(hdr, "ROUTE", CLR_ACCENT, f->md);
    lv_obj_set_flex_grow(w->t0_lbl_route_name, 1);
    lv_label_set_long_mode(w->t0_lbl_route_name, LV_LABEL_LONG_DOT);

    w->t0_btn_zoom = lv_btn_create(hdr);
    lv_obj_set_size(w->t0_btn_zoom, 76, 28);
    lv_obj_set_style_bg_color(w->t0_btn_zoom, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_border_color(w->t0_btn_zoom, CLR_BORDER, 0);
    lv_obj_set_style_border_width(w->t0_btn_zoom, 1, 0);
    lv_obj_set_style_radius(w->t0_btn_zoom, 14, 0);
    lv_obj_clear_flag(w->t0_btn_zoom, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(w->t0_btn_zoom, ui_vm_btn_zoom_cb, LV_EVENT_CLICKED, w);

    w->t0_lbl_zoom = lv_label_create(w->t0_btn_zoom);
    lv_label_set_text(w->t0_lbl_zoom, "250m");
    lv_obj_set_style_text_font(w->t0_lbl_zoom, f->xs, 0);
    lv_obj_set_style_text_color(w->t0_lbl_zoom, CLR_WHITE, 0);
    lv_obj_clear_flag(w->t0_lbl_zoom, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(w->t0_lbl_zoom);

    // Radar Map Canvas (fills circular middle)
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

    // Bottom Safe-Area Info (~66% width)
    lv_obj_t *row1 = make_centered_row(tile, 68, 8);
    lv_obj_set_flex_align(row1, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    w->t0_lbl_dist_rem = ui_theme_make_val_label(row1, "Rem: -- km", CLR_WHITE, f->md);
    w->t0_lbl_xte      = ui_theme_make_val_label(row1, "ON ROUTE", CLR_GREEN, f->md);

    lv_obj_t *row2 = make_centered_row(tile, 64, 8);
    lv_obj_set_flex_align(row2, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    w->t0_lbl_course    = ui_theme_make_val_label(row2, "HDG: 000 N", CLR_ORANGE, f->sm);
    w->t0_lbl_speed_alt = ui_theme_make_val_label(row2, "0.0 km/h", CLR_DIMTEXT, f->sm);

    w->t0_lbl_map_note = nullptr; // Omitted on round footer to preserve circular safe area
}

static void build_round_tile1_sensors(lv_obj_t *tile, UiWidgets *w)
{
    const UiThemeFonts *f = ui_theme_fonts();
    ui_theme_style_tile(tile);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(tile, 26, 0);
    lv_obj_set_style_pad_bottom(tile, 26, 0);
    lv_obj_set_style_pad_row(tile, 8, 0);

    // Top Safe-Area Header
    lv_obj_t *hdr = make_centered_row(tile, 66, 8);
    w->t1_dot_fix    = ui_theme_make_dot(hdr, CLR_RED, 10);
    w->t1_lbl_header = ui_theme_make_val_label(hdr, "Sats:0 | --:--", CLR_ACCENT, f->sm);
    w->t1_lbl_bat    = ui_theme_make_val_label(hdr, "--%", CLR_GREEN, f->sm);

    make_round_divider(tile, 76);

    // Speed
    lv_obj_t *spd = make_centered_col(tile, 86, 2);
    ui_theme_make_key_label(spd, "Speed");
    w->t1_lbl_speed = ui_theme_make_val_label(spd, "0.0 km/h", CLR_WHITE, f->xl);
    lv_obj_set_style_text_align(w->t1_lbl_speed, LV_TEXT_ALIGN_CENTER, 0);

    make_round_divider(tile, 90);

    // Coordinates
    lv_obj_t *pos = make_centered_col(tile, 86, 2);
    ui_theme_make_key_label(pos, "GPS Position & Alt");
    w->t1_lbl_coords = ui_theme_make_val_label(pos, "--.------ N\n--.------ E\nAlt: -- m (GPS)", CLR_YELLOW, f->md);
    lv_obj_set_style_text_align(w->t1_lbl_coords, LV_TEXT_ALIGN_CENTER, 0);

    make_round_divider(tile, 80);

    // Environment & IMU
    lv_obj_t *env = make_centered_col(tile, 74, 2);
    ui_theme_make_key_label(env, "Environment & IMU");
    w->t1_lbl_env = ui_theme_make_val_label(env, "-- C\n-- hPa | Baro: -- m\nP: -- | R: --", CLR_ENVGREEN, f->sm);
    lv_obj_set_style_text_align(w->t1_lbl_env, LV_TEXT_ALIGN_CENTER, 0);
}

static void build_round_tile2_system(lv_obj_t *tile, UiWidgets *w)
{
    const UiThemeFonts *f = ui_theme_fonts();
    ui_theme_style_tile(tile);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_top(tile, 24, 0);
    lv_obj_set_style_pad_bottom(tile, 26, 0);
    lv_obj_set_style_pad_row(tile, 5, 0);

    ui_theme_make_val_label(tile, "SYSTEM", CLR_ACCENT, f->md);
    make_round_divider(tile, 68);

    lv_obj_t *gps_row = make_centered_row(tile, 82, 6);
    w->t2_dot_gps     = ui_theme_make_dot(gps_row, CLR_RED, 10);
    w->t2_lbl_gps_stats = ui_theme_make_val_label(gps_row, "chars=0.0k fixes=0 err=0", CLR_WHITE, f->sm);

    w->t2_lbl_datetime = ui_theme_make_val_label(tile, "--/--/----  --:--:--", CLR_WHITE, f->sm);
    w->t2_lbl_uptime   = ui_theme_make_val_label(tile, "00:00:00", CLR_GRAY, f->sm);
    w->t2_lbl_battery  = ui_theme_make_val_label(tile, "-- V  |  -- %", CLR_YELLOW, f->md);
    w->t2_lbl_sd       = ui_theme_make_val_label(tile, "Checking SD...", CLR_ACCENT, f->sm);

    make_round_divider(tile, 82);

    // Utility Row (Safe width 78%)
    lv_obj_t *util_row = make_centered_row(tile, 78, 6);

    w->t2_btn_calib = lv_btn_create(util_row);
    lv_obj_set_height(w->t2_btn_calib, 40);
    lv_obj_set_flex_grow(w->t2_btn_calib, 2);
    lv_obj_clear_flag(w->t2_btn_calib, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_bg_color(w->t2_btn_calib, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_border_color(w->t2_btn_calib, CLR_ACCENT, 0);
    lv_obj_set_style_border_width(w->t2_btn_calib, 1, 0);
    lv_obj_set_style_radius(w->t2_btn_calib, 20, 0);
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
    lv_obj_set_style_radius(w->t2_btn_wifi, 20, 0);
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
    lv_obj_set_style_radius(w->t2_btn_ble, 20, 0);
    lv_obj_add_event_cb(w->t2_btn_ble, ui_vm_btn_ble_cb, LV_EVENT_CLICKED, w);

    w->t2_lbl_btn_ble = lv_label_create(w->t2_btn_ble);
    lv_label_set_text(w->t2_lbl_btn_ble, LV_SYMBOL_BLUETOOTH " BLE");
    lv_obj_set_style_text_font(w->t2_lbl_btn_ble, f->xs, 0);
    lv_obj_set_style_text_color(w->t2_lbl_btn_ble, CLR_WHITE, 0);
    lv_obj_clear_flag(w->t2_lbl_btn_ble, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(w->t2_lbl_btn_ble);

    // Power Button (Safe width 60%)
    w->t2_btn_power = lv_btn_create(tile);
    lv_obj_set_size(w->t2_btn_power, lv_pct(60), 34);
    lv_obj_clear_flag(w->t2_btn_power, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_bg_color(w->t2_btn_power, lv_color_hex(0x450A0A), 0);
    lv_obj_set_style_border_color(w->t2_btn_power, CLR_RED, 0);
    lv_obj_set_style_border_width(w->t2_btn_power, 1, 0);
    lv_obj_set_style_radius(w->t2_btn_power, 17, 0);
    lv_obj_add_event_cb(w->t2_btn_power, ui_vm_btn_power_cb, LV_EVENT_CLICKED, w);

    lv_obj_t *lbl_pwr = lv_label_create(w->t2_btn_power);
    lv_label_set_text(lbl_pwr, LV_SYMBOL_POWER " PWR OFF");
    lv_obj_set_style_text_font(lbl_pwr, f->xs, 0);
    lv_obj_set_style_text_color(lbl_pwr, CLR_RED, 0);
    lv_obj_clear_flag(lbl_pwr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(lbl_pwr);

    // Bottom Safe-Area Controls (64% width)
    lv_obj_t *bot = make_centered_row(tile, 64, 8);
    w->t2_dd_mode = lv_dropdown_create(bot);
    lv_dropdown_set_options(w->t2_dd_mode, "Hiking\nCycling");
    lv_dropdown_set_symbol(w->t2_dd_mode, NULL);
    lv_dropdown_set_dir(w->t2_dd_mode, LV_DIR_TOP);
    lv_obj_set_size(w->t2_dd_mode, 96, 44);
    lv_obj_clear_flag(w->t2_dd_mode, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_bg_color(w->t2_dd_mode, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_color(w->t2_dd_mode, CLR_WHITE, 0);
    lv_obj_set_style_text_font(w->t2_dd_mode, f->sm, 0);
    lv_obj_set_style_text_align(w->t2_dd_mode, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_border_color(w->t2_dd_mode, CLR_BORDER, 0);
    lv_obj_set_style_border_width(w->t2_dd_mode, 1, 0);
    lv_obj_set_style_radius(w->t2_dd_mode, 22, 0);
    lv_obj_set_style_pad_top(w->t2_dd_mode, 14, 0);
    lv_obj_set_style_pad_bottom(w->t2_dd_mode, 14, 0);
    lv_dropdown_set_selected(w->t2_dd_mode, (g_snap.sys.activity_mode == ACTIVITY_MODE_BIKE) ? 1 : 0);
    lv_obj_add_event_cb(w->t2_dd_mode, ui_vm_dd_mode_cb, LV_EVENT_VALUE_CHANGED, w);

    w->t2_btn_log = lv_btn_create(bot);
    lv_obj_set_height(w->t2_btn_log, 44);
    lv_obj_set_flex_grow(w->t2_btn_log, 1);
    lv_obj_clear_flag(w->t2_btn_log, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_bg_color(w->t2_btn_log, lv_color_hex(0x2A3B4C), 0);
    lv_obj_set_style_radius(w->t2_btn_log, 22, 0);
    lv_obj_add_event_cb(w->t2_btn_log, ui_vm_btn_log_cb, LV_EVENT_CLICKED, w);

    w->t2_lbl_btn_log = lv_label_create(w->t2_btn_log);
    lv_label_set_text(w->t2_lbl_btn_log, "START LOG");
    lv_obj_set_style_text_font(w->t2_lbl_btn_log, f->sm, 0);
    lv_obj_clear_flag(w->t2_lbl_btn_log, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(w->t2_lbl_btn_log);
}

void ui_layout_build_round(lv_obj_t *scr, UiWidgets *w)
{
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

    build_round_tile0_nav(w->tile[0], w);
    build_round_tile1_sensors(w->tile[1], w);
    build_round_tile2_system(w->tile[2], w);

    for (int i = 0; i < 3; i++) {
        if (w->tile[i]) ui_theme_strip_scrollbar(w->tile[i]);
    }

    lv_obj_set_tile(w->tileview, w->tile[1], LV_ANIM_OFF);
    ui_theme_strip_scrollbar(w->tileview);
}
