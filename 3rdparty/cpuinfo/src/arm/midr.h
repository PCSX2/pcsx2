#pragma once
#include <stdint.h>


#define CPUINFO_ARM_MIDR_IMPLEMENTER_MASK  UINT32_C(0xFF000000)
#define CPUINFO_ARM_MIDR_VARIANT_MASK      UINT32_C(0x00F00000)
#define CPUINFO_ARM_MIDR_ARCHITECTURE_MASK UINT32_C(0x000F0000)
#define CPUINFO_ARM_MIDR_PART_MASK         UINT32_C(0x0000FFF0)
#define CPUINFO_ARM_MIDR_REVISION_MASK     UINT32_C(0x0000000F)

#define CPUINFO_ARM_MIDR_IMPLEMENTER_OFFSET  24
#define CPUINFO_ARM_MIDR_VARIANT_OFFSET      20
#define CPUINFO_ARM_MIDR_ARCHITECTURE_OFFSET 16
#define CPUINFO_ARM_MIDR_PART_OFFSET          4
#define CPUINFO_ARM_MIDR_REVISION_OFFSET      0

#define CPUINFO_ARM_MIDR_ARM1156         UINT32_C(0x410FB560)
#define CPUINFO_ARM_MIDR_CORTEX_A7       UINT32_C(0x410FC070)
#define CPUINFO_ARM_MIDR_CORTEX_A9       UINT32_C(0x410FC090)
#define CPUINFO_ARM_MIDR_CORTEX_A15      UINT32_C(0x410FC0F0)
#define CPUINFO_ARM_MIDR_CORTEX_A17      UINT32_C(0x410FC0E0)
#define CPUINFO_ARM_MIDR_CORTEX_A35      UINT32_C(0x410FD040)
#define CPUINFO_ARM_MIDR_CORTEX_A53      UINT32_C(0x410FD030)
#define CPUINFO_ARM_MIDR_CORTEX_A55      UINT32_C(0x410FD050)
#define CPUINFO_ARM_MIDR_CORTEX_A57      UINT32_C(0x410FD070)
#define CPUINFO_ARM_MIDR_CORTEX_A72      UINT32_C(0x410FD080)
#define CPUINFO_ARM_MIDR_CORTEX_A73      UINT32_C(0x410FD090)
#define CPUINFO_ARM_MIDR_CORTEX_A75      UINT32_C(0x410FD0A0)
#define CPUINFO_ARM_MIDR_KRYO280_GOLD    UINT32_C(0x51AF8001)
#define CPUINFO_ARM_MIDR_KRYO280_SILVER  UINT32_C(0x51AF8014)
#define CPUINFO_ARM_MIDR_KRYO385_GOLD    UINT32_C(0x518F802D)
#define CPUINFO_ARM_MIDR_KRYO385_SILVER  UINT32_C(0x518F803C)
#define CPUINFO_ARM_MIDR_KRYO_SILVER_821 UINT32_C(0x510F2010)
#define CPUINFO_ARM_MIDR_KRYO_GOLD       UINT32_C(0x510F2050)
#define CPUINFO_ARM_MIDR_KRYO_SILVER_820 UINT32_C(0x510F2110)
#define CPUINFO_ARM_MIDR_EXYNOS_M1_M2    UINT32_C(0x530F0010)
#define CPUINFO_ARM_MIDR_DENVER2         UINT32_C(0x4E0F0030)

inline static uint32_t midr_set_implementer(uint32_t midr, uint32_t implementer) {
	return (midr & ~CPUINFO_ARM_MIDR_IMPLEMENTER_MASK) |
		((implementer << CPUINFO_ARM_MIDR_IMPLEMENTER_OFFSET) & CPUINFO_ARM_MIDR_IMPLEMENTER_MASK);
}

inline static uint32_t midr_set_variant(uint32_t midr, uint32_t variant) {
	return (midr & ~CPUINFO_ARM_MIDR_VARIANT_MASK) |
		((variant << CPUINFO_ARM_MIDR_VARIANT_OFFSET) & CPUINFO_ARM_MIDR_VARIANT_MASK);
}

