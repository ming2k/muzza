#include "project.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

float time_to_x(double time, float tracks_x, double scroll_x, float zoom, float s) {
    return tracks_x + (float)(time - scroll_x) * zoom * s;
}

double x_to_time(float x_coord, float tracks_x, double scroll_x, float zoom, float s) {
    return (double)((x_coord - tracks_x) / (zoom * s)) + scroll_x;
}

void apply_cursor_anchored_zoom(double* scroll_x, float* zoom, float tracks_x, float cursor_x, float zoom_factor, float s) {
    if (!scroll_x || !zoom) return;
    double mouse_time_before = x_to_time(cursor_x, tracks_x, *scroll_x, *zoom, s);
    *zoom *= zoom_factor;
    if (*zoom < 1.0f) *zoom = 1.0f;
    if (*zoom > 10000.0f) *zoom = 10000.0f;
    double mouse_time_after = x_to_time(cursor_x, tracks_x, *scroll_x, *zoom, s);
    *scroll_x += (mouse_time_before - mouse_time_after);
    if (*scroll_x < 0.0) *scroll_x = 0.0;
}

typedef enum {
    SECTION_NONE = 0,
    SECTION_PROJECT,
    SECTION_MEDIA,
    SECTION_CLIP,
} project_section;

typedef struct {
    bool active;
    uint64_t uid;
    char filepath[512];
    double duration;
    bool has_video;
    bool has_audio;
    bool is_image;
    bool missing;
} media_parse_state;

typedef struct {
    bool active;
    uint64_t uid;
    int media_id;
    int track_index;
    muzza_clip_type type;
    double tl_start;
    double tl_duration;
    double media_in;
    float opacity;
    float scale;
    float pos_x;
    float pos_y;
    muzza_filter_type filter;
    double fade_in_duration;
    double fade_out_duration;
    bool selected;
    uint64_t link_group_id;
    muzza_link_role link_role;
} clip_parse_state;

static void copy_string(char* dst, size_t dst_size, const char* src) {
    if (!dst || dst_size == 0) {
        return;
    }

    if (!src) {
        dst[0] = '\0';
        return;
    }

    snprintf(dst, dst_size, "%s", src);
}

static char* trim_in_place(char* text) {
    char* end = NULL;

    if (!text) {
        return NULL;
    }

    while (*text != '\0' && isspace((unsigned char)*text)) {
        text++;
    }

    if (*text == '\0') {
        return text;
    }

    end = text + strlen(text) - 1;
    while (end > text && isspace((unsigned char)*end)) {
        *end-- = '\0';
    }

    return text;
}

static bool ensure_media_capacity(muzza_project* proj, size_t needed) {
    muzza_media* grown = NULL;
    size_t new_cap = 0;

    if (!proj) {
        return false;
    }

    if (needed <= proj->media_cap) {
        return true;
    }

    new_cap = proj->media_cap ? proj->media_cap : 8;
    while (new_cap < needed) {
        new_cap *= 2;
    }

    grown = realloc(proj->media_pool, new_cap * sizeof(*proj->media_pool));
    if (!grown) {
        return false;
    }

    memset(grown + proj->media_cap, 0, (new_cap - proj->media_cap) * sizeof(*grown));
    proj->media_pool = grown;
    proj->media_cap = new_cap;
    return true;
}

static bool ensure_clip_capacity(muzza_project* proj, size_t needed) {
    muzza_clip* grown = NULL;
    size_t new_cap = 0;

    if (!proj) {
        return false;
    }

    if (needed <= proj->clips_cap) {
        return true;
    }

    new_cap = proj->clips_cap ? proj->clips_cap : 16;
    while (new_cap < needed) {
        new_cap *= 2;
    }

    grown = realloc(proj->clips, new_cap * sizeof(*proj->clips));
    if (!grown) {
        return false;
    }

    memset(grown + proj->clips_cap, 0, (new_cap - proj->clips_cap) * sizeof(*grown));
    proj->clips = grown;
    proj->clips_cap = new_cap;
    return true;
}

static void init_clip_defaults(clip_parse_state* clip) {
    if (!clip) {
        return;
    }

    memset(clip, 0, sizeof(*clip));
    clip->media_id = -1;
    clip->track_index = 0;
    clip->tl_duration = 1.0;
    clip->opacity = 1.0f;
    clip->scale = 1.0f;
    clip->filter = MUZZA_FILTER_NONE;
    clip->fade_in_duration = 0.0;
    clip->fade_out_duration = 0.0;
}

