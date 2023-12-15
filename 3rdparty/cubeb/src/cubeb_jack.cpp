/*
 * Copyright © 2012 David Richards
 * Copyright © 2013 Sebastien Alaiwan
 * Copyright © 2016 Damien Zammit
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#if !defined(__FreeBSD__) && !defined(__NetBSD__)
#define _POSIX_SOURCE
#endif
#include "cubeb-internal.h"
#include "cubeb/cubeb.h"
#include "cubeb_resampler.h"
#include "cubeb_utils.h"
#include <dlfcn.h>
#include <limits.h>
#include <math.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <jack/jack.h>
#include <jack/statistics.h>

#ifdef DISABLE_LIBJACK_DLOPEN
#define WRAP(x) x
#else
#define WRAP(x) (*api_##x)
#define JACK_API_VISIT(X)                                                      \
  X(jack_activate)                                                             \
  X(jack_client_close)                                                         \
  X(jack_client_open)                                                          \
  X(jack_connect)                                                              \
  X(jack_free)                                                                 \
  X(jack_get_ports)                                                            \
  X(jack_get_sample_rate)                                                      \
  X(jack_get_xrun_delayed_usecs)                                               \
  X(jack_get_buffer_size)                                                      \
  X(jack_port_get_buffer)                                                      \
  X(jack_port_name)                                                            \
  X(jack_port_register)                                                        \
  X(jack_port_unregister)                                                      \
  X(jack_port_get_latency_range)                                               \
  X(jack_set_process_callback)                                                 \
  X(jack_set_xrun_callback)                                                    \
  X(jack_set_graph_order_callback)                                             \
  X(jack_set_error_function)                                                   \
  X(jack_set_info_function)

#define IMPORT_FUNC(x) static decltype(x) * api_##x;
JACK_API_VISIT(IMPORT_FUNC);
#undef IMPORT_FUNC
#endif

#define JACK_DEFAULT_IN "JACK capture"
#define JACK_DEFAULT_OUT "JACK playback"

static const int MAX_STREAMS = 16;
static const int MAX_CHANNELS = 8;
static const int FIFO_SIZE = 4096 * sizeof(float);

enum devstream {
  NONE = 0,
  IN_ONLY,
  OUT_ONLY,
  DUPLEX,
};

enum cbjack_connect_ports_options {
  CBJACK_CP_OPTIONS_NONE = 0x0,
  CBJACK_CP_OPTIONS_SKIP_OUTPUT = 0x1,
  CBJACK_CP_OPTIONS_SKIP_INPUT = 0x2,
};

static void
s16ne_to_float(float * dst, const int16_t * src, size_t n)
{
  for (size_t i = 0; i < n; i++)
    *(dst++) = (float)((float)*(src++) / 32767.0f);
}

static void
float_to_s16ne(int16_t * dst, float * src, size_t n)
{
  for (size_t i = 0; i < n; i++) {
    if (*src > 1.f)
      *src = 1.f;
    if (*src < -1.f)
      *src = -1.f;
    *(dst++) = (int16_t)((int16_t)(*(src++) * 32767));
  }
}

extern "C" {
/*static*/ int
jack_init(cubeb ** context, char const * context_name);
}
static char const *
cbjack_get_backend_id(cubeb * context);
static int
cbjack_get_max_channel_count(cubeb * ctx, uint32_t * max_channels);
static int
cbjack_get_min_latency(cubeb * ctx, cubeb_stream_params params,
                       uint32_t * latency_frames);
static int
cbjack_get_latency(cubeb_stream * stm, unsigned int * latency_frames);
static int
cbjack_get_preferred_sample_rate(cubeb * ctx, uint32_t * rate);
static void
cbjack_destroy(cubeb * context);
static void
cbjack_interleave_capture(cubeb_stream * stream, float ** in,
                          jack_nframes_t nframes, bool format_mismatch);
static void
cbjack_deinterleave_playback_refill_s16ne(cubeb_stream * stream,
                                          short ** bufs_in, float ** bufs_out,
                                          jack_nframes_t nframes);
static void
cbjack_deinterleave_playback_refill_float(cubeb_stream * stream,
                                          float ** bufs_in, float ** bufs_out,
                                          jack_nframes_t nframes);
static int
cbjack_stream_device_destroy(cubeb_stream * stream, cubeb_device * device);
static int
cbjack_stream_get_current_device(cubeb_stream * stm,
                                 cubeb_device ** const device);
static int
cbjack_enumerate_devices(cubeb * context, cubeb_device_type type,
                         cubeb_device_collection * collection);
static int
cbjack_device_collection_destroy(cubeb * context,
                                 cubeb_device_collection * collection);
static int
cbjack_stream_init(cubeb * context, cubeb_stream ** stream,
                   char const * stream_name, cubeb_devid input_device,
                   cubeb_stream_params * input_stream_params,
                   cubeb_devid output_device,
                   cubeb_stream_params * output_stream_params,
                   unsigned int latency_frames,
                   cubeb_data_callback data_callback,
                   cubeb_state_callback state_callback, void * user_ptr);
