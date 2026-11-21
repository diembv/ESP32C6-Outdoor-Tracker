/**
 * @file route_nav.cpp
 * @brief Implementation of Sliding Window GPX/Binary Route Navigation Engine
 */

#include "route_nav.h"
#include "boards/board_select.h"
#include <Arduino.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Bán kính hiển thị tương ứng với từng mức zoom (mét)
static const float ZOOM_RADII[ZOOM_COUNT] = {
    100.0f,   // ZOOM_100M
    250.0f,   // ZOOM_250M
    500.0f,   // ZOOM_500M
    1000.0f,  // ZOOM_1000M
    2500.0f   // ZOOM_2500M
};

// ── State variables ──────────────────────────────────────────────────────────
static RouteHeader    s_header;
static RouteNavStatus s_status;
static char           s_filepath[128] = {0};

// Static RAM buffer cho Sliding Window (120 điểm * 8 bytes = 960 bytes)
static RoutePoint     s_window_buf[ROUTE_WINDOW_SIZE];
static uint32_t       s_window_start = 0;
static uint32_t       s_window_len   = 0;

static double         s_last_user_lat = 0;
static double         s_last_user_lon = 0;
static uint32_t       s_last_update_ms = 0;

// ── Helper Math Functions ───────────────────────────────────────────────────

static inline float distance_meters(double lat1, double lon1, double lat2, double lon2) {
    double mid_lat = (lat1 + lat2) * 0.5 * (M_PI / 180.0);
    double kx = 111320.0 * cos(mid_lat);
    double ky = 111320.0;
    double dx = (lon2 - lon1) * kx;
    double dy = (lat2 - lat1) * ky;
    return (float)sqrt(dx * dx + dy * dy);
}

static float distance_to_segment_meters(double px, double py,
                                        double x1, double y1,
                                        double x2, double y2) {
    double mid_lat = (py + y1 + y2) / 3.0 * (M_PI / 180.0);
    double kx = 111320.0 * cos(mid_lat);
    double ky = 111320.0;

    double p_x = px * kx, p_y = py * ky;
    double a_x = x1 * kx, a_y = y1 * ky;
    double b_x = x2 * kx, b_y = y2 * ky;

    double ab_x = b_x - a_x;
    double ab_y = b_y - a_y;
    double ab_len_sq = ab_x * ab_x + ab_y * ab_y;

    if (ab_len_sq < 1e-6) {
        double d_x = p_x - a_x;
        double d_y = p_y - a_y;
        return (float)sqrt(d_x * d_x + d_y * d_y);
    }

    double ap_x = p_x - a_x;
    double ap_y = p_y - a_y;
    double t = (ap_x * ab_x + ap_y * ab_y) / ab_len_sq;
    if (t < 0.0) t = 0.0;
    else if (t > 1.0) t = 1.0;

    double proj_x = a_x + t * ab_x;
    double proj_y = a_y + t * ab_y;
    double dx = p_x - proj_x;
    double dy = p_y - proj_y;
    return (float)sqrt(dx * dx + dy * dy);
}

// ── Nạp dữ liệu một cửa sổ từ thẻ MicroSD ─────────────────────────────────────

static bool read_window_from_sd(uint32_t start_idx, uint32_t count) {
    if (!s_status.is_active || s_filepath[0] == '\0') return false;
    if (start_idx >= s_header.point_count) return false;

    if (start_idx + count > s_header.point_count) {
        count = s_header.point_count - start_idx;
    }
    if (count > ROUTE_WINDOW_SIZE) {
        count = ROUTE_WINDOW_SIZE;
    }

    FILE *fp = fopen(s_filepath, "rb");
    if (!fp) {
        Serial.printf("[ROUTE] Khong the mo file: %s\n", s_filepath);
        return false;
    }

    long file_pos = (long)(sizeof(RouteHeader) + (size_t)start_idx * sizeof(RoutePoint));
    if (fseek(fp, file_pos, SEEK_SET) != 0) {
        fclose(fp);
        return false;
    }

    size_t read_bytes = fread(s_window_buf, sizeof(RoutePoint), count, fp);
    fclose(fp);

    if (read_bytes != count) {
        Serial.printf("[ROUTE] Doc khong du byte: read=%u, expected=%u\n", (unsigned)read_bytes, (unsigned)count);
        return false;
    }

    s_window_start = start_idx;
    s_window_len   = count;
    s_status.window_points    = s_window_buf;
    s_status.window_count     = s_window_len;
    s_status.window_start_idx = s_window_start;
    return true;
}

