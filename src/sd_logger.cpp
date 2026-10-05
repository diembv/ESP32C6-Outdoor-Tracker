#include "sd_logger.h"
#include <TinyGPS++.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/gpio.h"

#define SD_MOUNT_POINT   "/sdcard"
#define SD_LOG_DIR       "/sdcard/logs"
#define SD_CS_PIN        (gpio_num_t)15
#define SD_SPI_HOST      SPI2_HOST
#define LOG_BUFFER_SIZE  1024

static sdmmc_card_t *s_card = NULL;
static FILE *s_log_file = NULL;
static bool s_sd_ready = false;
static char s_filepath[128];

// Static RAM buffer cho Compact CSV logging
static char s_log_buffer[LOG_BUFFER_SIZE];
static size_t s_log_buffer_len = 0;
static uint32_t s_last_flush_time = 0;

// State Machine Tracking (Smart Logging)
static float s_last_heading = -1;
static float s_last_alt = 0;
static uint32_t s_last_sample_time = 0;
static uint32_t s_low_speed_start = 0;
static float s_session_distance = 0;
static double s_last_lat = 0;
static double s_last_lon = 0;
static uint32_t s_session_start_ms = 0;
static uint32_t s_points_logged = 0;

// Chuyển đổi GPS Date/Time UTC sang Unix Epoch Timestamp (giây từ 1970-01-01 00:00:00 UTC)
static uint32_t to_epoch_seconds(uint16_t year, uint8_t month, uint8_t day, uint8_t hour, uint8_t min, uint8_t sec) {
    if (year < 1970 || month < 1 || month > 12 || day < 1 || day > 31) return 0;
    static const uint16_t days_before_month[] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    uint32_t y = year - 1970;
    uint32_t leap_days = (year - 1969) / 4 - (year - 1901) / 100 + (year - 1601) / 400;
    uint32_t days = y * 365 + leap_days + days_before_month[month - 1] + (day - 1);
    bool is_leap = ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0));
    if (is_leap && month > 2) days++;
    return days * 86400UL + hour * 3600UL + min * 60UL + sec;
}

static void flush_buffer_to_sd() {
    if (s_log_file && s_log_buffer_len > 0) {
        size_t written = fwrite(s_log_buffer, 1, s_log_buffer_len, s_log_file);
        if (written != s_log_buffer_len) {
            Serial.printf("[SD] Canh bao: chi ghi duoc %u/%u byte\n",
                          (unsigned)written, (unsigned)s_log_buffer_len);
        }
        fflush(s_log_file);
        fsync(fileno(s_log_file));  // Ép FATFS ghi sector + cập nhật FAT xuống thẻ
        s_log_buffer_len = 0;
        s_last_flush_time = millis();
    }
}

bool sd_logger_init(SensorSnapshot *snap) {
    s_sd_ready = false;
    if (snap) {
        snap->sys.sd_present = false;
        snap->sys.sd_ok = false;
        snap->sys.sd_free_mb = 0;
        snap->sys.is_logging_active = false;
    }

    Serial.println(F("[SD] Khoi tao the nho MicroSD (SPI2, CS=GPIO15)..."));

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false
    };

    // Kéo pull-up cho các chân SPI
    gpio_set_pull_mode((gpio_num_t)4, GPIO_PULLUP_ONLY); // MOSI / D0
    gpio_set_pull_mode((gpio_num_t)5, GPIO_PULLUP_ONLY); // MISO / D1
    gpio_set_pull_mode((gpio_num_t)11, GPIO_PULLUP_ONLY); // CLK

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs   = SD_CS_PIN;
    slot_config.host_id   = SD_SPI_HOST;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = 4000; // 4MHz

    esp_err_t ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK) {
        Serial.printf("[SD] Mount that bai (ma loi: 0x%x).\n", ret);
        if (snap) {
            snap->sys.sd_present = false;
            snap->sys.sd_ok = false;
            snap->sys.sd_free_mb = 0; // Để UI hiển thị "No SD card" thân thiện
        }
        pinMode(15, OUTPUT);
        digitalWrite(15, HIGH);
        return false;
    }

    s_sd_ready = true;
    uint32_t capacity_mb = (uint32_t)((float)s_card->csd.capacity / 2048.0f);
    Serial.printf("[SD] Nhan dien the nho: %s | Dung luong: %lu MB\n", s_card->cid.name, (unsigned long)capacity_mb);

    if (snap) {
        snap->sys.sd_present = true;
        snap->sys.sd_ok = true;
        snap->sys.sd_free_mb = capacity_mb; 
    }

    struct stat st;
    if (stat(SD_LOG_DIR, &st) != 0) {
        Serial.println("[SD] Tao thu muc /logs");
        mkdir(SD_LOG_DIR, 0777);
    }

    return true;
}

