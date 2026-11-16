/**
 * @file touch_cst92xx.cpp
 * @brief Hynitron CST9217 / CST9220 capacitive touch driver over hal_i2c (0x5A).
 */

#include "touch_cst92xx.h"
#include "hal/hal_i2c.h"

#define CST92XX_ACK_VALUE 0xAB

bool touch_cst92xx_init(uint8_t addr) {
    if (!hal_i2c_probe(addr)) {
        return false;
    }
    // Clear any pending touch frame
    const uint8_t ack_cmd[3] = {0xD0, 0x00, CST92XX_ACK_VALUE};
    hal_i2c_write_bytes(addr, ack_cmd, sizeof(ack_cmd));
    return true;
}

bool touch_cst92xx_read(uint8_t addr, uint16_t *x, uint16_t *y) {
    const uint8_t read_cmd[2] = {0xD0, 0x00};
    if (!hal_i2c_write_bytes(addr, read_cmd, sizeof(read_cmd))) {
        return false;
    }

    uint8_t buf[15] = {0};
    if (!hal_i2c_read_bytes(addr, buf, sizeof(buf))) {
        return false;
    }

    // Acknowledge frame
    const uint8_t ack_cmd[3] = {0xD0, 0x00, CST92XX_ACK_VALUE};
    hal_i2c_write_bytes(addr, ack_cmd, sizeof(ack_cmd));

    // Validate frame ack marker at buf[6]
    if (buf[6] != CST92XX_ACK_VALUE) {
        return false;
    }

    uint8_t fingers = buf[5] & 0x7F;
    uint8_t status  = buf[0] & 0x0F;
    if (fingers > 0 && fingers <= 2 && status == 0x06) {
        if (x) *x = ((uint16_t)buf[1] << 4) | ((buf[3] >> 4) & 0x0F);
        if (y) *y = ((uint16_t)buf[2] << 4) | (buf[3] & 0x0F);
        return true;
    }
    return false;
}
