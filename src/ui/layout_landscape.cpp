/**
 * @file layout_landscape.cpp
 * @brief 2-Column Split-View Landscape UI Profile (e.g. S3-2.41 600x450 Bike Computer)
 */

#include "ui/ui_layout.h"
#include "ui/ui_theme.h"
#include "ui/ui_viewmodel.h"

extern SensorSnapshot g_snap;

static lv_obj_t *make_card(lv_obj_t *parent, lv_coord_t w_pct, lv_coord_t h_pct, lv_coord_t row_gap)
{
    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, lv_pct(w_pct), lv_pct(h_pct));
    lv_obj_set_style_bg_color(card, CLR_SURFACE, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, CLR_BORDER, 0);
    lv_obj_set_style_border_width(card, 1, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 12, 0);
    lv_obj_set_style_pad_row(card, row_gap, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    ui_theme_strip_scrollbar(card);
    return card;
}

static void build_land_tile0_nav(lv_obj_t *tile, UiWidgets *w)
{
    const UiThemeFonts *f = ui_theme_fonts();
    ui_theme_style_tile(tile);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_all(tile, 10, 0);
    lv_obj_set_style_pad_column(tile, 12, 0);

    // Left Column (56%): Route Header + Breadcrumb Map
    lv_obj_t *left = lv_obj_create(tile);
    lv_obj_set_size(left, lv_pct(56), lv_pct(100));
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);
    lv_obj_set_style_pad_row(left, 6, 0);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);
    ui_theme_strip_scrollbar(left);

    lv_obj_t *hdr = lv_obj_create(left);
    lv_obj_set_size(hdr, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    w->t0_lbl_route_name = ui_theme_make_val_label(hdr, "ROUTE", CLR_ACCENT, f->md);
    lv_obj_set_flex_grow(w->t0_lbl_route_name, 1);
    lv_label_set_long_mode(w->t0_lbl_route_name, LV_LABEL_LONG_DOT);

    w->t0_btn_zoom = lv_btn_create(hdr);
    lv_obj_set_size(w->t0_btn_zoom, 84, 32);
    lv_obj_set_style_bg_color(w->t0_btn_zoom, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_border_color(w->t0_btn_zoom, CLR_BORDER, 0);
    lv_obj_set_style_border_width(w->t0_btn_zoom, 1, 0);
    lv_obj_set_style_radius(w->t0_btn_zoom, 6, 0);
    lv_obj_clear_flag(w->t0_btn_zoom, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(w->t0_btn_zoom, ui_vm_btn_zoom_cb, LV_EVENT_CLICKED, w);

    w->t0_lbl_zoom = lv_label_create(w->t0_btn_zoom);
    lv_label_set_text(w->t0_lbl_zoom, "250m");
    lv_obj_set_style_text_font(w->t0_lbl_zoom, f->sm, 0);
    lv_obj_set_style_text_color(w->t0_lbl_zoom, CLR_WHITE, 0);
    lv_obj_clear_flag(w->t0_lbl_zoom, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(w->t0_lbl_zoom);

    ui_theme_make_divider(left);

    w->t0_map_area = lv_obj_create(left);
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

    // Right Column (44%): Bike-Computer Telemetry Card
    lv_obj_t *right = make_card(tile, 42, 100, 10);
    lv_obj_set_flex_grow(right, 1);

    ui_theme_make_key_label(right, "DISTANCE REMAINING");
    w->t0_lbl_dist_rem = ui_theme_make_val_label(right, "Rem: -- km", CLR_WHITE, f->lg);

    ui_theme_make_key_label(right, "ROUTE STATUS");
    w->t0_lbl_xte = ui_theme_make_val_label(right, "ON ROUTE", CLR_GREEN, f->md);

    ui_theme_make_divider(right);

    ui_theme_make_key_label(right, "HEADING");
    w->t0_lbl_course = ui_theme_make_val_label(right, "HDG: 000 deg N", CLR_ORANGE, f->md);

    ui_theme_make_key_label(right, "SPEED & ALTITUDE");
    w->t0_lbl_speed_alt = ui_theme_make_val_label(right, "0.0 km/h | --m", CLR_WHITE, f->md);

    lv_obj_t *spacer = lv_obj_create(right);
    lv_obj_set_size(spacer, lv_pct(100), 1);
    lv_obj_set_flex_grow(spacer, 1);
    lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(spacer, 0, 0);

    w->t0_lbl_map_note = ui_theme_make_val_label(right, "Tap map/btn to zoom", CLR_DIMTEXT, f->xs);
}

static void build_land_tile1_sensors(lv_obj_t *tile, UiWidgets *w)
{
    const UiThemeFonts *f = ui_theme_fonts();
    ui_theme_style_tile(tile);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tile, 12, 0);
    lv_obj_set_style_pad_row(tile, 10, 0);

    // Top Header Bar
    lv_obj_t *hdr = lv_obj_create(tile);
    lv_obj_set_size(hdr, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_set_style_pad_column(hdr, 10, 0);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    w->t1_dot_fix    = ui_theme_make_dot(hdr, CLR_RED, 12);
    w->t1_lbl_header = ui_theme_make_val_label(hdr, "Sats: 0 | --:-- | 100%", CLR_ACCENT, f->md);

    // 2-Column Body
    lv_obj_t *body = lv_obj_create(tile);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_set_style_pad_column(body, 12, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    // Left Card: Speed & GPS
    lv_obj_t *left = make_card(body, 49, 100, 8);
    ui_theme_make_key_label(left, "Speed");
    w->t1_lbl_speed = ui_theme_make_val_label(left, "0.0 km/h", CLR_WHITE, f->xl);
    ui_theme_make_divider(left);
    ui_theme_make_key_label(left, "GPS Position & Alt");
    w->t1_lbl_coords = ui_theme_make_val_label(left, "--.------ N\n--.------ E\nAlt: -- m (GPS)", CLR_YELLOW, f->md);

    // Right Card: Environment & IMU
    lv_obj_t *right = make_card(body, 49, 100, 8);
    lv_obj_set_flex_grow(right, 1);
    ui_theme_make_key_label(right, "Environment & IMU");
    w->t1_lbl_env = ui_theme_make_val_label(right, "-- C\n-- hPa  |  Baro: -- m\nP: --  |  R: --", CLR_ENVGREEN, f->md);
}

static void build_land_tile2_system(lv_obj_t *tile, UiWidgets *w)
{
    const UiThemeFonts *f = ui_theme_fonts();
    ui_theme_style_tile(tile);
    lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(tile, 12, 0);
    lv_obj_set_style_pad_row(tile, 10, 0);

    ui_theme_make_val_label(tile, "SYSTEM & CONTROLS", CLR_ACCENT, f->md);

    lv_obj_t *body = lv_obj_create(tile);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_set_style_pad_all(body, 0, 0);
    lv_obj_set_style_pad_column(body, 12, 0);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);

    // Left Card: Diagnostics
    lv_obj_t *left = make_card(body, 56, 100, 6);

    lv_obj_t *gps_hdr = lv_obj_create(left);
    lv_obj_set_size(gps_hdr, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(gps_hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(gps_hdr, 0, 0);
    lv_obj_set_style_pad_all(gps_hdr, 0, 0);
    lv_obj_set_style_pad_column(gps_hdr, 8, 0);
    lv_obj_set_flex_flow(gps_hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(gps_hdr, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(gps_hdr, LV_OBJ_FLAG_SCROLLABLE);
    w->t2_dot_gps = ui_theme_make_dot(gps_hdr, CLR_RED, 10);
    ui_theme_make_key_label(gps_hdr, "GPS Status");
    w->t2_lbl_gps_stats = ui_theme_make_val_label(left, "chars=0.0k  fixes=0  err=0", CLR_WHITE, f->sm);

    ui_theme_make_divider(left);
    ui_theme_make_key_label(left, "Date / Time");
    w->t2_lbl_datetime = ui_theme_make_val_label(left, "--/--/----  --:--:--", CLR_WHITE, f->md);

    ui_theme_make_divider(left);
    ui_theme_make_key_label(left, "Uptime");
    w->t2_lbl_uptime = ui_theme_make_val_label(left, "00:00:00", CLR_WHITE, f->md);

    ui_theme_make_divider(left);
    ui_theme_make_key_label(left, "Battery");
    w->t2_lbl_battery = ui_theme_make_val_label(left, "-- V  |  -- %", CLR_YELLOW, f->md);

    ui_theme_make_divider(left);
    ui_theme_make_key_label(left, "SD Card");
    w->t2_lbl_sd = ui_theme_make_val_label(left, "Checking...", CLR_ACCENT, f->md);

    // Right Column: Large Controls
    lv_obj_t *right = lv_obj_create(body);
    lv_obj_set_size(right, lv_pct(42), lv_pct(100));
    lv_obj_set_flex_grow(right, 1);
    lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right, 0, 0);
    lv_obj_set_style_pad_all(right, 0, 0);
    lv_obj_set_style_pad_row(right, 14, 0);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *conn_row = lv_obj_create(right);
    lv_obj_set_size(conn_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(conn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(conn_row, 0, 0);
    lv_obj_set_style_pad_all(conn_row, 0, 0);
    lv_obj_set_style_pad_column(conn_row, 8, 0);
    lv_obj_set_flex_flow(conn_row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(conn_row, LV_OBJ_FLAG_SCROLLABLE);

    w->t2_btn_wifi = lv_btn_create(conn_row);
    lv_obj_set_height(w->t2_btn_wifi, 48);
    lv_obj_set_flex_grow(w->t2_btn_wifi, 1);
    lv_obj_clear_flag(w->t2_btn_wifi, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_bg_color(w->t2_btn_wifi, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_border_color(w->t2_btn_wifi, CLR_BORDER, 0);
    lv_obj_set_style_border_width(w->t2_btn_wifi, 1, 0);
    lv_obj_set_style_radius(w->t2_btn_wifi, 10, 0);
    lv_obj_add_event_cb(w->t2_btn_wifi, ui_vm_btn_wifi_cb, LV_EVENT_CLICKED, w);

    w->t2_lbl_btn_wifi = lv_label_create(w->t2_btn_wifi);
    lv_label_set_text(w->t2_lbl_btn_wifi, LV_SYMBOL_WIFI " WIFI");
    lv_obj_set_style_text_font(w->t2_lbl_btn_wifi, f->sm, 0);
    lv_obj_set_style_text_color(w->t2_lbl_btn_wifi, CLR_WHITE, 0);
    lv_obj_clear_flag(w->t2_lbl_btn_wifi, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(w->t2_lbl_btn_wifi);

    w->t2_btn_ble = lv_btn_create(conn_row);
    lv_obj_set_height(w->t2_btn_ble, 48);
    lv_obj_set_flex_grow(w->t2_btn_ble, 1);
    lv_obj_clear_flag(w->t2_btn_ble, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_bg_color(w->t2_btn_ble, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_border_color(w->t2_btn_ble, CLR_BORDER, 0);
    lv_obj_set_style_border_width(w->t2_btn_ble, 1, 0);
    lv_obj_set_style_radius(w->t2_btn_ble, 10, 0);
    lv_obj_add_event_cb(w->t2_btn_ble, ui_vm_btn_ble_cb, LV_EVENT_CLICKED, w);

    w->t2_lbl_btn_ble = lv_label_create(w->t2_btn_ble);
    lv_label_set_text(w->t2_lbl_btn_ble, LV_SYMBOL_BLUETOOTH " BLE");
    lv_obj_set_style_text_font(w->t2_lbl_btn_ble, f->sm, 0);
    lv_obj_set_style_text_color(w->t2_lbl_btn_ble, CLR_WHITE, 0);
    lv_obj_clear_flag(w->t2_lbl_btn_ble, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(w->t2_lbl_btn_ble);

    w->t2_btn_calib = lv_btn_create(right);
    lv_obj_set_size(w->t2_btn_calib, lv_pct(100), 56);
    lv_obj_clear_flag(w->t2_btn_calib, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_bg_color(w->t2_btn_calib, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_border_color(w->t2_btn_calib, CLR_ACCENT, 0);
    lv_obj_set_style_border_width(w->t2_btn_calib, 1, 0);
    lv_obj_set_style_radius(w->t2_btn_calib, 10, 0);
    lv_obj_add_event_cb(w->t2_btn_calib, ui_vm_btn_calib_cb, LV_EVENT_CLICKED, w);

    lv_obj_t *lbl_calib = lv_label_create(w->t2_btn_calib);
    lv_label_set_text(lbl_calib, LV_SYMBOL_REFRESH " CALIB ALT (GPS)");
    lv_obj_set_style_text_font(lbl_calib, f->md, 0);
    lv_obj_set_style_text_color(lbl_calib, CLR_ACCENT, 0);
    lv_obj_clear_flag(lbl_calib, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(lbl_calib);

    w->t2_dd_mode = lv_dropdown_create(right);
    lv_dropdown_set_options(w->t2_dd_mode, "Hiking\nCycling");
    lv_dropdown_set_symbol(w->t2_dd_mode, NULL);
    lv_dropdown_set_dir(w->t2_dd_mode, LV_DIR_TOP);
    lv_obj_set_size(w->t2_dd_mode, lv_pct(100), 52);
    lv_obj_clear_flag(w->t2_dd_mode, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_bg_color(w->t2_dd_mode, lv_color_hex(0x1E293B), 0);
    lv_obj_set_style_text_color(w->t2_dd_mode, CLR_WHITE, 0);
    lv_obj_set_style_text_font(w->t2_dd_mode, f->md, 0);
    lv_obj_set_style_text_align(w->t2_dd_mode, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_border_color(w->t2_dd_mode, CLR_BORDER, 0);
    lv_obj_set_style_border_width(w->t2_dd_mode, 1, 0);
    lv_obj_set_style_radius(w->t2_dd_mode, 10, 0);
    lv_obj_set_style_pad_top(w->t2_dd_mode, 16, 0);
    lv_obj_set_style_pad_bottom(w->t2_dd_mode, 16, 0);
    lv_dropdown_set_selected(w->t2_dd_mode, (g_snap.sys.activity_mode == ACTIVITY_MODE_BIKE) ? 1 : 0);
    lv_obj_add_event_cb(w->t2_dd_mode, ui_vm_dd_mode_cb, LV_EVENT_VALUE_CHANGED, w);

    w->t2_btn_log = lv_btn_create(right);
    lv_obj_set_size(w->t2_btn_log, lv_pct(100), 64);
    lv_obj_clear_flag(w->t2_btn_log, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_set_style_bg_color(w->t2_btn_log, lv_color_hex(0x2A3B4C), 0);
    lv_obj_set_style_radius(w->t2_btn_log, 10, 0);
    lv_obj_add_event_cb(w->t2_btn_log, ui_vm_btn_log_cb, LV_EVENT_CLICKED, w);

    w->t2_lbl_btn_log = lv_label_create(w->t2_btn_log);
    lv_label_set_text(w->t2_lbl_btn_log, "START LOG");
    lv_obj_set_style_text_font(w->t2_lbl_btn_log, f->md, 0);
    lv_obj_clear_flag(w->t2_lbl_btn_log, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(w->t2_lbl_btn_log);
}

void ui_layout_build_landscape(lv_obj_t *scr, UiWidgets *w)
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

    build_land_tile0_nav(w->tile[0], w);
    build_land_tile1_sensors(w->tile[1], w);
    build_land_tile2_system(w->tile[2], w);

    for (int i = 0; i < 3; i++) {
        if (w->tile[i]) ui_theme_strip_scrollbar(w->tile[i]);
    }

    lv_obj_set_tile(w->tileview, w->tile[1], LV_ANIM_OFF);
    ui_theme_strip_scrollbar(w->tileview);
}
