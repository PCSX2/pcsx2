/* ex: set tabstop=2 shiftwidth=2 expandtab:
 * Copyright © 2019 Jan Kelling
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#include "cubeb-internal.h"
#include "cubeb/cubeb.h"
#include "cubeb_android.h"
#include "cubeb_log.h"
#include "cubeb_resampler.h"
#include <aaudio/AAudio.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <dlfcn.h>
#include <memory>
#include <mutex>
#include <thread>
#include <time.h>

#ifdef DISABLE_LIBAAUDIO_DLOPEN
#define WRAP(x) x
#else
#define WRAP(x) (*cubeb_##x)
#define LIBAAUDIO_API_VISIT(X)                                                 \
  X(AAudio_convertResultToText)                                                \
  X(AAudio_convertStreamStateToText)                                           \
  X(AAudio_createStreamBuilder)                                                \
  X(AAudioStreamBuilder_openStream)                                            \
  X(AAudioStreamBuilder_setChannelCount)                                       \
  X(AAudioStreamBuilder_setBufferCapacityInFrames)                             \
  X(AAudioStreamBuilder_setDirection)                                          \
  X(AAudioStreamBuilder_setFormat)                                             \
  X(AAudioStreamBuilder_setSharingMode)                                        \
  X(AAudioStreamBuilder_setPerformanceMode)                                    \
  X(AAudioStreamBuilder_setSampleRate)                                         \
  X(AAudioStreamBuilder_delete)                                                \
  X(AAudioStreamBuilder_setDataCallback)                                       \
  X(AAudioStreamBuilder_setErrorCallback)                                      \
  X(AAudioStream_close)                                                        \
  X(AAudioStream_read)                                                         \
  X(AAudioStream_requestStart)                                                 \
  X(AAudioStream_requestPause)                                                 \
  X(AAudioStream_setBufferSizeInFrames)                                        \
  X(AAudioStream_getTimestamp)                                                 \
  X(AAudioStream_requestFlush)                                                 \
  X(AAudioStream_requestStop)                                                  \
  X(AAudioStream_getPerformanceMode)                                           \
  X(AAudioStream_getSharingMode)                                               \
  X(AAudioStream_getBufferSizeInFrames)                                        \
  X(AAudioStream_getBufferCapacityInFrames)                                    \
  X(AAudioStream_getSampleRate)                                                \
  X(AAudioStream_waitForStateChange)                                           \
  X(AAudioStream_getFramesRead)                                                \
  X(AAudioStream_getState)                                                     \
  X(AAudioStream_getFramesWritten)                                             \
  X(AAudioStream_getFramesPerBurst)                                            \
  X(AAudioStreamBuilder_setInputPreset)                                        \
  X(AAudioStreamBuilder_setUsage)

// not needed or added later on
// X(AAudioStreamBuilder_setFramesPerDataCallback) \
  // X(AAudioStreamBuilder_setDeviceId)              \
  // X(AAudioStreamBuilder_setSamplesPerFrame)       \
  // X(AAudioStream_getSamplesPerFrame)              \
  // X(AAudioStream_getDeviceId)                     \
  // X(AAudioStream_write)                           \
  // X(AAudioStream_getChannelCount)                 \
  // X(AAudioStream_getFormat)                       \
  // X(AAudioStream_getXRunCount)                    \
  // X(AAudioStream_isMMapUsed)                      \
  // X(AAudioStreamBuilder_setContentType)           \
  // X(AAudioStreamBuilder_setSessionId)             \
  // X(AAudioStream_getUsage)                        \
  // X(AAudioStream_getContentType)                  \
  // X(AAudioStream_getInputPreset)                  \
  // X(AAudioStream_getSessionId)                    \
// END: not needed or added later on

#define MAKE_TYPEDEF(x) static decltype(x) * cubeb_##x;
LIBAAUDIO_API_VISIT(MAKE_TYPEDEF)
#undef MAKE_TYPEDEF
#endif

const uint8_t MAX_STREAMS = 16;

using unique_lock = std::unique_lock<std::mutex>;
using lock_guard = std::lock_guard<std::mutex>;

enum class stream_state {
  INIT = 0,
  STOPPED,
  STOPPING,
  STARTED,
  STARTING,
  DRAINING,
  ERROR,
  SHUTDOWN,
};

struct cubeb_stream {
  /* Note: Must match cubeb_stream layout in cubeb.c. */
  cubeb * context{};
  void * user_ptr{};

  std::atomic<bool> in_use{false};
  std::atomic<stream_state> state{stream_state::INIT};

  AAudioStream * ostream{};
  AAudioStream * istream{};
  cubeb_data_callback data_callback{};
  cubeb_state_callback state_callback{};
  cubeb_resampler * resampler{};

  // mutex synchronizes access to the stream from the state thread
  // and user-called functions. Everything that is accessed in the
  // aaudio data (or error) callback is synchronized only via atomics.
  std::mutex mutex;

  std::unique_ptr<char[]> in_buf;
  unsigned in_frame_size{}; // size of one input frame

  cubeb_sample_format out_format{};
  std::atomic<float> volume{1.f};
  unsigned out_channels{};
  unsigned out_frame_size{};
  int64_t latest_output_latency = 0;
  int64_t latest_input_latency = 0;
  bool voice_input;
  bool voice_output;
  uint64_t previous_clock;
};

struct cubeb {
  struct cubeb_ops const * ops{};
  void * libaaudio{};

  struct {
    // The state thread: it waits for state changes and stops
    // drained streams.
    std::thread thread;
    std::thread notifier;
    std::mutex mutex;
    std::condition_variable cond;
    std::atomic<bool> join{false};
    std::atomic<bool> waiting{false};
  } state;

  // streams[i].in_use signals whether a stream is used
  struct cubeb_stream streams[MAX_STREAMS];
};

// Only allowed from state thread, while mutex on stm is locked
static void
shutdown(cubeb_stream * stm)
{
  if (stm->istream) {
    WRAP(AAudioStream_requestStop)(stm->istream);
  }
  if (stm->ostream) {
    WRAP(AAudioStream_requestStop)(stm->ostream);
  }

  stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_ERROR);
  stm->state.store(stream_state::SHUTDOWN);
}

// Returns whether the given state is one in which we wait for
// an asynchronous change
static bool
waiting_state(stream_state state)
{
  switch (state) {
  case stream_state::DRAINING:
  case stream_state::STARTING:
  case stream_state::STOPPING:
    return true;
  default:
    return false;
  }
}

