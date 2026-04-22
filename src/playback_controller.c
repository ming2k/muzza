#include "playback_controller.h"

#include <SDL3/SDL.h>
#include <math.h>

static void pause_all_decoders(muzza_project* project, int except_media_id) {
    if (!project) {
        return;
    }

    for (size_t i = 0; i < project->num_media; ++i) {
        muzza_media* media = &project->media_pool[i];
        if (media->dec && media->id != except_media_id) {
            decoder_set_paused(media->dec, true);
        }
    }
}

static bool load_preview_frame(muzza_decoder* dec, muzza_ui_state* ui, double delta_time, bool keep_playing) {
    if (!dec || !ui) {
        return false;
    }

    ui->preview.current_frame = decoder_get_image(dec, &ui->preview.frame_width, &ui->preview.frame_height);
    if (ui->preview.current_frame) {
        return true;
    }

    decoder_set_paused(dec, false);
    if (!decoder_update(dec, delta_time > 0.0 ? delta_time : 0.0)) {
        if (!keep_playing) {
            decoder_set_paused(dec, true);
        }
        ui->preview.current_frame = decoder_get_image(dec, &ui->preview.frame_width, &ui->preview.frame_height);
        return ui->preview.current_frame != NULL;
    }

    if (!keep_playing) {
        decoder_set_paused(dec, true);
    }

    ui->preview.current_frame = decoder_get_image(dec, &ui->preview.frame_width, &ui->preview.frame_height);
    return ui->preview.current_frame != NULL;
}

void playback_reset_session(muzza_ui_state* ui) {
    if (!ui) {
        return;
    }

    ui->playback.valid = false;
    ui->playback.clip_index = -1;
    ui->playback.media_id = -1;
    ui->playback.source_mode = false;
}

static void update_source_preview(fx_context* ctx, muzza_project* project, muzza_ui_state* ui, double delta_time) {
    int selected_media_index = ui->media_panel.selected_media_index;

    if (selected_media_index < 0 || (size_t)selected_media_index >= project->num_media) {
        pause_all_decoders(project, -1);
        playback_reset_session(ui);
        return;
    }

    muzza_media* media = project_get_media(project, selected_media_index);
    muzza_decoder* dec = project_ensure_media_decoder(project, selected_media_index, ctx);

    pause_all_decoders(project, selected_media_index);
    if (!media || !dec) {
        playback_reset_session(ui);
        return;
    }

    if (!ui->playback.valid || !ui->playback.source_mode || ui->playback.media_id != selected_media_index) {
        decoder_set_paused(dec, true);
        decoder_seek_to_time(dec, 0.0);
        ui->playback.valid = true;
        ui->playback.source_mode = true;
        ui->playback.clip_index = -1;
        ui->playback.media_id = selected_media_index;
        SDL_Log(
            "Source preview session: media=%d duration=%.3f has_video=%d has_audio=%d",
            selected_media_index,
            media->duration,
            media->has_video ? 1 : 0,
            media->has_audio ? 1 : 0
        );
    }

    if (ui->is_playing) {
        decoder_set_paused(dec, false);
        decoder_update(dec, delta_time);
    }

    if (!ui->is_playing) {
        decoder_set_paused(dec, true);
    }

    ui->preview.preview_media_index = selected_media_index;
    load_preview_frame(dec, ui, delta_time, ui->is_playing);
}

static bool start_clip_session(
    fx_context* ctx,
    muzza_project* project,
    muzza_ui_state* ui,
    int clip_index,
    double timeline_time
) {
    const muzza_clip* clip = &project->clips[clip_index];
    muzza_media* media = project_get_media(project, clip->media_id);
    muzza_decoder* dec = project_ensure_media_decoder(project, clip->media_id, ctx);
    double target_time = project_get_clip_media_time(project, clip, timeline_time);

    if (!media || !dec || !decoder_has_video(dec)) {
        return false;
    }

    pause_all_decoders(project, media->id);
    decoder_set_paused(dec, true);
    if (!decoder_seek_to_time(dec, target_time)) {
        SDL_Log("Clip session start seek failed: clip=%d media=%d target=%.3f", clip_index, clip->media_id, target_time);
        return false;
    }

    if (ui->is_playing) {
        decoder_set_paused(dec, false);
    }

    ui->playback.valid = true;
    ui->playback.source_mode = false;
    ui->playback.clip_index = clip_index;
    ui->playback.media_id = clip->media_id;
    ui->preview.preview_media_index = -1;

    SDL_Log(
        "Timeline session start: clip=%d media=%d tl_start=%.3f media_in=%.3f clip_dur=%.3f target=%.3f",
        clip_index,
        clip->media_id,
        clip->tl_start,
        clip->media_in,
        clip->tl_duration,
        target_time
    );
    return true;
}

