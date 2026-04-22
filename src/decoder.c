#include "decoder.h"

#include <SDL3/SDL.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>
#include <libavutil/avutil.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
#include <stdio.h>
#include <stdlib.h>

struct muzza_decoder {
    fx_context* ctx;
    fx_image* image;

    AVFormatContext* fmt_ctx;
    AVPacket* pkt;

    AVCodecContext* video_ctx;
    int video_idx;
    int width;
    int height;
    AVFrame* frame;
    AVFrame* frame_bgra;
    struct SwsContext* sws_ctx;
    uint8_t* bgra_buf;

    AVCodecContext* audio_ctx;
    int audio_idx;
    AVFrame* audio_frame;
    struct SwrContext* swr_ctx;
    SDL_AudioStream* audio_stream;

    double current_pts;
    double duration;
    bool paused;
    bool is_eof;
};

static void log_av_error(const char* context, int errnum) {
    char error_text[AV_ERROR_MAX_STRING_SIZE] = {0};

    av_strerror(errnum, error_text, sizeof(error_text));
    SDL_Log("%s: %s", context, error_text);
}

static void decoder_disable_audio(muzza_decoder* dec) {
    if (!dec) {
        return;
    }

    if (dec->audio_stream) {
        SDL_DestroyAudioStream(dec->audio_stream);
        dec->audio_stream = NULL;
    }
    if (dec->swr_ctx) {
        swr_free(&dec->swr_ctx);
    }
    if (dec->audio_frame) {
        av_frame_free(&dec->audio_frame);
    }
    if (dec->audio_ctx) {
        avcodec_free_context(&dec->audio_ctx);
    }

    dec->audio_idx = -1;
}

static bool decoder_init_video(muzza_decoder* dec, const AVCodec* codec) {
    AVCodecParameters* params = NULL;
    int buffer_size = 0;

    dec->video_ctx = avcodec_alloc_context3(codec);
    if (!dec->video_ctx) {
        return false;
    }

    params = dec->fmt_ctx->streams[dec->video_idx]->codecpar;
    if (avcodec_parameters_to_context(dec->video_ctx, params) < 0) {
        return false;
    }

    if (avcodec_open2(dec->video_ctx, codec, NULL) < 0) {
        return false;
    }

    dec->width = dec->video_ctx->width;
    dec->height = dec->video_ctx->height;
    if (dec->width <= 0 || dec->height <= 0) {
        return false;
    }

    dec->sws_ctx = sws_getContext(
        dec->width,
        dec->height,
        dec->video_ctx->pix_fmt,
        dec->width,
        dec->height,
        AV_PIX_FMT_BGRA,
        SWS_BILINEAR,
        NULL,
        NULL,
        NULL
    );
    if (!dec->sws_ctx) {
        return false;
    }

    buffer_size = av_image_get_buffer_size(AV_PIX_FMT_BGRA, dec->width, dec->height, 1);
    if (buffer_size <= 0) {
        return false;
    }

    dec->bgra_buf = av_malloc((size_t)buffer_size);
    dec->frame = av_frame_alloc();
    dec->frame_bgra = av_frame_alloc();
    if (!dec->bgra_buf || !dec->frame || !dec->frame_bgra) {
        return false;
    }

    if (av_image_fill_arrays(
            dec->frame_bgra->data,
            dec->frame_bgra->linesize,
            dec->bgra_buf,
            AV_PIX_FMT_BGRA,
            dec->width,
            dec->height,
            1
        ) < 0) {
        return false;
    }

    dec->image = fx_image_create(dec->ctx, &(fx_image_desc) {
        .width = dec->width,
        .height = dec->height,
        .format = FX_FMT_BGRA8_UNORM,
        .data = dec->bgra_buf,
    });

    return dec->image != NULL;
}

