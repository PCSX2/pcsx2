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
