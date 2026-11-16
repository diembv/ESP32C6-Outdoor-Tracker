/**
 * @file touch_focaltech.cpp
 * @brief FocalTech FT3168 / FT6336 capacitive touch driver over hal_i2c.
 */

#include "touch_focaltech.h"
#include "hal/hal_i2c.h"

#define FT_REG_DEV_MODE   0x00
#define FT_REG_TD_STATUS  0x02
#define FT_REG_P1_XH      0x03

bool touch_focaltech_init(uint8_t addr) {
    if (!hal_i2c_probe(addr)) {
        return false;
    }
    // Switch to normal operating mode
    return hal_i2c_write_reg(addr, FT_REG_DEV_MODE, 0x00);
}

bool touch_focaltech_read(uint8_t addr, uint16_t *x, uint16_t *y) {
    uint8_t status = 0;
    if (!hal_i2c_read_regs(addr, FT_REG_TD_STATUS, &status, 1)) {
        return false;
    }

    uint8_t touches = status & 0x0F;
    if (touches > 0 && touches <= 2) {
        uint8_t buf[4] = {0};
        if (hal_i2c_read_regs(addr, FT_REG_P1_XH, buf, 4)) {
            if (x) *x = (((uint16_t)buf[0] & 0x0F) << 8) | (uint16_t)buf[1];
            if (y) *y = (((uint16_t)buf[2] & 0x0F) << 8) | (uint16_t)buf[3];
            return true;
        }
    }
    return false;
}
