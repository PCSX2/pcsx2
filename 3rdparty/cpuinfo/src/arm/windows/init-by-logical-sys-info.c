#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <sys/types.h>

#include <cpuinfo.h>
#include <cpuinfo/internal-api.h>
#include <cpuinfo/log.h>

#include "windows-arm-init.h"

#define MAX_NR_OF_CACHES	(cpuinfo_cache_level_max - 1)

/* Call chain:
 * cpu_info_init_by_logical_sys_info
 * 		read_packages_for_processors
 * 		read_cores_for_processors
 * 		read_caches_for_processors
 * 			read_all_logical_processor_info_of_relation
 * 				parse_relation_processor_info
 * 					store_package_info_per_processor
 * 					store_core_info_per_processor
 * 				parse_relation_cache_info
 * 					store_cache_info_per_processor
 */

static uint32_t count_logical_processors(
	const uint32_t max_group_count,
	uint32_t* global_proc_index_per_group);

static uint32_t read_packages_for_processors(
	struct cpuinfo_processor* processors,
	const uint32_t number_of_processors,
	const uint32_t* global_proc_index_per_group,
	const struct woa_chip_info *chip_info);

static uint32_t read_cores_for_processors(
	struct cpuinfo_processor* processors,
	const uint32_t number_of_processors,
	const uint32_t* global_proc_index_per_group,
	struct cpuinfo_core* cores,
	const struct woa_chip_info *chip_info);

static uint32_t read_caches_for_processors(
	struct cpuinfo_processor *processors,
	const uint32_t number_of_processors,
	struct cpuinfo_cache *caches,
	uint32_t* numbers_of_caches,
	const uint32_t* global_proc_index_per_group,
	const struct woa_chip_info *chip_info);

static uint32_t read_all_logical_processor_info_of_relation(
	LOGICAL_PROCESSOR_RELATIONSHIP info_type,
	struct cpuinfo_processor* processors,
	const uint32_t number_of_processors,
	struct cpuinfo_cache* caches,
	uint32_t* numbers_of_caches,
	struct cpuinfo_core* cores,
	const uint32_t* global_proc_index_per_group,
	const struct woa_chip_info *chip_info);

static bool parse_relation_processor_info(
	struct cpuinfo_processor* processors,
	uint32_t nr_of_processors,
	const uint32_t* global_proc_index_per_group,
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info,
	const uint32_t info_id,
	struct cpuinfo_core* cores,
	const struct woa_chip_info *chip_info);

static bool parse_relation_cache_info(
	struct cpuinfo_processor* processors,
	struct cpuinfo_cache* caches,
	uint32_t* numbers_of_caches,
	const uint32_t* global_proc_index_per_group,
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info);

static void store_package_info_per_processor(
	struct cpuinfo_processor* processors,
	const uint32_t processor_global_index,
	const uint32_t package_id,
	const uint32_t group_id,
	const uint32_t processor_id_in_group);

static void store_core_info_per_processor(
	struct cpuinfo_processor* processors,
	const uint32_t processor_global_index,
	const uint32_t core_id,
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX core_info,
	struct cpuinfo_core* cores,
	const struct woa_chip_info *chip_info);

static void store_cache_info_per_processor(
	struct cpuinfo_processor* processors,
	const uint32_t processor_global_index,
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info,
	struct cpuinfo_cache* current_cache);

static bool connect_packages_cores_clusters_by_processors(
	struct cpuinfo_processor* processors,
	const uint32_t nr_of_processors,
	struct cpuinfo_package* packages,
	const uint32_t nr_of_packages,
	struct cpuinfo_cluster* clusters,
	struct cpuinfo_core* cores,
	const uint32_t nr_of_cores,
	const struct woa_chip_info* chip_info,
	enum cpuinfo_vendor vendor);

static inline uint32_t low_index_from_kaffinity(KAFFINITY kaffinity);