static void
cbjack_stream_destroy(cubeb_stream * stream);
static int
cbjack_stream_start(cubeb_stream * stream);
static int
cbjack_stream_stop(cubeb_stream * stream);
static int
cbjack_stream_get_position(cubeb_stream * stream, uint64_t * position);
static int
cbjack_stream_set_volume(cubeb_stream * stm, float volume);

static struct cubeb_ops const cbjack_ops = {
    .init = jack_init,
    .get_backend_id = cbjack_get_backend_id,
    .get_max_channel_count = cbjack_get_max_channel_count,
    .get_min_latency = cbjack_get_min_latency,
    .get_preferred_sample_rate = cbjack_get_preferred_sample_rate,
    .enumerate_devices = cbjack_enumerate_devices,
    .device_collection_destroy = cbjack_device_collection_destroy,
    .destroy = cbjack_destroy,
    .stream_init = cbjack_stream_init,
    .stream_destroy = cbjack_stream_destroy,
    .stream_start = cbjack_stream_start,
    .stream_stop = cbjack_stream_stop,
    .stream_get_position = cbjack_stream_get_position,
    .stream_get_latency = cbjack_get_latency,
    .stream_get_input_latency = NULL,
    .stream_set_volume = cbjack_stream_set_volume,
    .stream_set_name = NULL,
    .stream_get_current_device = cbjack_stream_get_current_device,
    .stream_device_destroy = cbjack_stream_device_destroy,
    .stream_register_device_changed_callback = NULL,
    .register_device_collection_changed = NULL};

struct cubeb_stream {
  /* Note: Must match cubeb_stream layout in cubeb.c. */
  cubeb * context;
  void * user_ptr;
  /**/

  /**< Mutex for each stream */
  pthread_mutex_t mutex;

  bool in_use;      /**< Set to false iff the stream is free */
  bool ports_ready; /**< Set to true iff the JACK ports are ready */

  cubeb_data_callback data_callback;
  cubeb_state_callback state_callback;
  cubeb_stream_params in_params;
  cubeb_stream_params out_params;

  cubeb_resampler * resampler;

  uint64_t position;
  bool pause;
  float ratio;
  enum devstream devs;
  char stream_name[256];
  jack_port_t * output_ports[MAX_CHANNELS];
  jack_port_t * input_ports[MAX_CHANNELS];
  float volume;
};

struct cubeb {
  struct cubeb_ops const * ops;
  void * libjack;

  /**< Mutex for whole context */
  pthread_mutex_t mutex;

  /**< Audio buffers, converted to float */
  float in_float_interleaved_buffer[FIFO_SIZE * MAX_CHANNELS];
  float out_float_interleaved_buffer[FIFO_SIZE * MAX_CHANNELS];

  /**< Audio buffer, at the sampling rate of the output */
  float in_resampled_interleaved_buffer_float[FIFO_SIZE * MAX_CHANNELS * 3];
  int16_t in_resampled_interleaved_buffer_s16ne[FIFO_SIZE * MAX_CHANNELS * 3];
  float out_resampled_interleaved_buffer_float[FIFO_SIZE * MAX_CHANNELS * 3];
  int16_t out_resampled_interleaved_buffer_s16ne[FIFO_SIZE * MAX_CHANNELS * 3];

  cubeb_stream streams[MAX_STREAMS];
  unsigned int active_streams;

  cubeb_device_collection_changed_callback collection_changed_callback;

  bool active;
  unsigned int jack_sample_rate;
  unsigned int jack_latency;
  unsigned int jack_xruns;
  unsigned int jack_buffer_size;
  unsigned int fragment_size;
  unsigned int output_bytes_per_frame;
  jack_client_t * jack_client;
};

static int
load_jack_lib(cubeb * context)
{
#ifndef DISABLE_LIBJACK_DLOPEN
#ifdef __APPLE__
  context->libjack = dlopen("libjack.0.dylib", RTLD_LAZY);
  context->libjack = dlopen("/usr/local/lib/libjack.0.dylib", RTLD_LAZY);
#elif defined(__WIN32__)
#ifdef _WIN64
  context->libjack = LoadLibrary("libjack64.dll");
#else
  context->libjack = LoadLibrary("libjack.dll");
#endif
#else
  context->libjack = dlopen("libjack.so.0", RTLD_LAZY);
  if (!context->libjack) {
    context->libjack = dlopen("libjack.so", RTLD_LAZY);
  }
#endif
  if (!context->libjack) {
    return CUBEB_ERROR;
  }

#define LOAD(x)                                                                \
  {                                                                            \
    api_##x = (decltype(x) *)dlsym(context->libjack, #x);                      \
    if (!api_##x) {                                                            \
      dlclose(context->libjack);                                               \
      return CUBEB_ERROR;                                                      \
    }                                                                          \
  }

  JACK_API_VISIT(LOAD);
#undef LOAD
#endif
  return CUBEB_OK;
}

static void
cbjack_connect_port_out(cubeb_stream * stream, const size_t out_port,
                        const char * const phys_in_port)
{
  const char * src_port = WRAP(jack_port_name)(stream->output_ports[out_port]);

  WRAP(jack_connect)(stream->context->jack_client, src_port, phys_in_port);
}

