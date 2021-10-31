#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

#include <cpuinfo.h>
#include <cpuinfo/common.h>
#include <x86/api.h>


/* The state of the parser to be preserved between parsing different tokens. */
struct parser_state {
	/*
	 * Pointer to the start of the previous token if it is "model".
	 * NULL if previous token is not "model".
	 */
	char* context_model;
	/*
	 * Pointer to the start of the previous token if it is a single-uppercase-letter token.
	 * NULL if previous token is anything different.
	 */
	char* context_upper_letter;
	/*
	 * Pointer to the start of the previous token if it is "Dual".
	 * NULL if previous token is not "Dual".
	 */
	char* context_dual;
	/*
	 * Pointer to the start of the previous token if it is "Core", "Dual-Core", "QuadCore", etc.
	 * NULL if previous token is anything different.
	 */
	char* context_core;
	/*
	 * Pointer to the start of the previous token if it is "Eng" or "Engineering", etc.
	 * NULL if previous token is anything different.
	 */
	char* context_engineering;
	/*
	 * Pointer to the '@' symbol in the brand string (separates frequency specification).
	 * NULL if there is no '@' symbol.
	 */
	char* frequency_separator;
	/* Indicates whether the brand string (after transformations) contains frequency. */
	bool frequency_token;
	/* Indicates whether the processor is of Xeon family (contains "Xeon" substring). */
	bool xeon;
	/* Indicates whether the processor model number was already parsed. */
	bool parsed_model_number;
	/* Indicates whether the processor is an engineering sample (contains "Engineering Sample" or "Eng Sample" substrings). */
	bool engineering_sample;
};

/** @brief	Resets information about the previous token. Keeps all other state information. */
static void reset_context(struct parser_state* state) {
	state->context_model = NULL;
	state->context_upper_letter = NULL;
	state->context_dual = NULL;
	state->context_core = NULL;
}

/**
 * @brief	Overwrites the supplied string with space characters if it exactly matches the given string.
 * @param	string	The string to be compared against other string, and erased in case of matching.
 * @param	length	The length of the two string to be compared against each other.
 * @param	target	The string to compare against.
 * @retval	true	If the two strings match and the first supplied string was erased (overwritten with space characters).
 * @retval	false	If the two strings are different and the first supplied string remained unchanged.
 */
static inline bool erase_matching(char* string, size_t length, const char* target) {
	const bool match = memcmp(string, target, length) == 0;
	if (match) {
		memset(string, ' ', length);
	}
	return match;
}

/**
 * @brief	Checks if the supplied ASCII character is an uppercase latin letter.
 * @param	character	The character to analyse.
 * @retval	true	If the supplied character is an uppercase latin letter ('A' to 'Z').
 * @retval	false	If the supplied character is anything different.
 */
static inline bool is_upper_letter(char character) {
	return (uint32_t) (character - 'A') <= (uint32_t)('Z' - 'A');
}

/**
 * @brief	Checks if the supplied ASCII character is a digit.
 * @param	character	The character to analyse.
 * @retval	true	If the supplied character is a digit ('0' to '9').
 * @retval	false	If the supplied character is anything different.
 */
static inline bool is_digit(char character) {
	return (uint32_t) (character - '0') < UINT32_C(10);
}

static inline bool is_zero_number(const char* token_start, const char* token_end) {
	for (const char* char_ptr = token_start; char_ptr != token_end; char_ptr++) {
		if (*char_ptr != '0') {
			return false;
		}
	}
	return true;
}

static inline bool is_space(const char* token_start, const char* token_end) {
	for (const char* char_ptr = token_start; char_ptr != token_end; char_ptr++) {
		if (*char_ptr != ' ') {
			return false;
		}
	}
	return true;
}

static inline bool is_number(const char* token_start, const char* token_end) {
	for (const char* char_ptr = token_start; char_ptr != token_end; char_ptr++) {
		if (!is_digit(*char_ptr)) {
			return false;
		}
	}
	return true;
}

static inline bool is_model_number(const char* token_start, const char* token_end) {
	for (const char* char_ptr = token_start + 1; char_ptr < token_end; char_ptr++) {
		if (is_digit(char_ptr[-1]) && is_digit(char_ptr[0])) {
			return true;
		}
	}
	return false;
}

