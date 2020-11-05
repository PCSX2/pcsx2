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
#include <stdlib.h>

//#include <winsock2.h>
#include "..\DEV9.h"
#include "AppConfig.h"

BOOL WritePrivateProfileInt(LPCSTR lpAppName, LPCSTR lpKeyName, int intvar, LPCSTR lpFileName)
{
	return WritePrivateProfileStringA(lpAppName, lpKeyName, std::to_string(intvar).c_str(), lpFileName);
}
bool FileExists(std::string szPath)
{
	DWORD dwAttrib = GetFileAttributesA(szPath.c_str());
	return (dwAttrib != INVALID_FILE_ATTRIBUTES &&
			!(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

void SaveConf()
{
	const std::string file(GetSettingsFolder().Combine(wxString("DEV9.cfg")).GetFullPath());
	DeleteFileA(file.c_str());

	WritePrivateProfileStringA("DEV9", "Eth", config.Eth, file.c_str());
	WritePrivateProfileStringA("DEV9", "Hdd", config.Hdd, file.c_str());
	WritePrivateProfileInt("DEV9", "HddSize", config.HddSize, file.c_str());
	WritePrivateProfileInt("DEV9", "ethEnable", config.ethEnable, file.c_str());
	WritePrivateProfileInt("DEV9", "hddEnable", config.hddEnable, file.c_str());
}

void LoadConf()
{
	const std::string file(GetSettingsFolder().Combine(wxString("DEV9.cfg")).GetFullPath());
	if (FileExists(file.c_str()) == false)
		return;

	GetPrivateProfileStringA("DEV9", "Eth", ETH_DEF, config.Eth, sizeof(config.Eth), file.c_str());
	GetPrivateProfileStringA("DEV9", "Hdd", HDD_DEF, config.Hdd, sizeof(config.Hdd), file.c_str());
	config.HddSize = GetPrivateProfileIntA("DEV9", "HddSize", config.HddSize, file.c_str());
	config.ethEnable = GetPrivateProfileIntA("DEV9", "ethEnable", config.ethEnable, file.c_str());
	config.hddEnable = GetPrivateProfileIntA("DEV9", "hddEnable", config.hddEnable, file.c_str());
}
