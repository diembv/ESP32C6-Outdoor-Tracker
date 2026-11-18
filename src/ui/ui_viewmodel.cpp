/**
 * @file ui_viewmodel.cpp
 * @brief Shared UI ViewModel: formatting, map rendering, and control event callbacks.
 */

#include "ui/ui_viewmodel.h"
#include "ui/ui_theme.h"
#include "core/baro_fusion.h"
#include "core/track_logger.h"
#include "route_nav.h"
#include <Arduino.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

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

    // ── Tile 1: Sensors ───────────────────────────────────────────────────────
    char timeStr[16] = "--:-- --";
    if (snap->gps.time_valid) {
        uint8_t local_h = (snap->gps.hour + 7) % 24;
        uint8_t h12 = local_h % 12;
        if (h12 == 0) h12 = 12;
        const char *ampm = (local_h >= 12) ? "PM" : "AM";
        snprintf(timeStr, sizeof(timeStr), "%02d:%02d %s", h12, snap->gps.minute, ampm);
    }
    snprintf(vm->header, sizeof(vm->header), "Sats: %lu | %s | %d%%",
             (unsigned long)snap->gps.satellites, timeStr, (int)snap->sys.battery_pct);

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
        snprintf(vm->route_name, sizeof(vm->route_name), "%.14s", nav->route_name);
        snprintf(vm->dist_rem, sizeof(vm->dist_rem), "Rem: %.1fkm", nav->dist_remaining_km);
        if (nav->is_off_route) {
            snprintf(vm->xte, sizeof(vm->xte), "OFF +%dm", (int)nav->cross_track_err_m);
            vm->xte_color = CLR_RED;
        } else {
            snprintf(vm->xte, sizeof(vm->xte), "ON (%dm)", (int)nav->cross_track_err_m);
            vm->xte_color = CLR_GREEN;
        }
    } else {
        snprintf(vm->route_name, sizeof(vm->route_name), "FREE NAV");
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

void ui_vm_btn_calib_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;
    if (!g_snap.gps.fix_valid || !g_snap.gps.altitude_valid || g_snap.gps.satellites < 5) {
        static const char *btns[] = {"OK", ""};
        lv_obj_t *mbox = lv_msgbox_create(NULL, "CALIB ALTITUDE",
                                          "GPS fix not ready.\nNeed 3D fix (>= 5 sats)\nto calibrate altitude.",
                                          btns, false);
        lv_obj_add_event_cb(mbox, mbox_close_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_center(mbox);
    } else {
        baro_request_recalibration();
        static const char *btns[] = {"OK", ""};
        char msg[128];
        snprintf(msg, sizeof(msg), "Calibrated to GPS Alt: %.1fm\nSea-level P0 updated.",
                 g_snap.gps.altitude_m);
        lv_obj_t *mbox = lv_msgbox_create(NULL, "CALIB SUCCESS", msg, btns, false);
        lv_obj_add_event_cb(mbox, mbox_close_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_center(mbox);
    }
}

void ui_vm_btn_log_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) return;

    if (!g_snap.sys.sd_present || !g_snap.sys.sd_ok) {
        static const char *btns[] = {"OK", ""};
        lv_obj_t *mbox = lv_msgbox_create(NULL, "SD CARD ERROR",
                                          "No MicroSD card inserted\nor card not mounted.",
                                          btns, false);
        lv_obj_add_event_cb(mbox, mbox_close_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_center(mbox);
        return;
    }

    if (g_snap.sys.is_logging_active) {
        track_logger_toggle(&g_snap, millis());

        float dist_m = 0;
        uint32_t dur_s = 0;
        track_logger_get_summary(&dist_m, &dur_s, millis());

        uint32_t m = dur_s / 60;
        uint32_t s = dur_s % 60;
        float dist_km = dist_m / 1000.0f;
        float avg_speed = dur_s > 0 ? (dist_km / (dur_s / 3600.0f)) : 0;

        static const char *btns[] = {"OK", ""};
        char summary_text[200];
        snprintf(summary_text, sizeof(summary_text),
                 "Session saved to MicroSD.\n\n"
                 "Mode: %s\n"
                 "Elapsed Time: %02lu:%02lu\n"
                 "Distance: %.2f km\n"
                 "Avg Speed: %.1f km/h",
                 (g_snap.sys.activity_mode == ACTIVITY_MODE_BIKE) ? "Cycling" : "Hiking",
                 (unsigned long)m, (unsigned long)s, dist_km, avg_speed);

        lv_obj_t *mbox = lv_msgbox_create(NULL, "ACTIVITY SUMMARY", summary_text, btns, false);
        lv_obj_add_event_cb(mbox, mbox_close_cb, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_center(mbox);
    } else {
        track_logger_toggle(&g_snap, millis());

        if (!g_snap.gps.fix_valid) {
            static const char *btns[] = {"OK", ""};
            lv_obj_t *mbox = lv_msgbox_create(NULL, "WAITING FOR GPS",
                                              "Log armed in standby.\nSystem will auto-record\nonce GPS fix is acquired.",
                                              btns, false);
            lv_obj_add_event_cb(mbox, mbox_close_cb, LV_EVENT_VALUE_CHANGED, NULL);
            lv_obj_center(mbox);
        }
    }
}
