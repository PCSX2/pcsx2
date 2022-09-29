/*
 * Copyright © 2012 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#undef NDEBUG
#include <SLES/OpenSLES.h>
#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdlib.h>
#include <time.h>
#if defined(__ANDROID__)
#include "android/sles_definitions.h"
#include <SLES/OpenSLES_Android.h>
#include <android/api-level.h>
#include <android/log.h>
#include <dlfcn.h>
#include <sys/system_properties.h>
#endif
#include "android/cubeb-output-latency.h"
#include "cubeb-internal.h"
#include "cubeb/cubeb.h"
#include "cubeb_android.h"
#include "cubeb_array_queue.h"
#include "cubeb_resampler.h"

#if defined(__ANDROID__)
#ifdef LOG
#undef LOG
#endif
//#define LOGGING_ENABLED
#ifdef LOGGING_ENABLED
#define LOG(args...)                                                           \
  __android_log_print(ANDROID_LOG_INFO, "Cubeb_OpenSL", ##args)
#else
#define LOG(...)
#endif

//#define TIMESTAMP_ENABLED
#ifdef TIMESTAMP_ENABLED
#define FILENAME                                                               \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#define LOG_TS(args...)                                                        \
  __android_log_print(ANDROID_LOG_INFO, "Cubeb_OpenSL ES: Timestamp(usec)",    \
                      ##args)
#define TIMESTAMP(msg)                                                         \
  do {                                                                         \
    struct timeval timestamp;                                                  \
    int ts_ret = gettimeofday(&timestamp, NULL);                               \
    if (ts_ret == 0) {                                                         \
      LOG_TS("%lld: %s (%s %s:%d)",                                            \
             timestamp.tv_sec * 1000000LL + timestamp.tv_usec, msg,            \
             __FUNCTION__, FILENAME, __LINE__);                                \
    } else {                                                                   \
      LOG_TS("Error: %s (%s %s:%d) - %s", msg, __FUNCTION__, FILENAME,         \
             __LINE__);                                                        \
    }                                                                          \
  } while (0)
#else
#define TIMESTAMP(...)
#endif

#define ANDROID_VERSION_GINGERBREAD_MR1 10
#define ANDROID_VERSION_JELLY_BEAN 18
#define ANDROID_VERSION_LOLLIPOP 21
#define ANDROID_VERSION_MARSHMALLOW 23
#define ANDROID_VERSION_N_MR1 25
#endif

#define DEFAULT_SAMPLE_RATE 48000
#define DEFAULT_NUM_OF_FRAMES 480

static struct cubeb_ops const opensl_ops;

struct cubeb {
  struct cubeb_ops const * ops;
  void * lib;
  SLInterfaceID SL_IID_BUFFERQUEUE;
  SLInterfaceID SL_IID_PLAY;
#if defined(__ANDROID__)
  SLInterfaceID SL_IID_ANDROIDCONFIGURATION;
  SLInterfaceID SL_IID_ANDROIDSIMPLEBUFFERQUEUE;
#endif
  SLInterfaceID SL_IID_VOLUME;
  SLInterfaceID SL_IID_RECORD;
  SLObjectItf engObj;
  SLEngineItf eng;
  SLObjectItf outmixObj;
  output_latency_function * p_output_latency_function;
};

#define NELEMS(A) (sizeof(A) / sizeof A[0])
#define NBUFS 2

struct cubeb_stream {
  /* Note: Must match cubeb_stream layout in cubeb.c. */
  cubeb * context;
  void * user_ptr;
  /**/
  pthread_mutex_t mutex;
  SLObjectItf playerObj;
  SLPlayItf play;
  SLBufferQueueItf bufq;
  SLVolumeItf volume;
  void ** queuebuf;
  uint32_t queuebuf_capacity;
  int queuebuf_idx;
  long queuebuf_len;
  long bytespersec;
  long framesize;
  /* Total number of played frames.
   * Synchronized by stream::mutex lock. */
  long written;
  /* Flag indicating draining. Synchronized
   * by stream::mutex lock. */
  int draining;
  /* Flags to determine in/out.*/
  uint32_t input_enabled;
  uint32_t output_enabled;
  /* Recorder abstract object. */
  SLObjectItf recorderObj;
  /* Recorder Itf for input capture. */
  SLRecordItf recorderItf;
  /* Buffer queue for input capture. */
  SLAndroidSimpleBufferQueueItf recorderBufferQueueItf;
  /* Store input buffers. */
  void ** input_buffer_array;
  /* The capacity of the array.
   * On capture only can be small (4).
   * On full duplex is calculated to
   * store 1 sec of data buffers. */
  uint32_t input_array_capacity;
  /* Current filled index of input buffer array.
   * It is initiated to -1 indicating buffering
   * have not started yet. */
  int input_buffer_index;
  /* Length of input buffer.*/
  uint32_t input_buffer_length;
  /* Input frame size */
  uint32_t input_frame_size;
  /* Device sampling rate. If user rate is not
   * accepted an compatible rate is set. If it is
   * accepted this is equal to params.rate. */
  uint32_t input_device_rate;
  /* Exchange input buffers between input
   * and full duplex threads. */
  array_queue * input_queue;
  /* Silent input buffer used on full duplex. */
  void * input_silent_buffer;
  /* Number of input frames from the start of the stream*/
  uint32_t input_total_frames;
  /* Flag to stop the execution of user callback and
   * close all working threads. Synchronized by
   * stream::mutex lock. */
  uint32_t shutdown;
  /* Store user callback. */
  cubeb_data_callback data_callback;
  /* Store state callback. */
  cubeb_state_callback state_callback;

  cubeb_resampler * resampler;
  unsigned int user_output_rate;
  unsigned int output_configured_rate;
  unsigned int buffer_size_frames;
  // Audio output latency used in cubeb_stream_get_position().
  unsigned int output_latency_ms;
  int64_t lastPosition;
  int64_t lastPositionTimeStamp;
  int64_t lastCompensativePosition;
  int voice_input;
  int voice_output;
};

/* Forward declaration. */
static int
opensl_stop_player(cubeb_stream * stm);
static int
opensl_stop_recorder(cubeb_stream * stm);

static int
opensl_get_draining(cubeb_stream * stm)
{
#ifdef DEBUG
  int r = pthread_mutex_trylock(&stm->mutex);
  assert((r == EDEADLK || r == EBUSY) &&
         "get_draining: mutex should be locked but it's not.");
#endif
  return stm->draining;
}

static void
opensl_set_draining(cubeb_stream * stm, int value)
{
#ifdef DEBUG
  int r = pthread_mutex_trylock(&stm->mutex);
  LOG("set draining try r = %d", r);
  assert((r == EDEADLK || r == EBUSY) &&
         "set_draining: mutex should be locked but it's not.");
#endif
  assert(value == 0 || value == 1);
  stm->draining = value;
}

static void
opensl_notify_drained(cubeb_stream * stm)
{
  assert(stm);
  int r = pthread_mutex_lock(&stm->mutex);
  assert(r == 0);
  int draining = opensl_get_draining(stm);
  r = pthread_mutex_unlock(&stm->mutex);
  assert(r == 0);
  if (draining) {
    stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
    if (stm->play) {
      LOG("stop player in play_callback");
      r = opensl_stop_player(stm);
      assert(r == CUBEB_OK);
    }
    if (stm->recorderItf) {
      r = opensl_stop_recorder(stm);
      assert(r == CUBEB_OK);
    }
  }
}

static uint32_t
opensl_get_shutdown(cubeb_stream * stm)
{
#ifdef DEBUG
  int r = pthread_mutex_trylock(&stm->mutex);
  assert((r == EDEADLK || r == EBUSY) &&
         "get_shutdown: mutex should be locked but it's not.");
#endif
  return stm->shutdown;
}

static void
opensl_set_shutdown(cubeb_stream * stm, uint32_t value)
{
#ifdef DEBUG
  int r = pthread_mutex_trylock(&stm->mutex);
  LOG("set shutdown try r = %d", r);
  assert((r == EDEADLK || r == EBUSY) &&
         "set_shutdown: mutex should be locked but it's not.");
#endif
  assert(value == 0 || value == 1);
  stm->shutdown = value;
}

