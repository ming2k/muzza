#include "ui.h"
#include "ui_icons.h"

#define COLOR_BG_MAIN    0xFF121212
#define COLOR_BG_PANEL   0xFF1A1A1A
#define COLOR_BG_PANEL_H 0xFF222222
#define COLOR_BORDER     0xFF2A2A2A
#define COLOR_ACCENT     0xFF6F42C1 
#define COLOR_PLAYHEAD   0xFFFF3333
#define COLOR_TRACK_BG   0xFF161616
#define COLOR_TRACK_ALT  0xFF1A1A1A
#define COLOR_CLIP_VID   0xFF2B5B84
#define COLOR_TEXT_DIM   0xFF555555
#define COLOR_TEXT_BRT   0xFFAAAAAA

static void draw_border(fx_canvas* canvas, float x, float y, float w, float h, float thickness, fx_color color) {
    fx_rect r1 = { x, y, w, thickness }; fx_fill_rect(canvas, &r1, color);
    fx_rect r2 = { x, y + h - thickness, w, thickness }; fx_fill_rect(canvas, &r2, color);
    fx_rect r3 = { x, y, thickness, h }; fx_fill_rect(canvas, &r3, color);
    fx_rect r4 = { x + w - thickness, y, thickness, h }; fx_fill_rect(canvas, &r4, color);
}

static void draw_panel(fx_canvas* canvas, float x, float y, float w, float h) {
    fx_rect r = { x, y, w, h };
    fx_fill_rect(canvas, &r, COLOR_BG_PANEL);
    fx_rect rh = { x, y, w, 28.0f };
    fx_fill_rect(canvas, &rh, COLOR_BG_PANEL_H);
    draw_border(canvas, x, y, w, h, 1.0f, COLOR_BORDER);
}

void ui_render(fx_canvas* canvas, muzza_ui_state* state) {
    float W = (float)state->window_width;
    float H = (float)state->window_height;
    if (W <= 0 || H <= 0) return;

    fx_clear(canvas, COLOR_BG_MAIN);

    float menu_h = 32.0f;
    float tl_h = H * 0.38f;
    float main_h = H - menu_h - tl_h;
    
    // 1. Top Menu
    fx_rect r_menu = { 0, 0, W, menu_h };
    fx_fill_rect(canvas, &r_menu, COLOR_BG_PANEL);
    
    // 2. Asset Bin (Left)
    float left_w = W * 0.22f;
    draw_panel(canvas, 0, menu_h, left_w, main_h);
    for(int i=0; i<4; i++) {
        draw_icon_clip(canvas, 15, menu_h + 45 + i*40, 20, COLOR_ACCENT);
    }
    
    // 3. Program Monitor (Right)
    float right_w = W - left_w;
    draw_panel(canvas, left_w, menu_h, right_w, main_h);
    if (state->current_frame) {
        float video_padding = 40.0f;
        float v_target_w = right_w - video_padding * 2;
        float v_target_h = main_h - 28.0f - video_padding * 2;
        float scale = v_target_w / state->frame_width;
        if (v_target_h / state->frame_height < scale) scale = v_target_h / state->frame_height;
        float dw = state->frame_width * scale;
        float dh = state->frame_height * scale;
        float dx = left_w + (right_w - dw) / 2.0f;
        float dy = menu_h + 28.0f + (main_h - 28.0f - dh) / 2.0f;
        fx_rect dst = { dx, dy, dw, dh };
        fx_rect src = { 0, 0, state->frame_width, state->frame_height };
        fx_fill_rect(canvas, &dst, 0xFF000000);
        fx_draw_image(canvas, state->current_frame, &src, &dst);
        draw_border(canvas, dx, dy, dw, dh, 1.0f, COLOR_BORDER);
    }

    // 4. Bottom Timeline
    float tl_y = H - tl_h;
    draw_panel(canvas, 0, tl_y, W, tl_h);
    float track_header_w = 180.0f;
    float tl_interact_x = track_header_w;
    float tl_interact_w = W - track_header_w;
    float tl_content_y = tl_y + 28.0f;

    if (state->mouse_down) {
        if (state->mouse_y >= tl_content_y && state->mouse_y <= H) {
            if (state->mouse_x >= tl_interact_x) {
                state->playhead_pos = (state->mouse_x - tl_interact_x) / tl_interact_w;
                if (state->playhead_pos < 0) state->playhead_pos = 0;
                if (state->playhead_pos > 1) state->playhead_pos = 1;
            }
        }
    }

    for (int i = 0; i < 4; i++) {
        float ty = tl_content_y + 10 + i * 50;
        if (ty + 50 > H) break;
        fx_rect tr = { tl_interact_x, ty, tl_interact_w, 48 };
        fx_fill_rect(canvas, &tr, (i%2==0) ? COLOR_TRACK_BG : COLOR_TRACK_ALT);
        fx_rect th = { 0, ty, track_header_w - 1, 48 };
        fx_fill_rect(canvas, &th, COLOR_BG_PANEL);
        if (i == 0) {
            fx_rect clip = { tl_interact_x + 20, ty + 4, tl_interact_w * 0.8f, 40 };
            fx_fill_rect(canvas, &clip, COLOR_CLIP_VID);
            draw_border(canvas, clip.x, clip.y, clip.w, clip.h, 1.0f, COLOR_BORDER);
        }
    }

    float ph_x = tl_interact_x + state->playhead_pos * tl_interact_w;
    fx_rect line = { ph_x - 1, tl_content_y, 2, tl_h - 28 };
    fx_fill_rect(canvas, &line, COLOR_PLAYHEAD);
    fx_rect head = { ph_x - 8, tl_content_y, 16, 14 };
    fx_fill_rect(canvas, &head, COLOR_PLAYHEAD);
}