// ── Public API Implementation ───────────────────────────────────────────────

void route_nav_init(void) {
    memset(&s_header, 0, sizeof(s_header));
    memset(&s_status, 0, sizeof(s_status));
    s_status.zoom_level    = ZOOM_250M; // Mặc định 250m
    s_status.zoom_radius_m = ZOOM_RADII[ZOOM_250M];
    s_status.is_active     = false;
    s_window_len           = 0;
    s_window_start         = 0;
}

void route_nav_unload(void) {
    s_status.is_active = false;
    s_filepath[0]      = '\0';
    s_window_len       = 0;
    s_window_start     = 0;
    s_status.window_count = 0;
    s_status.window_points = nullptr;
    memset(&s_header, 0, sizeof(s_header));
    Serial.println(F("[ROUTE] Da unload route"));
}

static bool load_bin_file(const char *filepath) {
    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        Serial.printf("[ROUTE] File khong ton tai: %s\n", filepath);
        return false;
    }

    RouteHeader hdr;
    size_t n = fread(&hdr, 1, sizeof(RouteHeader), fp);
    fclose(fp);

    if (n != sizeof(RouteHeader)) {
        Serial.printf("[ROUTE] File qua ngan de chua header (read %u bytes)\n", (unsigned)n);
        return false;
    }

    if (memcmp(hdr.magic, ROUTE_HEADER_MAGIC, 4) != 0) {
        Serial.printf("[ROUTE] Magic khong hop le: %c%c%c%c\n",
                      hdr.magic[0], hdr.magic[1], hdr.magic[2], hdr.magic[3]);
        return false;
    }

    if (hdr.point_count == 0) {
        Serial.println(F("[ROUTE] Route khong co diem nao!"));
        return false;
    }

    memcpy(&s_header, &hdr, sizeof(RouteHeader));
    strncpy(s_filepath, filepath, sizeof(s_filepath) - 1);
    s_filepath[sizeof(s_filepath) - 1] = '\0';

    s_status.is_active          = true;
    strncpy(s_status.route_name, s_header.name, sizeof(s_status.route_name) - 1);
    s_status.route_name[sizeof(s_status.route_name) - 1] = '\0';
    s_status.total_dist_km      = s_header.total_dist_km;
    s_status.dist_remaining_km  = s_header.total_dist_km;
    s_status.cross_track_err_m  = 0.0f;
    s_status.is_off_route       = false;
    s_status.closest_idx        = 0;
    s_status.total_points       = s_header.point_count;

    // Nạp window đầu tiên (0 .. ROUTE_WINDOW_SIZE)
    uint32_t initial_count = (s_header.point_count < ROUTE_WINDOW_SIZE) ? s_header.point_count : ROUTE_WINDOW_SIZE;
    read_window_from_sd(0, initial_count);

    Serial.printf("[ROUTE] >> NAP ROUTE THANH CONG: '%s' (%u diem, %.2f km)\n",
                  s_status.route_name, (unsigned)s_header.point_count, s_header.total_dist_km);
    return true;
}

