#pragma once

#include <arm/api.h>
#include <arm/linux/api.h>
#include <cpuinfo.h>
#include <cpuinfo/common.h>

enum cpuinfo_android_chipset_property {
	cpuinfo_android_chipset_property_proc_cpuinfo_hardware = 0,
	cpuinfo_android_chipset_property_ro_product_board,
	cpuinfo_android_chipset_property_ro_board_platform,
	cpuinfo_android_chipset_property_ro_mediatek_platform,
	cpuinfo_android_chipset_property_ro_arch,
	cpuinfo_android_chipset_property_ro_chipname,
	cpuinfo_android_chipset_property_ro_hardware_chipname,
	cpuinfo_android_chipset_property_max,
};

CPUINFO_INTERNAL void cpuinfo_arm_android_parse_properties(
	struct cpuinfo_android_properties properties[restrict static 1]);
