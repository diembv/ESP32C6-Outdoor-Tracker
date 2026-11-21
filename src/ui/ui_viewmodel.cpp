/**
 * @file ui_viewmodel.cpp
 * @brief Shared UI ViewModel: formatting, map rendering, and control event callbacks.
 */

#include "ui/ui_viewmodel.h"
#include "ui/ui_theme.h"
#include "core/baro_fusion.h"
#include "core/track_logger.h"
#include "connectivity/wifi_sync.h"
#include "connectivity/ble_service.h"
#include "hal/hal_power.h"
#include "route_nav.h"
#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern SensorSnapshot g_snap;

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

void ui_vm_format(const SensorSnapshot *snap, UiViewModel *vm)
{
    if (!snap || !vm) return;

    vm->gps_fix_valid     = snap->gps.fix_valid;
    vm->is_logging_active = snap->sys.is_logging_active;
    vm->blink_on          = ((millis() / 500) % 2 == 0);
    vm->wifi_active       = wifi_sync_is_active();
    vm->ble_active        = ble_service_is_active();

    // ── Tile 1: Sensors ───────────────────────────────────────────────────────
    char timeStr[16] = "--:--";
    if (snap->gps.time_valid) {
        uint8_t local_h = (snap->gps.hour + 7) % 24;
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d", local_h, snap->gps.minute);
    }
    snprintf(vm->header, sizeof(vm->header), "Sats:%lu | %s",
             (unsigned long)snap->gps.satellites, timeStr);

    snprintf(vm->bat_badge, sizeof(vm->bat_badge), "%d%% %.2fV",
             (int)snap->sys.battery_pct, snap->sys.battery_v);
    if (snap->sys.battery_pct >= 50)      vm->bat_color = CLR_GREEN;
    else if (snap->sys.battery_pct >= 20) vm->bat_color = CLR_YELLOW;
    else                                  vm->bat_color = CLR_RED;

    snprintf(vm->speed, sizeof(vm->speed), "%.1f km/h", snap->gps.speed_kmh);

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
        int32_t gpsAltDec = (int32_t)((snap->gps.altitude_m >= 0
                                       ? snap->gps.altitude_m - gpsAltInt
                                       : -(snap->gps.altitude_m) - (-gpsAltInt)) * 10);

        snprintf(vm->coords, sizeof(vm->coords), "%ld.%06ld %c\n%ld.%06ld %c\nAlt: %ld.%01ld m (GPS)",
                 (long)latInt, (long)latDec, cLat,
                 (long)lonInt, (long)lonDec, cLon,
                 (long)gpsAltInt, (long)abs(gpsAltDec));
    } else {
        snprintf(vm->coords, sizeof(vm->coords), "--.------ N\n--.------ E\nAlt: -- m (GPS)");
    }

    char baroPart[64] = "-- C\n-- hPa  |  Baro: -- m";
    if (snap->baro.valid) {
        int32_t tInt = (int32_t)snap->baro.temperature_c;
        int32_t tDec = (int32_t)((snap->baro.temperature_c >= 0
                                  ? snap->baro.temperature_c - tInt
                                  : -snap->baro.temperature_c + tInt) * 10);
        int32_t pInt = (int32_t)snap->baro.pressure_hpa;
        int32_t pDec = (int32_t)((snap->baro.pressure_hpa - pInt) * 10);
        int32_t altInt = (int32_t)snap->baro.altitude_m;
        int32_t altDec = (int32_t)((snap->baro.altitude_m >= 0
                                    ? snap->baro.altitude_m - altInt
                                    : -(snap->baro.altitude_m) - (-altInt)) * 10);

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
    snprintf(vm->env, sizeof(vm->env), "%s\n%s", baroPart, imuPart);

    // ── Tile 0: Navigation ────────────────────────────────────────────────────
    const RouteNavStatus *nav = route_nav_get_status();
    if (nav && nav->is_active) {
        snprintf(vm->route_name, sizeof(vm->route_name), LV_SYMBOL_LIST " %.12s", nav->route_name);
        snprintf(vm->dist_rem, sizeof(vm->dist_rem), "Rem: %.1fkm", nav->dist_remaining_km);
        if (nav->is_off_route) {
            snprintf(vm->xte, sizeof(vm->xte), "OFF +%dm", (int)nav->cross_track_err_m);
            vm->xte_color = CLR_RED;
        } else {
            snprintf(vm->xte, sizeof(vm->xte), "ON (%dm)", (int)nav->cross_track_err_m);
            vm->xte_color = CLR_GREEN;
        }
    } else {
        snprintf(vm->route_name, sizeof(vm->route_name), LV_SYMBOL_LIST " ROUTE");
        snprintf(vm->dist_rem, sizeof(vm->dist_rem), "Rem: -- km");
        snprintf(vm->xte, sizeof(vm->xte), "NO ROUTE");
        vm->xte_color = CLR_DIMTEXT;
    }

    if (nav) {
        if (nav->zoom_radius_m >= 1000.0f) {
            snprintf(vm->zoom, sizeof(vm->zoom), "%.1fkm", nav->zoom_radius_m / 1000.0f);
        } else {
            snprintf(vm->zoom, sizeof(vm->zoom), "%dm", (int)nav->zoom_radius_m);
        }
    } else {
        snprintf(vm->zoom, sizeof(vm->zoom), "250m");
    }

    const char *dir = course_to_dir(snap->gps.course_deg);
    snprintf(vm->course, sizeof(vm->course), "HDG: %03ld deg %s",
             (long)snap->gps.course_deg, dir);

    int alt_val = snap->baro.valid ? (int)snap->baro.altitude_m : (int)snap->gps.altitude_m;
    snprintf(vm->speed_alt, sizeof(vm->speed_alt), "%.1f km/h | %dm",
             snap->gps.speed_kmh, alt_val);

    // ── Tile 2: System ────────────────────────────────────────────────────────
    uint32_t chars_k = snap->gps.chars_proc / 1000;
    uint32_t chars_r = (snap->gps.chars_proc % 1000) / 100;
    snprintf(vm->gps_stats, sizeof(vm->gps_stats), "chars=%lu.%luk  fixes=%lu  err=%lu",
             (unsigned long)chars_k, (unsigned long)chars_r,
             (unsigned long)snap->gps.fixes, (unsigned long)snap->gps.checksum_err);

    if (snap->gps.date_valid && snap->gps.time_valid) {
        uint8_t local_h = (snap->gps.hour + 7) % 24;
        snprintf(vm->datetime, sizeof(vm->datetime), "%02u/%02u/%04u  %02u:%02u:%02u",
                 snap->gps.day, snap->gps.month, snap->gps.year,
                 local_h, snap->gps.minute, snap->gps.second);
    } else {
        snprintf(vm->datetime, sizeof(vm->datetime), "--/--/----  --:--:--");
    }

    uint32_t h = snap->sys.uptime_s / 3600;
    uint32_t m = (snap->sys.uptime_s % 3600) / 60;
    uint32_t s = snap->sys.uptime_s % 60;
    snprintf(vm->uptime, sizeof(vm->uptime), "%02lu:%02lu:%02lu  #%lu %s",
             (unsigned long)h, (unsigned long)m, (unsigned long)s,
             (unsigned long)snap->sys.boot_count,
             snap->sys.reset_reason ? snap->sys.reset_reason : "");

    int32_t vInt = (int32_t)snap->sys.battery_v;
    int32_t vDec = (int32_t)((snap->sys.battery_v - vInt) * 100);
    snprintf(vm->battery, sizeof(vm->battery), "%ld.%02ld V  |  %d %%",
             (long)vInt, (long)abs(vDec), (int)snap->sys.battery_pct);

    if (snap->sys.sd_present && snap->sys.sd_ok) {
        if (snap->sys.is_logging_active) {
            uint32_t pts = 0;
            size_t buf_usage = 0;
            track_logger_get_stats(&pts, &buf_usage);
            snprintf(vm->sd_status, sizeof(vm->sd_status), "REC: %lu pts | RAM: %u/1024B",
                     (unsigned long)pts, (unsigned int)buf_usage);
            vm->sd_color = vm->blink_on ? CLR_YELLOW : CLR_GREEN;
        } else {
            snprintf(vm->sd_status, sizeof(vm->sd_status), "OK (%lu MB free)",
                     (unsigned long)snap->sys.sd_free_mb);
            vm->sd_color = CLR_GREEN;
        }
    } else {
        if (snap->sys.sd_free_mb > 0) {
            snprintf(vm->sd_status, sizeof(vm->sd_status), "Error 0x%X",
                     (unsigned int)snap->sys.sd_free_mb);
        } else {
            snprintf(vm->sd_status, sizeof(vm->sd_status), "No SD card");
        }
        vm->sd_color = CLR_RED;
    }
}

