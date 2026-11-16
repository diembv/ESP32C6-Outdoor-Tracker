/**
 * @file touch_cst8xx.h
 * @brief Hynitron CST816 / CST820 capacitive touch driver over hal_i2c (0x15).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

bool touch_cst8xx_init(uint8_t addr);
bool touch_cst8xx_read(uint8_t addr, uint16_t *x, uint16_t *y);