inline static uint32_t midr_set_architecture(uint32_t midr, uint32_t architecture) {
	return (midr & ~CPUINFO_ARM_MIDR_ARCHITECTURE_MASK) |
		((architecture << CPUINFO_ARM_MIDR_ARCHITECTURE_OFFSET) & CPUINFO_ARM_MIDR_ARCHITECTURE_MASK);
}

inline static uint32_t midr_set_part(uint32_t midr, uint32_t part) {
	return (midr & ~CPUINFO_ARM_MIDR_PART_MASK) |
		((part << CPUINFO_ARM_MIDR_PART_OFFSET) & CPUINFO_ARM_MIDR_PART_MASK);
}

inline static uint32_t midr_set_revision(uint32_t midr, uint32_t revision) {
	return (midr & ~CPUINFO_ARM_MIDR_REVISION_MASK) |
		((revision << CPUINFO_ARM_MIDR_REVISION_OFFSET) & CPUINFO_ARM_MIDR_REVISION_MASK);
}

inline static uint32_t midr_get_variant(uint32_t midr) {
	return (midr & CPUINFO_ARM_MIDR_VARIANT_MASK) >> CPUINFO_ARM_MIDR_VARIANT_OFFSET;
}

inline static uint32_t midr_get_implementer(uint32_t midr) {
	return (midr & CPUINFO_ARM_MIDR_IMPLEMENTER_MASK) >> CPUINFO_ARM_MIDR_IMPLEMENTER_OFFSET;
}

inline static uint32_t midr_get_part(uint32_t midr) {
	return (midr & CPUINFO_ARM_MIDR_PART_MASK) >> CPUINFO_ARM_MIDR_PART_OFFSET;
}

inline static uint32_t midr_get_revision(uint32_t midr) {
	return (midr & CPUINFO_ARM_MIDR_REVISION_MASK) >> CPUINFO_ARM_MIDR_REVISION_OFFSET;
}

inline static uint32_t midr_copy_implementer(uint32_t midr, uint32_t other_midr) {
	return (midr & ~CPUINFO_ARM_MIDR_IMPLEMENTER_MASK) | (other_midr & CPUINFO_ARM_MIDR_IMPLEMENTER_MASK);
}

inline static uint32_t midr_copy_variant(uint32_t midr, uint32_t other_midr) {
	return (midr & ~CPUINFO_ARM_MIDR_VARIANT_MASK) | (other_midr & CPUINFO_ARM_MIDR_VARIANT_MASK);
}

inline static uint32_t midr_copy_architecture(uint32_t midr, uint32_t other_midr) {
	return (midr & ~CPUINFO_ARM_MIDR_ARCHITECTURE_MASK) | (other_midr & CPUINFO_ARM_MIDR_ARCHITECTURE_MASK);
}

inline static uint32_t midr_copy_part(uint32_t midr, uint32_t other_midr) {
	return (midr & ~CPUINFO_ARM_MIDR_PART_MASK) | (other_midr & CPUINFO_ARM_MIDR_PART_MASK);
}

inline static uint32_t midr_copy_revision(uint32_t midr, uint32_t other_midr) {
	return (midr & ~CPUINFO_ARM_MIDR_REVISION_MASK) | (other_midr & CPUINFO_ARM_MIDR_REVISION_MASK);
}

inline static bool midr_is_arm1156(uint32_t midr) {
	const uint32_t uarch_mask = CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | CPUINFO_ARM_MIDR_PART_MASK;
	return (midr & uarch_mask) == (CPUINFO_ARM_MIDR_ARM1156 & uarch_mask);
}

inline static bool midr_is_arm11(uint32_t midr) {
	return (midr & (CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | 0x0000F000)) == UINT32_C(0x4100B000);
}

inline static bool midr_is_cortex_a9(uint32_t midr) {
	const uint32_t uarch_mask = CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | CPUINFO_ARM_MIDR_PART_MASK;
	return (midr & uarch_mask) == (CPUINFO_ARM_MIDR_CORTEX_A9 & uarch_mask);
}

