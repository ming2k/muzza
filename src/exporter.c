#include "exporter.h"

#include <SDL3/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <unistd.h>

#define EXPORT_FPS 30.0
#define EXPORT_AUDIO_RATE 48000
#define EXPORT_AUDIO_CH 2

/* -------------------------------------------------------------------------- */
/* Format registry                                                            */
/* -------------------------------------------------------------------------- */
typedef struct {
    muzza_export_format format;
    const char* name;
    const char* ext;
    const char* video_codec_name;
    const char* video_codec_fb;
    enum AVCodecID video_codec_id;
    const char* audio_codec_name;
    enum AVCodecID audio_codec_id;
    bool has_video;
    bool has_audio;
} format_desc;

static const format_desc FORMATS[] = {
    {MUZZA_FORMAT_MP4_H264,   "MP4 (H.264 / AAC)",   "mp4",  "libx264",      "h264",  AV_CODEC_ID_H264,   "aac",        AV_CODEC_ID_AAC,       true,  true},
    {MUZZA_FORMAT_MP4_HEVC,   "MP4 (H.265 / AAC)",   "mp4",  "libx265",      "hevc",  AV_CODEC_ID_HEVC,   "aac",        AV_CODEC_ID_AAC,       true,  true},
    {MUZZA_FORMAT_WEBM_VP9,   "WebM (VP9 / Opus)",   "webm", "libvpx-vp9",   "vp9",   AV_CODEC_ID_VP9,    "libopus",    AV_CODEC_ID_OPUS,      true,  true},
    {MUZZA_FORMAT_MOV_PRORES, "MOV (ProRes / PCM)",  "mov",  "prores",       NULL,    AV_CODEC_ID_PRORES, "pcm_s24le",  AV_CODEC_ID_PCM_S24LE, true,  true},
    {MUZZA_FORMAT_MKV,        "MKV (H.264 / FLAC)",  "mkv",  "libx264",      "h264",  AV_CODEC_ID_H264,   "flac",       AV_CODEC_ID_FLAC,      true,  true},
    {MUZZA_FORMAT_MP3,        "MP3 (Audio only)",    "mp3",  NULL,           NULL,    AV_CODEC_ID_NONE,   "libmp3lame", AV_CODEC_ID_MP3,       false, true},
    {MUZZA_FORMAT_WAV,        "WAV (Audio only)",    "wav",  NULL,           NULL,    AV_CODEC_ID_NONE,   "pcm_s16le",  AV_CODEC_ID_PCM_S16LE, false, true},
};

static const char* PRESET_NAMES[] = {
    "ultrafast", "superfast", "veryfast", "faster",
    "fast", "medium", "slow", "slower", "veryslow"
};
#define NUM_PRESETS (sizeof(PRESET_NAMES) / sizeof(PRESET_NAMES[0]))

static const format_desc* get_format_desc(muzza_export_format fmt) {
    for (size_t i = 0; i < sizeof(FORMATS) / sizeof(FORMATS[0]); ++i) {
        if (FORMATS[i].format == fmt) return &FORMATS[i];
    }
    return &FORMATS[0];
}

/* -------------------------------------------------------------------------- */
/* Export snapshot: immutable copy of project data captured at export start.  */
/* -------------------------------------------------------------------------- */
typedef struct {
    muzza_project proj;
    muzza_media* media_pool;
    muzza_clip* clips;
} export_snapshot;

static bool snapshot_create(export_snapshot* snap, const muzza_project* proj) {
    if (!snap || !proj) return false;
    memset(snap, 0, sizeof(*snap));
    snap->proj.duration = proj->duration;
    snap->proj.num_media = proj->num_media;
    snap->proj.num_clips = proj->num_clips;

    if (proj->num_media > 0) {
        snap->media_pool = calloc(proj->num_media, sizeof(*snap->media_pool));
        if (!snap->media_pool) return false;
        for (size_t i = 0; i < proj->num_media; ++i) {
            snap->media_pool[i] = proj->media_pool[i];
            snap->media_pool[i].dec = NULL;
            snap->media_pool[i].waveform.mins = NULL;
            snap->media_pool[i].waveform.maxs = NULL;
            snap->media_pool[i].waveform.num_peaks = 0;
        }
        snap->proj.media_pool = snap->media_pool;
    }

    if (proj->num_clips > 0) {
        snap->clips = calloc(proj->num_clips, sizeof(*snap->clips));
        if (!snap->clips) {
            free(snap->media_pool);
            snap->media_pool = NULL;
            return false;
        }
        for (size_t i = 0; i < proj->num_clips; ++i) {
            snap->clips[i] = proj->clips[i];
            snap->clips[i].dec = NULL;
        }
        snap->proj.clips = snap->clips;
    }

    return true;
}

static void snapshot_destroy(export_snapshot* snap) {
    if (!snap) return;
    free(snap->media_pool);
    free(snap->clips);
    memset(snap, 0, sizeof(*snap));
}

/* -------------------------------------------------------------------------- */
/* Per-source-media lightweight decoder used only by the export pipeline.     */
/* -------------------------------------------------------------------------- */
typedef struct {
    int media_id;
    bool for_video; /* true = video-only instance, false = audio-only instance */
    char filepath[512];
    AVFormatContext* fmt_ctx;
    AVCodecContext* video_ctx;
    AVCodecContext* audio_ctx;
    int video_idx;
    int audio_idx;
    AVFrame* frame;
    AVPacket* pkt;
    struct SwrContext* audio_swr;
    double duration;
    bool has_video;
    bool has_audio;

    /* Buffered audio output: FLTP, 48kHz, stereo */
    float* audio_buf[EXPORT_AUDIO_CH];
    int audio_buf_samples;
    int audio_buf_cap;

    /* Cursor tracking for sequential read (seconds, in source media time) */
    double audio_cursor;
} export_src_decoder;

/* -------------------------------------------------------------------------- */
/* Main exporter state                                                        */
/* -------------------------------------------------------------------------- */
struct muzza_exporter {
    export_snapshot snapshot;
    muzza_export_settings settings;
    char output_path[512];

    volatile muzza_export_status status;
    volatile double progress;
    volatile bool cancel_requested;
    char error_msg[256];

    /* Output muxer / encoders */
    AVFormatContext* out_fmt;
    AVCodecContext* venc;
    AVCodecContext* aenc;
    AVStream* vstream;
    AVStream* astream;

