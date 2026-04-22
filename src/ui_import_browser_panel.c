#include "ui_import_browser_panel.h"

#include <string.h>

#include "import_browser.h"
#include "path_utils.h"
#include "ui_shared.h"
#include "ui_text.h"

void ui_draw_import_browser(fx_canvas* canvas, muzza_ui_state* state, muzza_ui_actions* actions) {
    muzza_import_browser_state* browser = &state->import_browser;
    float s = state->ui_scale > 1.0f ? state->ui_scale : 1.0f;
    float overlay_w = (float)state->window_width;
    float overlay_h = (float)state->window_height;
    float panel_w = ui_clampf(overlay_w * 0.58f, 520.0f * s, 860.0f * s);
    float panel_h = ui_clampf(overlay_h * 0.72f, 360.0f * s, 680.0f * s);
    float panel_x = (overlay_w - panel_w) * 0.5f;
    float panel_y = (overlay_h - panel_h) * 0.5f;
    float list_x = panel_x + 18.0f * s;
    float list_y = panel_y + 84.0f * s;
    float list_w = panel_w - 36.0f * s;
    float list_h = panel_h - 150.0f * s;
    const float row_h = 30.0f * s;
    const float row_gap = 4.0f * s;
    int visible_rows = (int)(list_h / (row_h + row_gap));
    int max_scroll = 0;
    float up_x = panel_x + 18.0f * s;
    float close_x = panel_x + panel_w - 42.0f * s;
    float cancel_x = panel_x + 18.0f * s;
    float import_x = panel_x + panel_w - 116.0f * s;
    bool can_import = false;

    if (!browser->visible) {
        return;
    }

    if (visible_rows < 1) {
        visible_rows = 1;
    }

    max_scroll = browser->num_files - visible_rows;
    if (max_scroll < 0) {
        max_scroll = 0;
    }
    if (browser->scroll > max_scroll) {
        browser->scroll = max_scroll;
    }
    if (browser->scroll < 0) {
        browser->scroll = 0;
    }

    if (browser->selected_index >= 0 && browser->selected_index < browser->num_files) {
        const muzza_browser_entry* selected = &browser->files[browser->selected_index];
        can_import = !selected->is_dir;
    }

    ui_draw_rect(canvas, 0.0f, 0.0f, overlay_w, overlay_h, MUZZA_COLOR_BG_OVERLAY);
    ui_draw_rect(canvas, panel_x, panel_y, panel_w, panel_h, MUZZA_COLOR_BG_PANEL);
    ui_draw_rect(canvas, panel_x, panel_y, panel_w, 42.0f * s, MUZZA_COLOR_BG_HEADER);
    ui_draw_border(canvas, panel_x, panel_y, panel_w, panel_h, s, MUZZA_COLOR_BORDER);

    ui_draw_text(canvas, panel_x + 18.0f * s, panel_y + 14.0f * s, "IMPORT MEDIA", 2.0f * s, 0xFFD7DEE3);
    ui_draw_rect(canvas, panel_x + 18.0f * s, panel_y + 52.0f * s, panel_w - 36.0f * s, 18.0f * s, MUZZA_COLOR_TRACK_BG);
    ui_draw_text(canvas, panel_x + 56.0f * s, panel_y + 56.0f * s, "DIR", 1.0f * s, 0xFF7C8A92);
    ui_draw_text_ellipsis(canvas, panel_x + 84.0f * s, panel_y + 56.0f * s, panel_w - 132.0f * s, browser->current_dir, 1.0f * s, 0xFFCAD3D9);

    ui_draw_rect(canvas, up_x, panel_y + 52.0f * s, 28.0f * s, 18.0f * s, MUZZA_COLOR_ACCENT_DIM);
    ui_draw_rect(canvas, up_x + 7.0f * s, panel_y + 59.0f * s, 13.0f * s, 4.0f * s, 0xFFE7F4F3);
    ui_draw_rect(canvas, up_x + 7.0f * s, panel_y + 55.0f * s, 4.0f * s, 8.0f * s, 0xFFE7F4F3);

    ui_draw_rect(canvas, close_x, panel_y + 10.0f * s, 24.0f * s, 24.0f * s, MUZZA_COLOR_TRACK_ALT);
    ui_draw_rect(canvas, close_x + 6.0f * s, panel_y + 16.0f * s, 12.0f * s, 3.0f * s, 0xFFE7F4F3);
    ui_draw_rect(canvas, close_x + 6.0f * s, panel_y + 19.0f * s, 12.0f * s, 3.0f * s, 0xFFE7F4F3);

    ui_draw_rect(canvas, list_x, list_y, list_w, list_h, MUZZA_COLOR_TRACK_BG);
    ui_draw_border(canvas, list_x, list_y, list_w, list_h, s, MUZZA_COLOR_BORDER);

    for (int row = 0; row < visible_rows; ++row) {
        int file_index = browser->scroll + row;
        float row_y = list_y + 8.0f * s + row * (row_h + row_gap);

        if (file_index >= browser->num_files || row_y + row_h > list_y + list_h - 8.0f * s) {
            break;
        }

        const muzza_browser_entry* file = &browser->files[file_index];
        bool selected = file_index == browser->selected_index;
        bool hovered = ui_point_in_rect(state->input.x, state->input.y, list_x + 8.0f * s, row_y, list_w - 16.0f * s, row_h);

        ui_draw_rect(canvas, list_x + 8.0f * s, row_y, list_w - 16.0f * s, row_h, hovered ? MUZZA_COLOR_DIALOG_ROW_HI : MUZZA_COLOR_DIALOG_ROW);
        ui_draw_border(canvas, list_x + 8.0f * s, row_y, list_w - 16.0f * s, row_h, s, selected ? MUZZA_COLOR_ACCENT : MUZZA_COLOR_BORDER);

        if (file->is_parent_dir) {
            ui_draw_rect(canvas, list_x + 16.0f * s, row_y + 8.0f * s, 14.0f * s, 4.0f * s, MUZZA_COLOR_ACCENT);
            ui_draw_rect(canvas, list_x + 16.0f * s, row_y + 8.0f * s, 4.0f * s, 10.0f * s, MUZZA_COLOR_ACCENT);
        } else if (file->is_dir) {
            ui_draw_rect(canvas, list_x + 16.0f * s, row_y + 7.0f * s, 18.0f * s, 14.0f * s, 0xFFE3BE5A);
            ui_draw_rect(canvas, list_x + 18.0f * s, row_y + 5.0f * s, 8.0f * s, 4.0f * s, 0xFFE3BE5A);
        } else {
            ui_draw_rect(canvas, list_x + 16.0f * s, row_y + 6.0f * s, 14.0f * s, 16.0f * s, 0xFF87949C);
            ui_draw_rect(canvas, list_x + 22.0f * s, row_y + 6.0f * s, 8.0f * s, 4.0f * s, 0xFFBCC7CE);
        }

        ui_draw_text_ellipsis(canvas, list_x + 44.0f * s, row_y + 8.0f * s, list_w - 126.0f * s, file->name, 1.0f * s, 0xFFCAD3D9);
        ui_draw_text_right(canvas, list_x + list_w - 18.0f * s, row_y + 8.0f * s, file->is_parent_dir ? "UP" : (file->is_dir ? "DIR" : "FILE"), 1.0f * s, file->is_dir ? MUZZA_COLOR_ACCENT : 0xFF66737B);

        if (state->input.left_pressed && hovered) {
            char selected_path[1024] = {0};
            bool file_selected = import_browser_activate_entry(browser, file_index, selected_path, sizeof(selected_path));
            if (file_selected) {
                browser->selected_index = file_index;
            }
            return;
        }
    }

    if (browser->num_files > visible_rows) {
        float scroll_x = panel_x + panel_w - 32.0f * s;
        float scroll_up_y = list_y + 10.0f * s;
        float scroll_down_y = list_y + list_h - 34.0f * s;

        ui_draw_rect(canvas, scroll_x, scroll_up_y, 14.0f * s, 18.0f * s, MUZZA_COLOR_TRACK_ALT);
        ui_draw_rect(canvas, scroll_x + 4.0f * s, scroll_up_y + 7.0f * s, 6.0f * s, 3.0f * s, 0xFFE7F4F3);
        ui_draw_rect(canvas, scroll_x, scroll_down_y, 14.0f * s, 18.0f * s, MUZZA_COLOR_TRACK_ALT);
        ui_draw_rect(canvas, scroll_x + 4.0f * s, scroll_down_y + 8.0f * s, 6.0f * s, 3.0f * s, 0xFFE7F4F3);

        if (state->input.left_pressed && ui_point_in_rect(state->input.x, state->input.y, scroll_x, scroll_up_y, 14.0f * s, 18.0f * s) && browser->scroll > 0) {
            browser->scroll--;
        }
        if (state->input.left_pressed && ui_point_in_rect(state->input.x, state->input.y, scroll_x, scroll_down_y, 14.0f * s, 18.0f * s) && browser->scroll < max_scroll) {
            browser->scroll++;
        }
    }

    ui_draw_rect(canvas, cancel_x, panel_y + panel_h - 46.0f * s, 84.0f * s, 28.0f * s, MUZZA_COLOR_TRACK_ALT);
    ui_draw_border(canvas, cancel_x, panel_y + panel_h - 46.0f * s, 84.0f * s, 28.0f * s, s, MUZZA_COLOR_BORDER);
    ui_draw_text_centered(canvas, cancel_x, panel_y + panel_h - 46.0f * s, 84.0f * s, 28.0f * s, "CANCEL", 1.0f * s, 0xFFCAD3D9);

    ui_draw_rect(canvas, import_x, panel_y + panel_h - 46.0f * s, 98.0f * s, 28.0f * s, can_import ? MUZZA_COLOR_ACCENT : MUZZA_COLOR_TRACK_EDGE);
    ui_draw_border(canvas, import_x, panel_y + panel_h - 46.0f * s, 98.0f * s, 28.0f * s, s, can_import ? MUZZA_COLOR_ACCENT_DIM : MUZZA_COLOR_BORDER);
    ui_draw_text_centered(canvas, import_x, panel_y + panel_h - 46.0f * s, 98.0f * s, 28.0f * s, "IMPORT", 1.0f * s, 0xFFF0F7F6);

    if (browser->num_files == 0) {
        ui_draw_text_centered(canvas, list_x, list_y + list_h * 0.5f - 10.0f * s, list_w, 20.0f * s, "EMPTY DIRECTORY", 1.0f * s, 0xFF7C8A92);
    }

    if (state->input.left_pressed) {
        if (ui_point_in_rect(state->input.x, state->input.y, close_x, panel_y + 10.0f * s, 24.0f * s, 24.0f * s)
            || ui_point_in_rect(state->input.x, state->input.y, cancel_x, panel_y + panel_h - 46.0f * s, 84.0f * s, 28.0f * s)) {
            actions->close_import_browser = true;
            return;
        }

        if (ui_point_in_rect(state->input.x, state->input.y, up_x, panel_y + 52.0f * s, 28.0f * s, 18.0f * s)) {
            muzza_path_parent_in_place(browser->current_dir, sizeof(browser->current_dir));
            browser->selected_index = -1;
            browser->scroll = 0;
            import_browser_refresh(browser);
            return;
        }

        if (can_import && ui_point_in_rect(state->input.x, state->input.y, import_x, panel_y + panel_h - 46.0f * s, 98.0f * s, 28.0f * s)) {
            const muzza_browser_entry* selected = &browser->files[browser->selected_index];
            muzza_path_join(actions->import_path, sizeof(actions->import_path), browser->current_dir, selected->name);
            actions->close_import_browser = true;
        }
    }
}