inline static bool midr_is_scorpion(uint32_t midr) {
	switch (midr & (CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | CPUINFO_ARM_MIDR_PART_MASK)) {
		case UINT32_C(0x510000F0):
		case UINT32_C(0x510002D0):
			return true;
		default:
			return false;
	}
}

inline static bool midr_is_krait(uint32_t midr) {
	switch (midr & (CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | CPUINFO_ARM_MIDR_PART_MASK)) {
		case UINT32_C(0x510004D0):
		case UINT32_C(0x510006F0):
			return true;
		default:
			return false;
	}
}

inline static bool midr_is_cortex_a53(uint32_t midr) {
	const uint32_t uarch_mask = CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | CPUINFO_ARM_MIDR_PART_MASK;
	return (midr & uarch_mask) == (CPUINFO_ARM_MIDR_CORTEX_A53 & uarch_mask);
}

inline static bool midr_is_qualcomm_cortex_a53_silver(uint32_t midr) {
	const uint32_t uarch_mask = CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | CPUINFO_ARM_MIDR_PART_MASK;
	return (midr & uarch_mask) == (CPUINFO_ARM_MIDR_KRYO280_SILVER & uarch_mask);
}

inline static bool midr_is_qualcomm_cortex_a55_silver(uint32_t midr) {
	const uint32_t uarch_mask = CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | CPUINFO_ARM_MIDR_PART_MASK;
	return (midr & uarch_mask) == (CPUINFO_ARM_MIDR_KRYO385_SILVER & uarch_mask);
}

inline static bool midr_is_kryo280_gold(uint32_t midr) {
	const uint32_t uarch_mask = CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | CPUINFO_ARM_MIDR_PART_MASK;
	return (midr & uarch_mask) == (CPUINFO_ARM_MIDR_KRYO280_GOLD & uarch_mask);
}

inline static bool midr_is_kryo_silver(uint32_t midr) {
	const uint32_t uarch_mask =
		CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | CPUINFO_ARM_MIDR_ARCHITECTURE_MASK | CPUINFO_ARM_MIDR_PART_MASK;
	switch (midr & uarch_mask) {
		case CPUINFO_ARM_MIDR_KRYO_SILVER_820:
		case CPUINFO_ARM_MIDR_KRYO_SILVER_821:
			return true;
		default:
			return false;
	}
}

inline static bool midr_is_kryo_gold(uint32_t midr) {
	const uint32_t uarch_mask = CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | CPUINFO_ARM_MIDR_PART_MASK;
	return (midr & uarch_mask) == (CPUINFO_ARM_MIDR_KRYO_GOLD & uarch_mask);
}