static void
update_state(cubeb_stream * stm)
{
  // Fast path for streams that don't wait for state change or are invalid
  enum stream_state old_state = stm->state.load();
  if (old_state == stream_state::INIT || old_state == stream_state::STARTED ||
      old_state == stream_state::STOPPED ||
      old_state == stream_state::SHUTDOWN) {
    return;
  }

  // If the main thread currently operates on this thread, we don't
  // have to wait for it
  unique_lock lock(stm->mutex, std::try_to_lock);
  if (!lock.owns_lock()) {
    return;
  }

  // check again: if this is true now, the stream was destroyed or
  // changed between our fast path check and locking the mutex
  old_state = stm->state.load();
  if (old_state == stream_state::INIT || old_state == stream_state::STARTED ||
      old_state == stream_state::STOPPED ||
      old_state == stream_state::SHUTDOWN) {
    return;
  }

  // We compute the new state the stream has and then compare_exchange it
  // if it has changed. This way we will never just overwrite state
  // changes that were set from the audio thread in the meantime,
  // such as a DRAINING or error state.
  enum stream_state new_state;
  do {
    if (old_state == stream_state::SHUTDOWN) {
      return;
    }

    if (old_state == stream_state::ERROR) {
      shutdown(stm);
      return;
    }

    new_state = old_state;

    aaudio_stream_state_t istate = 0;
    aaudio_stream_state_t ostate = 0;

    // We use waitForStateChange (with zero timeout) instead of just
    // getState since only the former internally updates the state.
    // See the docs of aaudio getState/waitForStateChange for details,
    // why we are passing STATE_UNKNOWN.
    aaudio_result_t res;
    if (stm->istream) {
      res = WRAP(AAudioStream_waitForStateChange)(
          stm->istream, AAUDIO_STREAM_STATE_UNKNOWN, &istate, 0);
      if (res != AAUDIO_OK) {
        LOG("AAudioStream_waitForStateChanged: %s",
            WRAP(AAudio_convertResultToText)(res));
        return;
      }
      assert(istate);
    }

    if (stm->ostream) {
      res = WRAP(AAudioStream_waitForStateChange)(
          stm->ostream, AAUDIO_STREAM_STATE_UNKNOWN, &ostate, 0);
      if (res != AAUDIO_OK) {
        LOG("AAudioStream_waitForStateChanged: %s",
            WRAP(AAudio_convertResultToText)(res));
        return;
      }
      assert(ostate);
    }

    // handle invalid stream states
    if (istate == AAUDIO_STREAM_STATE_PAUSING ||
        istate == AAUDIO_STREAM_STATE_PAUSED ||
        istate == AAUDIO_STREAM_STATE_FLUSHING ||
        istate == AAUDIO_STREAM_STATE_FLUSHED ||
        istate == AAUDIO_STREAM_STATE_UNKNOWN ||
        istate == AAUDIO_STREAM_STATE_DISCONNECTED) {
      const char * name = WRAP(AAudio_convertStreamStateToText)(istate);
      LOG("Unexpected android input stream state %s", name);
      shutdown(stm);
      return;
    }

    if (ostate == AAUDIO_STREAM_STATE_PAUSING ||
        ostate == AAUDIO_STREAM_STATE_PAUSED ||
        ostate == AAUDIO_STREAM_STATE_FLUSHING ||
        ostate == AAUDIO_STREAM_STATE_FLUSHED ||
        ostate == AAUDIO_STREAM_STATE_UNKNOWN ||
        ostate == AAUDIO_STREAM_STATE_DISCONNECTED) {
      const char * name = WRAP(AAudio_convertStreamStateToText)(istate);
      LOG("Unexpected android output stream state %s", name);
      shutdown(stm);
      return;
    }

    switch (old_state) {
    case stream_state::STARTING:
      if ((!istate || istate == AAUDIO_STREAM_STATE_STARTED) &&
          (!ostate || ostate == AAUDIO_STREAM_STATE_STARTED)) {
        stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STARTED);
        new_state = stream_state::STARTED;
      }
      break;
    case stream_state::DRAINING:
      // The DRAINING state means that we want to stop the streams but
      // may not have done so yet.
      // The aaudio docs state that returning STOP from the callback isn't
      // enough, the stream has to be stopped from another thread
      // afterwards.
      // No callbacks are triggered anymore when requestStop returns.
      // That is important as we otherwise might read from a closed istream
      // for a duplex stream.
      // Therefor it is important to close ostream first.
      if (ostate && ostate != AAUDIO_STREAM_STATE_STOPPING &&
          ostate != AAUDIO_STREAM_STATE_STOPPED) {
        res = WRAP(AAudioStream_requestStop)(stm->ostream);
        if (res != AAUDIO_OK) {
          LOG("AAudioStream_requestStop: %s",
              WRAP(AAudio_convertResultToText)(res));
          return;
        }
      }
      if (istate && istate != AAUDIO_STREAM_STATE_STOPPING &&
          istate != AAUDIO_STREAM_STATE_STOPPED) {
        res = WRAP(AAudioStream_requestStop)(stm->istream);
        if (res != AAUDIO_OK) {
          LOG("AAudioStream_requestStop: %s",
              WRAP(AAudio_convertResultToText)(res));
          return;
        }
      }

      // we always wait until both streams are stopped until we
      // send CUBEB_STATE_DRAINED. Then we can directly transition
      // our logical state to STOPPED, not triggering
      // an additional CUBEB_STATE_STOPPED callback (which might
      // be unexpected for the user).
      if ((!ostate || ostate == AAUDIO_STREAM_STATE_STOPPED) &&
          (!istate || istate == AAUDIO_STREAM_STATE_STOPPED)) {
        new_state = stream_state::STOPPED;
        stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_DRAINED);
      }
      break;
    case stream_state::STOPPING:
      assert(!istate || istate == AAUDIO_STREAM_STATE_STOPPING ||
             istate == AAUDIO_STREAM_STATE_STOPPED);
      assert(!ostate || ostate == AAUDIO_STREAM_STATE_STOPPING ||
             ostate == AAUDIO_STREAM_STATE_STOPPED);
      if ((!istate || istate == AAUDIO_STREAM_STATE_STOPPED) &&
          (!ostate || ostate == AAUDIO_STREAM_STATE_STOPPED)) {
        stm->state_callback(stm, stm->user_ptr, CUBEB_STATE_STOPPED);
        new_state = stream_state::STOPPED;
      }
      break;
    default:
      assert(false && "Unreachable: invalid state");
    }
  } while (old_state != new_state &&
           !stm->state.compare_exchange_strong(old_state, new_state));
}

// See https://nyorain.github.io/lock-free-wakeup.html for a note
// why this is needed. The audio thread notifies the state thread about
// state changes and must not block. The state thread on the other hand should
// sleep until there is work to be done. So we need a lockfree producer
// and blocking producer. This can only be achieved safely with a new thread
// that only serves as notifier backup (in case the notification happens
// right between the state thread checking and going to sleep in which case
// this thread will kick in and signal it right again).
static void
notifier_thread(cubeb * ctx)
{
  unique_lock lock(ctx->state.mutex);

  while (!ctx->state.join.load()) {
    ctx->state.cond.wait(lock);
    if (ctx->state.waiting.load()) {
      // This must signal our state thread since there is no other
      // thread currently waiting on the condition variable.
      // The state change thread is guaranteed to be waiting since
      // we hold the mutex it locks when awake.
      ctx->state.cond.notify_one();
    }
  }

  // make sure other thread joins as well
  ctx->state.cond.notify_one();
  LOG("Exiting notifier thread");
}

