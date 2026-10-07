/**
 * @file ui_dashboard.cpp
 * @brief LVGL 3-page Dashboard — Milestone 4 Implementation
 */

#include "ui_dashboard.h"
#include "route_nav.h"
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

// ── Tile 0: Navigation & Breadcrumb Map (Bên trái) ───────────────────────────
static lv_obj_t *t0_lbl_route_name; 
static lv_obj_t *t0_btn_zoom;
static lv_obj_t *t0_lbl_zoom;
static lv_obj_t *t0_map_area;
static lv_obj_t *t0_lbl_dist_rem;
static lv_obj_t *t0_lbl_xte;
static lv_obj_t *t0_lbl_course;
static lv_obj_t *t0_lbl_speed_alt;
static lv_obj_t *t0_lbl_map_note;     

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
//  Tile 0: Navigation & Breadcrumb Map (Bên trái)
// ═══════════════════════════════════════════════════════════════════════════════

extern SensorSnapshot g_snap;

static void btn_zoom_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {
        route_nav_cycle_zoom();
        const RouteNavStatus *nav = route_nav_get_status();
        if (nav && t0_lbl_zoom) {
            char zbuf[16];
            if (nav->zoom_radius_m >= 1000.0f) {
                snprintf(zbuf, sizeof(zbuf), "%.1fkm", nav->zoom_radius_m / 1000.0f);
            } else {
                snprintf(zbuf, sizeof(zbuf), "%dm", (int)nav->zoom_radius_m);
            }
            lv_label_set_text(t0_lbl_zoom, zbuf);
        }
        if (t0_map_area) {
            lv_obj_invalidate(t0_map_area);
        }
    }
}

