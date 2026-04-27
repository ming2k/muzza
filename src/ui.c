#include "ui.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "import_browser.h"
#include "path_utils.h"
#include "ui_icons.h"
#include "ui_export_panel.h"
#include "ui_import_browser_panel.h"
#include "ui_media_panel.h"
#include "ui_monitor_panel.h"
#include "ui_shared.h"
#include "ui_text.h"
#include "ui_timeline_panel.h"

/* ---- Debug: cursor-hit region name (like web element inspector) ---- */
static const char* ui_hit_region_name(const muzza_ui_state* state) {
    static char name[256];
    name[0] = '\0';
    if (!state || !state->project) return "Canvas";

    const float s = state->ui_scale > 1.0f ? state->ui_scale : 1.0f;
    float wx = state->input.x, wy = state->input.y;
    float ww = (float)state->window_width, wh = (float)state->window_height;
    float menu_h = 32.0f * s;
    float split_v = ww * state->split_v;
    float split_h = wh * state->split_h;

    /* 1. Header bar (topmost) */
    if (wy < menu_h) {
        snprintf(name, sizeof(name), "Header > %s",
            wx < 36*s ? "PlayBtn" :
            wx > ww - 140*s ? "ExportBtn" :
            wx > 120*s + 130*s + 8*s && wx < 120*s + 130*s + 70*s ? "SpeedLabel" :
            wx > 120*s && wx < 120*s + 130*s ? "SpeedSlider" :
            "Canvas");
        return name;
    }

    /* 2. Splitters */
    if (fabsf(wx - split_v) < 5.0f * s) { snprintf(name, sizeof(name), "SplitterV"); return name; }
    if (fabsf(wy - split_h) < 5.0f * s) { snprintf(name, sizeof(name), "SplitterH"); return name; }

    /* 3. Media Panel */
    if (wx < split_v && wy < split_h) {
        int item_idx = 0;
        float item_h = 28.0f * s;
        float list_y = menu_h + 32.0f * s;
        if (wy > list_y && state->project) {
            item_idx = (int)((wy - list_y) / item_h);
            if (item_idx >= 0 && (size_t)item_idx < state->project->num_media) {
                snprintf(name, sizeof(name), "MediaPanel > Item[%d] %s",
                    item_idx,
                    muzza_path_basename(state->project->media_pool[item_idx].filepath));
                return name;
            }
        }
        snprintf(name, sizeof(name), "MediaPanel");
        return name;
    }

    /* 4. Monitor */
    if (wx > split_v && wy < split_h) {
        snprintf(name, sizeof(name), "Monitor");
        return name;
    }

    /* 5. Timeline */
    if (wy > split_h) {
        const muzza_timeline_state* tl = &state->timeline;
        float tx = 0, ty = split_h;
        float header_w = 170.0f * s;
        float tracks_x = tx + header_w;
        double scroll_x = tl->scroll_x;
        float zoom = tl->zoom;

        /* 5a. Timeline toolbar */
        float toolbar_x = tx + header_w + 6*s, toolbar_y = ty + 5.0f * s;
        float btn_w = 30.0f * s, btn_h = 18.0f * s;
        if (ui_point_in_rect(wx, wy, toolbar_x, toolbar_y, btn_w, btn_h)) {
            snprintf(name, sizeof(name), "Timeline > Toolbar > SelBtn"); return name;
        }
        if (ui_point_in_rect(wx, wy, toolbar_x + btn_w + 4*s, toolbar_y, btn_w, btn_h)) {
            snprintf(name, sizeof(name), "Timeline > Toolbar > RazorBtn"); return name;
        }
        if (ui_point_in_rect(wx, wy, toolbar_x + 2*(btn_w + 4*s), toolbar_y, btn_w, btn_h)) {
            snprintf(name, sizeof(name), "Timeline > Toolbar > RippleBtn"); return name;
        }

        /* 5b. Time ruler */
        if (wy > ty + 36*s && wy < ty + 46*s) {
            snprintf(name, sizeof(name), "Timeline > Ruler"); return name;
        }

        /* 5c. Track area */
        if (wx > tracks_x && wy > ty + 46*s) {
            float cur_y = ty + 46.0f * s;
            for (int t = 0; t < MUZZA_MAX_TRACKS; ++t) {
                float h = ui_clampf(state->project->tracks[t].height * s, 24.0f * s, wh - 50.0f * s);
                if (wy >= cur_y && wy < cur_y + h) {
                    /* Track edge drag zone */
                    if (wy > cur_y + h - 3*s && wy < cur_y + h + 3*s) {
                        snprintf(name, sizeof(name), "Timeline > Track%d > ResizeEdge", t+1); return name;
                    }

                    /* Check clips within this track */
                    for (size_t ci = 0; ci < state->project->num_clips; ++ci) {
                        const muzza_clip* clip = &state->project->clips[ci];
                        if (clip->track_index != t) continue;
                        float cx = time_to_x(clip->tl_start, tracks_x, scroll_x, zoom, s);
                        float cw = (float)clip->tl_duration * zoom * s;
                        if (cw < 4*s) cw = 4*s;
                        float cby = cur_y + 2*s, cbh = h - 4*s;

                        if (ui_point_in_rect(wx, wy, cx, cby, cw, cbh)) {
                            const char* bname = clip->media_id >= 0 && (size_t)clip->media_id < state->project->num_media
                                ? muzza_path_basename(state->project->media_pool[clip->media_id].filepath) : "?";
                            /* Sub-regions within clip */
                            if (wx < cx + 6*s) {
                                snprintf(name, sizeof(name), "Timeline > Track%d > Clip#%zu [%s] > TrimLeft", t+1, ci, bname);
                            } else if (wx > cx + cw - 6*s) {
                                snprintf(name, sizeof(name), "Timeline > Track%d > Clip#%zu [%s] > TrimRight", t+1, ci, bname);
                            } else if (clip->fade_in_duration > 0 && wx < cx + (float)clip->fade_in_duration * zoom * s) {
                                snprintf(name, sizeof(name), "Timeline > Track%d > Clip#%zu [%s] > FadeIn(%.1fs)", t+1, ci, bname, clip->fade_in_duration);
                            } else if (clip->fade_out_duration > 0 && wx > cx + cw - (float)clip->fade_out_duration * zoom * s) {
                                snprintf(name, sizeof(name), "Timeline > Track%d > Clip#%zu [%s] > FadeOut(%.1fs)", t+1, ci, bname, clip->fade_out_duration);
                            } else {
                                snprintf(name, sizeof(name), "Timeline > Track%d > Clip#%zu [%s] @%.1fs dur=%.1fs",
                                    t+1, ci, bname, clip->tl_start, clip->tl_duration);
                            }
                            return name;
                        }
                    }

                    /* No clip hit — scrub area */
                    snprintf(name, sizeof(name), "Timeline > Track%d (scrub)", t+1); return name;
                }
                cur_y += h + 1.0f * s;
                if (cur_y >= wh - 8*s) break;
            }
            /* Below all tracks */
            snprintf(name, sizeof(name), "Timeline > Empty"); return name;
        }

        /* 5d. Track header area */
        if (wx < tracks_x && wy > ty + 46*s) {
            float cur_y = ty + 46.0f * s;
            for (int t = 0; t < MUZZA_MAX_TRACKS; ++t) {
                float h = ui_clampf(state->project->tracks[t].height * s, 24.0f * s, wh - 50.0f * s);
                if (wy >= cur_y && wy < cur_y + h) {
                    float ctrl_btn_w = 16*s, ctrl_h = 15*s;
                    float ctrl_y = cur_y + h * 0.5f - ctrl_h * 0.5f;
                    float ctrl_x = tx + header_w - 60.0f * s;
                    if (ui_point_in_rect(wx, wy, ctrl_x, ctrl_y, ctrl_btn_w, ctrl_h)) {
                        snprintf(name, sizeof(name), "Timeline > Track%d > MuteBtn (%s)", t+1,
                            state->project->tracks[t].muted ? "ON" : "OFF"); return name;
                    }
                    ctrl_x += ctrl_btn_w + 2*s;
                    if (ui_point_in_rect(wx, wy, ctrl_x, ctrl_y, ctrl_btn_w, ctrl_h)) {
                        snprintf(name, sizeof(name), "Timeline > Track%d > SoloBtn (%s)", t+1,
                            state->project->tracks[t].solo ? "ON" : "OFF"); return name;
                    }
                    ctrl_x += ctrl_btn_w + 2*s;
                    if (ui_point_in_rect(wx, wy, ctrl_x, ctrl_y, ctrl_btn_w, ctrl_h)) {
                        snprintf(name, sizeof(name), "Timeline > Track%d > LockBtn (%s)", t+1,
                            state->project->tracks[t].locked ? "ON" : "OFF"); return name;
                    }
                    snprintf(name, sizeof(name), "Timeline > Track%d (header) %s", t+1, state->project->tracks[t].name);
                    return name;
                }
                cur_y += h + 1.0f * s;
                if (cur_y >= wh - 8*s) break;
            }
        }

        /* Timeline label area */
        snprintf(name, sizeof(name), "Timeline > Panel"); return name;
    }

    snprintf(name, sizeof(name), "Canvas");
    return name;
}

