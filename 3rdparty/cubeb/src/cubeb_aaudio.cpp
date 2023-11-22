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
#include "cubeb_triple_buffer.h"
#include <aaudio/AAudio.h>
#include <atomic>
#include <cassert>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <cstring>
#include <dlfcn.h>
#include <inttypes.h>
#include <limits>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

using namespace std;

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
  X(AAudioStreamBuilder_setUsage)                                              \
  X(AAudioStreamBuilder_setFramesPerDataCallback)

// not needed or added later on
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
const int64_t NS_PER_S = static_cast<int64_t>(1e9);

static void
aaudio_stream_destroy(cubeb_stream * stm);
static int
aaudio_stream_start(cubeb_stream * stm);
static int
aaudio_stream_stop(cubeb_stream * stm);

static int
aaudio_stream_init_impl(cubeb_stream * stm, lock_guard<mutex> & lock);
static int
aaudio_stream_stop_locked(cubeb_stream * stm, lock_guard<mutex> & lock);
static void
aaudio_stream_destroy_locked(cubeb_stream * stm, lock_guard<mutex> & lock);
static int
aaudio_stream_start_locked(cubeb_stream * stm, lock_guard<mutex> & lock);

static void
reinitialize_stream(cubeb_stream * stm);

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

struct AAudioTimingInfo {
  // The timestamp at which the audio engine last called the calback.
  uint64_t tstamp;
  // The number of output frames sent to the engine.
  uint64_t output_frame_index;
  // The current output latency in frames. 0 if there is no output stream.
  uint32_t output_latency;
  // The current input latency in frames. 0 if there is no input stream.
  uint32_t input_latency;
};

struct cubeb_stream {
  /* Note: Must match cubeb_stream layout in cubeb.c. */
  cubeb * context{};
  void * user_ptr{};

  std::atomic<bool> in_use{false};
  std::atomic<bool> latency_metrics_available{false};
  std::atomic<stream_state> state{stream_state::INIT};
  std::atomic<bool> in_data_callback{false};
  triple_buffer<AAudioTimingInfo> timing_info;

  AAudioStream * ostream{};
  AAudioStream * istream{};
  cubeb_data_callback data_callback{};
  cubeb_state_callback state_callback{};
  cubeb_resampler * resampler{};

  // mutex synchronizes access to the stream from the state thread
  // and user-called functions. Everything that is accessed in the
  // aaudio data (or error) callback is synchronized only via atomics.
  // This lock is acquired for the entirety of the reinitialization period, when
  // changing device.
  std::mutex mutex;

  std::vector<uint8_t> in_buf;
  unsigned in_frame_size{}; // size of one input frame

