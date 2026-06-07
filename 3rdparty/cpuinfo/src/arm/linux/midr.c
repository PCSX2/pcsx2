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
#include <cpuinfo/common.h>
#include <cpuinfo/internal-api.h>
#include <cpuinfo/log.h>
#include <linux/api.h>

#define CLUSTERS_MAX 3

static inline bool bitmask_all(uint32_t bitfield, uint32_t mask) {
	return (bitfield & mask) == mask;
}

/* Description of core clusters configuration in a chipset (identified by series
 * and model number) */
struct cluster_config {
	/* Number of cores (logical processors) */
	uint8_t cores;
	/* ARM chipset series (see cpuinfo_arm_chipset_series enum) */
	uint8_t series;
	/* Chipset model number (see cpuinfo_arm_chipset struct) */
	uint16_t model;
	/* Number of heterogenous clusters in the CPU package */
	uint8_t clusters;
	/*
	 * Number of cores in each cluster:
	 # - Symmetric configurations: [0] = # cores
	 * - big.LITTLE configurations: [0] = # LITTLE cores, [1] = # big cores
	 * - Max.Med.Min configurations: [0] = # Min cores, [1] = # Med cores,
	 [2] = # Max cores
	 */
	uint8_t cluster_cores[CLUSTERS_MAX];
	/*
	 * MIDR of cores in each cluster:
	 * - Symmetric configurations: [0] = core MIDR
	 * - big.LITTLE configurations: [0] = LITTLE core MIDR, [1] = big core
	 * MIDR
	 * - Max.Med.Min configurations: [0] = Min core MIDR, [1] = Med core
	 * MIDR, [2] = Max core MIDR
	 */
	uint32_t cluster_midr[CLUSTERS_MAX];
};

/*
 * The list of chipsets where MIDR may not be unambigiously decoded at least on
 * some devices. The typical reasons for impossibility to decoded MIDRs are
 * buggy kernels, which either do not report all MIDR information (e.g. on
 * ATM7029 kernel doesn't report CPU Part), or chipsets have more than one type
 * of cores (i.e. 4x Cortex-A53 + 4x Cortex-A53 is out) and buggy kernels report
 * MIDR information only about some cores in /proc/cpuinfo (either only online
 * cores, or only the core that reads /proc/cpuinfo). On these kernels/chipsets,
 * it is not possible to detect all core types by just parsing /proc/cpuinfo, so
 * we use chipset name and this table to find their MIDR (and thus
 * microarchitecture, cache, etc).
 *
 * Note: not all chipsets with heterogeneous multiprocessing need an entry in
 * this table. The following HMP chipsets always list information about all
 * cores in /proc/cpuinfo:
 *
 * - Snapdragon 660
 * - Snapdragon 820 (MSM8996)
 * - Snapdragon 821 (MSM8996PRO)
 * - Snapdragon 835 (MSM8998)
 * - Exynos 8895
 * - Kirin 960
 *
 * As these are all new processors, there is hope that this table won't
 * uncontrollably grow over time.
 */
static const struct cluster_config
	cluster_configs[] =
		{
#if CPUINFO_ARCH_ARM
			{
				/*
				 * MSM8916 (Snapdragon 410): 4x Cortex-A53
				 * Some AArch32 phones use non-standard /proc/cpuinfo format.
				 */
				.cores = 4,
				.series = cpuinfo_arm_chipset_series_qualcomm_msm,
				.model = UINT16_C(8916),
				.clusters = 1,
				.cluster_cores =
					{
						[0] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FD030),
					},
			},
			{
				/*
				 * MSM8939 (Snapdragon 615): 4x Cortex-A53 + 4x Cortex-A53
				 * Some AArch32 phones use non-standard /proc/cpuinfo format.
				 */
				.cores = 8,
				.series = cpuinfo_arm_chipset_series_qualcomm_msm,
				.model = UINT16_C(8939),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FD034),
						[1] = UINT32_C(0x410FD034),
					},
			},
