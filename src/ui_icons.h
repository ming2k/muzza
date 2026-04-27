#ifndef MUZZA_UI_ICONS_H
#define MUZZA_UI_ICONS_H

#include "flux/flux.h"

/* -------------------------------------------------------------------------- */
/* Deferred path destruction — fx_fill_path / fx_stroke_path record a         */
/* pointer to the path; the path must outlive fx_surface_present.  Since      */
/* icon functions are inline, they can't easily return paths to callers.      */
/* We cache them here and flush after present.                                */
/* -------------------------------------------------------------------------- */
#define ICON_PATH_CACHE_SIZE 128

typedef struct {
    fx_path* paths[ICON_PATH_CACHE_SIZE];
    int count;
} icon_path_cache;

extern icon_path_cache g_icon_paths;

static inline void icon_path_push(fx_path* p) {
    if (g_icon_paths.count < ICON_PATH_CACHE_SIZE) {
        g_icon_paths.paths[g_icon_paths.count++] = p;
    }
}

static inline void icon_path_flush(void) {
    for (int i = 0; i < g_icon_paths.count; i++) {
        fx_path_destroy(g_icon_paths.paths[i]);
    }
    g_icon_paths.count = 0;
}

/* ========================================================================== */
/* Icon Style Guide                                                           */
/* ========================================================================== */
/* All icons are vector-drawn via flux paths/shapes so they scale cleanly.   */
/*                                                                            */
/*  - Control icons (play, pause, arrows, close) use a STROKE style with     */
/*    rounded caps/joins for a modern, consistent look.                      */
/*  - Content icons (folder, file, clip) use FILL style with simple          */
/*    geometric silhouettes.                                                 */
/*                                                                            */
/*  Stroke icons: stroke_width = size * 0.12f, FX_CAP_ROUND / FX_JOIN_ROUND */
/*  Fill icons:   simple rects/paths with ~10-15% padding from the edge.     */
/* ========================================================================== */

/* -------------------------------------------------------------------------- */
/* Controls: Play (filled triangle)                                           */
/* -------------------------------------------------------------------------- */
static inline void draw_icon_play(fx_canvas* c, float x, float y, float size, fx_color color) {
    fx_path* p = fx_path_create();
    float pad = size * 0.18f;
    float right = x + size - pad;
    float cy = y + size * 0.5f;
    fx_path_move_to(p, x + pad, y + pad);
    fx_path_line_to(p, right, cy);
    fx_path_line_to(p, x + pad, y + size - pad);
    fx_path_close(p);
    fx_paint paint;
    fx_paint_init(&paint, color);
    fx_fill_path(c, p, &paint);
    icon_path_push(p);
}

/* -------------------------------------------------------------------------- */
/* Controls: Pause (two filled bars)                                          */
/* -------------------------------------------------------------------------- */
static inline void draw_icon_pause(fx_canvas* c, float x, float y, float size, fx_color color) {
    float pad = size * 0.18f;
    float bar_w = size * 0.18f;
    float gap = size * 0.14f;
    float bar_h = size - pad * 2.0f;
    fx_rect r1 = {x + pad, y + pad, bar_w, bar_h};
    fx_rect r2 = {x + pad + bar_w + gap, y + pad, bar_w, bar_h};
    fx_fill_rect(c, &r1, color);
    fx_fill_rect(c, &r2, color);
}

/* -------------------------------------------------------------------------- */
/* Content: Film / Clip                                                       */
/* -------------------------------------------------------------------------- */
static inline void draw_icon_clip(fx_canvas* c, float x, float y, float size, fx_color color) {
    fx_rect r = {x + size * 0.1f, y + size * 0.15f, size * 0.8f, size * 0.7f};
    fx_paint p;
    fx_paint_init(&p, color);
    p.stroke_width = size * 0.1f;
    fx_path* path = fx_path_create();
    fx_path_add_rect(path, &r);
    fx_stroke_path(c, path, &p);
    /* Sprocket holes */
    for (int i = 0; i < 3; i++) {
        fx_rect hole1 = {x + size * 0.15f, y + size * 0.22f + i * size * 0.22f, size * 0.08f, size * 0.12f};
        fx_rect hole2 = {x + size * 0.77f, y + size * 0.22f + i * size * 0.22f, size * 0.08f, size * 0.12f};
        fx_fill_rect(c, &hole1, color);
        fx_fill_rect(c, &hole2, color);
    }
    icon_path_push(path);
}

