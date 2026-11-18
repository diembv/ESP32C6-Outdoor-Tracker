/**
 * @file board_ws_s3_amoled_216.h
 * @brief Board descriptor for Waveshare ESP32-S3-Touch-AMOLED-2.16 (480x480 Square)
 */

#pragma once

#define BOARD_NAME                    "Waveshare ESP32-S3-Touch-AMOLED-2.16"

// ── MCU & Memory Capabilities ─────────────────────────────────────────────────
#define BOARD_MCU_ESP32S3             1
#define BOARD_HAS_PSRAM               1

// ── Onboard Power / Expander / RTC Capabilities ───────────────────────────────
#define BOARD_HAS_PMIC_AXP2101        1
#define BOARD_HAS_IO_EXPANDER_TCA9554 0
#define BOARD_HAS_RTC_PCF85063        1
#define BOARD_HAS_IMU_QMI8658         1

// ── Display & UI Profile ──────────────────────────────────────────────────────
#define BOARD_DISP_WIDTH              480
#define BOARD_DISP_HEIGHT             480
#define BOARD_DISP_X_GAP              0
#define BOARD_DISP_Y_GAP              0
#define BOARD_DISP_DRIVER_CO5300      1
#define BOARD_UI_PROFILE_PORTRAIT     1

// QSPI Display Pins
#define BOARD_LCD_CS_PIN              12
#define BOARD_LCD_PCLK_PIN            11
#define BOARD_LCD_D0_PIN              4
#define BOARD_LCD_D1_PIN              5
#define BOARD_LCD_D2_PIN              6
#define BOARD_LCD_D3_PIN              7
#define BOARD_LCD_RST_PIN             8
#define BOARD_LCD_BK_LIGHT_PIN        (-1)

// ── Touch Controller (CST9220 @ 0x5A) ─────────────────────────────────────────
#define BOARD_TOUCH_CST9220           1
#define BOARD_TOUCH_I2C_ADDR          0x5A

// ── I2C Bus ───────────────────────────────────────────────────────────────────
#define BOARD_I2C_SDA_PIN             15
#define BOARD_I2C_SCL_PIN             14
#define BOARD_I2C_FREQ_HZ             400000UL

// ── External Sensors (UART / I2C) ─────────────────────────────────────────────
#define BOARD_BMP580_I2C_ADDR         0x47
#define BOARD_GPS_UART_NUM            1
#define BOARD_GPS_RX_PIN              44
#define BOARD_GPS_TX_PIN              43
#define BOARD_GPS_BAUD                9600

// ── MicroSD Storage (SDMMC 1-bit) ─────────────────────────────────────────────
#define BOARD_SD_MODE_SDMMC           1
#define BOARD_SD_SHARES_DISPLAY_BUS   0
#define BOARD_SD_CLK_PIN              2
#define BOARD_SD_CMD_PIN              1
#define BOARD_SD_D0_PIN               3
#define BOARD_SD_MAX_FREQ_KHZ         20000

// ── Buttons & Battery ─────────────────────────────────────────────────────────
#define BOARD_BTN_PWR_PIN             0
#define BOARD_BAT_ADC_PIN             (-1)
