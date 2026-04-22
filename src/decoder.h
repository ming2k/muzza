#ifndef MUZZA_DECODER_H
#define MUZZA_DECODER_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#include "flux/flux.h"

typedef struct muzza_decoder muzza_decoder;

muzza_decoder* decoder_create(fx_context* ctx, const char* filepath);

/* Gets the current playback progress (0.0 - 1.0) */
float decoder_get_progress(muzza_decoder* dec);
void decoder_destroy(muzza_decoder* dec);

/* Decodes the next frame and updates the internal fx_image. 
   Returns true if a frame was successfully decoded and image updated. */
bool decoder_read_frame(muzza_decoder* dec);

/* Gets the current fx_image and dimensions */
fx_image* decoder_get_image(muzza_decoder* dec, int* width, int* height);

/* Seeks to a normalized progress (0.0 to 1.0) */
void decoder_seek(muzza_decoder* dec, float progress);

#endif // MUZZA_DECODER_H
