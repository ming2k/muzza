#ifndef MUZZA_UI_H
#define MUZZA_UI_H

#include "flux/flux.h"

typedef struct {
    int window_width;
    int window_height;
    
    float mouse_x;
    float mouse_y;
    bool  mouse_down;
    
    float playhead_pos; // 0.0 to 1.0 representing timeline progress
    
    fx_image* current_frame; // The decoded video frame
    int frame_width;
    int frame_height;
} muzza_ui_state;

// Renders the full video editor UI
void ui_render(fx_canvas* canvas, muzza_ui_state* state);

#endif
