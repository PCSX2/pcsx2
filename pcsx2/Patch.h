/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

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
// So, if the console title contains the following suffix: "... [2 Fixes] [1 Patches] [0 Cheats] [6 widescreen hacks]" then:
// - The 2 fixes are configuration (not patches) fixes applied from the Games DB (automatic game fixes).
// - The 1 Patch is one uncommented pnach-style patch line from the GamesDB (automatic game fixes).
// - The 0 cheats - cheats are enabled but nothing found/loaded from the "cheats" folder.
// - The 6 widescreen patches are 6 pnach-style patch lines loaded either from cheats_ws folder or from cheats_ws.zip


#include "common/Pcsx2Defs.h"
#include "SysForwardDefs.h"
#include "GameDatabase.h"
#include <string>
#include <string_view>

enum patch_cpu_type {
	NO_CPU,
	CPU_EE,
	CPU_IOP
};

enum patch_data_type {
	NO_TYPE,
	BYTE_T,
	SHORT_T,
	WORD_T,
	DOUBLE_T,
	EXTENDED_T,
	SHORT_LE_T,
	WORD_LE_T,
	DOUBLE_LE_T
};

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
enum patch_place_type {
	PPT_ONCE_ON_LOAD = 0,
	PPT_CONTINUOUSLY = 1,
	PPT_COMBINED_0_1 = 2,

	_PPT_END_MARKER
};

typedef void PATCHTABLEFUNC(const std::string_view& text1, const std::string_view& text2);

struct IniPatch
{
	int enabled;
	patch_data_type type;
	patch_cpu_type cpu;
	int placetopatch;
	u32 addr;
	u64 data;
};

namespace PatchFunc
{
	PATCHTABLEFUNC author;
	PATCHTABLEFUNC comment;
	PATCHTABLEFUNC gametitle;
	PATCHTABLEFUNC patch;
}

// The following LoadPatchesFrom* functions:
// - do not reset/unload previously loaded patches (use ForgetLoadedPatches() for that)
// - do not actually patch the emulation memory (that happens at ApplyLoadedPatches(...) )
extern int  LoadPatchesFromString(const std::string& patches);
extern int  LoadPatchesFromDir(const std::string& crc, const std::string& folder, const char* friendly_name, bool show_error_when_missing);
extern int  LoadPatchesFromZip(const std::string& crc, const u8* zip_data, size_t zip_data_size);

// Patches the emulation memory by applying all the loaded patches with a specific place value.
// Note: unless you know better, there's no need to check whether or not different patch sources
// are enabled (e.g. ws patches, auto game fixes, etc) before calling ApplyLoadedPatches,
// because on boot or on any configuration change --> all the loaded patches are invalidated,
// and then it loads only the ones which are enabled according to the current config
// (this happens at AppCoreThread::ApplySettings(...) )
extern void ApplyLoadedPatches(patch_place_type place);

// Empties the patches store ("unload" the patches) but doesn't touch the emulation memory.
// Following ApplyLoadedPatches calls will do nothing until some LoadPatchesFrom* are invoked.
extern void ForgetLoadedPatches();

extern const IConsoleWriter *PatchesCon;

#ifndef PCSX2_CORE
// Patch loading is verbose only once after the crc changes, this makes it think that the crc changed.
extern void PatchesVerboseReset();
#endif

// The following prototypes seem unused in PCSX2, but maybe part of the cheats browser?
// regardless, they don't seem to have an implementation anywhere.
// extern int  AddPatch(int Mode, int Place, int Address, int Size, u64 data);
// extern void ResetPatch(void);

// Swaps endianess of InputNum
// ex. 01020304 -> 04030201
// BitLength is length of InputNum in bits, ex. double,64  word,32  short,16
extern u64 SwapEndian(u64 InputNum, u8 BitLength);
