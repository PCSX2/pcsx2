/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#ifndef PCSX2_CORE

class PadData
{
public:
	/// Constants
	static const u8 ANALOG_VECTOR_NEUTRAL = 127;

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
	u8 circlePressure = 0;
	u8 crossPressure = 0;
	u8 squarePressure = 0;
	u8 trianglePressure = 0;
	u8 downPressure = 0;
	u8 leftPressure = 0;
	u8 rightPressure = 0;
	u8 upPressure = 0;
	u8 l1Pressure = 0;
	u8 l2Pressure = 0;
	u8 r1Pressure = 0;
	u8 r2Pressure = 0;

	/// Pressure Button Flags
	/// NOTE - It shouldn't be possible to depress a button while also having no pressure
	/// But for the sake of completeness, it should be tracked.
	bool circlePressed = false;
	bool crossPressed = false;
	bool squarePressed = false;
	bool trianglePressed = false;
	bool downPressed = false;
	bool leftPressed = false;
	bool rightPressed = false;
	bool upPressed = false;
	bool l1Pressed = false;
	bool l2Pressed = false;
	bool r1Pressed = false;
	bool r2Pressed = false;

	/// Normal (un)pressed buttons
	bool select = false;
	bool start = false;
	bool l3 = false;
	bool r3 = false;

	/// Analog Sticks - 0-255 (127 center)
	u8 leftAnalogX = ANALOG_VECTOR_NEUTRAL;
	u8 leftAnalogY = ANALOG_VECTOR_NEUTRAL;
	u8 rightAnalogX = ANALOG_VECTOR_NEUTRAL;
	u8 rightAnalogY = ANALOG_VECTOR_NEUTRAL;

	// Given the input buffer and the current index, updates the correct field(s)
	void UpdateControllerData(u16 bufIndex, u8 const& bufVal);
	u8 PollControllerData(u16 bufIndex);

	// Prints current PadData to the Controller Log filter which disabled by default
	void LogPadData(u8 const& port);

private:
	struct ButtonResolver
	{
		u8 buttonBitmask;
	};

	const ButtonResolver LEFT = ButtonResolver{0b10000000};
	const ButtonResolver DOWN = ButtonResolver{0b01000000};
	const ButtonResolver RIGHT = ButtonResolver{0b00100000};
	const ButtonResolver UP = ButtonResolver{0b00010000};
	const ButtonResolver START = ButtonResolver{0b00001000};
	const ButtonResolver R3 = ButtonResolver{0b00000100};
	const ButtonResolver L3 = ButtonResolver{0b00000010};
	const ButtonResolver SELECT = ButtonResolver{0b00000001};

	const ButtonResolver SQUARE = ButtonResolver{0b10000000};
	const ButtonResolver CROSS = ButtonResolver{0b01000000};
	const ButtonResolver CIRCLE = ButtonResolver{0b00100000};
	const ButtonResolver TRIANGLE = ButtonResolver{0b00010000};
	const ButtonResolver R1 = ButtonResolver{0b00001000};
	const ButtonResolver L1 = ButtonResolver{0b00000100};
	const ButtonResolver R2 = ButtonResolver{0b00000010};
	const ButtonResolver L2 = ButtonResolver{0b00000001};

	// Checks and returns if button a is pressed or not
	bool IsButtonPressed(ButtonResolver buttonResolver, u8 const& bufVal);
	u8 BitmaskOrZero(bool pressed, ButtonResolver buttonInfo);

#ifndef PCSX2_CORE
	wxString RawPadBytesToString(int start, int end);
#else
	std::string RawPadBytesToString(int start, int end);
#endif
};

#else

class PadData
{
public:
	/// Create a struct containing the PAD data from the global PAD state
	/// see - `g_key_status`
	PadData(const int port, const int slot);
	PadData(const int port, const int slot, const std::array<u8, 18> data);

	/// Constants
	static constexpr u8 ANALOG_VECTOR_NEUTRAL = 127;

	int m_ext_port;
	int m_port;
	int m_slot;

	// Analog Sticks - <x, y> - 0-255 (127 center)
	std::tuple<u8, u8> m_rightAnalog = {ANALOG_VECTOR_NEUTRAL, ANALOG_VECTOR_NEUTRAL};
	std::tuple<u8, u8> m_leftAnalog = {ANALOG_VECTOR_NEUTRAL, ANALOG_VECTOR_NEUTRAL};

	u8 m_compactPressFlagsGroupOne = 255;
	u8 m_compactPressFlagsGroupTwo = 255;

	// Buttons <pressed, pressure (0-255)>
	std::tuple<bool, u8> m_circle = {false, 0};
	std::tuple<bool, u8> m_cross = {false, 0};
	std::tuple<bool, u8> m_square = {false, 0};
	std::tuple<bool, u8> m_triangle = {false, 0};

	std::tuple<bool, u8> m_down = {false, 0};
	std::tuple<bool, u8> m_left = {false, 0};
	std::tuple<bool, u8> m_right = {false, 0};
	std::tuple<bool, u8> m_up = {false, 0};

	std::tuple<bool, u8> m_l1 = {false, 0};
	std::tuple<bool, u8> m_l2 = {false, 0};
	std::tuple<bool, u8> m_r1 = {false, 0};
	std::tuple<bool, u8> m_r2 = {false, 0};

	// Buttons <pressed>
	bool m_start = false;
	bool m_select = false;
	bool m_l3 = false;
	bool m_r3 = false;

	// Overrides the actual controller's state with the the values in this struct
	void OverrideActualController() const;

	// Prints current PadData to the Controller Log filter which is disabled by default
	void LogPadData() const;

private:

#ifndef PCSX2_CORE
	wxString RawPadBytesToString(int start, int end);
#endif
};

#endif
