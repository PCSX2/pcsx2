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
#include "Global.h"

#include "common/Path.h"
#include "common/StringUtil.h"

#include "resource_pad.h"
#include "InputManager.h"
#include "PADConfig.h"

#include "Tooltips.h"
#include "Diagnostics.h"
#include "DeviceEnumerator.h"
#include "KeyboardQueue.h"
#include "WndProcEater.h"
#include "DualShock3.h"

// Needed to know if raw input is available.  It requires XP or higher.
#include "PADRawInput.h"
#include "Config.h"

//max len 24 wchar_t
const wchar_t* padTypes[] = {
	L"Unplugged",
	L"Dualshock 2",
	L"Guitar",
	L"Pop'n Music controller",
	L"PS1 Mouse",
	L"neGcon"};

GeneralConfig config;
HWND hWndProp = 0;
int selected = 0;
bool quickSetup = false;

// Older versions of PCSX2 don't always create the ini dir on startup, so PAD does it
// for it.  But if PCSX2 sets the ini path with a call to setSettingsDir, then it means
// we shouldn't make our own.
bool createIniDir = true;

HWND hWnds[2][4][numPadTypes];
HWND hWndGeneral = 0;

struct GeneralSettingsBool
{
	wchar_t* name;
	unsigned int ControlId;
	u8 defaultValue;
};

// Ties together config data structure, config files, and general config
// dialog.
const GeneralSettingsBool BoolOptionsInfo[] = {
	{L"Mouse Unfocus", IDC_MOUSE_UNFOCUS, 1},
	{L"Background", IDC_BACKGROUND, 1},
	{L"Multiple Bindings", IDC_MULTIPLE_BINDING, 0},

	{L"DirectInput Game Devices", IDC_G_DI, 1},
	{L"XInput", IDC_G_XI, 1},
	{L"DualShock 3", IDC_G_DS3, 0},

	{L"Multitap 1", IDC_MULTITAP1, 0},
	{L"Multitap 2", IDC_MULTITAP2, 0},

	{L"Logging", IDC_DEBUG_FILE, 0},

	{L"GH2", IDC_GH2_HACK, 0},
};

void Populate(int port, int slot, int padtype);

void SetupLogSlider(HWND hWndSlider)
{
	SendMessage(hWndSlider, TBM_SETRANGEMIN, 0, 1);
	SendMessage(hWndSlider, TBM_SETPAGESIZE, 0, 1 << 13);
	SendMessage(hWndSlider, TBM_SETRANGEMAX, 1, 1 << 22);
	SendMessage(hWndSlider, TBM_SETTIC, 0, 1 << 21);
	SendMessage(hWndSlider, TBM_SETTIC, 0, 1 << 20);
	SendMessage(hWndSlider, TBM_SETTIC, 0, 3 << 20);
}

int GetLogSliderVal(HWND hWnd, int id)
{
	HWND hWndSlider = GetDlgItem(hWnd, id);
	int val = SendMessage(hWndSlider, TBM_GETPOS, 0, 0);
	if (val <= (1 << 21))
	{
		val = (val + 16) >> 5;
	}
	else
	{
		double v = (val - (1 << 21)) / (double)((1 << 22) - (1 << 21));
		double max = log((double)(1 << 23));
		double min = log((double)BASE_SENSITIVITY);
		v = v * (max - min) + min;
		// exp is not implemented in ntdll, so use pow instead.
		// More than accurate enough for my needs.
		// v = exp(v);
		v = pow(2.7182818284590451, v);
		if (v > (1 << 23))
			v = (1 << 23);
		val = (int)floor(v + 0.5);
	}
	if (!val)
		val = 1;
	if (IsDlgButtonChecked(hWnd, id + 1) == BST_CHECKED)
	{
		val = -val;
	}
	return val;
}

void SetLogSliderVal(HWND hWnd, int id, HWND hWndText, int val)
{
	HWND hWndSlider = GetDlgItem(hWnd, id);
	int sliderPos = 0;
	wchar_t temp[30];
	EnableWindow(hWndSlider, val != 0);
	EnableWindow(hWndText, val != 0);
	EnableWindow(GetDlgItem(hWnd, id + 1), val != 0);
	if (!val)
		val = BASE_SENSITIVITY;
	CheckDlgButton(hWnd, id + 1, BST_CHECKED * (val < 0));
	if (val < 0)
	{
		val = -val;
	}
	if (val <= BASE_SENSITIVITY)
	{
		sliderPos = val << 5;
	}
	else
	{
		double max = log((double)(1 << 23));
		double min = log((double)BASE_SENSITIVITY);
		double v = log((double)(val));
		v = (v - min) / (max - min);
		v = ((1 << 22) - (1 << 21)) * v + (1 << 21);
		// _ftol
		sliderPos = (int)(floor(v + 0.5));
	}
	SendMessage(hWndSlider, TBM_SETPOS, 1, sliderPos);
	int val2 = (int)(1000 * (__int64)val / BASE_SENSITIVITY);
	wsprintfW(temp, L"%i.%03i", val2 / 1000, val2 % 1000);
	SetWindowTextW(hWndText, temp);
}

void RefreshEnabledDevices(int updateDeviceList)
{
	// Clears all device state.
	static int lastXInputState = -1;
	if (updateDeviceList || lastXInputState != config.gameApis.xInput)
	{
		EnumDevices(config.gameApis.xInput);
		lastXInputState = config.gameApis.xInput;
	}

	for (int i = 0; i < dm->numDevices; i++)
	{
		Device* dev = dm->devices[i];

		if (!dev->attached && dev->displayName[0] != '[')
		{
			wchar_t* newName = (wchar_t*)malloc(sizeof(wchar_t) * (wcslen(dev->displayName) + 12));
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
			  (dev->api == XINPUT && config.gameApis.xInput))))
		{
			if (config.gameApis.dualShock3 && dev->api == DI && dev->displayName &&
				!wcsicmp(dev->displayName, L"DX PLAYSTATION(R)3 Controller"))
			{
				dm->DisableDevice(i);
			}
			else
			{
				dm->EnableDevice(i);
			}
		}
		else
		{
			dm->DisableDevice(i);
		}
	}
}

// Disables/enables devices as necessary.  Also updates diagnostic list
// and pad1/pad2 bindings lists, depending on params.  Also updates device
// list, if indicated.

