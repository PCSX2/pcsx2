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
#include <VersionHelpers.h>
#include <xinput.h>
#include "VKey.h"
#include "InputManager.h"
#include "XInputEnum.h"
#include "PADConfig.h"

// Extra enum
#define XINPUT_GAMEPAD_GUIDE 0x0400

typedef struct
{
	float SCP_UP;
	float SCP_RIGHT;
	float SCP_DOWN;
	float SCP_LEFT;

	float SCP_LX;
	float SCP_LY;

	float SCP_L1;
	float SCP_L2;
	float SCP_L3;

	float SCP_RX;
	float SCP_RY;

	float SCP_R1;
	float SCP_R2;
	float SCP_R3;

	float SCP_T;
	float SCP_C;
	float SCP_X;
	float SCP_S;

	float SCP_SELECT;
	float SCP_START;

	float SCP_PS;

} SCP_EXTN;


// This way, I don't require that XInput junk be installed.
typedef void(CALLBACK* _XInputEnable)(BOOL enable);
typedef DWORD(CALLBACK* _XInputGetStateEx)(DWORD dwUserIndex, XINPUT_STATE* pState);
typedef DWORD(CALLBACK* _XInputGetExtended)(DWORD dwUserIndex, SCP_EXTN* pPressure);
typedef DWORD(CALLBACK* _XInputSetState)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);

_XInputEnable pXInputEnable = 0;
_XInputGetStateEx pXInputGetStateEx = 0;
_XInputGetExtended pXInputGetExtended = 0;
_XInputSetState pXInputSetState = 0;
static bool xinputNotInstalled = false;

static int xInputActiveCount = 0;

// Completely unncessary, really.
__forceinline int ShortToAxis(int v)
{
	// If positive and at least 1 << 14, increment.
	v += (!((v >> 15) & 1)) & ((v >> 14) & 1);
	// Just double.
	return v * 2;
}

class XInputDevice : public Device
{
	// Cached last vibration values by pad and motor.
	// Need this, as only one value is changed at a time.
	int ps2Vibration[2][4][2];
	// Minor optimization - cache last set vibration values
	// When there's no change, no need to do anything.
	XINPUT_VIBRATION xInputVibration;

public:
	int index;

	XInputDevice(int index, wchar_t* displayName)
		: Device(XINPUT, OTHER, displayName)
	{
		memset(ps2Vibration, 0, sizeof(ps2Vibration));
		memset(&xInputVibration, 0, sizeof(xInputVibration));
		this->index = index;
		int i;
		for (i = 0; i < 17; i++)
		{ // Skip empty bit
			AddPhysicalControl(PRESSURE_BTN, i + (i > 10), 0);
		}
		for (; i < 21; i++)
		{
			AddPhysicalControl(ABSAXIS, i + 2, 0);
		}
		AddFFAxis(L"Slow Motor", 0);
		AddFFAxis(L"Fast Motor", 1);
		AddFFEffectType(L"Constant Effect", L"Constant", EFFECT_CONSTANT);
	}

	wchar_t* GetPhysicalControlName(PhysicalControl* c)
	{
		const static wchar_t* names[] = {
			L"D-pad Up",
			L"D-pad Down",
			L"D-pad Left",
			L"D-pad Right",
			L"Start",
			L"Back",
			L"Left Thumb",
			L"Right Thumb",
			L"Left Shoulder",
			L"Right Shoulder",
			L"Guide",
			L"A",
			L"B",
			L"X",
			L"Y",
			L"Left Trigger",
			L"Right Trigger",
			L"Left Thumb X",
			L"Left Thumb Y",
			L"Right Thumb X",
			L"Right Thumb Y",
		};
		unsigned int i = (unsigned int)(c - physicalControls);
		if (i < 21)
		{
			return (wchar_t*)names[i];
		}
		return Device::GetPhysicalControlName(c);
	}

	int Activate(InitInfo* initInfo)
	{
		if (active)
			Deactivate();
		if (!xInputActiveCount)
		{
			pXInputEnable(1);
		}
		xInputActiveCount++;
		active = 1;
		AllocState();
		return 1;
	}

	int Update()
	{
		if (!active)
			return 0;
		SCP_EXTN pressure;
		if (!pXInputGetExtended || (ERROR_SUCCESS != pXInputGetExtended(index, &pressure)))
		{
			XINPUT_STATE state;
			if (ERROR_SUCCESS != pXInputGetStateEx(index, &state))
			{
				Deactivate();
				return 0;
			}

			int buttons = state.Gamepad.wButtons;
			for (int i = 0; i < 15; i++)
			{
				physicalControlState[i] = ((buttons >> physicalControls[i].id) & 1) << 16;
			}
			physicalControlState[15] = (int)(state.Gamepad.bLeftTrigger * 257.005);
			physicalControlState[16] = (int)(state.Gamepad.bRightTrigger * 257.005);
			physicalControlState[17] = ShortToAxis(state.Gamepad.sThumbLX);
			physicalControlState[18] = ShortToAxis(state.Gamepad.sThumbLY);
			physicalControlState[19] = ShortToAxis(state.Gamepad.sThumbRX);
			physicalControlState[20] = ShortToAxis(state.Gamepad.sThumbRY);
		}
		else
		{
			physicalControlState[0] = (int)(pressure.SCP_UP * FULLY_DOWN);
			physicalControlState[1] = (int)(pressure.SCP_DOWN * FULLY_DOWN);
			physicalControlState[2] = (int)(pressure.SCP_LEFT * FULLY_DOWN);
			physicalControlState[3] = (int)(pressure.SCP_RIGHT * FULLY_DOWN);
			physicalControlState[4] = (int)(pressure.SCP_START * FULLY_DOWN);
			physicalControlState[5] = (int)(pressure.SCP_SELECT * FULLY_DOWN);
			physicalControlState[6] = (int)(pressure.SCP_L3 * FULLY_DOWN);
			physicalControlState[7] = (int)(pressure.SCP_R3 * FULLY_DOWN);
			physicalControlState[8] = (int)(pressure.SCP_L1 * FULLY_DOWN);
			physicalControlState[9] = (int)(pressure.SCP_R1 * FULLY_DOWN);
			physicalControlState[10] = (int)(pressure.SCP_PS * FULLY_DOWN);
			physicalControlState[11] = (int)(pressure.SCP_X * FULLY_DOWN);
			physicalControlState[12] = (int)(pressure.SCP_C * FULLY_DOWN);
			physicalControlState[13] = (int)(pressure.SCP_S * FULLY_DOWN);
			physicalControlState[14] = (int)(pressure.SCP_T * FULLY_DOWN);
			physicalControlState[15] = (int)(pressure.SCP_L2 * FULLY_DOWN);
			physicalControlState[16] = (int)(pressure.SCP_R2 * FULLY_DOWN);
			physicalControlState[17] = (int)(pressure.SCP_LX * FULLY_DOWN);
			physicalControlState[18] = (int)(pressure.SCP_LY * FULLY_DOWN);
			physicalControlState[19] = (int)(pressure.SCP_RX * FULLY_DOWN);
			physicalControlState[20] = (int)(pressure.SCP_RY * FULLY_DOWN);
		}
		return 1;
	}

