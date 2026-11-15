/**
 * @file qmi8658.cpp
 * @brief Direct-Wire driver for QMI8658A/C 6-axis IMU (auto-detects 0x6A / 0x6B).
 */

#include "hal/common/qmi8658.h"
#include "hal/hal_i2c.h"
#include <Arduino.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define QMI_REG_WHO_AM_I    0x00 // returns 0x05
#define QMI_REG_CTRL1       0x02 // SPI/I2C mode
#define QMI_REG_CTRL2       0x03 // Accel config: range + ODR
#define QMI_REG_CTRL7       0x08 // Enable sensors
#define QMI_REG_AX_L        0x35 // Accel X low byte (6 regs: AX, AY, AZ)

static bool    s_qmi_ready = false;
static uint8_t s_qmi_addr  = 0x6A;

bool qmi8658_init(void) {
    s_qmi_ready = false;
    Serial.print(F("[IMU] QMI8658 WHO_AM_I ... "));

    uint8_t who = 0;
    s_qmi_addr = 0x6A;
    if (!hal_i2c_read_regs(s_qmi_addr, QMI_REG_WHO_AM_I, &who, 1) || who != 0x05) {
        s_qmi_addr = 0x6B;
        if (!hal_i2c_read_regs(s_qmi_addr, QMI_REG_WHO_AM_I, &who, 1) || who != 0x05) {
            Serial.println(F("I2C error (Thu ca 0x6A va 0x6B)."));
            return false;
        }
    }

    Serial.printf("OK (0x%02X tai 0x%02X)\n", who, s_qmi_addr);

    if (!hal_i2c_write_reg(s_qmi_addr, QMI_REG_CTRL1, 0x40)) return false;
    if (!hal_i2c_write_reg(s_qmi_addr, QMI_REG_CTRL2, 0x13)) return false;
    if (!hal_i2c_write_reg(s_qmi_addr, QMI_REG_CTRL7, 0x01)) return false;
    delay(10);

    s_qmi_ready = true;
    return true;
}

bool qmi8658_is_ready(void) {
    return s_qmi_ready;
}

uint8_t qmi8658_get_addr(void) {
    return s_qmi_addr;
}

bool qmi8658_read_tilt(float *out_pitch_deg, float *out_roll_deg) {
    if (!s_qmi_ready) return false;

    uint8_t raw[6];
    if (!hal_i2c_read_regs(s_qmi_addr, QMI_REG_AX_L, raw, 6)) {
        return false;
    }

    int16_t ax_raw = (int16_t)((raw[1] << 8) | raw[0]);
    int16_t ay_raw = (int16_t)((raw[3] << 8) | raw[2]);
    int16_t az_raw = (int16_t)((raw[5] << 8) | raw[4]);

    // +-4g range -> sensitivity = 8192 LSB/g
    float ax = ax_raw / 8192.0f;
    float ay = ay_raw / 8192.0f;
    float az = az_raw / 8192.0f;

    if (out_pitch_deg) {
        *out_pitch_deg = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / (float)M_PI;
    }
    if (out_roll_deg) {
        *out_roll_deg = atan2f(ay, az) * 180.0f / (float)M_PI;
    }
    return true;
}