#endif
			{
				/* MSM8956 (Snapdragon 650): 2x Cortex-A72 + 4x Cortex-A53 */
				.cores = 6,
				.series = cpuinfo_arm_chipset_series_qualcomm_msm,
				.model = UINT16_C(8956),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 2,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FD034),
						[1] = UINT32_C(0x410FD080),
					},
			},
			{
				/* MSM8976/MSM8976PRO (Snapdragon 652/653): 4x Cortex-A72 + 4x
				   Cortex-A53 */
				.cores = 8,
				.series = cpuinfo_arm_chipset_series_qualcomm_msm,
				.model = UINT16_C(8976),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FD034),
						[1] = UINT32_C(0x410FD080),
					},
			},
			{
				/* MSM8992 (Snapdragon 808): 2x Cortex-A57 + 4x Cortex-A53 */
				.cores = 6,
				.series = cpuinfo_arm_chipset_series_qualcomm_msm,
				.model = UINT16_C(8992),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 2,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FD033),
						[1] = UINT32_C(0x411FD072),
					},
			},
			{
				/* MSM8994/MSM8994V (Snapdragon 810): 4x Cortex-A57 + 4x
				   Cortex-A53 */
				.cores = 8,
				.series = cpuinfo_arm_chipset_series_qualcomm_msm,
				.model = UINT16_C(8994),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FD032),
						[1] = UINT32_C(0x411FD071),
					},
			},
#if CPUINFO_ARCH_ARM
			{
				/* Exynos 5422: 4x Cortex-A15 + 4x Cortex-A7 */
				.cores = 8,
				.series = cpuinfo_arm_chipset_series_samsung_exynos,
				.model = UINT16_C(5422),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FC073),
						[1] = UINT32_C(0x412FC0F3),
					},
			},
			{
				/* Exynos 5430: 4x Cortex-A15 + 4x Cortex-A7 */
				.cores = 8,
				.series = cpuinfo_arm_chipset_series_samsung_exynos,
				.model = UINT16_C(5430),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FC074),
						[1] = UINT32_C(0x413FC0F3),
					},
			},
#endif /* CPUINFO_ARCH_ARM */
			{
				/* Exynos 5433: 4x Cortex-A57 + 4x Cortex-A53 */
				.cores = 8,
				.series = cpuinfo_arm_chipset_series_samsung_exynos,
				.model = UINT16_C(5433),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FD031),
						[1] = UINT32_C(0x411FD070),
					},
			},
			{
				/* Exynos 7420: 4x Cortex-A57 + 4x Cortex-A53 */
				.cores = 8,
				.series = cpuinfo_arm_chipset_series_samsung_exynos,
				.model = UINT16_C(7420),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FD032),
						[1] = UINT32_C(0x411FD070),
					},
			},
			{
				/* Exynos 8890: 4x Exynos M1 + 4x Cortex-A53 */
				.cores = 8,
				.series = cpuinfo_arm_chipset_series_samsung_exynos,
				.model = UINT16_C(8890),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FD034),
						[1] = UINT32_C(0x531F0011),
					},
			},
#if CPUINFO_ARCH_ARM
			{
				/* Kirin 920: 4x Cortex-A15 + 4x Cortex-A7 */
				.cores = 8,
				.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
				.model = UINT16_C(920),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FC075),
						[1] = UINT32_C(0x413FC0F3),
					},
			},
			{
				/* Kirin 925: 4x Cortex-A15 + 4x Cortex-A7 */
				.cores = 8,
				.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
				.model = UINT16_C(925),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FC075),
						[1] = UINT32_C(0x413FC0F3),
					},
			},
			{
				/* Kirin 928: 4x Cortex-A15 + 4x Cortex-A7 */
				.cores = 8,
				.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
				.model = UINT16_C(928),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FC075),
						[1] = UINT32_C(0x413FC0F3),
					},
			},
#endif /* CPUINFO_ARCH_ARM */
			{
				/* Kirin 950: 4x Cortex-A72 + 4x Cortex-A53 */
				.cores = 8,
				.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
				.model = UINT16_C(950),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FD034),
						[1] = UINT32_C(0x410FD080),
					},
			},
			{
				/* Kirin 955: 4x Cortex-A72 + 4x Cortex-A53 */
				.cores = 8,
				.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
				.model = UINT16_C(955),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FD034),
						[1] = UINT32_C(0x410FD080),
					},
			},
