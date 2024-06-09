#pragma once

#include <inttypes.h>
#include <stdarg.h>
#include <stdlib.h>

#ifndef CPUINFO_LOG_LEVEL
#error "Undefined CPUINFO_LOG_LEVEL"
#endif

#define CPUINFO_LOG_NONE 0
#define CPUINFO_LOG_FATAL 1
#define CPUINFO_LOG_ERROR 2
#define CPUINFO_LOG_WARNING 3
#define CPUINFO_LOG_INFO 4
#define CPUINFO_LOG_DEBUG 5

#ifndef CPUINFO_LOG_DEBUG_PARSERS
#define CPUINFO_LOG_DEBUG_PARSERS 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

#if CPUINFO_LOG_LEVEL >= CPUINFO_LOG_DEBUG
void cpuinfo_vlog_debug(const char* format, va_list args);
#endif

#if CPUINFO_LOG_LEVEL >= CPUINFO_LOG_INFO
void cpuinfo_vlog_info(const char* format, va_list args);
#endif

#if CPUINFO_LOG_LEVEL >= CPUINFO_LOG_WARNING
void cpuinfo_vlog_warning(const char* format, va_list args);
#endif

#if CPUINFO_LOG_LEVEL >= CPUINFO_LOG_ERROR
void cpuinfo_vlog_error(const char* format, va_list args);
#endif

#if CPUINFO_LOG_LEVEL >= CPUINFO_LOG_FATAL
void cpuinfo_vlog_fatal(const char* format, va_list args);
#endif

#ifdef __cplusplus
} // extern "C"
#endif

#ifndef CPUINFO_LOG_ARGUMENTS_FORMAT
#ifdef __GNUC__
#define CPUINFO_LOG_ARGUMENTS_FORMAT __attribute__((__format__(__printf__, 1, 2)))
#else
#define CPUINFO_LOG_ARGUMENTS_FORMAT
#endif
#endif

CPUINFO_LOG_ARGUMENTS_FORMAT inline static void cpuinfo_log_debug(const char* format, ...) {
#if CPUINFO_LOG_LEVEL >= CPUINFO_LOG_DEBUG
	va_list args;
	va_start(args, format);
	cpuinfo_vlog_debug(format, args);
	va_end(args);
#endif
}

CPUINFO_LOG_ARGUMENTS_FORMAT inline static void cpuinfo_log_info(const char* format, ...) {
#if CPUINFO_LOG_LEVEL >= CPUINFO_LOG_INFO
	va_list args;
	va_start(args, format);
	cpuinfo_vlog_info(format, args);
	va_end(args);
#endif
}

CPUINFO_LOG_ARGUMENTS_FORMAT inline static void cpuinfo_log_warning(const char* format, ...) {
#if CPUINFO_LOG_LEVEL >= CPUINFO_LOG_WARNING
	va_list args;
	va_start(args, format);
	cpuinfo_vlog_warning(format, args);
	va_end(args);
#endif
}

CPUINFO_LOG_ARGUMENTS_FORMAT inline static void cpuinfo_log_error(const char* format, ...) {
#if CPUINFO_LOG_LEVEL >= CPUINFO_LOG_ERROR
	va_list args;
	va_start(args, format);
	cpuinfo_vlog_error(format, args);
	va_end(args);
#endif
}

CPUINFO_LOG_ARGUMENTS_FORMAT inline static void cpuinfo_log_fatal(const char* format, ...) {
#if CPUINFO_LOG_LEVEL >= CPUINFO_LOG_FATAL
	va_list args;
	va_start(args, format);
	cpuinfo_vlog_fatal(format, args);
	va_end(args);
#endif
	abort();
}