static bool convert_csv_to_tmp_bin(const char *csv_path, const char *out_bin_path) {
    FILE *in = fopen(csv_path, "r");
    if (!in) {
        Serial.printf("[ROUTE] Khong mo duoc CSV: %s\n", csv_path);
        return false;
    }

    mkdir("/sdcard/routes", 0755);
    FILE *out = fopen(out_bin_path, "wb");
    if (!out) {
        fclose(in);
        Serial.printf("[ROUTE] Khong tao duoc tmp bin: %s\n", out_bin_path);
        return false;
    }

    RouteHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    memcpy(hdr.magic, ROUTE_HEADER_MAGIC, 4);
    hdr.version = 1;
    hdr.min_lat = 90.0f;
    hdr.max_lat = -90.0f;
    hdr.min_lon = 180.0f;
    hdr.max_lon = -180.0f;

    // Derive display name from CSV filename (without directory and .csv)
    const char *base = strrchr(csv_path, '/');
    base = base ? (base + 1) : csv_path;
    strncpy(hdr.name, base, sizeof(hdr.name) - 1);
    hdr.name[sizeof(hdr.name) - 1] = '\0';
    size_t nlen = strlen(hdr.name);
    if (nlen > 4 && strcasecmp(hdr.name + nlen - 4, ".csv") == 0) {
        hdr.name[nlen - 4] = '\0';
    }

    // Write placeholder header
    fwrite(&hdr, sizeof(RouteHeader), 1, out);

    char line[128];
    uint32_t count = 0;
    float total_km = 0.0f;
    double prev_lat = 0.0, prev_lon = 0.0;
    bool has_prev = false;

    while (fgets(line, sizeof(line), in) != NULL) {
        // Parse: ts,lat,lon,alt
        char *p1 = strchr(line, ',');
        if (!p1) continue;
        double lat = strtod(p1 + 1, &p1);
        if (*p1 != ',') continue;
        double lon = strtod(p1 + 1, NULL);
        if (lat == 0.0 && lon == 0.0) continue;
        if (lat < -90.0 || lat > 90.0 || lon < -180.0 || lon > 180.0) continue;

        if (has_prev) {
            float d_m = distance_meters(prev_lat, prev_lon, lat, lon);
            if (d_m < 5.0f) continue; // Downsample dense 1Hz points (<5m)
            total_km += d_m / 1000.0f;
        }

        RoutePoint pt = { (float)lat, (float)lon };
        fwrite(&pt, sizeof(RoutePoint), 1, out);
        if (pt.lat < hdr.min_lat) hdr.min_lat = pt.lat;
        if (pt.lat > hdr.max_lat) hdr.max_lat = pt.lat;
        if (pt.lon < hdr.min_lon) hdr.min_lon = pt.lon;
        if (pt.lon > hdr.max_lon) hdr.max_lon = pt.lon;

        prev_lat = lat;
        prev_lon = lon;
        has_prev = true;
        count++;
    }
    fclose(in);

    if (count < 2) {
        fclose(out);
        unlink(out_bin_path);
        Serial.println(F("[ROUTE] CSV khong du diem GPS hop le (<2)!"));
        return false;
    }

    hdr.point_count   = count;
    hdr.total_dist_km = total_km;
    fseek(out, 0, SEEK_SET);
    fwrite(&hdr, sizeof(RouteHeader), 1, out);
    fclose(out);

    Serial.printf("[ROUTE] Chuyen CSV -> %s (%u diem, %.2f km)\n", out_bin_path, (unsigned)count, total_km);
    return true;
}

bool route_nav_load_file(const char *filepath) {
    if (!filepath || strlen(filepath) == 0) return false;

    size_t len = strlen(filepath);
    if (len > 4 && strcasecmp(filepath + len - 4, ".csv") == 0) {
        const char *tmp_bin = "/sdcard/routes/_tmp_log.bin";
        if (!convert_csv_to_tmp_bin(filepath, tmp_bin)) {
            return false;
        }
        return load_bin_file(tmp_bin);
    }

    return load_bin_file(filepath);
}