// Must be called at some point when entering config mode, as I disable
// (non-keyboard/mice) devices with no active bindings when the emulator's running.
void RefreshEnabledDevicesAndDisplay(int updateDeviceList = 0, HWND hWnd = 0, int populate = 0)
{
	RefreshEnabledDevices(updateDeviceList);
	if (hWnd)
	{
		HWND hWndList = GetDlgItem(hWnd, IDC_DIAG_LIST);
		ListView_SetExtendedListViewStyleEx(hWndList, LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
		int count = ListView_GetItemCount(hWndList);
		LVITEM item;
		item.iItem = 0;
		item.iSubItem = 0;
		item.mask = LVIF_TEXT | LVIF_PARAM;
		for (int j = 0; j < dm->numDevices; j++)
		{
			if (dm->devices[j]->enabled && dm->devices[j]->api != IGNORE_KEYBOARD)
			{
				item.lParam = j;
				item.pszText = dm->devices[j]->displayName;
				if (count > 0)
				{
					ListView_SetItem(hWndList, &item);
					count--;
				}
				else
				{
					ListView_InsertItem(hWndList, &item);
				}
				item.iItem++;
			}
		}
		// This way, won't scroll list to start.
		while (count > 0)
		{
			ListView_DeleteItem(hWndList, item.iItem);
			count--;
		}
	}
	if (populate)
	{
		for (int j = 0; j < numPadTypes; j++)
		{
			for (int i = 0; i < 8; i++)
			{
				Populate(i & 1, i >> 1, j);
			}
		}
	}
}

wchar_t* GetCommandStringW(u8 command, int port, int slot)
{
	int padtype = config.padConfigs[port][slot].type;
	static wchar_t temp[34];
	if (command >= 0x20 && command <= 0x27)
	{
		if (padtype == GuitarPad && (command == 0x20 || command == 0x22))
		{
			HWND hWnd = GetDlgItem(hWnds[port][slot][padtype], 0x40F0 + command);
			int res = GetWindowTextW(hWnd, temp, 20);
			if ((unsigned int)res - 1 <= 18)
				return temp;
		}
		static wchar_t* stick[2] = {L"L-Stick", L"R-Stick"};
		static wchar_t* dir[] = {
			L"Up", L"Right",
			L"Down", L"Left"};
		wsprintfW(temp, L"%s %s", padtype == neGconPad ? L"Rotate" : stick[(command - 0x20) / 4], dir[command & 3]);
		return temp;
	}
	/* Get text from the buttons. */
	if (command >= 0x0F && command <= 0x2D)
	{
		HWND hWnd = GetDlgItem(hWnds[port][slot][padtype], 0x40F0 + command);
		if (!hWnd || ((padtype == Dualshock2Pad || padtype == neGconPad) && command >= 0x14 && command <= 0x17))
		{
			wchar_t* strings[] = {
				L"Mouse",          // 0x0F (15)
				L"Select",         // 0x10 (16)
				L"L3",             // 0x11 (17)
				L"R3",             // 0x12 (18)
				L"Start",          // 0x13 (19)
				L"D-Pad Up",       // 0x14 (20)
				L"D-Pad Right",    // 0x15 (21)
				L"D-Pad Down",     // 0x16 (22)
				L"D-Pad Left",     // 0x17 (23)
				L"L2",             // 0x18 (24)
				L"R2",             // 0x19 (25)
				L"L1",             // 0x1A (26)
				L"R1",             // 0x1B (27)
				L"Triangle",       // 0x1C (28)
				L"Circle",         // 0x1D (29)
				L"Square",         // 0x1E (30)
				L"Cross",          // 0x1F (31)
				L"L-Stick Up",     // 0x20 (32)
				L"L-Stick Right",  // 0x21 (33)
				L"L-Stick Down",   // 0x22 (34)
				L"L-Stick Left",   // 0x23 (35)
				L"R-Stick Up",     // 0x24 (36)
				L"R-Stick Right",  // 0x25 (37)
				L"R-Stick Down",   // 0x26 (38)
				L"R-Stick Left",   // 0x27 (39)
				L"Analog",         // 0x28 (40)
				L"Excluded Input", // 0x29 (41)
				L"Lock Buttons",   // 0x2A (42)
				L"Lock Input",     // 0x2B (43)
				L"Lock Direction", // 0x2C (44)
				L"Turbo",          // 0x2D (45)
			};
			return strings[command - 0xF];
		}
		int res = GetWindowTextW(hWnd, temp, 20);
		if ((unsigned int)res - 1 <= 18)
			return temp;
	}
	return L"";
}

static wchar_t iniFileUSB[MAX_PATH * 2] = L"inis/PAD.ini";

// we disregard dir and use global settings instead, we should probably clean that up someday...
void PADsetSettingsDir(const char* dir)
{
	//swprintf_s( iniFileUSB, L"%S", (dir==NULL) ? "inis" : dir );

	//uint targlen = MultiByteToWideChar(CP_ACP, 0, dir, -1, NULL, 0);
	StrCpyNW(iniFileUSB, StringUtil::UTF8StringToWideString(Path::Combine(EmuFolders::Settings, "PAD.ini")).c_str(), std::size(iniFileUSB));

	createIniDir = false;

	FILE* temp = nullptr;
	_wfopen_s(&temp, iniFileUSB, L"r");
	if (!temp)
	{ // File not found, possibly.
		HRSRC res = FindResource(GetModuleHandle(nullptr), MAKEINTRESOURCE(IDR_INI1), RT_RCDATA);
		if (!res)
			return;
		HGLOBAL data = LoadResource(GetModuleHandle(nullptr), res);
		if (!data)
			return;
		size_t size = SizeofResource(GetModuleHandle(nullptr), res);
		u8* fdata = (u8*)LockResource(data);
		_wfopen_s(&temp, iniFileUSB, L"w");
		if (!temp)
			return;
		fwrite(fdata, 1, size, temp);
		fclose(temp);
	}
	else
	{
		fclose(temp);
	}
}

int GetBinding(int port, int slot, int index, Device*& dev, Binding*& b, ForceFeedbackBinding*& ffb);
int BindCommand(Device* dev, unsigned int uid, unsigned int port, unsigned int slot, unsigned int padtype, int command, int sensitivity, int rapidFire, int deadZone, int skipDeadZone);

int CreateEffectBinding(Device* dev, wchar_t* effectName, unsigned int port, unsigned int slot, unsigned int padtype, unsigned int motor, ForceFeedbackBinding** binding);

void SelChanged(int port, int slot)
{
	int padtype = config.padConfigs[port][slot].type;
	HWND hWnd = hWnds[port][slot][padtype];
	if (!hWnd)
		return;
	HWND hWndTemp, hWndList = GetDlgItem(hWnd, IDC_BINDINGS_LIST);
	int j, i = ListView_GetSelectedCount(hWndList);
	wchar_t* devName = L"N/A";
	wchar_t* key = L"N/A";
	wchar_t* command = L"N/A";
	// Second value is now rapidFire.
	int rapidFire = -1;
	int sensitivity = 0;
	int deadZone = 0;
	int skipDeadZone = 0;
	int nonButtons = 0;
	// Set if sensitivity != 0, but need to disable flip anyways.
	// Only used to relative axes.
	int disableFlip = 0;
	wchar_t temp[4][1000];
	Device* dev;
	int bFound = 0;
	int ffbFound = 0;
	ForceFeedbackBinding* ffb = 0;
	Binding* b = 0;
	if (i >= 1)
	{
		int index = -1;
		int flipped = 0;
		while (1)
		{
			if (!(config.bind && (!config.configureOnBind || quickSetup)))
				index = ListView_GetNextItem(hWndList, index, LVNI_SELECTED);
			if (index < 0)
				break;
			LVITEMW item;
			item.iItem = index;
			item.mask = LVIF_TEXT;
			item.pszText = temp[3];
			for (j = 0; j < 3; j++)
			{
				item.iSubItem = j;
				item.cchTextMax = sizeof(temp[0]) / sizeof(temp[3][0]);
				if (!ListView_GetItem(hWndList, &item))
					break;
				if (!bFound && !ffbFound)
					wcscpy(temp[j], temp[3]);
				else if (wcsicmp(temp[j], temp[3]))
				{
					int q = 0;
					while (temp[j][q] == temp[3][q])
						q++;
					if (q && temp[j][q - 1] == ' ' && temp[j][q] && temp[j][q + 1] == 0)
						q--;
					if (j == 1)
					{
						// Really ugly, but merges labels for multiple directions for same axis.
						if ((temp[j][q] == 0 || (temp[j][q] == ' ' && temp[j][q + 2] == 0)) &&
							(temp[3][q] == 0 || (temp[3][q] == ' ' && temp[3][q + 2] == 0)))
						{
							temp[j][q] = 0;
							continue;
						}
					}
					// Merge different directions for same stick.
					else if (j == 2 && q > 4)
					{
						temp[j][q] = 0;
						continue;
					}
					wcscpy(temp[j], L"*");
				}
			}
			if (j == 3)
			{
				devName = temp[0];
				key = temp[1];
				command = temp[2];
				if (GetBinding(port, slot, index, dev, b, ffb))
				{
					if (b)
					{
						bFound++;
						VirtualControl* control = &dev->virtualControls[b->controlIndex];
						// Only relative axes can't have negative sensitivity.
						if (((control->uid >> 16) & 0xFF) == RELAXIS)
						{
							disableFlip = 1;
						}
						rapidFire += b->rapidFire;
						if (b->sensitivity < 0)
						{
							flipped++;
							sensitivity -= b->sensitivity;
						}
						else
						{
							sensitivity += b->sensitivity;
						}
						if (((control->uid >> 16) & 0xFF) != PSHBTN && ((control->uid >> 16) & 0xFF) != TGLBTN)
						{
							deadZone += b->deadZone;
							skipDeadZone += b->skipDeadZone;
							nonButtons++;
						}
					}
					else
						ffbFound++;
				}
			}
		}
		if ((bFound && ffbFound) || ffbFound > 1)
		{
			ffb = 0;
			rapidFire = -1;
			deadZone = 0;
			skipDeadZone = 0;
			sensitivity = 0;
			disableFlip = 1;
			bFound = ffbFound = 0;
		}
		else if (bFound)
		{
			rapidFire++;
			sensitivity /= bFound;
			if (nonButtons)
			{
				deadZone /= nonButtons;
				skipDeadZone /= nonButtons;
			}
			if (bFound > 1)
				disableFlip = 1;
			else if (flipped)
			{
				sensitivity = -sensitivity;
			}
		}
	}
	config.bind = false;

	// Input & Special Bindings:
	SetWindowTextW(GetDlgItem(hWnd, ID_SPECIAL_INPUTS), config.specialInputs[port][slot] ? L"Hide Special Inputs" : L"Show Special Inputs");
	for (i = IDC_DPAD; i <= ID_SPECIAL_INPUTS; i++)
	{
		hWndTemp = GetDlgItem(hWnd, i);
		if (hWndTemp)
		{
			if (i >= ID_EXCLUDE && i <= ID_TURBO_KEY)
				ShowWindow(hWndTemp, !ffb && !b && config.specialInputs[port][slot]);
			else
				ShowWindow(hWndTemp, !ffb && !b);
		}
	}
	// Input configuration:
	for (i = IDC_SLIDER_SENSITIVITY; i <= IDC_AXIS_SKIP_DEADZONE; i++)
	{
		hWndTemp = GetDlgItem(hWnd, i);
		if (hWndTemp)
			ShowWindow(hWndTemp, !ffb && b);
		if (i == IDC_SKIP_DEADZONE_OFF && skipDeadZone > deadZone)
			ShowWindow(hWndTemp, 0);
		if (i == IDC_AXIS_SKIP_DEADZONE && skipDeadZone <= deadZone)
			ShowWindow(hWndTemp, 0);
	}
	// Pad Force Feedback configuration:
	for (i = ID_TEST; i <= IDC_FF_AXIS8_SCALE; i++)
	{
		hWndTemp = GetDlgItem(hWnd, i);
		if (hWndTemp)
		{
			int enable = ffb != 0;
			if (ffb && i >= IDC_FF_AXIS1_ENABLED)
			{
				int index = (i - IDC_FF_AXIS1_ENABLED) / 16;
				if (index >= dev->numFFAxes)
				{
					enable = 0;
				}
				else
				{
					int type = i - index * 16;
					AxisEffectInfo* info = 0;
					if (dev->numFFAxes > index)
					{
						info = ffb->axes + index;
					}
					if (type == IDC_FF_AXIS1)
					{
						// takes care of IDC_FF_AXIS1_FLIP as well.
						SetupLogSlider(hWndTemp);
						int force = 0;
						if (info)
						{
							force = info->force;
						}
						SetLogSliderVal(hWnd, i, GetDlgItem(hWnd, i - IDC_FF_AXIS1 + IDC_FF_AXIS1_SCALE), force);
					}
					else if (type == IDC_FF_AXIS1_ENABLED)
					{
						CheckDlgButton(hWnd, i, BST_CHECKED * (info->force != 0));
						wsprintfW(temp[0], L"Axis %i", index + 1);
						if (dev->ffAxes[index].displayName && wcslen(dev->ffAxes[index].displayName) < 50)
						{
							wchar_t* end = wcschr(temp[0], 0);
							wsprintfW(end, L": %s", dev->ffAxes[index].displayName);
						}
						SetWindowText(hWndTemp, temp[0]);
					}
				}
			}
			ShowWindow(hWndTemp, enable);
		}
	}
	ShowWindow(GetDlgItem(hWnd, ID_CONTROLS), ffb || b);
	ShowWindow(GetDlgItem(hWnd, ID_RESET_CONFIG), ffb || b);

	if (!ffb)
	{
		SetLogSliderVal(hWnd, IDC_SLIDER_SENSITIVITY, GetDlgItem(hWnd, IDC_AXIS_SENSITIVITY), sensitivity);
		SetLogSliderVal(hWnd, IDC_SLIDER_DEADZONE, GetDlgItem(hWnd, IDC_AXIS_DEADZONE), deadZone);
		SetLogSliderVal(hWnd, IDC_SLIDER_SKIP_DEADZONE, GetDlgItem(hWnd, IDC_AXIS_SKIP_DEADZONE), skipDeadZone);

		if (disableFlip)
			EnableWindow(GetDlgItem(hWnd, IDC_FLIP), 0);

		EnableWindow(GetDlgItem(hWnd, IDC_RAPID_FIRE), rapidFire >= 0);
		if (rapidFire > 0 && rapidFire < bFound)
		{
			SendMessage(GetDlgItem(hWnd, IDC_RAPID_FIRE), BM_SETSTYLE, BS_AUTO3STATE, 0);
			CheckDlgButton(hWnd, IDC_RAPID_FIRE, BST_INDETERMINATE);
		}
		else
		{
			SendMessage(GetDlgItem(hWnd, IDC_RAPID_FIRE), BM_SETSTYLE, BS_AUTOCHECKBOX, 0);
			CheckDlgButton(hWnd, IDC_RAPID_FIRE, BST_CHECKED * (bFound && rapidFire == bFound));
		}
		HWND hWndCombo = GetDlgItem(hWnd, IDC_AXIS_DIRECTION);
		int enableCombo = 0;
		SendMessage(hWndCombo, CB_RESETCONTENT, 0, 0);
		if (b && bFound == 1)
		{
			VirtualControl* control = &dev->virtualControls[b->controlIndex];
			unsigned int uid = control->uid;
			if (((uid >> 16) & 0xFF) == ABSAXIS)
			{
				enableCombo = 1;
				wchar_t* endings[3] = {L" -", L" +", L""};
				wchar_t* string = temp[3];
				wcscpy(string, key);
				wchar_t* end = wcschr(string, 0);
				int sel = 2;
				if (!(uid & UID_AXIS))
				{
					end[-2] = 0;
					sel = (end[-1] == '+');
					end -= 2;
				}
				for (int i = 0; i < 3; i++)
				{
					wcscpy(end, endings[i]);
					SendMessage(hWndCombo, CB_ADDSTRING, 0, (LPARAM)string);
					if (i == sel)
						SendMessage(hWndCombo, CB_SETCURSEL, i, 0);
				}
			}
		}
		EnableWindow(hWndCombo, enableCombo);
		if (!enableCombo)
		{
			SendMessage(hWndCombo, CB_ADDSTRING, 0, (LPARAM)key);
			SendMessage(hWndCombo, CB_SELECTSTRING, i, (LPARAM)key);
		}

		SetDlgItemText(hWnd, IDC_AXIS_DEVICE, devName);
		SetDlgItemText(hWnd, IDC_AXIS_CONTROL, command);
	}
	else
	{
		wchar_t temp2[2000];
		wsprintfW(temp2, L"%s / %s", devName, command);
		SetDlgItemText(hWnd, ID_FF, temp2);

		hWndTemp = GetDlgItem(hWnd, IDC_FF_EFFECT);
		SendMessage(hWndTemp, CB_RESETCONTENT, 0, 0);
		for (i = 0; i < dev->numFFEffectTypes; i++)
		{
			SendMessage(hWndTemp, CB_INSERTSTRING, i, (LPARAM)dev->ffEffectTypes[i].displayName);
		}
		SendMessage(hWndTemp, CB_SETCURSEL, ffb->effectIndex, 0);
		EnableWindow(hWndTemp, dev->numFFEffectTypes > 1);
	}
}

void UnselectAll(HWND hWnd)
{
	int i = ListView_GetSelectedCount(hWnd);
	while (i-- > 0)
	{
		int index = ListView_GetNextItem(hWnd, -1, LVNI_SELECTED);
		ListView_SetItemState(hWnd, index, 0, LVIS_SELECTED);
	}
}


int GetItemIndex(int port, int slot, Device* dev, ForceFeedbackBinding* binding)
{
	int count = 0;
	int padtype = config.padConfigs[port][slot].type;
	int selectedDevice = config.deviceSelect[port][slot];
	for (int i = 0; i < dm->numDevices; i++)
	{
		Device* dev2 = dm->devices[i];
		if (!dev2->enabled || selectedDevice >= 0 && dm->devices[selectedDevice] != dev2)
			continue;
		if (dev2 != dev)
		{
			count += dev2->pads[port][slot][padtype].numBindings + dev2->pads[port][slot][padtype].numFFBindings;
			continue;
		}
		return count += dev2->pads[port][slot][padtype].numBindings + (binding - dev2->pads[port][slot][padtype].ffBindings);
	}
	return -1;
}
int GetItemIndex(int port, int slot, Device* dev, Binding* binding)
{
	int count = 0;
	int padtype = config.padConfigs[port][slot].type;
	int selectedDevice = config.deviceSelect[port][slot];
	for (int i = 0; i < dm->numDevices; i++)
	{
		Device* dev2 = dm->devices[i];
		if (!dev2->enabled || selectedDevice >= 0 && dm->devices[selectedDevice] != dev2)
			continue;
		if (dev2 != dev)
		{
			count += dev2->pads[port][slot][padtype].numBindings + dev2->pads[port][slot][padtype].numFFBindings;
			continue;
		}
		return count += binding - dev->pads[port][slot][padtype].bindings;
	}
	return -1;
}

// Doesn't check if already displayed.
int ListBoundCommand(int port, int slot, Device* dev, Binding* b)
{
	int padtype = config.padConfigs[port][slot].type;
	if (!hWnds[port][slot][padtype])
		return -1;
	HWND hWndList = GetDlgItem(hWnds[port][slot][padtype], IDC_BINDINGS_LIST);
	int index = -1;
	if (hWndList)
	{
		index = GetItemIndex(port, slot, dev, b);
		if (index >= 0)
		{
			LVITEM item;
			item.mask = LVIF_TEXT;
			item.pszText = dev->displayName;
			item.iItem = index;
			item.iSubItem = 0;
			SendMessage(hWndList, LVM_INSERTITEM, 0, (LPARAM)&item);
			item.mask = LVIF_TEXT;
			item.iSubItem = 1;
			item.pszText = dev->GetVirtualControlName(&dev->virtualControls[b->controlIndex]);
			SendMessage(hWndList, LVM_SETITEM, 0, (LPARAM)&item);
			item.iSubItem = 2;
			item.pszText = GetCommandStringW(b->command, port, slot);
			SendMessage(hWndList, LVM_SETITEM, 0, (LPARAM)&item);
		}
	}
	return index;
}

int ListBoundEffect(int port, int slot, Device* dev, ForceFeedbackBinding* b)
{
	int padtype = config.padConfigs[port][slot].type;
	if (!hWnds[port][slot][padtype])
		return -1;
	HWND hWndList = GetDlgItem(hWnds[port][slot][padtype], IDC_BINDINGS_LIST);
	int index = -1;
	if (hWndList)
	{
		index = GetItemIndex(port, slot, dev, b);
		if (index >= 0)
		{
			LVITEM item;
			item.mask = LVIF_TEXT;
			item.pszText = dev->displayName;
			item.iItem = index;
			item.iSubItem = 0;
			SendMessage(hWndList, LVM_INSERTITEM, 0, (LPARAM)&item);
			item.mask = LVIF_TEXT;
			item.iSubItem = 1;
			item.pszText = dev->ffEffectTypes[b->effectIndex].displayName;
			SendMessage(hWndList, LVM_SETITEM, 0, (LPARAM)&item);
			item.iSubItem = 2;
			wchar_t* ps2Motors[2] = {L"Big Motor", L"Small Motor"};
			item.pszText = ps2Motors[b->motor];
			SendMessage(hWndList, LVM_SETITEM, 0, (LPARAM)&item);
		}
	}
	return index;
}

// Only for use with control bindings.  Affects all highlighted bindings.
void ChangeValue(int port, int slot, int* newSensitivity, int* newRapidFire, int* newDeadZone, int* newSkipDeadZone)
{
	int padtype = config.padConfigs[port][slot].type;
	if (!hWnds[port][slot][padtype])
		return;
	HWND hWndList = GetDlgItem(hWnds[port][slot][padtype], IDC_BINDINGS_LIST);
	int count = ListView_GetSelectedCount(hWndList);
	if (count < 1)
		return;
	int index = -1;
	while (1)
	{
		index = ListView_GetNextItem(hWndList, index, LVNI_SELECTED);
		if (index < 0)
			break;
		Device* dev;
		Binding* b;
		ForceFeedbackBinding* ffb;
		if (!GetBinding(port, slot, index, dev, b, ffb) || ffb)
			return;
		if (newSensitivity)
		{
			// Don't change flip state when modifying multiple controls.
			if (count > 1 && b->sensitivity < 0)
				b->sensitivity = -*newSensitivity;
			else
				b->sensitivity = *newSensitivity;
		}
		if (newDeadZone)
		{
			if (b->deadZone)
			{
				b->deadZone = *newDeadZone;
			}
		}
		if (newSkipDeadZone)
		{
			if (b->skipDeadZone)
			{
				b->skipDeadZone = *newSkipDeadZone;
			}
		}
		if (newRapidFire)
		{
			b->rapidFire = *newRapidFire;
		}
	}
	PropSheet_Changed(hWndProp, hWnds[port][slot][padtype]);
	SelChanged(port, slot);
}

// Only for use with effect bindings.
void ChangeEffect(int port, int slot, int id, int* newForce, unsigned int* newEffectType)
{
	int padtype = config.padConfigs[port][slot].type;
	if (!hWnds[port][slot][padtype])
		return;
	HWND hWndList = GetDlgItem(hWnds[port][slot][padtype], IDC_BINDINGS_LIST);
	int i = ListView_GetSelectedCount(hWndList);
	if (i != 1)
		return;
	int index = ListView_GetNextItem(hWndList, -1, LVNI_SELECTED);
	Device* dev;
	Binding* b;
	ForceFeedbackBinding* ffb;
	if (!GetBinding(port, slot, index, dev, b, ffb) || b)
		return;
	if (newForce)
	{
		unsigned int axisIndex = (id - IDC_FF_AXIS1_ENABLED) / 16;
		if (axisIndex < (unsigned int)dev->numFFAxes)
		{
			ffb->axes[axisIndex].force = *newForce;
		}
	}
	if (newEffectType && *newEffectType < (unsigned int)dev->numFFEffectTypes)
	{
		ffb->effectIndex = *newEffectType;
		ListView_DeleteItem(hWndList, index);
		index = ListBoundEffect(port, slot, dev, ffb);
		ListView_SetItemState(hWndList, index, LVIS_SELECTED, LVIS_SELECTED);
	}
	PropSheet_Changed(hWndProp, hWnds[port][slot][padtype]);
	SelChanged(port, slot);
}



void Populate(int port, int slot, int padtype)
{
	if (!hWnds[port][slot][padtype])
		return;
	HWND hWnd = GetDlgItem(hWnds[port][slot][padtype], IDC_BINDINGS_LIST);
	ListView_DeleteAllItems(hWnd);

	int multipleBinding = config.multipleBinding;
	config.multipleBinding = 1;
	int selectedDevice = config.deviceSelect[port][slot];
	for (int j = 0; j < dm->numDevices; j++)
	{
		Device* dev = dm->devices[j];
		if (!dev->enabled || selectedDevice >= 0 && dm->devices[selectedDevice] != dev)
			continue;
		for (int i = 0; i < dev->pads[port][slot][padtype].numBindings; i++)
		{
			ListBoundCommand(port, slot, dev, dev->pads[port][slot][padtype].bindings + i);
		}
		for (int i = 0; i < dev->pads[port][slot][padtype].numFFBindings; i++)
		{
			ListBoundEffect(port, slot, dev, dev->pads[port][slot][padtype].ffBindings + i);
		}
	}
	config.multipleBinding = multipleBinding;

	hWnd = GetDlgItem(hWnds[port][slot][padtype], IDC_FORCEFEEDBACK);
	SendMessage(hWnd, CB_RESETCONTENT, 0, 0);
	int added = 0;
	bool enable = false;
	for (int i = 0; i < dm->numDevices; i++)
	{
		Device* dev = dm->devices[i];
		if (dev->enabled && dev->numFFAxes && dev->numFFEffectTypes)
		{
			SendMessage(hWnd, CB_INSERTSTRING, added, (LPARAM)dev->displayName);
			SendMessage(hWnd, CB_SETITEMDATA, added, i);
			added++;

			int selectedDevice = config.deviceSelect[port][slot];
			if (selectedDevice == -1 || dm->devices[selectedDevice] == dev)
			{
				enable = true;
			}
		}
	}
	SendMessage(hWnd, CB_SETCURSEL, 0, 0);
	EnableWindow(hWnd, added != 0 && enable);
	EnableWindow(GetDlgItem(hWnds[port][slot][padtype], ID_BIG_MOTOR), added != 0 && enable);
	EnableWindow(GetDlgItem(hWnds[port][slot][padtype], ID_SMALL_MOTOR), added != 0 && enable);
	SelChanged(port, slot);
}

int WritePrivateProfileInt(wchar_t* s1, wchar_t* s2, int v, wchar_t* ini)
{
	wchar_t temp[100];
	_itow(v, temp, 10);
	return WritePrivateProfileStringW(s1, s2, temp, ini);
}

int SaveSettings(wchar_t* file = 0)
{

	// Need this either way for saving path.
	if (!file)
	{
		file = iniFileUSB;
	}
	else
	{
		wchar_t* c = wcsrchr(file, '\\');
		if (*c)
		{
			*c = 0;
			wcscpy(config.lastSaveConfigPath, file);
			wcscpy(config.lastSaveConfigFileName, c + 1);
			*c = '\\';
		}
	}
	DeleteFileW(file);

	WritePrivateProfileStringW(L"General Settings", L"Last Config Path", config.lastSaveConfigPath, iniFileUSB);
	WritePrivateProfileStringW(L"General Settings", L"Last Config Name", config.lastSaveConfigFileName, iniFileUSB);

	// Just check first, last, and all pad bindings.  Should be more than enough.  No real need to check
	// config path.
	int noError = 1;

	for (int i = 0; i < sizeof(BoolOptionsInfo) / sizeof(BoolOptionsInfo[0]); i++)
	{
		noError &= WritePrivateProfileInt(L"General Settings", BoolOptionsInfo[i].name, config.bools[i], file);
	}

	WritePrivateProfileInt(L"General Settings", L"Keyboard Mode", config.keyboardApi, file);
	WritePrivateProfileInt(L"General Settings", L"Mouse Mode", config.mouseApi, file);

	WritePrivateProfileInt(L"Pad Settings", L"Configure On Bind", config.configureOnBind, file);

	for (int port = 0; port < 2; port++)
	{
		for (int slot = 0; slot < 4; slot++)
		{
			wchar_t temp[50];
			wsprintf(temp, L"Pad %i %i", port, slot);
			WritePrivateProfileInt(temp, L"Mode", config.padConfigs[port][slot].type, file);
		}
	}

	for (int i = 0; i < dm->numDevices; i++)
	{
		wchar_t id[50];
		wchar_t temp[50], temp2[1000];
		wsprintfW(id, L"Device %i", i);
		Device* dev = dm->devices[i];
		wchar_t* name = dev->displayName;
		while (name[0] == '[')
		{
			wchar_t* name2 = wcschr(name, ']');
			if (!name2)
				break;
			name = name2 + 1;
			while (iswspace(name[0]))
				name++;
		}
		WritePrivateProfileStringW(id, L"Display Name", name, file);
		WritePrivateProfileStringW(id, L"Instance ID", dev->instanceID, file);
		if (dev->productID)
		{
			WritePrivateProfileStringW(id, L"Product ID", dev->productID, file);
		}
		WritePrivateProfileInt(id, L"API", dev->api, file);
		WritePrivateProfileInt(id, L"Type", dev->type, file);
		int ffBindingCount = 0;
		int bindingCount = 0;
		for (int port = 0; port < 2; port++)
		{
			for (int slot = 0; slot < 4; slot++)
			{
				for (int padtype = 0; padtype < numPadTypes; padtype++)
				{
					for (int j = 0; j < dev->pads[port][slot][padtype].numBindings; j++)
					{
						Binding* b = dev->pads[port][slot][padtype].bindings + j;
						VirtualControl* c = &dev->virtualControls[b->controlIndex];
						wsprintfW(temp, L"Binding %i", bindingCount++);
						wsprintfW(temp2, L"0x%08X, %i, %i, %i, %i, %i, %i, %i, %i", c->uid, port, b->command, b->sensitivity, b->rapidFire, slot, b->deadZone, b->skipDeadZone, padtype);
						noError &= WritePrivateProfileStringW(id, temp, temp2, file);
					}
					for (int j = 0; j < dev->pads[port][slot][padtype].numFFBindings; j++)
					{
						ForceFeedbackBinding* b = dev->pads[port][slot][padtype].ffBindings + j;
						ForceFeedbackEffectType* eff = &dev->ffEffectTypes[b->effectIndex];
						wsprintfW(temp, L"FF Binding %i", ffBindingCount++);
						wsprintfW(temp2, L"%s %i, %i, %i, %i", eff->effectID, port, b->motor, slot, padtype);
						for (int k = 0; k < dev->numFFAxes; k++)
						{
							ForceFeedbackAxis* axis = dev->ffAxes + k;
							AxisEffectInfo* info = b->axes + k;
							wsprintfW(wcschr(temp2, 0), L", %i, %i", axis->id, info->force);
						}
						noError &= WritePrivateProfileStringW(id, temp, temp2, file);
					}
				}
			}
		}
	}
	if (!noError)
	{
		MessageBoxA(hWndProp, "Unable to save settings.  Make sure the disk is not full or write protected, the file isn't write protected, and that the app has permissions to write to the directory.  Try running in administrator mode.", "Error Writing Configuration File", MB_OK | MB_ICONERROR);
	}
	return !noError;
}

u8 GetPrivateProfileBool(wchar_t* s1, wchar_t* s2, int def, wchar_t* ini)
{
	return (0 != GetPrivateProfileIntW(s1, s2, def, ini));
}

int LoadSettings(int force, wchar_t* file)
{
	if (dm && !force)
		return 0;

	if (createIniDir)
	{
		PADsetSettingsDir("inis");
		createIniDir = false;
	}

	// Could just do ClearDevices() instead, but if I ever add any extra stuff,
	// this will still work.
	UnloadConfigs();
	dm = new InputDeviceManager();

	if (!file)
	{
		file = iniFileUSB;
		GetPrivateProfileStringW(L"General Settings", L"Last Config Path", L"inis", config.lastSaveConfigPath, sizeof(config.lastSaveConfigPath), file);
		GetPrivateProfileStringW(L"General Settings", L"Last Config Name", L"PAD.pad", config.lastSaveConfigFileName, sizeof(config.lastSaveConfigFileName), file);
	}
	else
	{
		wchar_t* c = wcsrchr(file, '\\');
		if (c)
		{
			*c = 0;
			wcscpy(config.lastSaveConfigPath, file);
			wcscpy(config.lastSaveConfigFileName, c + 1);
			*c = '\\';
			WritePrivateProfileStringW(L"General Settings", L"Last Config Path", config.lastSaveConfigPath, iniFileUSB);
			WritePrivateProfileStringW(L"General Settings", L"Last Config Name", config.lastSaveConfigFileName, iniFileUSB);
		}
	}

	for (int i = 0; i < sizeof(BoolOptionsInfo) / sizeof(BoolOptionsInfo[0]); i++)
	{
		config.bools[i] = GetPrivateProfileBool(L"General Settings", BoolOptionsInfo[i].name, BoolOptionsInfo[i].defaultValue, file);
	}

	config.keyboardApi = (DeviceAPI)GetPrivateProfileIntW(L"General Settings", L"Keyboard Mode", WM, file);
	if (!config.keyboardApi)
		config.keyboardApi = WM;
	config.mouseApi = (DeviceAPI)GetPrivateProfileIntW(L"General Settings", L"Mouse Mode", 0, file);

	config.configureOnBind = (DeviceAPI)GetPrivateProfileIntW(L"Pad Settings", L"Configure On Bind", 0, file);

	if (config.debug)
	{
		CreateDirectory(L"logs", 0);
	}

	for (int port = 0; port < 2; port++)
	{
		for (int slot = 0; slot < 4; slot++)
		{
			wchar_t temp[50];
			wsprintf(temp, L"Pad %i %i", port, slot);
			config.padConfigs[port][slot].type = (PadType)GetPrivateProfileInt(temp, L"Mode", Dualshock2Pad, file);
		}
	}

	bool oldIni = false;
	int i = 0;
	int multipleBinding = config.multipleBinding;
	// Disabling multiple binding only prevents new multiple bindings.
	config.multipleBinding = 1;
	while (1)
	{
		wchar_t id[50];
		wchar_t temp[50], temp2[1000], temp3[1000], temp4[1000];
		wsprintfW(id, L"Device %i", i++);
		if (!GetPrivateProfileStringW(id, L"Display Name", 0, temp2, 1000, file) || !temp2[0] ||
			!GetPrivateProfileStringW(id, L"Instance ID", 0, temp3, 1000, file) || !temp3[0])
		{
			if (i >= 100)
				break;
			continue;
		}
		wchar_t* id2 = 0;
		if (GetPrivateProfileStringW(id, L"Product ID", 0, temp4, 1000, file) && temp4[0])
			id2 = temp4;

		int api = GetPrivateProfileIntW(id, L"API", 0, file);
		int type = GetPrivateProfileIntW(id, L"Type", 0, file);
		if (!api || !type)
			continue;

		Device* dev = new Device((DeviceAPI)api, (DeviceType)type, temp2, temp3, id2);
		dev->attached = 0;
		dm->AddDevice(dev);
		int j = 0;
		int last = 0;
		while (1)
		{
			wsprintfW(temp, L"Binding %i", j++);
			if (!GetPrivateProfileStringW(id, temp, 0, temp2, 1000, file))
			{
				if (j >= 100)
				{
					if (!last)
						break;
					last = 0;
				}
				continue;
			}
			last = 1;
			unsigned int uid;
			int port, command, sensitivity, rapidFire, slot = 0, deadZone = 0, skipDeadZone = 0, padtype = 0;
			int w = 0;
			char string[1000];
			while (temp2[w])
			{
				string[w] = (char)temp2[w];
				w++;
			}
			string[w] = 0;
			int len = sscanf(string, " %i , %i , %i , %i , %i , %i , %i , %i , %i", &uid, &port, &command, &sensitivity, &rapidFire, &slot, &deadZone, &skipDeadZone, &padtype);
			if (len >= 5 && type)
			{
				VirtualControl* c = dev->GetVirtualControl(uid);
				if (!c)
					c = dev->AddVirtualControl(uid, -1);
				if (c)
				{
					if (len < 8)
					{ // If ini file is imported from older version, make sure bindings aren't applied to "Unplugged" padtype.
						oldIni = true;
						if (config.padConfigs[port][slot].type != 0)
						{
							padtype = config.padConfigs[port][slot].type;
						}
						else
						{
							padtype = 1;
						}
					}
					else if (len == 8)
					{
						padtype = skipDeadZone;
						skipDeadZone = 0;
					}
					BindCommand(dev, uid, port, slot, padtype, command, sensitivity, rapidFire, deadZone, skipDeadZone);
				}
			}
		}
		j = 0;
		while (1)
		{
			wsprintfW(temp, L"FF Binding %i", j++);
			if (!GetPrivateProfileStringW(id, temp, 0, temp2, 1000, file))
			{
				if (j >= 10)
				{
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
			while (temp2[w])
			{
				string[w] = (char)temp2[w];
				w++;
			}
			string[w] = 0;
			// wcstok not in ntdll.  More effore than its worth to shave off
			// whitespace without it.
			if (sscanf(string, " %100s %i , %i , %i , %i", effect, &port, &motor, &slot, &padtype) == 5)
			{
				char* s;
				if (oldIni)
				{ // Make sure bindings aren't applied to "Unplugged" padtype and FF settings are read from old location.
					if (config.padConfigs[port][slot].type != 0)
					{
						padtype = config.padConfigs[port][slot].type;
					}
					else
					{
						padtype = 1;
					}
					s = strchr(strchr(strchr(string, ',') + 1, ',') + 1, ',');
				}
				else
				{
					s = strchr(strchr(strchr(strchr(string, ',') + 1, ',') + 1, ',') + 1, ',');
				}
				if (!s)
					continue;
				s++;
				w = 0;
				while (effect[w])
				{
					temp2[w] = effect[w];
					w++;
				}
				temp2[w] = 0;
				ForceFeedbackEffectType* eff = dev->GetForcefeedbackEffect(temp2);
				if (!eff)
				{
					// At the moment, don't record effect types.
					// Only used internally, anyways, so not an issue.
					dev->AddFFEffectType(temp2, temp2, EFFECT_CONSTANT);
					// eff = &dev->ffEffectTypes[dev->numFFEffectTypes-1];
				}
				ForceFeedbackBinding* b;
				CreateEffectBinding(dev, temp2, port, slot, padtype, motor, &b);
				if (b)
				{
					while (1)
					{
						int axisID = atoi(s);
						if (!(s = strchr(s, ',')))
							break;
						s++;
						int force = atoi(s);
						int i;
						for (i = 0; i < dev->numFFAxes; i++)
						{
							if (axisID == dev->ffAxes[i].id)
								break;
						}
						if (i == dev->numFFAxes)
						{
							dev->AddFFAxis(L"?", axisID);
						}
						b->axes[i].force = force;
						if (!(s = strchr(s, ',')))
							break;
						s++;
					}
				}
			}
		}
	}
	config.multipleBinding = multipleBinding;

	RefreshEnabledDevicesAndDisplay(1);

	return 0;
}

inline int GetPort(HWND hWnd, int* slot)
{
	if (sizeof(hWnds) / sizeof(hWnds[0][0][0]) != (2 * 4 * numPadTypes))
		MessageBoxA(hWndProp, "The number of detected configurations does not equal the expected number of configurations.", "Pad configurations error", MB_OK | MB_ICONERROR);
	for (int j = 0; j < numPadTypes; j++)
	{
		for (int i = 0; i < 8; i++)
		{
			if (hWnds[i & 1][i >> 1][j] == hWnd)
			{
				*slot = i >> 1;
				return i & 1;
			}
		}
	}
	*slot = 0;
	return 0;
}

void Diagnostics(HWND hWnd)
{
	HWND hWndList = GetDlgItem(hWnd, IDC_DIAG_LIST);
	if (!hWndList)
		return;
	int index = ListView_GetNextItem(hWndList, -1, LVNI_SELECTED);
	if (index < 0)
		return;
	LVITEM item;
	memset(&item, 0, sizeof(item));
	item.mask = LVIF_PARAM;
	item.iSubItem = 0;
	item.iItem = index;
	if (!ListView_GetItem(hWndList, &item))
		return;
	Diagnose(item.lParam, hWnd);
	RefreshEnabledDevicesAndDisplay(0, hWnd, 1);
}

// Tooltips will only show if the TOOLINFO cbSize <= the struct size. If it's
// smaller some functionality might be disabled. So let's try and use the
// correct size.
UINT GetTooltipStructSize()
{
	DLLGETVERSIONPROC dllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(GetModuleHandle(L"ComCtl32.dll"), "DllGetVersion");
	if (dllGetVersion)
	{
		DLLVERSIONINFO2 dllversion = {0};
		dllversion.info1.cbSize = sizeof(DLLVERSIONINFO2);

		if (dllGetVersion((DLLVERSIONINFO*)&dllversion) == S_OK)
		{
			// Minor, then major version.
			DWORD version = MAKELONG(dllversion.info1.dwMinorVersion, dllversion.info1.dwMajorVersion);
			DWORD tooltip_v3 = MAKELONG(0, 6);
			if (version >= tooltip_v3)
				return TTTOOLINFOA_V3_SIZE;
		}
	}
	// Should be fine for XP and onwards, comctl versions >= 4.7 should at least
	// be this size.
	return TTTOOLINFOA_V2_SIZE;
}

void AddTooltip(UINT id, HWND hWnd)
{
	static UINT tooltipStructSize = GetTooltipStructSize();
	bool hasTooltip;
	LPWSTR message = pad_dialog_message(id, &hasTooltip);
	if (!hasTooltip)
		return;

	HWND hWndTooltip = GetDlgItem(hWnd, id);
	if (hWndTooltip == NULL)
		return;

	// TTS_NOPREFIX allows tabs and '&' to be used.
	HWND hWndTip = CreateWindowEx(WS_EX_TOPMOST, TOOLTIPS_CLASS, NULL,
								  TTS_ALWAYSTIP | TTS_NOPREFIX,
								  CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
								  hWnd, NULL, GetModuleHandle(nullptr), NULL);
	if (hWndTip == NULL)
		return;

	TOOLINFO toolInfo = {0};
	toolInfo.cbSize = tooltipStructSize;
	toolInfo.hwnd = hWnd;
	toolInfo.uFlags = TTF_IDISHWND | TTF_SUBCLASS;
	toolInfo.uId = (UINT_PTR)hWndTooltip;
	toolInfo.lpszText = message;
	SendMessage(hWndTip, TTM_ADDTOOL, 0, (LPARAM)&toolInfo);
	// 32.767s is the max show time.
	SendMessage(hWndTip, TTM_SETDELAYTIME, TTDT_AUTOPOP, 32767);
	// Setting a maximum width is necessary for displaying longer messages.
	SendMessage(hWndTip, TTM_SETMAXTIPWIDTH, 0, 400);
}

int GetBinding(int port, int slot, int index, Device*& dev, Binding*& b, ForceFeedbackBinding*& ffb)
{
	ffb = 0;
	b = 0;
	int padtype = config.padConfigs[port][slot].type;
	int selectedDevice = config.deviceSelect[port][slot];
	for (int i = 0; i < dm->numDevices; i++)
	{
		dev = dm->devices[i];
		if (!dev->enabled || selectedDevice >= 0 && dm->devices[selectedDevice] != dev)
			continue;
		if (index < dev->pads[port][slot][padtype].numBindings)
		{
			b = dev->pads[port][slot][padtype].bindings + index;
			return 1;
		}
		index -= dev->pads[port][slot][padtype].numBindings;

		if (index < dev->pads[port][slot][padtype].numFFBindings)
		{
			ffb = dev->pads[port][slot][padtype].ffBindings + index;
			return 1;
		}
		index -= dev->pads[port][slot][padtype].numFFBindings;
	}
	return 0;
}

// Only used when deleting things from ListView. Will remove from listview if needed.
void DeleteBinding(int port, int slot, int padtype, Device* dev, Binding* b)
{
	if (dev->enabled && hWnds[port][slot][padtype])
	{
		int count = GetItemIndex(port, slot, dev, b);
		if (count >= 0)
		{
			HWND hWndList = GetDlgItem(hWnds[port][slot][padtype], IDC_BINDINGS_LIST);
			if (hWndList)
			{
				ListView_DeleteItem(hWndList, count);
			}
		}
	}
	Binding* bindings = dev->pads[port][slot][padtype].bindings;
	int i = b - bindings;
	memmove(bindings + i, bindings + i + 1, sizeof(Binding) * (dev->pads[port][slot][padtype].numBindings - i - 1));
	dev->pads[port][slot][padtype].numBindings--;
}

void DeleteFFBinding(int port, int slot, Device* dev, ForceFeedbackBinding* b)
{
	int padtype = config.padConfigs[port][slot].type;
	if (dev->enabled && hWnds[port][slot][padtype])
	{
		int count = GetItemIndex(port, slot, dev, b);
		if (count >= 0)
		{
			HWND hWndList = GetDlgItem(hWnds[port][slot][padtype], IDC_BINDINGS_LIST);
			if (hWndList)
			{
				ListView_DeleteItem(hWndList, count);
			}
		}
	}
	ForceFeedbackBinding* bindings = dev->pads[port][slot][padtype].ffBindings;
	int i = b - bindings;
	memmove(bindings + i, bindings + i + 1, sizeof(Binding) * (dev->pads[port][slot][padtype].numFFBindings - i - 1));
	dev->pads[port][slot][padtype].numFFBindings--;
}

int DeleteByIndex(int port, int slot, int index)
{
	int padtype = config.padConfigs[port][slot].type;
	ForceFeedbackBinding* ffb;
	Binding* b;
	Device* dev;
	if (GetBinding(port, slot, index, dev, b, ffb))
	{
		if (b)
		{
			DeleteBinding(port, slot, padtype, dev, b);
		}
		else
		{
			DeleteFFBinding(port, slot, dev, ffb);
		}
		return 1;
	}
	return 0;
}

int DeleteSelected(int port, int slot)
{
	int padtype = config.padConfigs[port][slot].type;
	if (!hWnds[port][slot][padtype])
		return 0;
	HWND hWnd = GetDlgItem(hWnds[port][slot][padtype], IDC_BINDINGS_LIST);
	int changes = 0;
	while (1)
	{
		int index = ListView_GetNextItem(hWnd, -1, LVNI_SELECTED);
		if (index < 0)
			break;
		changes += DeleteByIndex(port, slot, index);
	}
	//ShowScrollBar(hWnd, SB_VERT, 1);
	return changes;
}

int CreateEffectBinding(Device* dev, wchar_t* effectID, unsigned int port, unsigned int slot, unsigned int padtype, unsigned int motor, ForceFeedbackBinding** binding)
{
	// Checks needed because I use this directly when loading bindings.
	// Note: dev->numFFAxes *can* be 0, for loading from file.
	*binding = 0;
	if (port > 1 || slot > 3 || motor > 1 || !dev->numFFEffectTypes)
	{
		return -1;
	}
	ForceFeedbackEffectType* eff = 0;
	if (effectID)
	{
		eff = dev->GetForcefeedbackEffect(effectID);
	}
	if (!eff)
	{
		eff = dev->ffEffectTypes;
	}
	if (!eff)
	{
		return -1;
	}
	int effectIndex = eff - dev->ffEffectTypes;
	dev->pads[port][slot][padtype].ffBindings = (ForceFeedbackBinding*)realloc(dev->pads[port][slot][padtype].ffBindings, (dev->pads[port][slot][padtype].numFFBindings + 1) * sizeof(ForceFeedbackBinding));
	int newIndex = dev->pads[port][slot][padtype].numFFBindings;
	while (newIndex && dev->pads[port][slot][padtype].ffBindings[newIndex - 1].motor >= motor)
	{
		dev->pads[port][slot][padtype].ffBindings[newIndex] = dev->pads[port][slot][padtype].ffBindings[newIndex - 1];
		newIndex--;
	}
	ForceFeedbackBinding* b = dev->pads[port][slot][padtype].ffBindings + newIndex;
	b->axes = (AxisEffectInfo*)calloc(dev->numFFAxes, sizeof(AxisEffectInfo));
	b->motor = motor;
	b->effectIndex = effectIndex;
	dev->pads[port][slot][padtype].numFFBindings++;
	if (binding)
		*binding = b;
	return ListBoundEffect(port, slot, dev, b);
}

int BindCommand(Device* dev, unsigned int uid, unsigned int port, unsigned int slot, unsigned int padtype, int command, int sensitivity, int rapidFire, int deadZone, int skipDeadZone)
{
	// Checks needed because I use this directly when loading bindings.
	if (port > 1 || slot > 3 || padtype >= numPadTypes)
		return -1;

	if (!sensitivity)
	{
		if (((uid >> 16) & 0xFF) == ABSAXIS)
		{
			sensitivity = BASE_ANALOG_SENSITIVITY;
		}
		else
		{
			sensitivity = BASE_SENSITIVITY;
		}
	}
	if ((uid >> 16) & (PSHBTN | TGLBTN))
	{
		deadZone = 0;
		skipDeadZone = 0;
	}
	else if (!deadZone)
	{
		if ((uid >> 16) & PRESSURE_BTN)
		{
			deadZone = 1;
		}
		else
		{
			deadZone = DEFAULT_DEADZONE;
		}
	}
	else if (!skipDeadZone)
	{
		skipDeadZone = 1;
	}

	// Relative axes can have negative sensitivity.
	else if (((uid >> 16) & 0xFF) == RELAXIS)
	{
		sensitivity = abs(sensitivity);
	}
	VirtualControl* c = dev->GetVirtualControl(uid);
	if (!c)
		return -1;
	// Add before deleting.  Means I won't scroll up one line when scrolled down to bottom.
	int controlIndex = c - dev->virtualControls;
	int index = 0;
	PadBindings* p = dev->pads[port][slot] + padtype;
	p->bindings = (Binding*)realloc(p->bindings, (p->numBindings + 1) * sizeof(Binding));
	for (index = p->numBindings; index > 0; index--)
	{
		if (p->bindings[index - 1].controlIndex < controlIndex)
			break;
		p->bindings[index] = p->bindings[index - 1];
	}
	Binding* b = p->bindings + index;
	p->numBindings++;
	b->command = command;
	b->controlIndex = controlIndex;
	b->rapidFire = rapidFire;
	b->sensitivity = sensitivity;
	b->deadZone = deadZone;
	b->skipDeadZone = skipDeadZone;
	// Where it appears in listview.
	int count = ListBoundCommand(port, slot, dev, b);

	int newBindingIndex = index;
	index = 0;
	while (index < p->numBindings)
	{
		if (index == newBindingIndex)
		{
			index++;
			continue;
		}
		b = p->bindings + index;
		int nuke = 0;
		if (config.multipleBinding)
		{
			if (b->controlIndex == controlIndex && b->command == command)
				nuke = 1;
		}
		else
		{
			int uid2 = dev->virtualControls[b->controlIndex].uid;
			if (b->controlIndex == controlIndex || (!((uid2 ^ uid) & 0xFFFFFF) && ((uid | uid2) & (UID_POV | UID_AXIS))))
				nuke = 1;
		}
		if (!nuke)
		{
			index++;
			continue;
		}
		if (index < newBindingIndex)
		{
			newBindingIndex--;
			count--;
		}
		DeleteBinding(port, slot, padtype, dev, b);
	}
	if (!config.multipleBinding)
	{
		for (int port2 = 0; port2 < 2; port2++)
		{
			for (int slot2 = 0; slot2 < 4; slot2++)
			{
				if (port2 == port && slot2 == slot)
					continue;
				int padtype2 = config.padConfigs[port2][slot2].type;
				PadBindings* p = dev->pads[port2][slot2] + padtype2;
				for (int i = 0; i < p->numBindings; i++)
				{
					Binding* b = p->bindings + i;
					int uid2 = dev->virtualControls[b->controlIndex].uid;
					if (b->controlIndex == controlIndex || (!((uid2 ^ uid) & 0xFFFFFF) && ((uid | uid2) & (UID_POV | UID_AXIS))))
					{
						DeleteBinding(port2, slot2, padtype2, dev, b);
						i--;
					}
				}
			}
		}
	}

	return count;
}

// Does nothing, but makes sure I'm overriding the dialog's window proc, to block
// default key handling.
ExtraWndProcResult DoNothingWndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* output)
{
	return CONTINUE_BLISSFULLY;
}

void EndBinding(HWND hWnd)
{
	if (selected)
	{
		KillTimer(hWnd, 1);

		int needRefreshDevices = 0;
		// If binding ignore keyboard, this will disable it and enable other devices.
		// 0xFF is used for testing force feedback.  I disable all other devices first
		// just so I don't needlessly steal the mouse.
		if (selected == 0x7F || selected == 0xFF)
		{
			needRefreshDevices = 1;
		}
		selected = 0;

		quickSetup = false;
		SetWindowTextW(GetDlgItem(hWnd, ID_QUICK_SETUP), L"Quick Setup");
		SetWindowTextW(GetDlgItem(hWnd, IDC_QUICK_SETUP_TEXT), L"");

		dm->ReleaseInput();
		ClearKeyQueue();
		hWndButtonProc.Release();

		// Safest to do this last.
		if (needRefreshDevices)
		{
			RefreshEnabledDevices();
		}
	}
}


INT_PTR CALLBACK DialogProc(HWND hWnd, unsigned int msg, WPARAM wParam, LPARAM lParam)
{
	int index = (hWnd == PropSheet_IndexToHwnd(hWndProp, 1));
	int slot;
	int port = GetPort(hWnd, &slot);
	int padtype = config.padConfigs[port][slot].type;
	HWND hWndList = GetDlgItem(hWnd, IDC_BINDINGS_LIST);
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			ListView_SetExtendedListViewStyleEx(hWndList, LVS_EX_DOUBLEBUFFER, LVS_EX_DOUBLEBUFFER);
			LVCOLUMN c;
			c.mask = LVCF_TEXT | LVCF_WIDTH;
			c.cx = 90;
			c.pszText = L"Device";
			ListView_InsertColumn(hWndList, 0, &c);
			c.cx = 70;
			c.pszText = L"PC Control";
			ListView_InsertColumn(hWndList, 1, &c);
			c.pszText = L"PS2 Control";
			ListView_InsertColumn(hWndList, 2, &c);
			selected = 0;
			port = (int)((PROPSHEETPAGE*)lParam)->lParam & 1;
			slot = (int)((PROPSHEETPAGE*)lParam)->lParam >> 1;
			padtype = config.padConfigs[port][slot].type;
			hWnds[port][slot][padtype] = hWnd;
			SendMessage(hWndList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);
			SetupLogSlider(GetDlgItem(hWnd, IDC_SLIDER_SENSITIVITY));
			SetupLogSlider(GetDlgItem(hWnd, IDC_SLIDER_DEADZONE));
			SetupLogSlider(GetDlgItem(hWnd, IDC_SLIDER_SKIP_DEADZONE));
			CheckDlgButton(hWnd, IDC_CONFIGURE_ON_BIND, BST_CHECKED * config.configureOnBind);

			AddTooltip(IDC_BINDINGS_LIST, hWnd);
			AddTooltip(IDC_DEVICE_SELECT, hWnd);
			AddTooltip(IDC_CONFIGURE_ON_BIND, hWnd);
			AddTooltip(ID_MOUSE, hWnd);
			AddTooltip(ID_ANALOG, hWnd);
			AddTooltip(ID_LOCK_ALL_INPUT, hWnd);
			AddTooltip(ID_LOCK_DIRECTION, hWnd);
			AddTooltip(ID_LOCK_BUTTONS, hWnd);
			AddTooltip(ID_TURBO_KEY, hWnd);
			AddTooltip(ID_EXCLUDE, hWnd);
			AddTooltip(IDC_RAPID_FIRE, hWnd);
			AddTooltip(IDC_FLIP, hWnd);
			AddTooltip(IDC_SLIDER_DEADZONE, hWnd);
			AddTooltip(IDC_SLIDER_SKIP_DEADZONE, hWnd);
			AddTooltip(IDC_SLIDER_SENSITIVITY, hWnd);

			config.bind = false;
			config.deviceSelect[port][slot] = -1;
			HWND hWndDS = GetDlgItem(hWnds[port][slot][padtype], IDC_DEVICE_SELECT);
			SendMessage(hWndDS, CB_RESETCONTENT, 0, 0);
			int added = 0;
			SendMessage(hWndDS, CB_INSERTSTRING, added, (LPARAM)L"Allow All Devices");
			SendMessage(hWndDS, CB_SETITEMDATA, added, -1);
			added++;
			for (int i = 0; i < dm->numDevices; i++)
			{
				Device* dev = dm->devices[i];
				if (dev->enabled && dev->api != IGNORE_KEYBOARD)
				{
					SendMessage(hWndDS, CB_INSERTSTRING, added, (LPARAM)dev->displayName);
					SendMessage(hWndDS, CB_SETITEMDATA, added, i);
					added++;
				}
			}
			SendMessage(hWndDS, CB_SETCURSEL, 0, 0);
			EnableWindow(hWndDS, added != 0);

			Populate(port, slot, padtype);
			ListView_SetColumnWidth(hWndList, 2, LVSCW_AUTOSIZE_USEHEADER);
		}
		break;
		case WM_DEVICECHANGE:
			if (wParam == DBT_DEVNODES_CHANGED)
			{
				EndBinding(hWnd);
				RefreshEnabledDevicesAndDisplay(1, hWndGeneral, 1);
			}
			break;
		case WM_TIMER:
			// ignore generic timer callback and handle the hwnd-specific one which comes later
			if (hWnd == 0)
				return 0;

			if (!selected || selected == 0xFF)
			{
				// !selected is mostly for device added/removed when binding.
				selected = 0xFF;
				EndBinding(hWnd);
			}
			else
			{
				unsigned int uid;
				int value;

				// The old code re-bound our button hWndProcEater to GetDlgItem(hWnd, selected).  But at best hWnd
				// was null (WM_TIMER is passed twice, once with a null parameter, and a second time that is hWnd
				// specific), and at worst 'selected' is a post-processed code "based" on cmd, so GetDlgItem
				// *always* returned NULL anyway.  This resulted in hWndButton being null, which meant Device code
				// used hWnd instead.  This may have caused odd behavior since the callbacks were still all eaten
				// by the initial GetDlgItem(hWnd, cmd) selection made when the timer was initialized.

				InitInfo info = {selected == 0x7F, 1, hWndProp, &hWndButtonProc};
				Device* dev = dm->GetActiveDevice(&info, &uid, &index, &value);
				static unsigned int prev_uid = !uid;

				if (dev == nullptr || (quickSetup && (uid == prev_uid || uid & UID_AXIS)))
					return 0;

				//Check the bindings for an excluded input, and ignore it if found.
				PadBindings* p_c = dev->pads[port][slot] + padtype;
				for (int i = 0; i < p_c->numBindings; i++)
				{
					Binding* b2 = p_c->bindings + i;
					int uid2 = dev->virtualControls[b2->controlIndex].uid;
					if (b2->command == 0x29 && uid == uid2)
					{
						return 0;
					}
				}

				int command = selected;
				// Good idea to do this first, as BindCommand modifies the ListView, which will
				// call it anyways, which is a bit funky.
				if (!quickSetup)
					EndBinding(hWnd);
				UnselectAll(hWndList);
				int index = -1;
				if (command < 0x30)
				{
					index = BindCommand(dev, uid, port, slot, padtype, command, BASE_SENSITIVITY, 0, 0, 0);
				}
				if (index >= 0)
				{
					PropSheet_Changed(hWndProp, hWnds[port][slot][padtype]);
					ListView_SetItemState(hWndList, index, LVIS_SELECTED, LVIS_SELECTED);
					ListView_EnsureVisible(hWndList, index, 0);
					config.bind = true;
					ListView_SetColumnWidth(hWndList, 2, LVSCW_AUTOSIZE_USEHEADER);

					if (quickSetup)
					{
						HWND hWndS = GetDlgItem(hWnds[port][slot][padtype], selected + 0x40F0);
						for (selected++; selected <= (ID_ANALOG - (ID_SELECT - 0x10)); selected++)
						{
							HWND hWndS = GetDlgItem(hWnds[port][slot][padtype], selected + 0x40F0);
							if (hWndS)
							{
								break;
							}
						}
						if (!hWndS || selected > (ID_ANALOG - (ID_SELECT - 0x10)))
						{
							prev_uid = !uid;
							EndBinding(hWnd);
							SetWindowTextW(GetDlgItem(hWnd, IDC_QUICK_SETUP_TEXT), L"Finished");
						}
						else
						{
							wchar_t temp[60];
							wsprintfW(temp, L"Please enter an input for %ws", GetCommandStringW(selected, port, slot));
							SetWindowTextW(GetDlgItem(hWnd, IDC_QUICK_SETUP_TEXT), temp);
							Sleep(80);
							for (int i = 0; i < dm->numDevices; i++)
							{
								if (dm->devices[i]->active && (dm->devices[i]->api != WM || dm->devices[i]->api != RAW))
									dm->devices[i]->Deactivate();
							}
							prev_uid = uid;
						}
					}
					else
					{
						SetWindowTextW(GetDlgItem(hWnd, IDC_QUICK_SETUP_TEXT), L"Succeeded");
					}
				}
			}
			break;
		case WM_SYSKEYDOWN:
		case WM_KEYDOWN:
			EndBinding(hWnd);
			break;
		case PSM_CANCELTOCLOSE:
			// Load();
			break;
		case WM_ACTIVATE:
			EndBinding(hWnd);
			break;
		case WM_NOTIFY:
		{
			PSHNOTIFY* n = (PSHNOTIFY*)lParam;
			if (n->hdr.hwndFrom == hWndProp)
			{
				switch (n->hdr.code)
				{
					case PSN_QUERYCANCEL:
					case PSN_KILLACTIVE:
						EndBinding(hWnd);
						return 0;
					case PSN_SETACTIVE:
						SetWindowTextW(GetDlgItem(hWnd, IDC_QUICK_SETUP_TEXT), L"");
						ListView_SetColumnWidth(hWndList, 2, LVSCW_AUTOSIZE_USEHEADER);
						return 0;
					case PSN_APPLY:
						SetWindowLongPtr(hWnd, DWLP_MSGRESULT, PSNRET_NOERROR);
						return 1;
				}
				break;
			}
			else if (n->hdr.idFrom == IDC_BINDINGS_LIST)
			{
				static int NeedUpdate = 0;
				if (n->hdr.code == LVN_KEYDOWN)
				{
					NMLVKEYDOWN* key = (NMLVKEYDOWN*)n;
					if (key->wVKey == VK_DELETE ||
						key->wVKey == VK_BACK)
					{

						if (DeleteSelected(port, slot))
						{
							PropSheet_Changed(hWndProp, hWnds[0]);
							ListView_SetColumnWidth(hWndList, 2, LVSCW_AUTOSIZE_USEHEADER);
						}
					}
				}
				// Update sensitivity and motor/binding display on redraw
				// rather than on itemchanged.  This reduces blinking, as
				// I get 3 LVN_ITEMCHANGED messages, and first is sent before
				// the new item is set as being selected.
				else if (n->hdr.code == LVN_ITEMCHANGED)
				{
					NeedUpdate = 1;
				}
				else if (n->hdr.code == NM_CUSTOMDRAW && NeedUpdate)
				{
					NeedUpdate = 0;
					SelChanged(port, slot);
				}
				if (n->hdr.code == NM_RCLICK)
				{
					HMENU hMenu = CreatePopupMenu();
					if (!hMenu)
						break;
					MENUITEMINFOW info;
					wchar_t text[40];
					info.cbSize = sizeof(info);
					info.fMask = MIIM_STRING | MIIM_ID;
					info.dwTypeData = text;
					info.wID = 1;
					wsprintfW(text, L"Delete Selected");
					InsertMenuItemW(hMenu, 0, 1, &info);
					info.wID = 2;
					wsprintfW(text, L"Clear All");
					InsertMenuItemW(hMenu, 1, 1, &info);
					POINT pos;
					GetCursorPos(&pos);
					short res = TrackPopupMenuEx(hMenu, TPM_NONOTIFY | TPM_RETURNCMD, pos.x, pos.y, hWndProp, 0);
					DestroyMenu(hMenu);
					if (res == 0)
						break;
					else if (res == 1)
					{
						if (DeleteSelected(port, slot))
							PropSheet_Changed(hWndProp, hWnd);
					}
					else if (res == 2)
					{
						while (DeleteByIndex(port, slot, 0))
							PropSheet_Changed(hWndProp, hWnd);
					}
				}
			}
		}
		break;
		case WM_HSCROLL:
		{
			int id = GetDlgCtrlID((HWND)lParam);
			int val = GetLogSliderVal(hWnd, id);
			if (id == IDC_SLIDER_SENSITIVITY)
			{
				ChangeValue(port, slot, &val, 0, 0, 0);
			}
			else if (id == IDC_SLIDER_DEADZONE)
			{
				ChangeValue(port, slot, 0, 0, &val, 0);
			}
			else if (id == IDC_SLIDER_SKIP_DEADZONE)
			{
				ChangeValue(port, slot, 0, 0, 0, &val);
			}
			else
			{
				ChangeEffect(port, slot, id, &val, 0);
			}
		}
		break;
		case WM_COMMAND:
			if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_AXIS_DIRECTION)
			{
				int index = ListView_GetNextItem(hWndList, -1, LVNI_SELECTED);
				if (index >= 0)
				{
					int cbsel = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
					if (cbsel >= 0)
					{
						ForceFeedbackBinding* ffb;
						Binding* b;
						Device* dev;
						if (GetBinding(port, slot, index, dev, b, ffb))
						{
							const static unsigned int axisUIDs[3] = {UID_AXIS_NEG, UID_AXIS_POS, UID_AXIS};
							int uid = dev->virtualControls[b->controlIndex].uid;
							uid = (uid & 0x00FFFFFF) | axisUIDs[cbsel];
							Binding backup = *b;
							DeleteSelected(port, slot);
							int index = BindCommand(dev, uid, port, slot, padtype, backup.command, backup.sensitivity, backup.rapidFire, backup.deadZone, backup.skipDeadZone);
							ListView_SetItemState(hWndList, index, LVIS_SELECTED, LVIS_SELECTED);
							PropSheet_Changed(hWndProp, hWnd);
						}
					}
				}
			}
			else if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_DEVICE_SELECT)
			{
				int selectedDev = SendMessage(GetDlgItem(hWnd, IDC_DEVICE_SELECT), CB_GETCURSEL, 0, 0);
				config.deviceSelect[port][slot] = SendMessage(GetDlgItem(hWnd, IDC_DEVICE_SELECT), CB_GETITEMDATA, selectedDev, 0);
				RefreshEnabledDevicesAndDisplay(1, hWndGeneral, 1);
				if (selectedDev > 0)
				{
					for (int i = 0; i < dm->numDevices; i++)
					{
						Device* dev = dm->devices[config.deviceSelect[port][slot]];
						if (dm->devices[i] != dev)
						{
							dm->DisableDevice(i);
						}
						else
						{
							dm->EnableDevice(i);
						}
					}
				}
				ListView_SetColumnWidth(hWndList, 2, LVSCW_AUTOSIZE_USEHEADER);
			}
			else if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_FF_EFFECT)
			{
				int typeIndex = SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0);
				if (typeIndex >= 0)
					ChangeEffect(port, slot, 0, 0, (unsigned int*)&typeIndex);
			}
			else if (HIWORD(wParam) == BN_CLICKED)
			{
				int cmd = LOWORD(wParam);
				if (quickSetup && cmd == ID_QUICK_SETUP)
				{
					SetWindowTextW(GetDlgItem(hWnd, IDC_QUICK_SETUP_TEXT), L"Stopped");
					EndBinding(hWnd);
					return 0;
				}
				EndBinding(hWnd);
				if (cmd == ID_BIG_MOTOR || cmd == ID_SMALL_MOTOR)
				{
					int i = (int)SendMessage(GetDlgItem(hWnd, IDC_FORCEFEEDBACK), CB_GETCURSEL, 0, 0);
					if (i >= 0)
					{
						unsigned int index = (unsigned int)SendMessage(GetDlgItem(hWnd, IDC_FORCEFEEDBACK), CB_GETITEMDATA, i, 0);
						if (index < (unsigned int)dm->numDevices)
						{
							Device* dev = dm->devices[index];
							ForceFeedbackBinding* b;
							wchar_t* effectID = 0;
							if (dev->api == DI)
							{
								// Constant effect.
								if (cmd == ID_BIG_MOTOR)
									effectID = L"13541C20-8E33-11D0-9AD0-00A0C9A06E35";
								// Square.
								else
									effectID = L"13541C22-8E33-11D0-9AD0-00A0C9A06E35";
							}
							int count = CreateEffectBinding(dev, effectID, port, slot, padtype, cmd - ID_BIG_MOTOR, &b);
							if (b)
							{
								int needSet = 1;
								if (dev->api == XINPUT && dev->numFFAxes == 2)
								{
									needSet = 0;
									if (cmd == ID_BIG_MOTOR)
									{
										b->axes[0].force = BASE_SENSITIVITY;
									}
									else
									{
										b->axes[1].force = BASE_SENSITIVITY;
									}
								}
								else if (dev->api == DS3 && dev->numFFAxes == 2)
								{
									needSet = 0;
									if (cmd == ID_BIG_MOTOR)
									{
										b->axes[0].force = BASE_SENSITIVITY;
									}
									else
									{
										b->axes[1].force = BASE_SENSITIVITY;
									}
								}
								else if (dev->api == DI)
								{
									int bigIndex = 0, littleIndex = 0;
									int j;
									for (j = 0; j < dev->numFFAxes; j++)
									{
										// DI object instance.  0 is x-axis, 1 is y-axis.
										int instance = (dev->ffAxes[j].id >> 8) & 0xFFFF;
										if (instance == 0)
										{
											bigIndex = j;
										}
										else if (instance == 1)
										{
											littleIndex = j;
										}
									}
									needSet = 0;
									if (cmd == ID_BIG_MOTOR)
									{
										b->axes[bigIndex].force = BASE_SENSITIVITY;
										b->axes[littleIndex].force = 1;
									}
									else
									{
										b->axes[bigIndex].force = 1;
										b->axes[littleIndex].force = BASE_SENSITIVITY;
									}
								}
								if (needSet)
								{
									for (int j = 0; j < 2 && j < dev->numFFAxes; j++)
									{
										b->axes[j].force = BASE_SENSITIVITY;
									}
								}
								UnselectAll(hWndList);
								ListView_SetItemState(hWndList, count, LVIS_SELECTED, LVIS_SELECTED);
								ListView_EnsureVisible(hWndList, count, 0);
							}
							PropSheet_Changed(hWndProp, hWnd);
						}
					}
				}
				else if (cmd == ID_CONTROLS)
				{
					UnselectAll(hWndList);
					SetWindowTextW(GetDlgItem(hWnd, IDC_QUICK_SETUP_TEXT), L"");
					ListView_SetColumnWidth(hWndList, 2, LVSCW_AUTOSIZE_USEHEADER);
				}
				else if (cmd == ID_TEST)
				{
					// Just in case...
					if (selected)
						break;
					int selIndex = ListView_GetNextItem(hWndList, -1, LVNI_SELECTED);
					if (selIndex >= 0)
					{
						Device* dev;
						Binding* b;
						ForceFeedbackBinding* ffb = 0;
						if (GetBinding(port, slot, selIndex, dev, b, ffb))
						{
							selected = 0xFF;
							hWndButtonProc.SetWndHandle(GetDlgItem(hWnd, cmd));
							InitInfo info = {0, 1, hWndProp, &hWndButtonProc};
							for (int i = 0; i < dm->numDevices; i++)
							{
								if (dm->devices[i] != dev)
								{
									dm->DisableDevice(i);
								}
							}
							dm->Update(&info);
							dm->PostRead();
							dev->SetEffect(ffb, 255);
							Sleep(200);
							dm->Update(&info);
							SetTimer(hWnd, 1, 3000, 0);
						}
					}
				}
				else if (cmd >= IDC_DPAD && cmd <= ID_QUICK_SETUP)
				{ // || cmd == ID_FORCE_FEEDBACK) {
					// Messes up things, unfortunately.
					// End binding on a bunch of notification messages, and
					// this will send a bunch.
					// UnselectAll(hWndList);
					EndBinding(hWnd);
					SetWindowTextW(GetDlgItem(hWnd, IDC_QUICK_SETUP_TEXT), L"");

					RefreshEnabledDevices(0);
					int selectedDev = SendMessage(GetDlgItem(hWnd, IDC_DEVICE_SELECT), CB_GETCURSEL, 0, 0);
					if (selectedDev > 0)
					{
						for (int i = 0; i < dm->numDevices; i++)
						{
							Device* dev = dm->devices[config.deviceSelect[port][slot]];
							if (dm->devices[i] != dev)
							{
								dm->DisableDevice(i);
							}
							else
							{
								dm->EnableDevice(i);
							}
						}
					}

					if (cmd != ID_QUICK_SETUP)
					{
						selected = cmd - (ID_SELECT - 0x10);
					}
					else
					{
						quickSetup = true;
						HWND hWndS;
						for (selected = 0x10; selected <= (ID_ANALOG - (ID_SELECT - 0x10)); selected++)
						{
							hWndS = GetDlgItem(hWnds[port][slot][padtype], selected + 0x40F0);
							if (hWndS)
							{
								break;
							}
						}
						if (hWndS)
						{
							SetWindowTextW(GetDlgItem(hWnd, ID_QUICK_SETUP), L"Stop");
						}
						else
						{
							EndBinding(hWnd);
							return 0;
						}
					}
					wchar_t temp[60];
					wsprintfW(temp, L"Please enter an input for %ws", GetCommandStringW(selected, port, slot));
					SetWindowTextW(GetDlgItem(hWnd, IDC_QUICK_SETUP_TEXT), temp);

					hWndButtonProc.SetWndHandle(GetDlgItem(hWnd, cmd));
					hWndButtonProc.Eat(DoNothingWndProc, 0);
					InitInfo info = {selected == 0x7F, 1, hWndProp, &hWndButtonProc};
					int w = timeGetTime();
					dm->Update(&info);
					dm->PostRead();
					// Workaround for things that return 0 on first poll and something else ever after.
					Sleep(80);
					dm->Update(&info);
					dm->PostRead();
					SetTimer(hWnd, 1, 30, 0);
				}
				else if (cmd == IDC_CONFIGURE_ON_BIND)
				{
					config.configureOnBind = IsDlgButtonChecked(hWnd, IDC_CONFIGURE_ON_BIND);
				}
				else if (cmd == ID_SPECIAL_INPUTS)
				{
					config.specialInputs[port][slot] = !config.specialInputs[port][slot];
					// FIXME: Fix DPI Scaling. Below code is commented out and a fixed size for inputs window is used to avoid DPI scaling issues.
					// SetWindowPos(hWndList, NULL, 0, 0, 275, config.specialInputs[port][slot] ? 324 : 440, SWP_NOZORDER | SWP_NOMOVE);
					// ListView_SetColumnWidth(hWndList, 2, LVSCW_AUTOSIZE_USEHEADER);
					// UnselectAll(hWndList);
					SelChanged(port, slot);
				}
				if (cmd == IDC_RAPID_FIRE)
				{
					// Don't allow setting it back to indeterminate.
					SendMessage(GetDlgItem(hWnd, IDC_RAPID_FIRE), BM_SETSTYLE, BS_AUTOCHECKBOX, 0);
					int rapidFire = (IsDlgButtonChecked(hWnd, IDC_RAPID_FIRE) == BST_CHECKED);
					ChangeValue(port, slot, 0, &rapidFire, 0, 0);
				}
				else if (cmd == IDC_FLIP)
				{
					int val = GetLogSliderVal(hWnd, IDC_SLIDER_SENSITIVITY);
					ChangeValue(port, slot, &val, 0, 0, 0);
				}
				else if (cmd == ID_RESET_CONFIG)
				{
					// Just in case...
					if (selected)
						break;
					int selIndex = ListView_GetNextItem(hWndList, -1, LVNI_SELECTED);
					if (selIndex >= 0)
					{
						Device* dev;
						Binding* b;
						ForceFeedbackBinding* ffb = 0;
						if (GetBinding(port, slot, selIndex, dev, b, ffb))
						{
							selected = 0xFF;
							if (b)
							{
								VirtualControl* control = &dev->virtualControls[b->controlIndex];
								for (int i = IDC_SLIDER_SENSITIVITY; i <= IDC_SLIDER_SKIP_DEADZONE; i++)
								{
									if (i == IDC_SLIDER_SENSITIVITY)
									{
										int val = BASE_SENSITIVITY;
										if (((control->uid >> 16) & 0xFF) == ABSAXIS)
										{
											val = BASE_ANALOG_SENSITIVITY;
										}
										ChangeValue(port, slot, &val, 0, 0, 0);
									}
									else if (i == IDC_SLIDER_DEADZONE)
									{
										int val = DEFAULT_DEADZONE;
										if ((control->uid >> 16) & (PSHBTN | TGLBTN))
										{
											val = 0;
										}
										else if ((control->uid >> 16) & PRESSURE_BTN)
										{
											val = 1;
										}
										ChangeValue(port, slot, 0, 0, &val, 0);
									}
									else if (i == IDC_SLIDER_SKIP_DEADZONE)
									{
										int val = 1;
										if ((control->uid >> 16) & (PSHBTN | TGLBTN))
										{
											val = 0;
										}
										ChangeValue(port, slot, 0, 0, 0, &val);
									}
									else if (i == IDC_RAPID_FIRE)
									{
										int val = 0;
										ChangeValue(port, slot, 0, &val, 0, 0);
									}
								}
							}
							else if (ffb)
							{
								for (int i = IDC_FF_AXIS1; i <= IDC_FF_AXIS8; i += 0x10)
								{
									int val = BASE_SENSITIVITY;
									if (ffb->motor != ((i - IDC_FF_AXIS1) / 0x10) || IsDlgButtonChecked(hWnd, i - 1) != BST_CHECKED)
									{
										val = 0;
									}
									ChangeEffect(port, slot, i, &val, 0);
								}
							}
						}
					}
				}
				else if (cmd >= IDC_FF_AXIS1_ENABLED && cmd < IDC_FF_AXIS8_ENABLED + 16)
				{
					int index = (cmd - IDC_FF_AXIS1_ENABLED) / 16;
					int val = GetLogSliderVal(hWnd, 16 * index + IDC_FF_AXIS1);
					if (IsDlgButtonChecked(hWnd, 16 * index + IDC_FF_AXIS1_ENABLED) != BST_CHECKED)
					{
						val = 0;
					}
					ChangeEffect(port, slot, cmd, &val, 0);
				}
			}
			break;
		default:
			break;
	}
	return 0;
}

