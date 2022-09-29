/*
 * Copyright © 2013 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#if !defined(NDEBUG)
#define NDEBUG
#endif
#include <android/log.h>
#include <assert.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>

#include "android/audiotrack_definitions.h"
#include "cubeb-internal.h"
#include "cubeb/cubeb.h"

#ifndef ALOG
#if defined(DEBUG) || defined(FORCE_ALOG)
#define ALOG(args...)                                                          \
  __android_log_print(ANDROID_LOG_INFO, "Gecko - Cubeb", ##args)
#else
#define ALOG(args...)
#endif
#endif

/**
 * A lot of bytes for safety. It should be possible to bring this down a bit. */
#define SIZE_AUDIOTRACK_INSTANCE 256

/**
 * call dlsym to get the symbol |mangled_name|, handle the error and store the
 * pointer in |pointer|. Because depending on Android version, we want different
 * symbols, not finding a symbol is not an error. */
#define DLSYM_DLERROR(mangled_name, pointer, lib)                              \
  do {                                                                         \
    pointer = dlsym(lib, mangled_name);                                        \
    if (!pointer) {                                                            \
      ALOG("error while loading %stm: %stm\n", mangled_name, dlerror());       \
    } else {                                                                   \
      ALOG("%stm: OK", mangled_name);                                          \
    }                                                                          \
  } while (0);

static struct cubeb_ops const audiotrack_ops;
void
audiotrack_destroy(cubeb * context);
void
audiotrack_stream_destroy(cubeb_stream * stream);

struct AudioTrack {
  /* only available on ICS and later. The second int paramter is in fact of type
   * audio_stream_type_t. */
  /* static */ status_t (*get_min_frame_count)(int * frame_count,
                                               int stream_type, uint32_t rate);
  /* if we have a recent ctor, but can't find the above symbol, we
   * can get the minimum frame count with this signature, and we are
   * running gingerbread. */
  /* static */ status_t (*get_min_frame_count_gingerbread)(int * frame_count,
                                                           int stream_type,
                                                           uint32_t rate);
  void * (*ctor)(void * instance, int, unsigned int, int, int, int,
                 unsigned int, void (*)(int, void *, void *), void *, int, int);
  void * (*dtor)(void * instance);
  void (*start)(void * instance);
  void (*pause)(void * instance);
  uint32_t (*latency)(void * instance);
  status_t (*check)(void * instance);
  status_t (*get_position)(void * instance, uint32_t * position);
  /* static */ int (*get_output_samplingrate)(int * samplerate, int stream);
  status_t (*set_marker_position)(void * instance, unsigned int);
  status_t (*set_volume)(void * instance, float left, float right);
};

struct cubeb {
  struct cubeb_ops const * ops;
  void * library;
  struct AudioTrack klass;
};

struct cubeb_stream {
  /* Note: Must match cubeb_stream layout in cubeb.c. */
  cubeb * context;
  void * user_ptr;
  /**/
  cubeb_stream_params params;
  cubeb_data_callback data_callback;
  cubeb_state_callback state_callback;
  void * instance;
  /* Number of frames that have been passed to the AudioTrack callback */
  long unsigned written;
  int draining;
};

static void
audiotrack_refill(int event, void * user, void * info)
{
  cubeb_stream * stream = user;
  switch (event) {
  case EVENT_MORE_DATA: {
    long got = 0;
    struct Buffer * b = (struct Buffer *)info;

    if (stream->draining) {
      return;
    }

    got = stream->data_callback(stream, stream->user_ptr, NULL, b->raw,
                                b->frameCount);

    stream->written += got;

    if (got != (long)b->frameCount) {
      stream->draining = 1;
      /* set a marker so we are notified when the are done draining, that is,
       * when every frame has been played by android. */
      stream->context->klass.set_marker_position(stream->instance,
                                                 stream->written);
    }

    break;
  }
  case EVENT_UNDERRUN:
    ALOG("underrun in cubeb backend.");
    break;
  case EVENT_LOOP_END:
    assert(0 && "We don't support the loop feature of audiotrack.");
    break;
  case EVENT_MARKER:
    assert(stream->draining);
    stream->state_callback(stream, stream->user_ptr, CUBEB_STATE_DRAINED);
    break;
  case EVENT_NEW_POS:
    assert(
        0 &&
        "We don't support the setPositionUpdatePeriod feature of audiotrack.");
    break;
  case EVENT_BUFFER_END:
    assert(0 && "Should not happen.");
    break;
  }
}

