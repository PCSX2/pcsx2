#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <cpuinfo.h>
#include <cpuinfo/internal-api.h>
#include <cpuinfo/log.h>
#include <linux/api.h>
#include <x86/api.h>
#include <x86/linux/api.h>

static inline uint32_t bit_mask(uint32_t bits) {
	return (UINT32_C(1) << bits) - UINT32_C(1);
}

static inline bool bitmask_all(uint32_t bitfield, uint32_t mask) {
	return (bitfield & mask) == mask;
}

static inline uint32_t min(uint32_t a, uint32_t b) {
	return a < b ? a : b;
}

static inline int cmp(uint32_t a, uint32_t b) {
	return (a > b) - (a < b);
}

static int cmp_x86_linux_processor(const void* ptr_a, const void* ptr_b) {
	const struct cpuinfo_x86_linux_processor* processor_a = (const struct cpuinfo_x86_linux_processor*)ptr_a;
	const struct cpuinfo_x86_linux_processor* processor_b = (const struct cpuinfo_x86_linux_processor*)ptr_b;

	/* Move usable processors towards the start of the array */
	const bool usable_a = bitmask_all(processor_a->flags, CPUINFO_LINUX_FLAG_VALID);
	const bool usable_b = bitmask_all(processor_b->flags, CPUINFO_LINUX_FLAG_VALID);
	if (usable_a != usable_b) {
		return (int)usable_b - (int)usable_a;
	}

	/* Compare based on APIC ID (i.e. processor 0 < processor 1) */
	const uint32_t id_a = processor_a->apic_id;
	const uint32_t id_b = processor_b->apic_id;
	return cmp(id_a, id_b);
}

static void cpuinfo_x86_count_objects(
	uint32_t linux_processors_count,
	const struct cpuinfo_x86_linux_processor linux_processors[restrict static linux_processors_count],
	const struct cpuinfo_x86_processor processor[restrict static 1],
	uint32_t valid_processor_mask,
	uint32_t llc_apic_bits,
	uint32_t cores_count_ptr[restrict static 1],
	uint32_t clusters_count_ptr[restrict static 1],
	uint32_t packages_count_ptr[restrict static 1],
	uint32_t l1i_count_ptr[restrict static 1],
	uint32_t l1d_count_ptr[restrict static 1],
	uint32_t l2_count_ptr[restrict static 1],
	uint32_t l3_count_ptr[restrict static 1],
	uint32_t l4_count_ptr[restrict static 1]) {
	const uint32_t core_apic_mask =
		~(bit_mask(processor->topology.thread_bits_length) << processor->topology.thread_bits_offset);
	const uint32_t package_apic_mask = core_apic_mask &
		~(bit_mask(processor->topology.core_bits_length) << processor->topology.core_bits_offset);
	const uint32_t llc_apic_mask = ~bit_mask(llc_apic_bits);
	const uint32_t cluster_apic_mask = package_apic_mask | llc_apic_mask;

	uint32_t cores_count = 0, clusters_count = 0, packages_count = 0;
	uint32_t l1i_count = 0, l1d_count = 0, l2_count = 0, l3_count = 0, l4_count = 0;
	uint32_t last_core_id = UINT32_MAX, last_cluster_id = UINT32_MAX, last_package_id = UINT32_MAX;
	uint32_t last_l1i_id = UINT32_MAX, last_l1d_id = UINT32_MAX;
	uint32_t last_l2_id = UINT32_MAX, last_l3_id = UINT32_MAX, last_l4_id = UINT32_MAX;
	for (uint32_t i = 0; i < linux_processors_count; i++) {
		if (bitmask_all(linux_processors[i].flags, valid_processor_mask)) {
			const uint32_t apic_id = linux_processors[i].apic_id;
			cpuinfo_log_debug(
				"APID ID %" PRIu32 ": system processor %" PRIu32,
				apic_id,
				linux_processors[i].linux_id);

			/* All bits of APIC ID except thread ID mask */
			const uint32_t core_id = apic_id & core_apic_mask;
			if (core_id != last_core_id) {
				last_core_id = core_id;
				cores_count++;
			}
			/* All bits of APIC ID except thread ID and core ID
			 * masks */
			const uint32_t package_id = apic_id & package_apic_mask;
			if (package_id != last_package_id) {
				last_package_id = package_id;
				packages_count++;
			}
			/* Bits of APIC ID which are part of either LLC or
			 * package ID mask */
			const uint32_t cluster_id = apic_id & cluster_apic_mask;
			if (cluster_id != last_cluster_id) {
				last_cluster_id = cluster_id;
				clusters_count++;
			}
			if (processor->cache.l1i.size != 0) {
				const uint32_t l1i_id = apic_id & ~bit_mask(processor->cache.l1i.apic_bits);
				if (l1i_id != last_l1i_id) {
					last_l1i_id = l1i_id;
					l1i_count++;
				}
			}
			if (processor->cache.l1d.size != 0) {
				const uint32_t l1d_id = apic_id & ~bit_mask(processor->cache.l1d.apic_bits);
				if (l1d_id != last_l1d_id) {
					last_l1d_id = l1d_id;
					l1d_count++;
				}
			}
			if (processor->cache.l2.size != 0) {
				const uint32_t l2_id = apic_id & ~bit_mask(processor->cache.l2.apic_bits);
				if (l2_id != last_l2_id) {
					last_l2_id = l2_id;
					l2_count++;
				}
			}
			if (processor->cache.l3.size != 0) {
				const uint32_t l3_id = apic_id & ~bit_mask(processor->cache.l3.apic_bits);
				if (l3_id != last_l3_id) {
					last_l3_id = l3_id;
					l3_count++;
				}
			}
			if (processor->cache.l4.size != 0) {
				const uint32_t l4_id = apic_id & ~bit_mask(processor->cache.l4.apic_bits);
				if (l4_id != last_l4_id) {
					last_l4_id = l4_id;
					l4_count++;
				}
			}
		}
	}
	*cores_count_ptr = cores_count;
	*clusters_count_ptr = clusters_count;
	*packages_count_ptr = packages_count;
	*l1i_count_ptr = l1i_count;
	*l1d_count_ptr = l1d_count;
	*l2_count_ptr = l2_count;
	*l3_count_ptr = l3_count;
	*l4_count_ptr = l4_count;
}

