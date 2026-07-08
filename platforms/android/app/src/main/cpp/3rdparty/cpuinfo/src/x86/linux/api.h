#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <cpuinfo.h>
#include <cpuinfo/common.h>
#include <linux/api.h>
#include <x86/api.h>

struct cpuinfo_x86_linux_processor {
	uint32_t apic_id;
	uint32_t linux_id;
	uint32_t flags;
};

CPUINFO_INTERNAL bool cpuinfo_x86_linux_parse_proc_cpuinfo(
	uint32_t max_processors_count,
	struct cpuinfo_x86_linux_processor processors[restrict static max_processors_count]);
