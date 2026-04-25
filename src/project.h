#ifndef MUZZA_PROJECT_H
#define MUZZA_PROJECT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "decoder.h"

#define MUZZA_MAX_TRACKS 8
#define MUZZA_DEFAULT_TRACK_HEIGHT 40.0f
#define MUZZA_DEFAULT_DURATION 60.0

typedef struct {
    float height;
    float gain;
    bool muted;
    bool solo;
    bool locked;
    char name[64];
} muzza_track;

typedef enum {
    MUZZA_FILTER_NONE = 0,
    MUZZA_FILTER_GRAYSCALE = 1,
    MUZZA_FILTER_INVERT = 2
} muzza_filter_type;

typedef enum {
    MUZZA_CLIP_VIDEO = 0,
    MUZZA_CLIP_AUDIO = 1
} muzza_clip_type;

typedef enum {
    MUZZA_LINK_NONE = 0,
    MUZZA_LINK_VIDEO,
    MUZZA_LINK_AUDIO
} muzza_link_role;

typedef struct {
    float* mins;       /* min amplitude per bucket, normalized to [-1, 0] */
    float* maxs;       /* max amplitude per bucket, normalized to [0, 1]  */
    int num_peaks;
} muzza_waveform;

typedef struct {
    int id;
    uint64_t uid;
    char filepath[512];
    muzza_decoder* dec;
    double duration;
    bool has_video;
    bool has_audio;
    bool is_image;
    bool missing;
    muzza_waveform waveform;
} muzza_media;

typedef struct {
    int media_id;
    uint64_t uid;
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

    uint64_t link_group_id;
    muzza_link_role link_role;

    muzza_decoder* dec;
} muzza_clip;

typedef struct {
    char filepath[512];
    uint64_t next_media_uid;
    uint64_t next_clip_uid;
    uint64_t next_link_group_id;
    muzza_media* media_pool;
    size_t num_media;
    size_t media_cap;

    muzza_clip* clips;
    size_t num_clips;
    size_t clips_cap;

    double duration;
    bool dirty;
    muzza_track tracks[MUZZA_MAX_TRACKS];
} muzza_project;

typedef enum {
    MUZZA_MOVE_OK = 0,
    MUZZA_MOVE_CLIPPED,
    MUZZA_MOVE_REJECTED
} muzza_move_result;

muzza_project* project_create(void);
void project_destroy(muzza_project* proj);

int project_add_media(muzza_project* proj, const char* filepath);
void project_remove_media(muzza_project* proj, int id);
muzza_media* project_get_media(muzza_project* proj, int id);
muzza_media* project_get_media_by_uid(muzza_project* proj, uint64_t uid);
bool project_relink_media(muzza_project* proj, int media_id, const char* new_filepath);
muzza_decoder* project_ensure_media_decoder(muzza_project* proj, int id, fx_context* ctx);
muzza_decoder* project_ensure_clip_decoder(muzza_project* proj, int clip_index, fx_context* ctx);

int project_add_clip(muzza_project* proj, int media_id, int track, muzza_clip_type type, double start, double dur, double media_in);
void project_remove_clip(muzza_project* proj, int index);
void project_clear_selection(muzza_project* proj);
int project_split_clip(muzza_project* proj, int clip_index, double split_time);
int project_find_clip_at_time(const muzza_project* proj, double time_seconds);
int project_find_top_video_clip(const muzza_project* proj, double time_seconds);
int project_find_active_audio_clips(const muzza_project* proj, double time_seconds, int* out_indices, int max_indices);
double project_get_clip_media_time(const muzza_project* proj, const muzza_clip* clip, double timeline_time);
void project_recalculate_duration(muzza_project* proj);

bool project_trim_clip_left(muzza_project* proj, int clip_index, double new_start, double* out_new_media_in, double* out_new_duration);
bool project_trim_clip_right(muzza_project* proj, int clip_index, double new_duration);
bool project_clips_would_overlap(const muzza_project* proj, int exclude_clip_index, int track_index, double start, double duration);
muzza_move_result project_move_clip(muzza_project* proj, int clip_index, double new_start, int new_track);
void project_ripple_delete_clip(muzza_project* proj, int clip_index);

bool project_save(muzza_project* proj, const char* filepath);
muzza_project* project_load(const char* filepath);

/* Timeline coordinate helpers (testable without rendering) */
float time_to_x(double time, float tracks_x, double scroll_x, float zoom, float s);
double x_to_time(float x_coord, float tracks_x, double scroll_x, float zoom, float s);
void apply_cursor_anchored_zoom(double* scroll_x, float* zoom, float tracks_x, float cursor_x, float zoom_factor, float s);

#endif
