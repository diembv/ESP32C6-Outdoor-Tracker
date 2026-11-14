/**
 * @file hal_storage.h
 * @brief Hardware Abstraction Layer for MicroSD / persistent storage.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Mount the storage volume (e.g. /sdcard).
 * @param out_capacity_mb Optional pointer to receive card capacity in MB.
 * @return true if mounted successfully.
 */
bool hal_storage_init(uint32_t *out_capacity_mb);

/**
 * @brief Check whether storage is mounted and ready.
 */
bool hal_storage_is_ready(void);

/**
 * @brief Get mounted storage capacity in MB.
 */
uint32_t hal_storage_capacity_mb(void);

/**
 * @brief Ensure a directory exists (creates it if missing).
 */
bool hal_storage_ensure_dir(const char *path);

/**
 * @brief Return the root mount point (e.g. "/sdcard").
 */
const char *hal_storage_mount_point(void);

#ifdef __cplusplus
}
#endif
