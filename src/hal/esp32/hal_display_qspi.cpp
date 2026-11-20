/**
 * @file hal_display_qspi.cpp
 * @brief Unified QSPI AMOLED + LVGL HAL for SH8601, CO5300, and RM690B0.
 */

#include "hal/hal_display.h"
#include "hal/hal_touch.h"
#include "boards/board_select.h"
#include "esp_lcd_sh8601.h"

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_check.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "lvgl.h"

#define LCD_HOST                   SPI2_HOST
#define LCD_BIT_PER_PIXEL          16
#define LVGL_TICK_PERIOD_MS        2
#define LVGL_TASK_MAX_DELAY_MS     500
#define LVGL_TASK_MIN_DELAY_MS     1
#define LVGL_TASK_STACK_SIZE       (8 * 1024)
#define LVGL_TASK_PRIORITY         2

#ifndef BOARD_DISP_X_GAP
#  define BOARD_DISP_X_GAP 0
#endif
#ifndef BOARD_DISP_Y_GAP
#  define BOARD_DISP_Y_GAP 0
#endif
#ifndef BOARD_DISP_MAX_BRIGHTNESS
#  if defined(BOARD_MCU_ESP32C6) && BOARD_MCU_ESP32C6
#    define BOARD_DISP_MAX_BRIGHTNESS 180
#  else
#    define BOARD_DISP_MAX_BRIGHTNESS 255
#  endif
#endif

static SemaphoreHandle_t         s_lvgl_mux     = NULL;
static esp_lcd_panel_io_handle_t s_panel_io     = NULL;
static esp_lcd_panel_handle_t    s_panel_handle = NULL;

// ── Panel Init Sequences ──────────────────────────────────────────────────────
static const sh8601_lcd_init_cmd_t s_sh8601_init_cmds[] = {
    {0x11, (uint8_t []){0x00}, 0, 120},
    {0xC4, (uint8_t []){0x80}, 1, 0},
    {0x35, (uint8_t []){0x00}, 1, 0},
    {0x53, (uint8_t []){0x20}, 1, 1},
    {0x63, (uint8_t []){0xFF}, 1, 1},
    {0x51, (uint8_t []){0x00}, 1, 1},
    {0x29, (uint8_t []){0x00}, 0, 50},
    {0x51, (uint8_t []){0xB4}, 1, 0},
};

static const sh8601_lcd_init_cmd_t s_co5300_init_cmds[] = {
    {0x11, (uint8_t []){0x00}, 0, 120},
    {0xFE, (uint8_t []){0x00}, 1, 0},
    {0xC4, (uint8_t []){0x80}, 1, 0},
    {0x3A, (uint8_t []){0x55}, 1, 0},
    {0x35, (uint8_t []){0x00}, 1, 0},
    {0x53, (uint8_t []){0x20}, 1, 0},
    {0x51, (uint8_t []){0xFF}, 1, 0},
    {0x63, (uint8_t []){0xFF}, 1, 0},
    {0x29, (uint8_t []){0x00}, 0, 60},
    {0x51, (uint8_t []){0xB4}, 1, 0},
};

static const sh8601_lcd_init_cmd_t s_rm690b0_init_cmds[] = {
    {0xFE, (uint8_t []){0x20}, 1, 0},
    {0x26, (uint8_t []){0x0A}, 1, 0},
    {0x24, (uint8_t []){0x80}, 1, 0},
    {0xFE, (uint8_t []){0x00}, 1, 0},
    {0x3A, (uint8_t []){0x55}, 1, 0},
    {0xC2, (uint8_t []){0x00}, 1, 10},
    {0x35, (uint8_t []){0x00}, 1, 0},
#if defined(BOARD_DISP_MADCTL)
    {0x36, (uint8_t []){BOARD_DISP_MADCTL}, 1, 0},
#endif
    {0x51, (uint8_t []){0x00}, 1, 10},
    {0x11, (uint8_t []){0x00}, 0, 120},
    {0x29, (uint8_t []){0x00}, 0, 50},
    {0x51, (uint8_t []){0xB4}, 1, 0},
};

// ── LVGL Callbacks ────────────────────────────────────────────────────────────
static bool notify_lvgl_flush_ready(esp_lcd_panel_io_handle_t panel_io,
                                    esp_lcd_panel_io_event_data_t *edata,
                                    void *user_ctx) {
    (void)panel_io;
    (void)edata;
    lv_disp_drv_t *disp_driver = (lv_disp_drv_t *)user_ctx;
    lv_disp_flush_ready(disp_driver);
    return false;
}

