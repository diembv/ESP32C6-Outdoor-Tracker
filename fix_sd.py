import re

with open('src/sd_logger.cpp', 'r', encoding='utf-8') as f:
    text = f.read()

target = '''    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs   = SD_CS_PIN;
    slot_config.host_id   = SD_SPI_HOST;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SD_SPI_HOST;
    host.max_freq_khz = SDMMC_FREQ_DEFAULT; 

    esp_err_t ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);'''

replacement = '''    // Keo pull-up cho cac chan SPI
    gpio_set_pull_mode((gpio_num_t)4, GPIO_PULLUP_ONLY); // MOSI / D0
    gpio_set_pull_mode((gpio_num_t)5, GPIO_PULLUP_ONLY); // MISO / D1
    gpio_set_pull_mode((gpio_num_t)11, GPIO_PULLUP_ONLY); // CLK

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs   = SD_CS_PIN;
    slot_config.host_id   = SD_SPI_HOST;

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.flags &= ~SDMMC_HOST_FLAG_DEINIT_ARG; // Khong deinit bus
    host.slot = SD_SPI_HOST;
    host.max_freq_khz = 4000; // 4MHz

    esp_err_t ret = esp_vfs_fat_sdspi_mount(SD_MOUNT_POINT, &host, &slot_config, &mount_config, &s_card);'''

text = text.replace(target, replacement)

with open('src/sd_logger.cpp', 'w', encoding='utf-8') as f:
    f.write(text)
