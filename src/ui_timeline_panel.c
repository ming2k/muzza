#include "ui_timeline_panel.h"

#include <stdio.h>

#include "path_utils.h"
#include "ui_shared.h"
#include "ui_text.h"

static bool timeline_drop_target(const muzza_ui_state* state, float x, float y, float w, float h, double* out_time, int* out_track, float* out_line_x, float* out_track_y, float* out_track_h) {
    const float s = (state && state->ui_scale > 1.0f) ? state->ui_scale : 1.0f;
    const float header_w = 170.0f * s;
    float tracks_x = x + header_w;
    float tracks_w = w - header_w - 10.0f * s;
    float current_y = y + 38.0f * s;

    if (!state || !state->project || tracks_w <= 0.0f || state->project->duration <= 0.0) {
        return false;
    }

    if (!ui_point_in_rect(state->input.x, state->input.y, tracks_x, y + 28.0f * s, tracks_w, h - 28.0f * s)) {
        return false;
    }

    for (int track = 0; track < MUZZA_MAX_TRACKS; ++track) {
        float track_h = ui_clampf(state->project->track_heights[track] * s, 24.0f * s, h - 50.0f * s);

        if (ui_point_in_rect(state->input.x, state->input.y, tracks_x, current_y, tracks_w, track_h)) {
            float normalized = (state->input.x - tracks_x) / tracks_w;

            if (out_time) {
                *out_time = ui_clampf(normalized, 0.0f, 1.0f) * state->project->duration;
            }
            if (out_track) {
                *out_track = track;
            }
            if (out_line_x) {
                *out_line_x = tracks_x + ui_clampf(normalized, 0.0f, 1.0f) * tracks_w;
            }
            if (out_track_y) {
                *out_track_y = current_y;
            }
            if (out_track_h) {
                *out_track_h = track_h;
            }
            return true;
        }

        current_y += track_h + 1.0f * s;
        if (current_y >= y + h - 8.0f * s) {
            break;
        }
    }

    return false;
}

static void update_timeline_interactions(muzza_ui_state* state, muzza_ui_actions* actions, float x, float y, float w, float h) {
    const float s = state->ui_scale > 1.0f ? state->ui_scale : 1.0f;
    muzza_project* proj = state->project;
    muzza_timeline_state* timeline = &state->timeline;
    const float header_w = 170.0f * s;
    float tracks_x = x + header_w;
    float tracks_w = w - header_w - 10.0f * s;
    float current_y = y + 38.0f * s;
    int clicked_clip = -1;

    if (!proj || state->import_browser.visible) {
        return;
    }

    for (int track = 0; track < MUZZA_MAX_TRACKS; ++track) {
        float track_h = ui_clampf(proj->track_heights[track] * s, 24.0f * s, h - 50.0f * s);
        float edge_y = current_y + track_h;

        if (state->input.left_pressed && ui_point_in_rect(state->input.x, state->input.y, x, edge_y - 3.0f * s, w, 6.0f * s)) {
            timeline->is_dragging_track_edge = true;
            timeline->dragged_track_index = track;
        }

        if (state->input.left_down && timeline->is_dragging_track_edge && timeline->dragged_track_index == track) {
            proj->track_heights[track] = ui_clampf((state->input.y - current_y) / s, 24.0f, 160.0f);
            track_h = ui_clampf(proj->track_heights[track] * s, 24.0f * s, 160.0f * s);
        }

        for (size_t clip_index = 0; clip_index < proj->num_clips; ++clip_index) {
            const muzza_clip* clip = &proj->clips[clip_index];
            float clip_x = 0.0f;
            float clip_w = 0.0f;

            if (clip->track_index != track || proj->duration <= 0.0) {
                continue;
            }

            clip_x = tracks_x + (float)(clip->tl_start / proj->duration) * tracks_w;
            clip_w = (float)(clip->tl_duration / proj->duration) * tracks_w;
            if (clip_w < 10.0f * s) {
                clip_w = 10.0f * s;
            }

            if (state->input.left_pressed && ui_point_in_rect(state->input.x, state->input.y, clip_x, current_y + 2.0f * s, clip_w, track_h - 4.0f * s)) {
                clicked_clip = (int)clip_index;
            }
        }

        current_y += track_h + 1.0f * s;
        if (current_y >= y + h - 8.0f * s) {
            break;
        }
    }

    if (state->input.left_released) {
        timeline->is_dragging_track_edge = false;
    }

    if (clicked_clip >= 0) {
        project_clear_selection(proj);
        proj->clips[clicked_clip].selected = true;
        timeline->selected_clip_index = clicked_clip;
        state->media_panel.selected_media_index = proj->clips[clicked_clip].media_id;
        state->media_panel.is_dragging_media = false;
        state->media_panel.dragged_media_index = -1;
    } else if (state->input.left_pressed && ui_point_in_rect(state->input.x, state->input.y, tracks_x, y + 28.0f * s, tracks_w, h - 28.0f * s)) {
        float normalized = (state->input.x - tracks_x) / tracks_w;
        timeline->is_scrubbing = true;
        timeline->playhead_pos = ui_clampf(normalized, 0.0f, 1.0f);
        project_clear_selection(proj);
        timeline->selected_clip_index = -1;
    }

    if (state->input.left_down && timeline->is_scrubbing) {
        float normalized = (state->input.x - tracks_x) / tracks_w;
        timeline->playhead_pos = ui_clampf(normalized, 0.0f, 1.0f);
    }

    if (state->input.left_released) {
        timeline->is_scrubbing = false;
    }

    if (actions && state->media_panel.is_dragging_media && state->input.left_released) {
        double drop_time = 0.0;
        int drop_track = -1;

        if (timeline_drop_target(state, x, y, w, h, &drop_time, &drop_track, NULL, NULL, NULL)) {
            actions->insert_media_id = state->media_panel.dragged_media_index;
            actions->insert_timeline_time = drop_time;
            actions->insert_track_index = drop_track;
            timeline->playhead_pos = (float)(drop_time / proj->duration);
        }
    }
}