static void lvgl_flush_cb(lv_disp_drv_t *drv, const lv_area_t *area, lv_color_t *color_map) {
    esp_lcd_panel_handle_t panel_handle = (esp_lcd_panel_handle_t)drv->user_data;
    esp_lcd_panel_draw_bitmap(panel_handle,
                              area->x1, area->y1,
                              area->x2 + 1, area->y2 + 1,
                              color_map);
}

static void lvgl_rounder_cb(struct _lv_disp_drv_t *disp_drv, lv_area_t *area) {
    (void)disp_drv;
    uint16_t x1 = area->x1;
    uint16_t x2 = area->x2;
    uint16_t y1 = area->y1;
    uint16_t y2 = area->y2;

    // QSPI AMOLED panels require even start and odd end coordinates
    area->x1 = (x1 >> 1) << 1;
    area->y1 = (y1 >> 1) << 1;
    area->x2 = ((x2 >> 1) << 1) + 1;
    area->y2 = ((y2 >> 1) << 1) + 1;
}

static void lvgl_touch_cb(lv_indev_drv_t *drv, lv_indev_data_t *data) {
    (void)drv;
    uint16_t tp_x = 0, tp_y = 0;
    if (hal_touch_read(&tp_x, &tp_y)) {
        data->point.x = tp_x;
        data->point.y = tp_y;
        data->state   = LV_INDEV_STATE_PRESSED;
    } else {
        data->state   = LV_INDEV_STATE_RELEASED;
    }
}

static void increase_lvgl_tick(void *arg) {
    (void)arg;
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

static void lvgl_port_task(void *arg) {
    (void)arg;
    uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
    for (;;) {
        if (hal_display_lock(-1)) {
            task_delay_ms = lv_timer_handler();
            hal_display_unlock();
        }
        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        } else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS) {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }
        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}

// ── Public HAL API ────────────────────────────────────────────────────────────
void hal_display_init(void) {
    static lv_disp_draw_buf_t disp_buf;
    static lv_disp_drv_t      disp_drv;
    static lv_indev_drv_t     indev_drv;

    const spi_bus_config_t buscfg = SH8601_PANEL_BUS_QSPI_CONFIG(
        BOARD_LCD_PCLK_PIN,
        BOARD_LCD_D0_PIN,
        BOARD_LCD_D1_PIN,
        BOARD_LCD_D2_PIN,
        BOARD_LCD_D3_PIN,
        BOARD_DISP_WIDTH * BOARD_DISP_HEIGHT * LCD_BIT_PER_PIXEL / 8);
    ESP_ERROR_CHECK_WITHOUT_ABORT(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));

    const esp_lcd_panel_io_spi_config_t io_config = SH8601_PANEL_IO_QSPI_CONFIG(
        BOARD_LCD_CS_PIN,
        notify_lvgl_flush_ready,
        &disp_drv);

    const sh8601_lcd_init_cmd_t *init_cmds      = s_sh8601_init_cmds;
    uint16_t                     init_cmds_size = sizeof(s_sh8601_init_cmds) / sizeof(s_sh8601_init_cmds[0]);

#if defined(BOARD_DISP_DRIVER_RM690B0) && BOARD_DISP_DRIVER_RM690B0
    init_cmds      = s_rm690b0_init_cmds;
    init_cmds_size = sizeof(s_rm690b0_init_cmds) / sizeof(s_rm690b0_init_cmds[0]);
#elif defined(BOARD_DISP_DRIVER_CO5300) && BOARD_DISP_DRIVER_CO5300
    init_cmds      = s_co5300_init_cmds;
    init_cmds_size = sizeof(s_co5300_init_cmds) / sizeof(s_co5300_init_cmds[0]);
#endif

    sh8601_vendor_config_t vendor_config = {
        .init_cmds      = init_cmds,
        .init_cmds_size = init_cmds_size,
        .flags = {
            .use_qspi_interface = 1,
        },
    };

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)LCD_HOST, &io_config, &s_panel_io));

    const esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = BOARD_LCD_RST_PIN,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = LCD_BIT_PER_PIXEL,
        .vendor_config  = &vendor_config,
    };

    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_new_panel_sh8601(s_panel_io, &panel_config, &s_panel_handle));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_reset(s_panel_handle));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_init(s_panel_handle));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_set_gap(s_panel_handle, BOARD_DISP_X_GAP, BOARD_DISP_Y_GAP));
    ESP_ERROR_CHECK_WITHOUT_ABORT(esp_lcd_panel_disp_on_off(s_panel_handle, true));

    // Initialize LVGL
    lv_init();