bool route_nav_autoload(void) {
    // 1. Ưu tiên /sdcard/routes/active.bin
    const char *primary = "/sdcard/routes/active.bin";
    if (access(primary, F_OK) == 0) {
        return route_nav_load_file(primary);
    }

    // 2. Quét thư mục /sdcard/routes tìm file .bin đầu tiên
    DIR *dir = opendir("/sdcard/routes");
    if (!dir) {
        // Thử tạo thư mục nếu chưa có
        mkdir("/sdcard/routes", 0755);
        return false;
    }

    struct dirent *ent;
    char found_file[128] = {0};
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        size_t len = strlen(ent->d_name);
        if (len > 4 && strcmp(&ent->d_name[len - 4], ".bin") == 0) {
            snprintf(found_file, sizeof(found_file), "/sdcard/routes/%s", ent->d_name);
            break;
        }
    }
    closedir(dir);

    if (found_file[0] != '\0') {
        return route_nav_load_file(found_file);
    }
    return false;
}

void route_nav_update(double lat, double lon, float speed_kmh) {
    if (!s_status.is_active || s_window_len == 0) return;

    // Giới hạn tần suất tính toán: 1 lần mỗi 500ms
    uint32_t now = millis();
    if (now - s_last_update_ms < 400) return;
    s_last_update_ms = now;

    s_last_user_lat = lat;
    s_last_user_lon = lon;

    // 1. Tìm điểm gần nhất trong window hiện tại
    float min_dist = 999999.0f;
    uint32_t best_win_idx = 0;

    for (uint32_t i = 0; i < s_window_len; i++) {
        float d = distance_meters(lat, lon, s_window_buf[i].lat, s_window_buf[i].lon);
        if (d < min_dist) {
            min_dist = d;
            best_win_idx = i;
        }
    }

    uint32_t global_closest = s_window_start + best_win_idx;
    s_status.closest_idx = global_closest;

    // 2. Tính Cross-Track Error (XTE) đến đoạn thẳng gần nhất
    float xte = min_dist;
    if (best_win_idx + 1 < s_window_len) {
        xte = distance_to_segment_meters(lon, lat,
                                         s_window_buf[best_win_idx].lon, s_window_buf[best_win_idx].lat,
                                         s_window_buf[best_win_idx + 1].lon, s_window_buf[best_win_idx + 1].lat);
    } else if (best_win_idx > 0) {
        xte = distance_to_segment_meters(lon, lat,
                                         s_window_buf[best_win_idx - 1].lon, s_window_buf[best_win_idx - 1].lat,
                                         s_window_buf[best_win_idx].lon, s_window_buf[best_win_idx].lat);
    }

    s_status.cross_track_err_m = xte;
    s_status.is_off_route      = (xte > 35.0f); // Lệch quá 35m coi như chệch hướng

    // 3. Ước lượng cự ly còn lại tới đích (O(1) không tốn CPU)
    if (s_header.point_count > 1) {
        float frac_remaining = 1.0f - ((float)global_closest / (float)(s_header.point_count - 1));
        if (frac_remaining < 0.0f) frac_remaining = 0.0f;
        s_status.dist_remaining_km = s_header.total_dist_km * frac_remaining;
    }

    // 4. Trượt cửa sổ (Sliding Window Shift) nếu tiến gần mép cửa sổ
    // Nếu điểm gần nhất cách mép trước < 20 hoặc mép sau < 20
    const uint32_t MARGIN = 20;
    bool need_shift = false;
    uint32_t new_start = s_window_start;

    if (best_win_idx > (s_window_len - MARGIN) && (s_window_start + s_window_len) < s_header.point_count) {
        // Đang đi về phía cuối window -> trượt tới
        new_start = (global_closest >= 30) ? (global_closest - 30) : 0;
        need_shift = true;
    } else if (best_win_idx < MARGIN && s_window_start > 0) {
        // Đang quay lui về phía đầu window -> trượt lui
        new_start = (global_closest >= 30) ? (global_closest - 30) : 0;
        need_shift = true;
    }

    if (need_shift && new_start != s_window_start) {
        read_window_from_sd(new_start, ROUTE_WINDOW_SIZE);
    }
}

