/**
 * @file track_logger.h
 * @brief Smart CSV Track Logger with Anti-Drift & Sport Profiles (Hiking/Cycling).
 */

#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

bool track_logger_init(SensorSnapshot *snap);
void track_logger_log(SensorSnapshot *snap, uint32_t now_ms);
bool track_logger_is_ok(void);
void track_logger_flush(void);
void track_logger_toggle(SensorSnapshot *snap, uint32_t now_ms);
void track_logger_get_stats(uint32_t *points_logged, size_t *buffer_usage);
void track_logger_get_summary(float *distance_m, uint32_t *duration_s, uint32_t now_ms);
void track_logger_set_activity_mode(ActivityMode mode);
ActivityMode track_logger_get_activity_mode(void);

#ifdef __cplusplus
}
#endif
