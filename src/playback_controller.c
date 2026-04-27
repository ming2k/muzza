#include "playback_controller.h"

#include <SDL3/SDL.h>
#include <math.h>

static void pause_inactive_decoders(muzza_project* project, int active_source_media, int active_v_clip, const int* active_a_clips, int num_a_clips) {
    if (!project) return;

    for (size_t i = 0; i < project->num_media; ++i) {
        muzza_media* media = &project->media_pool[i];
        if (media->dec && (int)i != active_source_media) {
            decoder_set_paused(media->dec, true);
        }
    }

    for (size_t i = 0; i < project->num_clips; ++i) {
        muzza_clip* clip = &project->clips[i];
        if (!clip->dec) continue;
        bool is_active = ((int)i == active_v_clip);
        if (!is_active) {
            for (int a = 0; a < num_a_clips; ++a) {
                if ((int)i == active_a_clips[a]) {
                    is_active = true;
                    break;
                }
            }
        }
        if (!is_active) {
            decoder_set_paused(clip->dec, true);
        }
    }
}

void playback_reset_session(muzza_ui_state* ui) {
    if (!ui) return;
    ui->playback.valid = false;
    ui->playback.clip_index = -1;
    ui->playback.audio_clip_index = -1;
    ui->playback.media_id = -1;
    ui->playback.audio_media_id = -1;
    ui->playback.source_mode = false;
}

static void update_timeline_preview(fx_context* ctx, muzza_project* project, muzza_ui_state* ui, double delta_time) {
    // Determine solo/mute mask for the project
    bool any_solo = false;
    for (int t = 0; t < MUZZA_MAX_TRACKS; ++t) {
        if (project->tracks[t].solo) { any_solo = true; break; }
    }

    // 1. Advance Playhead by delta_time (The Master Clock)
    if (ui->is_playing && !ui->timeline.is_scrubbing) {
        ui->timeline.playhead_time += delta_time * (double)ui->playback_speed;
        if (ui->timeline.playhead_time < 0.0) {
            ui->timeline.playhead_time = 0.0;
            ui->is_playing = false;
        }
        if (project->duration > 0.0 && ui->timeline.playhead_time >= project->duration) {
            ui->timeline.playhead_time = project->duration;
            ui->is_playing = false;
        }
    }

    double timeline_time = ui->timeline.playhead_time;
    int v_clip_idx = project_find_top_video_clip(project, timeline_time);
    int a_clips[16];
    int num_a_clips = project_find_active_audio_clips(project, timeline_time, a_clips, 16);

    // Filter by mute/solo
    if (v_clip_idx >= 0) {
        muzza_clip* v_clip = &project->clips[v_clip_idx];
        int track_idx = v_clip->track_index;
        bool solo_mask = !any_solo || (track_idx >= 0 && track_idx < MUZZA_MAX_TRACKS && project->tracks[track_idx].solo);
        bool muted = (track_idx >= 0 && track_idx < MUZZA_MAX_TRACKS && project->tracks[track_idx].muted);
        if (muted || !solo_mask) {
            v_clip_idx = -1;
        }
    }
    {
        int filtered = 0;
        for (int i = 0; i < num_a_clips; ++i) {
            int a_idx = a_clips[i];
            int track_idx = project->clips[a_idx].track_index;
            bool solo_mask = !any_solo || (track_idx >= 0 && track_idx < MUZZA_MAX_TRACKS && project->tracks[track_idx].solo);
            bool muted = (track_idx >= 0 && track_idx < MUZZA_MAX_TRACKS && project->tracks[track_idx].muted);
            if (!muted && solo_mask) {
                if (filtered != i) a_clips[filtered] = a_clips[i];
                filtered++;
            }
        }
        num_a_clips = filtered;
    }

    pause_inactive_decoders(project, -1, v_clip_idx, a_clips, num_a_clips);

    if (v_clip_idx < 0 && num_a_clips == 0) {
        ui->preview.current_frame = NULL;
        return;
    }

    // --- VIDEO SYNC ---
    if (v_clip_idx >= 0) {
        muzza_clip* v_clip = &project->clips[v_clip_idx];
        muzza_decoder* v_dec = project_ensure_clip_decoder(project, v_clip_idx, ctx);
        if (v_dec) {
            double target_media_time = project_get_clip_media_time(project, v_clip, timeline_time);
            
            // Hard Sync (Seek) if drift > 500ms or scrubbing
            double drift = fabs(decoder_get_time(v_dec) - target_media_time);
            if (ui->timeline.is_scrubbing || !ui->playback.valid || ui->playback.clip_index != v_clip_idx || drift > 0.5) {
                decoder_seek_to_time(v_dec, target_media_time);
            }

            if (ui->is_playing && !ui->timeline.is_scrubbing) {
                decoder_set_paused(v_dec, false);
                // Catch up to current timeline time
                decoder_update_to_time(v_dec, target_media_time, false);
            } else {
                decoder_set_paused(v_dec, true);
            }

            ui->preview.current_frame = decoder_get_image(v_dec, &ui->preview.frame_width, &ui->preview.frame_height);

            /* Compute fade opacity for the active video clip */
            ui->playback.fade_opacity = (float)project_get_clip_fade_opacity(v_clip, timeline_time);
        }
    } else {
        ui->preview.current_frame = NULL;
    }

    // --- AUDIO SYNC ---
    for (int i = 0; i < num_a_clips; ++i) {
        int a_idx = a_clips[i];
        muzza_clip* a_clip = &project->clips[a_idx];
        muzza_decoder* a_dec = project_ensure_clip_decoder(project, a_idx, ctx);
        if (a_dec) {
            double target_media_time = project_get_clip_media_time(project, a_clip, timeline_time);

            /* Drift = how far the audio currently being heard is from where the
               playhead points. decoder_get_time() reports the lookahead PTS
               (≈250ms past the playing position during steady-state); using
               that here would force a re-seek every frame. */
            double play_time = decoder_get_audio_play_time(a_dec);
            double drift = fabs(play_time - target_media_time);
            if (ui->timeline.is_scrubbing || !ui->playback.valid || drift > 0.15) {
                decoder_seek_to_time(a_dec, target_media_time);
            }

            if (ui->is_playing && !ui->timeline.is_scrubbing) {
                decoder_set_paused(a_dec, false);
                decoder_update_to_time(a_dec, target_media_time, true);
            } else {
                decoder_set_paused(a_dec, true);
            }
        }
    }

    ui->playback.valid = true;
    ui->playback.clip_index = v_clip_idx;
    ui->playback.audio_clip_index = num_a_clips > 0 ? a_clips[0] : -1;
}