static void reset_actions(muzza_ui_actions* actions) {
    if (!actions) {
        return;
    }

    memset(actions, 0, sizeof(*actions));
    actions->delete_media_id = -1;
    actions->insert_media_id = -1;
    actions->insert_track_index = -1;
    actions->insert_timeline_time = -1.0;
    actions->delete_clip_index = -1;
    actions->split_clip_index = -1;
    actions->ripple_delete_clip_index = -1;
}

static void sanitize_state(muzza_ui_state* state) {
    if (!state || !state->project) {
        return;
    }

    if (state->timeline.selected_clip_index >= (int)state->project->num_clips) {
        state->timeline.selected_clip_index = -1;
    }
    if (state->timeline.active_clip_index >= (int)state->project->num_clips) {
        state->timeline.active_clip_index = -1;
    }
    if (state->media_panel.selected_media_index >= (int)state->project->num_media) {
        state->media_panel.selected_media_index = state->project->num_media > 0 ? 0 : -1;
    }
    if (state->media_panel.dragged_media_index >= (int)state->project->num_media) {
        state->media_panel.dragged_media_index = -1;
        state->media_panel.is_dragging_media = false;
    }
    if (state->preview.preview_media_index >= (int)state->project->num_media) {
        state->preview.preview_media_index = -1;
    }
}

