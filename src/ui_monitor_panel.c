#include "ui_monitor_panel.h"

#include <stdio.h>

#include "ui_icons.h"
#include "ui_text.h"
#include "ui_shared.h"

void ui_draw_monitor_panel(fx_canvas* canvas, const muzza_ui_state* state, float x, float y, float w, float h) {
    float s = state->ui_scale > 1.0f ? state->ui_scale : 1.0f;
    char info_text[64];
    const char* mode_text = state->timeline.active_clip_index >= 0 ? "TIMELINE" : "SOURCE";
    float content_x = x + 20.0f * s;
    float content_y = y + 42.0f * s;
    float content_w = w - 40.0f * s;
    float content_h = h - 78.0f * s;

    ui_draw_text(canvas, x + 20.0f * s, y + 12.0f * s, "MONITOR", 2.0f * s, 0xFFD7DEE3);
    ui_draw_text_right(canvas, x + w - 20.0f * s, y + 12.0f * s, mode_text, 2.0f * s, 0xFF8FA0A9);
    ui_draw_rect(canvas, content_x, content_y, content_w, content_h, 0xFF050607);
    ui_draw_border(canvas, content_x, content_y, content_w, content_h, s, MUZZA_COLOR_BORDER);

    if (state->preview.current_frame && state->preview.frame_width > 0 && state->preview.frame_height > 0) {
        float scale = content_w / (float)state->preview.frame_width;
        float alt_scale = content_h / (float)state->preview.frame_height;
        float draw_w = 0.0f;
        float draw_h = 0.0f;

        if (alt_scale < scale) {
            scale = alt_scale;
        }

        draw_w = state->preview.frame_width * scale;
        draw_h = state->preview.frame_height * scale;
        fx_draw_image(
            canvas,
            state->preview.current_frame,
            NULL,
            &(fx_rect) {
                content_x + (content_w - draw_w) * 0.5f,
                content_y + (content_h - draw_h) * 0.5f,
                draw_w,
                draw_h,
            }
        );

        snprintf(info_text, sizeof(info_text), "%dx%d", state->preview.frame_width, state->preview.frame_height);
        ui_draw_text(canvas, content_x + 12.0f * s, content_y + content_h - 18.0f * s, info_text, 1.0f * s, 0xFFD7DEE3);
    } else {
        ui_draw_text_centered(canvas, content_x, content_y + content_h * 0.5f - 16.0f * s, content_w, 14.0f * s, "NO PREVIEW", 2.0f * s, 0xFF53616A);
        ui_draw_text_centered(canvas, content_x, content_y + content_h * 0.5f + 4.0f * s, content_w, 10.0f * s, "SELECT MEDIA OR MOVE PLAYHEAD", 1.0f * s, 0xFF53616A);
    }

    ui_draw_rect(canvas, x + 20.0f * s, y + h - 26.0f * s, 18.0f * s, 18.0f * s, MUZZA_COLOR_BG_HEADER);
    if (state->is_playing) {
        draw_icon_pause(canvas, x + 21.0f * s, y + h - 25.0f * s, 16.0f * s, 0xFFD7DEE3);
    } else {
        draw_icon_play(canvas, x + 21.0f * s, y + h - 25.0f * s, 16.0f * s, 0xFFD7DEE3);
    }
    ui_draw_text(canvas, x + 48.0f * s, y + h - 22.0f * s, "SPACE PLAY", 1.0f * s, 0xFF93A0A8);
}