bool cpu_info_init_by_logical_sys_info(
	const struct woa_chip_info *chip_info,
	const enum cpuinfo_vendor vendor)
{
	struct cpuinfo_processor* processors = NULL;
	struct cpuinfo_package* packages = NULL;
	struct cpuinfo_cluster* clusters = NULL;
	struct cpuinfo_core* cores = NULL;
	struct cpuinfo_cache* caches = NULL;
	struct cpuinfo_uarch_info* uarchs = NULL;

	uint32_t nr_of_packages = 0;
	uint32_t nr_of_cores = 0;
	uint32_t nr_of_all_caches = 0;
	uint32_t numbers_of_caches[MAX_NR_OF_CACHES] = {0};
	
	uint32_t nr_of_uarchs = 0;
	bool result = false;
	
	HANDLE heap = GetProcessHeap();

	/* 1. Count available logical processor groups and processors */
	const uint32_t max_group_count = (uint32_t) GetMaximumProcessorGroupCount();
	cpuinfo_log_debug("detected %"PRIu32" processor group(s)", max_group_count);
	/* We need to store the absolute processor ID offsets for every groups, because
	 *  1. We can't assume every processor groups include the same number of
	 *     logical processors.
	 *  2. Every processor groups know its group number and processor IDs within
	 *     the group, but not the global processor IDs.
	 *  3. We need to list every logical processors by global IDs.
	*/
	uint32_t* global_proc_index_per_group =
		(uint32_t*) HeapAlloc(heap, 0, max_group_count * sizeof(uint32_t));
	if (global_proc_index_per_group == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %"PRIu32" processor groups",
			max_group_count * sizeof(struct cpuinfo_processor), max_group_count);
		goto clean_up;
	}
	
	uint32_t nr_of_processors =
		count_logical_processors(max_group_count, global_proc_index_per_group);
	processors = HeapAlloc(heap, HEAP_ZERO_MEMORY, nr_of_processors * sizeof(struct cpuinfo_processor));
	if (processors == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %"PRIu32" logical processors",
			nr_of_processors * sizeof(struct cpuinfo_processor), nr_of_processors);
		goto clean_up;
	}

	/* 2. Read topology information via MSDN API: packages, cores and caches*/
	nr_of_packages = read_packages_for_processors(
						processors, nr_of_processors,
						global_proc_index_per_group,
						chip_info);
	if (!nr_of_packages) {
		cpuinfo_log_error("error in reading package information");
		goto clean_up;
	}
	cpuinfo_log_debug("detected %"PRIu32" processor package(s)", nr_of_packages);

	/* We need the EfficiencyClass to parse uarch from the core information,
	 * but we need to iterate first to count cores and allocate memory then
	 * we will iterate again to read and store data to cpuinfo_core structures.
	 */
	nr_of_cores = read_cores_for_processors(
					processors, nr_of_processors,
					global_proc_index_per_group, NULL,
					chip_info);
	if (!nr_of_cores) {
		cpuinfo_log_error("error in reading core information");
		goto clean_up;
	}
	cpuinfo_log_debug("detected %"PRIu32" processor core(s)", nr_of_cores);

	/* There is no API to read number of caches, so we need to iterate twice on caches:
		1. Count all type of caches -> allocate memory
		2. Read out cache data and store to allocated memory
	 */
	nr_of_all_caches = read_caches_for_processors(
						processors, nr_of_processors,
						caches, numbers_of_caches,
						global_proc_index_per_group, chip_info);
	if (!nr_of_all_caches) {
		cpuinfo_log_error("error in reading cache information");
		goto clean_up;
	}
	cpuinfo_log_debug("detected %"PRIu32" processor cache(s)", nr_of_all_caches);

	/* 3. Allocate memory for package, cluster, core and cache structures */
	packages = HeapAlloc(heap, HEAP_ZERO_MEMORY, nr_of_packages * sizeof(struct cpuinfo_package));
	if (packages == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" physical packages",
			nr_of_packages * sizeof(struct cpuinfo_package), nr_of_packages);
		goto clean_up;
	}

	/* We don't have cluster information so we explicitly set clusters to equal to cores. */
	clusters = HeapAlloc(heap, HEAP_ZERO_MEMORY, nr_of_cores * sizeof(struct cpuinfo_cluster));
	if (clusters == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" core clusters",
			nr_of_cores * sizeof(struct cpuinfo_cluster), nr_of_cores);
		goto clean_up;
	}

	cores = HeapAlloc(heap, HEAP_ZERO_MEMORY, nr_of_cores * sizeof(struct cpuinfo_core));
	if (cores == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" cores",
			nr_of_cores * sizeof(struct cpuinfo_core), nr_of_cores);
		goto clean_up;
	}

	/* We allocate one contiguous cache array for all caches, then use offsets per cache type. */
	caches = HeapAlloc(heap, HEAP_ZERO_MEMORY, nr_of_all_caches * sizeof(struct cpuinfo_cache));
	if (caches == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" caches",
			nr_of_all_caches * sizeof(struct cpuinfo_cache), nr_of_all_caches);
		goto clean_up;
	}

	/* 4.Read missing topology information that can't be saved without counted
	 *   allocate structures in the first round.
	 */
	nr_of_all_caches = read_caches_for_processors(
						processors, nr_of_processors,
						caches, numbers_of_caches, global_proc_index_per_group, chip_info);
	if (!nr_of_all_caches) {
		cpuinfo_log_error("error in reading cache information");
		goto clean_up;
	}

	nr_of_cores = read_cores_for_processors(
		processors, nr_of_processors,
		global_proc_index_per_group, cores,
		chip_info);
	if (!nr_of_cores) {
		cpuinfo_log_error("error in reading core information");
		goto clean_up;
	}

	/* 5. Now that we read out everything from the system we can, fill the package, cluster
	 *    and core structures respectively.
	 */
	result = connect_packages_cores_clusters_by_processors(
				processors, nr_of_processors,
				packages, nr_of_packages,
				clusters,
				cores, nr_of_cores,
				chip_info,
				vendor);
	if(!result) {
		cpuinfo_log_error("error in connecting information");
		goto clean_up;
	}

	/* 6. Count and store uarchs of cores, assuming same uarchs are neighbors */
	enum cpuinfo_uarch prev_uarch = cpuinfo_uarch_unknown;
	for (uint32_t i = 0; i < nr_of_cores; i++) {
		if (prev_uarch != cores[i].uarch) {
			nr_of_uarchs++;
			prev_uarch = cores[i].uarch;
		}
	}
	uarchs = HeapAlloc(heap, HEAP_ZERO_MEMORY, nr_of_uarchs * sizeof(struct cpuinfo_uarch_info));
	if (uarchs == NULL) {
		cpuinfo_log_error("failed to allocate %zu bytes for descriptions of %"PRIu32" uarchs",
			nr_of_uarchs * sizeof(struct cpuinfo_uarch_info), nr_of_uarchs);
		goto clean_up;
	}
	prev_uarch = cpuinfo_uarch_unknown;
	for (uint32_t i = 0, uarch_index = 0; i < nr_of_cores; i++) {
		if (prev_uarch != cores[i].uarch) {
			if (i != 0) {
				uarch_index++;
			}
			if (uarch_index >= nr_of_uarchs) {
				cpuinfo_log_error("more uarchs detected than reported");
			}
			prev_uarch = cores[i].uarch;
			uarchs[uarch_index].uarch = cores[i].uarch;
			uarchs[uarch_index].core_count = 1;
			uarchs[uarch_index].processor_count = cores[i].processor_count;
		} else if (prev_uarch != cpuinfo_uarch_unknown) {
			uarchs[uarch_index].core_count++;
			uarchs[uarch_index].processor_count += cores[i].processor_count;
		}
	}

	/* 7. Commit changes */
	cpuinfo_processors = processors;
	cpuinfo_packages = packages;
	cpuinfo_clusters = clusters;
	cpuinfo_cores = cores;
	cpuinfo_uarchs = uarchs;

	cpuinfo_processors_count = nr_of_processors;
	cpuinfo_packages_count = nr_of_packages;
	cpuinfo_clusters_count = nr_of_cores;
	cpuinfo_cores_count = nr_of_cores;
	cpuinfo_uarchs_count = nr_of_uarchs;

	for (uint32_t i = 0; i < MAX_NR_OF_CACHES; i++) {
		cpuinfo_cache_count[i] = numbers_of_caches[i];
	}
	cpuinfo_cache[cpuinfo_cache_level_1i] = caches;
	cpuinfo_cache[cpuinfo_cache_level_1d] = cpuinfo_cache[cpuinfo_cache_level_1i] + cpuinfo_cache_count[cpuinfo_cache_level_1i];
	cpuinfo_cache[cpuinfo_cache_level_2]  = cpuinfo_cache[cpuinfo_cache_level_1d] + cpuinfo_cache_count[cpuinfo_cache_level_1d];
	cpuinfo_cache[cpuinfo_cache_level_3]  = cpuinfo_cache[cpuinfo_cache_level_2]  + cpuinfo_cache_count[cpuinfo_cache_level_2];
	cpuinfo_cache[cpuinfo_cache_level_4]  = cpuinfo_cache[cpuinfo_cache_level_3]  + cpuinfo_cache_count[cpuinfo_cache_level_3];
	cpuinfo_max_cache_size = cpuinfo_compute_max_cache_size(&processors[0]);

	result = true;
	MemoryBarrier();

	processors = NULL;
	packages = NULL;
	clusters = NULL;
	cores = NULL;
	caches = NULL;
	uarchs = NULL;

