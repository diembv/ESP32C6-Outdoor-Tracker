/**
 * @file hal_i2c.h
 * @brief Thread-safe single-owner Wire I2C abstraction.
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the shared I2C bus and its mutex.
 */
void hal_i2c_init(int sda_pin, int scl_pin, uint32_t freq_hz);

/**
 * @brief Acquire I2C bus mutex.
 * @param timeout_ms Timeout in ms (-1 = wait forever).
 */
bool hal_i2c_lock(int timeout_ms);

/**
 * @brief Release I2C bus mutex.
 */
void hal_i2c_unlock(void);

/**
 * @brief Probe an I2C address (returns true if ACK received).
 */
bool hal_i2c_probe(uint8_t addr);

/**
 * @brief Read len bytes starting from an 8-bit register address.
 */
bool hal_i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len);

/**
 * @brief Write 1 byte to an 8-bit register address.
 */
bool hal_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val);

/**
 * @brief Write raw bytes to an I2C device.
 */
bool hal_i2c_write_bytes(uint8_t addr, const uint8_t *data, size_t len);

/**
 * @brief Read raw bytes from an I2C device without sending a register byte.
 */
bool hal_i2c_read_bytes(uint8_t addr, uint8_t *buf, size_t len);

/**
 * @brief Scan all I2C addresses (1..126) and print detected devices to Serial.
 */
void hal_i2c_scan(void);

#ifdef __cplusplus
}
#endif
