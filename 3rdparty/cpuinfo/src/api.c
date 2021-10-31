#include <stdbool.h>
#include <stddef.h>

#include <cpuinfo.h>
#include <cpuinfo/internal-api.h>
#include <cpuinfo/log.h>

#ifdef __linux__
	#include <linux/api.h>

	#include <unistd.h>
	#include <sys/syscall.h>
	#if !defined(__NR_getcpu)
		#include <asm-generic/unistd.h>
	#endif
#endif

bool cpuinfo_is_initialized = false;

struct cpuinfo_processor* cpuinfo_processors = NULL;
struct cpuinfo_core* cpuinfo_cores = NULL;
struct cpuinfo_cluster* cpuinfo_clusters = NULL;
struct cpuinfo_package* cpuinfo_packages = NULL;
struct cpuinfo_cache* cpuinfo_cache[cpuinfo_cache_level_max] = { NULL };

uint32_t cpuinfo_processors_count = 0;
uint32_t cpuinfo_cores_count = 0;
uint32_t cpuinfo_clusters_count = 0;
uint32_t cpuinfo_packages_count = 0;
uint32_t cpuinfo_cache_count[cpuinfo_cache_level_max] = { 0 };
uint32_t cpuinfo_max_cache_size = 0;

#if CPUINFO_ARCH_ARM || CPUINFO_ARCH_ARM64
	struct cpuinfo_uarch_info* cpuinfo_uarchs = NULL;
	uint32_t cpuinfo_uarchs_count = 0;
#else
	struct cpuinfo_uarch_info cpuinfo_global_uarch = { cpuinfo_uarch_unknown };
#endif

#ifdef __linux__
	uint32_t cpuinfo_linux_cpu_max = 0;
	const struct cpuinfo_processor** cpuinfo_linux_cpu_to_processor_map = NULL;
	const struct cpuinfo_core** cpuinfo_linux_cpu_to_core_map = NULL;
	#if CPUINFO_ARCH_ARM || CPUINFO_ARCH_ARM64
		const uint32_t* cpuinfo_linux_cpu_to_uarch_index_map = NULL;
	#endif
#endif


const struct cpuinfo_processor* cpuinfo_get_processors(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "processors");
	}
	return cpuinfo_processors;
}

const struct cpuinfo_core* cpuinfo_get_cores(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "core");
	}
	return cpuinfo_cores;
}

const struct cpuinfo_cluster* cpuinfo_get_clusters(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "clusters");
	}
	return cpuinfo_clusters;
}

const struct cpuinfo_package* cpuinfo_get_packages(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "packages");
	}
	return cpuinfo_packages;
}

const struct cpuinfo_uarch_info* cpuinfo_get_uarchs() {
	if (!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "uarchs");
	}
	#if CPUINFO_ARCH_ARM || CPUINFO_ARCH_ARM64
		return cpuinfo_uarchs;
	#else
		return &cpuinfo_global_uarch;
	#endif
}

const struct cpuinfo_processor* cpuinfo_get_processor(uint32_t index) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "processor");
	}
	if CPUINFO_UNLIKELY(index >= cpuinfo_processors_count) {
		return NULL;
	}
	return &cpuinfo_processors[index];
}

const struct cpuinfo_core* cpuinfo_get_core(uint32_t index) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "core");
	}
	if CPUINFO_UNLIKELY(index >= cpuinfo_cores_count) {
		return NULL;
	}
	return &cpuinfo_cores[index];
}

const struct cpuinfo_cluster* cpuinfo_get_cluster(uint32_t index) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "cluster");
	}
	if CPUINFO_UNLIKELY(index >= cpuinfo_clusters_count) {
		return NULL;
	}
	return &cpuinfo_clusters[index];
}

const struct cpuinfo_package* cpuinfo_get_package(uint32_t index) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "package");
	}
	if CPUINFO_UNLIKELY(index >= cpuinfo_packages_count) {
		return NULL;
	}
	return &cpuinfo_packages[index];
}

const struct cpuinfo_uarch_info* cpuinfo_get_uarch(uint32_t index) {
	if (!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "uarch");
	}
	#if CPUINFO_ARCH_ARM || CPUINFO_ARCH_ARM64
		if CPUINFO_UNLIKELY(index >= cpuinfo_uarchs_count) {
			return NULL;
		}
		return &cpuinfo_uarchs[index];
	#else
		if CPUINFO_UNLIKELY(index != 0) {
			return NULL;
		}
		return &cpuinfo_global_uarch;
	#endif
}