static void update_source_preview(fx_context* ctx, muzza_project* project, muzza_ui_state* ui, double delta_time) {
    int selected_media_index = ui->media_panel.selected_media_index;

    if (selected_media_index < 0 || (size_t)selected_media_index >= project->num_media) {
        pause_inactive_decoders(project, -1, -1, NULL, 0);
        playback_reset_session(ui);
        return;
    }

    muzza_media* media = project_get_media(project, selected_media_index);
    muzza_decoder* dec = project_ensure_media_decoder(project, selected_media_index, ctx);

    pause_inactive_decoders(project, selected_media_index, -1, NULL, 0);
    if (!media || !dec) {
        playback_reset_session(ui);
        return;
    }

    if (!ui->playback.valid || !ui->playback.source_mode || ui->playback.media_id != selected_media_index) {
        decoder_seek_to_time(dec, 0.0);
        ui->playback.valid = true;
        ui->playback.source_mode = true;
        ui->playback.clip_index = -1;
        ui->playback.audio_clip_index = -1;
        ui->playback.media_id = selected_media_index;
    }

    if (ui->is_playing) {
        decoder_set_paused(dec, false);
        // In source mode, we just push it forward by delta_time
        decoder_update_to_time(dec, decoder_get_time(dec) + delta_time, true);
    } else {
        decoder_set_paused(dec, true);
    }

    ui->preview.preview_media_index = selected_media_index;
    ui->preview.current_frame = decoder_get_image(dec, &ui->preview.frame_width, &ui->preview.frame_height);
}

void playback_sync_preview(fx_context* ctx, muzza_project* project, muzza_ui_state* ui, double delta_time) {
    if (!ui) return;
    ui->preview.current_frame = NULL;

    if (!project || project->duration <= 0.0) {
        pause_inactive_decoders(project, -1, -1, NULL, 0);
        playback_reset_session(ui);
        return;
    }

    update_timeline_preview(ctx, project, ui, delta_time);
    
    if (!ui->preview.current_frame && ui->playback.clip_index < 0 && ui->playback.audio_clip_index < 0) {
        update_source_preview(ctx, project, ui, delta_time);
    }
}