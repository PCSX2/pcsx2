#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <arm/linux/api.h>
#ifdef __ANDROID__
#include <arm/android/api.h>
#endif
#include <cpuinfo/common.h>
#include <cpuinfo/log.h>

static inline bool is_ascii_whitespace(char c) {
	switch (c) {
		case ' ':
		case '\t':
		case '\r':
		case '\n':
			return true;
		default:
			return false;
	}
}

static inline bool is_ascii_alphabetic(char c) {
	const char lower_c = c | '\x20';
	return (uint8_t)(lower_c - 'a') <= (uint8_t)('z' - 'a');
}

static inline bool is_ascii_alphabetic_uppercase(char c) {
	return (uint8_t)(c - 'A') <= (uint8_t)('Z' - 'A');
}

static inline bool is_ascii_numeric(char c) {
	return (uint8_t)(c - '0') < 10;
}

static inline uint16_t load_u16le(const void* ptr) {
	const uint8_t* byte_ptr = (const uint8_t*)ptr;
	return ((uint16_t)byte_ptr[1] << 8) | (uint16_t)byte_ptr[0];
}

static inline uint32_t load_u24le(const void* ptr) {
	return ((uint32_t)((const uint8_t*)ptr)[2] << 16) | (uint32_t)load_u16le(ptr);
}

static inline uint32_t load_u32le(const void* ptr) {
	const uint8_t* byte_ptr = (const uint8_t*)ptr;
	return ((uint32_t)byte_ptr[3] << 24) | ((uint32_t)byte_ptr[2] << 16) | ((uint32_t)byte_ptr[1] << 8) |
		(uint32_t)byte_ptr[0];
}

/*
 * Map from ARM chipset series ID to ARM chipset vendor ID.
 * This map is used to avoid storing vendor IDs in tables.
 */
static enum cpuinfo_arm_chipset_vendor chipset_series_vendor[cpuinfo_arm_chipset_series_max] = {
	[cpuinfo_arm_chipset_series_unknown] = cpuinfo_arm_chipset_vendor_unknown,
	[cpuinfo_arm_chipset_series_qualcomm_qsd] = cpuinfo_arm_chipset_vendor_qualcomm,
	[cpuinfo_arm_chipset_series_qualcomm_msm] = cpuinfo_arm_chipset_vendor_qualcomm,
	[cpuinfo_arm_chipset_series_qualcomm_apq] = cpuinfo_arm_chipset_vendor_qualcomm,
	[cpuinfo_arm_chipset_series_qualcomm_snapdragon] = cpuinfo_arm_chipset_vendor_qualcomm,
	[cpuinfo_arm_chipset_series_mediatek_mt] = cpuinfo_arm_chipset_vendor_mediatek,
	[cpuinfo_arm_chipset_series_samsung_exynos] = cpuinfo_arm_chipset_vendor_samsung,
	[cpuinfo_arm_chipset_series_hisilicon_k3v] = cpuinfo_arm_chipset_vendor_hisilicon,
	[cpuinfo_arm_chipset_series_hisilicon_hi] = cpuinfo_arm_chipset_vendor_hisilicon,
	[cpuinfo_arm_chipset_series_hisilicon_kirin] = cpuinfo_arm_chipset_vendor_hisilicon,
	[cpuinfo_arm_chipset_series_actions_atm] = cpuinfo_arm_chipset_vendor_actions,
	[cpuinfo_arm_chipset_series_allwinner_a] = cpuinfo_arm_chipset_vendor_allwinner,
	[cpuinfo_arm_chipset_series_amlogic_aml] = cpuinfo_arm_chipset_vendor_amlogic,
	[cpuinfo_arm_chipset_series_amlogic_s] = cpuinfo_arm_chipset_vendor_amlogic,
	[cpuinfo_arm_chipset_series_broadcom_bcm] = cpuinfo_arm_chipset_vendor_broadcom,
	[cpuinfo_arm_chipset_series_lg_nuclun] = cpuinfo_arm_chipset_vendor_lg,
	[cpuinfo_arm_chipset_series_leadcore_lc] = cpuinfo_arm_chipset_vendor_leadcore,
	[cpuinfo_arm_chipset_series_marvell_pxa] = cpuinfo_arm_chipset_vendor_marvell,
	[cpuinfo_arm_chipset_series_mstar_6a] = cpuinfo_arm_chipset_vendor_mstar,
	[cpuinfo_arm_chipset_series_novathor_u] = cpuinfo_arm_chipset_vendor_novathor,
	[cpuinfo_arm_chipset_series_nvidia_tegra_t] = cpuinfo_arm_chipset_vendor_nvidia,
	[cpuinfo_arm_chipset_series_nvidia_tegra_ap] = cpuinfo_arm_chipset_vendor_nvidia,
	[cpuinfo_arm_chipset_series_nvidia_tegra_sl] = cpuinfo_arm_chipset_vendor_nvidia,
	[cpuinfo_arm_chipset_series_pinecone_surge_s] = cpuinfo_arm_chipset_vendor_pinecone,
	[cpuinfo_arm_chipset_series_renesas_mp] = cpuinfo_arm_chipset_vendor_renesas,
	[cpuinfo_arm_chipset_series_rockchip_rk] = cpuinfo_arm_chipset_vendor_rockchip,
	[cpuinfo_arm_chipset_series_spreadtrum_sc] = cpuinfo_arm_chipset_vendor_spreadtrum,
	[cpuinfo_arm_chipset_series_telechips_tcc] = cpuinfo_arm_chipset_vendor_telechips,
	[cpuinfo_arm_chipset_series_texas_instruments_omap] = cpuinfo_arm_chipset_vendor_texas_instruments,
	[cpuinfo_arm_chipset_series_unisoc_t] = cpuinfo_arm_chipset_vendor_unisoc,
	[cpuinfo_arm_chipset_series_wondermedia_wm] = cpuinfo_arm_chipset_vendor_wondermedia,
};

/**
 * Tries to match /(MSM|APQ)\d{4}([A-Z\-]*)/ signature (case-insensitive) for
 * Qualcomm MSM and APQ chipsets. If match successful, extracts model
 * information into \p chipset argument.
 *
 * @param start - start of the platform identifier (/proc/cpuinfo Hardware
 * string, ro.product.board, ro.board.platform or ro.chipname) to match.
 * @param end - end of the platform identifier (/proc/cpuinfo Hardware string,
 * ro.product.board, ro.board.platform or ro.chipname) to match.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_msm_apq(const char* start, const char* end, struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect at least 7 symbols: 3 symbols "MSM" or "APQ" + 4 digits */
	if (start + 7 > end) {
		return false;
	}

	/* Check that string starts with "MSM" or "APQ", case-insensitive.
	 * The first three characters are loaded as 24-bit little endian word,
	 * binary ORed with 0x20 to convert to lower case, and compared to "MSM"
	 * and "APQ" strings as integers.
	 */
	const uint32_t series_signature = UINT32_C(0x00202020) | load_u24le(start);
	enum cpuinfo_arm_chipset_series series;
	switch (series_signature) {
		case UINT32_C(0x6D736D): /* "msm" = reverse("msm") */
			series = cpuinfo_arm_chipset_series_qualcomm_msm;
			break;
		case UINT32_C(0x717061): /* "qpa" = reverse("apq") */
			series = cpuinfo_arm_chipset_series_qualcomm_apq;
			break;
		default:
			return false;
	}

	/* Sometimes there is a space ' ' following the MSM/APQ series */
	const char* pos = start + 3;
	if (*pos == ' ') {
		pos++;

		/* Expect at least 4 more symbols (4-digit model number) */
		if (pos + 4 > end) {
			return false;
		}
	}

	/* Validate and parse 4-digit model number */
	uint32_t model = 0;
	for (uint32_t i = 0; i < 4; i++) {
		const uint32_t digit = (uint32_t)(uint8_t)(*pos++) - '0';
		if (digit >= 10) {
			/* Not really a digit */
			return false;
		}
		model = model * 10 + digit;
	}

	/* Suffix is optional, so if we got to this point, parsing is
	 * successful. Commit parsed chipset. */
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_qualcomm,
		.series = series,
		.model = model,
	};

	/* Parse as many suffix characters as match the pattern [A-Za-z\-] */
	for (uint32_t i = 0; i < CPUINFO_ARM_CHIPSET_SUFFIX_MAX; i++) {
		if (pos + i == end) {
			break;
		}

		const char c = pos[i];
		if (is_ascii_alphabetic(c)) {
			/* Matched a letter [A-Za-z] */
			chipset->suffix[i] = c & '\xDF';
		} else if (c == '-') {
			/* Matched a dash '-' */
			chipset->suffix[i] = c;
		} else {
			/* Neither of [A-Za-z\-] */
			break;
		}
	}
	return true;
}

/**
 * Tries to match /SDM\d{3}$/ signature for Qualcomm Snapdragon chipsets.
 * If match successful, extracts model information into \p chipset argument.
 *
 * @param start - start of the /proc/cpuinfo Hardware string to match.
 * @param end - end of the /proc/cpuinfo Hardware string to match.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_sdm(const char* start, const char* end, struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect exactly 6 symbols: 3 symbols "SDM" + 3 digits */
	if (start + 6 != end) {
		return false;
	}

	/* Check that string starts with "SDM".
	 * The first three characters are loaded and compared as 24-bit little
	 * endian word.
	 */
	const uint32_t expected_sdm = load_u24le(start);
	if (expected_sdm != UINT32_C(0x004D4453) /* "MDS" = reverse("SDM") */) {
		return false;
	}

	/* Validate and parse 3-digit model number */
	uint32_t model = 0;
	for (uint32_t i = 3; i < 6; i++) {
		const uint32_t digit = (uint32_t)(uint8_t)start[i] - '0';
		if (digit >= 10) {
			/* Not really a digit */
			return false;
		}
		model = model * 10 + digit;
	}

	/* Return parsed chipset. */
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_qualcomm,
		.series = cpuinfo_arm_chipset_series_qualcomm_snapdragon,
		.model = model,
	};
	return true;
}

/**
 * Tries to match /SM\d{4}$/ signature for Qualcomm Snapdragon chipsets.
 * If match successful, extracts model information into \p chipset argument.
 *
 * @param start - start of the /proc/cpuinfo Hardware string to match.
 * @param end - end of the /proc/cpuinfo Hardware string to match.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_sm(const char* start, const char* end, struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect exactly 6 symbols: 2 symbols "SM" + 4 digits */
	if (start + 6 != end) {
		return false;
	}

	/* Check that string starts with "SM".
	 * The first three characters are loaded and compared as 16-bit little
	 * endian word.
	 */
	const uint32_t expected_sm = load_u16le(start);
	if (expected_sm != UINT16_C(0x4D53) /* "MS" = reverse("SM") */) {
		return false;
	}

	/* Validate and parse 4-digit model number */
	uint32_t model = 0;
	for (uint32_t i = 2; i < 6; i++) {
		const uint32_t digit = (uint32_t)(uint8_t)start[i] - '0';
		if (digit >= 10) {
			/* Not really a digit */
			return false;
		}
		model = model * 10 + digit;
	}

	/* Return parsed chipset. */
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_qualcomm,
		.series = cpuinfo_arm_chipset_series_qualcomm_snapdragon,
		.model = model,
	};
	return true;
}

/**
 * Tries to match /Samsung Exynos\d{4}$/ signature (case-insensitive) for
 * Samsung Exynos chipsets. If match successful, extracts model information into
 * \p chipset argument.
 *
 * @param start - start of the /proc/cpuinfo Hardware string to match.
 * @param end - end of the /proc/cpuinfo Hardware string to match.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_samsung_exynos(
	const char* start,
	const char* end,
	struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/*
	 * Expect at 18-19 symbols:
	 * - "Samsung" (7 symbols) + space + "Exynos" (6 symbols) + optional
	 * space 4-digit model number
	 */
	const size_t length = end - start;
	switch (length) {
		case 18:
		case 19:
			break;
		default:
			return false;
	}

	/*
	 * Check that the string starts with "samsung exynos", case-insensitive.
	 * Blocks of 4 characters are loaded and compared as little-endian
	 * 32-bit word. Case-insensitive characters are binary ORed with 0x20 to
	 * convert them to lowercase.
	 */
	const uint32_t expected_sams = UINT32_C(0x20202000) | load_u32le(start);
	if (expected_sams != UINT32_C(0x736D6153) /* "smaS" = reverse("Sams") */) {
		return false;
	}
	const uint32_t expected_ung = UINT32_C(0x00202020) | load_u32le(start + 4);
	if (expected_ung != UINT32_C(0x20676E75) /* " ung" = reverse("ung ") */) {
		return false;
	}
	const uint32_t expected_exyn = UINT32_C(0x20202000) | load_u32le(start + 8);
	if (expected_exyn != UINT32_C(0x6E797845) /* "nyxE" = reverse("Exyn") */) {
		return false;
	}
	const uint16_t expected_os = UINT16_C(0x2020) | load_u16le(start + 12);
	if (expected_os != UINT16_C(0x736F) /* "so" = reverse("os") */) {
		return false;
	}

	const char* pos = start + 14;

	/* There can be a space ' ' following the "Exynos" string */
	if (*pos == ' ') {
		pos++;

		/* If optional space if present, we expect exactly 19 characters
		 */
		if (length != 19) {
			return false;
		}
	}

	/* Validate and parse 4-digit model number */
	uint32_t model = 0;
	for (uint32_t i = 0; i < 4; i++) {
		const uint32_t digit = (uint32_t)(uint8_t)(*pos++) - '0';
		if (digit >= 10) {
			/* Not really a digit */
			return false;
		}
		model = model * 10 + digit;
	}

	/* Return parsed chipset */
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_samsung,
		.series = cpuinfo_arm_chipset_series_samsung_exynos,
		.model = model,
	};
	return true;
}

/**
 * Tries to match /exynos\d{4}$/ signature for Samsung Exynos chipsets.
 * If match successful, extracts model information into \p chipset argument.
 *
 * @param start - start of the platform identifier (ro.board.platform or
 * ro.chipname) to match.
 * @param end - end of the platform identifier (ro.board.platform or
 * ro.chipname) to match.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_exynos(const char* start, const char* end, struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect exactly 10 symbols: "exynos" (6 symbols) + 4-digit model
	 * number */
	if (start + 10 != end) {
		return false;
	}

	/* Load first 4 bytes as little endian 32-bit word */
	const uint32_t expected_exyn = load_u32le(start);
	if (expected_exyn != UINT32_C(0x6E797865) /* "nyxe" = reverse("exyn") */) {
		return false;
	}

	/* Load next 2 bytes as little endian 16-bit word */
	const uint16_t expected_os = load_u16le(start + 4);
	if (expected_os != UINT16_C(0x736F) /* "so" = reverse("os") */) {
		return false;
	}

	/* Check and parse 4-digit model number */
	uint32_t model = 0;
	for (uint32_t i = 6; i < 10; i++) {
		const uint32_t digit = (uint32_t)(uint8_t)start[i] - '0';
		if (digit >= 10) {
			/* Not really a digit */
			return false;
		}
		model = model * 10 + digit;
	}

	/* Return parsed chipset. */
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_samsung,
		.series = cpuinfo_arm_chipset_series_samsung_exynos,
		.model = model,
	};
	return true;
}

/**
 * Tries to match /universal\d{4}$/ signature for Samsung Exynos chipsets.
 * If match successful, extracts model information into \p chipset argument.
 *
 * @param start - start of the platform identifier (/proc/cpuinfo Hardware
 * string, ro.product.board or ro.chipname) to match.
 * @param end - end of the platform identifier (/proc/cpuinfo Hardware string,
 * ro.product.board or ro.chipname) to match.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_universal(const char* start, const char* end, struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect exactly 13 symbols: "universal" (9 symbols) + 4-digit model
	 * number
	 */
	if (start + 13 != end) {
		return false;
	}

	/*
	 * Check that the string starts with "universal".
	 * Blocks of 4 characters are loaded and compared as little-endian
	 * 32-bit word. Case-insensitive characters are binary ORed with 0x20 to
	 * convert them to lowercase.
	 */
	const uint8_t expected_u = UINT8_C(0x20) | (uint8_t)start[0];
	if (expected_u != UINT8_C(0x75) /* "u" */) {
		return false;
	}
	const uint32_t expected_nive = UINT32_C(0x20202020) | load_u32le(start + 1);
	if (expected_nive != UINT32_C(0x6576696E) /* "evin" = reverse("nive") */) {
		return false;
	}
	const uint32_t expected_ersa = UINT32_C(0x20202020) | load_u32le(start + 5);
	if (expected_ersa != UINT32_C(0x6C617372) /* "lasr" = reverse("rsal") */) {
		return false;
	}

	/* Validate and parse 4-digit model number */
	uint32_t model = 0;
	for (uint32_t i = 9; i < 13; i++) {
		const uint32_t digit = (uint32_t)(uint8_t)start[i] - '0';
		if (digit >= 10) {
			/* Not really a digit */
			return false;
		}
		model = model * 10 + digit;
	}

	/* Return parsed chipset. */
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_samsung,
		.series = cpuinfo_arm_chipset_series_samsung_exynos,
		.model = model,
	};
	return true;
}