static bool finalize_media_section(muzza_project* proj, media_parse_state* media) {
    int media_id = -1;

    if (!proj || !media || !media->active) {
        return true;
    }

    media->active = false;
    if (media->filepath[0] == '\0') {
        return false;
    }

    media_id = project_add_media(proj, media->filepath);
    if (media_id < 0) {
        return false;
    }

    if (media->uid > 0) {
        proj->media_pool[media_id].uid = media->uid;
        if (media->uid >= proj->next_media_uid) {
            proj->next_media_uid = media->uid + 1;
        }
    }

    if (media->duration > 0.0) {
        proj->media_pool[media_id].duration = media->duration;
    }
    proj->media_pool[media_id].has_video = media->has_video;
    proj->media_pool[media_id].has_audio = media->has_audio;
    proj->media_pool[media_id].is_image = media->is_image;
    proj->media_pool[media_id].missing = media->missing;

    return true;
}

static bool finalize_clip_section(muzza_project* proj, clip_parse_state* clip) {
    int clip_index = -1;

    if (!proj || !clip || !clip->active) {
        return true;
    }

    clip->active = false;
    if (clip->media_id < 0 || clip->media_id >= (int)proj->num_media || clip->tl_duration <= 0.0) {
        return false;
    }
    if (clip->track_index < 0 || clip->track_index >= MUZZA_MAX_TRACKS) {
        return false;
    }

    clip_index = project_add_clip(
        proj,
        clip->media_id,
        clip->track_index,
        clip->type,
        clip->tl_start,
        clip->tl_duration,
        clip->media_in
    );
    if (clip_index < 0) {
        return false;
    }

    if (clip->uid > 0) {
        proj->clips[clip_index].uid = clip->uid;
        if (clip->uid >= proj->next_clip_uid) {
            proj->next_clip_uid = clip->uid + 1;
        }
    }

    proj->clips[clip_index].opacity = clip->opacity;
    proj->clips[clip_index].scale = clip->scale;
    proj->clips[clip_index].pos_x = clip->pos_x;
    proj->clips[clip_index].pos_y = clip->pos_y;
    proj->clips[clip_index].filter = clip->filter;
    proj->clips[clip_index].fade_in_duration = clip->fade_in_duration;
    proj->clips[clip_index].fade_out_duration = clip->fade_out_duration;
    proj->clips[clip_index].selected = clip->selected;
    proj->clips[clip_index].link_group_id = clip->link_group_id;
    proj->clips[clip_index].link_role = clip->link_role;
    return true;
}

muzza_project* project_create(void) {
    muzza_project* proj = calloc(1, sizeof(*proj));

    if (!proj) {
        return NULL;
    }

    proj->duration = MUZZA_DEFAULT_DURATION;
    proj->dirty = false;
    proj->next_media_uid = 1;
    proj->next_clip_uid = 1;
    proj->next_link_group_id = 1;
    for (int i = 0; i < MUZZA_MAX_TRACKS; ++i) {
        proj->tracks[i].height = MUZZA_DEFAULT_TRACK_HEIGHT;
        proj->tracks[i].gain = 1.0f;
        proj->tracks[i].muted = false;
        proj->tracks[i].solo = false;
        proj->tracks[i].locked = false;
        snprintf(proj->tracks[i].name, sizeof(proj->tracks[i].name), "TRACK %d", i + 1);
    }

    return proj;
}

void project_destroy(muzza_project* proj) {
    if (!proj) {
        return;
    }

    for (size_t i = 0; i < proj->num_media; ++i) {
        if (proj->media_pool[i].dec) {
            decoder_destroy(proj->media_pool[i].dec);
        }
        if (proj->media_pool[i].waveform.mins) {
            free(proj->media_pool[i].waveform.mins);
        }
        if (proj->media_pool[i].waveform.maxs) {
            free(proj->media_pool[i].waveform.maxs);
        }
    }

    for (size_t i = 0; i < proj->num_clips; ++i) {
        if (proj->clips[i].dec) {
            decoder_destroy(proj->clips[i].dec);
        }
    }

    free(proj->media_pool);
    free(proj->clips);
    free(proj);
}

int project_add_media(muzza_project* proj, const char* filepath) {
    muzza_media* media = NULL;

    if (!proj || !filepath || filepath[0] == '\0') {
        return -1;
    }

    for (size_t i = 0; i < proj->num_media; ++i) {
        if (strcmp(proj->media_pool[i].filepath, filepath) == 0) {
            return proj->media_pool[i].id;
        }
    }

    if (!ensure_media_capacity(proj, proj->num_media + 1)) {
        return -1;
    }

    media = &proj->media_pool[proj->num_media];
    memset(media, 0, sizeof(*media));
    media->id = (int)proj->num_media;
    media->uid = proj->next_media_uid++;
    copy_string(media->filepath, sizeof(media->filepath), filepath);
    proj->num_media++;
    proj->dirty = true;
    return media->id;
}

