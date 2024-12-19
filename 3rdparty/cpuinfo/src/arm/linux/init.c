#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <arm/linux/api.h>
#include <cpuinfo.h>
#if defined(__ANDROID__)
#include <arm/android/api.h>
#endif
#include <arm/api.h>
#include <arm/midr.h>
#include <cpuinfo/internal-api.h>
#include <cpuinfo/log.h>
#include <linux/api.h>

struct cpuinfo_arm_isa cpuinfo_isa = {0};

static struct cpuinfo_package package = {{0}};

static inline bool bitmask_all(uint32_t bitfield, uint32_t mask) {
	return (bitfield & mask) == mask;
}

static inline uint32_t min(uint32_t a, uint32_t b) {
	return a < b ? a : b;
}

static inline int cmp(uint32_t a, uint32_t b) {
	return (a > b) - (a < b);
}

static bool cluster_siblings_parser(
	uint32_t processor,
	uint32_t siblings_start,
	uint32_t siblings_end,
	struct cpuinfo_arm_linux_processor* processors) {
	processors[processor].flags |= CPUINFO_LINUX_FLAG_PACKAGE_CLUSTER;
	uint32_t package_leader_id = processors[processor].package_leader_id;

	for (uint32_t sibling = siblings_start; sibling < siblings_end; sibling++) {
		if (!bitmask_all(processors[sibling].flags, CPUINFO_LINUX_FLAG_VALID)) {
			cpuinfo_log_info(
				"invalid processor %" PRIu32 " reported as a sibling for processor %" PRIu32,
				sibling,
				processor);
			continue;
		}

		const uint32_t sibling_package_leader_id = processors[sibling].package_leader_id;
		if (sibling_package_leader_id < package_leader_id) {
			package_leader_id = sibling_package_leader_id;
		}

		processors[sibling].package_leader_id = package_leader_id;
		processors[sibling].flags |= CPUINFO_LINUX_FLAG_PACKAGE_CLUSTER;
	}

	processors[processor].package_leader_id = package_leader_id;

	return true;
}

static int cmp_arm_linux_processor(const void* ptr_a, const void* ptr_b) {
	const struct cpuinfo_arm_linux_processor* processor_a = (const struct cpuinfo_arm_linux_processor*)ptr_a;
	const struct cpuinfo_arm_linux_processor* processor_b = (const struct cpuinfo_arm_linux_processor*)ptr_b;

	/* Move usable processors towards the start of the array */
	const bool usable_a = bitmask_all(processor_a->flags, CPUINFO_LINUX_FLAG_VALID);
	const bool usable_b = bitmask_all(processor_b->flags, CPUINFO_LINUX_FLAG_VALID);
	if (usable_a != usable_b) {
		return (int)usable_b - (int)usable_a;
	}

	/* Compare based on core type (e.g. Cortex-A57 < Cortex-A53) */
	const uint32_t midr_a = processor_a->midr;
	const uint32_t midr_b = processor_b->midr;
	if (midr_a != midr_b) {
		const uint32_t score_a = midr_score_core(midr_a);
		const uint32_t score_b = midr_score_core(midr_b);
		if (score_a != score_b) {
			return score_a > score_b ? -1 : 1;
		}
	}

	/* Compare based on core frequency (e.g. 2.0 GHz < 1.2 GHz) */
	const uint32_t frequency_a = processor_a->max_frequency;
	const uint32_t frequency_b = processor_b->max_frequency;
	if (frequency_a != frequency_b) {
		return frequency_a > frequency_b ? -1 : 1;
	}

	/* Compare based on cluster leader id (i.e. cluster 1 < cluster 0) */
	const uint32_t cluster_a = processor_a->package_leader_id;
	const uint32_t cluster_b = processor_b->package_leader_id;
	if (cluster_a != cluster_b) {
		return cluster_a > cluster_b ? -1 : 1;
	}

	/* Compare based on system processor id (i.e. processor 0 < processor 1)
	 */
	const uint32_t id_a = processor_a->system_processor_id;
	const uint32_t id_b = processor_b->system_processor_id;
	return cmp(id_a, id_b);
}

