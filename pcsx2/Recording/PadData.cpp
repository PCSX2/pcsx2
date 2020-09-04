/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2019  PCSX2 Dev Team
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

#include "Common.h"
#include "ConsoleLogger.h"
#include "PadData.h"


#ifndef DISABLE_RECORDING
PadData::PadData()
{
	// TODO - multi-tap support eventually?
	for (int port = 0; port < 2; port++)
	{
		buf[port][0] = 255;
		buf[port][1] = 255;
		buf[port][2] = 127;
		buf[port][3] = 127;
		buf[port][4] = 127;
		buf[port][5] = 127;
	}
}

void PadData::LogPadData(u8 port, u16 bufCount, u8 buf[512]) {
	// skip first two bytes because they dont seem to matter
	if (port == 0 && bufCount > 2)
	{
		if (bufCount == 3)
		{
			controlLog(wxString::Format("\nController Port %d", port));
			controlLog(wxString::Format("\nPressed Flags - "));
		}
		if (bufCount == 5) // analog sticks
		{
			controlLog(wxString::Format("\nAnalog Sticks - "));
		}
		if (bufCount == 9) // pressure sensitive bytes
		{
			controlLog(wxString::Format("\nPressure Bytes - "));
		}
		controlLog(wxString::Format("%3d ", buf[bufCount]));
	}
}

std::vector<int> PadData::GetNormalButtons(int port) const
{
	std::vector<int> buttons(PadDataNormalButtonCount);
	for (int i = 0; i < PadDataNormalButtonCount; i++)
	{
		buttons[i] = GetNormalButton(port, PadData_NormalButton(i));
	}
	return buttons;
}

void PadData::SetNormalButtons(int port, std::vector<int> buttons)
{
	for (int i = 0; i < PadDataNormalButtonCount; i++)
	{
		SetNormalButton(port, PadData_NormalButton(i), buttons[i]);
	}
}

void PadData::SetNormalButton(int port, PadData_NormalButton button, int fpushed)
{
	wxByte keybit[2];
	GetKeyBit(keybit, button);
	int pressureByteIndex = GetPressureByte(button);

	if (fpushed > 0)
	{
		// set whether or not the button is pressed
		buf[port][0] = ~(~buf[port][0] | keybit[0]);
		buf[port][1] = ~(~buf[port][1] | keybit[1]);

		// if the button supports pressure sensitivity
		if (pressureByteIndex != -1)
		{
			buf[port][6 + pressureByteIndex] = fpushed;
		}
	}
	else
	{
		buf[port][0] = (buf[port][0] | keybit[0]);
		buf[port][1] = (buf[port][1] | keybit[1]);

		// if the button supports pressure sensitivity
		if (pressureByteIndex != -1)
		{
			buf[port][6 + pressureByteIndex] = 0;
		}
	}
}

int PadData::GetNormalButton(int port, PadData_NormalButton button) const
{
	wxByte keybit[2];
	GetKeyBit(keybit, button);
	int pressureByteIndex = GetPressureByte(button);

	// If the button is pressed on either controller
	bool f1 = (~buf[port][0] & keybit[0])>0;
	bool f2 = (~buf[port][1] & keybit[1])>0;

	if (f1 || f2)
	{
		// If the button does not support pressure sensitive inputs
		// just return 1 for pressed.
		if (pressureByteIndex == -1)
		{
			return 1;
		}
		// else return the pressure information
		return buf[port][6 + pressureByteIndex];
	}

	// else the button isnt pressed at all
	return 0;
}

