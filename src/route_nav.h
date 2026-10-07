/**
 * @file route_nav.h
 * @brief High-efficiency Sliding Window GPX/Binary Route Navigation Engine
 * 
 * Thiết kế chuyên biệt cho ESP32-C6 (512 KB SRAM, không có PSRAM ngoài):
 *  - Bộ đệm tĩnh trong RAM chỉ 120 điểm (~960 bytes)
 *  - fseek cuộn trực tiếp từ MicroSD (SPI2_HOST)
 *  - Thuật toán tính Cross-Track Error (XTE) và khoảng cách tới đích
 *  - Chiếu tọa độ Equirectangular sang màn hình AMOLED 280x456
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ROUTE_HEADER_MAGIC "ROUT"
#define ROUTE_WINDOW_SIZE  120

#pragma pack(push, 1)
typedef struct {
    char     magic[4];       // "ROUT"
    uint16_t version;        // 1
    uint16_t reserved1;      // 0
    uint32_t point_count;    // Tổng số điểm trong file
    float    total_dist_km;  // Tổng chiều dài tuyến đường (km)
    float    min_lat;        // Bounds vĩ độ min
    float    max_lat;        // Bounds vĩ độ max
    float    min_lon;        // Bounds kinh độ min
    float    max_lon;        // Bounds kinh độ max
    char     name[32];       // Tên lộ trình (null-terminated)
    uint8_t  reserved[32];   // Dành cho mở rộng tương lai
} RouteHeader;

typedef struct {
    float lat;
    float lon;
} RoutePoint;
#pragma pack(pop)

typedef enum {
    ZOOM_100M  = 0, // Bán kính 100m (Chi tiết từng lối rẽ)
    ZOOM_250M  = 1, // Bán kính 250m (Mặc định cho đi bộ / trekking)
    ZOOM_500M  = 2, // Bán kính 500m (Phù hợp đạp xe)
    ZOOM_1000M = 3, // Bán kính 1 km (Bao quát khu vực)
    ZOOM_2500M = 4, // Bán kính 2.5 km (Toàn cảnh cung đường)
    ZOOM_COUNT = 5
} MapZoomLevel;

typedef struct {
    bool         is_active;          // true nếu đã nạp route thành công
    char         route_name[32];     // Tên lộ trình đang nạp
    float        total_dist_km;      // Tổng cự ly lộ trình (km)
    float        dist_remaining_km;  // Cự ly còn lại tới đích (km)
    float        cross_track_err_m;  // Độ lệch tim đường XTE (mét)
    bool         is_off_route;       // true nếu lệch > 30 mét
    uint32_t     closest_idx;        // Chỉ số điểm gần nhất trên route
    uint32_t     total_points;       // Tổng số điểm
    MapZoomLevel zoom_level;         // Mức zoom hiện tại
    float        zoom_radius_m;      // Bán kính hiển thị (mét)
    
    // Con trỏ tới Sliding Window buffer trong RAM (960 bytes)
    const RoutePoint *window_points;
    uint32_t     window_count;       // Số điểm trong window hiện tại
    uint32_t     window_start_idx;   // Vị trí bắt đầu của window trong file
} RouteNavStatus;

/**
 * @brief Khởi tạo module Navigation
 */
void route_nav_init(void);

/**
 * @brief Tự động tìm kiếm hoặc nạp lộ trình mặc định (/sdcard/routes/active.bin)
 */
bool route_nav_autoload(void);

/**
 * @brief Nạp file lộ trình nhị phân từ đường dẫn cụ thể trên thẻ MicroSD
 */
bool route_nav_load_file(const char *filepath);

/**
 * @brief Đóng lộ trình hiện tại và giải phóng bộ nhớ
 */
void route_nav_unload(void);

/**
 * @brief Cập nhật tọa độ GPS người dùng, tính XTE và cuộn Sliding Window từ thẻ nhớ
 */
void route_nav_update(double lat, double lon, float speed_kmh);

/**
 * @brief Chuyển đổi vòng lặp mức Zoom (100m -> 250m -> 500m -> 1km -> 2.5km)
 */
void route_nav_cycle_zoom(void);

/**
 * @brief Đặt mức Zoom cụ thể
 */
void route_nav_set_zoom(MapZoomLevel level);

/**
 * @brief Lấy trạng thái Navigation hiện tại
 */
const RouteNavStatus *route_nav_get_status(void);

/**
 * @brief Chiếu điểm (lat, lon) sang pixel màn hình (x, y) theo tâm người dùng
 */
bool route_nav_project_to_screen(
    float pt_lat, float pt_lon,
    double center_lat, double center_lon,
    int16_t cx, int16_t cy,
    float radius_m,
    int16_t *out_x, int16_t *out_y
);

#ifdef __cplusplus
}
#endif
