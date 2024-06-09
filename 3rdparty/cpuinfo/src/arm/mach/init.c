#include <alloca.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <mach/machine.h>
#include <sys/sysctl.h>
#include <sys/types.h>

#include <cpuinfo.h>
#include <cpuinfo/internal-api.h>
#include <cpuinfo/log.h>
#include <mach/api.h>

/* Polyfill recent CPUFAMILY_ARM_* values for older SDKs */
#ifndef CPUFAMILY_ARM_VORTEX_TEMPEST
#define CPUFAMILY_ARM_VORTEX_TEMPEST 0x07D34B9F
#endif
#ifndef CPUFAMILY_ARM_LIGHTNING_THUNDER
#define CPUFAMILY_ARM_LIGHTNING_THUNDER 0x462504D2
#endif
#ifndef CPUFAMILY_ARM_FIRESTORM_ICESTORM
#define CPUFAMILY_ARM_FIRESTORM_ICESTORM 0x1B588BB3
#endif
#ifndef CPUFAMILY_ARM_AVALANCHE_BLIZZARD
#define CPUFAMILY_ARM_AVALANCHE_BLIZZARD 0xDA33D83D
#endif

struct cpuinfo_arm_isa cpuinfo_isa = {
	.aes = true,
	.sha1 = true,
	.sha2 = true,
	.pmull = true,
	.crc32 = true,
};

static uint32_t get_sys_info(int type_specifier, const char* name) {
	size_t size = 0;
	uint32_t result = 0;
	int mib[2] = {CTL_HW, type_specifier};
	if (sysctl(mib, 2, NULL, &size, NULL, 0) != 0) {
		cpuinfo_log_info("sysctl(\"%s\") failed: %s", name, strerror(errno));
	} else if (size == sizeof(uint32_t)) {
		sysctl(mib, 2, &result, &size, NULL, 0);
		cpuinfo_log_debug("%s: %" PRIu32 ", size = %lu", name, result, size);
	} else {
		cpuinfo_log_info("sysctl does not support non-integer lookup for (\"%s\")", name);
	}
	return result;
}

static uint32_t get_sys_info_by_name(const char* type_specifier) {
	size_t size = 0;
	uint32_t result = 0;
	if (sysctlbyname(type_specifier, NULL, &size, NULL, 0) != 0) {
		cpuinfo_log_info("sysctlbyname(\"%s\") failed: %s", type_specifier, strerror(errno));
	} else if (size == sizeof(uint32_t)) {
		sysctlbyname(type_specifier, &result, &size, NULL, 0);
		cpuinfo_log_debug("%s: %" PRIu32 ", size = %lu", type_specifier, result, size);
	} else {
		cpuinfo_log_info("sysctl does not support non-integer lookup for (\"%s\")", type_specifier);
	}
	return result;
}