// ═══════════════════════════════════════════════════════════════════════════════
//  Shared Event Callbacks
// ═══════════════════════════════════════════════════════════════════════════════

static void mbox_close_cb(lv_event_t *e)
{
    lv_msgbox_close(lv_event_get_current_target(e));
}

void ui_vm_btn_zoom_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    UiWidgets *w = (UiWidgets *)lv_event_get_user_data(e);

    route_nav_cycle_zoom();
    const RouteNavStatus *nav = route_nav_get_status();
    if (nav && w && w->t0_lbl_zoom) {
        char zbuf[16];
        if (nav->zoom_radius_m >= 1000.0f) {
            snprintf(zbuf, sizeof(zbuf), "%.1fkm", nav->zoom_radius_m / 1000.0f);
        } else {
            snprintf(zbuf, sizeof(zbuf), "%dm", (int)nav->zoom_radius_m);
        }
        lv_label_set_text(w->t0_lbl_zoom, zbuf);
    }
    if (w && w->t0_map_area) {
        lv_obj_invalidate(w->t0_map_area);
    }
}

void ui_vm_map_draw_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_DRAW_MAIN) return;

    lv_obj_t *obj = lv_event_get_target(e);
    lv_draw_ctx_t *draw_ctx = lv_event_get_draw_ctx(e);
    if (!draw_ctx) return;

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    int16_t w_px = (int16_t)(coords.x2 - coords.x1 + 1);
    int16_t h_px = (int16_t)(coords.y2 - coords.y1 + 1);
    int16_t cx   = (int16_t)((coords.x1 + coords.x2) / 2);
    int16_t cy   = (int16_t)((coords.y1 + coords.y2) / 2);

    int16_t min_dim = (w_px < h_px) ? w_px : h_px;
    float r_px = (float)min_dim * 0.42f;
    if (r_px < 40.0f) r_px = 40.0f;
    uint16_t r_outer = (uint16_t)r_px;
    uint16_t r_inner = (uint16_t)(r_px * 0.5f);

    // 1. Range Rings
    lv_point_t center_pt = { (lv_coord_t)cx, (lv_coord_t)cy };
    lv_draw_arc_dsc_t arc_dsc;
    lv_draw_arc_dsc_init(&arc_dsc);
    arc_dsc.color = lv_color_hex(0x21262D);
    arc_dsc.width = 1;
    lv_draw_arc(draw_ctx, &arc_dsc, &center_pt, r_outer, 0, 360);

    arc_dsc.color = lv_color_hex(0x161C24);
    lv_draw_arc(draw_ctx, &arc_dsc, &center_pt, r_inner, 0, 360);

    // North 'N' indicator
    const UiThemeFonts *f = ui_theme_fonts();
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    label_dsc.color = CLR_DIMTEXT;
    label_dsc.font  = f->xs;
    lv_area_t n_area = {
        (lv_coord_t)(cx - 8),
        (lv_coord_t)(cy - r_outer - 14),
        (lv_coord_t)(cx + 8),
        (lv_coord_t)(cy - r_outer)
    };
    lv_draw_label(draw_ctx, &label_dsc, &n_area, "N", NULL);

    // 2. Route Polyline
    const RouteNavStatus *nav = route_nav_get_status();
    if (nav && nav->is_active && nav->window_points && nav->window_count > 1) {
        lv_draw_line_dsc_t line_dsc;
        lv_draw_line_dsc_init(&line_dsc);
        line_dsc.color = CLR_ACCENT;
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
                cx, cy, radius, r_px,
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

        // Finish marker (Red)
        if (nav->window_start_idx + nav->window_count >= nav->total_points) {
            int16_t fx, fy;
            uint32_t last_i = nav->window_count - 1;
            if (route_nav_project_to_screen(nav->window_points[last_i].lat, nav->window_points[last_i].lon,
                                            ref_lat, ref_lon, cx, cy, radius, r_px, &fx, &fy)) {
                lv_draw_rect_dsc_t fin_dsc;
                lv_draw_rect_dsc_init(&fin_dsc);
                fin_dsc.bg_color = CLR_RED;
                fin_dsc.radius = LV_RADIUS_CIRCLE;
                lv_area_t fin_area = { (lv_coord_t)(fx - 4), (lv_coord_t)(fy - 4),
                                       (lv_coord_t)(fx + 4), (lv_coord_t)(fy + 4) };
                lv_draw_rect(draw_ctx, &fin_dsc, &fin_area);
            }
        }

        // Start marker (Green)
        if (nav->window_start_idx == 0) {
            int16_t sx, sy;
            if (route_nav_project_to_screen(nav->window_points[0].lat, nav->window_points[0].lon,
                                            ref_lat, ref_lon, cx, cy, radius, r_px, &sx, &sy)) {
                lv_draw_rect_dsc_t st_dsc;
                lv_draw_rect_dsc_init(&st_dsc);
                st_dsc.bg_color = CLR_GREEN;
                st_dsc.radius = LV_RADIUS_CIRCLE;
                lv_area_t st_area = { (lv_coord_t)(sx - 4), (lv_coord_t)(sy - 4),
                                      (lv_coord_t)(sx + 4), (lv_coord_t)(sy + 4) };
                lv_draw_rect(draw_ctx, &st_dsc, &st_area);
            }
        }
    } else {
        lv_draw_label_dsc_t no_route_dsc;
        lv_draw_label_dsc_init(&no_route_dsc);
        no_route_dsc.color = CLR_DIMTEXT;
        no_route_dsc.font  = f->xs;
        no_route_dsc.align = LV_TEXT_ALIGN_CENTER;
        lv_area_t msg_area = { coords.x1, (lv_coord_t)(cy - 12), coords.x2, (lv_coord_t)(cy + 12) };
        lv_draw_label(draw_ctx, &no_route_dsc, &msg_area, "NO ROUTE (Put .bin in /sdcard/routes/)", NULL);
    }

    // 3. User Marker at center (cx, cy)
    if (g_snap.gps.fix_valid) {
        if (g_snap.gps.speed_kmh >= 0.5f) {
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
            lv_draw_rect_dsc_t dot_dsc;
            lv_draw_rect_dsc_init(&dot_dsc);
            dot_dsc.bg_color = CLR_GREEN;
            dot_dsc.radius = LV_RADIUS_CIRCLE;
            dot_dsc.border_color = CLR_YELLOW;
            dot_dsc.border_width = 2;
            lv_area_t dot_area = { (lv_coord_t)(cx - 5), (lv_coord_t)(cy - 5),
                                   (lv_coord_t)(cx + 5), (lv_coord_t)(cy + 5) };
            lv_draw_rect(draw_ctx, &dot_dsc, &dot_area);
        }
    } else {
        lv_draw_rect_dsc_t dot_dsc;
        lv_draw_rect_dsc_init(&dot_dsc);
        dot_dsc.bg_color = CLR_RED;
        dot_dsc.radius = LV_RADIUS_CIRCLE;
        lv_area_t dot_area = { (lv_coord_t)(cx - 4), (lv_coord_t)(cy - 4),
                               (lv_coord_t)(cx + 4), (lv_coord_t)(cy + 4) };
        lv_draw_rect(draw_ctx, &dot_dsc, &dot_area);
    }
}

