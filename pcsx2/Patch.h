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

#include "Pcsx2Defs.h"
#include "SysForwardDefs.h"

#define MAX_PATCH 512
#define MAX_CHEAT 1024

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
	EXTENDED_T
};

// "place" is the first number at a pnach line (patch=<place>,...), e.g.:
// - patch=1,EE,001110e0,word,00000000 <-- place is 1
// - patch=0,EE,0010BC88,word,48468800 <-- place is 0
// In PCSX2 it indicates how/when/where the patch line should be applied. If
// place is not one of the supported values then the patch line is never applied.
// PCSX2 currently supports the following values:
// 0 - apply the patch line once on game boot/startup
// 1 - apply the patch line continuously (technically - on every vsync)
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
	PPT_CONTINUOUSLY = 1
};

typedef void PATCHTABLEFUNC( const wxString& text1, const wxString& text2 );

struct IniPatch
{
	int enabled;
	int group;
	patch_data_type type;
	patch_cpu_type cpu;
	int placetopatch;
	u32 addr;
	u64 data;
};

namespace PatchFunc
{
	PATCHTABLEFUNC comment;
	PATCHTABLEFUNC gametitle;
	PATCHTABLEFUNC patch;
	PATCHTABLEFUNC cheat;
}

extern void ResetPatchesCount();
extern void ResetCheatsCount();
extern int  LoadCheats(wxString name, const wxDirName& folderName, const wxString& friendlyName);
extern int  LoadCheatsFromZip(wxString gameCRC, const wxString& cheatsArchiveFilename);
extern void inifile_command(bool isCheat, const wxString& cmd);
extern void inifile_trim(wxString& buffer);

extern int  InitPatches(const wxString& name, const Game_Data& game);
extern int  AddPatch(int Mode, int Place, int Address, int Size, u64 data);
extern void ResetPatch(void);
extern void ApplyPatch(patch_place_type place);
extern void ApplyCheat(patch_place_type place);
extern void _ApplyPatch(IniPatch *p);

extern const IConsoleWriter *PatchesCon;