static enum cpuinfo_uarch decode_uarch(uint32_t cpu_family, uint32_t core_index, uint32_t core_count) {
	switch (cpu_family) {
		case CPUFAMILY_ARM_CYCLONE:
			return cpuinfo_uarch_cyclone;
		case CPUFAMILY_ARM_TYPHOON:
			return cpuinfo_uarch_typhoon;
		case CPUFAMILY_ARM_TWISTER:
			return cpuinfo_uarch_twister;
		case CPUFAMILY_ARM_HURRICANE:
			return cpuinfo_uarch_hurricane;
		case CPUFAMILY_ARM_MONSOON_MISTRAL:
			/* 2x Monsoon + 4x Mistral cores */
			return core_index < 2 ? cpuinfo_uarch_monsoon : cpuinfo_uarch_mistral;
		case CPUFAMILY_ARM_VORTEX_TEMPEST:
			/* Hexa-core: 2x Vortex + 4x Tempest; Octa-core: 4x
			 * Cortex + 4x Tempest */
			return core_index + 4 < core_count ? cpuinfo_uarch_vortex : cpuinfo_uarch_tempest;
		case CPUFAMILY_ARM_LIGHTNING_THUNDER:
			/* Hexa-core: 2x Lightning + 4x Thunder; Octa-core
			 * (presumed): 4x Lightning + 4x Thunder */
			return core_index + 4 < core_count ? cpuinfo_uarch_lightning : cpuinfo_uarch_thunder;
		case CPUFAMILY_ARM_FIRESTORM_ICESTORM:
			/* Hexa-core: 2x Firestorm + 4x Icestorm; Octa-core: 4x
			 * Firestorm + 4x Icestorm */
			return core_index + 4 < core_count ? cpuinfo_uarch_firestorm : cpuinfo_uarch_icestorm;
		case CPUFAMILY_ARM_AVALANCHE_BLIZZARD:
			/* Hexa-core: 2x Avalanche + 4x Blizzard */
			return core_index + 4 < core_count ? cpuinfo_uarch_avalanche : cpuinfo_uarch_blizzard;
		default:
			/* Use hw.cpusubtype for detection */
			break;
	}

	return cpuinfo_uarch_unknown;
}