inline static uint32_t midr_score_core(uint32_t midr) {
	const uint32_t core_mask = CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | CPUINFO_ARM_MIDR_PART_MASK;
	switch (midr & core_mask) {
		case UINT32_C(0x53000030): /* Exynos M4 */
		case UINT32_C(0x53000040): /* Exynos M5 */
		case UINT32_C(0x4100D440): /* Cortex-X1 */
			/* These cores are in big role w.r.t Cortex-A75/-A76/-A77/-A78 */
			return 6;
		case UINT32_C(0x4E000030): /* Denver 2 */
		case UINT32_C(0x53000010): /* Exynos M1 and Exynos M2 */
		case UINT32_C(0x53000020): /* Exynos M3 */
		case UINT32_C(0x51008040): /* Kryo 485 Gold / Gold Prime */
		case UINT32_C(0x51008020): /* Kryo 385 Gold */
		case UINT32_C(0x51008000): /* Kryo 260 / 280 Gold */
		case UINT32_C(0x51002050): /* Kryo Gold */
		case UINT32_C(0x4800D400): /* Cortex-A76 (HiSilicon) */
		case UINT32_C(0x4100D410): /* Cortex-A78 */
		case UINT32_C(0x4100D0D0): /* Cortex-A77 */
		case UINT32_C(0x4100D0E0): /* Cortex-A76AE */
		case UINT32_C(0x4100D0B0): /* Cortex-A76 */
		case UINT32_C(0x4100D0A0): /* Cortex-A75 */
		case UINT32_C(0x4100D090): /* Cortex-A73 */
		case UINT32_C(0x4100D080): /* Cortex-A72 */
#if CPUINFO_ARCH_ARM
		case UINT32_C(0x4100C0F0): /* Cortex-A15 */
		case UINT32_C(0x4100C0E0): /* Cortex-A17 */
		case UINT32_C(0x4100C0D0): /* Rockchip RK3288 cores */
		case UINT32_C(0x4100C0C0): /* Cortex-A12 */
#endif /* CPUINFO_ARCH_ARM */
			/* These cores are always in big role */
			return 5;
		case UINT32_C(0x4100D070): /* Cortex-A57 */
			/* Cortex-A57 can be in LITTLE role w.r.t. Denver 2, or in big role w.r.t. Cortex-A53 */
			return 4;
#if CPUINFO_ARCH_ARM64
		case UINT32_C(0x4100D060): /* Cortex-A65 */
#endif /* CPUINFO_ARCH_ARM64 */
		case UINT32_C(0x4100D050): /* Cortex-A55 */
		case UINT32_C(0x4100D030): /* Cortex-A53 */
			/* Cortex-A53 is usually in LITTLE role, but can be in big role w.r.t. Cortex-A35 */
			return 2;
		case UINT32_C(0x4100D040): /* Cortex-A35 */
#if CPUINFO_ARCH_ARM
		case UINT32_C(0x4100C070): /* Cortex-A7 */
#endif /* CPUINFO_ARCH_ARM */
		case UINT32_C(0x51008050): /* Kryo 485 Silver */
		case UINT32_C(0x51008030): /* Kryo 385 Silver */
		case UINT32_C(0x51008010): /* Kryo 260 / 280 Silver */
		case UINT32_C(0x51002110): /* Kryo Silver (Snapdragon 820) */
		case UINT32_C(0x51002010): /* Kryo Silver (Snapdragon 821) */
			/* These cores are always in LITTLE core */
			return 1;
		default:
			/*
			 * Unknown cores, or cores which do not have big/LITTLE roles.
			 * To be future-proof w.r.t. cores not yet recognized in cpuinfo, assume position between
			 * Cortex-A57/A72/A73/A75 and Cortex-A53/A55. Then at least future cores paired with
			 * one of these known cores will be properly scored.
			 */
			return 3;
	}
}

inline static uint32_t midr_little_core_for_big(uint32_t midr) {
	const uint32_t core_mask =
		CPUINFO_ARM_MIDR_IMPLEMENTER_MASK | CPUINFO_ARM_MIDR_ARCHITECTURE_MASK | CPUINFO_ARM_MIDR_PART_MASK;
	switch (midr & core_mask) {
		case CPUINFO_ARM_MIDR_CORTEX_A75:
			return CPUINFO_ARM_MIDR_CORTEX_A55;
		case CPUINFO_ARM_MIDR_CORTEX_A73:
		case CPUINFO_ARM_MIDR_CORTEX_A72:
		case CPUINFO_ARM_MIDR_CORTEX_A57:
		case CPUINFO_ARM_MIDR_EXYNOS_M1_M2:
			return CPUINFO_ARM_MIDR_CORTEX_A53;
		case CPUINFO_ARM_MIDR_CORTEX_A17:
		case CPUINFO_ARM_MIDR_CORTEX_A15:
			return CPUINFO_ARM_MIDR_CORTEX_A7;
		case CPUINFO_ARM_MIDR_KRYO280_GOLD:
			return CPUINFO_ARM_MIDR_KRYO280_SILVER;
		case CPUINFO_ARM_MIDR_KRYO_GOLD:
			return CPUINFO_ARM_MIDR_KRYO_SILVER_820;
		case CPUINFO_ARM_MIDR_DENVER2:
			return CPUINFO_ARM_MIDR_CORTEX_A57;
		default:
			return midr;
	}
}