static void
play_callback(SLPlayItf caller, void * user_ptr, SLuint32 event)
{
  cubeb_stream * stm = user_ptr;
  assert(stm);
  switch (event) {
  case SL_PLAYEVENT_HEADATMARKER:
    opensl_notify_drained(stm);
    break;
  default:
    break;
  }
}

static void
recorder_marker_callback(SLRecordItf caller, void * pContext, SLuint32 event)
{
  cubeb_stream * stm = pContext;
  assert(stm);

  if (event == SL_RECORDEVENT_HEADATMARKER) {
    int r = pthread_mutex_lock(&stm->mutex);
    assert(r == 0);
    int draining = opensl_get_draining(stm);
    r = pthread_mutex_unlock(&stm->mutex);
    assert(r == 0);
    if (draining) {
      stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
      if (stm->recorderItf) {
        r = opensl_stop_recorder(stm);
        assert(r == CUBEB_OK);
      }
      if (stm->play) {
        r = opensl_stop_player(stm);
        assert(r == CUBEB_OK);
      }
    }
  }
}

static void
bufferqueue_callback(SLBufferQueueItf caller, void * user_ptr)
{
  cubeb_stream * stm = user_ptr;
  assert(stm);
  SLBufferQueueState state;
  SLresult res;
  long written = 0;

  res = (*stm->bufq)->GetState(stm->bufq, &state);
  assert(res == SL_RESULT_SUCCESS);

  if (state.count > 1) {
    return;
  }

  uint8_t * buf = stm->queuebuf[stm->queuebuf_idx];
  written = 0;
  int r = pthread_mutex_lock(&stm->mutex);
  assert(r == 0);
  int draining = opensl_get_draining(stm);
  uint32_t shutdown = opensl_get_shutdown(stm);
  r = pthread_mutex_unlock(&stm->mutex);
  assert(r == 0);
  if (!draining && !shutdown) {
    written = cubeb_resampler_fill(stm->resampler, NULL, NULL, buf,
                                   stm->queuebuf_len / stm->framesize);
    LOG("bufferqueue_callback: resampler fill returned %ld frames", written);
    if (written < 0 || written * stm->framesize > stm->queuebuf_len) {
      r = pthread_mutex_lock(&stm->mutex);
      assert(r == 0);
      opensl_set_shutdown(stm, 1);
      r = pthread_mutex_unlock(&stm->mutex);
      assert(r == 0);
      opensl_stop_player(stm);
      stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
      return;
    }
  }

  // Keep sending silent data even in draining mode to prevent the audio
  // back-end from being stopped automatically by OpenSL/ES.
  assert(stm->queuebuf_len >= written * stm->framesize);
  memset(buf + written * stm->framesize, 0,
         stm->queuebuf_len - written * stm->framesize);
  res = (*stm->bufq)->Enqueue(stm->bufq, buf, stm->queuebuf_len);
  assert(res == SL_RESULT_SUCCESS);
  stm->queuebuf_idx = (stm->queuebuf_idx + 1) % stm->queuebuf_capacity;

  if (written > 0) {
    pthread_mutex_lock(&stm->mutex);
    stm->written += written;
    pthread_mutex_unlock(&stm->mutex);
  }

  if (!draining && written * stm->framesize < stm->queuebuf_len) {
    LOG("bufferqueue_callback draining");
    r = pthread_mutex_lock(&stm->mutex);
    assert(r == 0);
    int64_t written_duration =
        INT64_C(1000) * stm->written * stm->framesize / stm->bytespersec;
    opensl_set_draining(stm, 1);
    r = pthread_mutex_unlock(&stm->mutex);
    assert(r == 0);

    if (written_duration == 0) {
      // since we didn't write any sample, it's not possible to reach the marker
      // time and trigger the callback. We should initiative notify drained.
      opensl_notify_drained(stm);
    } else {
      // Use SL_PLAYEVENT_HEADATMARKER event from slPlayCallback of SLPlayItf
      // to make sure all the data has been processed.
      (*stm->play)
          ->SetMarkerPosition(stm->play, (SLmillisecond)written_duration);
    }
    return;
  }
}

static int
opensl_enqueue_recorder(cubeb_stream * stm, void ** last_filled_buffer)
{
  assert(stm);

  int current_index = stm->input_buffer_index;
  void * last_buffer = NULL;

  if (current_index < 0) {
    // This is the first enqueue
    current_index = 0;
  } else {
    // The current index hold the last filled buffer get it before advance
    // index.
    last_buffer = stm->input_buffer_array[current_index];
    // Advance to get next available buffer
    current_index = (current_index + 1) % stm->input_array_capacity;
  }
  // enqueue next empty buffer to be filled by the recorder
  SLresult res = (*stm->recorderBufferQueueItf)
                     ->Enqueue(stm->recorderBufferQueueItf,
                               stm->input_buffer_array[current_index],
                               stm->input_buffer_length);
  if (res != SL_RESULT_SUCCESS) {
    LOG("Enqueue recorder failed. Error code: %lu", res);
    return CUBEB_ERROR;
  }
  // All good, update buffer and index.
  stm->input_buffer_index = current_index;
  if (last_filled_buffer) {
    *last_filled_buffer = last_buffer;
  }
  return CUBEB_OK;
}

// input data callback
void
recorder_callback(SLAndroidSimpleBufferQueueItf bq, void * context)
{
  assert(context);
  cubeb_stream * stm = context;
  assert(stm->recorderBufferQueueItf);

  int r = pthread_mutex_lock(&stm->mutex);
  assert(r == 0);
  uint32_t shutdown = opensl_get_shutdown(stm);
  int draining = opensl_get_draining(stm);
  r = pthread_mutex_unlock(&stm->mutex);
  assert(r == 0);

  if (shutdown || draining) {
    // According to the OpenSL ES 1.1 Specification, 8.14 SLBufferQueueItf
    // page 184, on transition to the SL_RECORDSTATE_STOPPED state,
    // the application should continue to enqueue buffers onto the queue
    // to retrieve the residual recorded data in the system.
    r = opensl_enqueue_recorder(stm, NULL);
    assert(r == CUBEB_OK);
    return;
  }

  // Enqueue next available buffer and get the last filled buffer.
  void * input_buffer = NULL;
  r = opensl_enqueue_recorder(stm, &input_buffer);
  assert(r == CUBEB_OK);
  assert(input_buffer);
  // Fill resampler with last input
  long input_frame_count = stm->input_buffer_length / stm->input_frame_size;
  long got = cubeb_resampler_fill(stm->resampler, input_buffer,
                                  &input_frame_count, NULL, 0);
  // Error case
  if (got < 0 || got > input_frame_count) {
    r = pthread_mutex_lock(&stm->mutex);
    assert(r == 0);
    opensl_set_shutdown(stm, 1);
    r = pthread_mutex_unlock(&stm->mutex);
    assert(r == 0);
    r = opensl_stop_recorder(stm);
    assert(r == CUBEB_OK);
    stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
  }

  // Advance total stream frames
  stm->input_total_frames += got;

  if (got < input_frame_count) {
    r = pthread_mutex_lock(&stm->mutex);
    assert(r == 0);
    opensl_set_draining(stm, 1);
    r = pthread_mutex_unlock(&stm->mutex);
    assert(r == 0);
    int64_t duration =
        INT64_C(1000) * stm->input_total_frames / stm->input_device_rate;
    (*stm->recorderItf)
        ->SetMarkerPosition(stm->recorderItf, (SLmillisecond)duration);
    return;
  }
}

