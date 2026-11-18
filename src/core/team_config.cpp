/**
 * @file team_config.cpp
 * @brief NVS-persisted Team Beacon configuration using ESP32 Preferences.
 */

#include "core/team_config.h"
#include <Preferences.h>
#include <string.h>

static TeamConfig s_cfg = {
    .enabled      = false,
    .team_id      = 1024,
    .node_id      = 1,
    .callsign     = "HIKER1",
    .alert_dist_m = 250
};

static Preferences s_prefs;
static bool s_inited = false;

void team_config_init(void)
{
    if (s_inited) return;
    s_inited = true;

    if (s_prefs.begin("team_cfg", true)) {
        s_cfg.enabled      = s_prefs.getBool("en", false);
        s_cfg.team_id      = s_prefs.getUShort("tid", 1024);
        s_cfg.node_id      = s_prefs.getUChar("nid", 1);
        s_cfg.alert_dist_m = s_prefs.getUShort("adist", 250);
        String cs          = s_prefs.getString("cs", "HIKER1");
        snprintf(s_cfg.callsign, sizeof(s_cfg.callsign), "%-6.6s", cs.c_str());
        // Trim trailing spaces if any
        for (int i = 5; i >= 0; i--) {
            if (s_cfg.callsign[i] == ' ') s_cfg.callsign[i] = '\0';
            else break;
        }
        s_prefs.end();
    }
}

const TeamConfig *team_config_get(void)
{
    if (!s_inited) team_config_init();
    return &s_cfg;
}

void team_config_set(const TeamConfig *cfg)
{
    if (!cfg) return;
    if (!s_inited) team_config_init();

    s_cfg = *cfg;
    if (s_cfg.node_id < 1 || s_cfg.node_id > 8) s_cfg.node_id = 1;
    if (s_cfg.alert_dist_m < 50) s_cfg.alert_dist_m = 50;
    s_cfg.callsign[6] = '\0';

    if (s_prefs.begin("team_cfg", false)) {
        s_prefs.putBool("en", s_cfg.enabled);
        s_prefs.putUShort("tid", s_cfg.team_id);
        s_prefs.putUChar("nid", s_cfg.node_id);
        s_prefs.putUShort("adist", s_cfg.alert_dist_m);
        s_prefs.putString("cs", s_cfg.callsign);
        s_prefs.end();
    }
}