clean_up:
	/* The propagated pointers, shouldn't be freed, only in case of error
	 * and unfinished init.
	 */
	if (processors != NULL) {
		HeapFree(heap, 0, processors);
	}
	if (packages != NULL) {
		HeapFree(heap, 0, packages);
	}
	if (clusters != NULL) {
		HeapFree(heap, 0, clusters);
	}
	if (cores != NULL) {
		HeapFree(heap, 0, cores);
	}
	if (caches != NULL) {
		HeapFree(heap, 0, caches);
	}
	if (uarchs != NULL) {
		HeapFree(heap, 0, uarchs);
	}

	/* Free the locally used temporary pointers */
	HeapFree(heap, 0, global_proc_index_per_group);
	global_proc_index_per_group = NULL;
	return result;
}

static uint32_t count_logical_processors(
	const uint32_t max_group_count,
	uint32_t* global_proc_index_per_group)
{
	uint32_t nr_of_processors = 0;

	for (uint32_t i = 0; i < max_group_count; i++) {
		uint32_t nr_of_processors_per_group = GetMaximumProcessorCount((WORD) i);
		cpuinfo_log_debug("detected %"PRIu32" processor(s) in group %"PRIu32"",
			nr_of_processors_per_group, i);
		global_proc_index_per_group[i] = nr_of_processors;
		nr_of_processors += nr_of_processors_per_group;
	}
	return nr_of_processors;
}