static void
state_thread(cubeb * ctx)
{
  unique_lock lock(ctx->state.mutex);

  bool waiting = false;
  while (!ctx->state.join.load()) {
    waiting |= ctx->state.waiting.load();
    if (waiting) {
      ctx->state.waiting.store(false);
      waiting = false;
      for (unsigned i = 0u; i < MAX_STREAMS; ++i) {
        cubeb_stream * stm = &ctx->streams[i];
        update_state(stm);
        waiting |= waiting_state(atomic_load(&stm->state));
      }

      // state changed from another thread, update again immediately
      if (ctx->state.waiting.load()) {
        waiting = true;
        continue;
      }

      // Not waiting for any change anymore: we can wait on the
      // condition variable without timeout
      if (!waiting) {
        continue;
      }

      // while any stream is waiting for state change we sleep with regular
      // timeouts. But we wake up immediately if signaled.
      // This might seem like a poor man's implementation of state change
      // waiting but (as of october 2020), the implementation of
      // AAudioStream_waitForStateChange is just sleeping with regular
      // timeouts as well:
      // https://android.googlesource.com/platform/frameworks/av/+/refs/heads/master/media/libaaudio/src/core/AudioStream.cpp
      auto dur = std::chrono::milliseconds(5);
      ctx->state.cond.wait_for(lock, dur);
    } else {
      ctx->state.cond.wait(lock);
    }
  }

  // make sure other thread joins as well
  ctx->state.cond.notify_one();
  LOG("Exiting state thread");
}

static char const *
aaudio_get_backend_id(cubeb * /* ctx */)
{
  return "aaudio";
}

static int
aaudio_get_max_channel_count(cubeb * ctx, uint32_t * max_channels)
{
  assert(ctx && max_channels);
  // NOTE: we might get more, AAudio docs don't specify anything.
  *max_channels = 2;
  return CUBEB_OK;
}

static void
aaudio_destroy(cubeb * ctx)
{
  assert(ctx);

#ifndef NDEBUG
  // make sure all streams were destroyed
  for (unsigned i = 0u; i < MAX_STREAMS; ++i) {
    assert(!ctx->streams[i].in_use.load());
  }
#endif

  // broadcast joining to both threads
  // they will additionally signal each other before joining
  ctx->state.join.store(true);
  ctx->state.cond.notify_all();

  if (ctx->state.thread.joinable()) {
    ctx->state.thread.join();
  }
  if (ctx->state.notifier.joinable()) {
    ctx->state.notifier.join();
  }
#ifndef DISABLE_LIBAAUDIO_DLOPEN
  if (ctx->libaaudio) {
    dlclose(ctx->libaaudio);
  }
#endif
  delete ctx;
}

static void
apply_volume(cubeb_stream * stm, void * audio_data, uint32_t num_frames)
{
  float volume = stm->volume.load();
  // optimization: we don't have to change anything in this case
  if (volume == 1.f) {
    return;
  }

  switch (stm->out_format) {
  case CUBEB_SAMPLE_S16NE:
    for (uint32_t i = 0u; i < num_frames * stm->out_channels; ++i) {
      (static_cast<int16_t *>(audio_data))[i] *= volume;
    }
    break;
  case CUBEB_SAMPLE_FLOAT32NE:
    for (uint32_t i = 0u; i < num_frames * stm->out_channels; ++i) {
      (static_cast<float *>(audio_data))[i] *= volume;
    }
    break;
  default:
    assert(false && "Unreachable: invalid stream out_format");
  }
}

// Returning AAUDIO_CALLBACK_RESULT_STOP seems to put the stream in
// an invalid state. Seems like an AAudio bug/bad documentation.
// We therefore only return it on error.

