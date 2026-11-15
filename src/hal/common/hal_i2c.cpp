/**
 * @file hal_i2c.cpp
 * @brief Thread-safe Wire I2C wrapper (prevents driver_ng / multi-task collisions).
 */

#include "hal/hal_i2c.h"
#include <Arduino.h>
#include <Wire.h>

#if defined(ESP32)
#  include "freertos/FreeRTOS.h"
#  include "freertos/semphr.h"
static SemaphoreHandle_t s_i2c_mux = NULL;
#endif

void hal_i2c_init(int sda_pin, int scl_pin, uint32_t freq_hz) {
#if defined(ESP32)
    if (!s_i2c_mux) {
        s_i2c_mux = xSemaphoreCreateMutex();
    }
#endif
    Wire.begin(sda_pin, scl_pin);
    Wire.setClock(freq_hz);
    Wire.setTimeout(20);
}

bool hal_i2c_lock(int timeout_ms) {
#if defined(ESP32)
    if (!s_i2c_mux) return true;
    TickType_t ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(s_i2c_mux, ticks) == pdTRUE;
#else
    (void)timeout_ms;
    return true;
#endif
}

void hal_i2c_unlock(void) {
#if defined(ESP32)
    if (s_i2c_mux) {
        xSemaphoreGive(s_i2c_mux);
    }
#endif
}

bool hal_i2c_probe(uint8_t addr) {
    if (!hal_i2c_lock(100)) return false;
    Wire.beginTransmission(addr);
    bool ok = (Wire.endTransmission() == 0);
    hal_i2c_unlock();
    return ok;
}

bool hal_i2c_read_regs(uint8_t addr, uint8_t reg, uint8_t *buf, size_t len) {
    if (!buf || len == 0) return false;
    if (!hal_i2c_lock(100)) return false;

    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(true) != 0) {
        hal_i2c_unlock();
        return false;
    }

    size_t got = Wire.requestFrom((uint8_t)addr, (uint8_t)len);
    if (got != len) {
        hal_i2c_unlock();
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        buf[i] = Wire.read();
    }
    hal_i2c_unlock();
    return true;
}

bool hal_i2c_write_reg(uint8_t addr, uint8_t reg, uint8_t val) {
    if (!hal_i2c_lock(100)) return false;
    Wire.beginTransmission(addr);
    Wire.write(reg);
    Wire.write(val);
    bool ok = (Wire.endTransmission() == 0);
    hal_i2c_unlock();
    return ok;
}

bool hal_i2c_write_bytes(uint8_t addr, const uint8_t *data, size_t len) {
    if (!data || len == 0) return false;
    if (!hal_i2c_lock(100)) return false;
    Wire.beginTransmission(addr);
    Wire.write(data, len);
    bool ok = (Wire.endTransmission() == 0);
    hal_i2c_unlock();
    return ok;
}

bool hal_i2c_read_bytes(uint8_t addr, uint8_t *buf, size_t len) {
    if (!buf || len == 0) return false;
    if (!hal_i2c_lock(100)) return false;
    size_t got = Wire.requestFrom((uint8_t)addr, (uint8_t)len);
    if (got != len) {
        hal_i2c_unlock();
        return false;
    }
    for (size_t i = 0; i < len; i++) {
        buf[i] = Wire.read();
    }
    hal_i2c_unlock();
    return true;
}

void hal_i2c_scan(void) {
    Serial.println(F("\n[I2C] ── Bus Scan ──────────────────────────────────"));
    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        if (hal_i2c_probe(addr)) {
            const char *label = "Unknown device          ";
            switch (addr) {
                case 0x15: label = "CST816/CST820 Touch     "; break;
                case 0x20: label = "TCA9554 IO Expander     "; break;
                case 0x34: label = "AXP2101 PMIC            "; break;
                case 0x38: label = "FT3168/FT6336 Touch     "; break;
                case 0x46: label = "BMP580 (SDO=GND, 0x46)  "; break;
                case 0x47: label = "BMP580 (SDO=VCC, 0x47)  "; break;
                case 0x51: label = "PCF85063 RTC            "; break;
                case 0x5A: label = "CST9217/CST9220 Touch   "; break;
                case 0x6A: label = "QMI8658 IMU (SA0=GND)   "; break;
                case 0x6B: label = "QMI8658 IMU (SA0=VCC)   "; break;
                case 0x7E: label = "FT3168 Alt / PMIC       "; break;
                default:   break;
            }
            Serial.printf("  0x%02X -> %s\n", addr, label);
            found++;
        }
    }
    if (found == 0)
        Serial.println(F("  *** Khong tim thay thiet bi I2C! ***"));
    else
        Serial.printf("  Tong: %u thiet bi\n", found);
}