static uint32_t read_packages_for_processors(
	struct cpuinfo_processor* processors,
	const uint32_t number_of_processors,
	const uint32_t* global_proc_index_per_group,
	const struct woa_chip_info *chip_info)
{
	return read_all_logical_processor_info_of_relation(
		RelationProcessorPackage,
		processors,
		number_of_processors,
		NULL,
		NULL,
		NULL,
		global_proc_index_per_group,
		chip_info);
}

uint32_t read_cores_for_processors(
	struct cpuinfo_processor* processors,
	const uint32_t number_of_processors,
	const uint32_t* global_proc_index_per_group,
	struct cpuinfo_core* cores,
	const struct woa_chip_info *chip_info)
{
	return read_all_logical_processor_info_of_relation(
		RelationProcessorCore,
		processors,
		number_of_processors,
		NULL,
		NULL,
		cores,
		global_proc_index_per_group,
		chip_info);
}

static uint32_t read_caches_for_processors(
	struct cpuinfo_processor* processors,
	const uint32_t number_of_processors,
	struct cpuinfo_cache* caches,
	uint32_t* numbers_of_caches,
	const uint32_t* global_proc_index_per_group,
	const struct woa_chip_info *chip_info)
{
	/* Reset processor start indexes */
	if (caches) {
		uint32_t cache_offset = 0;
		for (uint32_t i = 0; i < MAX_NR_OF_CACHES; i++) {
			for (uint32_t j = 0; j < numbers_of_caches[i]; j++) {
				caches[cache_offset + j].processor_start = UINT32_MAX;
			}
			cache_offset += numbers_of_caches[i];
		}
	}

	return read_all_logical_processor_info_of_relation(
		RelationCache,
		processors,
		number_of_processors,
		caches,
		numbers_of_caches,
		NULL,
		global_proc_index_per_group,
		chip_info);
}

