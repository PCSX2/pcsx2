#pragma once

/* Efficiency class = 0 means little core, while 1 means big core for now. */
#define MAX_WOA_VALID_EFFICIENCY_CLASSES 2

/* List of known and supported Windows on Arm SoCs/chips. */
enum woa_chip_name {
	woa_chip_name_microsoft_sq_1 = 0,
	woa_chip_name_microsoft_sq_2 = 1,
	woa_chip_name_microsoft_sq_3 = 2,
	woa_chip_name_microsoft_sq_3_devkit = 3,
	woa_chip_name_ampere_altra = 4,
	woa_chip_name_unknown = 5,
	woa_chip_name_last = woa_chip_name_unknown
};

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
	enum woa_chip_name chip_name;
	struct core_info_by_chip_name uarchs[MAX_WOA_VALID_EFFICIENCY_CLASSES];
};

bool get_core_uarch_for_efficiency(
	enum woa_chip_name chip,
	BYTE EfficiencyClass,
	enum cpuinfo_uarch* uarch,
	uint64_t* frequency);

bool cpu_info_init_by_logical_sys_info(const struct woa_chip_info* chip_info, enum cpuinfo_vendor vendor);
