/**
 * @file board_ws_c6_amoled_164.h
 * @brief Board descriptor for Waveshare ESP32-C6-Touch-AMOLED-1.64
 */

#pragma once

#define BOARD_NAME                    "Waveshare ESP32-C6-Touch-AMOLED-1.64"

// ── MCU & Memory Capabilities ─────────────────────────────────────────────────
#define BOARD_MCU_ESP32C6             1
#define BOARD_HAS_PSRAM               0

// ── Onboard Power / Expander / RTC Capabilities ───────────────────────────────
#define BOARD_HAS_PMIC_AXP2101        0
#define BOARD_HAS_IO_EXPANDER_TCA9554 0
#define BOARD_HAS_RTC_PCF85063        0
#define BOARD_HAS_IMU_QMI8658         1

// ── Display & UI Profile ──────────────────────────────────────────────────────
#define BOARD_DISP_WIDTH              280
#define BOARD_DISP_HEIGHT             456
#define BOARD_DISP_X_GAP              0x14
#define BOARD_DISP_Y_GAP              0
#define BOARD_DISP_DRIVER_SH8601      1
#define BOARD_UI_PROFILE_PORTRAIT     1

// QSPI Display Pins
#define BOARD_LCD_CS_PIN              10
#define BOARD_LCD_PCLK_PIN            11
#define BOARD_LCD_D0_PIN              4
#define BOARD_LCD_D1_PIN              5
#define BOARD_LCD_D2_PIN              7
#define BOARD_LCD_D3_PIN              19
#define BOARD_LCD_RST_PIN             21
#define BOARD_LCD_BK_LIGHT_PIN        (-1)

// ── Touch Controller ──────────────────────────────────────────────────────────
#define BOARD_TOUCH_FT3168            1
#define BOARD_TOUCH_I2C_ADDR          0x38

// ── I2C Bus ───────────────────────────────────────────────────────────────────
#ifdef I2C_SDA_PIN
#  define BOARD_I2C_SDA_PIN           I2C_SDA_PIN
#else
#  define BOARD_I2C_SDA_PIN           18
#endif

#ifdef I2C_SCL_PIN
#  define BOARD_I2C_SCL_PIN           I2C_SCL_PIN
#else
#  define BOARD_I2C_SCL_PIN           8
#endif

#define BOARD_I2C_FREQ_HZ             100000UL

// ── External Sensors (UART / I2C) ─────────────────────────────────────────────
#ifdef BMP580_I2C_ADDR
#  define BOARD_BMP580_I2C_ADDR       BMP580_I2C_ADDR
#else
#  define BOARD_BMP580_I2C_ADDR       0x47
#endif

#define BOARD_GPS_UART_NUM            1
#ifdef GPS_RX_PIN
#  define BOARD_GPS_RX_PIN            GPS_RX_PIN
#else
#  define BOARD_GPS_RX_PIN            2
#endif

#ifdef GPS_TX_PIN
#  define BOARD_GPS_TX_PIN            GPS_TX_PIN
#else
#  define BOARD_GPS_TX_PIN            3
#endif

#ifdef GPS_BAUD
#  define BOARD_GPS_BAUD              GPS_BAUD
#else
#  define BOARD_GPS_BAUD              9600
#endif

// ── MicroSD Storage (Shared SPI2 with QSPI AMOLED) ────────────────────────────
#define BOARD_SD_MODE_SDSPI           1
#define BOARD_SD_SHARES_DISPLAY_BUS   1
#define BOARD_SD_CS_PIN               15
#define BOARD_SD_MOSI_PIN             4
#define BOARD_SD_MISO_PIN             5
#define BOARD_SD_CLK_PIN              11
#define BOARD_SD_MAX_FREQ_KHZ         4000

// ── Buttons & Battery ADC ─────────────────────────────────────────────────────
#define BOARD_BTN_PWR_PIN             9
#define BOARD_BAT_ADC_PIN             0
#define BOARD_BAT_ADC_DIVIDER         2.0f
