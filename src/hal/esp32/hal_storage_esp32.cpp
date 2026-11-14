/**
 * @file hal_storage_esp32.cpp
 * @brief ESP32 storage backend (SDSPI / SDMMC) implementing hal_storage.h.
 */

#include "hal/hal_storage.h"
#include "boards/board_select.h"

#include <Arduino.h>
#include <sys/stat.h>
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/sdspi_host.h"
#include "driver/gpio.h"

#define SD_MOUNT_POINT "/sdcard"

static sdmmc_card_t *s_card        = NULL;
static bool          s_sd_ready    = false;
static uint32_t      s_capacity_mb = 0;

bool hal_storage_init(uint32_t *out_capacity_mb) {
    s_sd_ready    = false;
    s_capacity_mb = 0;

#if defined(BOARD_SD_MODE_SDSPI) && BOARD_SD_MODE_SDSPI
    Serial.printf("[SD] Khoi tao the nho MicroSD (SDSPI, CS=GPIO%d)...\n", BOARD_SD_CS_PIN);

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed   = false,
        .max_files                = 4,
        .allocation_unit_size     = 16 * 1024,
        .disk_status_check_enable = false
    };

    gpio_set_pull_mode((gpio_num_t)BOARD_SD_MOSI_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)BOARD_SD_MISO_PIN, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode((gpio_num_t)BOARD_SD_CLK_PIN,  GPIO_PULLUP_ONLY);

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = (gpio_num_t)BOARD_SD_CS_PIN;
    slot_config.host_id = SPI2_HOST;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.max_freq_khz = BOARD_SD_MAX_FREQ_KHZ;

    esp_err_t ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);
    if (ret != ESP_OK) {
        Serial.printf("[SD] Mount that bai (ma loi: 0x%x).\n", ret);
        pinMode(BOARD_SD_CS_PIN, OUTPUT);
        digitalWrite(BOARD_SD_CS_PIN, HIGH);
        if (out_capacity_mb) *out_capacity_mb = 0;
        return false;
    }

    s_sd_ready    = true;
    s_capacity_mb = (uint32_t)((float)s_card->csd.capacity / 2048.0f);
    Serial.printf("[SD] Nhan dien the nho: %s | Dung luong: %lu MB\n",
                  s_card->cid.name, (unsigned long)s_capacity_mb);
#else
    // SDMMC backend will be added in Phase 3 for ESP32-S3 boards
    Serial.println(F("[SD] SDMMC backend chua duoc kich hoat."));
    if (out_capacity_mb) *out_capacity_mb = 0;
    return false;
#endif

    if (out_capacity_mb) *out_capacity_mb = s_capacity_mb;
    return true;
}

bool hal_storage_is_ready(void) {
    return s_sd_ready;
}

uint32_t hal_storage_capacity_mb(void) {
    return s_capacity_mb;
}

bool hal_storage_ensure_dir(const char *path) {
    if (!path) return false;
    struct stat st;
    if (stat(path, &st) == 0) {
        return true;
    }
    return mkdir(path, 0777) == 0;
}

const char *hal_storage_mount_point(void) {
    return SD_MOUNT_POINT;
}