  unique_ptr<cubeb_stream_params> output_stream_params;
  unique_ptr<cubeb_stream_params> input_stream_params;
  uint32_t latency_frames{};
  cubeb_sample_format out_format{};
  uint32_t sample_rate{};
  std::atomic<float> volume{1.f};
  unsigned out_channels{};
  unsigned out_frame_size{};
  bool voice_input{};
  bool voice_output{};
  uint64_t previous_clock{};
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

struct AutoInCallback {
  AutoInCallback(cubeb_stream * stm) : stm(stm)
  {
    stm->in_data_callback.store(true);
  }
  ~AutoInCallback() { stm->in_data_callback.store(false); }
  cubeb_stream * stm;
};

// Returns when aaudio_stream's state is equal to desired_state.
// poll_frequency_ns is the duration that is slept in between asking for
// state updates and getting the new state.
// When waiting for a stream to stop, it is best to pick a value similar
// to the callback time because STOPPED will happen after
// draining.
static int
wait_for_state_change(AAudioStream * aaudio_stream,
                      aaudio_stream_state_t desired_state,
                      int64_t poll_frequency_ns)
{
  aaudio_stream_state_t new_state;
  do {
    aaudio_result_t res = WRAP(AAudioStream_waitForStateChange)(
        aaudio_stream, AAUDIO_STREAM_STATE_UNKNOWN, &new_state,
        poll_frequency_ns);
    if (res != AAUDIO_OK) {
      LOG("AAudioStream_waitForStateChanged: %s",
          WRAP(AAudio_convertResultToText)(res));
      return CUBEB_ERROR;
    }
  } while (new_state != desired_state);

  LOG("wait_for_state_change: current state now: %s",
      cubeb_AAudio_convertStreamStateToText(new_state));

  return CUBEB_OK;
}

// Only allowed from state thread, while mutex on stm is locked
static void
shutdown_with_error(cubeb_stream * stm)
{
  if (stm->istream) {
    WRAP(AAudioStream_requestStop)(stm->istream);
  }
  if (stm->ostream) {
    WRAP(AAudioStream_requestStop)(stm->ostream);
  }

  int64_t poll_frequency_ns = NS_PER_S * stm->out_frame_size / stm->sample_rate;
  if (stm->istream) {
    wait_for_state_change(stm->istream, AAUDIO_STREAM_STATE_STOPPED,
                          poll_frequency_ns);
  }
  if (stm->ostream) {
    wait_for_state_change(stm->ostream, AAUDIO_STREAM_STATE_STOPPED,
                          poll_frequency_ns);
  }

  assert(!stm->in_data_callback.load());
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
      shutdown_with_error(stm);
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
      LOG("Unexpected android input stream state %s",
          WRAP(AAudio_convertStreamStateToText)(istate));
      shutdown_with_error(stm);
      return;
    }

