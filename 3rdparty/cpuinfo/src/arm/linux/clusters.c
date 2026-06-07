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

static inline bool bitmask_all(uint32_t bitfield, uint32_t mask) {
	return (bitfield & mask) == mask;
}

/*
 * Assigns logical processors to clusters of cores using heuristic based on the
 * typical configuration of clusters for 5, 6, 8, and 10 cores:
 * - 5 cores (ARM32 Android only): 2 clusters of 4+1 cores
 * - 6 cores: 2 clusters of 4+2 cores
 * - 8 cores: 2 clusters of 4+4 cores
 * - 10 cores: 3 clusters of 4+4+2 cores
 *
 * The function must be called after parsing OS-provided information on core
 * clusters. Its purpose is to detect clusters of cores when OS-provided
 * information is lacking or incomplete, i.e.
 * - Linux kernel is not configured to report information in sysfs topology
 * leaf.
 * - Linux kernel reports topology information only for online cores, and only
 * cores on one cluster are online, e.g.:
 *   - Exynos 8890 has 8 cores in 4+4 clusters, but only the first cluster of 4
 * cores is reported, and cluster configuration of logical processors 4-7 is not
 * reported (all remaining processors 4-7 form cluster 1)
 *   - MT6797 has 10 cores in 4+4+2, but only the first cluster of 4 cores is
 * reported, and cluster configuration of logical processors 4-9 is not reported
 * (processors 4-7 form cluster 1, and processors 8-9 form cluster 2).
 *
 * Heuristic assignment of processors to the above pre-defined clusters fails if
 * such assignment would contradict information provided by the operating
 * system:
 * - Any of the OS-reported processor clusters is different than the
 * corresponding heuristic cluster.
 * - Processors in a heuristic cluster have no OS-provided cluster siblings
 * information, but have known and different minimum/maximum frequency.
 * - Processors in a heuristic cluster have no OS-provided cluster siblings
 * information, but have known and different MIDR components.
 *
 * If the heuristic assignment of processors to clusters of cores fails, all
 * processors' clusters are unchanged.
 *
 * @param usable_processors - number of processors in the @p processors array
 * with CPUINFO_LINUX_FLAG_VALID flags.
 * @param max_processors - number of elements in the @p processors array.
 * @param[in,out] processors - processor descriptors with pre-parsed POSSIBLE
 * and PRESENT flags, minimum/maximum frequency, MIDR information, and core
 * cluster (package siblings list) information.
 *
 * @retval true if the heuristic successfully assigned all processors into
 * clusters of cores.
 * @retval false if known details about processors contradict the heuristic
 * configuration of core clusters.
 */
