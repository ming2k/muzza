#include "ui_media_panel.h"

#include <stdio.h>
#include <string.h>

#include "path_utils.h"
#include "ui_icons.h"
#include "ui_shared.h"
#include "ui_text.h"

static fx_color media_color(const muzza_media* media) {
    if (!media) {
        return MUZZA_COLOR_MEDIA_EMPTY;
    }
    if (media->has_video && media->has_audio) {
        return MUZZA_COLOR_MEDIA_MIXED;
    }
    if (media->has_video) {
        return MUZZA_COLOR_MEDIA_VIDEO;
    }
    if (media->has_audio) {
        return MUZZA_COLOR_MEDIA_AUDIO;
    }
    return MUZZA_COLOR_MEDIA_EMPTY;
}

static void draw_media_type_glyph(fx_canvas* canvas, const muzza_media* media, float x, float y, float size) {
    fx_color color = media_color(media);

    if (!media) {
        return;
    }

    if (media->has_video) {
        draw_icon_clip(canvas, x, y, size, color);
    } else {
        ui_draw_rect(canvas, x + size * 0.20f, y + size * 0.25f, size * 0.18f, size * 0.50f, color);
        ui_draw_rect(canvas, x + size * 0.50f, y + size * 0.18f, size * 0.14f, size * 0.64f, color);
        ui_draw_rect(canvas, x + size * 0.72f, y + size * 0.35f, size * 0.10f, size * 0.30f, color);
    }
}

static void draw_add_tile(fx_canvas* canvas, float x, float y, float w, float h, float scale, bool hovered) {
    fx_color fill = hovered ? MUZZA_COLOR_ACCENT_DIM : MUZZA_COLOR_TRACK_ALT;
    float cx = x + w * 0.5f;
    float cy = y + h * 0.5f - 6.0f * scale;

    ui_draw_rect(canvas, x, y, w, h, fill);
    ui_draw_border(canvas, x, y, w, h, scale, hovered ? MUZZA_COLOR_ACCENT : MUZZA_COLOR_BORDER);
    ui_draw_rect(canvas, cx - 14.0f * scale, cy - 2.0f * scale, 28.0f * scale, 4.0f * scale, 0xFFE7F4F3);
    ui_draw_rect(canvas, cx - 2.0f * scale, cy - 14.0f * scale, 4.0f * scale, 28.0f * scale, 0xFFE7F4F3);
    ui_draw_text_centered(canvas, x, y + 50.0f * scale, w, 12.0f * scale, "ADD", 2.0f * scale, 0xFFE7F4F3);
    ui_draw_text_centered(canvas, x, y + 64.0f * scale, w, 10.0f * scale, "MEDIA", 1.0f * scale, 0xFFCBD7D6);
}

static void sanitize_panel_state(muzza_ui_state* state) {
    if (!state || !state->project) {
        return;
    }

    if (state->media_panel.selected_media_index >= (int)state->project->num_media) {
        state->media_panel.selected_media_index = state->project->num_media > 0 ? 0 : -1;
    }
    if (state->media_panel.selected_media_index < -1) {
        state->media_panel.selected_media_index = -1;
    }
    if (state->media_panel.context_menu_media_index >= (int)state->project->num_media) {
        state->media_panel.context_menu_media_index = -1;
        state->media_panel.show_context_menu = false;
    }
    if (state->media_panel.dragged_media_index >= (int)state->project->num_media) {
        state->media_panel.dragged_media_index = -1;
        state->media_panel.is_dragging_media = false;
    }
}