static bool decoder_init_audio(muzza_decoder* dec, const AVCodec* codec) {
    AVCodecParameters* params = NULL;
    AVChannelLayout out_layout;
    int ret = 0;

    dec->audio_ctx = avcodec_alloc_context3(codec);
    if (!dec->audio_ctx) {
        SDL_Log("audio init: avcodec_alloc_context3 failed");
        return false;
    }

    params = dec->fmt_ctx->streams[dec->audio_idx]->codecpar;
    ret = avcodec_parameters_to_context(dec->audio_ctx, params);
    if (ret < 0) {
        log_av_error("audio init: avcodec_parameters_to_context failed", ret);
        return false;
    }

    ret = avcodec_open2(dec->audio_ctx, codec, NULL);
    if (ret < 0) {
        log_av_error("audio init: avcodec_open2 failed", ret);
        return false;
    }

    dec->audio_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &(SDL_AudioSpec) {
            .format = SDL_AUDIO_F32,
            .channels = 2,
            .freq = 48000,
        },
        NULL,
        NULL
    );
    if (!dec->audio_stream) {
        SDL_Log("audio init: SDL_OpenAudioDeviceStream failed: %s", SDL_GetError());
        return false;
    }

    av_channel_layout_default(&out_layout, 2);
    ret = swr_alloc_set_opts2(
            &dec->swr_ctx,
            &out_layout,
            AV_SAMPLE_FMT_FLT,
            48000,
            &dec->audio_ctx->ch_layout,
            dec->audio_ctx->sample_fmt,
            dec->audio_ctx->sample_rate,
            0,
            NULL
        );
    if (ret < 0) {
        av_channel_layout_uninit(&out_layout);
        log_av_error("audio init: swr_alloc_set_opts2 failed", ret);
        return false;
    }
    av_channel_layout_uninit(&out_layout);

    if (!dec->swr_ctx) {
        SDL_Log("audio init: swr context allocation failed");
        return false;
    }

    ret = swr_init(dec->swr_ctx);
    if (ret < 0) {
        log_av_error("audio init: swr_init failed", ret);
        return false;
    }

    dec->audio_frame = av_frame_alloc();
    if (!dec->audio_frame) {
        SDL_Log("audio init: av_frame_alloc failed");
    }
    return dec->audio_frame != NULL;
}

static bool decoder_upload_frame(muzza_decoder* dec, AVFrame* frame) {
    int64_t timestamp = frame->best_effort_timestamp;

    sws_scale(
        dec->sws_ctx,
        (const uint8_t* const*)frame->data,
        frame->linesize,
        0,
        dec->height,
        dec->frame_bgra->data,
        dec->frame_bgra->linesize
    );

    if (!fx_image_update(dec->image, dec->bgra_buf, dec->frame_bgra->linesize[0])) {
        return false;
    }

    if (timestamp != AV_NOPTS_VALUE) {
        dec->current_pts = timestamp * av_q2d(dec->fmt_ctx->streams[dec->video_idx]->time_base);
    }

    return true;
}

static bool decoder_process_video_packet(muzza_decoder* dec) {
    int ret = avcodec_send_packet(dec->video_ctx, dec->pkt);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        return false;
    }

    while ((ret = avcodec_receive_frame(dec->video_ctx, dec->frame)) >= 0) {
        return decoder_upload_frame(dec, dec->frame);
    }

    return false;
}

static void decoder_process_audio_packet(muzza_decoder* dec) {
    int ret = 0;

    if (!dec || !dec->audio_ctx || !dec->audio_frame || !dec->swr_ctx || !dec->audio_stream) {
        return;
    }

    ret = avcodec_send_packet(dec->audio_ctx, dec->pkt);
    if (ret < 0 && ret != AVERROR(EAGAIN)) {
        return;
    }

    while ((ret = avcodec_receive_frame(dec->audio_ctx, dec->audio_frame)) >= 0) {
        uint8_t* out_data = NULL;
        int out_linesize = 0;
        int out_samples = av_rescale_rnd(
            swr_get_delay(dec->swr_ctx, dec->audio_ctx->sample_rate) + dec->audio_frame->nb_samples,
            48000,
            dec->audio_ctx->sample_rate,
            AV_ROUND_UP
        );
        int converted = 0;

        if (out_samples <= 0) {
            continue;
        }

        if (av_samples_alloc(&out_data, &out_linesize, 2, out_samples, AV_SAMPLE_FMT_FLT, 0) < 0) {
            continue;
        }

        converted = swr_convert(
            dec->swr_ctx,
            &out_data,
            out_samples,
            (const uint8_t**)dec->audio_frame->data,
            dec->audio_frame->nb_samples
        );
        if (converted > 0) {
            SDL_PutAudioStreamData(dec->audio_stream, out_data, converted * 2 * (int)sizeof(float));
        }

        av_freep(&out_data);
    }
}

