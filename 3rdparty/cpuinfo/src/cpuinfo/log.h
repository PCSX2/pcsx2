#pragma once

#include <inttypes.h>

#include <clog.h>

#define CPUINFO_LOG_DEBUG_PARSERS 0

#ifndef CPUINFO_LOG_LEVEL
	#define CPUINFO_LOG_LEVEL CLOG_ERROR
#endif

CLOG_DEFINE_LOG_DEBUG(cpuinfo_log_debug, "cpuinfo", CPUINFO_LOG_LEVEL);
CLOG_DEFINE_LOG_INFO(cpuinfo_log_info, "cpuinfo", CPUINFO_LOG_LEVEL);
CLOG_DEFINE_LOG_WARNING(cpuinfo_log_warning, "cpuinfo", CPUINFO_LOG_LEVEL);
CLOG_DEFINE_LOG_ERROR(cpuinfo_log_error, "cpuinfo", CPUINFO_LOG_LEVEL);
CLOG_DEFINE_LOG_FATAL(cpuinfo_log_fatal, "cpuinfo", CPUINFO_LOG_LEVEL);
