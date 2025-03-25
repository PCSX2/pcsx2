#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <cpuinfo.h>
#include <cpuinfo/internal-api.h>
#include <cpuinfo/log.h>

#include <arm/api.h>
#include <arm/midr.h>

#include "windows-arm-init.h"

struct cpuinfo_arm_isa cpuinfo_isa;

static void set_cpuinfo_isa_fields(void);
static struct woa_chip_info* get_system_info_from_registry(void);

static struct woa_chip_info woa_chip_unknown = {L"Unknown", {{cpuinfo_vendor_unknown, cpuinfo_uarch_unknown, 0}}};

BOOL CALLBACK cpuinfo_arm_windows_init(PINIT_ONCE init_once, PVOID parameter, PVOID* context) {
	struct woa_chip_info* chip_info = NULL;
	enum cpuinfo_vendor vendor = cpuinfo_vendor_unknown;

	set_cpuinfo_isa_fields();

	chip_info = get_system_info_from_registry();
	if (chip_info == NULL) {
		chip_info = &woa_chip_unknown;
	}

	cpuinfo_is_initialized = cpu_info_init_by_logical_sys_info(chip_info, chip_info->uarchs[0].vendor);

	return true;
}

/* Static helper functions */

static wchar_t* read_registry(LPCWSTR subkey, LPCWSTR value) {
	DWORD key_type = 0;
	DWORD data_size = 0;
	const DWORD flags = RRF_RT_REG_SZ; /* Only read strings (REG_SZ) */
	wchar_t* text_buffer = NULL;
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

static uint64_t read_registry_qword(LPCWSTR subkey, LPCWSTR value) {
	DWORD key_type = 0;
	DWORD data_size = sizeof(uint64_t);
	const DWORD flags = RRF_RT_REG_QWORD; /* Only read QWORD (REG_QWORD) values */
	uint64_t qword_value = 0;
	LSTATUS result = RegGetValueW(HKEY_LOCAL_MACHINE, subkey, value, flags, &key_type, &qword_value, &data_size);
	if (result != ERROR_SUCCESS || data_size != sizeof(uint64_t)) {
		cpuinfo_log_error("Registry QWORD read error");
		return 0;
	}
	return qword_value;
}

static uint64_t read_registry_dword(LPCWSTR subkey, LPCWSTR value) {
	DWORD key_type = 0;
	DWORD data_size = sizeof(DWORD);
	DWORD dword_value = 0;
	LSTATUS result =
		RegGetValueW(HKEY_LOCAL_MACHINE, subkey, value, RRF_RT_REG_DWORD, &key_type, &dword_value, &data_size);
	if (result != ERROR_SUCCESS || data_size != sizeof(DWORD)) {
		cpuinfo_log_error("Registry DWORD read error");
		return 0;
	}
	return (uint64_t)dword_value;
}

static wchar_t* wcsndup(const wchar_t* src, size_t n) {
	size_t len = wcsnlen(src, n);
	wchar_t* dup = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, (len + 1) * sizeof(wchar_t));
	if (dup) {
		wcsncpy_s(dup, len + 1, src, len);
		dup[len] = L'\0';
	}
	return dup;
}

static struct core_info_by_chip_name get_core_info_from_midr(uint32_t midr, uint64_t frequency) {
	struct core_info_by_chip_name info;
	enum cpuinfo_vendor vendor;
	enum cpuinfo_uarch uarch;

#if CPUINFO_ARCH_ARM
	bool has_vfpv4 = false;
	cpuinfo_arm_decode_vendor_uarch(midr, has_vfpv4, &vendor, &uarch);
#else
	cpuinfo_arm_decode_vendor_uarch(midr, &vendor, &uarch);
#endif

	info.vendor = vendor;
	info.uarch = uarch;
	info.frequency = frequency;
	return info;
}

static struct woa_chip_info* get_system_info_from_registry(void) {
	wchar_t* text_buffer = NULL;
	LPCWSTR cpu0_subkey = L"HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0";
	LPCWSTR chip_name_value = L"ProcessorNameString";
	LPCWSTR chip_midr_value = L"CP 4000";
	LPCWSTR chip_mhz_value = L"~MHz";
	struct woa_chip_info* chip_info = NULL;

	/* Read processor model name from registry and find in the hard-coded
	 * list. */
	text_buffer = read_registry(cpu0_subkey, chip_name_value);
	if (text_buffer == NULL) {
		cpuinfo_log_error("Registry read error for processor name");
		return NULL;
	}

	/*
	 *  https://developer.arm.com/documentation/100442/0100/register-descriptions/aarch32-system-registers/midr--main-id-register
	 *	Regedit for MIDR :
	 *HKEY_LOCAL_MACHINE\HARDWARE\DESCRIPTION\System\CentralProcessor\0\CP 4000
	 */
	uint64_t midr_qword = (uint32_t)read_registry_qword(cpu0_subkey, chip_midr_value);
	if (midr_qword == 0) {
		cpuinfo_log_error("Registry read error for MIDR value");
		return NULL;
	}
	// MIDR is only 32 bits, so we need to cast it to uint32_t
	uint32_t midr_value = (uint32_t)midr_qword;

	/* Read the frequency from the registry
	 * The value is in MHz, so we need to convert it to Hz */
	uint64_t frequency_mhz = read_registry_dword(cpu0_subkey, chip_mhz_value);
	if (frequency_mhz == 0) {
		cpuinfo_log_error("Registry read error for frequency value");
		return NULL;
	}
	// Convert MHz to Hz
	uint64_t frequency_hz = frequency_mhz * 1000000;

	// Allocate chip_info before using it.
	chip_info = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(struct woa_chip_info));
	if (chip_info == NULL) {
		cpuinfo_log_error("Heap allocation error for chip_info");
		return NULL;
	}

	// set chip_info fields
	chip_info->chip_name_string = wcsndup(text_buffer, CPUINFO_PACKAGE_NAME_MAX - 1);
	chip_info->uarchs[0] = get_core_info_from_midr(midr_value, frequency_hz);

	cpuinfo_log_debug("detected chip model name: %ls", chip_info->chip_name_string);

	return chip_info;
}

static void set_cpuinfo_isa_fields(void) {
	cpuinfo_isa.atomics = IsProcessorFeaturePresent(PF_ARM_V81_ATOMIC_INSTRUCTIONS_AVAILABLE) != 0;

	const bool dotprod = IsProcessorFeaturePresent(PF_ARM_V82_DP_INSTRUCTIONS_AVAILABLE) != 0;
	cpuinfo_isa.dot = dotprod;

	SYSTEM_INFO system_info;
	GetSystemInfo(&system_info);
	switch (system_info.wProcessorLevel) {
		case 0x803: // Kryo 385 Silver (Snapdragon 850)
			cpuinfo_isa.fp16arith = dotprod;
			cpuinfo_isa.rdm = dotprod;
			break;
		default:
			// Assume that Dot Product support implies FP16
			// arithmetics and RDM support. ARM manuals don't
			// guarantee that, but it holds in practice.
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