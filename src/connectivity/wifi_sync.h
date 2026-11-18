/**
 * @file wifi_sync.h
 * @brief On-Demand Wi-Fi AP + Embedded Lite Portal & REST API (192.168.4.1)
 */

#pragma once

#include <stdbool.h>
#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start Wi-Fi AP ("OutdoorTracker-XXXX") and HTTP server at 192.168.4.1.
 */
bool wifi_sync_start(void);

/**
 * @brief Stop Wi-Fi AP and HTTP server, turning off Wi-Fi radio to save power.
 */
void wifi_sync_stop(void);

/**
 * @brief Toggle Wi-Fi Sync on/off.
 */
bool wifi_sync_toggle(void);

/**
 * @brief Returns true if Wi-Fi Sync AP is currently active.
 */
bool wifi_sync_is_active(void);

/**
 * @brief Returns the active AP SSID (e.g., "OutdoorTracker-A1B2").
 */
const char *wifi_sync_get_ssid(void);

/**
 * @brief Must be called regularly from main loop() to handle HTTP requests & 5-min idle timeout.
 */
void wifi_sync_loop(void);

#ifdef __cplusplus
}
#endif
