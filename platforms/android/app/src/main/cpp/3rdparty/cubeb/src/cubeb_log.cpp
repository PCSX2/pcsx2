/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */
#define NOMINMAX

#include "cubeb_log.h"
#include "cubeb_ringbuffer.h"
#include "cubeb_tracing.h"
#include <cstdarg>
#ifdef _WIN32
#include <windows.h>
#else
#include <time.h>
#endif

std::atomic<cubeb_log_level> g_cubeb_log_level;
std::atomic<cubeb_log_callback> g_cubeb_log_callback;

/** The maximum size of a log message, after having been formatted. */
const size_t CUBEB_LOG_MESSAGE_MAX_SIZE = 256;
/** The maximum number of log messages that can be queued before dropping
 * messages. */
const size_t CUBEB_LOG_MESSAGE_QUEUE_DEPTH = 40;
/** Number of milliseconds to wait before dequeuing log messages. */
const size_t CUBEB_LOG_BATCH_PRINT_INTERVAL_MS = 10;

void
cubeb_noop_log_callback(char const * /* fmt */, ...)
{
}

/**
 * This wraps an inline buffer, that represents a log message, that must be
 * null-terminated.
 * This class should not use system calls or other potentially blocking code.
 */
class cubeb_log_message {
public:
  cubeb_log_message() { *storage = '\0'; }
  cubeb_log_message(char const str[CUBEB_LOG_MESSAGE_MAX_SIZE])
  {
    size_t length = strlen(str);
    /* paranoia against malformed message */
    assert(length < CUBEB_LOG_MESSAGE_MAX_SIZE);
    if (length > CUBEB_LOG_MESSAGE_MAX_SIZE - 1) {
      return;
    }
    PodCopy(storage, str, length);
    storage[length] = '\0';
  }
  char const * get() { return storage; }

private:
  char storage[CUBEB_LOG_MESSAGE_MAX_SIZE]{};
};

/** Lock-free asynchronous logger, made so that logging from a
 *  real-time audio callback does not block the audio thread. */
class cubeb_async_logger {
public:
  /* This is thread-safe since C++11 */
  static cubeb_async_logger & get()
  {
    static cubeb_async_logger instance;
    return instance;
  }
  void push(char const str[CUBEB_LOG_MESSAGE_MAX_SIZE])
  {
    cubeb_log_message msg(str);
    auto * owned_queue = msg_queue.load();
    // Check if the queue is being deallocated. If not, grab ownership. If yes,
    // return, the message won't be logged.
    if (!owned_queue ||
        !msg_queue.compare_exchange_strong(owned_queue, nullptr)) {
      return;
    }
    owned_queue->enqueue(msg);
    // Return ownership.
    msg_queue.store(owned_queue);
  }
  void run()
  {
    assert(logging_thread.get_id() == std::thread::id());
    logging_thread = std::thread([this]() {
      CUBEB_REGISTER_THREAD("cubeb_log");
      while (!shutdown_thread) {
        cubeb_log_message msg;
        while (msg_queue_consumer.load()->dequeue(&msg, 1)) {
          cubeb_log_internal_no_format(msg.get());
        }
        std::this_thread::sleep_for(
            std::chrono::milliseconds(CUBEB_LOG_BATCH_PRINT_INTERVAL_MS));
      }
      CUBEB_UNREGISTER_THREAD();
    });
  }
  // Tell the underlying queue the producer thread has changed, so it does not
  // assert in debug. This should be called with the thread stopped.
  void reset_producer_thread()
  {
    if (msg_queue) {
      msg_queue.load()->reset_thread_ids();
    }
  }
  void start()
  {
    auto * queue =
        new lock_free_queue<cubeb_log_message>(CUBEB_LOG_MESSAGE_QUEUE_DEPTH);
    msg_queue.store(queue);
    msg_queue_consumer.store(queue);
    shutdown_thread = false;
    run();
  }
  void stop()
  {
    assert(((g_cubeb_log_callback == cubeb_noop_log_callback) ||
            !g_cubeb_log_callback) &&
           "Only call stop after logging has been disabled.");
    shutdown_thread = true;
    if (logging_thread.get_id() != std::thread::id()) {
      logging_thread.join();
      logging_thread = std::thread();
      auto * owned_queue = msg_queue.load();
      // Check if the queue is being used. If not, grab ownership. If yes,
      // try again shortly. At this point, the logging thread has been joined,
      // so nothing is going to dequeue.
      // If there is a valid pointer here, then the real-time audio thread that
      // logs won't attempt to write into the queue, and instead drop the
      // message.
      while (!msg_queue.compare_exchange_weak(owned_queue, nullptr)) {
      }
      delete owned_queue;
      msg_queue_consumer.store(nullptr);
    }
  }

private:
  cubeb_async_logger() {}
  ~cubeb_async_logger()
  {
    assert(logging_thread.get_id() == std::thread::id() &&
           (g_cubeb_log_callback == cubeb_noop_log_callback ||
            !g_cubeb_log_callback));
    if (msg_queue.load()) {
      delete msg_queue.load();
    }
  }
  /** This is quite a big data structure, but is only instantiated if the
   * asynchronous logger is used. The two pointers point to the same object, but
   * the first one can be temporarily null when a message is being enqueued. */
  std::atomic<lock_free_queue<cubeb_log_message> *> msg_queue = {nullptr};

