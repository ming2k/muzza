#include "ui_export_panel.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ui_icons.h"
#include "ui_shared.h"
#include "ui_text.h"

/* -------------------------------------------------------------------------- */
/* Dropdown IDs                                                               */
/* -------------------------------------------------------------------------- */
#define DROPDOWN_NONE   -1
#define DROPDOWN_FORMAT  0
#define DROPDOWN_RES     1
#define DROPDOWN_FPS     2
#define DROPDOWN_QUAL    3
#define DROPDOWN_AUDIO   4
#define NUM_DROPDOWNS    5

/* -------------------------------------------------------------------------- */
/* Preset tables                                                              */
/* -------------------------------------------------------------------------- */
static const struct {
    int w, h;
    const char* label;
} RES_PRESETS[] = {
    {1280, 720, "720p"},
    {1920, 1080, "1080p"},
    {2560, 1440, "1440p"},
    {3840, 2160, "4K"},
};
#define NUM_RES_PRESETS 4

static const double FPS_PRESETS[] = {24.0, 25.0, 30.0, 60.0};
#define NUM_FPS_PRESETS 4

static const int AUDIO_BITRATES[] = {64, 96, 128, 192, 256, 320};
#define NUM_AUDIO_BITRATES 6

static const int CRF_PRESETS[] = {0, 5, 10, 15, 20, 23, 26, 30, 35, 40, 45, 51};
#define NUM_CRF_PRESETS 12

static const char* FORMAT_LABELS[] = {
    "MP4 H.264", "MP4 H.265", "WebM VP9", "MOV ProRes", "MKV", "MP3", "WAV"};

static const char* CRF_LABELS[] = {
    "CRF 0  (Lossless)", "CRF 5", "CRF 10", "CRF 15",
    "CRF 20 (High)", "CRF 23 (Default)", "CRF 26 (Good)",
    "CRF 30 (Medium)", "CRF 35 (Low)", "CRF 40",
    "CRF 45", "CRF 51 (Worst)"};

/* -------------------------------------------------------------------------- */
static void init_default_settings(muzza_export_panel_state* panel) {
    if (panel->settings_initialized) return;
    memset(&panel->settings, 0, sizeof(panel->settings));
    panel->settings.width = 1920;
    panel->settings.height = 1080;
    panel->settings.fps = 30.0;
    panel->settings.audio_sample_rate = 48000;
    panel->settings.audio_channels = 2;
    panel->settings.include_video = true;
    panel->settings.include_audio = true;
    panel->settings.format = MUZZA_FORMAT_MP4_H264;
    panel->settings.video_bitrate = 0;
    panel->settings.crf = 23;
    panel->settings.audio_bitrate = 128;
    panel->settings.preset = 4; /* medium */
    panel->settings_initialized = true;
    panel->open_dropdown = DROPDOWN_NONE;
}

/* -------------------------------------------------------------------------- */
static int find_res_preset_index(int w, int h) {
    for (int i = 0; i < NUM_RES_PRESETS; ++i) {
        if (RES_PRESETS[i].w == w && RES_PRESETS[i].h == h) return i;
    }
    return -1;
}

static int find_fps_preset_index(double fps) {
    for (int i = 0; i < NUM_FPS_PRESETS; ++i) {
        if (fabs(FPS_PRESETS[i] - fps) < 0.5) return i;
    }
    return 2; /* default 30 */
}

static int find_audio_preset_index(int bitrate) {
    for (int i = 0; i < NUM_AUDIO_BITRATES; ++i) {
        if (AUDIO_BITRATES[i] == bitrate) return i;
    }
    return 2; /* default 128 */
}

static int find_crf_preset_index(int crf) {
    int best = 0;
    int best_diff = abs(CRF_PRESETS[0] - crf);
    for (int i = 1; i < NUM_CRF_PRESETS; ++i) {
        int diff = abs(CRF_PRESETS[i] - crf);
        if (diff < best_diff) {
            best_diff = diff;
            best = i;
        }
    }
    return best;
}

/* -------------------------------------------------------------------------- */
/* Deferred list geometry — populated during box draw, consumed at the end  */
/* -------------------------------------------------------------------------- */
typedef struct {
    float x, y, w, item_h;
    const char* const* options;
    int num_options;
    int selected_index;
} dd_list_geom;

