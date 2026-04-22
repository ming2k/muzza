#include "decoder.h"
#include <stdio.h>
#include <stdlib.h>
#include <SDL3/SDL.h>

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>

struct muzza_decoder {
    fx_context* ctx;
    fx_image* image;

    AVFormatContext* fmt_ctx;
    
    // Video
    AVCodecContext* video_ctx;
    int video_stream_index;
    AVFrame* frame;
    AVFrame* frame_bgra;
    struct SwsContext* sws_ctx;
    uint8_t* bgra_buffer;

    // Audio
    AVCodecContext* audio_ctx;
    int audio_stream_index;
    AVFrame* audio_frame;
    struct SwrContext* swr_ctx;
    SDL_AudioStream* audio_stream;

    AVPacket* pkt;
    int width, height;
    double current_pts;
    double duration;
};

muzza_decoder* decoder_create(fx_context* ctx, const char* filepath) {
    muzza_decoder* dec = calloc(1, sizeof(muzza_decoder));
    if (!dec) return NULL;
    
    dec->ctx = ctx;
    dec->video_stream_index = -1;
    dec->audio_stream_index = -1;

    if (avformat_open_input(&dec->fmt_ctx, filepath, NULL, NULL) < 0) {
        fprintf(stderr, "Failed to open video file: %s\n", filepath);
        decoder_destroy(dec);
        return NULL;
    }

    if (avformat_find_stream_info(dec->fmt_ctx, NULL) < 0) {
        decoder_destroy(dec);
        return NULL;
    }

    const AVCodec* v_codec = NULL;
    const AVCodec* a_codec = NULL;
    for (unsigned int i = 0; i < dec->fmt_ctx->nb_streams; i++) {
        AVCodecParameters* p = dec->fmt_ctx->streams[i]->codecpar;
        if (p->codec_type == AVMEDIA_TYPE_VIDEO && dec->video_stream_index == -1) {
            dec->video_stream_index = i;
            v_codec = avcodec_find_decoder(p->codec_id);
        } else if (p->codec_type == AVMEDIA_TYPE_AUDIO && dec->audio_stream_index == -1) {
            dec->audio_stream_index = i;
            a_codec = avcodec_find_decoder(p->codec_id);
        }
    }

    if (v_codec) {
        dec->video_ctx = avcodec_alloc_context3(v_codec);
        avcodec_parameters_to_context(dec->video_ctx, dec->fmt_ctx->streams[dec->video_stream_index]->codecpar);
        avcodec_open2(dec->video_ctx, v_codec, NULL);
        dec->width = dec->video_ctx->width;
        dec->height = dec->video_ctx->height;
        
        dec->sws_ctx = sws_getContext(dec->width, dec->height, dec->video_ctx->pix_fmt,
                                      dec->width, dec->height, AV_PIX_FMT_BGRA,
                                      SWS_BILINEAR, NULL, NULL, NULL);
        
        int num_bytes = av_image_get_buffer_size(AV_PIX_FMT_BGRA, dec->width, dec->height, 1);
        dec->bgra_buffer = av_malloc(num_bytes);
        dec->frame = av_frame_alloc();
        dec->frame_bgra = av_frame_alloc();
        av_image_fill_arrays(dec->frame_bgra->data, dec->frame_bgra->linesize, dec->bgra_buffer, AV_PIX_FMT_BGRA, dec->width, dec->height, 1);
        
        fx_image_desc d = { .width = dec->width, .height = dec->height, .format = FX_FMT_BGRA8_UNORM, .data = dec->bgra_buffer };
        dec->image = fx_image_create(ctx, &d);
    }

    if (a_codec) {
        dec->audio_ctx = avcodec_alloc_context3(a_codec);
        avcodec_parameters_to_context(dec->audio_ctx, dec->fmt_ctx->streams[dec->audio_stream_index]->codecpar);
        avcodec_open2(dec->audio_ctx, a_codec, NULL);

        SDL_AudioSpec spec = { .format = SDL_AUDIO_F32, .channels = 2, .freq = 48000 };
        dec->audio_stream = SDL_CreateAudioStream(&spec, &spec);
        SDL_BindAudioStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, dec->audio_stream);

        AVChannelLayout out_ch_layout;
        av_channel_layout_default(&out_ch_layout, 2);
        
        swr_alloc_set_opts2(&dec->swr_ctx,
                           &out_ch_layout, AV_SAMPLE_FMT_FLT, 48000,
                           &dec->audio_ctx->ch_layout, dec->audio_ctx->sample_fmt, dec->audio_ctx->sample_rate,
                           0, NULL);
        swr_init(dec->swr_ctx);
        dec->audio_frame = av_frame_alloc();
    }

    dec->pkt = av_packet_alloc();
    if (dec->video_stream_index != -1) {
        AVStream *st = dec->fmt_ctx->streams[dec->video_stream_index];
        dec->duration = st->duration * av_q2d(st->time_base);
    }

    return dec;
}