static inline bool is_frequency(const char* token_start, const char* token_end) {
	const size_t token_length = (size_t) (token_end - token_start);
	if (token_length > 3 && token_end[-2] == 'H' && token_end[-1] == 'z') {
		switch (token_end[-3]) {
			case 'K':
			case 'M':
			case 'G':
				return true;
		}
	}
	return false;
}

/**
 * @warning	Input and output tokens can overlap
 */
static inline char* move_token(const char* token_start, const char* token_end, char* output_ptr) {
	const size_t token_length = (size_t) (token_end - token_start);
	memmove(output_ptr, token_start, token_length);
	return output_ptr + token_length;
}

static bool transform_token(char* token_start, char* token_end, struct parser_state* state) {
	const struct parser_state previousState = *state;
	reset_context(state);

	size_t token_length = (size_t) (token_end - token_start);

	if (state->frequency_separator != NULL) {
		if (token_start > state->frequency_separator) {
			if (state->parsed_model_number) {
				memset(token_start, ' ', token_length);
			}
		}
	}


	/* Early AMD and Cyrix processors have "tm" suffix for trademark, e.g.
	 *   "AMD-K6tm w/ multimedia extensions"
	 *   "Cyrix MediaGXtm MMXtm Enhanced"
	 */
	if (token_length > 2) {
		const char context_char = token_end[-3];
		if (is_digit(context_char) || is_upper_letter(context_char)) {
			if (erase_matching(token_end - 2, 2, "tm")) {
				token_end -= 2;
				token_length -= 2;
			}
		}
	}
	if (token_length > 4) {
		/* Some early AMD CPUs have "AMD-" at the beginning, e.g.
		 *   "AMD-K5(tm) Processor"
		 *   "AMD-K6tm w/ multimedia extensions"
		 *   "AMD-K6(tm) 3D+ Processor"
		 *   "AMD-K6(tm)-III Processor"
		 */
		if (erase_matching(token_start, 4, "AMD-")) {
			token_start += 4;
			token_length -= 4;
		}
	}
	switch (token_length) {
		case 1:
			/*
			 * On some Intel processors there is a space between the first letter of
			 * the name and the number after it, e.g.
			 *   "Intel(R) Core(TM) i7 CPU X 990  @ 3.47GHz"
			 *   "Intel(R) Core(TM) CPU Q 820  @ 1.73GHz"
			 * We want to merge these parts together, in reverse order, i.e. "X 990" -> "990X", "820" -> "820Q"
			 */
			if (is_upper_letter(token_start[0])) {
				state->context_upper_letter = token_start;
				return true;
			}
			break;
		case 2:
			/* Erase everything after "w/" in "AMD-K6tm w/ multimedia extensions" */
			if (erase_matching(token_start, token_length, "w/")) {
				return false;
			}
			/*
			 * Intel Xeon processors since Ivy Bridge use versions, e.g.
			 *   "Intel Xeon E3-1230 v2"
			 * Some processor branch strings report them as "V<N>", others report as "v<N>".
			 * Normalize the former (upper-case) to the latter (lower-case) version
			 */
			if (token_start[0] == 'V' && is_digit(token_start[1])) {
				token_start[0] = 'v';
				return true;
			}
			break;
		case 3:
			/*
			 * Erase "CPU" in brand string on Intel processors, e.g.
			 *  "Intel(R) Core(TM) i5 CPU         650  @ 3.20GHz"
			 *  "Intel(R) Xeon(R) CPU           X3210  @ 2.13GHz"
			 *  "Intel(R) Atom(TM) CPU Z2760  @ 1.80GHz"
			 */
			if (erase_matching(token_start, token_length, "CPU")) {
				return true;
			}
			/*
			 * Erase everywhing after "SOC" on AMD System-on-Chips, e.g.
			 *  "AMD GX-212JC SOC with Radeon(TM) R2E Graphics  \0"
			 */
			if (erase_matching(token_start, token_length, "SOC")) {
				return false;
			}
			/*
			 * Erase "AMD" in brand string on AMD processors, e.g.
			 *  "AMD Athlon(tm) Processor"
			 *  "AMD Engineering Sample"
			 *  "Quad-Core AMD Opteron(tm) Processor 2344 HE"
			 */
			if (erase_matching(token_start, token_length, "AMD")) {
				return true;
			}
			/*
			 * Erase "VIA" in brand string on VIA processors, e.g.
			 *   "VIA C3 Ezra"
			 *   "VIA C7-M Processor 1200MHz"
			 *   "VIA Nano L3050@1800MHz"
			 */
			if (erase_matching(token_start, token_length, "VIA")) {
				return true;
			}
			/* Erase "IDT" in brand string on early Centaur processors, e.g. "IDT WinChip 2-3D" */
			if (erase_matching(token_start, token_length, "IDT")) {
				return true;
			}
			/*
			 * Erase everything starting with "MMX" in
			 * "Cyrix MediaGXtm MMXtm Enhanced" ("tm" suffix is removed by this point)
			 */
			if (erase_matching(token_start, token_length, "MMX")) {
				return false;
			}
			/*
			 * Erase everything starting with "APU" on AMD processors, e.g.
			 *   "AMD A10-4600M APU with Radeon(tm) HD Graphics"
			 *   "AMD A10-7850K APU with Radeon(TM) R7 Graphics"
			 *   "AMD A6-6310 APU with AMD Radeon R4 Graphics"
			 */
			if (erase_matching(token_start, token_length, "APU")) {
				return false;
			}
			/*
			 * Remember to discard string if it contains "Eng Sample",
			 * e.g. "Eng Sample, ZD302046W4K43_36/30/20_2/8_A"
			 */
			if (memcmp(token_start, "Eng", token_length) == 0) {
				state->context_engineering = token_start;
			}
			break;
		case 4:
			/* Remember to erase "Dual Core" in "AMD Athlon(tm) 64 X2 Dual Core Processor 3800+" */
			if (memcmp(token_start, "Dual", token_length) == 0) {
				state->context_dual = token_start;
			}
			/* Remember if the processor is on Xeon family */
			if (memcmp(token_start, "Xeon", token_length) == 0) {
				state->xeon = true;
			}
			/* Erase "Dual Core" in "AMD Athlon(tm) 64 X2 Dual Core Processor 3800+" */
			if (previousState.context_dual != NULL) {
				if (memcmp(token_start, "Core", token_length) == 0) {
					memset(previousState.context_dual, ' ', (size_t) (token_end - previousState.context_dual));
					state->context_core = token_end;
					return true;
				}
			}
			break;
		case 5:
			/*
			 * Erase "Intel" in brand string on Intel processors, e.g.
			 *   "Intel(R) Xeon(R) CPU X3210 @ 2.13GHz"
			 *   "Intel(R) Atom(TM) CPU D2700 @ 2.13GHz"
			 *   "Genuine Intel(R) processor 800MHz"
			 */
			if (erase_matching(token_start, token_length, "Intel")) {
				return true;
			}
			/*
			 * Erase "Cyrix" in brand string on Cyrix processors, e.g.
			 *   "Cyrix MediaGXtm MMXtm Enhanced"
			 */
			if (erase_matching(token_start, token_length, "Cyrix")) {
				return true;
			}
			/*
			 * Erase everything following "Geode" (but not "Geode" token itself) on Geode processors, e.g.
			 *   "Geode(TM) Integrated Processor by AMD PCS"
			 *   "Geode(TM) Integrated Processor by National Semi"
			 */
			if (memcmp(token_start, "Geode", token_length) == 0) {
				return false;
			}
			/* Remember to erase "model unknown" in "AMD Processor model unknown" */
			if (memcmp(token_start, "model", token_length) == 0) {
				state->context_model = token_start;
				return true;
			}
			break;
		case 6:
			/*
			 * Erase everything starting with "Radeon" or "RADEON" on AMD APUs, e.g.
			 *   "A8-7670K Radeon R7, 10 Compute Cores 4C+6G"
			 *   "FX-8800P Radeon R7, 12 Compute Cores 4C+8G"
			 *   "A12-9800 RADEON R7, 12 COMPUTE CORES 4C+8G"
			 *   "A9-9410 RADEON R5, 5 COMPUTE CORES 2C+3G"
			 */
			if (erase_matching(token_start, token_length, "Radeon") || erase_matching(token_start, token_length, "RADEON")) {
				return false;
			}
			/*
			 * Erase "Mobile" when it is not part of the processor name,
			 * e.g. in "AMD Turion(tm) X2 Ultra Dual-Core Mobile ZM-82"
			 */
			if (previousState.context_core != NULL) {
				if (erase_matching(token_start, token_length, "Mobile")) {
					return true;
				}
			}
			/* Erase "family" in "Intel(R) Pentium(R) III CPU family 1266MHz" */
			if (erase_matching(token_start, token_length, "family")) {
				return true;
			}
			/* Discard the string if it contains "Engineering Sample" */
			if (previousState.context_engineering != NULL) {
				if (memcmp(token_start, "Sample", token_length) == 0) {
					state->engineering_sample = true;
					return false;
				}
			}
			break;
		case 7:
			/*
			 * Erase "Geniune" in brand string on Intel engineering samples, e.g.
			 *   "Genuine Intel(R) processor 800MHz"
			 *   "Genuine Intel(R) CPU @ 2.13GHz"
			 *   "Genuine Intel(R) CPU 0000 @ 1.73GHz"
			 */
			if (erase_matching(token_start, token_length, "Genuine")) {
				return true;
			}
			/*
			 * Erase "12-core" in brand string on AMD Threadripper, e.g.
			 *   "AMD Ryzen Threadripper 1920X 12-Core Processor"
			 */
			if (erase_matching(token_start, token_length, "12-Core")) {
				return true;
			}
			/*
			 * Erase "16-core" in brand string on AMD Threadripper, e.g.
			 *   "AMD Ryzen Threadripper 1950X 16-Core Processor"
			 */
			if (erase_matching(token_start, token_length, "16-Core")) {
				return true;
			}
			/* Erase "model unknown" in "AMD Processor model unknown" */
			if (previousState.context_model != NULL) {
				if (memcmp(token_start, "unknown", token_length) == 0) {
					memset(previousState.context_model, ' ', token_end - previousState.context_model);
					return true;
				}
			}
			/*
			 * Discard the string if it contains "Eng Sample:" or "Eng Sample," e.g.
			 *   "AMD Eng Sample, ZD302046W4K43_36/30/20_2/8_A"
			 *   "AMD Eng Sample: 2D3151A2M88E4_35/31_N"
			 */
			if (previousState.context_engineering != NULL) {
				if (memcmp(token_start, "Sample,", token_length) == 0 || memcmp(token_start, "Sample:", token_length) == 0) {
					state->engineering_sample = true;
					return false;
				}
			}
			break;
		case 8:
			/* Erase "QuadCore" in "VIA QuadCore L4700 @ 1.2+ GHz" */
			if (erase_matching(token_start, token_length, "QuadCore")) {
				state->context_core = token_end;
				return true;
			}
			/* Erase "Six-Core" in "AMD FX(tm)-6100 Six-Core Processor" */
			if (erase_matching(token_start, token_length, "Six-Core")) {
				state->context_core = token_end;
				return true;
			}
			break;
		case 9:
			if (erase_matching(token_start, token_length, "Processor")) {
				return true;
			}
			if (erase_matching(token_start, token_length, "processor")) {
				return true;
			}
			/* Erase "Dual-Core" in "Pentium(R) Dual-Core CPU T4200 @ 2.00GHz" */
			if (erase_matching(token_start, token_length, "Dual-Core")) {
				state->context_core = token_end;
				return true;
			}
			/* Erase "Quad-Core" in AMD processors, e.g.
			 *   "Quad-Core AMD Opteron(tm) Processor 2347 HE"
			 *   "AMD FX(tm)-4170 Quad-Core Processor"
			 */
			if (erase_matching(token_start, token_length, "Quad-Core")) {
				state->context_core = token_end;
				return true;
			}
			/* Erase "Transmeta" in brand string on Transmeta processors, e.g.
			 *   "Transmeta(tm) Crusoe(tm) Processor TM5800"
			 *   "Transmeta Efficeon(tm) Processor TM8000"
			 */
			if (erase_matching(token_start, token_length, "Transmeta")) {
				return true;
			}
			break;
		case 10:
			/*
			 * Erase "Eight-Core" in AMD processors, e.g.
			 *   "AMD FX(tm)-8150 Eight-Core Processor"
			 */
			if (erase_matching(token_start, token_length, "Eight-Core")) {
				state->context_core = token_end;
				return true;
			}
			break;
		case 11:
			/*
			 * Erase "Triple-Core" in AMD processors, e.g.
			 *   "AMD Phenom(tm) II N830 Triple-Core Processor"
			 *   "AMD Phenom(tm) 8650 Triple-Core Processor"
			 */
			if (erase_matching(token_start, token_length, "Triple-Core")) {
				state->context_core = token_end;
				return true;
			}
			/*
			 * Remember to discard string if it contains "Engineering Sample",
			 * e.g. "AMD Engineering Sample"
			 */
			if (memcmp(token_start, "Engineering", token_length) == 0) {
				state->context_engineering = token_start;
				return true;
			}
			break;
	}
	if (is_zero_number(token_start, token_end)) {
		memset(token_start, ' ', token_length);
		return true;
	}
	/* On some Intel processors the last letter of the name is put before the number,
	 * and an additional space it added, e.g.
	 *   "Intel(R) Core(TM) i7 CPU X 990  @ 3.47GHz"
	 *   "Intel(R) Core(TM) CPU Q 820  @ 1.73GHz"
	 *   "Intel(R) Core(TM) i5 CPU M 480  @ 2.67GHz"
	 * We fix this issue, i.e. "X 990" -> "990X", "Q 820" -> "820Q"
	 */
	if (previousState.context_upper_letter != 0) {
		/* A single letter token followed by 2-to-5 digit letter is merged together */
		switch (token_length) {
			case 2:
			case 3:
			case 4:
			case 5:
				if (is_number(token_start, token_end)) {
					/* Load the previous single-letter token */
					const char letter = *previousState.context_upper_letter;
					/* Erase the previous single-letter token */
					*previousState.context_upper_letter = ' ';
					/* Move the current token one position to the left */
					move_token(token_start, token_end, token_start - 1);
					token_start -= 1;
					/*
					 * Add the letter on the end
					 * Note: accessing token_start[-1] is safe because this is not the first token
					 */
					token_end[-1] = letter;
				}
		}
	}
	if (state->frequency_separator != NULL) {
		if (is_model_number(token_start, token_end)) {
			state->parsed_model_number = true;
		}
	}
	if (is_frequency(token_start, token_end)) {
		state->frequency_token = true;
	}
	return true;
}