static bool decoder_decode_until_video_frame(muzza_decoder* dec, bool queue_audio) {
    int read_result = 0;

    if (!dec || !dec->pkt || dec->video_idx < 0 || !dec->video_ctx) {
        return false;
    }

    while ((read_result = av_read_frame(dec->fmt_ctx, dec->pkt)) >= 0) {
        bool got_frame = false;

        if (dec->pkt->stream_index == dec->video_idx) {
            got_frame = decoder_process_video_packet(dec);
        } else if (queue_audio && dec->audio_ctx && dec->pkt->stream_index == dec->audio_idx) {
            decoder_process_audio_packet(dec);
        }

        av_packet_unref(dec->pkt);
        if (got_frame) {
            return true;
        }
    }

    dec->is_eof = true;
    av_packet_unref(dec->pkt);
    return false;
}

muzza_decoder* decoder_create(fx_context* ctx, const char* filepath) {
    muzza_decoder* dec = NULL;
    const AVCodec* video_codec = NULL;
    const AVCodec* audio_codec = NULL;

    if (!ctx || !filepath || filepath[0] == '\0') {
        return NULL;
    }

    dec = calloc(1, sizeof(*dec));
    if (!dec) {
        return NULL;
    }

    dec->ctx = ctx;
    dec->video_idx = -1;
    dec->audio_idx = -1;
    dec->paused = true;

    if (avformat_open_input(&dec->fmt_ctx, filepath, NULL, NULL) < 0) {
        SDL_Log("decoder create: avformat_open_input failed for %s", filepath);
        decoder_destroy(dec);
        return NULL;
    }

    if (avformat_find_stream_info(dec->fmt_ctx, NULL) < 0) {
        SDL_Log("decoder create: avformat_find_stream_info failed for %s", filepath);
        decoder_destroy(dec);
        return NULL;
    }

    dec->video_idx = av_find_best_stream(dec->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &video_codec, 0);
    dec->audio_idx = av_find_best_stream(dec->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &audio_codec, 0);

    if (dec->video_idx >= 0 && video_codec && !decoder_init_video(dec, video_codec)) {
        SDL_Log("decoder create: video init failed for %s", filepath);
        decoder_destroy(dec);
        return NULL;
    }

    if (dec->audio_idx >= 0 && audio_codec && !decoder_init_audio(dec, audio_codec)) {
        SDL_Log("decoder create: audio init degraded for %s", filepath);
        decoder_disable_audio(dec);
    }

    if (dec->video_idx < 0 && dec->audio_idx < 0) {
        SDL_Log("decoder create: no usable streams for %s", filepath);
        decoder_destroy(dec);
        return NULL;
    }

    dec->pkt = av_packet_alloc();
    if (!dec->pkt) {
        SDL_Log("decoder create: av_packet_alloc failed for %s", filepath);
        decoder_destroy(dec);
        return NULL;
    }

    if (dec->fmt_ctx->duration > 0) {
        dec->duration = dec->fmt_ctx->duration / (double)AV_TIME_BASE;
    }

    if (dec->video_idx >= 0) {
        decoder_seek_to_time(dec, 0.0);
    }

    decoder_set_paused(dec, true);
    return dec;
}

void decoder_destroy(muzza_decoder* dec) {
    if (!dec) {
        return;
    }

    if (dec->audio_stream) {
        SDL_DestroyAudioStream(dec->audio_stream);
    }
    if (dec->swr_ctx) {
        swr_free(&dec->swr_ctx);
    }
    if (dec->audio_frame) {
        av_frame_free(&dec->audio_frame);
    }
    if (dec->audio_ctx) {
        avcodec_free_context(&dec->audio_ctx);
    }

    if (dec->image) {
        fx_image_destroy(dec->image);
    }
    if (dec->bgra_buf) {
        av_free(dec->bgra_buf);
    }
    if (dec->frame_bgra) {
        av_frame_free(&dec->frame_bgra);
    }
    if (dec->frame) {
        av_frame_free(&dec->frame);
    }
    if (dec->sws_ctx) {
        sws_freeContext(dec->sws_ctx);
    }
    if (dec->video_ctx) {
        avcodec_free_context(&dec->video_ctx);
    }

    if (dec->pkt) {
        av_packet_free(&dec->pkt);
    }
    if (dec->fmt_ctx) {
        avformat_close_input(&dec->fmt_ctx);
    }

    free(dec);
}