uint32_t cpuinfo_get_processors_count(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "processors_count");
	}
	return cpuinfo_processors_count;
}

uint32_t cpuinfo_get_cores_count(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "cores_count");
	}
	return cpuinfo_cores_count;
}

uint32_t cpuinfo_get_clusters_count(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "clusters_count");
	}
	return cpuinfo_clusters_count;
}

uint32_t cpuinfo_get_packages_count(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "packages_count");
	}
	return cpuinfo_packages_count;
}

uint32_t cpuinfo_get_uarchs_count(void) {
	if (!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "uarchs_count");
	}
	#if CPUINFO_ARCH_ARM || CPUINFO_ARCH_ARM64
		return cpuinfo_uarchs_count;
	#else
		return 1;
	#endif
}

const struct cpuinfo_cache* CPUINFO_ABI cpuinfo_get_l1i_caches(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "l1i_caches");
	}
	return cpuinfo_cache[cpuinfo_cache_level_1i];
}

const struct cpuinfo_cache* CPUINFO_ABI cpuinfo_get_l1d_caches(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "l1d_caches");
	}
	return cpuinfo_cache[cpuinfo_cache_level_1d];
}

const struct cpuinfo_cache* CPUINFO_ABI cpuinfo_get_l2_caches(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "l2_caches");
	}
	return cpuinfo_cache[cpuinfo_cache_level_2];
}

const struct cpuinfo_cache* CPUINFO_ABI cpuinfo_get_l3_caches(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "l3_caches");
	}
	return cpuinfo_cache[cpuinfo_cache_level_3];
}

const struct cpuinfo_cache* CPUINFO_ABI cpuinfo_get_l4_caches(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "l4_caches");
	}
	return cpuinfo_cache[cpuinfo_cache_level_4];
}

const struct cpuinfo_cache* CPUINFO_ABI cpuinfo_get_l1i_cache(uint32_t index) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "l1i_cache");
	}
	if CPUINFO_UNLIKELY(index >= cpuinfo_cache_count[cpuinfo_cache_level_1i]) {
		return NULL;
	}
	return &cpuinfo_cache[cpuinfo_cache_level_1i][index];
}

const struct cpuinfo_cache* CPUINFO_ABI cpuinfo_get_l1d_cache(uint32_t index) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "l1d_cache");
	}
	if CPUINFO_UNLIKELY(index >= cpuinfo_cache_count[cpuinfo_cache_level_1d]) {
		return NULL;
	}
	return &cpuinfo_cache[cpuinfo_cache_level_1d][index];
}

const struct cpuinfo_cache* CPUINFO_ABI cpuinfo_get_l2_cache(uint32_t index) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "l2_cache");
	}
	if CPUINFO_UNLIKELY(index >= cpuinfo_cache_count[cpuinfo_cache_level_2]) {
		return NULL;
	}
	return &cpuinfo_cache[cpuinfo_cache_level_2][index];
}

const struct cpuinfo_cache* CPUINFO_ABI cpuinfo_get_l3_cache(uint32_t index) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "l3_cache");
	}
	if CPUINFO_UNLIKELY(index >= cpuinfo_cache_count[cpuinfo_cache_level_3]) {
		return NULL;
	}
	return &cpuinfo_cache[cpuinfo_cache_level_3][index];
}

const struct cpuinfo_cache* CPUINFO_ABI cpuinfo_get_l4_cache(uint32_t index) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "l4_cache");
	}
	if CPUINFO_UNLIKELY(index >= cpuinfo_cache_count[cpuinfo_cache_level_4]) {
		return NULL;
	}
	return &cpuinfo_cache[cpuinfo_cache_level_4][index];
}

uint32_t CPUINFO_ABI cpuinfo_get_l1i_caches_count(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "l1i_caches_count");
	}
	return cpuinfo_cache_count[cpuinfo_cache_level_1i];
}

uint32_t CPUINFO_ABI cpuinfo_get_l1d_caches_count(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "l1d_caches_count");
	}
	return cpuinfo_cache_count[cpuinfo_cache_level_1d];
}

uint32_t CPUINFO_ABI cpuinfo_get_l2_caches_count(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "l2_caches_count");
	}
	return cpuinfo_cache_count[cpuinfo_cache_level_2];
}

