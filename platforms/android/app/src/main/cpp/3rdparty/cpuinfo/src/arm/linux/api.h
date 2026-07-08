#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <arm/api.h>
#include <arm/midr.h>
#include <cpuinfo.h>
#include <cpuinfo/common.h>
#include <linux/api.h>

/* No hard limit in the kernel, maximum length observed on non-rogue kernels is
 * 64 */
#define CPUINFO_HARDWARE_VALUE_MAX 64
/* No hard limit in the kernel, maximum length on Raspberry Pi is 8. Add 1
 * symbol to detect overly large revision strings */
#define CPUINFO_REVISION_VALUE_MAX 9

#ifdef __ANDROID__
/* As per include/sys/system_properties.h in Android NDK */
#define CPUINFO_BUILD_PROP_NAME_MAX 32
#define CPUINFO_BUILD_PROP_VALUE_MAX 92

struct cpuinfo_android_properties {
	char proc_cpuinfo_hardware[CPUINFO_HARDWARE_VALUE_MAX];
	char ro_product_board[CPUINFO_BUILD_PROP_VALUE_MAX];
	char ro_board_platform[CPUINFO_BUILD_PROP_VALUE_MAX];
	char ro_mediatek_platform[CPUINFO_BUILD_PROP_VALUE_MAX];
	char ro_arch[CPUINFO_BUILD_PROP_VALUE_MAX];
	char ro_chipname[CPUINFO_BUILD_PROP_VALUE_MAX];
	char ro_hardware_chipname[CPUINFO_BUILD_PROP_VALUE_MAX];
};
#endif

#define CPUINFO_ARM_LINUX_ARCH_T UINT32_C(0x00000001)
#define CPUINFO_ARM_LINUX_ARCH_E UINT32_C(0x00000002)
#define CPUINFO_ARM_LINUX_ARCH_J UINT32_C(0x00000004)

#define CPUINFO_ARM_LINUX_ARCH_TE UINT32_C(0x00000003)
#define CPUINFO_ARM_LINUX_ARCH_TEJ UINT32_C(0x00000007)

struct cpuinfo_arm_linux_proc_cpuinfo_cache {
	uint32_t i_size;
	uint32_t i_assoc;
	uint32_t i_line_length;
	uint32_t i_sets;
	uint32_t d_size;
	uint32_t d_assoc;
	uint32_t d_line_length;
	uint32_t d_sets;
};

#if CPUINFO_ARCH_ARM
/* arch/arm/include/uapi/asm/hwcap.h */

#define CPUINFO_ARM_LINUX_FEATURE_SWP UINT32_C(0x00000001)
#define CPUINFO_ARM_LINUX_FEATURE_HALF UINT32_C(0x00000002)
#define CPUINFO_ARM_LINUX_FEATURE_THUMB UINT32_C(0x00000004)
#define CPUINFO_ARM_LINUX_FEATURE_26BIT UINT32_C(0x00000008)
#define CPUINFO_ARM_LINUX_FEATURE_FASTMULT UINT32_C(0x00000010)
#define CPUINFO_ARM_LINUX_FEATURE_FPA UINT32_C(0x00000020)
#define CPUINFO_ARM_LINUX_FEATURE_VFP UINT32_C(0x00000040)
#define CPUINFO_ARM_LINUX_FEATURE_EDSP UINT32_C(0x00000080)
#define CPUINFO_ARM_LINUX_FEATURE_JAVA UINT32_C(0x00000100)
#define CPUINFO_ARM_LINUX_FEATURE_IWMMXT UINT32_C(0x00000200)
#define CPUINFO_ARM_LINUX_FEATURE_CRUNCH UINT32_C(0x00000400)
#define CPUINFO_ARM_LINUX_FEATURE_THUMBEE UINT32_C(0x00000800)
#define CPUINFO_ARM_LINUX_FEATURE_NEON UINT32_C(0x00001000)
#define CPUINFO_ARM_LINUX_FEATURE_VFPV3 UINT32_C(0x00002000)
#define CPUINFO_ARM_LINUX_FEATURE_VFPV3D16                                  \
	UINT32_C(0x00004000) /* Also set for VFPv4 with 16 double-precision \
				registers */
