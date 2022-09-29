/*
 * Copyright © 2013 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#if !defined(CUBEB_INTERNAL_0eb56756_4e20_4404_a76d_42bf88cd15a5)
#define CUBEB_INTERNAL_0eb56756_4e20_4404_a76d_42bf88cd15a5

#include "cubeb/cubeb.h"
#include "cubeb_assert.h"
#include "cubeb_log.h"
#include <stdio.h>
#include <string.h>

#ifdef __clang__
#ifndef CLANG_ANALYZER_NORETURN
#if __has_feature(attribute_analyzer_noreturn)
#define CLANG_ANALYZER_NORETURN __attribute__((analyzer_noreturn))
#else
#define CLANG_ANALYZER_NORETURN
#endif // ifndef CLANG_ANALYZER_NORETURN
#endif // __has_feature(attribute_analyzer_noreturn)
#else  // __clang__
#define CLANG_ANALYZER_NORETURN
#endif

#if defined(__cplusplus)
extern "C" {
#endif

#if defined(__cplusplus)
}
#endif

struct cubeb_ops {
  int (*init)(cubeb ** context, char const * context_name);
  char const * (*get_backend_id)(cubeb * context);
  int (*get_max_channel_count)(cubeb * context, uint32_t * max_channels);
  int (*get_min_latency)(cubeb * context, cubeb_stream_params params,
                         uint32_t * latency_ms);
  int (*get_preferred_sample_rate)(cubeb * context, uint32_t * rate);
  int (*enumerate_devices)(cubeb * context, cubeb_device_type type,
                           cubeb_device_collection * collection);
  int (*device_collection_destroy)(cubeb * context,
                                   cubeb_device_collection * collection);
  void (*destroy)(cubeb * context);
  int (*stream_init)(cubeb * context, cubeb_stream ** stream,
                     char const * stream_name, cubeb_devid input_device,
                     cubeb_stream_params * input_stream_params,
                     cubeb_devid output_device,
                     cubeb_stream_params * output_stream_params,
                     unsigned int latency, cubeb_data_callback data_callback,
                     cubeb_state_callback state_callback, void * user_ptr);
  void (*stream_destroy)(cubeb_stream * stream);
  int (*stream_start)(cubeb_stream * stream);
  int (*stream_stop)(cubeb_stream * stream);
  int (*stream_get_position)(cubeb_stream * stream, uint64_t * position);
  int (*stream_get_latency)(cubeb_stream * stream, uint32_t * latency);
  int (*stream_get_input_latency)(cubeb_stream * stream, uint32_t * latency);
  int (*stream_set_volume)(cubeb_stream * stream, float volumes);
  int (*stream_set_name)(cubeb_stream * stream, char const * stream_name);
  int (*stream_get_current_device)(cubeb_stream * stream,
                                   cubeb_device ** const device);
  int (*stream_device_destroy)(cubeb_stream * stream, cubeb_device * device);
  int (*stream_register_device_changed_callback)(
      cubeb_stream * stream,
      cubeb_device_changed_callback device_changed_callback);
  int (*register_device_collection_changed)(
      cubeb * context, cubeb_device_type devtype,
      cubeb_device_collection_changed_callback callback, void * user_ptr);
};

#endif /* CUBEB_INTERNAL_0eb56756_4e20_4404_a76d_42bf88cd15a5 */
