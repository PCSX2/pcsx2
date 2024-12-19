#include <stdint.h>

#include <arm/linux/api.h>
#include <cpuinfo/log.h>

#include <sys/prctl.h>

void cpuinfo_arm64_linux_decode_isa_from_proc_cpuinfo(
	uint32_t features,
	uint64_t features2,
	uint32_t midr,
	const struct cpuinfo_arm_chipset chipset[restrict static 1],
	struct cpuinfo_arm_isa isa[restrict static 1]) {
	if (features & CPUINFO_ARM_LINUX_FEATURE_AES) {
		isa->aes = true;
	}
	if (features & CPUINFO_ARM_LINUX_FEATURE_PMULL) {
		isa->pmull = true;
	}
	if (features & CPUINFO_ARM_LINUX_FEATURE_SHA1) {
		isa->sha1 = true;
	}
	if (features & CPUINFO_ARM_LINUX_FEATURE_SHA2) {
		isa->sha2 = true;
	}
	if (features & CPUINFO_ARM_LINUX_FEATURE_CRC32) {
		isa->crc32 = true;
	}
	if (features & CPUINFO_ARM_LINUX_FEATURE_ATOMICS) {
		isa->atomics = true;
	}

	/*
	 * Some phones ship with an old kernel configuration that doesn't report
	 * NEON FP16 compute extension and SQRDMLAH/SQRDMLSH/UQRDMLAH/UQRDMLSH
	 * instructions. Use a MIDR-based heuristic to whitelist processors
	 * known to support it:
	 * - Processors with Cortex-A55 cores
	 * - Processors with Cortex-A65 cores
	 * - Processors with Cortex-A75 cores
	 * - Processors with Cortex-A76 cores
	 * - Processors with Cortex-A77 cores
	 * - Processors with Exynos M4 cores
	 * - Processors with Exynos M5 cores
	 * - Neoverse N1 cores
	 * - Neoverse V1 cores
	 * - Neoverse N2 cores
	 * - Neoverse V2 cores
	 */
	if (chipset->series == cpuinfo_arm_chipset_series_samsung_exynos && chipset->model == 9810) {
		/* Exynos 9810 reports that it supports FP16 compute, but in
		 * fact only little cores do */
		cpuinfo_log_warning(
			"FP16 arithmetics and RDM disabled: only little cores in Exynos 9810 support these extensions");
	} else {
		const uint32_t fp16arith_mask = CPUINFO_ARM_LINUX_FEATURE_FPHP | CPUINFO_ARM_LINUX_FEATURE_ASIMDHP;
		switch (midr & (CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | CPUINFO_ARM_MIDR_PART_MASK)) {
			case UINT32_C(0x4100D050): /* Cortex-A55 */
			case UINT32_C(0x4100D060): /* Cortex-A65 */
			case UINT32_C(0x4100D0A0): /* Cortex-A75 */
			case UINT32_C(0x4100D0B0): /* Cortex-A76 */
			case UINT32_C(0x4100D0C0): /* Neoverse N1 */
			case UINT32_C(0x4100D0D0): /* Cortex-A77 */
			case UINT32_C(0x4100D0E0): /* Cortex-A76AE */
			case UINT32_C(0x4100D400): /* Neoverse V1 */
			case UINT32_C(0x4100D490): /* Neoverse N2 */
			case UINT32_C(0x4100D4F0): /* Neoverse V2 */
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
			default:
				if ((features & fp16arith_mask) == fp16arith_mask) {
					isa->fp16arith = true;
				} else if (features & CPUINFO_ARM_LINUX_FEATURE_FPHP) {
					cpuinfo_log_warning(
						"FP16 arithmetics disabled: detected support only for scalar operations");
				} else if (features & CPUINFO_ARM_LINUX_FEATURE_ASIMDHP) {
					cpuinfo_log_warning(
						"FP16 arithmetics disabled: detected support only for SIMD operations");
				}
				if (features & CPUINFO_ARM_LINUX_FEATURE_ASIMDRDM) {
					isa->rdm = true;
				}
				break;
		}
	}
	if (features2 & CPUINFO_ARM_LINUX_FEATURE2_I8MM) {
		isa->i8mm = true;
	}

	/*
	 * Many phones ship with an old kernel configuration that doesn't report
	 * UDOT/SDOT instructions. Use a MIDR-based heuristic to whitelist
	 * processors known to support it.
	 */
	switch (midr & (CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | CPUINFO_ARM_MIDR_PART_MASK)) {
		case UINT32_C(0x4100D060): /* Cortex-A65 */
		case UINT32_C(0x4100D0B0): /* Cortex-A76 */
		case UINT32_C(0x4100D0C0): /* Neoverse N1 */
		case UINT32_C(0x4100D0D0): /* Cortex-A77 */
		case UINT32_C(0x4100D0E0): /* Cortex-A76AE */
		case UINT32_C(0x4100D400): /* Neoverse V1 */
		case UINT32_C(0x4100D490): /* Neoverse N2 */
		case UINT32_C(0x4100D4A0): /* Neoverse E1 */
		case UINT32_C(0x4100D4F0): /* Neoverse V2 */
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
		default:
			if (features & CPUINFO_ARM_LINUX_FEATURE_ASIMDDP) {
				isa->dot = true;
			}
			break;
	}
	if (features & CPUINFO_ARM_LINUX_FEATURE_JSCVT) {
		isa->jscvt = true;
	}
	if (features & CPUINFO_ARM_LINUX_FEATURE_JSCVT) {
		isa->jscvt = true;
	}
	if (features & CPUINFO_ARM_LINUX_FEATURE_FCMA) {
		isa->fcma = true;
	}
	if (features & CPUINFO_ARM_LINUX_FEATURE_SVE) {
		isa->sve = true;
	}
	if (features2 & CPUINFO_ARM_LINUX_FEATURE2_SVE2) {
		isa->sve2 = true;
	}
	if (features2 & CPUINFO_ARM_LINUX_FEATURE2_SME) {
		isa->sme = true;
	}
	if (features2 & CPUINFO_ARM_LINUX_FEATURE2_SME2) {
		isa->sme2 = true;
	}
	if (features2 & CPUINFO_ARM_LINUX_FEATURE2_SME2P1) {
		isa->sme2p1 = true;
	}
	if (features2 & CPUINFO_ARM_LINUX_FEATURE2_SME_I16I32) {
		isa->sme_i16i32 = true;
	}
	if (features2 & CPUINFO_ARM_LINUX_FEATURE2_SME_BI32I32) {
		isa->sme_bi32i32 = true;
	}
	if (features2 & CPUINFO_ARM_LINUX_FEATURE2_SME_B16B16) {
		isa->sme_b16b16 = true;
	}
	if (features2 & CPUINFO_ARM_LINUX_FEATURE2_SME_F16F16) {
		isa->sme_f16f16 = true;
	}
	// SVEBF16 is set iff SVE and BF16 are both supported, but the SVEBF16
	// feature flag was added in Linux kernel before the BF16 feature flag,
	// so we check for either.
	if (features2 & (CPUINFO_ARM_LINUX_FEATURE2_BF16 | CPUINFO_ARM_LINUX_FEATURE2_SVEBF16)) {
		isa->bf16 = true;
	}
	if (features & CPUINFO_ARM_LINUX_FEATURE_ASIMDFHM) {
		isa->fhm = true;
	}

#ifndef PR_SVE_GET_VL
#define PR_SVE_GET_VL 51
#endif

#ifndef PR_SVE_VL_LEN_MASK
#define PR_SVE_VL_LEN_MASK 0xffff
#endif

	int ret = prctl(PR_SVE_GET_VL);
	if (ret < 0) {
		cpuinfo_log_warning("No SVE support on this machine");
		isa->svelen = 0; // Assume no SVE support if the call fails
	} else {
		// Mask out the SVE vector length bits
		isa->svelen = ret & PR_SVE_VL_LEN_MASK;
	}
}
