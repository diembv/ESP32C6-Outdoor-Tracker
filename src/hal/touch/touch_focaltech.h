/**
 * @file touch_focaltech.h
 * @brief FocalTech FT3168 / FT6336 capacitive touch driver over hal_i2c.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

bool touch_focaltech_init(uint8_t addr);
bool touch_focaltech_read(uint8_t addr, uint16_t *x, uint16_t *y);
