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