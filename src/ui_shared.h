#ifndef MUZZA_UI_SHARED_H
#define MUZZA_UI_SHARED_H

#include <stdbool.h>

#include "flux/flux.h"

#define MUZZA_COLOR_BG_MAIN       0xFF101214
#define MUZZA_COLOR_BG_PANEL      0xFF171A1D
#define MUZZA_COLOR_BG_HEADER     0xFF20252A
#define MUZZA_COLOR_BG_OVERLAY    0xCC0A0C0E
#define MUZZA_COLOR_BORDER        0xFF2B3238
#define MUZZA_COLOR_ACCENT        0xFF3AA7A3
#define MUZZA_COLOR_ACCENT_DIM    0xFF2D6E6B
#define MUZZA_COLOR_ALERT         0xFFE05757
#define MUZZA_COLOR_TRACK_BG      0xFF121518
#define MUZZA_COLOR_TRACK_ALT     0xFF161B1F
#define MUZZA_COLOR_TRACK_EDGE    0xFF35414A
#define MUZZA_COLOR_CLIP          0xFF2A567A
#define MUZZA_COLOR_CLIP_SEL      0xFF3A7FB6
#define MUZZA_COLOR_MEDIA_VIDEO   0xFF2F698A
#define MUZZA_COLOR_MEDIA_AUDIO   0xFF9B6B32
#define MUZZA_COLOR_MEDIA_MIXED   0xFF3C7A56
#define MUZZA_COLOR_MEDIA_IMAGE   0xFF8B5A8C
#define MUZZA_COLOR_MEDIA_EMPTY   0xFF4B555C
#define MUZZA_COLOR_DIALOG_ROW    0xFF13181C
#define MUZZA_COLOR_DIALOG_ROW_HI 0xFF1D262C

float ui_clampf(float value, float min_value, float max_value);
bool ui_point_in_rect(float x, float y, float rx, float ry, float rw, float rh);
void ui_draw_rect(fx_canvas* canvas, float x, float y, float w, float h, fx_color color);
void ui_draw_border(fx_canvas* canvas, float x, float y, float w, float h, float thickness, fx_color color);
void ui_draw_panel(fx_canvas* canvas, float x, float y, float w, float h, float scale, fx_color accent);

#endif