static void decode_package_name(char* package_name) {
	size_t size;
	if (sysctlbyname("hw.machine", NULL, &size, NULL, 0) != 0) {
		cpuinfo_log_warning("sysctlbyname(\"hw.machine\") failed: %s", strerror(errno));
		return;
	}

	char* machine_name = alloca(size);
	if (sysctlbyname("hw.machine", machine_name, &size, NULL, 0) != 0) {
		cpuinfo_log_warning("sysctlbyname(\"hw.machine\") failed: %s", strerror(errno));
		return;
	}
	cpuinfo_log_debug("hw.machine: %s", machine_name);

	char name[10];
	uint32_t major = 0, minor = 0;
	if (sscanf(machine_name, "%9[^,0123456789]%" SCNu32 ",%" SCNu32, name, &major, &minor) != 3) {
		cpuinfo_log_warning("parsing \"hw.machine\" failed: %s", strerror(errno));
		return;
	}

	uint32_t chip_model = 0;
	char suffix = '\0';
	if (strcmp(name, "iPhone") == 0) {
		/*
		 * iPhone 4 and up are supported:
		 *  - iPhone 4       [A4]:  iPhone3,1, iPhone3,2, iPhone3,3
		 *  - iPhone 4S      [A5]:  iPhone4,1
		 *  - iPhone 5       [A6]:  iPhone5,1, iPhone5,2
		 *  - iPhone 5c      [A6]:  iPhone5,3, iPhone5,4
		 *  - iPhone 5s      [A7]:  iPhone6,1, iPhone6,2
		 *  - iPhone 6       [A8]:  iPhone7,2
		 *  - iPhone 6 Plus  [A8]:  iPhone7,1
		 *  - iPhone 6s      [A9]:  iPhone8,1
		 *  - iPhone 6s Plus [A9]:  iPhone8,2
		 *  - iPhone SE      [A9]:  iPhone8,4
		 *  - iPhone 7       [A10]: iPhone9,1, iPhone9,3
		 *  - iPhone 7 Plus  [A10]: iPhone9,2, iPhone9,4
		 *  - iPhone 8       [A11]: iPhone10,1, iPhone10,4
		 *  - iPhone 8 Plus  [A11]: iPhone10,2, iPhone10,5
		 *  - iPhone X       [A11]: iPhone10,3, iPhone10,6
		 *  - iPhone XS      [A12]: iPhone11,2,
		 *  - iPhone XS Max  [A12]: iPhone11,4, iPhone11,6
		 *  - iPhone XR      [A12]: iPhone11,8
		 */
		chip_model = major + 1;
	} else if (strcmp(name, "iPad") == 0) {
		switch (major) {
			/* iPad 2 and up are supported */
			case 2:
				/*
				 * iPad 2    [A5]: iPad2,1, iPad2,2, iPad2,3,
				 * iPad2,4 iPad mini [A5]: iPad2,5, iPad2,6,
				 * iPad2,7
				 */
				chip_model = major + 3;
				break;
			case 3:
				/*
				 * iPad 3rd Gen [A5X]: iPad3,1, iPad3,2, iPad3,3
				 * iPad 4th Gen [A6X]: iPad3,4, iPad3,5, iPad3,6
				 */
				chip_model = (minor <= 3) ? 5 : 6;
				suffix = 'X';
				break;
			case 4:
				/*
				 * iPad Air         [A7]: iPad4,1, iPad4,2,
				 * iPad4,3 iPad mini Retina [A7]: iPad4,4,
				 * iPad4,5, iPad4,6 iPad mini 3      [A7]:
				 * iPad4,7, iPad4,8, iPad4,9
				 */
				chip_model = major + 3;
				break;
			case 5:
				/*
				 * iPad mini 4 [A8]:  iPad5,1, iPad5,2
				 * iPad Air 2  [A8X]: iPad5,3, iPad5,4
				 */
				chip_model = major + 3;
				suffix = (minor <= 2) ? '\0' : 'X';
				break;
			case 6:
				/*
				 * iPad Pro 9.7" [A9X]: iPad6,3, iPad6,4
				 * iPad Pro      [A9X]: iPad6,7, iPad6,8
				 * iPad 5th Gen  [A9]:  iPad6,11, iPad6,12
				 */
				chip_model = major + 3;
				suffix = minor <= 8 ? 'X' : '\0';
				break;
			case 7:
				/*
				 * iPad Pro 12.9" [A10X]: iPad7,1, iPad7,2
				 * iPad Pro 10.5" [A10X]: iPad7,3, iPad7,4
				 * iPad 6th Gen   [A10]:  iPad7,5, iPad7,6
				 */
				chip_model = major + 3;
				suffix = minor <= 4 ? 'X' : '\0';
				break;
			default:
				cpuinfo_log_info("unknown iPad: %s", machine_name);
				break;
		}
	} else if (strcmp(name, "iPod") == 0) {
		switch (major) {
			case 5:
				chip_model = 5;
				break;
				/* iPod touch (5th Gen) [A5]: iPod5,1 */
			case 7:
				/* iPod touch (6th Gen, 2015) [A8]: iPod7,1 */
				chip_model = 8;
				break;
			default:
				cpuinfo_log_info("unknown iPod: %s", machine_name);
				break;
		}
	} else {
		cpuinfo_log_info("unknown device: %s", machine_name);
	}
	if (chip_model != 0) {
		snprintf(package_name, CPUINFO_PACKAGE_NAME_MAX, "Apple A%" PRIu32 "%c", chip_model, suffix);
	}
}