uint32_t cpuinfo_x86_normalize_brand_string(
	const char raw_name[48],
	char normalized_name[48])
{
	normalized_name[0] = '\0';
	char name[48];
	memcpy(name, raw_name, sizeof(name));

	/*
	 * First find the end of the string
	 * Start search from the end because some brand strings contain zeroes in the middle
	 */
	char* name_end = &name[48];
	while (name_end[-1] == '\0') {
		/*
		 * Adject name_end by 1 position and check that we didn't reach the start of the brand string.
		 * This is possible if all characters are zero.
		 */
		if (--name_end == name) {
			/* All characters are zeros */
			return 0;
		}
	}

	struct parser_state parser_state = { 0 };

	/* Now unify all whitespace characters: replace tabs and '\0' with spaces */
	{
		bool inside_parentheses = false;
		for (char* char_ptr = name; char_ptr != name_end; char_ptr++) {
			switch (*char_ptr) {
				case '(':
					inside_parentheses = true;
					*char_ptr = ' ';
					break;
				case ')':
					inside_parentheses = false;
					*char_ptr = ' ';
					break;
				case '@':
					parser_state.frequency_separator = char_ptr;
				case '\0':
				case '\t':
					*char_ptr = ' ';
					break;
				default:
					if (inside_parentheses) {
						*char_ptr = ' ';
					}
			}
		}
	}

	/* Iterate through all tokens and erase redundant parts */
	{
		bool is_token = false;
		char* token_start;
		for (char* char_ptr = name; char_ptr != name_end; char_ptr++) {
			if (*char_ptr == ' ') {
				if (is_token) {
					is_token = false;
					if (!transform_token(token_start, char_ptr, &parser_state)) {
						name_end = char_ptr;
						break;
					}
				}
			} else {
				if (!is_token) {
					is_token = true;
					token_start = char_ptr;
				}
			}
		}
		if (is_token) {
			transform_token(token_start, name_end, &parser_state);
		}
	}

	/* If this is an engineering sample, return empty string */
	if (parser_state.engineering_sample) {
		return 0;
	}

	/* Check if there is some string before the frequency separator. */
	if (parser_state.frequency_separator != NULL) {
		if (is_space(name, parser_state.frequency_separator)) {
			/* If only frequency is available, return empty string */
			return 0;
		}
	}

	/* Compact tokens: collapse multiple spacing into one */
	{
		char* output_ptr = normalized_name;
		char* token_start;
		bool is_token = false;
		bool previous_token_ends_with_dash = true;
		bool current_token_starts_with_dash = false;
		uint32_t token_count = 1;
		for (char* char_ptr = name; char_ptr != name_end; char_ptr++) {
			const char character = *char_ptr;
			if (character == ' ') {
				if (is_token) {
					is_token = false;
					if (!current_token_starts_with_dash && !previous_token_ends_with_dash) {
						token_count += 1;
						*output_ptr++ = ' ';
					}
					output_ptr = move_token(token_start, char_ptr, output_ptr);
					/* Note: char_ptr[-1] exists because there is a token before this space */
					previous_token_ends_with_dash = (char_ptr[-1] == '-');
				}
			} else {
				if (!is_token) {
					is_token = true;
					token_start = char_ptr;
					current_token_starts_with_dash = (character == '-');
				}
			}
		}
		if (is_token) {
			if (!current_token_starts_with_dash && !previous_token_ends_with_dash) {
				token_count += 1;
				*output_ptr++ = ' ';
			}
			output_ptr = move_token(token_start, name_end, output_ptr);
		}
		if (parser_state.frequency_token && token_count <= 1) {
			/* The only remaining part is frequency */
			normalized_name[0] = '\0';
			return 0;
		}
		if (output_ptr < &normalized_name[48]) {
			*output_ptr = '\0';
		} else {
			normalized_name[47] = '\0';
		}
		return (uint32_t) (output_ptr - normalized_name);
	}
}