#define CPUINFO_ARM_LINUX_FEATURE_TLS UINT32_C(0x00008000)
#define CPUINFO_ARM_LINUX_FEATURE_VFPV4 UINT32_C(0x00010000)
#define CPUINFO_ARM_LINUX_FEATURE_IDIVA UINT32_C(0x00020000)
#define CPUINFO_ARM_LINUX_FEATURE_IDIVT UINT32_C(0x00040000)
#define CPUINFO_ARM_LINUX_FEATURE_IDIV UINT32_C(0x00060000)
#define CPUINFO_ARM_LINUX_FEATURE_VFPD32 UINT32_C(0x00080000)
#define CPUINFO_ARM_LINUX_FEATURE_LPAE UINT32_C(0x00100000)
#define CPUINFO_ARM_LINUX_FEATURE_EVTSTRM UINT32_C(0x00200000)

#define CPUINFO_ARM_LINUX_FEATURE2_AES UINT32_C(0x00000001)
#define CPUINFO_ARM_LINUX_FEATURE2_PMULL UINT32_C(0x00000002)
#define CPUINFO_ARM_LINUX_FEATURE2_SHA1 UINT32_C(0x00000004)
#define CPUINFO_ARM_LINUX_FEATURE2_SHA2 UINT32_C(0x00000008)
#define CPUINFO_ARM_LINUX_FEATURE2_CRC32 UINT32_C(0x00000010)
#elif CPUINFO_ARCH_ARM64
/* arch/arm64/include/uapi/asm/hwcap.h */
#define CPUINFO_ARM_LINUX_FEATURE_FP UINT32_C(0x00000001)
#define CPUINFO_ARM_LINUX_FEATURE_ASIMD UINT32_C(0x00000002)
#define CPUINFO_ARM_LINUX_FEATURE_EVTSTRM UINT32_C(0x00000004)
#define CPUINFO_ARM_LINUX_FEATURE_AES UINT32_C(0x00000008)
#define CPUINFO_ARM_LINUX_FEATURE_PMULL UINT32_C(0x00000010)
#define CPUINFO_ARM_LINUX_FEATURE_SHA1 UINT32_C(0x00000020)
#define CPUINFO_ARM_LINUX_FEATURE_SHA2 UINT32_C(0x00000040)
#define CPUINFO_ARM_LINUX_FEATURE_CRC32 UINT32_C(0x00000080)
#define CPUINFO_ARM_LINUX_FEATURE_ATOMICS UINT32_C(0x00000100)
#define CPUINFO_ARM_LINUX_FEATURE_FPHP UINT32_C(0x00000200)
#define CPUINFO_ARM_LINUX_FEATURE_ASIMDHP UINT32_C(0x00000400)
#define CPUINFO_ARM_LINUX_FEATURE_CPUID UINT32_C(0x00000800)
#define CPUINFO_ARM_LINUX_FEATURE_ASIMDRDM UINT32_C(0x00001000)
#define CPUINFO_ARM_LINUX_FEATURE_JSCVT UINT32_C(0x00002000)
#define CPUINFO_ARM_LINUX_FEATURE_FCMA UINT32_C(0x00004000)
#define CPUINFO_ARM_LINUX_FEATURE_LRCPC UINT32_C(0x00008000)
#define CPUINFO_ARM_LINUX_FEATURE_DCPOP UINT32_C(0x00010000)
#define CPUINFO_ARM_LINUX_FEATURE_SHA3 UINT32_C(0x00020000)
#define CPUINFO_ARM_LINUX_FEATURE_SM3 UINT32_C(0x00040000)
#define CPUINFO_ARM_LINUX_FEATURE_SM4 UINT32_C(0x00080000)
#define CPUINFO_ARM_LINUX_FEATURE_ASIMDDP UINT32_C(0x00100000)
#define CPUINFO_ARM_LINUX_FEATURE_SHA512 UINT32_C(0x00200000)
#define CPUINFO_ARM_LINUX_FEATURE_SVE UINT32_C(0x00400000)
#define CPUINFO_ARM_LINUX_FEATURE_ASIMDFHM UINT32_C(0x00800000)
#define CPUINFO_ARM_LINUX_FEATURE_DIT UINT32_C(0x01000000)
#define CPUINFO_ARM_LINUX_FEATURE_USCAT UINT32_C(0x02000000)
#define CPUINFO_ARM_LINUX_FEATURE_ILRCPC UINT32_C(0x04000000)
#define CPUINFO_ARM_LINUX_FEATURE_FLAGM UINT32_C(0x08000000)
#define CPUINFO_ARM_LINUX_FEATURE_SSBS UINT32_C(0x10000000)
#define CPUINFO_ARM_LINUX_FEATURE_SB UINT32_C(0x20000000)
#define CPUINFO_ARM_LINUX_FEATURE_PACA UINT32_C(0x40000000)
#define CPUINFO_ARM_LINUX_FEATURE_PACG UINT32_C(0x80000000)

