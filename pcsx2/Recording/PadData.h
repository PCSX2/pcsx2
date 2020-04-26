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

#pragma once

class PadData
{
public:
	/// Constants
	const u8 PRESSURE_BUTTON_UNPRESSED = 0;
	const bool BUTTON_PRESSED = true;
	const bool BUTTON_UNPRESSED = false;
	const u8 ANALOG_VECTOR_CENTER_POS = 127;

	enum class BufferIndex
	{
		PressedFlagsGroupOne,
		PressedFlagsGroupTwo,
		RightAnalogXVector,
		RightAnalogYVector,
		LeftAnalogXVector,
		LeftAnalogYVector,
		RightPressure,
		LeftPressure,
		UpPressure,
		DownPressure,
		TrianglePressure,
		CirclePressure,
		CrossPressure,
		SquarePressure,
		L1Pressure,
		R1Pressure,
		L2Pressure,
		R2Pressure
	};

	/// Pressure Buttons - 0-255
	u8 circlePressure = PRESSURE_BUTTON_UNPRESSED;
	u8 crossPressure = PRESSURE_BUTTON_UNPRESSED;
	u8 squarePressure = PRESSURE_BUTTON_UNPRESSED;
	u8 trianglePressure = PRESSURE_BUTTON_UNPRESSED;
	u8 downPressure = PRESSURE_BUTTON_UNPRESSED;
	u8 leftPressure = PRESSURE_BUTTON_UNPRESSED;
	u8 rightPressure = PRESSURE_BUTTON_UNPRESSED;
	u8 upPressure = PRESSURE_BUTTON_UNPRESSED;
	u8 l1Pressure = PRESSURE_BUTTON_UNPRESSED;
	u8 l2Pressure = PRESSURE_BUTTON_UNPRESSED;
	u8 r1Pressure = PRESSURE_BUTTON_UNPRESSED;
	u8 r2Pressure = PRESSURE_BUTTON_UNPRESSED;

	/// Pressure Button Flags
	/// NOTE - It shouldn't be possible to depress a button
	///	while also having no pressure (PAD plugin should default to max pressure).
	/// But for the sake of completeness, it should be tracked.
	bool circlePressed = BUTTON_UNPRESSED;
	bool crossPressed = BUTTON_UNPRESSED;
	bool squarePressed = BUTTON_UNPRESSED;
	bool trianglePressed = BUTTON_UNPRESSED;
	bool downPressed = BUTTON_UNPRESSED;
	bool leftPressed = BUTTON_UNPRESSED;
	bool rightPressed = BUTTON_UNPRESSED;
	bool upPressed = BUTTON_UNPRESSED;
	bool l1Pressed = BUTTON_UNPRESSED;
	bool l2Pressed = BUTTON_UNPRESSED;
	bool r1Pressed = BUTTON_UNPRESSED;
	bool r2Pressed = BUTTON_UNPRESSED;

	/// Normal (un)pressed buttons
	bool select = BUTTON_UNPRESSED;
	bool start = BUTTON_UNPRESSED;
	bool l3 = BUTTON_UNPRESSED;
	bool r3 = BUTTON_UNPRESSED;

	/// Analog Sticks - 0-255 (127 center)
	u8 leftAnalogX = ANALOG_VECTOR_CENTER_POS;
	u8 leftAnalogY = ANALOG_VECTOR_CENTER_POS;
	u8 rightAnalogX = ANALOG_VECTOR_CENTER_POS;
	u8 rightAnalogY = ANALOG_VECTOR_CENTER_POS;

	// Given the input buffer and the current index, updates the correct field(s) 
	void UpdateControllerData(u16 bufIndex, u8 const &bufVal);
	u8 PollControllerData(u16 bufIndex);

	// Prints current PadData to the Controller Log filter which disabled by default
	void LogPadData();

private:
	struct ButtonResolver
	{
		u8 buttonBitmask;
	};

	const ButtonResolver LEFT = ButtonResolver{ 0b10000000 };
	const ButtonResolver DOWN = ButtonResolver{ 0b01000000 };
	const ButtonResolver RIGHT = ButtonResolver{ 0b00100000 };
	const ButtonResolver UP = ButtonResolver{ 0b00010000 };
	const ButtonResolver START = ButtonResolver{ 0b00001000 };
	const ButtonResolver R3 = ButtonResolver{ 0b00000100 };
	const ButtonResolver L3 = ButtonResolver{ 0b00000010 };
	const ButtonResolver SELECT = ButtonResolver{ 0b00000001 };

	const ButtonResolver SQUARE = ButtonResolver{ 0b10000000 };
	const ButtonResolver CROSS = ButtonResolver{ 0b01000000 };
	const ButtonResolver CIRCLE = ButtonResolver{ 0b00100000 };
	const ButtonResolver TRIANGLE = ButtonResolver{ 0b00010000 };
	const ButtonResolver R1 = ButtonResolver{ 0b00001000 };
	const ButtonResolver L1 = ButtonResolver{ 0b00000100 };
	const ButtonResolver R2 = ButtonResolver{ 0b00000010 };
	const ButtonResolver L2 = ButtonResolver{ 0b00000001 };

	// Checks and returns if button a is pressed or not
	bool IsButtonPressed(ButtonResolver buttonResolver, u8 const &bufVal);
	u8 BitmaskOrZero(bool pressed, ButtonResolver buttonInfo);

	wxString RawPadBytesToString(int start, int end);
};
#endif