void project_remove_media(muzza_project* proj, int id) {
    size_t media_index = 0;

    if (!proj || id < 0 || (size_t)id >= proj->num_media) {
        return;
    }

    media_index = (size_t)id;
    if (proj->media_pool[media_index].dec) {
        decoder_destroy(proj->media_pool[media_index].dec);
        proj->media_pool[media_index].dec = NULL;
    }
    if (proj->media_pool[media_index].waveform.mins) {
        free(proj->media_pool[media_index].waveform.mins);
        proj->media_pool[media_index].waveform.mins = NULL;
    }
    if (proj->media_pool[media_index].waveform.maxs) {
        free(proj->media_pool[media_index].waveform.maxs);
        proj->media_pool[media_index].waveform.maxs = NULL;
    }
    proj->media_pool[media_index].waveform.num_peaks = 0;

    for (size_t clip_index = 0; clip_index < proj->num_clips;) {
        if (proj->clips[clip_index].media_id == id) {
            project_remove_clip(proj, (int)clip_index);
            continue;
        }

        if (proj->clips[clip_index].media_id > id) {
            proj->clips[clip_index].media_id--;
        }

        clip_index++;
    }

    if (media_index + 1 < proj->num_media) {
        memmove(
            &proj->media_pool[media_index],
            &proj->media_pool[media_index + 1],
            (proj->num_media - media_index - 1) * sizeof(*proj->media_pool)
        );
    }

    proj->num_media--;
    proj->dirty = true;
    for (size_t i = media_index; i < proj->num_media; ++i) {
        proj->media_pool[i].id = (int)i;
    }
}

muzza_media* project_get_media(muzza_project* proj, int id) {
    if (!proj || id < 0 || (size_t)id >= proj->num_media) {
        return NULL;
    }

    return &proj->media_pool[id];
}

bool project_relink_media(muzza_project* proj, int media_id, const char* new_filepath) {
    muzza_media* media = NULL;
    if (!proj || !new_filepath || new_filepath[0] == '\0') {
        return false;
    }
    media = project_get_media(proj, media_id);
    if (!media) {
        return false;
    }
    copy_string(media->filepath, sizeof(media->filepath), new_filepath);
    if (media->dec) {
        decoder_destroy(media->dec);
        media->dec = NULL;
    }
    media->missing = false;
    media->duration = 0.0;
    media->has_video = false;
    media->has_audio = false;
    media->is_image = false;
    if (media->waveform.mins) {
        free(media->waveform.mins);
        media->waveform.mins = NULL;
    }
    if (media->waveform.maxs) {
        free(media->waveform.maxs);
        media->waveform.maxs = NULL;
    }
    media->waveform.num_peaks = 0;
    proj->dirty = true;
    return true;
}

muzza_media* project_get_media_by_uid(muzza_project* proj, uint64_t uid) {
    if (!proj || uid == 0) {
        return NULL;
    }
    for (size_t i = 0; i < proj->num_media; ++i) {
        if (proj->media_pool[i].uid == uid) {
            return &proj->media_pool[i];
        }
    }
    return NULL;
}

muzza_decoder* project_ensure_media_decoder(muzza_project* proj, int id, fx_context* ctx) {
    muzza_media* media = project_get_media(proj, id);

    if (!media || !ctx) {
        return NULL;
    }

    if (!media->dec) {
        media->dec = decoder_create(ctx, media->filepath, MUZZA_DECODER_BOTH);
        if (media->dec) {
            media->missing = false;
            media->has_video = decoder_has_video(media->dec);
            media->has_audio = decoder_has_audio(media->dec);
            if (media->duration <= 0.0) {
                media->duration = decoder_get_duration(media->dec);
            }

            media->is_image = decoder_is_image(media->dec);
        } else {
            media->missing = true;
        }
    }

    if (media->dec && media->has_audio && !media->waveform.mins && media->duration > 0.0) {
        /* Dynamic resolution: ~5ms per bucket, clamped to sensible range */
        int num_peaks = (int)(media->duration * 200.0);
        if (num_peaks < 2048) num_peaks = 2048;
        if (num_peaks > 16384) num_peaks = 16384;
        media->waveform.num_peaks = num_peaks;
        media->waveform.mins = calloc((size_t)num_peaks, sizeof(float));
        media->waveform.maxs = calloc((size_t)num_peaks, sizeof(float));
        if (media->waveform.mins && media->waveform.maxs) {
            bool ok = decoder_generate_waveform(media->filepath, media->waveform.mins, media->waveform.maxs, num_peaks, media->duration);
            if (!ok) {
                free(media->waveform.mins);
                free(media->waveform.maxs);
                media->waveform.mins = NULL;
                media->waveform.maxs = NULL;
                media->waveform.num_peaks = 0;
            }
        }
    }

    return media->dec;
}