void ui_draw_media_panel(fx_canvas* canvas, muzza_ui_state* state, muzza_ui_actions* actions, float x, float y, float w, float h) {
    const float s = state->ui_scale > 1.0f ? state->ui_scale : 1.0f;
    const float pad = 12.0f * s;
    const float top = y + 40.0f * s;
    const float tile_w = 104.0f * s;
    const float tile_h = 84.0f * s;
    const float gap = 10.0f * s;
    const float footer_h = 74.0f * s;
    const int columns = (int)((w - pad * 2.0f + gap) / (tile_w + gap));
    int safe_columns = columns > 0 ? columns : 1;
    bool allow_interaction = true;
    bool clicked_empty = false;
    bool consumed_click = false;

    sanitize_panel_state(state);
    ui_draw_text(canvas, x + 14.0f * s, y + 12.0f * s, "MEDIA BIN", 2.0f * s, 0xFFD7DEE3);
    ui_draw_text_right(canvas, x + w - 14.0f * s, y + 12.0f * s, "PROJECT", 2.0f * s, 0xFF8FA0A9);

    if (allow_interaction && state->input.left_pressed && ui_point_in_rect(state->input.x, state->input.y, x, y, w, h)) {
        clicked_empty = true;
    }

    for (int slot = 0; slot <= (int)state->project->num_media; ++slot) {
        bool is_add_tile = slot == 0;
        int media_index = slot - 1;
        int row = slot / safe_columns;
        int col = slot % safe_columns;
        float tx = x + pad + col * (tile_w + gap);
        float ty = top + row * (tile_h + gap);
        bool hovered = ui_point_in_rect(state->input.x, state->input.y, tx, ty, tile_w, tile_h);

        if (ty + tile_h > y + h - footer_h) {
            break;
        }

        if (is_add_tile) {
            draw_add_tile(canvas, tx, ty, tile_w, tile_h, s, hovered && allow_interaction);
            if (allow_interaction && state->input.left_pressed && hovered) {
                actions->open_import_browser = true;
                state->media_panel.show_context_menu = false;
                consumed_click = true;
            }
            continue;
        }

        muzza_media* media = &state->project->media_pool[media_index];
        fx_color tile_color = media_color(media);
        bool selected = media_index == state->media_panel.selected_media_index;

        ui_draw_rect(canvas, tx, ty, tile_w, tile_h, MUZZA_COLOR_TRACK_ALT);
        ui_draw_rect(canvas, tx + 1.0f * s, ty + 1.0f * s, tile_w - 2.0f * s, 44.0f * s, 0xFF0A0C0D);
        draw_media_type_glyph(canvas, media, tx + 30.0f * s, ty + 7.0f * s, 44.0f * s);
        ui_draw_rect(canvas, tx + 10.0f * s, ty + 50.0f * s, tile_w - 20.0f * s, 2.0f * s, tile_color);
        ui_draw_text_ellipsis(canvas, tx + 9.0f * s, ty + 57.0f * s, tile_w - 18.0f * s, muzza_path_basename(media->filepath), 1.0f * s, 0xFFD7DEE3);
        ui_draw_text(canvas, tx + 9.0f * s, ty + 70.0f * s, media->has_video && media->has_audio ? "A/V" : (media->has_video ? "VIDEO" : (media->has_audio ? "AUDIO" : "EMPTY")), 1.0f * s, tile_color);
        ui_draw_border(canvas, tx, ty, tile_w, tile_h, s, selected ? MUZZA_COLOR_ACCENT : MUZZA_COLOR_BORDER);

        if (hovered && allow_interaction) {
            ui_draw_border(canvas, tx + 1.0f * s, ty + 1.0f * s, tile_w - 2.0f * s, tile_h - 2.0f * s, s, selected ? MUZZA_COLOR_ACCENT : MUZZA_COLOR_ACCENT_DIM);
        }

        if (allow_interaction && state->input.left_pressed && hovered) {
            state->media_panel.selected_media_index = media_index;
            state->timeline.selected_clip_index = -1;
            state->media_panel.show_context_menu = false;
            state->media_panel.is_dragging_media = true;
            state->media_panel.dragged_media_index = media_index;
            project_clear_selection(state->project);
            consumed_click = true;
        }

        if (allow_interaction && state->input.right_pressed && hovered) {
            state->media_panel.selected_media_index = media_index;
            state->timeline.selected_clip_index = -1;
            state->media_panel.show_context_menu = true;
            state->media_panel.context_menu_x = state->input.x;
            state->media_panel.context_menu_y = state->input.y;
            state->media_panel.context_menu_media_index = media_index;
            state->media_panel.is_dragging_media = false;
            state->media_panel.dragged_media_index = -1;
            project_clear_selection(state->project);
            consumed_click = true;
        }
    }

    if (allow_interaction && clicked_empty && !consumed_click) {
        state->media_panel.show_context_menu = false;
    }

    ui_draw_rect(canvas, x + 1.0f * s, y + h - footer_h, w - 2.0f * s, footer_h - 1.0f * s, MUZZA_COLOR_BG_HEADER);
    if (state->media_panel.selected_media_index >= 0 && (size_t)state->media_panel.selected_media_index < state->project->num_media) {
        char duration_text[32];
        const muzza_media* media = &state->project->media_pool[state->media_panel.selected_media_index];
        snprintf(duration_text, sizeof(duration_text), "%.1fs", media->duration);
        ui_draw_text_ellipsis(canvas, x + 14.0f * s, y + h - 57.0f * s, w - 156.0f * s, muzza_path_basename(media->filepath), 2.0f * s, 0xFFD4DDE1);
        ui_draw_text(canvas, x + 14.0f * s, y + h - 34.0f * s, media->has_video && media->has_audio ? "A/V MEDIA" : (media->has_video ? "VIDEO MEDIA" : "AUDIO MEDIA"), 1.0f * s, media_color(media));
        ui_draw_text(canvas, x + 14.0f * s, y + h - 20.0f * s, duration_text, 1.0f * s, 0xFF93A0A8);

        ui_draw_rect(canvas, x + w - 118.0f * s, y + h - 50.0f * s, 100.0f * s, 30.0f * s, MUZZA_COLOR_ACCENT);
        ui_draw_border(canvas, x + w - 118.0f * s, y + h - 50.0f * s, 100.0f * s, 30.0f * s, s, MUZZA_COLOR_ACCENT_DIM);
        ui_draw_text_centered(canvas, x + w - 118.0f * s, y + h - 50.0f * s, 100.0f * s, 30.0f * s, "INSERT", 1.0f * s, 0xFFF0F7F6);

        if (allow_interaction
            && state->input.left_pressed
            && ui_point_in_rect(state->input.x, state->input.y, x + w - 118.0f * s, y + h - 50.0f * s, 100.0f * s, 30.0f * s)) {
            actions->insert_media_id = media->id;
        }
    } else {
        ui_draw_text(canvas, x + 14.0f * s, y + h - 43.0f * s, "SELECT MEDIA TO PREVIEW OR INSERT", 1.0f * s, 0xFF93A0A8);
    }

    if (allow_interaction && state->media_panel.show_context_menu && state->media_panel.context_menu_media_index >= 0) {
        int media_index = state->media_panel.context_menu_media_index;
        const muzza_media* media = &state->project->media_pool[media_index];
        float menu_x = ui_clampf(state->media_panel.context_menu_x, x + 6.0f * s, x + w - 130.0f * s);
        float menu_y = ui_clampf(state->media_panel.context_menu_y, y + 34.0f * s, y + h - 86.0f * s);
        float insert_y = menu_y + 8.0f * s;
        float delete_y = menu_y + 38.0f * s;

        ui_draw_rect(canvas, menu_x, menu_y, 120.0f * s, 70.0f * s, MUZZA_COLOR_BG_HEADER);
        ui_draw_border(canvas, menu_x, menu_y, 120.0f * s, 70.0f * s, s, MUZZA_COLOR_BORDER);

        ui_draw_rect(canvas, menu_x + 8.0f * s, insert_y, 104.0f * s, 22.0f * s, MUZZA_COLOR_ACCENT_DIM);
        ui_draw_text_centered(canvas, menu_x + 8.0f * s, insert_y, 104.0f * s, 22.0f * s, "INSERT", 1.0f * s, 0xFFEFF6F5);
        ui_draw_rect(canvas, menu_x + 8.0f * s, delete_y, 104.0f * s, 22.0f * s, MUZZA_COLOR_ALERT);
        ui_draw_text_centered(canvas, menu_x + 8.0f * s, delete_y, 104.0f * s, 22.0f * s, "DELETE", 1.0f * s, 0xFFF7EAEA);

        if (state->input.left_pressed) {
            if (ui_point_in_rect(state->input.x, state->input.y, menu_x + 8.0f * s, insert_y, 104.0f * s, 22.0f * s)) {
                actions->insert_media_id = media->id;
                state->media_panel.show_context_menu = false;
            } else if (ui_point_in_rect(state->input.x, state->input.y, menu_x + 8.0f * s, delete_y, 104.0f * s, 22.0f * s)) {
                actions->delete_media_id = media->id;
                state->media_panel.show_context_menu = false;
            } else if (!ui_point_in_rect(state->input.x, state->input.y, menu_x, menu_y, 120.0f * s, 70.0f * s)) {
                state->media_panel.show_context_menu = false;
            }
        }
    }
}

