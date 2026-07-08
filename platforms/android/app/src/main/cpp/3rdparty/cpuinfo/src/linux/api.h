#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include <cpuinfo.h>
#include <cpuinfo/common.h>

#define CPUINFO_LINUX_FLAG_PRESENT UINT32_C(0x00000001)
#define CPUINFO_LINUX_FLAG_POSSIBLE UINT32_C(0x00000002)
#define CPUINFO_LINUX_FLAG_MAX_FREQUENCY UINT32_C(0x00000004)
#define CPUINFO_LINUX_FLAG_MIN_FREQUENCY UINT32_C(0x00000008)
#define CPUINFO_LINUX_FLAG_SMT_ID UINT32_C(0x00000010)
#define CPUINFO_LINUX_FLAG_CORE_ID UINT32_C(0x00000020)
#define CPUINFO_LINUX_FLAG_PACKAGE_ID UINT32_C(0x00000040)
#define CPUINFO_LINUX_FLAG_APIC_ID UINT32_C(0x00000080)
#define CPUINFO_LINUX_FLAG_SMT_CLUSTER UINT32_C(0x00000100)
#define CPUINFO_LINUX_FLAG_CORE_CLUSTER UINT32_C(0x00000200)
#define CPUINFO_LINUX_FLAG_PACKAGE_CLUSTER UINT32_C(0x00000400)
#define CPUINFO_LINUX_FLAG_PROC_CPUINFO UINT32_C(0x00000800)
#define CPUINFO_LINUX_FLAG_VALID UINT32_C(0x00001000)
#define CPUINFO_LINUX_FLAG_CUR_FREQUENCY UINT32_C(0x00002000)
#define CPUINFO_LINUX_FLAG_CLUSTER_CLUSTER UINT32_C(0x00004000)

typedef bool (*cpuinfo_cpulist_callback)(uint32_t, uint32_t, void*);
CPUINFO_INTERNAL bool cpuinfo_linux_parse_cpulist(
	const char* filename,
	cpuinfo_cpulist_callback callback,
	void* context);
typedef bool (*cpuinfo_smallfile_callback)(const char*, const char*, const char*, void*);
CPUINFO_INTERNAL bool cpuinfo_linux_parse_small_file(
	const char* filename,
	size_t buffer_size,
	cpuinfo_smallfile_callback,
	void* context);
typedef bool (*cpuinfo_line_callback)(const char*, const char*, void*, uint64_t);
CPUINFO_INTERNAL bool cpuinfo_linux_parse_multiline_file(
	const char* filename,
	size_t buffer_size,
	cpuinfo_line_callback,
	void* context);

CPUINFO_INTERNAL uint32_t cpuinfo_linux_get_max_processors_count(void);
CPUINFO_INTERNAL uint32_t cpuinfo_linux_get_max_possible_processor(uint32_t max_processors_count);
CPUINFO_INTERNAL uint32_t cpuinfo_linux_get_max_present_processor(uint32_t max_processors_count);
CPUINFO_INTERNAL uint32_t cpuinfo_linux_get_processor_cur_frequency(uint32_t processor);
CPUINFO_INTERNAL uint32_t cpuinfo_linux_get_processor_min_frequency(uint32_t processor);
CPUINFO_INTERNAL uint32_t cpuinfo_linux_get_processor_max_frequency(uint32_t processor);
CPUINFO_INTERNAL bool cpuinfo_linux_get_processor_package_id(
	uint32_t processor,
	uint32_t package_id[restrict static 1]);
CPUINFO_INTERNAL bool cpuinfo_linux_get_processor_core_id(uint32_t processor, uint32_t core_id[restrict static 1]);

CPUINFO_INTERNAL bool cpuinfo_linux_detect_possible_processors(
	uint32_t max_processors_count,
	uint32_t* processor0_flags,
	uint32_t processor_struct_size,
	uint32_t possible_flag);
CPUINFO_INTERNAL bool cpuinfo_linux_detect_present_processors(
	uint32_t max_processors_count,
	uint32_t* processor0_flags,
	uint32_t processor_struct_size,
	uint32_t present_flag);

typedef bool (*cpuinfo_siblings_callback)(uint32_t, uint32_t, uint32_t, void*);
CPUINFO_INTERNAL bool cpuinfo_linux_detect_core_siblings(
	uint32_t max_processors_count,
	uint32_t processor,
	cpuinfo_siblings_callback callback,
	void* context);
CPUINFO_INTERNAL bool cpuinfo_linux_detect_thread_siblings(
	uint32_t max_processors_count,
	uint32_t processor,
	cpuinfo_siblings_callback callback,
	void* context);
CPUINFO_INTERNAL bool cpuinfo_linux_detect_cluster_cpus(
	uint32_t max_processors_count,
	uint32_t processor,
	cpuinfo_siblings_callback callback,
	void* context);
CPUINFO_INTERNAL bool cpuinfo_linux_detect_core_cpus(
	uint32_t max_processors_count,
	uint32_t processor,
	cpuinfo_siblings_callback callback,
	void* context);
CPUINFO_INTERNAL bool cpuinfo_linux_detect_package_cpus(
	uint32_t max_processors_count,
	uint32_t processor,
	cpuinfo_siblings_callback callback,
	void* context);

extern CPUINFO_INTERNAL const struct cpuinfo_processor** cpuinfo_linux_cpu_to_processor_map;
extern CPUINFO_INTERNAL const struct cpuinfo_core** cpuinfo_linux_cpu_to_core_map;