void sd_logger_toggle(SensorSnapshot *snap) {
    if (!s_sd_ready || !snap) return;

    if (snap->sys.is_logging_active) {
        // DUNG GHI -> Flush buffer va Dong file an toan
        snap->sys.is_logging_active = false;
        if (s_log_file) {
            flush_buffer_to_sd(); // Ghi hết lượng byte còn lại trong RAM buffer
            fclose(s_log_file);
            s_log_file = NULL;
            Serial.println("[SD] Da DUNG ghi log va luu file CSV an toan.");
        }
    } else {
        // BAT DAU GHI -> Tao file CSV va Ghi Header
        if (snap->gps.date_valid && snap->gps.time_valid) {
            snprintf(s_filepath, sizeof(s_filepath), SD_LOG_DIR "/%04u%02u%02u_%02u%02u%02u.csv",
                     snap->gps.year, snap->gps.month, snap->gps.day,
                     snap->gps.hour, snap->gps.minute, snap->gps.second);
        } else {
            int file_idx = 1;
            while (file_idx < 1000) {
                snprintf(s_filepath, sizeof(s_filepath), SD_LOG_DIR "/track_%03d.csv", file_idx);
                struct stat st;
                if (stat(s_filepath, &st) != 0) {
                    break;
                }
                file_idx++;
            }
        }

        s_log_file = fopen(s_filepath, "w");
        if (!s_log_file) {
            Serial.printf("[SD] Loi tao file CSV moi: %s\n", s_filepath);
            return;
        }

        // Ghi CSV Header siêu gọn: "ts,lat,lon,alt\n"
        const char *header = "ts,lat,lon,alt\n";
        fwrite(header, 1, strlen(header), s_log_file);
        fflush(s_log_file);
        fsync(fileno(s_log_file));

        s_log_buffer_len = 0;
        s_points_logged = 0;
        s_last_flush_time = millis();
        s_last_heading = -1;
        s_session_distance = 0;
        s_last_lat = 0;
        s_last_lon = 0;
        s_session_start_ms = millis();

        Serial.printf("[SD] BAT DAU ghi log CSV (Compact) vao: %s\n", s_filepath);
        snap->sys.is_logging_active = true;
    }
}