/**
 * Compares, case insensitively, a string to known values "SMDK4210" and
 * "SMDK4x12" for Samsung Exynos chipsets. If platform identifier matches one of
 * the SMDK* values, extracts model information into \p chipset argument. For
 * "SMDK4x12" match, decodes the chipset name using number of cores.
 *
 * @param start - start of the platform identifier (/proc/cpuinfo Hardware
 * string or ro.product.board) to match.
 * @param end - end of the platform identifier (/proc/cpuinfo Hardware string or
 * ro.product.board) to match.
 * @param cores - number of cores in the chipset.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_and_parse_smdk(
	const char* start,
	const char* end,
	uint32_t cores,
	struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect exactly 8 symbols: "SMDK" (4 symbols) + 4-digit model number
	 */
	if (start + 8 != end) {
		return false;
	}

	/*
	 * Check that string starts with "MT" (case-insensitive).
	 * The first four characters are loaded as a 32-bit little endian word
	 * and converted to lowercase.
	 */
	const uint32_t expected_smdk = UINT32_C(0x20202020) | load_u32le(start);
	if (expected_smdk != UINT32_C(0x6B646D73) /* "kdms" = reverse("smdk") */) {
		return false;
	}

	/*
	 * Check that string ends with "4210" or "4x12".
	 * The last four characters are loaded and compared as a 32-bit little
	 * endian word.
	 */
	uint32_t model = 0;
	const uint32_t expected_model = load_u32le(start + 4);
	switch (expected_model) {
		case UINT32_C(0x30313234): /* "0124" = reverse("4210") */
			model = 4210;
			break;
		case UINT32_C(0x32317834): /* "21x4" = reverse("4x12") */
			switch (cores) {
				case 2:
					model = 4212;
					break;
				case 4:
					model = 4412;
					break;
				default:
					cpuinfo_log_warning(
						"system reported invalid %" PRIu32 "-core Exynos 4x12 chipset", cores);
			}
	}

	if (model == 0) {
		return false;
	}

	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_samsung,
		.series = cpuinfo_arm_chipset_series_samsung_exynos,
		.model = model,
	};
	return true;
}

/**
 * Tries to match /MTK?\d{4}[A-Z/]*$/ signature for MediaTek MT chipsets.
 * If match successful, extracts model information into \p chipset argument.
 *
 * @param start - start of the platform identifier (/proc/cpuinfo Hardware
 * string, ro.product.board, ro.board.platform, ro.mediatek.platform, or
 * ro.chipname) to match.
 * @param end - end of the platform identifier (/proc/cpuinfo Hardware string,
 * ro.product.board, ro.board.platform, ro.mediatek.platform, or ro.chipname) to
 * match.
 * @param match_end - indicates if the function should attempt to match through
 * the end of the string and fail if there are unparsed characters in the end,
 * or match only MTK signature, model number, and some of the suffix characters
 * (the ones that pass validation).
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_mt(
	const char* start,
	const char* end,
	bool match_end,
	struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect at least 6 symbols: "MT" (2 symbols) + 4-digit model number */
	if (start + 6 > end) {
		return false;
	}

	/*
	 * Check that string starts with "MT" (case-insensitive).
	 * The first two characters are loaded as 16-bit little endian word and
	 * converted to lowercase.
	 */
	const uint16_t mt = UINT16_C(0x2020) | load_u16le(start);
	if (mt != UINT16_C(0x746D) /* "tm" */) {
		return false;
	}

	/* Some images report "MTK" rather than "MT" */
	const char* pos = start + 2;
	if (((uint8_t)*pos | UINT8_C(0x20)) == (uint8_t)'k') {
		pos++;

		/* Expect 4 more symbols after "MTK" (4-digit model number) */
		if (pos + 4 > end) {
			return false;
		}
	}

	/* Validate and parse 4-digit model number */
	uint32_t model = 0;
	for (uint32_t i = 0; i < 4; i++) {
		const uint32_t digit = (uint32_t)(uint8_t)(*pos++) - '0';
		if (digit >= 10) {
			/* Not really a digit */
			return false;
		}
		model = model * 10 + digit;
	}

	/* Record parsed chipset. This implicitly zeroes-out suffix, which will
	 * be parsed later. */
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_mediatek,
		.series = cpuinfo_arm_chipset_series_mediatek_mt,
		.model = model,
	};

	if (match_end) {
		/* Check that the potential suffix does not exceed maximum
		 * length */
		const size_t suffix_length = end - pos;
		if (suffix_length > CPUINFO_ARM_CHIPSET_SUFFIX_MAX) {
			return false;
		}

		/* Validate suffix characters and copy them to chipset structure
		 */
		for (size_t i = 0; i < suffix_length; i++) {
			const char c = (*pos++);
			if (is_ascii_alphabetic(c)) {
				/* Matched a letter [A-Za-z], convert to
				 * uppercase */
				chipset->suffix[i] = c & '\xDF';
			} else if (c == '/') {
				/* Matched a slash '/' */
				chipset->suffix[i] = c;
			} else {
				/* Invalid suffix character (neither of
				 * [A-Za-z/]) */
				return false;
			}
		}
	} else {
		/* Validate and parse as many suffix characters as we can */
		for (size_t i = 0; i < CPUINFO_ARM_CHIPSET_SUFFIX_MAX; i++) {
			if (pos + i == end) {
				break;
			}

			const char c = pos[i];
			if (is_ascii_alphabetic(c)) {
				/* Matched a letter [A-Za-z], convert to
				 * uppercase */
				chipset->suffix[i] = c & '\xDF';
			} else if (c == '/') {
				/* Matched a slash '/' */
				chipset->suffix[i] = c;
			} else {
				/* Invalid suffix character (neither of
				 * [A-Za-z/]). This marks the end of the suffix.
				 */
				break;
			}
		}
	}
	/* All suffix characters successfully validated and copied to chipset
	 * data */
	return true;
}

/**
 * Tries to match /[Kk]irin\s?\d{3}$/ signature for HiSilicon Kirin chipsets.
 * If match successful, extracts model information into \p chipset argument.
 *
 * @param start - start of the /proc/cpuinfo Hardware string to match.
 * @param end - end of the /proc/cpuinfo Hardware string to match.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_kirin(const char* start, const char* end, struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect 8-9 symbols: "Kirin" (5 symbols) + optional whitespace (1
	 * symbol) + 3-digit model number */
	const size_t length = end - start;
	switch (length) {
		case 8:
		case 9:
			break;
		default:
			return false;
	}

	/* Check that the string starts with "Kirin" or "kirin". */
	if (((uint8_t)start[0] | UINT8_C(0x20)) != (uint8_t)'k') {
		return false;
	}
	/* Symbols 1-5 are loaded and compared as little-endian 32-bit word. */
	const uint32_t irin = load_u32le(start + 1);
	if (irin != UINT32_C(0x6E697269) /* "niri" = reverse("irin") */) {
		return false;
	}

	/* Check for optional whitespace after "Kirin" */
	if (is_ascii_whitespace(start[5])) {
		/* When whitespace is present after "Kirin", expect 9 symbols
		 * total */
		if (length != 9) {
			return false;
		}
	}

	/* Validate and parse 3-digit model number */
	uint32_t model = 0;
	for (int32_t i = 0; i < 3; i++) {
		const uint32_t digit = (uint32_t)(uint8_t)end[i - 3] - '0';
		if (digit >= 10) {
			/* Not really a digit */
			return false;
		}
		model = model * 10 + digit;
	}

	/*
	 * Thats it, return parsed chipset.
	 * Technically, Kirin 910T has a suffix, but it never appears in the
	 * form of "910T" string. Instead, Kirin 910T devices report "hi6620oem"
	 * string (handled outside of this function).
	 */
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_hisilicon,
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = model,
	};
	return true;
}

/**
 * Tries to match /rk\d{4}[a-z]?$/ signature for Rockchip RK chipsets.
 * If match successful, extracts model information into \p chipset argument.
 *
 * @param start - start of the platform identifier (/proc/cpuinfo Hardware
 * string or ro.board.platform) to match.
 * @param end - end of the platform identifier (/proc/cpuinfo Hardware string or
 * ro.board.platform) to match.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_rk(const char* start, const char* end, struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect 6-7 symbols: "RK" (2 symbols) + 4-digit model number +
	 * optional 1-letter suffix */
	const size_t length = end - start;
	switch (length) {
		case 6:
		case 7:
			break;
		default:
			return false;
	}

	/*
	 * Check that string starts with "RK" (case-insensitive).
	 * The first two characters are loaded as 16-bit little endian word and
	 * converted to lowercase.
	 */
	const uint16_t expected_rk = UINT16_C(0x2020) | load_u16le(start);
	if (expected_rk != UINT16_C(0x6B72) /* "kr" = reverse("rk") */) {
		return false;
	}

	/* Validate and parse 4-digit model number */
	uint32_t model = 0;
	for (uint32_t i = 2; i < 6; i++) {
		const uint32_t digit = (uint32_t)(uint8_t)start[i] - '0';
		if (digit >= 10) {
			/* Not really a digit */
			return false;
		}
		model = model * 10 + digit;
	}

	/* Parse optional suffix */
	char suffix = 0;
	if (length == 7) {
		/* Parse the suffix letter */
		const char c = start[6];
		if (is_ascii_alphabetic(c)) {
			/* Convert to upper case */
			suffix = c & '\xDF';
		} else {
			/* Invalid suffix character */
			return false;
		}
	}

	/* Return parsed chipset */
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_rockchip,
		.series = cpuinfo_arm_chipset_series_rockchip_rk,
		.model = model,
		.suffix =
			{
				[0] = suffix,
			},
	};
	return true;
}

/**
 * Tries to match, case-insentitively, /s[cp]\d{4}[a-z]*|scx15$/ signature for
 * Spreadtrum SC chipsets. If match successful, extracts model information into
 * \p chipset argument.
 *
 * @param start - start of the platform identifier (/proc/cpuinfo Hardware
 * string, ro.product.board, ro.board.platform, or ro.chipname) to match.
 * @param end - end of the platform identifier (/proc/cpuinfo Hardware string,
 * ro.product.board, ro.board.platform, or ro.chipname) to match.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_sc(const char* start, const char* end, struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect at least 5 symbols: "scx15" */
	if (start + 5 > end) {
		return false;
	}

	/*
	 * Check that string starts with "S[CP]" (case-insensitive).
	 * The first two characters are loaded as 16-bit little endian word and
	 * converted to lowercase.
	 */
	const uint16_t expected_sc_or_sp = UINT16_C(0x2020) | load_u16le(start);
	switch (expected_sc_or_sp) {
		case UINT16_C(0x6373): /* "cs" = reverse("sc") */
		case UINT16_C(0x7073): /* "ps" = reverse("sp") */
			break;
		default:
			return false;
	}

	/* Special case: "scx" prefix (SC7715 reported as "scx15") */
	if ((start[2] | '\x20') == 'x') {
		/* Expect exactly 5 characters: "scx15" */
		if (start + 5 != end) {
			return false;
		}

		/* Check that string ends with "15" */
		const uint16_t expected_15 = load_u16le(start + 3);
		if (expected_15 != UINT16_C(0x3531) /* "51" = reverse("15") */) {
			return false;
		}

		*chipset = (struct cpuinfo_arm_chipset){
			.vendor = cpuinfo_arm_chipset_vendor_spreadtrum,
			.series = cpuinfo_arm_chipset_series_spreadtrum_sc,
			.model = 7715,
		};
		return true;
	}

	/* Expect at least 6 symbols: "S[CP]" (2 symbols) + 4-digit model number
	 */
	if (start + 6 > end) {
		return false;
	}

	/* Validate and parse 4-digit model number */
	uint32_t model = 0;
	for (uint32_t i = 2; i < 6; i++) {
		const uint32_t digit = (uint32_t)(uint8_t)start[i] - '0';
		if (digit >= 10) {
			/* Not really a digit */
			return false;
		}
		model = model * 10 + digit;
	}

	/* Write parsed chipset */
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_spreadtrum,
		.series = cpuinfo_arm_chipset_series_spreadtrum_sc,
		.model = model,
	};

	/* Validate and copy suffix letters. If suffix is too long, truncate at
	 * CPUINFO_ARM_CHIPSET_SUFFIX_MAX letters. */
	const char* suffix = start + 6;
	for (size_t i = 0; i < CPUINFO_ARM_CHIPSET_SUFFIX_MAX; i++) {
		if (suffix + i == end) {
			break;
		}

		const char c = suffix[i];
		if (!is_ascii_alphabetic(c)) {
			/* Invalid suffix character */
			return false;
		}
		/* Convert suffix letter to uppercase */
		chipset->suffix[i] = c & '\xDF';
	}
	return true;
}

/**
 * Tries to match, case-sentitively, /Unisoc T\d{3,4}/ signature for Unisoc T
 * chipset. If match successful, extracts model information into \p chipset
 * argument.
 *
 * @param start - start of the platform identifier (/proc/cpuinfo Hardware
 * string, ro.product.board, ro.board.platform, or ro.chipname) to match.
 * @param end - end of the platform identifier (/proc/cpuinfo Hardware string,
 * ro.product.board, ro.board.platform, or ro.chipname) to match.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_t(const char* start, const char* end, struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect 11-12 symbols: "Unisoc T" (8 symbols) + 3-4-digit model number
	 */
	const size_t length = end - start;
	switch (length) {
		case 11:
		case 12:
			break;
		default:
			return false;
	}

	/* Check that string starts with "Unisoc T". The first four characters
	 * are loaded as 32-bit little endian word */
	const uint32_t expected_unis = load_u32le(start);
	if (expected_unis != UINT32_C(0x73696E55) /* "sinU" = reverse("Unis") */) {
		return false;
	}

	/* The next four characters are loaded as 32-bit little endian word */
	const uint32_t expected_oc_t = load_u32le(start + 4);
	if (expected_oc_t != UINT32_C(0x5420636F) /* "T co" = reverse("oc T") */) {
		return false;
	}

	/* Validate and parse 3-4 digit model number */
	uint32_t model = 0;
	for (uint32_t i = 8; i < length; i++) {
		const uint32_t digit = (uint32_t)(uint8_t)start[i] - '0';
		if (digit >= 10) {
			/* Not really a digit */
			return false;
		}
		model = model * 10 + digit;
	}

	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_unisoc,
		.series = cpuinfo_arm_chipset_series_unisoc_t,
		.model = model,
	};
	return true;
}

/**
 * Tries to match /lc\d{4}[a-z]?$/ signature for Leadcore LC chipsets.
 * If match successful, extracts model information into \p chipset argument.
 *
 * @param start - start of the platform identifier (ro.product.board or
 * ro.board.platform) to match.
 * @param end - end of the platform identifier (ro.product.board or
 * ro.board.platform) to match.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_lc(const char* start, const char* end, struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect 6-7 symbols: "lc" (2 symbols) + 4-digit model number +
	 * optional 1-letter suffix */
	const size_t length = end - start;
	switch (length) {
		case 6:
		case 7:
			break;
		default:
			return false;
	}

	/* Check that string starts with "lc". The first two characters are
	 * loaded as 16-bit little endian word */
	const uint16_t expected_lc = load_u16le(start);
	if (expected_lc != UINT16_C(0x636C) /* "cl" = reverse("lc") */) {
		return false;
	}

	/* Validate and parse 4-digit model number */
	uint32_t model = 0;
	for (uint32_t i = 2; i < 6; i++) {
		const uint32_t digit = (uint32_t)(uint8_t)start[i] - '0';
		if (digit >= 10) {
			/* Not really a digit */
			return false;
		}
		model = model * 10 + digit;
	}

	/* Parse optional suffix letter */
	char suffix = 0;
	if (length == 7) {
		const char c = start[6];
		if (is_ascii_alphabetic(c)) {
			/* Convert to uppercase */
			chipset->suffix[0] = c & '\xDF';
		} else {
			/* Invalid suffix character */
			return false;
		}
	}

	/* Return parsed chipset */
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_leadcore,
		.series = cpuinfo_arm_chipset_series_leadcore_lc,
		.model = model,
		.suffix =
			{
				[0] = suffix,
			},
	};
	return true;
}

/**
 * Tries to match /PXA(\d{3,4}|1L88)$/ signature for Marvell PXA chipsets.
 * If match successful, extracts model information into \p chipset argument.
 *
 * @param start - start of the platform identifier (/proc/cpuinfo Hardware
 * string, ro.product.board or ro.chipname) to match.
 * @param end - end of the platform identifier (/proc/cpuinfo Hardaware string,
 * ro.product.board or ro.chipname) to match.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_pxa(const char* start, const char* end, struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect 6-7 symbols: "PXA" (3 symbols) + 3-4 digit model number */
	const size_t length = end - start;
	switch (length) {
		case 6:
		case 7:
			break;
		default:
			return false;
	}

	/* Check that the string starts with "PXA". Symbols 1-3 are loaded and
	 * compared as little-endian 16-bit word. */
	if (start[0] != 'P') {
		return false;
	}
	const uint16_t expected_xa = load_u16le(start + 1);
	if (expected_xa != UINT16_C(0x4158) /* "AX" = reverse("XA") */) {
		return false;
	}

	uint32_t model = 0;

	/* Check for a very common typo: "PXA1L88" for "PXA1088" */
	if (length == 7) {
		/* Load 4 model "number" symbols as a little endian 32-bit word
		 * and compare to "1L88" */
		const uint32_t expected_1L88 = load_u32le(start + 3);
		if (expected_1L88 == UINT32_C(0x38384C31) /* "88L1" = reverse("1L88") */) {
			model = 1088;
			goto write_chipset;
		}
	}

	/* Check and parse 3-4 digit model number */
	for (uint32_t i = 3; i < length; i++) {
		const uint32_t digit = (uint32_t)(uint8_t)start[i] - '0';
		if (digit >= 10) {
			/* Not really a digit */
			return false;
		}
		model = model * 10 + digit;
	}

	/* Return parsed chipset. */