static void
cbjack_connect_port_in(cubeb_stream * stream, const char * const phys_out_port,
                       size_t in_port)
{
  const char * src_port = WRAP(jack_port_name)(stream->input_ports[in_port]);

  WRAP(jack_connect)(stream->context->jack_client, phys_out_port, src_port);
}

static int
cbjack_connect_ports(cubeb_stream * stream,
                     enum cbjack_connect_ports_options options)
{
  int r = CUBEB_ERROR;
  const char ** phys_in_ports =
      WRAP(jack_get_ports)(stream->context->jack_client, NULL, NULL,
                           JackPortIsInput | JackPortIsPhysical);
  const char ** phys_out_ports =
      WRAP(jack_get_ports)(stream->context->jack_client, NULL, NULL,
                           JackPortIsOutput | JackPortIsPhysical);

  if (phys_in_ports == NULL || *phys_in_ports == NULL ||
      options & CBJACK_CP_OPTIONS_SKIP_OUTPUT) {
    goto skipplayback;
  }

  // Connect outputs to playback
  for (unsigned int c = 0;
       c < stream->out_params.channels && phys_in_ports[c] != NULL; c++) {
    cbjack_connect_port_out(stream, c, phys_in_ports[c]);
  }

  // Special case playing mono source in stereo
  if (stream->out_params.channels == 1 && phys_in_ports[1] != NULL) {
    cbjack_connect_port_out(stream, 0, phys_in_ports[1]);
  }

  r = CUBEB_OK;

skipplayback:
  if (phys_out_ports == NULL || *phys_out_ports == NULL ||
      options & CBJACK_CP_OPTIONS_SKIP_INPUT) {
    goto end;
  }
  // Connect inputs to capture
  for (unsigned int c = 0;
       c < stream->in_params.channels && phys_out_ports[c] != NULL; c++) {
    cbjack_connect_port_in(stream, phys_out_ports[c], c);
  }
  r = CUBEB_OK;
end:
  if (phys_out_ports) {
    WRAP(jack_free)(phys_out_ports);
  }
  if (phys_in_ports) {
    WRAP(jack_free)(phys_in_ports);
  }
  return r;
}

static int
cbjack_xrun_callback(void * arg)
{
  cubeb * ctx = (cubeb *)arg;

  float delay = WRAP(jack_get_xrun_delayed_usecs)(ctx->jack_client);
  float fragments = ceilf(((delay / 1000000.0) * ctx->jack_sample_rate) /
                          ctx->jack_buffer_size);

  ctx->jack_xruns += (unsigned int)fragments;
  return 0;
}

static int
cbjack_graph_order_callback(void * arg)
{
  cubeb * ctx = (cubeb *)arg;
  int i;
  jack_latency_range_t latency_range;
  jack_nframes_t port_latency, max_latency = 0;

  for (int j = 0; j < MAX_STREAMS; j++) {
    cubeb_stream * stm = &ctx->streams[j];

    if (!stm->in_use)
      continue;
    if (!stm->ports_ready)
      continue;

    for (i = 0; i < (int)stm->out_params.channels; ++i) {
      WRAP(jack_port_get_latency_range)
      (stm->output_ports[i], JackPlaybackLatency, &latency_range);
      port_latency = latency_range.max;
      if (port_latency > max_latency)
        max_latency = port_latency;
    }
    /* Cap minimum latency to 128 frames */
    if (max_latency < 128)
      max_latency = 128;
  }

  ctx->jack_latency = max_latency;

  return 0;
}

