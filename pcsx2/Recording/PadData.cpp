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

#ifndef PCSX2_CORE

void PadData::UpdateControllerData(u16 bufIndex, u8 const& bufVal)
{
	const BufferIndex index = static_cast<BufferIndex>(bufIndex);
	switch (index)
	{
		case BufferIndex::PressedFlagsGroupOne:
			leftPressed = IsButtonPressed(LEFT, bufVal);
			downPressed = IsButtonPressed(DOWN, bufVal);
			rightPressed = IsButtonPressed(RIGHT, bufVal);
			upPressed = IsButtonPressed(UP, bufVal);
			start = IsButtonPressed(START, bufVal);
			r3 = IsButtonPressed(R3, bufVal);
			l3 = IsButtonPressed(L3, bufVal);
			select = IsButtonPressed(SELECT, bufVal);
			break;
		case BufferIndex::PressedFlagsGroupTwo:
			squarePressed = IsButtonPressed(SQUARE, bufVal);
			crossPressed = IsButtonPressed(CROSS, bufVal);
			circlePressed = IsButtonPressed(CIRCLE, bufVal);
			trianglePressed = IsButtonPressed(TRIANGLE, bufVal);
			r1Pressed = IsButtonPressed(R1, bufVal);
			l1Pressed = IsButtonPressed(L1, bufVal);
			r2Pressed = IsButtonPressed(R2, bufVal);
			l2Pressed = IsButtonPressed(L2, bufVal);
			break;
		case BufferIndex::RightAnalogXVector:
			rightAnalogX = bufVal;
			break;
		case BufferIndex::RightAnalogYVector:
			rightAnalogY = bufVal;
			break;
		case BufferIndex::LeftAnalogXVector:
			leftAnalogX = bufVal;
			break;
		case BufferIndex::LeftAnalogYVector:
			leftAnalogY = bufVal;
			break;
		case BufferIndex::RightPressure:
			rightPressure = bufVal;
			break;
		case BufferIndex::LeftPressure:
			leftPressure = bufVal;
			break;
		case BufferIndex::UpPressure:
			upPressure = bufVal;
			break;
		case BufferIndex::DownPressure:
			downPressure = bufVal;
			break;
		case BufferIndex::TrianglePressure:
			trianglePressure = bufVal;
			break;
		case BufferIndex::CirclePressure:
			circlePressure = bufVal;
			break;
		case BufferIndex::CrossPressure:
			crossPressure = bufVal;
			break;
		case BufferIndex::SquarePressure:
			squarePressure = bufVal;
			break;
		case BufferIndex::L1Pressure:
			l1Pressure = bufVal;
			break;
		case BufferIndex::R1Pressure:
			r1Pressure = bufVal;
			break;
		case BufferIndex::L2Pressure:
			l2Pressure = bufVal;
			break;
		case BufferIndex::R2Pressure:
			r2Pressure = bufVal;
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
			byte |= BitmaskOrZero(leftPressed, LEFT);
			byte |= BitmaskOrZero(downPressed, DOWN);
			byte |= BitmaskOrZero(rightPressed, RIGHT);
			byte |= BitmaskOrZero(upPressed, UP);
			byte |= BitmaskOrZero(start, START);
			byte |= BitmaskOrZero(r3, R3);
			byte |= BitmaskOrZero(l3, L3);
			byte |= BitmaskOrZero(select, SELECT);
			// We flip the bits because as mentioned below, 0 = pressed
			return ~byte;
		case BufferIndex::PressedFlagsGroupTwo:
			// Construct byte by combining flags if the buttons are pressed
			byte |= BitmaskOrZero(squarePressed, SQUARE);
			byte |= BitmaskOrZero(crossPressed, CROSS);
			byte |= BitmaskOrZero(circlePressed, CIRCLE);
			byte |= BitmaskOrZero(trianglePressed, TRIANGLE);
			byte |= BitmaskOrZero(r1Pressed, R1);
			byte |= BitmaskOrZero(l1Pressed, L1);
			byte |= BitmaskOrZero(r2Pressed, R2);
			byte |= BitmaskOrZero(l2Pressed, L2);
			// We flip the bits because as mentioned below, 0 = pressed
			return ~byte;
		case BufferIndex::RightAnalogXVector:
			return rightAnalogX;
		case BufferIndex::RightAnalogYVector:
			return rightAnalogY;
		case BufferIndex::LeftAnalogXVector:
			return leftAnalogX;
		case BufferIndex::LeftAnalogYVector:
			return leftAnalogY;
		case BufferIndex::RightPressure:
			return rightPressure;
		case BufferIndex::LeftPressure:
			return leftPressure;
		case BufferIndex::UpPressure:
			return upPressure;
		case BufferIndex::DownPressure:
			return downPressure;
		case BufferIndex::TrianglePressure:
			return trianglePressure;
		case BufferIndex::CirclePressure:
			return circlePressure;
		case BufferIndex::CrossPressure:
			return crossPressure;
		case BufferIndex::SquarePressure:
			return squarePressure;
		case BufferIndex::L1Pressure:
			return l1Pressure;
		case BufferIndex::R1Pressure:
			return r1Pressure;
		case BufferIndex::L2Pressure:
			return l2Pressure;
		case BufferIndex::R2Pressure:
			return r2Pressure;
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

#include <fmt/core.h>

#include "PAD/Host/KeyStatus.h"
#include "Sio.h"

PadData::PadData(const int port, const int slot)
{
	m_port = port;
	m_slot = slot;
	m_ext_port = sioConvertPortAndSlotToPad(m_port, m_slot);
	// Get the state of the buttons
	// TODO - for the new recording file format, allow informing max number of buttons per frame per controller as well (ie. the analog button)
	const u32 buttons = g_key_status.GetButtons(m_ext_port);
	// - pressed group one
	//	 - left
	//	 - down
	//	 - right
	//	 - up
	//	 - start
	//	 - r3
	//	 - l3
	//	 - select
	m_compactPressFlagsGroupOne = (buttons & 0b1111111100000000) >> 8;
	// - pressed group two
	//	 - square
	//	 - cross
	//	 - circle
	//	 - triangle
	//	 - r1
	//	 - l1
	//	 - r2
	//	 - l2
	m_compactPressFlagsGroupTwo = (buttons & 0b11111111);
	// Get the analog values
	m_rightAnalog = g_key_status.GetRawRightAnalog(m_ext_port);
	m_leftAnalog = g_key_status.GetRawLeftAnalog(m_ext_port);
	// Get pressure bytes (12 of them)
	m_left = {(0b10000000 & m_compactPressFlagsGroupOne) == 0, g_key_status.GetRawPressure(m_ext_port, gamePadValues::PAD_LEFT)};
	m_down = {(0b01000000 & m_compactPressFlagsGroupOne) == 0, g_key_status.GetRawPressure(m_ext_port, gamePadValues::PAD_DOWN)};
	m_right = {(0b00100000 & m_compactPressFlagsGroupOne) == 0, g_key_status.GetRawPressure(m_ext_port, gamePadValues::PAD_RIGHT)};
	m_up = {(0b00010000 & m_compactPressFlagsGroupOne) == 0, g_key_status.GetRawPressure(m_ext_port, gamePadValues::PAD_UP)};
	m_start = (0b00001000 & m_compactPressFlagsGroupOne) == 0;
	m_r3 = (0b00000100 & m_compactPressFlagsGroupOne) == 0;
	m_l3 = (0b00000010 & m_compactPressFlagsGroupOne) == 0;
	m_select = (0b00000001 & m_compactPressFlagsGroupOne) == 0;

	m_square = {(0b10000000 & m_compactPressFlagsGroupTwo) == 0, g_key_status.GetRawPressure(m_ext_port, gamePadValues::PAD_SQUARE)};
	m_cross = {(0b01000000 & m_compactPressFlagsGroupTwo) == 0, g_key_status.GetRawPressure(m_ext_port, gamePadValues::PAD_CROSS)};
	m_circle = {(0b00100000 & m_compactPressFlagsGroupTwo) == 0, g_key_status.GetRawPressure(m_ext_port, gamePadValues::PAD_CIRCLE)};
	m_triangle = {(0b00010000 & m_compactPressFlagsGroupTwo) == 0, g_key_status.GetRawPressure(m_ext_port, gamePadValues::PAD_TRIANGLE)};
	m_r1 = {(0b00001000 & m_compactPressFlagsGroupTwo) == 0, g_key_status.GetRawPressure(m_ext_port, gamePadValues::PAD_R1)};
	m_l1 = {(0b00000100 & m_compactPressFlagsGroupTwo) == 0, g_key_status.GetRawPressure(m_ext_port, gamePadValues::PAD_L1)};
	m_r2 = {(0b00000010 & m_compactPressFlagsGroupTwo) == 0, g_key_status.GetRawPressure(m_ext_port, gamePadValues::PAD_R2)};
	m_l2 = {(0b00000001 & m_compactPressFlagsGroupTwo) == 0, g_key_status.GetRawPressure(m_ext_port, gamePadValues::PAD_L2)};
}

PadData::PadData(const int port, const int slot, const std::array<u8, 18> data)
{
	m_port = port;
	m_slot = slot;
	m_ext_port = sioConvertPortAndSlotToPad(m_port, m_slot);

	m_compactPressFlagsGroupOne = data.at(0);
	m_compactPressFlagsGroupTwo = data.at(1);

	m_rightAnalog = {data.at(2), data.at(3)};
	m_leftAnalog = {data.at(4), data.at(5)};

	m_left = {(0b10000000 & m_compactPressFlagsGroupOne) == 0, data.at(7)};
	m_down = {(0b01000000 & m_compactPressFlagsGroupOne) == 0, data.at(9)};
	m_right = {(0b00100000 & m_compactPressFlagsGroupOne) == 0, data.at(6)};
	m_up = {(0b00010000 & m_compactPressFlagsGroupOne) == 0, data.at(8)};
	m_start = (0b00001000 & m_compactPressFlagsGroupOne) == 0;
	m_r3 = (0b00000100 & m_compactPressFlagsGroupOne) == 0;
	m_l3 = (0b00000010 & m_compactPressFlagsGroupOne) == 0;
	m_select = (0b00000001 & m_compactPressFlagsGroupOne) == 0;

	m_square = {(0b10000000 & m_compactPressFlagsGroupTwo) == 0, data.at(13)};
	m_cross = {(0b01000000 & m_compactPressFlagsGroupTwo) == 0, data.at(12)};
	m_circle = {(0b00100000 & m_compactPressFlagsGroupTwo) == 0, data.at(11)};
	m_triangle = {(0b00010000 & m_compactPressFlagsGroupTwo) == 0, data.at(10)};
	m_r1 = {(0b00001000 & m_compactPressFlagsGroupTwo) == 0, data.at(15)};
	m_l1 = {(0b00000100 & m_compactPressFlagsGroupTwo) == 0, data.at(14)};
	m_r2 = {(0b00000010 & m_compactPressFlagsGroupTwo) == 0, data.at(17)};
	m_l2 = {(0b00000001 & m_compactPressFlagsGroupTwo) == 0, data.at(16)};
}

void PadData::OverrideActualController() const
{
	g_key_status.SetRawAnalogs(m_ext_port, m_leftAnalog, m_rightAnalog);

	g_key_status.Set(m_ext_port, PAD_RIGHT, std::get<1>(m_right));
	g_key_status.Set(m_ext_port, PAD_LEFT, std::get<1>(m_left));
	g_key_status.Set(m_ext_port, PAD_UP, std::get<1>(m_up));
	g_key_status.Set(m_ext_port, PAD_DOWN, std::get<1>(m_down));
	g_key_status.Set(m_ext_port, PAD_START, m_start);
	g_key_status.Set(m_ext_port, PAD_SELECT, m_select);
	g_key_status.Set(m_ext_port, PAD_R3, m_r3);
	g_key_status.Set(m_ext_port, PAD_L3, m_l3);

	g_key_status.Set(m_ext_port, PAD_SQUARE, std::get<1>(m_square));
	g_key_status.Set(m_ext_port, PAD_CROSS, std::get<1>(m_cross));
	g_key_status.Set(m_ext_port, PAD_CIRCLE, std::get<1>(m_circle));
	g_key_status.Set(m_ext_port, PAD_TRIANGLE, std::get<1>(m_triangle));

	g_key_status.Set(m_ext_port, PAD_R1, std::get<1>(m_r1));
	g_key_status.Set(m_ext_port, PAD_L1, std::get<1>(m_l1));
	g_key_status.Set(m_ext_port, PAD_R2, std::get<1>(m_r2));
	g_key_status.Set(m_ext_port, PAD_L2, std::get<1>(m_l2));
}

void addButtonInfoToString(std::string label, std::string& str, std::tuple<bool, u8> buttonInfo)
{
	const auto& [pressed, pressure] = buttonInfo;
	if (pressed)
	{
		str += fmt::format(" {}:{}", label, pressure);
	}
}

void addButtonInfoToString(std::string label, std::string& str, bool pressed)
{
	if (pressed)
	{
		str += fmt::format(" {}", label);
	}
}

void PadData::LogPadData() const
{
	std::string pressedButtons = "";
	addButtonInfoToString("Square", pressedButtons, m_square);
	addButtonInfoToString("Cross", pressedButtons, m_cross);
	addButtonInfoToString("Circle", pressedButtons, m_circle);
	addButtonInfoToString("Triangle", pressedButtons, m_triangle);

	addButtonInfoToString("D-Right", pressedButtons, m_right);
	addButtonInfoToString("D-Left", pressedButtons, m_left);
	addButtonInfoToString("D-Up", pressedButtons, m_up);
	addButtonInfoToString("D-Down", pressedButtons, m_down);

	addButtonInfoToString("R1", pressedButtons, m_r1);
	addButtonInfoToString("L1", pressedButtons, m_l1);
	addButtonInfoToString("R2", pressedButtons, m_r2);
	addButtonInfoToString("L2", pressedButtons, m_l2);

	addButtonInfoToString("Start", pressedButtons, m_start);
	addButtonInfoToString("Select", pressedButtons, m_select);
	addButtonInfoToString("R3", pressedButtons, m_r3);
	addButtonInfoToString("L3", pressedButtons, m_l3);

	const auto& [left_x, left_y] = m_leftAnalog;
	const auto& [right_x, right_y] = m_rightAnalog;
	const std::string analogs = fmt::format("Left: [{}, {}] | Right: [{}, {}]", left_x, left_y, right_x, right_y);

	const std::string finalLog = fmt::format("[PAD {}:{}:{}]\n\t[Buttons]: {}\n\t[Analogs]: {}\n", m_ext_port, m_port, m_slot, pressedButtons, analogs);
	controlLog(finalLog);
}

#endif