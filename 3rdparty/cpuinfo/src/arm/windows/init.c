#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <errno.h>

#include <cpuinfo.h>
#include <cpuinfo/internal-api.h>
#include <cpuinfo/log.h>

#include "windows-arm-init.h"

struct cpuinfo_arm_isa cpuinfo_isa;

static void set_cpuinfo_isa_fields(void);
static struct woa_chip_info* get_system_info_from_registry(void);

static struct woa_chip_info woa_chip_unknown = {
	L"Unknown",
	woa_chip_name_unknown,
	{
		{
			cpuinfo_vendor_unknown,
			cpuinfo_uarch_unknown,
			0
		}
	}
};

/* Please add new SoC/chip info here! */
static struct woa_chip_info woa_chips[] = {
	/* Microsoft SQ1 Kryo 495 4 + 4 cores (3 GHz + 1.80 GHz) */
	{
		L"Microsoft SQ1",
		woa_chip_name_microsoft_sq_1,
		{
			{
				cpuinfo_vendor_arm,
				cpuinfo_uarch_cortex_a55,
				1800000000,
			},
			{
				cpuinfo_vendor_arm,
				cpuinfo_uarch_cortex_a76,
				3000000000,
			}
		}
	},
	/* Microsoft SQ2 Kryo 495 4 + 4 cores (3.15 GHz + 2.42 GHz) */
	{
		L"Microsoft SQ2",
		woa_chip_name_microsoft_sq_2,
		{
			{
				cpuinfo_vendor_arm,
				cpuinfo_uarch_cortex_a55,
				2420000000,
			},
			{
				cpuinfo_vendor_arm,
				cpuinfo_uarch_cortex_a76,
				3150000000
			}
		}
	},
	/* Microsoft Windows Dev Kit 2023 */
	{
		L"Snapdragon Compute Platform",
		woa_chip_name_microsoft_sq_3,
		{
			{
				cpuinfo_vendor_arm,
				cpuinfo_uarch_cortex_a78,
				2420000000,
			},
			{
				cpuinfo_vendor_arm,
				cpuinfo_uarch_cortex_x1,
				3000000000
			}
		}
	},
	/* Ampere Altra */
	{
		L"Ampere(R) Altra(R) Processor",
		woa_chip_name_ampere_altra,
		{
			{
				cpuinfo_vendor_arm,
				cpuinfo_uarch_neoverse_n1,
				3000000000
			}
		}
	}
};

BOOL CALLBACK cpuinfo_arm_windows_init(
	PINIT_ONCE init_once, PVOID parameter, PVOID* context)
{
	struct woa_chip_info *chip_info = NULL;
	enum cpuinfo_vendor vendor = cpuinfo_vendor_unknown;
	
	set_cpuinfo_isa_fields();

	chip_info = get_system_info_from_registry();
	if (chip_info == NULL) {
		chip_info = &woa_chip_unknown;
	}

	cpuinfo_is_initialized = cpu_info_init_by_logical_sys_info(chip_info, chip_info->uarchs[0].vendor);

	return true;
}

bool get_core_uarch_for_efficiency(
	enum woa_chip_name chip, BYTE EfficiencyClass,
	enum cpuinfo_uarch* uarch, uint64_t* frequency)
{
	/* For currently supported WoA chips, the Efficiency class selects
	 * the pre-defined little and big core.
	 * Any further supported SoC's logic should be implemented here.
	 */
	if (uarch && frequency && chip < woa_chip_name_last &&
		EfficiencyClass < MAX_WOA_VALID_EFFICIENCY_CLASSES) {
		*uarch = woa_chips[chip].uarchs[EfficiencyClass].uarch;
		*frequency = woa_chips[chip].uarchs[EfficiencyClass].frequency;
		return true;
	}
	return false;
}

/* Static helper functions */

