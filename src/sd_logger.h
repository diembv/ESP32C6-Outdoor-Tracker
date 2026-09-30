#pragma once

#include <Arduino.h>
#include "ui_dashboard.h"

#ifdef __cplusplus
extern "C" {
#endif

// Khởi tạo thẻ nhớ SD (giao tiếp SPI2_HOST, CS=GPIO15)
bool sd_logger_init(SensorSnapshot *snap);

// Ghi một dòng log CSV dữ liệu cảm biến
void sd_logger_log(SensorSnapshot *snap);

// Trả về trạng thái thẻ nhớ
bool sd_logger_is_ok(void);

// Đóng file an toàn trước khi tắt máy hoặc rút thẻ
void sd_logger_flush(void);

// Toggle logging state and file creation
void sd_logger_toggle(SensorSnapshot *snap);

// Lấy thông tin thống kê ghi log (số điểm đã ghi, dung lượng RAM buffer)
void sd_logger_get_stats(uint32_t *points_logged, size_t *buffer_usage);

#ifdef __cplusplus
}
#endif
