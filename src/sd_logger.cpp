#include "sd_logger.h"
#include <stdio.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/gpio.h"

#define SD_MOUNT_POINT "/sdcard"
#define SD_CS_PIN      (gpio_num_t)15
#define SD_SPI_HOST    SPI2_HOST

static sdmmc_card_t *s_card = NULL;
static FILE *s_log_file = NULL;
static bool s_sd_ready = false;
static char s_filepath[128];

bool sd_logger_init(SensorSnapshot *snap) {
    s_sd_ready = false;
    if (snap) {
        snap->sys.sd_present = false;
        snap->sys.sd_ok = false;
        snap->sys.sd_free_mb = 0;
        snap->sys.is_logging = false;
    }

    Serial.println(F("[SD] Khởi tạo thẻ nhớ MicroSD (SPI2, CS=GPIO15)..."));

    // Cấu hình mount FATFS
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false
    };

    // Cấu hình slot SD SPI (dùng chung bus SPI2 đã khởi tạo bởi màn hình AMOLED)
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs   = SD_CS_PIN;
    slot_config.host_id   = SD_SPI_HOST;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT; // 20MHz an toàn cho chia sẻ bus SPI

    esp_err_t ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK) {
        Serial.printf("[SD] Mount thất bại (mã lỗi: 0x%x). Có thể chưa cắm thẻ nhớ.\n", ret);
        if (snap) {
            snap->sys.sd_ok = false;
            snap->sys.sd_present = false;
            snap->sys.sd_free_mb = (uint32_t)ret; // Lưu mã lỗi để UI hiển thị nếu cần
        }
        return false;
    }

    s_sd_ready = true;
    uint32_t capacity_mb = (uint32_t)((float)s_card->csd.capacity / 2048.0f);
    Serial.printf("[SD] Nhận diện thẻ nhớ: %s | Dung lượng: %lu MB\n", s_card->cid.name, (unsigned long)capacity_mb);

    if (snap) {
        snap->sys.sd_present = true;
        snap->sys.sd_ok = true;
        snap->sys.sd_free_mb = capacity_mb; // Lưu dung lượng hiển thị
    }

    // Tạo thư mục tracker_log nếu chưa có
    struct stat st;
    if (stat(SD_MOUNT_POINT "/tracker_log", &st) != 0) {
        Serial.println("[SD] Tạo thư mục /tracker_log");
        mkdir(SD_MOUNT_POINT "/tracker_log", 0777);
    }

    return true;
}

void sd_logger_toggle(SensorSnapshot *snap) {
    if (!s_sd_ready || !snap) return;

    if (snap->sys.is_logging) {
        // Đang ghi -> Dừng ghi
        snap->sys.is_logging = false;
        if (s_log_file) {
            // Đóng XML tag của GPX
            fprintf(s_log_file, "    </trkseg>\n  </trk>\n</gpx>\n");
            fclose(s_log_file);
            s_log_file = NULL;
            Serial.println("[SD] Đã DỪNG ghi log và lưu file GPX an toàn.");
        }
    } else {
        // Chưa ghi -> Bắt đầu ghi
        char time_str[32] = "1970-01-01T00:00:00Z";
        
        // Tìm tên file mới: theo ngày giờ nếu có GPS, nếu không thì đếm số.
        if (snap->gps.date_valid && snap->gps.time_valid) {
            snprintf(s_filepath, sizeof(s_filepath), SD_MOUNT_POINT "/tracker_log/track_%04u%02u%02u_%02u%02u%02u.gpx",
                     snap->gps.year, snap->gps.month, snap->gps.day,
                     snap->gps.hour, snap->gps.minute, snap->gps.second);
                     
            snprintf(time_str, sizeof(time_str), "%04u-%02u-%02uT%02u:%02u:%02uZ",
                     snap->gps.year, snap->gps.month, snap->gps.day,
                     snap->gps.hour, snap->gps.minute, snap->gps.second);
        } else {
            int file_idx = 1;
            while (file_idx < 1000) {
                snprintf(s_filepath, sizeof(s_filepath), SD_MOUNT_POINT "/tracker_log/track_%03d.gpx", file_idx);
                struct stat st;
                if (stat(s_filepath, &st) != 0) {
                    break;
                }
                file_idx++;
            }
        }

        s_log_file = fopen(s_filepath, "w");
        if (!s_log_file) {
            Serial.printf("[SD] Lỗi tạo file mới: %s\n", s_filepath);
            return;
        }

        // Ghi header chuẩn GPX
        fprintf(s_log_file, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        fprintf(s_log_file, "<gpx version=\"1.1\" creator=\"ESP32-C6 Tracker\" xmlns=\"http://www.topografix.com/GPX/1/1\">\n");
        fprintf(s_log_file, "  <metadata>\n");
        fprintf(s_log_file, "    <time>%s</time>\n", time_str);
        fprintf(s_log_file, "  </metadata>\n");
        fprintf(s_log_file, "  <trk>\n");
        fprintf(s_log_file, "    <name>Track %s</name>\n", time_str);
        fprintf(s_log_file, "    <trkseg>\n");
        fflush(s_log_file);
        
        Serial.printf("[SD] BẮT ĐẦU ghi log GPX vào: %s\n", s_filepath);
        
        snap->sys.is_logging = true;
    }
}

void sd_logger_log(SensorSnapshot *snap) {
    if (!s_sd_ready || !s_log_file || !snap) return;
    
    // Chỉ ghi khi hệ thống đang ở chế độ logging
    if (!snap->sys.is_logging) return;
    
    // Chỉ ghi khi có GPS Fix
    if (!snap->gps.fix_valid) return;

    // Định dạng thời gian điểm point
    char time_str[32] = "";
    if (snap->gps.date_valid && snap->gps.time_valid) {
        snprintf(time_str, sizeof(time_str), "%04u-%02u-%02uT%02u:%02u:%02uZ",
                 snap->gps.year, snap->gps.month, snap->gps.day,
                 snap->gps.hour, snap->gps.minute, snap->gps.second);
    }

    // Ưu tiên cao độ Baro nếu khả dụng vì nó thường chính xác và phản hồi nhanh hơn GPS
    float ele = snap->baro.valid ? snap->baro.altitude_m : snap->gps.altitude_m;

    fprintf(s_log_file, "      <trkpt lat=\"%.6f\" lon=\"%.6f\">\n", snap->gps.latitude, snap->gps.longitude);
    fprintf(s_log_file, "        <ele>%.1f</ele>\n", ele);
    if (time_str[0] != '\0') {
        fprintf(s_log_file, "        <time>%s</time>\n", time_str);
    }
    fprintf(s_log_file, "      </trkpt>\n");

    fflush(s_log_file);
}

bool sd_logger_is_ok(void) {
    return s_sd_ready;
}

void sd_logger_flush(void) {
    if (s_log_file) {
        fflush(s_log_file);
    }
}
