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

#include <vector>
#include <string>
#include <map>
#include <sstream>
#include "osdebugout.h"
#include "platcompat.h"

#define RESULT_CANCELED 0
#define RESULT_OK       1
#define RESULT_FAILED   2

// freeze modes:
#define FREEZE_LOAD			0
#define FREEZE_SAVE			1
#define FREEZE_SIZE			2

// Device-level config related defines
#define S_DEVICE_API	TEXT("Device API")
#define S_WHEEL_TYPE	TEXT("Wheel type")
#define S_DEVICE_PORT0	TEXT("Port 0")
#define S_DEVICE_PORT1	TEXT("Port 1")
#define S_CONFIG_PATH	TEXT("Path")

#define N_DEVICE_API	TEXT("device_api")
#define N_DEVICES		TEXT("devices")
#define N_DEVICE		TEXT("device")
#define N_WHEEL_PT		TEXT("wheel_pt")
#define N_DEVICE_PORT0	TEXT("port_0")
#define N_DEVICE_PORT1	TEXT("port_1")
#define N_DEVICE_PORT	"port"
#define N_WHEEL_TYPE0	TEXT("wheel_type_0")
#define N_WHEEL_TYPE1	TEXT("wheel_type_1")
#define N_WHEEL_TYPE	TEXT("wheel_type")
#define N_CONFIG_PATH	TEXT("path")

#define PLAYER_TWO_PORT 0
#define PLAYER_ONE_PORT 1
#define USB_PORT PLAYER_ONE_PORT

struct Config {
  int Log;
  std::string Port[2];
  int WheelType[2];

  Config();
};

extern Config conf;
void SaveConfig();
void LoadConfig();
void ClearSection(const TCHAR* section);

extern TSTDSTRING IniPath;
extern TSTDSTRING LogDir;
extern const TCHAR* iniFile;
extern std::map<std::pair<int /*port*/, std::string /*devname*/>, std::string> changedAPIs;
std::string GetSelectedAPI(const std::pair<int /*port*/, std::string /*devname*/>& pair);

bool LoadSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, TSTDSTRING& value);
bool LoadSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, int32_t& value);

bool SaveSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, const TSTDSTRING& value);
bool SaveSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, int32_t value);

#ifdef _UNICODE
bool LoadSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, std::string& value);
bool SaveSettingValue(const TSTDSTRING& ini, const TSTDSTRING& section, const TCHAR* param, const std::string& value);
#endif

void USBsetSettingsDir( const char* dir );
void USBsetLogDir( const char* dir );

template<typename Type>
bool LoadSetting(const char* dev_type, int port, const std::string& key, const TCHAR* name, Type& var)
{
	bool ret = false;
	if (key.empty())
	{
		OSDebugOut(_T("Key is empty for '%s' on port %d\n"), name, port);
		return false;
	}

	TSTDSTRING tkey;
	tkey.assign(key.begin(), key.end());

	TSTDSTRINGSTREAM section;
	if (dev_type)
		section << dev_type << _T(" ");
	section << tkey << _T(" ") << port;
	TSTDSTRING str = section.str();

	OSDebugOut("[%s] '%s'=", str.c_str(), name);
	ret = LoadSettingValue(IniPath, str, name, var);
	if (ret)
		OSDebugOutStream_noprfx(var);
	else
		OSDebugOut_noprfx("<failed>\n");
	return ret;
}

template<typename Type>
bool LoadSetting(const TCHAR* section, const TCHAR* key, Type& var)
{
	bool ret = false;
	OSDebugOut("[%s] '%s'=", section, key);
	ret = LoadSettingValue(IniPath, section, key, var);
	if (ret)
		OSDebugOutStream_noprfx(var);
	else
		OSDebugOut_noprfx("<failed>\n");
	return ret;
}

/**
 *
 * [devices]
 * portX = pad
 *
 * [pad X]
 * api = joydev
 *
 * [joydev X]
 * button0 = 1
 * button1 = 2
 * ...
 *
 * */
template<typename Type>
bool SaveSetting(const char* dev_type, int port, const std::string& key, const TCHAR* name, const Type var)
{
	bool ret = false;
	if (key.empty())
	{
		OSDebugOut(_T("Key is empty for '%s' on port %d\n"), name, port);
		return false;
	}

	TSTDSTRING tkey;
	tkey.assign(key.begin(), key.end());

	TSTDSTRINGSTREAM section;
	if (dev_type)
		section << dev_type << _T(" ");
	section << tkey << _T(" ") << port;
	TSTDSTRING str = section.str();

	OSDebugOut(_T("[%s] '%s'="), str.c_str(), name);

	ret = SaveSettingValue(IniPath, str, name, var);
	OSDebugOutStream_noprfx(var);
	return ret;
}

template<typename Type>
bool SaveSetting(const TCHAR* section, const TCHAR* key, const Type var)
{
	bool ret = false;
	OSDebugOut(_T("[%s] '%s'="), section, key);

	ret = SaveSettingValue(IniPath, section, key, var);
	OSDebugOutStream_noprfx(var);
	return ret;
}
