/**
 * @file ui_viewmodel.h
 * @brief Hardware-independent UI ViewModel: data formatting, status colors, and shared event callbacks.
 */

#pragma once

#include <lvgl.h>
#include <stdbool.h>
#include "app_types.h"
#include "ui/ui_widgets.h"

typedef struct {
    // Tile 1: Sensors
    bool       gps_fix_valid;
    char       header[48];
    char       bat_badge[16];
    lv_color_t bat_color;
    char       speed[32];
    char       coords[96];
    char       env[128];

    // Tile 0: Navigation
    char       route_name[32];
    char       zoom[16];
    char       dist_rem[32];
    char       xte[32];
    lv_color_t xte_color;
    char       course[32];
    char       speed_alt[32];

    // Tile 2: System
    char       gps_stats[48];
    char       datetime[32];
    char       uptime[48];
    char       battery[32];
    char       sd_status[48];
    lv_color_t sd_color;
    bool       is_logging_active;
    bool       blink_on;
    bool       wifi_active;
    bool       ble_active;
} UiViewModel;

/**
 * @brief Formats SensorSnapshot and RouteNavStatus into display-ready strings & colors.
 */
void ui_vm_format(const SensorSnapshot *snap, UiViewModel *out_vm);

/**
 * @brief Shared LVGL event callbacks (pass UiWidgets* as user_data where needed).
 */
void ui_vm_btn_zoom_cb(lv_event_t *e);
void ui_vm_map_draw_cb(lv_event_t *e);
void ui_vm_dd_mode_cb(lv_event_t *e);
void ui_vm_btn_calib_cb(lv_event_t *e);
void ui_vm_btn_log_cb(lv_event_t *e);
void ui_vm_btn_wifi_cb(lv_event_t *e);
void ui_vm_btn_ble_cb(lv_event_t *e);
void ui_vm_btn_power_cb(lv_event_t *e);

/**
 * @brief Popup helpers to be called from main loop (inside hal_display_lock) after heavy I/O finishes.
 */
void ui_vm_show_wifi_popup(const char *ssid);
void ui_vm_show_log_popup(bool started, bool has_gps_fix, float dist_m, uint32_t dur_s);
