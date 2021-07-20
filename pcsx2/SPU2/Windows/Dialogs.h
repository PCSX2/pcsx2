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

#pragma once

#ifdef _WIN32
#include "WinConfig.h"
#else
#include "LnxConfig.h"
#endif

namespace DebugConfig
{
	extern void ReadSettings();
	extern void WriteSettings();
	extern void OpenDialog();
	extern void EnableControls(HWND hWnd);
} // namespace DebugConfig

namespace SoundtouchCfg
{
	extern void ReadSettings();
	extern void WriteSettings();
	extern void OpenDialog(HWND hWnd);
	extern BOOL CALLBACK DialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
} // namespace SoundtouchCfg

extern int SendDialogMsg(HWND hwnd, int dlgId, UINT code, WPARAM wParam, LPARAM lParam);

extern void AssignSliderValue(HWND idcwnd, HWND hwndDisplay, int value);
extern void AssignSliderValue(HWND hWnd, int idc, int editbox, int value);
extern int GetSliderValue(HWND hWnd, int idc);
extern BOOL DoHandleScrollMessage(HWND hwndDisplay, WPARAM wParam, LPARAM lParam);

extern void CfgSetSettingsDir(const char* dir);
extern void CfgSetLogDir(const char* dir);

extern bool CfgFindName(const TCHAR* Section, const TCHAR* Name);

extern void CfgWriteBool(const TCHAR* Section, const TCHAR* Name, bool Value);
extern void CfgWriteInt(const TCHAR* Section, const TCHAR* Name, int Value);
extern void CfgWriteFloat(const TCHAR* Section, const TCHAR* Name, float Value);
extern void CfgWriteStr(const TCHAR* Section, const TCHAR* Name, const wxString& Data);

extern bool CfgReadBool(const TCHAR* Section, const TCHAR* Name, bool Default);
extern void CfgReadStr(const TCHAR* Section, const TCHAR* Name, wxString& Data, const TCHAR* Default);
extern void CfgReadStr(const TCHAR* Section, const TCHAR* Name, TCHAR* Data, int DataSize, const TCHAR* Default);
extern int CfgReadInt(const TCHAR* Section, const TCHAR* Name, int Default);
extern float CfgReadFloat(const TCHAR* Section, const TCHAR* Name, float Default);