#define CPUINFO_ARM_LINUX_FEATURE2_DCPODP UINT32_C(0x00000001)
#define CPUINFO_ARM_LINUX_FEATURE2_SVE2 UINT32_C(0x00000002)
#define CPUINFO_ARM_LINUX_FEATURE2_SVEAES UINT32_C(0x00000004)
#define CPUINFO_ARM_LINUX_FEATURE2_SVEPMULL UINT32_C(0x00000008)
#define CPUINFO_ARM_LINUX_FEATURE2_SVEBITPERM UINT32_C(0x00000010)
#define CPUINFO_ARM_LINUX_FEATURE2_SVESHA3 UINT32_C(0x00000020)
#define CPUINFO_ARM_LINUX_FEATURE2_SVESM4 UINT32_C(0x00000040)
#define CPUINFO_ARM_LINUX_FEATURE2_FLAGM2 UINT32_C(0x00000080)
#define CPUINFO_ARM_LINUX_FEATURE2_FRINT UINT32_C(0x00000100)
#define CPUINFO_ARM_LINUX_FEATURE2_SVEI8MM UINT32_C(0x00000200)
#define CPUINFO_ARM_LINUX_FEATURE2_SVEF32MM UINT32_C(0x00000400)
#define CPUINFO_ARM_LINUX_FEATURE2_SVEF64MM UINT32_C(0x00000800)
#define CPUINFO_ARM_LINUX_FEATURE2_SVEBF16 UINT32_C(0x00001000)
#define CPUINFO_ARM_LINUX_FEATURE2_I8MM UINT32_C(0x00002000)
#define CPUINFO_ARM_LINUX_FEATURE2_BF16 UINT32_C(0x00004000)
#define CPUINFO_ARM_LINUX_FEATURE2_DGH UINT32_C(0x00008000)
#define CPUINFO_ARM_LINUX_FEATURE2_RNG UINT32_C(0x00010000)
#define CPUINFO_ARM_LINUX_FEATURE2_BTI UINT32_C(0x00020000)
#define CPUINFO_ARM_LINUX_FEATURE2_SME UINT32_C(0x00800000)
#define CPUINFO_ARM_LINUX_FEATURE2_SME2 UINT64_C(0x0000002000000000)
#define CPUINFO_ARM_LINUX_FEATURE2_SME2P1 UINT64_C(0x0000004000000000)
#define CPUINFO_ARM_LINUX_FEATURE2_SME_I16I32 UINT64_C(0x0000008000000000)
#define CPUINFO_ARM_LINUX_FEATURE2_SME_BI32I32 UINT64_C(0x0000010000000000)
#define CPUINFO_ARM_LINUX_FEATURE2_SME_B16B16 UINT64_C(0x0000020000000000)
#define CPUINFO_ARM_LINUX_FEATURE2_SME_F16F16 UINT64_C(0x0000040000000000)
#endif