void cpuinfo_arm_linux_init(void) {
	struct cpuinfo_arm_linux_processor* arm_linux_processors = NULL;
	struct cpuinfo_processor* processors = NULL;
	struct cpuinfo_core* cores = NULL;
	struct cpuinfo_cluster* clusters = NULL;
	struct cpuinfo_uarch_info* uarchs = NULL;
	struct cpuinfo_cache* l1i = NULL;
	struct cpuinfo_cache* l1d = NULL;
	struct cpuinfo_cache* l2 = NULL;
	struct cpuinfo_cache* l3 = NULL;
	const struct cpuinfo_processor** linux_cpu_to_processor_map = NULL;
	const struct cpuinfo_core** linux_cpu_to_core_map = NULL;
	uint32_t* linux_cpu_to_uarch_index_map = NULL;

	const uint32_t max_processors_count = cpuinfo_linux_get_max_processors_count();
	cpuinfo_log_debug("system maximum processors count: %" PRIu32, max_processors_count);

	const uint32_t max_possible_processors_count =
		1 + cpuinfo_linux_get_max_possible_processor(max_processors_count);
	cpuinfo_log_debug("maximum possible processors count: %" PRIu32, max_possible_processors_count);
	const uint32_t max_present_processors_count = 1 + cpuinfo_linux_get_max_present_processor(max_processors_count);
	cpuinfo_log_debug("maximum present processors count: %" PRIu32, max_present_processors_count);

	uint32_t valid_processor_mask = 0;
	uint32_t arm_linux_processors_count = max_processors_count;
	if (max_present_processors_count != 0) {
		arm_linux_processors_count = min(arm_linux_processors_count, max_present_processors_count);
		valid_processor_mask = CPUINFO_LINUX_FLAG_PRESENT;
	}
	if (max_possible_processors_count != 0) {
		arm_linux_processors_count = min(arm_linux_processors_count, max_possible_processors_count);
		valid_processor_mask |= CPUINFO_LINUX_FLAG_POSSIBLE;
	}
	if ((max_present_processors_count | max_possible_processors_count) == 0) {
		cpuinfo_log_error("failed to parse both lists of possible and present processors");
		return;
	}

	arm_linux_processors = calloc(arm_linux_processors_count, sizeof(struct cpuinfo_arm_linux_processor));
	if (arm_linux_processors == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " ARM logical processors",
			arm_linux_processors_count * sizeof(struct cpuinfo_arm_linux_processor),
			arm_linux_processors_count);
		return;
	}

	if (max_possible_processors_count) {
		cpuinfo_linux_detect_possible_processors(
			arm_linux_processors_count,
			&arm_linux_processors->flags,
			sizeof(struct cpuinfo_arm_linux_processor),
			CPUINFO_LINUX_FLAG_POSSIBLE);
	}

	if (max_present_processors_count) {
		cpuinfo_linux_detect_present_processors(
			arm_linux_processors_count,
			&arm_linux_processors->flags,
			sizeof(struct cpuinfo_arm_linux_processor),
			CPUINFO_LINUX_FLAG_PRESENT);
	}

#if defined(__ANDROID__)
	struct cpuinfo_android_properties android_properties;
	cpuinfo_arm_android_parse_properties(&android_properties);
#else
	char proc_cpuinfo_hardware[CPUINFO_HARDWARE_VALUE_MAX];
#endif
	char proc_cpuinfo_revision[CPUINFO_REVISION_VALUE_MAX];

	if (!cpuinfo_arm_linux_parse_proc_cpuinfo(
#if defined(__ANDROID__)
		    android_properties.proc_cpuinfo_hardware,
#else
		    proc_cpuinfo_hardware,
#endif
		    proc_cpuinfo_revision,
		    arm_linux_processors_count,
		    arm_linux_processors)) {
		cpuinfo_log_error("failed to parse processor information from /proc/cpuinfo");
		return;
	}

	for (uint32_t i = 0; i < arm_linux_processors_count; i++) {
		if (bitmask_all(arm_linux_processors[i].flags, valid_processor_mask)) {
			arm_linux_processors[i].flags |= CPUINFO_LINUX_FLAG_VALID;
			cpuinfo_log_debug(
				"parsed processor %" PRIu32 " MIDR 0x%08" PRIx32, i, arm_linux_processors[i].midr);
		}
	}

	uint32_t valid_processors = 0, last_midr = 0;
#if CPUINFO_ARCH_ARM
	uint32_t last_architecture_version = 0, last_architecture_flags = 0;