static int
cbjack_process(jack_nframes_t nframes, void * arg)
{
  cubeb * ctx = (cubeb *)arg;
  unsigned int t_jack_xruns = ctx->jack_xruns;
  int i;

  ctx->jack_xruns = 0;

  for (int j = 0; j < MAX_STREAMS; j++) {
    cubeb_stream * stm = &ctx->streams[j];
    float * bufs_out[stm->out_params.channels];
    float * bufs_in[stm->in_params.channels];

    if (!stm->in_use)
      continue;

    // handle xruns by skipping audio that should have been played
    stm->position += t_jack_xruns * ctx->fragment_size * stm->ratio;

    if (!stm->ports_ready)
      continue;

    if (stm->devs & OUT_ONLY) {
      // get jack output buffers
      for (i = 0; i < (int)stm->out_params.channels; i++)
        bufs_out[i] =
            (float *)WRAP(jack_port_get_buffer)(stm->output_ports[i], nframes);
    }
    if (stm->devs & IN_ONLY) {
      // get jack input buffers
      for (i = 0; i < (int)stm->in_params.channels; i++)
        bufs_in[i] =
            (float *)WRAP(jack_port_get_buffer)(stm->input_ports[i], nframes);
    }
    if (stm->pause) {
      // paused, play silence on output
      if (stm->devs & OUT_ONLY) {
        for (unsigned int c = 0; c < stm->out_params.channels; c++) {
          float * buffer_out = bufs_out[c];
          for (long f = 0; f < nframes; f++) {
            buffer_out[f] = 0.f;
          }
        }
      }
      if (stm->devs & IN_ONLY) {
        // paused, capture silence
        for (unsigned int c = 0; c < stm->in_params.channels; c++) {
          float * buffer_in = bufs_in[c];
          for (long f = 0; f < nframes; f++) {
            buffer_in[f] = 0.f;
          }
        }
      }
    } else {

      // try to lock stream mutex
      if (pthread_mutex_trylock(&stm->mutex) == 0) {

        int16_t * in_s16ne =
            stm->context->in_resampled_interleaved_buffer_s16ne;
        float * in_float = stm->context->in_resampled_interleaved_buffer_float;

        // unpaused, play audio
        if (stm->devs == DUPLEX) {
          if (stm->out_params.format == CUBEB_SAMPLE_S16NE) {
            cbjack_interleave_capture(stm, bufs_in, nframes, true);
            cbjack_deinterleave_playback_refill_s16ne(stm, &in_s16ne, bufs_out,
                                                      nframes);
          } else if (stm->out_params.format == CUBEB_SAMPLE_FLOAT32NE) {
            cbjack_interleave_capture(stm, bufs_in, nframes, false);
            cbjack_deinterleave_playback_refill_float(stm, &in_float, bufs_out,
                                                      nframes);
          }
        } else if (stm->devs == IN_ONLY) {
          if (stm->in_params.format == CUBEB_SAMPLE_S16NE) {
            cbjack_interleave_capture(stm, bufs_in, nframes, true);
            cbjack_deinterleave_playback_refill_s16ne(stm, &in_s16ne, nullptr,
                                                      nframes);
          } else if (stm->in_params.format == CUBEB_SAMPLE_FLOAT32NE) {
            cbjack_interleave_capture(stm, bufs_in, nframes, false);
            cbjack_deinterleave_playback_refill_float(stm, &in_float, nullptr,
                                                      nframes);
          }
        } else if (stm->devs == OUT_ONLY) {
          if (stm->out_params.format == CUBEB_SAMPLE_S16NE) {
            cbjack_deinterleave_playback_refill_s16ne(stm, nullptr, bufs_out,
                                                      nframes);
          } else if (stm->out_params.format == CUBEB_SAMPLE_FLOAT32NE) {
            cbjack_deinterleave_playback_refill_float(stm, nullptr, bufs_out,
                                                      nframes);
          }
        }
        // unlock stream mutex
        pthread_mutex_unlock(&stm->mutex);

      } else {
        // could not lock mutex
        // output silence
        if (stm->devs & OUT_ONLY) {
          for (unsigned int c = 0; c < stm->out_params.channels; c++) {
            float * buffer_out = bufs_out[c];
            for (long f = 0; f < nframes; f++) {
              buffer_out[f] = 0.f;
            }
          }
        }
        if (stm->devs & IN_ONLY) {
          // capture silence
          for (unsigned int c = 0; c < stm->in_params.channels; c++) {
            float * buffer_in = bufs_in[c];
            for (long f = 0; f < nframes; f++) {
              buffer_in[f] = 0.f;
            }
          }
        }
      }
    }
  }
  return 0;
}

static void
cbjack_deinterleave_playback_refill_float(cubeb_stream * stream, float ** in,
                                          float ** bufs_out,
                                          jack_nframes_t nframes)
{
  float * out_interleaved_buffer = nullptr;

  float * inptr = (in != NULL) ? *in : nullptr;
  float * outptr = (bufs_out != NULL) ? *bufs_out : nullptr;

  long needed_frames = (bufs_out != NULL) ? nframes : 0;
  long done_frames = 0;
  long input_frames_count = (in != NULL) ? nframes : 0;

  done_frames = cubeb_resampler_fill(
      stream->resampler, inptr, &input_frames_count,
      (bufs_out != NULL)
          ? stream->context->out_resampled_interleaved_buffer_float
          : NULL,
      needed_frames);

  out_interleaved_buffer =
      stream->context->out_resampled_interleaved_buffer_float;

  if (outptr) {
    // convert interleaved output buffers to contiguous buffers
    for (unsigned int c = 0; c < stream->out_params.channels; c++) {
      float * buffer = bufs_out[c];
      for (long f = 0; f < done_frames; f++) {
        buffer[f] =
            out_interleaved_buffer[(f * stream->out_params.channels) + c] *
            stream->volume;
      }
      if (done_frames < needed_frames) {
        // draining
        for (long f = done_frames; f < needed_frames; f++) {
          buffer[f] = 0.f;
        }
      }
      if (done_frames == 0) {
        // stop, but first zero out the existing buffer
        for (long f = 0; f < needed_frames; f++) {
          buffer[f] = 0.f;
        }
      }
    }
  }

  if (done_frames >= 0 && done_frames < needed_frames) {
    // set drained
    stream->state_callback(stream, stream->user_ptr, CUBEB_STATE_DRAINED);
    // stop stream
    cbjack_stream_stop(stream);
  }
  if (done_frames > 0 && done_frames <= needed_frames) {
    // advance stream position
    stream->position += done_frames * stream->ratio;
  }
  if (done_frames < 0 || done_frames > needed_frames) {
    // stream error
    stream->state_callback(stream, stream->user_ptr, CUBEB_STATE_ERROR);
  }
}

