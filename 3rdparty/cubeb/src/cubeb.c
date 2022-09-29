/*
 * Copyright © 2013 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#undef NDEBUG
#include "cubeb/cubeb.h"
#include "cubeb-internal.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#define NELEMS(x) ((int)(sizeof(x) / sizeof(x[0])))

struct cubeb {
  struct cubeb_ops * ops;
};

struct cubeb_stream {
  /*
   * Note: All implementations of cubeb_stream must keep the following
   * layout.
   */
  struct cubeb * context;
  void * user_ptr;
};

#if defined(USE_PULSE)
int
pulse_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_PULSE_RUST)
int
pulse_rust_init(cubeb ** contet, char const * context_name);
#endif
#if defined(USE_JACK)
int
jack_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_ALSA)
int
alsa_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_AUDIOUNIT)
int
audiounit_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_AUDIOUNIT_RUST)
int
audiounit_rust_init(cubeb ** contet, char const * context_name);
#endif
#if defined(USE_WINMM)
int
winmm_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_WASAPI)
int
wasapi_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_SNDIO)
int
sndio_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_SUN)
int
sun_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_OPENSL)
int
opensl_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_OSS)
int
oss_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_AAUDIO)
int
aaudio_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_AUDIOTRACK)
int
audiotrack_init(cubeb ** context, char const * context_name);
#endif
#if defined(USE_KAI)
int
kai_init(cubeb ** context, char const * context_name);
#endif

static int
validate_stream_params(cubeb_stream_params * input_stream_params,
                       cubeb_stream_params * output_stream_params)
{
  XASSERT(input_stream_params || output_stream_params);
  if (output_stream_params) {
    if (output_stream_params->rate < 1000 ||
        output_stream_params->rate > 192000 ||
        output_stream_params->channels < 1 ||
        output_stream_params->channels > UINT8_MAX) {
      return CUBEB_ERROR_INVALID_FORMAT;
    }
  }
  if (input_stream_params) {
    if (input_stream_params->rate < 1000 ||
        input_stream_params->rate > 192000 ||
        input_stream_params->channels < 1 ||
        input_stream_params->channels > UINT8_MAX) {
      return CUBEB_ERROR_INVALID_FORMAT;
    }
  }
  // Rate and sample format must be the same for input and output, if using a
  // duplex stream
  if (input_stream_params && output_stream_params) {
    if (input_stream_params->rate != output_stream_params->rate ||
        input_stream_params->format != output_stream_params->format) {
      return CUBEB_ERROR_INVALID_FORMAT;
    }
  }

  cubeb_stream_params * params =
      input_stream_params ? input_stream_params : output_stream_params;

  switch (params->format) {
  case CUBEB_SAMPLE_S16LE:
  case CUBEB_SAMPLE_S16BE:
  case CUBEB_SAMPLE_FLOAT32LE:
  case CUBEB_SAMPLE_FLOAT32BE:
    return CUBEB_OK;
  }

  return CUBEB_ERROR_INVALID_FORMAT;
}

static int
validate_latency(int latency)
{
  if (latency < 1 || latency > 96000) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }
  return CUBEB_OK;
}