write_chipset:
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_marvell,
		.series = cpuinfo_arm_chipset_series_marvell_pxa,
		.model = model,
	};
	return true;
}

/**
 * Tries to match /BCM\d{4}$/ signature for Broadcom BCM chipsets.
 * If match successful, extracts model information into \p chipset argument.
 *
 * @param start - start of the /proc/cpuinfo Hardware string to match.
 * @param end - end of the /proc/cpuinfo Hardware string to match.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_bcm(const char* start, const char* end, struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect exactly 7 symbols: "BCM" (3 symbols) + 4-digit model number */
	if (start + 7 != end) {
		return false;
	}

	/* Check that the string starts with "BCM".
	 * The first three characters are loaded and compared as a 24-bit little
	 * endian word.
	 */
	const uint32_t expected_bcm = load_u24le(start);
	if (expected_bcm != UINT32_C(0x004D4342) /* "MCB" = reverse("BCM") */) {
		return false;
	}

	/* Validate and parse 4-digit model number */
	uint32_t model = 0;
	for (uint32_t i = 3; i < 7; i++) {
		const uint32_t digit = (uint32_t)(uint8_t)start[i] - '0';
		if (digit >= 10) {
			/* Not really a digit */
			return false;
		}
		model = model * 10 + digit;
	}

	/* Return parsed chipset. */
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_broadcom,
		.series = cpuinfo_arm_chipset_series_broadcom_bcm,
		.model = model,
	};
	return true;
}

/**
 * Tries to match /OMAP\d{4}$/ signature for Texas Instruments OMAP chipsets.
 * If match successful, extracts model information into \p chipset argument.
 *
 * @param start - start of the /proc/cpuinfo Hardware string to match.
 * @param end - end of the /proc/cpuinfo Hardware string to match.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_omap(const char* start, const char* end, struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect exactly 8 symbols: "OMAP" (4 symbols) + 4-digit model number
	 */
	if (start + 8 != end) {
		return false;
	}

	/* Check that the string starts with "OMAP". Symbols 0-4 are loaded and
	 * compared as little-endian 32-bit word. */
	const uint32_t expected_omap = load_u32le(start);
	if (expected_omap != UINT32_C(0x50414D4F) /* "PAMO" = reverse("OMAP") */) {
		return false;
	}

	/* Validate and parse 4-digit model number */
	uint32_t model = 0;
	for (uint32_t i = 4; i < 8; i++) {
		const uint32_t digit = (uint32_t)(uint8_t)start[i] - '0';
		if (digit >= 10) {
			/* Not really a digit */
			return false;
		}
		model = model * 10 + digit;
	}

	/* Return parsed chipset. */
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_texas_instruments,
		.series = cpuinfo_arm_chipset_series_texas_instruments_omap,
		.model = model,
	};
	return true;
}

/**
 * Compares platform identifier string to known values for Broadcom chipsets.
 * If the string matches one of the known values, the function decodes Broadcom
 * chipset from frequency and number of cores into \p chipset argument.
 *
 * @param start - start of the platform identifier (ro.product.board or
 * ro.board.platform) to match.
 * @param end - end of the platform identifier (ro.product.board or
 * ro.board.platform) to match.
 * @param cores - number of cores in the chipset.
 * @param max_cpu_freq_max - maximum of
 * /sys/devices/system/cpu/cpu<number>/cpofreq/cpu_freq_max values.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match and decoding.
 *
 * @returns true if signature matched (even if exact model can't be decoded),
 * false otherwise.
 */
static bool match_and_parse_broadcom(
	const char* start,
	const char* end,
	uint32_t cores,
	uint32_t max_cpu_freq_max,
	struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect 4-6 symbols: "java" (4 symbols), "rhea" (4 symbols), "capri"
	 * (5 symbols), or "hawaii" (6 symbols) */
	const size_t length = end - start;
	switch (length) {
		case 4:
		case 5:
		case 6:
			break;
		default:
			return false;
	}

	/*
	 * Compare the platform identifier to known values for Broadcom
	 * chipsets:
	 * - "rhea"
	 * - "java"
	 * - "capri"
	 * - "hawaii"
	 * Upon a successful match, decode chipset name from frequency and
	 * number of cores.
	 */
	uint32_t model = 0;
	char suffix = 0;
	const uint32_t expected_platform = load_u32le(start);
	switch (expected_platform) {
		case UINT32_C(0x61656872): /* "aehr" = reverse("rhea") */
			if (length == 4) {
				/*
				 * Detected "rhea" platform:
				 * - 1 core @ 849999 KHz -> BCM21654
				 * - 1 core @ 999999 KHz -> BCM21654G
				 */
				if (cores == 1) {
					model = 21654;
					if (max_cpu_freq_max >= 999999) {
						suffix = 'G';
					}
				}
			}
			break;
		case UINT32_C(0x6176616A): /* "avaj" = reverse("java") */
			if (length == 4) {
				/*
				 * Detected "java" platform:
				 * - 4 cores -> BCM23550
				 */
				if (cores == 4) {
					model = 23550;
				}
			}
			break;
		case UINT32_C(0x61776168): /* "awah" = reverse("hawa") */
			if (length == 6) {
				/* Check that string equals "hawaii" */
				const uint16_t expected_ii = load_u16le(start + 4);
				if (expected_ii == UINT16_C(0x6969) /* "ii" */) {
					/*
					 * Detected "hawaii" platform:
					 * - 1 core -> BCM21663
					 * - 2 cores @ 999999 KHz -> BCM21664
					 * - 2 cores @ 1200000 KHz -> BCM21664T
					 */
					switch (cores) {
						case 1:
							model = 21663;
							break;
						case 2:
							model = 21664;
							if (max_cpu_freq_max >= 1200000) {
								suffix = 'T';
							}
							break;
					}
				}
			}
			break;
		case UINT32_C(0x72706163): /* "rpac" = reverse("capr") */
			if (length == 5) {
				/* Check that string equals "capri" */
				if (start[4] == 'i') {
					/*
					 * Detected "capri" platform:
					 * - 2 cores -> BCM28155
					 */
					if (cores == 2) {
						model = 28155;
					}
				}
			}
			break;
	}

	if (model != 0) {
		/* Chipset was successfully decoded */
		*chipset = (struct cpuinfo_arm_chipset){
			.vendor = cpuinfo_arm_chipset_vendor_broadcom,
			.series = cpuinfo_arm_chipset_series_broadcom_bcm,
			.model = model,
			.suffix =
				{
					[0] = suffix,
				},
		};
	}
	return model != 0;
}

struct sunxi_map_entry {
	uint8_t sunxi;
	uint8_t cores;
	uint8_t model;
	char suffix;
};

static const struct sunxi_map_entry sunxi_map_entries[] = {
#if CPUINFO_ARCH_ARM
	{
		/* ("sun4i", 1) -> "A10" */
		.sunxi = 4,
		.cores = 1,
		.model = 10,
	},
	{
		/* ("sun5i", 1) -> "A13" */
		.sunxi = 5,
		.cores = 1,
		.model = 13,
	},
	{
		/* ("sun6i", 4) -> "A31" */
		.sunxi = 6,
		.cores = 4,
		.model = 31,
	},
	{
		/* ("sun7i", 2) -> "A20" */
		.sunxi = 7,
		.cores = 2,
		.model = 20,

	},
	{
		/* ("sun8i", 2) -> "A23" */
		.sunxi = 8,
		.cores = 2,
		.model = 23,
	},
	{
		/* ("sun8i", 4) -> "A33" */
		.sunxi = 8,
		.cores = 4,
		.model = 33,
	},
	{
		/* ("sun8i", 8) -> "A83T" */
		.sunxi = 8,
		.cores = 8,
		.model = 83,
		.suffix = 'T',
	},
	{
		/* ("sun9i", 8) -> "A80" */
		.sunxi = 9,
		.cores = 8,
		.model = 80,
	},
#endif /* CPUINFO_ARCH_ARM */
	{
		/* ("sun50i", 4) -> "A64" */
		.sunxi = 50,
		.cores = 4,
		.model = 64,
	},
};

/**
 * Tries to match /proc/cpuinfo Hardware string to Allwinner /sun\d+i/
 * signature. If the string matches signature, the function decodes Allwinner
 * chipset from the number in the signature and the number of cores, and stores
 * it in \p chipset argument.
 *
 * @param start - start of the /proc/cpuinfo Hardware string to match.
 * @param end - end of the /proc/cpuinfo Hardware string to match.
 * @param cores - number of cores in the chipset.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match and decoding.
 *
 * @returns true if signature matched (even if exact model can't be decoded),
 * false otherwise.
 */
static bool match_and_parse_sunxi(
	const char* start,
	const char* end,
	uint32_t cores,
	struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect at least 5 symbols: "sun" (3 symbols) + platform id (1-2
	 * digits) + "i" (1 symbol) */
	if (start + 5 > end) {
		return false;
	}

	/* Compare the first 3 characters to "sun" */
	if (start[0] != 's') {
		return false;
	}
	const uint16_t expected_un = load_u16le(start + 1);
	if (expected_un != UINT16_C(0x6E75) /* "nu" = reverse("un") */) {
		return false;
	}

	/* Check and parse the first (required) digit of the sunXi platform id
	 */
	uint32_t sunxi_platform = 0;
	{
		const uint32_t digit = (uint32_t)(uint8_t)start[3] - '0';
		if (digit >= 10) {
			/* Not really a digit */
			return false;
		}
		sunxi_platform = digit;
	}

	/* Parse optional second digit of the sunXi platform id */
	const char* pos = start + 4;
	{
		const uint32_t digit = (uint32_t)(uint8_t)(*pos) - '0';
		if (digit < 10) {
			sunxi_platform = sunxi_platform * 10 + digit;
			if (++pos == end) {
				/* Expected one more character, final 'i' letter
				 */
				return false;
			}
		}
	}

	/* Validate the final 'i' letter */
	if (*pos != 'i') {
		return false;
	}

	/* Compare sunXi platform id and number of cores to tabulated values to
	 * decode chipset name */
	uint32_t model = 0;
	char suffix = 0;
	for (size_t i = 0; i < CPUINFO_COUNT_OF(sunxi_map_entries); i++) {
		if (sunxi_platform == sunxi_map_entries[i].sunxi && cores == sunxi_map_entries[i].cores) {
			model = sunxi_map_entries[i].model;
			suffix = sunxi_map_entries[i].suffix;
			break;
		}
	}

	if (model == 0) {
		cpuinfo_log_info(
			"unrecognized %" PRIu32 "-core Allwinner sun%" PRIu32 " platform", cores, sunxi_platform);
	}
	/* Create chipset name from decoded data */
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_allwinner,
		.series = cpuinfo_arm_chipset_series_allwinner_a,
		.model = model,
		.suffix =
			{
				[0] = suffix,
			},
	};
	return true;
}

/**
 * Compares /proc/cpuinfo Hardware string to "WMT" signature.
 * If the string matches signature, the function decodes WonderMedia chipset
 * from frequency and number of cores into \p chipset argument.
 *
 * @param start - start of the /proc/cpuinfo Hardware string to match.
 * @param end - end of the /proc/cpuinfo Hardware string to match.
 * @param cores - number of cores in the chipset.
 * @param max_cpu_freq_max - maximum of
 * /sys/devices/system/cpu/cpu<number>/cpofreq/cpu_freq_max values.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match and decoding.
 *
 * @returns true if signature matched (even if exact model can't be decoded),
 * false otherwise.
 */
static bool match_and_parse_wmt(
	const char* start,
	const char* end,
	uint32_t cores,
	uint32_t max_cpu_freq_max,
	struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expected 3 symbols: "WMT" */
	if (start + 3 != end) {
		return false;
	}

	/* Compare string to "WMT" */
	if (start[0] != 'W') {
		return false;
	}
	const uint16_t expected_mt = load_u16le(start + 1);
	if (expected_mt != UINT16_C(0x544D) /* "TM" = reverse("MT") */) {
		return false;
	}

	/* Decode chipset name from frequency and number of cores */
	uint32_t model = 0;
	switch (cores) {
		case 1:
			switch (max_cpu_freq_max) {
				case 1008000:
					/* 1 core @ 1008000 KHz -> WM8950 */
					model = 8950;
					break;
				case 1200000:
					/* 1 core @ 1200000 KHz -> WM8850 */
					model = 8850;
					break;
			}
			break;
		case 2:
			if (max_cpu_freq_max == 1500000) {
				/* 2 cores @ 1500000 KHz -> WM8880 */
				model = 8880;
			}
			break;
	}

	if (model == 0) {
		cpuinfo_log_info(
			"unrecognized WonderMedia platform with %" PRIu32 " cores at %" PRIu32 " KHz",
			cores,
			max_cpu_freq_max);
	}
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_wondermedia,
		.series = cpuinfo_arm_chipset_series_wondermedia_wm,
		.model = model,
	};
	return true;
}

struct huawei_map_entry {
	uint32_t platform;
	uint32_t model;
};

static const struct huawei_map_entry huawei_platform_map[] = {
	{
		/* "ALP" -> Kirin 970 */
		.platform = UINT32_C(0x00504C41), /* "\0PLA" = reverse("ALP\0") */
		.model = 970,
	},
	{
		/* "BAC" -> Kirin 659 */
		.platform = UINT32_C(0x00434142), /* "\0CAB" = reverse("BAC\0") */
		.model = 659,
	},
	{
		/* "BLA" -> Kirin 970 */
		.platform = UINT32_C(0x00414C42), /* "\0ALB" = reverse("BLA\0") */
		.model = 970,
	},
	{
		/* "BKL" -> Kirin 970 */
		.platform = UINT32_C(0x004C4B42), /* "\0LKB" = reverse("BKL\0") */
		.model = 970,
	},
	{
		/* "CLT" -> Kirin 970 */
		.platform = UINT32_C(0x00544C43), /* "\0TLC" = reverse("CLT\0") */
		.model = 970,
	},
	{
		/* "COL" -> Kirin 970 */
		.platform = UINT32_C(0x004C4F43), /* "\0LOC" = reverse("COL\0") */
		.model = 970,
	},
	{
		/* "COR" -> Kirin 970 */
		.platform = UINT32_C(0x00524F43), /* "\0ROC" = reverse("COR\0") */
		.model = 970,
	},
	{
		/* "DUK" -> Kirin 960 */
		.platform = UINT32_C(0x004B5544), /* "\0KUD" = reverse("DUK\0") */
		.model = 960,
	},
	{
		/* "EML" -> Kirin 970 */
		.platform = UINT32_C(0x004C4D45), /* "\0LME" = reverse("EML\0") */
		.model = 970,
	},
	{
		/* "EVA" -> Kirin 955 */
		.platform = UINT32_C(0x00415645), /* "\0AVE" = reverse("EVA\0") */
		.model = 955,
	},
	{
		/* "FRD" -> Kirin 950 */
		.platform = UINT32_C(0x00445246), /* "\0DRF" = reverse("FRD\0") */
		.model = 950,
	},
	{
		/* "INE" -> Kirin 710 */
		.platform = UINT32_C(0x00454E49), /* "\0ENI" = reverse("INE\0") */
		.model = 710,
	},
	{
		/* "KNT" -> Kirin 950 */
		.platform = UINT32_C(0x00544E4B), /* "\0TNK" = reverse("KNT\0") */
		.model = 950,
	},
	{
		/* "LON" -> Kirin 960 */
		.platform = UINT32_C(0x004E4F4C), /* "\0NOL" = reverse("LON\0") */
		.model = 960,
	},
	{
		/* "LYA" -> Kirin 980 */
		.platform = UINT32_C(0x0041594C), /* "\0AYL" = reverse("LYA\0") */
		.model = 980,
	},
	{
		/* "MCN" -> Kirin 980 */
		.platform = UINT32_C(0x004E434D), /* "\0NCM" = reverse("MCN\0") */
		.model = 980,
	},
	{
		/* "MHA" -> Kirin 960 */
		.platform = UINT32_C(0x0041484D), /* "\0AHM" = reverse("MHA\0") */
		.model = 960,
	},
	{
		/* "NEO" -> Kirin 970 */
		.platform = UINT32_C(0x004F454E), /* "\0OEN" = reverse("NEO\0") */
		.model = 970,
	},
	{
		/* "NXT" -> Kirin 950 */
		.platform = UINT32_C(0x0054584E), /* "\0TXN" = reverse("NXT\0") */
		.model = 950,
	},
	{
		/* "PAN" -> Kirin 980 */
		.platform = UINT32_C(0x004E4150), /* "\0NAP" = reverse("PAN\0") */
		.model = 980,
	},
	{
		/* "PAR" -> Kirin 970 */
		.platform = UINT32_C(0x00524150), /* "\0RAP" = reverse("PAR\0") */
		.model = 970,
	},
	{
		/* "RVL" -> Kirin 970 */
		.platform = UINT32_C(0x004C5652), /* "\0LVR" = reverse("RVL\0") */
		.model = 970,
	},
	{
		/* "STF" -> Kirin 960 */
		.platform = UINT32_C(0x00465453), /* "\0FTS" = reverse("STF\0") */
		.model = 960,
	},
	{
		/* "SUE" -> Kirin 980 */
		.platform = UINT32_C(0x00455553), /* "\0EUS" = reverse("SUE\0") */
		.model = 980,
	},
	{
		/* "VIE" -> Kirin 955 */
		.platform = UINT32_C(0x00454956), /* "\0EIV" = reverse("VIE\0") */
		.model = 955,
	},
	{
		/* "VKY" -> Kirin 960 */
		.platform = UINT32_C(0x00594B56), /* "\0YKV" = reverse("VKY\0") */
		.model = 960,
	},
	{
		/* "VTR" -> Kirin 960 */
		.platform = UINT32_C(0x00525456), /* "\0RTV" = reverse("VTR\0") */
		.model = 960,
	},
};