static void map_draw_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_DRAW_MAIN) return;

    lv_obj_t * obj = lv_event_get_target(e);
    lv_draw_ctx_t * draw_ctx = lv_event_get_draw_ctx(e);
    if (!draw_ctx) return;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    int16_t cx = (coords.x1 + coords.x2) / 2; // 140
    int16_t cy = (coords.y1 + coords.y2) / 2; // ~206

    // 1. Vẽ các vòng cự ly (Range Rings / Scale Circles)
    lv_point_t center_pt = { (lv_coord_t)cx, (lv_coord_t)cy };

    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = lv_color_hex(0x21262D); // Subtle dark slate
    arc_dsc.width = 1;
    lv_draw_arc(draw_ctx, &arc_dsc, &center_pt, 105, 0, 360);

    arc_dsc.color = lv_color_hex(0x161C24);
    lv_draw_arc(draw_ctx, &arc_dsc, &center_pt, 52, 0, 360);

    // Chữ 'N' chỉ hướng Bắc
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.color = CLR_DIMTEXT;
    label_dsc.font  = &lv_font_montserrat_12;
    lv_area_t n_area;
    n_area.x1 = cx - 8; n_area.x2 = cx + 8;
    n_area.y1 = cy - 105 - 14; n_area.y2 = cy - 105;
    lv_draw_label(draw_ctx, &label_dsc, &n_area, "N", NULL);

    // 2. Vẽ Vệt Lộ Trình Tuyến Đường (Route Polyline)
    const RouteNavStatus *nav = route_nav_get_status();
    if (nav && nav->is_active && nav->window_points && nav->window_count > 1) {
        lv_draw_line_dsc_t line_dsc;
        lv_draw_line_dsc_init(&line_dsc);
        line_dsc.color = CLR_ACCENT; // 0x00FFFF
        line_dsc.width = 3;
        line_dsc.round_start = 1;
        line_dsc.round_end = 1;

        double ref_lat = g_snap.gps.fix_valid ? g_snap.gps.latitude : (double)nav->window_points[0].lat;
        double ref_lon = g_snap.gps.fix_valid ? g_snap.gps.longitude : (double)nav->window_points[0].lon;
        float radius = nav->zoom_radius_m;

        int16_t prev_x = 0, prev_y = 0;
        bool has_prev = false;

        for (uint32_t i = 0; i < nav->window_count; i++) {
            int16_t sx, sy;
            bool ok = route_nav_project_to_screen(
                nav->window_points[i].lat, nav->window_points[i].lon,
                ref_lat, ref_lon,
                cx, cy, radius,
                &sx, &sy
            );

            if (has_prev && ok) {
                lv_point_t p1 = { (lv_coord_t)prev_x, (lv_coord_t)prev_y };
                lv_point_t p2 = { (lv_coord_t)sx, (lv_coord_t)sy };
                lv_draw_line(draw_ctx, &line_dsc, &p1, &p2);
            }
            prev_x = sx;
            prev_y = sy;
            has_prev = ok;
        }

        // Điểm Đích (Đỏ) nếu chạm đuôi route
        if (nav->window_start_idx + nav->window_count >= nav->total_points) {
            int16_t fx, fy;
            uint32_t last_i = nav->window_count - 1;
            if (route_nav_project_to_screen(nav->window_points[last_i].lat, nav->window_points[last_i].lon,
                                           ref_lat, ref_lon, cx, cy, radius, &fx, &fy)) {
                lv_draw_rect_dsc_t fin_dsc;
                lv_draw_rect_dsc_init(&fin_dsc);
                fin_dsc.bg_color = CLR_RED;
                fin_dsc.radius = LV_RADIUS_CIRCLE;
                lv_area_t fin_area = { (lv_coord_t)(fx - 4), (lv_coord_t)(fy - 4), (lv_coord_t)(fx + 4), (lv_coord_t)(fy + 4) };
                lv_draw_rect(draw_ctx, &fin_dsc, &fin_area);
            }
        }

        // Điểm Xuất phát (Xanh lá) nếu chạm đầu route
        if (nav->window_start_idx == 0) {
            int16_t sx, sy;
            if (route_nav_project_to_screen(nav->window_points[0].lat, nav->window_points[0].lon,
                                           ref_lat, ref_lon, cx, cy, radius, &sx, &sy)) {
                lv_draw_rect_dsc_t st_dsc;
                lv_draw_rect_dsc_init(&st_dsc);
                st_dsc.bg_color = CLR_GREEN;
                st_dsc.radius = LV_RADIUS_CIRCLE;
                lv_area_t st_area = { (lv_coord_t)(sx - 4), (lv_coord_t)(sy - 4), (lv_coord_t)(sx + 4), (lv_coord_t)(sy + 4) };
                lv_draw_rect(draw_ctx, &st_dsc, &st_area);
            }
        }
    } else {
        // Thông báo khi chưa nạp route
        lv_draw_label_dsc_t no_route_dsc;
        lv_draw_label_dsc_init(&no_route_dsc);
        no_route_dsc.color = CLR_DIMTEXT;
        no_route_dsc.font = &lv_font_montserrat_12;
        no_route_dsc.align = LV_TEXT_ALIGN_CENTER;
        lv_area_t msg_area = { coords.x1, (lv_coord_t)(cy - 12), coords.x2, (lv_coord_t)(cy + 12) };
        lv_draw_label(draw_ctx, &no_route_dsc, &msg_area, "NO ROUTE (Put .bin in /sdcard/routes/)", NULL);
    }

    // 3. Vẽ Vị Trí & Hướng Người Dùng (User Marker) ở tâm (cx, cy)
    if (g_snap.gps.fix_valid) {
        if (g_snap.gps.speed_kmh >= 0.5f) {
            // Đang di chuyển: Mũi tên hướng theo Course
            float rad = g_snap.gps.course_deg * (float)(M_PI / 180.0);
            float sin_h = sinf(rad);
            float cos_h = cosf(rad);

            lv_point_t p_tip   = { (lv_coord_t)(cx + 14 * sin_h), (lv_coord_t)(cy - 14 * cos_h) };
            lv_point_t p_left  = { (lv_coord_t)(cx - 9 * cos_h - 7 * sin_h), (lv_coord_t)(cy - 9 * sin_h + 7 * cos_h) };
            lv_point_t p_right = { (lv_coord_t)(cx + 9 * cos_h - 7 * sin_h), (lv_coord_t)(cy + 9 * sin_h + 7 * cos_h) };
            lv_point_t p_notch = { (lv_coord_t)(cx - 3 * sin_h), (lv_coord_t)(cy + 3 * cos_h) };

            lv_draw_line_dsc_t arrow_dsc;
            lv_draw_line_dsc_init(&arrow_dsc);
            arrow_dsc.color = CLR_ORANGE;
            arrow_dsc.width = 2;
            arrow_dsc.round_start = 1;
            arrow_dsc.round_end = 1;

            lv_draw_line(draw_ctx, &arrow_dsc, &p_left, &p_tip);
            lv_draw_line(draw_ctx, &arrow_dsc, &p_tip, &p_right);
            lv_draw_line(draw_ctx, &arrow_dsc, &p_right, &p_notch);
            lv_draw_line(draw_ctx, &arrow_dsc, &p_notch, &p_left);
        } else {
            // Đứng yên: Chấm xanh viền vàng
            lv_draw_rect_dsc_t dot_dsc;
            lv_draw_rect_dsc_init(&dot_dsc);
            dot_dsc.bg_color = CLR_GREEN;
            dot_dsc.radius = LV_RADIUS_CIRCLE;
            dot_dsc.border_color = CLR_YELLOW;
            dot_dsc.border_width = 2;
            lv_area_t dot_area = { (lv_coord_t)(cx - 5), (lv_coord_t)(cy - 5), (lv_coord_t)(cx + 5), (lv_coord_t)(cy + 5) };
            lv_draw_rect(draw_ctx, &dot_dsc, &dot_area);
        }
    } else {
        // Chưa fix: Chấm đỏ cảnh báo
        lv_draw_rect_dsc_t dot_dsc;
        lv_draw_rect_dsc_init(&dot_dsc);
        dot_dsc.bg_color = CLR_RED;
        dot_dsc.radius = LV_RADIUS_CIRCLE;
        lv_area_t dot_area = { (lv_coord_t)(cx - 4), (lv_coord_t)(cy - 4), (lv_coord_t)(cx + 4), (lv_coord_t)(cy + 4) };
        lv_draw_rect(draw_ctx, &dot_dsc, &dot_area);
    }
}

