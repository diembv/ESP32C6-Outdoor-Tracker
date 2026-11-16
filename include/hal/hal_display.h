/**
 * @file hal_display.h
 * @brief Hardware Abstraction Layer for Display, LVGL Port, Mutex, and Brightness.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize display hardware, touch controller, LVGL, and LVGL FreeRTOS task.
 *        Must be called after hal_i2c_init() and hal_power_init().
 */
void hal_display_init(void);

/**
 * @brief Acquire LVGL thread mutex.
 * @param timeout_ms Timeout in ms (-1 = wait forever).
 * @return true if lock acquired.
 */
bool hal_display_lock(int timeout_ms);

/**
 * @brief Release LVGL thread mutex.
 */
void hal_display_unlock(void);

/**
 * @brief Set AMOLED panel brightness (0 = off, up to 255).
 */
void hal_display_set_brightness(uint8_t level);

#ifdef __cplusplus
}
#endif