#if CPUINFO_ARCH_ARM
			{
				/* MediaTek MT8135: 2x Cortex-A7 + 2x Cortex-A15 */
				.cores = 4,
				.series = cpuinfo_arm_chipset_series_mediatek_mt,
				.model = UINT16_C(8135),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 2,
						[1] = 2,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FC073),
						[1] = UINT32_C(0x413FC0F2),
					},
			},
#endif
			{
				/* MediaTek MT8173: 2x Cortex-A72 + 2x Cortex-A53 */
				.cores = 4,
				.series = cpuinfo_arm_chipset_series_mediatek_mt,
				.model = UINT16_C(8173),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 2,
						[1] = 2,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FD032),
						[1] = UINT32_C(0x410FD080),
					},
			},
			{
				/* MediaTek MT8176: 2x Cortex-A72 + 4x Cortex-A53 */
				.cores = 6,
				.series = cpuinfo_arm_chipset_series_mediatek_mt,
				.model = UINT16_C(8176),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 2,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FD032),
						[1] = UINT32_C(0x410FD080),
					},
			},
#if CPUINFO_ARCH_ARM64
			{
				/*
				 * MediaTek MT8735: 4x Cortex-A53
				 * Some AArch64 phones use non-standard /proc/cpuinfo format.
				 */
				.cores = 4,
				.series = cpuinfo_arm_chipset_series_mediatek_mt,
				.model = UINT16_C(8735),
				.clusters = 1,
				.cluster_cores =
					{
						[0] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FD034),
					},
			},
#endif
#if CPUINFO_ARCH_ARM
			{
				/*
				 * MediaTek MT6592: 4x Cortex-A7 + 4x Cortex-A7
				 * Some phones use non-standard /proc/cpuinfo format.
				 */
				.cores = 4,
				.series = cpuinfo_arm_chipset_series_mediatek_mt,
				.model = UINT16_C(6592),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FC074),
						[1] = UINT32_C(0x410FC074),
					},
			},
			{
				/* MediaTek MT6595: 4x Cortex-A17 + 4x Cortex-A7 */
				.cores = 8,
				.series = cpuinfo_arm_chipset_series_mediatek_mt,
				.model = UINT16_C(6595),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FC075),
						[1] = UINT32_C(0x410FC0E0),
					},
			},
#endif
			{
				/* MediaTek MT6797: 2x Cortex-A72 + 4x Cortex-A53 + 4x
				   Cortex-A53 */
				.cores = 10,
				.series = cpuinfo_arm_chipset_series_mediatek_mt,
				.model = UINT16_C(6797),
				.clusters = 3,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 4,
						[2] = 2,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FD034),
						[1] = UINT32_C(0x410FD034),
						[2] = UINT32_C(0x410FD081),
					},
			},
			{
				/* MediaTek MT6799: 2x Cortex-A73 + 4x Cortex-A53 + 4x
				   Cortex-A35 */
				.cores = 10,
				.series = cpuinfo_arm_chipset_series_mediatek_mt,
				.model = UINT16_C(6799),
				.clusters = 3,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 4,
						[2] = 2,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FD041),
						[1] = UINT32_C(0x410FD034),
						[2] = UINT32_C(0x410FD092),
					},
			},
			{
				/* Rockchip RK3399: 2x Cortex-A72 + 4x Cortex-A53 */
				.cores = 6,
				.series = cpuinfo_arm_chipset_series_rockchip_rk,
				.model = UINT16_C(3399),
				.clusters = 2,
				.cluster_cores =
					{
						[0] = 4,
						[1] = 2,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FD034),
						[1] = UINT32_C(0x410FD082),
					},
			},