static wchar_t* read_registry(
	LPCWSTR subkey,
	LPCWSTR value)
{
	DWORD key_type = 0;
	DWORD data_size = 0;
	const DWORD flags = RRF_RT_REG_SZ; /* Only read strings (REG_SZ) */
	wchar_t *text_buffer = NULL;
	LSTATUS result = 0;
	HANDLE heap = GetProcessHeap();

	result = RegGetValueW(
		HKEY_LOCAL_MACHINE,
		subkey,
		value,
		flags,
		&key_type,
		NULL, /* Request buffer size */
		&data_size);
	if (result != 0 || data_size == 0) {
		cpuinfo_log_error("Registry entry size read error");
		return NULL;
	}

	text_buffer = HeapAlloc(heap, HEAP_ZERO_MEMORY, data_size);
	if (text_buffer == NULL) {
		cpuinfo_log_error("Registry textbuffer allocation error");
		return NULL;
	}

	result = RegGetValueW(
		HKEY_LOCAL_MACHINE,
		subkey,
		value,
		flags,
		NULL,
		text_buffer, /* Write string in this destination buffer */
		&data_size);
	if (result != 0) {
		cpuinfo_log_error("Registry read error");
		HeapFree(heap, 0, text_buffer);
		return NULL;
	}
	return text_buffer;
}

static struct woa_chip_info* get_system_info_from_registry(void)
{
	wchar_t* text_buffer = NULL;
	LPCWSTR cpu0_subkey = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
	LPCWSTR chip_name_value = L"ProcessorNameString";
	struct woa_chip_info* chip_info = NULL;

	HANDLE heap = GetProcessHeap();

	/* Read processor model name from registry and find in the hard-coded list. */
	text_buffer = read_registry(cpu0_subkey, chip_name_value);
	if (text_buffer == NULL) {
		cpuinfo_log_error("Registry read error");
		return NULL;
	}
	for (uint32_t i = 0; i < (uint32_t) woa_chip_name_last; i++) {
		size_t compare_length = wcsnlen(woa_chips[i].chip_name_string, CPUINFO_PACKAGE_NAME_MAX);
		int compare_result = wcsncmp(text_buffer, woa_chips[i].chip_name_string, compare_length);
		if (compare_result == 0) {
			chip_info = woa_chips+i;
			break;
		}
	}
	if (chip_info == NULL) {
		/* No match was found, so print a warning and assign the unknown case. */
		cpuinfo_log_error("Unknown chip model name '%ls'.\nPlease add new Windows on Arm SoC/chip support to arm/windows/init.c!", text_buffer);
	} else {
		cpuinfo_log_debug("detected chip model name: %s", chip_info->chip_name_string);
	}

	HeapFree(heap, 0, text_buffer);
	return chip_info;
}

static void set_cpuinfo_isa_fields(void)
{
	cpuinfo_isa.atomics = IsProcessorFeaturePresent(PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE) != 0;

	const bool dotprod = IsProcessorFeaturePresent(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE) != 0;
	cpuinfo_isa.dot = dotprod;

	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	switch (system_info.wProcessorLevel) {
		case 0x803:  // Kryo 385 Silver (Snapdragon 850)
			cpuinfo_isa.fp16arith = dotprod;
			cpuinfo_isa.rdm = dotprod;
			break;
		default:
			// Assume that Dot Product support implies FP16 arithmetics and RDM support.
			// ARM manuals don't guarantee that, but it holds in practice.
			cpuinfo_isa.fp16arith = dotprod;
			cpuinfo_isa.rdm = dotprod;
			break;
	}

	/* Windows API reports all or nothing for cryptographic instructions. */
	const bool crypto = IsProcessorFeaturePresent(PF_ARM_V8_CRYPTO_INSTRUCTIONS_AVAILABLE) != 0;
	cpuinfo_isa.aes = crypto;
	cpuinfo_isa.sha1 = crypto;
	cpuinfo_isa.sha2 = crypto;
	cpuinfo_isa.pmull = crypto;

	cpuinfo_isa.crc32 = IsProcessorFeaturePresent(PF_ARM_V8_CRC32_INSTRUCTIONS_AVAILABLE) != 0;
}