/**
 * Tries to match ro.product.board string to Huawei
 * /([A-Z]{3})(\-[A-Z]?L\d{2})$/ signature where \1 is one of the known values
 * for Huawei devices, which do not report chipset name elsewhere. If the string
 * matches signature, the function decodes chipset (always HiSilicon Kirin for
 * matched devices) from the Huawei platform ID in the signature and stores it
 * in \p chipset argument.
 *
 * @param start - start of the ro.product.board string to match.
 * @param end - end of the ro.product.board string to match.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match and decoding.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_and_parse_huawei(
	const char* start,
	const char* end,
	struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/*
	 * Expect length of either 3, 7 or 8, exactly:
	 * - 3-letter platform identifier (see huawei_platform_map)
	 * - 3-letter platform identifier + '-' + 'L' + two digits
	 * - 3-letter platform identifier + '-' + capital letter + 'L' + two
	 * digits
	 */
	const size_t length = end - start;
	switch (length) {
		case 3:
		case 7:
		case 8:
			break;
		default:
			return false;
	}

	/*
	 * Try to find the first three-letter substring in among the tabulated
	 * entries for Huawei devices. The first three letters are loaded and
	 * compared as a little-endian 24-bit word.
	 */
	uint32_t model = 0;
	const uint32_t target_platform_id = load_u24le(start);
	for (uint32_t i = 0; i < CPUINFO_COUNT_OF(huawei_platform_map); i++) {
		if (huawei_platform_map[i].platform == target_platform_id) {
			model = huawei_platform_map[i].model;
			break;
		}
	}

	if (model == 0) {
		/* Platform does not match the tabulated Huawei entries */
		return false;
	}

	if (length > 3) {
		/*
		 * Check that:
		 * - The symbol after platform id is a dash
		 * - The symbol after it is an uppercase letter. For 7-symbol
		 * strings, the symbol is just 'L'.
		 */
		if (start[3] != '-' || !is_ascii_alphabetic_uppercase(start[4])) {
			return false;
		}

		/* Check that the last 3 entries are /L\d\d/ */
		if (end[-3] != 'L' || !is_ascii_numeric(end[-2]) || !is_ascii_numeric(end[-1])) {
			return false;
		}
	}

	/* All checks succeeded, commit chipset name */
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_hisilicon,
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = model,
	};
	return true;
}

/**
 * Tries to match /tcc\d{3}x$/ signature for Telechips TCCXXXx chipsets.
 * If match successful, extracts model information into \p chipset argument.
 *
 * @param start - start of the /proc/cpuinfo Hardware string to match.
 * @param end - end of the /proc/cpuinfo Hardware string to match.
 * @param[out] chipset - location where chipset information will be stored upon
 * a successful match.
 *
 * @returns true if signature matched, false otherwise.
 */
static bool match_tcc(const char* start, const char* end, struct cpuinfo_arm_chipset chipset[restrict static 1]) {
	/* Expect exactly 7 symbols: "tcc" (3 symbols) + 3-digit model number +
	 * fixed "x" suffix */
	if (start + 7 != end) {
		return false;
	}

	/* Quick check for the first character */
	if (start[0] != 't') {
		return false;
	}

	/* Load the next 2 bytes as little endian 16-bit word */
	const uint16_t expected_cc = load_u16le(start + 1);
	if (expected_cc != UINT16_C(0x6363) /* "cc" */) {
		return false;
	}

	/* Check and parse 3-digit model number */
	uint32_t model = 0;
	for (uint32_t i = 3; i < 6; i++) {
		const uint32_t digit = (uint32_t)(uint8_t)start[i] - '0';
		if (digit >= 10) {
			/* Not really a digit */
			return false;
		}
		model = model * 10 + digit;
	}

	/* Check the fixed 'x' suffix in the end */
	if (start[6] != 'x') {
		return false;
	}

	/* Commit parsed chipset. */
	*chipset = (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_telechips,
		.series = cpuinfo_arm_chipset_series_telechips_tcc,
		.model = model,
		.suffix = {[0] = 'X'},
	};
	return true;
}

/*
 * Compares ro.board.platform string to Nvidia Tegra signatures ("tegra" and
 * "tegra3") This check has effect on how /proc/cpuinfo Hardware string is
 * interpreted.
 *
 * @param start - start of the ro.board.platform string to check.
 * @param end - end of the ro.board.platform string to check.
 *
 * @returns true if the string matches an Nvidia Tegra signature, and false
 * otherwise
 */
static bool is_tegra(const char* start, const char* end) {
	/* Expect 5 ("tegra") or 6 ("tegra3") symbols */
	const size_t length = end - start;
	switch (length) {
		case 5:
		case 6:
			break;
		default:
			return false;
	}

	/* Check that the first 5 characters match "tegra" */
	if (start[0] != 't') {
		return false;
	}
	const uint32_t expected_egra = load_u32le(start + 1);
	if (expected_egra != UINT32_C(0x61726765) /* "arge" = reverse("egra") */) {
		return false;
	}

	/* Check if the string is either "tegra" (length = 5) or "tegra3"
	 * (length != 5) and last character is '3' */
	return (length == 5 || start[5] == '3');
}

struct special_map_entry {
	const char* platform;
	uint16_t model;
	uint8_t series;
	char suffix;
};

static const struct special_map_entry special_hardware_map_entries[] = {
#if CPUINFO_ARCH_ARM
	{
		/* "k3v2oem1" -> HiSilicon K3V2 */
		.platform = "k3v2oem1",
		.series = cpuinfo_arm_chipset_series_hisilicon_k3v,
		.model = 2,
	},
	{/* "hi6620oem" -> HiSilicon Kirin 910T */
	 .platform = "hi6620oem",
	 .series = cpuinfo_arm_chipset_series_hisilicon_kirin,
	 .model = 910,
	 .suffix = 'T'},
#endif /* CPUINFO_ARCH_ARM */
	{
		/* "hi6250" -> HiSilicon Kirin 650 */
		.platform = "hi6250",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 650,
	},
	{
		/* "hi6210sft" -> HiSilicon Kirin 620 */
		.platform = "hi6210sft",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 620,
	},
	{
		/* "hi3751" -> HiSilicon Hi3751 */
		.platform = "hi3751",
		.series = cpuinfo_arm_chipset_series_hisilicon_hi,
		.model = 3751,
	},
#if CPUINFO_ARCH_ARM
	{
		/* "hi3630" -> HiSilicon Kirin 920 */
		.platform = "hi3630",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 920,
	},
#endif /* CPUINFO_ARCH_ARM */
	{
		/* "hi3635" -> HiSilicon Kirin 930 */
		.platform = "hi3635",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 930,
	},
#if CPUINFO_ARCH_ARM
	{
		/* "gs702a" -> Actions ATM7029 (Cortex-A5 + GC1000) */
		.platform = "gs702a",
		.series = cpuinfo_arm_chipset_series_actions_atm,
		.model = 7029,
	},
	{
		/* "gs702c" -> Actions ATM7029B (Cortex-A5 + SGX540) */
		.platform = "gs702c",
		.series = cpuinfo_arm_chipset_series_actions_atm,
		.model = 7029,
		.suffix = 'B',
	},
	{
		/* "gs703d" -> Actions ATM7039S */
		.platform = "gs703d",
		.series = cpuinfo_arm_chipset_series_actions_atm,
		.model = 7039,
		.suffix = 'S',
	},
	{
		/* "gs705a" -> Actions ATM7059A */
		.platform = "gs705a",
		.series = cpuinfo_arm_chipset_series_actions_atm,
		.model = 7059,
		.suffix = 'A',
	},
	{
		/* "Amlogic Meson8" -> Amlogic S812 */
		.platform = "Amlogic Meson8",
		.series = cpuinfo_arm_chipset_series_amlogic_s,
		.model = 812,
	},
	{
		/* "Amlogic Meson8B" -> Amlogic S805 */
		.platform = "Amlogic Meson8B",
		.series = cpuinfo_arm_chipset_series_amlogic_s,
		.model = 805,
	},
	{
		/* "mapphone_CDMA" -> Texas Instruments OMAP4430 */
		.platform = "mapphone_CDMA",
		.series = cpuinfo_arm_chipset_series_texas_instruments_omap,
		.model = 4430,
	},
	{
		/* "Superior" -> Texas Instruments OMAP4470 */
		.platform = "Superior",
		.series = cpuinfo_arm_chipset_series_texas_instruments_omap,
		.model = 4470,
	},
	{
		/* "Tuna" (Samsung Galaxy Nexus) -> Texas Instruments OMAP4460
		 */
		.platform = "Tuna",
		.series = cpuinfo_arm_chipset_series_texas_instruments_omap,
		.model = 4460,
	},
	{
		/* "Manta" (Samsung Nexus 10) -> Samsung Exynos 5250 */
		.platform = "Manta",
		.series = cpuinfo_arm_chipset_series_samsung_exynos,
		.model = 5250,
	},
	{
		/* "Odin" -> LG Nuclun 7111 */
		.platform = "Odin",
		.series = cpuinfo_arm_chipset_series_lg_nuclun,
		.model = 7111,
	},
	{
		/* "Madison" -> MStar 6A338 */
		.platform = "Madison",
		.series = cpuinfo_arm_chipset_series_mstar_6a,
		.model = 338,
	},
#endif /* CPUINFO_ARCH_ARM */
};

static const struct special_map_entry tegra_hardware_map_entries[] = {
#if CPUINFO_ARCH_ARM
	{
		/* "cardhu" (Nvidia Cardhu developer tablet) -> Tegra T30 */
		.platform = "cardhu",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
	},
	{
		/* "kai" -> Tegra T30L */
		.platform = "kai",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
		.suffix = 'L',
	},
	{
		/* "p3" (Samsung Galaxy Tab 8.9) -> Tegra T20 */
		.platform = "p3",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 20,
	},
	{
		/* "n1" (Samsung Galaxy R / Samsung Captivate Glide) -> Tegra
		   AP20H */
		.platform = "n1",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_ap,
		.model = 20,
		.suffix = 'H',
	},
	{
		/* "SHW-M380S" (Samsung Galaxy Tab 10.1) -> Tegra T20 */
		.platform = "SHW-M380S",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 20,
	},
	{
		/* "m470" (Hisense Sero 7 Pro) -> Tegra T30L */
		.platform = "m470",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
		.suffix = 'L',
	},
	{
		/* "endeavoru" (HTC One X) -> Tegra AP33 */
		.platform = "endeavoru",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_ap,
		.model = 33,
	},
	{
		/* "evitareul" (HTC One X+) -> Tegra T33 */
		.platform = "evitareul",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 33,
	},
	{
		/* "enrc2b" (HTC One X+) -> Tegra T33 */
		.platform = "enrc2b",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 33,
	},
	{
		/* "mozart" (Asus Transformer Pad TF701T) -> Tegra T114 */
		.platform = "mozart",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 114,
	},
	{
		/* "tegratab" (Tegra Note 7) -> Tegra T114 */
		.platform = "tegratab",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 114,
	},
	{
		/* "tn8" (Nvidia Shield Tablet K1) -> Tegra T124 */
		.platform = "tn8",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 124,
	},
	{
		/* "roth" (Nvidia Shield Portable) -> Tegra T114 */
		.platform = "roth",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 114,
	},
	{
		/* "pisces" (Xiaomi Mi 3) -> Tegra T114 */
		.platform = "pisces",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 114,
	},
	{
		/* "mocha" (Xiaomi Mi Pad) -> Tegra T124 */
		.platform = "mocha",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 124,
	},
	{
		/* "stingray" (Motorola XOOM) -> Tegra AP20H */
		.platform = "stingray",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_ap,
		.model = 20,
		.suffix = 'H',
	},
	{
		/* "Ceres" (Wiko Highway 4G) -> Tegra SL460N */
		.platform = "Ceres",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_sl,
		.model = 460,
		.suffix = 'N',
	},
	{
		/* "MT799" (nabi 2 Tablet) -> Tegra T30 */
		.platform = "MT799",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
	},
	{
		/* "t8400n" (nabi DreamTab HD8) -> Tegra T114 */
		.platform = "t8400n",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 114,
	},
	{
		/* "chagall" (Fujitsu Stylistic M532) -> Tegra T30 */
		.platform = "chagall",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
	},
	{
		/* "ventana" (Asus Transformer TF101) -> Tegra T20 */
		.platform = "ventana",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 20,
	},
	{
		/* "bobsleigh" (Fujitsu Arrows Tab F-05E) -> Tegra T33 */
		.platform = "bobsleigh",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 33,
	},
	{
		/* "tegra_fjdev101" (Fujitsu Arrows X F-10D) -> Tegra AP33 */
		.platform = "tegra_fjdev101",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_ap,
		.model = 33,
	},
	{
		/* "tegra_fjdev103" (Fujitsu Arrows V F-04E) -> Tegra T33 */
		.platform = "tegra_fjdev103",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 33,
	},
	{
		/* "nbx03" (Sony Tablet S) -> Tegra T20 */
		.platform = "nbx03",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 20,
	},
	{
		/* "txs03" (Sony Xperia Tablet S) -> Tegra T30L */
		.platform = "txs03",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
		.suffix = 'L',
	},
	{
		/* "x3" (LG Optimus 4X HD P880) -> Tegra AP33 */
		.platform = "x3",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_ap,
		.model = 33,
	},
	{
		/* "vu10" (LG Optimus Vu P895) -> Tegra AP33 */
		.platform = "vu10",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_ap,
		.model = 33,
	},
	{
		/* "BIRCH" (HP Slate 7 Plus) -> Tegra T30L */
		.platform = "BIRCH",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
		.suffix = 'L',
	},
	{
		/* "macallan" (HP Slate 8 Pro) -> Tegra T114 */
		.platform = "macallan",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 114,
	},
	{
		/* "maya" (HP SlateBook 10 x2) -> Tegra T114 */
		.platform = "maya",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 114,
	},
	{
		/* "antares" (Toshiba AT100) -> Tegra T20 */
		.platform = "antares",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 20,
	},
	{
		/* "tostab12AL" (Toshiba AT300SE "Excite 10 SE") -> Tegra T30L
		 */
		.platform = "tostab12AL",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
		.suffix = 'L',
	},
	{
		/* "tostab12BL" (Toshiba AT10-A "Excite Pure") -> Tegra T30L */
		.platform = "tostab12BL",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
		.suffix = 'L',
	},
	{
		/* "sphinx" (Toshiba AT270 "Excite 7.7") -> Tegra T30 */
		.platform = "sphinx",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
	},
	{
		/* "tostab11BS" (Toshiba AT570 "Regza 7.7") -> Tegra T30 */
		.platform = "tostab11BS",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
	},
	{
		/* "tostab12BA" (Toshiba AT10-LE-A "Excite Pro") -> Tegra T114
		 */
		.platform = "tostab12BA",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 114,
	},
	{
		/* "vangogh" (Acer Iconia Tab A100) -> Tegra T20 */
		.platform = "vangogh",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 20,
	},
	{
		/* "a110" (Acer Iconia Tab A110) -> Tegra T30L */
		.platform = "a110",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
		.suffix = 'L',
	},
	{
		/* "picasso_e" (Acer Iconia Tab A200) -> Tegra AP20H */
		.platform = "picasso_e",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_ap,
		.model = 20,
		.suffix = 'H',
	},
	{
		/* "picasso_e2" (Acer Iconia Tab A210) -> Tegra T30L */
		.platform = "picasso_e2",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
		.suffix = 'L',
	},
	{
		/* "picasso" (Acer Iconia Tab A500) -> Tegra AP20H */
		.platform = "picasso",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_ap,
		.model = 20,
		.suffix = 'H',
	},
	{
		/* "picasso_m" (Acer Iconia Tab A510) -> Tegra T30 */
		.platform = "picasso_m",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
	},
	{
		/* "picasso_mf" (Acer Iconia Tab A700) -> Tegra T30 */
		.platform = "picasso_mf",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
	},
	{
		/* "avalon" (Toshiba AT300 "Excite 10") -> Tegra T30L */
		.platform = "avalon",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
		.suffix = 'L',
	},
	{
		/* "NS_14T004" (iRiver NS-14T004) -> Tegra T30L */
		.platform = "NS_14T004",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
		.suffix = 'L',
	},
	{
		/* "WIKIPAD" (Wikipad) -> Tegra T30 */
		.platform = "WIKIPAD",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
	},
	{
		/* "kb" (Pegatron Q00Q) -> Tegra T114 */
		.platform = "kb",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 114,
	},
#endif /* CPUINFO_ARCH_ARM */
	{
		/* "foster_e" (Nvidia Shield TV, Flash) -> Tegra T210 */
		.platform = "foster_e",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 210,
	},
	{
		/* "foster_e_hdd" (Nvidia Shield TV, HDD) -> Tegra T210 */
		.platform = "foster_e_hdd",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 210,
	},
	{
		/* "darcy" (Nvidia Shield TV 2017) -> Tegra T210 */
		.platform = "darcy",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 210,
	},
};

