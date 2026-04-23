#include "ui.h"

#include <string.h>

#include "import_browser.h"
#include "path_utils.h"
#include "ui_icons.h"
#include "ui_import_browser_panel.h"
#include "ui_media_panel.h"
#include "ui_monitor_panel.h"
#include "ui_shared.h"
#include "ui_text.h"
#include "ui_timeline_panel.h"

static void reset_actions(muzza_ui_actions* actions) {
    if (!actions) {
        return;
    }

    memset(actions, 0, sizeof(*actions));
    actions->delete_media_id = -1;
    actions->insert_media_id = -1;
    actions->insert_track_index = -1;
    actions->insert_timeline_time = -1.0;
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

    if (state->import_browser.visible) {
        if (state->input.left_released) {
            state->is_dragging_splitter_v = false;
            state->is_dragging_splitter_h = false;
        }
        return;
    }

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
    state->preview.preview_media_index = -1;
    state->playback.clip_index = -1;
    state->playback.media_id = -1;
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
}

void ui_handle_event(muzza_ui_state* state, const SDL_Event* event, SDL_Window* window) {
    float scale = window ? SDL_GetWindowDisplayScale(window) : 1.0f;

    if (!state || !event) {
        return;
    }

    if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED) {
        return;
    }

    if (event->type == SDL_EVENT_MOUSE_MOTION) {
        state->input.x = event->motion.x * scale;
        state->input.y = event->motion.y * scale;
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_LEFT) {
        state->input.x = event->button.x * scale;
        state->input.y = event->button.y * scale;
        state->input.left_down = true;
        state->input.left_pressed = true;
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN && event->button.button == SDL_BUTTON_RIGHT) {
        state->input.x = event->button.x * scale;
        state->input.y = event->button.y * scale;
        state->input.right_down = true;
        state->input.right_pressed = true;
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_LEFT) {
        state->input.x = event->button.x * scale;
        state->input.y = event->button.y * scale;
        state->input.left_down = false;
        state->input.left_released = true;
    } else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP && event->button.button == SDL_BUTTON_RIGHT) {
        state->input.x = event->button.x * scale;
        state->input.y = event->button.y * scale;
        state->input.right_down = false;
        state->input.right_released = true;
    } else if (event->type == SDL_EVENT_MOUSE_WHEEL) {
        state->input.wheel_x = event->wheel.x;
        state->input.wheel_y = event->wheel.y;
    } else if (event->type == SDL_EVENT_KEY_DOWN) {
        switch (event->key.key) {
            case SDLK_LCTRL: case SDLK_RCTRL: state->input.ctrl_down = true; break;
            case SDLK_LSHIFT: case SDLK_RSHIFT: state->input.shift_down = true; break;
            case SDLK_LALT: case SDLK_RALT: state->input.alt_down = true; break;
            case SDLK_EQUALS: case SDLK_KP_PLUS: state->input.zoom_in_pressed = true; break;
            case SDLK_MINUS: case SDLK_KP_MINUS: state->input.zoom_out_pressed = true; break;
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
    ui_draw_text_right(canvas, window_w - 12.0f * scale, 10.0f * scale, state->import_browser.visible ? "IMPORT BROWSER" : "EDITOR", 2.0f * scale, 0xFF8FA0A9);
    if (state->is_playing) {
        draw_icon_pause(canvas, 9.0f * scale, 8.0f * scale, 16.0f * scale, 0xFFD7DEE3);
    } else {
        draw_icon_play(canvas, 9.0f * scale, 8.0f * scale, 16.0f * scale, 0xFFD7DEE3);
    }

    if (!state->import_browser.visible
        && state->input.left_pressed
        && ui_point_in_rect(state->input.x, state->input.y, 8.0f * scale, 7.0f * scale, 18.0f * scale, 18.0f * scale)) {
        actions->toggle_playback = true;
    }

    ui_draw_panel(canvas, 0.0f, menu_h, left_w, top_h - menu_h, scale, MUZZA_COLOR_ACCENT);
    ui_draw_panel(canvas, left_w, menu_h, window_w - left_w, top_h - menu_h, scale, MUZZA_COLOR_ACCENT);
    ui_draw_panel(canvas, 0.0f, top_h, window_w, window_h - top_h, scale, MUZZA_COLOR_ACCENT_DIM);

    ui_draw_media_panel(canvas, state, actions, 0.0f, menu_h, left_w, top_h - menu_h);
    ui_draw_monitor_panel(canvas, state, left_w, menu_h, window_w - left_w, top_h - menu_h);
    ui_draw_timeline_panel(canvas, state, actions, 0.0f, top_h, window_w, window_h - top_h);
    ui_draw_media_drag_overlay(canvas, state);
    ui_draw_import_browser(canvas, state, actions);

    if (state->input.left_released) {
        state->media_panel.is_dragging_media = false;
        state->media_panel.dragged_media_index = -1;
    }
}