/* We are running on gingerbread if we found the gingerbread signature for
 * getMinFrameCount */
static int
audiotrack_version_is_gingerbread(cubeb * ctx)
{
  return ctx->klass.get_min_frame_count_gingerbread != NULL;
}

int
audiotrack_get_min_frame_count(cubeb * ctx, cubeb_stream_params * params,
                               int * min_frame_count)
{
  status_t status;
  /* Recent Android have a getMinFrameCount method. */
  if (!audiotrack_version_is_gingerbread(ctx)) {
    status = ctx->klass.get_min_frame_count(
        min_frame_count, AUDIO_STREAM_TYPE_MUSIC, params->rate);
  } else {
    status = ctx->klass.get_min_frame_count_gingerbread(
        min_frame_count, AUDIO_STREAM_TYPE_MUSIC, params->rate);
  }
  if (status != 0) {
    ALOG("error getting the min frame count");
    return CUBEB_ERROR;
  }
  return CUBEB_OK;
}

int
audiotrack_init(cubeb ** context, char const * context_name)
{
  cubeb * ctx;
  struct AudioTrack * c;

  assert(context);
  *context = NULL;

  ctx = calloc(1, sizeof(*ctx));
  assert(ctx);

  /* If we use an absolute path here ("/system/lib/libmedia.so"), and on Android
   * 2.2, the dlopen succeeds, all the dlsym succeed, but a segfault happens on
   * the first call to a dlsym'ed function. Somehow this does not happen when
   * using only the name of the library. */
  ctx->library = dlopen("libmedia.so", RTLD_LAZY);
  if (!ctx->library) {
    ALOG("dlopen error: %s.", dlerror());
    free(ctx);
    return CUBEB_ERROR;
  }

  /* Recent Android first, then Gingerbread. */
  DLSYM_DLERROR("_ZN7android10AudioTrackC1EijiiijPFviPvS1_ES1_ii",
                ctx->klass.ctor, ctx->library);
  DLSYM_DLERROR("_ZN7android10AudioTrackD1Ev", ctx->klass.dtor, ctx->library);

  DLSYM_DLERROR("_ZNK7android10AudioTrack7latencyEv", ctx->klass.latency,
                ctx->library);
  DLSYM_DLERROR("_ZNK7android10AudioTrack9initCheckEv", ctx->klass.check,
                ctx->library);

  DLSYM_DLERROR("_ZN7android11AudioSystem21getOutputSamplingRateEPii",
                ctx->klass.get_output_samplingrate, ctx->library);

  /* |getMinFrameCount| is available on gingerbread and ICS with different
   * signatures. */
  DLSYM_DLERROR(
      "_ZN7android10AudioTrack16getMinFrameCountEPi19audio_stream_type_tj",
      ctx->klass.get_min_frame_count, ctx->library);
  if (!ctx->klass.get_min_frame_count) {
    DLSYM_DLERROR("_ZN7android10AudioTrack16getMinFrameCountEPiij",
                  ctx->klass.get_min_frame_count_gingerbread, ctx->library);
  }

  DLSYM_DLERROR("_ZN7android10AudioTrack5startEv", ctx->klass.start,
                ctx->library);
  DLSYM_DLERROR("_ZN7android10AudioTrack5pauseEv", ctx->klass.pause,
                ctx->library);
  DLSYM_DLERROR("_ZN7android10AudioTrack11getPositionEPj",
                ctx->klass.get_position, ctx->library);
  DLSYM_DLERROR("_ZN7android10AudioTrack17setMarkerPositionEj",
                ctx->klass.set_marker_position, ctx->library);
  DLSYM_DLERROR("_ZN7android10AudioTrack9setVolumeEff", ctx->klass.set_volume,
                ctx->library);

  /* check that we have a combination of symbol that makes sense */
  c = &ctx->klass;
  if (!(c->ctor && c->dtor && c->latency && c->check &&
        /* at least one way to get the minimum frame count to request. */
        (c->get_min_frame_count || c->get_min_frame_count_gingerbread) &&
        c->start && c->pause && c->get_position && c->set_marker_position)) {
    ALOG("Could not find all the symbols we need.");
    audiotrack_destroy(ctx);
    return CUBEB_ERROR;
  }

  ctx->ops = &audiotrack_ops;

  *context = ctx;

  return CUBEB_OK;
}

