#ifndef MUZZA_PROJECT_H
#define MUZZA_PROJECT_H

#include <stdbool.h>
#include <stddef.h>
#include "decoder.h"

#define MUZZA_MAX_TRACKS 8
#define MUZZA_DEFAULT_TRACK_HEIGHT 40.0f
#define MUZZA_DEFAULT_DURATION 60.0

typedef enum {
    MUZZA_FILTER_NONE = 0,
    MUZZA_FILTER_GRAYSCALE = 1,
    MUZZA_FILTER_INVERT = 2
} muzza_filter_type;

typedef enum {
    MUZZA_CLIP_VIDEO = 0,
    MUZZA_CLIP_AUDIO = 1
} muzza_clip_type;

typedef struct {
    float* peaks;
    int num_peaks;
} muzza_waveform;

typedef struct {
    int id;
    char filepath[512];
    muzza_decoder* dec;
    double duration;
    bool has_video;
    bool has_audio;
    muzza_waveform waveform;
} muzza_media;

typedef struct {
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
    bool selected;

    muzza_decoder* dec;
} muzza_clip;

typedef struct {
    char filepath[512];
    muzza_media* media_pool;
    size_t num_media;
    size_t media_cap;

    muzza_clip* clips;
    size_t num_clips;
    size_t clips_cap;

    double duration;
    float track_heights[MUZZA_MAX_TRACKS];
} muzza_project;

muzza_project* project_create(void);
void project_destroy(muzza_project* proj);

int project_add_media(muzza_project* proj, const char* filepath);
void project_remove_media(muzza_project* proj, int id);
muzza_media* project_get_media(muzza_project* proj, int id);
muzza_decoder* project_ensure_media_decoder(muzza_project* proj, int id, fx_context* ctx);
muzza_decoder* project_ensure_clip_decoder(muzza_project* proj, int clip_index, fx_context* ctx);

int project_add_clip(muzza_project* proj, int media_id, int track, muzza_clip_type type, double start, double dur, double media_in);
void project_remove_clip(muzza_project* proj, int index);
void project_clear_selection(muzza_project* proj);
int project_find_clip_at_time(const muzza_project* proj, double time_seconds);
double project_get_clip_media_time(const muzza_project* proj, const muzza_clip* clip, double timeline_time);
void project_recalculate_duration(muzza_project* proj);

bool project_save(muzza_project* proj, const char* filepath);
muzza_project* project_load(const char* filepath);

#endif
