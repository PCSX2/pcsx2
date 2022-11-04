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
	/// Constants
	static constexpr u8 ANALOG_VECTOR_NEUTRAL = 127;

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

	/// Analog Sticks - 0-255 (127 center)
	u8 m_leftAnalogX = ANALOG_VECTOR_NEUTRAL;
	u8 m_leftAnalogY = ANALOG_VECTOR_NEUTRAL;
	u8 m_rightAnalogX = ANALOG_VECTOR_NEUTRAL;
	u8 m_rightAnalogY = ANALOG_VECTOR_NEUTRAL;

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

	u8* const m_allIntensities[16]{
		&m_rightAnalogX,
		&m_rightAnalogY,
		&m_leftAnalogX,
		&m_leftAnalogY,
		&m_rightPressure,
		&m_leftPressure,
		&m_upPressure,
		&m_downPressure,
		&m_trianglePressure,
		&m_circlePressure,
		&m_crossPressure,
		&m_squarePressure,
		&m_l1Pressure,
		&m_r1Pressure,
		&m_l2Pressure,
		&m_r2Pressure,
	};

	/// Pressure Button Flags
	struct ButtonFlag
	{
		bool m_pressed = false;
		const u8 m_BITMASK;
		constexpr ButtonFlag(u8 maskValue)
			: m_BITMASK(maskValue)
		{
		}

		void setPressedState(u8 bufVal) noexcept
		{
			m_pressed = (~bufVal & m_BITMASK) > 0;
		}

		u8 getMaskIfPressed() const noexcept
		{
			return m_pressed ? m_BITMASK : 0;
		}
	};
	/// NOTE - It shouldn't be possible to depress a button while also having no pressure
	/// But for the sake of completeness, it should be tracked.
	ButtonFlag m_circlePressed{0b00100000};
	ButtonFlag m_crossPressed{0b01000000};
	ButtonFlag m_squarePressed{0b10000000};
	ButtonFlag m_trianglePressed{0b00010000};
	ButtonFlag m_downPressed{0b01000000};
	ButtonFlag m_leftPressed{0b10000000};
	ButtonFlag m_rightPressed{0b00100000};
	ButtonFlag m_upPressed{0b00010000};
	ButtonFlag m_l1Pressed{0b00000100};
	ButtonFlag m_l2Pressed{0b00000001};
	ButtonFlag m_r1Pressed{0b00001000};
	ButtonFlag m_r2Pressed{0b00000010};

	/// Normal (un)pressed buttons
	ButtonFlag m_select{0b00000001};
	ButtonFlag m_start{0b00001000};
	ButtonFlag m_l3{0b00000010};
	ButtonFlag m_r3{0b00000100};

	// Given the input buffer and the current index, updates the correct field(s)
	void UpdateControllerData(u16 bufIndex, u8 const bufVal) noexcept;
	u8 PollControllerData(u16 bufIndex) const noexcept;

	// Prints current PadData to the Controller Log filter which disabled by default
	void LogPadData(u8 const& port);

private:

#ifndef PCSX2_CORE
	wxString RawPadBytesToString(int start, int end);
#else
	std::string RawPadBytesToString(int start, int end);
#endif
};

#endif