void decoder_destroy(muzza_decoder* dec) {
    if (!dec) return;
    if (dec->image) fx_image_destroy(dec->image);
    if (dec->bgra_buffer) av_free(dec->bgra_buffer);
    if (dec->frame_bgra) av_frame_free(&dec->frame_bgra);
    if (dec->frame) av_frame_free(&dec->frame);
    if (dec->audio_frame) av_frame_free(&dec->audio_frame);
    if (dec->pkt) av_packet_free(&dec->pkt);
    if (dec->video_ctx) avcodec_free_context(&dec->video_ctx);
    if (dec->audio_ctx) avcodec_free_context(&dec->audio_ctx);
    if (dec->fmt_ctx) avformat_close_input(&dec->fmt_ctx);
    if (dec->swr_ctx) swr_free(&dec->swr_ctx);
    if (dec->sws_ctx) sws_freeContext(dec->sws_ctx);
    if (dec->audio_stream) SDL_DestroyAudioStream(dec->audio_stream);
    free(dec);
}

bool decoder_read_frame(muzza_decoder* dec) {
    while (av_read_frame(dec->fmt_ctx, dec->pkt) >= 0) {
        if (dec->pkt->stream_index == dec->video_stream_index) {
            avcodec_send_packet(dec->video_ctx, dec->pkt);
            if (avcodec_receive_frame(dec->video_ctx, dec->frame) == 0) {
                dec->current_pts = dec->frame->pts * av_q2d(dec->fmt_ctx->streams[dec->video_stream_index]->time_base);
                sws_scale(dec->sws_ctx, (uint8_t const * const *)dec->frame->data, dec->frame->linesize, 0, dec->height, dec->frame_bgra->data, dec->frame_bgra->linesize);
                fx_image_update(dec->image, dec->bgra_buffer, dec->frame_bgra->linesize[0]);
                av_packet_unref(dec->pkt);
                return true;
            }
        } else if (dec->pkt->stream_index == dec->audio_stream_index) {
            avcodec_send_packet(dec->audio_ctx, dec->pkt);
            if (avcodec_receive_frame(dec->audio_ctx, dec->audio_frame) == 0) {
                uint8_t* out_data[2];
                int out_samples = av_rescale_rnd(swr_get_delay(dec->swr_ctx, dec->audio_ctx->sample_rate) + dec->audio_frame->nb_samples, 48000, dec->audio_ctx->sample_rate, AV_ROUND_UP);
                av_samples_alloc(out_data, NULL, 2, out_samples, AV_SAMPLE_FMT_FLT, 0);
                int converted = swr_convert(dec->swr_ctx, out_data, out_samples, (const uint8_t**)dec->audio_frame->data, dec->audio_frame->nb_samples);
                SDL_PutAudioStreamData(dec->audio_stream, out_data[0], converted * 2 * sizeof(float));
                av_freep(&out_data[0]);
            }
        }
        av_packet_unref(dec->pkt);
    }
    return false;
}

float decoder_get_progress(muzza_decoder* dec) {
    if (!dec || dec->duration <= 0) return 0.0f;
    return (float)(dec->current_pts / dec->duration);
}

fx_image* decoder_get_image(muzza_decoder* dec, int* width, int* height) {
    if (width) *width = dec->width;
    if (height) *height = dec->height;
    return dec->image;
}

void decoder_seek(muzza_decoder* dec, float progress) {
    if (!dec || dec->video_stream_index == -1) return;
    int64_t target = (int64_t)(progress * dec->duration / av_q2d(dec->fmt_ctx->streams[dec->video_stream_index]->time_base));
    av_seek_frame(dec->fmt_ctx, dec->video_stream_index, target, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(dec->video_ctx);
    if (dec->audio_ctx) avcodec_flush_buffers(dec->audio_ctx);
    if (dec->audio_stream) SDL_ClearAudioStream(dec->audio_stream);
}
