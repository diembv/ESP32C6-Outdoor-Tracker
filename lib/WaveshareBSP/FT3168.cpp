#include "FT3168.h"
#include "esp_err.h"
#include "lcd_config.h"
#include <Wire.h>

uint8_t I2C_writr_buff(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len)
{
    Wire.beginTransmission(addr);
    Wire.write(reg);
    for(uint8_t i = 0; i < len; i++) {
        Wire.write(buf[i]);
    }
    return (Wire.endTransmission() == 0) ? ESP_OK : ESP_FAIL;
}

uint8_t I2C_read_buff(uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t len)
{
    Wire.beginTransmission(addr);
    Wire.write(reg);
    if (Wire.endTransmission(true) != 0) return ESP_FAIL;
    if (Wire.requestFrom((uint16_t)addr, (uint8_t)len) != len) return ESP_FAIL;
    for(uint8_t i = 0; i < len; i++) {
        buf[i] = Wire.read();
    }
    return ESP_OK;
}

uint8_t I2C_master_write_read_device(uint8_t addr, uint8_t *writeBuf, uint8_t writeLen, uint8_t *readBuf, uint8_t readLen)
{
    Wire.beginTransmission(addr);
    for(uint8_t i = 0; i < writeLen; i++) {
        Wire.write(writeBuf[i]);
    }
    if (Wire.endTransmission(true) != 0) return ESP_FAIL;
    if (Wire.requestFrom((uint16_t)addr, (uint8_t)readLen) != readLen) return ESP_FAIL;
    for(uint8_t i = 0; i < readLen; i++) {
        readBuf[i] = Wire.read();
    }
    return ESP_OK;
}

void Touch_Init(void)
{
    // Bỏ qua i2c_driver_install cũ của ESP-IDF vì sẽ gây conflict
    // với Wire.begin() (dùng new driver_ng) trong main.cpp
    uint8_t data = 0x00;
    I2C_writr_buff(I2C_ADDR_FT3168, 0x00, &data, 1); // Switch to normal mode
}

uint8_t getTouch(uint16_t *x, uint16_t *y)
{
    uint8_t data = 0;
    uint8_t buf[4];
    
    // Đọc số lượng điểm chạm (TD_STATUS)
    if (I2C_read_buff(I2C_ADDR_FT3168, 0x02, &data, 1) != ESP_OK) {
        return 0;
    }
    
    // Lọc giá trị rác: chỉ xử lý nếu có 1 hoặc 2 điểm chạm (4 bit thấp)
    uint8_t touches = data & 0x0F;
    if (touches > 0 && touches <= 2)
    {
        if (I2C_read_buff(I2C_ADDR_FT3168, 0x03, buf, 4) == ESP_OK) {
            *x = (((uint16_t)buf[0] & 0x0f) << 8) | (uint16_t)buf[1];
            *y = (((uint16_t)buf[2] & 0x0f) << 8) | (uint16_t)buf[3];
            if(*x > EXAMPLE_LCD_H_RES) *x = EXAMPLE_LCD_H_RES;
            if(*y > EXAMPLE_LCD_V_RES) *y = EXAMPLE_LCD_V_RES;
            return 1;
        }
    }
    return 0;
}
