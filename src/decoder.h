#ifndef MUZZA_DECODER_H
#define MUZZA_DECODER_H

#include <stdbool.h>
#include "flux/flux.h"

typedef struct muzza_decoder muzza_decoder;

muzza_decoder* decoder_create(fx_context* ctx, const char* filepath);
void decoder_destroy(muzza_decoder* dec);

/* Decodes forward until a video frame is uploaded to GPU. */
bool decoder_update(muzza_decoder* dec, double delta_time);

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

#endif
