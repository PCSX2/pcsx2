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

#include "PrecompiledHeader.h"
#include "DebugTools/Debug.h"
#include "Recording/PadData.h"

#include <fmt/core.h>

const PadData::ButtonResolver PadData::s_LEFT    {0b10000000};
const PadData::ButtonResolver PadData::s_DOWN    {0b01000000};
const PadData::ButtonResolver PadData::s_RIGHT   {0b00100000};
const PadData::ButtonResolver PadData::s_UP      {0b00010000};
const PadData::ButtonResolver PadData::s_START   {0b00001000};
const PadData::ButtonResolver PadData::s_R3      {0b00000100};
const PadData::ButtonResolver PadData::s_L3      {0b00000010};
const PadData::ButtonResolver PadData::s_SELECT  {0b00000001};

const PadData::ButtonResolver PadData::s_SQUARE  {0b10000000};
const PadData::ButtonResolver PadData::s_CROSS   {0b01000000};
const PadData::ButtonResolver PadData::s_CIRCLE  {0b00100000};
const PadData::ButtonResolver PadData::s_TRIANGLE{0b00010000};
const PadData::ButtonResolver PadData::s_R1      {0b00001000};
const PadData::ButtonResolver PadData::s_L1      {0b00000100};
const PadData::ButtonResolver PadData::s_R2      {0b00000010};
const PadData::ButtonResolver PadData::s_L2      {0b00000001};

void PadData::UpdateControllerData(u16 bufIndex, u8 const& bufVal)
{
	const BufferIndex index = static_cast<BufferIndex>(bufIndex);
	switch (index)
	{
		case BufferIndex::PressedFlagsGroupOne:
			m_leftPressed = IsButtonPressed(s_LEFT, bufVal);
			m_downPressed = IsButtonPressed(s_DOWN, bufVal);
			m_rightPressed = IsButtonPressed(s_RIGHT, bufVal);
			m_upPressed = IsButtonPressed(s_UP, bufVal);
			m_start = IsButtonPressed(s_START, bufVal);
			m_r3 = IsButtonPressed(s_R3, bufVal);
			m_l3 = IsButtonPressed(s_L3, bufVal);
			m_select = IsButtonPressed(s_SELECT, bufVal);
			break;
		case BufferIndex::PressedFlagsGroupTwo:
			m_squarePressed = IsButtonPressed(s_SQUARE, bufVal);
			m_crossPressed = IsButtonPressed(s_CROSS, bufVal);
			m_circlePressed = IsButtonPressed(s_CIRCLE, bufVal);
			m_trianglePressed = IsButtonPressed(s_TRIANGLE, bufVal);
			m_r1Pressed = IsButtonPressed(s_R1, bufVal);
			m_l1Pressed = IsButtonPressed(s_L1, bufVal);
			m_r2Pressed = IsButtonPressed(s_R2, bufVal);
			m_l2Pressed = IsButtonPressed(s_L2, bufVal);
			break;
		case BufferIndex::RightAnalogXVector:
			m_rightAnalogX = bufVal;
			break;
		case BufferIndex::RightAnalogYVector:
			m_rightAnalogY = bufVal;
			break;
		case BufferIndex::LeftAnalogXVector:
			m_leftAnalogX = bufVal;
			break;
		case BufferIndex::LeftAnalogYVector:
			m_leftAnalogY = bufVal;
			break;
		case BufferIndex::RightPressure:
			m_rightPressure = bufVal;
			break;
		case BufferIndex::LeftPressure:
			m_leftPressure = bufVal;
			break;
		case BufferIndex::UpPressure:
			m_upPressure = bufVal;
			break;
		case BufferIndex::DownPressure:
			m_downPressure = bufVal;
			break;
		case BufferIndex::TrianglePressure:
			m_trianglePressure = bufVal;
			break;
		case BufferIndex::CirclePressure:
			m_circlePressure = bufVal;
			break;
		case BufferIndex::CrossPressure:
			m_crossPressure = bufVal;
			break;
		case BufferIndex::SquarePressure:
			m_squarePressure = bufVal;
			break;
		case BufferIndex::L1Pressure:
			m_l1Pressure = bufVal;
			break;
		case BufferIndex::R1Pressure:
			m_r1Pressure = bufVal;
			break;
		case BufferIndex::L2Pressure:
			m_l2Pressure = bufVal;
			break;
		case BufferIndex::R2Pressure:
			m_r2Pressure = bufVal;
			break;
	}
}

