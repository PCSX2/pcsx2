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
#include "common/StringUtil.h"
#include "SPU2/Host/Config.h"
#include "SPU2/Host/Dialogs.h"
#include "HostSettings.h"

bool CfgReadBool(const wchar_t* Section, const wchar_t* Name, bool Default)
{
	return Host::GetBoolSettingValue(StringUtil::WideStringToUTF8String(Section).c_str(),
		StringUtil::WideStringToUTF8String(Name).c_str(), Default);
}

int CfgReadInt(const wchar_t* Section, const wchar_t* Name, int Default)
{
	return Host::GetIntSettingValue(StringUtil::WideStringToUTF8String(Section).c_str(),
		StringUtil::WideStringToUTF8String(Name).c_str(), Default);
}

float CfgReadFloat(const wchar_t* Section, const wchar_t* Name, float Default)
{
	return Host::GetFloatSettingValue(StringUtil::WideStringToUTF8String(Section).c_str(),
		StringUtil::WideStringToUTF8String(Name).c_str(), Default);
}

void CfgReadStr(const wchar_t* Section, const wchar_t* Name, wchar_t* Data, int DataSize, const wchar_t* Default)
{
	const std::wstring res(StringUtil::UTF8StringToWideString(
		Host::GetStringSettingValue(
			StringUtil::WideStringToUTF8String(Section).c_str(),
			StringUtil::WideStringToUTF8String(Name).c_str(),
			StringUtil::WideStringToUTF8String(Default).c_str())));

	std::wcsncpy(Data, res.c_str(), DataSize);
	Data[DataSize - 1] = 0;
}

void CfgReadStr(const wchar_t* Section, const wchar_t* Name, wxString& Data, const wchar_t* Default)
{
	Data = StringUtil::UTF8StringToWxString(
		Host::GetStringSettingValue(
			StringUtil::WideStringToUTF8String(Section).c_str(),
			StringUtil::WideStringToUTF8String(Name).c_str(),
			StringUtil::WideStringToUTF8String(Default).c_str()));
}

void CfgWriteBool(const wchar_t* Section, const wchar_t* Name, bool Value)
{
}

void CfgWriteInt(const wchar_t* Section, const wchar_t* Name, int Value)
{
}

void CfgWriteFloat(const wchar_t* Section, const wchar_t* Name, float Value)
{
}

void CfgWriteStr(const wchar_t* Section, const wchar_t* Name, const wxString& Data)
{
}