void ui_draw_timeline_panel(fx_canvas* canvas, muzza_ui_state* state, muzza_ui_actions* actions, float x, float y, float w, float h) {
    const float s = state->ui_scale > 1.0f ? state->ui_scale : 1.0f;
    muzza_project* proj = state->project;
    char time_text[32];
    const float header_w = 170.0f * s;
    float tracks_x = x + header_w;
    float tracks_w = w - header_w - 10.0f * s;
    float current_y = y + 38.0f * s;
    float playhead_x = tracks_x + state->timeline.playhead_pos * tracks_w;
    double drop_time = 0.0;
    int drop_track = -1;
    float drop_line_x = 0.0f;
    float drop_track_y = 0.0f;
    float drop_track_h = 0.0f;

    update_timeline_interactions(state, actions, x, y, w, h);
    snprintf(time_text, sizeof(time_text), "%.2fs", state->timeline.playhead_pos * proj->duration);
    ui_draw_text(canvas, x + 14.0f * s, y + 12.0f * s, "TIMELINE", 2.0f * s, 0xFFD7DEE3);
    ui_draw_text_right(canvas, x + w - 16.0f * s, y + 12.0f * s, time_text, 2.0f * s, 0xFF8FA0A9);

    for (int track = 0; track < MUZZA_MAX_TRACKS; ++track) {
        char track_label[24];
        float track_h = ui_clampf(proj->track_heights[track] * s, 24.0f * s, h - 50.0f * s);

        ui_draw_rect(canvas, x + 1.0f * s, current_y, header_w - 1.0f * s, track_h, MUZZA_COLOR_BG_HEADER);
        ui_draw_rect(canvas, tracks_x, current_y, tracks_w, track_h, (track % 2 == 0) ? MUZZA_COLOR_TRACK_BG : MUZZA_COLOR_TRACK_ALT);
        snprintf(track_label, sizeof(track_label), "TRACK %d", track + 1);
        ui_draw_text(canvas, x + 14.0f * s, current_y + 10.0f * s, track_label, 1.0f * s, 0xFFB8C3C9);
        ui_draw_rect(canvas, x, current_y + track_h, w, 1.0f * s, MUZZA_COLOR_TRACK_EDGE);

        for (size_t clip_index = 0; clip_index < proj->num_clips; ++clip_index) {
            const muzza_clip* clip = &proj->clips[clip_index];
            float clip_x = 0.0f;
            float clip_w = 0.0f;
            fx_color color = MUZZA_COLOR_CLIP;

            if (clip->track_index != track || proj->duration <= 0.0) {
                continue;
            }

            clip_x = tracks_x + (float)(clip->tl_start / proj->duration) * tracks_w;
            clip_w = (float)(clip->tl_duration / proj->duration) * tracks_w;
            if (clip_w < 10.0f * s) {
                clip_w = 10.0f * s;
            }

            if ((int)clip_index == state->timeline.selected_clip_index) {
                color = MUZZA_COLOR_CLIP_SEL;
            } else if ((int)clip_index == state->timeline.active_clip_index) {
                color = MUZZA_COLOR_ACCENT_DIM;
            }

            ui_draw_rect(canvas, clip_x, current_y + 2.0f * s, clip_w, track_h - 4.0f * s, color);
            ui_draw_border(canvas, clip_x, current_y + 2.0f * s, clip_w, track_h - 4.0f * s, s, MUZZA_COLOR_BORDER);
            if (clip->media_id >= 0 && (size_t)clip->media_id < proj->num_media) {
                ui_draw_text_ellipsis(
                    canvas,
                    clip_x + 6.0f * s,
                    current_y + 8.0f * s,
                    clip_w - 12.0f * s,
                    muzza_path_basename(proj->media_pool[clip->media_id].filepath),
                    1.0f * s,
                    0xFFF3F8FB
                );
            }
        }

        current_y += track_h + 1.0f * s;
        if (current_y >= y + h - 8.0f * s) {
            break;
        }
    }

    if (state->media_panel.is_dragging_media
        && timeline_drop_target(state, x, y, w, h, &drop_time, &drop_track, &drop_line_x, &drop_track_y, &drop_track_h)) {
        ui_draw_rect(canvas, drop_line_x - 1.0f * s, drop_track_y, 2.0f * s, drop_track_h, MUZZA_COLOR_ACCENT);
        ui_draw_rect(canvas, drop_line_x - 8.0f * s, drop_track_y, 16.0f * s, 10.0f * s, MUZZA_COLOR_ACCENT);
        ui_draw_border(canvas, tracks_x, drop_track_y, tracks_w, drop_track_h, s, MUZZA_COLOR_ACCENT_DIM);
    }

    ui_draw_rect(canvas, playhead_x - 1.0f * s, y + 28.0f * s, 2.0f * s, h - 28.0f * s, MUZZA_COLOR_ALERT);
    ui_draw_rect(canvas, playhead_x - 8.0f * s, y + 28.0f * s, 16.0f * s, 10.0f * s, MUZZA_COLOR_ALERT);
}
