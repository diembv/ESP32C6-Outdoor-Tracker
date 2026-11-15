/**
 * @file tca9554.cpp
 * @brief Direct-Wire driver for TCA9554/PCA9554 8-bit I2C IO Expander.
 */

#include "hal/common/tca9554.h"
#include "hal/hal_i2c.h"

#define TCA9554_REG_INPUT   0x00
#define TCA9554_REG_OUTPUT  0x01
#define TCA9554_REG_POLINV  0x02
#define TCA9554_REG_CONFIG  0x03

static uint8_t s_addr       = 0x20;
static bool    s_ready      = false;
static uint8_t s_dir_mask   = 0xFF; // 1 = input, 0 = output
static uint8_t s_out_mask   = 0x00;

bool tca9554_init(uint8_t addr) {
    s_addr  = addr;
    s_ready = false;

    if (!hal_i2c_probe(s_addr)) {
        return false;
    }

    // Read current direction & output registers so we preserve existing state
    uint8_t cfg = 0xFF, out = 0x00;
    if (!hal_i2c_read_regs(s_addr, TCA9554_REG_CONFIG, &cfg, 1)) return false;
    if (!hal_i2c_read_regs(s_addr, TCA9554_REG_OUTPUT, &out, 1)) return false;

    s_dir_mask = cfg;
    s_out_mask = out;
    hal_i2c_write_reg(s_addr, TCA9554_REG_POLINV, 0x00);
    s_ready = true;
    return true;
}

bool tca9554_set_dir(uint8_t pin, bool is_output) {
    if (!s_ready || pin > 7) return false;
    if (is_output) {
        s_dir_mask &= (uint8_t)~(1U << pin);
    } else {
        s_dir_mask |= (uint8_t)(1U << pin);
    }
    return hal_i2c_write_reg(s_addr, TCA9554_REG_CONFIG, s_dir_mask);
}

bool tca9554_write_pin(uint8_t pin, bool high) {
    if (!s_ready || pin > 7) return false;
    if (high) {
        s_out_mask |= (uint8_t)(1U << pin);
    } else {
        s_out_mask &= (uint8_t)~(1U << pin);
    }
    return hal_i2c_write_reg(s_addr, TCA9554_REG_OUTPUT, s_out_mask);
}

bool tca9554_read_pin(uint8_t pin, bool *out_high) {
    if (!s_ready || pin > 7 || !out_high) return false;
    uint8_t val = 0;
    if (!hal_i2c_read_regs(s_addr, TCA9554_REG_INPUT, &val, 1)) return false;
    *out_high = ((val >> pin) & 0x01) != 0;
    return true;
}