#if defined(BOARD_HAS_PSRAM) && BOARD_HAS_PSRAM
    const uint32_t buf_height = BOARD_DISP_HEIGHT / 4;
#else
    const uint32_t buf_height = BOARD_DISP_HEIGHT / 4;
#endif
    const size_t buf_pixels = (size_t)BOARD_DISP_WIDTH * buf_height;

    lv_color_t *buf1 = (lv_color_t *)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
    lv_color_t *buf2 = (lv_color_t *)heap_caps_malloc(buf_pixels * sizeof(lv_color_t), MALLOC_CAP_DMA);
    assert(buf1 && buf2);

    lv_disp_draw_buf_init(&disp_buf, buf1, buf2, buf_pixels);
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res    = BOARD_DISP_WIDTH;
    disp_drv.ver_res    = BOARD_DISP_HEIGHT;
    disp_drv.flush_cb   = lvgl_flush_cb;
    disp_drv.rounder_cb = lvgl_rounder_cb;
    disp_drv.draw_buf   = &disp_buf;
    disp_drv.user_data  = s_panel_handle;
    lv_disp_t *disp = lv_disp_drv_register(&disp_drv);

    // Initialize touch controller over shared hal_i2c bus
    hal_touch_init();

    lv_indev_drv_init(&indev_drv);
    indev_drv.type    = LV_INDEV_TYPE_POINTER;
    indev_drv.disp    = disp;
    indev_drv.read_cb = lvgl_touch_cb;
    lv_indev_drv_register(&indev_drv);

    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,
        .name     = "lvgl_tick",
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, LVGL_TICK_PERIOD_MS * 1000));

    lv_obj_set_style_bg_color(lv_scr_act(), lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(lv_scr_act(), LV_OPA_COVER, 0);

    s_lvgl_mux = xSemaphoreCreateMutex();
    assert(s_lvgl_mux);
    xTaskCreate(lvgl_port_task, "LVGL", LVGL_TASK_STACK_SIZE, NULL, LVGL_TASK_PRIORITY, NULL);
}

bool hal_display_lock(int timeout_ms) {
    assert(s_lvgl_mux && "hal_display_init must be called first");
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTake(s_lvgl_mux, timeout_ticks) == pdTRUE;
}

void hal_display_unlock(void) {
    assert(s_lvgl_mux && "hal_display_init must be called first");
    xSemaphoreGive(s_lvgl_mux);
}

static uint8_t s_last_brightness = 180;

static void panel_send_cmd(uint8_t cmd) {
    if (!s_panel_io) return;
    uint32_t lcd_cmd = ((uint32_t)cmd << 8) | (0x02UL << 24);
    esp_lcd_panel_io_tx_param(s_panel_io, lcd_cmd, NULL, 0);
}

void hal_display_set_brightness(uint8_t level) {
    if (!s_panel_io) return;
    if (level > BOARD_DISP_MAX_BRIGHTNESS) {
        level = BOARD_DISP_MAX_BRIGHTNESS;
    }
    if (level > 0) {
        s_last_brightness = level;
    }
    uint32_t lcd_cmd = 0x51;
    lcd_cmd &= 0xFF;
    lcd_cmd <<= 8;
    lcd_cmd |= (0x02 << 24);
    esp_lcd_panel_io_tx_param(s_panel_io, lcd_cmd, &level, 1);
}

void hal_display_sleep(bool sleep) {
    if (!s_panel_io) return;
    if (sleep) {
        hal_display_set_brightness(0);
        panel_send_cmd(0x28); // DISPOFF
        vTaskDelay(pdMS_TO_TICKS(10));
        panel_send_cmd(0x10); // SLPIN
        vTaskDelay(pdMS_TO_TICKS(10));
    } else {
        panel_send_cmd(0x11); // SLPOUT
        vTaskDelay(pdMS_TO_TICKS(120));
        panel_send_cmd(0x29); // DISPON
        vTaskDelay(pdMS_TO_TICKS(10));
        hal_display_set_brightness(s_last_brightness > 0 ? s_last_brightness : 180);
    }
}
