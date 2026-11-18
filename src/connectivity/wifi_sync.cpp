/**
 * @file wifi_sync.cpp
 * @brief On-Demand Wi-Fi AP + Embedded Lite Portal & REST API (192.168.4.1)
 */

#include "connectivity/wifi_sync.h"
#include "boards/board_select.h"
#include "hal/hal_display.h"
#include "core/team_config.h"
#include "route_nav.h"
#include "web_portal_gz.h"

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <stdio.h>
#include <string.h>

extern SensorSnapshot g_snap;

static constexpr uint32_t WIFI_IDLE_TIMEOUT_MS = 300000UL; // 5 minutes
static WebServer s_server(80);
static bool      s_active = false;
static uint32_t  s_last_activity_ms = 0;
static char      s_ssid[32] = "OutdoorTracker";

// ── Conditional SPI2 Display Bus Lock ─────────────────────────────────────────
static inline void sd_bus_lock(void)
{
#if defined(BOARD_SD_SHARES_DISPLAY_BUS) && BOARD_SD_SHARES_DISPLAY_BUS
    hal_display_lock(-1);
#endif
}

static inline void sd_bus_unlock(void)
{
#if defined(BOARD_SD_SHARES_DISPLAY_BUS) && BOARD_SD_SHARES_DISPLAY_BUS
    hal_display_unlock();
#endif
}

static void touch_activity(void)
{
    s_last_activity_ms = millis();
}

static bool is_safe_filename(const String &name)
{
    if (name.length() == 0 || name.length() > 64) return false;
    if (name.indexOf("..") >= 0 || name.indexOf('/') >= 0 || name.indexOf('\\') >= 0) return false;
    return true;
}

// ── HTTP Handlers ─────────────────────────────────────────────────────────────

static void handle_root(void)
{
    touch_activity();
    s_server.sendHeader("Content-Encoding", "gzip");
    s_server.sendHeader("Cache-Control", "no-cache");
    s_server.send_P(200, "text/html", (const char *)WEB_PORTAL_GZ, WEB_PORTAL_GZ_LEN);
}

static void handle_status(void)
{
    touch_activity();
    const RouteNavStatus *nav = route_nav_get_status();
    char json[512];
    snprintf(json, sizeof(json),
             "{\"board\":\"%s\",\"bat_v\":%.2f,\"bat_pct\":%d,"
             "\"gps_fix\":%s,\"sats\":%lu,\"lat\":%.6f,\"lon\":%.6f,"
             "\"baro_alt\":%.1f,\"sd_ok\":%s,\"sd_free_mb\":%lu,"
             "\"route_name\":\"%s\",\"logging\":%s}",
             BOARD_NAME,
             g_snap.sys.battery_v,
             (int)g_snap.sys.battery_pct,
             g_snap.gps.fix_valid ? "true" : "false",
             (unsigned long)g_snap.gps.satellites,
             g_snap.gps.latitude,
             g_snap.gps.longitude,
             g_snap.baro.valid ? g_snap.baro.altitude_m : g_snap.gps.altitude_m,
             (g_snap.sys.sd_present && g_snap.sys.sd_ok) ? "true" : "false",
             (unsigned long)g_snap.sys.sd_free_mb,
             (nav && nav->is_active) ? nav->route_name : "",
             g_snap.sys.is_logging_active ? "true" : "false");
    s_server.send(200, "application/json", json);
}

static void handle_list_dir(const char *dir_path, const char *ext)
{
    touch_activity();
    String json = "{\"files\":[";
    bool first = true;

    sd_bus_lock();
    DIR *dir = opendir(dir_path);
    if (dir) {
        struct dirent *ent;
        size_t ext_len = strlen(ext);
        while ((ent = readdir(dir)) != NULL) {
            size_t nlen = strlen(ent->d_name);
            if (nlen > ext_len && strcasecmp(ent->d_name + nlen - ext_len, ext) == 0) {
                char full[128];
                snprintf(full, sizeof(full), "%s/%s", dir_path, ent->d_name);
                struct stat st;
                uint32_t sz = 0;
                if (stat(full, &st) == 0) sz = (uint32_t)st.st_size;
                if (!first) json += ",";
                json += "{\"name\":\"";
                json += ent->d_name;
                json += "\",\"size\":";
                json += String(sz);
                json += "}";
                first = false;
            }
        }
        closedir(dir);
    }
    sd_bus_unlock();

    json += "]}";
    s_server.send(200, "application/json", json);
}

