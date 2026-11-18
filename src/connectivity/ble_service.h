/**
 * @file ble_service.h
 * @brief On-Demand NimBLE GATT Telemetry & Remote Control Service
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

bool ble_service_start(void);
void ble_service_stop(void);
bool ble_service_toggle(void);
bool ble_service_is_active(void);
bool ble_service_is_connected(void);
void ble_service_loop(void);

#ifdef __cplusplus
}
#endif