#if CPUINFO_ARCH_ARM
			{
				/* Actions ATM8029: 4x Cortex-A5
				 * Most devices use non-standard /proc/cpuinfo format.
				 */
				.cores = 4,
				.series = cpuinfo_arm_chipset_series_actions_atm,
				.model = UINT16_C(7029),
				.clusters = 1,
				.cluster_cores =
					{
						[0] = 4,
					},
				.cluster_midr =
					{
						[0] = UINT32_C(0x410FC051),
					},
			},
#endif
};

/*
 * Searches chipset name in mapping of chipset name to cores' MIDR values. If
 * match is successful, initializes MIDR for all clusters' leaders with
 * tabulated values.
 *
 * @param[in] chipset - chipset (SoC) name information.
 * @param clusters_count - number of CPU core clusters detected in the SoC.
 * @param cluster_leaders - indices of core clusters' leaders in the @p
 * processors array.
 * @param processors_count - number of usable logical processors in the system.
 * @param[in,out] processors - array of logical processor descriptions with
 * pre-parsed MIDR, maximum frequency, and decoded core cluster
 * (package_leader_id) information. Upon successful return, processors[i].midr
 * for all clusters' leaders contains the tabulated MIDR values.
 * @param verify_midr - indicated whether the function should check that the
 * MIDR values to be assigned to leaders of core clusters are consistent with
 * known parts of their parsed values. Set if to false if the only MIDR value
 * parsed from /proc/cpuinfo is for the last processor reported in /proc/cpuinfo
 * and thus can't be unambiguously attributed to that processor.
 *
 * @retval true if the chipset was found in the mapping and core clusters'
 * leaders initialized with MIDR values.
 * @retval false if the chipset was not found in the mapping, or any consistency
 * check failed.
 */
static bool cpuinfo_arm_linux_detect_cluster_midr_by_chipset(
	const struct cpuinfo_arm_chipset chipset[restrict static 1],
	uint32_t clusters_count,
	const uint32_t cluster_leaders[restrict static CLUSTERS_MAX],
	uint32_t processors_count,
	struct cpuinfo_arm_linux_processor processors[restrict static processors_count],
	bool verify_midr) {
	if (clusters_count <= CLUSTERS_MAX) {
		for (uint32_t c = 0; c < CPUINFO_COUNT_OF(cluster_configs); c++) {
			if (cluster_configs[c].model == chipset->model &&
			    cluster_configs[c].series == chipset->series) {
				/* Verify that the total number of cores and
				 * clusters of cores matches expectation */
				if (cluster_configs[c].cores != processors_count ||
				    cluster_configs[c].clusters != clusters_count) {
					return false;
				}

				/* Verify that core cluster configuration
				 * matches expectation */
				for (uint32_t cluster = 0; cluster < clusters_count; cluster++) {
					const uint32_t cluster_leader = cluster_leaders[cluster];
					if (cluster_configs[c].cluster_cores[cluster] !=
					    processors[cluster_leader].package_processor_count) {
						return false;
					}
				}

				if (verify_midr) {
					/* Verify known parts of MIDR */
					for (uint32_t cluster = 0; cluster < clusters_count; cluster++) {
						const uint32_t cluster_leader = cluster_leaders[cluster];

						/* Create a mask of known midr
						 * bits */
						uint32_t midr_mask = 0;
						if (processors[cluster_leader].flags &
						    CPUINFO_ARM_LINUX_VALID_IMPLEMENTER) {
							midr_mask |= CPUINFO_ARM_MIDR_IMPLEMENTER_MASK;
						}
						if (processors[cluster_leader].flags &
						    CPUINFO_ARM_LINUX_VALID_VARIANT) {
							midr_mask |= CPUINFO_ARM_MIDR_VARIANT_MASK;
						}
						if (processors[cluster_leader].flags & CPUINFO_ARM_LINUX_VALID_PART) {
							midr_mask |= CPUINFO_ARM_MIDR_PART_MASK;
						}
						if (processors[cluster_leader].flags &
						    CPUINFO_ARM_LINUX_VALID_REVISION) {
							midr_mask |= CPUINFO_ARM_MIDR_REVISION_MASK;
						}

						/* Verify the bits under the
						 * mask */
						if ((processors[cluster_leader].midr ^
						     cluster_configs[c].cluster_midr[cluster]) &
						    midr_mask) {
							cpuinfo_log_debug(
								"parsed MIDR of cluster %08" PRIu32
								" does not match tabulated value %08" PRIu32,
								processors[cluster_leader].midr,
								cluster_configs[c].cluster_midr[cluster]);
							return false;
						}
					}
				}

				/* Assign MIDRs according to tabulated
				 * configurations */
				for (uint32_t cluster = 0; cluster < clusters_count; cluster++) {
					const uint32_t cluster_leader = cluster_leaders[cluster];
					processors[cluster_leader].midr = cluster_configs[c].cluster_midr[cluster];
					processors[cluster_leader].flags |= CPUINFO_ARM_LINUX_VALID_MIDR;
					cpuinfo_log_debug(
						"cluster %" PRIu32 " MIDR = 0x%08" PRIx32,
						cluster,
						cluster_configs[c].cluster_midr[cluster]);
				}
				return true;
			}
		}
	}
	return false;
}

