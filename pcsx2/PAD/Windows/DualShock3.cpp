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
#include "InputManager.h"
#include "PADConfig.h"

#include "HidDevice.h"

#define VID 0x054c
#define PID 0x0268

//Sixaxis driver commands.
//All commands must be sent via WriteFile with 49-byte buffer containing output report.
//Byte 0 indicates reportId and must always be 0.
//Byte 1 indicates some command, supported values are specified below.

//This command allows to set user LEDs.
//Bytes 5,6.7.8 contain mode for corresponding LED: 0 value means LED is OFF, 1 means LEDs in ON and 2 means LEDs is flashing.
//Bytes 9-16 specify 64-bit LED flash period in 100 ns units if some LED is flashing, otherwise not used.
#define SIXAXIS_COMMAND_SET_LEDS 1
//This command allows to set left and right motors.
//Byte 5 is right motor duration (0-255) and byte 6, if not zero, activates right motor. Zero value disables right motor.
//Byte 7 is left motor duration (0-255) and byte 8 is left motor amplitude (0-255).
#define SIXAXIS_COMMAND_SET_MOTORS 2
//This command allows to block/unblock setting device LEDs by applications.
//Byte 5 is used as parameter - any non-zero value blocks LEDs, zero value will unblock LEDs.
#define SIXAXIS_COMMAND_BLOCK_LEDS 3
//This command refreshes driver settings. No parameters used.
//When sixaxis driver loads it reads 'CurrentDriverSetting' binary value from 'HKLM\System\CurrentControlSet\Services\sixaxis\Parameters' registry key.
//If the key is not present then default values are used. Sending this command forces sixaxis driver to re-read the registry and update driver settings.
#define SIXAXIS_COMMAND_REFRESH_DRIVER_SETTING 9
//This command clears current bluetooth pairing. No parameters used.
#define SIXAXIS_COMMAND_CLEAR_PAIRING 10

int DualShock3Possible()
{
	return 1;
}

int CharToAxis(unsigned char c)
{
	return ((c - 128) * FULLY_DOWN) >> 7;
}

int CharToButton(unsigned char c)
{
	const int v = (int)c + ((unsigned int)c >> 7);
	return (v * FULLY_DOWN) >> 8;
}

class DualShock3Device : public Device
{
	// Cached last vibration values by pad and motor.
	// Need this, as only one value is changed at a time.
	int ps2Vibration[2][4][2];
	int vibration[2];

public:
	int index;
	HANDLE hFile;
	unsigned char getState[49];

	DualShock3Device(int index, wchar_t* name, wchar_t* path)
		: Device(DS3, OTHER, name, path, L"DualShock 3")
	{
		memset(ps2Vibration, 0, sizeof(ps2Vibration));
		vibration[0] = vibration[1] = 0;
		this->index = index;
		int i;
		for (i = 0; i < 16; i++)
		{
			if (i != 14 && i != 15 && i != 8 && i != 9)
			{
				AddPhysicalControl(PRESSURE_BTN, i, 0);
			}
			else
			{
				AddPhysicalControl(PSHBTN, i, 0);
			}
		}
		for (; i < 23; i++)
		{
			AddPhysicalControl(ABSAXIS, i, 0);
		}
		AddFFAxis(L"Big Motor", 0);
		AddFFAxis(L"Small Motor", 1);
		AddFFEffectType(L"Constant Effect", L"Constant", EFFECT_CONSTANT);
		hFile = INVALID_HANDLE_VALUE;
	}

	wchar_t* GetPhysicalControlName(PhysicalControl* c)
	{
		const static wchar_t* names[] = {
			L"Square",
			L"Cross",
			L"Circle",
			L"Triangle",
			L"R1",
			L"L1",
			L"R2",
			L"L2",
			L"R3",
			L"L3",
			L"Left",
			L"Down",
			L"Right",
			L"Up",
			L"Start",
			L"Select",
			L"L-Stick X",
			L"L-Stick Y",
			L"R-Stick X",
			L"R-Stick Y",
			L"Left/Right Tilt",
			L"Forward/Back Tilt",
			L"???",
		};
		unsigned int i = (unsigned int)(c - physicalControls);
		if (i < sizeof(names) / sizeof(names[0]))
		{
			return (wchar_t*)names[i];
		}
		return Device::GetPhysicalControlName(c);
	}

