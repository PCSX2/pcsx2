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

#include "PrecompiledHeader.h"
#include "Config.h"
#include "SPU2/Global.h"
#include "Dialogs.h"
#include "common/StringUtil.h"
#include "gui/StringHelpers.h"

void SysMessage(const char* fmt, ...)
{
	va_list list;
	char tmp[512];
	wchar_t wtmp[512];

	va_start(list, fmt);
	vsprintf_s(tmp, fmt, list);
	va_end(list);
	swprintf_s(wtmp, L"%S", tmp);

	// TODO: Move this into app/host.
	MessageBox(NULL, wtmp, L"SPU2 System Message", MB_OK | MB_SETFOREGROUND);
}

void SysMessage(const wchar_t* fmt, ...)
{
	va_list list;
	va_start(list, fmt);
	wxString wtmp;
	wtmp.PrintfV(fmt, list);
	va_end(list);
	MessageBox(NULL, wtmp, L"SPU2 System Message", MB_OK | MB_SETFOREGROUND);
}

//////

#include "common/Path.h"

wxString CfgFile(L"SPU2.ini");
bool pathSet = false;

void initIni()
{
	if (!pathSet)
	{
		CfgFile = StringUtil::UTF8StringToWxString(Path::Combine(EmuFolders::Settings, "SPU2.ini"));
		pathSet = true;
	}
}

void CfgSetSettingsDir(const char* dir)
{
	initIni();
}


/*| Config File Format: |¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯¯*\
+--+---------------------+------------------------+
|												  |
| Option=Value									  |
|												  |
|												  |
| Boolean Values: TRUE,YES,1,T,Y mean 'true',	  |
|                 everything else means 'false'.  |
|												  |
| All Values are limited to 255 chars.			  |
|												  |
+-------------------------------------------------+
\*_____________________________________________*/


void CfgWriteBool(const TCHAR* Section, const TCHAR* Name, bool Value)
{
	initIni();
	const TCHAR* Data = Value ? L"TRUE" : L"FALSE";
	WritePrivateProfileString(Section, Name, Data, CfgFile);
}

void CfgWriteInt(const TCHAR* Section, const TCHAR* Name, int Value)
{
	initIni();
	TCHAR Data[32];
	_itow(Value, Data, 10);
	WritePrivateProfileString(Section, Name, Data, CfgFile);
}

void CfgWriteFloat(const TCHAR* Section, const TCHAR* Name, float Value)
{
	initIni();
	TCHAR Data[32];
	_swprintf(Data, L"%f", Value);
	WritePrivateProfileString(Section, Name, Data, CfgFile);
}

/*void CfgWriteStr(const TCHAR* Section, const TCHAR* Name, const TCHAR *Data)
{
WritePrivateProfileString( Section, Name, Data, CfgFile );
}*/

void CfgWriteStr(const TCHAR* Section, const TCHAR* Name, const wxString& Data)
{
	initIni();
	WritePrivateProfileString(Section, Name, Data, CfgFile);
}

/*****************************************************************************/

bool CfgReadBool(const TCHAR* Section, const TCHAR* Name, bool Default)
{
	initIni();
	TCHAR Data[255] = {0};

	GetPrivateProfileString(Section, Name, L"", Data, 255, CfgFile);
	Data[254] = 0;
	if (wcslen(Data) == 0)
	{
		CfgWriteBool(Section, Name, Default);
		return Default;
	}

	if (wcscmp(Data, L"1") == 0)
		return true;
	if (wcscmp(Data, L"Y") == 0)
		return true;
	if (wcscmp(Data, L"T") == 0)
		return true;
	if (wcscmp(Data, L"YES") == 0)
		return true;
	if (wcscmp(Data, L"TRUE") == 0)
		return true;
	return false;
}


int CfgReadInt(const TCHAR* Section, const TCHAR* Name, int Default)
{
	initIni();
	TCHAR Data[255] = {0};
	GetPrivateProfileString(Section, Name, L"", Data, 255, CfgFile);
	Data[254] = 0;

	if (wcslen(Data) == 0)
	{
		CfgWriteInt(Section, Name, Default);
		return Default;
	}

	return _wtoi(Data);
}

float CfgReadFloat(const TCHAR* Section, const TCHAR* Name, float Default)
{
	initIni();
	TCHAR Data[255] = {0};
	GetPrivateProfileString(Section, Name, L"", Data, 255, CfgFile);
	Data[254] = 0;

	if (wcslen(Data) == 0)
	{
		CfgWriteFloat(Section, Name, Default);
		return Default;
	}

	return (float)_wtof(Data);
}

void CfgReadStr(const TCHAR* Section, const TCHAR* Name, TCHAR* Data, int DataSize, const TCHAR* Default)
{
	initIni();
	GetPrivateProfileString(Section, Name, L"", Data, DataSize, CfgFile);

	if (wcslen(Data) == 0)
	{
		swprintf_s(Data, DataSize, L"%s", Default);
		CfgWriteStr(Section, Name, Data);
	}
}

void CfgReadStr(const TCHAR* Section, const TCHAR* Name, wxString& Data, const TCHAR* Default)
{
	initIni();
	wchar_t workspace[512];
	GetPrivateProfileString(Section, Name, L"", workspace, std::size(workspace), CfgFile);

	Data = workspace;

	if (Data.empty())
	{
		Data = Default;
		CfgWriteStr(Section, Name, Default);
	}
}

// Tries to read the requested value.
// Returns FALSE if the value isn't found.
bool CfgFindName(const TCHAR* Section, const TCHAR* Name)
{
	initIni();
	// Only load 24 characters.  No need to load more.
	TCHAR Data[24] = {0};
	GetPrivateProfileString(Section, Name, L"", Data, 24, CfgFile);
	Data[23] = 0;

	if (wcslen(Data) == 0)
		return false;
	return true;
}