int
cubeb_init(cubeb ** context, char const * context_name,
           char const * backend_name)
{
  int (*init_oneshot)(cubeb **, char const *) = NULL;

  if (backend_name != NULL) {
    if (!strcmp(backend_name, "pulse")) {
#if defined(USE_PULSE)
      init_oneshot = pulse_init;
#endif
    } else if (!strcmp(backend_name, "pulse-rust")) {
#if defined(USE_PULSE_RUST)
      init_oneshot = pulse_rust_init;
#endif
    } else if (!strcmp(backend_name, "jack")) {
#if defined(USE_JACK)
      init_oneshot = jack_init;
#endif
    } else if (!strcmp(backend_name, "alsa")) {
#if defined(USE_ALSA)
      init_oneshot = alsa_init;
#endif
    } else if (!strcmp(backend_name, "audiounit")) {
#if defined(USE_AUDIOUNIT)
      init_oneshot = audiounit_init;
#endif
    } else if (!strcmp(backend_name, "audiounit-rust")) {
#if defined(USE_AUDIOUNIT_RUST)
      init_oneshot = audiounit_rust_init;
#endif
    } else if (!strcmp(backend_name, "wasapi")) {
#if defined(USE_WASAPI)
      init_oneshot = wasapi_init;
#endif
    } else if (!strcmp(backend_name, "winmm")) {
#if defined(USE_WINMM)
      init_oneshot = winmm_init;
#endif
    } else if (!strcmp(backend_name, "sndio")) {
#if defined(USE_SNDIO)
      init_oneshot = sndio_init;
#endif
    } else if (!strcmp(backend_name, "sun")) {
#if defined(USE_SUN)
      init_oneshot = sun_init;
#endif
    } else if (!strcmp(backend_name, "opensl")) {
#if defined(USE_OPENSL)
      init_oneshot = opensl_init;
#endif
    } else if (!strcmp(backend_name, "oss")) {
#if defined(USE_OSS)
      init_oneshot = oss_init;
#endif
    } else if (!strcmp(backend_name, "aaudio")) {
#if defined(USE_AAUDIO)
      init_oneshot = aaudio_init;
#endif
    } else if (!strcmp(backend_name, "audiotrack")) {
#if defined(USE_AUDIOTRACK)
      init_oneshot = audiotrack_init;
#endif
    } else if (!strcmp(backend_name, "kai")) {
#if defined(USE_KAI)
      init_oneshot = kai_init;
#endif
    } else {
      /* Already set */
    }
  }

  int (*default_init[])(cubeb **, char const *) = {
    /*
     * init_oneshot must be at the top to allow user
     * to override all other choices
     */
    init_oneshot,
#if defined(USE_PULSE_RUST)
    pulse_rust_init,
#endif
#if defined(USE_PULSE)
    pulse_init,
#endif
#if defined(USE_JACK)
    jack_init,
#endif
#if defined(USE_SNDIO)
    sndio_init,
#endif
#if defined(USE_ALSA)
    alsa_init,
#endif
#if defined(USE_OSS)
    oss_init,
#endif
#if defined(USE_AUDIOUNIT_RUST)
    audiounit_rust_init,
#endif
#if defined(USE_AUDIOUNIT)
    audiounit_init,
#endif
#if defined(USE_WASAPI)
    wasapi_init,
#endif
#if defined(USE_WINMM)
    winmm_init,
#endif
#if defined(USE_SUN)
    sun_init,
#endif
#if defined(USE_OPENSL)
    opensl_init,
#endif
  // TODO: should probably be preferred over OpenSLES when available.
  // Initialization will fail on old android devices.
#if defined(USE_AAUDIO)
    aaudio_init,
#endif
#if defined(USE_AUDIOTRACK)
    audiotrack_init,
#endif
#if defined(USE_KAI)
    kai_init,
#endif
  };
  int i;

  if (!context) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

#define OK(fn) assert((*context)->ops->fn)
  for (i = 0; i < NELEMS(default_init); ++i) {
    if (default_init[i] && default_init[i](context, context_name) == CUBEB_OK) {
      /* Assert that the minimal API is implemented. */
      OK(get_backend_id);
      OK(destroy);
      OK(stream_init);
      OK(stream_destroy);
      OK(stream_start);
      OK(stream_stop);
      OK(stream_get_position);
      return CUBEB_OK;
    }
  }
  return CUBEB_ERROR;
}

char const *
cubeb_get_backend_id(cubeb * context)
{
  if (!context) {
    return NULL;
  }

  return context->ops->get_backend_id(context);
}

int
cubeb_get_max_channel_count(cubeb * context, uint32_t * max_channels)
{
  if (!context || !max_channels) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!context->ops->get_max_channel_count) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return context->ops->get_max_channel_count(context, max_channels);
}

int
cubeb_get_min_latency(cubeb * context, cubeb_stream_params * params,
                      uint32_t * latency_ms)
{
  if (!context || !params || !latency_ms) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!context->ops->get_min_latency) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return context->ops->get_min_latency(context, *params, latency_ms);
}

int
cubeb_get_preferred_sample_rate(cubeb * context, uint32_t * rate)
{
  if (!context || !rate) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!context->ops->get_preferred_sample_rate) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return context->ops->get_preferred_sample_rate(context, rate);
}

void
cubeb_destroy(cubeb * context)
{
  if (!context) {
    return;
  }

  context->ops->destroy(context);
}

