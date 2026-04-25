#ifndef MUZZA_EXPORTER_H
#define MUZZA_EXPORTER_H

#include <stdbool.h>
#include <SDL3/SDL.h>
#include "project.h"

typedef struct muzza_exporter muzza_exporter;

typedef enum {
    MUZZA_EXPORT_IDLE = 0,
    MUZZA_EXPORT_RUNNING,
    MUZZA_EXPORT_DONE,
    MUZZA_EXPORT_CANCELLED,
    MUZZA_EXPORT_ERROR
} muzza_export_status;

typedef enum {
    MUZZA_FORMAT_MP4_H264,    /* H.264 + AAC (default) */
    MUZZA_FORMAT_MP4_HEVC,    /* H.265 + AAC */
    MUZZA_FORMAT_WEBM_VP9,    /* VP9 + Opus */
    MUZZA_FORMAT_MOV_PRORES,  /* ProRes + PCM */
    MUZZA_FORMAT_MKV,         /* H.264 + FLAC */
    MUZZA_FORMAT_MP3,         /* Audio only: MP3 */
    MUZZA_FORMAT_WAV,         /* Audio only: WAV/PCM */
    MUZZA_FORMAT_COUNT
} muzza_export_format;

typedef struct {
    int width;
    int height;
    double fps;
    int audio_sample_rate;
    int audio_channels;
    bool include_video;
    bool include_audio;
    muzza_export_format format;
    int video_bitrate;        /* kbps, 0 = use CRF */
    int crf;                  /* 0-51, only used if video_bitrate == 0 */
    int audio_bitrate;        /* kbps */
    int preset;               /* 0=ultrafast ... 8=veryslow */
} muzza_export_settings;

muzza_exporter* exporter_create(const muzza_project* project, const char* output_path, const muzza_export_settings* settings);
void exporter_destroy(muzza_exporter* exporter);

SDL_Thread* exporter_start_thread(muzza_exporter* exporter);

muzza_export_status exporter_get_status(muzza_exporter* exporter);
double exporter_get_progress(muzza_exporter* exporter);
const char* exporter_get_error(muzza_exporter* exporter);
void exporter_request_cancel(muzza_exporter* exporter);

/* Format helpers */
const char* exporter_format_name(muzza_export_format format);
const char* exporter_format_extension(muzza_export_format format);
bool exporter_format_has_video(muzza_export_format format);
bool exporter_format_has_audio(muzza_export_format format);

/* Build default output path with timestamp, resolution, version-increment */
void exporter_build_default_path(const muzza_project* project, const muzza_export_settings* settings, char* out, size_t out_size);

#endif
