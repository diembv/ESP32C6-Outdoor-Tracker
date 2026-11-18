/**
 * @file ui_layout.h
 * @brief Declarations for the 3 adaptive UI layout profile builders.
 */

#pragma once

#include <lvgl.h>
#include "ui/ui_widgets.h"

void ui_layout_build_portrait(lv_obj_t *scr, UiWidgets *w);
void ui_layout_build_round(lv_obj_t *scr, UiWidgets *w);
void ui_layout_build_landscape(lv_obj_t *scr, UiWidgets *w);