void sd_logger_log(SensorSnapshot *snap) {
    if (!s_sd_ready || !s_log_file || !snap) return;

    uint32_t now = millis();

    // Lưu ý: Không tự ý ngắt ghi để cho phép cấp nguồn qua USB / sạc dự phòng khi chưa gắn pin Li-Po
    static uint32_t last_bat_warn = 0;
    if (snap->sys.is_logging_active && snap->sys.battery_v > 0.5f && snap->sys.battery_v < 3.30f) {
        if (now - last_bat_warn >= 30000) {
            last_bat_warn = now;
            Serial.printf("[SD] Dien ap nguon: %.2fV (Nguon USB hoac pin yeu)\n", snap->sys.battery_v);
        }
    }

    if (!snap->sys.is_logging_active || !snap->gps.fix_valid) {
        // Flush buffer nếu còn data tồn đọng quá 60s
        if (s_log_buffer_len > 0 && now - s_last_flush_time >= 60000) {
            flush_buffer_to_sd();
        }
        return;
    }

    float v = snap->gps.speed_kmh;
    float heading = snap->gps.course_deg;

    // Chọn nguồn cao độ đã được hiệu chuẩn (BMP580 + GPS)
    bool  has_ele = false;
    float alt = 0.0f;
    if (snap->baro.valid && snap->baro.calibrated) {
        alt = snap->baro.altitude_m;
        has_ele = true;
    } else if (snap->gps.altitude_valid && snap->gps.satellites >= 4) {
        alt = snap->gps.altitude_m;
        has_ele = true;
    } else if (snap->baro.valid) {
        alt = snap->baro.altitude_m;
        has_ele = true;
    } else if (snap->gps.altitude_valid) {
        alt = snap->gps.altitude_m;
        has_ele = true;
    }

    if (snap->gps.fix_valid) {
        if (s_last_lat != 0 && s_last_lon != 0) {
            float dist = TinyGPSPlus::distanceBetween(s_last_lat, s_last_lon, snap->gps.latitude, snap->gps.longitude);
            if (dist > 1.0f) {
                s_session_distance += dist;
                s_last_lat = snap->gps.latitude;
                s_last_lon = snap->gps.longitude;
            }
        } else {
            s_last_lat = snap->gps.latitude;
            s_last_lon = snap->gps.longitude;
        }
    }

    // Theo dõi trạng thái dừng / đứng yên (standby)
    if (v < 1.0f) {
        if (s_low_speed_start == 0) s_low_speed_start = now;
    } else {
        s_low_speed_start = 0;
    }

    bool is_standby = (s_low_speed_start != 0 && (now - s_low_speed_start) > 10000);

    float delta_heading = 0;
    if (s_last_heading >= 0) {
        delta_heading = abs(heading - s_last_heading);
        if (delta_heading > 180.0f) delta_heading = 360.0f - delta_heading;
    }
    float delta_alt = has_ele ? fabsf(alt - s_last_alt) : 0.0f;

    bool high_maneuver = (delta_heading >= 15.0f || delta_alt >= 3.0f);

    uint32_t sample_interval = 3000; // Mặc định chuyển động đều: 3s
    if (high_maneuver) {
        sample_interval = 1000;      // Khúc cua / đổi cao độ mạnh: 1s
    } else if (is_standby) {
        sample_interval = 10000;     // Đứng yên: 10s
    }

    if (s_last_heading >= 0 && (now - s_last_sample_time < sample_interval)) {
        // Kiểm tra flush định kỳ 60s
        if (now - s_last_flush_time >= 60000) flush_buffer_to_sd();
        return; 
    }

    // --- Record Sample ---
    s_last_heading = heading;
    if (has_ele) s_last_alt = alt;
    s_last_sample_time = now;
    s_points_logged++;

    // Tính Unix Epoch Timestamp (giây từ 1970 UTC)
    uint32_t epoch_time = 0;
    if (snap->gps.date_valid && snap->gps.time_valid) {
        epoch_time = to_epoch_seconds(snap->gps.year, snap->gps.month, snap->gps.day,
                                      snap->gps.hour, snap->gps.minute, snap->gps.second);
    }
    if (epoch_time == 0) {
        time_t t_now = time(NULL);
        if (t_now > 1672531199) { // >= 2023-01-01
            epoch_time = (uint32_t)t_now;
        }
    }

    // Format bản ghi Compact CSV (~32-35 bytes): ts,lat,lon,alt
    char line_buf[64];
    int len = snprintf(line_buf, sizeof(line_buf), "%lu,%.6f,%.6f,%.1f\n",
                       (unsigned long)epoch_time,
                       snap->gps.latitude, snap->gps.longitude, 
                       alt);

    if (len > 0 && len < (int)sizeof(line_buf)) {
        // Nếu bộ đệm RAM sắp đầy (>= 1024B), flush khối trước
        if (s_log_buffer_len + (size_t)len >= LOG_BUFFER_SIZE) {
            flush_buffer_to_sd();
        }

        // Thêm bản ghi vào bộ đệm RAM
        if (s_log_buffer_len + (size_t)len < LOG_BUFFER_SIZE) {
            memcpy(s_log_buffer + s_log_buffer_len, line_buf, (size_t)len);
            s_log_buffer_len += (size_t)len;
        }
    }

    // Chỉ kích hoạt ghi khối (fwrite) và chốt dữ liệu (fflush/fsync) khi:
    //  + Bộ đệm RAM tích lũy >= 512 bytes (chuẩn 1 sector FATFS)
    //  + Hoặc bộ đếm thời gian chờ đạt 60 giây
    if (s_log_buffer_len >= 512 || (now - s_last_flush_time >= 60000)) {
        flush_buffer_to_sd();
    }
}

bool sd_logger_is_ok(void) {
    return s_sd_ready;
}

void sd_logger_flush(void) {
    if (s_log_file) {
        flush_buffer_to_sd();
    }
}

void sd_logger_get_stats(uint32_t *points_logged, size_t *buffer_usage) {
    if (points_logged) *points_logged = s_points_logged;
    if (buffer_usage) *buffer_usage = s_log_buffer_len;
}

void sd_logger_get_summary(float *distance_m, uint32_t *duration_s) {
    if (distance_m) *distance_m = s_session_distance;
    if (duration_s) *duration_s = s_session_start_ms > 0 ? (millis() - s_session_start_ms) / 1000 : 0;
}