void
recorder_fullduplex_callback(SLAndroidSimpleBufferQueueItf bq, void * context)
{
  assert(context);
  cubeb_stream * stm = context;
  assert(stm->recorderBufferQueueItf);

  int r = pthread_mutex_lock(&stm->mutex);
  assert(r == 0);
  int draining = opensl_get_draining(stm);
  uint32_t shutdown = opensl_get_shutdown(stm);
  r = pthread_mutex_unlock(&stm->mutex);
  assert(r == 0);

  if (shutdown || draining) {
    /* On draining and shutdown the recorder should have been stoped from
     *  the one set the flags. Accordint to the doc, on transition to
     *  the SL_RECORDSTATE_STOPPED state, the application should
     *  continue to enqueue buffers onto the queue to retrieve the residual
     *  recorded data in the system. */
    LOG("Input shutdown %d or drain %d", shutdown, draining);
    int r = opensl_enqueue_recorder(stm, NULL);
    assert(r == CUBEB_OK);
    return;
  }

  // Enqueue next available buffer and get the last filled buffer.
  void * input_buffer = NULL;
  r = opensl_enqueue_recorder(stm, &input_buffer);
  assert(r == CUBEB_OK);
  assert(input_buffer);

  assert(stm->input_queue);
  r = array_queue_push(stm->input_queue, input_buffer);
  if (r == -1) {
    LOG("Input queue is full, drop input ...");
    return;
  }

  LOG("Input pushed in the queue, input array %zu",
      array_queue_get_size(stm->input_queue));
}

static void
player_fullduplex_callback(SLBufferQueueItf caller, void * user_ptr)
{
  TIMESTAMP("ENTER");
  cubeb_stream * stm = user_ptr;
  assert(stm);
  SLresult res;

  int r = pthread_mutex_lock(&stm->mutex);
  assert(r == 0);
  int draining = opensl_get_draining(stm);
  uint32_t shutdown = opensl_get_shutdown(stm);
  r = pthread_mutex_unlock(&stm->mutex);
  assert(r == 0);

  // Get output
  void * output_buffer = NULL;
  r = pthread_mutex_lock(&stm->mutex);
  assert(r == 0);
  output_buffer = stm->queuebuf[stm->queuebuf_idx];
  // Advance the output buffer queue index
  stm->queuebuf_idx = (stm->queuebuf_idx + 1) % stm->queuebuf_capacity;
  r = pthread_mutex_unlock(&stm->mutex);
  assert(r == 0);

  if (shutdown || draining) {
    LOG("Shutdown/draining, send silent");
    // Set silent on buffer
    memset(output_buffer, 0, stm->queuebuf_len);

    // Enqueue data in player buffer queue
    res = (*stm->bufq)->Enqueue(stm->bufq, output_buffer, stm->queuebuf_len);
    assert(res == SL_RESULT_SUCCESS);
    return;
  }

  // Get input.
  void * input_buffer = array_queue_pop(stm->input_queue);
  long input_frame_count = stm->input_buffer_length / stm->input_frame_size;
  long frames_needed = stm->queuebuf_len / stm->framesize;
  if (!input_buffer) {
    LOG("Input hole set silent input buffer");
    input_buffer = stm->input_silent_buffer;
  }

  long written = 0;
  // Trigger user callback through resampler
  written =
      cubeb_resampler_fill(stm->resampler, input_buffer, &input_frame_count,
                           output_buffer, frames_needed);

  LOG("Fill: written %ld, frames_needed %ld, input array size %zu", written,
      frames_needed, array_queue_get_size(stm->input_queue));

  if (written < 0 || written > frames_needed) {
    // Error case
    r = pthread_mutex_lock(&stm->mutex);
    assert(r == 0);
    opensl_set_shutdown(stm, 1);
    r = pthread_mutex_unlock(&stm->mutex);
    assert(r == 0);
    opensl_stop_player(stm);
    opensl_stop_recorder(stm);
    stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
    memset(output_buffer, 0, stm->queuebuf_len);

    // Enqueue data in player buffer queue
    res = (*stm->bufq)->Enqueue(stm->bufq, output_buffer, stm->queuebuf_len);
    assert(res == SL_RESULT_SUCCESS);
    return;
  }

  // Advance total out written  frames counter
  r = pthread_mutex_lock(&stm->mutex);
  assert(r == 0);
  stm->written += written;
  r = pthread_mutex_unlock(&stm->mutex);
  assert(r == 0);

  if (written < frames_needed) {
    r = pthread_mutex_lock(&stm->mutex);
    assert(r == 0);
    int64_t written_duration =
        INT64_C(1000) * stm->written * stm->framesize / stm->bytespersec;
    opensl_set_draining(stm, 1);
    r = pthread_mutex_unlock(&stm->mutex);
    assert(r == 0);

    // Use SL_PLAYEVENT_HEADATMARKER event from slPlayCallback of SLPlayItf
    // to make sure all the data has been processed.
    (*stm->play)->SetMarkerPosition(stm->play, (SLmillisecond)written_duration);
  }

  // Keep sending silent data even in draining mode to prevent the audio
  // back-end from being stopped automatically by OpenSL/ES.
  memset((uint8_t *)output_buffer + written * stm->framesize, 0,
         stm->queuebuf_len - written * stm->framesize);

  // Enqueue data in player buffer queue
  res = (*stm->bufq)->Enqueue(stm->bufq, output_buffer, stm->queuebuf_len);
  assert(res == SL_RESULT_SUCCESS);
  TIMESTAMP("EXIT");
}

static void
opensl_destroy(cubeb * ctx);

#if defined(__ANDROID__)
#if (__ANDROID_API__ >= ANDROID_VERSION_LOLLIPOP)
typedef int(system_property_get)(const char *, char *);

static int
wrap_system_property_get(const char * name, char * value)
{
  void * libc = dlopen("libc.so", RTLD_LAZY);
  if (!libc) {
    LOG("Failed to open libc.so");
    return -1;
  }
  system_property_get * func =
      (system_property_get *)dlsym(libc, "__system_property_get");
  int ret = -1;
  if (func) {
    ret = func(name, value);
  }
  dlclose(libc);
  return ret;
}
#endif

static int
get_android_version(void)
{
  char version_string[PROP_VALUE_MAX];

  memset(version_string, 0, PROP_VALUE_MAX);

#if (__ANDROID_API__ >= ANDROID_VERSION_LOLLIPOP)
  int len = wrap_system_property_get("ro.build.version.sdk", version_string);
#else
  int len = __system_property_get("ro.build.version.sdk", version_string);
#endif
  if (len <= 0) {
    LOG("Failed to get Android version!\n");
    return len;
  }

  int version = (int)strtol(version_string, NULL, 10);
  LOG("Android version %d", version);
  return version;
}
#endif