#endif
	for (uint32_t i = 0; i < arm_linux_processors_count; i++) {
		arm_linux_processors[i].system_processor_id = i;
		if (bitmask_all(arm_linux_processors[i].flags, CPUINFO_LINUX_FLAG_VALID)) {
			if (arm_linux_processors[i].flags & CPUINFO_ARM_LINUX_VALID_PROCESSOR) {
				/*
				 * Processor is in possible and present lists,
				 * and also reported in /proc/cpuinfo. This
				 * processor is availble for compute.
				 */
				valid_processors += 1;
			} else {
				/*
				 * Processor is in possible and present lists,
				 * but not reported in /proc/cpuinfo. This is
				 * fairly common: high-index processors can be
				 * not reported if they are offline.
				 */
				cpuinfo_log_info("processor %" PRIu32 " is not listed in /proc/cpuinfo", i);
			}

			if (bitmask_all(arm_linux_processors[i].flags, CPUINFO_ARM_LINUX_VALID_MIDR)) {
				last_midr = arm_linux_processors[i].midr;
			}
#if CPUINFO_ARCH_ARM
			if (bitmask_all(arm_linux_processors[i].flags, CPUINFO_ARM_LINUX_VALID_ARCHITECTURE)) {
				last_architecture_version = arm_linux_processors[i].architecture_version;
				last_architecture_flags = arm_linux_processors[i].architecture_flags;
			}
#endif
		} else {
			/* Processor reported in /proc/cpuinfo, but not in
			 * possible and/or present lists: log and ignore */
			if (!(arm_linux_processors[i].flags & CPUINFO_ARM_LINUX_VALID_PROCESSOR)) {
				cpuinfo_log_warning("invalid processor %" PRIu32 " reported in /proc/cpuinfo", i);
			}
		}
	}

#if defined(__ANDROID__)
	const struct cpuinfo_arm_chipset chipset =
		cpuinfo_arm_android_decode_chipset(&android_properties, valid_processors, 0);
#else
	const struct cpuinfo_arm_chipset chipset =
		cpuinfo_arm_linux_decode_chipset(proc_cpuinfo_hardware, proc_cpuinfo_revision, valid_processors, 0);
#endif

#if CPUINFO_ARCH_ARM
	uint32_t isa_features = 0;
	uint64_t isa_features2 = 0;
#ifdef __ANDROID__
	/*
	 * On Android before API 20, libc.so does not provide getauxval
	 * function. Thus, we try to dynamically find it, or use two fallback
	 * mechanisms:
	 * 1. dlopen libc.so, and try to find getauxval
	 * 2. Parse /proc/self/auxv procfs file
	 * 3. Use features reported in /proc/cpuinfo
	 */
	if (!cpuinfo_arm_linux_hwcap_from_getauxval(&isa_features, &isa_features2)) {
		/* getauxval can't be used, fall back to parsing /proc/self/auxv
		 */
		if (!cpuinfo_arm_linux_hwcap_from_procfs(&isa_features, &isa_features2)) {
			/*
			 * Reading /proc/self/auxv failed, probably due to file
			 * permissions. Use information from /proc/cpuinfo to
			 * detect ISA.
			 *
			 * If different processors report different ISA
			 * features, take the intersection.
			 */
			uint32_t processors_with_features = 0;
			for (uint32_t i = 0; i < arm_linux_processors_count; i++) {
				if (bitmask_all(
					    arm_linux_processors[i].flags,
					    CPUINFO_LINUX_FLAG_VALID | CPUINFO_ARM_LINUX_VALID_FEATURES)) {
					if (processors_with_features == 0) {
						isa_features = arm_linux_processors[i].features;
						isa_features2 = arm_linux_processors[i].features2;
					} else {
						isa_features &= arm_linux_processors[i].features;
						isa_features2 &= arm_linux_processors[i].features2;
					}
					processors_with_features += 1;
				}
			}
		}
	}
#else
	/* On GNU/Linux getauxval is always available */
	cpuinfo_arm_linux_hwcap_from_getauxval(&isa_features, &isa_features2);
#endif
	cpuinfo_arm_linux_decode_isa_from_proc_cpuinfo(
		isa_features,
		isa_features2,
		last_midr,
		last_architecture_version,
		last_architecture_flags,
		&chipset,
		&cpuinfo_isa);
#elif CPUINFO_ARCH_ARM64
	uint32_t isa_features = 0;
	uint64_t isa_features2 = 0;
	/* getauxval is always available on ARM64 Android */
	cpuinfo_arm_linux_hwcap_from_getauxval(&isa_features, &isa_features2);
	cpuinfo_arm64_linux_decode_isa_from_proc_cpuinfo(
		isa_features, isa_features2, last_midr, &chipset, &cpuinfo_isa);
