#ifndef MUZZA_PLAYBACK_CONTROLLER_H
#define MUZZA_PLAYBACK_CONTROLLER_H

#include "flux/flux.h"
#include "project.h"
#include "ui.h"

void playback_reset_session(muzza_ui_state* ui);
void playback_sync_preview(fx_context* ctx, muzza_project* project, muzza_ui_state* ui, double delta_time);

#endif