static void build_tile0_navigation(lv_obj_t *tile)
{
    style_tile(tile);

    // 1. Header: Route Name (trái) & Nút Zoom (phải)
    t0_lbl_route_name = make_val_label(tile, "ROUTE", CLR_ACCENT, &lv_font_montserrat_20, 8, 12);
    lv_obj_set_width(t0_lbl_route_name, 175);

    t0_btn_zoom = lv_btn_create(tile);
    lv_obj_set_size(t0_btn_zoom, 74, 30);
    lv_obj_set_pos(t0_btn_zoom, SCREEN_W - 82, 8);
    lv_obj_set_style_bg_color(t0_btn_zoom, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_bg_color(t0_btn_zoom, lv_color_hex(0x374151), LV_STATE_PRESSED);
    lv_obj_set_style_border_color(t0_btn_zoom, CLR_BORDER, 0);
    lv_obj_set_style_border_width(t0_btn_zoom, 1, 0);
    lv_obj_set_style_radius(t0_btn_zoom, 6, 0);
    lv_obj_clear_flag(t0_btn_zoom, LV_OBJ_FLAG_SCROLL_CHAIN);
    lv_obj_add_event_cb(t0_btn_zoom, btn_zoom_event_cb, LV_EVENT_CLICKED, NULL);

    t0_lbl_zoom = lv_label_create(t0_btn_zoom);
    lv_label_set_text(t0_lbl_zoom, "250m");
    lv_obj_set_style_text_font(t0_lbl_zoom, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(t0_lbl_zoom, CLR_WHITE, 0);
    lv_obj_clear_flag(t0_lbl_zoom, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(t0_lbl_zoom);

    make_hline(tile, 44);

    // 2. Map Canvas Area (y = 46..366, cao 320px)
    t0_map_area = lv_obj_create(tile);
    lv_obj_set_size(t0_map_area, SCREEN_W, 320);
    lv_obj_set_pos(t0_map_area, 0, 46);
    lv_obj_set_style_bg_color(t0_map_area, CLR_BG, 0);
    lv_obj_set_style_bg_opa(t0_map_area, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(t0_map_area, 0, 0);
    lv_obj_set_style_pad_all(t0_map_area, 0, 0);
    lv_obj_clear_flag(t0_map_area, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(t0_map_area, map_draw_event_cb, LV_EVENT_DRAW_MAIN, NULL);
    lv_obj_add_event_cb(t0_map_area, btn_zoom_event_cb, LV_EVENT_CLICKED, NULL);
    strip_scrollbar(t0_map_area);

    make_hline(tile, 368);

    // 3. Bottom Info Panel (y = 372..450)
    // Dòng 1: Dist Remaining & Cross-Track Error
    t0_lbl_dist_rem = make_val_label(tile, "Rem: -- km", CLR_WHITE, &lv_font_montserrat_20, 8, 372);
    t0_lbl_xte = make_val_label(tile, "ON ROUTE", CLR_GREEN, &lv_font_montserrat_20, 148, 372);

    make_hline(tile, 404);

    // Dòng 2: Heading & Speed / Alt
    t0_lbl_course = make_val_label(tile, "HDG: 000 deg N", CLR_ORANGE, &lv_font_montserrat_14, 8, 410);
    t0_lbl_speed_alt = make_val_label(tile, "0.0 km/h | --m", CLR_DIMTEXT, &lv_font_montserrat_14, 148, 410);

    // Dòng 3: Gợi ý thao tác
    t0_lbl_map_note = make_val_label(tile, "Tap map/btn to zoom (100m-2.5km)", CLR_DIMTEXT, &lv_font_montserrat_12, 8, 434);
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
        if (t0_map_area) {
            lv_obj_invalidate(t0_map_area);
        }

        const RouteNavStatus *nav = route_nav_get_status();
        if (nav && nav->is_active) {
            snprintf(buf, sizeof(buf), "%.14s", nav->route_name);
            lv_label_set_text(t0_lbl_route_name, buf);

            snprintf(buf, sizeof(buf), "Rem: %.1fkm", nav->dist_remaining_km);
            lv_label_set_text(t0_lbl_dist_rem, buf);

            if (nav->is_off_route) {
                snprintf(buf, sizeof(buf), "OFF +%dm", (int)nav->cross_track_err_m);
                lv_label_set_text(t0_lbl_xte, buf);
                lv_obj_set_style_text_color(t0_lbl_xte, CLR_RED, 0);
            } else {
                snprintf(buf, sizeof(buf), "ON (%dm)", (int)nav->cross_track_err_m);
                lv_label_set_text(t0_lbl_xte, buf);
                lv_obj_set_style_text_color(t0_lbl_xte, CLR_GREEN, 0);
            }
        } else {
            lv_label_set_text(t0_lbl_route_name, "FREE NAV");
            lv_label_set_text(t0_lbl_dist_rem, "Rem: -- km");
            lv_label_set_text(t0_lbl_xte, "NO ROUTE");
            lv_obj_set_style_text_color(t0_lbl_xte, CLR_DIMTEXT, 0);
        }

        if (nav && t0_lbl_zoom) {
            if (nav->zoom_radius_m >= 1000.0f) {
                snprintf(buf, sizeof(buf), "%.1fkm", nav->zoom_radius_m / 1000.0f);
            } else {
                snprintf(buf, sizeof(buf), "%dm", (int)nav->zoom_radius_m);
            }
            lv_label_set_text(t0_lbl_zoom, buf);
        }

        const char *dir = course_to_dir(snap->gps.course_deg);
        int32_t courseInt = (int32_t)snap->gps.course_deg;
        snprintf(buf, sizeof(buf), "HDG: %03ld deg %s", (long)courseInt, dir);
        lv_label_set_text(t0_lbl_course, buf);

        int alt_val = snap->baro.valid ? (int)snap->baro.altitude_m : (int)snap->gps.altitude_m;
        snprintf(buf, sizeof(buf), "%.1f km/h | %dm", snap->gps.speed_kmh, alt_val);
        lv_label_set_text(t0_lbl_speed_alt, buf);
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
