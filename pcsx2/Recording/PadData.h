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

#ifndef DISABLE_RECORDING

class PadData
{
public:
	/// Constants
	static const u8 s_ANALOG_VECTOR_NEUTRAL = 127;
	static const u16 s_END_INDEX_CONTROLLER_BUFFER = 17;

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
	u8 m_circlePressure = 0;
	u8 m_crossPressure = 0;
	u8 m_squarePressure = 0;
	u8 m_trianglePressure = 0;
	u8 m_downPressure = 0;
	u8 m_leftPressure = 0;
	u8 m_rightPressure = 0;
	u8 m_upPressure = 0;
	u8 m_l1Pressure = 0;
	u8 m_l2Pressure = 0;
	u8 m_r1Pressure = 0;
	u8 m_r2Pressure = 0;

	/// Pressure Button Flags
	/// NOTE - It shouldn't be possible to depress a button while also having no pressure
	/// But for the sake of completeness, it should be tracked.
	bool m_circlePressed = false;
	bool m_crossPressed = false;
	bool m_squarePressed = false;
	bool m_trianglePressed = false;
	bool m_downPressed = false;
	bool m_leftPressed = false;
	bool m_rightPressed = false;
	bool m_upPressed = false;
	bool m_l1Pressed = false;
	bool m_l2Pressed = false;
	bool m_r1Pressed = false;
	bool m_r2Pressed = false;

	/// Normal (un)pressed buttons
	bool m_select = false;
	bool m_start = false;
	bool m_l3 = false;
	bool m_r3 = false;

	/// Analog Sticks - 0-255 (127 center)
	u8 m_leftAnalogX = s_ANALOG_VECTOR_NEUTRAL;
	u8 m_leftAnalogY = s_ANALOG_VECTOR_NEUTRAL;
	u8 m_rightAnalogX = s_ANALOG_VECTOR_NEUTRAL;
	u8 m_rightAnalogY = s_ANALOG_VECTOR_NEUTRAL;

	// Given the input buffer and the current index, updates the correct field(s)
	void UpdateControllerData(u16 bufIndex, u8 const& bufVal);
	u8 PollControllerData(u16 bufIndex);

	// Prints current PadData to the Controller Log filter which disabled by default
	void LogPadData(u8 const& port);

private:
	struct ButtonResolver
	{
		u8 m_buttonBitmask;
	};

	const ButtonResolver m_LEFT = ButtonResolver{0b10000000};
	const ButtonResolver m_DOWN = ButtonResolver{0b01000000};
	const ButtonResolver m_RIGHT = ButtonResolver{0b00100000};
	const ButtonResolver m_UP = ButtonResolver{0b00010000};
	const ButtonResolver m_START = ButtonResolver{0b00001000};
	const ButtonResolver m_R3 = ButtonResolver{0b00000100};
	const ButtonResolver m_L3 = ButtonResolver{0b00000010};
	const ButtonResolver m_SELECT = ButtonResolver{0b00000001};

	const ButtonResolver m_SQUARE = ButtonResolver{0b10000000};
	const ButtonResolver m_CROSS = ButtonResolver{0b01000000};
	const ButtonResolver m_CIRCLE = ButtonResolver{0b00100000};
	const ButtonResolver m_TRIANGLE = ButtonResolver{0b00010000};
	const ButtonResolver m_R1 = ButtonResolver{0b00001000};
	const ButtonResolver m_L1 = ButtonResolver{0b00000100};
	const ButtonResolver m_R2 = ButtonResolver{0b00000010};
	const ButtonResolver m_L2 = ButtonResolver{0b00000001};

	// Checks and returns if button a is pressed or not
	bool IsButtonPressed(ButtonResolver buttonResolver, u8 const& bufVal);
	u8 BitmaskOrZero(bool pressed, ButtonResolver buttonInfo);

	wxString RawPadBytesToString(int start, int end);
};

#endif
