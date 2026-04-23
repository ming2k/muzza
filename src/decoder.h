#ifndef MUZZA_DECODER_H
#define MUZZA_DECODER_H

#include <stdbool.h>
#include "flux/flux.h"

typedef enum {
    MUZZA_DECODER_VIDEO = 1 << 0,
    MUZZA_DECODER_AUDIO = 1 << 1,
    MUZZA_DECODER_BOTH = (1 << 0) | (1 << 1)
} muzza_decoder_mode;

typedef struct muzza_decoder muzza_decoder;

muzza_decoder* decoder_create(fx_context* ctx, const char* filepath, muzza_decoder_mode mode);
void decoder_destroy(muzza_decoder* dec);

/* Decodes forward until the decoder's current time reaches or exceeds target_pts. */
bool decoder_update_to_time(muzza_decoder* dec, double target_pts, bool process_audio);

fx_image* decoder_get_image(muzza_decoder* dec, int* w, int* h);
float decoder_get_progress(muzza_decoder* dec);
double decoder_get_duration(muzza_decoder* dec);
double decoder_get_time(muzza_decoder* dec);
void decoder_seek(muzza_decoder* dec, float progress);
bool decoder_seek_to_time(muzza_decoder* dec, double time_seconds);

void decoder_set_paused(muzza_decoder* dec, bool paused);
bool decoder_is_paused(muzza_decoder* dec);
bool decoder_has_video(muzza_decoder* dec);
bool decoder_has_audio(muzza_decoder* dec);

bool decoder_is_image(muzza_decoder* dec);

bool decoder_generate_waveform(const char* filepath, float* out_peaks, int num_peaks);

#endif
