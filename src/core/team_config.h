/**
 * @file team_config.h
 * @brief NVS-persisted Team Beacon configuration shared by Wi-Fi, BLE, and Radio.
 */

#pragma once

#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

void team_config_init(void);
const TeamConfig *team_config_get(void);
void team_config_set(const TeamConfig *cfg);

#ifdef __cplusplus
}
#endif
