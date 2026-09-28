#ifndef LCD_BSP_H
#define LCD_BSP_H

#include "Arduino.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_timer.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "lvgl.h"
/* #include "demos/lv_demos.h" — tắt: LV_BUILD_EXAMPLES=0, LV_USE_DEMO_WIDGETS=0 */
#include "esp_check.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Public API của WaveshareBSP — gọi được từ C và C++ (main.cpp)
 *
 * lcd_lvgl_Init()        : Khởi tạo SPI display, LVGL, FreeRTOS LVGL task, touch driver.
 *                          Sau khi hàm này return, gọi example_lvgl_lock rồi build UI.
 *
 * example_lvgl_lock(ms)  : Acquire LVGL mutex. ms=-1 → chờ vô hạn.
 *                          Return true nếu thành công.
 *
 * example_lvgl_unlock()  : Release LVGL mutex.
 *
 * example_lvgl_rounder_cb: LVGL flush rounder callback (dùng nội bộ bởi driver).
 *
 * set_amoled_backlight(b): Đặt độ sáng AMOLED (0–255).
 */
void      lcd_lvgl_Init(void);
bool      example_lvgl_lock(int timeout_ms);
void      example_lvgl_unlock(void);
void      example_lvgl_rounder_cb(struct _lv_disp_drv_t *disp_drv, lv_area_t *area);
esp_err_t set_amoled_backlight(uint8_t brig);

#ifdef __cplusplus
}
#endif

#endif /* LCD_BSP_H */