#define CPUINFO_ARM_LINUX_VALID_ARCHITECTURE UINT32_C(0x00010000)
#define CPUINFO_ARM_LINUX_VALID_IMPLEMENTER UINT32_C(0x00020000)
#define CPUINFO_ARM_LINUX_VALID_VARIANT UINT32_C(0x00040000)
#define CPUINFO_ARM_LINUX_VALID_PART UINT32_C(0x00080000)
#define CPUINFO_ARM_LINUX_VALID_REVISION UINT32_C(0x00100000)
#define CPUINFO_ARM_LINUX_VALID_PROCESSOR UINT32_C(0x00200000)
#define CPUINFO_ARM_LINUX_VALID_FEATURES UINT32_C(0x00400000)
#if CPUINFO_ARCH_ARM
#define CPUINFO_ARM_LINUX_VALID_ICACHE_SIZE UINT32_C(0x01000000)
#define CPUINFO_ARM_LINUX_VALID_ICACHE_SETS UINT32_C(0x02000000)
#define CPUINFO_ARM_LINUX_VALID_ICACHE_WAYS UINT32_C(0x04000000)
#define CPUINFO_ARM_LINUX_VALID_ICACHE_LINE UINT32_C(0x08000000)
#define CPUINFO_ARM_LINUX_VALID_DCACHE_SIZE UINT32_C(0x10000000)
#define CPUINFO_ARM_LINUX_VALID_DCACHE_SETS UINT32_C(0x20000000)
#define CPUINFO_ARM_LINUX_VALID_DCACHE_WAYS UINT32_C(0x40000000)
#define CPUINFO_ARM_LINUX_VALID_DCACHE_LINE UINT32_C(0x80000000)
#endif

#define CPUINFO_ARM_LINUX_VALID_INFO UINT32_C(0x007F0000)
#define CPUINFO_ARM_LINUX_VALID_MIDR UINT32_C(0x003F0000)
#if CPUINFO_ARCH_ARM
#define CPUINFO_ARM_LINUX_VALID_ICACHE UINT32_C(0x0F000000)
#define CPUINFO_ARM_LINUX_VALID_DCACHE UINT32_C(0xF0000000)
#define CPUINFO_ARM_LINUX_VALID_CACHE_LINE UINT32_C(0x88000000)
#endif

struct cpuinfo_arm_linux_processor {
	uint32_t architecture_version;
#if CPUINFO_ARCH_ARM
	uint32_t architecture_flags;
	struct cpuinfo_arm_linux_proc_cpuinfo_cache proc_cpuinfo_cache;
#endif
	uint32_t features;
	uint64_t features2;
	/**
	 * Main ID Register value.
	 */
	uint32_t midr;
	enum cpuinfo_vendor vendor;
	enum cpuinfo_uarch uarch;
	uint32_t uarch_index;
	/**
	 * ID of the physical package which includes this logical processor.
	 * The value is parsed from
	 * /sys/devices/system/cpu/cpu<N>/topology/physical_package_id
	 */
	uint32_t package_id;
	/**
	 * Minimum processor ID on the package which includes this logical
	 * processor. This value can serve as an ID for the cluster of logical
	 * processors: it is the same for all logical processors on the same
	 * package.
	 */
	uint32_t package_leader_id;
	/**
	 * Number of logical processors in the package.
	 */
	uint32_t package_processor_count;
	/**
	 * Maximum frequency, in kHZ.
	 * The value is parsed from
	 * /sys/devices/system/cpu/cpu<N>/cpufreq/cpuinfo_max_freq If failed to
	 * read or parse the file, the value is 0.
	 */
	uint32_t max_frequency;
	/**
	 * Minimum frequency, in kHZ.
	 * The value is parsed from
	 * /sys/devices/system/cpu/cpu<N>/cpufreq/cpuinfo_min_freq If failed to
	 * read or parse the file, the value is 0.
	 */
	uint32_t min_frequency;
	/** Linux processor ID */
	uint32_t system_processor_id;
	uint32_t flags;
};

struct cpuinfo_arm_linux_cluster {
	uint32_t processor_id_min;
	uint32_t processor_id_max;
};