u8 PadData::PollControllerData(u16 bufIndex)
{
	u8 byte = 0;
	const BufferIndex index = static_cast<BufferIndex>(bufIndex);
	switch (index)
	{
		case BufferIndex::PressedFlagsGroupOne:
			// Construct byte by combining flags if the buttons are pressed
			byte |= BitmaskOrZero(m_leftPressed, s_LEFT);
			byte |= BitmaskOrZero(m_downPressed, s_DOWN);
			byte |= BitmaskOrZero(m_rightPressed, s_RIGHT);
			byte |= BitmaskOrZero(m_upPressed, s_UP);
			byte |= BitmaskOrZero(m_start, s_START);
			byte |= BitmaskOrZero(m_r3, s_R3);
			byte |= BitmaskOrZero(m_l3, s_L3);
			byte |= BitmaskOrZero(m_select, s_SELECT);
			// We flip the bits because as mentioned below, 0 = pressed
			return ~byte;
		case BufferIndex::PressedFlagsGroupTwo:
			// Construct byte by combining flags if the buttons are pressed
			byte |= BitmaskOrZero(m_squarePressed, s_SQUARE);
			byte |= BitmaskOrZero(m_crossPressed, s_CROSS);
			byte |= BitmaskOrZero(m_circlePressed, s_CIRCLE);
			byte |= BitmaskOrZero(m_trianglePressed, s_TRIANGLE);
			byte |= BitmaskOrZero(m_r1Pressed, s_R1);
			byte |= BitmaskOrZero(m_l1Pressed, s_L1);
			byte |= BitmaskOrZero(m_r2Pressed, s_R2);
			byte |= BitmaskOrZero(m_l2Pressed, s_L2);
			// We flip the bits because as mentioned below, 0 = pressed
			return ~byte;
		case BufferIndex::RightAnalogXVector:
			return m_rightAnalogX;
		case BufferIndex::RightAnalogYVector:
			return m_rightAnalogY;
		case BufferIndex::LeftAnalogXVector:
			return m_leftAnalogX;
		case BufferIndex::LeftAnalogYVector:
			return m_leftAnalogY;
		case BufferIndex::RightPressure:
			return m_rightPressure;
		case BufferIndex::LeftPressure:
			return m_leftPressure;
		case BufferIndex::UpPressure:
			return m_upPressure;
		case BufferIndex::DownPressure:
			return m_downPressure;
		case BufferIndex::TrianglePressure:
			return m_trianglePressure;
		case BufferIndex::CirclePressure:
			return m_circlePressure;
		case BufferIndex::CrossPressure:
			return m_crossPressure;
		case BufferIndex::SquarePressure:
			return m_squarePressure;
		case BufferIndex::L1Pressure:
			return m_l1Pressure;
		case BufferIndex::R1Pressure:
			return m_r1Pressure;
		case BufferIndex::L2Pressure:
			return m_l2Pressure;
		case BufferIndex::R2Pressure:
			return m_r2Pressure;
		default:
			return 0;
	}
}

bool PadData::IsButtonPressed(ButtonResolver buttonResolver, u8 const& bufVal)
{
	// Rather than the flags being SET if the button is pressed, it is the opposite
	// For example: 0111 1111 with `left` being the first bit indicates `left` is pressed.
	// So, we are forced to flip the pressed bits with a NOT first
	return (~bufVal & buttonResolver.buttonBitmask) > 0;
}

u8 PadData::BitmaskOrZero(bool pressed, ButtonResolver buttonInfo)
{
	return pressed ? buttonInfo.buttonBitmask : 0;
}

#ifndef PCSX2_CORE
// TODO - Vaser - kill with wxWidgets
// TODO - Vaser - replace with this something better in Qt
wxString PadData::RawPadBytesToString(int start, int end)
{
	wxString str;
	for (int i = start; i < end; i++)
	{
		str += wxString::Format("%d", PollControllerData(i));
		if (i != end - 1)
			str += ", ";
	}
	return str;
}

void PadData::LogPadData(u8 const& port)
{
	wxString pressedBytes = RawPadBytesToString(0, 2);
	wxString rightAnalogBytes = RawPadBytesToString(2, 4);
	wxString leftAnalogBytes = RawPadBytesToString(4, 6);
	wxString pressureBytes = RawPadBytesToString(6, 17);
	wxString fullLog =
		wxString::Format("[PAD %d] Raw Bytes: Pressed = [%s]\n", port + 1, pressedBytes) +
		wxString::Format("[PAD %d] Raw Bytes: Right Analog = [%s]\n", port + 1, rightAnalogBytes) +
		wxString::Format("[PAD %d] Raw Bytes: Left Analog = [%s]\n", port + 1, leftAnalogBytes) +
		wxString::Format("[PAD %d] Raw Bytes: Pressure = [%s]\n", port + 1, pressureBytes);
	controlLog(fullLog.ToUTF8());
}

#else

std::string PadData::RawPadBytesToString(int start, int end)
{
	std::string str;
	for (int i = start; i < end; i++)
	{
		str += fmt::format("{}", PollControllerData(i));

		if (i != end - 1)
			str += ", ";
	}
	return str;
}

void PadData::LogPadData(u8 const& port)
{
	std::string pressedBytes = RawPadBytesToString(0, 2);
	std::string rightAnalogBytes = RawPadBytesToString(2, 4);
	std::string leftAnalogBytes = RawPadBytesToString(4, 6);
	std::string pressureBytes = RawPadBytesToString(6, 17);
	std::string fullLog =
		fmt::format("[PAD {}] Raw Bytes: Pressed = [{}]\n", port + 1, pressedBytes) +
		fmt::format("[PAD {}] Raw Bytes: Right Analog = [{}]\n", port + 1, rightAnalogBytes) +
		fmt::format("[PAD {}] Raw Bytes: Left Analog = [{}]\n", port + 1, leftAnalogBytes) +
		fmt::format("[PAD {}] Raw Bytes: Pressure = [{}]\n", port + 1, pressureBytes);
	controlLog(fullLog);
}

#endif