void ui_vm_dd_mode_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
    lv_obj_t *dropdown = lv_event_get_target(e);
    uint16_t selected = lv_dropdown_get_selected(dropdown);
    ActivityMode mode = (selected == 1) ? ACTIVITY_MODE_BIKE : ACTIVITY_MODE_HIKE;
    g_snap.sys.activity_mode = mode;
    track_logger_set_activity_mode(mode);
    Serial.printf("[UI] Che do the thao doi sang: %s\n",
                  (mode == ACTIVITY_MODE_BIKE) ? "Cycling" : "Hiking");
}

void ui_vm_btn_mode_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    UiWidgets *w = (UiWidgets *)lv_event_get_user_data(e);

    ActivityMode mode = (g_snap.sys.activity_mode == ACTIVITY_MODE_BIKE)
                            ? ACTIVITY_MODE_HIKE
                            : ACTIVITY_MODE_BIKE;
    g_snap.sys.activity_mode = mode;
    track_logger_set_activity_mode(mode);

    if (w && w->t2_lbl_mode) {
        lv_label_set_text(w->t2_lbl_mode,
                          (mode == ACTIVITY_MODE_BIKE) ? "Cycling" : "Hiking");
    }
    if (w && w->t2_dd_mode) {
        lv_obj_set_style_border_color(w->t2_dd_mode,
                                      (mode == ACTIVITY_MODE_BIKE) ? CLR_ORANGE : CLR_GREEN, 0);
    }
    Serial.printf("[UI] Che do log doi sang: %s\n",
                  (mode == ACTIVITY_MODE_BIKE) ? "Cycling" : "Hiking");
}

