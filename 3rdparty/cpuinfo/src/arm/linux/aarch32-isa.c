#include <stdint.h>

#if CPUINFO_MOCK
	#include <cpuinfo-mock.h>
#endif
#include <arm/linux/api.h>
#include <arm/linux/cp.h>
#include <arm/midr.h>
#include <cpuinfo/log.h>


#if CPUINFO_MOCK
	uint32_t cpuinfo_arm_fpsid = 0;
	uint32_t cpuinfo_arm_mvfr0 = 0;
	uint32_t cpuinfo_arm_wcid = 0;

	void cpuinfo_set_fpsid(uint32_t fpsid) {
		cpuinfo_arm_fpsid = fpsid;
	}

	void cpuinfo_set_wcid(uint32_t wcid) {
		cpuinfo_arm_wcid = wcid;
	}
#endif


void cpuinfo_arm_linux_decode_isa_from_proc_cpuinfo(
	uint32_t features,
	uint32_t features2,
	uint32_t midr,
	uint32_t architecture_version,
	uint32_t architecture_flags,
	const struct cpuinfo_arm_chipset chipset[restrict static 1],
	struct cpuinfo_arm_isa isa[restrict static 1])
{
	if (architecture_version >= 8) {
		/*
		 * ARMv7 code running on ARMv8: IDIV, VFP, NEON are always supported,
		 * but may be not reported in /proc/cpuinfo features.
		 */
		isa->armv5e  = true;
		isa->armv6   = true;
		isa->armv6k  = true;
		isa->armv7   = true;
		isa->armv7mp = true;
		isa->armv8   = true;
		isa->thumb  = true;
		isa->thumb2 = true;
		isa->idiv = true;
		isa->vfpv3 = true;
		isa->d32 = true;
		isa->fp16 = true;
		isa->fma = true;
		isa->neon = true;

		/*
		 * NEON FP16 compute extension and VQRDMLAH/VQRDMLSH instructions are not indicated in /proc/cpuinfo.
		 * Use a MIDR-based heuristic to whitelist processors known to support it:
		 * - Processors with Cortex-A55 cores
		 * - Processors with Cortex-A65 cores
		 * - Processors with Cortex-A75 cores
		 * - Processors with Cortex-A76 cores
		 * - Processors with Cortex-A77 cores
		 * - Processors with Exynos M4 cores
		 * - Processors with Exynos M5 cores
		 * - Neoverse N1 cores
		 */
		if (chipset->series == cpuinfo_arm_chipset_series_samsung_exynos && chipset->model == 9810) {
			/* Only little cores of Exynos 9810 support FP16 & RDM */
			cpuinfo_log_warning("FP16 arithmetics and RDM disabled: only little cores in Exynos 9810 support these extensions");
		} else {
			switch (midr & (CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | CPUINFO_ARM_MIDR_PART_MASK)) {
				case UINT32_C(0x4100D050): /* Cortex-A55 */
				case UINT32_C(0x4100D060): /* Cortex-A65 */
				case UINT32_C(0x4100D0B0): /* Cortex-A76 */
				case UINT32_C(0x4100D0C0): /* Neoverse N1 */
				case UINT32_C(0x4100D0D0): /* Cortex-A77 */
				case UINT32_C(0x4100D0E0): /* Cortex-A76AE */
				case UINT32_C(0x4800D400): /* Cortex-A76 (HiSilicon) */
				case UINT32_C(0x51008020): /* Kryo 385 Gold (Cortex-A75) */
				case UINT32_C(0x51008030): /* Kryo 385 Silver (Cortex-A55) */
				case UINT32_C(0x51008040): /* Kryo 485 Gold (Cortex-A76) */
				case UINT32_C(0x51008050): /* Kryo 485 Silver (Cortex-A55) */
				case UINT32_C(0x53000030): /* Exynos M4 */
				case UINT32_C(0x53000040): /* Exynos M5 */
					isa->fp16arith = true;
					isa->rdm = true;
					break;
			}
		}

		/*
		 * NEON VDOT instructions are not indicated in /proc/cpuinfo.
		 * Use a MIDR-based heuristic to whitelist processors known to support it.
		 */
		switch (midr & (CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | CPUINFO_ARM_MIDR_PART_MASK)) {
			case UINT32_C(0x4100D0B0): /* Cortex-A76 */
			case UINT32_C(0x4100D0D0): /* Cortex-A77 */
			case UINT32_C(0x4100D0E0): /* Cortex-A76AE */
			case UINT32_C(0x4800D400): /* Cortex-A76 (HiSilicon) */
			case UINT32_C(0x51008040): /* Kryo 485 Gold (Cortex-A76) */
			case UINT32_C(0x51008050): /* Kryo 485 Silver (Cortex-A55) */
			case UINT32_C(0x53000030): /* Exynos-M4 */
			case UINT32_C(0x53000040): /* Exynos-M5 */
				isa->dot = true;
				break;
			case UINT32_C(0x4100D050): /* Cortex A55: revision 1 or later only */
				isa->dot = !!(midr_get_variant(midr) >= 1);
				break;
			case UINT32_C(0x4100D0A0): /* Cortex A75: revision 2 or later only */
				isa->dot = !!(midr_get_variant(midr) >= 2);
				break;
		}
	} else {
		/* ARMv7 or lower: use feature flags to detect optional features */

		/*
		 * ARM11 (ARM 1136/1156/1176/11 MPCore) processors can report v7 architecture
		 * even though they support only ARMv6 instruction set.
		 */
		if (architecture_version == 7 && midr_is_arm11(midr)) {
			cpuinfo_log_warning("kernel-reported architecture ARMv7 ignored due to mismatch with processor microarchitecture (ARM11)");
			architecture_version = 6;
		}

		if (architecture_version < 7) {
			const uint32_t armv7_features_mask = CPUINFO_ARM_LINUX_FEATURE_VFPV3 | CPUINFO_ARM_LINUX_FEATURE_VFPV3D16 | CPUINFO_ARM_LINUX_FEATURE_VFPD32 |
				CPUINFO_ARM_LINUX_FEATURE_VFPV4 | CPUINFO_ARM_LINUX_FEATURE_NEON | CPUINFO_ARM_LINUX_FEATURE_IDIVT | CPUINFO_ARM_LINUX_FEATURE_IDIVA;
			if (features & armv7_features_mask) {
				architecture_version = 7;
			}
		}
		if ((architecture_version >= 6) || (features & CPUINFO_ARM_LINUX_FEATURE_EDSP) || (architecture_flags & CPUINFO_ARM_LINUX_ARCH_E)) {
			isa->armv5e = true;
		}
		if (architecture_version >= 6) {
			isa->armv6 = true;
		}
		if (architecture_version >= 7) {
			isa->armv6k = true;
			isa->armv7 = true;

			/*
			 * ARMv7 MP extension (PLDW instruction) is not indicated in /proc/cpuinfo.
			 * Use heuristic list of supporting processors:
			 * - Processors supporting UDIV/SDIV instructions ("idiva" + "idivt" features in /proc/cpuinfo)
			 * - Cortex-A5
			 * - Cortex-A9
			 * - Dual-Core Scorpion
			 * - Krait (supports UDIV/SDIV, but kernels may not report it in /proc/cpuinfo)
			 *
			 * TODO: check single-core Qualcomm Scorpion.
			 */
			switch (midr & (CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | CPUINFO_ARM_MIDR_PART_MASK)) {
				case UINT32_C(0x4100C050): /* Cortex-A5 */
				case UINT32_C(0x4100C090): /* Cortex-A9 */
				case UINT32_C(0x510002D0): /* Scorpion (dual-core) */
				case UINT32_C(0x510004D0): /* Krait (dual-core) */
				case UINT32_C(0x510006F0): /* Krait (quad-core) */
					isa->armv7mp = true;
					break;
				default:
					/* In practice IDIV instruction implies ARMv7+MP ISA */
					isa->armv7mp = (features & CPUINFO_ARM_LINUX_FEATURE_IDIV) == CPUINFO_ARM_LINUX_FEATURE_IDIV;
					break;
			}
		}

		if (features & CPUINFO_ARM_LINUX_FEATURE_IWMMXT) {
			const uint32_t wcid = read_wcid();
			cpuinfo_log_debug("WCID = 0x%08"PRIx32, wcid);
			const uint32_t coprocessor_type = (wcid >> 8) & UINT32_C(0xFF);
			if (coprocessor_type >= 0x10) {
				isa->wmmx = true;
				if (coprocessor_type >= 0x20) {
					isa->wmmx2 = true;
				}
			} else {
				cpuinfo_log_warning("WMMX ISA disabled: OS reported iwmmxt feature, "
					"but WCID coprocessor type 0x%"PRIx32" indicates no WMMX support",
					coprocessor_type);
			}
		}

		if ((features & CPUINFO_ARM_LINUX_FEATURE_THUMB) || (architecture_flags & CPUINFO_ARM_LINUX_ARCH_T)) {
			isa->thumb = true;

			/*
			 * There is no separate feature flag for Thumb 2.
			 * All ARMv7 processors and ARM 1156 support Thumb 2.
			 */
			if (architecture_version >= 7 || midr_is_arm1156(midr)) {
				isa->thumb2 = true;
			}
		}
		if (features & CPUINFO_ARM_LINUX_FEATURE_THUMBEE) {
			isa->thumbee = true;
		}
		if ((features & CPUINFO_ARM_LINUX_FEATURE_JAVA) || (architecture_flags & CPUINFO_ARM_LINUX_ARCH_J)) {
			isa->jazelle = true;
		}

		/* Qualcomm Krait may have buggy kernel configuration that doesn't report IDIV */
		if ((features & CPUINFO_ARM_LINUX_FEATURE_IDIV) == CPUINFO_ARM_LINUX_FEATURE_IDIV || midr_is_krait(midr)) {
			isa->idiv = true;
		}

		const uint32_t vfp_mask = \
			CPUINFO_ARM_LINUX_FEATURE_VFP | CPUINFO_ARM_LINUX_FEATURE_VFPV3 | CPUINFO_ARM_LINUX_FEATURE_VFPV3D16 | \
			CPUINFO_ARM_LINUX_FEATURE_VFPD32 | CPUINFO_ARM_LINUX_FEATURE_VFPV4 | CPUINFO_ARM_LINUX_FEATURE_NEON;
		if (features & vfp_mask) {
			const uint32_t vfpv3_mask = CPUINFO_ARM_LINUX_FEATURE_VFPV3 | CPUINFO_ARM_LINUX_FEATURE_VFPV3D16 | \
				CPUINFO_ARM_LINUX_FEATURE_VFPD32 | CPUINFO_ARM_LINUX_FEATURE_VFPV4 | CPUINFO_ARM_LINUX_FEATURE_NEON;
			if ((architecture_version >= 7) || (features & vfpv3_mask)) {
				isa->vfpv3 = true;

				const uint32_t d32_mask = CPUINFO_ARM_LINUX_FEATURE_VFPD32 | CPUINFO_ARM_LINUX_FEATURE_NEON;
				if (features & d32_mask) {
					isa->d32 = true;
				}
			} else {
				#if defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_8A__) || defined(__ARM_ARCH) && (__ARM_ARCH >= 7)
					isa->vfpv3 = true;
				#else
					const uint32_t fpsid = read_fpsid();
					cpuinfo_log_debug("FPSID = 0x%08"PRIx32, fpsid);
					const uint32_t subarchitecture = (fpsid >> 16) & UINT32_C(0x7F);
					if (subarchitecture >= 0x01) {
						isa->vfpv2 = true;
					}
				#endif
			}
		}
		if (features & CPUINFO_ARM_LINUX_FEATURE_NEON) {
			isa->neon = true;
		}

		/*
		 * There is no separate feature flag for FP16 support.
		 * VFPv4 implies VFPv3-FP16 support (and in practice, NEON-HP as well).
		 * Additionally, ARM Cortex-A9 and Qualcomm Scorpion support FP16.
		 */
		if ((features & CPUINFO_ARM_LINUX_FEATURE_VFPV4) || midr_is_cortex_a9(midr) || midr_is_scorpion(midr)) {
			isa->fp16 = true;
		}

		if (features & CPUINFO_ARM_LINUX_FEATURE_VFPV4) {
			isa->fma = true;
		}
	}

	if (features2 & CPUINFO_ARM_LINUX_FEATURE2_AES) {
		isa->aes = true;
	}
	if (features2 & CPUINFO_ARM_LINUX_FEATURE2_PMULL) {
		isa->pmull = true;
	}
	if (features2 & CPUINFO_ARM_LINUX_FEATURE2_SHA1) {
		isa->sha1 = true;
	}
	if (features2 & CPUINFO_ARM_LINUX_FEATURE2_SHA2) {
		isa->sha2 = true;
	}
	if (features2 & CPUINFO_ARM_LINUX_FEATURE2_CRC32) {
		isa->crc32 = true;
	}
}