// Returns 0 if pad doesn't exist due to mtap settings, as a convenience.
int GetPadString(wchar_t* string, unsigned int port, unsigned int slot)
{
	if (!slot && !config.multitap[port])
	{
		wsprintfW(string, L"Pad %i", port + 1);
	}
	else
	{
		wsprintfW(string, L"Pad %i%c", port + 1, 'A' + slot);
		if (!config.multitap[port])
			return 0;
	}
	return 1;
}

// Returns 0 if pad doesn't exist due to mtap settings, as a convenience.
int GetPadTypeString(wchar_t* string, unsigned int port, unsigned int slot, unsigned int padtype)
{
	if (!slot && !config.multitap[port])
	{
		wsprintfW(string, L"Pad %i, %s", port + 1, padTypes[padtype]);
	}
	else
	{
		wsprintfW(string, L"Pad %i%c, %s", port + 1, 'A' + slot, padTypes[padtype]);
		if (!config.multitap[port])
			return 0;
	}
	return 1;
}

bool ProfilesBindingCheck(unsigned int port, unsigned int slot, unsigned int padtype)
{
	bool showWarning = true;
	int count = 0;
	for (int i = 0; i < dm->numDevices; i++)
	{
		for (int portCheck = 0; portCheck < 2; portCheck++)
		{
			int maxslot = 1;
			if (config.multitap[portCheck])
				maxslot = 4;
			if (portCheck == port && !config.multitap[portCheck])
				continue;
			for (int slotCheck = 0; slotCheck < maxslot; slotCheck++)
			{
				if (portCheck == port && slotCheck == slot)
					continue;
				int padtypeCheck = config.padConfigs[portCheck][slotCheck].type;
				PadBindings* pCheck = dm->devices[i]->pads[portCheck][slotCheck] + padtypeCheck;
				PadBindings* pActive = dm->devices[i]->pads[port][slot] + padtype;
				for (int j = 0; j < pCheck->numBindings; j++)
				{
					for (int k = 0; k < pActive->numBindings; k++)
					{
						Binding* bCheck = pCheck->bindings + j;
						Binding* bActive = pActive->bindings + k;
						int uidCheck = dm->devices[i]->virtualControls[bCheck->controlIndex].uid;
						int uidActive = dm->devices[i]->virtualControls[bActive->controlIndex].uid;
						if (bCheck->controlIndex == bActive->controlIndex || (!((uidActive ^ uidCheck) & 0xFFFFFF) && ((uidCheck | uidActive) & (UID_POV | UID_AXIS))))
						{
							if (showWarning)
							{
								int msgboxID = MessageBoxA(hWndProp, "Warning!  You have selected a pad type that has one or several bindings that conflict with the active pad type of the opposing port or slot(s).\n\n"
																	 "Do you want to keep the bindings of the pad type you are switching to?\n"
																	 "Click \"Yes\" to continue without deleting any binding.\n"
																	 "Click \"No\" to continue and delete any conflicting bindings from the selected pad type.\n"
																	 "Click \"Cancel\" to revert to the previously selected pad type and avoid any further action.\n\n"
																	 "Note: Enable the 'Allow binding multiple PS2 controls to one PC control' option to allow conflicting bindings between opposing ports and slots, and avoid this warning and the possibility of bindings getting deleted.",
														   "Duplicate Binding Warning", MB_YESNOCANCEL | MB_DEFBUTTON3 | MB_ICONWARNING);
								switch (msgboxID)
								{
									case IDCANCEL:
										return false;
										break;
									case IDYES:
										return true;
										break;
									case IDNO:
										showWarning = false;
										break;
								}
							}
							DeleteBinding(port, slot, padtype, dm->devices[i], bActive);
							k--;
							count++;
						}
					}
				}
			}
		}
	}
	return true;
}

