#include "project.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    SECTION_NONE = 0,
    SECTION_PROJECT,
    SECTION_MEDIA,
    SECTION_CLIP,
} project_section;

typedef struct {
    bool active;
    char filepath[512];
    double duration;
    bool has_video;
    bool has_audio;
} media_parse_state;

typedef struct {
    bool active;
    int media_id;
    int track_index;
    double tl_start;
    double tl_duration;
    double media_in;
    float opacity;
    float scale;
    float pos_x;
    float pos_y;
    muzza_filter_type filter;
    bool selected;
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

    if (media->duration > 0.0) {
        proj->media_pool[media_id].duration = media->duration;
    }
    proj->media_pool[media_id].has_video = media->has_video;
    proj->media_pool[media_id].has_audio = media->has_audio;

    return true;
}

static bool finalize_clip_section(muzza_project* proj, clip_parse_state* clip) {
    int clip_index = -1;

    if (!proj || !clip || !clip->active) {
        return true;
    }

    clip->active = false;
    if (clip->media_id < 0 || clip->tl_duration <= 0.0) {
        return false;
    }

    clip_index = project_add_clip(
        proj,
        clip->media_id,
        clip->track_index,
        clip->tl_start,
        clip->tl_duration,
        clip->media_in
    );
    if (clip_index < 0) {
        return false;
    }

    proj->clips[clip_index].opacity = clip->opacity;
    proj->clips[clip_index].scale = clip->scale;
    proj->clips[clip_index].pos_x = clip->pos_x;
    proj->clips[clip_index].pos_y = clip->pos_y;
    proj->clips[clip_index].filter = clip->filter;
    proj->clips[clip_index].selected = clip->selected;
    return true;
}

muzza_project* project_create(void) {
    muzza_project* proj = calloc(1, sizeof(*proj));

    if (!proj) {
        return NULL;
    }

    proj->duration = MUZZA_DEFAULT_DURATION;
    for (int i = 0; i < MUZZA_MAX_TRACKS; ++i) {
        proj->track_heights[i] = MUZZA_DEFAULT_TRACK_HEIGHT;
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
    copy_string(media->filepath, sizeof(media->filepath), filepath);
    proj->num_media++;
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

muzza_decoder* project_ensure_media_decoder(muzza_project* proj, int id, fx_context* ctx) {
    muzza_media* media = project_get_media(proj, id);

    if (!media || !ctx) {
        return NULL;
    }

    if (!media->dec) {
        media->dec = decoder_create(ctx, media->filepath);
        if (media->dec) {
            media->has_video = decoder_has_video(media->dec);
            media->has_audio = decoder_has_audio(media->dec);
            if (media->duration <= 0.0) {
                media->duration = decoder_get_duration(media->dec);
            }
        }
    }

    return media->dec;
}

int project_add_clip(muzza_project* proj, int media_id, int track, double start, double dur, double media_in) {
    muzza_clip* clip = NULL;

    if (!proj || media_id < 0 || (size_t)media_id >= proj->num_media) {
        return -1;
    }

    if (track < 0 || track >= MUZZA_MAX_TRACKS || dur <= 0.0) {
        return -1;
    }

    if (!ensure_clip_capacity(proj, proj->num_clips + 1)) {
        return -1;
    }

    clip = &proj->clips[proj->num_clips];
    memset(clip, 0, sizeof(*clip));
    clip->media_id = media_id;
    clip->track_index = track;
    clip->tl_start = start < 0.0 ? 0.0 : start;
    clip->tl_duration = dur;
    clip->media_in = media_in < 0.0 ? 0.0 : media_in;
    clip->opacity = 1.0f;
    clip->scale = 1.0f;
    clip->filter = MUZZA_FILTER_NONE;

    proj->num_clips++;
    project_recalculate_duration(proj);
    return (int)(proj->num_clips - 1);
}

void project_remove_clip(muzza_project* proj, int index) {
    size_t idx = (size_t)index;

    if (!proj || index < 0 || idx >= proj->num_clips) {
        return;
    }

    if (idx + 1 < proj->num_clips) {
        memmove(
            &proj->clips[idx],
            &proj->clips[idx + 1],
            (proj->num_clips - idx - 1) * sizeof(*proj->clips)
        );
    }

    proj->num_clips--;
    project_recalculate_duration(proj);
}

void project_clear_selection(muzza_project* proj) {
    if (!proj) {
        return;
    }

    for (size_t i = 0; i < proj->num_clips; ++i) {
        proj->clips[i].selected = false;
    }
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

        if (clip->track_index >= highest_track) {
            highest_track = clip->track_index;
            found = (int)i;
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
    if (media && media->duration > 0.0 && local_time > media->duration) {
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
    for (int i = 0; i < MUZZA_MAX_TRACKS; ++i) {
        fprintf(file, "track_height_%d=%.2f\n", i, proj->track_heights[i]);
    }

    for (size_t i = 0; i < proj->num_media; ++i) {
        fprintf(file, "\n[Media %zu]\n", i);
        fprintf(file, "path=%s\n", proj->media_pool[i].filepath);
        fprintf(file, "duration=%.6f\n", proj->media_pool[i].duration);
        fprintf(file, "has_video=%d\n", proj->media_pool[i].has_video ? 1 : 0);
        fprintf(file, "has_audio=%d\n", proj->media_pool[i].has_audio ? 1 : 0);
    }

    for (size_t i = 0; i < proj->num_clips; ++i) {
        const muzza_clip* clip = &proj->clips[i];
        fprintf(file, "\n[Clip %zu]\n", i);
        fprintf(file, "media=%d\n", clip->media_id);
        fprintf(file, "track=%d\n", clip->track_index);
        fprintf(file, "start=%.6f\n", clip->tl_start);
        fprintf(file, "dur=%.6f\n", clip->tl_duration);
        fprintf(file, "in=%.6f\n", clip->media_in);
        fprintf(file, "opacity=%.3f\n", clip->opacity);
        fprintf(file, "scale=%.3f\n", clip->scale);
        fprintf(file, "pos_x=%.3f\n", clip->pos_x);
        fprintf(file, "pos_y=%.3f\n", clip->pos_y);
        fprintf(file, "filter=%d\n", (int)clip->filter);
        fprintf(file, "selected=%d\n", clip->selected ? 1 : 0);
    }

    fclose(file);
    copy_string(proj->filepath, sizeof(proj->filepath), filepath);
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
                    proj->track_heights[track] = (float)strtod(value, NULL);
                }
            }
        } else if (section == SECTION_MEDIA && media.active) {
            if (strcmp(key, "path") == 0) {
                copy_string(media.filepath, sizeof(media.filepath), value);
            } else if (strcmp(key, "duration") == 0) {
                media.duration = strtod(value, NULL);
            } else if (strcmp(key, "has_video") == 0) {
                media.has_video = atoi(value) != 0;
            } else if (strcmp(key, "has_audio") == 0) {
                media.has_audio = atoi(value) != 0;
            }
        } else if (section == SECTION_CLIP && clip.active) {
            if (strcmp(key, "media") == 0) {
                clip.media_id = atoi(value);
            } else if (strcmp(key, "track") == 0) {
                clip.track_index = atoi(value);
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
            } else if (strcmp(key, "selected") == 0) {
                clip.selected = atoi(value) != 0;
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