/*static*/ int
opensl_init(cubeb ** context, char const * context_name)
{
  cubeb * ctx;

#if defined(__ANDROID__)
  int android_version = get_android_version();
  if (android_version > 0 &&
      android_version <= ANDROID_VERSION_GINGERBREAD_MR1) {
    // Don't even attempt to run on Gingerbread and lower
    return CUBEB_ERROR;
  }
#endif

  *context = NULL;

  ctx = calloc(1, sizeof(*ctx));
  assert(ctx);

  ctx->ops = &opensl_ops;

  ctx->lib = dlopen("libOpenSLES.so", RTLD_LAZY);
  if (!ctx->lib) {
    free(ctx);
    return CUBEB_ERROR;
  }

  typedef SLresult (*slCreateEngine_t)(
      SLObjectItf *, SLuint32, const SLEngineOption *, SLuint32,
      const SLInterfaceID *, const SLboolean *);
  slCreateEngine_t f_slCreateEngine =
      (slCreateEngine_t)dlsym(ctx->lib, "slCreateEngine");
  SLInterfaceID SL_IID_ENGINE =
      *(SLInterfaceID *)dlsym(ctx->lib, "SL_IID_ENGINE");
  SLInterfaceID SL_IID_OUTPUTMIX =
      *(SLInterfaceID *)dlsym(ctx->lib, "SL_IID_OUTPUTMIX");
  ctx->SL_IID_VOLUME = *(SLInterfaceID *)dlsym(ctx->lib, "SL_IID_VOLUME");
  ctx->SL_IID_BUFFERQUEUE =
      *(SLInterfaceID *)dlsym(ctx->lib, "SL_IID_BUFFERQUEUE");
#if defined(__ANDROID__)
  ctx->SL_IID_ANDROIDCONFIGURATION =
      *(SLInterfaceID *)dlsym(ctx->lib, "SL_IID_ANDROIDCONFIGURATION");
  ctx->SL_IID_ANDROIDSIMPLEBUFFERQUEUE =
      *(SLInterfaceID *)dlsym(ctx->lib, "SL_IID_ANDROIDSIMPLEBUFFERQUEUE");
#endif
  ctx->SL_IID_PLAY = *(SLInterfaceID *)dlsym(ctx->lib, "SL_IID_PLAY");
  ctx->SL_IID_RECORD = *(SLInterfaceID *)dlsym(ctx->lib, "SL_IID_RECORD");

  if (!f_slCreateEngine || !SL_IID_ENGINE || !SL_IID_OUTPUTMIX ||
      !ctx->SL_IID_BUFFERQUEUE ||
#if defined(__ANDROID__)
      !ctx->SL_IID_ANDROIDCONFIGURATION ||
      !ctx->SL_IID_ANDROIDSIMPLEBUFFERQUEUE ||
#endif
      !ctx->SL_IID_PLAY || !ctx->SL_IID_RECORD) {
    opensl_destroy(ctx);
    return CUBEB_ERROR;
  }

  const SLEngineOption opt[] = {{SL_ENGINEOPTION_THREADSAFE, SL_BOOLEAN_TRUE}};

  SLresult res;
  res = f_slCreateEngine(&ctx->engObj, 1, opt, 0, NULL, NULL);

  if (res != SL_RESULT_SUCCESS) {
    opensl_destroy(ctx);
    return CUBEB_ERROR;
  }

  res = (*ctx->engObj)->Realize(ctx->engObj, SL_BOOLEAN_FALSE);
  if (res != SL_RESULT_SUCCESS) {
    opensl_destroy(ctx);
    return CUBEB_ERROR;
  }

  res = (*ctx->engObj)->GetInterface(ctx->engObj, SL_IID_ENGINE, &ctx->eng);
  if (res != SL_RESULT_SUCCESS) {
    opensl_destroy(ctx);
    return CUBEB_ERROR;
  }

  const SLInterfaceID idsom[] = {SL_IID_OUTPUTMIX};
  const SLboolean reqom[] = {SL_BOOLEAN_TRUE};
  res =
      (*ctx->eng)->CreateOutputMix(ctx->eng, &ctx->outmixObj, 1, idsom, reqom);
  if (res != SL_RESULT_SUCCESS) {
    opensl_destroy(ctx);
    return CUBEB_ERROR;
  }

  res = (*ctx->outmixObj)->Realize(ctx->outmixObj, SL_BOOLEAN_FALSE);
  if (res != SL_RESULT_SUCCESS) {
    opensl_destroy(ctx);
    return CUBEB_ERROR;
  }

  ctx->p_output_latency_function =
      cubeb_output_latency_load_method(android_version);
  if (!cubeb_output_latency_method_is_loaded(ctx->p_output_latency_function)) {
    LOG("Warning: output latency is not available, cubeb_stream_get_position() "
        "is not supported");
  }

  *context = ctx;

  LOG("Cubeb init (%p) success", ctx);
  return CUBEB_OK;
}

static char const *
opensl_get_backend_id(cubeb * ctx)
{
  return "opensl";
}

static int
opensl_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
  assert(ctx && max_channels);
  /* The android mixer handles up to two channels, see
     http://androidxref.com/4.2.2_r1/xref/frameworks/av/services/audioflinger/AudioFlinger.h#67
   */
  *max_channels = 2;

  return CUBEB_OK;
}

static void
opensl_destroy(cubeb * ctx)
{
  if (ctx->outmixObj) {
    (*ctx->outmixObj)->Destroy(ctx->outmixObj);
  }
  if (ctx->engObj) {
    (*ctx->engObj)->Destroy(ctx->engObj);
  }
  dlclose(ctx->lib);
  if (ctx->p_output_latency_function)
    cubeb_output_latency_unload_method(ctx->p_output_latency_function);
  free(ctx);
}

static void
opensl_stream_destroy(cubeb_stream * stm);

#if defined(__ANDROID__) && (__ANDROID_API__ >= ANDROID_VERSION_LOLLIPOP)
static int
opensl_set_format_ext(SLAndroidDataFormat_PCM_EX * format,
                      cubeb_stream_params * params)
{
  assert(format);
  assert(params);

  format->formatType = SL_ANDROID_DATAFORMAT_PCM_EX;
  format->numChannels = params->channels;
  // sampleRate is in milliHertz
  format->sampleRate = params->rate * 1000;
  format->channelMask = params->channels == 1
                            ? SL_SPEAKER_FRONT_CENTER
                            : SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;

  switch (params->format) {
  case CUBEB_SAMPLE_S16LE:
    format->bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    format->containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
    format->representation = SL_ANDROID_PCM_REPRESENTATION_SIGNED_INT;
    format->endianness = SL_BYTEORDER_LITTLEENDIAN;
    break;
  case CUBEB_SAMPLE_S16BE:
    format->bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
    format->containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
    format->representation = SL_ANDROID_PCM_REPRESENTATION_SIGNED_INT;
    format->endianness = SL_BYTEORDER_BIGENDIAN;
    break;
  case CUBEB_SAMPLE_FLOAT32LE:
    format->bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_32;
    format->containerSize = SL_PCMSAMPLEFORMAT_FIXED_32;
    format->representation = SL_ANDROID_PCM_REPRESENTATION_FLOAT;
    format->endianness = SL_BYTEORDER_LITTLEENDIAN;
    break;
  case CUBEB_SAMPLE_FLOAT32BE:
    format->bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_32;
    format->containerSize = SL_PCMSAMPLEFORMAT_FIXED_32;
    format->representation = SL_ANDROID_PCM_REPRESENTATION_FLOAT;
    format->endianness = SL_BYTEORDER_BIGENDIAN;
    break;
  default:
    return CUBEB_ERROR_INVALID_FORMAT;
  }
  return CUBEB_OK;
}
#endif

static int
opensl_set_format(SLDataFormat_PCM * format, cubeb_stream_params * params)
{
  assert(format);
  assert(params);

  format->formatType = SL_DATAFORMAT_PCM;
  format->numChannels = params->channels;
  // samplesPerSec is in milliHertz
  format->samplesPerSec = params->rate * 1000;
  format->bitsPerSample = SL_PCMSAMPLEFORMAT_FIXED_16;
  format->containerSize = SL_PCMSAMPLEFORMAT_FIXED_16;
  format->channelMask = params->channels == 1
                            ? SL_SPEAKER_FRONT_CENTER
                            : SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT;

  switch (params->format) {
  case CUBEB_SAMPLE_S16LE:
    format->endianness = SL_BYTEORDER_LITTLEENDIAN;
    break;
  case CUBEB_SAMPLE_S16BE:
    format->endianness = SL_BYTEORDER_BIGENDIAN;
    break;
  default:
    return CUBEB_ERROR_INVALID_FORMAT;
  }
  return CUBEB_OK;
}

