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

#pragma once

#include <map>
#include <vector>


#ifndef DISABLE_RECORDING
static const int PadDataNormalButtonCount = 16;
enum PadData_NormalButton
{
	PadData_NormalButton_UP,
	PadData_NormalButton_RIGHT,
	PadData_NormalButton_LEFT,
	PadData_NormalButton_DOWN,
	PadData_NormalButton_CROSS,
	PadData_NormalButton_CIRCLE,
	PadData_NormalButton_SQUARE,
	PadData_NormalButton_TRIANGLE,
	PadData_NormalButton_L1,
	PadData_NormalButton_L2,
	PadData_NormalButton_R1,
	PadData_NormalButton_R2,
	PadData_NormalButton_L3,
	PadData_NormalButton_R3,
	PadData_NormalButton_SELECT,
	PadData_NormalButton_START
};

static const int PadDataAnalogVectorCount = 4;
enum PadData_AnalogVector
{
	PadData_AnalogVector_LEFT_ANALOG_X,
	PadData_AnalogVector_LEFT_ANALOG_Y,
	PadData_AnalogVector_RIGHT_ANALOG_X,
	PadData_AnalogVector_RIGHT_ANALOG_Y
};

struct PadData
{
public:
	PadData();
	~PadData() {}

	bool fExistKey = false;
	u8 buf[2][18];

	// Prints controlller data every frame to the Controller Log filter, disabled by default
	static void LogPadData(u8 port, u16 bufCount, u8 buf[512]);

	// Normal Buttons
	std::vector<int> GetNormalButtons(int port) const;
	void SetNormalButtons(int port, std::vector<int> buttons);

	// Analog Vectors
	// max left/up    : 0
	// neutral        : 127
	// max right/down : 255
	std::vector<int> GetAnalogVectors(int port) const;
	// max left/up    : 0
	// neutral        : 127
	// max right/down : 255
	void SetAnalogVectors(int port, std::vector<int> vector);

private:
	void SetNormalButton(int port, PadData_NormalButton button, int pressure);
	int GetNormalButton(int port, PadData_NormalButton button) const;
	void GetKeyBit(wxByte keybit[2], PadData_NormalButton button) const;
	int GetPressureByte(PadData_NormalButton button) const;

	void SetAnalogVector(int port, PadData_AnalogVector vector, int val);
	int GetAnalogVector(int port, PadData_AnalogVector vector) const;
	int GetAnalogVectorByte(PadData_AnalogVector vector) const;
};
#endif
