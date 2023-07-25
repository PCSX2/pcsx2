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
#include "SIO/Pad/Pad.h"
#include "SIO/Pad/PadDualshock2.h"
#include "SIO/Sio.h"

#include <fmt/core.h>

PadData::PadData(const int port, const int slot)
{
	m_port = port;
	m_slot = slot;
	m_ext_port = sioConvertPortAndSlotToPad(m_port, m_slot);
	PadBase* pad = Pad::GetPad(m_ext_port);
	// Get the state of the buttons
	// TODO - for the new recording file format, allow informing max number of buttons per frame per controller as well (ie. the analog button)
	const u32 buttons = pad->GetButtons();
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
	m_rightAnalog = pad->GetRawRightAnalog();
	m_leftAnalog = pad->GetRawLeftAnalog();
	// Get pressure bytes (12 of them)
	m_left = {(0b10000000 & m_compactPressFlagsGroupOne) == 0, pad->GetRawInput(PadDualshock2::Inputs::PAD_LEFT)};
	m_down = {(0b01000000 & m_compactPressFlagsGroupOne) == 0, pad->GetRawInput(PadDualshock2::Inputs::PAD_DOWN)};
	m_right = {(0b00100000 & m_compactPressFlagsGroupOne) == 0, pad->GetRawInput(PadDualshock2::Inputs::PAD_RIGHT)};
	m_up = {(0b00010000 & m_compactPressFlagsGroupOne) == 0, pad->GetRawInput(PadDualshock2::Inputs::PAD_UP)};
	m_start = (0b00001000 & m_compactPressFlagsGroupOne) == 0;
	m_r3 = (0b00000100 & m_compactPressFlagsGroupOne) == 0;
	m_l3 = (0b00000010 & m_compactPressFlagsGroupOne) == 0;
	m_select = (0b00000001 & m_compactPressFlagsGroupOne) == 0;

	m_square = {(0b10000000 & m_compactPressFlagsGroupTwo) == 0, pad->GetRawInput(PadDualshock2::Inputs::PAD_SQUARE)};
	m_cross = {(0b01000000 & m_compactPressFlagsGroupTwo) == 0, pad->GetRawInput(PadDualshock2::Inputs::PAD_CROSS)};
	m_circle = {(0b00100000 & m_compactPressFlagsGroupTwo) == 0, pad->GetRawInput(PadDualshock2::Inputs::PAD_CIRCLE)};
	m_triangle = {(0b00010000 & m_compactPressFlagsGroupTwo) == 0, pad->GetRawInput(PadDualshock2::Inputs::PAD_TRIANGLE)};
	m_r1 = {(0b00001000 & m_compactPressFlagsGroupTwo) == 0, pad->GetRawInput(PadDualshock2::Inputs::PAD_R1)};
	m_l1 = {(0b00000100 & m_compactPressFlagsGroupTwo) == 0, pad->GetRawInput(PadDualshock2::Inputs::PAD_L1)};
	m_r2 = {(0b00000010 & m_compactPressFlagsGroupTwo) == 0, pad->GetRawInput(PadDualshock2::Inputs::PAD_R2)};
	m_l2 = {(0b00000001 & m_compactPressFlagsGroupTwo) == 0, pad->GetRawInput(PadDualshock2::Inputs::PAD_L2)};
}

PadData::PadData(const int port, const int slot, const std::array<u8, 18> data)
{
	m_port = port;
	m_slot = slot;
	m_ext_port = sioConvertPortAndSlotToPad(m_port, m_slot);

	m_compactPressFlagsGroupOne = data[0];
	m_compactPressFlagsGroupTwo = data[1];

	m_rightAnalog = {data[2], data[3]};
	m_leftAnalog = {data[4], data[5]};

	m_left = {(0b10000000 & m_compactPressFlagsGroupOne) == 0, data[7]};
	m_down = {(0b01000000 & m_compactPressFlagsGroupOne) == 0, data[9]};
	m_right = {(0b00100000 & m_compactPressFlagsGroupOne) == 0, data[6]};
	m_up = {(0b00010000 & m_compactPressFlagsGroupOne) == 0, data[8]};
	m_start = (0b00001000 & m_compactPressFlagsGroupOne) == 0;
	m_r3 = (0b00000100 & m_compactPressFlagsGroupOne) == 0;
	m_l3 = (0b00000010 & m_compactPressFlagsGroupOne) == 0;
	m_select = (0b00000001 & m_compactPressFlagsGroupOne) == 0;

	m_square = {(0b10000000 & m_compactPressFlagsGroupTwo) == 0, data[13]};
	m_cross = {(0b01000000 & m_compactPressFlagsGroupTwo) == 0, data[12]};
	m_circle = {(0b00100000 & m_compactPressFlagsGroupTwo) == 0, data[11]};
	m_triangle = {(0b00010000 & m_compactPressFlagsGroupTwo) == 0, data[10]};
	m_r1 = {(0b00001000 & m_compactPressFlagsGroupTwo) == 0, data[15]};
	m_l1 = {(0b00000100 & m_compactPressFlagsGroupTwo) == 0, data[14]};
	m_r2 = {(0b00000010 & m_compactPressFlagsGroupTwo) == 0, data[17]};
	m_l2 = {(0b00000001 & m_compactPressFlagsGroupTwo) == 0, data[16]};
}

void PadData::OverrideActualController() const
{
	PadBase* pad = Pad::GetPad(m_ext_port);
	pad->SetRawAnalogs(m_leftAnalog, m_rightAnalog);

	pad->Set(PadDualshock2::Inputs::PAD_RIGHT, std::get<1>(m_right));
	pad->Set(PadDualshock2::Inputs::PAD_LEFT, std::get<1>(m_left));
	pad->Set(PadDualshock2::Inputs::PAD_UP, std::get<1>(m_up));
	pad->Set(PadDualshock2::Inputs::PAD_DOWN, std::get<1>(m_down));
	pad->Set(PadDualshock2::Inputs::PAD_START, m_start);
	pad->Set(PadDualshock2::Inputs::PAD_SELECT, m_select);
	pad->Set(PadDualshock2::Inputs::PAD_R3, m_r3);
	pad->Set(PadDualshock2::Inputs::PAD_L3, m_l3);

	pad->Set(PadDualshock2::Inputs::PAD_SQUARE, std::get<1>(m_square));
	pad->Set(PadDualshock2::Inputs::PAD_CROSS, std::get<1>(m_cross));
	pad->Set(PadDualshock2::Inputs::PAD_CIRCLE, std::get<1>(m_circle));
	pad->Set(PadDualshock2::Inputs::PAD_TRIANGLE, std::get<1>(m_triangle));

	pad->Set(PadDualshock2::Inputs::PAD_R1, std::get<1>(m_r1));
	pad->Set(PadDualshock2::Inputs::PAD_L1, std::get<1>(m_l1));
	pad->Set(PadDualshock2::Inputs::PAD_R2, std::get<1>(m_r2));
	pad->Set(PadDualshock2::Inputs::PAD_L2, std::get<1>(m_l2));
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

	const std::string finalLog = fmt::format("[PAD {}:{}]\n\t[Buttons]: {}\n\t[Analogs]: {}\n", m_port, m_slot, pressedButtons, analogs);
	controlLog(finalLog);
}