/*
 * Initializes MIDR for leaders of core clusters using a heuristic for
 * big.LITTLE systems:
 * - If the only known MIDR is for the big core cluster, guess the matching MIDR
 * for the LITTLE cluster.
 * - Estimate which of the clusters is big using maximum frequency, if known,
 * otherwise using system processor ID.
 * - Initialize the MIDR for big and LITTLE core clusters using the guesstimates
 * values.
 *
 * @param clusters_count - number of CPU core clusters detected in the SoC.
 * @param cluster_with_midr_count - number of CPU core clusters in the SoC with
 * known MIDR values.
 * @param last_processor_with_midr - index of the last logical processor with
 * known MIDR in the @p processors array.
 * @param cluster_leaders - indices of core clusters' leaders in the @p
 * processors array.
 * @param[in,out] processors - array of logical processor descriptions with
 * pre-parsed MIDR, maximum frequency, and decoded core cluster
 * (package_leader_id) information. Upon successful return, processors[i].midr
 * for all core clusters' leaders contains the heuristically detected MIDR
 * value.
 * @param verify_midr - indicated whether the function should check that the
 * MIDR values to be assigned to leaders of core clusters are consistent with
 * known parts of their parsed values. Set if to false if the only MIDR value
 * parsed from /proc/cpuinfo is for the last processor reported in /proc/cpuinfo
 * and thus can't be unambiguously attributed to that processor.
 *
 * @retval true if this is a big.LITTLE system with only one known MIDR and the
 * CPU core clusters' leaders were initialized with MIDR values.
 * @retval false if this is not a big.LITTLE system.
 */
