#pragma once

#include <Arduino.h>
#include "ui_dashboard.h"

#ifdef __cplusplus
extern "C" {
#endif

// Khởi tạo thẻ nhớ SD (giao tiếp SPI2_HOST, CS=GPIO15)
bool sd_logger_init(SensorSnapshot *snap);

// Ghi log dữ liệu tọa độ định dạng Compact CSV siêu nhẹ (ts,lat,lon,alt)
void sd_logger_log(SensorSnapshot *snap);

// Trả về trạng thái thẻ nhớ
bool sd_logger_is_ok(void);

// Đóng/flush file an toàn trước khi tắt máy hoặc rút thẻ
void sd_logger_flush(void);

// Bật/tắt ghi log, tạo file CSV mới hoặc chốt đóng file
void sd_logger_toggle(SensorSnapshot *snap);

// Lấy thông tin thống kê ghi log (số điểm đã ghi, dung lượng RAM buffer 1024B)
void sd_logger_get_stats(uint32_t *points_logged, size_t *buffer_usage);

// Lấy thông tin tổng kết hành trình (quãng đường mét, thời gian giây)
void sd_logger_get_summary(float *distance_m, uint32_t *duration_s);

#ifdef __cplusplus
}
#endif