void ui_draw_media_drag_overlay(fx_canvas* canvas, const muzza_ui_state* state) {
    const float s = (state && state->ui_scale > 1.0f) ? state->ui_scale : 1.0f;
    float card_w = 180.0f * s;
    float card_h = 44.0f * s;

    if (!state || !state->project || !state->media_panel.is_dragging_media || !state->input.left_down) {
        return;
    }

    if (state->media_panel.dragged_media_index < 0 || (size_t)state->media_panel.dragged_media_index >= state->project->num_media) {
        return;
    }

    const muzza_media* media = &state->project->media_pool[state->media_panel.dragged_media_index];
    float x = state->input.x + 14.0f * s;
    float y = state->input.y + 14.0f * s;

    ui_draw_rect(canvas, x, y, card_w, card_h, MUZZA_COLOR_BG_HEADER);
    ui_draw_border(canvas, x, y, card_w, card_h, s, media_color(media));
    ui_draw_text_ellipsis(canvas, x + 10.0f * s, y + 8.0f * s, card_w - 20.0f * s, muzza_path_basename(media->filepath), 1.0f * s, 0xFFEAF1F4);
    ui_draw_text(canvas, x + 10.0f * s, y + 22.0f * s, "DRAG TO TIMELINE", 1.0f * s, media_color(media));
}