uint32_t CPUINFO_ABI cpuinfo_get_l3_caches_count(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "l3_caches_count");
	}
	return cpuinfo_cache_count[cpuinfo_cache_level_3];
}

uint32_t CPUINFO_ABI cpuinfo_get_l4_caches_count(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "l4_caches_count");
	}
	return cpuinfo_cache_count[cpuinfo_cache_level_4];
}

uint32_t CPUINFO_ABI cpuinfo_get_max_cache_size(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "max_cache_size");
	}
	return cpuinfo_max_cache_size;
}

const struct cpuinfo_processor* CPUINFO_ABI cpuinfo_get_current_processor(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "current_processor");
	}
	#ifdef __linux__
		/* Initializing this variable silences a MemorySanitizer error. */
		unsigned cpu = 0;
		if CPUINFO_UNLIKELY(syscall(__NR_getcpu, &cpu, NULL, NULL) != 0) {
			return 0;
		}
		if CPUINFO_UNLIKELY((uint32_t) cpu >= cpuinfo_linux_cpu_max) {
			return 0;
		}
		return cpuinfo_linux_cpu_to_processor_map[cpu];
	#else
		return NULL;
	#endif
}

const struct cpuinfo_core* CPUINFO_ABI cpuinfo_get_current_core(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "current_core");
	}
	#ifdef __linux__
		/* Initializing this variable silences a MemorySanitizer error. */
		unsigned cpu = 0;
		if CPUINFO_UNLIKELY(syscall(__NR_getcpu, &cpu, NULL, NULL) != 0) {
			return 0;
		}
		if CPUINFO_UNLIKELY((uint32_t) cpu >= cpuinfo_linux_cpu_max) {
			return 0;
		}
		return cpuinfo_linux_cpu_to_core_map[cpu];
	#else
		return NULL;
	#endif
}

uint32_t CPUINFO_ABI cpuinfo_get_current_uarch_index(void) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "current_uarch_index");
	}
	#if CPUINFO_ARCH_ARM || CPUINFO_ARCH_ARM64
		#ifdef __linux__
			if (cpuinfo_linux_cpu_to_uarch_index_map == NULL) {
				/* Special case: avoid syscall on systems with only a single type of cores */
				return 0;
			}

			/* General case */
			/* Initializing this variable silences a MemorySanitizer error. */
			unsigned cpu = 0;
			if CPUINFO_UNLIKELY(syscall(__NR_getcpu, &cpu, NULL, NULL) != 0) {
				return 0;
			}
			if CPUINFO_UNLIKELY((uint32_t) cpu >= cpuinfo_linux_cpu_max) {
				return 0;
			}
			return cpuinfo_linux_cpu_to_uarch_index_map[cpu];
		#else
			/* Fallback: pretend to be on the big core. */
			return 0;
		#endif
	#else
		/* Only ARM/ARM64 processors may include cores of different types in the same package. */
		return 0;
	#endif
}

uint32_t CPUINFO_ABI cpuinfo_get_current_uarch_index_with_default(uint32_t default_uarch_index) {
	if CPUINFO_UNLIKELY(!cpuinfo_is_initialized) {
		cpuinfo_log_fatal("cpuinfo_get_%s called before cpuinfo is initialized", "current_uarch_index_with_default");
	}
	#if CPUINFO_ARCH_ARM || CPUINFO_ARCH_ARM64
		#ifdef __linux__
			if (cpuinfo_linux_cpu_to_uarch_index_map == NULL) {
				/* Special case: avoid syscall on systems with only a single type of cores */
				return 0;
			}

			/* General case */
			/* Initializing this variable silences a MemorySanitizer error. */
			unsigned cpu = 0;
			if CPUINFO_UNLIKELY(syscall(__NR_getcpu, &cpu, NULL, NULL) != 0) {
				return default_uarch_index;
			}
			if CPUINFO_UNLIKELY((uint32_t) cpu >= cpuinfo_linux_cpu_max) {
				return default_uarch_index;
			}
			return cpuinfo_linux_cpu_to_uarch_index_map[cpu];
		#else
			/* Fallback: no API to query current core, use default uarch index. */
			return default_uarch_index;
		#endif
	#else
		/* Only ARM/ARM64 processors may include cores of different types in the same package. */
		return 0;
	#endif
}
