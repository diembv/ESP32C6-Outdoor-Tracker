/**
 * @file ble_service.cpp
 * @brief On-Demand NimBLE GATT Telemetry (1Hz, 32B) & Remote Control
 */

#include "connectivity/ble_service.h"
#include "app_types.h"
#include "core/baro_fusion.h"
#include "core/track_logger.h"
#include "core/team_config.h"

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string.h>

extern SensorSnapshot g_snap;

#define BLE_SVC_UUID        "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_CHAR_TELEM_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define BLE_CHAR_CTRL_UUID  "6E400004-B5A3-F393-E0A9-E50E24DCCA9E"

#pragma pack(push, 1)
typedef struct {
    int32_t  lat_e7;        // Latitude * 1e7
    int32_t  lon_e7;        // Longitude * 1e7
    int32_t  alt_dm;        // Altitude in decimeters (0.1m)
    uint16_t speed_cms;     // Speed in cm/s
    uint16_t course_deg10;  // Course * 10
    uint8_t  sats;          // Satellites count
    uint8_t  hdop_x10;      // HDOP * 10 (capped 255)
    uint8_t  bat_pct;       // Battery 0..100%
    uint8_t  flags;         // BIT0:fix | BIT1:logging | BIT2:baro_cal | BIT3:team_en | BIT4:sos | BIT5:bike
    uint32_t uptime_s;      // Uptime in seconds
    uint8_t  reserved[8];   // Padding to 32 bytes
} BleTelemetryPacket;
#pragma pack(pop)

static bool                s_active    = false;
static bool                s_connected = false;
static uint32_t            s_last_tx   = 0;
static NimBLEServer       *s_server    = nullptr;
static NimBLECharacteristic *s_telem_chr = nullptr;

class TrackerServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo) override {
        (void)pServer;
        (void)connInfo;
        s_connected = true;
        Serial.println(F("[BLE] Client connected"));
    }
    void onDisconnect(NimBLEServer *pServer, NimBLEConnInfo &connInfo, int reason) override {
        (void)connInfo;
        (void)reason;
        s_connected = false;
        Serial.println(F("[BLE] Client disconnected"));
        if (s_active && pServer) {
            NimBLEDevice::startAdvertising();
        }
    }
};

class TrackerCtrlCallbacks : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pChar, NimBLEConnInfo &connInfo) override {
        (void)connInfo;
        std::string val = pChar->getValue();
        if (val.empty()) return;

        uint8_t cmd = (uint8_t)val[0];
        switch (cmd) {
            case 0x01: // Toggle CSV logging
                track_logger_toggle(&g_snap, millis());
                break;
            case 0x02: // Request Baro recalibration
                baro_request_recalibration();
                break;
            case 0x03: // Set activity mode (0=Hike, 1=Bike)
                if (val.size() >= 2) {
                    ActivityMode mode = (val[1] == 1) ? ACTIVITY_MODE_BIKE : ACTIVITY_MODE_HIKE;
                    g_snap.sys.activity_mode = mode;
                    track_logger_set_activity_mode(mode);
                }
                break;
            case 0x04: // Update TeamConfig: [0x04, enabled, team_id_lo, team_id_hi, node_id, cs[6]]
                if (val.size() >= 11) {
                    TeamConfig cfg = *team_config_get();
                    cfg.enabled = (val[1] != 0);
                    cfg.team_id = (uint16_t)((uint8_t)val[2] | ((uint8_t)val[3] << 8));
                    cfg.node_id = (uint8_t)val[4];
                    memcpy(cfg.callsign, &val[5], 6);
                    cfg.callsign[6] = '\0';
                    team_config_set(&cfg);
                }
                break;
            case 0x05: // Toggle/Set SOS
                if (val.size() >= 2) {
                    g_snap.sys.sos_active = (val[1] != 0);
                } else {
                    g_snap.sys.sos_active = !g_snap.sys.sos_active;
                }
                break;
            default:
                break;
        }
        Serial.printf("[BLE] Ctrl cmd 0x%02X executed\n", cmd);
    }
};