static lv_obj_t *make_styled_mbox(const char *title, const char *text, const char *btns[], lv_event_cb_t cb)
{
    const UiThemeFonts *f = ui_theme_fonts();
    lv_obj_t *mbox = lv_msgbox_create(NULL, title, text, btns, false);
    lv_coord_t w = (SCREEN_W - 24 > 320) ? 320 : (SCREEN_W - 24);
    lv_obj_set_width(mbox, w);
    lv_obj_set_style_bg_color(mbox, CLR_SURFACE, 0);
    lv_obj_set_style_bg_opa(mbox, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(mbox, CLR_ACCENT, 0);
    lv_obj_set_style_border_width(mbox, 2, 0);
    lv_obj_set_style_radius(mbox, 10, 0);
    lv_obj_set_style_pad_all(mbox, 12, 0);
    lv_obj_set_style_text_color(mbox, CLR_WHITE, 0);
    lv_obj_set_style_text_font(mbox, f->sm, 0);

    lv_obj_t *t = lv_msgbox_get_title(mbox);
    if (t) {
        lv_obj_set_style_text_color(t, CLR_ACCENT, 0);
        lv_obj_set_style_text_font(t, f->sm, 0);
    }
    lv_obj_t *txt = lv_msgbox_get_text(mbox);
    if (txt) {
        lv_obj_set_style_text_color(txt, CLR_WHITE, 0);
        lv_obj_set_style_text_font(txt, f->xs, 0);
    }
    lv_obj_t *bm = lv_msgbox_get_btns(mbox);
    if (bm) {
        lv_obj_set_width(bm, w - 28);
        lv_obj_set_height(bm, 36);
        lv_obj_set_style_text_font(bm, f->sm, 0);
    }

    lv_obj_add_event_cb(mbox, cb ? cb : mbox_close_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_center(mbox);
    return mbox;
}

void ui_vm_btn_calib_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    static const char *btns[] = {"OK", ""};
    if (!g_snap.gps.fix_valid || !g_snap.gps.altitude_valid || g_snap.gps.satellites < 5) {
        make_styled_mbox("CALIB ALTITUDE",
                         "GPS fix not ready.\nNeed 3D fix (>= 5 sats).",
                         btns, mbox_close_cb);
    } else {
        baro_request_recalibration();
        char msg[96];
        snprintf(msg, sizeof(msg), "Calibrated to %.1fm\nSea-level P0 updated.",
                 g_snap.gps.altitude_m);
        make_styled_mbox("CALIB SUCCESS", msg, btns, mbox_close_cb);
    }
}

void ui_vm_btn_log_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (!g_snap.sys.sd_present || !g_snap.sys.sd_ok) {
        static const char *btns[] = {"OK", ""};
        make_styled_mbox("SD CARD ERROR",
                         "No MicroSD card inserted\nor card not mounted.",
                         btns, mbox_close_cb);
        return;
    }

    // Defer SD card open/flush/close to main loop (never block LVGL task / SPI2)
    hal_power_request_log_toggle();
}

