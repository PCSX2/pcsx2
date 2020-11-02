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

void USBsetSettingsDir( const char* dir )
{
	IniPath = dir;
	IniPath.append(iniFile);
}

void USBsetLogDir( const char* dir )
{
	LogDir = dir;
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
	CIniKey *key;
	auto sect = ciniFile.GetSection(section);
	if (sect && (key = sect->GetKey(param))) {
		value = key->GetValue();
		return true;
	}
	return false;
}

bool LoadSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, int32_t& value)
{
	CIniKey *key;
	auto sect = ciniFile.GetSection(section);
	if (sect && (key = sect->GetKey(param))) {
		try {
			value = std::stoi(key->GetValue());
			return true;
		}
		catch (std::exception& err) {
			OSDebugOut(TEXT("%" SFMTs "\n"), err.what());
		}
	}
	return false;
}

bool SaveSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, const TSTDSTRING& value)
{
	ciniFile.SetKeyValue(section, param, value);
	return true;
}

bool SaveSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, int32_t value)
{
	ciniFile.SetKeyValue(section, param, TSTDTOSTRING(value));
	return true;
}

void SaveConfig() {

	SaveSetting(_T("MAIN"), _T("log"), conf.Log);

	SaveSetting(nullptr, 0, N_DEVICE_PORT, N_DEVICE, conf.Port[0]);
	SaveSetting(nullptr, 1, N_DEVICE_PORT, N_DEVICE, conf.Port[1]);

	SaveSetting(nullptr, 0, N_DEVICE_PORT, N_WHEEL_TYPE, conf.WheelType[0]);
	SaveSetting(nullptr, 1, N_DEVICE_PORT, N_WHEEL_TYPE, conf.WheelType[1]);

	for (auto& k : changedAPIs)
	{
		SaveSetting(nullptr, k.first.first, k.first.second, N_DEVICE_API, k.second);
	}

	bool ret = ciniFile.Save(IniPath);
	OSDebugOut(_T("ciniFile.Save: %d [%s]\n"), ret, IniPath.c_str());
}

void LoadConfig() {
	std::cerr << "USB load config\n" << std::endl;
	ciniFile.Load(IniPath);

	LoadSetting(_T("MAIN"), _T("log"), conf.Log);

	LoadSetting(nullptr, 0, N_DEVICE_PORT, N_DEVICE, conf.Port[0]);
	LoadSetting(nullptr, 1, N_DEVICE_PORT, N_DEVICE, conf.Port[1]);

	LoadSetting(nullptr, 0, N_DEVICE_PORT, N_WHEEL_TYPE, conf.WheelType[0]);
	LoadSetting(nullptr, 1, N_DEVICE_PORT, N_WHEEL_TYPE, conf.WheelType[1]);

	auto& instance = RegisterDevice::instance();

	for (int i=0; i<2; i++)
	{
		std::string api;
		LoadSetting(nullptr, i, conf.Port[i], N_DEVICE_API, api);
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

		if(api.size())
			changedAPIs[std::make_pair(i, conf.Port[i])] = api;
	}
}

void ClearSection(const TCHAR* section)
{
	auto s = ciniFile.GetSection(section);
	if (s) {
		s->RemoveAllKeys();
	}
}