    /* Video encode frame (YUV420P) */
    AVFrame* vframe;
    struct SwsContext* vsws;
    int out_w;
    int out_h;
    int64_t vpts;

    /* Audio encode frame (FLTP) */
    AVFrame* aframe;
    int64_t apts;

    /* Mixing buffer: planar float [channel][sample] */
    float* mix_buf[EXPORT_AUDIO_CH];
    int mix_cap;

    /* Playback tracking to avoid seeking every frame */
    int current_vclip_idx;
    double current_vclip_media_time;

    /* Source decoder cache */
    export_src_decoder* srcs;
    int num_srcs;
    int src_cap;
};

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */
static void set_error(muzza_exporter* exp, const char* ctx, int errnum) {
    char av_err[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errnum, av_err, sizeof(av_err));
    snprintf(exp->error_msg, sizeof(exp->error_msg), "%s: %s", ctx, av_err);
    exp->status = MUZZA_EXPORT_ERROR;
}

static void set_error_str(muzza_exporter* exp, const char* msg) {
    snprintf(exp->error_msg, sizeof(exp->error_msg), "%s", msg);
    exp->status = MUZZA_EXPORT_ERROR;
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

/* -------------------------------------------------------------------------- */
/* Source decoder cache                                                       */
/* -------------------------------------------------------------------------- */
static void src_dec_close(export_src_decoder* dec) {
    if (!dec) return;
    if (dec->audio_swr) swr_free(&dec->audio_swr);
    if (dec->frame) av_frame_free(&dec->frame);
    if (dec->pkt) av_packet_free(&dec->pkt);
    if (dec->video_ctx) avcodec_free_context(&dec->video_ctx);
    if (dec->audio_ctx) avcodec_free_context(&dec->audio_ctx);
    if (dec->fmt_ctx) avformat_close_input(&dec->fmt_ctx);
    for (int c = 0; c < EXPORT_AUDIO_CH; ++c) {
        free(dec->audio_buf[c]);
        dec->audio_buf[c] = NULL;
    }
    dec->audio_buf_samples = 0;
    dec->audio_buf_cap = 0;
    dec->audio_cursor = -1.0;
    memset(dec, 0, sizeof(*dec));
}

static bool src_dec_open(export_src_decoder* dec, const char* filepath, bool for_video) {
    const AVCodec* vcodec = NULL;
    const AVCodec* acodec = NULL;
    int ret = 0;

    memset(dec, 0, sizeof(*dec));
    dec->for_video = for_video;
    snprintf(dec->filepath, sizeof(dec->filepath), "%s", filepath);

    ret = avformat_open_input(&dec->fmt_ctx, filepath, NULL, NULL);
    if (ret < 0) return false;

    ret = avformat_find_stream_info(dec->fmt_ctx, NULL);
    if (ret < 0) {
        src_dec_close(dec);
        return false;
    }

    if (for_video) {
        dec->video_idx = av_find_best_stream(dec->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &vcodec, 0);
        if (dec->video_idx >= 0 && vcodec) {
            dec->video_ctx = avcodec_alloc_context3(vcodec);
            if (!dec->video_ctx) goto fail;
            if (avcodec_parameters_to_context(dec->video_ctx, dec->fmt_ctx->streams[dec->video_idx]->codecpar) < 0) goto fail;
            if (avcodec_open2(dec->video_ctx, vcodec, NULL) < 0) goto fail;
            dec->has_video = true;
        }
    } else {
        dec->audio_idx = av_find_best_stream(dec->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &acodec, 0);
        if (dec->audio_idx >= 0 && acodec) {
            dec->audio_ctx = avcodec_alloc_context3(acodec);
            if (!dec->audio_ctx) goto fail;
            if (avcodec_parameters_to_context(dec->audio_ctx, dec->fmt_ctx->streams[dec->audio_idx]->codecpar) < 0) goto fail;
            if (avcodec_open2(dec->audio_ctx, acodec, NULL) < 0) goto fail;

            AVChannelLayout out_layout;
            av_channel_layout_default(&out_layout, EXPORT_AUDIO_CH);
            ret = swr_alloc_set_opts2(
                &dec->audio_swr,
                &out_layout, AV_SAMPLE_FMT_FLTP, EXPORT_AUDIO_RATE,
                &dec->audio_ctx->ch_layout, dec->audio_ctx->sample_fmt, dec->audio_ctx->sample_rate,
                0, NULL);
            av_channel_layout_uninit(&out_layout);
            if (ret < 0 || !dec->audio_swr) goto fail;
            if (swr_init(dec->audio_swr) < 0) goto fail;
            dec->has_audio = true;
        }
    }

    dec->frame = av_frame_alloc();
    dec->pkt = av_packet_alloc();
    if (!dec->frame || !dec->pkt) goto fail;

    if (dec->fmt_ctx->duration > 0) {
        dec->duration = dec->fmt_ctx->duration / (double)AV_TIME_BASE;
    }
    return true;

fail:
    src_dec_close(dec);
    return false;
}

static export_src_decoder* get_src_dec(muzza_exporter* exp, int media_id, bool for_video) {
    for (int i = 0; i < exp->num_srcs; ++i) {
        if (exp->srcs[i].media_id == media_id && exp->srcs[i].for_video == for_video) return &exp->srcs[i];
    }
    return NULL;
}

static export_src_decoder* ensure_src_dec_video(muzza_exporter* exp, int media_id) {
    export_src_decoder* dec = get_src_dec(exp, media_id, true);
    if (dec) return dec;

    muzza_media* media = project_get_media((muzza_project*)&exp->snapshot.proj, media_id);
    if (!media) return NULL;

    if (exp->num_srcs >= exp->src_cap) {
        int new_cap = exp->src_cap < 4 ? 4 : exp->src_cap * 2;
        export_src_decoder* new_arr = realloc(exp->srcs, sizeof(export_src_decoder) * (size_t)new_cap);
        if (!new_arr) return NULL;
        exp->srcs = new_arr;
        exp->src_cap = new_cap;
    }

    dec = &exp->srcs[exp->num_srcs];
    if (!src_dec_open(dec, media->filepath, true)) {
        return NULL;
    }
    dec->media_id = media_id;
    exp->num_srcs++;
    return dec;
}

static export_src_decoder* ensure_src_dec_audio(muzza_exporter* exp, int media_id) {
    export_src_decoder* dec = get_src_dec(exp, media_id, false);
    if (dec) return dec;

    muzza_media* media = project_get_media((muzza_project*)&exp->snapshot.proj, media_id);
    if (!media) return NULL;

    if (exp->num_srcs >= exp->src_cap) {
        int new_cap = exp->src_cap < 4 ? 4 : exp->src_cap * 2;
        export_src_decoder* new_arr = realloc(exp->srcs, sizeof(export_src_decoder) * (size_t)new_cap);
        if (!new_arr) return NULL;
        exp->srcs = new_arr;
        exp->src_cap = new_cap;
    }

    dec = &exp->srcs[exp->num_srcs];
    if (!src_dec_open(dec, media->filepath, false)) {
        return NULL;
    }
    dec->media_id = media_id;
    exp->num_srcs++;
    return dec;
}

/* -------------------------------------------------------------------------- */
/* Source decoder: video read                                                 */
/* -------------------------------------------------------------------------- */
static bool src_dec_seek(export_src_decoder* dec, double time_seconds) {
    if (!dec || !dec->fmt_ctx) return false;
    if (time_seconds < 0) time_seconds = 0;
    if (dec->duration > 0 && time_seconds > dec->duration) time_seconds = dec->duration;

    int64_t ts = (int64_t)(time_seconds * AV_TIME_BASE);
    if (av_seek_frame(dec->fmt_ctx, -1, ts, AVSEEK_FLAG_BACKWARD) < 0) return false;

    if (dec->video_ctx) avcodec_flush_buffers(dec->video_ctx);
    if (dec->audio_ctx) avcodec_flush_buffers(dec->audio_ctx);

    /* Clear any buffered audio from previous position */
    dec->audio_buf_samples = 0;
    dec->audio_cursor = -1.0;

    return true;
}

static bool src_dec_read_video(export_src_decoder* dec, AVFrame* out_frame) {
    int ret = 0;
    if (!dec || dec->video_idx < 0 || !dec->video_ctx) return false;

    while ((ret = av_read_frame(dec->fmt_ctx, dec->pkt)) >= 0) {
        if (dec->pkt->stream_index == dec->video_idx) {
            ret = avcodec_send_packet(dec->video_ctx, dec->pkt);
            av_packet_unref(dec->pkt);
            if (ret < 0 && ret != AVERROR(EAGAIN)) return false;

            ret = avcodec_receive_frame(dec->video_ctx, out_frame);
            if (ret == 0) return true;
            if (ret == AVERROR(EAGAIN)) continue;
            return false;
        }
        av_packet_unref(dec->pkt);
    }
    return false;
}

/* -------------------------------------------------------------------------- */
/* Source decoder: audio read                                                 */
/* -------------------------------------------------------------------------- */
static bool src_dec_refill_audio(export_src_decoder* dec) {
    int ret = 0;
    if (!dec || !dec->audio_ctx || !dec->audio_swr) return false;

    int target = dec->audio_buf_samples + 2048;
    /* Decode packets until buffer grows by at least 2048 samples or EOF */
    while (dec->audio_buf_samples < target) {
        ret = av_read_frame(dec->fmt_ctx, dec->pkt);
        if (ret < 0) break; /* EOF */

        if (dec->pkt->stream_index == dec->audio_idx) {
            ret = avcodec_send_packet(dec->audio_ctx, dec->pkt);
            av_packet_unref(dec->pkt);
            if (ret < 0 && ret != AVERROR(EAGAIN)) return false;

            while ((ret = avcodec_receive_frame(dec->audio_ctx, dec->frame)) == 0) {
                int out_samples = (int)av_rescale_rnd(
                    swr_get_delay(dec->audio_swr, dec->audio_ctx->sample_rate) + dec->frame->nb_samples,
                    EXPORT_AUDIO_RATE, dec->audio_ctx->sample_rate, AV_ROUND_UP);

                if (out_samples <= 0) continue;

                /* Grow buffer if needed */
                int needed = dec->audio_buf_samples + out_samples;
                if (needed > dec->audio_buf_cap) {
                    int new_cap = needed < 4096 ? 4096 : needed * 2;
                    for (int c = 0; c < EXPORT_AUDIO_CH; ++c) {
                        float* nb = realloc(dec->audio_buf[c], sizeof(float) * (size_t)new_cap);
                        if (!nb) return false;
                        dec->audio_buf[c] = nb;
                    }
                    dec->audio_buf_cap = new_cap;
                }

                uint8_t* out_planes[EXPORT_AUDIO_CH] = {
                    (uint8_t*)(dec->audio_buf[0] + dec->audio_buf_samples),
                    (uint8_t*)(dec->audio_buf[1] + dec->audio_buf_samples),
                };
                int converted = swr_convert(dec->audio_swr, out_planes, out_samples,
                    (const uint8_t**)dec->frame->data, dec->frame->nb_samples);
                if (converted > 0) {
                    dec->audio_buf_samples += converted;
                }
            }
        } else {
            av_packet_unref(dec->pkt);
        }
    }
    return true;
}

static int src_dec_consume_audio(export_src_decoder* dec, float* out_planes[EXPORT_AUDIO_CH], int requested) {
    if (!dec || !dec->has_audio) return 0;

    if (dec->audio_buf_samples < requested) {
        src_dec_refill_audio(dec);
    }

    int give = dec->audio_buf_samples < requested ? dec->audio_buf_samples : requested;
    if (give > 0) {
        for (int c = 0; c < EXPORT_AUDIO_CH; ++c) {
            memcpy(out_planes[c], dec->audio_buf[c], sizeof(float) * (size_t)give);
            /* Shift remaining samples left */
            int remain = dec->audio_buf_samples - give;
            if (remain > 0) {
                memmove(dec->audio_buf[c], dec->audio_buf[c] + give, sizeof(float) * (size_t)remain);
            }
        }
        dec->audio_buf_samples -= give;
    }
    return give;
}


/* -------------------------------------------------------------------------- */
/* Output encoder setup                                                       */
/* -------------------------------------------------------------------------- */
static bool init_video_encoder(muzza_exporter* exp) {
    const format_desc* fmt = get_format_desc(exp->settings.format);
    if (!fmt || !fmt->has_video || !exp->settings.include_video) {
        return true; /* no video requested, skip */
    }

    const AVCodec* codec = NULL;
    if (fmt->video_codec_name) {
        codec = avcodec_find_encoder_by_name(fmt->video_codec_name);
    }
    if (!codec && fmt->video_codec_fb) {
        codec = avcodec_find_encoder_by_name(fmt->video_codec_fb);
    }
    if (!codec && fmt->video_codec_id != AV_CODEC_ID_NONE) {
        codec = avcodec_find_encoder(fmt->video_codec_id);
    }
    if (!codec) {
        set_error_str(exp, "No suitable video encoder found for selected format");
        return false;
    }

    exp->venc = avcodec_alloc_context3(codec);
    if (!exp->venc) {
        set_error_str(exp, "Failed to alloc video encoder context");
        return false;
    }

    exp->venc->width = exp->settings.width;
    exp->venc->height = exp->settings.height;
    exp->venc->time_base = (AVRational){1, (int)exp->settings.fps};
    exp->venc->framerate = (AVRational){(int)exp->settings.fps, 1};
    exp->venc->pix_fmt = AV_PIX_FMT_YUV420P;
    exp->venc->gop_size = 30;
    exp->venc->max_b_frames = 2;

    if (exp->out_fmt->oformat->flags & AVFMT_GLOBALHEADER) {
        exp->venc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    /* Apply codec-specific quality settings */
    if (strcmp(codec->name, "libx264") == 0 || strcmp(codec->name, "libx265") == 0) {
        int p = exp->settings.preset;
        if (p < 0) p = 0;
        if (p >= (int)NUM_PRESETS) p = (int)NUM_PRESETS - 1;
        av_opt_set(exp->venc->priv_data, "preset", PRESET_NAMES[p], 0);
        av_opt_set(exp->venc->priv_data, "tune", "fastdecode", 0);

        if (exp->settings.video_bitrate > 0) {
            exp->venc->bit_rate = (int64_t)exp->settings.video_bitrate * 1000;
            exp->venc->rc_min_rate = exp->venc->bit_rate;
            exp->venc->rc_max_rate = exp->venc->bit_rate;
            exp->venc->bit_rate_tolerance = exp->venc->bit_rate / 10;
            exp->venc->rc_buffer_size = exp->venc->bit_rate;
        } else {
            char crf_str[8];
            int crf = exp->settings.crf;
            if (crf < 0) crf = 0;
            if (crf > 51) crf = 51;
            snprintf(crf_str, sizeof(crf_str), "%d", crf);
            av_opt_set(exp->venc->priv_data, "crf", crf_str, 0);
        }
    } else if (strcmp(codec->name, "libvpx-vp9") == 0) {
        av_opt_set(exp->venc->priv_data, "deadline", "good", 0);
        av_opt_set(exp->venc->priv_data, "cpu-used", "2", 0);
        if (exp->settings.video_bitrate > 0) {
            exp->venc->bit_rate = (int64_t)exp->settings.video_bitrate * 1000;
        } else {
            char crf_str[8];
            int crf = exp->settings.crf;
            if (crf < 0) crf = 23;
            if (crf > 63) crf = 63;
            snprintf(crf_str, sizeof(crf_str), "%d", crf);
            av_opt_set(exp->venc->priv_data, "crf", crf_str, 0);
            exp->venc->bit_rate = 0;
        }
    } else if (strstr(codec->name, "prores") != NULL) {
        av_opt_set(exp->venc->priv_data, "profile", "standard", 0);
        exp->venc->bit_rate = 0;
    } else {
        if (exp->settings.video_bitrate > 0) {
            exp->venc->bit_rate = (int64_t)exp->settings.video_bitrate * 1000;
        }
    }

    if (avcodec_open2(exp->venc, codec, NULL) < 0) {
        set_error_str(exp, "Failed to open video encoder");
        return false;
    }

    exp->vstream = avformat_new_stream(exp->out_fmt, NULL);
    if (!exp->vstream) {
        set_error_str(exp, "Failed to create video stream");
        return false;
    }

    if (avcodec_parameters_from_context(exp->vstream->codecpar, exp->venc) < 0) {
        set_error_str(exp, "Failed to copy video encoder params");
        return false;
    }
    exp->vstream->time_base = exp->venc->time_base;
    exp->vstream->avg_frame_rate = exp->venc->framerate;

    /* Allocate YUV420P frame */
    exp->vframe = av_frame_alloc();
    if (!exp->vframe) {
        set_error_str(exp, "Failed to alloc video frame");
        return false;
    }
    exp->vframe->format = AV_PIX_FMT_YUV420P;
    exp->vframe->width = exp->out_w;
    exp->vframe->height = exp->out_h;
    if (av_frame_get_buffer(exp->vframe, 0) < 0) {
        set_error_str(exp, "Failed to alloc video frame buffer");
        return false;
    }

    return true;
}

static bool init_audio_encoder(muzza_exporter* exp) {
    const format_desc* fmt = get_format_desc(exp->settings.format);
    if (!fmt || !fmt->has_audio || !exp->settings.include_audio) {
        return true; /* no audio requested, skip */
    }

    const AVCodec* codec = NULL;
    if (fmt->audio_codec_name) {
        codec = avcodec_find_encoder_by_name(fmt->audio_codec_name);
    }
    if (!codec && fmt->audio_codec_id != AV_CODEC_ID_NONE) {
        codec = avcodec_find_encoder(fmt->audio_codec_id);
    }
    if (!codec) {
        set_error_str(exp, "Audio encoder not available for selected format");
        return false;
    }

    exp->aenc = avcodec_alloc_context3(codec);
    if (!exp->aenc) {
        set_error_str(exp, "Failed to alloc audio encoder context");
        return false;
    }

    exp->aenc->sample_rate = exp->settings.audio_sample_rate;
    av_channel_layout_default(&exp->aenc->ch_layout, exp->settings.audio_channels);

    if (strcmp(codec->name, "pcm_s16le") == 0) {
        exp->aenc->sample_fmt = AV_SAMPLE_FMT_S16;
    } else if (strcmp(codec->name, "pcm_s24le") == 0) {
        exp->aenc->sample_fmt = AV_SAMPLE_FMT_S32;
    } else {
        exp->aenc->sample_fmt = AV_SAMPLE_FMT_FLTP;
    }

    exp->aenc->time_base = (AVRational){1, exp->aenc->sample_rate};

    if (exp->settings.audio_bitrate > 0 &&
        fmt->format != MUZZA_FORMAT_WAV && fmt->format != MUZZA_FORMAT_MOV_PRORES) {
        exp->aenc->bit_rate = (int64_t)exp->settings.audio_bitrate * 1000;
    }

    if (exp->out_fmt->oformat->flags & AVFMT_GLOBALHEADER) {
        exp->aenc->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    if (avcodec_open2(exp->aenc, codec, NULL) < 0) {
        set_error_str(exp, "Failed to open audio encoder");
        return false;
    }

    exp->astream = avformat_new_stream(exp->out_fmt, NULL);
    if (!exp->astream) {
        set_error_str(exp, "Failed to create audio stream");
        return false;
    }

    if (avcodec_parameters_from_context(exp->astream->codecpar, exp->aenc) < 0) {
        set_error_str(exp, "Failed to copy audio encoder params");
        return false;
    }
    exp->astream->time_base = exp->aenc->time_base;

    /* Allocate frame matching encoder sample format */
    exp->aframe = av_frame_alloc();
    if (!exp->aframe) {
        set_error_str(exp, "Failed to alloc audio frame");
        return false;
    }
    exp->aframe->format = exp->aenc->sample_fmt;
    av_channel_layout_default(&exp->aframe->ch_layout, EXPORT_AUDIO_CH);
    exp->aframe->sample_rate = EXPORT_AUDIO_RATE;
    exp->aframe->nb_samples = exp->aenc->frame_size;
    if (av_frame_get_buffer(exp->aframe, 0) < 0) {
        set_error_str(exp, "Failed to alloc audio frame buffer");
        return false;
    }

    /* Mix buffer: always planar float for internal mixing */
    exp->mix_cap = exp->aenc->frame_size > 0 ? exp->aenc->frame_size : 1024;
    for (int c = 0; c < EXPORT_AUDIO_CH; ++c) {
        exp->mix_buf[c] = calloc((size_t)exp->mix_cap, sizeof(float));
        if (!exp->mix_buf[c]) {
            set_error_str(exp, "Failed to alloc audio mix buffer");
            return false;
        }
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/* Write a packet from encoder to output file                                 */
/* -------------------------------------------------------------------------- */
static bool write_packet(muzza_exporter* exp, AVCodecContext* enc, AVStream* st) {
    AVPacket* pkt = av_packet_alloc();
    if (!pkt) return false;

    int ret = avcodec_receive_packet(enc, pkt);
    if (ret == AVERROR(EAGAIN)) {
        av_packet_free(&pkt);
        return true; /* No packet available yet, not an error */
    }
    if (ret == AVERROR_EOF) {
        av_packet_free(&pkt);
        return false; /* Encoder fully drained */
    }
    if (ret < 0) {
        av_packet_free(&pkt);
        set_error(exp, "avcodec_receive_packet", ret);
        return false;
    }

    av_packet_rescale_ts(pkt, enc->time_base, st->time_base);
    pkt->stream_index = st->index;

    ret = av_interleaved_write_frame(exp->out_fmt, pkt);
    av_packet_free(&pkt);
    if (ret < 0) {
        set_error(exp, "av_interleaved_write_frame", ret);
        return false;
    }
    return true;
}

/* -------------------------------------------------------------------------- */
/* Encode one video frame                                                     */
/* -------------------------------------------------------------------------- */
static bool encode_video_frame(muzza_exporter* exp, double timeline_time) {
    int vclip_idx = find_top_video_clip(&exp->snapshot.proj, timeline_time);
    AVFrame* src_frame = av_frame_alloc();
    bool got_frame = false;

    if (!src_frame) {
        set_error_str(exp, "Failed to alloc temp video frame");
        return false;
    }

    if (vclip_idx >= 0) {
        const muzza_clip* clip = &exp->snapshot.proj.clips[vclip_idx];
        double media_time = project_get_clip_media_time(&exp->snapshot.proj, clip, timeline_time);
        export_src_decoder* dec = ensure_src_dec_video(exp, clip->media_id);

        if (dec && dec->has_video) {
            muzza_media* media = project_get_media((muzza_project*)&exp->snapshot.proj, clip->media_id);
            bool is_image = media && media->is_image;

            /* Seek only on clip change or large time jump (>0.5s) */
            if (vclip_idx != exp->current_vclip_idx ||
                fabs(media_time - exp->current_vclip_media_time) > 0.5) {
                src_dec_seek(dec, media_time);
                exp->current_vclip_idx = vclip_idx;
            }
            exp->current_vclip_media_time = media_time;

            /* Decode forward until we get a frame with pts >= media_time */
            int safety = 0;
            while (safety < 300) {
                if (!src_dec_read_video(dec, src_frame)) break;

                if (is_image) {
                    got_frame = true;
                    break;
                }

                double pts = 0.0;
                if (src_frame->pts != AV_NOPTS_VALUE && dec->video_idx >= 0) {
                    pts = src_frame->pts * av_q2d(dec->fmt_ctx->streams[dec->video_idx]->time_base);
                }

                if (pts >= media_time - 0.02) {
                    got_frame = true;
                    break;
                }
                safety++;
            }

            if (got_frame) {
                if (!exp->vsws || src_frame->width != exp->out_w || src_frame->height != exp->out_h) {
                    if (exp->vsws) sws_freeContext(exp->vsws);
                    exp->vsws = sws_getContext(
                        src_frame->width, src_frame->height, (enum AVPixelFormat)src_frame->format,
                        exp->out_w, exp->out_h, AV_PIX_FMT_YUV420P,
                        SWS_BILINEAR, NULL, NULL, NULL);
                }
                if (exp->vsws) {
                    sws_scale(exp->vsws,
                        (const uint8_t* const*)src_frame->data, src_frame->linesize,
                        0, src_frame->height,
                        exp->vframe->data, exp->vframe->linesize);

                    /* Apply fade-to-black */
                    double fade = project_get_clip_fade_opacity(clip, timeline_time);
                    if (fade < 1.0) {
                        for (int y = 0; y < exp->out_h; ++y) {
                            uint8_t* row = exp->vframe->data[0] + y * exp->vframe->linesize[0];
                            for (int x = 0; x < exp->out_w; ++x) {
                                row[x] = (uint8_t)(row[x] * fade);
                            }
                        }
                        /* Desaturate U/V channels proportionally */
                        for (int y = 0; y < exp->out_h / 2; ++y) {
                            uint8_t* u_row = exp->vframe->data[1] + y * exp->vframe->linesize[1];
                            uint8_t* v_row = exp->vframe->data[2] + y * exp->vframe->linesize[2];
                            for (int x = 0; x < exp->out_w / 2; ++x) {
                                u_row[x] = (uint8_t)((u_row[x] - 128) * fade + 128);
                                v_row[x] = (uint8_t)((v_row[x] - 128) * fade + 128);
                            }
                        }
                    }
                } else {
                    got_frame = false;
                }
            }
        }
    }

    if (!got_frame) {
        exp->current_vclip_idx = -1;
        memset(exp->vframe->data[0], 16, (size_t)exp->vframe->linesize[0] * exp->out_h);
        memset(exp->vframe->data[1], 128, (size_t)exp->vframe->linesize[1] * (exp->out_h / 2));
        memset(exp->vframe->data[2], 128, (size_t)exp->vframe->linesize[2] * (exp->out_h / 2));
    }

    exp->vframe->pts = exp->vpts++;
    int ret = avcodec_send_frame(exp->venc, exp->vframe);
    av_frame_free(&src_frame);

    if (ret < 0) {
        set_error(exp, "avcodec_send_frame (video)", ret);
        return false;
    }

    return write_packet(exp, exp->venc, exp->vstream);
}

/* -------------------------------------------------------------------------- */
/* Encode one audio frame                                                     */
/* -------------------------------------------------------------------------- */
static bool encode_audio_frame(muzza_exporter* exp, double timeline_time) {
    int aclip_indices[16];
    int num_aclips = find_active_audio_clips(&exp->snapshot.proj, timeline_time, aclip_indices, 16);

    int frame_size = exp->aenc->frame_size;
    if (frame_size <= 0) frame_size = 1024;

    /* Clear mix buffer */
    for (int c = 0; c < EXPORT_AUDIO_CH; ++c) {
        memset(exp->mix_buf[c], 0, sizeof(float) * (size_t)frame_size);
    }

    float* temp_planes[EXPORT_AUDIO_CH] = {NULL};
    for (int c = 0; c < EXPORT_AUDIO_CH; ++c) {
        temp_planes[c] = calloc((size_t)frame_size, sizeof(float));
    }

    int max_got = 0;
    for (int i = 0; i < num_aclips; ++i) {
        const muzza_clip* clip = &exp->snapshot.proj.clips[aclip_indices[i]];
        export_src_decoder* dec = ensure_src_dec_audio(exp, clip->media_id);
        if (!dec || !dec->has_audio) continue;

        double media_time = project_get_clip_media_time(&exp->snapshot.proj, clip, timeline_time);

        /* Seek only on first use or large time jump (>0.5s) */
        if (dec->audio_cursor < 0.0 || fabs(media_time - dec->audio_cursor) > 0.5) {
            src_dec_seek(dec, media_time);
            /* Decode-and-discard until audio buffer covers media_time */
            int safety = 0;
            while (safety < 500) {
                if (!src_dec_refill_audio(dec)) break;
                /* audio_buf now has some samples starting from seek point */
                if (dec->audio_buf_samples > 0) {
                    /* Roughly estimate current pts from cursor + buffered samples */
                    double buf_dur = (double)dec->audio_buf_samples / EXPORT_AUDIO_RATE;
                    if (dec->audio_cursor + buf_dur >= media_time - 0.05) {
                        break;
                    }
                }
                safety++;
            }
            dec->audio_cursor = media_time;
        }

        int got = src_dec_consume_audio(dec, temp_planes, frame_size);
        if (got > max_got) max_got = got;
        for (int c = 0; c < EXPORT_AUDIO_CH; ++c) {
            for (int s = 0; s < got; ++s) {
                exp->mix_buf[c][s] += temp_planes[c][s];
            }
        }
        /* Clear temp for next clip */
        for (int c = 0; c < EXPORT_AUDIO_CH; ++c) {
            memset(temp_planes[c], 0, sizeof(float) * (size_t)frame_size);
        }

        /* Advance cursor by consumed samples */
        dec->audio_cursor += (double)got / EXPORT_AUDIO_RATE;
    }

    for (int c = 0; c < EXPORT_AUDIO_CH; ++c) {
        free(temp_planes[c]);
    }

    /* Mix gain: attenuate by number of active clips to avoid clipping,
       then apply soft limiting (tanh approximation) for pleasant saturation. */
    float mix_gain = 1.0f;
    if (num_aclips > 1) {
        mix_gain = 1.0f / sqrtf((float)num_aclips);
    }
    for (int c = 0; c < EXPORT_AUDIO_CH; ++c) {
        for (int s = 0; s < frame_size; ++s) {
            float v = exp->mix_buf[c][s] * mix_gain;
            /* Soft clip: tanh-like curve, smoother than hard clipping */
            v = v * (27.0f + v * v) / (27.0f + 9.0f * v * v);
            exp->mix_buf[c][s] = v;
        }
    }

    /* If no audio was produced, pad with silence */
    if (max_got == 0) {
        for (int c = 0; c < EXPORT_AUDIO_CH; ++c) {
            memset(exp->mix_buf[c], 0, sizeof(float) * (size_t)frame_size);
        }
    }

    /* Copy mix buffer into encoder frame */
    if (av_frame_make_writable(exp->aframe) < 0) {
        set_error_str(exp, "av_frame_make_writable (audio) failed");
        return false;
    }
    /* Convert planar-float mix buffer to encoder sample format */
    if (exp->aframe->format == AV_SAMPLE_FMT_FLTP) {
        for (int c = 0; c < EXPORT_AUDIO_CH; ++c) {
            memcpy(exp->aframe->data[c], exp->mix_buf[c], sizeof(float) * (size_t)frame_size);
        }
    } else if (exp->aframe->format == AV_SAMPLE_FMT_S16) {
        for (int c = 0; c < EXPORT_AUDIO_CH; ++c) {
            int16_t* dst = (int16_t*)exp->aframe->data[c];
            for (int s = 0; s < frame_size; ++s) {
                float v = exp->mix_buf[c][s] * 0.98f;
                if (v > 1.0f) v = 1.0f;
                if (v < -1.0f) v = -1.0f;
                dst[s] = (int16_t)(v * 32767.0f);
            }
        }
    } else if (exp->aframe->format == AV_SAMPLE_FMT_S32) {
        for (int c = 0; c < EXPORT_AUDIO_CH; ++c) {
            int32_t* dst = (int32_t*)exp->aframe->data[c];
            for (int s = 0; s < frame_size; ++s) {
                float v = exp->mix_buf[c][s] * 0.98f;
                if (v > 1.0f) v = 1.0f;
                if (v < -1.0f) v = -1.0f;
                dst[s] = (int32_t)(v * 2147483647.0f);
            }
        }
    }
    exp->aframe->pts = exp->apts;
    exp->apts += frame_size;

    int ret = avcodec_send_frame(exp->aenc, exp->aframe);
    if (ret < 0) {
        set_error(exp, "avcodec_send_frame (audio)", ret);
        return false;
    }

    return write_packet(exp, exp->aenc, exp->astream);
}


/* -------------------------------------------------------------------------- */
/* Main export loop (runs on background thread)                               */
/* -------------------------------------------------------------------------- */
static int export_worker(void* userdata) {
    muzza_exporter* exp = (muzza_exporter*)userdata;
    double total_dur = exp->snapshot.proj.duration;
    double vtime = 0.0;
    double atime = 0.0;
    double vstep = 1.0 / exp->settings.fps;
    int aframe_size = exp->aenc->frame_size > 0 ? exp->aenc->frame_size : 1024;
    double astep = (double)aframe_size / exp->settings.audio_sample_rate;

    exp->status = MUZZA_EXPORT_RUNNING;

    while ((vtime < total_dur || atime < total_dur) && exp->status == MUZZA_EXPORT_RUNNING) {
        if (exp->cancel_requested) {
            exp->status = MUZZA_EXPORT_CANCELLED;
            break;
        }

        if (vtime <= atime && vtime < total_dur) {
            if (!encode_video_frame(exp, vtime)) {
                if (exp->status != MUZZA_EXPORT_CANCELLED) {
                    exp->status = MUZZA_EXPORT_ERROR;
                }
                break;
            }
            vtime += vstep;
        } else if (atime < total_dur) {
            if (!encode_audio_frame(exp, atime)) {
                if (exp->status != MUZZA_EXPORT_CANCELLED) {
                    exp->status = MUZZA_EXPORT_ERROR;
                }
                break;
            }
            atime += astep;
        } else {
            break;
        }

        exp->progress = (vtime > atime ? vtime : atime) / total_dur;
        if (exp->progress > 1.0) exp->progress = 1.0;
    }

    /* Flush video encoder */
    if (exp->venc && exp->status != MUZZA_EXPORT_ERROR) {
        avcodec_send_frame(exp->venc, NULL);
        while (write_packet(exp, exp->venc, exp->vstream)) {
            /* Drain packets */
        }
    }

    /* Flush audio encoder */
    if (exp->aenc && exp->status != MUZZA_EXPORT_ERROR) {
        avcodec_send_frame(exp->aenc, NULL);
        while (write_packet(exp, exp->aenc, exp->astream)) {
            /* Drain packets */
        }
    }

    if (exp->out_fmt && exp->status != MUZZA_EXPORT_ERROR && exp->status != MUZZA_EXPORT_CANCELLED) {
        if (av_write_trailer(exp->out_fmt) < 0) {
            set_error_str(exp, "av_write_trailer failed");
        }
    }

    if (exp->status == MUZZA_EXPORT_RUNNING) {
        exp->status = MUZZA_EXPORT_DONE;
        exp->progress = 1.0;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */
muzza_exporter* exporter_create(const muzza_project* project, const char* output_path, const muzza_export_settings* settings) {
    muzza_exporter* exp = NULL;
    int ret = 0;

    if (!project || !output_path || output_path[0] == '\0') return NULL;

    exp = calloc(1, sizeof(*exp));
    if (!exp) return NULL;

    if (settings) {
        exp->settings = *settings;
    } else {
        exp->settings.width = 1920;
        exp->settings.height = 1080;
        exp->settings.fps = 30.0;
        exp->settings.audio_sample_rate = 48000;
        exp->settings.audio_channels = 2;
        exp->settings.include_video = true;
        exp->settings.include_audio = true;
        exp->settings.format = MUZZA_FORMAT_MP4_H264;
        exp->settings.video_bitrate = 0;
        exp->settings.crf = 23;
        exp->settings.audio_bitrate = 128;
        exp->settings.preset = 4; /* medium */
    }

    if (!snapshot_create(&exp->snapshot, project)) {
        free(exp);
        return NULL;
    }
    snprintf(exp->output_path, sizeof(exp->output_path), "%s", output_path);
    exp->status = MUZZA_EXPORT_IDLE;
    exp->progress = 0.0;
    exp->cancel_requested = false;
    exp->current_vclip_idx = -1;
    exp->current_vclip_media_time = -1.0;

    /* Determine output resolution from settings or first video clip */
    exp->out_w = exp->settings.width;
    exp->out_h = exp->settings.height;
    if (exp->out_w <= 0 || exp->out_h <= 0) {
        for (size_t i = 0; i < project->num_clips; ++i) {
            if (project->clips[i].type == MUZZA_CLIP_VIDEO) {
                muzza_media* media = project_get_media((muzza_project*)project, project->clips[i].media_id);
                if (media && media->dec) {
                    int w = 0, h = 0;
                    decoder_get_image(media->dec, &w, &h);
                    if (w > 0 && h > 0) {
                        exp->out_w = w;
                        exp->out_h = h;
                        break;
                    }
                }
                /* Try to open a temp decoder to read dimensions */
                export_src_decoder tmp_dec = {0};
                if (src_dec_open(&tmp_dec, media->filepath, true)) {
                    if (tmp_dec.has_video && tmp_dec.video_ctx) {
                        exp->out_w = tmp_dec.video_ctx->width;
                        exp->out_h = tmp_dec.video_ctx->height;
                    }
                    src_dec_close(&tmp_dec);
                    if (exp->out_w > 0 && exp->out_h > 0) break;
                }
            }
        }
    }

    if (exp->out_w <= 0 || exp->out_h <= 0) {
        exp->out_w = 1920;
        exp->out_h = 1080;
    }

    /* Ensure dimensions are even for YUV420P */
    exp->out_w = (exp->out_w / 2) * 2;
    exp->out_h = (exp->out_h / 2) * 2;

    /* Open output file */
    ret = avformat_alloc_output_context2(&exp->out_fmt, NULL, NULL, output_path);
    if (ret < 0 || !exp->out_fmt) {
        set_error(exp, "avformat_alloc_output_context2", ret);
        goto fail;
    }

    if (!init_video_encoder(exp)) goto fail;
    if (!init_audio_encoder(exp)) goto fail;

    if (!(exp->out_fmt->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&exp->out_fmt->pb, output_path, AVIO_FLAG_WRITE);
        if (ret < 0) {
            set_error(exp, "avio_open", ret);
            goto fail;
        }
    }

    ret = avformat_write_header(exp->out_fmt, NULL);
    if (ret < 0) {
        set_error(exp, "avformat_write_header", ret);
        goto fail;
    }

    return exp;

fail:
    exporter_destroy(exp);
    return NULL;
}

void exporter_destroy(muzza_exporter* exp) {
    if (!exp) return;

    for (int i = 0; i < exp->num_srcs; ++i) {
        src_dec_close(&exp->srcs[i]);
    }
    free(exp->srcs);

    if (exp->vsws) sws_freeContext(exp->vsws);
    if (exp->vframe) av_frame_free(&exp->vframe);
    if (exp->aframe) av_frame_free(&exp->aframe);
    if (exp->venc) avcodec_free_context(&exp->venc);
    if (exp->aenc) avcodec_free_context(&exp->aenc);

    for (int c = 0; c < EXPORT_AUDIO_CH; ++c) {
        free(exp->mix_buf[c]);
    }

    if (exp->out_fmt) {
        if (!(exp->out_fmt->oformat->flags & AVFMT_NOFILE) && exp->out_fmt->pb) {
            avio_closep(&exp->out_fmt->pb);
        }
        avformat_free_context(exp->out_fmt);
    }

    snapshot_destroy(&exp->snapshot);
    free(exp);
}

SDL_Thread* exporter_start_thread(muzza_exporter* exp) {
    if (!exp) return NULL;
    if (exp->status != MUZZA_EXPORT_IDLE) return NULL;

    exp->status = MUZZA_EXPORT_RUNNING;
    exp->progress = 0.0;
    exp->cancel_requested = false;

    SDL_Thread* thread = SDL_CreateThread(export_worker, "export_worker", exp);
    if (!thread) {
        exp->status = MUZZA_EXPORT_ERROR;
        snprintf(exp->error_msg, sizeof(exp->error_msg), "SDL_CreateThread failed");
    }
    return thread;
}

muzza_export_status exporter_get_status(muzza_exporter* exp) {
    if (!exp) return MUZZA_EXPORT_IDLE;
    return exp->status;
}

double exporter_get_progress(muzza_exporter* exp) {
    if (!exp) return 0.0;
    return exp->progress;
}

const char* exporter_get_error(muzza_exporter* exp) {
    if (!exp) return "";
    return exp->error_msg;
}

const char* exporter_format_name(muzza_export_format format) {
    const format_desc* d = get_format_desc(format);
    return d ? d->name : "Unknown";
}

const char* exporter_format_extension(muzza_export_format format) {
    const format_desc* d = get_format_desc(format);
    return d ? d->ext : "mp4";
}

bool exporter_format_has_video(muzza_export_format format) {
    const format_desc* d = get_format_desc(format);
    return d ? d->has_video : true;
}

bool exporter_format_has_audio(muzza_export_format format) {
    const format_desc* d = get_format_desc(format);
    return d ? d->has_audio : true;
}

void exporter_build_default_path(const muzza_project* project, const muzza_export_settings* settings, char* out, size_t out_size) {
    if (!out || out_size == 0) return;

    const char* base = (project && project->filepath[0]) ? project->filepath : "untitled";
    char stem[512] = {0};
    snprintf(stem, sizeof(stem), "%s", base);
    char* dot = strrchr(stem, '.');
    if (dot) *dot = '\0';

    /* Extract just the filename stem (no directory) */
    char* slash = strrchr(stem, '/');
    if (!slash) slash = strrchr(stem, '\\');
    const char* filename_stem = slash ? slash + 1 : stem;

    time_t now = time(NULL);
    struct tm* tm_info = localtime(&now);
    char timestamp[32] = {0};
    if (tm_info) {
        strftime(timestamp, sizeof(timestamp), "%Y%m%d-%H%M%S", tm_info);
    } else {
        snprintf(timestamp, sizeof(timestamp), "unknown");
    }

    const char* ext = exporter_format_extension(settings ? settings->format : MUZZA_FORMAT_MP4_H264);
    int w = settings ? settings->width : 1920;
    int h = settings ? settings->height : 1080;

    /* Determine output directory from project path, or current directory */
    const char* dir = ".";
    char dir_buf[512] = {0};
    if (project && project->filepath[0]) {
        snprintf(dir_buf, sizeof(dir_buf), "%s", project->filepath);
        char* last_slash = strrchr(dir_buf, '/');
        if (!last_slash) last_slash = strrchr(dir_buf, '\\');
        if (last_slash) {
            *last_slash = '\0';
            dir = dir_buf;
        }
    }

    char candidate[2048] = {0};
    snprintf(candidate, sizeof(candidate), "%s/%s_%s_%dx%d.%s",
             dir, filename_stem, timestamp, w, h, ext);

    /* Version increment to avoid overwrite */
    if (access(candidate, F_OK) == 0) {
        for (int v = 2; v < 1000; ++v) {
            char try_path[2048] = {0};
            snprintf(try_path, sizeof(try_path), "%s/%s_%s_%dx%d_v%d.%s",
                     dir, filename_stem, timestamp, w, h, v, ext);
            if (access(try_path, F_OK) != 0) {
                snprintf(candidate, sizeof(candidate), "%s", try_path);
                break;
            }
        }
    }

    snprintf(out, out_size, "%s", candidate);
}

void exporter_request_cancel(muzza_exporter* exp) {
    if (!exp) return;
    exp->cancel_requested = true;
}
