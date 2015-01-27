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

struct GeneralSettingsBool {
	const wchar_t *name;
	unsigned int ControlId;
	u8 defaultValue;
};

// Ties together config data structure, config files, and general config
// dialog.
const GeneralSettingsBool BoolOptionsInfo[] = {
	{L"Force Cursor Hide", 0 /*IDC_FORCE_HIDE*/, 0},
	{L"Mouse Unfocus", 0 /*IDC_MOUSE_UNFOCUS*/, 1},
	{L"Background", 0 /*IDC_BACKGROUND*/, 1},
	{L"Multiple Bindings", 0 /*IDC_MULTIPLE_BINDING*/, 0},

	{L"DirectInput Game Devices", 0 /*IDC_G_DI*/, 1},
	{L"XInput", 0 /*IDC_G_XI*/, 1},
	{L"DualShock 3", 0 /*IDC_G_DS3*/, 0},

	{L"Multitap 1", 0 /*IDC_MULTITAP1*/, 0},
	{L"Multitap 2", 0 /*IDC_MULTITAP2*/, 0},

	{L"Escape Fullscreen Hack", 0 /*IDC_ESCAPE_FULLSCREEN_HACK*/, 1},
	{L"Disable Screen Saver", 0 /*IDC_DISABLE_SCREENSAVER*/, 1},
	{L"Logging", 0 /*IDC_DEBUG_FILE*/, 0},

	{L"Save State in Title", 0 /*IDC_SAVE_STATE_TITLE*/, 0}, //No longer required, PCSX2 now handles it - avih 2011-05-17
	{L"GH2", 0 /*IDC_GH2_HACK*/, 0},
	{L"Turbo Key Hack", 0 /*IDC_TURBO_KEY_HACK*/, 0},

	{L"Vista Volume", 0 /*IDC_VISTA_VOLUME*/, 1},
};

void CALLBACK PADsetSettingsDir( const char *dir )
{
	CfgHelper::SetSettingsDir(dir);
}

int SaveSettings(wchar_t *file=0) {
	CfgHelper cfg;

	for (int i=0; i<sizeof(BoolOptionsInfo)/sizeof(BoolOptionsInfo[0]); i++) {
		 cfg.WriteBool(L"General Settings", BoolOptionsInfo[i].name, config.bools[i]);
	}
	cfg.WriteInt(L"General Settings", L"Close Hacks", config.closeHacks);

	cfg.WriteInt(L"General Settings", L"Keyboard Mode", config.keyboardApi);
	cfg.WriteInt(L"General Settings", L"Mouse Mode", config.mouseApi);

	cfg.WriteInt(L"General Settings", L"Volume", config.volume);

	for (int port=0; port<2; port++) {
		for (int slot=0; slot<4; slot++) {
			wchar_t temp[50];
			wsprintf(temp, L"Pad %i %i", port, slot);
			cfg.WriteInt(temp, L"Mode", config.padConfigs[port][slot].type);
			cfg.WriteInt(temp, L"Auto Analog", config.padConfigs[port][slot].autoAnalog);
		}
	}

	if (!dm)
		return 0;

	for (int i=0; i<dm->numDevices; i++) {
		wchar_t id[50];
		wchar_t temp[50], temp2[1000];
		wsprintfW(id, L"Device %i", i);
		Device *dev = dm->devices[i];
		wchar_t *name = dev->displayName;
		while (name[0] == '[') {
			wchar_t *name2 = wcschr(name, ']');
			if (!name2) break;
			name = name2+1;
			while (iswspace(name[0])) name++;
		}

		cfg.WriteStr(id, L"Display Name", name);
		cfg.WriteStr(id, L"Instance ID", dev->instanceID);
		if (dev->productID) {
			cfg.WriteStr(id, L"Product ID", dev->productID);
		}
		cfg.WriteInt(id, L"API", dev->api);
		cfg.WriteInt(id, L"Type", dev->type);
		int ffBindingCount = 0;
		int bindingCount = 0;
		for (int port=0; port<2; port++) {
			for (int slot=0; slot<4; slot++) {
				for (int j=0; j<dev->pads[port][slot].numBindings; j++) {
					Binding *b = dev->pads[port][slot].bindings+j;
					VirtualControl *c = &dev->virtualControls[b->controlIndex];
					wsprintfW(temp, L"Binding %i", bindingCount++);
					wsprintfW(temp2, L"0x%08X, %i, %i, %i, %i, %i, %i", c->uid, port, b->command, b->sensitivity, b->turbo, slot, b->deadZone);
					cfg.WriteStr(id, temp, temp2);
				}

				for (int j=0; j<dev->pads[port][slot].numFFBindings; j++) {
					ForceFeedbackBinding *b = dev->pads[port][slot].ffBindings+j;
					ForceFeedbackEffectType *eff = &dev->ffEffectTypes[b->effectIndex];
					wsprintfW(temp, L"FF Binding %i", ffBindingCount++);
					wsprintfW(temp2, L"%s %i, %i, %i", eff->effectID, port, b->motor, slot);
					for (int k=0; k<dev->numFFAxes; k++) {
						ForceFeedbackAxis *axis = dev->ffAxes + k;
						AxisEffectInfo *info = b->axes + k;
						//wsprintfW(wcschr(temp2,0), L", %i, %i", axis->id, info->force);
						// Not secure because I'm too lazy to compute the remaining size
						wprintf(wcschr(temp2, 0), L", %i, %i", axis->id, info->force);
					}
					cfg.WriteStr(id, temp, temp2);
				}
			}
		}
	}

	return 0;
}

