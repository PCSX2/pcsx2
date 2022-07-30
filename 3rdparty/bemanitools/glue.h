#ifndef BEMANITOOLS_GLUE_H
#define BEMANITOOLS_GLUE_H

/* Common definitions for integration bindings */

#include <stdint.h>

#ifdef __GNUC__
/* Bemanitools is compiled with GCC (MinGW, specifically) as of version 5 */
#define LOG_CHECK_FMT __attribute__((format(printf, 2, 3)))
#else
/* Compile it out for MSVC plebs */
#define LOG_CHECK_FMT
#endif

/* An AVS-style logger function. Comes in four flavors: misc, info, warning,
   and fatal, with increasing severity. Fatal loggers do not return, they
   abort the running process after writing their message to the log.

   "module" is an arbitrary short string identifying the source of the log
   message. The name of the calling DLL is a good default choice for this
   string, although you might want to identify a module within your DLL here
   instead.

   "fmt" is a printf-style format string. Depending on the context in which
   your DLL is running you might end up calling a logger function exported
   from libavs, which has its own printf implementation (including a number of
   proprietary extensions), so don't use any overly exotic formats. */

typedef void (*log_formatter_t)(const char *module, const char *fmt, ...)
    LOG_CHECK_FMT;

/* An API for spawning threads. This API is defined by libavs, although
   Bemanitools itself may supply compatible implementations of these functions
   to your DLL, depending on the context in which it runs.

   NOTE: You may only use the logging functions from a thread where Bemanitools
   calls you, or a thread that you create using this API. Failure to observe
   this restriction will cause the process to crash. This is a limitation of
   libavs itself, not Bemanitools. */

typedef int (*thread_create_t)(
    int (*proc)(void *), void *ctx, uint32_t stack_sz, unsigned int priority);
typedef void (*thread_join_t)(int thread_id, int *result);
typedef void (*thread_destroy_t)(int thread_id);

#endif