/*
 * Decodes chipset name from /proc/cpuinfo Hardware string.
 * For some chipsets, the function relies frequency and on number of cores for
 * chipset detection.
 *
 * @param[in] platform - /proc/cpuinfo Hardware string.
 * @param cores - number of cores in the chipset.
 * @param max_cpu_freq_max - maximum of
 * /sys/devices/system/cpu/cpu<number>/cpofreq/cpu_freq_max values.
 *
 * @returns Decoded chipset name. If chipset could not be decoded, the resulting
 * structure would use `unknown` vendor and series identifiers.
 */
struct cpuinfo_arm_chipset cpuinfo_arm_linux_decode_chipset_from_proc_cpuinfo_hardware(
	const char hardware[restrict static CPUINFO_HARDWARE_VALUE_MAX],
	uint32_t cores,
	uint32_t max_cpu_freq_max,
	bool is_tegra) {
	struct cpuinfo_arm_chipset chipset;
	const size_t hardware_length = strnlen(hardware, CPUINFO_HARDWARE_VALUE_MAX);
	const char* hardware_end = hardware + hardware_length;

	if (is_tegra) {
		/*
		 * Nvidia Tegra-specific path: compare /proc/cpuinfo Hardware
		 * string to tabulated Hardware values for popular
		 * chipsets/devices with Tegra chipsets. This path is only used
		 * when ro.board.platform indicates a Tegra chipset (albeit does
		 * not indicate which exactly Tegra chipset).
		 */
		for (size_t i = 0; i < CPUINFO_COUNT_OF(tegra_hardware_map_entries); i++) {
			if (strncmp(tegra_hardware_map_entries[i].platform, hardware, hardware_length) == 0 &&
			    tegra_hardware_map_entries[i].platform[hardware_length] == 0) {
				cpuinfo_log_debug(
					"found /proc/cpuinfo Hardware string \"%.*s\" in Nvidia Tegra chipset table",
					(int)hardware_length,
					hardware);
				/* Create chipset name from entry */
				return (struct cpuinfo_arm_chipset){
					.vendor = chipset_series_vendor[tegra_hardware_map_entries[i].series],
					.series = (enum cpuinfo_arm_chipset_series)tegra_hardware_map_entries[i].series,
					.model = tegra_hardware_map_entries[i].model,
					.suffix =
						{
							[0] = tegra_hardware_map_entries[i].suffix,
						},
				};
			}
		}
	} else {
		/* Generic path: consider all other vendors */

		bool word_start = true;
		for (const char* pos = hardware; pos != hardware_end; pos++) {
			const char c = *pos;
			switch (c) {
				case ' ':
				case '\t':
				case ',':
					word_start = true;
					break;
				default:
					if (word_start && is_ascii_alphabetic(c)) {
						/* Check Qualcomm MSM/APQ
						 * signature */
						if (match_msm_apq(pos, hardware_end, &chipset)) {
							cpuinfo_log_debug(
								"matched Qualcomm MSM/APQ signature in /proc/cpuinfo Hardware string \"%.*s\"",
								(int)hardware_length,
								hardware);
							return chipset;
						}

						/* Check SDMxxx (Qualcomm
						 * Snapdragon) signature */
						if (match_sdm(pos, hardware_end, &chipset)) {
							cpuinfo_log_debug(
								"matched Qualcomm SDM signature in /proc/cpuinfo Hardware string \"%.*s\"",
								(int)hardware_length,
								hardware);
							return chipset;
						}

						/* Check SMxxxx (Qualcomm
						 * Snapdragon) signature */
						if (match_sm(pos, hardware_end, &chipset)) {
							cpuinfo_log_debug(
								"matched Qualcomm SM signature in /proc/cpuinfo Hardware string \"%.*s\"",
								(int)hardware_length,
								hardware);
							return chipset;
						}

						/* Check MediaTek MT signature
						 */
						if (match_mt(pos, hardware_end, true, &chipset)) {
							cpuinfo_log_debug(
								"matched MediaTek MT signature in /proc/cpuinfo Hardware string \"%.*s\"",
								(int)hardware_length,
								hardware);
							return chipset;
						}

						/* Check HiSilicon Kirin
						 * signature */
						if (match_kirin(pos, hardware_end, &chipset)) {
							cpuinfo_log_debug(
								"matched HiSilicon Kirin signature in /proc/cpuinfo Hardware string \"%.*s\"",
								(int)hardware_length,
								hardware);
							return chipset;
						}

						/* Check Rockchip RK signature
						 */
						if (match_rk(pos, hardware_end, &chipset)) {
							cpuinfo_log_debug(
								"matched Rockchip RK signature in /proc/cpuinfo Hardware string \"%.*s\"",
								(int)hardware_length,
								hardware);
							return chipset;
						}
					}
					word_start = false;
					break;
			}
		}

		/* Check Samsung Exynos signature */
		if (match_samsung_exynos(hardware, hardware_end, &chipset)) {
			cpuinfo_log_debug(
				"matched Samsung Exynos signature in /proc/cpuinfo Hardware string \"%.*s\"",
				(int)hardware_length,
				hardware);
			return chipset;
		}

		/* Check universalXXXX (Samsung Exynos) signature */
		if (match_universal(hardware, hardware_end, &chipset)) {
			cpuinfo_log_debug(
				"matched UNIVERSAL (Samsung Exynos) signature in /proc/cpuinfo Hardware string \"%.*s\"",
				(int)hardware_length,
				hardware);
			return chipset;
		}

#if CPUINFO_ARCH_ARM
		/* Match /SMDK(4410|4x12)$/ */
		if (match_and_parse_smdk(hardware, hardware_end, cores, &chipset)) {
			cpuinfo_log_debug(
				"matched SMDK (Samsung Exynos) signature in /proc/cpuinfo Hardware string \"%.*s\"",
				(int)hardware_length,
				hardware);
			return chipset;
		}
#endif

		/* Check Spreadtrum SC signature */
		if (match_sc(hardware, hardware_end, &chipset)) {
			cpuinfo_log_debug(
				"matched Spreadtrum SC signature in /proc/cpuinfo Hardware string \"%.*s\"",
				(int)hardware_length,
				hardware);

			return chipset;
		}

		/* Check Unisoc T signature */
		if (match_t(hardware, hardware_end, &chipset)) {
			cpuinfo_log_debug(
				"matched Unisoc T signature in /proc/cpuinfo Hardware string \"%.*s\"",
				(int)hardware_length,
				hardware);

			return chipset;
		}

#if CPUINFO_ARCH_ARM
		/* Check Marvell PXA signature */
		if (match_pxa(hardware, hardware_end, &chipset)) {
			cpuinfo_log_debug(
				"matched Marvell PXA signature in /proc/cpuinfo Hardware string \"%.*s\"",
				(int)hardware_length,
				hardware);
			return chipset;
		}
#endif

		/* Match /sun\d+i/ signature and map to Allwinner chipset name
		 */
		if (match_and_parse_sunxi(hardware, hardware_end, cores, &chipset)) {
			cpuinfo_log_debug(
				"matched sunxi (Allwinner Ax) signature in /proc/cpuinfo Hardware string \"%.*s\"",
				(int)hardware_length,
				hardware);
			return chipset;
		}

		/* Check Broadcom BCM signature */
		if (match_bcm(hardware, hardware_end, &chipset)) {
			cpuinfo_log_debug(
				"matched Broadcom BCM signature in /proc/cpuinfo Hardware string \"%.*s\"",
				(int)hardware_length,
				hardware);
			return chipset;
		}

#if CPUINFO_ARCH_ARM
		/* Check Texas Instruments OMAP signature */
		if (match_omap(hardware, hardware_end, &chipset)) {
			cpuinfo_log_debug(
				"matched Texas Instruments OMAP signature in /proc/cpuinfo Hardware string \"%.*s\"",
				(int)hardware_length,
				hardware);
			return chipset;
		}

		/* Check WonderMedia WMT signature and decode chipset from
		 * frequency and number of cores  */
		if (match_and_parse_wmt(hardware, hardware_end, cores, max_cpu_freq_max, &chipset)) {
			cpuinfo_log_debug(
				"matched WonderMedia WMT signature in /proc/cpuinfo Hardware string \"%.*s\"",
				(int)hardware_length,
				hardware);
			return chipset;
		}

#endif

		/* Check Telechips TCC signature */
		if (match_tcc(hardware, hardware_end, &chipset)) {
			cpuinfo_log_debug(
				"matched Telechips TCC signature in /proc/cpuinfo Hardware string \"%.*s\"",
				(int)hardware_length,
				hardware);
			return chipset;
		}

		/* Compare to tabulated Hardware values for popular
		 * chipsets/devices which can't be otherwise detected */
		for (size_t i = 0; i < CPUINFO_COUNT_OF(special_hardware_map_entries); i++) {
			if (strncmp(special_hardware_map_entries[i].platform, hardware, hardware_length) == 0 &&
			    special_hardware_map_entries[i].platform[hardware_length] == 0) {
				cpuinfo_log_debug(
					"found /proc/cpuinfo Hardware string \"%.*s\" in special chipset table",
					(int)hardware_length,
					hardware);
				/* Create chipset name from entry */
				return (struct cpuinfo_arm_chipset){
					.vendor = chipset_series_vendor[special_hardware_map_entries[i].series],
					.series =
						(enum cpuinfo_arm_chipset_series)special_hardware_map_entries[i].series,
					.model = special_hardware_map_entries[i].model,
					.suffix =
						{
							[0] = special_hardware_map_entries[i].suffix,
						},
				};
			}
		}
	}

	return (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_unknown,
		.series = cpuinfo_arm_chipset_series_unknown,
	};
}

#ifdef __ANDROID__
static const struct special_map_entry special_board_map_entries[] = {
	{
		/* "hi6250" -> HiSilicon Kirin 650 */
		.platform = "hi6250",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 650,
	},
	{
		/* "hi6210sft" -> HiSilicon Kirin 620 */
		.platform = "hi6210sft",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 620,
	},
#if CPUINFO_ARCH_ARM
	{
		/* "hi3630" -> HiSilicon Kirin 920 */
		.platform = "hi3630",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 920,
	},
#endif /* CPUINFO_ARCH_ARM */
	{
		/* "hi3635" -> HiSilicon Kirin 930 */
		.platform = "hi3635",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 930,
	},
	{
		/* "hi3650" -> HiSilicon Kirin 950 */
		.platform = "hi3650",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 950,
	},
	{
		/* "hi3660" -> HiSilicon Kirin 960 */
		.platform = "hi3660",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 960,
	},
#if CPUINFO_ARCH_ARM
	{
		/* "mp523x" -> Renesas MP5232 */
		.platform = "mp523x",
		.series = cpuinfo_arm_chipset_series_renesas_mp,
		.model = 5232,
	},
#endif /* CPUINFO_ARCH_ARM */
	{
		/* "BEETHOVEN" (Huawei MadiaPad M3) -> HiSilicon Kirin 950 */
		.platform = "BEETHOVEN",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 950,
	},
#if CPUINFO_ARCH_ARM
	{
		/* "hws7701u" (Huawei MediaPad 7 Youth) -> Rockchip RK3168 */
		.platform = "hws7701u",
		.series = cpuinfo_arm_chipset_series_rockchip_rk,
		.model = 3168,
	},
	{
		/* "g2mv" (LG G2 mini LTE) -> Nvidia Tegra SL460N */
		.platform = "g2mv",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_sl,
		.model = 460,
		.suffix = 'N',
	},
	{
		/* "K00F" (Asus MeMO Pad 10) -> Rockchip RK3188 */
		.platform = "K00F",
		.series = cpuinfo_arm_chipset_series_rockchip_rk,
		.model = 3188,
	},
	{
		/* "T7H" (HP Slate 7) -> Rockchip RK3066 */
		.platform = "T7H",
		.series = cpuinfo_arm_chipset_series_rockchip_rk,
		.model = 3066,
	},
	{
		/* "tuna" (Samsung Galaxy Nexus) -> Texas Instruments OMAP4460
		 */
		.platform = "tuna",
		.series = cpuinfo_arm_chipset_series_texas_instruments_omap,
		.model = 4460,
	},
	{
		/* "grouper" (Asus Nexus 7 2012) -> Nvidia Tegra T30L */
		.platform = "grouper",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 30,
		.suffix = 'L',
	},
#endif /* CPUINFO_ARCH_ARM */
	{
		/* "flounder" (HTC Nexus 9) -> Nvidia Tegra T132 */
		.platform = "flounder",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 132,
	},
	{
		/* "dragon" (Google Pixel C) -> Nvidia Tegra T210 */
		.platform = "dragon",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 210,
	},
	{
		/* "sailfish" (Google Pixel) -> Qualcomm MSM8996PRO */
		.platform = "sailfish",
		.series = cpuinfo_arm_chipset_series_qualcomm_msm,
		.model = 8996,
		.suffix = 'P',
	},
	{
		/* "marlin" (Google Pixel XL) -> Qualcomm MSM8996PRO */
		.platform = "marlin",
		.series = cpuinfo_arm_chipset_series_qualcomm_msm,
		.model = 8996,
		.suffix = 'P',
	},
};

/*
 * Decodes chipset name from ro.product.board Android system property.
 * For some chipsets, the function relies frequency and on number of cores for
 * chipset detection.
 *
 * @param[in] platform - ro.product.board value.
 * @param cores - number of cores in the chipset.
 * @param max_cpu_freq_max - maximum of
 * /sys/devices/system/cpu/cpu<number>/cpofreq/cpu_freq_max values.
 *
 * @returns Decoded chipset name. If chipset could not be decoded, the resulting
 * structure would use `unknown` vendor and series identifiers.
 */
struct cpuinfo_arm_chipset cpuinfo_arm_android_decode_chipset_from_ro_product_board(
	const char ro_product_board[restrict static CPUINFO_BUILD_PROP_VALUE_MAX],
	uint32_t cores,
	uint32_t max_cpu_freq_max) {
	struct cpuinfo_arm_chipset chipset;
	const char* board = ro_product_board;
	const size_t board_length = strnlen(ro_product_board, CPUINFO_BUILD_PROP_VALUE_MAX);
	const char* board_end = ro_product_board + board_length;

	/* Check Qualcomm MSM/APQ signature */
	if (match_msm_apq(board, board_end, &chipset)) {
		cpuinfo_log_debug(
			"matched Qualcomm MSM/APQ signature in ro.product.board string \"%.*s\"",
			(int)board_length,
			board);
		return chipset;
	}

	/* Check universaXXXX (Samsung Exynos) signature */
	if (match_universal(board, board_end, &chipset)) {
		cpuinfo_log_debug(
			"matched UNIVERSAL (Samsung Exynos) signature in ro.product.board string \"%.*s\"",
			(int)board_length,
			board);
		return chipset;
	}

#if CPUINFO_ARCH_ARM
	/* Check SMDK (Samsung Exynos) signature */
	if (match_and_parse_smdk(board, board_end, cores, &chipset)) {
		cpuinfo_log_debug(
			"matched SMDK (Samsung Exynos) signature in ro.product.board string \"%.*s\"",
			(int)board_length,
			board);
		return chipset;
	}
#endif

	/* Check MediaTek MT signature */
	if (match_mt(board, board_end, true, &chipset)) {
		cpuinfo_log_debug(
			"matched MediaTek MT signature in ro.product.board string \"%.*s\"", (int)board_length, board);
		return chipset;
	}

	/* Check Spreadtrum SC signature */
	if (match_sc(board, board_end, &chipset)) {
		cpuinfo_log_debug(
			"matched Spreadtrum SC signature in ro.product.board string \"%.*s\"",
			(int)board_length,
			board);
		return chipset;
	}

#if CPUINFO_ARCH_ARM
	/* Check Marvell PXA signature */
	if (match_pxa(board, board_end, &chipset)) {
		cpuinfo_log_debug(
			"matched Marvell PXA signature in ro.product.board string \"%.*s\"", (int)board_length, board);
		return chipset;
	}

	/* Check Leadcore LCxxxx signature */
	if (match_lc(board, board_end, &chipset)) {
		cpuinfo_log_debug(
			"matched Leadcore LC signature in ro.product.board string \"%.*s\"", (int)board_length, board);
		return chipset;
	}

	/*
	 * Compare to tabulated ro.product.board values for Broadcom chipsets
	 * and decode chipset from frequency and number of cores.
	 */
	if (match_and_parse_broadcom(board, board_end, cores, max_cpu_freq_max, &chipset)) {
		cpuinfo_log_debug(
			"found ro.product.board string \"%.*s\" in Broadcom chipset table", (int)board_length, board);
		return chipset;
	}
#endif

	/* Compare to tabulated ro.product.board values for Huawei devices which
	 * don't report chipset elsewhere */
	if (match_and_parse_huawei(board, board_end, &chipset)) {
		cpuinfo_log_debug(
			"found ro.product.board string \"%.*s\" in Huawei chipset table", (int)board_length, board);
		return chipset;
	}

	/* Compare to tabulated ro.product.board values for popular
	 * chipsets/devices which can't be otherwise detected */
	for (size_t i = 0; i < CPUINFO_COUNT_OF(special_board_map_entries); i++) {
		if (strncmp(special_board_map_entries[i].platform, board, board_length) == 0 &&
		    special_board_map_entries[i].platform[board_length] == 0) {
			cpuinfo_log_debug(
				"found ro.product.board string \"%.*s\" in special chipset table",
				(int)board_length,
				board);
			/* Create chipset name from entry */
			return (struct cpuinfo_arm_chipset){
				.vendor = chipset_series_vendor[special_board_map_entries[i].series],
				.series = (enum cpuinfo_arm_chipset_series)special_board_map_entries[i].series,
				.model = special_board_map_entries[i].model,
				.suffix =
					{
						[0] = special_board_map_entries[i].suffix,
						/* The suffix of MSM8996PRO is
						   truncated at the first
						   letter, reconstruct it here.
						 */
						[1] = special_board_map_entries[i].suffix == 'P' ? 'R' : 0,
						[2] = special_board_map_entries[i].suffix == 'P' ? 'O' : 0,
					},
			};
		}
	}

	return (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_unknown,
		.series = cpuinfo_arm_chipset_series_unknown,
	};
}

