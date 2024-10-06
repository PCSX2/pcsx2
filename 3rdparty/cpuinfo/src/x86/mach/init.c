#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <cpuinfo.h>
#include <cpuinfo/internal-api.h>
#include <cpuinfo/log.h>
#include <mach/api.h>
#include <x86/api.h>

static inline uint32_t max(uint32_t a, uint32_t b) {
	return a > b ? a : b;
}

static inline uint32_t bit_mask(uint32_t bits) {
	return (UINT32_C(1) << bits) - UINT32_C(1);
}

void cpuinfo_x86_mach_init(void) {
	struct cpuinfo_processor* processors = NULL;
	struct cpuinfo_core* cores = NULL;
	struct cpuinfo_cluster* clusters = NULL;
	struct cpuinfo_package* packages = NULL;
	struct cpuinfo_cache* l1i = NULL;
	struct cpuinfo_cache* l1d = NULL;
	struct cpuinfo_cache* l2 = NULL;
	struct cpuinfo_cache* l3 = NULL;
	struct cpuinfo_cache* l4 = NULL;

	struct cpuinfo_mach_topology mach_topology = cpuinfo_mach_detect_topology();
	processors = calloc(mach_topology.threads, sizeof(struct cpuinfo_processor));
	if (processors == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " logical processors",
			mach_topology.threads * sizeof(struct cpuinfo_processor),
			mach_topology.threads);
		goto cleanup;
	}
	cores = calloc(mach_topology.cores, sizeof(struct cpuinfo_core));
	if (cores == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " cores",
			mach_topology.cores * sizeof(struct cpuinfo_core),
			mach_topology.cores);
		goto cleanup;
	}
	/* On x86 cluster of cores is a physical package */
	clusters = calloc(mach_topology.packages, sizeof(struct cpuinfo_cluster));
	if (clusters == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " core clusters",
			mach_topology.packages * sizeof(struct cpuinfo_cluster),
			mach_topology.packages);
		goto cleanup;
	}
	packages = calloc(mach_topology.packages, sizeof(struct cpuinfo_package));
	if (packages == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " physical packages",
			mach_topology.packages * sizeof(struct cpuinfo_package),
			mach_topology.packages);
		goto cleanup;
	}

	struct cpuinfo_x86_processor x86_processor;
	memset(&x86_processor, 0, sizeof(x86_processor));
	cpuinfo_x86_init_processor(&x86_processor);
	char brand_string[48];
	cpuinfo_x86_normalize_brand_string(x86_processor.brand_string, brand_string);

	const uint32_t threads_per_core = mach_topology.threads / mach_topology.cores;
	const uint32_t threads_per_package = mach_topology.threads / mach_topology.packages;
	const uint32_t cores_per_package = mach_topology.cores / mach_topology.packages;
	for (uint32_t i = 0; i < mach_topology.packages; i++) {
		clusters[i] = (struct cpuinfo_cluster){
			.processor_start = i * threads_per_package,
			.processor_count = threads_per_package,
			.core_start = i * cores_per_package,
			.core_count = cores_per_package,
			.cluster_id = 0,
			.package = packages + i,
			.vendor = x86_processor.vendor,
			.uarch = x86_processor.uarch,
			.cpuid = x86_processor.cpuid,
		};
		packages[i].processor_start = i * threads_per_package;
		packages[i].processor_count = threads_per_package;
		packages[i].core_start = i * cores_per_package;
		packages[i].core_count = cores_per_package;
		packages[i].cluster_start = i;
		packages[i].cluster_count = 1;
		cpuinfo_x86_format_package_name(x86_processor.vendor, brand_string, packages[i].name);
	}
	for (uint32_t i = 0; i < mach_topology.cores; i++) {
		cores[i] = (struct cpuinfo_core){
			.processor_start = i * threads_per_core,
			.processor_count = threads_per_core,
			.core_id = i % cores_per_package,
			.cluster = clusters + i / cores_per_package,
			.package = packages + i / cores_per_package,
			.vendor = x86_processor.vendor,
			.uarch = x86_processor.uarch,
			.cpuid = x86_processor.cpuid,
		};
	}
	for (uint32_t i = 0; i < mach_topology.threads; i++) {
		const uint32_t smt_id = i % threads_per_core;
		const uint32_t core_id = i / threads_per_core;
		const uint32_t package_id = i / threads_per_package;

		/* Reconstruct APIC IDs from topology components */
		const uint32_t thread_bits_mask = bit_mask(x86_processor.topology.thread_bits_length);
		const uint32_t core_bits_mask = bit_mask(x86_processor.topology.core_bits_length);
		const uint32_t package_bits_offset =
			max(x86_processor.topology.thread_bits_offset + x86_processor.topology.thread_bits_length,
			    x86_processor.topology.core_bits_offset + x86_processor.topology.core_bits_length);
		const uint32_t apic_id = ((smt_id & thread_bits_mask) << x86_processor.topology.thread_bits_offset) |
			((core_id & core_bits_mask) << x86_processor.topology.core_bits_offset) |
			(package_id << package_bits_offset);
		cpuinfo_log_debug("reconstructed APIC ID 0x%08" PRIx32 " for thread %" PRIu32, apic_id, i);

		processors[i].smt_id = smt_id;
		processors[i].core = cores + i / threads_per_core;
		processors[i].cluster = clusters + i / threads_per_package;
		processors[i].package = packages + i / threads_per_package;
		processors[i].apic_id = apic_id;
	}

	uint32_t threads_per_l1 = 0, l1_count = 0;
	if (x86_processor.cache.l1i.size != 0 || x86_processor.cache.l1d.size != 0) {
		threads_per_l1 = mach_topology.threads_per_cache[1];
		if (threads_per_l1 == 0) {
			/* Assume that threads on the same core share L1 */
			threads_per_l1 = mach_topology.threads / mach_topology.cores;
			cpuinfo_log_warning(
				"Mach kernel did not report number of threads sharing L1 cache; assume %" PRIu32,
				threads_per_l1);
		}
		l1_count = mach_topology.threads / threads_per_l1;
		cpuinfo_log_debug("detected %" PRIu32 " L1 caches", l1_count);
	}

	uint32_t threads_per_l2 = 0, l2_count = 0;
	if (x86_processor.cache.l2.size != 0) {
		threads_per_l2 = mach_topology.threads_per_cache[2];
		if (threads_per_l2 == 0) {
			if (x86_processor.cache.l3.size != 0) {
				/* This is not a last-level cache; assume that
				 * threads on the same core share L2 */
				threads_per_l2 = mach_topology.threads / mach_topology.cores;
			} else {
				/* This is a last-level cache; assume that
				 * threads on the same package share L2 */
				threads_per_l2 = mach_topology.threads / mach_topology.packages;
			}
			cpuinfo_log_warning(
				"Mach kernel did not report number of threads sharing L2 cache; assume %" PRIu32,
				threads_per_l2);
		}
		l2_count = mach_topology.threads / threads_per_l2;
		cpuinfo_log_debug("detected %" PRIu32 " L2 caches", l2_count);
	}

	uint32_t threads_per_l3 = 0, l3_count = 0;
	if (x86_processor.cache.l3.size != 0) {
		threads_per_l3 = mach_topology.threads_per_cache[3];
		if (threads_per_l3 == 0) {
			/*
			 * Assume that threads on the same package share L3.
			 * However, is it not necessarily the last-level cache
			 * (there may be L4 cache as well)
			 */
			threads_per_l3 = mach_topology.threads / mach_topology.packages;
			cpuinfo_log_warning(
				"Mach kernel did not report number of threads sharing L3 cache; assume %" PRIu32,
				threads_per_l3);
		}
		l3_count = mach_topology.threads / threads_per_l3;
		cpuinfo_log_debug("detected %" PRIu32 " L3 caches", l3_count);
	}

	uint32_t threads_per_l4 = 0, l4_count = 0;
	if (x86_processor.cache.l4.size != 0) {
		threads_per_l4 = mach_topology.threads_per_cache[4];
		if (threads_per_l4 == 0) {
			/*
			 * Assume that all threads share this L4.
			 * As of now, L4 cache exists only on notebook x86 CPUs,
			 * which are single-package, but multi-socket systems
			 * could have shared L4 (like on IBM POWER8).
			 */
			threads_per_l4 = mach_topology.threads;
			cpuinfo_log_warning(
				"Mach kernel did not report number of threads sharing L4 cache; assume %" PRIu32,
				threads_per_l4);
		}
		l4_count = mach_topology.threads / threads_per_l4;
		cpuinfo_log_debug("detected %" PRIu32 " L4 caches", l4_count);
	}

	if (x86_processor.cache.l1i.size != 0) {
		l1i = calloc(l1_count, sizeof(struct cpuinfo_cache));
		if (l1i == NULL) {
			cpuinfo_log_error(
				"failed to allocate %zu bytes for descriptions of %" PRIu32 " L1I caches",
				l1_count * sizeof(struct cpuinfo_cache),
				l1_count);
			return;
		}
		for (uint32_t c = 0; c < l1_count; c++) {
			l1i[c] = (struct cpuinfo_cache){
				.size = x86_processor.cache.l1i.size,
				.associativity = x86_processor.cache.l1i.associativity,
				.sets = x86_processor.cache.l1i.sets,
				.partitions = x86_processor.cache.l1i.partitions,
				.line_size = x86_processor.cache.l1i.line_size,
				.flags = x86_processor.cache.l1i.flags,
				.processor_start = c * threads_per_l1,
				.processor_count = threads_per_l1,
			};
		}
		for (uint32_t t = 0; t < mach_topology.threads; t++) {
			processors[t].cache.l1i = &l1i[t / threads_per_l1];
		}
	}

	if (x86_processor.cache.l1d.size != 0) {
		l1d = calloc(l1_count, sizeof(struct cpuinfo_cache));
		if (l1d == NULL) {
			cpuinfo_log_error(
				"failed to allocate %zu bytes for descriptions of %" PRIu32 " L1D caches",
				l1_count * sizeof(struct cpuinfo_cache),
				l1_count);
			return;
		}
		for (uint32_t c = 0; c < l1_count; c++) {
			l1d[c] = (struct cpuinfo_cache){
				.size = x86_processor.cache.l1d.size,
				.associativity = x86_processor.cache.l1d.associativity,
				.sets = x86_processor.cache.l1d.sets,
				.partitions = x86_processor.cache.l1d.partitions,
				.line_size = x86_processor.cache.l1d.line_size,
				.flags = x86_processor.cache.l1d.flags,
				.processor_start = c * threads_per_l1,
				.processor_count = threads_per_l1,
			};
		}
		for (uint32_t t = 0; t < mach_topology.threads; t++) {
			processors[t].cache.l1d = &l1d[t / threads_per_l1];
		}
	}

	if (l2_count != 0) {
		l2 = calloc(l2_count, sizeof(struct cpuinfo_cache));
		if (l2 == NULL) {
			cpuinfo_log_error(
				"failed to allocate %zu bytes for descriptions of %" PRIu32 " L2 caches",
				l2_count * sizeof(struct cpuinfo_cache),
				l2_count);
			return;
		}
		for (uint32_t c = 0; c < l2_count; c++) {
			l2[c] = (struct cpuinfo_cache){
				.size = x86_processor.cache.l2.size,
				.associativity = x86_processor.cache.l2.associativity,
				.sets = x86_processor.cache.l2.sets,
				.partitions = x86_processor.cache.l2.partitions,
				.line_size = x86_processor.cache.l2.line_size,
				.flags = x86_processor.cache.l2.flags,
				.processor_start = c * threads_per_l2,
				.processor_count = threads_per_l2,
			};
		}
		for (uint32_t t = 0; t < mach_topology.threads; t++) {
			processors[t].cache.l2 = &l2[t / threads_per_l2];
		}
	}

	if (l3_count != 0) {
		l3 = calloc(l3_count, sizeof(struct cpuinfo_cache));
		if (l3 == NULL) {
			cpuinfo_log_error(
				"failed to allocate %zu bytes for descriptions of %" PRIu32 " L3 caches",
				l3_count * sizeof(struct cpuinfo_cache),
				l3_count);
			return;
		}
		for (uint32_t c = 0; c < l3_count; c++) {
			l3[c] = (struct cpuinfo_cache){
				.size = x86_processor.cache.l3.size,
				.associativity = x86_processor.cache.l3.associativity,
				.sets = x86_processor.cache.l3.sets,
				.partitions = x86_processor.cache.l3.partitions,
				.line_size = x86_processor.cache.l3.line_size,
				.flags = x86_processor.cache.l3.flags,
				.processor_start = c * threads_per_l3,
				.processor_count = threads_per_l3,
			};
		}
		for (uint32_t t = 0; t < mach_topology.threads; t++) {
			processors[t].cache.l3 = &l3[t / threads_per_l3];
		}
	}

	if (l4_count != 0) {
		l4 = calloc(l4_count, sizeof(struct cpuinfo_cache));
		if (l4 == NULL) {
			cpuinfo_log_error(
				"failed to allocate %zu bytes for descriptions of %" PRIu32 " L4 caches",
				l4_count * sizeof(struct cpuinfo_cache),
				l4_count);
			return;
		}
		for (uint32_t c = 0; c < l4_count; c++) {
			l4[c] = (struct cpuinfo_cache){
				.size = x86_processor.cache.l4.size,
				.associativity = x86_processor.cache.l4.associativity,
				.sets = x86_processor.cache.l4.sets,
				.partitions = x86_processor.cache.l4.partitions,
				.line_size = x86_processor.cache.l4.line_size,
				.flags = x86_processor.cache.l4.flags,
				.processor_start = c * threads_per_l4,
				.processor_count = threads_per_l4,
			};
		}
		for (uint32_t t = 0; t < mach_topology.threads; t++) {
			processors[t].cache.l4 = &l4[t / threads_per_l4];
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

	cpuinfo_processors_count = mach_topology.threads;
	cpuinfo_cores_count = mach_topology.cores;
	cpuinfo_clusters_count = mach_topology.packages;
	cpuinfo_packages_count = mach_topology.packages;
	cpuinfo_cache_count[cpuinfo_cache_level_1i] = l1_count;
	cpuinfo_cache_count[cpuinfo_cache_level_1d] = l1_count;
	cpuinfo_cache_count[cpuinfo_cache_level_2] = l2_count;
	cpuinfo_cache_count[cpuinfo_cache_level_3] = l3_count;
	cpuinfo_cache_count[cpuinfo_cache_level_4] = l4_count;
	cpuinfo_max_cache_size = cpuinfo_compute_max_cache_size(&processors[0]);

	cpuinfo_global_uarch = (struct cpuinfo_uarch_info){
		.uarch = x86_processor.uarch,
		.cpuid = x86_processor.cpuid,
		.processor_count = mach_topology.threads,
		.core_count = mach_topology.cores,
	};

	__sync_synchronize();

	cpuinfo_is_initialized = true;

	processors = NULL;
	cores = NULL;
	clusters = NULL;
	packages = NULL;
	l1i = l1d = l2 = l3 = l4 = NULL;

cleanup:
	free(processors);
	free(cores);
	free(clusters);
	free(packages);
	free(l1i);
	free(l1d);
	free(l2);
	free(l3);
	free(l4);
}