static void update_splitters(muzza_ui_state* state, float window_width, float window_height, float menu_height) {
    float scale = state->ui_scale > 1.0f ? state->ui_scale : 1.0f;
    float split_v_px = window_width * state->split_v;
    float split_h_px = window_height * state->split_h;

    if (state->input.left_pressed) {
        if (ui_point_in_rect(state->input.x, state->input.y, split_v_px - 5.0f * scale, menu_height, 10.0f * scale, split_h_px - menu_height)) {
            state->is_dragging_splitter_v = true;
        }
        if (ui_point_in_rect(state->input.x, state->input.y, 0.0f, split_h_px - 5.0f * scale, window_width, 10.0f * scale)) {
            state->is_dragging_splitter_h = true;
        }
    }

    if (state->input.left_down && state->is_dragging_splitter_v) {
        state->split_v = ui_clampf(state->input.x / window_width, 0.16f, 0.42f);
    }
    if (state->input.left_down && state->is_dragging_splitter_h) {
        state->split_h = ui_clampf(state->input.y / window_height, 0.46f, 0.84f);
    }

    if (state->input.left_released) {
        state->is_dragging_splitter_v = false;
        state->is_dragging_splitter_h = false;
    }
}

void ui_state_init(muzza_ui_state* state, muzza_project* project) {
    if (!state) {
        return;
    }

    memset(state, 0, sizeof(*state));
    state->project = project;
    state->ui_scale = 1.0f;
    state->split_v = 0.24f;
    state->split_h = 0.62f;
    state->media_panel.selected_media_index = project && project->num_media > 0 ? 0 : -1;
    state->media_panel.context_menu_media_index = -1;
    state->media_panel.dragged_media_index = -1;
    state->timeline.selected_clip_index = -1;
    state->timeline.active_clip_index = -1;
    state->timeline.playhead_time = 0.0;
    state->timeline.zoom = 100.0f; // 100 pixels per second default
    state->timeline.scroll_x = 0.0;
    state->timeline.active_tool = MUZZA_TOOL_SELECT;
    state->timeline.is_trimming = false;
    state->timeline.trim_clip_index = -1;
    state->timeline.trim_edge = 0;
    state->timeline.trim_hover_clip = -1;
    state->timeline.trim_hover_edge = 0;
    state->timeline.is_dragging_fade = false;
    state->timeline.fade_clip_index = -1;
    state->timeline.fade_edge = 0;
    state->preview.preview_media_index = -1;
    state->playback.clip_index = -1;
    state->playback.media_id = -1;
    state->playback_speed = 1.0f;
    state->tooltip_visible = false;
    state->tooltip_text[0] = '\0';
    state->debug_overlay = false;
    import_browser_init(&state->import_browser);
}

