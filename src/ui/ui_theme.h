/**
 * @file ui_theme.h
 * @brief AMOLED color palette, adaptive font selector, and LVGL helper utilities.
 */

#pragma once

#include <lvgl.h>
#include "boards/board_select.h"

#define SCREEN_W   BOARD_DISP_WIDTH
#define SCREEN_H   BOARD_DISP_HEIGHT

// ── AMOLED Color palette ──────────────────────────────────────────────────────
#define CLR_BG        lv_color_hex(0x000000)   // Pure black (AMOLED power saving)
#define CLR_SURFACE   lv_color_hex(0x0D1117)   // Card surface
#define CLR_BORDER    lv_color_hex(0x21262D)   // Card / divider border
#define CLR_ACCENT    lv_color_hex(0x00FFFF)   // Cyan — header, accent
#define CLR_GREEN     lv_color_hex(0x00FF87)   // GPS fix OK, battery OK
#define CLR_YELLOW    lv_color_hex(0xFFFF00)   // Coords
#define CLR_ORANGE    lv_color_hex(0xFF8800)   // Course / compass
#define CLR_WHITE     lv_color_hex(0xFFFFFF)   // Primary values
#define CLR_GRAY      lv_color_hex(0xAAAAAA)   // Secondary values
#define CLR_RED       lv_color_hex(0xFF3B30)   // No fix, error, REC
#define CLR_DIMTEXT   lv_color_hex(0x6B7280)   // Dim label keys
#define CLR_ENVGREEN  lv_color_hex(0x00FF00)   // Environment data

typedef struct {
    const lv_font_t *xs;
    const lv_font_t *sm;
    const lv_font_t *md;
    const lv_font_t *lg;
    const lv_font_t *xl;
} UiThemeFonts;

/**
 * @brief Returns adaptive font set based on screen width and available compiled fonts.
 */
const UiThemeFonts *ui_theme_fonts(void);

/**
 * @brief Strips scrollbars completely from an LVGL object (prevents white pill artifact).
 */
void ui_theme_strip_scrollbar(lv_obj_t *obj);

/**
 * @brief Applies standard AMOLED black background & non-scrollable styling to a tile.
 */
void ui_theme_style_tile(lv_obj_t *tile);

/**
 * @brief Creates a 1px horizontal divider inside a Flex container (width = 100%).
 */
lv_obj_t *ui_theme_make_divider(lv_obj_t *parent);

/**
 * @brief Creates a dim key label (xs font).
 */
lv_obj_t *ui_theme_make_key_label(lv_obj_t *parent, const char *text);

/**
 * @brief Creates a value label with specified color and font.
 */
lv_obj_t *ui_theme_make_val_label(lv_obj_t *parent, const char *init_text,
                                  lv_color_t color, const lv_font_t *font);

/**
 * @brief Creates a circular status indicator dot.
 */
lv_obj_t *ui_theme_make_dot(lv_obj_t *parent, lv_color_t color, lv_coord_t size);