void PadData::GetKeyBit(wxByte keybit[2], PadData_NormalButton button) const
{
	switch (button)
	{
		case PadData_NormalButton_LEFT:
			keybit[0] = 0b10000000;
			keybit[1] = 0b00000000;
			break;
		case PadData_NormalButton_DOWN:
			keybit[0] = 0b01000000;
			keybit[1] = 0b00000000;
			break;
		case PadData_NormalButton_RIGHT:
			keybit[0] = 0b00100000;
			keybit[1] = 0b00000000;
			break;
		case PadData_NormalButton_UP:
			keybit[0] = 0b00010000;
			keybit[1] = 0b00000000;
			break;
		case PadData_NormalButton_START:
			keybit[0] = 0b00001000;
			keybit[1] = 0b00000000;
			break;
		case PadData_NormalButton_R3:
			keybit[0] = 0b00000100;
			keybit[1] = 0b00000000;
			break;
		case PadData_NormalButton_L3:
			keybit[0] = 0b00000010;
			keybit[1] = 0b00000000;
			break;
		case PadData_NormalButton_SELECT:
			keybit[0] = 0b00000001;
			keybit[1] = 0b00000000;
			break;
		case PadData_NormalButton_SQUARE:
			keybit[0] = 0b00000000;
			keybit[1] = 0b10000000;
			break;
		case PadData_NormalButton_CROSS:
			keybit[0] = 0b00000000;
			keybit[1] = 0b01000000;
			break;
		case PadData_NormalButton_CIRCLE:
			keybit[0] = 0b00000000;
			keybit[1] = 0b00100000;
			break;
		case PadData_NormalButton_TRIANGLE:
			keybit[0] = 0b00000000;
			keybit[1] = 0b00010000;
			break;
		case PadData_NormalButton_R1:
			keybit[0] = 0b00000000;
			keybit[1] = 0b00001000;
			break;
		case PadData_NormalButton_L1:
			keybit[0] = 0b00000000;
			keybit[1] = 0b00000100;
			break;
		case PadData_NormalButton_R2:
			keybit[0] = 0b00000000;
			keybit[1] = 0b00000010;
			break;
		case PadData_NormalButton_L2:
			keybit[0] = 0b00000000;
			keybit[1] = 0b00000001;
			break;
		default:
			keybit[0] = 0;
			keybit[1] = 0;
	}
}

// Returns an index for the buffer to set the pressure byte
// Returns -1 if it is a button that does not support pressure sensitivty
int PadData::GetPressureByte(PadData_NormalButton button) const
{
	// Pressure Byte Order
	// R - L - U - D - Tri - Circle - Cross - Sqr - L1 - R1 - L2 - R2
	switch (button)
	{
		case PadData_NormalButton_RIGHT:
			return 0;
			break;
		case PadData_NormalButton_LEFT:
			return 1;
			break;
		case PadData_NormalButton_UP:
			return 2;
			break;
		case PadData_NormalButton_DOWN:
			return 3;
			break;
		case PadData_NormalButton_TRIANGLE:
			return 4;
			break;
		case PadData_NormalButton_CIRCLE:
			return 5;
			break;
		case PadData_NormalButton_CROSS:
			return 6;
			break;
		case PadData_NormalButton_SQUARE:
			return 7;
			break;
		case PadData_NormalButton_L1:
			return 8;
			break;
		case PadData_NormalButton_R1:
			return 9;
			break;
		case PadData_NormalButton_L2:
			return 10;
			break;
		case PadData_NormalButton_R2:
			return 11;
			break;
		default:
			return -1;
	}
}

std::vector<int> PadData::GetAnalogVectors(int port) const
{
	std::vector<int> vectors(PadDataAnalogVectorCount);
	for (int i = 0; i < PadDataAnalogVectorCount; i++)
	{
		vectors[i] = GetAnalogVector(port, PadData_AnalogVector(i));
	}
	return vectors;
}

void PadData::SetAnalogVectors(int port, std::vector<int> vectors)
{
	for (int i = 0; i < PadDataAnalogVectorCount; i++)
	{
		SetAnalogVector(port, PadData_AnalogVector(i), vectors[i]);
	}
}

void PadData::SetAnalogVector(int port, PadData_AnalogVector vector, int val)
{
	if (val < 0)
	{
		val = 0;
	}
	else if (val > 255)
	{
		val = 255;
	}

	buf[port][GetAnalogVectorByte(vector)] = val;
}

int PadData::GetAnalogVector(int port, PadData_AnalogVector vector) const
{

	return buf[port][GetAnalogVectorByte(vector)];
}

// Returns an index for the buffer to set the analog's vector
int PadData::GetAnalogVectorByte(PadData_AnalogVector vector) const
{
	// Vector Byte Ordering
	// RX - RY - LX - LY
	switch (vector)
	{
		case PadData_AnalogVector_RIGHT_ANALOG_X:
			return 2;
			break;
		case PadData_AnalogVector_RIGHT_ANALOG_Y:
			return 3;
			break;
		case PadData_AnalogVector_LEFT_ANALOG_X:
			return 4;
			break;
		case PadData_AnalogVector_LEFT_ANALOG_Y:
			return 5;
			break;
		default:
			return -1;
	}
}
#endif
