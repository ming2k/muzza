#ifndef MUZZA_UI_TEXT_H
#define MUZZA_UI_TEXT_H

#include "flux/flux.h"

float ui_text_line_height(float scale);
float ui_text_measure(const char* text, float scale);
void ui_draw_text(fx_canvas* canvas, float x, float y, const char* text, float scale, fx_color color);
void ui_draw_text_centered(fx_canvas* canvas, float x, float y, float w, float h, const char* text, float scale, fx_color color);
void ui_draw_text_right(fx_canvas* canvas, float right_x, float y, const char* text, float scale, fx_color color);
void ui_draw_text_ellipsis(fx_canvas* canvas, float x, float y, float max_width, const char* text, float scale, fx_color color);

#endif
