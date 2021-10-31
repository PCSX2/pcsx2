#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <cpuinfo.h>
#include <x86/api.h>
#include <cpuinfo/internal-api.h>
#include <cpuinfo/log.h>

#include <windows.h>

#ifdef __GNUC__
  #define CPUINFO_ALLOCA __builtin_alloca
#else
  #define CPUINFO_ALLOCA _alloca
#endif


static inline uint32_t bit_mask(uint32_t bits) {
	return (UINT32_C(1) << bits) - UINT32_C(1);
}

static inline uint32_t low_index_from_kaffinity(KAFFINITY kaffinity) {
	#if defined(_M_X64) || defined(_M_AMD64)
		unsigned long index;
		_BitScanForward64(&index, (unsigned __int64) kaffinity);
		return (uint32_t) index;
	#elif defined(_M_IX86)
		unsigned long index;
		_BitScanForward(&index, (unsigned long) kaffinity);
		return (uint32_t) index;
	#else
		#error Platform-specific implementation required
	#endif
}

static void cpuinfo_x86_count_caches(
	uint32_t processors_count,
	const struct cpuinfo_processor* processors,
	const struct cpuinfo_x86_processor* x86_processor,
	uint32_t* l1i_count_ptr,
	uint32_t* l1d_count_ptr,
	uint32_t* l2_count_ptr,
	uint32_t* l3_count_ptr,
	uint32_t* l4_count_ptr)
{
	uint32_t l1i_count = 0, l1d_count = 0, l2_count = 0, l3_count = 0, l4_count = 0;
	uint32_t last_l1i_id = UINT32_MAX, last_l1d_id = UINT32_MAX;
	uint32_t last_l2_id = UINT32_MAX, last_l3_id = UINT32_MAX, last_l4_id = UINT32_MAX;
	for (uint32_t i = 0; i < processors_count; i++) {
		const uint32_t apic_id = processors[i].apic_id;
		cpuinfo_log_debug("APID ID %"PRIu32": logical processor %"PRIu32, apic_id, i);

		if (x86_processor->cache.l1i.size != 0) {
			const uint32_t l1i_id = apic_id & ~bit_mask(x86_processor->cache.l1i.apic_bits);
			if (l1i_id != last_l1i_id) {
				last_l1i_id = l1i_id;
				l1i_count++;
			}
		}
		if (x86_processor->cache.l1d.size != 0) {
			const uint32_t l1d_id = apic_id & ~bit_mask(x86_processor->cache.l1d.apic_bits);
			if (l1d_id != last_l1d_id) {
				last_l1d_id = l1d_id;
				l1d_count++;
			}
		}
		if (x86_processor->cache.l2.size != 0) {
			const uint32_t l2_id = apic_id & ~bit_mask(x86_processor->cache.l2.apic_bits);
			if (l2_id != last_l2_id) {
				last_l2_id = l2_id;
				l2_count++;
			}
		}
		if (x86_processor->cache.l3.size != 0) {
			const uint32_t l3_id = apic_id & ~bit_mask(x86_processor->cache.l3.apic_bits);
			if (l3_id != last_l3_id) {
				last_l3_id = l3_id;
				l3_count++;
			}
		}
		if (x86_processor->cache.l4.size != 0) {
			const uint32_t l4_id = apic_id & ~bit_mask(x86_processor->cache.l4.apic_bits);
			if (l4_id != last_l4_id) {
				last_l4_id = l4_id;
				l4_count++;
			}
		}
	}
	*l1i_count_ptr = l1i_count;
	*l1d_count_ptr = l1d_count;
	*l2_count_ptr  = l2_count;
	*l3_count_ptr  = l3_count;
	*l4_count_ptr  = l4_count;
}

static bool cpuinfo_x86_windows_is_wine(void) {
	HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
	if (ntdll == NULL) {
		return false;
	}

	return GetProcAddress(ntdll, "wine_get_version") != NULL;
}