/* -------------------------------------------------------------------------- */
/* Draw the dropdown box (label + value field).                               */
/* If this dropdown is open, fills out_geom with list coordinates.            */
/* -------------------------------------------------------------------------- */
static void draw_dropdown_box(fx_canvas* canvas, muzza_ui_state* state,
    float x, float y, float w, float s, const char* label,
    const char* value_text, int dropdown_id, int* open_dropdown,
    dd_list_geom* out_geom)
{
    float label_w = 50.0f * s;
    float box_x = x + label_w;
    float box_w = w - label_w;
    float box_h = 22.0f * s;

    ui_draw_text(canvas, x, y + 5.0f * s, label, 1.0f * s, 0xFF7C8A92);

    fx_color box_color = (*open_dropdown == dropdown_id)
        ? MUZZA_COLOR_DIALOG_ROW_HI : MUZZA_COLOR_TRACK_BG;
    ui_draw_rect(canvas, box_x, y, box_w, box_h, box_color);
    ui_draw_border(canvas, box_x, y, box_w, box_h, s, MUZZA_COLOR_BORDER);

    ui_draw_text(canvas, box_x + 8.0f * s, y + 5.0f * s, value_text, 1.0f * s, 0xFFCAD3D9);

    float arrow_sz = 8.0f * s;
    float arrow_x = box_x + box_w - arrow_sz - 6.0f * s;
    float arrow_y = y + (box_h - arrow_sz) * 0.5f;
    draw_icon_arrow_down(canvas, arrow_x, arrow_y, arrow_sz, 0xFF87949C);

    if (state->input.left_pressed &&
        ui_point_in_rect(state->input.x, state->input.y, box_x, y, box_w, box_h)) {
        *open_dropdown = (*open_dropdown == dropdown_id) ? DROPDOWN_NONE : dropdown_id;
    }

    if (out_geom && *open_dropdown == dropdown_id) {
        out_geom->x = box_x;
        out_geom->y = y + box_h;
        out_geom->w = box_w;
        out_geom->item_h = box_h;
    }
}

/* -------------------------------------------------------------------------- */
/* Draw the dropdown list and handle item clicks.                             */
/* Returns selected item index, or -1 if no selection was made.               */
/* -------------------------------------------------------------------------- */
static int draw_dropdown_list(fx_canvas* canvas, muzza_ui_state* state,
    const dd_list_geom* g, float s, int* open_dropdown)
{
    float list_h = g->item_h * g->num_options;
    int selected = -1;

    ui_draw_rect(canvas, g->x, g->y, g->w, list_h, MUZZA_COLOR_TRACK_ALT);
    ui_draw_border(canvas, g->x, g->y, g->w, list_h, s, MUZZA_COLOR_BORDER);

    for (int i = 0; i < g->num_options; ++i) {
        float item_y = g->y + i * g->item_h;
        bool hovered = ui_point_in_rect(state->input.x, state->input.y,
            g->x, item_y, g->w, g->item_h);
        bool is_sel = (i == g->selected_index);

        if (hovered || is_sel) {
            ui_draw_rect(canvas, g->x, item_y, g->w, g->item_h,
                hovered ? MUZZA_COLOR_DIALOG_ROW_HI : MUZZA_COLOR_DIALOG_ROW);
        }

        ui_draw_text(canvas, g->x + 8.0f * s, item_y + 5.0f * s,
            g->options[i], 1.0f * s,
            is_sel ? MUZZA_COLOR_ACCENT : 0xFFCAD3D9);

        if (state->input.left_pressed && hovered) {
            selected = i;
            *open_dropdown = DROPDOWN_NONE;
        }
    }

    /* Close when clicking outside the entire dropdown area (box + list) */
    if (state->input.left_pressed && selected < 0) {
        float total_h = g->item_h + list_h;
        if (!ui_point_in_rect(state->input.x, state->input.y,
            g->x, g->y - g->item_h, g->w, total_h)) {
            *open_dropdown = DROPDOWN_NONE;
        }
    }

    return selected;
}