static aaudio_data_callback_result_t
aaudio_duplex_data_cb(AAudioStream * astream, void * user_data,
                      void * audio_data, int32_t num_frames)
{
  cubeb_stream * stm = (cubeb_stream *)user_data;
  assert(stm->ostream == astream);
  assert(stm->istream);
  assert(num_frames >= 0);

  stream_state state = atomic_load(&stm->state);
  // int istate = WRAP(AAudioStream_getState)(stm->istream);
  // int ostate = WRAP(AAudioStream_getState)(stm->ostream);
  // ALOGV("aaudio duplex data cb on stream %p: state %ld (in: %d, out: %d),
  // num_frames: %ld",
  //     (void*) stm, state, istate, ostate, num_frames);

  // all other states may happen since the callback might be called
  // from within requestStart
  assert(state != stream_state::SHUTDOWN);

  // This might happen when we started draining but not yet actually
  // stopped the stream from the state thread.
  if (state == stream_state::DRAINING) {
    std::memset(audio_data, 0x0, num_frames * stm->out_frame_size);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }

  // The aaudio docs state that AAudioStream_read must not be called on
  // the stream associated with a callback. But we call it on the input stream
  // while this callback is for the output stream so this is ok.
  // We also pass timeout 0, giving us strong non-blocking guarantees.
  // This is exactly how it's done in the aaudio duplex example code snippet.
  long in_num_frames =
      WRAP(AAudioStream_read)(stm->istream, stm->in_buf.get(), num_frames, 0);
  if (in_num_frames < 0) { // error
    stm->state.store(stream_state::ERROR);
    LOG("AAudioStream_read: %s",
        WRAP(AAudio_convertResultToText)(in_num_frames));
    return AAUDIO_CALLBACK_RESULT_STOP;
  }

  // This can happen shortly after starting the stream. AAudio might immediately
  // begin to buffer output but not have any input ready yet. We could
  // block AAudioStream_read (passing a timeout > 0) but that leads to issues
  // since blocking in this callback is a bad idea in general and it might break
  // the stream when it is stopped by another thread shortly after being
  // started. We therefore simply send silent input to the application, as shown
  // in the AAudio duplex stream code example.
  if (in_num_frames < num_frames) {
    // LOG("AAudioStream_read returned not enough frames: %ld instead of %d",
    //   in_num_frames, num_frames);
    unsigned left = num_frames - in_num_frames;
    char * buf = stm->in_buf.get() + in_num_frames * stm->in_frame_size;
    std::memset(buf, 0x0, left * stm->in_frame_size);
    in_num_frames = num_frames;
  }

  long done_frames =
      cubeb_resampler_fill(stm->resampler, stm->in_buf.get(), &in_num_frames,
                           audio_data, num_frames);

  if (done_frames < 0 || done_frames > num_frames) {
    LOG("Error in data callback or resampler: %ld", done_frames);
    stm->state.store(stream_state::ERROR);
    return AAUDIO_CALLBACK_RESULT_STOP;
  } else if (done_frames < num_frames) {
    stm->state.store(stream_state::DRAINING);
    stm->context->state.waiting.store(true);
    stm->context->state.cond.notify_one();

    char * begin =
        static_cast<char *>(audio_data) + done_frames * stm->out_frame_size;
    std::memset(begin, 0x0, (num_frames - done_frames) * stm->out_frame_size);
  }

  apply_volume(stm, audio_data, done_frames);
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static aaudio_data_callback_result_t
aaudio_output_data_cb(AAudioStream * astream, void * user_data,
                      void * audio_data, int32_t num_frames)
{
  cubeb_stream * stm = (cubeb_stream *)user_data;
  assert(stm->ostream == astream);
  assert(!stm->istream);
  assert(num_frames >= 0);

  stream_state state = stm->state.load();
  // int ostate = WRAP(AAudioStream_getState)(stm->ostream);
  // ALOGV("aaudio output data cb on stream %p: state %ld (%d), num_frames:
  // %ld",
  //     (void*) stm, state, ostate, num_frames);

  // all other states may happen since the callback might be called
  // from within requestStart
  assert(state != stream_state::SHUTDOWN);

  // This might happen when we started draining but not yet actually
  // stopped the stream from the state thread.
  if (state == stream_state::DRAINING) {
    std::memset(audio_data, 0x0, num_frames * stm->out_frame_size);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }

  long done_frames =
      cubeb_resampler_fill(stm->resampler, NULL, NULL, audio_data, num_frames);
  if (done_frames < 0 || done_frames > num_frames) {
    LOG("Error in data callback or resampler: %ld", done_frames);
    stm->state.store(stream_state::ERROR);
    return AAUDIO_CALLBACK_RESULT_STOP;
  } else if (done_frames < num_frames) {
    stm->state.store(stream_state::DRAINING);
    stm->context->state.waiting.store(true);
    stm->context->state.cond.notify_one();

    char * begin =
        static_cast<char *>(audio_data) + done_frames * stm->out_frame_size;
    std::memset(begin, 0x0, (num_frames - done_frames) * stm->out_frame_size);
  }

  apply_volume(stm, audio_data, done_frames);
  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static aaudio_data_callback_result_t
aaudio_input_data_cb(AAudioStream * astream, void * user_data,
                     void * audio_data, int32_t num_frames)
{
  cubeb_stream * stm = (cubeb_stream *)user_data;
  assert(stm->istream == astream);
  assert(!stm->ostream);
  assert(num_frames >= 0);

  stream_state state = stm->state.load();
  // int istate = WRAP(AAudioStream_getState)(stm->istream);
  // ALOGV("aaudio input data cb on stream %p: state %ld (%d), num_frames: %ld",
  //     (void*) stm, state, istate, num_frames);

  // all other states may happen since the callback might be called
  // from within requestStart
  assert(state != stream_state::SHUTDOWN);

  // This might happen when we started draining but not yet actually
  // STOPPED the stream from the state thread.
  if (state == stream_state::DRAINING) {
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }

  long input_frame_count = num_frames;
  long done_frames = cubeb_resampler_fill(stm->resampler, audio_data,
                                          &input_frame_count, NULL, 0);
  if (done_frames < 0 || done_frames > num_frames) {
    LOG("Error in data callback or resampler: %ld", done_frames);
    stm->state.store(stream_state::ERROR);
    return AAUDIO_CALLBACK_RESULT_STOP;
  } else if (done_frames < input_frame_count) {
    // we don't really drain an input stream, just have to
    // stop it from the state thread. That is signaled via the
    // DRAINING state.
    stm->state.store(stream_state::DRAINING);
    stm->context->state.waiting.store(true);
    stm->context->state.cond.notify_one();
  }

  return AAUDIO_CALLBACK_RESULT_CONTINUE;
}

static void
aaudio_error_cb(AAudioStream * astream, void * user_data, aaudio_result_t error)
{
  cubeb_stream * stm = static_cast<cubeb_stream *>(user_data);
  assert(stm->ostream == astream || stm->istream == astream);
  LOG("AAudio error callback: %s", WRAP(AAudio_convertResultToText)(error));
  stm->state.store(stream_state::ERROR);
}

static int
realize_stream(AAudioStreamBuilder * sb, const cubeb_stream_params * params,
               AAudioStream ** stream, unsigned * frame_size)
{
  aaudio_result_t res;
  assert(params->rate);
  assert(params->channels);

  WRAP(AAudioStreamBuilder_setSampleRate)(sb, params->rate);
  WRAP(AAudioStreamBuilder_setChannelCount)(sb, params->channels);

  aaudio_format_t fmt;
  switch (params->format) {
  case CUBEB_SAMPLE_S16NE:
    fmt = AAUDIO_FORMAT_PCM_I16;
    *frame_size = sizeof(int16_t) * params->channels;
    break;
  case CUBEB_SAMPLE_FLOAT32NE:
    fmt = AAUDIO_FORMAT_PCM_FLOAT;
    *frame_size = sizeof(float) * params->channels;
    break;
  default:
    return CUBEB_ERROR_INVALID_FORMAT;
  }

  WRAP(AAudioStreamBuilder_setFormat)(sb, fmt);
  res = WRAP(AAudioStreamBuilder_openStream)(sb, stream);
  if (res == AAUDIO_ERROR_INVALID_FORMAT) {
    LOG("AAudio device doesn't support output format %d", fmt);
    return CUBEB_ERROR_INVALID_FORMAT;
  } else if (params->rate && res == AAUDIO_ERROR_INVALID_RATE) {
    // The requested rate is not supported.
    // Just try again with default rate, we create a resampler anyways
    WRAP(AAudioStreamBuilder_setSampleRate)(sb, AAUDIO_UNSPECIFIED);
    res = WRAP(AAudioStreamBuilder_openStream)(sb, stream);
    LOG("Requested rate of %u is not supported, inserting resampler",
        params->rate);
  }

  // When the app has no permission to record audio
  // (android.permission.RECORD_AUDIO) but requested and input stream, this will
  // return INVALID_ARGUMENT.
  if (res != AAUDIO_OK) {
    LOG("AAudioStreamBuilder_openStream: %s",
        WRAP(AAudio_convertResultToText)(res));
    return CUBEB_ERROR;
  }

  return CUBEB_OK;
}

static void
aaudio_stream_destroy(cubeb_stream * stm)
{
  lock_guard lock(stm->mutex);
  assert(stm->state == stream_state::STOPPED ||
         stm->state == stream_state::STOPPING ||
         stm->state == stream_state::INIT ||
         stm->state == stream_state::DRAINING ||
         stm->state == stream_state::ERROR ||
         stm->state == stream_state::SHUTDOWN);

  aaudio_result_t res;

  // No callbacks are triggered anymore when requestStop returns.
  // That is important as we otherwise might read from a closed istream
  // for a duplex stream.
  if (stm->ostream) {
    if (stm->state != stream_state::STOPPED &&
        stm->state != stream_state::STOPPING &&
        stm->state != stream_state::SHUTDOWN) {
      res = WRAP(AAudioStream_requestStop)(stm->ostream);
      if (res != AAUDIO_OK) {
        LOG("AAudioStreamBuilder_requestStop: %s",
            WRAP(AAudio_convertResultToText)(res));
      }
    }

    WRAP(AAudioStream_close)(stm->ostream);
    stm->ostream = NULL;
  }

  if (stm->istream) {
    if (stm->state != stream_state::STOPPED &&
        stm->state != stream_state::STOPPING &&
        stm->state != stream_state::SHUTDOWN) {
      res = WRAP(AAudioStream_requestStop)(stm->istream);
      if (res != AAUDIO_OK) {
        LOG("AAudioStreamBuilder_requestStop: %s",
            WRAP(AAudio_convertResultToText)(res));
      }
    }

    WRAP(AAudioStream_close)(stm->istream);
    stm->istream = NULL;
  }

  if (stm->resampler) {
    cubeb_resampler_destroy(stm->resampler);
    stm->resampler = NULL;
  }

  stm->in_buf = {};
  stm->in_frame_size = {};
  stm->out_format = {};
  stm->out_channels = {};
  stm->out_frame_size = {};

  stm->state.store(stream_state::INIT);
  stm->in_use.store(false);
}

static int
aaudio_stream_init_impl(cubeb_stream * stm, cubeb_devid input_device,
                        cubeb_stream_params * input_stream_params,
                        cubeb_devid output_device,
                        cubeb_stream_params * output_stream_params,
                        unsigned int latency_frames)
{
  assert(stm->state.load() == stream_state::INIT);
  stm->in_use.store(true);

  aaudio_result_t res;
  AAudioStreamBuilder * sb;
  res = WRAP(AAudio_createStreamBuilder)(&sb);
  if (res != AAUDIO_OK) {
    LOG("AAudio_createStreamBuilder: %s",
        WRAP(AAudio_convertResultToText)(res));
    return CUBEB_ERROR;
  }

  // make sure the builder is always destroyed
  struct StreamBuilderDestructor {
    void operator()(AAudioStreamBuilder * sb)
    {
      WRAP(AAudioStreamBuilder_delete)(sb);
    }
  };

  std::unique_ptr<AAudioStreamBuilder, StreamBuilderDestructor> sbPtr(sb);

  WRAP(AAudioStreamBuilder_setErrorCallback)(sb, aaudio_error_cb, stm);
  WRAP(AAudioStreamBuilder_setBufferCapacityInFrames)(sb, latency_frames);

  AAudioStream_dataCallback in_data_callback{};
  AAudioStream_dataCallback out_data_callback{};
  if (output_stream_params && input_stream_params) {
    out_data_callback = aaudio_duplex_data_cb;
    in_data_callback = NULL;
  } else if (input_stream_params) {
    in_data_callback = aaudio_input_data_cb;
  } else if (output_stream_params) {
    out_data_callback = aaudio_output_data_cb;
  } else {
    LOG("Tried to open stream without input or output parameters");
    return CUBEB_ERROR;
  }

#ifdef CUBEB_AAUDIO_EXCLUSIVE_STREAM
  LOG("AAudio setting exclusive share mode for stream");
  WRAP(AAudioStreamBuilder_setSharingMode)(sb, AAUDIO_SHARING_MODE_EXCLUSIVE);
#endif

  if (latency_frames <= POWERSAVE_LATENCY_FRAMES_THRESHOLD) {
    LOG("AAudio setting low latency mode for stream");
    WRAP(AAudioStreamBuilder_setPerformanceMode)
    (sb, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);
  } else {
    LOG("AAudio setting power saving mode for stream");
    WRAP(AAudioStreamBuilder_setPerformanceMode)
    (sb, AAUDIO_PERFORMANCE_MODE_POWER_SAVING);
  }

  unsigned frame_size;

  // initialize streams
  // output
  uint32_t target_sample_rate = 0;
  cubeb_stream_params out_params;
  if (output_stream_params) {
    int output_preset = stm->voice_output ? AAUDIO_USAGE_VOICE_COMMUNICATION
                                          : AAUDIO_USAGE_MEDIA;
    WRAP(AAudioStreamBuilder_setUsage)(sb, output_preset);
    WRAP(AAudioStreamBuilder_setDirection)(sb, AAUDIO_DIRECTION_OUTPUT);
    WRAP(AAudioStreamBuilder_setDataCallback)(sb, out_data_callback, stm);
    int res_err =
        realize_stream(sb, output_stream_params, &stm->ostream, &frame_size);
    if (res_err) {
      return res_err;
    }

    // output debug information
    aaudio_sharing_mode_t sm = WRAP(AAudioStream_getSharingMode)(stm->ostream);
    aaudio_performance_mode_t pm =
        WRAP(AAudioStream_getPerformanceMode)(stm->ostream);
    int bcap = WRAP(AAudioStream_getBufferCapacityInFrames)(stm->ostream);
    int bsize = WRAP(AAudioStream_getBufferSizeInFrames)(stm->ostream);
    int rate = WRAP(AAudioStream_getSampleRate)(stm->ostream);
    LOG("AAudio output stream sharing mode: %d", sm);
    LOG("AAudio output stream performance mode: %d", pm);
    LOG("AAudio output stream buffer capacity: %d", bcap);
    LOG("AAudio output stream buffer size: %d", bsize);
    LOG("AAudio output stream buffer rate: %d", rate);

    target_sample_rate = output_stream_params->rate;
    out_params = *output_stream_params;
    out_params.rate = rate;

    stm->out_channels = output_stream_params->channels;
    stm->out_format = output_stream_params->format;
    stm->out_frame_size = frame_size;
    stm->volume.store(1.f);
  }

  // input
  cubeb_stream_params in_params;
  if (input_stream_params) {
    // Match what the OpenSL backend does for now, we could use UNPROCESSED and
    // VOICE_COMMUNICATION here, but we'd need to make it clear that
    // application-level AEC and other voice processing should be disabled
    // there.
    int input_preset = stm->voice_input ? AAUDIO_INPUT_PRESET_VOICE_RECOGNITION
                                        : AAUDIO_INPUT_PRESET_CAMCORDER;
    WRAP(AAudioStreamBuilder_setInputPreset)(sb, input_preset);
    WRAP(AAudioStreamBuilder_setDirection)(sb, AAUDIO_DIRECTION_INPUT);
    WRAP(AAudioStreamBuilder_setDataCallback)(sb, in_data_callback, stm);
    int res_err =
        realize_stream(sb, input_stream_params, &stm->istream, &frame_size);
    if (res_err) {
      return res_err;
    }

    // output debug information
    aaudio_sharing_mode_t sm = WRAP(AAudioStream_getSharingMode)(stm->istream);
    aaudio_performance_mode_t pm =
        WRAP(AAudioStream_getPerformanceMode)(stm->istream);
    int bcap = WRAP(AAudioStream_getBufferCapacityInFrames)(stm->istream);
    int bsize = WRAP(AAudioStream_getBufferSizeInFrames)(stm->istream);
    int rate = WRAP(AAudioStream_getSampleRate)(stm->istream);
    LOG("AAudio input stream sharing mode: %d", sm);
    LOG("AAudio input stream performance mode: %d", pm);
    LOG("AAudio input stream buffer capacity: %d", bcap);
    LOG("AAudio input stream buffer size: %d", bsize);
    LOG("AAudio input stream buffer rate: %d", rate);

    stm->in_buf.reset(new char[bcap * frame_size]());
    assert(!target_sample_rate ||
           target_sample_rate == input_stream_params->rate);

    target_sample_rate = input_stream_params->rate;
    in_params = *input_stream_params;
    in_params.rate = rate;
    stm->in_frame_size = frame_size;
  }

  // initialize resampler
  stm->resampler = cubeb_resampler_create(
      stm, input_stream_params ? &in_params : NULL,
      output_stream_params ? &out_params : NULL, target_sample_rate,
      stm->data_callback, stm->user_ptr, CUBEB_RESAMPLER_QUALITY_DEFAULT,
      CUBEB_RESAMPLER_RECLOCK_NONE);

  if (!stm->resampler) {
    LOG("Failed to create resampler");
    return CUBEB_ERROR;
  }

  // the stream isn't started initially. We don't need to differentiate
  // between a stream that was just initialized and one that played
  // already but was stopped.
  stm->state.store(stream_state::STOPPED);
  LOG("Cubeb stream (%p) INIT success", (void *)stm);
  return CUBEB_OK;
}

static int
aaudio_stream_init(cubeb * ctx, cubeb_stream ** stream,
                   char const * /* stream_name */, cubeb_devid input_device,
                   cubeb_stream_params * input_stream_params,
                   cubeb_devid output_device,
                   cubeb_stream_params * output_stream_params,
                   unsigned int latency_frames,
                   cubeb_data_callback data_callback,
                   cubeb_state_callback state_callback, void * user_ptr)
{
  assert(!input_device);
  assert(!output_device);

  // atomically find a free stream.
  cubeb_stream * stm = NULL;
  unique_lock lock;
  for (unsigned i = 0u; i < MAX_STREAMS; ++i) {
    // This check is only an optimization, we don't strictly need it
    // since we check again after locking the mutex.
    if (ctx->streams[i].in_use.load()) {
      continue;
    }

    // if this fails, another thread initialized this stream
    // between our check of in_use and this.
    lock = unique_lock(ctx->streams[i].mutex, std::try_to_lock);
    if (!lock.owns_lock()) {
      continue;
    }

    if (ctx->streams[i].in_use.load()) {
      lock = {};
      continue;
    }

    stm = &ctx->streams[i];
    break;
  }

  if (!stm) {
    LOG("Error: maximum number of streams reached");
    return CUBEB_ERROR;
  }

  stm->context = ctx;
  stm->user_ptr = user_ptr;
  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->voice_input = input_stream_params &&
                     !!(input_stream_params->prefs & CUBEB_STREAM_PREF_VOICE);
  stm->voice_output = output_stream_params &&
                      !!(output_stream_params->prefs & CUBEB_STREAM_PREF_VOICE);
  stm->previous_clock = 0;

  LOG("cubeb stream prefs: voice_input: %s voice_output: %s",
      stm->voice_input ? "true" : "false",
      stm->voice_output ? "true" : "false");

  int err = aaudio_stream_init_impl(stm, input_device, input_stream_params,
                                    output_device, output_stream_params,
                                    latency_frames);
  if (err != CUBEB_OK) {
    // This is needed since aaudio_stream_destroy will lock the mutex again.
    // It's no problem that there is a gap in between as the stream isn't
    // actually in u se.
    lock.unlock();
    aaudio_stream_destroy(stm);
    return err;
  }

  *stream = stm;
  return CUBEB_OK;
}

static int
aaudio_stream_start(cubeb_stream * stm)
{
  assert(stm && stm->in_use.load());
  lock_guard lock(stm->mutex);

  stream_state state = stm->state.load();
  int istate = stm->istream ? WRAP(AAudioStream_getState)(stm->istream) : 0;
  int ostate = stm->ostream ? WRAP(AAudioStream_getState)(stm->ostream) : 0;
  LOGV("STARTING stream %p: %d (%d %d)", (void *)stm, state, istate, ostate);

  switch (state) {
  case stream_state::STARTED:
  case stream_state::STARTING:
    LOG("cubeb stream %p already STARTING/STARTED", (void *)stm);
    return CUBEB_OK;
  case stream_state::ERROR:
  case stream_state::SHUTDOWN:
    return CUBEB_ERROR;
  case stream_state::INIT:
    assert(false && "Invalid stream");
    return CUBEB_ERROR;
  case stream_state::STOPPED:
  case stream_state::STOPPING:
  case stream_state::DRAINING:
    break;
  }

  aaudio_result_t res;

  // Important to start istream before ostream.
  // As soon as we start ostream, the callbacks might be triggered an we
  // might read from istream (on duplex). If istream wasn't started yet
  // this is a problem.
  if (stm->istream) {
    res = WRAP(AAudioStream_requestStart)(stm->istream);
    if (res != AAUDIO_OK) {
      LOG("AAudioStream_requestStart (istream): %s",
          WRAP(AAudio_convertResultToText)(res));
      stm->state.store(stream_state::ERROR);
      return CUBEB_ERROR;
    }
  }

  if (stm->ostream) {
    res = WRAP(AAudioStream_requestStart)(stm->ostream);
    if (res != AAUDIO_OK) {
      LOG("AAudioStream_requestStart (ostream): %s",
          WRAP(AAudio_convertResultToText)(res));
      stm->state.store(stream_state::ERROR);
      return CUBEB_ERROR;
    }
  }

  int ret = CUBEB_OK;
  bool success;

  while (!(success = stm->state.compare_exchange_strong(
               state, stream_state::STARTING))) {
    // we land here only if the state has changed in the meantime
    switch (state) {
    // If an error ocurred in the meantime, we can't change that.
    // The stream will be stopped when shut down.
    case stream_state::ERROR:
      ret = CUBEB_ERROR;
      break;
    // The only situation in which the state could have switched to draining
    // is if the callback was already fired and requested draining. Don't
    // overwrite that. It's not an error either though.
    case stream_state::DRAINING:
      break;

    // If the state switched [DRAINING -> STOPPING] or [DRAINING/STOPPING ->
    // STOPPED] in the meantime, we can simply overwrite that since we restarted
    // the stream.
    case stream_state::STOPPING:
    case stream_state::STOPPED:
      continue;

    // There is no situation in which the state could have been valid before
    // but now in shutdown mode, since we hold the streams mutex.
    // There is also no way that it switched *into* STARTING or
    // STARTED mode.
    default:
      assert(false && "Invalid state change");
      ret = CUBEB_ERROR;
      break;
    }

    break;
  }

  if (success) {
    stm->context->state.waiting.store(true);
    stm->context->state.cond.notify_one();
  }

  return ret;
}

static int
aaudio_stream_stop(cubeb_stream * stm)
{
  assert(stm && stm->in_use.load());
  lock_guard lock(stm->mutex);

  stream_state state = stm->state.load();
  int istate = stm->istream ? WRAP(AAudioStream_getState)(stm->istream) : 0;
  int ostate = stm->ostream ? WRAP(AAudioStream_getState)(stm->ostream) : 0;
  LOGV("STOPPING stream %p: %d (%d %d)", (void *)stm, state, istate, ostate);

  switch (state) {
  case stream_state::STOPPED:
  case stream_state::STOPPING:
  case stream_state::DRAINING:
    LOG("cubeb stream %p already STOPPING/STOPPED", (void *)stm);
    return CUBEB_OK;
  case stream_state::ERROR:
  case stream_state::SHUTDOWN:
    return CUBEB_ERROR;
  case stream_state::INIT:
    assert(false && "Invalid stream");
    return CUBEB_ERROR;
  case stream_state::STARTED:
  case stream_state::STARTING:
    break;
  }

  aaudio_result_t res;

  // No callbacks are triggered anymore when requestStop returns.
  // That is important as we otherwise might read from a closed istream
  // for a duplex stream.
  // Therefor it is important to close ostream first.
  if (stm->ostream) {
    // Could use pause + flush here as well, the public cubeb interface
    // doesn't state behavior.
    res = WRAP(AAudioStream_requestStop)(stm->ostream);
    if (res != AAUDIO_OK) {
      LOG("AAudioStream_requestStop (ostream): %s",
          WRAP(AAudio_convertResultToText)(res));
      stm->state.store(stream_state::ERROR);
      return CUBEB_ERROR;
    }
  }

  if (stm->istream) {
    res = WRAP(AAudioStream_requestStop)(stm->istream);
    if (res != AAUDIO_OK) {
      LOG("AAudioStream_requestStop (istream): %s",
          WRAP(AAudio_convertResultToText)(res));
      stm->state.store(stream_state::ERROR);
      return CUBEB_ERROR;
    }
  }

  int ret = CUBEB_OK;
  bool success;
  while (!(success = atomic_compare_exchange_strong(&stm->state, &state,
                                                    stream_state::STOPPING))) {
    // we land here only if the state has changed in the meantime
    switch (state) {
    // If an error ocurred in the meantime, we can't change that.
    // The stream will be STOPPED when shut down.
    case stream_state::ERROR:
      ret = CUBEB_ERROR;
      break;
    // If it was switched to DRAINING in the meantime, it was or
    // will be STOPPED soon anyways. We don't interfere with
    // the DRAINING process, no matter in which state.
    // Not an error
    case stream_state::DRAINING:
    case stream_state::STOPPING:
    case stream_state::STOPPED:
      break;

    // If the state switched from STARTING to STARTED in the meantime
    // we can simply overwrite that since we just STOPPED it.
    case stream_state::STARTED:
      continue;

    // There is no situation in which the state could have been valid before
    // but now in shutdown mode, since we hold the streams mutex.
    // There is also no way that it switched *into* STARTING mode.
    default:
      assert(false && "Invalid state change");
      ret = CUBEB_ERROR;
      break;
    }

    break;
  }

  if (success) {
    stm->context->state.waiting.store(true);
    stm->context->state.cond.notify_one();
  }

  return ret;
}

static int
aaudio_stream_get_position(cubeb_stream * stm, uint64_t * position)
{
  assert(stm && stm->in_use.load());
  lock_guard lock(stm->mutex);

  stream_state state = stm->state.load();
  AAudioStream * stream = stm->ostream ? stm->ostream : stm->istream;
  switch (state) {
  case stream_state::ERROR:
  case stream_state::SHUTDOWN:
    return CUBEB_ERROR;
  case stream_state::DRAINING:
  case stream_state::STOPPED:
  case stream_state::STOPPING:
    // getTimestamp is only valid when the stream is playing.
    // Simply return the number of frames passed to aaudio
    *position = WRAP(AAudioStream_getFramesRead)(stream);
    if (*position < stm->previous_clock) {
      *position = stm->previous_clock;
    } else {
      stm->previous_clock = *position;
    }
    return CUBEB_OK;
  case stream_state::INIT:
    assert(false && "Invalid stream");
    return CUBEB_ERROR;
  case stream_state::STARTED:
  case stream_state::STARTING:
    break;
  }

  int64_t pos;
  int64_t ns;
  aaudio_result_t res;
  res = WRAP(AAudioStream_getTimestamp)(stream, CLOCK_MONOTONIC, &pos, &ns);
  if (res != AAUDIO_OK) {
    // When the audio stream is not running, invalid_state is returned and we
    // simply fall back to the method we use for non-playing streams.
    if (res == AAUDIO_ERROR_INVALID_STATE) {
      *position = WRAP(AAudioStream_getFramesRead)(stream);
      if (*position < stm->previous_clock) {
        *position = stm->previous_clock;
      } else {
        stm->previous_clock = *position;
      }
      return CUBEB_OK;
    }

    LOG("AAudioStream_getTimestamp: %s", WRAP(AAudio_convertResultToText)(res));
    return CUBEB_ERROR;
  }

  *position = pos;
  if (*position < stm->previous_clock) {
    *position = stm->previous_clock;
  } else {
    stm->previous_clock = *position;
  }
  return CUBEB_OK;
}

static int
aaudio_stream_get_latency(cubeb_stream * stm, uint32_t * latency)
{
  int64_t pos;
  int64_t ns;
  aaudio_result_t res;

  if (!stm->ostream) {
    LOG("error: aaudio_stream_get_latency on input-only stream");
    return CUBEB_ERROR;
  }

  res =
      WRAP(AAudioStream_getTimestamp)(stm->ostream, CLOCK_MONOTONIC, &pos, &ns);
  if (res != AAUDIO_OK) {
    LOG("aaudio_stream_get_latency, AAudioStream_getTimestamp: %s, returning "
        "memoized value",
        WRAP(AAudio_convertResultToText)(res));
    // Expected when the stream is paused.
    *latency = stm->latest_output_latency;
    return CUBEB_OK;
  }

  int64_t read = WRAP(AAudioStream_getFramesRead)(stm->ostream);

  *latency = stm->latest_output_latency = read - pos;
  LOG("aaudio_stream_get_latency, %u", *latency);

  return CUBEB_OK;
}

static int
aaudio_stream_get_input_latency(cubeb_stream * stm, uint32_t * latency)
{
  int64_t pos;
  int64_t ns;
  aaudio_result_t res;

  if (!stm->istream) {
    LOG("error: aaudio_stream_get_input_latency on an ouput-only stream");
    return CUBEB_ERROR;
  }

  res =
      WRAP(AAudioStream_getTimestamp)(stm->istream, CLOCK_MONOTONIC, &pos, &ns);
  if (res != AAUDIO_OK) {
    // Expected when the stream is paused.
    LOG("aaudio_stream_get_input_latency, AAudioStream_getTimestamp: %s, "
        "returning memoized value",
        WRAP(AAudio_convertResultToText)(res));
    *latency = stm->latest_input_latency;
    return CUBEB_OK;
  }

  int64_t written = WRAP(AAudioStream_getFramesWritten)(stm->istream);

  *latency = stm->latest_input_latency = written - pos;
  LOG("aaudio_stream_get_input_latency, %u", *latency);

  return CUBEB_OK;
}

static int
aaudio_stream_set_volume(cubeb_stream * stm, float volume)
{
  assert(stm && stm->in_use.load() && stm->ostream);
  stm->volume.store(volume);
  return CUBEB_OK;
}

aaudio_data_callback_result_t
dummy_callback(AAudioStream * stream, void * userData, void * audioData,
               int32_t numFrames)
{
  return AAUDIO_CALLBACK_RESULT_STOP;
}

// Returns a dummy stream with all default settings
static AAudioStream *
init_dummy_stream()
{
  AAudioStreamBuilder * streamBuilder;
  aaudio_result_t res;
  res = WRAP(AAudio_createStreamBuilder)(&streamBuilder);
  if (res != AAUDIO_OK) {
    LOG("init_dummy_stream: AAudio_createStreamBuilder: %s",
        WRAP(AAudio_convertResultToText)(res));
    return nullptr;
  }
  WRAP(AAudioStreamBuilder_setDataCallback)
  (streamBuilder, dummy_callback, nullptr);
  WRAP(AAudioStreamBuilder_setPerformanceMode)
  (streamBuilder, AAUDIO_PERFORMANCE_MODE_LOW_LATENCY);

  AAudioStream * stream;
  res = WRAP(AAudioStreamBuilder_openStream)(streamBuilder, &stream);
  if (res != AAUDIO_OK) {
    LOG("init_dummy_stream: AAudioStreamBuilder_openStream %s",
        WRAP(AAudio_convertResultToText)(res));
    return nullptr;
  }
  WRAP(AAudioStreamBuilder_delete)(streamBuilder);

  return stream;
}

static void
destroy_dummy_stream(AAudioStream * stream)
{
  WRAP(AAudioStream_close)(stream);
}

static int
aaudio_get_min_latency(cubeb * ctx, cubeb_stream_params params,
                       uint32_t * latency_frames)
{
  AAudioStream * stream = init_dummy_stream();

  if (!stream) {
    return CUBEB_ERROR;
  }

  // https://android.googlesource.com/platform/compatibility/cdd/+/refs/heads/master/5_multimedia/5_6_audio-latency.md
  *latency_frames = WRAP(AAudioStream_getFramesPerBurst)(stream);

  LOG("aaudio_get_min_latency: %u frames", *latency_frames);

  destroy_dummy_stream(stream);

  return CUBEB_OK;
}

int
aaudio_get_preferred_sample_rate(cubeb * ctx, uint32_t * rate)
{
  AAudioStream * stream = init_dummy_stream();

  if (!stream) {
    return CUBEB_ERROR;
  }

  *rate = WRAP(AAudioStream_getSampleRate)(stream);

  LOG("aaudio_get_preferred_sample_rate %uHz", *rate);

  destroy_dummy_stream(stream);

  return CUBEB_OK;
}

extern "C" int
aaudio_init(cubeb ** context, char const * context_name);

const static struct cubeb_ops aaudio_ops = {
    /*.init =*/aaudio_init,
    /*.get_backend_id =*/aaudio_get_backend_id,
    /*.get_max_channel_count =*/aaudio_get_max_channel_count,
    /* .get_min_latency =*/aaudio_get_min_latency,
    /*.get_preferred_sample_rate =*/aaudio_get_preferred_sample_rate,
    /*.enumerate_devices =*/NULL,
    /*.device_collection_destroy =*/NULL,
    /*.destroy =*/aaudio_destroy,
    /*.stream_init =*/aaudio_stream_init,
    /*.stream_destroy =*/aaudio_stream_destroy,
    /*.stream_start =*/aaudio_stream_start,
    /*.stream_stop =*/aaudio_stream_stop,
    /*.stream_get_position =*/aaudio_stream_get_position,
    /*.stream_get_latency =*/aaudio_stream_get_latency,
    /*.stream_get_input_latency =*/aaudio_stream_get_input_latency,
    /*.stream_set_volume =*/aaudio_stream_set_volume,
    /*.stream_set_name =*/NULL,
    /*.stream_get_current_device =*/NULL,
    /*.stream_device_destroy =*/NULL,
    /*.stream_register_device_changed_callback =*/NULL,
    /*.register_device_collection_changed =*/NULL};

extern "C" /*static*/ int
aaudio_init(cubeb ** context, char const * /* context_name */)
{
  // load api
  void * libaaudio = NULL;
#ifndef DISABLE_LIBAAUDIO_DLOPEN
  libaaudio = dlopen("libaaudio.so", RTLD_NOW);
  if (!libaaudio) {
    return CUBEB_ERROR;
  }

#define LOAD(x)                                                                \
  {                                                                            \
    cubeb_##x = (decltype(x) *)(dlsym(libaaudio, #x));                         \
    if (!WRAP(x)) {                                                            \
      LOG("AAudio: Failed to load %s", #x);                                    \
      dlclose(libaaudio);                                                      \
      return CUBEB_ERROR;                                                      \
    }                                                                          \
  }

  LIBAAUDIO_API_VISIT(LOAD);
#undef LOAD
#endif

  cubeb * ctx = new cubeb;
  ctx->ops = &aaudio_ops;
  ctx->libaaudio = libaaudio;

  ctx->state.thread = std::thread(state_thread, ctx);

  // NOTE: using platform-specific APIs we could set the priority of the
  // notifier thread lower than the priority of the state thread.
  // This way, it's more likely that the state thread will be woken up
  // by the condition variable signal when both are currently waiting
  ctx->state.notifier = std::thread(notifier_thread, ctx);

  *context = ctx;
  return CUBEB_OK;
}