static bool cpuinfo_arm_linux_detect_cluster_midr_by_big_little_heuristic(
	uint32_t clusters_count,
	uint32_t cluster_with_midr_count,
	uint32_t last_processor_with_midr,
	const uint32_t cluster_leaders[restrict static CLUSTERS_MAX],
	struct cpuinfo_arm_linux_processor processors[restrict static last_processor_with_midr],
	bool verify_midr) {
	if (clusters_count != 2 || cluster_with_midr_count != 1) {
		/* Not a big.LITTLE system, or MIDR is known for both/neither
		 * clusters */
		return false;
	}

	const uint32_t midr_flags =
		(processors[processors[last_processor_with_midr].package_leader_id].flags &
		 CPUINFO_ARM_LINUX_VALID_MIDR);
	const uint32_t big_midr = processors[processors[last_processor_with_midr].package_leader_id].midr;
	const uint32_t little_midr = midr_little_core_for_big(big_midr);

	/* Default assumption: the first reported cluster is LITTLE cluster
	 * (this holds on most Linux kernels) */
	uint32_t little_cluster_leader = cluster_leaders[0];
	const uint32_t other_cluster_leader = cluster_leaders[1];
	/* If maximum frequency is known for both clusters, assume LITTLE
	 * cluster is the one with lower frequency */
	if (processors[little_cluster_leader].flags & processors[other_cluster_leader].flags &
	    CPUINFO_LINUX_FLAG_MAX_FREQUENCY) {
		if (processors[little_cluster_leader].max_frequency > processors[other_cluster_leader].max_frequency) {
			little_cluster_leader = other_cluster_leader;
		}
	}

	if (verify_midr) {
		/* Verify known parts of MIDR */
		for (uint32_t cluster = 0; cluster < clusters_count; cluster++) {
			const uint32_t cluster_leader = cluster_leaders[cluster];

			/* Create a mask of known midr bits */
			uint32_t midr_mask = 0;
			if (processors[cluster_leader].flags & CPUINFO_ARM_LINUX_VALID_IMPLEMENTER) {
				midr_mask |= CPUINFO_ARM_MIDR_IMPLEMENTER_MASK;
			}
			if (processors[cluster_leader].flags & CPUINFO_ARM_LINUX_VALID_VARIANT) {
				midr_mask |= CPUINFO_ARM_MIDR_VARIANT_MASK;
			}
			if (processors[cluster_leader].flags & CPUINFO_ARM_LINUX_VALID_PART) {
				midr_mask |= CPUINFO_ARM_MIDR_PART_MASK;
			}
			if (processors[cluster_leader].flags & CPUINFO_ARM_LINUX_VALID_REVISION) {
				midr_mask |= CPUINFO_ARM_MIDR_REVISION_MASK;
			}

			/* Verify the bits under the mask */
			const uint32_t midr = (cluster_leader == little_cluster_leader) ? little_midr : big_midr;
			if ((processors[cluster_leader].midr ^ midr) & midr_mask) {
				cpuinfo_log_debug(
					"parsed MIDR %08" PRIu32 " of cluster leader %" PRIu32
					" is inconsistent with expected value %08" PRIu32,
					processors[cluster_leader].midr,
					cluster_leader,
					midr);
				return false;
			}
		}
	}

	for (uint32_t c = 0; c < clusters_count; c++) {
		/* Skip cluster with already assigned MIDR */
		const uint32_t cluster_leader = cluster_leaders[c];
		if (bitmask_all(processors[cluster_leader].flags, CPUINFO_ARM_LINUX_VALID_MIDR)) {
			continue;
		}

		const uint32_t midr = (cluster_leader == little_cluster_leader) ? little_midr : big_midr;
		cpuinfo_log_info("assume processor %" PRIu32 " to have MIDR %08" PRIx32, cluster_leader, midr);
		/* To be consistent, we copy the MIDR entirely, rather than by
		 * parts */
		processors[cluster_leader].midr = midr;
		processors[cluster_leader].flags |= midr_flags;
	}
	return true;
}

/*
 * Initializes MIDR for leaders of core clusters in a single sequential scan:
 *  - Clusters preceding the first reported MIDR value are assumed to have
 * default MIDR value.
 *  - Clusters following any reported MIDR value to have that MIDR value.
 *
 * @param default_midr - MIDR value that will be assigned to cluster leaders
 * preceding any reported MIDR value.
 * @param processors_count - number of logical processor descriptions in the @p
 * processors array.
 * @param[in,out] processors - array of logical processor descriptions with
 * pre-parsed MIDR, maximum frequency, and decoded core cluster
 * (package_leader_id) information. Upon successful return, processors[i].midr
 * for all core clusters' leaders contains the assigned MIDR value.
 */
static void cpuinfo_arm_linux_detect_cluster_midr_by_sequential_scan(
	uint32_t default_midr,
	uint32_t processors_count,
	struct cpuinfo_arm_linux_processor processors[restrict static processors_count]) {
	uint32_t midr = default_midr;
	for (uint32_t i = 0; i < processors_count; i++) {
		if (bitmask_all(processors[i].flags, CPUINFO_LINUX_FLAG_VALID)) {
			if (processors[i].package_leader_id == i) {
				if (bitmask_all(processors[i].flags, CPUINFO_ARM_LINUX_VALID_MIDR)) {
					midr = processors[i].midr;
				} else {
					cpuinfo_log_info(
						"assume processor %" PRIu32 " to have MIDR %08" PRIx32, i, midr);
					/* To be consistent, we copy the MIDR
					 * entirely, rather than by parts
					 */
					processors[i].midr = midr;
					processors[i].flags |= CPUINFO_ARM_LINUX_VALID_MIDR;
				}
			}
		}
	}
}

