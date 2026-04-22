#ifndef MUZZA_UI_MEDIA_PANEL_H
#define MUZZA_UI_MEDIA_PANEL_H

#include "ui.h"

void ui_draw_media_panel(fx_canvas* canvas, muzza_ui_state* state, muzza_ui_actions* actions, float x, float y, float w, float h);
void ui_draw_media_drag_overlay(fx_canvas* canvas, const muzza_ui_state* state);

#endif
