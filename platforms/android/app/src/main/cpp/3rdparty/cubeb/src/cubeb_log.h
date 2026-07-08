/*
 * Copyright © 2016 Mozilla Foundation
 *
 * This program is made available under an ISC-style license.  See the
 * accompanying file LICENSE for details.
 */

#ifndef CUBEB_LOG
#define CUBEB_LOG

#include "cubeb/cubeb.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__GNUC__) || defined(__clang__)
#define PRINTF_FORMAT(fmt, args) __attribute__((format(printf, fmt, args)))
#if defined(__FILE_NAME__)
#define __FILENAME__ __FILE_NAME__
#else
#define __FILENAME__                                                           \
  (__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1     \
                                    : __FILE__)
#endif
#else
#define PRINTF_FORMAT(fmt, args)
#include <string.h>
#define __FILENAME__                                                           \
  (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#endif

void
cubeb_log_set(cubeb_log_level log_level, cubeb_log_callback log_callback);
cubeb_log_level
cubeb_log_get_level(void);
cubeb_log_callback
cubeb_log_get_callback(void);
void
cubeb_log_internal_no_format(const char * msg);
void
cubeb_log_internal(const char * filename, uint32_t line, const char * fmt, ...)
    PRINTF_FORMAT(3, 4);
void
cubeb_async_log(const char * fmt, ...) PRINTF_FORMAT(1, 2);
void
cubeb_async_log_reset_threads(void);

#ifdef __cplusplus
}
#endif

#define LOGV(msg, ...) LOG_INTERNAL(CUBEB_LOG_VERBOSE, msg, ##__VA_ARGS__)
#define LOG(msg, ...) LOG_INTERNAL(CUBEB_LOG_NORMAL, msg, ##__VA_ARGS__)

#define LOG_INTERNAL(level, fmt, ...)                                          \
  do {                                                                         \
    if (cubeb_log_get_level() >= level && cubeb_log_get_callback()) {          \
      cubeb_log_internal(__FILENAME__, __LINE__, fmt, ##__VA_ARGS__);          \
    }                                                                          \
  } while (0)

#define ALOG_INTERNAL(level, fmt, ...)                                         \
  do {                                                                         \
    if (cubeb_log_get_level() >= level && cubeb_log_get_callback()) {          \
      cubeb_async_log(fmt, ##__VA_ARGS__);                                     \
    }                                                                          \
  } while (0)

/* Asynchronous logging macros to log in real-time callbacks. */
/* Should not be used on android due to the use of global/static variables. */
#define ALOGV(msg, ...) ALOG_INTERNAL(CUBEB_LOG_VERBOSE, msg, ##__VA_ARGS__)
#define ALOG(msg, ...) ALOG_INTERNAL(CUBEB_LOG_NORMAL, msg, ##__VA_ARGS__)

#endif // CUBEB_LOG
