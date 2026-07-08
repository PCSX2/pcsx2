#pragma once

#include <stdbool.h>
#include <stdint.h>

#if defined(_WIN32) || defined(__CYGWIN__)
#include <windows.h>
#endif

#include <cpuinfo.h>
#include <cpuinfo/common.h>

enum cpuinfo_cache_level {
	cpuinfo_cache_level_1i = 0,
	cpuinfo_cache_level_1d = 1,
	cpuinfo_cache_level_2 = 2,
	cpuinfo_cache_level_3 = 3,
	cpuinfo_cache_level_4 = 4,
	cpuinfo_cache_level_max = 5,
};

extern CPUINFO_INTERNAL bool cpuinfo_is_initialized;

extern CPUINFO_INTERNAL struct cpuinfo_processor* cpuinfo_processors;
extern CPUINFO_INTERNAL struct cpuinfo_core* cpuinfo_cores;
extern CPUINFO_INTERNAL struct cpuinfo_cluster* cpuinfo_clusters;
extern CPUINFO_INTERNAL struct cpuinfo_package* cpuinfo_packages;
extern CPUINFO_INTERNAL struct cpuinfo_cache* cpuinfo_cache[cpuinfo_cache_level_max];

extern CPUINFO_INTERNAL uint32_t cpuinfo_processors_count;
extern CPUINFO_INTERNAL uint32_t cpuinfo_cores_count;
extern CPUINFO_INTERNAL uint32_t cpuinfo_clusters_count;
extern CPUINFO_INTERNAL uint32_t cpuinfo_packages_count;
extern CPUINFO_INTERNAL uint32_t cpuinfo_cache_count[cpuinfo_cache_level_max];
extern CPUINFO_INTERNAL uint32_t cpuinfo_max_cache_size;

#if CPUINFO_ARCH_ARM || CPUINFO_ARCH_ARM64 || CPUINFO_ARCH_RISCV32 || CPUINFO_ARCH_RISCV64
extern CPUINFO_INTERNAL struct cpuinfo_uarch_info* cpuinfo_uarchs;
extern CPUINFO_INTERNAL uint32_t cpuinfo_uarchs_count;
#else
extern CPUINFO_INTERNAL struct cpuinfo_uarch_info cpuinfo_global_uarch;
#endif

#ifdef __linux__
extern CPUINFO_INTERNAL uint32_t cpuinfo_linux_cpu_max;
extern CPUINFO_INTERNAL const struct cpuinfo_processor** cpuinfo_linux_cpu_to_processor_map;
extern CPUINFO_INTERNAL const struct cpuinfo_core** cpuinfo_linux_cpu_to_core_map;
#endif

CPUINFO_PRIVATE void cpuinfo_x86_mach_init(void);
CPUINFO_PRIVATE void cpuinfo_x86_linux_init(void);
CPUINFO_PRIVATE void cpuinfo_x86_freebsd_init(void);
#if defined(_WIN32) || defined(__CYGWIN__)
#if CPUINFO_ARCH_ARM64
CPUINFO_PRIVATE BOOL CALLBACK cpuinfo_arm_windows_init(PINIT_ONCE init_once, PVOID parameter, PVOID* context);
#else
CPUINFO_PRIVATE BOOL CALLBACK cpuinfo_x86_windows_init(PINIT_ONCE init_once, PVOID parameter, PVOID* context);
#endif
#endif
CPUINFO_PRIVATE void cpuinfo_arm_mach_init(void);
CPUINFO_PRIVATE void cpuinfo_arm_linux_init(void);
CPUINFO_PRIVATE void cpuinfo_riscv_linux_init(void);
CPUINFO_PRIVATE void cpuinfo_emscripten_init(void);

CPUINFO_PRIVATE uint32_t cpuinfo_compute_max_cache_size(const struct cpuinfo_processor* processor);

typedef void (*cpuinfo_processor_callback)(uint32_t);
