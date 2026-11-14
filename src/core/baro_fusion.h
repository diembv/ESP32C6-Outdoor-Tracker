/**
 * @file baro_fusion.h
 * @brief Garmin-style 2-tier barometric altitude calibration & drift compensation.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize barometric fusion state (P0 = 1013.25 hPa, uncalibrated).
 */
void baro_fusion_init(void);

/**
 * @brief Request immediate manual P0 recalibration from current GPS altitude.
 */
void baro_request_recalibration(void);

/**
 * @brief Update barometric altitude from raw pressure/temperature and GPS state.
 *        Should be called at ~1 Hz when a valid barometer reading is available.
 *
 * @param now_ms         Current system uptime in milliseconds
 * @param pressure_hpa   Measured pressure in hPa
 * @param temperature_c  Measured temperature in Celsius
 * @param gps            Current GPS snapshot (can be NULL if unavailable)
 * @param out_baro       Output BaroData struct to populate
 */
void baro_fusion_update(uint32_t now_ms,
                        float pressure_hpa,
                        float temperature_c,
                        const GpsData *gps,
                        BaroData *out_baro);

#ifdef __cplusplus
}
#endif
