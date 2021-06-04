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

#ifndef CONFIG_H
#define CONFIG_H

extern const wchar_t* padTypes[numPadTypes];

struct PadConfig
{
	PadType type;
};

struct GeneralConfig
{
public:
	PadConfig padConfigs[2][4];

	int deviceSelect[2][4];

	DeviceAPI keyboardApi;
	DeviceAPI mouseApi;

	// Derived value, calculated by GetInput().
	u8 configureOnBind;
	bool bind;

	bool specialInputs[2][4];

	union
	{
		struct
		{
			u8 mouseUnfocus;
			u8 background;
			u8 multipleBinding;

			struct
			{
				u8 directInput;
				u8 xInput;
				u8 dualShock3;
			} gameApis;

			u8 multitap[2];

			u8 debug;

			u8 GH2;
		};
		u8 bools[15];
	};

	wchar_t lastSaveConfigPath[MAX_PATH + 1];
	wchar_t lastSaveConfigFileName[MAX_PATH + 1];
};

extern GeneralConfig config;

void UnloadConfigs();

int LoadSettings(int force = 0, wchar_t* file = 0);

// Refreshes the set of enabled devices.
void RefreshEnabledDevices(int updateDeviceList = 0);

void Configure();
#endif