bool cpuinfo_arm_linux_detect_core_clusters_by_heuristic(
	uint32_t usable_processors,
	uint32_t max_processors,
	struct cpuinfo_arm_linux_processor processors[restrict static max_processors]) {
	uint32_t cluster_processors[3];
	switch (usable_processors) {
		case 10:
			cluster_processors[0] = 4;
			cluster_processors[1] = 4;
			cluster_processors[2] = 2;
			break;
		case 8:
			cluster_processors[0] = 4;
			cluster_processors[1] = 4;
			break;
		case 6:
			cluster_processors[0] = 4;
			cluster_processors[1] = 2;
			break;
#if defined(__ANDROID__) && CPUINFO_ARCH_ARM
		case 5:
			/*
			 * The only processor with 5 cores is Leadcore L1860C
			 * (ARMv7, mobile), but this configuration is not too
			 * unreasonable for a virtualized ARM server.
			 */
			cluster_processors[0] = 4;
			cluster_processors[1] = 1;
			break;
#endif
		default:
			return false;
	}

	/*
	 * Assignment of processors to core clusters is done in two passes:
	 * 1. Verify that the clusters proposed by heuristic are compatible with
	 * known details about processors.
	 * 2. If verification passed, update core clusters for the processors.
	 */

	uint32_t cluster = 0;
	uint32_t expected_cluster_processors = 0;
	uint32_t cluster_start, cluster_flags, cluster_midr, cluster_max_frequency, cluster_min_frequency;
	bool expected_cluster_exists;
	for (uint32_t i = 0; i < max_processors; i++) {
		if (bitmask_all(processors[i].flags, CPUINFO_LINUX_FLAG_VALID)) {
			if (expected_cluster_processors == 0) {
				/* Expect this processor to start a new cluster
				 */

				expected_cluster_exists = !!(processors[i].flags & CPUINFO_LINUX_FLAG_PACKAGE_CLUSTER);
				if (expected_cluster_exists) {
					if (processors[i].package_leader_id != i) {
						cpuinfo_log_debug(
							"heuristic detection of core clusters failed: "
							"processor %" PRIu32
							" is expected to start a new cluster #%" PRIu32 " with %" PRIu32
							" cores, "
							"but system siblings lists reported it as a sibling of processor %" PRIu32,
							i,
							cluster,
							cluster_processors[cluster],
							processors[i].package_leader_id);
						return false;
					}
				} else {
					cluster_flags = 0;
				}

				cluster_start = i;
				expected_cluster_processors = cluster_processors[cluster++];
			} else {
				/* Expect this processor to belong to the same
				 * cluster as processor */

				if (expected_cluster_exists) {
					/*
					 * The cluster suggested by the
					 * heuristic was already parsed from
					 * system siblings lists. For all
					 * processors we expect in the cluster,
					 * check that:
					 * - They have pre-assigned cluster from
					 * siblings lists
					 * (CPUINFO_LINUX_FLAG_PACKAGE_CLUSTER
					 * flag).
					 * - They were assigned to the same
					 * cluster based on siblings lists
					 *   (package_leader_id points to the
					 * first processor in the cluster).
					 */

					if ((processors[i].flags & CPUINFO_LINUX_FLAG_PACKAGE_CLUSTER) == 0) {
						cpuinfo_log_debug(
							"heuristic detection of core clusters failed: "
							"processor %" PRIu32
							" is expected to belong to the cluster of processor %" PRIu32
							", "
							"but system siblings lists did not report it as a sibling of processor %" PRIu32,
							i,
							cluster_start,
							cluster_start);
						return false;
					}
					if (processors[i].package_leader_id != cluster_start) {
						cpuinfo_log_debug(
							"heuristic detection of core clusters failed: "
							"processor %" PRIu32
							" is expected to belong to the cluster of processor %" PRIu32
							", "
							"but system siblings lists reported it to belong to the cluster of processor %" PRIu32,
							i,
							cluster_start,
							cluster_start);
						return false;
					}
				} else {
					/*
					 * The cluster suggest by the heuristic
					 * was not parsed from system siblings
					 * lists. For all processors we expect
					 * in the cluster, check that:
					 * - They have no pre-assigned cluster
					 * from siblings lists.
					 * - If their min/max CPU frequency is
					 * known, it is the same.
					 * - If any part of their MIDR
					 * (Implementer, Variant, Part,
					 * Revision) is known, it is the same.
					 */

					if (processors[i].flags & CPUINFO_LINUX_FLAG_PACKAGE_CLUSTER) {
						cpuinfo_log_debug(
							"heuristic detection of core clusters failed: "
							"processor %" PRIu32
							" is expected to be unassigned to any cluster, "
							"but system siblings lists reported it to belong to the cluster of processor %" PRIu32,
							i,
							processors[i].package_leader_id);
						return false;
					}

					if (processors[i].flags & CPUINFO_LINUX_FLAG_MIN_FREQUENCY) {
						if (cluster_flags & CPUINFO_LINUX_FLAG_MIN_FREQUENCY) {
							if (cluster_min_frequency != processors[i].min_frequency) {
								cpuinfo_log_debug(
									"heuristic detection of core clusters failed: "
									"minimum frequency of processor %" PRIu32
									" (%" PRIu32
									" KHz) is different than of its expected cluster (%" PRIu32
									" KHz)",
									i,
									processors[i].min_frequency,
									cluster_min_frequency);
								return false;
							}
						} else {
							cluster_min_frequency = processors[i].min_frequency;
							cluster_flags |= CPUINFO_LINUX_FLAG_MIN_FREQUENCY;
						}
					}

					if (processors[i].flags & CPUINFO_LINUX_FLAG_MAX_FREQUENCY) {
						if (cluster_flags & CPUINFO_LINUX_FLAG_MAX_FREQUENCY) {
							if (cluster_max_frequency != processors[i].max_frequency) {
								cpuinfo_log_debug(
									"heuristic detection of core clusters failed: "
									"maximum frequency of processor %" PRIu32
									" (%" PRIu32
									" KHz) is different than of its expected cluster (%" PRIu32
									" KHz)",
									i,
									processors[i].max_frequency,
									cluster_max_frequency);
								return false;
							}
						} else {
							cluster_max_frequency = processors[i].max_frequency;
							cluster_flags |= CPUINFO_LINUX_FLAG_MAX_FREQUENCY;
						}
					}

					if (processors[i].flags & CPUINFO_ARM_LINUX_VALID_IMPLEMENTER) {
						if (cluster_flags & CPUINFO_ARM_LINUX_VALID_IMPLEMENTER) {
							if ((cluster_midr & CPUINFO_ARM_MIDR_IMPLEMENTER_MASK) !=
							    (processors[i].midr & CPUINFO_ARM_MIDR_IMPLEMENTER_MASK)) {
								cpuinfo_log_debug(
									"heuristic detection of core clusters failed: "
									"CPU Implementer of processor %" PRIu32
									" (0x%02" PRIx32
									") is different than of its expected cluster (0x%02" PRIx32
									")",
									i,
									midr_get_implementer(processors[i].midr),
									midr_get_implementer(cluster_midr));
								return false;
							}
						} else {
							cluster_midr =
								midr_copy_implementer(cluster_midr, processors[i].midr);
							cluster_flags |= CPUINFO_ARM_LINUX_VALID_IMPLEMENTER;
						}
					}

					if (processors[i].flags & CPUINFO_ARM_LINUX_VALID_VARIANT) {
						if (cluster_flags & CPUINFO_ARM_LINUX_VALID_VARIANT) {
							if ((cluster_midr & CPUINFO_ARM_MIDR_VARIANT_MASK) !=
							    (processors[i].midr & CPUINFO_ARM_MIDR_VARIANT_MASK)) {
								cpuinfo_log_debug(
									"heuristic detection of core clusters failed: "
									"CPU Variant of processor %" PRIu32
									" (0x%" PRIx32
									") is different than of its expected cluster (0x%" PRIx32
									")",
									i,
									midr_get_variant(processors[i].midr),
									midr_get_variant(cluster_midr));
								return false;
							}
						} else {
							cluster_midr =
								midr_copy_variant(cluster_midr, processors[i].midr);
							cluster_flags |= CPUINFO_ARM_LINUX_VALID_VARIANT;
						}
					}

					if (processors[i].flags & CPUINFO_ARM_LINUX_VALID_PART) {
						if (cluster_flags & CPUINFO_ARM_LINUX_VALID_PART) {
							if ((cluster_midr & CPUINFO_ARM_MIDR_PART_MASK) !=
							    (processors[i].midr & CPUINFO_ARM_MIDR_PART_MASK)) {
								cpuinfo_log_debug(
									"heuristic detection of core clusters failed: "
									"CPU Part of processor %" PRIu32
									" (0x%03" PRIx32
									") is different than of its expected cluster (0x%03" PRIx32
									")",
									i,
									midr_get_part(processors[i].midr),
									midr_get_part(cluster_midr));
								return false;
							}
						} else {
							cluster_midr = midr_copy_part(cluster_midr, processors[i].midr);
							cluster_flags |= CPUINFO_ARM_LINUX_VALID_PART;
						}
					}

					if (processors[i].flags & CPUINFO_ARM_LINUX_VALID_REVISION) {
						if (cluster_flags & CPUINFO_ARM_LINUX_VALID_REVISION) {
							if ((cluster_midr & CPUINFO_ARM_MIDR_REVISION_MASK) !=
							    (processors[i].midr & CPUINFO_ARM_MIDR_REVISION_MASK)) {
								cpuinfo_log_debug(
									"heuristic detection of core clusters failed: "
									"CPU Revision of processor %" PRIu32
									" (0x%" PRIx32
									") is different than of its expected cluster (0x%" PRIx32
									")",
									i,
									midr_get_revision(cluster_midr),
									midr_get_revision(processors[i].midr));
								return false;
							}
						} else {
							cluster_midr =
								midr_copy_revision(cluster_midr, processors[i].midr);
							cluster_flags |= CPUINFO_ARM_LINUX_VALID_REVISION;
						}
					}
				}
			}
			expected_cluster_processors--;
		}
	}

	/* Verification passed, assign all processors to new clusters */
	cluster = 0;
	expected_cluster_processors = 0;
	for (uint32_t i = 0; i < max_processors; i++) {
		if (bitmask_all(processors[i].flags, CPUINFO_LINUX_FLAG_VALID)) {
			if (expected_cluster_processors == 0) {
				/* Expect this processor to start a new cluster
				 */

				cluster_start = i;
				expected_cluster_processors = cluster_processors[cluster++];
			} else {
				/* Expect this processor to belong to the same
				 * cluster as processor */

				if (!(processors[i].flags & CPUINFO_LINUX_FLAG_PACKAGE_CLUSTER)) {
					cpuinfo_log_debug(
						"assigned processor %" PRIu32 " to cluster of processor %" PRIu32
						" based on heuristic",
						i,
						cluster_start);
				}

				processors[i].package_leader_id = cluster_start;
				processors[i].flags |= CPUINFO_LINUX_FLAG_PACKAGE_CLUSTER;
			}
			expected_cluster_processors--;
		}
	}
	return true;
}

