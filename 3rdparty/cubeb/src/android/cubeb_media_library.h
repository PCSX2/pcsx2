#include <cassert>
#include <cstdint>
#include <dlfcn.h>
#include <stdlib.h>
#ifndef _CUBEB_MEDIA_LIBRARY_H_
#define _CUBEB_MEDIA_LIBRARY_H_

typedef int32_t (*get_output_latency_ptr)(uint32_t * latency, int stream_type);

struct media_lib {
  void * libmedia;
  get_output_latency_ptr get_output_latency;
};

typedef struct media_lib media_lib;

media_lib *
cubeb_load_media_library()
{
  media_lib ml = {};
  ml.libmedia = dlopen("libmedia.so", RTLD_LAZY);
  if (!ml.libmedia) {
    return nullptr;
  }

  // Get the latency, in ms, from AudioFlinger. First, try the most recent
  // signature. status_t AudioSystem::getOutputLatency(uint32_t* latency,
  // audio_stream_type_t streamType)
  ml.get_output_latency = (get_output_latency_ptr)dlsym(
      ml.libmedia,
      "_ZN7android11AudioSystem16getOutputLatencyEPj19audio_stream_type_t");
  if (!ml.get_output_latency) {
    // In case of failure, try the signature from legacy version.
    // status_t AudioSystem::getOutputLatency(uint32_t* latency, int streamType)
    ml.get_output_latency = (get_output_latency_ptr)dlsym(
        ml.libmedia, "_ZN7android11AudioSystem16getOutputLatencyEPji");
    if (!ml.get_output_latency) {
      return nullptr;
    }
  }

  media_lib * rv = nullptr;
  rv = (media_lib *)calloc(1, sizeof(media_lib));
  assert(rv);
  *rv = ml;
  return rv;
}

void
cubeb_close_media_library(media_lib * ml)
{
  dlclose(ml->libmedia);
  ml->libmedia = NULL;
  ml->get_output_latency = NULL;
  free(ml);
}

uint32_t
cubeb_get_output_latency_from_media_library(media_lib * ml)
{
  uint32_t latency = 0;
  const int audio_stream_type_music = 3;
  int32_t r = ml->get_output_latency(&latency, audio_stream_type_music);
  if (r) {
    return 0;
  }
  return latency;
}

#endif // _CUBEB_MEDIA_LIBRARY_H_