/*
 * Detects MIDR of each CPU core clusters' leader.
 *
 * @param[in] chipset - chipset (SoC) name information.
 * @param max_processors - number of processor descriptions in the @p processors
 * array.
 * @param usable_processors - number of processor descriptions in the @p
 * processors array with both POSSIBLE and PRESENT flags.
 * @param[in,out] processors - array of logical processor descriptions with
 * pre-parsed MIDR, maximum frequency, and decoded core cluster
 * (package_leader_id) information. Upon return, processors[i].midr for all
 * clusters' leaders contains the MIDR value.
 *
 * @returns The number of core clusters
 */
uint32_t cpuinfo_arm_linux_detect_cluster_midr(
	const struct cpuinfo_arm_chipset chipset[restrict static 1],
	uint32_t max_processors,
	uint32_t usable_processors,
	struct cpuinfo_arm_linux_processor processors[restrict static max_processors]) {
	uint32_t clusters_count = 0;
	uint32_t cluster_leaders[CLUSTERS_MAX];
	uint32_t last_processor_in_cpuinfo = max_processors;
	uint32_t last_processor_with_midr = max_processors;
	uint32_t processors_with_midr_count = 0;
	for (uint32_t i = 0; i < max_processors; i++) {
		if (bitmask_all(processors[i].flags, CPUINFO_LINUX_FLAG_VALID)) {
			if (processors[i].flags & CPUINFO_ARM_LINUX_VALID_PROCESSOR) {
				last_processor_in_cpuinfo = i;
			}
			if (bitmask_all(
				    processors[i].flags,
				    CPUINFO_ARM_LINUX_VALID_IMPLEMENTER | CPUINFO_ARM_LINUX_VALID_PART)) {
				last_processor_with_midr = i;
				processors_with_midr_count += 1;
			}
			const uint32_t group_leader = processors[i].package_leader_id;
			if (group_leader == i) {
				if (clusters_count < CLUSTERS_MAX) {
					cluster_leaders[clusters_count] = i;
				}
				clusters_count += 1;
			} else {
				/* Copy known bits of information to cluster
				 * leader */

				if ((processors[i].flags & ~processors[group_leader].flags) &
				    CPUINFO_LINUX_FLAG_MAX_FREQUENCY) {
					processors[group_leader].max_frequency = processors[i].max_frequency;
					processors[group_leader].flags |= CPUINFO_LINUX_FLAG_MAX_FREQUENCY;
				}
				if (!bitmask_all(processors[group_leader].flags, CPUINFO_ARM_LINUX_VALID_MIDR) &&
				    bitmask_all(processors[i].flags, CPUINFO_ARM_LINUX_VALID_MIDR)) {
					processors[group_leader].midr = processors[i].midr;
					processors[group_leader].flags |= CPUINFO_ARM_LINUX_VALID_MIDR;
				}
			}
		}
	}
	cpuinfo_log_debug("detected %" PRIu32 " core clusters", clusters_count);

	/*
	 * Two relations between reported /proc/cpuinfo information, and cores
	 * is possible:
	 * - /proc/cpuinfo reports information for all or some of the cores
	 * below the corresponding "processor : <number>" lines. Information on
	 * offline cores may be missing.
	 * - /proc/cpuinfo reports information only once, after all "processor :
	 * <number>" lines. The reported information may relate to processor #0
	 * or to the processor which executed the system calls to read
	 * /proc/cpuinfo. It is also indistinguishable from /proc/cpuinfo
	 * reporting information only for the last core (e.g. if all other cores
	 * are offline).
	 *
	 * We detect the second case by checking if /proc/cpuinfo contains valid
	 * MIDR only for one, last reported, processor. Note, that the last
	 * reported core may be not the last present & possible processor, as
	 * /proc/cpuinfo may non-report high-index offline cores.
	 */
	if (processors_with_midr_count == 1 && last_processor_in_cpuinfo == last_processor_with_midr &&
	    clusters_count > 1) {
		/*
		 * There are multiple core clusters, but /proc/cpuinfo reported
		 * MIDR only for one processor, and we don't even know which
		 * logical processor this information refers to.
		 *
		 * We make three attempts to detect MIDR for all clusters:
		 * 1. Search tabulated MIDR values for chipsets which have
		 * heterogeneous clusters and ship with Linux kernels which do
		 * not always report all cores in /proc/cpuinfo. If found, use
		 * the tabulated values.
		 * 2. For systems with 2 clusters and MIDR known for one
		 * cluster, assume big.LITTLE configuration, and estimate MIDR
		 * for the other cluster under assumption that MIDR for the big
		 * cluster is known.
		 * 3. Initialize MIDRs for all core clusters to the only parsed
		 * MIDR value.
		 */
		cpuinfo_log_debug("the only reported MIDR can not be attributed to a particular processor");

		if (cpuinfo_arm_linux_detect_cluster_midr_by_chipset(
			    chipset, clusters_count, cluster_leaders, usable_processors, processors, false)) {
			return clusters_count;
		}

		/* Try big.LITTLE heuristic */
		if (cpuinfo_arm_linux_detect_cluster_midr_by_big_little_heuristic(
			    clusters_count, 1, last_processor_with_midr, cluster_leaders, processors, false)) {
			return clusters_count;
		}

		/* Fall back to sequential initialization of MIDR values for
		 * core clusters
		 */
		cpuinfo_arm_linux_detect_cluster_midr_by_sequential_scan(
			processors[processors[last_processor_with_midr].package_leader_id].midr,
			max_processors,
			processors);
	} else if (processors_with_midr_count < usable_processors) {
		/*
		 * /proc/cpuinfo reported MIDR only for some processors, and
		 * probably some core clusters do not have MIDR for any of the
		 * cores. Check if this is the case.
		 */
		uint32_t clusters_with_midr_count = 0;
		for (uint32_t i = 0; i < max_processors; i++) {
			if (bitmask_all(processors[i].flags, CPUINFO_LINUX_FLAG_VALID | CPUINFO_ARM_LINUX_VALID_MIDR)) {
				if (processors[i].package_leader_id == i) {
					clusters_with_midr_count += 1;
				}
			}
		}

		if (clusters_with_midr_count < clusters_count) {
			/*
			 * /proc/cpuinfo reported MIDR only for some clusters,
			 * need to reconstruct others. We make three attempts to
			 * detect MIDR for clusters without it:
			 * 1. Search tabulated MIDR values for chipsets which
			 * have heterogeneous clusters and ship with Linux
			 * kernels which do not always report all cores in
			 * /proc/cpuinfo. If found, use the tabulated values.
			 * 2. For systems with 2 clusters and MIDR known for one
			 * cluster, assume big.LITTLE configuration, and
			 * estimate MIDR for the other cluster under assumption
			 * that MIDR for the big cluster is known.
			 * 3. Initialize MIDRs for core clusters in a single
			 * sequential scan:
			 *    - Clusters preceding the first reported MIDR value
			 * are assumed to have the last reported MIDR value.
			 *    - Clusters following any reported MIDR value to
			 * have that MIDR value.
			 */

			if (cpuinfo_arm_linux_detect_cluster_midr_by_chipset(
				    chipset, clusters_count, cluster_leaders, usable_processors, processors, true)) {
				return clusters_count;
			}

			if (last_processor_with_midr != max_processors) {
				/* Try big.LITTLE heuristic */
				if (cpuinfo_arm_linux_detect_cluster_midr_by_big_little_heuristic(
					    clusters_count,
					    processors_with_midr_count,
					    last_processor_with_midr,
					    cluster_leaders,
					    processors,
					    true)) {
					return clusters_count;
				}

				/* Fall back to sequential initialization of
				 * MIDR values for core clusters */
				cpuinfo_arm_linux_detect_cluster_midr_by_sequential_scan(
					processors[processors[last_processor_with_midr].package_leader_id].midr,
					max_processors,
					processors);
			}
		}
	}
	return clusters_count;
}