/*
 * Assigns logical processors to clusters of cores in sequential manner:
 * - Clusters detected from OS-provided information are unchanged:
 *   - Processors assigned to these clusters stay assigned to the same clusters
 *   - No new processors are added to these clusters
 * - Processors without pre-assigned cluster are clustered in one sequential
 * scan:
 *   - If known details (min/max frequency, MIDR components) of a processor are
 * compatible with a preceding processor, without pre-assigned cluster, the
 * processor is assigned to the cluster of the preceding processor.
 *   - If known details (min/max frequency, MIDR components) of a processor are
 * not compatible with a preceding processor, the processor is assigned to a
 * newly created cluster.
 *
 * The function must be called after parsing OS-provided information on core
 * clusters, and usually is called only if heuristic assignment of processors to
 * clusters (cpuinfo_arm_linux_cluster_processors_by_heuristic) failed.
 *
 * Its purpose is to detect clusters of cores when OS-provided information is
 * lacking or incomplete, i.e.
 * - Linux kernel is not configured to report information in sysfs topology
 * leaf.
 * - Linux kernel reports topology information only for online cores, and all
 * cores on some of the clusters are offline.
 *
 * Sequential assignment of processors to clusters always succeeds, and upon
 * exit, all usable processors in the
 * @p processors array have cluster information.
 *
 * @param max_processors - number of elements in the @p processors array.
 * @param[in,out] processors - processor descriptors with pre-parsed POSSIBLE
 * and PRESENT flags, minimum/maximum frequency, MIDR information, and core
 * cluster (package siblings list) information.
 *
 * @retval true if the heuristic successfully assigned all processors into
 * clusters of cores.
 * @retval false if known details about processors contradict the heuristic
 * configuration of core clusters.
 */
