/**
 * @file touch_cst8xx.cpp
 * @brief Hynitron CST816 / CST820 capacitive touch driver over hal_i2c (0x15).
 */

#include "touch_cst8xx.h"
#include "hal/hal_i2c.h"

#define CST8XX_REG_GESTURE_ID  0x01
#define CST8XX_REG_DIS_AUTOSLP 0xFE

bool touch_cst8xx_init(uint8_t addr) {
    if (!hal_i2c_probe(addr)) {
        return false;
    }
    // Disable auto-sleep so polling remains responsive
    hal_i2c_write_reg(addr, CST8XX_REG_DIS_AUTOSLP, 0x01);
    return true;
}

bool touch_cst8xx_read(uint8_t addr, uint16_t *x, uint16_t *y) {
    uint8_t buf[6] = {0};
    // 0x01: Gesture, 0x02: FingerNum, 0x03: XposH, 0x04: XposL, 0x05: YposH, 0x06: YposL
    if (!hal_i2c_read_regs(addr, CST8XX_REG_GESTURE_ID, buf, 6)) {
        return false;
    }

    uint8_t fingers = buf[1] & 0x0F;
    if (fingers > 0 && fingers <= 2) {
        if (x) *x = (((uint16_t)buf[2] & 0x0F) << 8) | (uint16_t)buf[3];
        if (y) *y = (((uint16_t)buf[4] & 0x0F) << 8) | (uint16_t)buf[5];
        return true;
    }
    return false;
}