char const *
audiotrack_get_backend_id(cubeb * context)
{
  return "audiotrack";
}

static int
audiotrack_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
  assert(ctx && max_channels);

  /* The android mixer handles up to two channels, see
     http://androidxref.com/4.2.2_r1/xref/frameworks/av/services/audioflinger/AudioFlinger.h#67
   */
  *max_channels = 2;

  return CUBEB_OK;
}

static int
audiotrack_get_min_latency(cubeb * ctx, cubeb_stream_params params,
                           uint32_t * latency_ms)
{
  /* We always use the lowest latency possible when using this backend (see
   * audiotrack_stream_init), so this value is not going to be used. */
  int r;

  r = audiotrack_get_min_frame_count(ctx, &params, (int *)latency_ms);
  if (r != CUBEB_OK) {
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

static int
audiotrack_get_preferred_sample_rate(cubeb * ctx, uint32_t * rate)
{
  status_t r;

  r = ctx->klass.get_output_samplingrate((int32_t *)rate, 3 /* MUSIC */);

  return r == 0 ? CUBEB_OK : CUBEB_ERROR;
}

void
audiotrack_destroy(cubeb * context)
{
  assert(context);

  dlclose(context->library);

  free(context);
}

int
audiotrack_stream_init(cubeb * ctx, cubeb_stream ** stream,
                       char const * stream_name, cubeb_devid input_device,
                       cubeb_stream_params * input_stream_params,
                       cubeb_devid output_device,
                       cubeb_stream_params * output_stream_params,
                       unsigned int latency, cubeb_data_callback data_callback,
                       cubeb_state_callback state_callback, void * user_ptr)
{
  cubeb_stream * stm;
  int32_t channels;
  uint32_t min_frame_count;

  assert(ctx && stream);

  assert(!input_stream_params && "not supported");
  if (input_device || output_device) {
    /* Device selection not yet implemented. */
    return CUBEB_ERROR_DEVICE_UNAVAILABLE;
  }

  if (output_stream_params->format == CUBEB_SAMPLE_FLOAT32LE ||
      output_stream_params->format == CUBEB_SAMPLE_FLOAT32BE) {
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  if (audiotrack_get_min_frame_count(ctx, output_stream_params,
                                     (int *)&min_frame_count)) {
    return CUBEB_ERROR;
  }

  stm = calloc(1, sizeof(*stm));
  assert(stm);

  stm->context = ctx;
  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->user_ptr = user_ptr;
  stm->params = *output_stream_params;

  stm->instance = calloc(SIZE_AUDIOTRACK_INSTANCE, 1);
  (*(uint32_t *)((intptr_t)stm->instance + SIZE_AUDIOTRACK_INSTANCE - 4)) =
      0xbaadbaad;
  assert(stm->instance && "cubeb: EOM");

  /* gingerbread uses old channel layout enum */
  if (audiotrack_version_is_gingerbread(ctx)) {
    channels = stm->params.channels == 2 ? AUDIO_CHANNEL_OUT_STEREO_Legacy
                                         : AUDIO_CHANNEL_OUT_MONO_Legacy;
  } else {
    channels = stm->params.channels == 2 ? AUDIO_CHANNEL_OUT_STEREO_ICS
                                         : AUDIO_CHANNEL_OUT_MONO_ICS;
  }

  ctx->klass.ctor(stm->instance, AUDIO_STREAM_TYPE_MUSIC, stm->params.rate,
                  AUDIO_FORMAT_PCM_16_BIT, channels, min_frame_count, 0,
                  audiotrack_refill, stm, 0, 0);

  assert((*(uint32_t *)((intptr_t)stm->instance + SIZE_AUDIOTRACK_INSTANCE -
                        4)) == 0xbaadbaad);

  if (ctx->klass.check(stm->instance)) {
    ALOG("stream not initialized properly.");
    audiotrack_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  *stream = stm;

  return CUBEB_OK;
}

void
audiotrack_stream_destroy(cubeb_stream * stream)
{
  assert(stream->context);

  stream->context->klass.dtor(stream->instance);

  free(stream->instance);
  stream->instance = NULL;
  free(stream);
}

int
audiotrack_stream_start(cubeb_stream * stream)
{
  assert(stream->instance);

  stream->context->klass.start(stream->instance);
  stream->state_callback(stream, stream->user_ptr, CUBEB_STATE_STARTED);

  return CUBEB_OK;
}

int
audiotrack_stream_stop(cubeb_stream * stream)
{
  assert(stream->instance);

  stream->context->klass.pause(stream->instance);
  stream->state_callback(stream, stream->user_ptr, CUBEB_STATE_STOPPED);

  return CUBEB_OK;
}

int
audiotrack_stream_get_position(cubeb_stream * stream, uint64_t * position)
{
  uint32_t p;

  assert(stream->instance && position);
  stream->context->klass.get_position(stream->instance, &p);
  *position = p;

  return CUBEB_OK;
}

int
audiotrack_stream_get_latency(cubeb_stream * stream, uint32_t * latency)
{
  assert(stream->instance && latency);

  /* Android returns the latency in ms, we want it in frames. */
  *latency = stream->context->klass.latency(stream->instance);
  /* with rate <= 96000, we won't overflow until 44.739 seconds of latency */
  *latency = (*latency * stream->params.rate) / 1000;

  return 0;
}

int
audiotrack_stream_set_volume(cubeb_stream * stream, float volume)
{
  status_t status;

  status = stream->context->klass.set_volume(stream->instance, volume, volume);

  if (status) {
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

static struct cubeb_ops const audiotrack_ops = {
    .init = audiotrack_init,
    .get_backend_id = audiotrack_get_backend_id,
    .get_max_channel_count = audiotrack_get_max_channel_count,
    .get_min_latency = audiotrack_get_min_latency,
    .get_preferred_sample_rate = audiotrack_get_preferred_sample_rate,
    .enumerate_devices = NULL,
    .device_collection_destroy = NULL,
    .destroy = audiotrack_destroy,
    .stream_init = audiotrack_stream_init,
    .stream_destroy = audiotrack_stream_destroy,
    .stream_start = audiotrack_stream_start,
    .stream_stop = audiotrack_stream_stop,
    .stream_get_position = audiotrack_stream_get_position,
    .stream_get_latency = audiotrack_stream_get_latency,
    .stream_get_input_latency = NULL,
    .stream_set_volume = audiotrack_stream_set_volume,
    .stream_set_name = NULL,
    .stream_get_current_device = NULL,
    .stream_device_destroy = NULL,
    .stream_register_device_changed_callback = NULL,
    .register_device_collection_changed = NULL};
