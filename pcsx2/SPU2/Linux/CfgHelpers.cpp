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
#include "AppConfig.h"
#include "Dialogs.h"
#include <wx/fileconf.h>

wxFileConfig* spuConfig = nullptr;
wxString path(L"SPU2.ini");
bool pathSet = false;

void initIni()
{
	if (!pathSet)
	{
		path = GetSettingsFolder().Combine(path).GetFullPath();
		pathSet = true;
	}
	if (spuConfig == nullptr)
		spuConfig = new wxFileConfig(L"", L"", path, L"", wxCONFIG_USE_LOCAL_FILE);
}

void setIni(const wchar_t* Section)
{
	initIni();
	spuConfig->SetPath(wxsFormat(L"/%s", Section));
}

void CfgSetSettingsDir(const char* dir)
{
	FileLog("CfgSetSettingsDir(%s)\n", dir);
	path = Path::Combine((dir == nullptr) ? wxString(L"inis") : wxString::FromUTF8(dir), L"SPU2.ini");
	pathSet = true;
}

void CfgWriteBool(const wchar_t* Section, const wchar_t* Name, bool Value)
{
	setIni(Section);
	spuConfig->Write(Name, Value);
}

void CfgWriteInt(const wchar_t* Section, const wchar_t* Name, int Value)
{
	setIni(Section);
	spuConfig->Write(Name, Value);
}

void CfgWriteFloat(const wchar_t* Section, const wchar_t* Name, float Value)
{
	setIni(Section);
	spuConfig->Write(Name, (double)Value);
}

void CfgWriteStr(const wchar_t* Section, const wchar_t* Name, const wxString& Data)
{
	setIni(Section);
	spuConfig->Write(Name, Data);
}

bool CfgReadBool(const wchar_t* Section, const wchar_t* Name, bool Default)
{
	bool ret;

	setIni(Section);
	spuConfig->Read(Name, &ret, Default);

	return ret;
}

int CfgReadInt(const wchar_t* Section, const wchar_t* Name, int Default)
{
	int ret;

	setIni(Section);
	spuConfig->Read(Name, &ret, Default);

	return ret;
}

float CfgReadFloat(const wchar_t* Section, const wchar_t* Name, float Default)
{
	double ret;

	setIni(Section);
	spuConfig->Read(Name, &ret, (double)Default);

	return (float)ret;
}

void CfgReadStr(const wchar_t* Section, const wchar_t* Name, wchar_t* Data, int DataSize, const wchar_t* Default)
{
	setIni(Section);
	wcscpy(Data, spuConfig->Read(Name, Default).wc_str());
}

void CfgReadStr(const wchar_t* Section, const wchar_t* Name, wxString& Data, const wchar_t* Default)
{
	setIni(Section);
	Data = spuConfig->Read(Name, Default);
}