struct amlogic_map_entry {
	char ro_board_platform[6];
	uint16_t model;
	uint8_t series;
	char suffix[3];
};

static const struct amlogic_map_entry amlogic_map_entries[] = {
#if CPUINFO_ARCH_ARM
	{
		/* "meson3" -> Amlogic AML8726-M */
		.ro_board_platform = "meson3",
		.series = cpuinfo_arm_chipset_series_amlogic_aml,
		.model = 8726,
		.suffix = "-M",
	},
	{
		/* "meson6" -> Amlogic AML8726-MX */
		.ro_board_platform = "meson6",
		.series = cpuinfo_arm_chipset_series_amlogic_aml,
		.model = 8726,
		.suffix = "-MX",
	},
	{
		/* "meson8" -> Amlogic S805 */
		.ro_board_platform = "meson8",
		.series = cpuinfo_arm_chipset_series_amlogic_s,
		.model = 805,
	},
#endif /* CPUINFO_ARCH_ARM */
	{
		/* "gxbaby" -> Amlogic S905 */
		.ro_board_platform = "gxbaby",
		.series = cpuinfo_arm_chipset_series_amlogic_s,
		.model = 905,
	},
	{
		/* "gxl" -> Amlogic S905X */
		.ro_board_platform = "gxl",
		.series = cpuinfo_arm_chipset_series_amlogic_s,
		.model = 905,
		.suffix = "X",
	},
	{
		/* "gxm" -> Amlogic S912 */
		.ro_board_platform = "gxm",
		.series = cpuinfo_arm_chipset_series_amlogic_s,
		.model = 912,
	},
};

static const struct special_map_entry special_platform_map_entries[] = {
#if CPUINFO_ARCH_ARM
	{
		/* "hi6620oem" -> HiSilicon Kirin 910T */
		.platform = "hi6620oem",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 910,
		.suffix = 'T',
	},
#endif /* CPUINFO_ARCH_ARM */
	{
		/* "hi6250" -> HiSilicon Kirin 650 */
		.platform = "hi6250",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 650,
	},
	{
		/* "hi6210sft" -> HiSilicon Kirin 620 */
		.platform = "hi6210sft",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 620,
	},
#if CPUINFO_ARCH_ARM
	{
		/* "hi3630" -> HiSilicon Kirin 920 */
		.platform = "hi3630",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 920,
	},
#endif /* CPUINFO_ARCH_ARM */
	{
		/* "hi3635" -> HiSilicon Kirin 930 */
		.platform = "hi3635",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 930,
	},
	{
		/* "hi3650" -> HiSilicon Kirin 950 */
		.platform = "hi3650",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 950,
	},
	{
		/* "hi3660" -> HiSilicon Kirin 960 */
		.platform = "hi3660",
		.series = cpuinfo_arm_chipset_series_hisilicon_kirin,
		.model = 960,
	},
#if CPUINFO_ARCH_ARM
	{
		/* "k3v2oem1" -> HiSilicon K3V2 */
		.platform = "k3v2oem1",
		.series = cpuinfo_arm_chipset_series_hisilicon_k3v,
		.model = 2,
	},
	{
		/* "k3v200" -> HiSilicon K3V2 */
		.platform = "k3v200",
		.series = cpuinfo_arm_chipset_series_hisilicon_k3v,
		.model = 2,
	},
	{
		/* "montblanc" -> NovaThor U8500 */
		.platform = "montblanc",
		.series = cpuinfo_arm_chipset_series_novathor_u,
		.model = 8500,
	},
#endif /* CPUINFO_ARCH_ARM */
	{
		/* "song" -> Pinecone Surge S1 */
		.platform = "song",
		.series = cpuinfo_arm_chipset_series_pinecone_surge_s,
		.model = 1,
	},
#if CPUINFO_ARCH_ARM
	{
		/* "rk322x" -> RockChip RK3229 */
		.platform = "rk322x",
		.series = cpuinfo_arm_chipset_series_rockchip_rk,
		.model = 3229,
	},
#endif /* CPUINFO_ARCH_ARM */
	{
		/* "tegra132" -> Nvidia Tegra T132 */
		.platform = "tegra132",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 132,
	},
	{
		/* "tegra210_dragon" -> Nvidia Tegra T210 */
		.platform = "tegra210_dragon",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 210,
	},
#if CPUINFO_ARCH_ARM
	{
		/* "tegra4" -> Nvidia Tegra T114 */
		.platform = "tegra4",
		.series = cpuinfo_arm_chipset_series_nvidia_tegra_t,
		.model = 114,
	},
	{
		/* "s5pc110" -> Samsung Exynos 3110 */
		.platform = "s5pc110",
		.series = cpuinfo_arm_chipset_series_samsung_exynos,
		.model = 3110,
	},
#endif /* CPUINFO_ARCH_ARM */
};

/*
 * Decodes chipset name from ro.board.platform Android system property.
 * For some chipsets, the function relies frequency and on number of cores for
 * chipset detection.
 *
 * @param[in] platform - ro.board.platform value.
 * @param cores - number of cores in the chipset.
 * @param max_cpu_freq_max - maximum of
 * /sys/devices/system/cpu/cpu<number>/cpofreq/cpu_freq_max values.
 *
 * @returns Decoded chipset name. If chipset could not be decoded, the resulting
 * structure would use `unknown` vendor and series identifiers.
 */
struct cpuinfo_arm_chipset cpuinfo_arm_android_decode_chipset_from_ro_board_platform(
	const char platform[restrict static CPUINFO_BUILD_PROP_VALUE_MAX],
	uint32_t cores,
	uint32_t max_cpu_freq_max) {
	struct cpuinfo_arm_chipset chipset;
	const size_t platform_length = strnlen(platform, CPUINFO_BUILD_PROP_VALUE_MAX);
	const char* platform_end = platform + platform_length;

	/* Check Qualcomm MSM/APQ signature */
	if (match_msm_apq(platform, platform_end, &chipset)) {
		cpuinfo_log_debug(
			"matched Qualcomm MSM/APQ signature in ro.board.platform string \"%.*s\"",
			(int)platform_length,
			platform);
		return chipset;
	}

	/* Check exynosXXXX (Samsung Exynos) signature */
	if (match_exynos(platform, platform_end, &chipset)) {
		cpuinfo_log_debug(
			"matched exynosXXXX (Samsung Exynos) signature in ro.board.platform string \"%.*s\"",
			(int)platform_length,
			platform);
		return chipset;
	}

	/* Check MediaTek MT signature */
	if (match_mt(platform, platform_end, true, &chipset)) {
		cpuinfo_log_debug(
			"matched MediaTek MT signature in ro.board.platform string \"%.*s\"",
			(int)platform_length,
			platform);
		return chipset;
	}

	/* Check HiSilicon Kirin signature */
	if (match_kirin(platform, platform_end, &chipset)) {
		cpuinfo_log_debug(
			"matched HiSilicon Kirin signature in ro.board.platform string \"%.*s\"",
			(int)platform_length,
			platform);
		return chipset;
	}

	/* Check Spreadtrum SC signature */
	if (match_sc(platform, platform_end, &chipset)) {
		cpuinfo_log_debug(
			"matched Spreadtrum SC signature in ro.board.platform string \"%.*s\"",
			(int)platform_length,
			platform);
		return chipset;
	}

	/* Check Rockchip RK signature */
	if (match_rk(platform, platform_end, &chipset)) {
		cpuinfo_log_debug(
			"matched Rockchip RK signature in ro.board.platform string \"%.*s\"",
			(int)platform_length,
			platform);
		return chipset;
	}

#if CPUINFO_ARCH_ARM
	/* Check Leadcore LCxxxx signature */
	if (match_lc(platform, platform_end, &chipset)) {
		cpuinfo_log_debug(
			"matched Leadcore LC signature in ro.board.platform string \"%.*s\"",
			(int)platform_length,
			platform);
		return chipset;
	}
#endif

	/* Compare to tabulated ro.board.platform values for Huawei devices
	 * which don't report chipset elsewhere */
	if (match_and_parse_huawei(platform, platform_end, &chipset)) {
		cpuinfo_log_debug(
			"found ro.board.platform string \"%.*s\" in Huawei chipset table",
			(int)platform_length,
			platform);
		return chipset;
	}

#if CPUINFO_ARCH_ARM
	/*
	 * Compare to known ro.board.platform values for Broadcom devices and
	 * detect chipset from frequency and number of cores
	 */
	if (match_and_parse_broadcom(platform, platform_end, cores, max_cpu_freq_max, &chipset)) {
		cpuinfo_log_debug(
			"found ro.board.platform string \"%.*s\" in Broadcom chipset table",
			(int)platform_length,
			platform);
		return chipset;
	}

	/*
	 * Compare to ro.board.platform value ("omap4") for OMAP4xxx chipsets.
	 * Upon successful match, detect OMAP4430 from frequency and number of
	 * cores.
	 */
	if (platform_length == 5 && cores == 2 && max_cpu_freq_max == 1008000 && memcmp(platform, "omap4", 5) == 0) {
		cpuinfo_log_debug(
			"matched Texas Instruments OMAP4 signature in ro.board.platform string \"%.*s\"",
			(int)platform_length,
			platform);

		return (struct cpuinfo_arm_chipset){
			.vendor = cpuinfo_arm_chipset_vendor_texas_instruments,
			.series = cpuinfo_arm_chipset_series_texas_instruments_omap,
			.model = 4430,
		};
	}
#endif

	/*
	 * Compare to tabulated ro.board.platform values for Amlogic
	 * chipsets/devices which can't be otherwise detected. The tabulated
	 * Amlogic ro.board.platform values have not more than 6 characters.
	 */
	if (platform_length <= 6) {
		for (size_t i = 0; i < CPUINFO_COUNT_OF(amlogic_map_entries); i++) {
			if (strncmp(amlogic_map_entries[i].ro_board_platform, platform, 6) == 0) {
				cpuinfo_log_debug(
					"found ro.board.platform string \"%.*s\" in Amlogic chipset table",
					(int)platform_length,
					platform);
				/* Create chipset name from entry */
				return (struct cpuinfo_arm_chipset){
					.vendor = cpuinfo_arm_chipset_vendor_amlogic,
					.series = (enum cpuinfo_arm_chipset_series)amlogic_map_entries[i].series,
					.model = amlogic_map_entries[i].model,
					.suffix =
						{
							[0] = amlogic_map_entries[i].suffix[0],
							[1] = amlogic_map_entries[i].suffix[1],
							[2] = amlogic_map_entries[i].suffix[2],
						},
				};
			}
		}
	}

	/* Compare to tabulated ro.board.platform values for popular
	 * chipsets/devices which can't be otherwise detected */
	for (size_t i = 0; i < CPUINFO_COUNT_OF(special_platform_map_entries); i++) {
		if (strncmp(special_platform_map_entries[i].platform, platform, platform_length) == 0 &&
		    special_platform_map_entries[i].platform[platform_length] == 0) {
			/* Create chipset name from entry */
			cpuinfo_log_debug(
				"found ro.board.platform string \"%.*s\" in special chipset table",
				(int)platform_length,
				platform);
			return (struct cpuinfo_arm_chipset){
				.vendor = chipset_series_vendor[special_platform_map_entries[i].series],
				.series = (enum cpuinfo_arm_chipset_series)special_platform_map_entries[i].series,
				.model = special_platform_map_entries[i].model,
				.suffix =
					{
						[0] = special_platform_map_entries[i].suffix,
					},
			};
		}
	}

	/* None of the ro.board.platform signatures matched, indicate unknown
	 * chipset
	 */
	return (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_unknown,
		.series = cpuinfo_arm_chipset_series_unknown,
	};
}

/*
 * Decodes chipset name from ro.mediatek.platform Android system property.
 *
 * @param[in] platform - ro.mediatek.platform value.
 *
 * @returns Decoded chipset name. If chipset could not be decoded, the resulting
 * structure would use `unknown` vendor and series identifiers.
 */
struct cpuinfo_arm_chipset cpuinfo_arm_android_decode_chipset_from_ro_mediatek_platform(
	const char platform[restrict static CPUINFO_BUILD_PROP_VALUE_MAX]) {
	struct cpuinfo_arm_chipset chipset;
	const char* platform_end = platform + strnlen(platform, CPUINFO_BUILD_PROP_VALUE_MAX);

	/* Check MediaTek MT signature */
	if (match_mt(platform, platform_end, false, &chipset)) {
		return chipset;
	}

	return (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_unknown,
		.series = cpuinfo_arm_chipset_series_unknown,
	};
}

/*
 * Decodes chipset name from ro.arch Android system property.
 *
 * The ro.arch property is matched only against Samsung Exynos signature.
 * Systems with other chipset rarely configure ro.arch Android system property,
 * and can be decoded through other properties, but some Exynos chipsets are
 * identified only in ro.arch.
 *
 * @param[in] arch - ro.arch value.
 *
 * @returns Decoded chipset name. If chipset could not be decoded, the resulting
 * structure would use `unknown` vendor and series identifiers.
 */
struct cpuinfo_arm_chipset cpuinfo_arm_android_decode_chipset_from_ro_arch(
	const char arch[restrict static CPUINFO_BUILD_PROP_VALUE_MAX]) {
	struct cpuinfo_arm_chipset chipset;
	const char* arch_end = arch + strnlen(arch, CPUINFO_BUILD_PROP_VALUE_MAX);

	/* Check Samsung exynosXXXX signature */
	if (match_exynos(arch, arch_end, &chipset)) {
		return chipset;
	}

	return (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_unknown,
		.series = cpuinfo_arm_chipset_series_unknown,
	};
}

/*
 * Decodes chipset name from ro.chipname or ro.hardware.chipname Android system
 * property.
 *
 * @param[in] chipname - ro.chipname or ro.hardware.chipname value.
 *
 * @returns Decoded chipset name. If chipset could not be decoded, the resulting
 * structure would use `unknown` vendor and series identifiers.
 */