static uint32_t read_all_logical_processor_info_of_relation(
	LOGICAL_PROCESSOR_RELATIONSHIP info_type,
	struct cpuinfo_processor* processors,
	const uint32_t number_of_processors,
	struct cpuinfo_cache* caches,
	uint32_t* numbers_of_caches,
	struct cpuinfo_core* cores,
	const uint32_t* global_proc_index_per_group,
	const struct woa_chip_info* chip_info)
{
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX infos = NULL;
	uint32_t nr_of_structs = 0;
	DWORD info_size = 0;
	bool result = false;
	HANDLE heap = GetProcessHeap();

	/* 1. Query the size of the information structure first */
	if (GetLogicalProcessorInformationEx(info_type, NULL, &info_size) == FALSE) {
		const DWORD last_error = GetLastError();
		if (last_error != ERROR_INSUFFICIENT_BUFFER) {
			cpuinfo_log_error(
				"failed to query size of processor %"PRIu32" information information: error %"PRIu32"",
				(uint32_t)info_type, (uint32_t) last_error);
			goto clean_up;
		}
	}
	/* 2. Allocate memory for the information structure */
	infos = HeapAlloc(heap, 0, info_size);
	if (infos == NULL) {
		cpuinfo_log_error("failed to allocate %"PRIu32" bytes for logical processor information",
			(uint32_t) info_size);
		goto clean_up;
	}
	/* 3. Read the information structure */
	if (GetLogicalProcessorInformationEx(info_type, infos, &info_size) == FALSE) {
		cpuinfo_log_error("failed to query processor %"PRIu32" information: error %"PRIu32"",
			(uint32_t)info_type, (uint32_t) GetLastError());
		goto clean_up;
	}

	/* 4. Parse the structure and store relevant data */
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info_end =
		(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) ((uintptr_t) infos + info_size);
	for (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info = infos;
		info < info_end;
		info = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX) ((uintptr_t) info + info->Size))
	{
		if (info->Relationship != info_type) {
			cpuinfo_log_warning(
				"unexpected processor info type (%"PRIu32") for processor information",
				(uint32_t) info->Relationship);
			continue;
		}

		const uint32_t info_id = nr_of_structs++;

		switch(info_type) {
			case RelationProcessorPackage:
				result = parse_relation_processor_info(
							processors,
							number_of_processors,
							global_proc_index_per_group,
							info,
							info_id,
							cores,
							chip_info);
			break;
			case RelationProcessorCore:
				result = parse_relation_processor_info(
							processors,
							number_of_processors,
							global_proc_index_per_group,
							info,
							info_id,
							cores,
							chip_info);
			break;
			case RelationCache:
				result = parse_relation_cache_info(
							processors,
							caches,
							numbers_of_caches,
							global_proc_index_per_group,
							info);
			break;
			default:
				cpuinfo_log_error(
					"unexpected processor info type (%"PRIu32") for processor information",
					(uint32_t) info->Relationship);
				result = false;
			break;
		}
		if (!result) {
			nr_of_structs = 0;
			goto clean_up;
		}
	}
clean_up:
	/* 5. Release dynamically allocated info structure. */
	HeapFree(heap, 0, infos);
	infos = NULL;
	return nr_of_structs;
}

