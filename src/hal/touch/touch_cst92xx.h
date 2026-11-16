/**
 * @file touch_cst92xx.h
 * @brief Hynitron CST9217 / CST9220 capacitive touch driver over hal_i2c (0x5A).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

bool touch_cst92xx_init(uint8_t addr);
bool touch_cst92xx_read(uint8_t addr, uint16_t *x, uint16_t *y);
