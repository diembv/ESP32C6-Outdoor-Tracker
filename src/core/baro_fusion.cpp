/**
 * @file baro_fusion.cpp
 * @brief Implementation of Garmin-style 2-tier barometric altitude calibration
 *        & continuous weather drift compensation.
 */

#include "core/baro_fusion.h"
#include <math.h>
#include <stddef.h>

#ifdef ARDUINO
#  include <Arduino.h>
#  define BARO_LOG(...) Serial.printf(__VA_ARGS__)
#else
#  include <stdio.h>
#  define BARO_LOG(...) printf(__VA_ARGS__)
#endif

static constexpr float   DEFAULT_SEA_LEVEL_HPA = 1013.25f;
static constexpr uint8_t CAL_SAMPLES           = 5;
static constexpr uint16_t GPS_WARMUP_SEC       = 35;
static constexpr uint32_t DRIFT_CHECK_MS       = 900000UL; // 15 minutes

static volatile bool s_manual_cal_req = false;
static float         s_p0_hpa         = DEFAULT_SEA_LEVEL_HPA;
static bool          s_baro_cal       = false;
static uint16_t      s_gps_warmup_sec = 0;
static double        s_cal_sum_p0     = 0.0;
static uint8_t       s_cal_count      = 0;
static uint32_t      s_last_drift_ms  = 0;

void baro_fusion_init(void) {
    s_manual_cal_req = false;
    s_p0_hpa         = DEFAULT_SEA_LEVEL_HPA;
    s_baro_cal       = false;
    s_gps_warmup_sec = 0;
    s_cal_sum_p0     = 0.0;
    s_cal_count      = 0;
    s_last_drift_ms  = 0;
}

void baro_request_recalibration(void) {
    s_manual_cal_req = true;
}

void baro_fusion_update(uint32_t now_ms,
                        float pressure_hpa,
                        float temperature_c,
                        const GpsData *gps,
                        BaroData *out_baro) {
    if (!out_baro) return;

    out_baro->pressure_hpa  = pressure_hpa;
    out_baro->temperature_c = temperature_c;

    // 1. Handle manual recalibration request ("CALIB ALT" button on UI)
    if (s_manual_cal_req) {
        if (gps && gps->fix_valid && gps->altitude_valid && gps->satellites >= 5) {
            float k = 1.0f - gps->altitude_m / 44330.0f;
            if (k > 0.5f) {
                float p0 = pressure_hpa / powf(k, 5.255f);
                if (p0 > 970.0f && p0 < 1060.0f) {
                    s_p0_hpa         = p0;
                    s_baro_cal       = true;
                    s_gps_warmup_sec = GPS_WARMUP_SEC;
                    s_cal_count      = 0;
                    s_cal_sum_p0     = 0.0;
                    BARO_LOG("[BARO] >>> THU CONG CALIB: P0 = %.2f hPa (Alt_GPS = %.1fm) <<<\n",
                             s_p0_hpa, gps->altitude_m);
                }
            }
        }
        s_manual_cal_req = false;
    }

    // 2. Initial 1-time P0 calibration at trip start (!s_baro_cal)
    if (!s_baro_cal) {
        bool gps_usable = gps && gps->fix_valid && gps->altitude_valid &&
                          gps->satellites >= 5 &&
                          (gps->hdop <= 2.2f || gps->hdop > 90.0f);

        if (gps_usable) {
            s_gps_warmup_sec++;
            if (s_gps_warmup_sec >= GPS_WARMUP_SEC) {
                float k = 1.0f - gps->altitude_m / 44330.0f;
                if (k > 0.5f) {
                    float p0 = pressure_hpa / powf(k, 5.255f);
                    if (p0 > 970.0f && p0 < 1060.0f) {
                        s_cal_sum_p0 += p0;
                        s_cal_count++;
                    }
                }

                if (s_cal_count >= CAL_SAMPLES) {
                    s_p0_hpa     = (float)(s_cal_sum_p0 / s_cal_count);
                    s_baro_cal   = true;
                    s_cal_count  = 0;
                    s_cal_sum_p0 = 0.0;
                    BARO_LOG("[BARO] >>> GPS DA ON DINH (%us)! KHOA CUNG MOC P0 = %.2f hPa (sats=%u, hdop=%.2f, Alt_GPS=%.1fm) <<<\n",
                             s_gps_warmup_sec, s_p0_hpa, (unsigned)gps->satellites, gps->hdop, gps->altitude_m);
                }
            }
        } else {
            s_gps_warmup_sec = 0;
            s_cal_count      = 0;
            s_cal_sum_p0     = 0.0;
        }
    } else {
        // 3. Continuous slow weather-drift compensation every 15 minutes
        if (now_ms - s_last_drift_ms >= DRIFT_CHECK_MS) {
            s_last_drift_ms = now_ms;
            bool gps_excellent = gps && gps->fix_valid && gps->altitude_valid &&
                                 gps->satellites >= 7 && gps->hdop <= 1.2f;
            if (gps_excellent && out_baro->valid) {
                float alt_diff = gps->altitude_m - out_baro->altitude_m;
                if (fabsf(alt_diff) >= 8.0f) {
                    float p0_nudge = (alt_diff > 0.0f) ? 0.05f : -0.05f;
                    s_p0_hpa += p0_nudge;
                    BARO_LOG("[BARO] >>> BU TROI THOI TIET (15p): Alt_GPS=%.1fm vs Baro=%.1fm (lech %.1fm) -> Nudge P0 to %.2f hPa <<<\n",
                             gps->altitude_m, out_baro->altitude_m, alt_diff, s_p0_hpa);
                }
            }
        }
    }

    float ratio = pressure_hpa / s_p0_hpa;
    out_baro->altitude_m    = 44330.0f * (1.0f - powf(ratio, 0.1903f));
    out_baro->sea_level_hpa = s_p0_hpa;
    out_baro->calibrated    = s_baro_cal;
    out_baro->valid         = true;
}