int
cubeb_stream_init(cubeb * context, cubeb_stream ** stream,
                  char const * stream_name, cubeb_devid input_device,
                  cubeb_stream_params * input_stream_params,
                  cubeb_devid output_device,
                  cubeb_stream_params * output_stream_params,
                  unsigned int latency, cubeb_data_callback data_callback,
                  cubeb_state_callback state_callback, void * user_ptr)
{
  int r;

  if (!context || !stream || !data_callback || !state_callback) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if ((r = validate_stream_params(input_stream_params, output_stream_params)) !=
          CUBEB_OK ||
      (r = validate_latency(latency)) != CUBEB_OK) {
    return r;
  }

  r = context->ops->stream_init(context, stream, stream_name, input_device,
                                input_stream_params, output_device,
                                output_stream_params, latency, data_callback,
                                state_callback, user_ptr);

  if (r == CUBEB_ERROR_INVALID_FORMAT) {
    LOG("Invalid format, %p %p %d %d", output_stream_params,
        input_stream_params,
        output_stream_params && output_stream_params->format,
        input_stream_params && input_stream_params->format);
  }

  return r;
}

void
cubeb_stream_destroy(cubeb_stream * stream)
{
  if (!stream) {
    return;
  }

  stream->context->ops->stream_destroy(stream);
}

int
cubeb_stream_start(cubeb_stream * stream)
{
  if (!stream) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  return stream->context->ops->stream_start(stream);
}

int
cubeb_stream_stop(cubeb_stream * stream)
{
  if (!stream) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  return stream->context->ops->stream_stop(stream);
}

int
cubeb_stream_get_position(cubeb_stream * stream, uint64_t * position)
{
  if (!stream || !position) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  return stream->context->ops->stream_get_position(stream, position);
}

int
cubeb_stream_get_latency(cubeb_stream * stream, uint32_t * latency)
{
  if (!stream || !latency) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!stream->context->ops->stream_get_latency) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return stream->context->ops->stream_get_latency(stream, latency);
}

int
cubeb_stream_get_input_latency(cubeb_stream * stream, uint32_t * latency)
{
  if (!stream || !latency) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!stream->context->ops->stream_get_input_latency) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return stream->context->ops->stream_get_input_latency(stream, latency);
}

int
cubeb_stream_set_volume(cubeb_stream * stream, float volume)
{
  if (!stream || volume > 1.0 || volume < 0.0) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!stream->context->ops->stream_set_volume) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return stream->context->ops->stream_set_volume(stream, volume);
}

int
cubeb_stream_set_name(cubeb_stream * stream, char const * stream_name)
{
  if (!stream || !stream_name) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!stream->context->ops->stream_set_name) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return stream->context->ops->stream_set_name(stream, stream_name);
}

int
cubeb_stream_get_current_device(cubeb_stream * stream,
                                cubeb_device ** const device)
{
  if (!stream || !device) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!stream->context->ops->stream_get_current_device) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return stream->context->ops->stream_get_current_device(stream, device);
}

int
cubeb_stream_device_destroy(cubeb_stream * stream, cubeb_device * device)
{
  if (!stream || !device) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!stream->context->ops->stream_device_destroy) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return stream->context->ops->stream_device_destroy(stream, device);
}

int
cubeb_stream_register_device_changed_callback(
    cubeb_stream * stream,
    cubeb_device_changed_callback device_changed_callback)
{
  if (!stream) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (!stream->context->ops->stream_register_device_changed_callback) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return stream->context->ops->stream_register_device_changed_callback(
      stream, device_changed_callback);
}

void *
cubeb_stream_user_ptr(cubeb_stream * stream)
{
  if (!stream) {
    return NULL;
  }

  return stream->user_ptr;
}

