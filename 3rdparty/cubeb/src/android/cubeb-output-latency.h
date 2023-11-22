#ifndef _CUBEB_OUTPUT_LATENCY_H_
#define _CUBEB_OUTPUT_LATENCY_H_

#include "../cubeb-jni.h"
#include "cubeb_media_library.h"
#include <stdbool.h>

struct output_latency_function {
  media_lib * from_lib;
  cubeb_jni * from_jni;
  int version;
};

typedef struct output_latency_function output_latency_function;

const int ANDROID_JELLY_BEAN_MR1_4_2 = 17;

output_latency_function *
cubeb_output_latency_load_method(int version)
{
  output_latency_function * ol = NULL;
  ol = (output_latency_function *)calloc(1, sizeof(output_latency_function));

  ol->version = version;

  if (ol->version > ANDROID_JELLY_BEAN_MR1_4_2) {
    ol->from_jni = cubeb_jni_init();
    return ol;
  }

  ol->from_lib = cubeb_load_media_library();
  return ol;
}

bool
cubeb_output_latency_method_is_loaded(output_latency_function * ol)
{
  assert(ol);
  if (ol->version > ANDROID_JELLY_BEAN_MR1_4_2) {
    return !!ol->from_jni;
  }

  return !!ol->from_lib;
}

void
cubeb_output_latency_unload_method(output_latency_function * ol)
{
  if (!ol) {
    return;
  }

  if (ol->version > ANDROID_JELLY_BEAN_MR1_4_2 && ol->from_jni) {
    cubeb_jni_destroy(ol->from_jni);
  }

  if (ol->version <= ANDROID_JELLY_BEAN_MR1_4_2 && ol->from_lib) {
    cubeb_close_media_library(ol->from_lib);
  }

  free(ol);
}

extern "C" {

uint32_t
cubeb_get_output_latency(output_latency_function * ol)
{
  assert(cubeb_output_latency_method_is_loaded(ol));

  if (ol->version > ANDROID_JELLY_BEAN_MR1_4_2) {
    return cubeb_get_output_latency_from_jni(ol->from_jni);
  }

  return cubeb_get_output_latency_from_media_library(ol->from_lib);
}
}

#endif // _CUBEB_OUTPUT_LATENCY_H_
