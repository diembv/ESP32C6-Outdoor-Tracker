/**
 * @file hal_power.h
 * @brief Power management, PMIC/ADC battery reading, IO expander, and boot diagnostics.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Track boot counter across soft resets and return last reset reason string.
 */
void hal_power_track_boot(uint32_t *out_boot_count, const char **out_reset_reason);

/**
 * @brief Initialize board power rails (AXP2101 PMIC, TCA9554 EXIO, power button).
 *        Must be called after hal_i2c_init().
 */
void hal_power_init(void);

/**
 * @brief Read battery voltage (V) and percentage (0-100).
 */
void hal_power_read_battery(float *out_volts, uint8_t *out_pct);

/**
 * @brief Check if the physical power/boot button is currently pressed.
 */
bool hal_power_is_button_pressed(void);

/**
 * @brief Shut down the device (AXP2101 hardware power-off if available, else ESP32 Deep Sleep).
 */
void hal_power_shutdown(void);

/**
 * @brief Thread-safe UI-to-main-loop power action requests.
 */
void hal_power_request_screen_sleep(void);
bool hal_power_consume_screen_sleep_req(void);
void hal_power_request_power_off(void);
bool hal_power_consume_power_off_req(void);

#ifdef __cplusplus
}
#endif
