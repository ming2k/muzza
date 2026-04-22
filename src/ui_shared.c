#include "ui_shared.h"

float ui_clampf(float value, float min_value, float max_value) {
    if (value < min_value) {
        return min_value;
    }
    if (value > max_value) {
        return max_value;
    }
    return value;
}

bool ui_point_in_rect(float x, float y, float rx, float ry, float rw, float rh) {
    return x >= rx && x <= rx + rw && y >= ry && y <= ry + rh;
}

void ui_draw_rect(fx_canvas* canvas, float x, float y, float w, float h, fx_color color) {
    fx_fill_rect(canvas, &(fx_rect) { x, y, w, h }, color);
}

void ui_draw_border(fx_canvas* canvas, float x, float y, float w, float h, float thickness, fx_color color) {
    ui_draw_rect(canvas, x, y, w, thickness, color);
    ui_draw_rect(canvas, x, y + h - thickness, w, thickness, color);
    ui_draw_rect(canvas, x, y, thickness, h, color);
    ui_draw_rect(canvas, x + w - thickness, y, thickness, h, color);
}

void ui_draw_panel(fx_canvas* canvas, float x, float y, float w, float h, float scale, fx_color accent) {
    ui_draw_rect(canvas, x, y, w, h, MUZZA_COLOR_BG_PANEL);
    ui_draw_rect(canvas, x, y, w, 28.0f * scale, MUZZA_COLOR_BG_HEADER);
    ui_draw_rect(canvas, x + 10.0f * scale, y + 9.0f * scale, 56.0f * scale, 8.0f * scale, accent);
    ui_draw_border(canvas, x, y, w, h, scale, MUZZA_COLOR_BORDER);
}
