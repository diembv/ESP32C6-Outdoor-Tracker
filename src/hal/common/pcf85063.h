/**
 * @file pcf85063.h
 * @brief Lightweight direct-Wire driver for PCF85063 RTC (0x51).
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

bool pcf85063_init(void);
bool pcf85063_is_ready(void);
bool pcf85063_get_time(struct tm *out_tm);
bool pcf85063_set_time(const struct tm *tm_val);
bool pcf85063_sync_system_clock(void);

#ifdef __cplusplus
}
#endif