	void SetEffects(unsigned char port, unsigned int slot, unsigned char motor, unsigned char force)
	{
		ps2Vibration[port][slot][motor] = force;
		int newVibration[2] = {0, 0};
		for (int p = 0; p < 2; p++)
		{
			for (int s = 0; s < 4; s++)
			{
				int padtype = config.padConfigs[p][s].type;
				for (int i = 0; i < pads[p][s][padtype].numFFBindings; i++)
				{
					// Technically should also be a *65535/BASE_SENSITIVITY, but that's close enough to 1 for me.
					ForceFeedbackBinding* ffb = &pads[p][s][padtype].ffBindings[i];
					newVibration[0] += (int)((ffb->axes[0].force * (__int64)ps2Vibration[p][s][ffb->motor]) / 255);
					newVibration[1] += (int)((ffb->axes[1].force * (__int64)ps2Vibration[p][s][ffb->motor]) / 255);
				}
			}
		}
		newVibration[0] = abs(newVibration[0]);
		if (newVibration[0] > 65535)
		{
			newVibration[0] = 65535;
		}
		newVibration[1] = abs(newVibration[1]);
		if (newVibration[1] > 65535)
		{
			newVibration[1] = 65535;
		}
		if (newVibration[0] || newVibration[1] || newVibration[0] != xInputVibration.wLeftMotorSpeed || newVibration[1] != xInputVibration.wRightMotorSpeed)
		{
			XINPUT_VIBRATION newv = {(WORD)newVibration[0], (WORD)newVibration[1]};
			if (ERROR_SUCCESS == pXInputSetState(index, &newv))
			{
				xInputVibration = newv;
			}
		}
	}

	void SetEffect(ForceFeedbackBinding* binding, unsigned char force)
	{
		PadBindings pBackup = pads[0][0][0];
		pads[0][0][0].ffBindings = binding;
		pads[0][0][0].numFFBindings = 1;
		SetEffects(0, 0, binding->motor, 255);
		pads[0][0][0] = pBackup;
	}

	void Deactivate()
	{
		memset(&xInputVibration, 0, sizeof(xInputVibration));
		memset(ps2Vibration, 0, sizeof(ps2Vibration));
		pXInputSetState(index, &xInputVibration);

		FreeState();
		if (active)
		{
			if (!--xInputActiveCount)
			{
				pXInputEnable(0);
			}
			active = 0;
		}
	}

	~XInputDevice()
	{
	}
};

void EnumXInputDevices()
{
	wchar_t temp[30];
	if (!pXInputSetState)
	{
		// XInput not installed, so don't repeatedly try to load it.
		if (xinputNotInstalled)
			return;

		// Prefer XInput 1.3 since SCP only has an XInput 1.3 wrapper right now.
		// Also use LoadLibrary and not LoadLibraryEx for XInput 1.3, since some
		// Windows 7 systems have issues with it.
		// FIXME: Missing FreeLibrary call.
		HMODULE hMod = LoadLibrary(L"xinput1_3.dll");
		if (hMod == nullptr && IsWindows8OrGreater())
		{
			hMod = LoadLibraryEx(L"XInput1_4.dll", nullptr, LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
		}

		if (hMod)
		{
			if ((pXInputEnable = (_XInputEnable)GetProcAddress(hMod, "XInputEnable")) &&
				((pXInputGetStateEx = (_XInputGetStateEx)GetProcAddress(hMod, (LPCSTR)100)) || // Try Ex version first
				 (pXInputGetStateEx = (_XInputGetStateEx)GetProcAddress(hMod, "XInputGetState"))))
			{
				pXInputGetExtended = (_XInputGetExtended)GetProcAddress(hMod, "XInputGetExtended");
				pXInputSetState = (_XInputSetState)GetProcAddress(hMod, "XInputSetState");
			}
		}
		if (!pXInputSetState)
		{
			xinputNotInstalled = true;
			return;
		}
	}
	pXInputEnable(1);
	for (int i = 0; i < 4; i++)
	{
		wsprintfW(temp, L"XInput Pad %i", i);
		dm->AddDevice(new XInputDevice(i, temp));
	}
	pXInputEnable(0);
}
