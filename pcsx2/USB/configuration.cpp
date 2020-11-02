#include "osdebugout.h"
#include "deviceproxy.h"
#include "configuration.h"
#include "shared/inifile.h"
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
#ifdef _UNICODE
	OSDebugOut(L"USBsetSettingsDir: %S\n", dir);
	wchar_t dst[4096] = {0};
	size_t num = 0;
	mbstowcs_s(&num, dst, dir, countof(dst));
	IniPath = dst;
	IniPath.append(iniFile);
	OSDebugOut(L"USBsetSettingsDir: %s\n", IniPath.c_str());

#else
	IniPath = dir;
	IniPath.append(iniFile);
#endif
}

void USBsetLogDir( const char* dir )
{
#ifdef _UNICODE
	OSDebugOut(L"USBsetLogDir: %S\n", dir);
	wchar_t dst[4096] = {0};
	size_t num = 0;
	mbstowcs_s(&num, dst, dir, countof(dst));
	LogDir = dst;
	LogDir.append(_T("USBqemu-wheel.log"));
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

#ifdef _UNICODE
bool LoadSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, std::string& value)
{
	char tmpA[4096] = { 0 };
	size_t num = 0;
	std::wstring str;

	CIniKey *key;
	auto sect = ciniFile.GetSection(section);
	if (sect && (key = sect->GetKey(param))) {
		str = key->GetValue();
		wcstombs_s(&num, tmpA, str.c_str(), sizeof(tmpA)); //TODO error-check
		value = tmpA;
		return true;
	}
	return false;
}

bool SaveSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, const std::string& value)
{
	std::wstring wstr;
	wstr.assign(value.begin(), value.end());
	ciniFile.SetKeyValue(section, param, wstr);
	return true;
}
#endif

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
