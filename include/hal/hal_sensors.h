/**
 * @file hal_sensors.h
 * @brief Hardware Abstraction Layer for GNSS, IMU, Barometer, and RTC.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize onboard & external sensors (BMP580, QMI8658, PCF85063 RTC, GPS UART).
 *        Must be called after hal_i2c_init() and hal_power_init().
 */
void hal_sensors_init(void);

/**
 * @brief Drain available bytes from GPS UART into NMEA parser.
 * @return true if at least one valid NMEA sentence was decoded.
 */
bool hal_sensors_poll_gps(void);

/**
 * @brief Read fast sensors (IMU pitch/roll) into snapshot (~5 Hz).
 */
void hal_sensors_read_fast(SensorSnapshot *snap);

/**
 * @brief Read slow sensors (GPS + Barometer + RTC sync) into snapshot (~1 Hz).
 */
void hal_sensors_read_slow(uint32_t now_ms, uint32_t boot_ms, SensorSnapshot *snap);

/**
 * @brief Retry initializing offline I2C sensors (BMP580 / QMI8658).
 */
void hal_sensors_retry_i2c(void);

#ifdef __cplusplus
}
#endif
