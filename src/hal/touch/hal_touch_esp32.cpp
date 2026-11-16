/**
 * @file hal_touch_esp32.cpp
 * @brief Unified touch dispatcher implementing hal_touch.h with V1/V2 auto-detect.
 */

#include "hal/hal_touch.h"
#include "boards/board_select.h"
#include "touch_focaltech.h"
#include "touch_cst8xx.h"
#include "touch_cst92xx.h"

#include <Arduino.h>

enum TouchBackend {
    TOUCH_NONE = 0,
    TOUCH_FOCALTECH,
    TOUCH_CST8XX,
    TOUCH_CST92XX
};

static TouchBackend s_backend = TOUCH_NONE;
static uint8_t      s_addr    = 0;
static const char  *s_name    = "None";

bool hal_touch_init(void) {
    s_backend = TOUCH_NONE;
    s_name    = "None";

#if defined(BOARD_TOUCH_AUTO_V1_V2) && BOARD_TOUCH_AUTO_V1_V2
    // Waveshare S3-AMOLED-1.8: V1 uses FT3168 (0x38), V2 uses CST820 (0x15)
    if (touch_focaltech_init(0x38)) {
        s_backend = TOUCH_FOCALTECH;
        s_addr    = 0x38;
        s_name    = "FT3168 (V1 Auto)";
    } else if (touch_cst8xx_init(0x15)) {
        s_backend = TOUCH_CST8XX;
        s_addr    = 0x15;
        s_name    = "CST820 (V2 Auto)";
    }
#elif (defined(BOARD_TOUCH_FT3168) && BOARD_TOUCH_FT3168) || (defined(BOARD_TOUCH_FT6336) && BOARD_TOUCH_FT6336)
    s_addr = BOARD_TOUCH_I2C_ADDR;
    if (touch_focaltech_init(s_addr)) {
        s_backend = TOUCH_FOCALTECH;
#  if defined(BOARD_TOUCH_FT6336) && BOARD_TOUCH_FT6336
        s_name = "FT6336";
#  else
        s_name = "FT3168";
#  endif
    }
#elif defined(BOARD_TOUCH_CST820) && BOARD_TOUCH_CST820
    s_addr = BOARD_TOUCH_I2C_ADDR;
    if (touch_cst8xx_init(s_addr)) {
        s_backend = TOUCH_CST8XX;
        s_name    = "CST820";
    }
#elif (defined(BOARD_TOUCH_CST9217) && BOARD_TOUCH_CST9217) || (defined(BOARD_TOUCH_CST9220) && BOARD_TOUCH_CST9220)
    s_addr = BOARD_TOUCH_I2C_ADDR;
    if (touch_cst92xx_init(s_addr)) {
        s_backend = TOUCH_CST92XX;
#  if defined(BOARD_TOUCH_CST9217) && BOARD_TOUCH_CST9217
        s_name = "CST9217";
#  else
        s_name = "CST9220";
#  endif
    }
#endif

    if (s_backend != TOUCH_NONE) {
        Serial.printf("[TOUCH] %s OK (0x%02X)\n", s_name, s_addr);
        return true;
    }

    Serial.println(F("[TOUCH] Controller offline."));
    return false;
}

bool hal_touch_read(uint16_t *x, uint16_t *y) {
    uint16_t raw_x = 0, raw_y = 0;
    bool pressed = false;

    switch (s_backend) {
        case TOUCH_FOCALTECH:
            pressed = touch_focaltech_read(s_addr, &raw_x, &raw_y);
            break;
        case TOUCH_CST8XX:
            pressed = touch_cst8xx_read(s_addr, &raw_x, &raw_y);
            break;
        case TOUCH_CST92XX:
            pressed = touch_cst92xx_read(s_addr, &raw_x, &raw_y);
            break;
        default:
            return false;
    }

    if (!pressed) return false;

#if defined(BOARD_TOUCH_SWAP_XY) && BOARD_TOUCH_SWAP_XY
    uint16_t tmp = raw_x;
    raw_x = raw_y;
    raw_y = tmp;
#endif

#if defined(BOARD_TOUCH_MIRROR_X) && BOARD_TOUCH_MIRROR_X
    if (raw_x < BOARD_DISP_WIDTH) raw_x = (BOARD_DISP_WIDTH - 1) - raw_x;
    else raw_x = 0;
#endif

#if defined(BOARD_TOUCH_MIRROR_Y) && BOARD_TOUCH_MIRROR_Y
    if (raw_y < BOARD_DISP_HEIGHT) raw_y = (BOARD_DISP_HEIGHT - 1) - raw_y;
    else raw_y = 0;
#endif

    if (raw_x >= BOARD_DISP_WIDTH)  raw_x = BOARD_DISP_WIDTH - 1;
    if (raw_y >= BOARD_DISP_HEIGHT) raw_y = BOARD_DISP_HEIGHT - 1;

    if (x) *x = raw_x;
    if (y) *y = raw_y;
    return true;
}

const char *hal_touch_driver_name(void) {
    return s_name;
}
