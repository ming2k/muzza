#include "ui_export_panel.h"

#include <string.h>
#include <stdio.h>

#include "ui_shared.h"
#include "ui_text.h"

void ui_draw_export_panel(fx_canvas* canvas, muzza_ui_state* state, muzza_ui_actions* actions) {
    muzza_export_panel_state* panel = &state->export_panel;
    float s = state->ui_scale > 1.0f ? state->ui_scale : 1.0f;
    float overlay_w = (float)state->window_width;
    float overlay_h = (float)state->window_height;
    float panel_w = ui_clampf(overlay_w * 0.48f, 420.0f * s, 640.0f * s);
    float panel_h = 280.0f * s;
    float panel_x = (overlay_w - panel_w) * 0.5f;
    float panel_y = (overlay_h - panel_h) * 0.5f;
    float close_x = panel_x + panel_w - 42.0f * s;
    float cancel_x = panel_x + 18.0f * s;
    float export_x = panel_x + panel_w - 116.0f * s;
    bool has_content = state->project && state->project->duration > 0.0;
    bool is_exporting = panel->is_exporting;
    float progress = panel->progress;

    if (!panel->visible) {
        return;
    }

    /* Dim overlay */
    ui_draw_rect(canvas, 0.0f, 0.0f, overlay_w, overlay_h, MUZZA_COLOR_BG_OVERLAY);
    ui_draw_rect(canvas, panel_x, panel_y, panel_w, panel_h, MUZZA_COLOR_BG_PANEL);
    ui_draw_rect(canvas, panel_x, panel_y, panel_w, 42.0f * s, MUZZA_COLOR_BG_HEADER);
    ui_draw_border(canvas, panel_x, panel_y, panel_w, panel_h, s, MUZZA_COLOR_BORDER);

    /* Title */
    ui_draw_text(canvas, panel_x + 18.0f * s, panel_y + 14.0f * s, "EXPORT PROJECT", 2.0f * s, 0xFFD7DEE3);

    /* Close button */
    ui_draw_rect(canvas, close_x, panel_y + 10.0f * s, 24.0f * s, 24.0f * s, MUZZA_COLOR_TRACK_ALT);
    ui_draw_rect(canvas, close_x + 6.0f * s, panel_y + 16.0f * s, 12.0f * s, 3.0f * s, 0xFFE7F4F3);
    ui_draw_rect(canvas, close_x + 6.0f * s, panel_y + 19.0f * s, 12.0f * s, 3.0f * s, 0xFFE7F4F3);

    /* Output path */
    ui_draw_rect(canvas, panel_x + 18.0f * s, panel_y + 56.0f * s, panel_w - 36.0f * s, 18.0f * s, MUZZA_COLOR_TRACK_BG);
    ui_draw_text_ellipsis(canvas, panel_x + 28.0f * s, panel_y + 60.0f * s, panel_w - 56.0f * s,
        panel->output_path[0] ? panel->output_path : "output.mp4", 1.0f * s, 0xFFCAD3D9);

    /* Info row */
    char info_buf[128] = {0};
    if (state->project) {
        int mins = (int)(state->project->duration / 60.0);
        int secs = (int)(state->project->duration) % 60;
        snprintf(info_buf, sizeof(info_buf), "Duration: %02d:%02d  |  Format: MP4 (H.264 / AAC)", mins, secs);
    } else {
        snprintf(info_buf, sizeof(info_buf), "No project loaded");
    }
    ui_draw_text(canvas, panel_x + 18.0f * s, panel_y + 86.0f * s, info_buf, 1.0f * s, 0xFF7C8A92);

    /* Progress bar (visible when exporting or done) */
    if (is_exporting || progress >= 1.0 || panel->show_progress) {
        float bar_y = panel_y + 116.0f * s;
        float bar_w = panel_w - 36.0f * s;
        float bar_h = 14.0f * s;
        float fill_w = bar_w * progress;

        ui_draw_rect(canvas, panel_x + 18.0f * s, bar_y, bar_w, bar_h, MUZZA_COLOR_TRACK_BG);
        ui_draw_border(canvas, panel_x + 18.0f * s, bar_y, bar_w, bar_h, s, MUZZA_COLOR_BORDER);
        if (fill_w > 0.0f) {
            ui_draw_rect(canvas, panel_x + 18.0f * s, bar_y, fill_w, bar_h, MUZZA_COLOR_ACCENT);
        }

        /* Status text */
        char status_buf[128] = {0};
        if (panel->status == 2) { /* done */
            snprintf(status_buf, sizeof(status_buf), "Export complete!");
        } else if (panel->status == 4) { /* error */
            snprintf(status_buf, sizeof(status_buf), "Error: %s", panel->status_msg);
        } else if (panel->status == 3) { /* cancelled */
            snprintf(status_buf, sizeof(status_buf), "Export cancelled.");
        } else {
            snprintf(status_buf, sizeof(status_buf), "Exporting... %.0f%%", progress * 100.0f);
        }
        ui_draw_text(canvas, panel_x + 18.0f * s, bar_y + 22.0f * s, status_buf, 1.0f * s,
            panel->status == 4 ? MUZZA_COLOR_ALERT : 0xFFCAD3D9);
    }

    /* No content warning */
    if (!has_content && !is_exporting) {
        ui_draw_text_centered(canvas, panel_x + 18.0f * s, panel_y + 130.0f * s,
            panel_w - 36.0f * s, 20.0f * s, "TIMELINE IS EMPTY", 1.0f * s, 0xFF7C8A92);
    }

    /* Cancel button */
    ui_draw_rect(canvas, cancel_x, panel_y + panel_h - 46.0f * s, 84.0f * s, 28.0f * s, MUZZA_COLOR_TRACK_ALT);
    ui_draw_border(canvas, cancel_x, panel_y + panel_h - 46.0f * s, 84.0f * s, 28.0f * s, s, MUZZA_COLOR_BORDER);
    ui_draw_text_centered(canvas, cancel_x, panel_y + panel_h - 46.0f * s, 84.0f * s, 28.0f * s,
        is_exporting ? "CANCEL" : "CLOSE", 1.0f * s, 0xFFCAD3D9);

    /* Export button */
    bool can_export = has_content && !is_exporting && panel->status != 2;
    ui_draw_rect(canvas, export_x, panel_y + panel_h - 46.0f * s, 98.0f * s, 28.0f * s,
        can_export ? MUZZA_COLOR_ACCENT : MUZZA_COLOR_TRACK_EDGE);
    ui_draw_border(canvas, export_x, panel_y + panel_h - 46.0f * s, 98.0f * s, 28.0f * s, s,
        can_export ? MUZZA_COLOR_ACCENT_DIM : MUZZA_COLOR_BORDER);
    ui_draw_text_centered(canvas, export_x, panel_y + panel_h - 46.0f * s, 98.0f * s, 28.0f * s,
        "EXPORT", 1.0f * s, 0xFFF0F7F6);

    /* Interaction handling */
    if (state->input.left_pressed) {
        /* Close button */
        if (ui_point_in_rect(state->input.x, state->input.y, close_x, panel_y + 10.0f * s, 24.0f * s, 24.0f * s)) {
            if (is_exporting) {
                panel->cancel_requested = true;
            }
            panel->visible = false;
            return;
        }

        /* Cancel/Close button */
        if (ui_point_in_rect(state->input.x, state->input.y, cancel_x, panel_y + panel_h - 46.0f * s, 84.0f * s, 28.0f * s)) {
            if (is_exporting) {
                panel->cancel_requested = true;
            } else {
                panel->visible = false;
            }
            return;
        }

        /* Export button */
        if (can_export && ui_point_in_rect(state->input.x, state->input.y,
            export_x, panel_y + panel_h - 46.0f * s, 98.0f * s, 28.0f * s)) {
            actions->start_export = true;
            return;
        }
    }
}
