/*
 * Copyright © 2015 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <sys/fmutex.h>

#include <kai.h>

#include "cubeb-internal.h"
#include "cubeb/cubeb.h"

/* We don't support more than 2 channels in KAI */
#define MAX_CHANNELS 2

#define NBUFS 2
#define FRAME_SIZE 2048

struct cubeb_stream_item {
  cubeb_stream * stream;
};

static struct cubeb_ops const kai_ops;

struct cubeb {
  struct cubeb_ops const * ops;
};

struct cubeb_stream {
  /* Note: Must match cubeb_stream layout in cubeb.c. */
  cubeb * context;
  void * user_ptr;
  /**/
  cubeb_stream_params params;
  cubeb_data_callback data_callback;
  cubeb_state_callback state_callback;

  HKAI hkai;
  KAISPEC spec;
  uint64_t total_frames;
  float soft_volume;
  _fmutex mutex;
  float float_buffer[FRAME_SIZE * MAX_CHANNELS];
};

static inline long
frames_to_bytes(long frames, cubeb_stream_params params)
{
  return frames * 2 * params.channels; /* 2 bytes per frame */
}

static inline long
bytes_to_frames(long bytes, cubeb_stream_params params)
{
  return bytes / 2 / params.channels; /* 2 bytes per frame */
}

static void
kai_destroy(cubeb * ctx);

/*static*/ int
kai_init(cubeb ** context, char const * context_name)
{
  cubeb * ctx;

  XASSERT(context);
  *context = NULL;

  if (kaiInit(KAIM_AUTO))
    return CUBEB_ERROR;

  ctx = calloc(1, sizeof(*ctx));
  XASSERT(ctx);

  ctx->ops = &kai_ops;

  *context = ctx;

  return CUBEB_OK;
}

static char const *
kai_get_backend_id(cubeb * ctx)
{
  return "kai";
}

static void
kai_destroy(cubeb * ctx)
{
  kaiDone();

  free(ctx);
}

static void
float_to_s16ne(int16_t * dst, float * src, size_t n)
{
  long l;

  while (n--) {
    l = lrintf(*src++ * 0x8000);
    if (l > 32767)
      l = 32767;
    if (l < -32768)
      l = -32768;
    *dst++ = (int16_t)l;
  }
}

static ULONG APIENTRY
kai_callback(PVOID cbdata, PVOID buffer, ULONG len)
{
  cubeb_stream * stm = cbdata;
  void * p;
  long wanted_frames;
  long frames;
  float soft_volume;
  int elements = len / sizeof(int16_t);

  p = stm->params.format == CUBEB_SAMPLE_FLOAT32NE ? stm->float_buffer : buffer;

  wanted_frames = bytes_to_frames(len, stm->params);
  frames = stm->data_callback(stm, stm->user_ptr, NULL, p, wanted_frames);

  _fmutex_request(&stm->mutex, 0);
  stm->total_frames += frames;
  soft_volume = stm->soft_volume;
  _fmutex_release(&stm->mutex);

  if (frames < wanted_frames)
    stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);

  if (stm->params.format == CUBEB_SAMPLE_FLOAT32NE)
    float_to_s16ne(buffer, p, elements);

  if (soft_volume != -1.0f) {
    int16_t * b = buffer;
    int i;

    for (i = 0; i < elements; i++)
      *b++ *= soft_volume;
  }

  return frames_to_bytes(frames, stm->params);
}

static void
kai_stream_destroy(cubeb_stream * stm);

static int
kai_stream_init(cubeb * context, cubeb_stream ** stream,
                char const * stream_name, cubeb_devid input_device,
                cubeb_stream_params * input_stream_params,
                cubeb_devid output_device,
                cubeb_stream_params * output_stream_params,
                unsigned int latency, cubeb_data_callback data_callback,
                cubeb_state_callback state_callback, void * user_ptr)
{
  cubeb_stream * stm;
  KAISPEC wanted_spec;

  XASSERT(!input_stream_params && "not supported.");
  if (input_device || output_device) {
    /* Device selection not yet implemented. */
    return CUBEB_ERROR_DEVICE_UNAVAILABLE;
  }

  if (!output_stream_params)
    return CUBEB_ERROR_INVALID_PARAMETER;

  // Loopback is unsupported
  if (output_stream_params->prefs & CUBEB_STREAM_PREF_LOOPBACK) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  if (output_stream_params->channels < 1 ||
      output_stream_params->channels > MAX_CHANNELS)
    return CUBEB_ERROR_INVALID_FORMAT;

  XASSERT(context);
  XASSERT(stream);

  *stream = NULL;

  stm = calloc(1, sizeof(*stm));
  XASSERT(stm);

  stm->context = context;
  stm->params = *output_stream_params;
  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->user_ptr = user_ptr;
  stm->soft_volume = -1.0f;

  if (_fmutex_create(&stm->mutex, 0)) {
    free(stm);
    return CUBEB_ERROR;
  }

  wanted_spec.usDeviceIndex = 0;
  wanted_spec.ulType = KAIT_PLAY;
  wanted_spec.ulBitsPerSample = BPS_16;
  wanted_spec.ulSamplingRate = stm->params.rate;
  wanted_spec.ulDataFormat = MCI_WAVE_FORMAT_PCM;
  wanted_spec.ulChannels = stm->params.channels;
  wanted_spec.ulNumBuffers = NBUFS;
  wanted_spec.ulBufferSize = frames_to_bytes(FRAME_SIZE, stm->params);
  wanted_spec.fShareable = TRUE;
  wanted_spec.pfnCallBack = kai_callback;
  wanted_spec.pCallBackData = stm;

  if (kaiOpen(&wanted_spec, &stm->spec, &stm->hkai)) {
    _fmutex_close(&stm->mutex);
    free(stm);
    return CUBEB_ERROR;
  }

  *stream = stm;

  return CUBEB_OK;
}