static void
log_device(cubeb_device_info * device_info)
{
  char devfmts[128] = "";
  const char *devtype, *devstate, *devdeffmt;

  switch (device_info->type) {
  case CUBEB_DEVICE_TYPE_INPUT:
    devtype = "input";
    break;
  case CUBEB_DEVICE_TYPE_OUTPUT:
    devtype = "output";
    break;
  case CUBEB_DEVICE_TYPE_UNKNOWN:
  default:
    devtype = "unknown?";
    break;
  };

  switch (device_info->state) {
  case CUBEB_DEVICE_STATE_DISABLED:
    devstate = "disabled";
    break;
  case CUBEB_DEVICE_STATE_UNPLUGGED:
    devstate = "unplugged";
    break;
  case CUBEB_DEVICE_STATE_ENABLED:
    devstate = "enabled";
    break;
  default:
    devstate = "unknown?";
    break;
  };

  switch (device_info->default_format) {
  case CUBEB_DEVICE_FMT_S16LE:
    devdeffmt = "S16LE";
    break;
  case CUBEB_DEVICE_FMT_S16BE:
    devdeffmt = "S16BE";
    break;
  case CUBEB_DEVICE_FMT_F32LE:
    devdeffmt = "F32LE";
    break;
  case CUBEB_DEVICE_FMT_F32BE:
    devdeffmt = "F32BE";
    break;
  default:
    devdeffmt = "unknown?";
    break;
  };

  if (device_info->format & CUBEB_DEVICE_FMT_S16LE) {
    strcat(devfmts, " S16LE");
  }
  if (device_info->format & CUBEB_DEVICE_FMT_S16BE) {
    strcat(devfmts, " S16BE");
  }
  if (device_info->format & CUBEB_DEVICE_FMT_F32LE) {
    strcat(devfmts, " F32LE");
  }
  if (device_info->format & CUBEB_DEVICE_FMT_F32BE) {
    strcat(devfmts, " F32BE");
  }

  LOG("DeviceID: \"%s\"%s\n"
      "\tName:\t\"%s\"\n"
      "\tGroup:\t\"%s\"\n"
      "\tVendor:\t\"%s\"\n"
      "\tType:\t%s\n"
      "\tState:\t%s\n"
      "\tMaximum channels:\t%u\n"
      "\tFormat:\t%s (0x%x) (default: %s)\n"
      "\tRate:\t[%u, %u] (default: %u)\n"
      "\tLatency: lo %u frames, hi %u frames",
      device_info->device_id, device_info->preferred ? " (PREFERRED)" : "",
      device_info->friendly_name, device_info->group_id,
      device_info->vendor_name, devtype, devstate, device_info->max_channels,
      (devfmts[0] == '\0') ? devfmts : devfmts + 1,
      (unsigned int)device_info->format, devdeffmt, device_info->min_rate,
      device_info->max_rate, device_info->default_rate, device_info->latency_lo,
      device_info->latency_hi);
}

int
cubeb_enumerate_devices(cubeb * context, cubeb_device_type devtype,
                        cubeb_device_collection * collection)
{
  int rv;
  if ((devtype & (CUBEB_DEVICE_TYPE_INPUT | CUBEB_DEVICE_TYPE_OUTPUT)) == 0)
    return CUBEB_ERROR_INVALID_PARAMETER;
  if (context == NULL || collection == NULL)
    return CUBEB_ERROR_INVALID_PARAMETER;
  if (!context->ops->enumerate_devices)
    return CUBEB_ERROR_NOT_SUPPORTED;

  rv = context->ops->enumerate_devices(context, devtype, collection);

  if (cubeb_log_get_callback()) {
    for (size_t i = 0; i < collection->count; i++) {
      log_device(&collection->device[i]);
    }
  }

  return rv;
}

int
cubeb_device_collection_destroy(cubeb * context,
                                cubeb_device_collection * collection)
{
  int r;

  if (context == NULL || collection == NULL)
    return CUBEB_ERROR_INVALID_PARAMETER;

  if (!context->ops->device_collection_destroy)
    return CUBEB_ERROR_NOT_SUPPORTED;

  if (!collection->device)
    return CUBEB_OK;

  r = context->ops->device_collection_destroy(context, collection);
  if (r == CUBEB_OK) {
    collection->device = NULL;
    collection->count = 0;
  }

  return r;
}

int
cubeb_register_device_collection_changed(
    cubeb * context, cubeb_device_type devtype,
    cubeb_device_collection_changed_callback callback, void * user_ptr)
{
  if (context == NULL ||
      (devtype & (CUBEB_DEVICE_TYPE_INPUT | CUBEB_DEVICE_TYPE_OUTPUT)) == 0)
    return CUBEB_ERROR_INVALID_PARAMETER;

  if (!context->ops->register_device_collection_changed) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  return context->ops->register_device_collection_changed(context, devtype,
                                                          callback, user_ptr);
}

int
cubeb_set_log_callback(cubeb_log_level log_level,
                       cubeb_log_callback log_callback)
{
  if (log_level < CUBEB_LOG_DISABLED || log_level > CUBEB_LOG_VERBOSE) {
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  if (!log_callback && log_level != CUBEB_LOG_DISABLED) {
    return CUBEB_ERROR_INVALID_PARAMETER;
  }

  if (cubeb_log_get_callback() && log_callback) {
    return CUBEB_ERROR_NOT_SUPPORTED;
  }

  cubeb_log_set(log_level, log_callback);

  return CUBEB_OK;
}
