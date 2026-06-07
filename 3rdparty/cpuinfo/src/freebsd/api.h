#pragma once

#include <stdint.h>

struct cpuinfo_freebsd_topology {
	uint32_t packages;
	uint32_t cores;
	uint32_t threads;
	uint32_t threads_per_core;
};

struct cpuinfo_freebsd_topology cpuinfo_freebsd_detect_topology(void);
