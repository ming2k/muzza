#ifndef MUZZA_UI_ICONS_H
#define MUZZA_UI_ICONS_H

#include "flux/flux.h"

// Draws a standard Play icon (Triangle)
static inline void draw_icon_play(fx_canvas* c, float x, float y, float size, fx_color color) {
    fx_path* p = fx_path_create();
    fx_path_move_to(p, x + size * 0.2f, y + size * 0.15f);
    fx_path_line_to(p, x + size * 0.85f, y + size * 0.5f);
    fx_path_line_to(p, x + size * 0.2f, y + size * 0.85f);
    fx_path_close(p);
    fx_paint paint;
    fx_paint_init(&paint, color);
    fx_fill_path(c, p, &paint);
    fx_path_destroy(p);
}

// Draws a Pause icon (Two bars)
static inline void draw_icon_pause(fx_canvas* c, float x, float y, float size, fx_color color) {
    fx_rect r1 = { x + size * 0.25f, y + size * 0.2f, size * 0.15f, size * 0.6f };
    fx_rect r2 = { x + size * 0.6f,  y + size * 0.2f, size * 0.15f, size * 0.6f };
    fx_fill_rect(c, &r1, color);
    fx_fill_rect(c, &r2, color);
}

// Draws a Film/Clip icon
static inline void draw_icon_clip(fx_canvas* c, float x, float y, float size, fx_color color) {
    fx_rect r = { x + size * 0.1f, y + size * 0.1f, size * 0.8f, size * 0.8f };
    fx_paint p;
    fx_paint_init(&p, color);
    p.stroke_width = 1.5f;
    fx_path* path = fx_path_create();
    fx_path_add_rect(path, &r);
    fx_stroke_path(c, path, &p);
    // Sprocket holes
    for (int i = 0; i < 3; i++) {
        fx_rect hole1 = { x + size * 0.15f, y + size * 0.2f + i * size * 0.25f, size * 0.1f, size * 0.15f };
        fx_rect hole2 = { x + size * 0.75f, y + size * 0.2f + i * size * 0.25f, size * 0.1f, size * 0.15f };
        fx_fill_rect(c, &hole1, color);
        fx_fill_rect(c, &hole2, color);
    }
    fx_path_destroy(path);
}

#endif
