/**
 * @file hal_touch.h
 * @brief Hardware Abstraction Layer for multi-IC capacitive touch controllers.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize touch controller over shared hal_i2c bus.
 * @return true if touch controller responded and initialized.
 */
bool hal_touch_init(void);

/**
 * @brief Read current touch point.
 * @param x Output X coordinate (clamped to display width).
 * @param y Output Y coordinate (clamped to display height).
 * @return true if screen is currently touched.
 */
bool hal_touch_read(uint16_t *x, uint16_t *y);

/**
 * @brief Return active touch controller name (for diagnostics).
 */
const char *hal_touch_driver_name(void);

#ifdef __cplusplus
}
#endif