static const char* vendor_string_map[] = {
	[cpuinfo_vendor_intel] = "Intel",
	[cpuinfo_vendor_amd] = "AMD",
	[cpuinfo_vendor_via] = "VIA",
	[cpuinfo_vendor_hygon] = "Hygon",
	[cpuinfo_vendor_rdc] = "RDC",
	[cpuinfo_vendor_dmp] = "DM&P",
	[cpuinfo_vendor_transmeta] = "Transmeta",
	[cpuinfo_vendor_cyrix] = "Cyrix",
	[cpuinfo_vendor_rise] = "Rise",
	[cpuinfo_vendor_nsc] = "NSC",
	[cpuinfo_vendor_sis] = "SiS",
	[cpuinfo_vendor_nexgen] = "NexGen",
	[cpuinfo_vendor_umc] = "UMC",
};

uint32_t cpuinfo_x86_format_package_name(
	enum cpuinfo_vendor vendor,
	const char normalized_brand_string[48],
	char package_name[CPUINFO_PACKAGE_NAME_MAX])
{
	if (normalized_brand_string[0] == '\0') {
		package_name[0] = '\0';
		return 0;
	}

	const char* vendor_string = NULL;
	if ((uint32_t) vendor < (uint32_t) CPUINFO_COUNT_OF(vendor_string_map)) {
		vendor_string = vendor_string_map[(uint32_t) vendor];
	}
	if (vendor_string == NULL) {
		strncpy(package_name, normalized_brand_string, CPUINFO_PACKAGE_NAME_MAX);
		package_name[CPUINFO_PACKAGE_NAME_MAX - 1] = '\0';
		return 0;
	} else {
		snprintf(package_name, CPUINFO_PACKAGE_NAME_MAX,
			"%s %s", vendor_string, normalized_brand_string);
		return (uint32_t) strlen(vendor_string) + 1;
	}
}
