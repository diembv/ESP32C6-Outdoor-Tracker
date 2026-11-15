/**
 * @file axp2101.h
 * @brief Lightweight direct-Wire driver for X-Powers AXP2101 PMIC.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool axp2101_init(void);
bool axp2101_is_ready(void);
bool axp2101_read_battery(float *out_volts, uint8_t *out_pct, bool *out_charging);

#ifdef __cplusplus
}
#endif
