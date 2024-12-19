#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <sys/system_properties.h>

#include <arm/android/api.h>
#include <arm/linux/api.h>
#include <cpuinfo/log.h>
#include <linux/api.h>

#if CPUINFO_MOCK
#include <cpuinfo-mock.h>

static struct cpuinfo_mock_property* cpuinfo_mock_properties = NULL;

void CPUINFO_ABI cpuinfo_mock_android_properties(struct cpuinfo_mock_property* properties) {
	cpuinfo_log_info("Android properties mocking enabled");
	cpuinfo_mock_properties = properties;
}

static int cpuinfo_android_property_get(const char* key, char* value) {
	if (cpuinfo_mock_properties != NULL) {
		for (const struct cpuinfo_mock_property* prop = cpuinfo_mock_properties; prop->key != NULL; prop++) {
			if (strncmp(key, prop->key, CPUINFO_BUILD_PROP_NAME_MAX) == 0) {
				strncpy(value, prop->value, CPUINFO_BUILD_PROP_VALUE_MAX);
				return (int)strnlen(prop->value, CPUINFO_BUILD_PROP_VALUE_MAX);
			}
		}
	}
	*value = '\0';
	return 0;
}
#else
static inline int cpuinfo_android_property_get(const char* key, char* value) {
	return __system_property_get(key, value);
}
#endif

void cpuinfo_arm_android_parse_properties(struct cpuinfo_android_properties properties[restrict static 1]) {
	const int ro_product_board_length =
		cpuinfo_android_property_get("ro.product.board", properties->ro_product_board);
	cpuinfo_log_debug("read ro.product.board = \"%.*s\"", ro_product_board_length, properties->ro_product_board);

	const int ro_board_platform_length =
		cpuinfo_android_property_get("ro.board.platform", properties->ro_board_platform);
	cpuinfo_log_debug("read ro.board.platform = \"%.*s\"", ro_board_platform_length, properties->ro_board_platform);

	const int ro_mediatek_platform_length =
		cpuinfo_android_property_get("ro.mediatek.platform", properties->ro_mediatek_platform);
	cpuinfo_log_debug(
		"read ro.mediatek.platform = \"%.*s\"", ro_mediatek_platform_length, properties->ro_mediatek_platform);

	const int ro_arch_length = cpuinfo_android_property_get("ro.arch", properties->ro_arch);
	cpuinfo_log_debug("read ro.arch = \"%.*s\"", ro_arch_length, properties->ro_arch);

	const int ro_chipname_length = cpuinfo_android_property_get("ro.chipname", properties->ro_chipname);
	cpuinfo_log_debug("read ro.chipname = \"%.*s\"", ro_chipname_length, properties->ro_chipname);

	const int ro_hardware_chipname_length =
		cpuinfo_android_property_get("ro.hardware.chipname", properties->ro_hardware_chipname);
	cpuinfo_log_debug(
		"read ro.hardware.chipname = \"%.*s\"", ro_hardware_chipname_length, properties->ro_hardware_chipname);
}