static void update_timeline_preview(fx_context* ctx, muzza_project* project, muzza_ui_state* ui, double delta_time) {
    double timeline_time = ui->timeline.playhead_pos * project->duration;
    int active_index = project_find_clip_at_time(project, timeline_time);

    ui->timeline.active_clip_index = active_index;

    if (active_index < 0 || (size_t)active_index >= project->num_clips) {
        playback_reset_session(ui);
        pause_all_decoders(project, -1);

        if (ui->is_playing && !ui->timeline.is_scrubbing && project->duration > 0.0) {
            ui->timeline.playhead_pos += (float)(delta_time / project->duration);
            if (ui->timeline.playhead_pos >= 1.0f) {
                ui->timeline.playhead_pos = 1.0f;
                ui->is_playing = false;
            }
        }
        return;
    }

    const muzza_clip* clip = &project->clips[active_index];
    muzza_media* media = project_get_media(project, clip->media_id);
    muzza_decoder* dec = project_ensure_media_decoder(project, clip->media_id, ctx);
    if (!media || !dec || !decoder_has_video(dec)) {
        playback_reset_session(ui);
        pause_all_decoders(project, -1);
        return;
    }

    if (ui->timeline.is_scrubbing || !ui->is_playing) {
        double target_time = project_get_clip_media_time(project, clip, timeline_time);
        double drift = fabs(decoder_get_time(dec) - target_time);

        pause_all_decoders(project, media->id);
        decoder_set_paused(dec, true);
        if (!ui->playback.valid || ui->playback.source_mode || ui->playback.clip_index != active_index || ui->playback.media_id != clip->media_id || drift > 0.03) {
            decoder_seek_to_time(dec, target_time);
        }
        ui->playback.valid = true;
        ui->playback.source_mode = false;
        ui->playback.clip_index = active_index;
        ui->playback.media_id = clip->media_id;
        ui->preview.preview_media_index = -1;
        load_preview_frame(dec, ui, 0.0, false);
        return;
    }

    if (!ui->playback.valid || ui->playback.source_mode || ui->playback.clip_index != active_index || ui->playback.media_id != clip->media_id) {
        if (!start_clip_session(ctx, project, ui, active_index, timeline_time)) {
            ui->is_playing = false;
            return;
        }
    }

    decoder_set_paused(dec, false);
    if (!decoder_update(dec, delta_time)) {
        double clip_end_time = clip->tl_start + clip->tl_duration;
        ui->timeline.playhead_pos = (float)(clip_end_time / project->duration);
        playback_reset_session(ui);
        pause_all_decoders(project, -1);
        return;
    }

    {
        double media_time = decoder_get_time(dec);
        double clip_local_time = media_time - clip->media_in;
        double clip_end_time = clip->tl_start + clip->tl_duration;
        double new_timeline_time = clip->tl_start + (clip_local_time > 0.0 ? clip_local_time : 0.0);

        if (new_timeline_time >= clip_end_time) {
            new_timeline_time = clip_end_time;
            playback_reset_session(ui);
        }

        ui->timeline.playhead_pos = project->duration > 0.0 ? (float)(new_timeline_time / project->duration) : 0.0f;
        if (ui->timeline.playhead_pos > 1.0f) {
            ui->timeline.playhead_pos = 1.0f;
        }
    }

    ui->preview.preview_media_index = -1;
    load_preview_frame(dec, ui, delta_time, true);
}

void playback_sync_preview(fx_context* ctx, muzza_project* project, muzza_ui_state* ui, double delta_time) {
    if (!ui) {
        return;
    }

    ui->preview.current_frame = NULL;
    ui->preview.frame_width = 0;
    ui->preview.frame_height = 0;
    ui->timeline.active_clip_index = -1;

    if (!project || project->duration <= 0.0) {
        pause_all_decoders(project, -1);
        playback_reset_session(ui);
        return;
    }

    update_timeline_preview(ctx, project, ui, delta_time);
    if (!ui->preview.current_frame && ui->timeline.active_clip_index < 0) {
        update_source_preview(ctx, project, ui, delta_time);
    }
}