void ui_begin_frame(muzza_ui_state* state) {
    if (!state) {
        return;
    }

    state->input.left_pressed = false;
    state->input.left_released = false;
    state->input.right_pressed = false;
    state->input.right_released = false;
    state->input.zoom_in_pressed = false;
    state->input.zoom_out_pressed = false;
    state->input.wheel_x = 0.0f;
    state->input.wheel_y = 0.0f;

    state->dialog_input.left_pressed = false;
    state->dialog_input.left_released = false;
    state->dialog_input.right_pressed = false;
    state->dialog_input.right_released = false;
    state->dialog_input.zoom_in_pressed = false;
    state->dialog_input.zoom_out_pressed = false;
    state->dialog_input.wheel_x = 0.0f;
    state->dialog_input.wheel_y = 0.0f;
}

void ui_set_tooltip(muzza_ui_state* state, const char* text) {
    if (!state || !text) return;
    snprintf(state->tooltip_text, sizeof(state->tooltip_text), "%s", text);
    state->tooltip_visible = true;
}

void ui_handle_event(muzza_ui_state* state, muzza_input_state* target_input, const SDL_Event* event, SDL_Window* window) {
    float scale = window ? SDL_GetWindowDisplayScale(window) : 1.0f;

    if (!state || !target_input || !event) {
        return;
    }

    // Debug input log
    if (state->debug_overlay) {
        const char* type_name = "?";
        switch (event->type) {
            case SDL_EVENT_KEY_DOWN: type_name = "KEY_DN"; break;
            case SDL_EVENT_KEY_UP:   type_name = "KEY_UP"; break;
            case SDL_EVENT_MOUSE_BUTTON_DOWN: type_name = "M_DOWN"; break;
            case SDL_EVENT_MOUSE_BUTTON_UP:   type_name = "M_UP";   break;
            case SDL_EVENT_MOUSE_MOTION:       type_name = "M_MOVE";  break;
            case SDL_EVENT_MOUSE_WHEEL:        type_name = "WHEEL";  break;
            default: break;
        }
        if (type_name[0] != '?') {
            snprintf(state->input_log[state->input_log_head % 8], 64, "%s", type_name);
            state->input_log_head++;
        }
    }

    if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        return;
    }

    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        target_input->x = event->motion.x * scale;
        target_input->y = event->motion.y * scale;
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT) {
        target_input->x = event->button.x * scale;
        target_input->y = event->button.y * scale;
        target_input->left_down = true;
        target_input->left_pressed = true;
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_RIGHT) {
        target_input->x = event->button.x * scale;
        target_input->y = event->button.y * scale;
        target_input->right_down = true;
        target_input->right_pressed = true;
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_LEFT) {
        target_input->x = event->button.x * scale;
        target_input->y = event->button.y * scale;
        target_input->left_down = false;
        target_input->left_released = true;
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_RIGHT) {
        target_input->x = event->button.x * scale;
        target_input->y = event->button.y * scale;
        target_input->right_down = false;
        target_input->right_released = true;
    } else if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        target_input->wheel_x = event->wheel.x;
        target_input->wheel_y = event->wheel.y;
    } else if (event->type == SDL_EVENT_KEY_DOWN) {
        switch (event->key.key) {
            case SDLK_LCTRL: case SDLK_RCTRL: state->input.ctrl_down = true; break;
            case SDLK_LSHIFT: case SDLK_RSHIFT: state->input.shift_down = true; break;
            case SDLK_LALT: case SDLK_RALT: state->input.alt_down = true; break;
            case SDLK_EQUALS: case SDLK_KP_PLUS: state->input.zoom_in_pressed = true; break;
            case SDLK_MINUS: case SDLK_KP_MINUS: state->input.zoom_out_pressed = true; break;
            case SDLK_V: state->timeline.active_tool = MUZZA_TOOL_SELECT; break;
            case SDLK_C: state->timeline.active_tool = MUZZA_TOOL_RAZOR; break;
            case SDLK_R: state->timeline.active_tool = MUZZA_TOOL_RIPPLE; break;
        }
    } else if (event->type == SDL_EVENT_KEY_UP) {
        switch (event->key.key) {
            case SDLK_LCTRL: case SDLK_RCTRL: state->input.ctrl_down = false; break;
            case SDLK_LSHIFT: case SDLK_RSHIFT: state->input.shift_down = false; break;
            case SDLK_LALT: case SDLK_RALT: state->input.alt_down = false; break;
        }
    }
}

