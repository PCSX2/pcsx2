/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#ifndef DIALOG_H_INCLUDED
#define DIALOG_H_INCLUDED

#include "SPU2/Global.h"
#include "SPU2/Config.h"

namespace DebugConfig
{
	extern void ReadSettings();
	extern void WriteSettings();
} // namespace DebugConfig

extern void CfgSetSettingsDir(const char* dir);
extern void CfgSetLogDir(const char* dir);

extern void CfgWriteBool(const wchar_t* Section, const wchar_t* Name, bool Value);
extern void CfgWriteInt(const wchar_t* Section, const wchar_t* Name, int Value);
extern void CfgWriteFloat(const wchar_t* Section, const wchar_t* Name, float Value);
extern void CfgWriteStr(const wchar_t* Section, const wchar_t* Name, const wxString& Data);

extern bool CfgReadBool(const wchar_t* Section, const wchar_t* Name, bool Default);
extern void CfgReadStr(const wchar_t* Section, const wchar_t* Name, wxString& Data, const wchar_t* Default);
//extern void		CfgReadStr(const wchar_t* Section, const wchar_t* Name, wchar_t* Data, int DataSize, const wchar_t* Default);
extern int CfgReadInt(const wchar_t* Section, const wchar_t* Name, int Default);
extern float CfgReadFloat(const wchar_t* Section, const wchar_t* Name, float Default);

#endif