void route_nav_cycle_zoom(void) {
    uint8_t next = (uint8_t)s_status.zoom_level + 1;
    if (next >= ZOOM_COUNT) next = 0;
    route_nav_set_zoom((MapZoomLevel)next);
}

void route_nav_set_zoom(MapZoomLevel level) {
    if (level >= ZOOM_COUNT) level = ZOOM_250M;
    s_status.zoom_level    = level;
    s_status.zoom_radius_m = ZOOM_RADII[level];
    Serial.printf("[ROUTE] Zoom doi sang: %.0fm\n", s_status.zoom_radius_m);
}

const RouteNavStatus *route_nav_get_status(void) {
    return &s_status;
}

void route_nav_remove_demo(void) {
    DIR *dir = opendir("/sdcard/routes");
    if (!dir) return;

    struct dirent *ent;
    char fullpath[160];
    while ((ent = readdir(dir)) != NULL) {
        if (ent->d_name[0] == '.') continue;
        size_t len = strlen(ent->d_name);
        if (len <= 4 || strcmp(&ent->d_name[len - 4], ".bin") != 0) continue;

        snprintf(fullpath, sizeof(fullpath), "/sdcard/routes/%s", ent->d_name);
        FILE *fp = fopen(fullpath, "rb");
        if (!fp) continue;
        RouteHeader hdr;
        size_t n = fread(&hdr, 1, sizeof(RouteHeader), fp);
        fclose(fp);

        if (n == sizeof(RouteHeader) &&
            memcmp(hdr.magic, ROUTE_HEADER_MAGIC, 4) == 0 &&
            strstr(hdr.name, "Ba Vi") != NULL) {
            if (unlink(fullpath) == 0) {
                Serial.printf("[ROUTE] Da xoa route demo: %s (%s)\n", fullpath, hdr.name);
            }
        }
    }
    closedir(dir);
}

int route_nav_list_routes(RouteListEntry *out_list, int max_count) {
    if (!out_list || max_count <= 0) return 0;

    DIR *dir = opendir("/sdcard/routes");
    if (!dir) {
        mkdir("/sdcard/routes", 0755);
        return 0;
    }

    int count = 0;
    struct dirent *ent;
    char fullpath[160];
    while ((ent = readdir(dir)) != NULL && count < max_count) {
        if (ent->d_name[0] == '.' || ent->d_name[0] == '_') continue;
        size_t len = strlen(ent->d_name);
        if (len <= 4 || strcasecmp(&ent->d_name[len - 4], ".bin") != 0) continue;

        snprintf(fullpath, sizeof(fullpath), "/sdcard/routes/%s", ent->d_name);
        FILE *fp = fopen(fullpath, "rb");
        if (!fp) continue;
        RouteHeader hdr;
        size_t n = fread(&hdr, 1, sizeof(RouteHeader), fp);
        fclose(fp);

        if (n != sizeof(RouteHeader) || memcmp(hdr.magic, ROUTE_HEADER_MAGIC, 4) != 0) {
            continue;
        }

        strncpy(out_list[count].path, fullpath, sizeof(out_list[count].path) - 1);
        out_list[count].path[sizeof(out_list[count].path) - 1] = '\0';

        struct stat st;
        out_list[count].size_bytes = 0;
        out_list[count].mtime      = 0;
        if (stat(fullpath, &st) == 0) {
            out_list[count].size_bytes = (uint32_t)st.st_size;
            if (st.st_mtime > 0) out_list[count].mtime = (uint32_t)st.st_mtime;
        }

        if (hdr.name[0] != '\0') {
            strncpy(out_list[count].name, hdr.name, sizeof(out_list[count].name) - 1);
            out_list[count].name[sizeof(out_list[count].name) - 1] = '\0';
        } else {
            strncpy(out_list[count].name, ent->d_name, sizeof(out_list[count].name) - 1);
            out_list[count].name[sizeof(out_list[count].name) - 1] = '\0';
        }
        count++;
    }
    closedir(dir);

    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            bool swap_it = (out_list[j].mtime > out_list[i].mtime) ||
                           (out_list[j].mtime == out_list[i].mtime &&
                            strcmp(out_list[j].name, out_list[i].name) > 0);
            if (swap_it) {
                RouteListEntry tmp = out_list[i];
                out_list[i] = out_list[j];
                out_list[j] = tmp;
            }
        }
    }
    return count;
}