void cpuinfo_arm_linux_detect_core_clusters_by_sequential_scan(
	uint32_t max_processors,
	struct cpuinfo_arm_linux_processor processors[restrict static max_processors]) {
	uint32_t cluster_flags = 0;
	uint32_t cluster_processors = 0;
	uint32_t cluster_start, cluster_midr, cluster_max_frequency, cluster_min_frequency;
	for (uint32_t i = 0; i < max_processors; i++) {
		if ((processors[i].flags & (CPUINFO_LINUX_FLAG_VALID | CPUINFO_LINUX_FLAG_PACKAGE_CLUSTER)) ==
		    CPUINFO_LINUX_FLAG_VALID) {
			if (cluster_processors == 0) {
				goto new_cluster;
			}

			if (processors[i].flags & CPUINFO_LINUX_FLAG_MIN_FREQUENCY) {
				if (cluster_flags & CPUINFO_LINUX_FLAG_MIN_FREQUENCY) {
					if (cluster_min_frequency != processors[i].min_frequency) {
						cpuinfo_log_info(
							"minimum frequency of processor %" PRIu32 " (%" PRIu32
							" KHz) is different than of preceding cluster (%" PRIu32
							" KHz); "
							"processor %" PRIu32 " starts to a new cluster",
							i,
							processors[i].min_frequency,
							cluster_min_frequency,
							i);
						goto new_cluster;
					}
				} else {
					cluster_min_frequency = processors[i].min_frequency;
					cluster_flags |= CPUINFO_LINUX_FLAG_MIN_FREQUENCY;
				}
			}

			if (processors[i].flags & CPUINFO_LINUX_FLAG_MAX_FREQUENCY) {
				if (cluster_flags & CPUINFO_LINUX_FLAG_MAX_FREQUENCY) {
					if (cluster_max_frequency != processors[i].max_frequency) {
						cpuinfo_log_debug(
							"maximum frequency of processor %" PRIu32 " (%" PRIu32
							" KHz) is different than of preceding cluster (%" PRIu32
							" KHz); "
							"processor %" PRIu32 " starts a new cluster",
							i,
							processors[i].max_frequency,
							cluster_max_frequency,
							i);
						goto new_cluster;
					}
				} else {
					cluster_max_frequency = processors[i].max_frequency;
					cluster_flags |= CPUINFO_LINUX_FLAG_MAX_FREQUENCY;
				}
			}

			if (processors[i].flags & CPUINFO_ARM_LINUX_VALID_IMPLEMENTER) {
				if (cluster_flags & CPUINFO_ARM_LINUX_VALID_IMPLEMENTER) {
					if ((cluster_midr & CPUINFO_ARM_MIDR_IMPLEMENTER_MASK) !=
					    (processors[i].midr & CPUINFO_ARM_MIDR_IMPLEMENTER_MASK)) {
						cpuinfo_log_debug(
							"CPU Implementer of processor %" PRIu32 " (0x%02" PRIx32
							") is different than of preceding cluster (0x%02" PRIx32
							"); "
							"processor %" PRIu32 " starts to a new cluster",
							i,
							midr_get_implementer(processors[i].midr),
							midr_get_implementer(cluster_midr),
							i);
						goto new_cluster;
					}
				} else {
					cluster_midr = midr_copy_implementer(cluster_midr, processors[i].midr);
					cluster_flags |= CPUINFO_ARM_LINUX_VALID_IMPLEMENTER;
				}
			}

			if (processors[i].flags & CPUINFO_ARM_LINUX_VALID_VARIANT) {
				if (cluster_flags & CPUINFO_ARM_LINUX_VALID_VARIANT) {
					if ((cluster_midr & CPUINFO_ARM_MIDR_VARIANT_MASK) !=
					    (processors[i].midr & CPUINFO_ARM_MIDR_VARIANT_MASK)) {
						cpuinfo_log_debug(
							"CPU Variant of processor %" PRIu32 " (0x%" PRIx32
							") is different than of its expected cluster (0x%" PRIx32
							")"
							"processor %" PRIu32 " starts to a new cluster",
							i,
							midr_get_variant(processors[i].midr),
							midr_get_variant(cluster_midr),
							i);
						goto new_cluster;
					}
				} else {
					cluster_midr = midr_copy_variant(cluster_midr, processors[i].midr);
					cluster_flags |= CPUINFO_ARM_LINUX_VALID_VARIANT;
				}
			}

			if (processors[i].flags & CPUINFO_ARM_LINUX_VALID_PART) {
				if (cluster_flags & CPUINFO_ARM_LINUX_VALID_PART) {
					if ((cluster_midr & CPUINFO_ARM_MIDR_PART_MASK) !=
					    (processors[i].midr & CPUINFO_ARM_MIDR_PART_MASK)) {
						cpuinfo_log_debug(
							"CPU Part of processor %" PRIu32 " (0x%03" PRIx32
							") is different than of its expected cluster (0x%03" PRIx32
							")"
							"processor %" PRIu32 " starts to a new cluster",
							i,
							midr_get_part(processors[i].midr),
							midr_get_part(cluster_midr),
							i);
						goto new_cluster;
					}
				} else {
					cluster_midr = midr_copy_part(cluster_midr, processors[i].midr);
					cluster_flags |= CPUINFO_ARM_LINUX_VALID_PART;
				}
			}

			if (processors[i].flags & CPUINFO_ARM_LINUX_VALID_REVISION) {
				if (cluster_flags & CPUINFO_ARM_LINUX_VALID_REVISION) {
					if ((cluster_midr & CPUINFO_ARM_MIDR_REVISION_MASK) !=
					    (processors[i].midr & CPUINFO_ARM_MIDR_REVISION_MASK)) {
						cpuinfo_log_debug(
							"CPU Revision of processor %" PRIu32 " (0x%" PRIx32
							") is different than of its expected cluster (0x%" PRIx32
							")"
							"processor %" PRIu32 " starts to a new cluster",
							i,
							midr_get_revision(cluster_midr),
							midr_get_revision(processors[i].midr),
							i);
						goto new_cluster;
					}
				} else {
					cluster_midr = midr_copy_revision(cluster_midr, processors[i].midr);
					cluster_flags |= CPUINFO_ARM_LINUX_VALID_REVISION;
				}
			}

			/* All checks passed, attach processor to the preceding
			 * cluster */
			cluster_processors++;
			processors[i].package_leader_id = cluster_start;
			processors[i].flags |= CPUINFO_LINUX_FLAG_PACKAGE_CLUSTER;
			cpuinfo_log_debug(
				"assigned processor %" PRIu32 " to preceding cluster of processor %" PRIu32,
				i,
				cluster_start);
			continue;

		new_cluster:
			/* Create a new cluster starting with processor i */
			cluster_start = i;
			processors[i].package_leader_id = i;
			processors[i].flags |= CPUINFO_LINUX_FLAG_PACKAGE_CLUSTER;
			cluster_processors = 1;

			/* Copy known information from processor to cluster, and
			 * set the flags accordingly */
			cluster_flags = 0;
			if (processors[i].flags & CPUINFO_LINUX_FLAG_MIN_FREQUENCY) {
				cluster_min_frequency = processors[i].min_frequency;
				cluster_flags |= CPUINFO_LINUX_FLAG_MIN_FREQUENCY;
			}
			if (processors[i].flags & CPUINFO_LINUX_FLAG_MAX_FREQUENCY) {
				cluster_max_frequency = processors[i].max_frequency;
				cluster_flags |= CPUINFO_LINUX_FLAG_MAX_FREQUENCY;
			}
			if (processors[i].flags & CPUINFO_ARM_LINUX_VALID_IMPLEMENTER) {
				cluster_midr = midr_copy_implementer(cluster_midr, processors[i].midr);
				cluster_flags |= CPUINFO_ARM_LINUX_VALID_IMPLEMENTER;
			}
			if (processors[i].flags & CPUINFO_ARM_LINUX_VALID_VARIANT) {
				cluster_midr = midr_copy_variant(cluster_midr, processors[i].midr);
				cluster_flags |= CPUINFO_ARM_LINUX_VALID_VARIANT;
			}
			if (processors[i].flags & CPUINFO_ARM_LINUX_VALID_PART) {
				cluster_midr = midr_copy_part(cluster_midr, processors[i].midr);
				cluster_flags |= CPUINFO_ARM_LINUX_VALID_PART;
			}
			if (processors[i].flags & CPUINFO_ARM_LINUX_VALID_REVISION) {
				cluster_midr = midr_copy_revision(cluster_midr, processors[i].midr);
				cluster_flags |= CPUINFO_ARM_LINUX_VALID_REVISION;
			}
		}
	}
}