static int
opensl_configure_capture(cubeb_stream * stm, cubeb_stream_params * params)
{
  assert(stm);
  assert(params);

  SLDataLocator_AndroidSimpleBufferQueue lDataLocatorOut;
  lDataLocatorOut.locatorType = SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE;
  lDataLocatorOut.numBuffers = NBUFS;

  SLDataFormat_PCM lDataFormat;
  int r = opensl_set_format(&lDataFormat, params);
  if (r != CUBEB_OK) {
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  /* For now set device rate to params rate. */
  stm->input_device_rate = params->rate;

  SLDataSink lDataSink;
  lDataSink.pLocator = &lDataLocatorOut;
  lDataSink.pFormat = &lDataFormat;

  SLDataLocator_IODevice lDataLocatorIn;
  lDataLocatorIn.locatorType = SL_DATALOCATOR_IODEVICE;
  lDataLocatorIn.deviceType = SL_IODEVICE_AUDIOINPUT;
  lDataLocatorIn.deviceID = SL_DEFAULTDEVICEID_AUDIOINPUT;
  lDataLocatorIn.device = NULL;

  SLDataSource lDataSource;
  lDataSource.pLocator = &lDataLocatorIn;
  lDataSource.pFormat = NULL;

  const SLInterfaceID lSoundRecorderIIDs[] = {
      stm->context->SL_IID_RECORD,
      stm->context->SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
      stm->context->SL_IID_ANDROIDCONFIGURATION};

  const SLboolean lSoundRecorderReqs[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE,
                                          SL_BOOLEAN_TRUE};
  // create the audio recorder abstract object
  SLresult res = (*stm->context->eng)
                     ->CreateAudioRecorder(
                         stm->context->eng, &stm->recorderObj, &lDataSource,
                         &lDataSink, NELEMS(lSoundRecorderIIDs),
                         lSoundRecorderIIDs, lSoundRecorderReqs);
  // Sample rate not supported. Try again with default sample rate!
  if (res == SL_RESULT_CONTENT_UNSUPPORTED) {
    if (stm->output_enabled && stm->output_configured_rate != 0) {
      // Set the same with the player. Since there is no
      // api for input device this is a safe choice.
      stm->input_device_rate = stm->output_configured_rate;
    } else {
      // The output preferred rate is used for an input only scenario.
      // The default rate expected to be supported from all android devices.
      stm->input_device_rate = DEFAULT_SAMPLE_RATE;
    }
    lDataFormat.samplesPerSec = stm->input_device_rate * 1000;
    res = (*stm->context->eng)
              ->CreateAudioRecorder(stm->context->eng, &stm->recorderObj,
                                    &lDataSource, &lDataSink,
                                    NELEMS(lSoundRecorderIIDs),
                                    lSoundRecorderIIDs, lSoundRecorderReqs);

    if (res != SL_RESULT_SUCCESS) {
      LOG("Failed to create recorder. Error code: %lu", res);
      return CUBEB_ERROR;
    }
  }

  if (get_android_version() > ANDROID_VERSION_JELLY_BEAN) {
    SLAndroidConfigurationItf recorderConfig;
    res = (*stm->recorderObj)
              ->GetInterface(stm->recorderObj,
                             stm->context->SL_IID_ANDROIDCONFIGURATION,
                             &recorderConfig);

    if (res != SL_RESULT_SUCCESS) {
      LOG("Failed to get the android configuration interface for recorder. "
          "Error "
          "code: %lu",
          res);
      return CUBEB_ERROR;
    }

    // Voice recognition is the lowest latency, according to the docs. Camcorder
    // uses a microphone that is in the same direction as the camera.
    SLint32 streamType = stm->voice_input
                             ? SL_ANDROID_RECORDING_PRESET_VOICE_RECOGNITION
                             : SL_ANDROID_RECORDING_PRESET_CAMCORDER;

    res =
        (*recorderConfig)
            ->SetConfiguration(recorderConfig, SL_ANDROID_KEY_RECORDING_PRESET,
                               &streamType, sizeof(SLint32));

    if (res != SL_RESULT_SUCCESS) {
      LOG("Failed to set the android configuration to VOICE for the recorder. "
          "Error code: %lu",
          res);
      return CUBEB_ERROR;
    }
  }
  // realize the audio recorder
  res = (*stm->recorderObj)->Realize(stm->recorderObj, SL_BOOLEAN_FALSE);
  if (res != SL_RESULT_SUCCESS) {
    LOG("Failed to realize recorder. Error code: %lu", res);
    return CUBEB_ERROR;
  }
  // get the record interface
  res = (*stm->recorderObj)
            ->GetInterface(stm->recorderObj, stm->context->SL_IID_RECORD,
                           &stm->recorderItf);
  if (res != SL_RESULT_SUCCESS) {
    LOG("Failed to get recorder interface. Error code: %lu", res);
    return CUBEB_ERROR;
  }

  res = (*stm->recorderItf)
            ->RegisterCallback(stm->recorderItf, recorder_marker_callback, stm);
  if (res != SL_RESULT_SUCCESS) {
    LOG("Failed to register recorder marker callback. Error code: %lu", res);
    return CUBEB_ERROR;
  }

  (*stm->recorderItf)->SetMarkerPosition(stm->recorderItf, (SLmillisecond)0);

  res = (*stm->recorderItf)
            ->SetCallbackEventsMask(stm->recorderItf,
                                    (SLuint32)SL_RECORDEVENT_HEADATMARKER);
  if (res != SL_RESULT_SUCCESS) {
    LOG("Failed to set headatmarker event mask. Error code: %lu", res);
    return CUBEB_ERROR;
  }
  // get the simple android buffer queue interface
  res = (*stm->recorderObj)
            ->GetInterface(stm->recorderObj,
                           stm->context->SL_IID_ANDROIDSIMPLEBUFFERQUEUE,
                           &stm->recorderBufferQueueItf);
  if (res != SL_RESULT_SUCCESS) {
    LOG("Failed to get recorder (android) buffer queue interface. Error code: "
        "%lu",
        res);
    return CUBEB_ERROR;
  }

  // register callback on record (input) buffer queue
  slAndroidSimpleBufferQueueCallback rec_callback = recorder_callback;
  if (stm->output_enabled) {
    // Register full duplex callback instead.
    rec_callback = recorder_fullduplex_callback;
  }
  res = (*stm->recorderBufferQueueItf)
            ->RegisterCallback(stm->recorderBufferQueueItf, rec_callback, stm);
  if (res != SL_RESULT_SUCCESS) {
    LOG("Failed to register recorder buffer queue callback. Error code: %lu",
        res);
    return CUBEB_ERROR;
  }

  // Calculate length of input buffer according to requested latency
  stm->input_frame_size = params->channels * sizeof(int16_t);
  stm->input_buffer_length = (stm->input_frame_size * stm->buffer_size_frames);

  // Calculate the capacity of input array
  stm->input_array_capacity = NBUFS;
  if (stm->output_enabled) {
    // Full duplex, update capacity to hold 1 sec of data
    stm->input_array_capacity =
        1 * stm->input_device_rate / stm->input_buffer_length;
  }
  // Allocate input array
  stm->input_buffer_array =
      (void **)calloc(1, sizeof(void *) * stm->input_array_capacity);
  // Buffering has not started yet.
  stm->input_buffer_index = -1;
  // Prepare input buffers
  for (uint32_t i = 0; i < stm->input_array_capacity; ++i) {
    stm->input_buffer_array[i] = calloc(1, stm->input_buffer_length);
  }

  // On full duplex allocate input queue and silent buffer
  if (stm->output_enabled) {
    stm->input_queue = array_queue_create(stm->input_array_capacity);
    assert(stm->input_queue);
    stm->input_silent_buffer = calloc(1, stm->input_buffer_length);
    assert(stm->input_silent_buffer);
  }

  // Enqueue buffer to start rolling once recorder started
  r = opensl_enqueue_recorder(stm, NULL);
  if (r != CUBEB_OK) {
    return r;
  }

  LOG("Cubeb stream init recorder success");

  return CUBEB_OK;
}

static int
opensl_configure_playback(cubeb_stream * stm, cubeb_stream_params * params)
{
  assert(stm);
  assert(params);

  stm->user_output_rate = params->rate;
  if (params->format == CUBEB_SAMPLE_S16NE ||
      params->format == CUBEB_SAMPLE_S16BE) {
    stm->framesize = params->channels * sizeof(int16_t);
  } else if (params->format == CUBEB_SAMPLE_FLOAT32NE ||
             params->format == CUBEB_SAMPLE_FLOAT32BE) {
    stm->framesize = params->channels * sizeof(float);
  }
  stm->lastPosition = -1;
  stm->lastPositionTimeStamp = 0;
  stm->lastCompensativePosition = -1;

  void * format = NULL;
  SLuint32 * format_sample_rate = NULL;

#if defined(__ANDROID__) && (__ANDROID_API__ >= ANDROID_VERSION_LOLLIPOP)
  SLAndroidDataFormat_PCM_EX pcm_ext_format;
  if (get_android_version() >= ANDROID_VERSION_LOLLIPOP) {
    if (opensl_set_format_ext(&pcm_ext_format, params) != CUBEB_OK) {
      return CUBEB_ERROR_INVALID_FORMAT;
    }
    format = &pcm_ext_format;
    format_sample_rate = &pcm_ext_format.sampleRate;
  }
#endif

  SLDataFormat_PCM pcm_format;
  if (!format) {
    if (opensl_set_format(&pcm_format, params) != CUBEB_OK) {
      return CUBEB_ERROR_INVALID_FORMAT;
    }
    format = &pcm_format;
    format_sample_rate = &pcm_format.samplesPerSec;
  }

  SLDataLocator_BufferQueue loc_bufq;
  loc_bufq.locatorType = SL_DATALOCATOR_BUFFERQUEUE;
  loc_bufq.numBuffers = NBUFS;
  SLDataSource source;
  source.pLocator = &loc_bufq;
  source.pFormat = format;

  SLDataLocator_OutputMix loc_outmix;
  loc_outmix.locatorType = SL_DATALOCATOR_OUTPUTMIX;
  loc_outmix.outputMix = stm->context->outmixObj;
  SLDataSink sink;
  sink.pLocator = &loc_outmix;
  sink.pFormat = NULL;

#if defined(__ANDROID__)
  const SLInterfaceID ids[] = {stm->context->SL_IID_BUFFERQUEUE,
                               stm->context->SL_IID_VOLUME,
                               stm->context->SL_IID_ANDROIDCONFIGURATION};
  const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
#else
  const SLInterfaceID ids[] = {ctx->SL_IID_BUFFERQUEUE, ctx->SL_IID_VOLUME};
  const SLboolean req[] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
#endif
  assert(NELEMS(ids) == NELEMS(req));

  uint32_t preferred_sampling_rate = stm->user_output_rate;
  SLresult res = SL_RESULT_CONTENT_UNSUPPORTED;
  if (preferred_sampling_rate) {
    res = (*stm->context->eng)
              ->CreateAudioPlayer(stm->context->eng, &stm->playerObj, &source,
                                  &sink, NELEMS(ids), ids, req);
  }

  // Sample rate not supported? Try again with primary sample rate!
  if (res == SL_RESULT_CONTENT_UNSUPPORTED &&
      preferred_sampling_rate != DEFAULT_SAMPLE_RATE) {
    preferred_sampling_rate = DEFAULT_SAMPLE_RATE;
    *format_sample_rate = preferred_sampling_rate * 1000;
    res = (*stm->context->eng)
              ->CreateAudioPlayer(stm->context->eng, &stm->playerObj, &source,
                                  &sink, NELEMS(ids), ids, req);
  }

  if (res != SL_RESULT_SUCCESS) {
    LOG("Failed to create audio player. Error code: %lu", res);
    return CUBEB_ERROR;
  }

  stm->output_configured_rate = preferred_sampling_rate;
  stm->bytespersec = stm->output_configured_rate * stm->framesize;
  stm->queuebuf_len = stm->framesize * stm->buffer_size_frames;

  // Calculate the capacity of input array
  stm->queuebuf_capacity = NBUFS;
  if (stm->output_enabled) {
    // Full duplex, update capacity to hold 1 sec of data
    stm->queuebuf_capacity =
        1 * stm->output_configured_rate / stm->queuebuf_len;
  }
  // Allocate input array
  stm->queuebuf = (void **)calloc(1, sizeof(void *) * stm->queuebuf_capacity);
  for (uint32_t i = 0; i < stm->queuebuf_capacity; ++i) {
    stm->queuebuf[i] = calloc(1, stm->queuebuf_len);
    assert(stm->queuebuf[i]);
  }

  SLAndroidConfigurationItf playerConfig = NULL;

  if (get_android_version() >= ANDROID_VERSION_N_MR1) {
    res = (*stm->playerObj)
              ->GetInterface(stm->playerObj,
                             stm->context->SL_IID_ANDROIDCONFIGURATION,
                             &playerConfig);
    if (res != SL_RESULT_SUCCESS) {
      LOG("Failed to get Android configuration interface. Error code: %lu",
          res);
      return CUBEB_ERROR;
    }

    SLint32 streamType = SL_ANDROID_STREAM_MEDIA;
    if (stm->voice_output) {
      streamType = SL_ANDROID_STREAM_VOICE;
    }
    res = (*playerConfig)
              ->SetConfiguration(playerConfig, SL_ANDROID_KEY_STREAM_TYPE,
                                 &streamType, sizeof(streamType));
    if (res != SL_RESULT_SUCCESS) {
      LOG("Failed to set Android configuration to %d Error code: %lu",
          streamType, res);
    }

    SLuint32 performanceMode = SL_ANDROID_PERFORMANCE_LATENCY;
    if (stm->buffer_size_frames > POWERSAVE_LATENCY_FRAMES_THRESHOLD) {
      performanceMode = SL_ANDROID_PERFORMANCE_POWER_SAVING;
    }

    res = (*playerConfig)
              ->SetConfiguration(playerConfig, SL_ANDROID_KEY_PERFORMANCE_MODE,
                                 &performanceMode, sizeof(performanceMode));
    if (res != SL_RESULT_SUCCESS) {
      LOG("Failed to set Android performance mode to %d Error code: %lu. This "
          "is"
          " not fatal",
          performanceMode, res);
    }
  }

  res = (*stm->playerObj)->Realize(stm->playerObj, SL_BOOLEAN_FALSE);
  if (res != SL_RESULT_SUCCESS) {
    LOG("Failed to realize player object. Error code: %lu", res);
    return CUBEB_ERROR;
  }

  // There are two ways of getting the audio output latency:
  // - a configuration value, only available on some devices (notably devices
  // running FireOS)
  // - A Java method, that we call using JNI.
  //
  // The first method is prefered, if available, because it can account for more
  // latency causes, and is more precise.

  // Latency has to be queried after the realization of the interface, when
  // using SL_IID_ANDROIDCONFIGURATION.
  SLuint32 audioLatency = 0;
  SLuint32 paramSize = sizeof(SLuint32);
  // The reported latency is in milliseconds.
  if (playerConfig) {
    res = (*playerConfig)
              ->GetConfiguration(playerConfig,
                                 (const SLchar *)"androidGetAudioLatency",
                                 &paramSize, &audioLatency);
    if (res == SL_RESULT_SUCCESS) {
      LOG("Got playback latency using android configuration extension");
      stm->output_latency_ms = audioLatency;
    }
  }
  // `playerConfig` is available, but the above failed, or `playerConfig` is not
  // available. In both cases, we need to acquire the output latency by an other
  // mean.
  if ((playerConfig && res != SL_RESULT_SUCCESS) || !playerConfig) {
    if (cubeb_output_latency_method_is_loaded(
            stm->context->p_output_latency_function)) {
      LOG("Got playback latency using JNI");
      stm->output_latency_ms =
          cubeb_get_output_latency(stm->context->p_output_latency_function);
    } else {
      LOG("No alternate latency querying method loaded, A/V sync will be off.");
      stm->output_latency_ms = 0;
    }
  }

  LOG("Audio output latency: %dms", stm->output_latency_ms);

  res =
      (*stm->playerObj)
          ->GetInterface(stm->playerObj, stm->context->SL_IID_PLAY, &stm->play);
  if (res != SL_RESULT_SUCCESS) {
    LOG("Failed to get play interface. Error code: %lu", res);
    return CUBEB_ERROR;
  }

  res = (*stm->playerObj)
            ->GetInterface(stm->playerObj, stm->context->SL_IID_BUFFERQUEUE,
                           &stm->bufq);
  if (res != SL_RESULT_SUCCESS) {
    LOG("Failed to get bufferqueue interface. Error code: %lu", res);
    return CUBEB_ERROR;
  }

  res = (*stm->playerObj)
            ->GetInterface(stm->playerObj, stm->context->SL_IID_VOLUME,
                           &stm->volume);
  if (res != SL_RESULT_SUCCESS) {
    LOG("Failed to get volume interface. Error code: %lu", res);
    return CUBEB_ERROR;
  }

  res = (*stm->play)->RegisterCallback(stm->play, play_callback, stm);
  if (res != SL_RESULT_SUCCESS) {
    LOG("Failed to register play callback. Error code: %lu", res);
    return CUBEB_ERROR;
  }

  // Work around wilhelm/AudioTrack badness, bug 1221228
  (*stm->play)->SetMarkerPosition(stm->play, (SLmillisecond)0);

  res = (*stm->play)
            ->SetCallbackEventsMask(stm->play,
                                    (SLuint32)SL_PLAYEVENT_HEADATMARKER);
  if (res != SL_RESULT_SUCCESS) {
    LOG("Failed to set headatmarker event mask. Error code: %lu", res);
    return CUBEB_ERROR;
  }

  slBufferQueueCallback player_callback = bufferqueue_callback;
  if (stm->input_enabled) {
    player_callback = player_fullduplex_callback;
  }
  res = (*stm->bufq)->RegisterCallback(stm->bufq, player_callback, stm);
  if (res != SL_RESULT_SUCCESS) {
    LOG("Failed to register bufferqueue callback. Error code: %lu", res);
    return CUBEB_ERROR;
  }

  {
    // Enqueue a silent frame so once the player becomes playing, the frame
    // will be consumed and kick off the buffer queue callback.
    // Note the duration of a single frame is less than 1ms. We don't bother
    // adjusting the playback position.
    uint8_t * buf = stm->queuebuf[stm->queuebuf_idx++];
    memset(buf, 0, stm->framesize);
    res = (*stm->bufq)->Enqueue(stm->bufq, buf, stm->framesize);
    assert(res == SL_RESULT_SUCCESS);
  }

  LOG("Cubeb stream init playback success");
  return CUBEB_OK;
}

static int
opensl_validate_stream_param(cubeb_stream_params * stream_params)
{
  if ((stream_params &&
       (stream_params->channels < 1 || stream_params->channels > 32))) {
    return CUBEB_ERROR_INVALID_FORMAT;
  }
  if ((stream_params && (stream_params->prefs & CUBEB_STREAM_PREF_LOOPBACK))) {
    LOG("Loopback is not supported");
    return CUBEB_ERROR_NOT_SUPPORTED;
  }
  return CUBEB_OK;
}

int
has_pref_set(cubeb_stream_params * input_params,
             cubeb_stream_params * output_params, cubeb_stream_prefs pref)
{
  return (input_params && input_params->prefs & pref) ||
         (output_params && output_params->prefs & pref);
}

static int
opensl_stream_init(cubeb * ctx, cubeb_stream ** stream,
                   char const * stream_name, cubeb_devid input_device,
                   cubeb_stream_params * input_stream_params,
                   cubeb_devid output_device,
                   cubeb_stream_params * output_stream_params,
                   unsigned int latency_frames,
                   cubeb_data_callback data_callback,
                   cubeb_state_callback state_callback, void * user_ptr)
{
  cubeb_stream * stm;

  assert(ctx);
  if (input_device || output_device) {
    LOG("Device selection is not supported in Android. The default will be "
        "used");
  }

  *stream = NULL;

  int r = opensl_validate_stream_param(output_stream_params);
  if (r != CUBEB_OK) {
    LOG("Output stream params not valid");
    return r;
  }
  r = opensl_validate_stream_param(input_stream_params);
  if (r != CUBEB_OK) {
    LOG("Input stream params not valid");
    return r;
  }

  stm = calloc(1, sizeof(*stm));
  assert(stm);

  stm->context = ctx;
  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->user_ptr = user_ptr;
  stm->buffer_size_frames =
      latency_frames ? latency_frames : DEFAULT_NUM_OF_FRAMES;
  stm->input_enabled = (input_stream_params) ? 1 : 0;
  stm->output_enabled = (output_stream_params) ? 1 : 0;
  stm->shutdown = 1;
  stm->voice_input =
      has_pref_set(input_stream_params, NULL, CUBEB_STREAM_PREF_VOICE);
  stm->voice_output =
      has_pref_set(NULL, output_stream_params, CUBEB_STREAM_PREF_VOICE);

  LOG("cubeb stream prefs: voice_input: %s voice_output: %s",
      stm->voice_input ? "true" : "false",
      stm->voice_output ? "true" : "false");

#ifdef DEBUG
  pthread_mutexattr_t attr;
  pthread_mutexattr_init(&attr);
  pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK);
  r = pthread_mutex_init(&stm->mutex, &attr);
#else
  r = pthread_mutex_init(&stm->mutex, NULL);
#endif
  assert(r == 0);

  if (output_stream_params) {
    LOG("Playback params: Rate %d, channels %d, format %d, latency in frames "
        "%d.",
        output_stream_params->rate, output_stream_params->channels,
        output_stream_params->format, stm->buffer_size_frames);
    r = opensl_configure_playback(stm, output_stream_params);
    if (r != CUBEB_OK) {
      opensl_stream_destroy(stm);
      return r;
    }
  }

  if (input_stream_params) {
    LOG("Capture params: Rate %d, channels %d, format %d, latency in frames "
        "%d.",
        input_stream_params->rate, input_stream_params->channels,
        input_stream_params->format, stm->buffer_size_frames);
    r = opensl_configure_capture(stm, input_stream_params);
    if (r != CUBEB_OK) {
      opensl_stream_destroy(stm);
      return r;
    }
  }

  /* Configure resampler*/
  uint32_t target_sample_rate;
  if (input_stream_params) {
    target_sample_rate = input_stream_params->rate;
  } else {
    assert(output_stream_params);
    target_sample_rate = output_stream_params->rate;
  }

  // Use the actual configured rates for input
  // and output.
  cubeb_stream_params input_params;
  if (input_stream_params) {
    input_params = *input_stream_params;
    input_params.rate = stm->input_device_rate;
  }
  cubeb_stream_params output_params;
  if (output_stream_params) {
    output_params = *output_stream_params;
    output_params.rate = stm->output_configured_rate;
  }

  stm->resampler = cubeb_resampler_create(
      stm, input_stream_params ? &input_params : NULL,
      output_stream_params ? &output_params : NULL, target_sample_rate,
      data_callback, user_ptr, CUBEB_RESAMPLER_QUALITY_DEFAULT,
      CUBEB_RESAMPLER_RECLOCK_NONE);
  if (!stm->resampler) {
    LOG("Failed to create resampler");
    opensl_stream_destroy(stm);
    return CUBEB_ERROR;
  }

  *stream = stm;
  LOG("Cubeb stream (%p) init success", stm);
  return CUBEB_OK;
}