void cpuinfo_arm_mach_init(void) {
	struct cpuinfo_processor* processors = NULL;
	struct cpuinfo_core* cores = NULL;
	struct cpuinfo_cluster* clusters = NULL;
	struct cpuinfo_package* packages = NULL;
	struct cpuinfo_uarch_info* uarchs = NULL;
	struct cpuinfo_cache* l1i = NULL;
	struct cpuinfo_cache* l1d = NULL;
	struct cpuinfo_cache* l2 = NULL;
	struct cpuinfo_cache* l3 = NULL;

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
	packages = calloc(mach_topology.packages, sizeof(struct cpuinfo_package));
	if (packages == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " packages",
			mach_topology.packages * sizeof(struct cpuinfo_package),
			mach_topology.packages);
		goto cleanup;
	}

	const uint32_t threads_per_core = mach_topology.threads / mach_topology.cores;
	const uint32_t threads_per_package = mach_topology.threads / mach_topology.packages;
	const uint32_t cores_per_package = mach_topology.cores / mach_topology.packages;

	for (uint32_t i = 0; i < mach_topology.packages; i++) {
		packages[i] = (struct cpuinfo_package){
			.processor_start = i * threads_per_package,
			.processor_count = threads_per_package,
			.core_start = i * cores_per_package,
			.core_count = cores_per_package,
		};
		decode_package_name(packages[i].name);
	}

	const uint32_t cpu_family = get_sys_info_by_name("hw.cpufamily");

	/*
	 * iOS 15 and macOS 12 added sysctls for ARM features, use them where
	 * possible. Otherwise, fallback to hardcoded set of CPUs with known
	 * support.
	 */
	const uint32_t has_feat_lse = get_sys_info_by_name("hw.optional.arm.FEAT_LSE");
	if (has_feat_lse != 0) {
		cpuinfo_isa.atomics = true;
	} else {
		// Mandatory in ARMv8.1-A, list only cores released before iOS
		// 15 / macOS 12
		switch (cpu_family) {
			case CPUFAMILY_ARM_MONSOON_MISTRAL:
			case CPUFAMILY_ARM_VORTEX_TEMPEST:
			case CPUFAMILY_ARM_LIGHTNING_THUNDER:
			case CPUFAMILY_ARM_FIRESTORM_ICESTORM:
				cpuinfo_isa.atomics = true;
		}
	}

	const uint32_t has_feat_rdm = get_sys_info_by_name("hw.optional.arm.FEAT_RDM");
	if (has_feat_rdm != 0) {
		cpuinfo_isa.rdm = true;
	} else {
		// Optional in ARMv8.2-A (implemented in Apple cores),
		// list only cores released before iOS 15 / macOS 12
		switch (cpu_family) {
			case CPUFAMILY_ARM_MONSOON_MISTRAL:
			case CPUFAMILY_ARM_VORTEX_TEMPEST:
			case CPUFAMILY_ARM_LIGHTNING_THUNDER:
			case CPUFAMILY_ARM_FIRESTORM_ICESTORM:
				cpuinfo_isa.rdm = true;
		}
	}

	const uint32_t has_feat_fp16 = get_sys_info_by_name("hw.optional.arm.FEAT_FP16");
	if (has_feat_fp16 != 0) {
		cpuinfo_isa.fp16arith = true;
	} else {
		// Optional in ARMv8.2-A (implemented in Apple cores),
		// list only cores released before iOS 15 / macOS 12
		switch (cpu_family) {
			case CPUFAMILY_ARM_MONSOON_MISTRAL:
			case CPUFAMILY_ARM_VORTEX_TEMPEST:
			case CPUFAMILY_ARM_LIGHTNING_THUNDER:
			case CPUFAMILY_ARM_FIRESTORM_ICESTORM:
				cpuinfo_isa.fp16arith = true;
		}
	}

	const uint32_t has_feat_fhm = get_sys_info_by_name("hw.optional.arm.FEAT_FHM");
	if (has_feat_fhm != 0) {
		cpuinfo_isa.fhm = true;
	} else {
		// Prior to iOS 15, use 'hw.optional.armv8_2_fhm'
		const uint32_t has_feat_fhm_legacy = get_sys_info_by_name("hw.optional.armv8_2_fhm");
		if (has_feat_fhm_legacy != 0) {
			cpuinfo_isa.fhm = true;
		} else {
			// Mandatory in ARMv8.4-A when FP16 arithmetics is
			// implemented, list only cores released before iOS 15 /
			// macOS 12
			switch (cpu_family) {
				case CPUFAMILY_ARM_LIGHTNING_THUNDER:
				case CPUFAMILY_ARM_FIRESTORM_ICESTORM:
					cpuinfo_isa.fhm = true;
			}
		}
	}

	const uint32_t has_feat_bf16 = get_sys_info_by_name("hw.optional.arm.FEAT_BF16");
	if (has_feat_bf16 != 0) {
		cpuinfo_isa.bf16 = true;
	}

	const uint32_t has_feat_fcma = get_sys_info_by_name("hw.optional.arm.FEAT_FCMA");
	if (has_feat_fcma != 0) {
		cpuinfo_isa.fcma = true;
	} else {
		// Mandatory in ARMv8.3-A, list only cores released before iOS
		// 15 / macOS 12
		switch (cpu_family) {
			case CPUFAMILY_ARM_LIGHTNING_THUNDER:
			case CPUFAMILY_ARM_FIRESTORM_ICESTORM:
				cpuinfo_isa.fcma = true;
		}
	}

	const uint32_t has_feat_jscvt = get_sys_info_by_name("hw.optional.arm.FEAT_JSCVT");
	if (has_feat_jscvt != 0) {
		cpuinfo_isa.jscvt = true;
	} else {
		// Mandatory in ARMv8.3-A, list only cores released before iOS
		// 15 / macOS 12
		switch (cpu_family) {
			case CPUFAMILY_ARM_LIGHTNING_THUNDER:
			case CPUFAMILY_ARM_FIRESTORM_ICESTORM:
				cpuinfo_isa.jscvt = true;
		}
	}

	const uint32_t has_feat_dotprod = get_sys_info_by_name("hw.optional.arm.FEAT_DotProd");
	if (has_feat_dotprod != 0) {
		cpuinfo_isa.dot = true;
	} else {
		// Mandatory in ARMv8.4-A, list only cores released before iOS
		// 15 / macOS 12
		switch (cpu_family) {
			case CPUFAMILY_ARM_LIGHTNING_THUNDER:
			case CPUFAMILY_ARM_FIRESTORM_ICESTORM:
				cpuinfo_isa.dot = true;
		}
	}

	const uint32_t has_feat_i8mm = get_sys_info_by_name("hw.optional.arm.FEAT_I8MM");
	if (has_feat_i8mm != 0) {
		cpuinfo_isa.i8mm = true;
	}

	uint32_t num_clusters = 1;
	for (uint32_t i = 0; i < mach_topology.cores; i++) {
		cores[i] = (struct cpuinfo_core){
			.processor_start = i * threads_per_core,
			.processor_count = threads_per_core,
			.core_id = i % cores_per_package,
			.package = packages + i / cores_per_package,
			.vendor = cpuinfo_vendor_apple,
			.uarch = decode_uarch(cpu_family, i, mach_topology.cores),
		};
		if (i != 0 && cores[i].uarch != cores[i - 1].uarch) {
			num_clusters++;
		}
	}
	for (uint32_t i = 0; i < mach_topology.threads; i++) {
		const uint32_t smt_id = i % threads_per_core;
		const uint32_t core_id = i / threads_per_core;
		const uint32_t package_id = i / threads_per_package;

		processors[i].smt_id = smt_id;
		processors[i].core = &cores[core_id];
		processors[i].package = &packages[package_id];
	}

	clusters = calloc(num_clusters, sizeof(struct cpuinfo_cluster));
	if (clusters == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " clusters",
			num_clusters * sizeof(struct cpuinfo_cluster),
			num_clusters);
		goto cleanup;
	}
	uarchs = calloc(num_clusters, sizeof(struct cpuinfo_uarch_info));
	if (uarchs == NULL) {
		cpuinfo_log_error(
			"failed to allocate %zu bytes for descriptions of %" PRIu32 " uarchs",
			num_clusters * sizeof(enum cpuinfo_uarch),
			num_clusters);
		goto cleanup;
	}
	uint32_t cluster_idx = UINT32_MAX;
	for (uint32_t i = 0; i < mach_topology.cores; i++) {
		if (i == 0 || cores[i].uarch != cores[i - 1].uarch) {
			cluster_idx++;
			uarchs[cluster_idx] = (struct cpuinfo_uarch_info){
				.uarch = cores[i].uarch,
				.processor_count = 1,
				.core_count = 1,
			};
			clusters[cluster_idx] = (struct cpuinfo_cluster){
				.processor_start = i * threads_per_core,
				.processor_count = 1,
				.core_start = i,
				.core_count = 1,
				.cluster_id = cluster_idx,
				.package = cores[i].package,
				.vendor = cores[i].vendor,
				.uarch = cores[i].uarch,
			};
		} else {
			uarchs[cluster_idx].processor_count++;
			uarchs[cluster_idx].core_count++;
			clusters[cluster_idx].processor_count++;
			clusters[cluster_idx].core_count++;
		}
		cores[i].cluster = &clusters[cluster_idx];
	}

	for (uint32_t i = 0; i < mach_topology.threads; i++) {
		const uint32_t core_id = i / threads_per_core;
		processors[i].cluster = cores[core_id].cluster;
	}

	for (uint32_t i = 0; i < mach_topology.packages; i++) {
		packages[i].cluster_start = 0;
		packages[i].cluster_count = num_clusters;
	}

	const uint32_t cacheline_size = get_sys_info(HW_CACHELINE, "HW_CACHELINE");
	const uint32_t l1d_cache_size = get_sys_info(HW_L1DCACHESIZE, "HW_L1DCACHESIZE");
	const uint32_t l1i_cache_size = get_sys_info(HW_L1ICACHESIZE, "HW_L1ICACHESIZE");
	const uint32_t l2_cache_size = get_sys_info(HW_L2CACHESIZE, "HW_L2CACHESIZE");
	const uint32_t l3_cache_size = get_sys_info(HW_L3CACHESIZE, "HW_L3CACHESIZE");
	const uint32_t l1_cache_associativity = 4;
	const uint32_t l2_cache_associativity = 8;
	const uint32_t l3_cache_associativity = 16;
	const uint32_t cache_partitions = 1;
	const uint32_t cache_flags = 0;

	uint32_t threads_per_l1 = 0, l1_count = 0;
	if (l1i_cache_size != 0 || l1d_cache_size != 0) {
		/* Assume L1 caches are private to each core */
		threads_per_l1 = 1;
		l1_count = mach_topology.threads / threads_per_l1;
		cpuinfo_log_debug("detected %" PRIu32 " L1 caches", l1_count);
	}

	uint32_t threads_per_l2 = 0, l2_count = 0;
	if (l2_cache_size != 0) {
		/* Assume L2 cache is shared between all cores */
		threads_per_l2 = mach_topology.cores;
		l2_count = 1;
		cpuinfo_log_debug("detected %" PRIu32 " L2 caches", l2_count);
	}

	uint32_t threads_per_l3 = 0, l3_count = 0;
	if (l3_cache_size != 0) {
		/* Assume L3 cache is shared between all cores */
		threads_per_l3 = mach_topology.cores;
		l3_count = 1;
		cpuinfo_log_debug("detected %" PRIu32 " L3 caches", l3_count);
	}

	if (l1i_cache_size != 0) {
		l1i = calloc(l1_count, sizeof(struct cpuinfo_cache));
		if (l1i == NULL) {
			cpuinfo_log_error(
				"failed to allocate %zu bytes for descriptions of %" PRIu32 " L1I caches",
				l1_count * sizeof(struct cpuinfo_cache),
				l1_count);
			goto cleanup;
		}
		for (uint32_t c = 0; c < l1_count; c++) {
			l1i[c] = (struct cpuinfo_cache){
				.size = l1i_cache_size,
				.associativity = l1_cache_associativity,
				.sets = l1i_cache_size / (l1_cache_associativity * cacheline_size),
				.partitions = cache_partitions,
				.line_size = cacheline_size,
				.flags = cache_flags,
				.processor_start = c * threads_per_l1,
				.processor_count = threads_per_l1,
			};
		}
		for (uint32_t t = 0; t < mach_topology.threads; t++) {
			processors[t].cache.l1i = &l1i[t / threads_per_l1];
		}
	}

	if (l1d_cache_size != 0) {
		l1d = calloc(l1_count, sizeof(struct cpuinfo_cache));
		if (l1d == NULL) {
			cpuinfo_log_error(
				"failed to allocate %zu bytes for descriptions of %" PRIu32 " L1D caches",
				l1_count * sizeof(struct cpuinfo_cache),
				l1_count);
			goto cleanup;
		}
		for (uint32_t c = 0; c < l1_count; c++) {
			l1d[c] = (struct cpuinfo_cache){
				.size = l1d_cache_size,
				.associativity = l1_cache_associativity,
				.sets = l1d_cache_size / (l1_cache_associativity * cacheline_size),
				.partitions = cache_partitions,
				.line_size = cacheline_size,
				.flags = cache_flags,
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
			goto cleanup;
		}
		for (uint32_t c = 0; c < l2_count; c++) {
			l2[c] = (struct cpuinfo_cache){
				.size = l2_cache_size,
				.associativity = l2_cache_associativity,
				.sets = l2_cache_size / (l2_cache_associativity * cacheline_size),
				.partitions = cache_partitions,
				.line_size = cacheline_size,
				.flags = cache_flags,
				.processor_start = c * threads_per_l2,
				.processor_count = threads_per_l2,
			};
		}
		for (uint32_t t = 0; t < mach_topology.threads; t++) {
			processors[t].cache.l2 = &l2[0];
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
		for (uint32_t c = 0; c < l3_count; c++) {
			l3[c] = (struct cpuinfo_cache){
				.size = l3_cache_size,
				.associativity = l3_cache_associativity,
				.sets = l3_cache_size / (l3_cache_associativity * cacheline_size),
				.partitions = cache_partitions,
				.line_size = cacheline_size,
				.flags = cache_flags,
				.processor_start = c * threads_per_l3,
				.processor_count = threads_per_l3,
			};
		}
		for (uint32_t t = 0; t < mach_topology.threads; t++) {
			processors[t].cache.l3 = &l3[0];
		}
	}

	/* Commit changes */
	cpuinfo_processors = processors;
	cpuinfo_cores = cores;
	cpuinfo_clusters = clusters;
	cpuinfo_packages = packages;
	cpuinfo_uarchs = uarchs;
	cpuinfo_cache[cpuinfo_cache_level_1i] = l1i;
	cpuinfo_cache[cpuinfo_cache_level_1d] = l1d;
	cpuinfo_cache[cpuinfo_cache_level_2] = l2;
	cpuinfo_cache[cpuinfo_cache_level_3] = l3;

	cpuinfo_processors_count = mach_topology.threads;
	cpuinfo_cores_count = mach_topology.cores;
	cpuinfo_clusters_count = num_clusters;
	cpuinfo_packages_count = mach_topology.packages;
	cpuinfo_uarchs_count = num_clusters;
	cpuinfo_cache_count[cpuinfo_cache_level_1i] = l1_count;
	cpuinfo_cache_count[cpuinfo_cache_level_1d] = l1_count;
	cpuinfo_cache_count[cpuinfo_cache_level_2] = l2_count;
	cpuinfo_cache_count[cpuinfo_cache_level_3] = l3_count;
	cpuinfo_max_cache_size = cpuinfo_compute_max_cache_size(&processors[0]);

	__sync_synchronize();

	cpuinfo_is_initialized = true;

	processors = NULL;
	cores = NULL;
	clusters = NULL;
	packages = NULL;
	uarchs = NULL;
	l1i = l1d = l2 = l3 = NULL;

cleanup:
	free(processors);
	free(cores);
	free(clusters);
	free(packages);
	free(uarchs);
	free(l1i);
	free(l1d);
	free(l2);
	free(l3);
}