/* -------------------------------------------------------------------------- */
/* Controls: Close (X)                                                        */
/* -------------------------------------------------------------------------- */
static inline void draw_icon_close(fx_canvas* c, float x, float y, float size, fx_color color) {
    float pad = size * 0.25f;
    fx_paint paint;
    fx_paint_init(&paint, color);
    paint.stroke_width = size * 0.12f;
    paint.line_cap = FX_CAP_ROUND;

    fx_path* p = fx_path_create();
    fx_path_move_to(p, x + pad, y + pad);
    fx_path_line_to(p, x + size - pad, y + size - pad);
    fx_path_move_to(p, x + size - pad, y + pad);
    fx_path_line_to(p, x + pad, y + size - pad);
    fx_stroke_path(c, p, &paint);
    icon_path_push(p);
}

/* -------------------------------------------------------------------------- */
/* Controls: Arrow Up                                                         */
/* -------------------------------------------------------------------------- */
static inline void draw_icon_arrow_up(fx_canvas* c, float x, float y, float size, fx_color color) {
    float cx = x + size * 0.5f;
    float top = y + size * 0.2f;
    float stem_bottom = y + size * 0.8f;
    float wing = size * 0.3f;
    fx_paint paint;
    fx_paint_init(&paint, color);
    paint.stroke_width = size * 0.12f;
    paint.line_cap = FX_CAP_ROUND;

    fx_path* p = fx_path_create();
    fx_path_move_to(p, cx, top);
    fx_path_line_to(p, cx - wing, y + size * 0.55f);
    fx_path_move_to(p, cx, top);
    fx_path_line_to(p, cx + wing, y + size * 0.55f);
    fx_path_move_to(p, cx, top + size * 0.1f);
    fx_path_line_to(p, cx, stem_bottom);
    fx_stroke_path(c, p, &paint);
    icon_path_push(p);
}

/* -------------------------------------------------------------------------- */
/* Controls: Arrow Down                                                       */
/* -------------------------------------------------------------------------- */
static inline void draw_icon_arrow_down(fx_canvas* c, float x, float y, float size, fx_color color) {
    float cx = x + size * 0.5f;
    float bottom = y + size * 0.8f;
    float stem_top = y + size * 0.2f;
    float wing = size * 0.3f;
    fx_paint paint;
    fx_paint_init(&paint, color);
    paint.stroke_width = size * 0.12f;
    paint.line_cap = FX_CAP_ROUND;

    fx_path* p = fx_path_create();
    fx_path_move_to(p, cx, bottom);
    fx_path_line_to(p, cx - wing, y + size * 0.45f);
    fx_path_move_to(p, cx, bottom);
    fx_path_line_to(p, cx + wing, y + size * 0.45f);
    fx_path_move_to(p, cx, bottom - size * 0.1f);
    fx_path_line_to(p, cx, stem_top);
    fx_stroke_path(c, p, &paint);
    icon_path_push(p);
}

/* -------------------------------------------------------------------------- */
/* Controls: Arrow Left  (NEW)                                                */
/* -------------------------------------------------------------------------- */
static inline void draw_icon_arrow_left(fx_canvas* c, float x, float y, float size, fx_color color) {
    float left = x + size * 0.2f;
    float cy = y + size * 0.5f;
    float stem_right = x + size * 0.8f;
    float wing = size * 0.3f;
    fx_paint paint;
    fx_paint_init(&paint, color);
    paint.stroke_width = size * 0.12f;
    paint.line_cap = FX_CAP_ROUND;

    fx_path* p = fx_path_create();
    fx_path_move_to(p, left, cy);
    fx_path_line_to(p, left + wing, cy - wing);
    fx_path_move_to(p, left, cy);
    fx_path_line_to(p, left + wing, cy + wing);
    fx_path_move_to(p, left + size * 0.1f, cy);
    fx_path_line_to(p, stem_right, cy);
    fx_stroke_path(c, p, &paint);
    icon_path_push(p);
}