int LoadSettings(int force, wchar_t *file) {
	if (dm && !force) return 0;

	// Could just do ClearDevices() instead, but if I ever add any extra stuff,
	// this will still work.
	UnloadConfigs();
	dm = new InputDeviceManager();

	CfgHelper cfg;

	for (int i=0; i<sizeof(BoolOptionsInfo)/sizeof(BoolOptionsInfo[0]); i++) {
		config.bools[i] = cfg.ReadBool(L"General Settings", BoolOptionsInfo[i].name, BoolOptionsInfo[i].defaultValue);
	}


	config.closeHacks = (u8)cfg.ReadInt(L"General Settings", L"Close Hacks");
	if (config.closeHacks&1) config.closeHacks &= ~2;

	config.keyboardApi = (DeviceAPI)cfg.ReadInt(L"General Settings", L"Keyboard Mode", WM);
	if (!config.keyboardApi) config.keyboardApi = WM;
	config.mouseApi = (DeviceAPI) cfg.ReadInt(L"General Settings", L"Mouse Mode");

	config.volume = cfg.ReadInt(L"General Settings", L"Volume", 100);

	for (int port=0; port<2; port++) {
		for (int slot=0; slot<4; slot++) {
			wchar_t temp[50];
			wsprintf(temp, L"Pad %i %i", port, slot);
			config.padConfigs[port][slot].type = (PadType) cfg.ReadInt(temp, L"Mode", Dualshock2Pad);
			config.padConfigs[port][slot].autoAnalog = cfg.ReadBool(temp, L"Auto Analog");
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
		if (!cfg.ReadStr(id, L"Display Name", temp2) || !temp2[0] ||
			!cfg.ReadStr(id, L"Instance ID", temp3) || !temp3[0]) {
			if (i >= 100) break;
			continue;
		}
		wchar_t *id2 = 0;
		if (cfg.ReadStr(id, L"Product ID", temp4) && temp4[0])
			id2 = temp4;

		int api = cfg.ReadInt(id, L"API");
		int type = cfg.ReadInt(id, L"Type");
		if (!api || !type) continue;

		Device *dev = new Device((DeviceAPI)api, (DeviceType)type, temp2, temp3, id2);
		dev->attached = 0;
		dm->AddDevice(dev);
		int j = 0;
		int last = 0;
		while (1) {
			wsprintfW(temp, L"Binding %i", j++);
			if (!cfg.ReadStr(id, temp, temp2)) {
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
					//TODO BindCommand(dev, uid, port, slot, command, sensitivity, turbo, deadZone);
				}
			}
		}
		j = 0;
		while (1) {
			wsprintfW(temp, L"FF Binding %i", j++);
			if (!cfg.ReadStr(id, temp, temp2)) {
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
				//TODO CreateEffectBinding(dev, temp2, port, slot, motor, &b);
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

	//TODO RefreshEnabledDevicesAndDisplay(1);

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