void cpuinfo_x86_linux_init(void) {
	struct cpuinfo_x86_linux_processor* x86_linux_processors = NULL;
	struct cpuinfo_processor* processors = NULL;
	struct cpuinfo_core* cores = NULL;
	struct cpuinfo_cluster* clusters = NULL;
	struct cpuinfo_package* packages = NULL;
	const struct cpuinfo_processor** linux_cpu_to_processor_map = NULL;
	const struct cpuinfo_core** linux_cpu_to_core_map = NULL;
	struct cpuinfo_cache* l1i = NULL;
	struct cpuinfo_cache* l1d = NULL;
	struct cpuinfo_cache* l2 = NULL;
	struct cpuinfo_cache* l3 = NULL;
	struct cpuinfo_cache* l4 = NULL;

	const uint32_t max_processors_count = cpuinfo_linux_get_max_processors_count();
	cpuinfo_log_debug("system maximum processors count: %" PRIu32, max_processors_count);

	const uint32_t max_possible_processors_count =
		1 + cpuinfo_linux_get_max_possible_processor(max_processors_count);
	cpuinfo_log_debug("maximum possible processors count: %" PRIu32, max_possible_processors_count);
	const uint32_t max_present_processors_count = 1 + cpuinfo_linux_get_max_present_processor(max_processors_count);
	cpuinfo_log_debug("maximum present processors count: %" PRIu32, max_present_processors_count);

	uint32_t valid_processor_mask = 0;
	uint32_t x86_linux_processors_count = max_processors_count;
	if (max_present_processors_count != 0) {
		x86_linux_processors_count = min(x86_linux_processors_count, max_present_processors_count);
		valid_processor_mask = CPUINFO_LINUX_FLAG_PRESENT;
	} else {
		valid_processor_mask = CPUINFO_LINUX_FLAG_PROC_CPUINFO;
	}
	if (max_possible_processors_count != 0) {
		x86_linux_processors_count = min(x86_linux_processors_count, max_possible_processors_count);
		valid_processor_mask |= CPUINFO_LINUX_FLAG_POSSIBLE;
	}

	x86_linux_processors = calloc(x86_linux_processors_count, sizeof(struct cpuinfo_x86_linux_processor));
	if (x86_linux_processors == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " x86 logical processors",
			x86_linux_processors_count * sizeof(struct cpuinfo_x86_linux_processor),
			x86_linux_processors_count);
		return;
	}

	if (max_possible_processors_count != 0) {
		cpuinfo_linux_detect_possible_processors(
			x86_linux_processors_count,
			&x86_linux_processors->flags,
			sizeof(struct cpuinfo_x86_linux_processor),
			CPUINFO_LINUX_FLAG_POSSIBLE);
	}

	if (max_present_processors_count != 0) {
		cpuinfo_linux_detect_present_processors(
			x86_linux_processors_count,
			&x86_linux_processors->flags,
			sizeof(struct cpuinfo_x86_linux_processor),
			CPUINFO_LINUX_FLAG_PRESENT);
	}

	if (!cpuinfo_x86_linux_parse_proc_cpuinfo(x86_linux_processors_count, x86_linux_processors)) {
		cpuinfo_log_error("failed to parse processor information from /proc/cpuinfo");
		return;
	}

	for (uint32_t i = 0; i < x86_linux_processors_count; i++) {
		if (bitmask_all(x86_linux_processors[i].flags, valid_processor_mask)) {
			x86_linux_processors[i].flags |= CPUINFO_LINUX_FLAG_VALID;
		}
	}

	struct cpuinfo_x86_processor x86_processor;
	memset(&x86_processor, 0, sizeof(x86_processor));
	cpuinfo_x86_init_processor(&x86_processor);
	char brand_string[48];
	cpuinfo_x86_normalize_brand_string(x86_processor.brand_string, brand_string);

	uint32_t processors_count = 0;
	for (uint32_t i = 0; i < x86_linux_processors_count; i++) {
		if (bitmask_all(x86_linux_processors[i].flags, CPUINFO_LINUX_FLAG_VALID)) {
			x86_linux_processors[i].linux_id = i;
			processors_count++;
		}
	}

	qsort(x86_linux_processors,
	      x86_linux_processors_count,
	      sizeof(struct cpuinfo_x86_linux_processor),
	      cmp_x86_linux_processor);

	processors = calloc(processors_count, sizeof(struct cpuinfo_processor));
	if (processors == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " logical processors",
			processors_count * sizeof(struct cpuinfo_processor),
			processors_count);
		goto cleanup;
	}

	uint32_t llc_apic_bits = 0;
	if (x86_processor.cache.l4.size != 0) {
		llc_apic_bits = x86_processor.cache.l4.apic_bits;
	} else if (x86_processor.cache.l3.size != 0) {
		llc_apic_bits = x86_processor.cache.l3.apic_bits;
	} else if (x86_processor.cache.l2.size != 0) {
		llc_apic_bits = x86_processor.cache.l2.apic_bits;
	} else if (x86_processor.cache.l1d.size != 0) {
		llc_apic_bits = x86_processor.cache.l1d.apic_bits;
	}
	uint32_t packages_count = 0, clusters_count = 0, cores_count = 0;
	uint32_t l1i_count = 0, l1d_count = 0, l2_count = 0, l3_count = 0, l4_count = 0;
	cpuinfo_x86_count_objects(
		x86_linux_processors_count,
		x86_linux_processors,
		&x86_processor,
		valid_processor_mask,
		llc_apic_bits,
		&cores_count,
		&clusters_count,
		&packages_count,
		&l1i_count,
		&l1d_count,
		&l2_count,
		&l3_count,
		&l4_count);

	cpuinfo_log_debug("detected %" PRIu32 " cores", cores_count);
	cpuinfo_log_debug("detected %" PRIu32 " clusters", clusters_count);
	cpuinfo_log_debug("detected %" PRIu32 " packages", packages_count);
	cpuinfo_log_debug("detected %" PRIu32 " L1I caches", l1i_count);
	cpuinfo_log_debug("detected %" PRIu32 " L1D caches", l1d_count);
	cpuinfo_log_debug("detected %" PRIu32 " L2 caches", l2_count);
	cpuinfo_log_debug("detected %" PRIu32 " L3 caches", l3_count);
	cpuinfo_log_debug("detected %" PRIu32 " L4 caches", l4_count);

	linux_cpu_to_processor_map = calloc(x86_linux_processors_count, sizeof(struct cpuinfo_processor*));
	if (linux_cpu_to_processor_map == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for mapping entries of %" PRIu32 " logical processors",
			x86_linux_processors_count * sizeof(struct cpuinfo_processor*),
			x86_linux_processors_count);
		goto cleanup;
	}

	linux_cpu_to_core_map = calloc(x86_linux_processors_count, sizeof(struct cpuinfo_core*));
	if (linux_cpu_to_core_map == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for mapping entries of %" PRIu32 " cores",
			x86_linux_processors_count * sizeof(struct cpuinfo_core*),
			x86_linux_processors_count);
		goto cleanup;
	}

	cores = calloc(cores_count, sizeof(struct cpuinfo_core));
	if (cores == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " cores",
			cores_count * sizeof(struct cpuinfo_core),
			cores_count);
		goto cleanup;
	}

	clusters = calloc(clusters_count, sizeof(struct cpuinfo_cluster));
	if (clusters == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " core clusters",
			clusters_count * sizeof(struct cpuinfo_cluster),
			clusters_count);
		goto cleanup;
	}

	packages = calloc(packages_count, sizeof(struct cpuinfo_package));
	if (packages == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " physical packages",
			packages_count * sizeof(struct cpuinfo_package),
			packages_count);
		goto cleanup;
	}

	if (l1i_count != 0) {
		l1i = calloc(l1i_count, sizeof(struct cpuinfo_cache));
		if (l1i == NULL) {
			cpuinfo_log_error(
				"failed to allocate %zu bytes for descriptions of %" PRIu32 " L1I caches",
				l1i_count * sizeof(struct cpuinfo_cache),
				l1i_count);
			goto cleanup;
		}
	}
	if (l1d_count != 0) {
		l1d = calloc(l1d_count, sizeof(struct cpuinfo_cache));
		if (l1d == NULL) {
			cpuinfo_log_error(
				"failed to allocate %zu bytes for descriptions of %" PRIu32 " L1D caches",
				l1d_count * sizeof(struct cpuinfo_cache),
				l1d_count);
			goto cleanup;
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
	if (l4_count != 0) {
		l4 = calloc(l4_count, sizeof(struct cpuinfo_cache));
		if (l4 == NULL) {
			cpuinfo_log_error(
				"failed to allocate %zu bytes for descriptions of %" PRIu32 " L4 caches",
				l4_count * sizeof(struct cpuinfo_cache),
				l4_count);
			goto cleanup;
		}
	}

	const uint32_t core_apic_mask =
		~(bit_mask(x86_processor.topology.thread_bits_length) << x86_processor.topology.thread_bits_offset);
	const uint32_t package_apic_mask = core_apic_mask &
		~(bit_mask(x86_processor.topology.core_bits_length) << x86_processor.topology.core_bits_offset);
	const uint32_t llc_apic_mask = ~bit_mask(llc_apic_bits);
	const uint32_t cluster_apic_mask = package_apic_mask | llc_apic_mask;

	uint32_t processor_index = UINT32_MAX, core_index = UINT32_MAX, cluster_index = UINT32_MAX,
		 package_index = UINT32_MAX;
	uint32_t l1i_index = UINT32_MAX, l1d_index = UINT32_MAX, l2_index = UINT32_MAX, l3_index = UINT32_MAX,
		 l4_index = UINT32_MAX;
	uint32_t cluster_id = 0, core_id = 0, smt_id = 0;
	uint32_t last_apic_core_id = UINT32_MAX, last_apic_cluster_id = UINT32_MAX, last_apic_package_id = UINT32_MAX;
	uint32_t last_l1i_id = UINT32_MAX, last_l1d_id = UINT32_MAX;
	uint32_t last_l2_id = UINT32_MAX, last_l3_id = UINT32_MAX, last_l4_id = UINT32_MAX;
	for (uint32_t i = 0; i < x86_linux_processors_count; i++) {
		if (bitmask_all(x86_linux_processors[i].flags, CPUINFO_LINUX_FLAG_VALID)) {
			const uint32_t apic_id = x86_linux_processors[i].apic_id;
			processor_index++;
			smt_id++;

			/* All bits of APIC ID except thread ID mask */
			const uint32_t apid_core_id = apic_id & core_apic_mask;
			if (apid_core_id != last_apic_core_id) {
				core_index++;
				core_id++;
				smt_id = 0;
			}
			/* Bits of APIC ID which are part of either LLC or
			 * package ID mask */
			const uint32_t apic_cluster_id = apic_id & cluster_apic_mask;
			if (apic_cluster_id != last_apic_cluster_id) {
				cluster_index++;
				cluster_id++;
			}
			/* All bits of APIC ID except thread ID and core ID
			 * masks */
			const uint32_t apic_package_id = apic_id & package_apic_mask;
			if (apic_package_id != last_apic_package_id) {
				package_index++;
				core_id = 0;
				cluster_id = 0;
			}

			/* Initialize logical processor object */
			processors[processor_index].smt_id = smt_id;
			processors[processor_index].core = cores + core_index;
			processors[processor_index].cluster = clusters + cluster_index;
			processors[processor_index].package = packages + package_index;
			processors[processor_index].linux_id = x86_linux_processors[i].linux_id;
			processors[processor_index].apic_id = x86_linux_processors[i].apic_id;

			if (apid_core_id != last_apic_core_id) {
				/* new core */
				cores[core_index] = (struct cpuinfo_core){
					.processor_start = processor_index,
					.processor_count = 1,
					.core_id = core_id,
					.cluster = clusters + cluster_index,
					.package = packages + package_index,
					.vendor = x86_processor.vendor,
					.uarch = x86_processor.uarch,
					.cpuid = x86_processor.cpuid,
				};
				clusters[cluster_index].core_count += 1;
				packages[package_index].core_count += 1;
				last_apic_core_id = apid_core_id;
			} else {
				/* another logical processor on the same core */
				cores[core_index].processor_count++;
			}

			if (apic_cluster_id != last_apic_cluster_id) {
				/* new cluster */
				clusters[cluster_index].processor_start = processor_index;
				clusters[cluster_index].processor_count = 1;
				clusters[cluster_index].core_start = core_index;
				clusters[cluster_index].cluster_id = cluster_id;
				clusters[cluster_index].package = packages + package_index;
				clusters[cluster_index].vendor = x86_processor.vendor;
				clusters[cluster_index].uarch = x86_processor.uarch;
				clusters[cluster_index].cpuid = x86_processor.cpuid;
				packages[package_index].cluster_count += 1;
				last_apic_cluster_id = apic_cluster_id;
			} else {
				/* another logical processor on the same cluster
				 */
				clusters[cluster_index].processor_count++;
			}

			if (apic_package_id != last_apic_package_id) {
				/* new package */
				packages[package_index].processor_start = processor_index;
				packages[package_index].processor_count = 1;
				packages[package_index].core_start = core_index;
				packages[package_index].cluster_start = cluster_index;
				cpuinfo_x86_format_package_name(
					x86_processor.vendor, brand_string, packages[package_index].name);
				last_apic_package_id = apic_package_id;
			} else {
				/* another logical processor on the same package
				 */
				packages[package_index].processor_count++;
			}

			linux_cpu_to_processor_map[x86_linux_processors[i].linux_id] = processors + processor_index;
			linux_cpu_to_core_map[x86_linux_processors[i].linux_id] = cores + core_index;

			if (x86_processor.cache.l1i.size != 0) {
				const uint32_t l1i_id = apic_id & ~bit_mask(x86_processor.cache.l1i.apic_bits);
				processors[i].cache.l1i = &l1i[l1i_index];
				if (l1i_id != last_l1i_id) {
					/* new cache */
					last_l1i_id = l1i_id;
					l1i[++l1i_index] = (struct cpuinfo_cache){
						.size = x86_processor.cache.l1i.size,
						.associativity = x86_processor.cache.l1i.associativity,
						.sets = x86_processor.cache.l1i.sets,
						.partitions = x86_processor.cache.l1i.partitions,
						.line_size = x86_processor.cache.l1i.line_size,
						.flags = x86_processor.cache.l1i.flags,
						.processor_start = processor_index,
						.processor_count = 1,
					};
				} else {
					/* another processor sharing the same
					 * cache */
					l1i[l1i_index].processor_count += 1;
				}
				processors[i].cache.l1i = &l1i[l1i_index];
			} else {
				/* reset cache id */
				last_l1i_id = UINT32_MAX;
			}
			if (x86_processor.cache.l1d.size != 0) {
				const uint32_t l1d_id = apic_id & ~bit_mask(x86_processor.cache.l1d.apic_bits);
				processors[i].cache.l1d = &l1d[l1d_index];
				if (l1d_id != last_l1d_id) {
					/* new cache */
					last_l1d_id = l1d_id;
					l1d[++l1d_index] = (struct cpuinfo_cache){
						.size = x86_processor.cache.l1d.size,
						.associativity = x86_processor.cache.l1d.associativity,
						.sets = x86_processor.cache.l1d.sets,
						.partitions = x86_processor.cache.l1d.partitions,
						.line_size = x86_processor.cache.l1d.line_size,
						.flags = x86_processor.cache.l1d.flags,
						.processor_start = processor_index,
						.processor_count = 1,
					};
				} else {
					/* another processor sharing the same
					 * cache */
					l1d[l1d_index].processor_count += 1;
				}
				processors[i].cache.l1d = &l1d[l1d_index];
			} else {
				/* reset cache id */
				last_l1d_id = UINT32_MAX;
			}
			if (x86_processor.cache.l2.size != 0) {
				const uint32_t l2_id = apic_id & ~bit_mask(x86_processor.cache.l2.apic_bits);
				processors[i].cache.l2 = &l2[l2_index];
				if (l2_id != last_l2_id) {
					/* new cache */
					last_l2_id = l2_id;
					l2[++l2_index] = (struct cpuinfo_cache){
						.size = x86_processor.cache.l2.size,
						.associativity = x86_processor.cache.l2.associativity,
						.sets = x86_processor.cache.l2.sets,
						.partitions = x86_processor.cache.l2.partitions,
						.line_size = x86_processor.cache.l2.line_size,
						.flags = x86_processor.cache.l2.flags,
						.processor_start = processor_index,
						.processor_count = 1,
					};
				} else {
					/* another processor sharing the same
					 * cache */
					l2[l2_index].processor_count += 1;
				}
				processors[i].cache.l2 = &l2[l2_index];
			} else {
				/* reset cache id */
				last_l2_id = UINT32_MAX;
			}
			if (x86_processor.cache.l3.size != 0) {
				const uint32_t l3_id = apic_id & ~bit_mask(x86_processor.cache.l3.apic_bits);
				processors[i].cache.l3 = &l3[l3_index];
				if (l3_id != last_l3_id) {
					/* new cache */
					last_l3_id = l3_id;
					l3[++l3_index] = (struct cpuinfo_cache){
						.size = x86_processor.cache.l3.size,
						.associativity = x86_processor.cache.l3.associativity,
						.sets = x86_processor.cache.l3.sets,
						.partitions = x86_processor.cache.l3.partitions,
						.line_size = x86_processor.cache.l3.line_size,
						.flags = x86_processor.cache.l3.flags,
						.processor_start = processor_index,
						.processor_count = 1,
					};
				} else {
					/* another processor sharing the same
					 * cache */
					l3[l3_index].processor_count += 1;
				}
				processors[i].cache.l3 = &l3[l3_index];
			} else {
				/* reset cache id */
				last_l3_id = UINT32_MAX;
			}
			if (x86_processor.cache.l4.size != 0) {
				const uint32_t l4_id = apic_id & ~bit_mask(x86_processor.cache.l4.apic_bits);
				processors[i].cache.l4 = &l4[l4_index];
				if (l4_id != last_l4_id) {
					/* new cache */
					last_l4_id = l4_id;
					l4[++l4_index] = (struct cpuinfo_cache){
						.size = x86_processor.cache.l4.size,
						.associativity = x86_processor.cache.l4.associativity,
						.sets = x86_processor.cache.l4.sets,
						.partitions = x86_processor.cache.l4.partitions,
						.line_size = x86_processor.cache.l4.line_size,
						.flags = x86_processor.cache.l4.flags,
						.processor_start = processor_index,
						.processor_count = 1,
					};
				} else {
					/* another processor sharing the same
					 * cache */
					l4[l4_index].processor_count += 1;
				}
				processors[i].cache.l4 = &l4[l4_index];
			} else {
				/* reset cache id */
				last_l4_id = UINT32_MAX;
			}
		}
	}

	/* Commit changes */
	cpuinfo_processors = processors;
	cpuinfo_cores = cores;
	cpuinfo_clusters = clusters;
	cpuinfo_packages = packages;
	cpuinfo_cache[cpuinfo_cache_level_1i] = l1i;
	cpuinfo_cache[cpuinfo_cache_level_1d] = l1d;
	cpuinfo_cache[cpuinfo_cache_level_2] = l2;
	cpuinfo_cache[cpuinfo_cache_level_3] = l3;
	cpuinfo_cache[cpuinfo_cache_level_4] = l4;

	cpuinfo_processors_count = processors_count;
	cpuinfo_cores_count = cores_count;
	cpuinfo_clusters_count = clusters_count;
	cpuinfo_packages_count = packages_count;
	cpuinfo_cache_count[cpuinfo_cache_level_1i] = l1i_count;
	cpuinfo_cache_count[cpuinfo_cache_level_1d] = l1d_count;
	cpuinfo_cache_count[cpuinfo_cache_level_2] = l2_count;
	cpuinfo_cache_count[cpuinfo_cache_level_3] = l3_count;
	cpuinfo_cache_count[cpuinfo_cache_level_4] = l4_count;
	cpuinfo_max_cache_size = cpuinfo_compute_max_cache_size(&processors[0]);

	cpuinfo_global_uarch = (struct cpuinfo_uarch_info){
		.uarch = x86_processor.uarch,
		.cpuid = x86_processor.cpuid,
		.processor_count = processors_count,
		.core_count = cores_count,
	};

	cpuinfo_linux_cpu_max = x86_linux_processors_count;
	cpuinfo_linux_cpu_to_processor_map = linux_cpu_to_processor_map;
	cpuinfo_linux_cpu_to_core_map = linux_cpu_to_core_map;

	__sync_synchronize();

	cpuinfo_is_initialized = true;

	processors = NULL;
	cores = NULL;
	clusters = NULL;
	packages = NULL;
	l1i = l1d = l2 = l3 = l4 = NULL;
	linux_cpu_to_processor_map = NULL;
	linux_cpu_to_core_map = NULL;

cleanup:
	free(x86_linux_processors);
	free(processors);
	free(cores);
	free(clusters);
	free(packages);
	free(l1i);
	free(l1d);
	free(l2);
	free(l3);
	free(l4);
	free(linux_cpu_to_processor_map);
	free(linux_cpu_to_core_map);
}
