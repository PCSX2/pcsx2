/*  LilyPad - Pad plugin for PS2 Emulator
 *  Copyright (C) 2002-2014  PCSX2 Dev Team/ChickenLiver
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the
 *  terms of the GNU Lesser General Public License as published by the Free
 *  Software Found- ation, either version 3 of the License, or (at your option)
 *  any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY
 *  WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 *  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
 *  details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with PCSX2.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "Global.h"

#include "InputManager.h"
#include "Config.h"
#include "DeviceEnumerator.h"
#include "Linux/ConfigHelper.h"

GeneralConfig config;
u8 ps2e = 0;

void CALLBACK PADsetSettingsDir( const char *dir )
{
	CfgSetSettingsDir(dir);
}

int LoadSettings(int force, wchar_t *file) {
	if (dm && !force) return 0;

	// Could just do ClearDevices() instead, but if I ever add any extra stuff,
	// this will still work.
	UnloadConfigs();
	dm = new InputDeviceManager();

#ifdef _MSC_VER
	if (!file) {
		file = iniFile;
		GetPrivateProfileStringW(L"General Settings", L"Last Config Path", L"inis", config.lastSaveConfigPath, sizeof(config.lastSaveConfigPath), file);
		GetPrivateProfileStringW(L"General Settings", L"Last Config Name", L"LilyPad.lily", config.lastSaveConfigFileName, sizeof(config.lastSaveConfigFileName), file);
	}
	else {
		wchar_t *c = wcsrchr(file, '\\');
		if (c) {
			*c = 0;
			wcscpy(config.lastSaveConfigPath, file);
			wcscpy(config.lastSaveConfigFileName, c+1);
			*c = '\\';
			WritePrivateProfileStringW(L"General Settings", L"Last Config Path", config.lastSaveConfigPath, iniFile);
			WritePrivateProfileStringW(L"General Settings", L"Last Config Name", config.lastSaveConfigFileName, iniFile);
		}
	}

	for (int i=0; i<sizeof(BoolOptionsInfo)/sizeof(BoolOptionsInfo[0]); i++) {
		config.bools[i] = GetPrivateProfileBool(L"General Settings", BoolOptionsInfo[i].name, BoolOptionsInfo[i].defaultValue, file);
	}

	config.closeHacks = (u8)GetPrivateProfileIntW(L"General Settings", L"Close Hacks", 0, file);
	if (config.closeHacks&1) config.closeHacks &= ~2;

	config.keyboardApi = (DeviceAPI)GetPrivateProfileIntW(L"General Settings", L"Keyboard Mode", WM, file);
	if (!config.keyboardApi) config.keyboardApi = WM;
	config.mouseApi = (DeviceAPI) GetPrivateProfileIntW(L"General Settings", L"Mouse Mode", 0, file);

	config.volume = GetPrivateProfileInt(L"General Settings", L"Volume", 100, file);
	OSVERSIONINFO os;
	os.dwOSVersionInfoSize = sizeof(os);
	config.osVersion = 0;
	if (GetVersionEx(&os)) {
		config.osVersion = os.dwMajorVersion;
	}
	if (config.osVersion < 6) config.vistaVolume = 0;
	if (!config.vistaVolume) config.volume = 100;
	if (config.vistaVolume) SetVolume(config.volume);

	if (!InitializeRawInput()) {
		if (config.keyboardApi == RAW) config.keyboardApi = WM;
		if (config.mouseApi == RAW) config.mouseApi = WM;
	}

	if (config.debug) {
		CreateDirectory(L"logs", 0);
	}


	for (int port=0; port<2; port++) {
		for (int slot=0; slot<4; slot++) {
			wchar_t temp[50];
			wsprintf(temp, L"Pad %i %i", port, slot);
			config.padConfigs[port][slot].type = (PadType) GetPrivateProfileInt(temp, L"Mode", Dualshock2Pad, file);
			config.padConfigs[port][slot].autoAnalog = GetPrivateProfileBool(temp, L"Auto Analog", 0, file);
		}
	}

	int i=0;
	int multipleBinding = config.multipleBinding;
	// Disabling multiple binding only prevents new multiple bindings.
	config.multipleBinding = 1;
	while (1) {
		wchar_t id[50];
		wchar_t temp[50], temp2[1000], temp3[1000], temp4[1000];
		wsprintfW(id, L"Device %i", i++);
		if (!GetPrivateProfileStringW(id, L"Display Name", 0, temp2, 1000, file) || !temp2[0] ||
			!GetPrivateProfileStringW(id, L"Instance ID", 0, temp3, 1000, file) || !temp3[0]) {
			if (i >= 100) break;
			continue;
		}
		wchar_t *id2 = 0;
		if (GetPrivateProfileStringW(id, L"Product ID", 0, temp4, 1000, file) && temp4[0])
			id2 = temp4;

		int api = GetPrivateProfileIntW(id, L"API", 0, file);
		int type = GetPrivateProfileIntW(id, L"Type", 0, file);
		if (!api || !type) continue;

		Device *dev = new Device((DeviceAPI)api, (DeviceType)type, temp2, temp3, id2);
		dev->attached = 0;
		dm->AddDevice(dev);
		int j = 0;
		int last = 0;
		while (1) {
			wsprintfW(temp, L"Binding %i", j++);
			if (!GetPrivateProfileStringW(id, temp, 0, temp2, 1000, file)) {
				if (j >= 100) {
					if (!last) break;
					last = 0;
				}
				continue;
			}
			last = 1;
			unsigned int uid;
			int port, command, sensitivity, turbo, slot = 0, deadZone = 0;
			int w = 0;
			char string[1000];
			while (temp2[w]) {
				string[w] = (char)temp2[w];
				w++;
			}
			string[w] = 0;
			int len = sscanf(string, " %i , %i , %i , %i , %i , %i , %i", &uid, &port, &command, &sensitivity, &turbo, &slot, &deadZone);
			if (len >= 5 && type) {
				VirtualControl *c = dev->GetVirtualControl(uid);
				if (!c) c = dev->AddVirtualControl(uid, -1);
				if (c) {
					BindCommand(dev, uid, port, slot, command, sensitivity, turbo, deadZone);
				}
			}
		}
		j = 0;
		while (1) {
			wsprintfW(temp, L"FF Binding %i", j++);
			if (!GetPrivateProfileStringW(id, temp, 0, temp2, 1000, file)) {
				if (j >= 10) {
					if (!last) break;
					last = 0;
				}
				continue;
			}
			last = 1;
			int port, slot, motor;
			int w = 0;
			char string[1000];
			char effect[1000];
			while (temp2[w]) {
				string[w] = (char)temp2[w];
				w++;
			}
			string[w] = 0;
			// wcstok not in ntdll.  More effore than its worth to shave off
			// whitespace without it.
			if (sscanf(string, " %s %i , %i , %i", effect, &port, &motor, &slot) == 4) {
				char *s = strchr(strchr(strchr(string, ',')+1, ',')+1, ',');
				if (!s) continue;
				s++;
				w = 0;
				while (effect[w]) {
					temp2[w] = effect[w];
					w++;
				}
				temp2[w] = 0;
				ForceFeedbackEffectType *eff = dev->GetForcefeedbackEffect(temp2);
				if (!eff) {
					// At the moment, don't record effect types.
					// Only used internally, anyways, so not an issue.
					dev->AddFFEffectType(temp2, temp2, EFFECT_CONSTANT);
					// eff = &dev->ffEffectTypes[dev->numFFEffectTypes-1];
				}
				ForceFeedbackBinding *b;
				CreateEffectBinding(dev, temp2, port, slot, motor, &b);
				if (b) {
					while (1) {
						int axisID = atoi(s);
						if (!(s = strchr(s, ','))) break;
						s++;
						int force = atoi(s);
						int i;
						for (i=0; i<dev->numFFAxes; i++) {
							if (axisID == dev->ffAxes[i].id) break;
						}
						if (i == dev->numFFAxes) {
							dev->AddFFAxis(L"?", axisID);
						}
						b->axes[i].force = force;
						if (!(s = strchr(s, ','))) break;
						s++;
					}
				}
			}
		}
	}
	config.multipleBinding = multipleBinding;

	RefreshEnabledDevicesAndDisplay(1);
#endif

	return 0;
}

void UnloadConfigs() {
	if (dm) {
		delete dm;
		dm = 0;
	}
}

void RefreshEnabledDevices(int updateDeviceList) {
	// Clears all device state.
	static int lastXInputState = -1;
	if (updateDeviceList || lastXInputState != config.gameApis.xInput) {
		EnumDevices(config.gameApis.xInput);
		lastXInputState = config.gameApis.xInput;
	}

	for (int i=0; i<dm->numDevices; i++) {
		Device *dev = dm->devices[i];

		if (!dev->attached && dev->displayName[0] != '[') {
			wchar_t *newName = (wchar_t*) malloc(sizeof(wchar_t) * (wcslen(dev->displayName) + 12));
			wsprintfW(newName, L"[Detached] %s", dev->displayName);
			free(dev->displayName);
			dev->displayName = newName;
		}

		if ((dev->type == KEYBOARD && dev->api == IGNORE_KEYBOARD) ||
			(dev->type == KEYBOARD && dev->api == config.keyboardApi) ||
			(dev->type == MOUSE && dev->api == config.mouseApi) ||
			(dev->type == OTHER &&
				((dev->api == DI && config.gameApis.directInput) ||
				 (dev->api == DS3 && config.gameApis.dualShock3) ||
				 (dev->api == XINPUT && config.gameApis.xInput)))) {
					if (config.gameApis.dualShock3 && dev->api == DI && dev->displayName &&
						!wcsicmp(dev->displayName, L"DX PLAYSTATION(R)3 Controller")) {
							dm->DisableDevice(i);
					}
					else {
						dm->EnableDevice(i);
					}
		}
		else {
			dm->DisableDevice(i);
		}
	}
}

void Configure() {
	// Can end up here without PADinit() being called first.
	LoadSettings();
	// Can also end up here after running emulator a bit, and possibly
	// disabling some devices due to focus changes, or releasing mouse.
	RefreshEnabledDevices(0);
}
