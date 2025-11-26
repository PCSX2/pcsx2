#pragma once

/* Efficiency class = 0 means little core, while 1 means big core for now. */
#define MAX_WOA_VALID_EFFICIENCY_CLASSES 2

/* Topology information hard-coded by SoC/chip name */
struct core_info_by_chip_name {
	enum cpuinfo_vendor vendor;
	enum cpuinfo_uarch uarch;
	uint64_t frequency; /* Hz */
};

/* SoC/chip info that's currently not readable by logical system information,
 * but can be read from registry.
 */
struct woa_chip_info {
	wchar_t* chip_name_string;
	struct core_info_by_chip_name uarchs[MAX_WOA_VALID_EFFICIENCY_CLASSES];
};

bool cpu_info_init_by_logical_sys_info(const struct woa_chip_info* chip_info, enum cpuinfo_vendor vendor);

#ifndef PF_ARM_FMAC_INSTRUCTIONS_AVAILABLE
#define PF_ARM_FMAC_INSTRUCTIONS_AVAILABLE (27)
#endif

#ifndef PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE
#define PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE (34)
#endif

#ifndef PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE
#define PF_ARM_V83_JSCVT_INSTRUCTIONS_AVAILABLE (44)
#endif

#ifndef PF_ARM_SVE_INSTRUCTIONS_AVAILABLE
#define PF_ARM_SVE_INSTRUCTIONS_AVAILABLE (46)
#endif

#ifndef PF_ARM_SVE2_INSTRUCTIONS_AVAILABLE
#define PF_ARM_SVE2_INSTRUCTIONS_AVAILABLE (47)
#endif

#ifndef PF_ARM_SME_BI32I32_INSTRUCTIONS_AVAILABLE
#define PF_ARM_SME_BI32I32_INSTRUCTIONS_AVAILABLE (55)
#endif

#ifndef PF_ARM_V82_I8MM_INSTRUCTIONS_AVAILABLE
#define PF_ARM_V82_I8MM_INSTRUCTIONS_AVAILABLE (66)
#endif

#ifndef PF_ARM_V86_BF16_INSTRUCTIONS_AVAILABLE
#define PF_ARM_V86_BF16_INSTRUCTIONS_AVAILABLE (68)
#endif

#ifndef PF_ARM_SME_INSTRUCTIONS_AVAILABLE
#define PF_ARM_SME_INSTRUCTIONS_AVAILABLE (70)
#endif

#ifndef PF_ARM_SME2_INSTRUCTIONS_AVAILABLE
#define PF_ARM_SME2_INSTRUCTIONS_AVAILABLE (71)
#endif

#ifndef PF_ARM_SME2_1_INSTRUCTIONS_AVAILABLE
#define PF_ARM_SME2_1_INSTRUCTIONS_AVAILABLE (72)
#endif

#ifndef PF_ARM_SME2_2_INSTRUCTIONS_AVAILABLE
#define PF_ARM_SME2_2_INSTRUCTIONS_AVAILABLE (73)
#endif

#ifndef PF_ARM_SME_F16F16_INSTRUCTIONS_AVAILABLE
#define PF_ARM_SME_F16F16_INSTRUCTIONS_AVAILABLE (83)
#endif

#ifndef PF_ARM_SME_B16B16_INSTRUCTIONS_AVAILABLE
#define PF_ARM_SME_B16B16_INSTRUCTIONS_AVAILABLE (84)
#endif

#ifndef PF_ARM_V82_FP16_INSTRUCTIONS_AVAILABLE
#define PF_ARM_V82_FP16_INSTRUCTIONS_AVAILABLE (67)
#endif