static bool parse_relation_processor_info(
	struct cpuinfo_processor* processors,
	uint32_t nr_of_processors,
	const uint32_t* global_proc_index_per_group,
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info,
	const uint32_t info_id,
	struct cpuinfo_core* cores,
	const struct woa_chip_info *chip_info)
{
	for (uint32_t i = 0; i < info->Processor.GroupCount; i++) {
		const uint32_t group_id = info->Processor.GroupMask[i].Group;
		/* Bitmask representing processors in this group belonging to this package */
		KAFFINITY group_processors_mask = info->Processor.GroupMask[i].Mask;
		while (group_processors_mask != 0) {
			const uint32_t processor_id_in_group =
				low_index_from_kaffinity(group_processors_mask);
			const uint32_t processor_global_index =
				global_proc_index_per_group[group_id] + processor_id_in_group;

			if(processor_global_index >= nr_of_processors) {
				cpuinfo_log_error("unexpected processor index %"PRIu32"",
					processor_global_index);
				return false;
			}

			switch(info->Relationship) {
				case RelationProcessorPackage:
					store_package_info_per_processor(
						processors, processor_global_index, info_id,
						group_id, processor_id_in_group);
				break;
				case RelationProcessorCore:
					store_core_info_per_processor(
						processors, processor_global_index,
						info_id, info,
						cores, chip_info);
				break;
				default:
					cpuinfo_log_error(
						"unexpected processor info type (%"PRIu32") for processor information",
						(uint32_t) info->Relationship);
				break;
			}
			/* Clear the bits in affinity mask, lower the least set bit. */
			group_processors_mask &= (group_processors_mask - 1);
		}
	}
	return true;
}

static bool parse_relation_cache_info(
	struct cpuinfo_processor* processors,
	struct cpuinfo_cache* caches,
	uint32_t* numbers_of_caches,
	const uint32_t* global_proc_index_per_group,
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info)
{
	static uint32_t l1i_counter = 0;
	static uint32_t l1d_counter = 0;
	static uint32_t l2_counter = 0;
	static uint32_t l3_counter = 0;

	/* Count cache types for allocation at first. */
	if (caches == NULL) {
		switch(info->Cache.Level) {
			case 1:
				switch (info->Cache.Type) {
					case CacheInstruction:
						numbers_of_caches[cpuinfo_cache_level_1i]++;
					break;
					case CacheData:
						numbers_of_caches[cpuinfo_cache_level_1d]++;
					break;
					case CacheUnified:
					break;
					case CacheTrace:
					break;
					default:
					break;
				}
			break;
			case 2:
				numbers_of_caches[cpuinfo_cache_level_2]++;
			break;
			case 3:
				numbers_of_caches[cpuinfo_cache_level_3]++;
			break;
		}
		return true;
	}
	struct cpuinfo_cache* l1i_base = caches;
	struct cpuinfo_cache* l1d_base = l1i_base + numbers_of_caches[cpuinfo_cache_level_1i];
	struct cpuinfo_cache* l2_base  = l1d_base + numbers_of_caches[cpuinfo_cache_level_1d];
	struct cpuinfo_cache* l3_base  = l2_base  + numbers_of_caches[cpuinfo_cache_level_2];

	cpuinfo_log_debug(
		"info->Cache.GroupCount:%"PRIu32", info->Cache.GroupMask:%"PRIu32","
		"info->Cache.Level:%"PRIu32", info->Cache.Associativity:%"PRIu32","
		"info->Cache.LineSize:%"PRIu32","
		"info->Cache.CacheSize:%"PRIu32", info->Cache.Type:%"PRIu32"",
		info->Cache.GroupCount, (unsigned int)info->Cache.GroupMask.Mask,
		info->Cache.Level, info->Cache.Associativity, info->Cache.LineSize,
		info->Cache.CacheSize, info->Cache.Type);

	struct cpuinfo_cache* current_cache = NULL;
	switch (info->Cache.Level) {
		case 1:
			switch (info->Cache.Type) {
				case CacheInstruction:
					current_cache = l1i_base + l1i_counter;
					l1i_counter++;
				break;
				case CacheData:
					current_cache = l1d_base + l1d_counter;
					l1d_counter++;
				break;
				case CacheUnified:
				break;
				case CacheTrace:
				break;
				default:
				break;
			}
		break;
		case 2:
			current_cache = l2_base + l2_counter;
			l2_counter++;
		break;
		case 3:
			current_cache = l3_base + l3_counter;
			l3_counter++;
		break;
	}
	current_cache->size = info->Cache.CacheSize;
	current_cache->line_size = info->Cache.LineSize;
	current_cache->associativity = info->Cache.Associativity;
	/* We don't have partition and set information of caches on Windows,
	 * so we set partitions to 1 and calculate the expected sets.
	 */
	current_cache->partitions = 1;
	current_cache->sets =
		current_cache->size / current_cache->line_size / current_cache->associativity;
	if (info->Cache.Type == CacheUnified) {
		current_cache->flags = CPUINFO_CACHE_UNIFIED;
	}

	for (uint32_t i = 0; i < info->Cache.GroupCount; i++) {
	/* Zero GroupCount is valid, GroupMask still can store bits set. */
		const uint32_t group_id = info->Cache.GroupMasks[i].Group;
		/* Bitmask representing processors in this group belonging to this package */
		KAFFINITY group_processors_mask = info->Cache.GroupMasks[i].Mask;
		while (group_processors_mask != 0) {
			const uint32_t processor_id_in_group =
				low_index_from_kaffinity(group_processors_mask);
			const uint32_t processor_global_index =
				global_proc_index_per_group[group_id] + processor_id_in_group;

			store_cache_info_per_processor(
				processors, processor_global_index,
				info, current_cache);

			/* Clear the bits in affinity mask, lower the least set bit. */
			group_processors_mask &= (group_processors_mask - 1);
		}
	}
	return true;
}

