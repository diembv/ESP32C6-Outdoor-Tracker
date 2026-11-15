/**
 * @file axp2101.cpp
 * @brief Lightweight direct-Wire driver for X-Powers AXP2101 PMIC.
 *        Handles LDO power rails (AMOLED/Touch/SD), ADC battery voltage,
 *        fuel-gauge percentage, and VBUS charging detection.
 */

#include "hal/common/axp2101.h"
#include "hal/hal_i2c.h"

#define AXP2101_I2C_ADDR          0x34
#define AXP2101_REG_STATUS1       0x00
#define AXP2101_REG_STATUS2       0x01
#define AXP2101_REG_CHIP_ID       0x03
#define AXP2101_REG_ADC_CTRL      0x30
#define AXP2101_REG_VBAT_H        0x34
#define AXP2101_REG_VBAT_L        0x35
#define AXP2101_REG_LDO_ONOFF0    0x90
#define AXP2101_REG_ALDO1_VOLT    0x92
#define AXP2101_REG_ALDO2_VOLT    0x93
#define AXP2101_REG_ALDO3_VOLT    0x94
#define AXP2101_REG_ALDO4_VOLT    0x95
#define AXP2101_REG_BLDO1_VOLT    0x96
#define AXP2101_REG_BAT_PERCENT   0xA4

static bool s_axp_ready = false;

bool axp2101_init(void) {
    s_axp_ready = false;

    uint8_t chip_id = 0;
    if (!hal_i2c_read_regs(AXP2101_I2C_ADDR, AXP2101_REG_CHIP_ID, &chip_id, 1)) {
        return false;
    }

    // AXP2101 chip ID is 0x4A (lower 6 bits 0x0A, version bits vary)
    if ((chip_id & 0x0F) != 0x0A) {
        return false;
    }

    // Enable VBAT & VBUS ADC channels (bit 0 = VBAT, bit 2 = VBUS, bit 3 = VSYS)
    hal_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_ADC_CTRL, 0x0D);

    // Configure standard 3.3V on ALDO1..ALDO4, BLDO1 for Waveshare AMOLED/Touch/SD/Audio
    // Voltage = 0.5V + reg * 0.1V -> 3.3V = 28 (0x1C)
    hal_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_ALDO1_VOLT, 0x1C);
    hal_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_ALDO2_VOLT, 0x1C);
    hal_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_ALDO3_VOLT, 0x1C);
    hal_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_ALDO4_VOLT, 0x1C);
    hal_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_BLDO1_VOLT, 0x1C);

    // Enable ALDO1..4 + BLDO1 (bits 0..4)
    uint8_t ldo_en = 0;
    if (hal_i2c_read_regs(AXP2101_I2C_ADDR, AXP2101_REG_LDO_ONOFF0, &ldo_en, 1)) {
        ldo_en |= 0x1F;
        hal_i2c_write_reg(AXP2101_I2C_ADDR, AXP2101_REG_LDO_ONOFF0, ldo_en);
    }

    s_axp_ready = true;
    return true;
}

bool axp2101_is_ready(void) {
    return s_axp_ready;
}

bool axp2101_read_battery(float *out_volts, uint8_t *out_pct, bool *out_charging) {
    if (!s_axp_ready) return false;

    uint8_t raw[2] = {0, 0};
    if (!hal_i2c_read_regs(AXP2101_I2C_ADDR, AXP2101_REG_VBAT_H, raw, 2)) {
        return false;
    }

    // 14-bit VBAT ADC in mV: ((H & 0x3F) << 8) | L
    uint16_t mv = (uint16_t)(((raw[0] & 0x3F) << 8) | raw[1]);
    if (out_volts) {
        *out_volts = mv / 1000.0f;
    }

    if (out_pct) {
        uint8_t pct = 0;
        if (hal_i2c_read_regs(AXP2101_I2C_ADDR, AXP2101_REG_BAT_PERCENT, &pct, 1)) {
            if (pct > 100) pct = 100;
            *out_pct = pct;
        } else {
            float v = mv / 1000.0f;
            if (v >= 4.15f) *out_pct = 100;
            else if (v <= 3.30f) *out_pct = 0;
            else *out_pct = (uint8_t)(((v - 3.30f) / (4.15f - 3.30f)) * 100.0f);
        }
    }

    if (out_charging) {
        uint8_t st2 = 0;
        if (hal_i2c_read_regs(AXP2101_I2C_ADDR, AXP2101_REG_STATUS2, &st2, 1)) {
            // bits 6:5 == 0b01 -> charging
            *out_charging = ((st2 >> 5) & 0x03) == 0x01;
        } else {
            *out_charging = false;
        }
    }

    return true;
}