/* -------------------------------------------------------------------------- */
void ui_draw_export_panel(fx_canvas* canvas, muzza_ui_state* state, muzza_ui_actions* actions,
    int win_w, int win_h) {
    muzza_export_panel_state* panel = &state->export_panel;
    float s = state->ui_scale > 1.0f ? state->ui_scale : 1.0f;
    float panel_w = (float)win_w;
    float panel_h = (float)win_h;
    float margin = 18.0f * s;
    float content_w = panel_w - margin * 2.0f;
    float half_w = (content_w - 12.0f * s) * 0.5f;
    float cancel_x = margin;
    float export_x = panel_w - margin - 98.0f * s;
    bool has_content = state->project && state->project->duration > 0.0;
    bool is_exporting = panel->is_exporting;
    float progress = panel->progress;

    dd_list_geom dd_geom[NUM_DROPDOWNS];
    memset(dd_geom, 0, sizeof(dd_geom));

    if (!panel->visible) return;
    init_default_settings(panel);

    bool has_video = exporter_format_has_video(panel->settings.format);

    /* Background */
    ui_draw_rect(canvas, 0.0f, 0.0f, panel_w, panel_h, MUZZA_COLOR_BG_PANEL);
    ui_draw_rect(canvas, 0.0f, 0.0f, panel_w, 42.0f * s, MUZZA_COLOR_BG_HEADER);
    ui_draw_border(canvas, 0.0f, 0.0f, panel_w, panel_h, s, MUZZA_COLOR_BORDER);

    ui_draw_text(canvas, margin, 14.0f * s, "EXPORT PROJECT", 2.0f * s, 0xFFD7DEE3);

    /* Row 1: Format */
    float y = 56.0f * s;
    int fmt_idx = (int)panel->settings.format;
    if (fmt_idx < 0) fmt_idx = 0;
    if (fmt_idx >= MUZZA_FORMAT_COUNT) fmt_idx = MUZZA_FORMAT_COUNT - 1;
    dd_geom[DROPDOWN_FORMAT].options = FORMAT_LABELS;
    dd_geom[DROPDOWN_FORMAT].num_options = MUZZA_FORMAT_COUNT;
    dd_geom[DROPDOWN_FORMAT].selected_index = fmt_idx;
    draw_dropdown_box(canvas, state, margin, y, content_w, s, "FORMAT",
        FORMAT_LABELS[fmt_idx], DROPDOWN_FORMAT, &panel->open_dropdown,
        &dd_geom[DROPDOWN_FORMAT]);

    /* Row 2: Resolution + FPS (video formats only) */
    y += 36.0f * s;
    if (has_video) {
        int res_idx = find_res_preset_index(panel->settings.width, panel->settings.height);
        int res_sel = res_idx >= 0 ? res_idx : 0;
        const char* res_labels[NUM_RES_PRESETS];
        for (int i = 0; i < NUM_RES_PRESETS; ++i) res_labels[i] = RES_PRESETS[i].label;
        char res_value[32];
        snprintf(res_value, sizeof(res_value), "%s", res_labels[res_sel]);
        dd_geom[DROPDOWN_RES].options = res_labels;
        dd_geom[DROPDOWN_RES].num_options = NUM_RES_PRESETS;
        dd_geom[DROPDOWN_RES].selected_index = res_sel;
        draw_dropdown_box(canvas, state, margin, y, half_w, s, "RES",
            res_value, DROPDOWN_RES, &panel->open_dropdown,
            &dd_geom[DROPDOWN_RES]);

        int fps_idx = find_fps_preset_index(panel->settings.fps);
        char fps_bufs[NUM_FPS_PRESETS][16];
        const char* fps_labels[NUM_FPS_PRESETS];
        for (int i = 0; i < NUM_FPS_PRESETS; ++i) {
            snprintf(fps_bufs[i], sizeof(fps_bufs[i]), "%.0f fps", FPS_PRESETS[i]);
            fps_labels[i] = fps_bufs[i];
        }
        dd_geom[DROPDOWN_FPS].options = fps_labels;
        dd_geom[DROPDOWN_FPS].num_options = NUM_FPS_PRESETS;
        dd_geom[DROPDOWN_FPS].selected_index = fps_idx;
        draw_dropdown_box(canvas, state, margin + half_w + 12.0f * s, y, half_w, s, "FPS",
            fps_labels[fps_idx], DROPDOWN_FPS, &panel->open_dropdown,
            &dd_geom[DROPDOWN_FPS]);
    }

    /* Row 3: Quality (CRF) + Audio bitrate */
    y += 36.0f * s;
    if (has_video) {
        int qual_idx = find_crf_preset_index(panel->settings.crf);
        dd_geom[DROPDOWN_QUAL].options = CRF_LABELS;
        dd_geom[DROPDOWN_QUAL].num_options = NUM_CRF_PRESETS;
        dd_geom[DROPDOWN_QUAL].selected_index = qual_idx;
        draw_dropdown_box(canvas, state, margin, y, half_w, s, "QUAL",
            CRF_LABELS[qual_idx], DROPDOWN_QUAL, &panel->open_dropdown,
            &dd_geom[DROPDOWN_QUAL]);
    }

    int aud_idx = find_audio_preset_index(panel->settings.audio_bitrate);
    char aud_bufs[NUM_AUDIO_BITRATES][16];
    const char* aud_labels[NUM_AUDIO_BITRATES];
    for (int i = 0; i < NUM_AUDIO_BITRATES; ++i) {
        snprintf(aud_bufs[i], sizeof(aud_bufs[i]), "%d kbps", AUDIO_BITRATES[i]);
        aud_labels[i] = aud_bufs[i];
    }
    float audio_x = has_video ? margin + half_w + 12.0f * s : margin;
    float audio_w = has_video ? half_w : content_w;
    dd_geom[DROPDOWN_AUDIO].options = aud_labels;
    dd_geom[DROPDOWN_AUDIO].num_options = NUM_AUDIO_BITRATES;
    dd_geom[DROPDOWN_AUDIO].selected_index = aud_idx;
    draw_dropdown_box(canvas, state, audio_x, y, audio_w, s, "AUDIO",
        aud_labels[aud_idx], DROPDOWN_AUDIO, &panel->open_dropdown,
        &dd_geom[DROPDOWN_AUDIO]);

    /* Output path */
    y += 38.0f * s;
    ui_draw_rect(canvas, margin, y, content_w, 18.0f * s, MUZZA_COLOR_TRACK_BG);
    ui_draw_border(canvas, margin, y, content_w, 18.0f * s, s, MUZZA_COLOR_BORDER);
    ui_draw_text_ellipsis(canvas, margin + 10.0f * s, y + 4.0f * s, content_w - 20.0f * s,
        panel->output_path[0] ? panel->output_path : "output.mp4", 1.0f * s, 0xFFCAD3D9);

    /* Info row */
    y += 28.0f * s;
    char info_buf[128] = {0};
    if (state->project) {
        int mins = (int)(state->project->duration / 60.0);
        int secs = (int)(state->project->duration) % 60;
        snprintf(info_buf, sizeof(info_buf), "Duration: %02d:%02d  |  %s", mins, secs,
            exporter_format_name(panel->settings.format));
    } else {
        snprintf(info_buf, sizeof(info_buf), "No project loaded");
    }
    ui_draw_text(canvas, margin, y, info_buf, 1.0f * s, 0xFF7C8A92);

    /* Progress bar */
    y += 24.0f * s;
    if (is_exporting || progress >= 1.0f || panel->show_progress) {
        float bar_h = 14.0f * s;
        float fill_w = content_w * progress;
        ui_draw_rect(canvas, margin, y, content_w, bar_h, MUZZA_COLOR_TRACK_BG);
        ui_draw_border(canvas, margin, y, content_w, bar_h, s, MUZZA_COLOR_BORDER);
        if (fill_w > 0.0f) {
            ui_draw_rect(canvas, margin, y, fill_w, bar_h, MUZZA_COLOR_ACCENT);
        }

        char status_buf[320] = {0};
        if (panel->status == 2) {
            snprintf(status_buf, sizeof(status_buf), "Export complete!");
        } else if (panel->status == 4) {
            snprintf(status_buf, sizeof(status_buf), "Error: %s", panel->status_msg);
        } else if (panel->status == 3) {
            snprintf(status_buf, sizeof(status_buf), "Export cancelled.");
        } else {
            snprintf(status_buf, sizeof(status_buf), "Exporting... %.0f%%", progress * 100.0f);
        }
        ui_draw_text(canvas, margin, y + 22.0f * s, status_buf, 1.0f * s,
            panel->status == 4 ? MUZZA_COLOR_ALERT : 0xFFCAD3D9);
    }

    /* No content warning */
    if (!has_content && !is_exporting) {
        ui_draw_text_centered(canvas, margin, y + 10.0f * s, content_w, 20.0f * s,
            "TIMELINE IS EMPTY", 1.0f * s, 0xFF7C8A92);
    }

    /* Bottom buttons */
    float btn_y = panel_h - 46.0f * s;
    ui_draw_rect(canvas, cancel_x, btn_y, 84.0f * s, 28.0f * s, MUZZA_COLOR_TRACK_ALT);
    ui_draw_border(canvas, cancel_x, btn_y, 84.0f * s, 28.0f * s, s, MUZZA_COLOR_BORDER);
    ui_draw_text_centered(canvas, cancel_x, btn_y, 84.0f * s, 28.0f * s,
        is_exporting ? "CANCEL" : "CLOSE", 1.0f * s, 0xFFCAD3D9);

    bool can_export = has_content && !is_exporting && panel->status != 2;
    bool is_done = panel->status == 2;
    const char* export_label = is_done ? "CLOSE" : "EXPORT";
    fx_color export_btn_bg = is_done ? MUZZA_COLOR_ACCENT
                             : (can_export ? MUZZA_COLOR_ACCENT : MUZZA_COLOR_TRACK_EDGE);
    fx_color export_btn_border = is_done ? MUZZA_COLOR_ACCENT_DIM
                                 : (can_export ? MUZZA_COLOR_ACCENT_DIM : MUZZA_COLOR_BORDER);
    ui_draw_rect(canvas, export_x, btn_y, 98.0f * s, 28.0f * s, export_btn_bg);
    ui_draw_border(canvas, export_x, btn_y, 98.0f * s, 28.0f * s, s, export_btn_border);
    ui_draw_text_centered(canvas, export_x, btn_y, 98.0f * s, 28.0f * s,
        export_label, 1.0f * s, 0xFFF0F7F6);

    /* ---------------------------------------------------------------------- */
    /* Interaction: suppress button clicks when a dropdown list is open       */
    /* ---------------------------------------------------------------------- */
    bool click_in_list = false;
    int opened_dd = panel->open_dropdown;
    if (opened_dd >= 0 && opened_dd < NUM_DROPDOWNS && dd_geom[opened_dd].options) {
        float list_h = dd_geom[opened_dd].item_h * dd_geom[opened_dd].num_options;
        if (state->input.left_pressed && ui_point_in_rect(state->input.x, state->input.y,
            dd_geom[opened_dd].x, dd_geom[opened_dd].y,
            dd_geom[opened_dd].w, list_h)) {
            click_in_list = true;
        }
    }

    if (!click_in_list && state->input.left_pressed) {
        if (ui_point_in_rect(state->input.x, state->input.y, cancel_x, btn_y, 84.0f * s,
                28.0f * s)) {
            if (is_exporting) {
                panel->cancel_requested = true;
            } else {
                panel->visible = false;
            }
            return;
        }
        if (is_done && ui_point_in_rect(state->input.x, state->input.y, export_x, btn_y,
                98.0f * s, 28.0f * s)) {
            panel->visible = false;
            return;
        }
        if (can_export && ui_point_in_rect(state->input.x, state->input.y, export_x, btn_y,
                98.0f * s, 28.0f * s)) {
            actions->start_export = true;
            return;
        }
    }

    /* ---------------------------------------------------------------------- */
    /* Deferred list draw — always on top of everything else                  */
    /* ---------------------------------------------------------------------- */
    int new_sel = -1;
    if (opened_dd >= 0 && opened_dd < NUM_DROPDOWNS && dd_geom[opened_dd].options) {
        new_sel = draw_dropdown_list(canvas, state, &dd_geom[opened_dd], s,
            &panel->open_dropdown);
    }

    if (new_sel >= 0) {
        switch (opened_dd) {
            case DROPDOWN_FORMAT:
                panel->settings.format = (muzza_export_format)new_sel;
                break;
            case DROPDOWN_RES:
                panel->settings.width = RES_PRESETS[new_sel].w;
                panel->settings.height = RES_PRESETS[new_sel].h;
                break;
            case DROPDOWN_FPS:
                panel->settings.fps = FPS_PRESETS[new_sel];
                break;
            case DROPDOWN_QUAL:
                panel->settings.crf = CRF_PRESETS[new_sel];
                break;
            case DROPDOWN_AUDIO:
                panel->settings.audio_bitrate = AUDIO_BITRATES[new_sel];
                break;
        }
    }
}
