#ifndef MUZZA_UI_H
#define MUZZA_UI_H

#include <stdbool.h>

#include <SDL3/SDL.h>

#include "flux/flux.h"
#include "import_browser.h"
#include "project.h"

typedef struct {
    float x;
    float y;
    float wheel_x;
    float wheel_y;
    bool left_down;
    bool left_pressed;
    bool left_released;
    bool right_down;
    bool right_pressed;
    bool right_released;
    bool ctrl_down;
    bool shift_down;
    bool alt_down;
    bool zoom_in_pressed;
    bool zoom_out_pressed;
} muzza_input_state;

typedef struct {
    int selected_media_index;
    bool show_context_menu;
    float context_menu_x;
    float context_menu_y;
    int context_menu_media_index;
    bool is_dragging_media;
    int dragged_media_index;
} muzza_media_panel_state;

typedef struct {
    bool is_scrubbing;
    bool is_dragging_track_edge;
    int dragged_track_index;
    bool is_dragging_clip;
    int dragged_clip_index;
    double drag_start_offset;
    int selected_clip_index;
    int active_clip_index;
    double playhead_time;

    float zoom;      // pixels per second (unscaled by ui_scale)
    double scroll_x; // time offset in seconds
} muzza_timeline_state;

typedef struct {
    fx_image* current_frame;
    int frame_width;
    int frame_height;
    int preview_media_index;
} muzza_preview_state;

typedef struct {
    bool valid;
    int clip_index;
    int audio_clip_index;
    int media_id;
    int audio_media_id;
    bool source_mode;
} muzza_playback_session_state;

typedef struct {
    bool visible;
    bool is_exporting;
    bool cancel_requested;
    bool show_progress;
    int status; /* 0=idle, 1=running, 2=done, 3=cancelled, 4=error */
    float progress;
    char output_path[512];
    char status_msg[256];
} muzza_export_panel_state;

typedef struct {
    int window_width;
    int window_height;
    float ui_scale;
    float split_v;
    float split_h;
    bool is_dragging_splitter_v;
    bool is_dragging_splitter_h;
    bool is_playing;

    muzza_project* project;
    muzza_input_state input;
    muzza_media_panel_state media_panel;
    muzza_import_browser_state import_browser;
    muzza_timeline_state timeline;
    muzza_preview_state preview;
    muzza_playback_session_state playback;
    muzza_export_panel_state export_panel;
} muzza_ui_state;

typedef struct {
    bool toggle_playback;
    bool open_import_browser;
    bool close_import_browser;
    bool open_export_panel;
    bool start_export;
    int delete_media_id;
    int insert_media_id;
    int insert_track_index;
    double insert_timeline_time;
    char import_path[1024];
} muzza_ui_actions;

void ui_state_init(muzza_ui_state* state, muzza_project* project);
void ui_begin_frame(muzza_ui_state* state);
void ui_handle_event(muzza_ui_state* state, const SDL_Event* event, SDL_Window* window);
void ui_render(fx_canvas* canvas, muzza_ui_state* state, muzza_ui_actions* actions);

#endif
