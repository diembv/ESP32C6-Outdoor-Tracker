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
static char s_filepath[64] = SD_MOUNT_POINT "/tracker_log.csv";

bool sd_logger_init(SensorSnapshot *snap) {
    s_sd_ready = false;
    if (snap) {
        snap->sys.sd_present = false;
        snap->sys.sd_ok = false;
        snap->sys.sd_free_mb = 0;
    }

    Serial.println(F("[SD] Khởi tạo thẻ nhớ MicroSD (SPI2, CS=GPIO15)..."));

    // Cấu hình mount FATFS
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 4,
        .allocation_unit_size = 16 * 1024,
        .disk_status_check_enable = false
    };

    // Kéo pull-up cho các chân SPI để tránh nhiễu/lỗi nhận thẻ khi dùng chung bus QSPI
    gpio_set_pull_mode((gpio_num_t)4, GPIO_PULLUP_ONLY); // MOSI / D0
    gpio_set_pull_mode((gpio_num_t)5, GPIO_PULLUP_ONLY); // MISO / D1
    gpio_set_pull_mode((gpio_num_t)11, GPIO_PULLUP_ONLY); // CLK

    // Cấu hình slot SD SPI (dùng chung bus SPI2 đã khởi tạo bởi màn hình AMOLED)
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs   = SD_CS_PIN;
    slot_config.host_id   = SD_SPI_HOST;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    // Quan trọng: Bus SPI2 đã được màn hình AMOLED khởi tạo, KHÔNG cho phép thẻ SD init lại bus!
    host.flags &= ~SDMMC_HOST_FLAG_DEINIT_ARG; // Giữ các cờ khác, chỉ xoá DEINIT_ARG
    host.slot = SD_SPI_HOST;
    host.max_freq_khz = 4000; // Hạ xuống 4MHz để an toàn khi dùng chung bus QSPI tốc độ cao

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

    // Mở file CSV, nếu chưa có thì ghi tiêu đề cột (Header)
    struct stat st;
    bool write_header = (stat(s_filepath, &st) != 0);

    s_log_file = fopen(s_filepath, "a");
    if (!s_log_file) {
        Serial.printf("[SD] Lỗi mở file %s để ghi!\n", s_filepath);
        return true;
    }

    if (write_header) {
        fprintf(s_log_file, "uptime_s,fix,sats,lat,lon,speed_kmh,course_deg,alt_gps_m,alt_baro_m,press_hpa,temp_c,pitch_deg,roll_deg,bat_v,bat_pct\n");
        fflush(s_log_file);
        Serial.printf("[SD] Đã tạo file log mới: %s\n", s_filepath);
    } else {
        Serial.printf("[SD] Tiếp tục ghi vào file: %s\n", s_filepath);
    }

    return true;
}

void sd_logger_log(const SensorSnapshot *snap) {
    if (!s_sd_ready || !s_log_file || !snap) return;

    // Ghi một dòng CSV:
    // uptime_s,fix,sats,lat,lon,speed_kmh,course_deg,alt_gps_m,alt_baro_m,press_hpa,temp_c,pitch_deg,roll_deg,bat_v,bat_pct
    int fix = snap->gps.fix_valid ? 1 : 0;
    
    fprintf(s_log_file, "%lu,%d,%u,%.6f,%.6f,%.2f,%.1f,%.1f,%.1f,%.2f,%.2f,%.1f,%.1f,%.2f,%u\n",
            (unsigned long)snap->sys.uptime_s,
            fix,
            (unsigned int)snap->gps.satellites,
            snap->gps.latitude,
            snap->gps.longitude,
            snap->gps.speed_kmh,
            snap->gps.course_deg,
            snap->gps.altitude_m,
            snap->baro.altitude_m,
            snap->baro.pressure_hpa,
            snap->baro.temperature_c,
            snap->imu.pitch_deg,
            snap->imu.roll_deg,
            snap->sys.battery_v,
            (unsigned int)snap->sys.battery_pct);

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
