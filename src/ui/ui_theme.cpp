/**
 * @file ui_theme.cpp
 * @brief Implementation of AMOLED theme, adaptive font selector, and LVGL helpers.
 */

#include "ui/ui_theme.h"

const UiThemeFonts *ui_theme_fonts(void)
{
    static UiThemeFonts s_fonts;
    static bool s_init = false;
    if (!s_init) {
        s_fonts.xs = &lv_font_montserrat_12;
        s_fonts.sm = &lv_font_montserrat_14;
        s_fonts.md = &lv_font_montserrat_20;

#if (BOARD_DISP_WIDTH >= 360) && defined(LV_FONT_MONTSERRAT_36) && LV_FONT_MONTSERRAT_36
        s_fonts.lg = &lv_font_montserrat_28;
        s_fonts.xl = &lv_font_montserrat_36;
#elif (BOARD_DISP_WIDTH >= 440) && defined(LV_FONT_MONTSERRAT_48) && LV_FONT_MONTSERRAT_48
        s_fonts.lg = &lv_font_montserrat_32;
        s_fonts.xl = &lv_font_montserrat_48;
#else
        s_fonts.lg = &lv_font_montserrat_28;
        s_fonts.xl = &lv_font_montserrat_32;
#endif
        s_init = true;
    }
    return &s_fonts;
}

static void no_scrollbar_draw_cb(lv_event_t *e)
{
    lv_obj_draw_part_dsc_t *dsc = lv_event_get_draw_part_dsc(e);
    if (dsc && (dsc->type == LV_OBJ_DRAW_PART_SCROLLBAR || dsc->part == LV_PART_SCROLLBAR)) {
        if (dsc->rect_dsc) {
            dsc->rect_dsc->bg_opa      = LV_OPA_TRANSP;
            dsc->rect_dsc->border_opa  = LV_OPA_TRANSP;
            dsc->rect_dsc->shadow_opa  = LV_OPA_TRANSP;
            dsc->rect_dsc->outline_opa = LV_OPA_TRANSP;
        }
        if (dsc->draw_area) {
            dsc->draw_area->x2 = dsc->draw_area->x1 - 1;
            dsc->draw_area->y2 = dsc->draw_area->y1 - 1;
        }
    }
}

void ui_theme_strip_scrollbar(lv_obj_t *obj)
{
    if (!obj) return;
    lv_obj_remove_style(obj, NULL, (lv_style_selector_t)LV_PART_SCROLLBAR | (lv_style_selector_t)LV_STATE_ANY);
    lv_obj_set_scrollbar_mode(obj, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_width(obj, 0, (lv_style_selector_t)LV_PART_SCROLLBAR | (lv_style_selector_t)LV_STATE_ANY);
    lv_obj_set_style_pad_all(obj, 0, (lv_style_selector_t)LV_PART_SCROLLBAR | (lv_style_selector_t)LV_STATE_ANY);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, (lv_style_selector_t)LV_PART_SCROLLBAR | (lv_style_selector_t)LV_STATE_ANY);
    lv_obj_set_style_border_opa(obj, LV_OPA_TRANSP, (lv_style_selector_t)LV_PART_SCROLLBAR | (lv_style_selector_t)LV_STATE_ANY);
    lv_obj_set_style_shadow_opa(obj, LV_OPA_TRANSP, (lv_style_selector_t)LV_PART_SCROLLBAR | (lv_style_selector_t)LV_STATE_ANY);
    lv_obj_add_event_cb(obj, no_scrollbar_draw_cb, LV_EVENT_DRAW_PART_BEGIN, NULL);
}

void ui_theme_style_tile(lv_obj_t *tile)
{
    lv_obj_set_style_bg_color(tile, CLR_BG, 0);
    lv_obj_set_style_bg_opa(tile, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(tile, 0, 0);
    lv_obj_set_style_border_width(tile, 0, 0);
    lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
    ui_theme_strip_scrollbar(tile);
}

lv_obj_t *ui_theme_make_divider(lv_obj_t *parent)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_set_size(line, lv_pct(100), 1);
    lv_obj_set_style_bg_color(line, CLR_BORDER, 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(line, 0, 0);
    lv_obj_set_style_radius(line, 0, 0);
    lv_obj_set_style_pad_all(line, 0, 0);
    lv_obj_clear_flag(line, LV_OBJ_FLAG_SCROLLABLE);
    return line;
}

lv_obj_t *ui_theme_make_key_label(lv_obj_t *parent, const char *text)
{
    const UiThemeFonts *f = ui_theme_fonts();
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, f->xs, 0);
    lv_obj_set_style_text_color(lbl, CLR_DIMTEXT, 0);
    return lbl;
}

lv_obj_t *ui_theme_make_val_label(lv_obj_t *parent, const char *init_text,
                                  lv_color_t color, const lv_font_t *font)
{
    lv_obj_t *lbl = lv_label_create(parent);
    lv_label_set_text(lbl, init_text);
    lv_obj_set_style_text_font(lbl, font, 0);
    lv_obj_set_style_text_color(lbl, color, 0);
    return lbl;
}

lv_obj_t *ui_theme_make_dot(lv_obj_t *parent, lv_color_t color, lv_coord_t size)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, size, size);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(dot, color, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);
    return dot;
}
