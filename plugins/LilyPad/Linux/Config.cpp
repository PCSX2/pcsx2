/*  LilyPad - Pad plugin for PS2 Emulator
 *  Copyright (C) 2002-2017  PCSX2 Dev Team/ChickenLiver
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

#if 0
remove 0x10F0 to compute the cmd value

#define ID_SENSITIVITY 0x1007
#define ID_LOCK_BUTTONS 0x10FC
#define ID_LOCK 0x10FD
#define ID_LOCK_DIRECTION 0x10FE
#define ID_MOUSE 0x10FF
#define ID_SELECT 0x1100
#define ID_L3 0x1101
#define ID_R3 0x1102
#define ID_START 0x1103
#define ID_DPAD_UP 0x1104
#define ID_DPAD_RIGHT 0x1105
#define ID_DPAD_DOWN 0x1106
#define ID_DPAD_LEFT 0x1107
#define ID_L2 0x1108
#define ID_R2 0x1109
#define ID_L1 0x110A
#define ID_R1 0x110B
#define ID_TRIANGLE 0x110C
#define ID_CIRCLE 0x110D
#define ID_CROSS 0x110E
#define ID_SQUARE 0x110F
#define ID_LSTICK_UP 0x1110
#define ID_LSTICK_RIGHT 0x1111
#define ID_LSTICK_DOWN 0x1112
#define ID_LSTICK_LEFT 0x1113
#define ID_RSTICK_UP 0x1114
#define ID_RSTICK_RIGHT 0x1115
#define ID_RSTICK_DOWN 0x1116
#define ID_RSTICK_LEFT 0x1117
#define ID_ANALOG 0x1118
#define ID_DELETE 0x11FF
#define ID_DEBUG 0x1200
#define ID_IGNORE 0x1201
#define ID_CLEAR 0x1202
#define ID_REFRESH 0x1202
#define ID_SAVE 0x1204
#define ID_LOAD 0x1205
#define ID_BIG_MOTOR 0x120A
#define ID_SMALL_MOTOR 0x120B
#define ID_TEST 0x1300
#define ID_CONTROLS 0x1301
#define ID_FF 0x1304

#endif

struct GeneralSettingsBool
{
    const wchar_t *name;
    unsigned int ControlId;
    u8 defaultValue;
};

// XXX: I try to remove only gui stuff
void DeleteBinding(int port, int slot, int padtype, Device *dev, Binding *b)
{
    fprintf(stderr, "delete binding %d:%d\n", port, slot);
    Binding *bindings = dev->pads[port][slot][padtype].bindings;
    int i = b - bindings;
    memmove(bindings + i, bindings + i + 1, sizeof(Binding) * (dev->pads[port][slot][padtype].numBindings - i - 1));
    dev->pads[port][slot][padtype].numBindings--;
}

void DeleteBinding(int port, int slot, Device *dev, ForceFeedbackBinding *b)
{
    int padtype = config.padConfigs[port][slot].type;
    ForceFeedbackBinding *bindings = dev->pads[port][slot][padtype].ffBindings;
    int i = b - bindings;
    memmove(bindings + i, bindings + i + 1, sizeof(Binding) * (dev->pads[port][slot][padtype].numFFBindings - i - 1));
    dev->pads[port][slot][padtype].numFFBindings--;
}

int BindCommand(Device *dev, unsigned int uid, unsigned int port, unsigned int slot, unsigned int padtype, int command, int sensitivity, int rapidFire, int deadZone)
{
    // Checks needed because I use this directly when loading bindings.
    if (port > 1 || slot > 3 || padtype >= numPadTypes)
        return -1;
    if (!sensitivity)
        sensitivity = BASE_SENSITIVITY;
    if ((uid >> 16) & (PSHBTN | TGLBTN)) {
        deadZone = 0;
    } else if (!deadZone) {
        if ((uid >> 16) & PRESSURE_BTN) {
            deadZone = 1;
        } else {
            deadZone = DEFAULT_DEADZONE;
        }
    }
    // Relative axes can have negative sensitivity.
    else if (((uid >> 16) & 0xFF) == RELAXIS) {
        sensitivity = abs(sensitivity);
    }
    VirtualControl *c = dev->GetVirtualControl(uid);
    if (!c)
        return -1;
    // Add before deleting.  Means I won't scroll up one line when scrolled down to bottom.
    int controlIndex = c - dev->virtualControls;
    int index = 0;
    PadBindings *p = dev->pads[port][slot] + padtype;
    p->bindings = (Binding *)realloc(p->bindings, (p->numBindings + 1) * sizeof(Binding));
    for (index = p->numBindings; index > 0; index--) {
        if (p->bindings[index - 1].controlIndex < controlIndex)
            break;
        p->bindings[index] = p->bindings[index - 1];
    }
    Binding *b = p->bindings + index;
    p->numBindings++;
    b->command = command;
    b->controlIndex = controlIndex;
    b->rapidFire = rapidFire;
    b->sensitivity = sensitivity;
    b->deadZone = deadZone;
    // Where it appears in listview.
    //int count = ListBoundCommand(port, slot, dev, b);

    int newBindingIndex = index;
    index = 0;
    while (index < p->numBindings) {
        if (index == newBindingIndex) {
            index++;
            continue;
        }
        b = p->bindings + index;
        int nuke = 0;
        if (config.multipleBinding) {
            if (b->controlIndex == controlIndex && b->command == command)
                nuke = 1;
        } else {
            int uid2 = dev->virtualControls[b->controlIndex].uid;
            if (b->controlIndex == controlIndex || (!((uid2 ^ uid) & 0xFFFFFF) && ((uid | uid2) & (UID_POV | UID_AXIS))))
                nuke = 1;
        }
        if (!nuke) {
            index++;
            continue;
        }
        if (index < newBindingIndex) {
            newBindingIndex--;
            //count --;
        }
        DeleteBinding(port, slot, padtype, dev, b);
    }
    if (!config.multipleBinding) {
        for (int port2 = 0; port2 < 2; port2++) {
            for (int slot2 = 0; slot2 < 4; slot2++) {
                if (port2 == (int)port && slot2 == (int)slot)
                    continue;
                for (int padtype2 = 0; padtype2 < numPadTypes; padtype2++) {
                    PadBindings *p = dev->pads[port2][slot2] + padtype2;
                    for (int i = 0; i < p->numBindings; i++) {
                        Binding *b = p->bindings + i;
                        int uid2 = dev->virtualControls[b->controlIndex].uid;
                        if (b->controlIndex == controlIndex || (!((uid2 ^ uid) & 0xFFFFFF) && ((uid | uid2) & (UID_POV | UID_AXIS)))) {
                            DeleteBinding(port2, slot2, padtype2, dev, b);
                            i--;
                        }
                    }
                }
            }
        }
    }

    //return count;
    return 0;
}

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

    {L"Logging", 0 /*IDC_DEBUG_FILE*/, 0},

    {L"GH2", 0 /*IDC_GH2_HACK*/, 0},
};

