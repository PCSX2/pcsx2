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

#define PadDataNormalButtonCount 16
enum PadDataNormalButton
{
	UP,
	RIGHT,
	LEFT,
	DOWN,
	CROSS,
	CIRCLE,
	SQUARE,
	TRIANGLE,
	L1,
	L2,
	R1,
	R2,
	L3,
	R3,
	SELECT,
	START
};

#define PadDataAnalogVectorCount 4
enum PadDataAnalogVector
{
	LEFT_ANALOG_X,
	LEFT_ANALOG_Y,
	RIGHT_ANALOG_X,
	RIGHT_ANALOG_Y
};

struct PadData
{
public:
	PadData();
	~PadData() {}

	bool fExistKey = false;
	u8 buf[2][18];

	// Prints controlller data every frame to the Controller Log filter, disabled by default
	static void logPadData(u8 port, u16 bufCount, u8 buf[512]);

	// Normal Buttons
	int* getNormalButtons(int port) const;
	void setNormalButtons(int port, int* buttons);

	// Analog Vectors
	// max left/up    : 0
	// neutral        : 127
	// max right/down : 255
	int* getAnalogVectors(int port) const;
	// max left/up    : 0
	// neutral        : 127
	// max right/down : 255
	void setAnalogVectors(int port, int* vector);

private:
	void setNormalButton(int port, PadDataNormalButton button, int pressure);
	int getNormalButton(int port, PadDataNormalButton button) const;
	void getKeyBit(wxByte keybit[2], PadDataNormalButton button) const;
	int getPressureByte(PadDataNormalButton button) const;

	void setAnalogVector(int port, PadDataAnalogVector vector, int val);
	int getAnalogVector(int port, PadDataAnalogVector vector) const;
	int getAnalogVectorByte(PadDataAnalogVector vector) const;
};