    if (ostate == AAUDIO_STREAM_STATE_PAUSING ||
        ostate == AAUDIO_STREAM_STATE_PAUSED ||
        ostate == AAUDIO_STREAM_STATE_FLUSHING ||
        ostate == AAUDIO_STREAM_STATE_FLUSHED ||
        ostate == AAUDIO_STREAM_STATE_UNKNOWN ||
        ostate == AAUDIO_STREAM_STATE_DISCONNECTED) {
      LOG("Unexpected android output stream state %s",
          WRAP(AAudio_convertStreamStateToText)(istate));
      shutdown_with_error(stm);
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
      for (auto & stream : ctx->streams) {
        cubeb_stream * stm = &stream;
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
  for (auto & stream : ctx->streams) {
    assert(!stream.in_use.load());
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
  case CUBEB_SAMPLE_S16NE: {
    int16_t * integer_data = static_cast<int16_t *>(audio_data);
    for (uint32_t i = 0u; i < num_frames * stm->out_channels; ++i) {
      integer_data[i] =
          static_cast<int16_t>(static_cast<float>(integer_data[i]) * volume);
    }
    break;
  }
  case CUBEB_SAMPLE_FLOAT32NE:
    for (uint32_t i = 0u; i < num_frames * stm->out_channels; ++i) {
      (static_cast<float *>(audio_data))[i] *= volume;
    }
    break;
  default:
    assert(false && "Unreachable: invalid stream out_format");
  }
}

uint64_t
now_ns()
{
  using namespace std::chrono;
  return duration_cast<nanoseconds>(steady_clock::now().time_since_epoch())
      .count();
}

// To be called from the real-time audio callback
uint64_t
aaudio_get_latency(cubeb_stream * stm, aaudio_direction_t direction,
                   uint64_t tstamp_ns)
{
  bool is_output = direction == AAUDIO_DIRECTION_OUTPUT;
  int64_t hw_frame_index;
  int64_t hw_tstamp;
  AAudioStream * stream = is_output ? stm->ostream : stm->istream;
  // For an output stream (resp. input stream), get the number of frames
  // written to (resp read from) the hardware.
  int64_t app_frame_index = is_output
                                ? WRAP(AAudioStream_getFramesWritten)(stream)
                                : WRAP(AAudioStream_getFramesRead)(stream);

  assert(tstamp_ns < std::numeric_limits<uint64_t>::max());
  int64_t signed_tstamp_ns = static_cast<int64_t>(tstamp_ns);

  // Get a timestamp for a particular frame index written to or read from the
  // hardware.
  auto result = WRAP(AAudioStream_getTimestamp)(stream, CLOCK_MONOTONIC,
                                                &hw_frame_index, &hw_tstamp);
  if (result != AAUDIO_OK) {
    LOG("AAudioStream_getTimestamp failure for %s: %s",
        is_output ? "output" : "input",
        WRAP(AAudio_convertResultToText)(result));
    return 0;
  }

  // Compute the difference between the app and the hardware indices.
  int64_t frame_index_delta = app_frame_index - hw_frame_index;
  // Convert to ns
  int64_t frame_time_delta = (frame_index_delta * NS_PER_S) / stm->sample_rate;
  // Extrapolate from the known timestamp for a particular frame presented.
  int64_t app_frame_hw_time = hw_tstamp + frame_time_delta;
  // For an output stream, the latency is positive, for an input stream, it's
  // negative.
  int64_t latency_ns = is_output ? app_frame_hw_time - signed_tstamp_ns
                                 : signed_tstamp_ns - app_frame_hw_time;
  int64_t latency_frames = stm->sample_rate * latency_ns / NS_PER_S;

  LOGV("Latency in frames (%s): %d (%dms)", is_output ? "output" : "input",
       latency_frames, latency_ns / 1e6);

  return latency_frames;
}

void
compute_and_report_latency_metrics(cubeb_stream * stm)
{
  AAudioTimingInfo info = {};

  info.tstamp = now_ns();

  if (stm->ostream) {
    uint64_t latency_frames =
        aaudio_get_latency(stm, AAUDIO_DIRECTION_OUTPUT, info.tstamp);
    if (latency_frames) {
      info.output_latency = latency_frames;
      info.output_frame_index =
          WRAP(AAudioStream_getFramesWritten)(stm->ostream);
    }
  }
  if (stm->istream) {
    uint64_t latency_frames =
        aaudio_get_latency(stm, AAUDIO_DIRECTION_INPUT, info.tstamp);
    if (latency_frames) {
      info.input_latency = latency_frames;
    }
  }

  if (info.output_latency || info.input_latency) {
    stm->latency_metrics_available = true;
    stm->timing_info.write(info);
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
  AutoInCallback aic(stm);
  assert(stm->ostream == astream);
  assert(stm->istream);
  assert(num_frames >= 0);

  stream_state state = atomic_load(&stm->state);
  int istate = WRAP(AAudioStream_getState)(stm->istream);
  int ostate = WRAP(AAudioStream_getState)(stm->ostream);

  // all other states may happen since the callback might be called
  // from within requestStart
  assert(state != stream_state::SHUTDOWN);

  // This might happen when we started draining but not yet actually
  // stopped the stream from the state thread.
  if (state == stream_state::DRAINING) {
    LOG("Draining in duplex callback");
    std::memset(audio_data, 0x0, num_frames * stm->out_frame_size);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }

  if (num_frames * stm->in_frame_size > stm->in_buf.size()) {
    LOG("Resizing input buffer in duplex callback");
    stm->in_buf.resize(num_frames * stm->in_frame_size);
  }
  // The aaudio docs state that AAudioStream_read must not be called on
  // the stream associated with a callback. But we call it on the input stream
  // while this callback is for the output stream so this is ok.
  // We also pass timeout 0, giving us strong non-blocking guarantees.
  // This is exactly how it's done in the aaudio duplex example code snippet.
  long in_num_frames =
      WRAP(AAudioStream_read)(stm->istream, stm->in_buf.data(), num_frames, 0);
  if (in_num_frames < 0) { // error
    if (in_num_frames == AAUDIO_STREAM_STATE_DISCONNECTED) {
      LOG("AAudioStream_read: %s (reinitializing)",
          WRAP(AAudio_convertResultToText)(in_num_frames));
      reinitialize_stream(stm);
    } else {
      stm->state.store(stream_state::ERROR);
    }
    LOG("AAudioStream_read: %s",
        WRAP(AAudio_convertResultToText)(in_num_frames));
    return AAUDIO_CALLBACK_RESULT_STOP;
  }

  ALOGV("aaudio duplex data cb on stream %p: state %ld (in: %d, out: %d), "
        "num_frames: %ld, read: %ld",
        (void *)stm, state, istate, ostate, num_frames, in_num_frames);

  compute_and_report_latency_metrics(stm);

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
    uint8_t * buf = stm->in_buf.data() + in_num_frames * stm->in_frame_size;
    std::memset(buf, 0x0, left * stm->in_frame_size);
    in_num_frames = num_frames;
  }

  long done_frames =
      cubeb_resampler_fill(stm->resampler, stm->in_buf.data(), &in_num_frames,
                           audio_data, num_frames);

  if (done_frames < 0 || done_frames > num_frames) {
    LOG("Error in data callback or resampler: %ld", done_frames);
    stm->state.store(stream_state::ERROR);
    return AAUDIO_CALLBACK_RESULT_STOP;
  }
  if (done_frames < num_frames) {
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
  AutoInCallback aic(stm);
  assert(stm->ostream == astream);
  assert(!stm->istream);
  assert(num_frames >= 0);

  stream_state state = stm->state.load();
  int ostate = WRAP(AAudioStream_getState)(stm->ostream);
  ALOGV("aaudio output data cb on stream %p: state %ld (%d), num_frames: %ld",
        stm, state, ostate, num_frames);

  // all other states may happen since the callback might be called
  // from within requestStart
  assert(state != stream_state::SHUTDOWN);

  // This might happen when we started draining but not yet actually
  // stopped the stream from the state thread.
  if (state == stream_state::DRAINING) {
    std::memset(audio_data, 0x0, num_frames * stm->out_frame_size);
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }

  compute_and_report_latency_metrics(stm);

  long done_frames = cubeb_resampler_fill(stm->resampler, nullptr, nullptr,
                                          audio_data, num_frames);
  if (done_frames < 0 || done_frames > num_frames) {
    LOG("Error in data callback or resampler: %ld", done_frames);
    stm->state.store(stream_state::ERROR);
    return AAUDIO_CALLBACK_RESULT_STOP;
  }

  if (done_frames < num_frames) {
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
  AutoInCallback aic(stm);
  assert(stm->istream == astream);
  assert(!stm->ostream);
  assert(num_frames >= 0);

  stream_state state = stm->state.load();
  int istate = WRAP(AAudioStream_getState)(stm->istream);
  ALOGV("aaudio input data cb on stream %p: state %ld (%d), num_frames: %ld",
        stm, state, istate, num_frames);

  // all other states may happen since the callback might be called
  // from within requestStart
  assert(state != stream_state::SHUTDOWN);

  // This might happen when we started draining but not yet actually
  // STOPPED the stream from the state thread.
  if (state == stream_state::DRAINING) {
    return AAUDIO_CALLBACK_RESULT_CONTINUE;
  }

  compute_and_report_latency_metrics(stm);

  long input_frame_count = num_frames;
  long done_frames = cubeb_resampler_fill(stm->resampler, audio_data,
                                          &input_frame_count, nullptr, 0);

  if (done_frames < 0 || done_frames > num_frames) {
    LOG("Error in data callback or resampler: %ld", done_frames);
    stm->state.store(stream_state::ERROR);
    return AAUDIO_CALLBACK_RESULT_STOP;
  }

  if (done_frames < input_frame_count) {
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
reinitialize_stream(cubeb_stream * stm)
{
  // This cannot be done from within the error callback, bounce to another
  // thread.
  // In this situation, the lock is acquired for the entire duration of the
  // function, so that this reinitialization period is atomic.
  std::thread([stm] {
    lock_guard lock(stm->mutex);
    stream_state state = stm->state.load();
    bool was_playing = state == stream_state::STARTED ||
                       state == stream_state::STARTING ||
                       state == stream_state::DRAINING;
    int err = aaudio_stream_stop_locked(stm, lock);
    // error ignored.
    aaudio_stream_destroy_locked(stm, lock);
    err = aaudio_stream_init_impl(stm, lock);

    assert(stm->in_use.load());

    if (err != CUBEB_OK) {
      aaudio_stream_destroy_locked(stm, lock);
      LOG("aaudio_stream_init_impl error while reiniting: %s",
          WRAP(AAudio_convertResultToText)(err));
      stm->state.store(stream_state::ERROR);
      return;
    }

    if (was_playing) {
      err = aaudio_stream_start_locked(stm, lock);
      if (err != CUBEB_OK) {
        aaudio_stream_destroy_locked(stm, lock);
        LOG("aaudio_stream_start error while reiniting: %s",
            WRAP(AAudio_convertResultToText)(err));
        stm->state.store(stream_state::ERROR);
        return;
      }
    }
  }).detach();
}

static void
aaudio_error_cb(AAudioStream * astream, void * user_data, aaudio_result_t error)
{
  cubeb_stream * stm = static_cast<cubeb_stream *>(user_data);
  assert(stm->ostream == astream || stm->istream == astream);

  // Device change -- reinitialize on the new default device.
  if (error == AAUDIO_ERROR_DISCONNECTED) {
    LOG("Audio device change, reinitializing stream");
    reinitialize_stream(stm);
    return;
  }

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

  WRAP(AAudioStreamBuilder_setSampleRate)
  (sb, static_cast<int32_t>(params->rate));
  WRAP(AAudioStreamBuilder_setChannelCount)
  (sb, static_cast<int32_t>(params->channels));

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
  }

  if (params->rate && res == AAUDIO_ERROR_INVALID_RATE) {
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
  stm->in_use.store(false);
  aaudio_stream_destroy_locked(stm, lock);
}

static void
aaudio_stream_destroy_locked(cubeb_stream * stm, lock_guard<mutex> & lock)
{
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
    stm->ostream = nullptr;
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
    stm->istream = nullptr;
  }

  if (stm->resampler) {
    cubeb_resampler_destroy(stm->resampler);
    stm->resampler = nullptr;
  }

  stm->in_buf = {};
  stm->in_frame_size = {};
  stm->out_format = {};
  stm->out_channels = {};
  stm->out_frame_size = {};

  stm->state.store(stream_state::INIT);
}

static int
aaudio_stream_init_impl(cubeb_stream * stm, lock_guard<mutex> & lock)
{
  assert(stm->state.load() == stream_state::INIT);

  cubeb_async_log_reset_threads();

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
  WRAP(AAudioStreamBuilder_setBufferCapacityInFrames)
  (sb, static_cast<int32_t>(stm->latency_frames));

  AAudioStream_dataCallback in_data_callback{};
  AAudioStream_dataCallback out_data_callback{};
  if (stm->output_stream_params && stm->input_stream_params) {
    out_data_callback = aaudio_duplex_data_cb;
    in_data_callback = nullptr;
  } else if (stm->input_stream_params) {
    in_data_callback = aaudio_input_data_cb;
  } else if (stm->output_stream_params) {
    out_data_callback = aaudio_output_data_cb;
  } else {
    LOG("Tried to open stream without input or output parameters");
    return CUBEB_ERROR;
  }

#ifdef CUBEB_AAUDIO_EXCLUSIVE_STREAM
  LOG("AAudio setting exclusive share mode for stream");
  WRAP(AAudioStreamBuilder_setSharingMode)(sb, AAUDIO_SHARING_MODE_EXCLUSIVE);
#endif

  if (stm->latency_frames <= POWERSAVE_LATENCY_FRAMES_THRESHOLD) {
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
  cubeb_stream_params out_params;
  if (stm->output_stream_params) {
    int output_preset = stm->voice_output ? AAUDIO_USAGE_VOICE_COMMUNICATION
                                          : AAUDIO_USAGE_MEDIA;
    WRAP(AAudioStreamBuilder_setUsage)(sb, output_preset);
    WRAP(AAudioStreamBuilder_setDirection)(sb, AAUDIO_DIRECTION_OUTPUT);
    WRAP(AAudioStreamBuilder_setDataCallback)(sb, out_data_callback, stm);
    assert(stm->latency_frames < std::numeric_limits<int32_t>::max());
    LOG("Frames per callback set to %d for output", stm->latency_frames);
    WRAP(AAudioStreamBuilder_setFramesPerDataCallback)
    (sb, static_cast<int32_t>(stm->latency_frames));

    int res_err = realize_stream(sb, stm->output_stream_params.get(),
                                 &stm->ostream, &frame_size);
    if (res_err) {
      return res_err;
    }

    int32_t output_burst_size =
        WRAP(AAudioStream_getFramesPerBurst)(stm->ostream);
    LOG("AAudio output burst size: %d", output_burst_size);
    // 3 times the burst size seems to be robust.
    res = WRAP(AAudioStream_setBufferSizeInFrames)(stm->ostream,
                                                   output_burst_size * 3);
    if (res < 0) {
      LOG("AAudioStream_setBufferSizeInFrames error (ostream): %s",
          WRAP(AAudio_convertResultToText)(res));
      // Not fatal
    }

    int rate = WRAP(AAudioStream_getSampleRate)(stm->ostream);
    LOG("AAudio output stream sharing mode: %d",
        WRAP(AAudioStream_getSharingMode)(stm->ostream));
    LOG("AAudio output stream performance mode: %d",
        WRAP(AAudioStream_getPerformanceMode)(stm->ostream));
    LOG("AAudio output stream buffer capacity: %d",
        WRAP(AAudioStream_getBufferCapacityInFrames)(stm->ostream));
    LOG("AAudio output stream buffer size: %d",
        WRAP(AAudioStream_getBufferSizeInFrames)(stm->ostream));
    LOG("AAudio output stream sample-rate: %d", rate);

    stm->sample_rate = stm->output_stream_params->rate;
    out_params = *stm->output_stream_params;
    out_params.rate = rate;

    stm->out_channels = stm->output_stream_params->channels;
    stm->out_format = stm->output_stream_params->format;
    stm->out_frame_size = frame_size;
    stm->volume.store(1.f);
  }

  // input
  cubeb_stream_params in_params;
  if (stm->input_stream_params) {
    // Match what the OpenSL backend does for now, we could use UNPROCESSED and
    // VOICE_COMMUNICATION here, but we'd need to make it clear that
    // application-level AEC and other voice processing should be disabled
    // there.
    int input_preset = stm->voice_input ? AAUDIO_INPUT_PRESET_VOICE_RECOGNITION
                                        : AAUDIO_INPUT_PRESET_CAMCORDER;
    WRAP(AAudioStreamBuilder_setInputPreset)(sb, input_preset);
    WRAP(AAudioStreamBuilder_setDirection)(sb, AAUDIO_DIRECTION_INPUT);
    WRAP(AAudioStreamBuilder_setDataCallback)(sb, in_data_callback, stm);
    assert(stm->latency_frames < std::numeric_limits<int32_t>::max());
    LOG("Frames per callback set to %d for input", stm->latency_frames);
    WRAP(AAudioStreamBuilder_setFramesPerDataCallback)
    (sb, static_cast<int32_t>(stm->latency_frames));
    int res_err = realize_stream(sb, stm->input_stream_params.get(),
                                 &stm->istream, &frame_size);
    if (res_err) {
      return res_err;
    }

    int32_t input_burst_size =
        WRAP(AAudioStream_getFramesPerBurst)(stm->istream);
    LOG("AAudio input burst size: %d", input_burst_size);
    // 3 times the burst size seems to be robust.
    res = WRAP(AAudioStream_setBufferSizeInFrames)(stm->istream,
                                                   input_burst_size * 3);
    if (res < AAUDIO_OK) {
      LOG("AAudioStream_setBufferSizeInFrames error (istream): %s",
          WRAP(AAudio_convertResultToText)(res));
      // Not fatal
    }

    int bcap = WRAP(AAudioStream_getBufferCapacityInFrames)(stm->istream);
    int rate = WRAP(AAudioStream_getSampleRate)(stm->istream);
    LOG("AAudio input stream sharing mode: %d",
        WRAP(AAudioStream_getSharingMode)(stm->istream));
    LOG("AAudio input stream performance mode: %d",
        WRAP(AAudioStream_getPerformanceMode)(stm->istream));
    LOG("AAudio input stream buffer capacity: %d", bcap);
    LOG("AAudio input stream buffer size: %d",
        WRAP(AAudioStream_getBufferSizeInFrames)(stm->istream));
    LOG("AAudio input stream buffer rate: %d", rate);

    stm->in_buf.resize(bcap * frame_size);
    assert(!stm->sample_rate ||
           stm->sample_rate == stm->input_stream_params->rate);

    stm->sample_rate = stm->input_stream_params->rate;
    in_params = *stm->input_stream_params;
    in_params.rate = rate;
    stm->in_frame_size = frame_size;
  }

  // initialize resampler
  stm->resampler = cubeb_resampler_create(
      stm, stm->input_stream_params ? &in_params : nullptr,
      stm->output_stream_params ? &out_params : nullptr, stm->sample_rate,
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
  cubeb_stream * stm = nullptr;
  unique_lock<mutex> lock;
  for (auto & stream : ctx->streams) {
    // This check is only an optimization, we don't strictly need it
    // since we check again after locking the mutex.
    if (stream.in_use.load()) {
      continue;
    }

    // if this fails, another thread initialized this stream
    // between our check of in_use and this.
    lock = unique_lock(stream.mutex, std::try_to_lock);
    if (!lock.owns_lock()) {
      continue;
    }

    if (stream.in_use.load()) {
      lock = {};
      continue;
    }

    stm = &stream;
    break;
  }

  if (!stm) {
    LOG("Error: maximum number of streams reached");
    return CUBEB_ERROR;
  }

  stm->in_use.store(true);
  stm->context = ctx;
  stm->user_ptr = user_ptr;
  stm->data_callback = data_callback;
  stm->state_callback = state_callback;
  stm->voice_input = input_stream_params &&
                     !!(input_stream_params->prefs & CUBEB_STREAM_PREF_VOICE);
  stm->voice_output = output_stream_params &&
                      !!(output_stream_params->prefs & CUBEB_STREAM_PREF_VOICE);
  stm->previous_clock = 0;
  stm->latency_frames = latency_frames;
  if (output_stream_params) {
    stm->output_stream_params = std::make_unique<cubeb_stream_params>();
    *(stm->output_stream_params) = *output_stream_params;
  }
  if (input_stream_params) {
    stm->input_stream_params = std::make_unique<cubeb_stream_params>();
    *(stm->input_stream_params) = *input_stream_params;
  }

  LOG("cubeb stream prefs: voice_input: %s voice_output: %s",
      stm->voice_input ? "true" : "false",
      stm->voice_output ? "true" : "false");

  // This is ok: the thread is marked as being in use
  lock.unlock();
  int err;

  {
    lock_guard guard(stm->mutex);
    err = aaudio_stream_init_impl(stm, guard);
  }

  if (err != CUBEB_OK) {
    aaudio_stream_destroy(stm);
    return err;
  }

  *stream = stm;
  return CUBEB_OK;
}

static int
aaudio_stream_start(cubeb_stream * stm)
{
  lock_guard lock(stm->mutex);
  return aaudio_stream_start_locked(stm, lock);
}

static int
aaudio_stream_start_locked(cubeb_stream * stm, lock_guard<mutex> & lock)
{
  assert(stm && stm->in_use.load());
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
    // STOPPED] in the meantime, we can simply overwrite that since we
    // restarted the stream.
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
  return aaudio_stream_stop_locked(stm, lock);
}

static int
aaudio_stream_stop_locked(cubeb_stream * stm, lock_guard<mutex> & lock)
{
  assert(stm && stm->in_use.load());

  stream_state state = stm->state.load();
  int istate = stm->istream ? WRAP(AAudioStream_getState)(stm->istream) : 0;
  int ostate = stm->ostream ? WRAP(AAudioStream_getState)(stm->ostream) : 0;
  LOG("STOPPING stream %p: %d (%d %d)", (void *)stm, state, istate, ostate);

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

  // No callback yet, the stream hasn't really started.
  if (stm->previous_clock == 0 && !stm->timing_info.updated()) {
    LOG("Not timing info yet");
    *position = 0;
    return CUBEB_OK;
  }

  AAudioTimingInfo info = stm->timing_info.read();
  LOGV("AAudioTimingInfo idx:%lu tstamp:%lu latency:%u",
       info.output_frame_index, info.tstamp, info.output_latency);
  // Interpolate client side since the last callback.
  uint64_t interpolation =
      stm->sample_rate * (now_ns() - info.tstamp) / NS_PER_S;
  *position = info.output_frame_index + interpolation - info.output_latency;
  if (*position < stm->previous_clock) {
    *position = stm->previous_clock;
  } else {
    stm->previous_clock = *position;
  }

  LOG("aaudio_stream_get_position: %" PRIu64 " frames", *position);

  return CUBEB_OK;
}

static int
aaudio_stream_get_latency(cubeb_stream * stm, uint32_t * latency)
{
  if (!stm->ostream) {
    LOG("error: aaudio_stream_get_latency on input-only stream");
    return CUBEB_ERROR;
  }

  if (!stm->latency_metrics_available) {
    LOG("Not timing info yet (output)");
    return CUBEB_OK;
  }

  AAudioTimingInfo info = stm->timing_info.read();

  *latency = info.output_latency;
  LOG("aaudio_stream_get_latency, %u frames", *latency);

  return CUBEB_OK;
}

static int
aaudio_stream_get_input_latency(cubeb_stream * stm, uint32_t * latency)
{
  if (!stm->istream) {
    LOG("error: aaudio_stream_get_input_latency on an output-only stream");
    return CUBEB_ERROR;
  }

  if (!stm->latency_metrics_available) {
    LOG("Not timing info yet (input)");
    return CUBEB_OK;
  }

  AAudioTimingInfo info = stm->timing_info.read();

  *latency = info.input_latency;
  LOG("aaudio_stream_get_latency, %u frames", *latency);

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
    /*.enumerate_devices =*/nullptr,
    /*.device_collection_destroy =*/nullptr,
    /*.destroy =*/aaudio_destroy,
    /*.stream_init =*/aaudio_stream_init,
    /*.stream_destroy =*/aaudio_stream_destroy,
    /*.stream_start =*/aaudio_stream_start,
    /*.stream_stop =*/aaudio_stream_stop,
    /*.stream_get_position =*/aaudio_stream_get_position,
    /*.stream_get_latency =*/aaudio_stream_get_latency,
    /*.stream_get_input_latency =*/aaudio_stream_get_input_latency,
    /*.stream_set_volume =*/aaudio_stream_set_volume,
    /*.stream_set_name =*/nullptr,
    /*.stream_get_current_device =*/nullptr,
    /*.stream_device_destroy =*/nullptr,
    /*.stream_register_device_changed_callback =*/nullptr,
    /*.register_device_collection_changed =*/nullptr};

extern "C" /*static*/ int
aaudio_init(cubeb ** context, char const * /* context_name */)
{
  // load api
  void * libaaudio = nullptr;
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