static int
opensl_start_player(cubeb_stream * stm)
{
  assert(stm->playerObj);
  SLuint32 playerState;
  (*stm->playerObj)->GetState(stm->playerObj, &playerState);
  if (playerState == SL_OBJECT_STATE_REALIZED) {
    SLresult res = (*stm->play)->SetPlayState(stm->play, SL_PLAYSTATE_PLAYING);
    if (res != SL_RESULT_SUCCESS) {
      LOG("Failed to start player. Error code: %lu", res);
      return CUBEB_ERROR;
    }
  }
  return CUBEB_OK;
}

static int
opensl_start_recorder(cubeb_stream * stm)
{
  assert(stm->recorderObj);
  SLuint32 recorderState;
  (*stm->recorderObj)->GetState(stm->recorderObj, &recorderState);
  if (recorderState == SL_OBJECT_STATE_REALIZED) {
    SLresult res =
        (*stm->recorderItf)
            ->SetRecordState(stm->recorderItf, SL_RECORDSTATE_RECORDING);
    if (res != SL_RESULT_SUCCESS) {
      LOG("Failed to start recorder. Error code: %lu", res);
      return CUBEB_ERROR;
    }
  }
  return CUBEB_OK;
}

static int
opensl_stream_start(cubeb_stream * stm)
{
  assert(stm);

  int r = pthread_mutex_lock(&stm->mutex);
  assert(r == 0);
  opensl_set_shutdown(stm, 0);
  opensl_set_draining(stm, 0);
  r = pthread_mutex_unlock(&stm->mutex);
  assert(r == 0);

  if (stm->playerObj) {
    r = opensl_start_player(stm);
    if (r != CUBEB_OK) {
      return r;
    }
  }

  if (stm->recorderObj) {
    int r = opensl_start_recorder(stm);
    if (r != CUBEB_OK) {
      return r;
    }
  }

  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STARTED);
  LOG("Cubeb stream (%p) started", stm);
  return CUBEB_OK;
}