/* Returns true if the two processors do belong to the same cluster */
static inline bool cpuinfo_arm_linux_processor_equals(
	struct cpuinfo_arm_linux_processor processor_i[restrict static 1],
	struct cpuinfo_arm_linux_processor processor_j[restrict static 1]) {
	const uint32_t joint_flags = processor_i->flags & processor_j->flags;

	bool same_max_frequency = false;
	if (joint_flags & CPUINFO_LINUX_FLAG_MAX_FREQUENCY) {
		if (processor_i->max_frequency != processor_j->max_frequency) {
			return false;
		} else {
			same_max_frequency = true;
		}
	}

	bool same_min_frequency = false;
	if (joint_flags & CPUINFO_LINUX_FLAG_MIN_FREQUENCY) {
		if (processor_i->min_frequency != processor_j->min_frequency) {
			return false;
		} else {
			same_min_frequency = true;
		}
	}

	if ((joint_flags & CPUINFO_ARM_LINUX_VALID_MIDR) == CPUINFO_ARM_LINUX_VALID_MIDR) {
		if (processor_i->midr == processor_j->midr) {
			if (midr_is_cortex_a53(processor_i->midr)) {
				return same_min_frequency & same_max_frequency;
			} else {
				return true;
			}
		}
	}

	return same_max_frequency && same_min_frequency;
}

/* Returns true if the two processors certainly don't belong to the same cluster
 */
static inline bool cpuinfo_arm_linux_processor_not_equals(
	struct cpuinfo_arm_linux_processor processor_i[restrict static 1],
	struct cpuinfo_arm_linux_processor processor_j[restrict static 1]) {
	const uint32_t joint_flags = processor_i->flags & processor_j->flags;

	if (joint_flags & CPUINFO_LINUX_FLAG_MAX_FREQUENCY) {
		if (processor_i->max_frequency != processor_j->max_frequency) {
			return true;
		}
	}

	if (joint_flags & CPUINFO_LINUX_FLAG_MIN_FREQUENCY) {
		if (processor_i->min_frequency != processor_j->min_frequency) {
			return true;
		}
	}

	if ((joint_flags & CPUINFO_ARM_LINUX_VALID_MIDR) == CPUINFO_ARM_LINUX_VALID_MIDR) {
		if (processor_i->midr != processor_j->midr) {
			return true;
		}
	}

	return false;
}

CPUINFO_INTERNAL bool cpuinfo_arm_linux_parse_proc_cpuinfo(
	char hardware[restrict static CPUINFO_HARDWARE_VALUE_MAX],
	char revision[restrict static CPUINFO_REVISION_VALUE_MAX],
	uint32_t max_processors_count,
	struct cpuinfo_arm_linux_processor processors[restrict static max_processors_count]);

#if CPUINFO_ARCH_ARM
CPUINFO_INTERNAL bool cpuinfo_arm_linux_hwcap_from_getauxval(
	uint32_t hwcap[restrict static 1],
	uint64_t hwcap2[restrict static 1]);
CPUINFO_INTERNAL bool cpuinfo_arm_linux_hwcap_from_procfs(
	uint32_t hwcap[restrict static 1],
	uint64_t hwcap2[restrict static 1]);

CPUINFO_INTERNAL void cpuinfo_arm_linux_decode_isa_from_proc_cpuinfo(
	uint32_t features,
	uint64_t features2,
	uint32_t midr,
	uint32_t architecture_version,
	uint32_t architecture_flags,
	const struct cpuinfo_arm_chipset chipset[restrict static 1],
	struct cpuinfo_arm_isa isa[restrict static 1]);
#elif CPUINFO_ARCH_ARM64
CPUINFO_INTERNAL void cpuinfo_arm_linux_hwcap_from_getauxval(
	uint32_t hwcap[restrict static 1],
	uint64_t hwcap2[restrict static 1]);

CPUINFO_INTERNAL void cpuinfo_arm64_linux_decode_isa_from_proc_cpuinfo(
	uint32_t features,
	uint64_t features2,
	uint32_t midr,
	const struct cpuinfo_arm_chipset chipset[restrict static 1],
	struct cpuinfo_arm_isa isa[restrict static 1]);
#endif

#if defined(__ANDROID__)
CPUINFO_INTERNAL struct cpuinfo_arm_chipset cpuinfo_arm_android_decode_chipset(
	const struct cpuinfo_android_properties properties[restrict static 1],
	uint32_t cores,
	uint32_t max_cpu_freq_max);