static void store_package_info_per_processor(
	struct cpuinfo_processor* processors,
	const uint32_t processor_global_index,
	const uint32_t package_id,
	const uint32_t group_id,
	const uint32_t processor_id_in_group)
{
	processors[processor_global_index].windows_group_id =
		(uint16_t) group_id;
	processors[processor_global_index].windows_processor_id =
		(uint16_t) processor_id_in_group;

	/* As we're counting the number of packages now, we haven't allocated memory for
	 * cpuinfo_packages yet, so we only set the package pointer's offset now.
	 */
	processors[processor_global_index].package =
		(const struct cpuinfo_package*) NULL + package_id;
}

void store_core_info_per_processor(
	struct cpuinfo_processor* processors,
	const uint32_t processor_global_index,
	const uint32_t core_id,
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX core_info,
	struct cpuinfo_core* cores,
	const struct woa_chip_info *chip_info)
{
	if (cores) {
		processors[processor_global_index].core = cores + core_id;
		cores[core_id].core_id = core_id;
		get_core_uarch_for_efficiency(
			chip_info->chip_name, core_info->Processor.EfficiencyClass,
			&(cores[core_id].uarch), &(cores[core_id].frequency));

		/* We don't have cluster information, so we handle it as
		 * fixed 1 to (cluster / cores).
		 * Set the cluster offset ID now, as soon as we have the
		 * cluster base address, we'll set the absolute address.
		 */
		processors[processor_global_index].cluster =
			(const struct cpuinfo_cluster*) NULL + core_id;
	}
}

static void store_cache_info_per_processor(
	struct cpuinfo_processor* processors,
	const uint32_t processor_global_index,
	PSYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX info,
	struct cpuinfo_cache* current_cache)
{
	if (current_cache->processor_start > processor_global_index) {
		current_cache->processor_start = processor_global_index;
	}
	current_cache->processor_count++;

