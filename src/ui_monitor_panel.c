#include "ui_monitor_panel.h"

#include <stdio.h>
#include <math.h>

#include "path_utils.h"
#include "ui_icons.h"
#include "ui_text.h"
#include "ui_shared.h"

void ui_draw_monitor_panel(fx_canvas* canvas, const muzza_ui_state* state, float x, float y, float w, float h) {
    float s = state->ui_scale > 1.0f ? state->ui_scale : 1.0f;
    char info_text[128];
    const char* mode_text = state->timeline.active_clip_index >= 0 ? "TIMELINE" : "SOURCE";
    float content_x = x + 20.0f * s;
    float content_y = y + 42.0f * s;
    float content_w = w - 40.0f * s;
    float content_h = h - 44.0f * s - 30.0f * s - 6.0f * s; /* header + transport + gap */

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

        /* Fade-to-black overlay */
        if (state->playback.fade_opacity < 1.0f && state->playback.fade_opacity >= 0.0f) {
            int alpha = (int)((1.0f - state->playback.fade_opacity) * 200.0f);
            if (alpha > 0xFF) alpha = 0xFF;
            fx_color overlay = ((fx_color)alpha << 24) | 0x000000;
            ui_draw_rect(canvas, content_x, content_y, content_w, content_h, overlay);
            snprintf(info_text, sizeof(info_text), "fade %.0f%%", state->playback.fade_opacity * 100.0f);
            ui_draw_text_right(canvas, content_x + content_w - 8.0f * s, content_y + content_h - 18.0f * s, info_text, 1.0f * s, 0xFFE05757);
        }
    } else {
        /* Show clip inspector when a clip is selected */
        int sel = state->timeline.selected_clip_index;
        if (sel >= 0 && state->project && (size_t)sel < state->project->num_clips) {
            const muzza_clip* clip = &state->project->clips[sel];
            const char* bname = clip->media_id >= 0 && (size_t)clip->media_id < state->project->num_media
                ? muzza_path_basename(state->project->media_pool[clip->media_id].filepath) : "?";
            char buf[256];
            float ly = content_y + 8.0f * s;
            float lh = 16.0f * s;
            float pad = 12.0f * s;

            ui_draw_rect(canvas, content_x, content_y, content_w, 28.0f * s, MUZZA_COLOR_BG_HEADER);
            ui_draw_text(canvas, content_x + pad, ly, "CLIP INSPECTOR", 1.2f * s, 0xFFD7DEE3);
            ly += 32.0f * s;

            snprintf(buf, sizeof(buf), "Name:  %s", bname);
            ui_draw_text(canvas, content_x + pad, ly, buf, 1.1f * s, 0xFFF0F7F6); ly += lh;
            snprintf(buf, sizeof(buf), "Type:  %s   Track: %d",
                clip->type == MUZZA_CLIP_VIDEO ? "Video" : "Audio", clip->track_index + 1);
            ui_draw_text(canvas, content_x + pad, ly, buf, 1.1f * s, 0xFFAABBCC); ly += lh + 4.0f * s;

            snprintf(buf, sizeof(buf), "Start:  %.2fs   Dur: %.2fs   End: %.2fs",
                clip->tl_start, clip->tl_duration, clip->tl_start + clip->tl_duration);
            ui_draw_text(canvas, content_x + pad, ly, buf, 1.0f * s, 0xFF93A0A8); ly += lh;
            snprintf(buf, sizeof(buf), "Media:  in=%.2fs  dur=%.1fs",
                clip->media_in,
                clip->media_id >= 0 && (size_t)clip->media_id < state->project->num_media
                    ? state->project->media_pool[clip->media_id].duration : 0.0);
            ui_draw_text(canvas, content_x + pad, ly, buf, 1.0f * s, 0xFF93A0A8); ly += lh + 4.0f * s;

            snprintf(buf, sizeof(buf), "Fade:   In=%.2fs   Out=%.2fs",
                clip->fade_in_duration, clip->fade_out_duration);
            ui_draw_text(canvas, content_x + pad, ly, buf, 1.0f * s, 0xFF93A0A8); ly += lh + 4.0f * s;

            snprintf(buf, sizeof(buf), "Opacity: %.2f  Scale: %.2f  Pos: (%.0f, %.0f)",
                (double)clip->opacity, (double)clip->scale, (double)clip->pos_x, (double)clip->pos_y);
            ui_draw_text(canvas, content_x + pad, ly, buf, 1.0f * s, 0xFF7A8A9A); ly += lh;

            const char* filter_names[] = { "None", "Grayscale", "Invert" };
            int fidx = clip->filter >= 0 && clip->filter <= 2 ? clip->filter : 0;
            snprintf(buf, sizeof(buf), "Filter:  %s", filter_names[fidx]);
            ui_draw_text(canvas, content_x + pad, ly, buf, 1.0f * s, 0xFF7A8A9A); ly += lh;

            if (clip->link_group_id > 0) {
                snprintf(buf, sizeof(buf), "Linked:  group=%llu  role=%s",
                    (unsigned long long)clip->link_group_id,
                    clip->link_role == MUZZA_LINK_VIDEO ? "V" :
                    clip->link_role == MUZZA_LINK_AUDIO ? "A" : "none");
                ui_draw_text(canvas, content_x + pad, ly, buf, 1.0f * s, 0xFF555555); ly += lh;
            }

            if (state->project->tracks[clip->track_index].locked) {
                ui_draw_text(canvas, content_x + pad, ly, "[LOCKED]", 1.0f * s, 0xFFE05757);
            }
        } else {
            ui_draw_text_centered(canvas, content_x, content_y + content_h * 0.5f - 16.0f * s, content_w, 14.0f * s, "NO PREVIEW", 2.0f * s, 0xFF53616A);
            ui_draw_text_centered(canvas, content_x, content_y + content_h * 0.5f + 4.0f * s, content_w, 10.0f * s, "SELECT MEDIA OR MOVE PLAYHEAD", 1.0f * s, 0xFF53616A);
        }
    }

    /* ============================================== */
    /* Transport bar — speed slider + play control    */
    /* ============================================== */
    /* Transport bar — Monitor > SpeedControl         */
    /* ============================================== */
    float tb_margin = 16.0f * s;
    float tb_gap = 6.0f * s;
    float tb_h = 30.0f * s;
    float tb_y = y + h - tb_h - tb_gap;
    float tb_left = x + tb_margin;
    float tb_w = w - tb_margin * 2.0f;

    // Background
    ui_draw_rect(canvas, tb_left, tb_y, tb_w, tb_h, MUZZA_COLOR_BG_HEADER);
    ui_draw_border(canvas, tb_left, tb_y, tb_w, tb_h, 0.5f * s, MUZZA_COLOR_BORDER);

    // Play / Pause button
    float btn_pad = 6.0f * s;
    float play_btn_x = tb_left + btn_pad;
    float play_btn_sz = tb_h - btn_pad * 2.0f;
    ui_draw_rect(canvas, play_btn_x, tb_y + btn_pad, play_btn_sz, play_btn_sz, MUZZA_COLOR_TRACK_ALT);
    if (state->is_playing) {
        draw_icon_pause(canvas, play_btn_x + 1.0f * s, tb_y + btn_pad + 1.0f * s, play_btn_sz - 2.0f * s, 0xFFD7DEE3);
    } else {
        draw_icon_play(canvas, play_btn_x + 1.0f * s, tb_y + btn_pad + 1.0f * s, play_btn_sz - 2.0f * s, 0xFFD7DEE3);
    }

    // Speed slider
    float sl_label_w = 64.0f * s;
    float sl_x = play_btn_x + play_btn_sz + 14.0f * s;
    float sl_w = tb_left + tb_w - sl_label_w - 8.0f * s - sl_x;
    float sl_cy = tb_y + tb_h * 0.5f;

    if (sl_w > 40.0f * s) {
        // Track
        ui_draw_rect(canvas, sl_x, sl_cy - 2.5f * s, sl_w, 5.0f * s, MUZZA_COLOR_TRACK_EDGE);

        // Center notch
        float ctr_x = sl_x + sl_w * 0.5f;
        ui_draw_rect(canvas, ctr_x - 1.0f * s, sl_cy - 6.0f * s, 2.0f * s, 12.0f * s, 0xFF555555);

        // Preset notch marks
        float presets[] = { -8.0f, -4.0f, -2.0f, -1.0f, -0.5f, 0.0f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };
        for (int i = 0; i < 11; ++i) {
            float px = sl_x + (presets[i] / 16.0f + 0.5f) * sl_w;
            float mark_h = (presets[i] == 0.0f) ? 10.0f * s : 4.0f * s;
            ui_draw_rect(canvas, px - 0.5f * s, sl_cy - mark_h * 0.5f, 1.0f * s, mark_h, 0xFF555555);
        }

        // Filled track (from center to knob)
        float slide_pos = state->playback_speed / 16.0f + 0.5f;
        if (slide_pos < 0.0f) slide_pos = 0.0f;
        if (slide_pos > 1.0f) slide_pos = 1.0f;
        float fill_x = sl_x + sl_w * 0.5f; // start from center
        float fill_w = (slide_pos - 0.5f) * sl_w;
        if (fill_w < 0.0f) { fill_x = sl_x + slide_pos * sl_w; fill_w = (0.5f - slide_pos) * sl_w; }
        ui_draw_rect(canvas, fill_x, sl_cy - 2.5f * s, fill_w, 5.0f * s,
            state->playback_speed < 0.0f ? MUZZA_COLOR_ALERT : MUZZA_COLOR_ACCENT_DIM);

        // Knob
        float knob_x = sl_x + slide_pos * sl_w;
        float kr = 7.0f * s;
        ui_draw_rect(canvas, knob_x - kr, sl_cy - kr, kr * 2.0f, kr * 2.0f, MUZZA_COLOR_ACCENT);

        // Speed label — right side, centered, supports negative
        float sl_lx = sl_x + sl_w + 4.0f * s;
        {
            char sp_label[20];
            fx_color sp_color = 0xFF8FA0A9;
            if (state->playback_speed > 0.01f) {
                snprintf(sp_label, sizeof(sp_label), "+%.1fx", (double)state->playback_speed);
                sp_color = MUZZA_COLOR_ACCENT;
            } else if (state->playback_speed < -0.01f) {
                snprintf(sp_label, sizeof(sp_label), "%.1fx", (double)state->playback_speed);
                sp_color = MUZZA_COLOR_ALERT;
            } else {
                snprintf(sp_label, sizeof(sp_label), "Stop");
            }
            ui_draw_text_centered(canvas, sl_lx, tb_y, sl_label_w, tb_h, sp_label, 1.2f * s, sp_color);
        }

        // Slider interaction
        muzza_ui_state* mstate = (muzza_ui_state*)state;
        float sl_hit_x = sl_x - 4.0f * s;
        float sl_hit_w = sl_w + sl_label_w + 12.0f * s;
        if (mstate->input.left_pressed && ui_point_in_rect(mstate->input.x, mstate->input.y, sl_hit_x, tb_y, sl_hit_w, tb_h)) {
            mstate->is_dragging_speed_slider = true;
        }
        if (mstate->input.left_down && mstate->is_dragging_speed_slider) {
            float raw = (mstate->input.x - sl_x) / sl_w;
            if (raw < 0.0f) raw = 0.0f;
            if (raw > 1.0f) raw = 1.0f;
            mstate->playback_speed = (raw - 0.5f) * 16.0f;
        }
        if (mstate->input.left_released && mstate->is_dragging_speed_slider) {
            mstate->is_dragging_speed_slider = false;
            float spd = roundf(mstate->playback_speed * 20.0f) / 20.0f;
            float snap_list[] = { -8.0f, -4.0f, -2.0f, -1.0f, -0.5f, -0.25f, 0.0f, 0.25f, 0.5f, 1.0f, 2.0f, 4.0f, 8.0f };
            float best = spd;
            for (int i = 0; i < 13; ++i) {
                if (fabsf(spd - snap_list[i]) < 0.15f) { best = snap_list[i]; break; }
            }
            mstate->playback_speed = best;
        }
    }
}