	int Activate(InitInfo* initInfo)
	{
		if (active)
			Deactivate();
		hFile = CreateFileW(instanceID, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
		if (hFile == INVALID_HANDLE_VALUE)
		{
			Deactivate();
			return 0;
		}
		active = 1;
		AllocState();
		return 1;
	}

	int Update()
	{
		if (!active)
			return 0;

		unsigned char Buffer[50];

		Buffer[0] = 0; //reportId

		if (!HidD_GetFeature(hFile,
				Buffer,
				sizeof(Buffer)))
		{
			Deactivate();
			return 0;
		}

		unsigned char* getState = &Buffer[1];

		physicalControlState[0] = CharToButton(getState[25]); //Square
		physicalControlState[1] = CharToButton(getState[24]); //Cross
		physicalControlState[2] = CharToButton(getState[23]); //Circle
		physicalControlState[3] = CharToButton(getState[22]); //Triangle
		physicalControlState[4] = CharToButton(getState[21]); //R1
		physicalControlState[5] = CharToButton(getState[20]); //L1
		physicalControlState[6] = CharToButton(getState[19]); //R2
		physicalControlState[7] = CharToButton(getState[18]); //L2
		physicalControlState[10] = CharToButton(getState[17]); //Dpad Left
		physicalControlState[11] = CharToButton(getState[16]); //Dpad Down
		physicalControlState[12] = CharToButton(getState[15]); //Dpad Right
		physicalControlState[13] = CharToButton(getState[14]); //Dpad Up
		physicalControlState[8] = ((getState[2] & 4) / 4) * FULLY_DOWN; //R3
		physicalControlState[9] = ((getState[2] & 2) / 2) * FULLY_DOWN; //L3
		physicalControlState[15] = ((getState[2] & 1) / 1) * FULLY_DOWN; //SELECT
		physicalControlState[14] = ((getState[2] & 8) / 8) * FULLY_DOWN; //START
		physicalControlState[16] = CharToAxis(getState[6]); //Left Stick X
		physicalControlState[17] = CharToAxis(getState[7]); //Left Stick Y
		physicalControlState[18] = CharToAxis(getState[8]); //Right Stick X
		physicalControlState[19] = CharToAxis(getState[9]); //Right Stick Y
		//Compared to libusb all sensor values on sixaxis driver are little-endian and X axis is inversed
		physicalControlState[20] = CharToAxis(128 - getState[41]); //Accel X (Left/Right Tilt)
		physicalControlState[21] = CharToAxis(getState[43] + 128); //Accel Y (Forward/Back Tilt)
		physicalControlState[22] = CharToAxis(getState[45] + 128); //Accel Z

		return 1;
	}

	void SetEffects(unsigned char port, unsigned int slot, unsigned char motor, unsigned char force)
	{
		ps2Vibration[port][slot][motor] = force;
		vibration[0] = vibration[1] = 0;
		for (int p = 0; p < 2; p++)
		{
			for (int s = 0; s < 4; s++)
			{
				int padtype = config.padConfigs[p][s].type;
				for (int i = 0; i < pads[p][s][padtype].numFFBindings; i++)
				{
					// Technically should also be a *65535/BASE_SENSITIVITY, but that's close enough to 1 for me.
					ForceFeedbackBinding* ffb = &pads[p][s][padtype].ffBindings[i];
					vibration[0] += (int)((ffb->axes[0].force * (__int64)ps2Vibration[p][s][ffb->motor]) / 255);
					vibration[1] += (int)((ffb->axes[1].force * (__int64)ps2Vibration[p][s][ffb->motor]) / 255);
				}
			}
		}

		unsigned char outputReport[49];

		//Clear all data and set reportId to 0
		memset(outputReport, 0, sizeof(outputReport));

		//This is command for sixaxis driver to set motors
		outputReport[1] = SIXAXIS_COMMAND_SET_MOTORS;

		outputReport[5] = 0x50; //right_duration

		outputReport[6] = (unsigned char)(vibration[1] >= FULLY_DOWN / 2); //right_motor_on

		outputReport[7] = 0x50; //left_duration

		int bigForce = vibration[0] * 256 / FULLY_DOWN;
		if (bigForce > 255)
			bigForce = 255;

		outputReport[8] = (unsigned char)bigForce; //left_motor_force

		DWORD lpNumberOfBytesWritten = 0;

		WriteFile(hFile, outputReport, sizeof(outputReport), &lpNumberOfBytesWritten, NULL);
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
		if (hFile != INVALID_HANDLE_VALUE)
		{
			CancelIo(hFile);
			CloseHandle(hFile);
			hFile = INVALID_HANDLE_VALUE;
		}

		memset(ps2Vibration, 0, sizeof(ps2Vibration));
		vibration[0] = vibration[1] = 0;

		FreeState();
		active = 0;
	}

	~DualShock3Device()
	{
	}
};

void EnumDualShock3s()
{
	HidDeviceInfo* foundDevs = 0;

	int numDevs = FindHids(&foundDevs, VID, PID);
	if (!numDevs)
		return;
	int index = 0;
	for (int i = 0; i < numDevs; i++)
	{
		if (foundDevs[i].caps.FeatureReportByteLength == 50 &&
			foundDevs[i].caps.OutputReportByteLength == 49)
		{
			HANDLE hDevice = CreateFileW(foundDevs[i].path,
				GENERIC_READ | GENERIC_WRITE,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				NULL,
				OPEN_EXISTING,
				0, NULL);
			if (hDevice != INVALID_HANDLE_VALUE)
			{
				unsigned char Buffer[50];

				Buffer[0] = 0; //reportId

				if (HidD_GetFeature(hDevice,
						Buffer,
						sizeof(Buffer)))
				{
					wchar_t temp[100];
					wsprintfW(temp, L"DualShock 3 #%i", index + 1);
					dm->AddDevice(new DualShock3Device(index, temp, foundDevs[i].path));
					index++;
				}

				CloseHandle(hDevice);
			}
		}
		free(foundDevs[i].path);
	}
	free(foundDevs);
}