	switch(info->Cache.Level) {
		case 1:
			switch (info->Cache.Type) {
				case CacheInstruction:
					processors[processor_global_index].cache.l1i = current_cache;
				break;
				case CacheData:
					processors[processor_global_index].cache.l1d = current_cache;
				break;
				case CacheUnified:
				break;
				case CacheTrace:
				break;
				default:
				break;
			}
		break;
		case 2:
			processors[processor_global_index].cache.l2 = current_cache;
		break;
		case 3:
			processors[processor_global_index].cache.l3 = current_cache;
		break;
	}
}

static bool connect_packages_cores_clusters_by_processors(
	struct cpuinfo_processor* processors,
	const uint32_t nr_of_processors,
	struct cpuinfo_package* packages,
	const uint32_t nr_of_packages,
	struct cpuinfo_cluster* clusters,
	struct cpuinfo_core* cores,
	const uint32_t nr_of_cores,
	const struct woa_chip_info* chip_info,
	enum cpuinfo_vendor vendor)
{
	/* Adjust core and package pointers for all logical processors. */
	for (uint32_t i = nr_of_processors; i != 0; i--) {
		const uint32_t processor_id = i - 1;
		struct cpuinfo_processor* processor = processors + processor_id;

		struct cpuinfo_core* core = (struct cpuinfo_core*)processor->core;

		/* We stored the offset of pointers when we haven't allocated memory
		 * for packages and clusters, so now add offsets to base addresses.
		 */
		struct cpuinfo_package* package =
			(struct cpuinfo_package*) ((uintptr_t) packages + (uintptr_t) processor->package);
		if (package < packages ||
			package >= (packages + nr_of_packages)) {
			cpuinfo_log_error("invalid package indexing");
			return false;
		}
		processor->package = package;

		struct cpuinfo_cluster* cluster =
			(struct cpuinfo_cluster*) ((uintptr_t) clusters + (uintptr_t) processor->cluster);
		if (cluster < clusters ||
			cluster >= (clusters + nr_of_cores)) {
			cpuinfo_log_error("invalid cluster indexing");
			return false;
		}
		processor->cluster = cluster;

		if (chip_info) {
			size_t converted_chars = 0;
			if (!WideCharToMultiByte(
					CP_UTF8,
					WC_ERR_INVALID_CHARS,
					chip_info->chip_name_string,
					-1,
					package->name,
					CPUINFO_PACKAGE_NAME_MAX,
					NULL,
					NULL)) {
				cpuinfo_log_error("cpu name character conversion error");
				return false;
			};
		}

		/* Set start indexes and counts per packages / clusters / cores - going backwards */

		/* This can be overwritten by lower-index processors on the same package. */
		package->processor_start = processor_id;
		package->processor_count++;

		/* This can be overwritten by lower-index processors on the same cluster. */
		cluster->processor_start = processor_id;
		cluster->processor_count++;

		/* This can be overwritten by lower-index processors on the same core. */
		core->processor_start = processor_id;
		core->processor_count++;
	}
	/* Fill cores */
	for (uint32_t i = nr_of_cores; i != 0; i--) {
		const uint32_t global_core_id = i - 1;
		struct cpuinfo_core* core = cores + global_core_id;
		const struct cpuinfo_processor* processor = processors + core->processor_start;
		struct cpuinfo_package* package = (struct cpuinfo_package*) processor->package;
		struct cpuinfo_cluster* cluster = (struct cpuinfo_cluster*) processor->cluster;

		core->package = package;
		core->cluster = cluster;
		core->vendor = vendor;

		/* This can be overwritten by lower-index cores on the same cluster/package. */
		cluster->core_start = global_core_id;
		cluster->core_count++;
		package->core_start = global_core_id;
		package->core_count++;
		package->cluster_start = global_core_id;
		package->cluster_count = package->core_count;

		cluster->package = package;
		cluster->vendor = cores[cluster->core_start].vendor;
		cluster->uarch = cores[cluster->core_start].uarch;
		cluster->frequency = cores[cluster->core_start].frequency;
	}
	return true;
}

static inline uint32_t low_index_from_kaffinity(KAFFINITY kaffinity) {
	unsigned long index;
	_BitScanForward64(&index, (unsigned __int64) kaffinity);
	return (uint32_t) index;
}