static void
cbjack_deinterleave_playback_refill_s16ne(cubeb_stream * stream, short ** in,
                                          float ** bufs_out,
                                          jack_nframes_t nframes)
{
  float * out_interleaved_buffer = nullptr;

  short * inptr = (in != NULL) ? *in : nullptr;
  float * outptr = (bufs_out != NULL) ? *bufs_out : nullptr;

  long needed_frames = (bufs_out != NULL) ? nframes : 0;
  long done_frames = 0;
  long input_frames_count = (in != NULL) ? nframes : 0;

  done_frames = cubeb_resampler_fill(
      stream->resampler, inptr, &input_frames_count,
      (bufs_out != NULL)
          ? stream->context->out_resampled_interleaved_buffer_s16ne
          : NULL,
      needed_frames);

  s16ne_to_float(stream->context->out_resampled_interleaved_buffer_float,
                 stream->context->out_resampled_interleaved_buffer_s16ne,
                 done_frames * stream->out_params.channels);

  out_interleaved_buffer =
      stream->context->out_resampled_interleaved_buffer_float;

  if (outptr) {
    // convert interleaved output buffers to contiguous buffers
    for (unsigned int c = 0; c < stream->out_params.channels; c++) {
      float * buffer = bufs_out[c];
      for (long f = 0; f < done_frames; f++) {
        buffer[f] =
            out_interleaved_buffer[(f * stream->out_params.channels) + c] *
            stream->volume;
      }
      if (done_frames < needed_frames) {
        // draining
        for (long f = done_frames; f < needed_frames; f++) {
          buffer[f] = 0.f;
        }
      }
      if (done_frames == 0) {
        // stop, but first zero out the existing buffer
        for (long f = 0; f < needed_frames; f++) {
          buffer[f] = 0.f;
        }
      }
    }
  }

  if (done_frames >= 0 && done_frames < needed_frames) {
    // set drained
    stream->state_callback(stream, stream->user_ptr, CUBEB_STATE_DRAINED);
    // stop stream
    cbjack_stream_stop(stream);
  }
  if (done_frames > 0 && done_frames <= needed_frames) {
    // advance stream position
    stream->position += done_frames * stream->ratio;
  }
  if (done_frames < 0 || done_frames > needed_frames) {
    // stream error
    stream->state_callback(stream, stream->user_ptr, CUBEB_STATE_ERROR);
  }
}

static void
cbjack_interleave_capture(cubeb_stream * stream, float ** in,
                          jack_nframes_t nframes, bool format_mismatch)
{
  float * in_buffer = stream->context->in_float_interleaved_buffer;

  for (unsigned int c = 0; c < stream->in_params.channels; c++) {
    for (long f = 0; f < nframes; f++) {
      in_buffer[(f * stream->in_params.channels) + c] =
          in[c][f] * stream->volume;
    }
  }
  if (format_mismatch) {
    float_to_s16ne(stream->context->in_resampled_interleaved_buffer_s16ne,
                   in_buffer, nframes * stream->in_params.channels);
  } else {
    memset(stream->context->in_resampled_interleaved_buffer_float, 0,
           (FIFO_SIZE * MAX_CHANNELS * 3) * sizeof(float));
    memcpy(stream->context->in_resampled_interleaved_buffer_float, in_buffer,
           (FIFO_SIZE * MAX_CHANNELS * 2) * sizeof(float));
  }
}

static void
silent_jack_error_callback(char const * /*msg*/)
{
}

/*static*/ int
jack_init(cubeb ** context, char const * context_name)
{
  int r;

  *context = NULL;

  cubeb * ctx = (cubeb *)calloc(1, sizeof(*ctx));
  if (ctx == NULL) {
    return CUBEB_ERROR;
  }

  r = load_jack_lib(ctx);
  if (r != 0) {
    cbjack_destroy(ctx);
    return CUBEB_ERROR;
  }

  WRAP(jack_set_error_function)(silent_jack_error_callback);
  WRAP(jack_set_info_function)(silent_jack_error_callback);

  ctx->ops = &cbjack_ops;

  ctx->mutex = PTHREAD_MUTEX_INITIALIZER;
  for (r = 0; r < MAX_STREAMS; r++) {
    ctx->streams[r].mutex = PTHREAD_MUTEX_INITIALIZER;
  }

  const char * jack_client_name = "cubeb";
  if (context_name)
    jack_client_name = context_name;

  ctx->jack_client =
      WRAP(jack_client_open)(jack_client_name, JackNoStartServer, NULL);

  if (ctx->jack_client == NULL) {
    cbjack_destroy(ctx);
    return CUBEB_ERROR;
  }

  ctx->jack_xruns = 0;

  WRAP(jack_set_process_callback)(ctx->jack_client, cbjack_process, ctx);
  WRAP(jack_set_xrun_callback)(ctx->jack_client, cbjack_xrun_callback, ctx);
  WRAP(jack_set_graph_order_callback)
  (ctx->jack_client, cbjack_graph_order_callback, ctx);

  if (WRAP(jack_activate)(ctx->jack_client)) {
    cbjack_destroy(ctx);
    return CUBEB_ERROR;
  }

  ctx->jack_sample_rate = WRAP(jack_get_sample_rate)(ctx->jack_client);
  ctx->jack_latency = 128 * 1000 / ctx->jack_sample_rate;

  ctx->active = true;
  *context = ctx;

  return CUBEB_OK;
}