void UpdatePadPages()
{
	HPROPSHEETPAGE pages[10];
	int count = 0;
	memset(hWnds, 0, sizeof(hWnds));
	for (int port = 0; port < 2; port++)
	{
		for (int slot = 0; slot < 4; slot++)
		{
			if (config.padConfigs[port][slot].type == DisabledPad)
				continue;
			wchar_t title[20];
			if (!GetPadString(title, port, slot))
				continue;

			PROPSHEETPAGE psp;
			ZeroMemory(&psp, sizeof(psp));
			psp.dwSize = sizeof(psp);
			psp.dwFlags = PSP_USETITLE | PSP_PREMATURE;
			psp.hInstance = GetModuleHandle(nullptr);
			psp.pfnDlgProc = DialogProc;
			psp.lParam = port | (slot << 1);
			psp.pszTitle = title;
			if (config.padConfigs[port][slot].type == GuitarPad)
				psp.pszTemplate = MAKEINTRESOURCE(IDD_CONFIG_GUITAR);
			else if (config.padConfigs[port][slot].type == PopnPad)
				psp.pszTemplate = MAKEINTRESOURCE(IDD_CONFIG_POPN);
			else if (config.padConfigs[port][slot].type == MousePad)
				psp.pszTemplate = MAKEINTRESOURCE(IDD_CONFIG_PS1_MOUSE);
			else if (config.padConfigs[port][slot].type == neGconPad)
				psp.pszTemplate = MAKEINTRESOURCE(IDD_CONFIG_NEGCON);
			else
				psp.pszTemplate = MAKEINTRESOURCE(IDD_CONFIG);

			pages[count] = CreatePropertySheetPage(&psp);
			if (pages[count])
				count++;
		}
	}
	while (SendMessage(hWndProp, PSM_INDEXTOPAGE, 1, 0))
	{
		PropSheet_RemovePage(hWndProp, 1, 0);
	}
	for (int i = 0; i < count; i++)
	{
		PropSheet_AddPage(hWndProp, pages[i]);
	}
}

