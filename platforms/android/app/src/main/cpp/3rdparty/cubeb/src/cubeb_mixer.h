/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#ifndef CUBEB_MIXER
#define CUBEB_MIXER

#include "cubeb/cubeb.h" // for cubeb_channel_layout and cubeb_stream_params.

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct cubeb_mixer cubeb_mixer;
cubeb_mixer *
cubeb_mixer_create(cubeb_sample_format format, uint32_t in_channels,
                   cubeb_channel_layout in_layout, uint32_t out_channels,
                   cubeb_channel_layout out_layout);
void
cubeb_mixer_destroy(cubeb_mixer * mixer);
int
cubeb_mixer_mix(cubeb_mixer * mixer, size_t frames, const void * input_buffer,
                size_t input_buffer_size, void * output_buffer,
                size_t output_buffer_size);

unsigned int
cubeb_channel_layout_nb_channels(cubeb_channel_layout channel_layout);

#if defined(__cplusplus)
}
#endif

#endif // CUBEB_MIXER