muzza_decoder* project_ensure_clip_decoder(muzza_project* proj, int clip_index, fx_context* ctx) {
    muzza_media* media = NULL;

    if (!proj || clip_index < 0 || (size_t)clip_index >= proj->num_clips || !ctx) {
        return NULL;
    }

    muzza_clip* clip = &proj->clips[clip_index];
    if (!clip->dec) {
        media = project_get_media(proj, clip->media_id);
        if (media) {
            muzza_decoder_mode mode = (clip->type == MUZZA_CLIP_VIDEO) ? MUZZA_DECODER_VIDEO : MUZZA_DECODER_AUDIO;
            clip->dec = decoder_create(ctx, media->filepath, mode);
        }
    }

    /* Ensure waveform is generated for audio clips when needed */
    if (clip->type == MUZZA_CLIP_AUDIO) {
        media = project_get_media(proj, clip->media_id);
        if (media && media->has_audio && !media->waveform.mins) {
            project_ensure_media_decoder(proj, clip->media_id, ctx);
        }
    }

    return clip->dec;
}

int project_add_clip(muzza_project* proj, int media_id, int track, muzza_clip_type type, double start, double dur, double media_in) {
    muzza_clip* clip = NULL;

    if (!proj || media_id < 0 || (size_t)media_id >= proj->num_media) {
        return -1;
    }

    if (track < 0 || track >= MUZZA_MAX_TRACKS || dur <= 0.0) {
        return -1;
    }

    if (project_clips_would_overlap(proj, -1, track, start, dur)) {
        return -1;
    }

    if (!ensure_clip_capacity(proj, proj->num_clips + 1)) {
        return -1;
    }

    clip = &proj->clips[proj->num_clips];
    memset(clip, 0, sizeof(*clip));
    clip->media_id = media_id;
    clip->uid = proj->next_clip_uid++;
    clip->track_index = track;
    clip->type = type;
    clip->tl_start = start < 0.0 ? 0.0 : start;
    clip->tl_duration = dur;
    clip->media_in = media_in < 0.0 ? 0.0 : media_in;
    clip->opacity = 1.0f;
    clip->scale = 1.0f;
    clip->filter = MUZZA_FILTER_NONE;
    clip->fade_in_duration = 0.0;
    clip->fade_out_duration = 0.0;
    clip->link_group_id = 0;
    clip->link_role = MUZZA_LINK_NONE;

    proj->num_clips++;
    proj->dirty = true;
    project_recalculate_duration(proj);
    return (int)(proj->num_clips - 1);
}

void project_remove_clip(muzza_project* proj, int index) {
    size_t idx = (size_t)index;

    if (!proj || index < 0 || idx >= proj->num_clips) {
        return;
    }

    if (proj->clips[idx].dec) {
        decoder_destroy(proj->clips[idx].dec);
    }

    if (idx + 1 < proj->num_clips) {
        memmove(
            &proj->clips[idx],
            &proj->clips[idx + 1],
            (proj->num_clips - idx - 1) * sizeof(*proj->clips)
        );
    }

    proj->num_clips--;
    proj->dirty = true;
    project_recalculate_duration(proj);
}

int project_split_clip(muzza_project* proj, int clip_index, double split_time) {
    if (!proj || clip_index < 0 || (size_t)clip_index >= proj->num_clips) return -1;

    muzza_clip* orig = &proj->clips[clip_index];
    double old_end = orig->tl_start + orig->tl_duration;

    if (split_time <= orig->tl_start + 0.05 || split_time >= old_end - 0.05) return -1;

    double split_offset = split_time - orig->tl_start;
    double new_media_in = orig->media_in + split_offset;

    // Ensure capacity
    if (proj->num_clips >= proj->clips_cap) {
        size_t new_cap = proj->clips_cap < 8 ? 8 : proj->clips_cap * 2;
        muzza_clip* new_arr = realloc(proj->clips, sizeof(*proj->clips) * new_cap);
        if (!new_arr) return -1;
        proj->clips = new_arr;
        proj->clips_cap = new_cap;
    }

    // Shift clips after orig to make room
    for (size_t i = proj->num_clips; i > (size_t)clip_index + 1; --i) {
        proj->clips[i] = proj->clips[i - 1];
    }

    // New clip (right side)
    muzza_clip* new_clip = &proj->clips[clip_index + 1];
    new_clip->media_id = orig->media_id;
    new_clip->track_index = orig->track_index;
    new_clip->type = orig->type;
    new_clip->tl_start = split_time;
    new_clip->tl_duration = old_end - split_time;
    new_clip->media_in = new_media_in;
    new_clip->opacity = orig->opacity;
    new_clip->scale = orig->scale;
    new_clip->pos_x = orig->pos_x;
    new_clip->pos_y = orig->pos_y;
    new_clip->filter = orig->filter;
    new_clip->fade_in_duration = 0.0;
    new_clip->fade_out_duration = orig->fade_out_duration;
    new_clip->selected = false;
    new_clip->dec = NULL;

    // Shorten original (left side)
    orig->tl_duration = split_time - orig->tl_start;
    orig->fade_out_duration = 0.0;
    orig->selected = true;

    proj->num_clips++;
    proj->dirty = true;
    project_recalculate_duration(proj);
    return clip_index + 1;
}