/*
 * Counts the number of logical processors in each core cluster.
 * This function should be called after all processors are assigned to core
 * clusters.
 *
 * @param max_processors - number of elements in the @p processors array.
 * @param[in,out] processors - processor descriptors with pre-parsed POSSIBLE
 * and PRESENT flags, and decoded core cluster (package_leader_id) information.
 *                             The function expects the value of
 * processors[i].package_processor_count to be zero. Upon return,
 * processors[i].package_processor_count will contain the number of logical
 *                             processors in the respective core cluster.
 */
void cpuinfo_arm_linux_count_cluster_processors(
	uint32_t max_processors,
	struct cpuinfo_arm_linux_processor processors[restrict static max_processors]) {
	/* First pass: accumulate the number of processors at the group leader's
	 * package_processor_count */
	for (uint32_t i = 0; i < max_processors; i++) {
		if (bitmask_all(processors[i].flags, CPUINFO_LINUX_FLAG_VALID)) {
			const uint32_t package_leader_id = processors[i].package_leader_id;
			processors[package_leader_id].package_processor_count += 1;
		}
	}
	/* Second pass: copy the package_processor_count from the group leader
	 * processor */
	for (uint32_t i = 0; i < max_processors; i++) {
		if (bitmask_all(processors[i].flags, CPUINFO_LINUX_FLAG_VALID)) {
			const uint32_t package_leader_id = processors[i].package_leader_id;
			processors[i].package_processor_count = processors[package_leader_id].package_processor_count;
		}
	}
}
