#include "ui_timeline_panel.h"

#include <stdio.h>
#include <math.h>

#include "path_utils.h"
#include "project.h"
#include "ui_icons.h"
#include "ui_shared.h"
#include "ui_text.h"

static bool timeline_drop_target(const muzza_ui_state* state, float x, float y, float w, float h, double* out_time, int* out_track, float* out_line_x, float* out_track_y, float* out_track_h) {
    const float s = (state && state->ui_scale > 1.0f) ? state->ui_scale : 1.0f;
    const float header_w = 170.0f * s;
    float tracks_x = x + header_w;
    float tracks_w = w - header_w - 10.0f * s;
    float current_y = y + 46.0f * s;
    const muzza_timeline_state* timeline = &state->timeline;

    if (!state || !state->project || tracks_w <= 0.0f) {
        return false;
    }

    if (!ui_point_in_rect(state->input.x, state->input.y, tracks_x, y + 36.0f * s, tracks_w, h - 36.0f * s)) {
        return false;
    }

    for (int track = 0; track < MUZZA_MAX_TRACKS; ++track) {
        float track_h = ui_clampf(state->project->tracks[track].height * s, 24.0f * s, h - 50.0f * s);

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
    float current_y = y + 46.0f * s;
    int clicked_clip = -1;
    int trim_hover_clip = -1;
    int trim_hover_edge = 0;

    if (!proj) {
        return;
    }

    // Toolbar hit test (header area, right of TIMELINE label)
    float toolbar_x = x + header_w + 6.0f * s;
    float toolbar_y = y + 5.0f * s;
    float btn_w = 30.0f * s;
    float btn_h = 18.0f * s;
    if (state->input.left_pressed) {
        if (ui_point_in_rect(state->input.x, state->input.y, toolbar_x, toolbar_y, btn_w, btn_h)) {
            timeline->active_tool = MUZZA_TOOL_SELECT;
        } else if (ui_point_in_rect(state->input.x, state->input.y, toolbar_x + btn_w + 4.0f * s, toolbar_y, btn_w, btn_h)) {
            timeline->active_tool = MUZZA_TOOL_RAZOR;
        } else if (ui_point_in_rect(state->input.x, state->input.y, toolbar_x + 2.0f * (btn_w + 4.0f * s), toolbar_y, btn_w, btn_h)) {
            timeline->active_tool = MUZZA_TOOL_RIPPLE;
        }
    }

    // Toolbar hover tooltips
    if (ui_point_in_rect(state->input.x, state->input.y, toolbar_x, toolbar_y, btn_w, btn_h)) {
        ui_set_tooltip(state, "Select (V)");
    } else if (ui_point_in_rect(state->input.x, state->input.y, toolbar_x + btn_w + 4.0f * s, toolbar_y, btn_w, btn_h)) {
        ui_set_tooltip(state, "Razor / Split (C)");
    } else if (ui_point_in_rect(state->input.x, state->input.y, toolbar_x + 2.0f * (btn_w + 4.0f * s), toolbar_y, btn_w, btn_h)) {
        ui_set_tooltip(state, "Ripple Delete (R)");
    }

    // Handle Zooming and Scrolling
    if (ui_point_in_rect(state->input.x, state->input.y, x, y, w, h)) {
        if (state->input.ctrl_down || state->input.zoom_in_pressed || state->input.zoom_out_pressed) {
            float zoom_factor = 0.0f;
            if (state->input.wheel_y > 0.0f || state->input.zoom_in_pressed) zoom_factor = 1.15f;
            else if (state->input.wheel_y < 0.0f || state->input.zoom_out_pressed) zoom_factor = 1.0f / 1.15f;

            if (zoom_factor != 0.0f) {
                apply_cursor_anchored_zoom(&timeline->scroll_x, &timeline->zoom, tracks_x, state->input.x, zoom_factor, s);
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

        if (timeline->trim_edge == -1) {
            double new_media_in = 0.0;
            double new_dur = 0.0;
            project_trim_clip_left(proj, timeline->trim_clip_index, mouse_time, &new_media_in, &new_dur);
        } else if (timeline->trim_edge == 1) {
            double new_dur = mouse_time - clip->tl_start;
            project_trim_clip_right(proj, timeline->trim_clip_index, new_dur);
        }
    }

    // Fade drag handling
    if (timeline->is_dragging_fade && timeline->fade_clip_index >= 0 && state->input.left_down) {
        muzza_clip* clip = &proj->clips[timeline->fade_clip_index];
        double mouse_time = x_to_time(state->input.x, tracks_x, timeline->scroll_x, timeline->zoom, s);

        if (timeline->fade_edge == -1) {
            double new_fade = mouse_time - clip->tl_start;
            if (new_fade < 0.0) new_fade = 0.0;
            if (new_fade > clip->tl_duration * 0.9) new_fade = clip->tl_duration * 0.9;
            clip->fade_in_duration = new_fade;
        } else if (timeline->fade_edge == 1) {
            double clip_end = clip->tl_start + clip->tl_duration;
            double new_fade = clip_end - mouse_time;
            if (new_fade < 0.0) new_fade = 0.0;
            if (new_fade > clip->tl_duration * 0.9) new_fade = clip->tl_duration * 0.9;
            clip->fade_out_duration = new_fade;
        }
        proj->dirty = true;
    }

    for (int track = 0; track < MUZZA_MAX_TRACKS; ++track) {
        float track_h = ui_clampf(proj->tracks[track].height * s, 24.0f * s, h - 50.0f * s);
        float edge_y = current_y + track_h;

        // Track header control buttons (right side of header)
        float ctrl_btn_w = 16.0f * s;
        float ctrl_btn_h = 15.0f * s;
        float ctrl_y = current_y + track_h * 0.5f - ctrl_btn_h * 0.5f;
        float ctrl_x_start = x + header_w - 60.0f * s;
        if (state->input.left_pressed) {
            float bx = ctrl_x_start;
            if (ui_point_in_rect(state->input.x, state->input.y, bx, ctrl_y, ctrl_btn_w, ctrl_btn_h)) {
                proj->tracks[track].muted = !proj->tracks[track].muted;
            }
            bx += ctrl_btn_w + 2.0f * s;
            if (ui_point_in_rect(state->input.x, state->input.y, bx, ctrl_y, ctrl_btn_w, ctrl_btn_h)) {
                proj->tracks[track].solo = !proj->tracks[track].solo;
            }
            bx += ctrl_btn_w + 2.0f * s;
            if (ui_point_in_rect(state->input.x, state->input.y, bx, ctrl_y, ctrl_btn_w, ctrl_btn_h)) {
                proj->tracks[track].locked = !proj->tracks[track].locked;
            }
        }

        // Track button hover tooltips
        {
            char tip[128];
            float tx = ctrl_x_start;
            if (ui_point_in_rect(state->input.x, state->input.y, tx, ctrl_y, ctrl_btn_w, ctrl_btn_h)) {
                snprintf(tip, sizeof(tip), "%s %s", proj->tracks[track].muted ? "Unmute" : "Mute", proj->tracks[track].name);
                ui_set_tooltip(state, tip);
            }
            tx += ctrl_btn_w + 2.0f * s;
            if (ui_point_in_rect(state->input.x, state->input.y, tx, ctrl_y, ctrl_btn_w, ctrl_btn_h)) {
                snprintf(tip, sizeof(tip), "%s %s", proj->tracks[track].solo ? "Un-solo" : "Solo", proj->tracks[track].name);
                ui_set_tooltip(state, tip);
            }
            tx += ctrl_btn_w + 2.0f * s;
            if (ui_point_in_rect(state->input.x, state->input.y, tx, ctrl_y, ctrl_btn_w, ctrl_btn_h)) {
                snprintf(tip, sizeof(tip), "%s %s", proj->tracks[track].locked ? "Unlock" : "Lock", proj->tracks[track].name);
                ui_set_tooltip(state, tip);
            }
        }

        if (state->input.left_pressed && ui_point_in_rect(state->input.x, state->input.y, x, edge_y - 3.0f * s, w, 6.0f * s)) {
            if (!proj->tracks[track].locked) {
                timeline->is_dragging_track_edge = true;
                timeline->dragged_track_index = track;
            }
        }

        if (state->input.left_down && timeline->is_dragging_track_edge && timeline->dragged_track_index == track) {
            proj->tracks[track].height = ui_clampf((state->input.y - current_y) / s, 24.0f, 160.0f);
            track_h = ui_clampf(proj->tracks[track].height * s, 24.0f * s, 160.0f * s);
        }

        bool track_locked = proj->tracks[track].locked;

        for (size_t clip_index = 0; clip_index < proj->num_clips; ++clip_index) {
            const muzza_clip* clip = &proj->clips[clip_index];
            if (clip->track_index != track) continue;

            float clip_x = time_to_x(clip->tl_start, tracks_x, timeline->scroll_x, timeline->zoom, s);
            float clip_w = (float)clip->tl_duration * timeline->zoom * s;
            if (clip_w < 4.0f * s) clip_w = 4.0f * s;

            float body_y = current_y + 2.0f * s;
            float body_h = track_h - 4.0f * s;

            // Check hover for trim handles (SELECT) or clip body (RAZOR/RIPPLE)
            if (timeline->active_tool == MUZZA_TOOL_SELECT && !timeline->is_trimming && !timeline->is_dragging_fade) {
                if (!track_locked && ui_point_in_rect(state->input.x, state->input.y, clip_x, body_y, 6.0f * s, body_h)) {
                    trim_hover_clip = (int)clip_index;
                    trim_hover_edge = -1;
                } else if (ui_point_in_rect(state->input.x, state->input.y, clip_x + clip_w - 6.0f * s, body_y, 6.0f * s, body_h)) {
                    trim_hover_clip = (int)clip_index;
                    trim_hover_edge = 1;
                }

                // Fade handle hover (top-left and top-right corners above clip)
                timeline->fade_hover_clip = -1;
                timeline->fade_hover_edge = 0;
                if (!track_locked && clip->fade_in_duration > 0.0 && ui_point_in_rect(state->input.x, state->input.y, clip_x, body_y - 8.0f * s, 14.0f * s, 10.0f * s)) {
                    timeline->fade_hover_clip = (int)clip_index;
                    timeline->fade_hover_edge = -1;
                }
                if (clip->fade_out_duration > 0.0 && ui_point_in_rect(state->input.x, state->input.y, clip_x + clip_w - 14.0f * s, body_y - 8.0f * s, 14.0f * s, 10.0f * s)) {
                    timeline->fade_hover_clip = (int)clip_index;
                    timeline->fade_hover_edge = 1;
                }
            } else if (timeline->active_tool == MUZZA_TOOL_RAZOR && !timeline->is_trimming) {
                if (ui_point_in_rect(state->input.x, state->input.y, clip_x, body_y, clip_w, body_h)) {
                    trim_hover_clip = (int)clip_index;
                    trim_hover_edge = 0;
                }
            } else if (timeline->active_tool == MUZZA_TOOL_RIPPLE && !timeline->is_trimming) {
                if (ui_point_in_rect(state->input.x, state->input.y, clip_x, body_y, clip_w, body_h)) {
                    trim_hover_clip = (int)clip_index;
                    trim_hover_edge = 0;
                }
            }

            // Clip body hover tooltip
            if (ui_point_in_rect(state->input.x, state->input.y, clip_x, body_y, clip_w, body_h)) {
                if (clip->media_id >= 0 && (size_t)clip->media_id < proj->num_media) {
                    const char* bname = muzza_path_basename(proj->media_pool[clip->media_id].filepath);
                    const char* type_str = clip->type == MUZZA_CLIP_VIDEO ? "V" : "A";
                    char tip[512];
                    int pos = snprintf(tip, sizeof(tip), "%s [%s] @%.1fs dur=%.1fs",
                        bname, type_str, clip->tl_start, clip->tl_duration);
                    if (clip->fade_in_duration > 0.0 || clip->fade_out_duration > 0.0) {
                        snprintf(tip + pos, sizeof(tip) - pos, " fi=%.1f fo=%.1f",
                            clip->fade_in_duration, clip->fade_out_duration);
                    }
                    if (track_locked) {
                        snprintf(tip + strlen(tip), sizeof(tip) - strlen(tip), " [LOCKED]");
                    }
                    ui_set_tooltip(state, tip);
                }
            }

            // Click detection - skip locked tracks
            if (state->input.left_pressed && !track_locked && ui_point_in_rect(state->input.x, state->input.y, clip_x, body_y, clip_w, body_h)) {
                // In SELECT mode, check if clicking on a trim handle or fade handle
                if (timeline->active_tool == MUZZA_TOOL_SELECT) {
                    if (timeline->fade_hover_clip == (int)clip_index && timeline->fade_hover_edge != 0) {
                        timeline->is_dragging_fade = true;
                        timeline->fade_clip_index = (int)clip_index;
                        timeline->fade_edge = timeline->fade_hover_edge;
                    } else if (ui_point_in_rect(state->input.x, state->input.y, clip_x, body_y, 6.0f * s, body_h)) {
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
                } else if (!track_locked && ui_point_in_rect(state->input.x, state->input.y, clip_x + clip_w - 6.0f * s, body_y, 6.0f * s, body_h)) {
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
                } else if (timeline->active_tool == MUZZA_TOOL_RIPPLE && actions) {
                    // Ripple: delete clip and shift remaining clips
                    actions->ripple_delete_clip_index = (int)clip_index;
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
        timeline->is_dragging_fade = false;
        timeline->fade_clip_index = -1;
        timeline->fade_edge = 0;
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
    } else if (state->input.left_pressed && !timeline->is_trimming && ui_point_in_rect(state->input.x, state->input.y, tracks_x, y + 36.0f * s, tracks_w, h - 36.0f * s)) {
        timeline->is_scrubbing = true;
        timeline->playhead_time = x_to_time(state->input.x, tracks_x, timeline->scroll_x, timeline->zoom, s);
        if (timeline->playhead_time < 0.0) timeline->playhead_time = 0.0;
        project_clear_selection(proj);
        timeline->selected_clip_index = -1;
    }

    if (state->input.left_down && timeline->is_dragging_clip && timeline->dragged_clip_index >= 0 && !timeline->is_trimming) {
        muzza_clip* clip = &proj->clips[timeline->dragged_clip_index];
        double new_tl_start = x_to_time(state->input.x, tracks_x, timeline->scroll_x, timeline->zoom, s) - timeline->drag_start_offset;
        int drop_track = clip->track_index;
        double drop_time = 0.0;
        if (timeline_drop_target(state, x, y, w, h, &drop_time, &drop_track, NULL, NULL, NULL)) {
            if (drop_track < 0 || drop_track >= MUZZA_MAX_TRACKS) {
                drop_track = clip->track_index;
            }
        }
        project_move_clip(proj, timeline->dragged_clip_index, new_tl_start, drop_track);
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
    const float header_w = 170.0f * s;
    float tracks_x = x + header_w;
    float tracks_w = w - header_w - 10.0f * s;
    float current_y = y + 46.0f * s;
    muzza_timeline_state* timeline = &state->timeline;

    update_timeline_interactions(state, actions, x, y, w, h);

    float playhead_x = time_to_x(timeline->playhead_time, tracks_x, timeline->scroll_x, timeline->zoom, s);

    ui_draw_text(canvas, x + 14.0f * s, y + 9.0f * s, "TIMELINE", 2.0f * s, 0xFFD7DEE3);

    // Toolbar — right of track headers
    float toolbar_x = x + header_w + 6.0f * s;
    float toolbar_y = y + 5.0f * s;
    float btn_w = 30.0f * s;
    float btn_h = 18.0f * s;
    fx_color sel_color = (timeline->active_tool == MUZZA_TOOL_SELECT) ? MUZZA_COLOR_ACCENT : MUZZA_COLOR_TRACK_ALT;
    fx_color cut_color = (timeline->active_tool == MUZZA_TOOL_RAZOR) ? MUZZA_COLOR_ACCENT : MUZZA_COLOR_TRACK_ALT;
    fx_color rip_color = (timeline->active_tool == MUZZA_TOOL_RIPPLE) ? MUZZA_COLOR_ACCENT : MUZZA_COLOR_TRACK_ALT;
    ui_draw_rect(canvas, toolbar_x, toolbar_y, btn_w, btn_h, sel_color);
    ui_draw_border(canvas, toolbar_x, toolbar_y, btn_w, btn_h, s, MUZZA_COLOR_BORDER);
    draw_icon_arrow_right(canvas, toolbar_x + 2.0f * s, toolbar_y + 1.0f * s, 16.0f * s, 0xFFF0F7F6);

    ui_draw_rect(canvas, toolbar_x + btn_w + 4.0f * s, toolbar_y, btn_w, btn_h, cut_color);
    ui_draw_border(canvas, toolbar_x + btn_w + 4.0f * s, toolbar_y, btn_w, btn_h, s, MUZZA_COLOR_BORDER);
    draw_icon_razor(canvas, toolbar_x + btn_w + 4.0f * s + 2.0f * s, toolbar_y + 1.0f * s, 16.0f * s, 0xFFF0F7F6);

    ui_draw_rect(canvas, toolbar_x + 2.0f * (btn_w + 4.0f * s), toolbar_y, btn_w, btn_h, rip_color);
    ui_draw_border(canvas, toolbar_x + 2.0f * (btn_w + 4.0f * s), toolbar_y, btn_w, btn_h, s, MUZZA_COLOR_BORDER);
    draw_icon_close(canvas, toolbar_x + 2.0f * (btn_w + 4.0f * s) + 2.0f * s, toolbar_y + 1.0f * s, 16.0f * s, 0xFFF0F7F6);

    /* Time display module */
    {
        float tm_w = 140.0f * s;
        float tm_h = 20.0f * s;
        float tm_x = x + w - tm_w - 8.0f * s;
        float tm_y = y + 4.0f * s;

        ui_draw_rect(canvas, tm_x, tm_y, tm_w, tm_h, MUZZA_COLOR_BG_HEADER);
        ui_draw_border(canvas, tm_x, tm_y, tm_w, tm_h, 0.5f * s, MUZZA_COLOR_BORDER);

        char time_text[32];
        snprintf(time_text, sizeof(time_text), "%.2f / %.2f", timeline->playhead_time, proj->duration);
        if (state->playback_speed != 1.0f) {
            char speed_text[48];
            snprintf(speed_text, sizeof(speed_text), "%s [%.1fx]", time_text, (double)state->playback_speed);
            ui_draw_text_centered(canvas, tm_x, tm_y, tm_w, tm_h, speed_text, 1.1f * s,
                state->is_playing ? 0xFF3AA7A3 : 0xFF8FA0A9);
        } else {
            ui_draw_text_centered(canvas, tm_x, tm_y, tm_w, tm_h, time_text, 1.1f * s, 0xFF8FA0A9);
        }
    }

    // Draw background for tracks area
    ui_draw_rect(canvas, tracks_x, y + 36.0f * s, tracks_w, h - 36.0f * s, MUZZA_COLOR_TRACK_BG);

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
            ui_draw_rect(canvas, tx, y + 40.0f * s, 1.0f * s, 6.0f * s, 0xFF4A5661);
            if (fmod(t, tick_spacing * 5.0) < tick_spacing * 0.1) {
                char tick_label[16];
                snprintf(tick_label, sizeof(tick_label), "%.1fs", t);
                ui_draw_text(canvas, tx + 2.0f * s, y + 30.0f * s, tick_label, 1.0f * s, 0xFF6A7681);
                ui_draw_rect(canvas, tx, y + 36.0f * s, 1.0f * s, 10.0f * s, 0xFF6A7681);
            }
        }
    }

    for (int track = 0; track < MUZZA_MAX_TRACKS; ++track) {
        float track_h = ui_clampf(proj->tracks[track].height * s, 24.0f * s, h - 50.0f * s);

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
            // Ripple tool hover = red tint
            if (timeline->active_tool == MUZZA_TOOL_RIPPLE && (int)clip_index == timeline->trim_hover_clip) {
                color = MUZZA_COLOR_ALERT;
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
            
            /* Draw waveform for audio clips. Per-pixel column rectangles —
               robust against quiet/silent stretches that would collapse a
               filled-polygon approach into a degenerate shape. */
            if (clip->type == MUZZA_CLIP_AUDIO && clip->media_id >= 0 && (size_t)clip->media_id < proj->num_media) {
                const muzza_media* media = &proj->media_pool[clip->media_id];
                if (media->waveform.mins && media->waveform.maxs && media->waveform.num_peaks > 1
                    && media->duration > 0.0) {
                    float wf_y_center = current_y + track_h * 0.5f;
                    float wf_h_max = track_h - 12.0f * s;

                    float vis_x0 = (clip_x < tracks_x) ? tracks_x : clip_x;
                    float vis_x1 = (clip_x + clip_w > tracks_x + tracks_w)
                        ? tracks_x + tracks_w : clip_x + clip_w;

                    const fx_color wf_color = 0xCCCCCCCC;
                    const float col_w = 1.0f * s;
                    const double inv_zoom_s = 1.0 / ((double)timeline->zoom * s);
                    const double inv_duration = 1.0 / media->duration;
                    const int npm1 = media->waveform.num_peaks - 1;

                    for (float px = vis_x0; px < vis_x1; px += col_w) {
                        double tl_a = (double)(px - tracks_x) * inv_zoom_s + timeline->scroll_x;
                        double tl_b = tl_a + (double)col_w * inv_zoom_s;
                        double ma = clip->media_in + (tl_a - clip->tl_start);
                        double mb = clip->media_in + (tl_b - clip->tl_start);
                        if (ma < 0.0) ma = 0.0;
                        if (mb > media->duration) mb = media->duration;
                        if (mb <= ma) continue;

                        int pa = (int)((ma * inv_duration) * npm1);
                        int pb = (int)((mb * inv_duration) * npm1);
                        if (pa < 0) pa = 0;
                        if (pb < pa) pb = pa;
                        if (pb > npm1) pb = npm1;

                        float lo = 0.0f;
                        float hi = 0.0f;
                        for (int pi = pa; pi <= pb; ++pi) {
                            if (media->waveform.mins[pi] < lo) lo = media->waveform.mins[pi];
                            if (media->waveform.maxs[pi] > hi) hi = media->waveform.maxs[pi];
                        }

                        float top_y = wf_y_center - hi * wf_h_max * 0.5f;
                        float bot_y = wf_y_center - lo * wf_h_max * 0.5f;
                        float h_px = bot_y - top_y;
                        if (h_px < col_w) h_px = col_w;
                        ui_draw_rect(canvas, px, top_y, col_w, h_px, wf_color);
                    }
                }
            }

            // Fade overlays
            if (clip->fade_in_duration > 0.0) {
                float fade_w = (float)clip->fade_in_duration * timeline->zoom * s;
                float fade_x = clip_x;
                if (fade_w > clip_w) fade_w = clip_w;
                if (fade_x < tracks_x) { fade_w -= (tracks_x - fade_x); fade_x = tracks_x; }
                if (fade_x + fade_w > tracks_x + tracks_w) fade_w = (tracks_x + tracks_w) - fade_x;
                if (fade_w > 0.0f) {
                    // Fade-in: dark gradient at left edge
                    for (int step = 0; step < 8; ++step) {
                        float frac = (float)step / 8.0f;
                        float sx = fade_x + frac * fade_w;
                        float sw = fade_w / 8.0f + 1.0f;
                        fx_color c = 0xAA000000;
                        int alpha = (int)(170.0f * (1.0f - frac)) & 0xFF;
                        ui_draw_rect(canvas, sx, current_y + 2.0f * s, sw, track_h - 4.0f * s, c | ((fx_color)alpha << 24));
                    }
                }
            }
            if (clip->fade_out_duration > 0.0) {
                float fade_w = (float)clip->fade_out_duration * timeline->zoom * s;
                float fade_end = clip_x + clip_w;
                float fade_x = fade_end - fade_w;
                if (fade_w > clip_w) { fade_w = clip_w; fade_x = fade_end - fade_w; }
                if (fade_x < tracks_x) { fade_w -= (tracks_x - fade_x); fade_x = tracks_x; }
                if (fade_x + fade_w > tracks_x + tracks_w) fade_w = (tracks_x + tracks_w) - fade_x;
                if (fade_w > 0.0f) {
                    // Fade-out: dark gradient at right edge
                    for (int step = 0; step < 8; ++step) {
                        float frac = (float)step / 8.0f;
                        float sx = fade_x + frac * fade_w;
                        float sw = fade_w / 8.0f + 1.0f;
                        fx_color c = 0xAA000000;
                        int alpha = (int)(170.0f * frac) & 0xFF;
                        ui_draw_rect(canvas, sx, current_y + 2.0f * s, sw, track_h - 4.0f * s, c | ((fx_color)alpha << 24));
                    }
                }
            }

            // Fade handle indicators (triangles above clip)
            if (clip->fade_in_duration > 0.0) {
                float fx_h = clip_x;
                float fy = current_y - 2.0f * s;
                float fh_h = 6.0f * s;
                fx_color fc = (timeline->fade_hover_clip == (int)clip_index && timeline->fade_hover_edge == -1) ? MUZZA_COLOR_ACCENT : MUZZA_COLOR_ACCENT_DIM;
                // Small triangle pointing down
                ui_draw_rect(canvas, fx_h + 4.0f * s, fy, 6.0f * s, fh_h, fc);
                ui_draw_rect(canvas, fx_h + 2.0f * s, fy + fh_h, 10.0f * s, fh_h * 0.5f, fc);
                ui_draw_rect(canvas, fx_h, fy + fh_h * 1.5f, 4.0f * s, fh_h * 0.5f, fc);
            }
            if (clip->fade_out_duration > 0.0) {
                float fx_h = clip_x + clip_w - 14.0f * s;
                float fy = current_y - 2.0f * s;
                float fh_h = 6.0f * s;
                fx_color fc = (timeline->fade_hover_clip == (int)clip_index && timeline->fade_hover_edge == 1) ? MUZZA_COLOR_ACCENT : MUZZA_COLOR_ACCENT_DIM;
                // Small triangle pointing down
                ui_draw_rect(canvas, fx_h + 4.0f * s, fy, 6.0f * s, fh_h, fc);
                ui_draw_rect(canvas, fx_h + 2.0f * s, fy + fh_h, 10.0f * s, fh_h * 0.5f, fc);
                ui_draw_rect(canvas, fx_h + 10.0f * s, fy + fh_h * 1.5f, 4.0f * s, fh_h * 0.5f, fc);
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
    current_y = y + 46.0f * s;
    for (int track = 0; track < MUZZA_MAX_TRACKS; ++track) {
        float track_h = ui_clampf(proj->tracks[track].height * s, 24.0f * s, h - 50.0f * s);

        ui_draw_rect(canvas, x + 1.0f * s, current_y, header_w - 1.0f * s, track_h, MUZZA_COLOR_BG_HEADER);

        // Track name (left side, bigger font, vertically centered)
        float name_y = current_y + track_h * 0.5f - 7.0f * s;
        ui_draw_text(canvas, x + 6.0f * s, name_y, proj->tracks[track].name, 1.3f * s, 0xFFB8C3C9);

        // Control buttons (right side of header, vertically centered)
        float ctrl_btn_w = 16.0f * s;
        float ctrl_btn_h = 15.0f * s;
        float ctrl_y = current_y + track_h * 0.5f - ctrl_btn_h * 0.5f;
        float ctrl_x = x + header_w - 60.0f * s;

        // M (Mute) button
        fx_color mute_color = proj->tracks[track].muted ? MUZZA_COLOR_ALERT : MUZZA_COLOR_TRACK_EDGE;
        ui_draw_rect(canvas, ctrl_x, ctrl_y, ctrl_btn_w, ctrl_btn_h, mute_color);
        ui_draw_border(canvas, ctrl_x, ctrl_y, ctrl_btn_w, ctrl_btn_h, 0.5f * s, MUZZA_COLOR_BORDER);
        ui_draw_text_centered(canvas, ctrl_x, ctrl_y, ctrl_btn_w, ctrl_btn_h, "M", 1.0f * s, 0xFFF0F7F6);
        ctrl_x += ctrl_btn_w + 2.0f * s;

        // S (Solo) button
        fx_color solo_color = proj->tracks[track].solo ? MUZZA_COLOR_ACCENT : MUZZA_COLOR_TRACK_EDGE;
        ui_draw_rect(canvas, ctrl_x, ctrl_y, ctrl_btn_w, ctrl_btn_h, solo_color);
        ui_draw_border(canvas, ctrl_x, ctrl_y, ctrl_btn_w, ctrl_btn_h, 0.5f * s, MUZZA_COLOR_BORDER);
        ui_draw_text_centered(canvas, ctrl_x, ctrl_y, ctrl_btn_w, ctrl_btn_h, "S", 1.0f * s, 0xFFF0F7F6);
        ctrl_x += ctrl_btn_w + 2.0f * s;

        // L (Lock) button
        fx_color lock_color = proj->tracks[track].locked ? MUZZA_COLOR_ACCENT : MUZZA_COLOR_TRACK_EDGE;
        ui_draw_rect(canvas, ctrl_x, ctrl_y, ctrl_btn_w, ctrl_btn_h, lock_color);
        ui_draw_border(canvas, ctrl_x, ctrl_y, ctrl_btn_w, ctrl_btn_h, 0.5f * s, MUZZA_COLOR_BORDER);
        ui_draw_text_centered(canvas, ctrl_x, ctrl_y, ctrl_btn_w, ctrl_btn_h, "L", 1.0f * s, 0xFFF0F7F6);
        
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
        ui_draw_rect(canvas, playhead_x - 1.0f * s, y + 36.0f * s, 2.0f * s, h - 36.0f * s, MUZZA_COLOR_ALERT);
        ui_draw_rect(canvas, playhead_x - 8.0f * s, y + 36.0f * s, 16.0f * s, 10.0f * s, MUZZA_COLOR_ALERT);
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
        } else if (timeline->active_tool == MUZZA_TOOL_RIPPLE) {
            float cx = state->input.x;
            float cy = state->input.y;
            // X-shaped delete indicator
            ui_draw_rect(canvas, cx - 6.0f * s, cy - 6.0f * s, 12.0f * s, 2.0f * s, MUZZA_COLOR_ALERT);
            ui_draw_rect(canvas, cx - 6.0f * s, cy + 4.0f * s, 12.0f * s, 2.0f * s, MUZZA_COLOR_ALERT);
            ui_draw_rect(canvas, cx - 1.0f * s, cy - 6.0f * s, 2.0f * s, 12.0f * s, MUZZA_COLOR_ALERT);
            // Crosshair guide
            ui_draw_rect(canvas, cx - 0.5f * s, cy - 18.0f * s, 1.0f * s, 36.0f * s, 0x44FF4444);
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
