/**
 * @file ui_dashboard.cpp
 * @brief Adaptive Dashboard Orchestrator — selects Portrait, Round, or Landscape profile.
 */

#include "ui/ui_dashboard.h"
#include "ui/ui_layout.h"
#include "ui/ui_theme.h"
#include "ui/ui_viewmodel.h"
#include "ui/ui_widgets.h"
#include "boards/board_select.h"
#include <string.h>

static UiWidgets s_widgets;

void ui_dashboard_init(void)
{
    memset(&s_widgets, 0, sizeof(s_widgets));

    lv_obj_t *scr = lv_scr_act();
    lv_obj_set_style_bg_color(scr, CLR_BG, 0);
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, 0);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    ui_theme_strip_scrollbar(scr);

#if defined(BOARD_UI_PROFILE_ROUND) && BOARD_UI_PROFILE_ROUND
    ui_layout_build_round(scr, &s_widgets);
#elif defined(BOARD_UI_PROFILE_LANDSCAPE) && BOARD_UI_PROFILE_LANDSCAPE
    ui_layout_build_landscape(scr, &s_widgets);
#else
    ui_layout_build_portrait(scr, &s_widgets);
#endif
}

void ui_dashboard_update(const SensorSnapshot *snap)
{
    if (!snap) return;

    UiViewModel vm;
    ui_vm_format(snap, &vm);

    lv_obj_t *act_tile = s_widgets.tileview ? lv_tileview_get_tile_act(s_widgets.tileview) : nullptr;

    // ═══ Tile 1: Sensors (Default Center) ═════════════════════════════════════
    if (!act_tile || act_tile == s_widgets.tile[1]) {
        if (s_widgets.t1_dot_fix) {
            lv_obj_set_style_bg_color(s_widgets.t1_dot_fix,
                                      vm.gps_fix_valid ? CLR_GREEN : CLR_RED, 0);
        }
        if (s_widgets.t1_lbl_header) lv_label_set_text(s_widgets.t1_lbl_header, vm.header);
        if (s_widgets.t1_lbl_speed)  lv_label_set_text(s_widgets.t1_lbl_speed, vm.speed);
        if (s_widgets.t1_lbl_coords) lv_label_set_text(s_widgets.t1_lbl_coords, vm.coords);
        if (s_widgets.t1_lbl_env)    lv_label_set_text(s_widgets.t1_lbl_env, vm.env);
    }
    // ═══ Tile 0: Navigation ═══════════════════════════════════════════════════
    else if (act_tile == s_widgets.tile[0]) {
        if (s_widgets.t0_map_area) {
            lv_obj_invalidate(s_widgets.t0_map_area);
        }
        if (s_widgets.t0_lbl_route_name) lv_label_set_text(s_widgets.t0_lbl_route_name, vm.route_name);
        if (s_widgets.t0_lbl_dist_rem)   lv_label_set_text(s_widgets.t0_lbl_dist_rem, vm.dist_rem);
        if (s_widgets.t0_lbl_xte) {
            lv_label_set_text(s_widgets.t0_lbl_xte, vm.xte);
            lv_obj_set_style_text_color(s_widgets.t0_lbl_xte, vm.xte_color, 0);
        }
        if (s_widgets.t0_lbl_zoom)      lv_label_set_text(s_widgets.t0_lbl_zoom, vm.zoom);
        if (s_widgets.t0_lbl_course)    lv_label_set_text(s_widgets.t0_lbl_course, vm.course);
        if (s_widgets.t0_lbl_speed_alt) lv_label_set_text(s_widgets.t0_lbl_speed_alt, vm.speed_alt);
    }
    // ═══ Tile 2: System ═══════════════════════════════════════════════════════
    else if (act_tile == s_widgets.tile[2]) {
        if (s_widgets.t2_dot_gps) {
            lv_obj_set_style_bg_color(s_widgets.t2_dot_gps,
                                      vm.gps_fix_valid ? CLR_GREEN : CLR_RED, 0);
        }
        if (s_widgets.t2_lbl_gps_stats) lv_label_set_text(s_widgets.t2_lbl_gps_stats, vm.gps_stats);
        if (s_widgets.t2_lbl_datetime)  lv_label_set_text(s_widgets.t2_lbl_datetime, vm.datetime);
        if (s_widgets.t2_lbl_uptime)    lv_label_set_text(s_widgets.t2_lbl_uptime, vm.uptime);
        if (s_widgets.t2_lbl_battery)   lv_label_set_text(s_widgets.t2_lbl_battery, vm.battery);
        if (s_widgets.t2_lbl_sd) {
            lv_label_set_text(s_widgets.t2_lbl_sd, vm.sd_status);
            lv_obj_set_style_text_color(s_widgets.t2_lbl_sd, vm.sd_color, 0);
        }

        static bool last_logging_state = false;
        if (vm.is_logging_active != last_logging_state) {
            last_logging_state = vm.is_logging_active;
            if (vm.is_logging_active) {
                if (s_widgets.t2_lbl_btn_log) lv_label_set_text(s_widgets.t2_lbl_btn_log, "STOP (REC)");
                if (s_widgets.t2_btn_log)     lv_obj_set_style_bg_color(s_widgets.t2_btn_log, CLR_RED, 0);
                if (s_widgets.t2_dd_mode)     lv_obj_add_state(s_widgets.t2_dd_mode, LV_STATE_DISABLED);
            } else {
                if (s_widgets.t2_lbl_btn_log) lv_label_set_text(s_widgets.t2_lbl_btn_log, "START LOG");
                if (s_widgets.t2_btn_log)     lv_obj_set_style_bg_color(s_widgets.t2_btn_log, lv_color_hex(0x2A3B4C), 0);
                if (s_widgets.t2_dd_mode)     lv_obj_clear_state(s_widgets.t2_dd_mode, LV_STATE_DISABLED);
            }
        } else if (vm.is_logging_active && s_widgets.t2_btn_log) {
            lv_obj_set_style_bg_color(s_widgets.t2_btn_log,
                                      vm.blink_on ? CLR_RED : lv_color_hex(0x8B0000), 0);
        }

        if (s_widgets.t2_btn_wifi && s_widgets.t2_lbl_btn_wifi) {
            lv_obj_set_style_bg_color(s_widgets.t2_btn_wifi,
                                      vm.wifi_active ? lv_color_hex(0x0369A1) : lv_color_hex(0x1F2937), 0);
            lv_label_set_text(s_widgets.t2_lbl_btn_wifi,
                              vm.wifi_active ? LV_SYMBOL_WIFI " ON" : LV_SYMBOL_WIFI " WIFI");
        }
        if (s_widgets.t2_btn_ble && s_widgets.t2_lbl_btn_ble) {
            lv_obj_set_style_bg_color(s_widgets.t2_btn_ble,
                                      vm.ble_active ? lv_color_hex(0x065F46) : lv_color_hex(0x1F2937), 0);
            lv_label_set_text(s_widgets.t2_lbl_btn_ble,
                              vm.ble_active ? LV_SYMBOL_BLUETOOTH " ON" : LV_SYMBOL_BLUETOOTH " BLE");
        }
    }
}