static TrackerServerCallbacks s_srv_cb;
static TrackerCtrlCallbacks   s_ctrl_cb;

bool ble_service_start(void)
{
    if (s_active) return true;

    NimBLEDevice::init("OutdoorTracker");
    NimBLEDevice::setPower(ESP_PWR_LVL_P3);

    s_server = NimBLEDevice::createServer();
    s_server->setCallbacks(&s_srv_cb);

    NimBLEService *svc = s_server->createService(BLE_SVC_UUID);

    s_telem_chr = svc->createCharacteristic(
        BLE_CHAR_TELEM_UUID,
        NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY
    );

    NimBLECharacteristic *ctrl_chr = svc->createCharacteristic(
        BLE_CHAR_CTRL_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    ctrl_chr->setCallbacks(&s_ctrl_cb);

    NimBLEAdvertising *adv = NimBLEDevice::getAdvertising();
    adv->addServiceUUID(BLE_SVC_UUID);
    adv->setName("OutdoorTracker");
    adv->start();

    s_active = true;
    g_snap.sys.ble_active = true;
    Serial.println(F("[BLE] NimBLE GATT service started"));
    return true;
}

void ble_service_stop(void)
{
    if (!s_active) return;
    NimBLEDevice::deinit(true);
    s_server    = nullptr;
    s_telem_chr = nullptr;
    s_connected = false;
    s_active    = false;
    g_snap.sys.ble_active = false;
    Serial.println(F("[BLE] NimBLE GATT service stopped"));
}

bool ble_service_toggle(void)
{
    if (s_active) {
        ble_service_stop();
        return false;
    }
    return ble_service_start();
}

bool ble_service_is_active(void)
{
    return s_active;
}

bool ble_service_is_connected(void)
{
    return s_connected;
}

void ble_service_loop(void)
{
    if (!s_active || !s_connected || !s_telem_chr) return;
    uint32_t now = millis();
    if ((now - s_last_tx) < 1000) return;
    s_last_tx = now;

    BleTelemetryPacket pkt;
    memset(&pkt, 0, sizeof(pkt));
    pkt.lat_e7       = (int32_t)(g_snap.gps.latitude * 1e7);
    pkt.lon_e7       = (int32_t)(g_snap.gps.longitude * 1e7);
    float alt        = g_snap.baro.valid ? g_snap.baro.altitude_m : g_snap.gps.altitude_m;
    pkt.alt_dm       = (int32_t)(alt * 10.0f);
    pkt.speed_cms    = (uint16_t)(g_snap.gps.speed_kmh * 27.7778f);
    pkt.course_deg10 = (uint16_t)(g_snap.gps.course_deg * 10.0f);
    pkt.sats         = (uint8_t)(g_snap.gps.satellites > 255 ? 255 : g_snap.gps.satellites);
    float hdop10     = g_snap.gps.hdop * 10.0f;
    pkt.hdop_x10     = (uint8_t)(hdop10 > 255.0f ? 255 : (hdop10 < 0.0f ? 0 : hdop10));
    pkt.bat_pct      = g_snap.sys.battery_pct;
    pkt.uptime_s     = g_snap.sys.uptime_s;

    const TeamConfig *tcfg = team_config_get();
    uint8_t fl = 0;
    if (g_snap.gps.fix_valid)          fl |= (1 << 0);
    if (g_snap.sys.is_logging_active)  fl |= (1 << 1);
    if (g_snap.baro.calibrated)        fl |= (1 << 2);
    if (tcfg && tcfg->enabled)         fl |= (1 << 3);
    if (g_snap.sys.sos_active)         fl |= (1 << 4);
    if (g_snap.sys.activity_mode == ACTIVITY_MODE_BIKE) fl |= (1 << 5);
    pkt.flags = fl;

    s_telem_chr->setValue((const uint8_t *)&pkt, sizeof(pkt));
    s_telem_chr->notify();
}
