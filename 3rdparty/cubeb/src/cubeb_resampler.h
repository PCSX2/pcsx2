/*
 * Copyright © 2014 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#ifndef CUBEB_RESAMPLER_H
#define CUBEB_RESAMPLER_H

#include "cubeb/cubeb.h"

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct cubeb_resampler cubeb_resampler;

typedef enum {
  CUBEB_RESAMPLER_QUALITY_VOIP,
  CUBEB_RESAMPLER_QUALITY_DEFAULT,
  CUBEB_RESAMPLER_QUALITY_DESKTOP
} cubeb_resampler_quality;

typedef enum {
  CUBEB_RESAMPLER_RECLOCK_NONE,
  CUBEB_RESAMPLER_RECLOCK_INPUT
} cubeb_resampler_reclock;

/**
 * Create a resampler to adapt the requested sample rate into something that
 * is accepted by the audio backend.
 * @param stream A cubeb_stream instance supplied to the data callback.
 * @param input_params Used to calculate bytes per frame and buffer size for
 * resampling of the input side of the stream. NULL if input should not be
 * resampled.
 * @param output_params Used to calculate bytes per frame and buffer size for
 * resampling of the output side of the stream. NULL if output should not be
 * resampled.
 * @param target_rate The sampling rate after resampling for the input side of
 * the stream, and/or the sampling rate prior to resampling of the output side
 * of the stream.
 * @param callback A callback to request data for resampling.
 * @param user_ptr User data supplied to the data callback.
 * @param quality Quality of the resampler.
 * @retval A non-null pointer if success.
 */
cubeb_resampler *
cubeb_resampler_create(cubeb_stream * stream,
                       cubeb_stream_params * input_params,
                       cubeb_stream_params * output_params,
                       unsigned int target_rate, cubeb_data_callback callback,
                       void * user_ptr, cubeb_resampler_quality quality,
                       cubeb_resampler_reclock reclock);

/**
 * Fill the buffer with frames acquired using the data callback. Resampling will
 * happen if necessary.
 * @param resampler A cubeb_resampler instance.
 * @param input_buffer A buffer of input samples
 * @param input_frame_count The size of the buffer. Returns the number of frames
 * consumed.
 * @param output_buffer The buffer to be filled.
 * @param output_frames_needed Number of frames that should be produced.
 * @retval Number of frames that are actually produced.
 * @retval CUBEB_ERROR on error.
 */
long
cubeb_resampler_fill(cubeb_resampler * resampler, void * input_buffer,
                     long * input_frame_count, void * output_buffer,
                     long output_frames_needed);

/**
 * Destroy a cubeb_resampler.
 * @param resampler A cubeb_resampler instance.
 */
void
cubeb_resampler_destroy(cubeb_resampler * resampler);

/**
 * Returns the latency, in frames, of the resampler.
 * @param resampler A cubeb resampler instance.
 * @retval The latency, in frames, induced by the resampler.
 */
long
cubeb_resampler_latency(cubeb_resampler * resampler);

#if defined(__cplusplus)
}
#endif

#endif /* CUBEB_RESAMPLER_H */
