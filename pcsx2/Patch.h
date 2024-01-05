// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

// Note about terminology:
// "patch" in pcsx2 terminology is a single pnach style patch line, e.g. patch=1,EE,001110e0,word,00000000
// Such patches can appear in several places:
// - At <CRC>.pnach files where each file could have several such patches:
//   - At the "cheats" folder
//     - UI name: "Cheats", controlled via system -> enable cheats
//   - At the "cheats_ws" folder or inside "cheats_ws.zip" (the zip also called "widescreen cheats DB")
//     - the latter is searched if the former is not found for a CRC
//     - UI name: "Widescreen hacks/patches", controlled via system -> enable widescreen patches
// - At GameIndex.yaml inside a [patches] section
//   - UI name: "Patches", controlled via system -> enable automatic game fixes
//   - note that automatic game fixes also controls automatic config changes from GameIndex.dbf (UI name: "fixes")
//

#include "Config.h"

#include <string>
#include <string_view>
#include <vector>

namespace Patch
{
	// "place" is the first number at a pnach line (patch=<place>,...), e.g.:
	// - patch=1,EE,001110e0,word,00000000 <-- place is 1
	// - patch=0,EE,0010BC88,word,48468800 <-- place is 0
	// In PCSX2 it indicates how/when/where the patch line should be applied. If
	// place is not one of the supported values then the patch line is never applied.
	// PCSX2 currently supports the following values:
	// 0 - apply the patch line once on game boot/startup
	// 1 - apply the patch line continuously (technically - on every vsync)
	// 2 - effect of 0 and 1 combined, see below
	// Note:
	// - while it may seem that a value of 1 does the same as 0, but also later
	//   continues to apply the patch on every vsync - it's not.
	//   The current (and past) behavior is that these patches are applied at different
	//   places at the code, and it's possible, depending on circumstances, that 0 patches
	//   will get applied before the first vsync and therefore earlier than 1 patches.
	// - There's no "place" value which indicates to apply both once on startup
	//   and then also continuously, however such behavior can be achieved by
	//   duplicating the line where one has a 0 place and the other has a 1 place.
	enum patch_place_type : u8
	{
		PPT_ONCE_ON_LOAD = 0,
		PPT_CONTINUOUSLY = 1,
		PPT_COMBINED_0_1 = 2,

		PPT_END_MARKER
	};

	struct PatchInfo
	{
		std::string name;
		std::string description;
		std::string author;

		std::string_view GetNamePart() const;
		std::string_view GetNameParentPart() const;
	};

	using PatchInfoList = std::vector<PatchInfo>;

	struct DynamicPatchEntry
	{
		u32 offset;
		u32 value;
	};

	struct DynamicPatch
	{
		std::vector<DynamicPatchEntry> pattern;
		std::vector<DynamicPatchEntry> replacement;
	};

	// Config sections/keys to use to enable patches.
	extern const char* PATCHES_CONFIG_SECTION;
	extern const char* CHEATS_CONFIG_SECTION;
	extern const char* PATCH_ENABLE_CONFIG_KEY;

	extern PatchInfoList GetPatchInfo(const std::string_view& serial, u32 crc, bool cheats, bool showAllCRCS, u32* num_unlabelled_patches);

	/// Returns the path to a new cheat/patch pnach for the specified serial and CRC.
	extern std::string GetPnachFilename(const std::string_view& serial, u32 crc, bool cheats);

	/// Reloads cheats/patches. If verbose is set, the number of patches loaded will be shown in the OSD.
	extern void ReloadPatches(const std::string& serial, u32 crc, bool reload_files, bool reload_enabled_list, bool verbose, bool verbose_if_changed);

	extern void UpdateActivePatches(bool reload_enabled_list, bool verbose, bool verbose_if_changed);
	extern void ApplyPatchSettingOverrides();
	extern bool ReloadPatchAffectingOptions();
	extern void UnloadPatches();

	// Functions for Dynamic EE patching.
	extern void LoadDynamicPatches(const std::vector<DynamicPatch>& patches);
	extern void ApplyDynamicPatches(u32 pc);

	// Patches the emulation memory by applying all the loaded patches with a specific place value.
	// Note: unless you know better, there's no need to check whether or not different patch sources
	// are enabled (e.g. ws patches, auto game fixes, etc) before calling ApplyLoadedPatches,
	// because on boot or on any configuration change --> all the loaded patches are invalidated,
	// and then it loads only the ones which are enabled according to the current config
	// (this happens at AppCoreThread::ApplySettings(...) )
	extern void ApplyLoadedPatches(patch_place_type place);
} // namespace Patch