/* -------------------------------------------------------------------------- */
/* Controls: Arrow Right (NEW)                                                */
/* -------------------------------------------------------------------------- */
static inline void draw_icon_arrow_right(fx_canvas* c, float x, float y, float size, fx_color color) {
    float right = x + size * 0.8f;
    float cy = y + size * 0.5f;
    float stem_left = x + size * 0.2f;
    float wing = size * 0.3f;
    fx_paint paint;
    fx_paint_init(&paint, color);
    paint.stroke_width = size * 0.12f;
    paint.line_cap = FX_CAP_ROUND;

    fx_path* p = fx_path_create();
    fx_path_move_to(p, right, cy);
    fx_path_line_to(p, right - wing, cy - wing);
    fx_path_move_to(p, right, cy);
    fx_path_line_to(p, right - wing, cy + wing);
    fx_path_move_to(p, right - size * 0.1f, cy);
    fx_path_line_to(p, stem_left, cy);
    fx_stroke_path(c, p, &paint);
    icon_path_push(p);
}

/* -------------------------------------------------------------------------- */
/* Content: Folder                                                            */
/* -------------------------------------------------------------------------- */
static inline void draw_icon_folder(fx_canvas* c, float x, float y, float size, fx_color color) {
    float body_w = size * 0.85f;
    float body_h = size * 0.55f;
    float tab_w = size * 0.35f;
    float tab_h = size * 0.18f;
    fx_rect tab = {x + size * 0.08f, y, tab_w, tab_h};
    fx_rect body = {x + size * 0.08f, y + tab_h - 1.0f, body_w, body_h};
    fx_fill_rect(c, &tab, color);
    fx_fill_rect(c, &body, color);
}

/* -------------------------------------------------------------------------- */
/* Content: Document / File                                                   */
/* -------------------------------------------------------------------------- */
static inline void draw_icon_file(fx_canvas* c, float x, float y, float size, fx_color color) {
    float w = size * 0.65f;
    float h = size * 0.8f;
    float fold = size * 0.22f;
    fx_rect body = {x + size * 0.175f, y + size * 0.08f, w, h};
    fx_fill_rect(c, &body, color);
    fx_rect corner = {x + size * 0.175f + w - fold, y + size * 0.08f, fold, fold};
    fx_color corner_color = 0xFFBCC7CE;
    fx_fill_rect(c, &corner, corner_color);
}

/* ========================================================================== */
/* Tools: Razor — single diagonal blade line                                  */
/* ========================================================================== */
static inline void draw_icon_select(fx_canvas* c, float x, float y, float size, fx_color color) {
    float pad = size * 0.10f;
    fx_path* p = fx_path_create();
    // Arrow body: left-pointing chevron
    fx_path_move_to(p, x + size - pad, y + pad);
    fx_path_line_to(p, x + pad, y + size * 0.5f);
    fx_path_line_to(p, x + size - pad, y + size - pad);
    fx_path_close(p);
    fx_paint paint;
    fx_paint_init(&paint, color);
    fx_fill_path(c, p, &paint);
    icon_path_push(p);
}

/* ========================================================================== */
/* Tools: Razor — single diagonal blade line                                  */
/* ========================================================================== */
static inline void draw_icon_razor(fx_canvas* c, float x, float y, float size, fx_color color) {
    float pad = size * 0.25f;
    fx_paint paint;
    fx_paint_init(&paint, color);
    paint.stroke_width = size * 0.18f;
    paint.line_cap = FX_CAP_ROUND;

    fx_path* p = fx_path_create();
    fx_path_move_to(p, x + pad, y + size - pad);
    fx_path_line_to(p, x + size - pad, y + pad);
    fx_stroke_path(c, p, &paint);
    icon_path_push(p);
}

/* ========================================================================== */
/* Tools: Ripple — diagonal cross (delete/take marker)                        */
/* ========================================================================== */
static inline void draw_icon_ripple(fx_canvas* c, float x, float y, float size, fx_color color) {
    float pad = size * 0.22f;
    fx_paint paint;
    fx_paint_init(&paint, color);
    paint.stroke_width = size * 0.16f;
    paint.line_cap = FX_CAP_ROUND;

    fx_path* p = fx_path_create();
    fx_path_move_to(p, x + pad, y + pad);
    fx_path_line_to(p, x + size - pad, y + size - pad);
    fx_path_move_to(p, x + size - pad, y + pad);
    fx_path_line_to(p, x + pad, y + size - pad);
    fx_stroke_path(c, p, &paint);
    icon_path_push(p);
}

#endif