int route_nav_list_logs(RouteListEntry *out_list, int max_count) {
    if (!out_list || max_count <= 0) return 0;

    DIR *dir = opendir("/sdcard/logs");
    if (!dir) return 0;

    int count = 0;
    struct dirent *ent;
    while ((ent = readdir(dir)) != NULL && count < max_count) {
        if (ent->d_name[0] == '.') continue;
        size_t len = strlen(ent->d_name);
        if (len <= 4 || strcasecmp(&ent->d_name[len - 4], ".csv") != 0) continue;

        snprintf(out_list[count].path, sizeof(out_list[count].path), "/sdcard/logs/%s", ent->d_name);
        struct stat st;
        out_list[count].size_bytes = 0;
        out_list[count].mtime      = 0;
        if (stat(out_list[count].path, &st) == 0) {
            out_list[count].size_bytes = (uint32_t)st.st_size;
            if (st.st_mtime > 0) out_list[count].mtime = (uint32_t)st.st_mtime;
        }

        strncpy(out_list[count].name, ent->d_name, sizeof(out_list[count].name) - 1);
        out_list[count].name[sizeof(out_list[count].name) - 1] = '\0';
        size_t nlen = strlen(out_list[count].name);
        if (nlen > 4 && strcasecmp(out_list[count].name + nlen - 4, ".csv") == 0) {
            out_list[count].name[nlen - 4] = '\0';
        }
        count++;
    }
    closedir(dir);

    for (int i = 0; i < count - 1; i++) {
        for (int j = i + 1; j < count; j++) {
            bool swap_it = (out_list[j].mtime > out_list[i].mtime) ||
                           (out_list[j].mtime == out_list[i].mtime &&
                            strcmp(out_list[j].name, out_list[i].name) > 0);
            if (swap_it) {
                RouteListEntry tmp = out_list[i];
                out_list[i] = out_list[j];
                out_list[j] = tmp;
            }
        }
    }
    return count;
}

bool route_nav_project_to_screen(
    float pt_lat, float pt_lon,
    double center_lat, double center_lon,
    int16_t cx, int16_t cy,
    float radius_m,
    float viewport_radius_px,
    int16_t *out_x, int16_t *out_y
) {
    if (radius_m <= 0.0f || viewport_radius_px <= 0.0f) return false;

    double mid_lat = center_lat * (M_PI / 180.0);
    double kx = 111320.0 * cos(mid_lat);
    double ky = 111320.0;

    double dx_m = (pt_lon - center_lon) * kx;
    double dy_m = (pt_lat - center_lat) * ky;

    float scale = viewport_radius_px / radius_m;

    int32_t px = cx + (int32_t)(dx_m * scale);
    int32_t py = cy - (int32_t)(dy_m * scale); // Trục Y màn hình hướng xuống dưới

    *out_x = (int16_t)px;
    *out_y = (int16_t)py;

    // Kiểm tra điểm có nằm trong phạm vi vẽ an toàn (có lề 100px ngoài màn hình)
    return (px >= -100 && px <= (BOARD_DISP_WIDTH + 100) && py >= -100 && py <= (BOARD_DISP_HEIGHT + 100));
}