void ui_render(fx_canvas* canvas, muzza_ui_state* state, muzza_ui_actions* actions) {
    float scale = state->ui_scale > 1.0f ? state->ui_scale : 1.0f;
    float window_w = (float)state->window_width;
    float window_h = (float)state->window_height;
    const float menu_h = 32.0f * scale;
    float left_w = 0.0f;
    float top_h = 0.0f;

    if (!canvas || !state || !state->project || window_w <= 0.0f || window_h <= 0.0f) {
        return;
    }

    reset_actions(actions);
    sanitize_state(state);

    fx_clear(canvas, MUZZA_COLOR_BG_MAIN);
    update_splitters(state, window_w, window_h, menu_h);

    left_w = window_w * state->split_v;
    top_h = window_h * state->split_h;

    ui_draw_rect(canvas, 0.0f, 0.0f, window_w, menu_h, MUZZA_COLOR_BG_PANEL);
    ui_draw_rect(canvas, 8.0f * scale, 7.0f * scale, 18.0f * scale, 18.0f * scale, MUZZA_COLOR_BG_HEADER);
    ui_draw_text(canvas, 36.0f * scale, 10.0f * scale, "MUZZA", 2.0f * scale, 0xFFD7DEE3);

    if (state->is_playing) {
        draw_icon_pause(canvas, 9.0f * scale, 8.0f * scale, 16.0f * scale, 0xFFD7DEE3);
    } else {
        draw_icon_play(canvas, 9.0f * scale, 8.0f * scale, 16.0f * scale, 0xFFD7DEE3);
    }

    /* Export button — flush right */
    float export_w = 70.0f * scale;
    float export_x = window_w - export_w - 8.0f * scale;
    ui_draw_rect(canvas, export_x, 7.0f * scale, export_w, 18.0f * scale, MUZZA_COLOR_ACCENT);
    ui_draw_text_centered(canvas, export_x, 7.0f * scale, export_w, 18.0f * scale, "EXPORT", 1.0f * scale, 0xFFF0F7F6);

    if (state->input.left_pressed
        && ui_point_in_rect(state->input.x, state->input.y, 8.0f * scale, 7.0f * scale, 18.0f * scale, 18.0f * scale)) {
        actions->toggle_playback = true;
    }

    if (state->input.left_pressed
        && ui_point_in_rect(state->input.x, state->input.y, export_x, 7.0f * scale, export_w, 18.0f * scale)) {
        actions->open_export_panel = true;
    }

    ui_draw_panel(canvas, 0.0f, menu_h, left_w, top_h - menu_h, scale, MUZZA_COLOR_ACCENT);
    ui_draw_panel(canvas, left_w, menu_h, window_w - left_w, top_h - menu_h, scale, MUZZA_COLOR_ACCENT);
    ui_draw_panel(canvas, 0.0f, top_h, window_w, window_h - top_h, scale, MUZZA_COLOR_ACCENT_DIM);

    ui_draw_media_panel(canvas, state, actions, 0.0f, menu_h, left_w, top_h - menu_h);
    ui_draw_monitor_panel(canvas, state, left_w, menu_h, window_w - left_w, top_h - menu_h);
    ui_draw_timeline_panel(canvas, state, actions, 0.0f, top_h, window_w, window_h - top_h);
    ui_draw_media_drag_overlay(canvas, state);

    if (state->input.left_released) {
        state->media_panel.is_dragging_media = false;
        state->media_panel.dragged_media_index = -1;
    }

    /* Tooltip */
    if (state->tooltip_visible && state->tooltip_text[0] != '\0') {
        float tx = state->input.x + 18.0f * scale;
        float ty = state->input.y + 18.0f * scale;
        float pad = 6.0f * scale;
        float text_w = (float)strlen(state->tooltip_text) * 8.0f * scale;
        float tw = text_w + pad * 2.0f;
        float th = 16.0f * scale + pad * 2.0f;
        if (tx + tw > window_w) tx = window_w - tw - 4.0f * scale;
        if (ty + th > window_h) ty = state->input.y - th - 4.0f * scale;
        ui_draw_rect(canvas, tx, ty, tw, th, MUZZA_COLOR_BG_OVERLAY);
        ui_draw_border(canvas, tx, ty, tw, th, scale, MUZZA_COLOR_ACCENT);
        ui_draw_text(canvas, tx + pad, ty + pad, state->tooltip_text, 1.1f * scale, 0xFFF0F7F6);
    }
    state->tooltip_visible = false;
    state->tooltip_text[0] = '\0';

    /* Debug Overlay */
    if (state->debug_overlay) {
        float dx = 8.0f * scale;
        float dy = menu_h + 4.0f * scale;
        float lh = 13.0f * scale;
        float dbg_w = 320.0f * scale;

        // Count lines needed
        int num_lines = 7; // core lines (FPS, hit, speed, mouse, tool, proj, preview)
        if (state->project) num_lines += MUZZA_MAX_TRACKS; // track lines
        num_lines += 3; // input log header + separators
        float dbg_h = num_lines * lh + 12.0f * scale;
        if (dy + dbg_h > window_h) dy = window_h - dbg_h - 4.0f * scale;

        ui_draw_rect(canvas, dx, dy, dbg_w, dbg_h, MUZZA_COLOR_BG_OVERLAY);
        ui_draw_border(canvas, dx, dy, dbg_w, dbg_h, scale, MUZZA_COLOR_ACCENT);

        char buf[128];
        int line = 0;

        // FPS and frame timing
        snprintf(buf, sizeof(buf), "FPS: %5.1f   dt: %4.1f ms",
            state->fps_display, 1000.0f / (state->fps_display > 0.5f ? state->fps_display : 60.0f));
        ui_draw_text(canvas, dx + 4.0f * scale, dy + 4.0f * scale + lh * (line++), buf, 1.0f * scale, 0xFF00FF88);

        // Cursor hit region (like web inspector)
        snprintf(buf, sizeof(buf), "Hit: %s", ui_hit_region_name(state));
        ui_draw_text(canvas, dx + 4.0f * scale, dy + 4.0f * scale + lh * (line++), buf, 1.0f * scale, 0xFFFFAA33);

        // Playback state
        snprintf(buf, sizeof(buf), "Speed: %+5.1fx  %s  @ %.2fs / %.2fs",
            (double)state->playback_speed, state->is_playing ? "PLAY" : "STOP",
            state->timeline.playhead_time, state->project ? state->project->duration : 0.0);
        ui_draw_text(canvas, dx + 4.0f * scale, dy + 4.0f * scale + lh * (line++), buf, 1.0f * scale, 0xFFD7DEE3);

        // Mouse position
        snprintf(buf, sizeof(buf), "Mouse: (%.0f, %.0f)  Win: %dx%d  Scale: %.1fx",
            state->input.x, state->input.y, state->window_width, state->window_height, (double)state->ui_scale);
        ui_draw_text(canvas, dx + 4.0f * scale, dy + 4.0f * scale + lh * (line++), buf, 1.0f * scale, 0xFFD7DEE3);

        // Timeline state
        const char* tool_names[] = { "SEL", "RAZ", "RIP" };
        int tool_idx = state->timeline.active_tool;
        snprintf(buf, sizeof(buf), "Tool: %s  Clip#%d  Zoom: %.0fpx/s  Scrl: %.1f  Trim: %d  Fade: %d",
            tool_idx < 3 ? tool_names[tool_idx] : "?",
            state->timeline.selected_clip_index,
            (double)state->timeline.zoom, state->timeline.scroll_x,
            state->timeline.is_trimming, state->timeline.is_dragging_fade);
        ui_draw_text(canvas, dx + 4.0f * scale, dy + 4.0f * scale + lh * (line++), buf, 1.0f * scale, 0xFFD7DEE3);

        // Project stats
        int active_decs = 0;
        if (state->project) {
            for (size_t i = 0; i < state->project->num_clips; ++i) {
                if (state->project->clips[i].dec) active_decs++;
            }
        }
        size_t clip_mem = state->project ? state->project->num_clips * sizeof(muzza_clip) : 0;
        size_t media_mem = state->project ? state->project->num_media * sizeof(muzza_media) : 0;
        snprintf(buf, sizeof(buf), "Proj: %zuM %zuC  Dec: %d  Mem: %zu KB",
            state->project ? state->project->num_media : 0,
            state->project ? state->project->num_clips : 0,
            active_decs,
            (clip_mem + media_mem) / 1024);
        ui_draw_text(canvas, dx + 4.0f * scale, dy + 4.0f * scale + lh * (line++), buf, 1.0f * scale, 0xFFD7DEE3);

        // Preview / Export
        snprintf(buf, sizeof(buf), "Preview: %dx%d  Export: %s %d%%",
            state->preview.frame_width, state->preview.frame_height,
            state->export_panel.is_exporting ? "RUN" : state->export_panel.visible ? "OPEN" : "OFF",
            (int)(state->export_panel.progress * 100.0f));
        ui_draw_text(canvas, dx + 4.0f * scale, dy + 4.0f * scale + lh * (line++), buf, 1.0f * scale, 0xFF8FA0A9);

        // Track states
        if (state->project) {
            ui_draw_text(canvas, dx + 4.0f * scale, dy + 4.0f * scale + lh * (line++), "-- Tracks --", 0.9f * scale, 0xFF555555);
            for (int t = 0; t < MUZZA_MAX_TRACKS; ++t) {
                snprintf(buf, sizeof(buf), " T%d: %s%s%s  %-5s h=%3.0f  gn=%1.1f",
                    t + 1,
                    state->project->tracks[t].muted ? "M" : ".",
                    state->project->tracks[t].solo ? "S" : ".",
                    state->project->tracks[t].locked ? "L" : ".",
                    state->project->tracks[t].name,
                    (double)state->project->tracks[t].height,
                    (double)state->project->tracks[t].gain);
                ui_draw_text(canvas, dx + 4.0f * scale, dy + 4.0f * scale + lh * (line++), buf, 0.9f * scale, 0xFF7A8A9A);
            }
        }

        // Input event log
        ui_draw_text(canvas, dx + 4.0f * scale, dy + 4.0f * scale + lh * (line++), "-- Input Log --", 0.9f * scale, 0xFF555555);
        int log_start = state->input_log_head - 8;
        if (log_start < 0) log_start = 0;
        char log_line[128] = "";
        for (int i = 0; i < 8 && (log_start + i) < state->input_log_head; ++i) {
            size_t len = strlen(log_line);
            snprintf(log_line + len, sizeof(log_line) - len, " %s", state->input_log[(log_start + i) % 8]);
        }
        ui_draw_text(canvas, dx + 4.0f * scale, dy + 4.0f * scale + lh * (line++), log_line[0] ? log_line : "  (none)", 0.8f * scale, 0xFF555555);

        // Footer
        snprintf(buf, sizeof(buf), "Ctrl-D: hide  |  v0.1.7");
        ui_draw_text(canvas, dx + 4.0f * scale, dy + 4.0f * scale + lh * (line++), buf, 0.8f * scale, 0xFF444444);
    }
}