struct cpuinfo_arm_chipset cpuinfo_arm_android_decode_chipset_from_ro_chipname(
	const char chipname[restrict static CPUINFO_BUILD_PROP_VALUE_MAX]) {
	struct cpuinfo_arm_chipset chipset;
	const size_t chipname_length = strnlen(chipname, CPUINFO_BUILD_PROP_VALUE_MAX);
	const char* chipname_end = chipname + chipname_length;

	/* Check Qualcomm MSM/APQ signatures */
	if (match_msm_apq(chipname, chipname_end, &chipset)) {
		cpuinfo_log_debug(
			"matched Qualcomm MSM/APQ signature in ro.chipname string \"%.*s\"",
			(int)chipname_length,
			chipname);
		return chipset;
	}

	/* Check SMxxxx (Qualcomm Snapdragon) signature */
	if (match_sm(chipname, chipname_end, &chipset)) {
		cpuinfo_log_debug(
			"matched Qualcomm SM signature in /proc/cpuinfo Hardware string \"%.*s\"",
			(int)chipname_length,
			chipname);
		return chipset;
	}

	/* Check exynosXXXX (Samsung Exynos) signature */
	if (match_exynos(chipname, chipname_end, &chipset)) {
		cpuinfo_log_debug(
			"matched exynosXXXX (Samsung Exynos) signature in ro.chipname string \"%.*s\"",
			(int)chipname_length,
			chipname);
		return chipset;
	}

	/* Check universalXXXX (Samsung Exynos) signature */
	if (match_universal(chipname, chipname_end, &chipset)) {
		cpuinfo_log_debug(
			"matched UNIVERSAL (Samsung Exynos) signature in ro.chipname Hardware string \"%.*s\"",
			(int)chipname_length,
			chipname);
		return chipset;
	}

	/* Check MediaTek MT signature */
	if (match_mt(chipname, chipname_end, true, &chipset)) {
		cpuinfo_log_debug(
			"matched MediaTek MT signature in ro.chipname string \"%.*s\"", (int)chipname_length, chipname);
		return chipset;
	}

	/* Check Spreadtrum SC signature */
	if (match_sc(chipname, chipname_end, &chipset)) {
		cpuinfo_log_debug(
			"matched Spreadtrum SC signature in ro.chipname string \"%.*s\"",
			(int)chipname_length,
			chipname);
		return chipset;
	}

#if CPUINFO_ARCH_ARM
	/* Check Marvell PXA signature */
	if (match_pxa(chipname, chipname_end, &chipset)) {
		cpuinfo_log_debug(
			"matched Marvell PXA signature in ro.chipname string \"%.*s\"", (int)chipname_length, chipname);
		return chipset;
	}

	/* Compare to ro.chipname value ("mp523x") for Renesas MP5232 which
	 * can't be otherwise detected */
	if (chipname_length == 6 && memcmp(chipname, "mp523x", 6) == 0) {
		cpuinfo_log_debug(
			"matched Renesas MP5232 signature in ro.chipname string \"%.*s\"",
			(int)chipname_length,
			chipname);

		return (struct cpuinfo_arm_chipset){
			.vendor = cpuinfo_arm_chipset_vendor_renesas,
			.series = cpuinfo_arm_chipset_series_renesas_mp,
			.model = 5232,
		};
	}
#endif

	return (struct cpuinfo_arm_chipset){
		.vendor = cpuinfo_arm_chipset_vendor_unknown,
		.series = cpuinfo_arm_chipset_series_unknown,
	};
}
#endif /* __ANDROID__ */

/*
 * Fix common bugs, typos, and renames in chipset name.
 *
 * @param[in,out] chipset - chipset name to fix.
 * @param cores - number of cores in the chipset.
 * @param max_cpu_freq_max - maximum of
 * /sys/devices/system/cpu/cpu<number>/cpofreq/cpu_freq_max values.
 */
void cpuinfo_arm_fixup_chipset(
	struct cpuinfo_arm_chipset chipset[restrict static 1],
	uint32_t cores,
	uint32_t max_cpu_freq_max) {
	switch (chipset->series) {
		case cpuinfo_arm_chipset_series_qualcomm_msm:
			/* Check if there is suffix */
			if (chipset->suffix[0] == 0) {
				/* No suffix, but the model may be misreported
				 */
				switch (chipset->model) {
					case 8216:
						/* MSM8216 was renamed to
						 * MSM8916 */
						cpuinfo_log_info("reinterpreted MSM8216 chipset as MSM8916");
						chipset->model = 8916;
						break;
					case 8916:
						/* Common bug: MSM8939
						 * (Octa-core) reported as
						 * MSM8916 (Quad-core)
						 */
						switch (cores) {
							case 4:
								break;
							case 8:
								cpuinfo_log_info(
									"reinterpreted MSM8916 chipset with 8 cores as MSM8939");
								chipset->model = 8939;
								break;
							default:
								cpuinfo_log_warning(
									"system reported invalid %" PRIu32
									"-core MSM%" PRIu32 " chipset",
									cores,
									chipset->model);
								chipset->model = 0;
						}
						break;
					case 8937:
						/* Common bug: MSM8917
						 * (Quad-core) reported as
						 * MSM8937 (Octa-core)
						 */
						switch (cores) {
							case 4:
								cpuinfo_log_info(
									"reinterpreted MSM8937 chipset with 4 cores as MSM8917");
								chipset->model = 8917;
								break;
							case 8:
								break;
							default:
								cpuinfo_log_warning(
									"system reported invalid %" PRIu32
									"-core MSM%" PRIu32 " chipset",
									cores,
									chipset->model);
								chipset->model = 0;
						}
						break;
					case 8960:
						/* Common bug: APQ8064
						 * (Quad-core) reported as
						 * MSM8960 (Dual-core)
						 */
						switch (cores) {
							case 2:
								break;
							case 4:
								cpuinfo_log_info(
									"reinterpreted MSM8960 chipset with 4 cores as APQ8064");
								chipset->series =
									cpuinfo_arm_chipset_series_qualcomm_apq;
								chipset->model = 8064;
								break;
							default:
								cpuinfo_log_warning(
									"system reported invalid %" PRIu32
									"-core MSM%" PRIu32 " chipset",
									cores,
									chipset->model);
								chipset->model = 0;
						}
						break;
					case 8996:
						/* Common bug: MSM8994
						 * (Octa-core) reported as
						 * MSM8996 (Quad-core)
						 */
						switch (cores) {
							case 4:
								break;
							case 8:
								cpuinfo_log_info(
									"reinterpreted MSM8996 chipset with 8 cores as MSM8994");
								chipset->model = 8994;
								break;
							default:
								cpuinfo_log_warning(
									"system reported invalid %" PRIu32
									"-core MSM%" PRIu32 " chipset",
									cores,
									chipset->model);
								chipset->model = 0;
						}
						break;
#if CPUINFO_ARCH_ARM
					case 8610:
						/* Common bug: MSM8612
						 * (Quad-core) reported as
						 * MSM8610 (Dual-core)
						 */
						switch (cores) {
							case 2:
								break;
							case 4:
								cpuinfo_log_info(
									"reinterpreted MSM8610 chipset with 4 cores as MSM8612");
								chipset->model = 8612;
								break;
							default:
								cpuinfo_log_warning(
									"system reported invalid %" PRIu32
									"-core MSM%" PRIu32 " chipset",
									cores,
									chipset->model);
								chipset->model = 0;
						}
						break;
#endif /* CPUINFO_ARCH_ARM */
				}
			} else {
				/* Suffix may need correction */
				const uint32_t suffix_word = load_u32le(chipset->suffix);
				if (suffix_word == UINT32_C(0x004D534D) /* "\0MSM" = reverse("MSM\0") */) {
					/*
					 * Common bug: model name repeated
					 * twice, e.g. "MSM8916MSM8916" In this
					 * case, model matching code parses the
					 * second "MSM" as a suffix
					 */
					chipset->suffix[0] = 0;
					chipset->suffix[1] = 0;
					chipset->suffix[2] = 0;
				} else {
					switch (chipset->model) {
						case 8976:
							/* MSM8976SG ->
							 * MSM8976PRO */
							if (suffix_word ==
							    UINT32_C(0x00004753) /* "\0\0GS" = reverse("SG\0\0") */) {
								chipset->suffix[0] = 'P';
								chipset->suffix[1] = 'R';
								chipset->suffix[2] = 'O';
							}
							break;
						case 8996:
							/* MSM8996PRO ->
							 * MSM8996PRO-AB or
							 * MSM8996PRO-AC */
							if (suffix_word ==
							    UINT32_C(0x004F5250) /* "\0ORP" = reverse("PRO\0") */) {
								chipset->suffix[3] = '-';
								chipset->suffix[4] = 'A';
								chipset->suffix[5] =
									'B' + (char)(max_cpu_freq_max >= 2188800);
							}
							break;
					}
				}
			}
			break;
		case cpuinfo_arm_chipset_series_qualcomm_apq: {
			/* Suffix may need correction */
			const uint32_t expected_apq = load_u32le(chipset->suffix);
			if (expected_apq == UINT32_C(0x00515041) /* "\0QPA" = reverse("APQ\0") */) {
				/*
				 * Common bug: model name repeated twice, e.g.
				 * "APQ8016APQ8016" In this case, model matching
				 * code parses the second "APQ" as a suffix
				 */
				chipset->suffix[0] = 0;
				chipset->suffix[1] = 0;
				chipset->suffix[2] = 0;
			}
			break;
		}
		case cpuinfo_arm_chipset_series_samsung_exynos:
			switch (chipset->model) {
#if CPUINFO_ARCH_ARM
				case 4410:
					/* Exynos 4410 was renamed to Exynos
					 * 4412 */
					chipset->model = 4412;
					break;
				case 5420:
					/* Common bug: Exynos 5260 (Hexa-core)
					 * reported as Exynos 5420 (Quad-core)
					 */
					switch (cores) {
						case 4:
							break;
						case 6:
							cpuinfo_log_info(
								"reinterpreted Exynos 5420 chipset with 6 cores as Exynos 5260");
							chipset->model = 5260;
							break;
						default:
							cpuinfo_log_warning(
								"system reported invalid %" PRIu32
								"-core Exynos 5420 chipset",
								cores);
							chipset->model = 0;
					}
					break;
#endif /* CPUINFO_ARCH_ARM */
				case 7580:
					/* Common bug: Exynos 7578 (Quad-core)
					 * reported as Exynos 7580 (Octa-core)
					 */
					switch (cores) {
						case 4:
							cpuinfo_log_info(
								"reinterpreted Exynos 7580 chipset with 4 cores as Exynos 7578");
							chipset->model = 7578;
							break;
						case 8:
							break;
						default:
							cpuinfo_log_warning(
								"system reported invalid %" PRIu32
								"-core Exynos 7580 chipset",
								cores);
							chipset->model = 0;
					}
					break;
			}
			break;
		case cpuinfo_arm_chipset_series_mediatek_mt:
			if (chipset->model == 6752) {
				/* Common bug: MT6732 (Quad-core) reported as
				 * MT6752 (Octa-core) */
				switch (cores) {
					case 4:
						cpuinfo_log_info("reinterpreted MT6752 chipset with 4 cores as MT6732");
						chipset->model = 6732;
						break;
					case 8:
						break;
					default:
						cpuinfo_log_warning(
							"system reported invalid %" PRIu32 "-core MT6752 chipset",
							cores);
						chipset->model = 0;
				}
			}
			if (chipset->suffix[0] == 'T') {
				/* Normalization: "TURBO" and "TRUBO"
				 * (apparently a typo) -> "T" */
				const uint32_t suffix_word = load_u32le(chipset->suffix + 1);
				switch (suffix_word) {
					case UINT32_C(0x4F425255): /* "OBRU" =
								      reverse("URBO")
								    */
					case UINT32_C(0x4F425552): /* "OBUR" =
								      reverse("RUBO")
								    */
						if (chipset->suffix[5] == 0) {
							chipset->suffix[1] = 0;
							chipset->suffix[2] = 0;
							chipset->suffix[3] = 0;
							chipset->suffix[4] = 0;
						}
						break;
				}
			}
			break;
		case cpuinfo_arm_chipset_series_rockchip_rk:
			if (chipset->model == 3288) {
				/* Common bug: Rockchip RK3399 (Hexa-core)
				 * always reported as RK3288 (Quad-core) */
				switch (cores) {
					case 4:
						break;
					case 6:
						cpuinfo_log_info("reinterpreted RK3288 chipset with 6 cores as RK3399");
						chipset->model = 3399;
						break;
					default:
						cpuinfo_log_warning(
							"system reported invalid %" PRIu32 "-core RK3288 chipset",
							cores);
						chipset->model = 0;
				}
			}
			break;
		default:
			break;
	}
}

/* Map from ARM chipset vendor ID to its string representation */
static const char* chipset_vendor_string[cpuinfo_arm_chipset_vendor_max] = {
	[cpuinfo_arm_chipset_vendor_unknown] = "Unknown",
	[cpuinfo_arm_chipset_vendor_qualcomm] = "Qualcomm",
	[cpuinfo_arm_chipset_vendor_mediatek] = "MediaTek",
	[cpuinfo_arm_chipset_vendor_samsung] = "Samsung",
	[cpuinfo_arm_chipset_vendor_hisilicon] = "HiSilicon",
	[cpuinfo_arm_chipset_vendor_actions] = "Actions",
	[cpuinfo_arm_chipset_vendor_allwinner] = "Allwinner",
	[cpuinfo_arm_chipset_vendor_amlogic] = "Amlogic",
	[cpuinfo_arm_chipset_vendor_broadcom] = "Broadcom",
	[cpuinfo_arm_chipset_vendor_lg] = "LG",
	[cpuinfo_arm_chipset_vendor_leadcore] = "Leadcore",
	[cpuinfo_arm_chipset_vendor_marvell] = "Marvell",
	[cpuinfo_arm_chipset_vendor_mstar] = "MStar",
	[cpuinfo_arm_chipset_vendor_novathor] = "NovaThor",
	[cpuinfo_arm_chipset_vendor_nvidia] = "Nvidia",
	[cpuinfo_arm_chipset_vendor_pinecone] = "Pinecone",
	[cpuinfo_arm_chipset_vendor_renesas] = "Renesas",
	[cpuinfo_arm_chipset_vendor_rockchip] = "Rockchip",
	[cpuinfo_arm_chipset_vendor_spreadtrum] = "Spreadtrum",
	[cpuinfo_arm_chipset_vendor_telechips] = "Telechips",
	[cpuinfo_arm_chipset_vendor_texas_instruments] = "Texas Instruments",
	[cpuinfo_arm_chipset_vendor_unisoc] = "Unisoc",
	[cpuinfo_arm_chipset_vendor_wondermedia] = "WonderMedia",
};

/* Map from ARM chipset series ID to its string representation */
static const char* chipset_series_string[cpuinfo_arm_chipset_series_max] = {
	[cpuinfo_arm_chipset_series_unknown] = NULL,
	[cpuinfo_arm_chipset_series_qualcomm_qsd] = "QSD",
	[cpuinfo_arm_chipset_series_qualcomm_msm] = "MSM",
	[cpuinfo_arm_chipset_series_qualcomm_apq] = "APQ",
	[cpuinfo_arm_chipset_series_qualcomm_snapdragon] = "Snapdragon ",
	[cpuinfo_arm_chipset_series_mediatek_mt] = "MT",
	[cpuinfo_arm_chipset_series_samsung_exynos] = "Exynos ",
	[cpuinfo_arm_chipset_series_hisilicon_k3v] = "K3V",
	[cpuinfo_arm_chipset_series_hisilicon_hi] = "Hi",
	[cpuinfo_arm_chipset_series_hisilicon_kirin] = "Kirin ",
	[cpuinfo_arm_chipset_series_actions_atm] = "ATM",
	[cpuinfo_arm_chipset_series_allwinner_a] = "A",
	[cpuinfo_arm_chipset_series_amlogic_aml] = "AML",
	[cpuinfo_arm_chipset_series_amlogic_s] = "S",
	[cpuinfo_arm_chipset_series_broadcom_bcm] = "BCM",
	[cpuinfo_arm_chipset_series_lg_nuclun] = "Nuclun ",
	[cpuinfo_arm_chipset_series_leadcore_lc] = "LC",
	[cpuinfo_arm_chipset_series_marvell_pxa] = "PXA",
	[cpuinfo_arm_chipset_series_mstar_6a] = "6A",
	[cpuinfo_arm_chipset_series_novathor_u] = "U",
	[cpuinfo_arm_chipset_series_nvidia_tegra_t] = "Tegra T",
	[cpuinfo_arm_chipset_series_nvidia_tegra_ap] = "Tegra AP",
	[cpuinfo_arm_chipset_series_nvidia_tegra_sl] = "Tegra SL",
	[cpuinfo_arm_chipset_series_pinecone_surge_s] = "Surge S",
	[cpuinfo_arm_chipset_series_renesas_mp] = "MP",
	[cpuinfo_arm_chipset_series_rockchip_rk] = "RK",
	[cpuinfo_arm_chipset_series_spreadtrum_sc] = "SC",
	[cpuinfo_arm_chipset_series_telechips_tcc] = "TCC",
	[cpuinfo_arm_chipset_series_texas_instruments_omap] = "OMAP",
	[cpuinfo_arm_chipset_series_unisoc_t] = "T",
	[cpuinfo_arm_chipset_series_wondermedia_wm] = "WM",
};

/* Convert chipset name represented by cpuinfo_arm_chipset structure to a string
 * representation */
void cpuinfo_arm_chipset_to_string(
	const struct cpuinfo_arm_chipset chipset[restrict static 1],
	char name[restrict static CPUINFO_ARM_CHIPSET_NAME_MAX]) {
	enum cpuinfo_arm_chipset_vendor vendor = chipset->vendor;
	if (vendor >= cpuinfo_arm_chipset_vendor_max) {
		vendor = cpuinfo_arm_chipset_vendor_unknown;
	}
	enum cpuinfo_arm_chipset_series series = chipset->series;
	if (series >= cpuinfo_arm_chipset_series_max) {
		series = cpuinfo_arm_chipset_series_unknown;
	}
	const char* vendor_string = chipset_vendor_string[vendor];
	const char* series_string = chipset_series_string[series];
	const uint32_t model = chipset->model;
	if (model == 0) {
		if (series == cpuinfo_arm_chipset_series_unknown) {
			strncpy(name, vendor_string, CPUINFO_ARM_CHIPSET_NAME_MAX);
		} else {
			snprintf(name, CPUINFO_ARM_CHIPSET_NAME_MAX, "%s %s", vendor_string, series_string);
		}
	} else {
		const size_t suffix_length = strnlen(chipset->suffix, CPUINFO_ARM_CHIPSET_SUFFIX_MAX);
		snprintf(
			name,
			CPUINFO_ARM_CHIPSET_NAME_MAX,
			"%s %s%" PRIu32 "%.*s",
			vendor_string,
			series_string,
			model,
			(int)suffix_length,
			chipset->suffix);
	}
}