static void
kai_stream_destroy(cubeb_stream * stm)
{
  kaiClose(stm->hkai);
  _fmutex_close(&stm->mutex);
  free(stm);
}

static int
kai_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
  XASSERT(ctx && max_channels);

  *max_channels = MAX_CHANNELS;

  return CUBEB_OK;
}

static int
kai_get_min_latency(cubeb * ctx, cubeb_stream_params params, uint32_t * latency)
{
  /* We have at least two buffers. One is being played, the other one is being
     filled. So there is as much latency as one buffer. */
  *latency = FRAME_SIZE;

  return CUBEB_OK;
}

static int
kai_get_preferred_sample_rate(cubeb * ctx, uint32_t * rate)
{
  cubeb_stream_params params;
  KAISPEC wanted_spec;
  KAISPEC spec;
  HKAI hkai;

  params.format = CUBEB_SAMPLE_S16NE;
  params.rate = 48000;
  params.channels = 2;

  wanted_spec.usDeviceIndex = 0;
  wanted_spec.ulType = KAIT_PLAY;
  wanted_spec.ulBitsPerSample = BPS_16;
  wanted_spec.ulSamplingRate = params.rate;
  wanted_spec.ulDataFormat = MCI_WAVE_FORMAT_PCM;
  wanted_spec.ulChannels = params.channels;
  wanted_spec.ulNumBuffers = NBUFS;
  wanted_spec.ulBufferSize = frames_to_bytes(FRAME_SIZE, params);
  wanted_spec.fShareable = TRUE;
  wanted_spec.pfnCallBack = kai_callback;
  wanted_spec.pCallBackData = NULL;

  /* Test 48KHz */
  if (kaiOpen(&wanted_spec, &spec, &hkai)) {
    /* Not supported. Fall back to 44.1KHz */
    params.rate = 44100;
  } else {
    /* Supported. Use 48KHz */
    kaiClose(hkai);
  }

  *rate = params.rate;

  return CUBEB_OK;
}

static int
kai_stream_start(cubeb_stream * stm)
{
  if (kaiPlay(stm->hkai))
    return CUBEB_ERROR;

  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STARTED);

  return CUBEB_OK;
}

static int
kai_stream_stop(cubeb_stream * stm)
{
  if (kaiStop(stm->hkai))
    return CUBEB_ERROR;

  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STOPPED);

  return CUBEB_OK;
}

static int
kai_stream_get_position(cubeb_stream * stm, uint64_t * position)
{
  _fmutex_request(&stm->mutex, 0);
  *position = stm->total_frames;
  _fmutex_release(&stm->mutex);

  return CUBEB_OK;
}

static int
kai_stream_get_latency(cubeb_stream * stm, uint32_t * latency)
{
  /* Out of buffers, one is being played, the others are being filled.
     So there is as much latency as total buffers - 1. */
  *latency = bytes_to_frames(stm->spec.ulBufferSize, stm->params) *
             (stm->spec.ulNumBuffers - 1);

  return CUBEB_OK;
}

static int
kai_stream_set_volume(cubeb_stream * stm, float volume)
{
  _fmutex_request(&stm->mutex, 0);
  stm->soft_volume = volume;
  _fmutex_release(&stm->mutex);

  return CUBEB_OK;
}

static struct cubeb_ops const kai_ops = {
    /*.init =*/kai_init,
    /*.get_backend_id =*/kai_get_backend_id,
    /*.get_max_channel_count=*/kai_get_max_channel_count,
    /*.get_min_latency=*/kai_get_min_latency,
    /*.get_preferred_sample_rate =*/kai_get_preferred_sample_rate,
    /*.get_preferred_channel_layout =*/NULL,
    /*.enumerate_devices =*/NULL,
    /*.device_collection_destroy =*/NULL,
    /*.destroy =*/kai_destroy,
    /*.stream_init =*/kai_stream_init,
    /*.stream_destroy =*/kai_stream_destroy,
    /*.stream_start =*/kai_stream_start,
    /*.stream_stop =*/kai_stream_stop,
    /*.stream_get_position =*/kai_stream_get_position,
    /*.stream_get_latency = */ kai_stream_get_latency,
    /*.stream_get_input_latency = */ NULL,
    /*.stream_set_volume =*/kai_stream_set_volume,
    /*.stream_set_name =*/NULL,
    /*.stream_get_current_device =*/NULL,
    /*.stream_device_destroy =*/NULL,
    /*.stream_register_device_changed_callback=*/NULL,
    /*.register_device_collection_changed=*/NULL};