static void handle_logs_list(void)
{
    handle_list_dir("/sdcard/logs", ".csv");
}

static void handle_logs_download(void)
{
    touch_activity();
    String name = s_server.arg("name");
    if (!is_safe_filename(name)) {
        s_server.send(400, "text/plain", "Invalid filename");
        return;
    }

    char path[128];
    snprintf(path, sizeof(path), "/sdcard/logs/%s", name.c_str());

    sd_bus_lock();
    struct stat st;
    if (stat(path, &st) != 0) {
        sd_bus_unlock();
        s_server.send(404, "text/plain", "Not found");
        return;
    }
    FILE *f = fopen(path, "rb");
    sd_bus_unlock();

    if (!f) {
        s_server.send(500, "text/plain", "Open failed");
        return;
    }

    s_server.setContentLength((size_t)st.st_size);
    s_server.send(200, "text/csv", "");

    uint8_t buf[1024];
    WiFiClient client = s_server.client();
    while (client.connected()) {
        sd_bus_lock();
        size_t n = fread(buf, 1, sizeof(buf), f);
        sd_bus_unlock();
        if (n == 0) break;
        client.write(buf, n);
        touch_activity();
        delay(1);
    }

    sd_bus_lock();
    fclose(f);
    sd_bus_unlock();
}

static void handle_logs_delete(void)
{
    touch_activity();
    String name = s_server.arg("name");
    if (!is_safe_filename(name)) {
        s_server.send(400, "application/json", "{\"ok\":false}");
        return;
    }
    char path[128];
    snprintf(path, sizeof(path), "/sdcard/logs/%s", name.c_str());

    sd_bus_lock();
    int rc = unlink(path);
    sd_bus_unlock();

    s_server.send(rc == 0 ? 200 : 404, "application/json", rc == 0 ? "{\"ok\":true}" : "{\"ok\":false}");
}

static void handle_routes_list(void)
{
    handle_list_dir("/sdcard/routes", ".bin");
}

static void handle_routes_upload(void)
{
    touch_activity();
    String name = s_server.arg("name");
    if (name.length() == 0) name = "active.bin";
    if (!is_safe_filename(name)) {
        s_server.send(400, "application/json", "{\"ok\":false,\"err\":\"bad_name\"}");
        return;
    }

    if (!s_server.hasArg("plain")) {
        s_server.send(400, "application/json", "{\"ok\":false,\"err\":\"empty_body\"}");
        return;
    }

    const String &body = s_server.arg("plain");
    if (body.length() < sizeof(RouteHeader) || memcmp(body.c_str(), ROUTE_HEADER_MAGIC, 4) != 0) {
        s_server.send(400, "application/json", "{\"ok\":false,\"err\":\"invalid_rout\"}");
        return;
    }

    char path[128];
    snprintf(path, sizeof(path), "/sdcard/routes/%s", name.c_str());

    sd_bus_lock();
    mkdir("/sdcard/routes", 0777);
    FILE *f = fopen(path, "wb");
    bool ok = false;
    if (f) {
        size_t written = fwrite(body.c_str(), 1, body.length(), f);
        fflush(f);
        fclose(f);
        ok = (written == body.length());
    }
    sd_bus_unlock();

    if (ok) {
        route_nav_load_file(path);
        s_server.send(200, "application/json", "{\"ok\":true}");
    } else {
        s_server.send(500, "application/json", "{\"ok\":false,\"err\":\"write_fail\"}");
    }
}

static void handle_routes_activate(void)
{
    touch_activity();
    String name = s_server.arg("name");
    if (!is_safe_filename(name)) {
        s_server.send(400, "application/json", "{\"ok\":false}");
        return;
    }
    char path[128];
    snprintf(path, sizeof(path), "/sdcard/routes/%s", name.c_str());
    bool ok = route_nav_load_file(path);
    s_server.send(ok ? 200 : 404, "application/json", ok ? "{\"ok\":true}" : "{\"ok\":false}");
}

