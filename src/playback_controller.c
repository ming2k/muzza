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

static int find_top_video_clip(const muzza_project* proj, double time_seconds) {
    int found = -1;
    int highest_track = -1;
    if (!proj) return -1;
    for (size_t i = 0; i < proj->num_clips; ++i) {
        const muzza_clip* clip = &proj->clips[i];
        if (clip->type != MUZZA_CLIP_VIDEO) continue;
        double clip_end = clip->tl_start + clip->tl_duration;
        if (time_seconds >= clip->tl_start && time_seconds < clip_end) {
            if (clip->track_index >= highest_track) {
                highest_track = clip->track_index;
                found = (int)i;
            }
        }
    }
    return found;
}

static int find_active_audio_clips(const muzza_project* proj, double time_seconds, int* out_indices, int max_indices) {
    int count = 0;
    if (!proj) return 0;
    for (size_t i = 0; i < proj->num_clips && count < max_indices; ++i) {
        const muzza_clip* clip = &proj->clips[i];
        if (clip->type != MUZZA_CLIP_AUDIO) continue;
        double clip_end = clip->tl_start + clip->tl_duration;
        if (time_seconds >= clip->tl_start && time_seconds < clip_end) {
            out_indices[count++] = (int)i;
        }
    }
    return count;
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
    // 1. Advance Playhead by delta_time (The Master Clock)
    if (ui->is_playing && !ui->timeline.is_scrubbing && project->duration > 0.0) {
        ui->timeline.playhead_pos += (float)(delta_time / project->duration);
        if (ui->timeline.playhead_pos >= 1.0f) {
            ui->timeline.playhead_pos = 1.0f;
            ui->is_playing = false;
        }
    }

    double timeline_time = ui->timeline.playhead_pos * project->duration;
    int v_clip_idx = find_top_video_clip(project, timeline_time);
    int a_clips[16];
    int num_a_clips = find_active_audio_clips(project, timeline_time, a_clips, 16);

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
            
            double drift = fabs(decoder_get_time(a_dec) - target_media_time);
            if (ui->timeline.is_scrubbing || !ui->playback.valid || drift > 1.0) {
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