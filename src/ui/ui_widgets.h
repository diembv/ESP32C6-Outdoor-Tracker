/**
 * @file ui_widgets.h
 * @brief Shared widget handle registry populated by all UI layout profiles.
 */

#pragma once

#include <lvgl.h>

typedef struct {
    lv_obj_t *tileview;
    lv_obj_t *tile[3];

    // ── Tile 0: Navigation & Breadcrumb Map ───────────────────────────────────
    lv_obj_t *t0_lbl_route_name;
    lv_obj_t *t0_btn_zoom;
    lv_obj_t *t0_lbl_zoom;
    lv_obj_t *t0_map_area;
    lv_obj_t *t0_lbl_dist_rem;
    lv_obj_t *t0_lbl_xte;
    lv_obj_t *t0_lbl_course;
    lv_obj_t *t0_lbl_speed_alt;
    lv_obj_t *t0_lbl_map_note;

    // ── Tile 1: Outdoor Sensors (Default Center) ──────────────────────────────
    lv_obj_t *t1_dot_fix;
    lv_obj_t *t1_lbl_header;
    lv_obj_t *t1_lbl_speed;
    lv_obj_t *t1_lbl_coords;
    lv_obj_t *t1_lbl_env;

    // ── Tile 2: System & Controls ─────────────────────────────────────────────
    lv_obj_t *t2_dot_gps;
    lv_obj_t *t2_lbl_gps_stats;
    lv_obj_t *t2_lbl_datetime;
    lv_obj_t *t2_lbl_uptime;
    lv_obj_t *t2_lbl_battery;
    lv_obj_t *t2_lbl_sd;
    lv_obj_t *t2_btn_calib;
    lv_obj_t *t2_dd_mode;
    lv_obj_t *t2_btn_log;
    lv_obj_t *t2_lbl_btn_log;
} UiWidgets;