void CALLBACK PADsetSettingsDir(const char *dir)
{
    CfgHelper::SetSettingsDir(dir);
}

int SaveSettings(wchar_t *file = 0)
{
    CfgHelper cfg;

    for (size_t i = 0; i < sizeof(BoolOptionsInfo) / sizeof(BoolOptionsInfo[0]); i++) {
        cfg.WriteBool(L"General Settings", BoolOptionsInfo[i].name, config.bools[i]);
    }

    cfg.WriteInt(L"General Settings", L"Keyboard Mode", config.keyboardApi);
    cfg.WriteInt(L"General Settings", L"Mouse Mode", config.mouseApi);

    for (int port = 0; port < 2; port++) {
        for (int slot = 0; slot < 4; slot++) {
            wchar_t temp[50];
            wsprintf(temp, L"Pad %i %i", port, slot);
            cfg.WriteInt(temp, L"Mode", config.padConfigs[port][slot].type);
        }
    }

    if (!dm)
        return 0;

    for (int i = 0; i < dm->numDevices; i++) {
        wchar_t id[50];
        wchar_t temp[50], temp2[1000];
        wsprintfW(id, L"Device %i", i);
        Device *dev = dm->devices[i];
        wchar_t *name = dev->displayName;
        while (name[0] == '[') {
            wchar_t *name2 = wcschr(name, ']');
            if (!name2)
                break;
            name = name2 + 1;
            while (iswspace(name[0]))
                name++;
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
        for (int port = 0; port < 2; port++) {
            for (int slot = 0; slot < 4; slot++) {
                for (int padtype = 0; padtype < numPadTypes; padtype++) {
                    for (int j = 0; j < dev->pads[port][slot][padtype].numBindings; j++) {
                        Binding *b = dev->pads[port][slot][padtype].bindings + j;
                        VirtualControl *c = &dev->virtualControls[b->controlIndex];
                        wsprintfW(temp, L"Binding %i", bindingCount++);
                        wsprintfW(temp2, L"0x%08X, %i, %i, %i, %i, %i, %i, %i", c->uid, port, b->command, b->sensitivity, b->rapidFire, slot, b->deadZone, padtype);
                        cfg.WriteStr(id, temp, temp2);
                    }

                    for (int j = 0; j < dev->pads[port][slot][padtype].numFFBindings; j++) {
                        ForceFeedbackBinding *b = dev->pads[port][slot][padtype].ffBindings + j;
                        ForceFeedbackEffectType *eff = &dev->ffEffectTypes[b->effectIndex];
                        wsprintfW(temp, L"FF Binding %i", ffBindingCount++);
                        wsprintfW(temp2, L"%s %i, %i, %i, %i", eff->effectID, port, b->motor, slot, padtype);
                        for (int k = 0; k < dev->numFFAxes; k++) {
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
    }

    return 0;
}

int LoadSettings(int force, wchar_t *file)
{
    if (dm && !force)
        return 0;

    // Could just do ClearDevices() instead, but if I ever add any extra stuff,
    // this will still work.
    UnloadConfigs();
    dm = new InputDeviceManager();

    CfgHelper cfg;

    for (size_t i = 0; i < sizeof(BoolOptionsInfo) / sizeof(BoolOptionsInfo[0]); i++) {
        config.bools[i] = cfg.ReadBool(L"General Settings", BoolOptionsInfo[i].name, BoolOptionsInfo[i].defaultValue);
    }

    config.keyboardApi = (DeviceAPI)cfg.ReadInt(L"General Settings", L"Keyboard Mode", LNX_KEYBOARD);
    if (!config.keyboardApi)
        config.keyboardApi = LNX_KEYBOARD;
    config.mouseApi = (DeviceAPI)cfg.ReadInt(L"General Settings", L"Mouse Mode");

    for (int port = 0; port < 2; port++) {
        for (int slot = 0; slot < 4; slot++) {
            wchar_t temp[50];
            wsprintf(temp, L"Pad %i %i", port, slot);
            config.padConfigs[port][slot].type = (PadType)cfg.ReadInt(temp, L"Mode", Dualshock2Pad);
        }
    }

    bool oldIni = false;
    int i = 0;
    int multipleBinding = config.multipleBinding;
    // Disabling multiple binding only prevents new multiple bindings.
    config.multipleBinding = 1;
    while (1) {
        wchar_t id[50];
        wchar_t temp[50], temp2[1000], temp3[1000], temp4[1000];
        wsprintfW(id, L"Device %i", i++);
        if (!cfg.ReadStr(id, L"Display Name", temp2) || !temp2[0] ||
            !cfg.ReadStr(id, L"Instance ID", temp3) || !temp3[0]) {
            if (i >= 100)
                break;
            continue;
        }
        wchar_t *id2 = 0;
        if (cfg.ReadStr(id, L"Product ID", temp4) && temp4[0])
            id2 = temp4;

        int api = cfg.ReadInt(id, L"API");
        int type = cfg.ReadInt(id, L"Type");
        if (!api || !type)
            continue;

        Device *dev = new Device((DeviceAPI)api, (DeviceType)type, temp2, temp3, id2);
        dev->attached = 0;
        dm->AddDevice(dev);
        int j = 0;
        int last = 0;
        while (1) {
            wsprintfW(temp, L"Binding %i", j++);
            if (!cfg.ReadStr(id, temp, temp2)) {
                if (j >= 100) {
                    if (!last)
                        break;
                    last = 0;
                }
                continue;
            }
            last = 1;
            unsigned int uid;
            int port, command, sensitivity, rapidFire, slot = 0, deadZone = 0, padtype = 0;
            int w = 0;
            char string[1000];
            while (temp2[w]) {
                string[w] = (char)temp2[w];
                w++;
            }
            string[w] = 0;
            int len = sscanf(string, " %u , %i , %i , %i , %i , %i , %i , %i", &uid, &port, &command, &sensitivity, &rapidFire, &slot, &deadZone, &padtype);
            if (len >= 5 && type) {
                VirtualControl *c = dev->GetVirtualControl(uid);
                if (!c)
                    c = dev->AddVirtualControl(uid, -1);
                if (c) {
                    if (len < 8) { // If ini file is imported from older version, make sure bindings aren't applied to "Unplugged" padtype.
                        oldIni = true;
                        if (config.padConfigs[port][slot].type != 0) {
                            padtype = config.padConfigs[port][slot].type;
                        } else {
                            padtype = 1;
                        }
                    }
                    BindCommand(dev, uid, port, slot, padtype, command, sensitivity, rapidFire, deadZone);
                }
            }
        }
        j = 0;
        while (1) {
            wsprintfW(temp, L"FF Binding %i", j++);
            if (!cfg.ReadStr(id, temp, temp2)) {
                if (j >= 10) {
                    if (!last)
                        break;
                    last = 0;
                }
                continue;
            }
            last = 1;
            int port, slot, motor, padtype;
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
            if (sscanf(string, " %20s %i , %i , %i , %i", effect, &port, &motor, &slot, &padtype) == 5) {
                char *s;
                if (oldIni) { // Make sure bindings aren't applied to "Unplugged" padtype and FF settings are read from old location.
                    if (config.padConfigs[port][slot].type != 0) {
                        padtype = config.padConfigs[port][slot].type;
                    } else {
                        padtype = 1;
                    }
                    s = strchr(strchr(strchr(string, ',') + 1, ',') + 1, ',');
                } else {
                    s = strchr(strchr(strchr(strchr(string, ',') + 1, ',') + 1, ',') + 1, ',');
                }
                if (!s)
                    continue;
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
#if 0
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
#endif
            }
        }
    }
    config.multipleBinding = multipleBinding;

    //TODO RefreshEnabledDevicesAndDisplay(1);
    RefreshEnabledDevices(1); // XXX For the moment only a subfonction

    return 0;
}

void UnloadConfigs()
{
    if (dm) {
        delete dm;
        dm = 0;
    }
}

void RefreshEnabledDevices(int updateDeviceList)
{
    // Clears all device state.
    static int lastXInputState = -1;
    if (updateDeviceList || lastXInputState != config.gameApis.xInput) {
        EnumDevices(config.gameApis.xInput);
        lastXInputState = config.gameApis.xInput;
    }

    for (int i = 0; i < dm->numDevices; i++) {
        Device *dev = dm->devices[i];

        // XXX windows magic?
        if (!dev->attached && dev->displayName[0] != '[') {
            wchar_t *newName = (wchar_t *)malloc(sizeof(wchar_t) * (wcslen(dev->displayName) + 12));
            wsprintfW(newName, L"[Detached] %s", dev->displayName);
            free(dev->displayName);
            dev->displayName = newName;
        }

        dm->EnableDevice(i);
#if 0 // windows magic?
		if ((dev->type == KEYBOARD && dev->api == IGNORE_KEYBOARD) ||
			(dev->type == KEYBOARD && dev->api == config.keyboardApi) ||
			(dev->type == MOUSE && dev->api == config.mouseApi) ||
			(dev->type == OTHER &&
				((dev->api == DI && config.gameApis.directInput) ||
				 (dev->api == DS3 && config.gameApis.dualShock3) ||
				 (dev->api == XINPUT && config.gameApis.xInput)))) {
			dm->EnableDevice(i);
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
#endif
    }
}

void Configure()
{
    // Can end up here without PADinit() being called first.
    LoadSettings();
    // Can also end up here after running emulator a bit, and possibly
    // disabling some devices due to focus changes, or releasing mouse.
    RefreshEnabledDevices(0);
}