void ui_vm_btn_wifi_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    Serial.println(F("[UI] WIFI button clicked"));
    // Defer Wi-Fi AP start/stop to main loop (prevents LVGL freeze / popup glitch)
    hal_power_request_wifi_toggle();
}

void ui_vm_btn_ble_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    hal_power_request_ble_toggle();
}

static void power_confirm_cb(lv_event_t *e)
{
    lv_obj_t *mbox = lv_event_get_current_target(e);
    const char *txt = lv_msgbox_get_active_btn_text(mbox);
    if (txt && strcmp(txt, "OFF") == 0) {
        lv_msgbox_close(mbox);
        hal_power_request_power_off();
    } else {
        lv_msgbox_close(mbox);
    }
}

void ui_vm_btn_power_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    static const char *btns[] = {"OFF", "CANCEL", ""};
    make_styled_mbox("POWER OFF",
                     "Save active log and\nshut down device?",
                     btns, power_confirm_cb);
}

void ui_vm_show_wifi_popup(const char *ssid)
{
    static const char *btns[] = {"OK", ""};
    char msg[128];
    snprintf(msg, sizeof(msg), "SSID: %s\nhttp://192.168.4.1\nAuto-off in 5 min",
             ssid ? ssid : "OutdoorTracker");
    make_styled_mbox("WIFI SYNC ON", msg, btns, mbox_close_cb);
}

void ui_vm_show_wifi_error_popup(uint32_t free_heap)
{
    static const char *btns[] = {"OK", ""};
    char msg[128];
    snprintf(msg, sizeof(msg), "Cannot start Wi-Fi AP.\nFree RAM: %lu KB\nTry rebooting.",
             (unsigned long)(free_heap / 1024));
    make_styled_mbox("WIFI ERROR", msg, btns, mbox_close_cb);
}

void ui_vm_btn_route_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    hal_power_request_route_picker();
}

static lv_obj_t *s_route_modal = NULL;
static lv_obj_t *s_list_routes = NULL;
static lv_obj_t *s_list_logs   = NULL;
static lv_obj_t *s_tab_routes  = NULL;
static lv_obj_t *s_tab_logs    = NULL;
static char      s_picker_paths[24][128];

static void route_modal_close_cb(lv_event_t *e)
{
    (void)e;
    if (s_route_modal) {
        lv_obj_del(s_route_modal);
        s_route_modal = NULL;
        s_list_routes = NULL;
        s_list_logs   = NULL;
        s_tab_routes  = NULL;
        s_tab_logs    = NULL;
    }
}