#endif

	/* Detect min/max frequency and package ID */
	for (uint32_t i = 0; i < arm_linux_processors_count; i++) {
		if (bitmask_all(arm_linux_processors[i].flags, CPUINFO_LINUX_FLAG_VALID)) {
			const uint32_t max_frequency = cpuinfo_linux_get_processor_max_frequency(i);
			if (max_frequency != 0) {
				arm_linux_processors[i].max_frequency = max_frequency;
				arm_linux_processors[i].flags |= CPUINFO_LINUX_FLAG_MAX_FREQUENCY;
			}

			const uint32_t min_frequency = cpuinfo_linux_get_processor_min_frequency(i);
			if (min_frequency != 0) {
				arm_linux_processors[i].min_frequency = min_frequency;
				arm_linux_processors[i].flags |= CPUINFO_LINUX_FLAG_MIN_FREQUENCY;
			}

			if (cpuinfo_linux_get_processor_package_id(i, &arm_linux_processors[i].package_id)) {
				arm_linux_processors[i].flags |= CPUINFO_LINUX_FLAG_PACKAGE_ID;
			}
		}
	}

	/* Initialize topology group IDs */
	for (uint32_t i = 0; i < arm_linux_processors_count; i++) {
		arm_linux_processors[i].package_leader_id = i;
	}

	/* Propagate topology group IDs among siblings */
	bool detected_core_siblings_list_node = false;
	bool detected_cluster_cpus_list_node = false;
	for (uint32_t i = 0; i < arm_linux_processors_count; i++) {
		if (!bitmask_all(arm_linux_processors[i].flags, CPUINFO_LINUX_FLAG_VALID)) {
			continue;
		}

		if (!bitmask_all(arm_linux_processors[i].flags, CPUINFO_LINUX_FLAG_PACKAGE_ID)) {
			continue;
		}

		/* Use the cluster_cpus_list topology node if available. If not
		 * found, cache the result to avoid repeatedly attempting to
		 * read the non-existent paths.
		 * */
		if (!detected_core_siblings_list_node && !detected_cluster_cpus_list_node) {
			if (cpuinfo_linux_detect_cluster_cpus(
				    arm_linux_processors_count,
				    i,
				    (cpuinfo_siblings_callback)cluster_siblings_parser,
				    arm_linux_processors)) {
				detected_cluster_cpus_list_node = true;
				continue;
			} else {
				detected_core_siblings_list_node = true;
			}
		}

		/* The cached result above will guarantee only one of the blocks
		 * below will execute, with a bias towards cluster_cpus_list.
		 **/
		if (detected_core_siblings_list_node) {
			cpuinfo_linux_detect_core_siblings(
				arm_linux_processors_count,
				i,
				(cpuinfo_siblings_callback)cluster_siblings_parser,
				arm_linux_processors);
		}

		if (detected_cluster_cpus_list_node) {
			cpuinfo_linux_detect_cluster_cpus(
				arm_linux_processors_count,
				i,
				(cpuinfo_siblings_callback)cluster_siblings_parser,
				arm_linux_processors);
		}
	}

	/* Propagate all cluster IDs */
	uint32_t clustered_processors = 0;
	for (uint32_t i = 0; i < arm_linux_processors_count; i++) {
		if (bitmask_all(
			    arm_linux_processors[i].flags,
			    CPUINFO_LINUX_FLAG_VALID | CPUINFO_LINUX_FLAG_PACKAGE_CLUSTER)) {
			clustered_processors += 1;

			const uint32_t package_leader_id = arm_linux_processors[i].package_leader_id;
			if (package_leader_id < i) {
				arm_linux_processors[i].package_leader_id =
					arm_linux_processors[package_leader_id].package_leader_id;
			}

			cpuinfo_log_debug(
				"processor %" PRIu32 " clustered with processor %" PRIu32
				" as inferred from system siblings lists",
				i,
				arm_linux_processors[i].package_leader_id);
		}
	}

	if (clustered_processors != valid_processors) {
		/*
		 * Topology information about some or all logical processors may
		 * be unavailable, for the following reasons:
		 * - Linux kernel is too old, or configured without support for
		 * topology information in sysfs.
		 * - Core is offline, and Linux kernel is configured to not
		 * report topology for offline cores.
		 *
		 * In this case, we assign processors to clusters using two
		 * methods:
		 * - Try heuristic cluster configurations (e.g. 6-core SoC
		 * usually has 4+2 big.LITTLE configuration).
		 * - If heuristic failed, assign processors to core clusters in
		 * a sequential scan.
		 */
		if (!cpuinfo_arm_linux_detect_core_clusters_by_heuristic(
			    valid_processors, arm_linux_processors_count, arm_linux_processors)) {
			cpuinfo_arm_linux_detect_core_clusters_by_sequential_scan(
				arm_linux_processors_count, arm_linux_processors);
		}
	}

	cpuinfo_arm_linux_count_cluster_processors(arm_linux_processors_count, arm_linux_processors);

	const uint32_t cluster_count = cpuinfo_arm_linux_detect_cluster_midr(
		&chipset, arm_linux_processors_count, valid_processors, arm_linux_processors);

	/* Initialize core vendor, uarch, MIDR, and frequency for every logical
	 * processor */
	for (uint32_t i = 0; i < arm_linux_processors_count; i++) {
		if (bitmask_all(arm_linux_processors[i].flags, CPUINFO_LINUX_FLAG_VALID)) {
			const uint32_t cluster_leader = arm_linux_processors[i].package_leader_id;
			if (cluster_leader == i) {
				/* Cluster leader: decode core vendor and uarch
				 */
				cpuinfo_arm_decode_vendor_uarch(
					arm_linux_processors[cluster_leader].midr,
#if CPUINFO_ARCH_ARM
					!!(arm_linux_processors[cluster_leader].features &
					   CPUINFO_ARM_LINUX_FEATURE_VFPV4),
#endif
					&arm_linux_processors[cluster_leader].vendor,
					&arm_linux_processors[cluster_leader].uarch);
			} else {
				/* Cluster non-leader: copy vendor, uarch, MIDR,
				 * and frequency from cluster leader */
				arm_linux_processors[i].flags |= arm_linux_processors[cluster_leader].flags &
					(CPUINFO_ARM_LINUX_VALID_MIDR | CPUINFO_LINUX_FLAG_MAX_FREQUENCY);
				arm_linux_processors[i].midr = arm_linux_processors[cluster_leader].midr;
				arm_linux_processors[i].vendor = arm_linux_processors[cluster_leader].vendor;
				arm_linux_processors[i].uarch = arm_linux_processors[cluster_leader].uarch;
				arm_linux_processors[i].max_frequency =
					arm_linux_processors[cluster_leader].max_frequency;
			}
		}
	}

	for (uint32_t i = 0; i < arm_linux_processors_count; i++) {
		if (bitmask_all(arm_linux_processors[i].flags, CPUINFO_LINUX_FLAG_VALID)) {
			cpuinfo_log_debug(
				"post-analysis processor %" PRIu32 ": MIDR %08" PRIx32 " frequency %" PRIu32,
				i,
				arm_linux_processors[i].midr,
				arm_linux_processors[i].max_frequency);
		}
	}

	qsort(arm_linux_processors,
	      arm_linux_processors_count,
	      sizeof(struct cpuinfo_arm_linux_processor),
	      cmp_arm_linux_processor);

	for (uint32_t i = 0; i < arm_linux_processors_count; i++) {
		if (bitmask_all(arm_linux_processors[i].flags, CPUINFO_LINUX_FLAG_VALID)) {
			cpuinfo_log_debug(
				"post-sort processor %" PRIu32 ": system id %" PRIu32 " MIDR %08" PRIx32
				" frequency %" PRIu32,
				i,
				arm_linux_processors[i].system_processor_id,
				arm_linux_processors[i].midr,
				arm_linux_processors[i].max_frequency);
		}
	}

	uint32_t uarchs_count = 0;
	enum cpuinfo_uarch last_uarch;
	for (uint32_t i = 0; i < arm_linux_processors_count; i++) {
		if (bitmask_all(arm_linux_processors[i].flags, CPUINFO_LINUX_FLAG_VALID)) {
			if (uarchs_count == 0 || arm_linux_processors[i].uarch != last_uarch) {
				last_uarch = arm_linux_processors[i].uarch;
				uarchs_count += 1;
			}
			arm_linux_processors[i].uarch_index = uarchs_count - 1;
		}
	}

	/*
	 * Assumptions:
	 * - No SMP (i.e. each core supports only one hardware thread).
	 * - Level 1 instruction and data caches are private to the core
	 * clusters.
	 * - Level 2 and level 3 cache is shared between cores in the same
	 * cluster.
	 */
	cpuinfo_arm_chipset_to_string(&chipset, package.name);
	package.processor_count = valid_processors;
	package.core_count = valid_processors;
	package.cluster_count = cluster_count;

	processors = calloc(valid_processors, sizeof(struct cpuinfo_processor));
	if (processors == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " logical processors",
			valid_processors * sizeof(struct cpuinfo_processor),
			valid_processors);
		goto cleanup;
	}

	cores = calloc(valid_processors, sizeof(struct cpuinfo_core));
	if (cores == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " cores",
			valid_processors * sizeof(struct cpuinfo_core),
			valid_processors);
		goto cleanup;
	}

	clusters = calloc(cluster_count, sizeof(struct cpuinfo_cluster));
	if (clusters == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " core clusters",
			cluster_count * sizeof(struct cpuinfo_cluster),
			cluster_count);
		goto cleanup;
	}

	uarchs = calloc(uarchs_count, sizeof(struct cpuinfo_uarch_info));
	if (uarchs == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " microarchitectures",
			uarchs_count * sizeof(struct cpuinfo_uarch_info),
			uarchs_count);
		goto cleanup;
	}

	linux_cpu_to_processor_map = calloc(arm_linux_processors_count, sizeof(struct cpuinfo_processor*));
	if (linux_cpu_to_processor_map == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for %" PRIu32 " logical processor mapping entries",
			arm_linux_processors_count * sizeof(struct cpuinfo_processor*),
			arm_linux_processors_count);
		goto cleanup;
	}

	linux_cpu_to_core_map = calloc(arm_linux_processors_count, sizeof(struct cpuinfo_core*));
	if (linux_cpu_to_core_map == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for %" PRIu32 " core mapping entries",
			arm_linux_processors_count * sizeof(struct cpuinfo_core*),
			arm_linux_processors_count);
		goto cleanup;
	}

	if (uarchs_count > 1) {
		linux_cpu_to_uarch_index_map = calloc(arm_linux_processors_count, sizeof(uint32_t));
		if (linux_cpu_to_uarch_index_map == NULL) {
			cpuinfo_log_error(
				"failed to allocate %zu bytes for %" PRIu32 " uarch index mapping entries",
				arm_linux_processors_count * sizeof(uint32_t),
				arm_linux_processors_count);
			goto cleanup;
		}
	}

	l1i = calloc(valid_processors, sizeof(struct cpuinfo_cache));
	if (l1i == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " L1I caches",
			valid_processors * sizeof(struct cpuinfo_cache),
			valid_processors);
		goto cleanup;
	}

	l1d = calloc(valid_processors, sizeof(struct cpuinfo_cache));
	if (l1d == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " L1D caches",
			valid_processors * sizeof(struct cpuinfo_cache),
			valid_processors);
		goto cleanup;
	}

	uint32_t uarchs_index = 0;
	for (uint32_t i = 0; i < arm_linux_processors_count; i++) {
		if (bitmask_all(arm_linux_processors[i].flags, CPUINFO_LINUX_FLAG_VALID)) {
			if (uarchs_index == 0 || arm_linux_processors[i].uarch != last_uarch) {
				last_uarch = arm_linux_processors[i].uarch;
				uarchs[uarchs_index] = (struct cpuinfo_uarch_info){
					.uarch = arm_linux_processors[i].uarch,
					.midr = arm_linux_processors[i].midr,
				};
				uarchs_index += 1;
			}
			uarchs[uarchs_index - 1].processor_count += 1;
			uarchs[uarchs_index - 1].core_count += 1;
		}
	}

	uint32_t l2_count = 0, l3_count = 0, big_l3_size = 0, cluster_id = UINT32_MAX;
	/* Indication whether L3 (if it exists) is shared between all cores */
	bool shared_l3 = true;
	/* Populate cache information structures in l1i, l1d */
	for (uint32_t i = 0; i < valid_processors; i++) {
		if (arm_linux_processors[i].package_leader_id == arm_linux_processors[i].system_processor_id) {
			cluster_id += 1;
			clusters[cluster_id] = (struct cpuinfo_cluster){
				.processor_start = i,
				.processor_count = arm_linux_processors[i].package_processor_count,
				.core_start = i,
				.core_count = arm_linux_processors[i].package_processor_count,
				.cluster_id = cluster_id,
				.package = &package,
				.vendor = arm_linux_processors[i].vendor,
				.uarch = arm_linux_processors[i].uarch,
				.midr = arm_linux_processors[i].midr,
			};
		}

		processors[i].smt_id = 0;
		processors[i].core = cores + i;
		processors[i].cluster = clusters + cluster_id;
		processors[i].package = &package;
		processors[i].linux_id = (int)arm_linux_processors[i].system_processor_id;
		processors[i].cache.l1i = l1i + i;
		processors[i].cache.l1d = l1d + i;
		linux_cpu_to_processor_map[arm_linux_processors[i].system_processor_id] = &processors[i];

		cores[i].processor_start = i;
		cores[i].processor_count = 1;
		cores[i].core_id = i;
		cores[i].cluster = clusters + cluster_id;
		cores[i].package = &package;
		cores[i].vendor = arm_linux_processors[i].vendor;
		cores[i].uarch = arm_linux_processors[i].uarch;
		cores[i].midr = arm_linux_processors[i].midr;
		linux_cpu_to_core_map[arm_linux_processors[i].system_processor_id] = &cores[i];

		if (linux_cpu_to_uarch_index_map != NULL) {
			linux_cpu_to_uarch_index_map[arm_linux_processors[i].system_processor_id] =
				arm_linux_processors[i].uarch_index;
		}

		struct cpuinfo_cache temp_l2 = {0}, temp_l3 = {0};
		cpuinfo_arm_decode_cache(
			arm_linux_processors[i].uarch,
			arm_linux_processors[i].package_processor_count,
			arm_linux_processors[i].midr,
			&chipset,
			cluster_id,
			arm_linux_processors[i].architecture_version,
			&l1i[i],
			&l1d[i],
			&temp_l2,
			&temp_l3);
		l1i[i].processor_start = l1d[i].processor_start = i;
		l1i[i].processor_count = l1d[i].processor_count = 1;
#if CPUINFO_ARCH_ARM
		/* L1I reported in /proc/cpuinfo overrides defaults */
		if (bitmask_all(arm_linux_processors[i].flags, CPUINFO_ARM_LINUX_VALID_ICACHE)) {
			l1i[i] = (struct cpuinfo_cache){
				.size = arm_linux_processors[i].proc_cpuinfo_cache.i_size,
				.associativity = arm_linux_processors[i].proc_cpuinfo_cache.i_assoc,
				.sets = arm_linux_processors[i].proc_cpuinfo_cache.i_sets,
				.partitions = 1,
				.line_size = arm_linux_processors[i].proc_cpuinfo_cache.i_line_length};
		}
		/* L1D reported in /proc/cpuinfo overrides defaults */
		if (bitmask_all(arm_linux_processors[i].flags, CPUINFO_ARM_LINUX_VALID_DCACHE)) {
			l1d[i] = (struct cpuinfo_cache){
				.size = arm_linux_processors[i].proc_cpuinfo_cache.d_size,
				.associativity = arm_linux_processors[i].proc_cpuinfo_cache.d_assoc,
				.sets = arm_linux_processors[i].proc_cpuinfo_cache.d_sets,
				.partitions = 1,
				.line_size = arm_linux_processors[i].proc_cpuinfo_cache.d_line_length};
		}
#endif

		if (temp_l3.size != 0) {
			/*
			 * Assumptions:
			 * - L2 is private to each core
			 * - L3 is shared by cores in the same cluster
			 * - If cores in different clusters report the same L3,
			 * it is shared between all cores.
			 */
			l2_count += 1;
			if (arm_linux_processors[i].package_leader_id == arm_linux_processors[i].system_processor_id) {
				if (cluster_id == 0) {
					big_l3_size = temp_l3.size;
					l3_count = 1;
				} else if (temp_l3.size != big_l3_size) {
					/* If some cores have different L3 size,
					 * L3 is not shared between all cores */
					shared_l3 = false;
					l3_count += 1;
				}
			}
		} else {
			/* If some cores don't have L3 cache, L3 is not shared
			 * between all cores
			 */
			shared_l3 = false;
			if (temp_l2.size != 0) {
				/* Assume L2 is shared by cores in the same
				 * cluster */
				if (arm_linux_processors[i].package_leader_id ==
				    arm_linux_processors[i].system_processor_id) {
					l2_count += 1;
				}
			}
		}
	}

	if (l2_count != 0) {
		l2 = calloc(l2_count, sizeof(struct cpuinfo_cache));
		if (l2 == NULL) {
			cpuinfo_log_error(
				"failed to allocate %zu bytes for descriptions of %" PRIu32 " L2 caches",
				l2_count * sizeof(struct cpuinfo_cache),
				l2_count);
			goto cleanup;
		}

		if (l3_count != 0) {
			l3 = calloc(l3_count, sizeof(struct cpuinfo_cache));
			if (l3 == NULL) {
				cpuinfo_log_error(
					"failed to allocate %zu bytes for descriptions of %" PRIu32 " L3 caches",
					l3_count * sizeof(struct cpuinfo_cache),
					l3_count);
				goto cleanup;
			}
		}
	}

	cluster_id = UINT32_MAX;
	uint32_t l2_index = UINT32_MAX, l3_index = UINT32_MAX;
	for (uint32_t i = 0; i < valid_processors; i++) {
		if (arm_linux_processors[i].package_leader_id == arm_linux_processors[i].system_processor_id) {
			cluster_id++;
		}

		struct cpuinfo_cache dummy_l1i, dummy_l1d, temp_l2 = {0}, temp_l3 = {0};
		cpuinfo_arm_decode_cache(
			arm_linux_processors[i].uarch,
			arm_linux_processors[i].package_processor_count,
			arm_linux_processors[i].midr,
			&chipset,
			cluster_id,
			arm_linux_processors[i].architecture_version,
			&dummy_l1i,
			&dummy_l1d,
			&temp_l2,
			&temp_l3);

		if (temp_l3.size != 0) {
			/*
			 * Assumptions:
			 * - L2 is private to each core
			 * - L3 is shared by cores in the same cluster
			 * - If cores in different clusters report the same L3,
			 * it is shared between all cores.
			 */
			l2_index += 1;
			l2[l2_index] = (struct cpuinfo_cache){
				.size = temp_l2.size,
				.associativity = temp_l2.associativity,
				.sets = temp_l2.sets,
				.partitions = 1,
				.line_size = temp_l2.line_size,
				.flags = temp_l2.flags,
				.processor_start = i,
				.processor_count = 1,
			};
			processors[i].cache.l2 = l2 + l2_index;
			if (arm_linux_processors[i].package_leader_id == arm_linux_processors[i].system_processor_id) {
				l3_index += 1;
				if (l3_index < l3_count) {
					l3[l3_index] = (struct cpuinfo_cache){
						.size = temp_l3.size,
						.associativity = temp_l3.associativity,
						.sets = temp_l3.sets,
						.partitions = 1,
						.line_size = temp_l3.line_size,
						.flags = temp_l3.flags,
						.processor_start = i,
						.processor_count = shared_l3
							? valid_processors
							: arm_linux_processors[i].package_processor_count,
					};
				}
			}
			if (shared_l3) {
				processors[i].cache.l3 = l3;
			} else if (l3_index < l3_count) {
				processors[i].cache.l3 = l3 + l3_index;
			}
		} else if (temp_l2.size != 0) {
			/* Assume L2 is shared by cores in the same cluster */
			if (arm_linux_processors[i].package_leader_id == arm_linux_processors[i].system_processor_id) {
				l2_index += 1;
				l2[l2_index] = (struct cpuinfo_cache){
					.size = temp_l2.size,
					.associativity = temp_l2.associativity,
					.sets = temp_l2.sets,
					.partitions = 1,
					.line_size = temp_l2.line_size,
					.flags = temp_l2.flags,
					.processor_start = i,
					.processor_count = arm_linux_processors[i].package_processor_count,
				};
			}
			processors[i].cache.l2 = l2 + l2_index;
		}
	}

	/* Commit */
	cpuinfo_processors = processors;
	cpuinfo_cores = cores;
	cpuinfo_clusters = clusters;
	cpuinfo_packages = &package;
	cpuinfo_uarchs = uarchs;
	cpuinfo_cache[cpuinfo_cache_level_1i] = l1i;
	cpuinfo_cache[cpuinfo_cache_level_1d] = l1d;
	cpuinfo_cache[cpuinfo_cache_level_2] = l2;
	cpuinfo_cache[cpuinfo_cache_level_3] = l3;

	cpuinfo_processors_count = valid_processors;
	cpuinfo_cores_count = valid_processors;
	cpuinfo_clusters_count = cluster_count;
	cpuinfo_packages_count = 1;
	cpuinfo_uarchs_count = uarchs_count;
	cpuinfo_cache_count[cpuinfo_cache_level_1i] = valid_processors;
	cpuinfo_cache_count[cpuinfo_cache_level_1d] = valid_processors;
	cpuinfo_cache_count[cpuinfo_cache_level_2] = l2_count;
	cpuinfo_cache_count[cpuinfo_cache_level_3] = l3_count;
	cpuinfo_max_cache_size = cpuinfo_arm_compute_max_cache_size(&processors[0]);

	cpuinfo_linux_cpu_max = arm_linux_processors_count;
	cpuinfo_linux_cpu_to_processor_map = linux_cpu_to_processor_map;
	cpuinfo_linux_cpu_to_core_map = linux_cpu_to_core_map;
	cpuinfo_linux_cpu_to_uarch_index_map = linux_cpu_to_uarch_index_map;

	__sync_synchronize();

	cpuinfo_is_initialized = true;

	processors = NULL;
	cores = NULL;
	clusters = NULL;
	uarchs = NULL;
	l1i = l1d = l2 = l3 = NULL;
	linux_cpu_to_processor_map = NULL;
	linux_cpu_to_core_map = NULL;
	linux_cpu_to_uarch_index_map = NULL;

cleanup:
	free(arm_linux_processors);
	free(processors);
	free(cores);
	free(clusters);
	free(uarchs);
	free(l1i);
	free(l1d);
	free(l2);
	free(l3);
	free(linux_cpu_to_processor_map);
	free(linux_cpu_to_core_map);
	free(linux_cpu_to_uarch_index_map);
}
