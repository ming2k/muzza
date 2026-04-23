#include "ui_timeline_panel.h"

#include <stdio.h>
#include <math.h>

#include "path_utils.h"
#include "ui_shared.h"
#include "ui_text.h"

static float time_to_x(double time, float tracks_x, double scroll_x, float zoom, float s) {
    return tracks_x + (float)(time - scroll_x) * zoom * s;
}

static double x_to_time(float x_coord, float tracks_x, double scroll_x, float zoom, float s) {
    return (double)((x_coord - tracks_x) / (zoom * s)) + scroll_x;
}

static bool timeline_drop_target(const muzza_ui_state* state, float x, float y, float w, float h, double* out_time, int* out_track, float* out_line_x, float* out_track_y, float* out_track_h) {
    const float s = (state && state->ui_scale > 1.0f) ? state->ui_scale : 1.0f;
    const float header_w = 170.0f * s;
    float tracks_x = x + header_w;
    float tracks_w = w - header_w - 10.0f * s;
    float current_y = y + 38.0f * s;
    const muzza_timeline_state* timeline = &state->timeline;

    if (!state || !state->project || tracks_w <= 0.0f) {
        return false;
    }

    if (!ui_point_in_rect(state->input.x, state->input.y, tracks_x, y + 28.0f * s, tracks_w, h - 28.0f * s)) {
        return false;
    }

    for (int track = 0; track < MUZZA_MAX_TRACKS; ++track) {
        float track_h = ui_clampf(state->project->track_heights[track] * s, 24.0f * s, h - 50.0f * s);

        if (ui_point_in_rect(state->input.x, state->input.y, tracks_x, current_y, tracks_w, track_h)) {
            double time = x_to_time(state->input.x, tracks_x, timeline->scroll_x, timeline->zoom, s);

            if (out_time) {
                *out_time = time < 0.0 ? 0.0 : time;
            }
            if (out_track) {
                *out_track = track;
            }
            if (out_line_x) {
                *out_line_x = state->input.x;
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
    int trim_hover_clip = -1;
    int trim_hover_edge = 0;

    if (!proj || state->import_browser.visible || state->export_panel.visible) {
        return;
    }

    // Toolbar hit test (header area, right of TIMELINE label)
    float toolbar_x = x + 90.0f * s;
    float toolbar_y = y + 8.0f * s;
    float btn_w = 34.0f * s;
    float btn_h = 18.0f * s;
    if (state->input.left_pressed) {
        if (ui_point_in_rect(state->input.x, state->input.y, toolbar_x, toolbar_y, btn_w, btn_h)) {
            timeline->active_tool = MUZZA_TOOL_SELECT;
        } else if (ui_point_in_rect(state->input.x, state->input.y, toolbar_x + btn_w + 4.0f * s, toolbar_y, btn_w, btn_h)) {
            timeline->active_tool = MUZZA_TOOL_RAZOR;
        }
    }

    // Handle Zooming and Scrolling
    if (ui_point_in_rect(state->input.x, state->input.y, x, y, w, h)) {
        if (state->input.ctrl_down || state->input.zoom_in_pressed || state->input.zoom_out_pressed) {
            float zoom_factor = 0.0f;
            if (state->input.wheel_y > 0.0f || state->input.zoom_in_pressed) zoom_factor = 1.15f;
            else if (state->input.wheel_y < 0.0f || state->input.zoom_out_pressed) zoom_factor = 1.0f / 1.15f;

            if (zoom_factor != 0.0f) {
                double mouse_time = x_to_time(state->input.x, tracks_x, timeline->scroll_x, timeline->zoom, s);
                timeline->zoom *= zoom_factor;
                timeline->zoom = ui_clampf(timeline->zoom, 1.0f, 10000.0f);
                
                double new_mouse_time = x_to_time(state->input.x, tracks_x, timeline->scroll_x, timeline->zoom, s);
                timeline->scroll_x += (mouse_time - new_mouse_time);
            }
        } else {
            if (state->input.wheel_y != 0.0f) {
                timeline->scroll_x -= (double)(state->input.wheel_y * 100.0f / (timeline->zoom * s));
            }
            if (state->input.wheel_x != 0.0f) {
                timeline->scroll_x += (double)(state->input.wheel_x * 100.0f / (timeline->zoom * s));
            }
        }
    }
    if (timeline->scroll_x < 0.0) timeline->scroll_x = 0.0;

    // Trim drag handling
    if (timeline->is_trimming && timeline->trim_clip_index >= 0 && state->input.left_down) {
        muzza_clip* clip = &proj->clips[timeline->trim_clip_index];
        double mouse_time = x_to_time(state->input.x, tracks_x, timeline->scroll_x, timeline->zoom, s);
        muzza_media* media = project_get_media(proj, clip->media_id);
        double media_dur = media ? media->duration : 0.0;

        if (timeline->trim_edge == -1) {
            // Left trim: adjust tl_start and media_in, keep right edge fixed
            double old_end = timeline->trim_original_start + timeline->trim_original_duration;
            double delta = mouse_time - timeline->trim_original_start;
            double new_start = timeline->trim_original_start + delta;
            double new_media_in = timeline->trim_original_media_in + delta;
            double new_dur = old_end - new_start;

            if (new_media_in < 0.0) { new_media_in = 0.0; new_start = timeline->trim_original_start - timeline->trim_original_media_in; new_dur = old_end - new_start; }
            if (new_dur < 0.1) { new_dur = 0.1; new_start = old_end - 0.1; new_media_in = timeline->trim_original_media_in + (new_start - timeline->trim_original_start); }
            if (new_media_in + new_dur > media_dur && media_dur > 0.0) {
                double overflow = (new_media_in + new_dur) - media_dur;
                new_media_in -= overflow;
                new_start -= overflow;
                new_dur = old_end - new_start;
            }
            if (new_media_in < 0.0) new_media_in = 0.0;
            if (new_start < 0.0) { new_start = 0.0; new_dur = old_end; new_media_in = timeline->trim_original_media_in - timeline->trim_original_start; }

            clip->tl_start = new_start;
            clip->media_in = new_media_in;
            clip->tl_duration = new_dur;
        } else if (timeline->trim_edge == 1) {
            // Right trim: adjust tl_duration, keep left edge fixed
            double new_dur = mouse_time - timeline->trim_original_start;
            if (new_dur < 0.1) new_dur = 0.1;
            if (timeline->trim_original_media_in + new_dur > media_dur && media_dur > 0.0) {
                new_dur = media_dur - timeline->trim_original_media_in;
            }
            clip->tl_duration = new_dur;
        }
        project_recalculate_duration(proj);
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
            if (clip->track_index != track) continue;

            float clip_x = time_to_x(clip->tl_start, tracks_x, timeline->scroll_x, timeline->zoom, s);
            float clip_w = (float)clip->tl_duration * timeline->zoom * s;
            if (clip_w < 4.0f * s) clip_w = 4.0f * s;

            float body_y = current_y + 2.0f * s;
            float body_h = track_h - 4.0f * s;

            // Check hover for trim handles (SELECT) or clip body (RAZOR)
            if (timeline->active_tool == MUZZA_TOOL_SELECT && !timeline->is_trimming) {
                if (ui_point_in_rect(state->input.x, state->input.y, clip_x, body_y, 6.0f * s, body_h)) {
                    trim_hover_clip = (int)clip_index;
                    trim_hover_edge = -1;
                } else if (ui_point_in_rect(state->input.x, state->input.y, clip_x + clip_w - 6.0f * s, body_y, 6.0f * s, body_h)) {
                    trim_hover_clip = (int)clip_index;
                    trim_hover_edge = 1;
                }
            } else if (timeline->active_tool == MUZZA_TOOL_RAZOR && !timeline->is_trimming) {
                if (ui_point_in_rect(state->input.x, state->input.y, clip_x, body_y, clip_w, body_h)) {
                    trim_hover_clip = (int)clip_index;
                    trim_hover_edge = 0;
                }
            }

            // Click detection
            if (state->input.left_pressed && ui_point_in_rect(state->input.x, state->input.y, clip_x, body_y, clip_w, body_h)) {
                // In SELECT mode, check if clicking on a trim handle
                if (timeline->active_tool == MUZZA_TOOL_SELECT) {
                    if (ui_point_in_rect(state->input.x, state->input.y, clip_x, body_y, 6.0f * s, body_h)) {
                        timeline->is_trimming = true;
                        timeline->trim_clip_index = (int)clip_index;
                        timeline->trim_edge = -1;
                        timeline->trim_start_time = x_to_time(state->input.x, tracks_x, timeline->scroll_x, timeline->zoom, s);
                        timeline->trim_original_start = clip->tl_start;
                        timeline->trim_original_duration = clip->tl_duration;
                        timeline->trim_original_media_in = clip->media_in;
                        project_clear_selection(proj);
                        proj->clips[clip_index].selected = true;
                        timeline->selected_clip_index = (int)clip_index;
                    } else if (ui_point_in_rect(state->input.x, state->input.y, clip_x + clip_w - 6.0f * s, body_y, 6.0f * s, body_h)) {
                        timeline->is_trimming = true;
                        timeline->trim_clip_index = (int)clip_index;
                        timeline->trim_edge = 1;
                        timeline->trim_start_time = x_to_time(state->input.x, tracks_x, timeline->scroll_x, timeline->zoom, s);
                        timeline->trim_original_start = clip->tl_start;
                        timeline->trim_original_duration = clip->tl_duration;
                        timeline->trim_original_media_in = clip->media_in;
                        project_clear_selection(proj);
                        proj->clips[clip_index].selected = true;
                        timeline->selected_clip_index = (int)clip_index;
                    } else {
                        clicked_clip = (int)clip_index;
                    }
                } else if (timeline->active_tool == MUZZA_TOOL_RAZOR && actions) {
                    // Razor: split clip at click position
                    actions->split_clip_index = (int)clip_index;
                    actions->split_time = x_to_time(state->input.x, tracks_x, timeline->scroll_x, timeline->zoom, s);
                }
            }
        }

        current_y += track_h + 1.0f * s;
        if (current_y >= y + h - 8.0f * s) {
            break;
        }
    }

    // Store trim hover for rendering
    timeline->trim_hover_clip = trim_hover_clip;
    timeline->trim_hover_edge = trim_hover_edge;

    if (state->input.left_released) {
        timeline->is_dragging_track_edge = false;
        timeline->is_trimming = false;
        timeline->trim_clip_index = -1;
        timeline->trim_edge = 0;
    }

    if (clicked_clip >= 0 && !timeline->is_trimming) {
        project_clear_selection(proj);
        proj->clips[clicked_clip].selected = true;
        timeline->selected_clip_index = clicked_clip;
        state->media_panel.selected_media_index = proj->clips[clicked_clip].media_id;
        state->media_panel.is_dragging_media = false;
        state->media_panel.dragged_media_index = -1;

        // Start clip dragging
        timeline->is_dragging_clip = true;
        timeline->dragged_clip_index = clicked_clip;
        timeline->drag_start_offset = x_to_time(state->input.x, tracks_x, timeline->scroll_x, timeline->zoom, s) - proj->clips[clicked_clip].tl_start;
    } else if (state->input.left_pressed && !timeline->is_trimming && ui_point_in_rect(state->input.x, state->input.y, tracks_x, y + 28.0f * s, tracks_w, h - 28.0f * s)) {
        timeline->is_scrubbing = true;
        timeline->playhead_time = x_to_time(state->input.x, tracks_x, timeline->scroll_x, timeline->zoom, s);
        if (timeline->playhead_time < 0.0) timeline->playhead_time = 0.0;
        project_clear_selection(proj);
        timeline->selected_clip_index = -1;
    }

    if (state->input.left_down && timeline->is_dragging_clip && timeline->dragged_clip_index >= 0 && !timeline->is_trimming) {
        muzza_clip* clip = &proj->clips[timeline->dragged_clip_index];
        double new_tl_start = x_to_time(state->input.x, tracks_x, timeline->scroll_x, timeline->zoom, s) - timeline->drag_start_offset;
        clip->tl_start = new_tl_start < 0.0 ? 0.0 : new_tl_start;

        // Track dragging
        double drop_time = 0.0;
        int drop_track = -1;
        if (timeline_drop_target(state, x, y, w, h, &drop_time, &drop_track, NULL, NULL, NULL)) {
            if (drop_track >= 0 && drop_track < MUZZA_MAX_TRACKS) {
                clip->track_index = drop_track;
            }
        }
        project_recalculate_duration(proj);
    }

    if (state->input.left_down && timeline->is_scrubbing && !timeline->is_trimming) {
        timeline->playhead_time = x_to_time(state->input.x, tracks_x, timeline->scroll_x, timeline->zoom, s);
        if (timeline->playhead_time < 0.0) timeline->playhead_time = 0.0;
    }

    if (state->input.left_released) {
        timeline->is_scrubbing = false;
        timeline->is_dragging_clip = false;
        timeline->dragged_clip_index = -1;
    }

    if (actions && state->media_panel.is_dragging_media && state->input.left_released) {
        double drop_time = 0.0;
        int drop_track = -1;

        if (timeline_drop_target(state, x, y, w, h, &drop_time, &drop_track, NULL, NULL, NULL)) {
            actions->insert_media_id = state->media_panel.dragged_media_index;
            actions->insert_timeline_time = drop_time;
            actions->insert_track_index = drop_track;
            timeline->playhead_time = drop_time;
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
    muzza_timeline_state* timeline = &state->timeline;

    update_timeline_interactions(state, actions, x, y, w, h);

    float playhead_x = time_to_x(timeline->playhead_time, tracks_x, timeline->scroll_x, timeline->zoom, s);

    snprintf(time_text, sizeof(time_text), "%.2fs / %.2fs", timeline->playhead_time, proj->duration);
    ui_draw_text(canvas, x + 14.0f * s, y + 12.0f * s, "TIMELINE", 2.0f * s, 0xFFD7DEE3);

    // Toolbar
    float toolbar_x = x + 90.0f * s;
    float toolbar_y = y + 8.0f * s;
    float btn_w = 34.0f * s;
    float btn_h = 18.0f * s;
    fx_color sel_color = (timeline->active_tool == MUZZA_TOOL_SELECT) ? MUZZA_COLOR_ACCENT : MUZZA_COLOR_TRACK_ALT;
    fx_color cut_color = (timeline->active_tool == MUZZA_TOOL_RAZOR) ? MUZZA_COLOR_ACCENT : MUZZA_COLOR_TRACK_ALT;
    ui_draw_rect(canvas, toolbar_x, toolbar_y, btn_w, btn_h, sel_color);
    ui_draw_border(canvas, toolbar_x, toolbar_y, btn_w, btn_h, s, MUZZA_COLOR_BORDER);
    ui_draw_text_centered(canvas, toolbar_x, toolbar_y, btn_w, btn_h, "SEL", 1.0f * s, 0xFFF0F7F6);
    ui_draw_rect(canvas, toolbar_x + btn_w + 4.0f * s, toolbar_y, btn_w, btn_h, cut_color);
    ui_draw_border(canvas, toolbar_x + btn_w + 4.0f * s, toolbar_y, btn_w, btn_h, s, MUZZA_COLOR_BORDER);
    ui_draw_text_centered(canvas, toolbar_x + btn_w + 4.0f * s, toolbar_y, btn_w, btn_h, "CUT", 1.0f * s, 0xFFF0F7F6);

    ui_draw_text_right(canvas, x + w - 16.0f * s, y + 12.0f * s, time_text, 2.0f * s, 0xFF8FA0A9);

    // Draw background for tracks area
    ui_draw_rect(canvas, tracks_x, y + 28.0f * s, tracks_w, h - 28.0f * s, MUZZA_COLOR_TRACK_BG);

    // Draw Time Ticks
    double tick_spacing = 1.0;
    if (timeline->zoom * s < 10.0f) tick_spacing = 10.0;
    else if (timeline->zoom * s < 30.0f) tick_spacing = 5.0;
    else if (timeline->zoom * s < 80.0f) tick_spacing = 2.0;
    else if (timeline->zoom * s > 400.0f) tick_spacing = 0.2;
    else if (timeline->zoom * s > 1000.0f) tick_spacing = 0.1;

    double first_tick = floor(timeline->scroll_x / tick_spacing) * tick_spacing;
    for (double t = first_tick; ; t += tick_spacing) {
        float tx = time_to_x(t, tracks_x, timeline->scroll_x, timeline->zoom, s);
        if (tx > tracks_x + tracks_w) break;
        if (tx >= tracks_x) {
            ui_draw_rect(canvas, tx, y + 32.0f * s, 1.0f * s, 6.0f * s, 0xFF4A5661);
            if (fmod(t, tick_spacing * 5.0) < tick_spacing * 0.1) {
                char tick_label[16];
                snprintf(tick_label, sizeof(tick_label), "%.1fs", t);
                ui_draw_text(canvas, tx + 2.0f * s, y + 22.0f * s, tick_label, 0.8f * s, 0xFF4A5661);
                ui_draw_rect(canvas, tx, y + 28.0f * s, 1.0f * s, 10.0f * s, 0xFF6A7681);
            }
        }
    }

    for (int track = 0; track < MUZZA_MAX_TRACKS; ++track) {
        float track_h = ui_clampf(proj->track_heights[track] * s, 24.0f * s, h - 50.0f * s);

        // Track background (only for the alternate tracks)
        if (track % 2 != 0) {
            ui_draw_rect(canvas, tracks_x, current_y, tracks_w, track_h, MUZZA_COLOR_TRACK_ALT);
        }
        ui_draw_rect(canvas, x, current_y + track_h, w, 1.0f * s, MUZZA_COLOR_TRACK_EDGE);

        for (size_t clip_index = 0; clip_index < proj->num_clips; ++clip_index) {
            const muzza_clip* clip = &proj->clips[clip_index];
            if (clip->track_index != track) continue;

            float clip_x = time_to_x(clip->tl_start, tracks_x, timeline->scroll_x, timeline->zoom, s);
            float clip_w = (float)clip->tl_duration * timeline->zoom * s;

            // Only draw if visible
            if (clip_x + clip_w < tracks_x || clip_x > tracks_x + tracks_w) continue;

            float draw_x = clip_x;
            float draw_w = clip_w;
            if (draw_x < tracks_x) {
                draw_w -= (tracks_x - draw_x);
                draw_x = tracks_x;
            }
            if (draw_x + draw_w > tracks_x + tracks_w) {
                draw_w = (tracks_x + tracks_w) - draw_x;
            }
            if (draw_w <= 0) continue;

            fx_color color = MUZZA_COLOR_CLIP;
            if ((int)clip_index == timeline->selected_clip_index) {
                color = MUZZA_COLOR_CLIP_SEL;
            } else if ((int)clip_index == timeline->active_clip_index) {
                color = MUZZA_COLOR_ACCENT_DIM;
            } else if (clip->type == MUZZA_CLIP_AUDIO) {
                color = 0xFF4A6B5D;
            }

            ui_draw_rect(canvas, draw_x, current_y + 2.0f * s, draw_w, track_h - 4.0f * s, color);

            // Draw trim handle highlights
            if (timeline->active_tool == MUZZA_TOOL_SELECT) {
                bool hover_left = ((int)clip_index == timeline->trim_hover_clip && timeline->trim_hover_edge == -1);
                bool hover_right = ((int)clip_index == timeline->trim_hover_clip && timeline->trim_hover_edge == 1);
                bool trim_left = (timeline->is_trimming && timeline->trim_clip_index == (int)clip_index && timeline->trim_edge == -1);
                bool trim_right = (timeline->is_trimming && timeline->trim_clip_index == (int)clip_index && timeline->trim_edge == 1);
                if (hover_left || trim_left) {
                    ui_draw_rect(canvas, clip_x, current_y + 2.0f * s, 2.0f * s, track_h - 4.0f * s, MUZZA_COLOR_ACCENT);
                }
                if (hover_right || trim_right) {
                    ui_draw_rect(canvas, clip_x + clip_w - 2.0f * s, current_y + 2.0f * s, 2.0f * s, track_h - 4.0f * s, MUZZA_COLOR_ACCENT);
                }
            }

            // Draw razor preview line
            if (timeline->active_tool == MUZZA_TOOL_RAZOR && !timeline->is_trimming) {
                if ((int)clip_index == timeline->trim_hover_clip) {
                    float razor_x = state->input.x;
                    if (razor_x >= clip_x && razor_x <= clip_x + clip_w) {
                        ui_draw_rect(canvas, razor_x - 1.0f * s, current_y + 2.0f * s, 2.0f * s, track_h - 4.0f * s, MUZZA_COLOR_ALERT);
                    }
                }
            }
            
            // Draw waveform for audio clips
            if (clip->type == MUZZA_CLIP_AUDIO && clip->media_id >= 0 && (size_t)clip->media_id < proj->num_media) {
                const muzza_media* media = &proj->media_pool[clip->media_id];
                if (media->waveform.peaks && media->waveform.num_peaks > 0) {
                    float wf_y_center = current_y + track_h * 0.5f;
                    float wf_h_max = track_h - 12.0f * s;
                    
                    // We only iterate over the visible part of the clip
                    float visible_clip_x = (clip_x < tracks_x) ? tracks_x : clip_x;
                    float visible_clip_end = (clip_x + clip_w > tracks_x + tracks_w) ? tracks_x + tracks_w : clip_x + clip_w;
                    
                    for (float ix = visible_clip_x; ix < visible_clip_end; ix += (1.0f * s)) {
                        double current_tl_time = x_to_time(ix, tracks_x, timeline->scroll_x, timeline->zoom, s);
                        double media_time = clip->media_in + (current_tl_time - clip->tl_start);
                        float peak_normalized = (float)(media_time / media->duration);
                        
                        if (peak_normalized >= 0.0f && peak_normalized <= 1.0f) {
                            int peak_idx = (int)(peak_normalized * (media->waveform.num_peaks - 1));
                            float peak = media->waveform.peaks[peak_idx];
                            float line_h = peak * wf_h_max;
                            ui_draw_rect(canvas, ix, wf_y_center - line_h * 0.5f, 1.0f * s, line_h, 0xAAFFFFFF);
                        }
                    }
                }
            }

            ui_draw_border(canvas, draw_x, current_y + 2.0f * s, draw_w, track_h - 4.0f * s, s, MUZZA_COLOR_BORDER);
            
            if (clip->media_id >= 0 && (size_t)clip->media_id < proj->num_media) {
                char label[1024];
                snprintf(label, sizeof(label), "%s [%s]", 
                    muzza_path_basename(proj->media_pool[clip->media_id].filepath),
                    clip->type == MUZZA_CLIP_VIDEO ? "V" : "A");

                ui_draw_text_ellipsis(
                    canvas,
                    draw_x + 6.0f * s,
                    current_y + 8.0f * s,
                    draw_w - 12.0f * s,
                    label,
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

    // Draw Track Headers (AFTER tracks to cover them)
    current_y = y + 38.0f * s;
    for (int track = 0; track < MUZZA_MAX_TRACKS; ++track) {
        char track_label[24];
        float track_h = ui_clampf(proj->track_heights[track] * s, 24.0f * s, h - 50.0f * s);

        ui_draw_rect(canvas, x + 1.0f * s, current_y, header_w - 1.0f * s, track_h, MUZZA_COLOR_BG_HEADER);
        snprintf(track_label, sizeof(track_label), "TRACK %d", track + 1);
        ui_draw_text(canvas, x + 14.0f * s, current_y + 10.0f * s, track_label, 1.0f * s, 0xFFB8C3C9);
        
        current_y += track_h + 1.0f * s;
        if (current_y >= y + h - 8.0f * s) break;
    }

    // Drag and Drop indicator
    double drop_time = 0.0;
    int drop_track = -1;
    float drop_line_x = 0.0f;
    float drop_track_y = 0.0f;
    float drop_track_h = 0.0f;
    if (state->media_panel.is_dragging_media
        && timeline_drop_target(state, x, y, w, h, &drop_time, &drop_track, &drop_line_x, &drop_track_y, &drop_track_h)) {
        ui_draw_rect(canvas, drop_line_x - 1.0f * s, drop_track_y, 2.0f * s, drop_track_h, MUZZA_COLOR_ACCENT);
        ui_draw_rect(canvas, drop_line_x - 8.0f * s, drop_track_y, 16.0f * s, 10.0f * s, MUZZA_COLOR_ACCENT);
        ui_draw_border(canvas, tracks_x, drop_track_y, tracks_w, drop_track_h, s, MUZZA_COLOR_ACCENT_DIM);
    }

    // Playhead
    if (playhead_x >= tracks_x && playhead_x <= tracks_x + tracks_w) {
        ui_draw_rect(canvas, playhead_x - 1.0f * s, y + 28.0f * s, 2.0f * s, h - 28.0f * s, MUZZA_COLOR_ALERT);
        ui_draw_rect(canvas, playhead_x - 8.0f * s, y + 28.0f * s, 16.0f * s, 10.0f * s, MUZZA_COLOR_ALERT);
    }

    // Tool cursor indicator (drawn last so it appears on top)
    if (ui_point_in_rect(state->input.x, state->input.y, x, y, w, h)) {
        if (timeline->active_tool == MUZZA_TOOL_RAZOR) {
            float cx = state->input.x;
            float cy = state->input.y;
            // Blade (vertical line)
            ui_draw_rect(canvas, cx - 1.0f * s, cy - 10.0f * s, 2.0f * s, 20.0f * s, MUZZA_COLOR_ALERT);
            // Blade edge highlight
            ui_draw_rect(canvas, cx - 2.0f * s, cy - 10.0f * s, 1.0f * s, 20.0f * s, 0xFFFF6666);
            // Handle top
            ui_draw_rect(canvas, cx - 5.0f * s, cy - 14.0f * s, 10.0f * s, 5.0f * s, MUZZA_COLOR_ALERT);
            // Handle bottom
            ui_draw_rect(canvas, cx - 5.0f * s, cy + 9.0f * s, 10.0f * s, 5.0f * s, MUZZA_COLOR_ALERT);
            // Crosshair guide lines
            ui_draw_rect(canvas, cx - 20.0f * s, cy - 0.5f * s, 40.0f * s, 1.0f * s, 0x44FF4444);
            ui_draw_rect(canvas, cx - 0.5f * s, cy - 20.0f * s, 1.0f * s, 40.0f * s, 0x44FF4444);
        } else if (timeline->active_tool == MUZZA_TOOL_SELECT && timeline->trim_hover_clip >= 0 && timeline->trim_hover_edge != 0) {
            float cx = state->input.x;
            float cy = state->input.y;
            fx_color indicator_color = MUZZA_COLOR_ACCENT;
            // Draw small directional arrows at cursor
            if (timeline->trim_hover_edge < 0) {
                // Left edge — draw left-pointing arrow
                ui_draw_rect(canvas, cx - 6.0f * s, cy - 2.0f * s, 8.0f * s, 4.0f * s, indicator_color);
                ui_draw_rect(canvas, cx - 6.0f * s, cy - 5.0f * s, 4.0f * s, 3.0f * s, indicator_color);
                ui_draw_rect(canvas, cx - 6.0f * s, cy + 2.0f * s, 4.0f * s, 3.0f * s, indicator_color);
            } else {
                // Right edge — draw right-pointing arrow
                ui_draw_rect(canvas, cx - 2.0f * s, cy - 2.0f * s, 8.0f * s, 4.0f * s, indicator_color);
                ui_draw_rect(canvas, cx + 2.0f * s, cy - 5.0f * s, 4.0f * s, 3.0f * s, indicator_color);
                ui_draw_rect(canvas, cx + 2.0f * s, cy + 2.0f * s, 4.0f * s, 3.0f * s, indicator_color);
            }
        }
    }
}
