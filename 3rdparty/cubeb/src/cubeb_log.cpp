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

static std::atomic<cubeb_log_level> g_cubeb_log_level;
static std::atomic<cubeb_log_callback> g_cubeb_log_callback;

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
cubeb_log_set(cubeb_log_level log_level, cubeb_log_callback log_callback)
{
  g_cubeb_log_level = log_level;
  // Once a callback has a been set, `g_cubeb_log_callback` is never set back to
  // nullptr, to prevent a TOCTOU race between checking the pointer
  if (log_callback && log_level != CUBEB_LOG_DISABLED) {
    g_cubeb_log_callback = log_callback;
  } else if (!log_callback || CUBEB_LOG_DISABLED) {
    g_cubeb_log_callback = cubeb_noop_log_callback;
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
