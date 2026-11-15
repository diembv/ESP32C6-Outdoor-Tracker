/**
 * @file qmi8658.h
 * @brief Lightweight direct-Wire driver for QMI8658 6-axis IMU.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool qmi8658_init(void);
bool qmi8658_is_ready(void);
uint8_t qmi8658_get_addr(void);
bool qmi8658_read_tilt(float *out_pitch_deg, float *out_roll_deg);

#ifdef __cplusplus
}
#endif