bool decoder_update(muzza_decoder* dec, double delta_time) {
    const int min_audio_bytes = 48000 * 2 * (int)sizeof(float) / 4;
    double start_pts = 0.0;
    double target_advance = 0.0;
    bool got_frame = false;
    int steps = 0;

    if (!dec || dec->paused || dec->is_eof) {
        return false;
    }

    start_pts = dec->current_pts;
    target_advance = delta_time > 0.0 ? delta_time * 1.25 : (1.0 / 30.0);

    while (steps < 12 && !dec->is_eof) {
        int queued_audio = dec->audio_stream ? SDL_GetAudioStreamQueued(dec->audio_stream) : min_audio_bytes;

        if (got_frame && dec->current_pts >= start_pts + target_advance && queued_audio >= min_audio_bytes) {
            break;
        }

        if (!decoder_decode_until_video_frame(dec, true)) {
            break;
        }

        got_frame = true;
        steps++;
    }

    return got_frame;
}

fx_image* decoder_get_image(muzza_decoder* dec, int* w, int* h) {
    if (!dec) {
        return NULL;
    }

    if (w) {
        *w = dec->width;
    }
    if (h) {
        *h = dec->height;
    }

    return dec->image;
}

float decoder_get_progress(muzza_decoder* dec) {
    if (!dec || dec->duration <= 0.0) {
        return 0.0f;
    }

    if (dec->current_pts <= 0.0) {
        return 0.0f;
    }

    if (dec->current_pts >= dec->duration) {
        return 1.0f;
    }

    return (float)(dec->current_pts / dec->duration);
}

double decoder_get_duration(muzza_decoder* dec) {
    return dec ? dec->duration : 0.0;
}

double decoder_get_time(muzza_decoder* dec) {
    return dec ? dec->current_pts : 0.0;
}

void decoder_seek(muzza_decoder* dec, float progress) {
    if (!dec) {
        return;
    }

    if (progress < 0.0f) {
        progress = 0.0f;
    } else if (progress > 1.0f) {
        progress = 1.0f;
    }

    decoder_seek_to_time(dec, progress * dec->duration);
}

bool decoder_seek_to_time(muzza_decoder* dec, double time_seconds) {
    AVStream* stream = NULL;
    int64_t target_ts = 0;
    bool was_paused = false;

    if (!dec || dec->video_idx < 0 || !dec->fmt_ctx || !dec->video_ctx) {
        return false;
    }

    if (time_seconds < 0.0) {
        time_seconds = 0.0;
    } else if (dec->duration > 0.0 && time_seconds > dec->duration) {
        time_seconds = dec->duration;
    }

    stream = dec->fmt_ctx->streams[dec->video_idx];
    target_ts = av_rescale_q((int64_t)(time_seconds * AV_TIME_BASE), AV_TIME_BASE_Q, stream->time_base);
    if (av_seek_frame(dec->fmt_ctx, dec->video_idx, target_ts, AVSEEK_FLAG_BACKWARD) < 0) {
        SDL_Log("decoder seek failed: target=%.3f", time_seconds);
        return false;
    }

    avcodec_flush_buffers(dec->video_ctx);
    if (dec->audio_ctx) {
        avcodec_flush_buffers(dec->audio_ctx);
    }
    if (dec->audio_stream) {
        SDL_ClearAudioStream(dec->audio_stream);
    }

    dec->is_eof = false;
    dec->current_pts = time_seconds;

    was_paused = dec->paused;
    dec->paused = false;
    if (!decoder_decode_until_video_frame(dec, false)) {
        dec->current_pts = time_seconds;
    }
    dec->paused = was_paused;

    if (dec->audio_stream) {
        SDL_ClearAudioStream(dec->audio_stream);
    }

    return true;
}

void decoder_set_paused(muzza_decoder* dec, bool paused) {
    if (!dec) {
        return;
    }

    dec->paused = paused;
    if (dec->audio_stream) {
        if (paused) {
            SDL_PauseAudioStreamDevice(dec->audio_stream);
        } else {
            SDL_ResumeAudioStreamDevice(dec->audio_stream);
        }
    }
}

bool decoder_is_paused(muzza_decoder* dec) {
    return dec ? dec->paused : true;
}

bool decoder_has_video(muzza_decoder* dec) {
    return dec && dec->video_idx >= 0 && dec->image != NULL;
}

bool decoder_has_audio(muzza_decoder* dec) {
    return dec && dec->audio_idx >= 0 && dec->audio_ctx != NULL;
}