#else
CPUINFO_INTERNAL struct cpuinfo_arm_chipset cpuinfo_arm_linux_decode_chipset(
	const char hardware[restrict static CPUINFO_HARDWARE_VALUE_MAX],
	const char revision[restrict static CPUINFO_REVISION_VALUE_MAX],
	uint32_t cores,
	uint32_t max_cpu_freq_max);
#endif

CPUINFO_INTERNAL struct cpuinfo_arm_chipset cpuinfo_arm_linux_decode_chipset_from_proc_cpuinfo_hardware(
	const char proc_cpuinfo_hardware[restrict static CPUINFO_HARDWARE_VALUE_MAX],
	uint32_t cores,
	uint32_t max_cpu_freq_max,
	bool is_tegra);

#ifdef __ANDROID__
CPUINFO_INTERNAL struct cpuinfo_arm_chipset cpuinfo_arm_android_decode_chipset_from_ro_product_board(
	const char ro_product_board[restrict static CPUINFO_BUILD_PROP_VALUE_MAX],
	uint32_t cores,
	uint32_t max_cpu_freq_max);
CPUINFO_INTERNAL struct cpuinfo_arm_chipset cpuinfo_arm_android_decode_chipset_from_ro_board_platform(
	const char ro_board_platform[restrict static CPUINFO_BUILD_PROP_VALUE_MAX],
	uint32_t cores,
	uint32_t max_cpu_freq_max);
CPUINFO_INTERNAL struct cpuinfo_arm_chipset cpuinfo_arm_android_decode_chipset_from_ro_mediatek_platform(
	const char ro_mediatek_platform[restrict static CPUINFO_BUILD_PROP_VALUE_MAX]);
CPUINFO_INTERNAL struct cpuinfo_arm_chipset cpuinfo_arm_android_decode_chipset_from_ro_arch(
	const char ro_arch[restrict static CPUINFO_BUILD_PROP_VALUE_MAX]);
CPUINFO_INTERNAL struct cpuinfo_arm_chipset cpuinfo_arm_android_decode_chipset_from_ro_chipname(
	const char ro_chipname[restrict static CPUINFO_BUILD_PROP_VALUE_MAX]);
CPUINFO_INTERNAL struct cpuinfo_arm_chipset cpuinfo_arm_android_decode_chipset_from_ro_hardware_chipname(
	const char ro_hardware_chipname[restrict static CPUINFO_BUILD_PROP_VALUE_MAX]);
#else
CPUINFO_INTERNAL struct cpuinfo_arm_chipset cpuinfo_arm_linux_decode_chipset_from_proc_cpuinfo_revision(
	const char proc_cpuinfo_revision[restrict static CPUINFO_REVISION_VALUE_MAX]);
#endif

CPUINFO_INTERNAL bool cpuinfo_arm_linux_detect_core_clusters_by_heuristic(
	uint32_t usable_processors,
	uint32_t max_processors,
	struct cpuinfo_arm_linux_processor processors[restrict static max_processors]);

CPUINFO_INTERNAL void cpuinfo_arm_linux_detect_core_clusters_by_sequential_scan(
	uint32_t max_processors,
	struct cpuinfo_arm_linux_processor processors[restrict static max_processors]);

CPUINFO_INTERNAL void cpuinfo_arm_linux_count_cluster_processors(
	uint32_t max_processors,
	struct cpuinfo_arm_linux_processor processors[restrict static max_processors]);

CPUINFO_INTERNAL uint32_t cpuinfo_arm_linux_detect_cluster_midr(
	const struct cpuinfo_arm_chipset chipset[restrict static 1],
	uint32_t max_processors,
	uint32_t usable_processors,
	struct cpuinfo_arm_linux_processor processors[restrict static max_processors]);

extern CPUINFO_INTERNAL const uint32_t* cpuinfo_linux_cpu_to_uarch_index_map;
extern CPUINFO_INTERNAL uint32_t cpuinfo_linux_cpu_to_uarch_index_map_entries;