static void route_item_click_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    const char *path = (const char *)lv_event_get_user_data(e);
    hal_power_request_route_select(path ? path : "");
    route_modal_close_cb(e);
}

static void route_tab_switch_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    bool show_logs = (lv_event_get_target(e) == s_tab_logs);
    if (s_list_routes && s_list_logs) {
        if (show_logs) {
            lv_obj_add_flag(s_list_routes, LV_OBJ_FLAG_HIDDEN);
            lv_obj_clear_flag(s_list_logs, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_clear_flag(s_list_routes, LV_OBJ_FLAG_HIDDEN);
            lv_obj_add_flag(s_list_logs, LV_OBJ_FLAG_HIDDEN);
        }
    }
    if (s_tab_routes && s_tab_logs) {
        lv_obj_set_style_bg_color(s_tab_routes, show_logs ? lv_color_hex(0x1F2937) : lv_color_hex(0x0369A1), 0);
        lv_obj_set_style_border_color(s_tab_routes, show_logs ? CLR_BORDER : CLR_ACCENT, 0);
        lv_obj_set_style_bg_color(s_tab_logs, show_logs ? lv_color_hex(0x0369A1) : lv_color_hex(0x1F2937), 0);
        lv_obj_set_style_border_color(s_tab_logs, show_logs ? CLR_ACCENT : CLR_BORDER, 0);
    }
}

static void format_entry_meta(uint32_t size_bytes, uint32_t mtime, char *out, size_t out_len)
{
    float kb = (float)size_bytes / 1024.0f;
    if (mtime >= 1577836800UL) {
        time_t local_ts = (time_t)(mtime + 7UL * 3600UL);
        struct tm tm_info;
        gmtime_r(&local_ts, &tm_info);
        snprintf(out, out_len, "%.1f KB  |  %02d/%02d/%04d %02d:%02d",
                 kb, tm_info.tm_mday, tm_info.tm_mon + 1, tm_info.tm_year + 1900,
                 tm_info.tm_hour, tm_info.tm_min);
    } else {
        snprintf(out, out_len, "%.1f KB  |  No GPS time", kb);
    }
}

