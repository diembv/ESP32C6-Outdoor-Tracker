#pragma once

#include <Arduino.h>
#include "ui_dashboard.h"

#ifdef __cplusplus
extern "C" {
#endif

// Khởi tạo thẻ nhớ SD (giao tiếp SPI2_HOST, CS=GPIO15)
bool sd_logger_init(SensorSnapshot *snap);

// Ghi một dòng log CSV dữ liệu cảm biến
void sd_logger_log(const SensorSnapshot *snap);

// Trả về trạng thái thẻ nhớ
bool sd_logger_is_ok(void);

// Đóng file an toàn trước khi tắt máy hoặc rút thẻ
void sd_logger_flush(void);

#ifdef __cplusplus
}
#endif
