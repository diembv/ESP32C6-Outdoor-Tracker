/**
 * @file ui_dashboard.h
 * @brief Adaptive Multi-Profile LVGL Dashboard Entry Point
 */

#pragma once

#include "app_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Khởi tạo Dashboard UI theo profile của board hiện tại.
 * @note  Phải gọi TRONG vùng hal_display_lock() / hal_display_unlock().
 */
void ui_dashboard_init(void);

/**
 * @brief Cập nhật toàn bộ UI với dữ liệu mới nhất.
 * @note  Phải gọi TRONG vùng hal_display_lock() / hal_display_unlock().
 */
void ui_dashboard_update(const SensorSnapshot *snap);

#ifdef __cplusplus
}
#endif