static void handle_team_get(void)
{
    touch_activity();
    const TeamConfig *cfg = team_config_get();
    char json[192];
    snprintf(json, sizeof(json),
             "{\"enabled\":%s,\"team_id\":%u,\"node_id\":%u,\"callsign\":\"%s\",\"alert_dist_m\":%u}",
             cfg->enabled ? "true" : "false",
             (unsigned)cfg->team_id,
             (unsigned)cfg->node_id,
             cfg->callsign,
             (unsigned)cfg->alert_dist_m);
    s_server.send(200, "application/json", json);
}

static void handle_team_post(void)
{
    touch_activity();
    if (!s_server.hasArg("plain")) {
        s_server.send(400, "application/json", "{\"ok\":false}");
        return;
    }
    const String &body = s_server.arg("plain");
    TeamConfig cfg = *team_config_get();

    // Lightweight zero-dependency JSON field extraction
    if (body.indexOf("\"enabled\":true") >= 0) cfg.enabled = true;
    else if (body.indexOf("\"enabled\":false") >= 0) cfg.enabled = false;

    int idx;
    if ((idx = body.indexOf("\"team_id\":")) >= 0) {
        cfg.team_id = (uint16_t)atoi(body.c_str() + idx + 10);
    }
    if ((idx = body.indexOf("\"node_id\":")) >= 0) {
        cfg.node_id = (uint8_t)atoi(body.c_str() + idx + 10);
    }
    if ((idx = body.indexOf("\"alert_dist_m\":")) >= 0) {
        cfg.alert_dist_m = (uint16_t)atoi(body.c_str() + idx + 15);
    }
    if ((idx = body.indexOf("\"callsign\":\"")) >= 0) {
        const char *p = body.c_str() + idx + 12;
        int i = 0;
        while (i < 6 && p[i] && p[i] != '"') {
            cfg.callsign[i] = p[i];
            i++;
        }
        cfg.callsign[i] = '\0';
    }

    team_config_set(&cfg);
    s_server.send(200, "application/json", "{\"ok\":true}");
}

// ── Public API ────────────────────────────────────────────────────────────────

bool wifi_sync_start(void)
{
    if (s_active) return true;

    uint8_t mac[6] = {0};
    WiFi.mode(WIFI_AP);
    WiFi.softAPmacAddress(mac);
    snprintf(s_ssid, sizeof(s_ssid), "OutdoorTracker-%02X%02X", mac[4], mac[5]);

    if (!WiFi.softAP(s_ssid)) {
        Serial.println(F("[WIFI] softAP start failed"));
        WiFi.mode(WIFI_OFF);
        return false;
    }

    s_server.on("/", HTTP_GET, handle_root);
    s_server.on("/api/status", HTTP_GET, handle_status);
    s_server.on("/api/logs", HTTP_GET, handle_logs_list);
    s_server.on("/api/logs/download", HTTP_GET, handle_logs_download);
    s_server.on("/api/logs", HTTP_DELETE, handle_logs_delete);
    s_server.on("/api/routes", HTTP_GET, handle_routes_list);
    s_server.on("/api/routes/upload", HTTP_POST, handle_routes_upload);
    s_server.on("/api/routes/activate", HTTP_POST, handle_routes_activate);
    s_server.on("/api/team", HTTP_GET, handle_team_get);
    s_server.on("/api/team", HTTP_POST, handle_team_post);

    s_server.begin();
    s_active = true;
    g_snap.sys.wifi_sync_active = true;
    touch_activity();

    Serial.printf("[WIFI] Sync AP started: SSID=%s IP=%s\n",
                  s_ssid, WiFi.softAPIP().toString().c_str());
    return true;
}

void wifi_sync_stop(void)
{
    if (!s_active) return;
    s_server.stop();
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    s_active = false;
    g_snap.sys.wifi_sync_active = false;
    Serial.println(F("[WIFI] Sync AP stopped"));
}

bool wifi_sync_toggle(void)
{
    if (s_active) {
        wifi_sync_stop();
        return false;
    }
    return wifi_sync_start();
}

bool wifi_sync_is_active(void)
{
    return s_active;
}

const char *wifi_sync_get_ssid(void)
{
    return s_ssid;
}

void wifi_sync_loop(void)
{
    if (!s_active) return;
    s_server.handleClient();
    if ((millis() - s_last_activity_ms) >= WIFI_IDLE_TIMEOUT_MS) {
        Serial.println(F("[WIFI] Idle 5-min timeout -> auto-stopping AP"));
        wifi_sync_stop();
    }
}
