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
#include "osdebugout.h"
#include "deviceproxy.h"
#include "configuration.h"
#include "shared/inifile.h"
#include "platcompat.h"
#include <map>
#include <vector>

std::map<std::pair<int, std::string>, std::string> changedAPIs;
const TCHAR* iniFile = TEXT("USBqemu-wheel.ini");
static TSTDSTRING usb_path;
TSTDSTRING IniPath = TSTDSTRING(TEXT("./inis/")) + iniFile; // default path, just in case
TSTDSTRING LogDir;
CIniFile ciniFile;

void USBsetSettingsDir(const char* dir)
{
#ifdef _WIN32
	IniPath = str_to_wstr(dir);
#else
	IniPath = dir;
#endif
	IniPath.append(iniFile);
}

void USBsetLogDir(const char* dir)
{
#ifdef _WIN32
	LogDir = str_to_wstr(dir);
#else
	LogDir = dir;
#endif
}

std::string GetSelectedAPI(const std::pair<int, std::string>& pair)
{
	auto it = changedAPIs.find(pair);
	if (it != changedAPIs.end())
		return it->second;
	return std::string();
}

bool LoadSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, TSTDSTRING& value)
{
	CIniKey* key;
#ifdef _WIN32
	auto sect = ciniFile.GetSection(section);
	if (sect && (key = sect->GetKey(param)))
	{
		value = key->GetValue();
		return true;
	}
#else
	auto sect = ciniFile.GetSection(str_to_wstr(section));
	if (sect && (key = sect->GetKey(str_to_wstr(param))))
	{
		value = wstr_to_str(key->GetValue());
		return true;
	}
#endif
	return false;
}

bool LoadSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, int32_t& value)
{
	CIniKey* key;
#ifdef _WIN32
	auto sect = ciniFile.GetSection(section);
	if (sect && (key = sect->GetKey(param)))
	{
		try
		{
			value = std::stoi(key->GetValue());
			return true;
		}
#else
	auto sect = ciniFile.GetSection(str_to_wstr(section));
	if (sect && (key = sect->GetKey(str_to_wstr(param))))
	{
		try
		{
			value = std::stoi(key->GetValue());
			return true;
		}
#endif
		catch (std::exception& err)
		{
			OSDebugOut(TEXT("%" SFMTs "\n"), err.what());
		}
	}
	return false;
}

bool SaveSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, const TSTDSTRING& value)
{
#ifdef _WIN32
	ciniFile.SetKeyValue(section, param, value);
#else
	ciniFile.SetKeyValue(str_to_wstr(section), str_to_wstr(param), str_to_wstr(value));
#endif
	return true;
}

bool SaveSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, int32_t value)
{
#ifdef _WIN32
	ciniFile.SetKeyValue(section, param, TSTDTOSTRING(value));
#else
	ciniFile.SetKeyValue(str_to_wstr(section), str_to_wstr(param), str_to_wstr(TSTDTOSTRING(value)));
#endif
	return true;
}

void SaveConfig()
{

#ifdef _WIN32
	SaveSetting(L"MAIN", L"log", conf.Log);
#else
	SaveSetting("MAIN", "log", conf.Log);
#endif

	SaveSetting(nullptr, 0, N_DEVICE_PORT, N_DEVICE, str_to_wstr(conf.Port[0]));
	SaveSetting(nullptr, 1, N_DEVICE_PORT, N_DEVICE, str_to_wstr(conf.Port[1]));

	SaveSetting(nullptr, 0, N_DEVICE_PORT, N_WHEEL_TYPE, conf.WheelType[0]);
	SaveSetting(nullptr, 1, N_DEVICE_PORT, N_WHEEL_TYPE, conf.WheelType[1]);

	for (auto& k : changedAPIs)
	{
		SaveSetting(nullptr, k.first.first, k.first.second, N_DEVICE_API, str_to_wstr(k.second));
	}

#ifdef _WIN32
	bool ret = ciniFile.Save(IniPath);
	OSDebugOut(_T("ciniFile.Save: %d [%s]\n"), ret, IniPath.c_str());
#else
	bool ret = ciniFile.Save(str_to_wstr(IniPath));
	OSDebugOut(_T("ciniFile.Save: %d [%s]\n"), ret, IniPath.c_str());
#endif
}

void LoadConfig()
{
	std::cerr << "USB load config\n"
			  << std::endl;

#ifdef _WIN32
	ciniFile.Load(IniPath);
	LoadSetting(L"MAIN", L"log", conf.Log);
#else
	ciniFile.Load(str_to_wstr(IniPath));
	LoadSetting("MAIN", "log", conf.Log);
#endif

	LoadSetting(nullptr, 0, N_DEVICE_PORT, N_DEVICE, str_to_wstr(conf.Port[0]));
	LoadSetting(nullptr, 1, N_DEVICE_PORT, N_DEVICE, str_to_wstr(conf.Port[1]));

	LoadSetting(nullptr, 0, N_DEVICE_PORT, N_WHEEL_TYPE, conf.WheelType[0]);
	LoadSetting(nullptr, 1, N_DEVICE_PORT, N_WHEEL_TYPE, conf.WheelType[1]);

	auto& instance = RegisterDevice::instance();

	for (int i = 0; i < 2; i++)
	{
		std::string api;
		LoadSetting(nullptr, i, conf.Port[i], N_DEVICE_API, str_to_wstr(api));
		auto dev = instance.Device(conf.Port[i]);

		if (dev)
		{
			OSDebugOut(_T("Checking device '%" SFMTs "' api: '%" SFMTs "'...\n"), conf.Port[i].c_str(), api.c_str());
			if (!dev->IsValidAPI(api))
			{
				api = "<invalid>";
				const auto& apis = dev->ListAPIs();
				if (!apis.empty())
					api = *apis.begin();

				OSDebugOut(_T("Invalid! Defaulting to '%" SFMTs "'\n"), api.c_str());
			}
			else
				OSDebugOut(_T("API OK\n"));
		}

		if (api.size())
			changedAPIs[std::make_pair(i, conf.Port[i])] = api;
	}
}

void ClearSection(const TCHAR* section)
{
#ifdef _WIN32
	auto s = ciniFile.GetSection(section);
#else
	auto s = ciniFile.GetSection(str_to_wstr(section));
#endif
	if (s)
	{
		s->RemoveAllKeys();
	}
}

void RemoveSection(const char* dev_type, int port, const std::string& key)
{
	TSTDSTRING tkey;
	tkey.assign(key.begin(), key.end());

	TSTDSTRINGSTREAM section;
	if (dev_type)
		section << dev_type << _T(" ");
	section << tkey << _T(" ") << port;
	TSTDSTRING str = section.str();

#ifdef _WIN32
	ciniFile.RemoveSection(section.str());
#else
	ciniFile.RemoveSection(str_to_wstr(section.str()));
#endif
}