static char const *
cbjack_get_backend_id(cubeb * /*context*/)
{
  return "jack";
}

static int
cbjack_get_max_channel_count(cubeb * /*ctx*/, uint32_t * max_channels)
{
  *max_channels = MAX_CHANNELS;
  return CUBEB_OK;
}

static int
cbjack_get_latency(cubeb_stream * stm, unsigned int * latency_ms)
{
  *latency_ms = stm->context->jack_latency;
  return CUBEB_OK;
}

static int
cbjack_get_min_latency(cubeb * ctx, cubeb_stream_params /*params*/,
                       uint32_t * latency_ms)
{
  *latency_ms = ctx->jack_latency;
  return CUBEB_OK;
}

static int
cbjack_get_preferred_sample_rate(cubeb * ctx, uint32_t * rate)
{
  if (!ctx->jack_client) {
    jack_client_t * testclient =
        WRAP(jack_client_open)("test-samplerate", JackNoStartServer, NULL);
    if (!testclient) {
      return CUBEB_ERROR;
    }

    *rate = WRAP(jack_get_sample_rate)(testclient);
    WRAP(jack_client_close)(testclient);

  } else {
    *rate = WRAP(jack_get_sample_rate)(ctx->jack_client);
  }
  return CUBEB_OK;
}

static void
cbjack_destroy(cubeb * context)
{
  context->active = false;

  if (context->jack_client != NULL)
    WRAP(jack_client_close)(context->jack_client);
#ifndef DISABLE_LIBJACK_DLOPEN
  if (context->libjack)
    dlclose(context->libjack);
#endif
  free(context);
}

static cubeb_stream *
context_alloc_stream(cubeb * context, char const * stream_name)
{
  for (int i = 0; i < MAX_STREAMS; i++) {
    if (!context->streams[i].in_use) {
      cubeb_stream * stm = &context->streams[i];
      stm->in_use = true;
      snprintf(stm->stream_name, 255, "%s_%u", stream_name, i);
      return stm;
    }
  }
  return NULL;
}