  std::atomic<lock_free_queue<cubeb_log_message> *> msg_queue_consumer = {
      nullptr};
  std::atomic<bool> shutdown_thread = {false};
  std::thread logging_thread;
};

void
cubeb_log_internal(char const * file, uint32_t line, char const * fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  char msg[CUBEB_LOG_MESSAGE_MAX_SIZE];
  vsnprintf(msg, CUBEB_LOG_MESSAGE_MAX_SIZE, fmt, args);
  va_end(args);
  g_cubeb_log_callback.load()("%s:%d:%s", file, line, msg);
}

void
cubeb_log_internal_no_format(const char * msg)
{
  g_cubeb_log_callback.load()(msg);
}

void
cubeb_async_log(char const * fmt, ...)
{
  // This is going to copy a 256 bytes array around, which is fine.
  // We don't want to allocate memory here, because this is made to
  // be called from a real-time callback.
  va_list args;
  va_start(args, fmt);
  char msg[CUBEB_LOG_MESSAGE_MAX_SIZE];
  vsnprintf(msg, CUBEB_LOG_MESSAGE_MAX_SIZE, fmt, args);
  cubeb_async_logger::get().push(msg);
  va_end(args);
}

void
cubeb_async_log_reset_threads(void)
{
  if (!g_cubeb_log_callback) {
    return;
  }
  cubeb_async_logger::get().reset_producer_thread();
}

void
cubeb_log_set(cubeb_log_level log_level, cubeb_log_callback log_callback)
{
  g_cubeb_log_level = log_level;
  // Once a callback has a been set, `g_cubeb_log_callback` is never set back to
  // nullptr, to prevent a TOCTOU race between checking the pointer
  if (log_callback && log_level != CUBEB_LOG_DISABLED) {
    g_cubeb_log_callback = log_callback;
    if (log_level == CUBEB_LOG_VERBOSE) {
      cubeb_async_logger::get().start();
    }
  } else if (!log_callback || CUBEB_LOG_DISABLED) {
    g_cubeb_log_callback = cubeb_noop_log_callback;
    // This returns once the thread has joined.
    // This is safe even if CUBEB_LOG_VERBOSE was not set; the thread will
    // simply not be joinable.
    cubeb_async_logger::get().stop();
  } else {
    assert(false && "Incorrect parameters passed to cubeb_log_set");
  }
}

cubeb_log_level
cubeb_log_get_level()
{
  return g_cubeb_log_level;
}

cubeb_log_callback
cubeb_log_get_callback()
{
  if (g_cubeb_log_callback == cubeb_noop_log_callback) {
    return nullptr;
  }
  return g_cubeb_log_callback;
}