BOOL CALLBACK cpuinfo_x86_windows_init(PINIT_ONCE init_once, PVOID parameter, PVOID* context) {
	struct cpuinfo_processor* processors = NULL;
	struct cpuinfo_core* cores = NULL;
	struct cpuinfo_cluster* clusters = NULL;
	struct cpuinfo_package* packages = NULL;
	struct cpuinfo_cache* l1i = NULL;
	struct cpuinfo_cache* l1d = NULL;
	struct cpuinfo_cache* l2 = NULL;
	struct cpuinfo_cache* l3 = NULL;
	struct cpuinfo_cache* l4 = NULL;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX processor_infos = NULL;

	HANDLE heap = GetProcessHeap();
	const bool is_wine = cpuinfo_x86_windows_is_wine();

	struct cpuinfo_x86_processor x86_processor;
	ZeroMemory(&x86_processor, sizeof(x86_processor));
	cpuinfo_x86_init_processor(&x86_processor);
	char brand_string[48];
	cpuinfo_x86_normalize_brand_string(x86_processor.brand_string, brand_string);

	const uint32_t thread_bits_mask = bit_mask(x86_processor.topology.thread_bits_length);
	const uint32_t core_bits_mask   = bit_mask(x86_processor.topology.core_bits_length);
	const uint32_t package_bits_offset = max(
		x86_processor.topology.thread_bits_offset + x86_processor.topology.thread_bits_length,
		x86_processor.topology.core_bits_offset + x86_processor.topology.core_bits_length);

	/* WINE doesn't implement GetMaximumProcessorGroupCount and aborts when calling it */
	const uint32_t max_group_count = is_wine ? 1 : (uint32_t) GetMaximumProcessorGroupCount();
	cpuinfo_log_debug("detected %"PRIu32" processor groups", max_group_count);

	uint32_t processors_count = 0;
	uint32_t* processors_per_group = (uint32_t*) CPUINFO_ALLOCA(max_group_count * sizeof(uint32_t));
	for (uint32_t i = 0; i < max_group_count; i++) {
		processors_per_group[i] = GetMaximumProcessorCount((WORD) i);
		cpuinfo_log_debug("detected %"PRIu32" processors in group %"PRIu32,
			processors_per_group[i], i);
		processors_count += processors_per_group[i];
	}

	uint32_t* processors_before_group = (uint32_t*) CPUINFO_ALLOCA(max_group_count * sizeof(uint32_t));
	for (uint32_t i = 0, count = 0; i < max_group_count; i++) {
		processors_before_group[i] = count;
		cpuinfo_log_debug("detected %"PRIu32" processors before group %"PRIu32,
			processors_before_group[i], i);
		count += processors_per_group[i];
	}

	processors = HeapAlloc(heap, HEAP_ZERO_MEMORY, processors_count * sizeof(struct cpuinfo_processor));
	if (processors == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" logical processors",
			processors_count * sizeof(struct cpuinfo_processor), processors_count);
		goto cleanup;
	}

	DWORD cores_info_size = 0;
	if (GetLogicalProcessorInformationEx(RelationProcessorCore, NULL, &cores_info_size) == FALSE) {
		const DWORD last_error = GetLastError();
		if (last_error != ERROR_INSUFFICIENT_BUFFER) {
			cpuinfo_log_error("failed to query size of processor cores information: error %"PRIu32,
				(uint32_t) last_error);
			goto cleanup;
		}
	}

	DWORD packages_info_size = 0;
	if (GetLogicalProcessorInformationEx(RelationProcessorPackage, NULL, &packages_info_size) == FALSE) {
		const DWORD last_error = GetLastError();
		if (last_error != ERROR_INSUFFICIENT_BUFFER) {
			cpuinfo_log_error("failed to query size of processor packages information: error %"PRIu32,
				(uint32_t) last_error);
			goto cleanup;
		}
	}

	DWORD max_info_size = max(cores_info_size, packages_info_size);

	processor_infos = HeapAlloc(heap, 0, max_info_size);
	if (processor_infos == NULL) {
		cpuinfo_log_error("failed to allocate %"PRIu32" bytes for logical processor information",
			(uint32_t) max_info_size);
		goto cleanup;
	}

	if (GetLogicalProcessorInformationEx(RelationProcessorPackage, processor_infos, &max_info_size) == FALSE) {
		cpuinfo_log_error("failed to query processor packages information: error %"PRIu32,
			(uint32_t) GetLastError());
		goto cleanup;
	}

	uint32_t packages_count = 0;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX packages_info_end =
		(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) ((uintptr_t) processor_infos + packages_info_size);
	for (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX package_info = processor_infos;
		package_info < packages_info_end;
		package_info = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) ((uintptr_t) package_info + package_info->Size))
	{
		if (package_info->Relationship != RelationProcessorPackage) {
			cpuinfo_log_warning("unexpected processor info type (%"PRIu32") for processor package information",
				(uint32_t) package_info->Relationship);
			continue;
		}

		/* We assume that packages are reported in APIC order */
		const uint32_t package_id = packages_count++;
		/* Reconstruct package part of APIC ID */
		const uint32_t package_apic_id = package_id << package_bits_offset;
		/* Iterate processor groups and set the package part of APIC ID */
		for (uint32_t i = 0; i < package_info->Processor.GroupCount; i++) {
			const uint32_t group_id = package_info->Processor.GroupMask[i].Group;
			/* Global index of the first logical processor belonging to this group */
			const uint32_t group_processors_start = processors_before_group[group_id];
			/* Bitmask representing processors in this group belonging to this package */
			KAFFINITY group_processors_mask = package_info->Processor.GroupMask[i].Mask;
			while (group_processors_mask != 0) {
				const uint32_t group_processor_id = low_index_from_kaffinity(group_processors_mask);
				const uint32_t processor_id = group_processors_start + group_processor_id;
				processors[processor_id].package = (const struct cpuinfo_package*) NULL + package_id;
				processors[processor_id].windows_group_id = (uint16_t) group_id;
				processors[processor_id].windows_processor_id = (uint16_t) group_processor_id;
				processors[processor_id].apic_id = package_apic_id;

				/* Reset the lowest bit in affinity mask */
				group_processors_mask &= (group_processors_mask - 1);
			}
		}
	}

	max_info_size = max(cores_info_size, packages_info_size);
	if (GetLogicalProcessorInformationEx(RelationProcessorCore, processor_infos, &max_info_size) == FALSE) {
		cpuinfo_log_error("failed to query processor cores information: error %"PRIu32,
			(uint32_t) GetLastError());
		goto cleanup;
	}

	uint32_t cores_count = 0;
	/* Index (among all cores) of the the first core on the current package */
	uint32_t package_core_start = 0;
	uint32_t current_package_apic_id = 0;
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX cores_info_end =
		(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) ((uintptr_t) processor_infos + cores_info_size);
	for (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX core_info = processor_infos;
		core_info < cores_info_end;
		core_info = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) ((uintptr_t) core_info + core_info->Size))
	{
		if (core_info->Relationship != RelationProcessorCore) {
			cpuinfo_log_warning("unexpected processor info type (%"PRIu32") for processor core information",
				(uint32_t) core_info->Relationship);
			continue;
		}

		/* We assume that cores and logical processors are reported in APIC order */
		const uint32_t core_id = cores_count++;
		uint32_t smt_id = 0;
		/* Reconstruct core part of APIC ID */
		const uint32_t core_apic_id = (core_id & core_bits_mask) << x86_processor.topology.core_bits_offset;
		/* Iterate processor groups and set the core & SMT parts of APIC ID */
		for (uint32_t i = 0; i < core_info->Processor.GroupCount; i++) {
			const uint32_t group_id = core_info->Processor.GroupMask[i].Group;
			/* Global index of the first logical processor belonging to this group */
			const uint32_t group_processors_start = processors_before_group[group_id];
			/* Bitmask representing processors in this group belonging to this package */
			KAFFINITY group_processors_mask = core_info->Processor.GroupMask[i].Mask;
			while (group_processors_mask != 0) {
				const uint32_t group_processor_id = low_index_from_kaffinity(group_processors_mask);
				const uint32_t processor_id = group_processors_start + group_processor_id;

				/* Check if this is the first core on a new package */
				if (processors[processor_id].apic_id != current_package_apic_id) {
					package_core_start = core_id;
					current_package_apic_id = processors[processor_id].apic_id;
				}
				/* Core ID w.r.t package */
				const uint32_t package_core_id = core_id - package_core_start;

				/* Update APIC ID with core and SMT parts */
				processors[processor_id].apic_id |=
					((smt_id & thread_bits_mask) << x86_processor.topology.thread_bits_offset) |
					((package_core_id & core_bits_mask) << x86_processor.topology.core_bits_offset);
				cpuinfo_log_debug("reconstructed APIC ID 0x%08"PRIx32" for processor %"PRIu32" in group %"PRIu32,
					processors[processor_id].apic_id, group_processor_id, group_id);

				/* Set SMT ID (assume logical processors within the core are reported in APIC order) */
				processors[processor_id].smt_id = smt_id++;
				processors[processor_id].core = (const struct cpuinfo_core*) NULL + core_id;

				/* Reset the lowest bit in affinity mask */
				group_processors_mask &= (group_processors_mask - 1);
			}
		}
	}

	cores = HeapAlloc(heap, HEAP_ZERO_MEMORY, cores_count * sizeof(struct cpuinfo_core));
	if (cores == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" cores",
			cores_count * sizeof(struct cpuinfo_core), cores_count);
		goto cleanup;
	}

	clusters = HeapAlloc(heap, HEAP_ZERO_MEMORY, packages_count * sizeof(struct cpuinfo_cluster));
	if (clusters == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" core clusters",
			packages_count * sizeof(struct cpuinfo_cluster), packages_count);
		goto cleanup;
	}

	packages = HeapAlloc(heap, HEAP_ZERO_MEMORY, packages_count * sizeof(struct cpuinfo_package));
	if (packages == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" physical packages",
			packages_count * sizeof(struct cpuinfo_package), packages_count);
		goto cleanup;
	}

	for (uint32_t i = processors_count; i != 0; i--) {
		const uint32_t processor_id = i - 1;
		struct cpuinfo_processor* processor = processors + processor_id;

		/* Adjust core and package pointers for all logical processors */
		struct cpuinfo_core* core =
			(struct cpuinfo_core*) ((uintptr_t) cores + (uintptr_t) processor->core);
		processor->core = core;
		struct cpuinfo_cluster* cluster =
			(struct cpuinfo_cluster*) ((uintptr_t) clusters + (uintptr_t) processor->cluster);
		processor->cluster = cluster;
		struct cpuinfo_package* package =
			(struct cpuinfo_package*) ((uintptr_t) packages + (uintptr_t) processor->package);
		processor->package = package;

		/* This can be overwritten by lower-index processors on the same package */
		package->processor_start = processor_id;
		package->processor_count += 1;

		/* This can be overwritten by lower-index processors on the same cluster */
		cluster->processor_start = processor_id;
		cluster->processor_count += 1;

		/* This can be overwritten by lower-index processors on the same core*/
		core->processor_start = processor_id;
		core->processor_count += 1;
	}

	/* Set vendor/uarch/CPUID information for cores */
	for (uint32_t i = cores_count; i != 0; i--) {
		const uint32_t global_core_id = i - 1;
		struct cpuinfo_core* core = cores + global_core_id;
		const struct cpuinfo_processor* processor = processors + core->processor_start;
		struct cpuinfo_package* package = (struct cpuinfo_package*) processor->package;
		struct cpuinfo_cluster* cluster = (struct cpuinfo_cluster*) processor->cluster;

		core->cluster = cluster;
		core->package = package;
		core->core_id = core_bits_mask &
			(processor->apic_id >> x86_processor.topology.core_bits_offset);
		core->vendor = x86_processor.vendor;
		core->uarch  = x86_processor.uarch;
		core->cpuid  = x86_processor.cpuid;

		/* This can be overwritten by lower-index cores on the same cluster/package */
		cluster->core_start = global_core_id;
		cluster->core_count += 1;
		package->core_start = global_core_id;
		package->core_count += 1;
	}

	for (uint32_t i = 0; i < packages_count; i++) {
		struct cpuinfo_package* package = packages + i;
		struct cpuinfo_cluster* cluster = clusters + i;

		cluster->package = package;
		cluster->vendor = cores[cluster->core_start].vendor;
		cluster->uarch = cores[cluster->core_start].uarch;
		cluster->cpuid = cores[cluster->core_start].cpuid;
		package->cluster_start = i;
		package->cluster_count = 1;
		cpuinfo_x86_format_package_name(x86_processor.vendor, brand_string, package->name);
	}

	/* Count caches */
	uint32_t l1i_count, l1d_count, l2_count, l3_count, l4_count;
	cpuinfo_x86_count_caches(processors_count, processors, &x86_processor,
		&l1i_count, &l1d_count, &l2_count, &l3_count, &l4_count);

	/* Allocate cache descriptions */
	if (l1i_count != 0) {
		l1i = HeapAlloc(heap, HEAP_ZERO_MEMORY, l1i_count * sizeof(struct cpuinfo_cache));
		if (l1i == NULL) {
			cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" L1I caches",
				l1i_count * sizeof(struct cpuinfo_cache), l1i_count);
			goto cleanup;
		}
	}
	if (l1d_count != 0) {
		l1d = HeapAlloc(heap, HEAP_ZERO_MEMORY, l1d_count * sizeof(struct cpuinfo_cache));
		if (l1d == NULL) {
			cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" L1D caches",
				l1d_count * sizeof(struct cpuinfo_cache), l1d_count);
			goto cleanup;
		}
	}
	if (l2_count != 0) {
		l2 = HeapAlloc(heap, HEAP_ZERO_MEMORY, l2_count * sizeof(struct cpuinfo_cache));
		if (l2 == NULL) {
			cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" L2 caches",
				l2_count * sizeof(struct cpuinfo_cache), l2_count);
			goto cleanup;
		}
	}
	if (l3_count != 0) {
		l3 = HeapAlloc(heap, HEAP_ZERO_MEMORY, l3_count * sizeof(struct cpuinfo_cache));
		if (l3 == NULL) {
			cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" L3 caches",
				l3_count * sizeof(struct cpuinfo_cache), l3_count);
			goto cleanup;
		}
	}
	if (l4_count != 0) {
		l4 = HeapAlloc(heap, HEAP_ZERO_MEMORY, l4_count * sizeof(struct cpuinfo_cache));
		if (l4 == NULL) {
			cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" L4 caches",
				l4_count * sizeof(struct cpuinfo_cache), l4_count);
			goto cleanup;
		}
	}

	/* Set cache information */
	uint32_t l1i_index = UINT32_MAX, l1d_index = UINT32_MAX, l2_index = UINT32_MAX, l3_index = UINT32_MAX, l4_index = UINT32_MAX;
	uint32_t last_l1i_id = UINT32_MAX, last_l1d_id = UINT32_MAX;
	uint32_t last_l2_id = UINT32_MAX, last_l3_id = UINT32_MAX, last_l4_id = UINT32_MAX;
	for (uint32_t i = 0; i < processors_count; i++) {
		const uint32_t apic_id = processors[i].apic_id;

		if (x86_processor.cache.l1i.size != 0) {
			const uint32_t l1i_id = apic_id & ~bit_mask(x86_processor.cache.l1i.apic_bits);
			processors[i].cache.l1i = &l1i[l1i_index];
			if (l1i_id != last_l1i_id) {
				/* new cache */
				last_l1i_id = l1i_id;
				l1i[++l1i_index] = (struct cpuinfo_cache) {
					.size            = x86_processor.cache.l1i.size,
					.associativity   = x86_processor.cache.l1i.associativity,
					.sets            = x86_processor.cache.l1i.sets,
					.partitions      = x86_processor.cache.l1i.partitions,
					.line_size       = x86_processor.cache.l1i.line_size,
					.flags           = x86_processor.cache.l1i.flags,
					.processor_start = i,
					.processor_count = 1,
				};
			} else {
				/* another processor sharing the same cache */
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
				l1d[++l1d_index] = (struct cpuinfo_cache) {
					.size            = x86_processor.cache.l1d.size,
					.associativity   = x86_processor.cache.l1d.associativity,
					.sets            = x86_processor.cache.l1d.sets,
					.partitions      = x86_processor.cache.l1d.partitions,
					.line_size       = x86_processor.cache.l1d.line_size,
					.flags           = x86_processor.cache.l1d.flags,
					.processor_start = i,
					.processor_count = 1,
				};
			} else {
				/* another processor sharing the same cache */
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
				l2[++l2_index] = (struct cpuinfo_cache) {
					.size            = x86_processor.cache.l2.size,
					.associativity   = x86_processor.cache.l2.associativity,
					.sets            = x86_processor.cache.l2.sets,
					.partitions      = x86_processor.cache.l2.partitions,
					.line_size       = x86_processor.cache.l2.line_size,
					.flags           = x86_processor.cache.l2.flags,
					.processor_start = i,
					.processor_count = 1,
				};
			} else {
				/* another processor sharing the same cache */
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
				l3[++l3_index] = (struct cpuinfo_cache) {
					.size            = x86_processor.cache.l3.size,
					.associativity   = x86_processor.cache.l3.associativity,
					.sets            = x86_processor.cache.l3.sets,
					.partitions      = x86_processor.cache.l3.partitions,
					.line_size       = x86_processor.cache.l3.line_size,
					.flags           = x86_processor.cache.l3.flags,
					.processor_start = i,
					.processor_count = 1,
				};
			} else {
				/* another processor sharing the same cache */
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
				l4[++l4_index] = (struct cpuinfo_cache) {
					.size            = x86_processor.cache.l4.size,
					.associativity   = x86_processor.cache.l4.associativity,
					.sets            = x86_processor.cache.l4.sets,
					.partitions      = x86_processor.cache.l4.partitions,
					.line_size       = x86_processor.cache.l4.line_size,
					.flags           = x86_processor.cache.l4.flags,
					.processor_start = i,
					.processor_count = 1,
				};
			} else {
				/* another processor sharing the same cache */
				l4[l4_index].processor_count += 1;
			}
			processors[i].cache.l4 = &l4[l4_index];
		} else {
			/* reset cache id */
			last_l4_id = UINT32_MAX;
		}
	}


	/* Commit changes */
	cpuinfo_processors = processors;
	cpuinfo_cores = cores;
	cpuinfo_clusters = clusters;
	cpuinfo_packages = packages;
	cpuinfo_cache[cpuinfo_cache_level_1i] = l1i;
	cpuinfo_cache[cpuinfo_cache_level_1d] = l1d;
	cpuinfo_cache[cpuinfo_cache_level_2]  = l2;
	cpuinfo_cache[cpuinfo_cache_level_3]  = l3;
	cpuinfo_cache[cpuinfo_cache_level_4]  = l4;

	cpuinfo_processors_count = processors_count;
	cpuinfo_cores_count = cores_count;
	cpuinfo_clusters_count = packages_count;
	cpuinfo_packages_count = packages_count;
	cpuinfo_cache_count[cpuinfo_cache_level_1i] = l1i_count;
	cpuinfo_cache_count[cpuinfo_cache_level_1d] = l1d_count;
	cpuinfo_cache_count[cpuinfo_cache_level_2]  = l2_count;
	cpuinfo_cache_count[cpuinfo_cache_level_3]  = l3_count;
	cpuinfo_cache_count[cpuinfo_cache_level_4]  = l4_count;
	cpuinfo_max_cache_size = cpuinfo_compute_max_cache_size(&processors[0]);

	cpuinfo_global_uarch = (struct cpuinfo_uarch_info) {
		.uarch = x86_processor.uarch,
		.cpuid = x86_processor.cpuid,
		.processor_count = processors_count,
		.core_count = cores_count,
	};

	MemoryBarrier();

	cpuinfo_is_initialized = true;

	processors = NULL;
	cores = NULL;
	clusters = NULL;
	packages = NULL;
	l1i = l1d = l2 = l3 = l4 = NULL;

cleanup:
	if (processors != NULL) {
		HeapFree(heap, 0, processors);
	}
	if (cores != NULL) {
		HeapFree(heap, 0, cores);
	}
	if (clusters != NULL) {
		HeapFree(heap, 0, clusters);
	}
	if (packages != NULL) {
		HeapFree(heap, 0, packages);
	}
	if (l1i != NULL) {
		HeapFree(heap, 0, l1i);
	}
	if (l1d != NULL) {
		HeapFree(heap, 0, l1d);
	}
	if (l2 != NULL) {
		HeapFree(heap, 0, l2);
	}
	if (l3 != NULL) {
		HeapFree(heap, 0, l3);
	}
	if (l4 != NULL) {
		HeapFree(heap, 0, l4);
	}
	return TRUE;
}
