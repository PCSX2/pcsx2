#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <cpuinfo.h>
#include <cpuinfo/common.h>

enum cpuinfo_arm_chipset_vendor {
	cpuinfo_arm_chipset_vendor_unknown = 0,
	cpuinfo_arm_chipset_vendor_qualcomm,
	cpuinfo_arm_chipset_vendor_mediatek,
	cpuinfo_arm_chipset_vendor_samsung,
	cpuinfo_arm_chipset_vendor_hisilicon,
	cpuinfo_arm_chipset_vendor_actions,
	cpuinfo_arm_chipset_vendor_allwinner,
	cpuinfo_arm_chipset_vendor_amlogic,
	cpuinfo_arm_chipset_vendor_broadcom,
	cpuinfo_arm_chipset_vendor_lg,
	cpuinfo_arm_chipset_vendor_leadcore,
	cpuinfo_arm_chipset_vendor_marvell,
	cpuinfo_arm_chipset_vendor_mstar,
	cpuinfo_arm_chipset_vendor_novathor,
	cpuinfo_arm_chipset_vendor_nvidia,
	cpuinfo_arm_chipset_vendor_pinecone,
	cpuinfo_arm_chipset_vendor_renesas,
	cpuinfo_arm_chipset_vendor_rockchip,
	cpuinfo_arm_chipset_vendor_spreadtrum,
	cpuinfo_arm_chipset_vendor_telechips,
	cpuinfo_arm_chipset_vendor_texas_instruments,
	cpuinfo_arm_chipset_vendor_wondermedia,
	cpuinfo_arm_chipset_vendor_max,
};

enum cpuinfo_arm_chipset_series {
	cpuinfo_arm_chipset_series_unknown = 0,
	cpuinfo_arm_chipset_series_qualcomm_qsd,
	cpuinfo_arm_chipset_series_qualcomm_msm,
	cpuinfo_arm_chipset_series_qualcomm_apq,
	cpuinfo_arm_chipset_series_qualcomm_snapdragon,
	cpuinfo_arm_chipset_series_mediatek_mt,
	cpuinfo_arm_chipset_series_samsung_exynos,
	cpuinfo_arm_chipset_series_hisilicon_k3v,
	cpuinfo_arm_chipset_series_hisilicon_hi,
	cpuinfo_arm_chipset_series_hisilicon_kirin,
	cpuinfo_arm_chipset_series_actions_atm,
	cpuinfo_arm_chipset_series_allwinner_a,
	cpuinfo_arm_chipset_series_amlogic_aml,
	cpuinfo_arm_chipset_series_amlogic_s,
	cpuinfo_arm_chipset_series_broadcom_bcm,
	cpuinfo_arm_chipset_series_lg_nuclun,
	cpuinfo_arm_chipset_series_leadcore_lc,
	cpuinfo_arm_chipset_series_marvell_pxa,
	cpuinfo_arm_chipset_series_mstar_6a,
	cpuinfo_arm_chipset_series_novathor_u,
	cpuinfo_arm_chipset_series_nvidia_tegra_t,
	cpuinfo_arm_chipset_series_nvidia_tegra_ap,
	cpuinfo_arm_chipset_series_nvidia_tegra_sl,
	cpuinfo_arm_chipset_series_pinecone_surge_s,
	cpuinfo_arm_chipset_series_renesas_mp,
	cpuinfo_arm_chipset_series_rockchip_rk,
	cpuinfo_arm_chipset_series_spreadtrum_sc,
	cpuinfo_arm_chipset_series_telechips_tcc,
	cpuinfo_arm_chipset_series_texas_instruments_omap,
	cpuinfo_arm_chipset_series_wondermedia_wm,
	cpuinfo_arm_chipset_series_max,
};

#define CPUINFO_ARM_CHIPSET_SUFFIX_MAX 8

struct cpuinfo_arm_chipset {
	enum cpuinfo_arm_chipset_vendor vendor;
	enum cpuinfo_arm_chipset_series series;
	uint32_t model;
	char suffix[CPUINFO_ARM_CHIPSET_SUFFIX_MAX];
};

#define CPUINFO_ARM_CHIPSET_NAME_MAX CPUINFO_PACKAGE_NAME_MAX

#ifndef __cplusplus
	CPUINFO_INTERNAL void cpuinfo_arm_chipset_to_string(
		const struct cpuinfo_arm_chipset chipset[restrict static 1],
		char name[restrict static CPUINFO_ARM_CHIPSET_NAME_MAX]);

	CPUINFO_INTERNAL void cpuinfo_arm_fixup_chipset(
		struct cpuinfo_arm_chipset chipset[restrict static 1], uint32_t cores, uint32_t max_cpu_freq_max);

	CPUINFO_INTERNAL void cpuinfo_arm_decode_vendor_uarch(
		uint32_t midr,
	#if CPUINFO_ARCH_ARM
		bool has_vfpv4,
	#endif
		enum cpuinfo_vendor vendor[restrict static 1],
		enum cpuinfo_uarch uarch[restrict static 1]);

	CPUINFO_INTERNAL void cpuinfo_arm_decode_cache(
		enum cpuinfo_uarch uarch,
		uint32_t cluster_cores,
		uint32_t midr,
		const struct cpuinfo_arm_chipset chipset[restrict static 1],
		uint32_t cluster_id,
		uint32_t arch_version,
		struct cpuinfo_cache l1i[restrict static 1],
		struct cpuinfo_cache l1d[restrict static 1],
		struct cpuinfo_cache l2[restrict static 1],
		struct cpuinfo_cache l3[restrict static 1]);

	CPUINFO_INTERNAL uint32_t cpuinfo_arm_compute_max_cache_size(
		const struct cpuinfo_processor processor[restrict static 1]);
#else /* defined(__cplusplus) */
	CPUINFO_INTERNAL void cpuinfo_arm_decode_cache(
		enum cpuinfo_uarch uarch,
		uint32_t cluster_cores,
		uint32_t midr,
		const struct cpuinfo_arm_chipset chipset[1],
		uint32_t cluster_id,
		uint32_t arch_version,
		struct cpuinfo_cache l1i[1],
		struct cpuinfo_cache l1d[1],
		struct cpuinfo_cache l2[1],
		struct cpuinfo_cache l3[1]);
#endif