void project_clear_selection(muzza_project* proj) {
    if (!proj) {
        return;
    }

    for (size_t i = 0; i < proj->num_clips; ++i) {
        proj->clips[i].selected = false;
    }
}

int project_find_top_video_clip(const muzza_project* proj, double time_seconds) {
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

int project_find_active_audio_clips(const muzza_project* proj, double time_seconds, int* out_indices, int max_indices) {
    int count = 0;
    if (!proj || !out_indices || max_indices <= 0) return 0;
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

int project_find_clip_at_time(const muzza_project* proj, double time_seconds) {
    int found = -1;
    int highest_track = -1;

    if (!proj) {
        return -1;
    }

    for (size_t i = 0; i < proj->num_clips; ++i) {
        const muzza_clip* clip = &proj->clips[i];
        double clip_end = clip->tl_start + clip->tl_duration;

        if (time_seconds < clip->tl_start || time_seconds >= clip_end) {
            continue;
        }

        // Prefer video clips for the active slot so monitor preview works
        bool is_video = (clip->type == MUZZA_CLIP_VIDEO);
        bool found_is_video = (found >= 0 && proj->clips[found].type == MUZZA_CLIP_VIDEO);

        if (is_video && !found_is_video) {
            highest_track = clip->track_index;
            found = (int)i;
        } else if (is_video == found_is_video) {
            if (clip->track_index >= highest_track) {
                highest_track = clip->track_index;
                found = (int)i;
            }
        }
    }

    return found;
}

double project_get_clip_media_time(const muzza_project* proj, const muzza_clip* clip, double timeline_time) {
    double local_time = 0.0;
    muzza_media* media = NULL;

    if (!proj || !clip) {
        return 0.0;
    }

    local_time = clip->media_in + (timeline_time - clip->tl_start);
    if (local_time < 0.0) {
        local_time = 0.0;
    }

    media = project_get_media((muzza_project*)proj, clip->media_id);
    if (media && media->duration > 0.0 && !media->is_image && local_time > media->duration) {
        local_time = media->duration;
    }

    return local_time;
}

void project_recalculate_duration(muzza_project* proj) {
    double duration = MUZZA_DEFAULT_DURATION;

    if (!proj) {
        return;
    }

    for (size_t i = 0; i < proj->num_clips; ++i) {
        double clip_end = proj->clips[i].tl_start + proj->clips[i].tl_duration;
        if (clip_end > duration) {
            duration = clip_end;
        }
    }

    proj->duration = duration;
}

double project_get_clip_fade_opacity(const muzza_clip* clip, double timeline_time) {
    if (!clip) return 1.0;
    double t = timeline_time - clip->tl_start;
    if (t < 0.0) return 0.0;
    if (t > clip->tl_duration) return 0.0;

    if (clip->fade_in_duration > 0.0 && t < clip->fade_in_duration) {
        return t / clip->fade_in_duration;
    }
    if (clip->fade_out_duration > 0.0) {
        double fade_start = clip->tl_duration - clip->fade_out_duration;
        if (t > fade_start) {
            double rem = clip->tl_duration - t;
            if (rem < 0.0) rem = 0.0;
            return rem / clip->fade_out_duration;
        }
    }
    return 1.0;
}

bool project_trim_clip_left(muzza_project* proj, int clip_index, double new_start, double* out_new_media_in, double* out_new_duration) {
    if (!proj || clip_index < 0 || (size_t)clip_index >= proj->num_clips || !out_new_media_in || !out_new_duration) {
        return false;
    }

    muzza_clip* clip = &proj->clips[clip_index];
    muzza_media* media = project_get_media(proj, clip->media_id);
    double media_dur = media ? media->duration : 0.0;
    double old_end = clip->tl_start + clip->tl_duration;
    double delta = new_start - clip->tl_start;
    double resolved_start = new_start;
    double resolved_media_in = clip->media_in + delta;
    double resolved_dur = old_end - resolved_start;

    // Clamp to media bounds for non-image media
    if (resolved_media_in < 0.0) {
        resolved_media_in = 0.0;
        resolved_start = clip->tl_start - clip->media_in;
        resolved_dur = old_end - resolved_start;
    }
    if (resolved_dur < 0.1) {
        resolved_dur = 0.1;
        resolved_start = old_end - 0.1;
        resolved_media_in = clip->media_in + (resolved_start - clip->tl_start);
    }
    if (media_dur > 0.0 && !(media && media->is_image)) {
        if (resolved_media_in + resolved_dur > media_dur) {
            double overflow = (resolved_media_in + resolved_dur) - media_dur;
            resolved_media_in -= overflow;
            resolved_start -= overflow;
            resolved_dur = old_end - resolved_start;
        }
    }
    if (resolved_media_in < 0.0) resolved_media_in = 0.0;
    if (resolved_start < 0.0) {
        resolved_start = 0.0;
        resolved_dur = old_end;
        resolved_media_in = clip->media_in - clip->tl_start;
        if (resolved_media_in < 0.0) resolved_media_in = 0.0;
    }

    clip->tl_start = resolved_start;
    clip->media_in = resolved_media_in;
    clip->tl_duration = resolved_dur;
    *out_new_media_in = resolved_media_in;
    *out_new_duration = resolved_dur;
    project_recalculate_duration(proj);
    return true;
}

bool project_trim_clip_right(muzza_project* proj, int clip_index, double new_duration) {
    if (!proj || clip_index < 0 || (size_t)clip_index >= proj->num_clips) {
        return false;
    }

    muzza_clip* clip = &proj->clips[clip_index];
    muzza_media* media = project_get_media(proj, clip->media_id);
    double media_dur = media ? media->duration : 0.0;
    double resolved_dur = new_duration;

    if (resolved_dur < 0.1) resolved_dur = 0.1;
    if (media_dur > 0.0 && !(media && media->is_image)) {
        if (clip->media_in + resolved_dur > media_dur) {
            resolved_dur = media_dur - clip->media_in;
            if (resolved_dur < 0.1) resolved_dur = 0.1;
        }
    }

    clip->tl_duration = resolved_dur;
    proj->dirty = true;
    project_recalculate_duration(proj);
    return true;
}

bool project_clips_would_overlap(const muzza_project* proj, int exclude_clip_index, int track_index, double start, double duration) {
    if (!proj || track_index < 0 || track_index >= MUZZA_MAX_TRACKS || duration <= 0.0) {
        return false;
    }
    double new_end = start + duration;
    for (size_t i = 0; i < proj->num_clips; ++i) {
        if ((int)i == exclude_clip_index) continue;
        const muzza_clip* clip = &proj->clips[i];
        if (clip->track_index != track_index) continue;
        double clip_end = clip->tl_start + clip->tl_duration;
        if (!(new_end <= clip->tl_start || start >= clip_end)) {
            return true;
        }
    }
    return false;
}

void project_ripple_delete_clip(muzza_project* proj, int clip_index) {
    if (!proj || clip_index < 0 || (size_t)clip_index >= proj->num_clips) {
        return;
    }

    muzza_clip target = proj->clips[clip_index];
    double gap_start = target.tl_start;
    double gap_dur = target.tl_duration;
    int track = target.track_index;
    uint64_t link_group = target.link_group_id;

    project_remove_clip(proj, clip_index);

    for (size_t i = 0; i < proj->num_clips; ++i) {
        muzza_clip* clip = &proj->clips[i];
        if (clip->track_index == track && clip->tl_start >= gap_start) {
            clip->tl_start -= gap_dur;
            if (clip->tl_start < 0.0) clip->tl_start = 0.0;
        }
        if (link_group > 0 && clip->link_group_id == link_group && clip->track_index != track && clip->tl_start >= gap_start) {
            clip->tl_start -= gap_dur;
            if (clip->tl_start < 0.0) clip->tl_start = 0.0;
        }
    }

    proj->dirty = true;
    project_recalculate_duration(proj);
}

muzza_move_result project_move_clip(muzza_project* proj, int clip_index, double new_start, int new_track) {
    if (!proj || clip_index < 0 || (size_t)clip_index >= proj->num_clips) {
        return MUZZA_MOVE_REJECTED;
    }

    muzza_clip* clip = &proj->clips[clip_index];
    if (new_track < 0 || new_track >= MUZZA_MAX_TRACKS) {
        return MUZZA_MOVE_REJECTED;
    }
    if (new_start < 0.0) {
        new_start = 0.0;
    }

    if (project_clips_would_overlap(proj, clip_index, new_track, new_start, clip->tl_duration)) {
        return MUZZA_MOVE_REJECTED;
    }

    int old_track = clip->track_index;
    int track_delta = new_track - old_track;
    clip->tl_start = new_start;
    clip->track_index = new_track;

    /* Move linked AV clip together */
    if (clip->link_group_id > 0) {
        for (size_t i = 0; i < proj->num_clips; ++i) {
            if ((int)i == clip_index) continue;
            muzza_clip* linked = &proj->clips[i];
            if (linked->link_group_id == clip->link_group_id) {
                int linked_new_track = linked->track_index + track_delta;
                if (linked_new_track < 0) linked_new_track = 0;
                if (linked_new_track >= MUZZA_MAX_TRACKS) linked_new_track = MUZZA_MAX_TRACKS - 1;
                /* Only move if no overlap on target track */
                if (!project_clips_would_overlap(proj, (int)i, linked_new_track, new_start, linked->tl_duration)) {
                    linked->tl_start = new_start;
                    linked->track_index = linked_new_track;
                }
                break;
            }
        }
    }

    project_recalculate_duration(proj);
    return MUZZA_MOVE_OK;
}

bool project_save(muzza_project* proj, const char* filepath) {
    FILE* file = NULL;

    if (!proj || !filepath || filepath[0] == '\0') {
        return false;
    }

    file = fopen(filepath, "w");
    if (!file) {
        return false;
    }

    fprintf(file, "[Project]\n");
    fprintf(file, "version=1\n");
    fprintf(file, "duration=%.6f\n", proj->duration);
    fprintf(file, "next_media_uid=%llu\n", (unsigned long long)proj->next_media_uid);
    fprintf(file, "next_clip_uid=%llu\n", (unsigned long long)proj->next_clip_uid);
    for (int i = 0; i < MUZZA_MAX_TRACKS; ++i) {
        fprintf(file, "track_height_%d=%.2f\n", i, proj->tracks[i].height);
        fprintf(file, "track_gain_%d=%.2f\n", i, proj->tracks[i].gain);
        fprintf(file, "track_muted_%d=%d\n", i, proj->tracks[i].muted ? 1 : 0);
        fprintf(file, "track_solo_%d=%d\n", i, proj->tracks[i].solo ? 1 : 0);
        fprintf(file, "track_locked_%d=%d\n", i, proj->tracks[i].locked ? 1 : 0);
        fprintf(file, "track_name_%d=%s\n", i, proj->tracks[i].name);
    }

    for (size_t i = 0; i < proj->num_media; ++i) {
        fprintf(file, "\n[Media %zu]\n", i);
        fprintf(file, "uid=%llu\n", (unsigned long long)proj->media_pool[i].uid);
        fprintf(file, "path=%s\n", proj->media_pool[i].filepath);
        fprintf(file, "duration=%.6f\n", proj->media_pool[i].duration);
        fprintf(file, "has_video=%d\n", proj->media_pool[i].has_video ? 1 : 0);
        fprintf(file, "has_audio=%d\n", proj->media_pool[i].has_audio ? 1 : 0);
        fprintf(file, "is_image=%d\n", proj->media_pool[i].is_image ? 1 : 0);
        fprintf(file, "missing=%d\n", proj->media_pool[i].missing ? 1 : 0);
    }

    for (size_t i = 0; i < proj->num_clips; ++i) {
        const muzza_clip* clip = &proj->clips[i];
        fprintf(file, "\n[Clip %zu]\n", i);
        fprintf(file, "uid=%llu\n", (unsigned long long)clip->uid);
        fprintf(file, "media=%d\n", clip->media_id);
        fprintf(file, "track=%d\n", clip->track_index);
        fprintf(file, "type=%d\n", (int)clip->type);
        fprintf(file, "start=%.6f\n", clip->tl_start);
        fprintf(file, "dur=%.6f\n", clip->tl_duration);
        fprintf(file, "in=%.6f\n", clip->media_in);
        fprintf(file, "opacity=%.3f\n", clip->opacity);
        fprintf(file, "scale=%.3f\n", clip->scale);
        fprintf(file, "pos_x=%.3f\n", clip->pos_x);
        fprintf(file, "pos_y=%.3f\n", clip->pos_y);
    fprintf(file, "filter=%d\n", (int)clip->filter);
    fprintf(file, "fade_in=%.3f\n", clip->fade_in_duration);
    fprintf(file, "fade_out=%.3f\n", clip->fade_out_duration);
    fprintf(file, "selected=%d\n", clip->selected ? 1 : 0);
    fprintf(file, "link_group=%llu\n", (unsigned long long)clip->link_group_id);
        fprintf(file, "link_role=%d\n", (int)clip->link_role);
    }

    fclose(file);
    copy_string(proj->filepath, sizeof(proj->filepath), filepath);
    proj->dirty = false;
    return true;
}

muzza_project* project_load(const char* filepath) {
    FILE* file = NULL;
    muzza_project* proj = NULL;
    project_section section = SECTION_NONE;
    media_parse_state media = {0};
    clip_parse_state clip;
    char line[1024];

    if (!filepath || filepath[0] == '\0') {
        return NULL;
    }

    file = fopen(filepath, "r");
    if (!file) {
        return NULL;
    }

    proj = project_create();
    if (!proj) {
        fclose(file);
        return NULL;
    }

    copy_string(proj->filepath, sizeof(proj->filepath), filepath);
    init_clip_defaults(&clip);

    while (fgets(line, sizeof(line), file)) {
        char* entry = trim_in_place(line);
        char* equals = NULL;

        if (*entry == '\0' || *entry == '#' || *entry == ';') {
            continue;
        }

        if (*entry == '[') {
            if (!finalize_media_section(proj, &media) || !finalize_clip_section(proj, &clip)) {
                project_destroy(proj);
                fclose(file);
                return NULL;
            }

            if (strncmp(entry, "[Project]", 9) == 0) {
                section = SECTION_PROJECT;
            } else if (strncmp(entry, "[Media", 6) == 0) {
                section = SECTION_MEDIA;
                memset(&media, 0, sizeof(media));
                media.active = true;
            } else if (strncmp(entry, "[Clip", 5) == 0) {
                section = SECTION_CLIP;
                init_clip_defaults(&clip);
                clip.active = true;
            } else {
                section = SECTION_NONE;
            }
            continue;
        }

        equals = strchr(entry, '=');
        if (!equals) {
            continue;
        }

        *equals = '\0';
        char* key = trim_in_place(entry);
        char* value = trim_in_place(equals + 1);

        if (section == SECTION_PROJECT) {
            if (strcmp(key, "duration") == 0) {
                proj->duration = strtod(value, NULL);
            } else if (strncmp(key, "track_height_", 13) == 0) {
                int track = atoi(key + 13);
                if (track >= 0 && track < MUZZA_MAX_TRACKS) {
                    proj->tracks[track].height = (float)strtod(value, NULL);
                }
            }
        } else if (section == SECTION_MEDIA && media.active) {
            if (strcmp(key, "uid") == 0) {
                media.uid = (uint64_t)strtoull(value, NULL, 10);
            } else if (strcmp(key, "path") == 0) {
                copy_string(media.filepath, sizeof(media.filepath), value);
            } else if (strcmp(key, "duration") == 0) {
                media.duration = strtod(value, NULL);
            } else if (strcmp(key, "has_video") == 0) {
                media.has_video = atoi(value) != 0;
            } else if (strcmp(key, "has_audio") == 0) {
                media.has_audio = atoi(value) != 0;
            } else if (strcmp(key, "is_image") == 0) {
                media.is_image = atoi(value) != 0;
            } else if (strcmp(key, "missing") == 0) {
                media.missing = atoi(value) != 0;
            }
        } else if (section == SECTION_CLIP && clip.active) {
            if (strcmp(key, "uid") == 0) {
                clip.uid = (uint64_t)strtoull(value, NULL, 10);
            } else if (strcmp(key, "media") == 0) {
                clip.media_id = atoi(value);
            } else if (strcmp(key, "track") == 0) {
                clip.track_index = atoi(value);
            } else if (strcmp(key, "type") == 0) {
                clip.type = (muzza_clip_type)atoi(value);
            } else if (strcmp(key, "start") == 0) {
                clip.tl_start = strtod(value, NULL);
            } else if (strcmp(key, "dur") == 0) {
                clip.tl_duration = strtod(value, NULL);
            } else if (strcmp(key, "in") == 0) {
                clip.media_in = strtod(value, NULL);
            } else if (strcmp(key, "opacity") == 0) {
                clip.opacity = (float)strtod(value, NULL);
            } else if (strcmp(key, "scale") == 0) {
                clip.scale = (float)strtod(value, NULL);
            } else if (strcmp(key, "pos_x") == 0) {
                clip.pos_x = (float)strtod(value, NULL);
            } else if (strcmp(key, "pos_y") == 0) {
                clip.pos_y = (float)strtod(value, NULL);
            } else if (strcmp(key, "filter") == 0) {
                clip.filter = (muzza_filter_type)atoi(value);
            } else if (strcmp(key, "fade_in") == 0) {
                clip.fade_in_duration = strtod(value, NULL);
            } else if (strcmp(key, "fade_out") == 0) {
                clip.fade_out_duration = strtod(value, NULL);
            } else if (strcmp(key, "selected") == 0) {
                clip.selected = atoi(value) != 0;
            } else if (strcmp(key, "link_group") == 0) {
                clip.link_group_id = (uint64_t)strtoull(value, NULL, 10);
            } else if (strcmp(key, "link_role") == 0) {
                clip.link_role = (muzza_link_role)atoi(value);
            }
        }
    }

    if (!finalize_media_section(proj, &media) || !finalize_clip_section(proj, &clip)) {
        project_destroy(proj);
        fclose(file);
        return NULL;
    }

    fclose(file);
    project_recalculate_duration(proj);
    return proj;
}