int ListIndexToPortAndSlot(int index, int* port, int* slot)
{
	if (index < 0 || index >= 2 + 3 * (config.multitap[0] + config.multitap[1]))
	{
		*port = 0;
		*slot = 0;
		return 0;
	}
	if (index < 1 + 3 * config.multitap[0])
	{
		*port = 0;
		*slot = index;
	}
	else
	{
		*port = 1;
		*slot = index - 1 - 3 * config.multitap[0];
	}
	return 1;
}

void UpdatePadList(HWND hWnd)
{
	static u8 recurse = 0;
	if (recurse)
		return;
	recurse = 1;
	HWND hWndList = GetDlgItem(hWnd, IDC_PAD_LIST);
	HWND hWndCombo = GetDlgItem(hWnd, IDC_PAD_TYPE);
	int slot;
	int port;
	int index = 0;

	for (port = 0; port < 2; port++)
	{
		for (slot = 0; slot < 4; slot++)
		{
			int padtype = config.padConfigs[port][slot].type;
			wchar_t text[25];
			if (!GetPadString(text, port, slot))
				continue;
			LVITEM item;
			item.iItem = index;
			item.iSubItem = 0;
			item.mask = LVIF_TEXT;
			item.pszText = text;
			if (SendMessage(hWndList, LVM_GETITEMCOUNT, 0, 0) <= index)
			{
				ListView_InsertItem(hWndList, &item);
			}
			else
			{
				ListView_SetItem(hWndList, &item);
			}

			item.iSubItem = 1;
			if (numPadTypes - 1 < (unsigned int)padtype)
				padtype = Dualshock2Pad;
			wcsncpy(item.pszText, padTypes[padtype], 25);
			//if (!slot && !config.padConfigs[port][slot].type)
			//	item.pszText = L"Unplugged (Kinda)";

			ListView_SetItem(hWndList, &item);

			item.iSubItem = 2;
			int count = 0;
			int selectedDevice = config.deviceSelect[port][slot];
			for (int i = 0; i < dm->numDevices; i++)
			{
				Device* dev = dm->devices[i];
				if (!dev->enabled)
					continue;
				count += dev->pads[port][slot][padtype].numBindings + dev->pads[port][slot][padtype].numFFBindings;
			}
			wsprintf(text, L"%i", count);
			item.pszText = text;
			ListView_SetItem(hWndList, &item);
			index++;
		}
	}
	while (ListView_DeleteItem(hWndList, index))
		;
	int sel = ListView_GetNextItem(hWndList, -1, LVNI_SELECTED);

	int enable;
	if (!ListIndexToPortAndSlot(sel, &port, &slot))
	{
		enable = 0;
		SendMessage(hWndCombo, CB_SETCURSEL, -1, 0);
	}
	else
	{
		enable = 1;
		SendMessage(hWndCombo, CB_SETCURSEL, config.padConfigs[port][slot].type, 0);
	}
	EnableWindow(hWndCombo, enable);
	//ListView_SetExtendedListViewStyleEx(hWndList, LVS_EX_DOUBLEBUFFER|LVS_EX_ONECLICKACTIVATE, LVS_EX_DOUBLEBUFFER|LVS_EX_ONECLICKACTIVATE);
	recurse = 0;
}