#if defined(__ANDROID__)
static inline struct cpuinfo_arm_chipset disambiguate_qualcomm_chipset(
	const struct cpuinfo_arm_chipset proc_cpuinfo_hardware_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_product_board_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_board_platform_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_chipname_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_hardware_chipname_chipset[restrict static 1]) {
	if (ro_hardware_chipname_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *ro_hardware_chipname_chipset;
	}
	if (ro_chipname_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *ro_chipname_chipset;
	}
	if (proc_cpuinfo_hardware_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *proc_cpuinfo_hardware_chipset;
	}
	if (ro_product_board_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *ro_product_board_chipset;
	}
	return *ro_board_platform_chipset;
}

static inline struct cpuinfo_arm_chipset disambiguate_mediatek_chipset(
	const struct cpuinfo_arm_chipset proc_cpuinfo_hardware_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_product_board_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_board_platform_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_mediatek_platform_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_chipname_chipset[restrict static 1]) {
	if (ro_chipname_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *ro_chipname_chipset;
	}
	if (proc_cpuinfo_hardware_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *proc_cpuinfo_hardware_chipset;
	}
	if (ro_product_board_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *ro_product_board_chipset;
	}
	if (ro_board_platform_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *ro_board_platform_chipset;
	}
	return *ro_mediatek_platform_chipset;
}

static inline struct cpuinfo_arm_chipset disambiguate_hisilicon_chipset(
	const struct cpuinfo_arm_chipset proc_cpuinfo_hardware_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_product_board_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_board_platform_chipset[restrict static 1]) {
	if (proc_cpuinfo_hardware_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *proc_cpuinfo_hardware_chipset;
	}
	if (ro_product_board_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *ro_product_board_chipset;
	}
	return *ro_board_platform_chipset;
}

static inline struct cpuinfo_arm_chipset disambiguate_amlogic_chipset(
	const struct cpuinfo_arm_chipset proc_cpuinfo_hardware_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_board_platform_chipset[restrict static 1]) {
	if (proc_cpuinfo_hardware_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *proc_cpuinfo_hardware_chipset;
	}
	return *ro_board_platform_chipset;
}

static inline struct cpuinfo_arm_chipset disambiguate_marvell_chipset(
	const struct cpuinfo_arm_chipset proc_cpuinfo_hardware_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_product_board_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_chipname_chipset[restrict static 1]) {
	if (ro_chipname_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *ro_chipname_chipset;
	}
	if (ro_product_board_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *ro_product_board_chipset;
	}
	return *proc_cpuinfo_hardware_chipset;
}

static inline struct cpuinfo_arm_chipset disambiguate_rockchip_chipset(
	const struct cpuinfo_arm_chipset proc_cpuinfo_hardware_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_product_board_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_board_platform_chipset[restrict static 1]) {
	if (ro_product_board_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *ro_product_board_chipset;
	}
	if (proc_cpuinfo_hardware_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *proc_cpuinfo_hardware_chipset;
	}
	return *ro_board_platform_chipset;
}

static inline struct cpuinfo_arm_chipset disambiguate_spreadtrum_chipset(
	const struct cpuinfo_arm_chipset proc_cpuinfo_hardware_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_product_board_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_board_platform_chipset[restrict static 1],
	const struct cpuinfo_arm_chipset ro_chipname_chipset[restrict static 1]) {
	if (ro_chipname_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *ro_chipname_chipset;
	}
	if (ro_product_board_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *ro_product_board_chipset;
	}
	if (proc_cpuinfo_hardware_chipset->series != cpuinfo_arm_chipset_series_unknown) {
		return *proc_cpuinfo_hardware_chipset;
	}
	return *ro_board_platform_chipset;
}

/*
 * Decodes chipset name from Android system properties:
 * - /proc/cpuinfo Hardware string
 * - ro.product.board
 * - ro.board.platform
 * - ro.mediatek.platform
 * - ro.chipname
 * For some chipsets, the function relies frequency and on number of cores for
 * chipset detection.
 *
 * @param[in] properties - structure with the Android system properties
 * described above.
 * @param cores - number of cores in the chipset.
 * @param max_cpu_freq_max - maximum of
 * /sys/devices/system/cpu/cpu<number>/cpofreq/cpu_freq_max values.
 *
 * @returns Decoded chipset name. If chipset could not be decoded, the resulting
 * structure would use `unknown` vendor and series identifiers.
 */
struct cpuinfo_arm_chipset cpuinfo_arm_android_decode_chipset(
	const struct cpuinfo_android_properties properties[restrict static 1],
	uint32_t cores,
	uint32_t max_cpu_freq_max) {
	struct cpuinfo_arm_chipset chipset = {
		.vendor = cpuinfo_arm_chipset_vendor_unknown,
		.series = cpuinfo_arm_chipset_series_unknown,
	};

	const bool tegra_platform = is_tegra(
		properties->ro_board_platform,
		properties->ro_board_platform + strnlen(properties->ro_board_platform, CPUINFO_BUILD_PROP_VALUE_MAX));

	struct cpuinfo_arm_chipset chipsets[cpuinfo_android_chipset_property_max] = {
		[cpuinfo_android_chipset_property_proc_cpuinfo_hardware] =
			cpuinfo_arm_linux_decode_chipset_from_proc_cpuinfo_hardware(
				properties->proc_cpuinfo_hardware, cores, max_cpu_freq_max, tegra_platform),
		[cpuinfo_android_chipset_property_ro_product_board] =
			cpuinfo_arm_android_decode_chipset_from_ro_product_board(
				properties->ro_product_board, cores, max_cpu_freq_max),
		[cpuinfo_android_chipset_property_ro_board_platform] =
			cpuinfo_arm_android_decode_chipset_from_ro_board_platform(
				properties->ro_board_platform, cores, max_cpu_freq_max),
		[cpuinfo_android_chipset_property_ro_mediatek_platform] =
			cpuinfo_arm_android_decode_chipset_from_ro_mediatek_platform(properties->ro_mediatek_platform),
		[cpuinfo_android_chipset_property_ro_arch] =
			cpuinfo_arm_android_decode_chipset_from_ro_arch(properties->ro_arch),
		[cpuinfo_android_chipset_property_ro_chipname] =
			cpuinfo_arm_android_decode_chipset_from_ro_chipname(properties->ro_chipname),
		[cpuinfo_android_chipset_property_ro_hardware_chipname] =
			cpuinfo_arm_android_decode_chipset_from_ro_chipname(properties->ro_hardware_chipname),
	};
	enum cpuinfo_arm_chipset_vendor vendor = cpuinfo_arm_chipset_vendor_unknown;
	for (size_t i = 0; i < cpuinfo_android_chipset_property_max; i++) {
		const enum cpuinfo_arm_chipset_vendor decoded_vendor = chipsets[i].vendor;
		if (decoded_vendor != cpuinfo_arm_chipset_vendor_unknown) {
			if (vendor == cpuinfo_arm_chipset_vendor_unknown) {
				vendor = decoded_vendor;
			} else if (vendor != decoded_vendor) {
				/* Parsing different system properties produces
				 * different chipset vendors. This situation is
				 * rare. */
				cpuinfo_log_error(
					"chipset detection failed: different chipset vendors reported in different system properties");
				goto finish;
			}
		}
	}
	if (vendor == cpuinfo_arm_chipset_vendor_unknown) {
		cpuinfo_log_warning("chipset detection failed: none of the system properties matched known signatures");
		goto finish;
	}

	/* Fix common bugs in reported chipsets */
	for (size_t i = 0; i < cpuinfo_android_chipset_property_max; i++) {
		cpuinfo_arm_fixup_chipset(&chipsets[i], cores, max_cpu_freq_max);
	}

	/*
	 * Propagate suffixes: consider all pairs of chipsets, if both chipsets
	 * in the pair are from the same series, and one's suffix is a prefix of
	 * another's chipset suffix, use the longest suffix.
	 */
	for (size_t i = 0; i < cpuinfo_android_chipset_property_max; i++) {
		const size_t chipset_i_suffix_length = strnlen(chipsets[i].suffix, CPUINFO_ARM_CHIPSET_SUFFIX_MAX);
		for (size_t j = 0; j < i; j++) {
			if (chipsets[i].series == chipsets[j].series) {
				const size_t chipset_j_suffix_length =
					strnlen(chipsets[j].suffix, CPUINFO_ARM_CHIPSET_SUFFIX_MAX);
				if (chipset_i_suffix_length != chipset_j_suffix_length) {
					const size_t common_prefix_length =
						(chipset_i_suffix_length < chipset_j_suffix_length)
						? chipset_i_suffix_length
						: chipset_j_suffix_length;
					if (common_prefix_length == 0 ||
					    memcmp(chipsets[i].suffix, chipsets[j].suffix, common_prefix_length) == 0) {
						if (chipset_i_suffix_length > chipset_j_suffix_length) {
							memcpy(chipsets[j].suffix,
							       chipsets[i].suffix,
							       chipset_i_suffix_length);
						} else {
							memcpy(chipsets[i].suffix,
							       chipsets[j].suffix,
							       chipset_j_suffix_length);
						}
					}
				}
			}
		}
	}

	for (size_t i = 0; i < cpuinfo_android_chipset_property_max; i++) {
		if (chipsets[i].series != cpuinfo_arm_chipset_series_unknown) {
			if (chipset.series == cpuinfo_arm_chipset_series_unknown) {
				chipset = chipsets[i];
			} else if (
				chipsets[i].series != chipset.series || chipsets[i].model != chipset.model ||
				strncmp(chipsets[i].suffix, chipset.suffix, CPUINFO_ARM_CHIPSET_SUFFIX_MAX) != 0) {
				cpuinfo_log_info(
					"different chipsets reported in different system properties; "
					"vendor-specific disambiguation heuristic would be used");
				switch (vendor) {
					case cpuinfo_arm_chipset_vendor_qualcomm:
						return disambiguate_qualcomm_chipset(
							&chipsets
								[cpuinfo_android_chipset_property_proc_cpuinfo_hardware],
							&chipsets[cpuinfo_android_chipset_property_ro_product_board],
							&chipsets[cpuinfo_android_chipset_property_ro_board_platform],
							&chipsets[cpuinfo_android_chipset_property_ro_chipname],
							&chipsets
								[cpuinfo_android_chipset_property_ro_hardware_chipname]);
					case cpuinfo_arm_chipset_vendor_mediatek:
						return disambiguate_mediatek_chipset(
							&chipsets
								[cpuinfo_android_chipset_property_proc_cpuinfo_hardware],
							&chipsets[cpuinfo_android_chipset_property_ro_product_board],
							&chipsets[cpuinfo_android_chipset_property_ro_board_platform],
							&chipsets
								[cpuinfo_android_chipset_property_ro_mediatek_platform],
							&chipsets[cpuinfo_android_chipset_property_ro_chipname]);
					case cpuinfo_arm_chipset_vendor_hisilicon:
						return disambiguate_hisilicon_chipset(
							&chipsets
								[cpuinfo_android_chipset_property_proc_cpuinfo_hardware],
							&chipsets[cpuinfo_android_chipset_property_ro_product_board],
							&chipsets[cpuinfo_android_chipset_property_ro_board_platform]);
					case cpuinfo_arm_chipset_vendor_amlogic:
						return disambiguate_amlogic_chipset(
							&chipsets
								[cpuinfo_android_chipset_property_proc_cpuinfo_hardware],
							&chipsets[cpuinfo_android_chipset_property_ro_board_platform]);
					case cpuinfo_arm_chipset_vendor_marvell:
						return disambiguate_marvell_chipset(
							&chipsets
								[cpuinfo_android_chipset_property_proc_cpuinfo_hardware],
							&chipsets[cpuinfo_android_chipset_property_ro_product_board],
							&chipsets[cpuinfo_android_chipset_property_ro_chipname]);
					case cpuinfo_arm_chipset_vendor_rockchip:
						return disambiguate_rockchip_chipset(
							&chipsets
								[cpuinfo_android_chipset_property_proc_cpuinfo_hardware],
							&chipsets[cpuinfo_android_chipset_property_ro_product_board],
							&chipsets[cpuinfo_android_chipset_property_ro_board_platform]);
					case cpuinfo_arm_chipset_vendor_spreadtrum:
						return disambiguate_spreadtrum_chipset(
							&chipsets
								[cpuinfo_android_chipset_property_proc_cpuinfo_hardware],
							&chipsets[cpuinfo_android_chipset_property_ro_product_board],
							&chipsets[cpuinfo_android_chipset_property_ro_board_platform],
							&chipsets[cpuinfo_android_chipset_property_ro_chipname]);
					default:
						cpuinfo_log_error(
							"chipset detection failed: "
							"could not disambiguate different chipsets reported in different system properties");
						/* chipset variable contains
						 * valid, but inconsistent
						 * chipset information,
						 * overwrite it */
						chipset = (struct cpuinfo_arm_chipset){
							.vendor = cpuinfo_arm_chipset_vendor_unknown,
							.series = cpuinfo_arm_chipset_series_unknown,
						};
						goto finish;
				}
			}
		}
	}

finish:
	return chipset;
}
#else /* !defined(__ANDROID__) */
/*
 * Fix commonly misreported Broadcom BCM models on Raspberry Pi boards.
 *
 * @param[in,out] chipset - chipset name to fix.
 * @param[in] revision - /proc/cpuinfo Revision string.
 */
void cpuinfo_arm_fixup_raspberry_pi_chipset(
	struct cpuinfo_arm_chipset chipset[restrict static 1],
	const char revision[restrict static CPUINFO_REVISION_VALUE_MAX]) {
	const size_t revision_length = strnlen(revision, CPUINFO_REVISION_VALUE_MAX);

/* Parse revision codes according to
 * https://www.raspberrypi.org/documentation/hardware/raspberrypi/revision-codes/README.md
 */
#if CPUINFO_ARCH_ARM
	if (revision_length == 4) {
		/*
		 * Old-style revision codes.
		 * All Raspberry Pi models with old-style revision code use
		 * Broadcom BCM2835.
		 */

		/* BCM2835 often misreported as BCM2708 */
		if (chipset->model == 2708) {
			chipset->model = 2835;
		}
		return;
	}
#endif
	if ((size_t)(revision_length - 5) <= (size_t)(8 - 5) /* 5 <= length(revision) <= 8 */) {
		/* New-style revision codes */

		uint32_t model = 0;
		switch (revision[revision_length - 4]) {
			case '0':
				/* BCM2835 */
				model = 2835;
				break;
			case '1':
				/* BCM2836 */
				model = 2836;
				break;
			case '2':
				/* BCM2837 */
				model = 2837;
				break;
			case '3':
				/* BCM2711 */
				model = 2711;
				break;
		}

		if (model != 0) {
			chipset->model = model;
			chipset->suffix[0] = 0;
		}
	}
}

/*
 * Decodes chipset name from /proc/cpuinfo Hardware string.
 * For some chipsets, the function relies frequency and on number of cores for
 * chipset detection.
 *
 * @param[in] hardware - /proc/cpuinfo Hardware string.
 * @param cores - number of cores in the chipset.
 * @param max_cpu_freq_max - maximum of
 * /sys/devices/system/cpu/cpu<number>/cpofreq/cpu_freq_max values.
 *
 * @returns Decoded chipset name. If chipset could not be decoded, the resulting
 * structure would use `unknown` vendor and series identifiers.
 */
struct cpuinfo_arm_chipset cpuinfo_arm_linux_decode_chipset(
	const char hardware[restrict static CPUINFO_HARDWARE_VALUE_MAX],
	const char revision[restrict static CPUINFO_REVISION_VALUE_MAX],
	uint32_t cores,
	uint32_t max_cpu_freq_max) {
	struct cpuinfo_arm_chipset chipset =
		cpuinfo_arm_linux_decode_chipset_from_proc_cpuinfo_hardware(hardware, cores, max_cpu_freq_max, false);
	if (chipset.vendor == cpuinfo_arm_chipset_vendor_unknown) {
		cpuinfo_log_warning(
			"chipset detection failed: /proc/cpuinfo Hardware string did not match known signatures");
	} else if (chipset.vendor == cpuinfo_arm_chipset_vendor_broadcom) {
		/* Raspberry Pi kernel reports bogus chipset models; detect
		 * chipset from RPi revision */
		cpuinfo_arm_fixup_raspberry_pi_chipset(&chipset, revision);
	} else {
		cpuinfo_arm_fixup_chipset(&chipset, cores, max_cpu_freq_max);
	}
	return chipset;
}

#endif