static int
cbjack_stream_init(cubeb * context, cubeb_stream ** stream,
                   char const * stream_name, cubeb_devid input_device,
                   cubeb_stream_params * input_stream_params,
                   cubeb_devid output_device,
                   cubeb_stream_params * output_stream_params,
                   unsigned int /*latency_frames*/,
                   cubeb_data_callback data_callback,
                   cubeb_state_callback state_callback, void * user_ptr)
{
  int stream_actual_rate = 0;
  int jack_rate = WRAP(jack_get_sample_rate)(context->jack_client);

  if (output_stream_params &&
      (output_stream_params->format != CUBEB_SAMPLE_FLOAT32NE &&
       output_stream_params->format != CUBEB_SAMPLE_S16NE)) {
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  if (input_stream_params &&
      (input_stream_params->format != CUBEB_SAMPLE_FLOAT32NE &&
       input_stream_params->format != CUBEB_SAMPLE_S16NE)) {
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  if ((input_device && input_device != JACK_DEFAULT_IN) ||
      (output_device && output_device != JACK_DEFAULT_OUT)) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  // Loopback is unsupported
  if ((input_stream_params &&
       (input_stream_params->prefs & CUBEB_STREAM_PREF_LOOPBACK)) ||
      (output_stream_params &&
       (output_stream_params->prefs & CUBEB_STREAM_PREF_LOOPBACK))) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  *stream = NULL;

  // Find a free stream.
  pthread_mutex_lock(&context->mutex);
  cubeb_stream * stm = context_alloc_stream(context, stream_name);

  // No free stream?
  if (stm == NULL) {
    pthread_mutex_unlock(&context->mutex);
    return CUBEB_ERROR;
  }

  // unlock context mutex
  pthread_mutex_unlock(&context->mutex);

  // Lock active stream
  pthread_mutex_lock(&stm->mutex);

  stm->ports_ready = false;
  stm->user_ptr = user_ptr;
  stm->context = context;
  stm->devs = NONE;
  if (output_stream_params && !input_stream_params) {
    stm->out_params = *output_stream_params;
    stream_actual_rate = stm->out_params.rate;
    stm->out_params.rate = jack_rate;
    stm->devs = OUT_ONLY;
    if (stm->out_params.format == CUBEB_SAMPLE_FLOAT32NE) {
      context->output_bytes_per_frame = sizeof(float);
    } else {
      context->output_bytes_per_frame = sizeof(short);
    }
  }
  if (input_stream_params && output_stream_params) {
    stm->in_params = *input_stream_params;
    stm->out_params = *output_stream_params;
    stream_actual_rate = stm->out_params.rate;
    stm->in_params.rate = jack_rate;
    stm->out_params.rate = jack_rate;
    stm->devs = DUPLEX;
    if (stm->out_params.format == CUBEB_SAMPLE_FLOAT32NE) {
      context->output_bytes_per_frame = sizeof(float);
      stm->in_params.format = CUBEB_SAMPLE_FLOAT32NE;
    } else {
      context->output_bytes_per_frame = sizeof(short);
      stm->in_params.format = CUBEB_SAMPLE_S16NE;
    }
  } else if (input_stream_params && !output_stream_params) {
    stm->in_params = *input_stream_params;
    stream_actual_rate = stm->in_params.rate;
    stm->in_params.rate = jack_rate;
    stm->devs = IN_ONLY;
    if (stm->in_params.format == CUBEB_SAMPLE_FLOAT32NE) {
      context->output_bytes_per_frame = sizeof(float);
    } else {
      context->output_bytes_per_frame = sizeof(short);
    }
  }

  stm->ratio = (float)stream_actual_rate / (float)jack_rate;

  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->position = 0;
  stm->volume = 1.0f;
  context->jack_buffer_size = WRAP(jack_get_buffer_size)(context->jack_client);
  context->fragment_size = context->jack_buffer_size;

  if (stm->devs == NONE) {
    pthread_mutex_unlock(&stm->mutex);
    return CUBEB_ERROR;
  }

  stm->resampler = NULL;

  if (stm->devs == DUPLEX) {
    stm->resampler = cubeb_resampler_create(
        stm, &stm->in_params, &stm->out_params, stream_actual_rate,
        stm->data_callback, stm->user_ptr, CUBEB_RESAMPLER_QUALITY_DESKTOP,
        CUBEB_RESAMPLER_RECLOCK_NONE);
  } else if (stm->devs == IN_ONLY) {
    stm->resampler = cubeb_resampler_create(
        stm, &stm->in_params, nullptr, stream_actual_rate, stm->data_callback,
        stm->user_ptr, CUBEB_RESAMPLER_QUALITY_DESKTOP,
        CUBEB_RESAMPLER_RECLOCK_NONE);
  } else if (stm->devs == OUT_ONLY) {
    stm->resampler = cubeb_resampler_create(
        stm, nullptr, &stm->out_params, stream_actual_rate, stm->data_callback,
        stm->user_ptr, CUBEB_RESAMPLER_QUALITY_DESKTOP,
        CUBEB_RESAMPLER_RECLOCK_NONE);
  }

  if (!stm->resampler) {
    stm->in_use = false;
    pthread_mutex_unlock(&stm->mutex);
    return CUBEB_ERROR;
  }

  if (stm->devs == DUPLEX || stm->devs == OUT_ONLY) {
    for (unsigned int c = 0; c < stm->out_params.channels; c++) {
      char portname[256];
      snprintf(portname, 255, "%s_out_%d", stm->stream_name, c);
      stm->output_ports[c] = WRAP(jack_port_register)(
          stm->context->jack_client, portname, JACK_DEFAULT_AUDIO_TYPE,
          JackPortIsOutput, 0);
      if (!(output_stream_params->prefs &
            CUBEB_STREAM_PREF_JACK_NO_AUTO_CONNECT)) {
        if (cbjack_connect_ports(stm, CBJACK_CP_OPTIONS_SKIP_INPUT) !=
            CUBEB_OK) {
          pthread_mutex_unlock(&stm->mutex);
          cbjack_stream_destroy(stm);
          return CUBEB_ERROR;
        }
      }
    }
  }

  if (stm->devs == DUPLEX || stm->devs == IN_ONLY) {
    for (unsigned int c = 0; c < stm->in_params.channels; c++) {
      char portname[256];
      snprintf(portname, 255, "%s_in_%d", stm->stream_name, c);
      stm->input_ports[c] =
          WRAP(jack_port_register)(stm->context->jack_client, portname,
                                   JACK_DEFAULT_AUDIO_TYPE, JackPortIsInput, 0);
      if (!(input_stream_params->prefs &
            CUBEB_STREAM_PREF_JACK_NO_AUTO_CONNECT)) {
        if (cbjack_connect_ports(stm, CBJACK_CP_OPTIONS_SKIP_OUTPUT) !=
            CUBEB_OK) {
          pthread_mutex_unlock(&stm->mutex);
          cbjack_stream_destroy(stm);
          return CUBEB_ERROR;
        }
      }
    }
  }

  *stream = stm;

  stm->ports_ready = true;
  stm->pause = true;
  pthread_mutex_unlock(&stm->mutex);

  return CUBEB_OK;
}

static void
cbjack_stream_destroy(cubeb_stream * stream)
{
  pthread_mutex_lock(&stream->mutex);
  stream->ports_ready = false;

  if (stream->devs == DUPLEX || stream->devs == OUT_ONLY) {
    for (unsigned int c = 0; c < stream->out_params.channels; c++) {
      if (stream->output_ports[c]) {
        WRAP(jack_port_unregister)
        (stream->context->jack_client, stream->output_ports[c]);
        stream->output_ports[c] = NULL;
      }
    }
  }

  if (stream->devs == DUPLEX || stream->devs == IN_ONLY) {
    for (unsigned int c = 0; c < stream->in_params.channels; c++) {
      if (stream->input_ports[c]) {
        WRAP(jack_port_unregister)
        (stream->context->jack_client, stream->input_ports[c]);
        stream->input_ports[c] = NULL;
      }
    }
  }

  if (stream->resampler) {
    cubeb_resampler_destroy(stream->resampler);
    stream->resampler = NULL;
  }
  stream->in_use = false;
  pthread_mutex_unlock(&stream->mutex);
}

static int
cbjack_stream_start(cubeb_stream * stream)
{
  stream->pause = false;
  stream->state_callback(stream, stream->user_ptr, CUBEB_STATE_STARTED);
  return CUBEB_OK;
}

static int
cbjack_stream_stop(cubeb_stream * stream)
{
  stream->pause = true;
  stream->state_callback(stream, stream->user_ptr, CUBEB_STATE_STOPPED);
  return CUBEB_OK;
}

static int
cbjack_stream_get_position(cubeb_stream * stream, uint64_t * position)
{
  *position = stream->position;
  return CUBEB_OK;
}

static int
cbjack_stream_set_volume(cubeb_stream * stm, float volume)
{
  stm->volume = volume;
  return CUBEB_OK;
}

static int
cbjack_stream_get_current_device(cubeb_stream * stm,
                                 cubeb_device ** const device)
{
  *device = (cubeb_device *)calloc(1, sizeof(cubeb_device));
  if (*device == NULL)
    return CUBEB_ERROR;

  const char * j_in = JACK_DEFAULT_IN;
  const char * j_out = JACK_DEFAULT_OUT;
  const char * empty = "";

  if (stm->devs == DUPLEX) {
    (*device)->input_name = strdup(j_in);
    (*device)->output_name = strdup(j_out);
  } else if (stm->devs == IN_ONLY) {
    (*device)->input_name = strdup(j_in);
    (*device)->output_name = strdup(empty);
  } else if (stm->devs == OUT_ONLY) {
    (*device)->input_name = strdup(empty);
    (*device)->output_name = strdup(j_out);
  }

  return CUBEB_OK;
}

static int
cbjack_stream_device_destroy(cubeb_stream * /*stream*/, cubeb_device * device)
{
  if (device->input_name)
    free(device->input_name);
  if (device->output_name)
    free(device->output_name);
  free(device);
  return CUBEB_OK;
}

static int
cbjack_enumerate_devices(cubeb * context, cubeb_device_type type,
                         cubeb_device_collection * collection)
{
  if (!context)
    return CUBEB_ERROR;

  uint32_t rate;
  cbjack_get_preferred_sample_rate(context, &rate);

  cubeb_device_info * devices = new cubeb_device_info[2];
  if (!devices)
    return CUBEB_ERROR;
  PodZero(devices, 2);
  collection->count = 0;

  if (type & CUBEB_DEVICE_TYPE_OUTPUT) {
    cubeb_device_info * cur = &devices[collection->count];
    cur->device_id = JACK_DEFAULT_OUT;
    cur->devid = (cubeb_devid)cur->device_id;
    cur->friendly_name = JACK_DEFAULT_OUT;
    cur->group_id = JACK_DEFAULT_OUT;
    cur->vendor_name = JACK_DEFAULT_OUT;
    cur->type = CUBEB_DEVICE_TYPE_OUTPUT;
    cur->state = CUBEB_DEVICE_STATE_ENABLED;
    cur->preferred = CUBEB_DEVICE_PREF_ALL;
    cur->format = CUBEB_DEVICE_FMT_F32NE;
    cur->default_format = CUBEB_DEVICE_FMT_F32NE;
    cur->max_channels = MAX_CHANNELS;
    cur->min_rate = rate;
    cur->max_rate = rate;
    cur->default_rate = rate;
    cur->latency_lo = 0;
    cur->latency_hi = 0;
    collection->count += 1;
  }

  if (type & CUBEB_DEVICE_TYPE_INPUT) {
    cubeb_device_info * cur = &devices[collection->count];
    cur->device_id = JACK_DEFAULT_IN;
    cur->devid = (cubeb_devid)cur->device_id;
    cur->friendly_name = JACK_DEFAULT_IN;
    cur->group_id = JACK_DEFAULT_IN;
    cur->vendor_name = JACK_DEFAULT_IN;
    cur->type = CUBEB_DEVICE_TYPE_INPUT;
    cur->state = CUBEB_DEVICE_STATE_ENABLED;
    cur->preferred = CUBEB_DEVICE_PREF_ALL;
    cur->format = CUBEB_DEVICE_FMT_F32NE;
    cur->default_format = CUBEB_DEVICE_FMT_F32NE;
    cur->max_channels = MAX_CHANNELS;
    cur->min_rate = rate;
    cur->max_rate = rate;
    cur->default_rate = rate;
    cur->latency_lo = 0;
    cur->latency_hi = 0;
    collection->count += 1;
  }

  collection->device = devices;

  return CUBEB_OK;
}

static int
cbjack_device_collection_destroy(cubeb * /*ctx*/,
                                 cubeb_device_collection * collection)
{
  XASSERT(collection);
  delete[] collection->device;
  return CUBEB_OK;
}
