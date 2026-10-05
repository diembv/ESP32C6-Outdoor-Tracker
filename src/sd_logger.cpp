#include "sd_logger.h"
#include <TinyGPS++.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/gpio.h"

#define SD_MOUNT_POINT "/sdcard"
#define SD_LOG_DIR     "/sdcard/logs"
#define SD_CS_PIN      (gpio_num_t)15
#define SD_SPI_HOST    SPI2_HOST
#define LOG_BUFFER_SIZE 1024

static sdmmc_card_t *s_card = NULL;
static FILE *s_log_file = NULL;
static bool s_sd_ready = false;
static char s_filepath[128];

// Static RAM buffer for logging
static char s_log_buffer[LOG_BUFFER_SIZE];
static size_t s_log_buffer_len = 0;
static uint32_t s_last_flush_time = 0;

// State Machine Tracking
static float s_last_heading = -1;
static float s_last_alt = 0;
static uint32_t s_last_sample_time = 0;
static uint32_t s_low_speed_start = 0;
static float s_session_distance = 0;
static double s_last_lat = 0;
static double s_last_lon = 0;
static uint32_t s_session_start_ms = 0;
static uint32_t s_points_logged = 0;

static void flush_buffer_to_sd() {
    if (s_log_file && s_log_buffer_len > 0) {
        fwrite(s_log_buffer, 1, s_log_buffer_len, s_log_file);
        fflush(s_log_file);
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

    // Keo pull-up cho cac chan SPI
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
        // DUNG GHI -> Ghi dong The Tag va Close
        snap->sys.is_logging_active = false;
        if (s_log_file) {
            flush_buffer_to_sd(); // Ghi not buffer
            
            const char* footer = "    </trkseg>\n  </trk>\n</gpx>\n";
            fwrite(footer, 1, strlen(footer), s_log_file);
            fflush(s_log_file);
            fclose(s_log_file);
            
            s_log_file = NULL;
            Serial.println("[SD] Da DUNG ghi log va luu file GPX an toan.");
        }
    } else {
        // BAT DAU GHI -> Tao file GPX va Ghi Header
        if (snap->gps.date_valid && snap->gps.time_valid) {
            snprintf(s_filepath, sizeof(s_filepath), SD_LOG_DIR "/%04u%02u%02u_%02u%02u%02u.gpx",
                     snap->gps.year, snap->gps.month, snap->gps.day,
                     snap->gps.hour, snap->gps.minute, snap->gps.second);
        } else {
            int file_idx = 1;
            while (file_idx < 1000) {
                snprintf(s_filepath, sizeof(s_filepath), SD_LOG_DIR "/track_%03d.gpx", file_idx);
                struct stat st;
                if (stat(s_filepath, &st) != 0) {
                    break;
                }
                file_idx++;
            }
        }

        s_log_file = fopen(s_filepath, "w");
        if (!s_log_file) {
            Serial.printf("[SD] Loi tao file moi: %s\n", s_filepath);
            return;
        }

        // Ghi GPX Header
        char meta_time[64] = "";
        if (snap->gps.date_valid && snap->gps.time_valid) {
            snprintf(meta_time, sizeof(meta_time), "  <metadata><time>%04u-%02u-%02uT%02u:%02u:%02uZ</time></metadata>\n",
                     snap->gps.year, snap->gps.month, snap->gps.day,
                     snap->gps.hour, snap->gps.minute, snap->gps.second);
        }
        char header[384];
        snprintf(header, sizeof(header), 
            "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
            "<gpx version=\"1.1\" creator=\"ESP32-C6 Outdoor Tracker\" xmlns=\"http://www.topografix.com/GPX/1/1\">\n"
            "%s"
            "  <trk>\n"
            "    <name>Track %04u%02u%02u</name>\n"
            "    <trkseg>\n", 
            meta_time, snap->gps.year, snap->gps.month, snap->gps.day);
            
        fwrite(header, 1, strlen(header), s_log_file);
        fflush(s_log_file);
        
        s_log_buffer_len = 0;
        s_points_logged = 0;
        s_last_flush_time = millis();
        s_last_heading = -1;
        s_session_distance = 0;
        s_last_lat = 0;
        s_last_lon = 0;
        s_session_start_ms = millis();
        
        Serial.printf("[SD] BAT DAU ghi log GPX vao: %s\n", s_filepath);
        snap->sys.is_logging_active = true;
    }
}

void sd_logger_log(SensorSnapshot *snap) {
    if (!s_sd_ready || !s_log_file || !snap) return;
    
    uint32_t now = millis();
    
    // Stop logging automatically if battery is critically low (e.g. < 3.45V)
    if (snap->sys.is_logging_active && snap->sys.battery_v > 0.5f && snap->sys.battery_v < 3.45f) {
        // Disabled for testing without battery
    }

    if (!snap->sys.is_logging_active || !snap->gps.fix_valid) {
        // Flush conditionally based on timeout (60s) even when not logging, 
        // just in case data was lingering.
        if (s_log_buffer_len > 0 && now - s_last_flush_time >= 60000) {
            flush_buffer_to_sd();
        }
        return;
    }

    float v = snap->gps.speed_kmh;
    float heading = snap->gps.course_deg;
    float alt = snap->baro.valid ? snap->baro.altitude_m : snap->gps.altitude_m;

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

    // Track standby condition
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
    float delta_alt = abs(alt - s_last_alt);

    bool high_maneuver = (delta_heading >= 15.0f || delta_alt >= 3.0f);

    uint32_t sample_interval = 3000; // default steady
    if (high_maneuver) {
        sample_interval = 1000;
    } else if (is_standby) {
        sample_interval = 10000;
    }

    if (s_last_heading >= 0 && (now - s_last_sample_time < sample_interval)) {
        // Still wait to flush conditionally based on timeout (60s)
        if (now - s_last_flush_time >= 60000) flush_buffer_to_sd();
        return; 
    }

    // --- Record Sample ---
    s_last_heading = heading;
    s_last_alt = alt;
    s_last_sample_time = now;
    s_points_logged++;

    char time_str[32] = "";
    if (snap->gps.date_valid && snap->gps.time_valid) {
        snprintf(time_str, sizeof(time_str), "%04u-%02u-%02uT%02u:%02u:%02uZ",
                 snap->gps.year, snap->gps.month, snap->gps.day,
                 snap->gps.hour, snap->gps.minute, snap->gps.second);
    }

    // Format GPX Trkpt
    char line_buf[128];
    int len = snprintf(line_buf, sizeof(line_buf), 
                       "      <trkpt lat=\"%.6f\" lon=\"%.6f\">\n"
                       "        <ele>%.1f</ele>\n"
                       "        <time>%s</time>\n"
                       "      </trkpt>\n",
                       snap->gps.latitude, snap->gps.longitude, 
                       alt, time_str);
                       
    if (len > 0) {
        // If buffer is full, flush it first
        if (s_log_buffer_len + len >= LOG_BUFFER_SIZE) {
            flush_buffer_to_sd();
        }
        
        // Append to buffer
        if (s_log_buffer_len + len < LOG_BUFFER_SIZE) {
            memcpy(s_log_buffer + s_log_buffer_len, line_buf, len);
            s_log_buffer_len += len;
        }
    }

    // Trigger flush if >= 512 bytes OR timeout (60s)
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