INT_PTR CALLBACK GeneralDialogProc(HWND hWnd, unsigned int msg, WPARAM wParam, LPARAM lParam)
{
	HWND hWndList = GetDlgItem(hWnd, IDC_PAD_LIST);
	switch (msg)
	{
		case WM_INITDIALOG:
		{
			HWND hWndCombo = GetDlgItem(hWnd, IDC_PAD_TYPE);
			if (SendMessage(hWndCombo, CB_GETCOUNT, 0, 0) == 0)
			{
				LVCOLUMN c;
				c.mask = LVCF_TEXT | LVCF_WIDTH;
				c.cx = 50;
				c.pszText = L"Pad";
				ListView_InsertColumn(hWndList, 0, &c);
				c.cx = 120;
				c.pszText = L"Type";
				ListView_InsertColumn(hWndList, 1, &c);
				c.pszText = L"Bindings";
				ListView_InsertColumn(hWndList, 2, &c);
				ListView_SetColumnWidth(hWndList, 2, LVSCW_AUTOSIZE_USEHEADER);
				selected = 0;
				ListView_SetExtendedListViewStyleEx(hWndList, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER, LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER);
				SendMessage(hWndList, LVM_SETEXTENDEDLISTVIEWSTYLE, 0, LVS_EX_FULLROWSELECT);
				for (int i = 0; i < numPadTypes; i++)
					SendMessage(hWndCombo, CB_ADDSTRING, 0, (LPARAM)padTypes[i]);
			}
		}
			UpdatePadPages();
			hWndGeneral = hWnd;
			RefreshEnabledDevicesAndDisplay(0, hWnd, 0);
			UpdatePadList(hWnd);

			if (!DualShock3Possible())
			{
				config.gameApis.dualShock3 = 0;
				EnableWindow(GetDlgItem(hWnd, IDC_G_DS3), 0);
			}

			for (int j = 0; j < sizeof(BoolOptionsInfo) / sizeof(BoolOptionsInfo[0]); j++)
			{
				CheckDlgButton(hWnd, BoolOptionsInfo[j].ControlId, BST_CHECKED * config.bools[j]);
			}

			AddTooltip(IDC_M_WM, hWnd);
			AddTooltip(IDC_M_RAW, hWnd);
			AddTooltip(IDC_MOUSE_UNFOCUS, hWnd);
			AddTooltip(IDC_MULTIPLE_BINDING, hWnd);
			AddTooltip(IDC_PAD_LIST, hWnd);
			AddTooltip(IDC_PAD_TYPE, hWnd);
			AddTooltip(IDC_DIAG_LIST, hWnd);
			AddTooltip(IDC_G_DI, hWnd);
			AddTooltip(IDC_G_XI, hWnd);
			AddTooltip(ID_RESTORE_DEFAULTS, hWnd);

			if (config.keyboardApi < 0 || config.keyboardApi > 3)
				config.keyboardApi = NO_API;
			CheckRadioButton(hWnd, IDC_KB_DISABLE, IDC_KB_RAW, IDC_KB_DISABLE + config.keyboardApi);
			if (config.mouseApi < 0 || config.mouseApi > 3)
				config.mouseApi = NO_API;
			CheckRadioButton(hWnd, IDC_M_DISABLE, IDC_M_RAW, IDC_M_DISABLE + config.mouseApi);

			break;
		case WM_DEVICECHANGE:
			if (wParam == DBT_DEVNODES_CHANGED)
			{
				RefreshEnabledDevicesAndDisplay(1, hWndGeneral, 1);
				UpdatePadList(hWnd);
			}
			break;
		case WM_COMMAND:
			if (LOWORD(wParam) == IDC_PAD_TYPE)
			{
				if (HIWORD(wParam) == CBN_SELCHANGE)
				{
					HWND hWndCombo = GetDlgItem(hWnd, IDC_PAD_TYPE);
					int index = ListView_GetNextItem(hWndList, -1, LVNI_SELECTED);
					int sel = SendMessage(hWndCombo, CB_GETCURSEL, 0, 0);
					int port, slot;
					if (sel < 0 || !ListIndexToPortAndSlot(index, &port, &slot))
						break;
					if (sel != config.padConfigs[port][slot].type)
					{
						if (!config.multipleBinding && !ProfilesBindingCheck(port, slot, sel))
							break;
						config.padConfigs[port][slot].type = (PadType)sel;
						UpdatePadList(hWnd);
						UpdatePadPages();
						RefreshEnabledDevicesAndDisplay(0, hWnd, 1);
						PropSheet_Changed(hWndProp, hWnd);
					}
				}
			}
			else if (HIWORD(wParam) == BN_CLICKED && (LOWORD(wParam) == ID_LOAD || LOWORD(wParam) == ID_SAVE))
			{
				OPENFILENAMEW ofn;
				memset(&ofn, 0, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFilter = L"PAD Config Files\0*.pad\0All Files\0*.*\0\0";
				wchar_t file[MAX_PATH + 1];
				ofn.lpstrFile = file;
				ofn.nMaxFile = MAX_PATH;
				wcscpy(file, config.lastSaveConfigFileName);
				ofn.lpstrInitialDir = config.lastSaveConfigPath;
				ofn.Flags = OFN_DONTADDTORECENT | OFN_LONGNAMES | OFN_NOCHANGEDIR;
				ofn.lpstrDefExt = L"pad";
				if (LOWORD(wParam) == ID_LOAD)
				{
					ofn.lpstrTitle = L"Load PAD Configuration";
					ofn.Flags |= OFN_FILEMUSTEXIST;
					if (GetOpenFileNameW(&ofn))
					{
						LoadSettings(1, ofn.lpstrFile);
						GeneralDialogProc(hWnd, WM_INITDIALOG, 0, 0);
						PropSheet_Changed(hWndProp, hWnd);
					}
				}
				else
				{
					ofn.lpstrTitle = L"Save PAD Configuration";
					ofn.Flags |= OFN_OVERWRITEPROMPT;
					if (GetSaveFileNameW(&ofn))
					{
						if (SaveSettings(ofn.lpstrFile) == -1)
						{
							MessageBox(hWnd, L"Save fail", L"Couldn't save file.", MB_OK);
						}
					}
				}
				break;
			}
			else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == ID_RESTORE_DEFAULTS)
			{
				int msgboxID = MessageBoxA(hWndProp, "This will delete all current settings and revert back to the default settings of PAD. Continue?",
										   "Restore Defaults Confirmation", MB_YESNO | MB_DEFBUTTON2 | MB_ICONEXCLAMATION);
				switch (msgboxID)
				{
					case IDNO:
						break;
					case IDYES:
						char iniLocation[MAX_PATH * 2] = "inis/PAD.ini";
						remove(iniLocation);
						createIniDir = true;
						LoadSettings(1);
						GeneralDialogProc(hWnd, WM_INITDIALOG, 0, 0);
						PropSheet_Changed(hWndProp, hWnd);
						break;
				}
			}
			else
			{

				int mtap = config.multitap[0] + 2 * config.multitap[1];

				for (int j = 0; j < sizeof(BoolOptionsInfo) / sizeof(BoolOptionsInfo[0]); j++)
				{
					config.bools[j] = (IsDlgButtonChecked(hWnd, BoolOptionsInfo[j].ControlId) == BST_CHECKED);
				}

				for (int i = 0; i < 4; i++)
				{
					if (i && IsDlgButtonChecked(hWnd, IDC_KB_DISABLE + i) == BST_CHECKED)
					{
						config.keyboardApi = (DeviceAPI)i;
					}
					if (IsDlgButtonChecked(hWnd, IDC_M_DISABLE + i) == BST_CHECKED)
					{
						config.mouseApi = (DeviceAPI)i;
					}
				}

				if (mtap != config.multitap[0] + 2 * config.multitap[1])
				{
					UpdatePadPages();
				}
				RefreshEnabledDevicesAndDisplay(0, hWnd, 1);
				UpdatePadList(hWnd);

				PropSheet_Changed(hWndProp, hWnd);

				if (mtap != config.multitap[0] + 2 * config.multitap[1])
				{
					ListView_SetColumnWidth(hWndList, 2, LVSCW_AUTOSIZE_USEHEADER);
				}
			}
			break;
		case WM_NOTIFY:
		{
			PSHNOTIFY* n = (PSHNOTIFY*)lParam;
			if (n->hdr.hwndFrom == hWndProp)
			{
				switch (n->hdr.code)
				{
					case PSN_QUERYCANCEL:
					case PSN_KILLACTIVE:
						EndBinding(hWnd);
						return 0;
					case PSN_SETACTIVE:
						//selected = 0;
						UpdatePadList(hWnd);
						return 0;
					case PSN_APPLY:
						selected = 0;
						if (SaveSettings())
						{
							SetWindowLongPtr(hWnd, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
							return 0;
						}
						SetWindowLongPtr(hWnd, DWLP_MSGRESULT, PSNRET_NOERROR);
						return 1;
				}
			}
			else if (n->hdr.idFrom == IDC_DIAG_LIST)
			{
				if (n->hdr.code == NM_DBLCLK)
				{
					Diagnostics(hWnd);
				}
				else if (n->hdr.code == NM_RCLICK)
				{
					HMENU hMenu = CreatePopupMenu();
					if (!hMenu)
						break;
					MENUITEMINFOW info;
					wchar_t text[40];
					info.cbSize = sizeof(info);
					info.fMask = MIIM_STRING | MIIM_ID;
					info.dwTypeData = text;
					info.wID = 1;
					wsprintfW(text, L"Test Device");
					InsertMenuItemW(hMenu, 0, 1, &info);
					info.wID = 2;
					wsprintfW(text, L"Refresh");
					InsertMenuItemW(hMenu, 1, 1, &info);
					POINT pos;
					GetCursorPos(&pos);
					short res = TrackPopupMenuEx(hMenu, TPM_NONOTIFY | TPM_RETURNCMD, pos.x, pos.y, hWndProp, 0);
					DestroyMenu(hMenu);
					if (res == 0)
						break;
					else if (res == 1)
					{
						Diagnostics(hWnd);
						RefreshEnabledDevices();
					}
					else if (res == 2)
					{
						RefreshEnabledDevicesAndDisplay(1, hWnd, 1);
						UpdatePadList(hWnd);
					}
				}
			}
			else if (n->hdr.idFrom == IDC_PAD_LIST)
			{
				if (n->hdr.code == LVN_ITEMCHANGED)
				{
					UpdatePadList(hWnd);
				}
				if (n->hdr.code == NM_DBLCLK)
				{
					UpdatePadList(hWnd);
					int index = ListView_GetNextItem(hWndList, -1, LVNI_SELECTED);
					PropSheet_SetCurSel(hWndProp, 0, index + 1);
				}
				else if (n->hdr.code == NM_RCLICK)
				{
					UpdatePadList(hWnd);
					int index = ListView_GetNextItem(hWndList, -1, LVNI_SELECTED);
					int port1, slot1, port2, slot2, padtype1, padtype2;
					if (!ListIndexToPortAndSlot(index, &port1, &slot1))
						break;
					padtype1 = config.padConfigs[port1][slot1].type;
					HMENU hMenu = CreatePopupMenu();
					if (!hMenu)
						break;
					MENUITEMINFOW info;
					for (int IndividualPadtypes = 1; IndividualPadtypes >= 0; IndividualPadtypes--)
					{
						info.fMask = MIIM_TYPE;
						info.fType = MFT_SEPARATOR;
						InsertMenuItemW(hMenu, 0, 1, &info);
						for (port2 = 1; port2 >= 0; port2--)
						{
							for (slot2 = 3; slot2 >= 0; slot2--)
							{
								padtype2 = config.padConfigs[port2][slot2].type;
								wchar_t text[60];
								wchar_t pad[40];
								if (IndividualPadtypes == 0)
								{
									if (!GetPadString(pad, port2, slot2))
										continue;
								}
								else
								{
									if (!GetPadTypeString(pad, port2, slot2, padtype2))
										continue;
								}
								info.cbSize = sizeof(info);
								info.fMask = MIIM_STRING | MIIM_ID;
								info.dwTypeData = text;
								if (port2 == port1 && slot2 == slot1 && padtype2 == padtype1)
								{
									int index = GetMenuItemCount(hMenu);
									wsprintfW(text, L"Clear %s Bindings", pad);
									if (IndividualPadtypes == 0)
									{
										info.wID = -1;
										wsprintfW(text, L"Clear all %s Bindings", pad);
									}
									else
									{
										info.wID = -2;
										wsprintfW(text, L"Clear %s Bindings only", pad);
									}
									InsertMenuItemW(hMenu, index, 1, &info);
									info.fMask = MIIM_TYPE;
									info.fType = MFT_SEPARATOR;
									InsertMenuItemW(hMenu, index, 1, &info);
								}
								else if (!(port2 == port1 && slot2 == slot1))
								{
									info.wID = port2 + 2 * slot2 + 1;
									if (IndividualPadtypes == 1)
									{
										if (!config.multipleBinding && padtype2 != padtype1)
											continue;
										info.wID += 8;
										wsprintfW(text, L"Swap with %s bindings only", pad);
									}
									else
									{
										wsprintfW(text, L"Swap all bindings with %s", pad);
									}
									InsertMenuItemW(hMenu, 0, 1, &info);
								}
							}
						}
					}
					POINT pos;
					GetCursorPos(&pos);
					short res = TrackPopupMenuEx(hMenu, TPM_NONOTIFY | TPM_RETURNCMD, pos.x, pos.y, hWndProp, 0);
					DestroyMenu(hMenu);
					if (!res)
						break;
					bool swapIndividualPadtypes = false;
					if (res > 0)
					{
						if (res > 8)
						{
							res -= 8;
							swapIndividualPadtypes = true;
						}
						res--;
						slot2 = res / 2;
						port2 = res & 1;
						padtype2 = config.padConfigs[port2][slot2].type;
						PadConfig padCfgTemp = config.padConfigs[port1][slot1];
						config.padConfigs[port1][slot1] = config.padConfigs[port2][slot2];
						config.padConfigs[port2][slot2] = padCfgTemp;
						if (swapIndividualPadtypes)
						{
							config.padConfigs[port2][slot2].type = config.padConfigs[port1][slot1].type;
							config.padConfigs[port1][slot1].type = padCfgTemp.type;
						}
						for (int i = 0; i < dm->numDevices; i++)
						{
							if (dm->devices[i]->type == IGNORE)
								continue;
							if (swapIndividualPadtypes)
							{
								PadBindings bindings = dm->devices[i]->pads[port1][slot1][padtype1];
								dm->devices[i]->pads[port1][slot1][padtype1] = dm->devices[i]->pads[port2][slot2][padtype2];
								dm->devices[i]->pads[port2][slot2][padtype2] = bindings;
							}
							else
							{
								for (int padtype = 0; padtype < numPadTypes; padtype++)
								{
									PadBindings bindings = dm->devices[i]->pads[port1][slot1][padtype];
									dm->devices[i]->pads[port1][slot1][padtype] = dm->devices[i]->pads[port2][slot2][padtype];
									dm->devices[i]->pads[port2][slot2][padtype] = bindings;
								}
							}
						}
					}
					else
					{
						if (res == -2)
						{
							swapIndividualPadtypes = true;
						}
						for (int i = 0; i < dm->numDevices; i++)
						{
							if (dm->devices[i]->type == IGNORE)
								continue;
							if (swapIndividualPadtypes)
							{
								free(dm->devices[i]->pads[port1][slot1][padtype1].bindings);
								for (int j = 0; j < dm->devices[i]->pads[port1][slot1][padtype1].numFFBindings; j++)
								{
									free(dm->devices[i]->pads[port1][slot1][padtype1].ffBindings[j].axes);
								}
								free(dm->devices[i]->pads[port1][slot1][padtype1].ffBindings);
								memset(&dm->devices[i]->pads[port1][slot1][padtype1], 0, sizeof(dm->devices[i]->pads[port1][slot1][padtype1]));
							}
							else
							{
								for (int padtype = 0; padtype < numPadTypes; padtype++)
								{
									free(dm->devices[i]->pads[port1][slot1][padtype].bindings);
									for (int j = 0; j < dm->devices[i]->pads[port1][slot1][padtype].numFFBindings; j++)
									{
										free(dm->devices[i]->pads[port1][slot1][padtype].ffBindings[j].axes);
									}
									free(dm->devices[i]->pads[port1][slot1][padtype].ffBindings);
									memset(&dm->devices[i]->pads[port1][slot1][padtype], 0, sizeof(dm->devices[i]->pads[port1][slot1][padtype]));
								}
							}
						}
					}
					UpdatePadPages();
					UpdatePadList(hWnd);
					PropSheet_Changed(hWndProp, hWnd);
				}
			}
		}
		break;
		default:
			break;
	}
	return 0;
}

int CALLBACK PropSheetProc(HWND hWnd, UINT msg, LPARAM lParam)
{
	if (hWnd)
		hWndProp = hWnd;
	return 0;
}

void Configure()
{
	// Can end up here without PADinit() being called first.
	LoadSettings();
	// Can also end up here after running emulator a bit, and possibly
	// disabling some devices due to focus changes, or releasing mouse.
	RefreshEnabledDevices(0);

	memset(hWnds, 0, sizeof(hWnds));

	PROPSHEETPAGE psp;
	ZeroMemory(&psp, sizeof(psp));
	psp.dwSize = sizeof(psp);
	psp.dwFlags = PSP_USETITLE | PSP_PREMATURE;
	psp.hInstance = GetModuleHandle(nullptr);
	psp.pfnDlgProc = GeneralDialogProc;
	psp.pszTitle = L"General";
	psp.pszTemplate = MAKEINTRESOURCE(IDD_GENERAL);
	HPROPSHEETPAGE page = CreatePropertySheetPage(&psp);
	if (!page)
		return;

	PROPSHEETHEADER psh;
	ZeroMemory(&psh, sizeof(psh));
	psh.dwFlags = PSH_DEFAULT | PSH_USECALLBACK | PSH_NOCONTEXTHELP;
	psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.pfnCallback = PropSheetProc;
	psh.hwndParent = GetActiveWindow();
	psh.nPages = 1;
	psh.phpage = &page;
	wchar_t title[200];
	GetNameAndVersionString(title);
	wcscat(title, L" Settings");
	psh.pszCaption = title;
	PropertySheet(&psh);
	LoadSettings(1);
	memset(hWnds, 0, sizeof(hWnds));
}

void UnloadConfigs()
{
	if (dm)
	{
		delete dm;
		dm = 0;
	}
}
