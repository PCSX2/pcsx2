#include <stdint.h>

#include <arm/api.h>
#include <arm/midr.h>
#include <cpuinfo/log.h>

void cpuinfo_arm_decode_vendor_uarch(
	uint32_t midr,
#if CPUINFO_ARCH_ARM
	bool has_vfpv4,
#endif /* CPUINFO_ARCH_ARM */
	enum cpuinfo_vendor vendor[restrict static 1],
	enum cpuinfo_uarch uarch[restrict static 1]) {
	switch (midr_get_implementer(midr)) {
		case 'A':
			*vendor = cpuinfo_vendor_arm;
			switch (midr_get_part(midr)) {
#if CPUINFO_ARCH_ARM
				case 0xC05:
					*uarch = cpuinfo_uarch_cortex_a5;
					break;
				case 0xC07:
					*uarch = cpuinfo_uarch_cortex_a7;
					break;
				case 0xC08:
					*uarch = cpuinfo_uarch_cortex_a8;
					break;
				case 0xC09:
					*uarch = cpuinfo_uarch_cortex_a9;
					break;
				case 0xC0C:
					*uarch = cpuinfo_uarch_cortex_a12;
					break;
				case 0xC0E:
					*uarch = cpuinfo_uarch_cortex_a17;
					break;
				case 0xC0D:
					/*
					 * Rockchip RK3288 only.
					 * Core information is ambiguous: some
					 * sources specify Cortex-A12, others -
					 * Cortex-A17. Assume it is Cortex-A12.
					 */
					*uarch = cpuinfo_uarch_cortex_a12;
					break;
				case 0xC0F:
					*uarch = cpuinfo_uarch_cortex_a15;
					break;
#endif /* CPUINFO_ARCH_ARM */
				case 0xD01:
					*uarch = cpuinfo_uarch_cortex_a32;
					break;
				case 0xD03:
					*uarch = cpuinfo_uarch_cortex_a53;
					break;
				case 0xD04:
					*uarch = cpuinfo_uarch_cortex_a35;
					break;
				case 0xD05:
					// Note: use Variant, not Revision,
					// field
					*uarch = (midr & CPUINFO_ARM_MIDR_VARIANT_MASK) == 0
						? cpuinfo_uarch_cortex_a55r0
						: cpuinfo_uarch_cortex_a55;
					break;
				case 0xD06:
					*uarch = cpuinfo_uarch_cortex_a65;
					break;
				case 0xD07:
					*uarch = cpuinfo_uarch_cortex_a57;
					break;
				case 0xD08:
					*uarch = cpuinfo_uarch_cortex_a72;
					break;
				case 0xD09:
					*uarch = cpuinfo_uarch_cortex_a73;
					break;
				case 0xD0A:
					*uarch = cpuinfo_uarch_cortex_a75;
					break;
				case 0xD0B:
					*uarch = cpuinfo_uarch_cortex_a76;
					break;
				case 0xD0C:
					*uarch = cpuinfo_uarch_neoverse_n1;
					break;
				case 0xD0D:
					*uarch = cpuinfo_uarch_cortex_a77;
					break;
				case 0xD0E: /* Cortex-A76AE */
					*uarch = cpuinfo_uarch_cortex_a76;
					break;
				case 0xD40: /* Neoverse V1 */
					*uarch = cpuinfo_uarch_neoverse_v1;
					break;
				case 0xD41: /* Cortex-A78 */
					*uarch = cpuinfo_uarch_cortex_a78;
					break;
				case 0xD44: /* Cortex-X1 */
					*uarch = cpuinfo_uarch_cortex_x1;
					break;
				case 0xD46: /* Cortex-A510 */
					*uarch = cpuinfo_uarch_cortex_a510;
					break;
				case 0xD47: /* Cortex-A710 */
					*uarch = cpuinfo_uarch_cortex_a710;
					break;
				case 0xD48: /* Cortex-X2 */
					*uarch = cpuinfo_uarch_cortex_x2;
					break;
				case 0xD49: /* Neoverse N2 */
					*uarch = cpuinfo_uarch_neoverse_n2;
					break;
#if CPUINFO_ARCH_ARM64
				case 0xD4A:
					*uarch = cpuinfo_uarch_neoverse_e1;
					break;
#endif /* CPUINFO_ARCH_ARM64 */
				case 0xD4D: /* Cortex-A715 */
					*uarch = cpuinfo_uarch_cortex_a715;
					break;
				case 0xD4E: /* Cortex-X3 */
					*uarch = cpuinfo_uarch_cortex_x3;
					break;
				case 0xD4F: /* Neoverse V2 */
					*uarch = cpuinfo_uarch_neoverse_v2;
					break;
				default:
					switch (midr_get_part(midr) >> 8) {
#if CPUINFO_ARCH_ARM
						case 7:
							*uarch = cpuinfo_uarch_arm7;
							break;
						case 9:
							*uarch = cpuinfo_uarch_arm9;
							break;
						case 11:
							*uarch = cpuinfo_uarch_arm11;
							break;
#endif /* CPUINFO_ARCH_ARM */
						default:
							cpuinfo_log_warning(
								"unknown ARM CPU part 0x%03" PRIx32 " ignored",
								midr_get_part(midr));
					}
			}
			break;
		case 'B':
			*vendor = cpuinfo_vendor_broadcom;
			switch (midr_get_part(midr)) {
				case 0x00F:
					*uarch = cpuinfo_uarch_brahma_b15;
					break;
				case 0x100:
					*uarch = cpuinfo_uarch_brahma_b53;
					break;
#if CPUINFO_ARCH_ARM64
				case 0x516:
					/* Broadcom Vulkan was sold to Cavium
					 * before it reached the market, so we
					 * identify it as Cavium ThunderX2 */
					*vendor = cpuinfo_vendor_cavium;
					*uarch = cpuinfo_uarch_thunderx2;
					break;
#endif /* CPUINFO_ARCH_ARM64 */
				default:
					cpuinfo_log_warning(
						"unknown Broadcom CPU part 0x%03" PRIx32 " ignored",
						midr_get_part(midr));
			}
			break;
#if CPUINFO_ARCH_ARM64
		case 'C':
			*vendor = cpuinfo_vendor_cavium;
			switch (midr_get_part(midr)) {
				case 0x0A0: /* ThunderX */
				case 0x0A1: /* ThunderX 88XX */
				case 0x0A2: /* ThunderX 81XX */
				case 0x0A3: /* ThunderX 83XX */
					*uarch = cpuinfo_uarch_thunderx;
					break;
				case 0x0AF: /* ThunderX2 99XX */
					*uarch = cpuinfo_uarch_thunderx2;
					break;
				default:
					cpuinfo_log_warning(
						"unknown Cavium CPU part 0x%03" PRIx32 " ignored", midr_get_part(midr));
			}
			break;
#endif /* CPUINFO_ARCH_ARM64 */
		case 'H':
			*vendor = cpuinfo_vendor_huawei;
			switch (midr_get_part(midr)) {
#if CPUINFO_ARCH_ARM64
				case 0xD01: /* Kunpeng 920 series */
					*uarch = cpuinfo_uarch_taishan_v110;
					break;
#endif /* CPUINFO_ARCH_ARM64 */
				case 0xD40: /* Kirin 980 Big/Medium cores ->
					       Cortex-A76 */
					*vendor = cpuinfo_vendor_arm;
					*uarch = cpuinfo_uarch_cortex_a76;
					break;
				default:
					cpuinfo_log_warning(
						"unknown Huawei CPU part 0x%03" PRIx32 " ignored", midr_get_part(midr));
			}
			break;
#if CPUINFO_ARCH_ARM
		case 'i':
			*vendor = cpuinfo_vendor_intel;
			switch (midr_get_part(midr) >> 8) {
				case 2: /* PXA 210/25X/26X */
				case 4: /* PXA 27X */
				case 6: /* PXA 3XX */
					*uarch = cpuinfo_uarch_xscale;
					break;
				default:
					cpuinfo_log_warning(
						"unknown Intel CPU part 0x%03" PRIx32 " ignored", midr_get_part(midr));
			}
			break;
#endif /* CPUINFO_ARCH_ARM */
		case 'N':
			*vendor = cpuinfo_vendor_nvidia;
			switch (midr_get_part(midr)) {
				case 0x000:
					*uarch = cpuinfo_uarch_denver;
					break;
				case 0x003:
					*uarch = cpuinfo_uarch_denver2;
					break;
				case 0x004:
					*uarch = cpuinfo_uarch_carmel;
					break;
				default:
					cpuinfo_log_warning(
						"unknown Nvidia CPU part 0x%03" PRIx32 " ignored", midr_get_part(midr));
			}
			break;
		case 'P':
			*vendor = cpuinfo_vendor_apm;
			switch (midr_get_part(midr)) {
				case 0x000:
					*uarch = cpuinfo_uarch_xgene;
					break;
				default:
					cpuinfo_log_warning(
						"unknown Applied Micro CPU part 0x%03" PRIx32 " ignored",
						midr_get_part(midr));
			}
			break;
		case 'Q':
			*vendor = cpuinfo_vendor_qualcomm;
			switch (midr_get_part(midr)) {
#if CPUINFO_ARCH_ARM
				case 0x00F:
					/* Mostly Scorpions, but some Cortex A5
					 * may report this value as well
					 */
					if (has_vfpv4) {
						/* Unlike Scorpion, Cortex-A5
						 * comes with VFPv4 */
						*vendor = cpuinfo_vendor_arm;
						*uarch = cpuinfo_uarch_cortex_a5;
					} else {
						*uarch = cpuinfo_uarch_scorpion;
					}
					break;
				case 0x02D: /* Dual-core Scorpions */
					*uarch = cpuinfo_uarch_scorpion;
					break;
				case 0x04D:
					/*
					 * Dual-core Krait:
					 * - r1p0 -> Krait 200
					 * - r1p4 -> Krait 200
					 * - r2p0 -> Krait 300
					 */
				case 0x06F:
					/*
					 * Quad-core Krait:
					 * - r0p1 -> Krait 200
					 * - r0p2 -> Krait 200
					 * - r1p0 -> Krait 300
					 * - r2p0 -> Krait 400 (Snapdragon 800
					 * MSMxxxx)
					 * - r2p1 -> Krait 400 (Snapdragon 801
					 * MSMxxxxPRO)
					 * - r3p1 -> Krait 450
					 */
					*uarch = cpuinfo_uarch_krait;
					break;
#endif /* CPUINFO_ARCH_ARM */
				case 0x201: /* Qualcomm Snapdragon 821:
					       Low-power Kryo "Silver" */
				case 0x205: /* Qualcomm Snapdragon 820 & 821:
					       High-performance Kryo "Gold" */
				case 0x211: /* Qualcomm Snapdragon 820:
					       Low-power Kryo "Silver" */
					*uarch = cpuinfo_uarch_kryo;
					break;
				case 0x800: /* High-performance Kryo 260 (r10p2)
					       / Kryo 280 (r10p1) "Gold" ->
					       Cortex-A73 */
					*vendor = cpuinfo_vendor_arm;
					*uarch = cpuinfo_uarch_cortex_a73;
					break;
				case 0x801: /* Low-power Kryo 260 / 280 "Silver"
					       -> Cortex-A53 */
					*vendor = cpuinfo_vendor_arm;
					*uarch = cpuinfo_uarch_cortex_a53;
					break;
				case 0x802: /* High-performance Kryo 385 "Gold"
					       -> Cortex-A75 */
					*vendor = cpuinfo_vendor_arm;
					*uarch = cpuinfo_uarch_cortex_a75;
					break;
				case 0x803: /* Low-power Kryo 385 "Silver" ->
					       Cortex-A55r0 */
					*vendor = cpuinfo_vendor_arm;
					*uarch = cpuinfo_uarch_cortex_a55r0;
					break;
				case 0x804: /* High-performance Kryo 485 "Gold"
					       / "Gold Prime" -> Cortex-A76 */
					*vendor = cpuinfo_vendor_arm;
					*uarch = cpuinfo_uarch_cortex_a76;
					break;
				case 0x805: /* Low-performance Kryo 485 "Silver"
					       -> Cortex-A55 */
					*vendor = cpuinfo_vendor_arm;
					*uarch = cpuinfo_uarch_cortex_a55;
					break;
#if CPUINFO_ARCH_ARM64
				case 0xC00:
					*uarch = cpuinfo_uarch_falkor;
					break;
				case 0xC01:
					*uarch = cpuinfo_uarch_saphira;
					break;
#endif /* CPUINFO_ARCH_ARM64 */
				default:
					cpuinfo_log_warning(
						"unknown Qualcomm CPU part 0x%03" PRIx32 " ignored",
						midr_get_part(midr));
			}
			break;
		case 'S':
			*vendor = cpuinfo_vendor_samsung;
			switch (midr & (CPUINFO_ARM_MIDR_VARIANT_MASK | CPUINFO_ARM_MIDR_PART_MASK)) {
				case 0x00100010:
					/*
					 * Exynos 8890 MIDR = 0x531F0011, assume
					 * Exynos M1 has:
					 * - CPU variant 0x1
					 * - CPU part 0x001
					 */
					*uarch = cpuinfo_uarch_exynos_m1;
					break;
				case 0x00400010:
					/*
					 * Exynos 8895 MIDR = 0x534F0010, assume
					 * Exynos M2 has:
					 * - CPU variant 0x4
					 * - CPU part 0x001
					 */
					*uarch = cpuinfo_uarch_exynos_m2;
					break;
				case 0x00100020:
					/*
					 * Exynos 9810 MIDR = 0x531F0020, assume
					 * Exynos M3 has:
					 * - CPU variant 0x1
					 * - CPU part 0x002
					 */
					*uarch = cpuinfo_uarch_exynos_m3;
					break;
				case 0x00100030:
					/*
					 * Exynos 9820 MIDR = 0x531F0030, assume
					 * Exynos M4 has:
					 * - CPU variant 0x1
					 * - CPU part 0x003
					 */
					*uarch = cpuinfo_uarch_exynos_m4;
					break;
				case 0x00100040:
					/*
					 * Exynos 9820 MIDR = 0x531F0040, assume
					 * Exynos M5 has:
					 * - CPU variant 0x1
					 * - CPU part 0x004
					 */
					*uarch = cpuinfo_uarch_exynos_m5;
					break;
				default:
					cpuinfo_log_warning(
						"unknown Samsung CPU variant 0x%01" PRIx32 " part 0x%03" PRIx32
						" ignored",
						midr_get_variant(midr),
						midr_get_part(midr));
			}
			break;
#if CPUINFO_ARCH_ARM
		case 'V':
			*vendor = cpuinfo_vendor_marvell;
			switch (midr_get_part(midr)) {
				case 0x581: /* PJ4 / PJ4B */
				case 0x584: /* PJ4B-MP / PJ4C */
					*uarch = cpuinfo_uarch_pj4;
					break;
				default:
					cpuinfo_log_warning(
						"unknown Marvell CPU part 0x%03" PRIx32 " ignored",
						midr_get_part(midr));
			}
			break;
#endif /* CPUINFO_ARCH_ARM */
		default:
			cpuinfo_log_warning(
				"unknown CPU implementer '%c' (0x%02" PRIx32 ") with CPU part 0x%03" PRIx32 " ignored",
				(char)midr_get_implementer(midr),
				midr_get_implementer(midr),
				midr_get_part(midr));
	}
}
