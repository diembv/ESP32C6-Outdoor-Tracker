/**
 * @file tca9554.h
 * @brief Lightweight direct-Wire driver for TCA9554/PCA9554 8-bit I2C IO Expander.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool tca9554_init(uint8_t addr);
bool tca9554_set_dir(uint8_t pin, bool is_output);
bool tca9554_write_pin(uint8_t pin, bool high);
bool tca9554_read_pin(uint8_t pin, bool *out_high);

#ifdef __cplusplus
}
#endif
