/**
 * @file pcf85063.cpp
 * @brief Lightweight direct-Wire driver for PCF85063A/ATL RTC (0x51).
 */

#include "hal/common/pcf85063.h"
#include "hal/hal_i2c.h"
#include <sys/time.h>
#include <string.h>

#define PCF85063_I2C_ADDR    0x51
#define PCF85063_REG_CTRL1   0x00
#define PCF85063_REG_SECONDS 0x04

static bool s_rtc_ready = false;

static inline uint8_t bcd_to_dec(uint8_t val) {
    return (uint8_t)(((val >> 4) * 10) + (val & 0x0F));
}

static inline uint8_t dec_to_bcd(uint8_t val) {
    return (uint8_t)(((val / 10) << 4) | (val % 10));
}

bool pcf85063_init(void) {
    s_rtc_ready = hal_i2c_probe(PCF85063_I2C_ADDR);
    if (!s_rtc_ready) return false;

    // Ensure STOP=0, 24h mode (CTRL1 = 0x00)
    hal_i2c_write_reg(PCF85063_I2C_ADDR, PCF85063_REG_CTRL1, 0x00);
    return true;
}

bool pcf85063_is_ready(void) {
    return s_rtc_ready;
}

bool pcf85063_get_time(struct tm *out_tm) {
    if (!s_rtc_ready || !out_tm) return false;

    uint8_t raw[7] = {0};
    if (!hal_i2c_read_regs(PCF85063_I2C_ADDR, PCF85063_REG_SECONDS, raw, 7)) {
        return false;
    }

    // Bit 7 of SECONDS is Oscillator Stop (OS) flag: 1 = clock integrity not guaranteed
    if (raw[0] & 0x80) {
        return false;
    }

    memset(out_tm, 0, sizeof(struct tm));
    out_tm->tm_sec  = bcd_to_dec(raw[0] & 0x7F);
    out_tm->tm_min  = bcd_to_dec(raw[1] & 0x7F);
    out_tm->tm_hour = bcd_to_dec(raw[2] & 0x3F);
    out_tm->tm_mday = bcd_to_dec(raw[3] & 0x3F);
    out_tm->tm_wday = raw[4] & 0x07;
    out_tm->tm_mon  = bcd_to_dec(raw[5] & 0x1F) - 1;
    out_tm->tm_year = bcd_to_dec(raw[6]) + 100; // 2000 + YY - 1900

    return (out_tm->tm_year >= (2023 - 1900));
}

bool pcf85063_set_time(const struct tm *tm_val) {
    if (!s_rtc_ready || !tm_val) return false;
    if (tm_val->tm_year < 100) return false; // < 2000

    uint8_t buf[8];
    buf[0] = PCF85063_REG_SECONDS;
    buf[1] = dec_to_bcd((uint8_t)tm_val->tm_sec) & 0x7F; // Clear OS flag
    buf[2] = dec_to_bcd((uint8_t)tm_val->tm_min);
    buf[3] = dec_to_bcd((uint8_t)tm_val->tm_hour);
    buf[4] = dec_to_bcd((uint8_t)tm_val->tm_mday);
    buf[5] = (uint8_t)(tm_val->tm_wday & 0x07);
    buf[6] = dec_to_bcd((uint8_t)(tm_val->tm_mon + 1));
    buf[7] = dec_to_bcd((uint8_t)((tm_val->tm_year - 100) % 100));

    return hal_i2c_write_bytes(PCF85063_I2C_ADDR, buf, sizeof(buf));
}

bool pcf85063_sync_system_clock(void) {
    struct tm t = {};
    if (!pcf85063_get_time(&t)) return false;

    time_t epoch = mktime(&t);
    if (epoch == -1) return false;

    struct timeval tv = { .tv_sec = epoch, .tv_usec = 0 };
    settimeofday(&tv, NULL);
    return true;
}
