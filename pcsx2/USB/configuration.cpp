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
#include "deviceproxy.h"
#include "configuration.h"
#include "shared/inifile_usb.h"
#include "platcompat.h"
#include "AppConfig.h"
#include <map>
#include <vector>

std::map<std::pair<int, std::string>, std::string> changedAPIs;
wxString iniFileUSB(L"USB.ini");
static TSTDSTRING usb_path;
TSTDSTRING IniPath;  // default path, just in case
TSTDSTRING LogDir;
CIniFile ciniFile;
bool USBpathSet = false;

void USBsetSettingsDir()
{
	if(!USBpathSet)
	{
#ifdef _WIN32
		IniPath = GetSettingsFolder().Combine( iniFileUSB ).GetFullPath(); // default path, just in case
#else
		IniPath = std::string(GetSettingsFolder().Combine( iniFileUSB ).GetFullPath()); // default path, just in case
#endif
		USBpathSet = true;
	}
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
	USBsetSettingsDir();	
	auto it = changedAPIs.find(pair);
	if (it != changedAPIs.end())
		return it->second;
	return std::string();
}

bool LoadSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, TSTDSTRING& value)
{
	USBsetSettingsDir();	
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
	USBsetSettingsDir();	
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
			DevCon.WriteLn("%s\n", err.what());
		}
	}
	return false;
}

bool SaveSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, const TSTDSTRING& value)
{
	USBsetSettingsDir();	
#ifdef _WIN32
	ciniFile.SetKeyValue(section, param, value);
#else
	ciniFile.SetKeyValue(str_to_wstr(section), str_to_wstr(param), str_to_wstr(value));
#endif
	return true;
}

bool SaveSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, int32_t value)
{
	USBsetSettingsDir();	
#ifdef _WIN32
	ciniFile.SetKeyValue(section, param, TSTDTOSTRING(value));
#else
	ciniFile.SetKeyValue(str_to_wstr(section), str_to_wstr(param), str_to_wstr(TSTDTOSTRING(value)));
#endif
	return true;
}

void SaveConfig()
{
	USBsetSettingsDir();	
#ifdef _WIN32
	SaveSetting(L"MAIN", L"log", conf.Log);
#else
	SaveSetting("MAIN", "log", conf.Log);
#endif

#ifdef _WIN32
	SaveSetting(nullptr, 0, N_DEVICE_PORT, N_DEVICE, str_to_wstr(conf.Port[0]));
	SaveSetting(nullptr, 1, N_DEVICE_PORT, N_DEVICE, str_to_wstr(conf.Port[1]));
#else
	SaveSetting(nullptr, 0, N_DEVICE_PORT, N_DEVICE, conf.Port[0]);
	SaveSetting(nullptr, 1, N_DEVICE_PORT, N_DEVICE, conf.Port[1]);
#endif

	SaveSetting(nullptr, 0, N_DEVICE_PORT, N_WHEEL_TYPE, conf.WheelType[0]);
	SaveSetting(nullptr, 1, N_DEVICE_PORT, N_WHEEL_TYPE, conf.WheelType[1]);

	for (auto& k : changedAPIs)
	{
#ifdef _WIN32
		SaveSetting(nullptr, k.first.first, k.first.second, N_DEVICE_API, str_to_wstr(k.second));
#else
		SaveSetting(nullptr, k.first.first, k.first.second, N_DEVICE_API, k.second);
#endif
	}

#ifdef _WIN32
	bool ret = ciniFile.Save(IniPath);
#else
	[[maybe_unused]]bool ret = ciniFile.Save(str_to_wstr(IniPath));
#endif
}

void LoadConfig()
{
	USBsetSettingsDir();

	static bool loaded = false;
	if (loaded)
		return;
	loaded = true;

#ifdef _WIN32
	ciniFile.Load(IniPath);
	LoadSetting(L"MAIN", L"log", conf.Log);
#else
	ciniFile.Load(str_to_wstr(IniPath));
	LoadSetting("MAIN", "log", conf.Log);
#endif

#ifdef _WIN32
	std::wstring tmp;
	LoadSetting(nullptr, 0, N_DEVICE_PORT, N_DEVICE, tmp);
	conf.Port[0] = wstr_to_str(tmp);
	LoadSetting(nullptr, 1, N_DEVICE_PORT, N_DEVICE, tmp);
	conf.Port[1] = wstr_to_str(tmp);
#else
	LoadSetting(nullptr, 0, N_DEVICE_PORT, N_DEVICE, conf.Port[0]);
	LoadSetting(nullptr, 1, N_DEVICE_PORT, N_DEVICE, conf.Port[1]);
#endif

	LoadSetting(nullptr, 0, N_DEVICE_PORT, N_WHEEL_TYPE, conf.WheelType[0]);
	LoadSetting(nullptr, 1, N_DEVICE_PORT, N_WHEEL_TYPE, conf.WheelType[1]);

	auto& instance = RegisterDevice::instance();

	for (int i = 0; i < 2; i++)
	{
		std::string api;
#ifdef _WIN32
		LoadSetting(nullptr, i, conf.Port[i], N_DEVICE_API, tmp);
		api = wstr_to_str(tmp);
#else
		LoadSetting(nullptr, i, conf.Port[i], N_DEVICE_API, api);
#endif
		auto dev = instance.Device(conf.Port[i]);

		if (dev)
		{
			if (!dev->IsValidAPI(api))
			{
				api = "<invalid>";
				const auto& apis = dev->ListAPIs();
				if (!apis.empty())
					api = *apis.begin();

			}
		}

		if (api.size())
			changedAPIs[std::make_pair(i, conf.Port[i])] = api;
	}
}

void ClearSection(const TCHAR* section)
{
	USBsetSettingsDir();
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
	USBsetSettingsDir();	
	TSTDSTRING tkey;
	tkey.assign(key.begin(), key.end());

	TSTDSTRINGSTREAM section;
	if (dev_type)
		section << dev_type << TEXT(" ");
	section << tkey << TEXT(" ") << port;
	TSTDSTRING str = section.str();

#ifdef _WIN32
	ciniFile.RemoveSection(section.str());
#else
	ciniFile.RemoveSection(str_to_wstr(section.str()));
#endif
}