static void populate_picker_list(lv_obj_t *list, const RouteListEntry *entries, int count,
                               int path_offset, const char *empty_msg, const UiThemeFonts *f)
{
    const RouteNavStatus *nav = route_nav_get_status();
    for (int i = 0; i < count; i++) {
        int slot = path_offset + i;
        strncpy(s_picker_paths[slot], entries[i].path, sizeof(s_picker_paths[slot]) - 1);
        s_picker_paths[slot][sizeof(s_picker_paths[slot]) - 1] = '\0';

        bool is_cur = (nav && nav->is_active && strcmp(nav->route_name, entries[i].name) == 0);

        lv_obj_t *btn = lv_btn_create(list);
        lv_obj_set_size(btn, lv_pct(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_color(btn, is_cur ? lv_color_hex(0x164E63) : lv_color_hex(0x1F2937), 0);
        lv_obj_set_style_border_color(btn, is_cur ? CLR_ACCENT : CLR_BORDER, 0);
        lv_obj_set_style_border_width(btn, 1, 0);
        lv_obj_set_style_radius(btn, 8, 0);
        lv_obj_set_style_pad_hor(btn, 8, 0);
        lv_obj_set_style_pad_ver(btn, 6, 0);
        lv_obj_set_style_pad_row(btn, 2, 0);
        lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
        lv_obj_add_event_cb(btn, route_item_click_cb, LV_EVENT_CLICKED, s_picker_paths[slot]);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_label_set_text(lbl, entries[i].name);
        lv_label_set_long_mode(lbl, LV_LABEL_LONG_DOT);
        lv_obj_set_width(lbl, lv_pct(100));
        lv_obj_set_style_text_font(lbl, f->sm, 0);
        lv_obj_set_style_text_color(lbl, is_cur ? CLR_ACCENT : CLR_WHITE, 0);
        lv_obj_clear_flag(lbl, LV_OBJ_FLAG_CLICKABLE);

        char meta[48];
        format_entry_meta(entries[i].size_bytes, entries[i].mtime, meta, sizeof(meta));
        lv_obj_t *sub = lv_label_create(btn);
        lv_label_set_text(sub, meta);
        lv_obj_set_width(sub, lv_pct(100));
        lv_obj_set_style_text_font(sub, f->xs, 0);
        lv_obj_set_style_text_color(sub, CLR_DIMTEXT, 0);
        lv_obj_clear_flag(sub, LV_OBJ_FLAG_CLICKABLE);
    }

    if (count == 0) {
        lv_obj_t *hint = lv_label_create(list);
        lv_label_set_text(hint, empty_msg);
        lv_obj_set_width(hint, lv_pct(100));
        lv_obj_set_style_text_font(hint, f->xs, 0);
        lv_obj_set_style_text_color(hint, CLR_DIMTEXT, 0);
        lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_CENTER, 0);
    }
}

void ui_vm_show_route_picker(const void *route_entries_ptr, int route_count,
                             const void *log_entries_ptr, int log_count)
{
    route_modal_close_cb(NULL);

    const RouteListEntry *route_entries = (const RouteListEntry *)route_entries_ptr;
    const RouteListEntry *log_entries   = (const RouteListEntry *)log_entries_ptr;
    const UiThemeFonts *f = ui_theme_fonts();

    int n_routes = (route_count > 12) ? 12 : (route_count < 0 ? 0 : route_count);
    int n_logs   = (log_count > 12) ? 12 : (log_count < 0 ? 0 : log_count);

    s_route_modal = lv_obj_create(lv_layer_top());
    lv_obj_set_size(s_route_modal, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_route_modal, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_route_modal, LV_OPA_70, 0);
    lv_obj_set_style_border_width(s_route_modal, 0, 0);
    lv_obj_set_style_pad_all(s_route_modal, 0, 0);
    lv_obj_clear_flag(s_route_modal, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *panel = lv_obj_create(s_route_modal);
    lv_obj_set_size(panel, lv_pct(92), lv_pct(84));
    lv_obj_center(panel);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0x161B22), 0);
    lv_obj_set_style_border_color(panel, CLR_ACCENT, 0);
    lv_obj_set_style_border_width(panel, 2, 0);
    lv_obj_set_style_radius(panel, 12, 0);
    lv_obj_set_style_pad_all(panel, 10, 0);
    lv_obj_set_style_pad_row(panel, 6, 0);
    lv_obj_set_flex_flow(panel, LV_FLEX_FLOW_COLUMN);

    // Header row
    lv_obj_t *hdr = lv_obj_create(panel);
    lv_obj_set_size(hdr, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(hdr, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(hdr, 0, 0);
    lv_obj_set_style_pad_all(hdr, 0, 0);
    lv_obj_set_flex_flow(hdr, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(hdr, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(hdr, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(hdr);
    lv_label_set_text(title, "SELECT NAVIGATION");
    lv_obj_set_style_text_font(title, f->sm, 0);
    lv_obj_set_style_text_color(title, CLR_ACCENT, 0);

    lv_obj_t *btn_x = lv_btn_create(hdr);
    lv_obj_set_size(btn_x, 34, 28);
    lv_obj_set_style_bg_color(btn_x, lv_color_hex(0x374151), 0);
    lv_obj_set_style_radius(btn_x, 6, 0);
    lv_obj_add_event_cb(btn_x, route_modal_close_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_x = lv_label_create(btn_x);
    lv_label_set_text(lbl_x, LV_SYMBOL_CLOSE);
    lv_obj_set_style_text_font(lbl_x, f->xs, 0);
    lv_obj_center(lbl_x);

    // OFF / No Route button
    lv_obj_t *btn_off = lv_btn_create(panel);
    lv_obj_set_size(btn_off, lv_pct(100), 36);
    lv_obj_set_style_bg_color(btn_off, lv_color_hex(0x2D1B1B), 0);
    lv_obj_set_style_border_color(btn_off, CLR_RED, 0);
    lv_obj_set_style_border_width(btn_off, 1, 0);
    lv_obj_set_style_radius(btn_off, 8, 0);
    lv_obj_add_event_cb(btn_off, route_item_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_off = lv_label_create(btn_off);
    lv_label_set_text(lbl_off, "OFF (No Route)");
    lv_obj_set_style_text_font(lbl_off, f->xs, 0);
    lv_obj_set_style_text_color(lbl_off, CLR_WHITE, 0);
    lv_obj_clear_flag(lbl_off, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(lbl_off);

    // 2-Tab Switcher: [ROUTES (n)] | [LOGS (m)]
    lv_obj_t *tab_row = lv_obj_create(panel);
    lv_obj_set_size(tab_row, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(tab_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(tab_row, 0, 0);
    lv_obj_set_style_pad_all(tab_row, 0, 0);
    lv_obj_set_style_pad_column(tab_row, 6, 0);
    lv_obj_set_flex_flow(tab_row, LV_FLEX_FLOW_ROW);
    lv_obj_clear_flag(tab_row, LV_OBJ_FLAG_SCROLLABLE);

    char tab_buf[32];
    s_tab_routes = lv_btn_create(tab_row);
    lv_obj_set_height(s_tab_routes, 34);
    lv_obj_set_flex_grow(s_tab_routes, 1);
    lv_obj_set_style_bg_color(s_tab_routes, lv_color_hex(0x0369A1), 0);
    lv_obj_set_style_border_color(s_tab_routes, CLR_ACCENT, 0);
    lv_obj_set_style_border_width(s_tab_routes, 1, 0);
    lv_obj_set_style_radius(s_tab_routes, 6, 0);
    lv_obj_add_event_cb(s_tab_routes, route_tab_switch_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_tr = lv_label_create(s_tab_routes);
    snprintf(tab_buf, sizeof(tab_buf), "ROUTES (%d)", n_routes);
    lv_label_set_text(lbl_tr, tab_buf);
    lv_obj_set_style_text_font(lbl_tr, f->xs, 0);
    lv_obj_clear_flag(lbl_tr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(lbl_tr);

    s_tab_logs = lv_btn_create(tab_row);
    lv_obj_set_height(s_tab_logs, 34);
    lv_obj_set_flex_grow(s_tab_logs, 1);
    lv_obj_set_style_bg_color(s_tab_logs, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_border_color(s_tab_logs, CLR_BORDER, 0);
    lv_obj_set_style_border_width(s_tab_logs, 1, 0);
    lv_obj_set_style_radius(s_tab_logs, 6, 0);
    lv_obj_add_event_cb(s_tab_logs, route_tab_switch_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl_tl = lv_label_create(s_tab_logs);
    snprintf(tab_buf, sizeof(tab_buf), "LOGS (%d)", n_logs);
    lv_label_set_text(lbl_tl, tab_buf);
    lv_obj_set_style_text_font(lbl_tl, f->xs, 0);
    lv_obj_clear_flag(lbl_tl, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(lbl_tl);

    // List 1: Routes (.bin)
    s_list_routes = lv_obj_create(panel);
    lv_obj_set_width(s_list_routes, lv_pct(100));
    lv_obj_set_flex_grow(s_list_routes, 1);
    lv_obj_set_style_bg_opa(s_list_routes, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list_routes, 0, 0);
    lv_obj_set_style_pad_all(s_list_routes, 2, 0);
    lv_obj_set_style_pad_row(s_list_routes, 6, 0);
    lv_obj_set_flex_flow(s_list_routes, LV_FLEX_FLOW_COLUMN);
    populate_picker_list(s_list_routes, route_entries, n_routes, 0,
                         "No .bin routes in /sdcard/routes/\nUpload via Wi-Fi Sync.", f);

    // List 2: Recorded Logs (.csv)
    s_list_logs = lv_obj_create(panel);
    lv_obj_set_width(s_list_logs, lv_pct(100));
    lv_obj_set_flex_grow(s_list_logs, 1);
    lv_obj_set_style_bg_opa(s_list_logs, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_list_logs, 0, 0);
    lv_obj_set_style_pad_all(s_list_logs, 2, 0);
    lv_obj_set_style_pad_row(s_list_logs, 6, 0);
    lv_obj_set_flex_flow(s_list_logs, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(s_list_logs, LV_OBJ_FLAG_HIDDEN);
    populate_picker_list(s_list_logs, log_entries, n_logs, 12,
                         "No .csv logs in /sdcard/logs/\nRecord an activity first.", f);
}

void ui_vm_show_log_popup(bool started, bool has_gps_fix, float dist_m, uint32_t dur_s)
{
    static const char *btns[] = {"OK", ""};
    if (started) {
        if (!has_gps_fix) {
            make_styled_mbox("WAITING FOR GPS",
                             "Log armed in standby.\nWill auto-record once\nGPS fix is acquired.",
                             btns, mbox_close_cb);
        }
    } else {
        uint32_t m = dur_s / 60;
        uint32_t s = dur_s % 60;
        float dist_km = dist_m / 1000.0f;
        float avg_speed = dur_s > 0 ? (dist_km / (dur_s / 3600.0f)) : 0.0f;

        char summary_text[180];
        snprintf(summary_text, sizeof(summary_text),
                 "Saved to MicroSD.\nMode: %s\nTime: %02lu:%02lu\nDist: %.2f km\nAvg: %.1f km/h",
                 (g_snap.sys.activity_mode == ACTIVITY_MODE_BIKE) ? "Cycling" : "Hiking",
                 (unsigned long)m, (unsigned long)s, dist_km, avg_speed);
        make_styled_mbox("ACTIVITY SUMMARY", summary_text, btns, mbox_close_cb);
    }
}