static int
opensl_stop_player(cubeb_stream * stm)
{
  assert(stm->playerObj);
  assert(stm->shutdown || stm->draining);

  SLresult res = (*stm->play)->SetPlayState(stm->play, SL_PLAYSTATE_PAUSED);
  if (res != SL_RESULT_SUCCESS) {
    LOG("Failed to stop player. Error code: %lu", res);
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

static int
opensl_stop_recorder(cubeb_stream * stm)
{
  assert(stm->recorderObj);
  assert(stm->shutdown || stm->draining);

  SLresult res = (*stm->recorderItf)
                     ->SetRecordState(stm->recorderItf, SL_RECORDSTATE_PAUSED);
  if (res != SL_RESULT_SUCCESS) {
    LOG("Failed to stop recorder. Error code: %lu", res);
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

static int
opensl_stream_stop(cubeb_stream * stm)
{
  assert(stm);

  int r = pthread_mutex_lock(&stm->mutex);
  assert(r == 0);
  opensl_set_shutdown(stm, 1);
  r = pthread_mutex_unlock(&stm->mutex);
  assert(r == 0);

  if (stm->playerObj) {
    r = opensl_stop_player(stm);
    if (r != CUBEB_OK) {
      return r;
    }
  }

  if (stm->recorderObj) {
    int r = opensl_stop_recorder(stm);
    if (r != CUBEB_OK) {
      return r;
    }
  }

  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STOPPED);
  LOG("Cubeb stream (%p) stopped", stm);
  return CUBEB_OK;
}

static int
opensl_destroy_recorder(cubeb_stream * stm)
{
  assert(stm);
  assert(stm->recorderObj);

  if (stm->recorderBufferQueueItf) {
    SLresult res =
        (*stm->recorderBufferQueueItf)->Clear(stm->recorderBufferQueueItf);
    if (res != SL_RESULT_SUCCESS) {
      LOG("Failed to clear recorder buffer queue. Error code: %lu", res);
      return CUBEB_ERROR;
    }
    stm->recorderBufferQueueItf = NULL;
    for (uint32_t i = 0; i < stm->input_array_capacity; ++i) {
      free(stm->input_buffer_array[i]);
    }
  }

  (*stm->recorderObj)->Destroy(stm->recorderObj);
  stm->recorderObj = NULL;
  stm->recorderItf = NULL;

  if (stm->input_queue) {
    array_queue_destroy(stm->input_queue);
  }
  free(stm->input_silent_buffer);

  return CUBEB_OK;
}

static void
opensl_stream_destroy(cubeb_stream * stm)
{
  assert(stm->draining || stm->shutdown);

  if (stm->playerObj) {
    (*stm->playerObj)->Destroy(stm->playerObj);
    stm->playerObj = NULL;
    stm->play = NULL;
    stm->bufq = NULL;
    for (uint32_t i = 0; i < stm->queuebuf_capacity; ++i) {
      free(stm->queuebuf[i]);
    }
  }

  if (stm->recorderObj) {
    int r = opensl_destroy_recorder(stm);
    assert(r == CUBEB_OK);
  }

  if (stm->resampler) {
    cubeb_resampler_destroy(stm->resampler);
  }

  pthread_mutex_destroy(&stm->mutex);

  LOG("Cubeb stream (%p) destroyed", stm);
  free(stm);
}

static int
opensl_stream_get_position(cubeb_stream * stm, uint64_t * position)
{
  SLmillisecond msec;
  uint32_t compensation_msec = 0;
  SLresult res;

  res = (*stm->play)->GetPosition(stm->play, &msec);
  if (res != SL_RESULT_SUCCESS)
    return CUBEB_ERROR;

  struct timespec t;
  clock_gettime(CLOCK_MONOTONIC, &t);
  if (stm->lastPosition == msec) {
    compensation_msec =
        (t.tv_sec * 1000000000LL + t.tv_nsec - stm->lastPositionTimeStamp) /
        1000000;
  } else {
    stm->lastPositionTimeStamp = t.tv_sec * 1000000000LL + t.tv_nsec;
    stm->lastPosition = msec;
  }

  uint64_t samplerate = stm->user_output_rate;
  uint32_t output_latency = stm->output_latency_ms;

  pthread_mutex_lock(&stm->mutex);
  int64_t maximum_position = stm->written * (int64_t)stm->user_output_rate /
                             stm->output_configured_rate;
  pthread_mutex_unlock(&stm->mutex);
  assert(maximum_position >= 0);

  if (msec > output_latency) {
    int64_t unadjusted_position;
    if (stm->lastCompensativePosition > msec + compensation_msec) {
      // Over compensation, use lastCompensativePosition.
      unadjusted_position =
          samplerate * (stm->lastCompensativePosition - output_latency) / 1000;
    } else {
      unadjusted_position =
          samplerate * (msec - output_latency + compensation_msec) / 1000;
      stm->lastCompensativePosition = msec + compensation_msec;
    }
    *position = unadjusted_position < maximum_position ? unadjusted_position
                                                       : maximum_position;
  } else {
    *position = 0;
  }
  return CUBEB_OK;
}

static int
opensl_stream_get_latency(cubeb_stream * stm, uint32_t * latency)
{
  assert(stm);
  assert(latency);

  uint32_t stream_latency_frames =
      stm->user_output_rate * stm->output_latency_ms / 1000;

  return stream_latency_frames + cubeb_resampler_latency(stm->resampler);
}

int
opensl_stream_set_volume(cubeb_stream * stm, float volume)
{
  SLresult res;
  SLmillibel max_level, millibels;
  float unclamped_millibels;

  res = (*stm->volume)->GetMaxVolumeLevel(stm->volume, &max_level);

  if (res != SL_RESULT_SUCCESS) {
    return CUBEB_ERROR;
  }

  /* millibels are 100*dB, so the conversion from the volume's linear amplitude
   * is 100 * 20 * log(volume). However we clamp the resulting value before
   * passing it to lroundf() in order to prevent it from silently returning an
   * erroneous value when the unclamped value exceeds the size of a long. */
  unclamped_millibels = 100.0f * 20.0f * log10f(fmaxf(volume, 0.0f));
  unclamped_millibels = fmaxf(unclamped_millibels, SL_MILLIBEL_MIN);
  unclamped_millibels = fminf(unclamped_millibels, max_level);

  millibels = lroundf(unclamped_millibels);

  res = (*stm->volume)->SetVolumeLevel(stm->volume, millibels);

  if (res != SL_RESULT_SUCCESS) {
    return CUBEB_ERROR;
  }
  return CUBEB_OK;
}

static struct cubeb_ops const opensl_ops = {
    .init = opensl_init,
    .get_backend_id = opensl_get_backend_id,
    .get_max_channel_count = opensl_get_max_channel_count,
    .get_min_latency = NULL,
    .get_preferred_sample_rate = NULL,
    .enumerate_devices = NULL,
    .device_collection_destroy = NULL,
    .destroy = opensl_destroy,
    .stream_init = opensl_stream_init,
    .stream_destroy = opensl_stream_destroy,
    .stream_start = opensl_stream_start,
    .stream_stop = opensl_stream_stop,
    .stream_get_position = opensl_stream_get_position,
    .stream_get_latency = opensl_stream_get_latency,
    .stream_get_input_latency = NULL,
    .stream_set_volume = opensl_stream_set_volume,
    .stream_set_name = NULL,
    .stream_get_current_device = NULL,
    .stream_device_destroy = NULL,
    .stream_register_device_changed_callback = NULL,
    .register_device_collection_changed = NULL};
