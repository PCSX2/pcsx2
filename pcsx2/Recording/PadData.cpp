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

void PadData::UpdateControllerData(u16 bufIndex, u8 const bufVal) noexcept
{
	if (bufIndex == static_cast<u8>(BufferIndex::PressedFlagsGroupOne))
	{
		m_leftPressed.setPressedState(bufVal);
		m_downPressed.setPressedState(bufVal);
		m_rightPressed.setPressedState(bufVal);
		m_upPressed.setPressedState(bufVal);
		m_start.setPressedState(bufVal);
		m_r3.setPressedState(bufVal);
		m_l3.setPressedState(bufVal);
		m_select.setPressedState(bufVal);
	}
	else if (bufIndex == static_cast<u8>(BufferIndex::PressedFlagsGroupTwo))
	{
		m_squarePressed.setPressedState(bufVal);
		m_crossPressed.setPressedState(bufVal);
		m_circlePressed.setPressedState(bufVal);
		m_trianglePressed.setPressedState(bufVal);
		m_r1Pressed.setPressedState(bufVal);
		m_l1Pressed.setPressedState(bufVal);
		m_r2Pressed.setPressedState(bufVal);
		m_l2Pressed.setPressedState(bufVal);
	}
	else
	{
		bufIndex -= 2;
		if (bufIndex < sizeof(m_allIntensities) / sizeof(u8*))
			*m_allIntensities[bufIndex] = bufVal;
	}
}

u8 PadData::PollControllerData(u16 bufIndex) const noexcept
{
	u8 byte = 0;
	if (bufIndex == static_cast<u8>(BufferIndex::PressedFlagsGroupOne))
	{
		// Construct byte by combining flags if the buttons are pressed
		byte |= m_leftPressed.getMaskIfPressed();
		byte |= m_downPressed.getMaskIfPressed();
		byte |= m_rightPressed.getMaskIfPressed();
		byte |= m_upPressed.getMaskIfPressed();
		byte |= m_start.getMaskIfPressed();
		byte |= m_r3.getMaskIfPressed();
		byte |= m_l3.getMaskIfPressed();
		byte |= m_select.getMaskIfPressed();
		// We flip the bits because as mentioned below, 0 = pressed
		byte = ~byte;
	}
	else if (bufIndex == static_cast<u8>(BufferIndex::PressedFlagsGroupTwo))
	{
		// Construct byte by combining flags if the buttons are pressed
		byte |= m_squarePressed.getMaskIfPressed();
		byte |= m_crossPressed.getMaskIfPressed();
		byte |= m_circlePressed.getMaskIfPressed();
		byte |= m_trianglePressed.getMaskIfPressed();
		byte |= m_r1Pressed.getMaskIfPressed();
		byte |= m_l1Pressed.getMaskIfPressed();
		byte |= m_r2Pressed.getMaskIfPressed();
		byte |= m_l2Pressed.getMaskIfPressed();
		// We flip the bits because as mentioned below, 0 = pressed
		byte = ~byte;
	}
	else
	{
		bufIndex -= 2;
		if (bufIndex < sizeof(m_allIntensities) / sizeof(u8*))
			byte = *m_allIntensities[bufIndex - 2];
	}

	return byte;
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
	const std::string pressedBytes = RawPadBytesToString(0, 2);
	const std::string rightAnalogBytes = RawPadBytesToString(2, 4);
	const std::string leftAnalogBytes = RawPadBytesToString(4, 6);
	const std::string pressureBytes = RawPadBytesToString(6, 17);
	const std::string fullLog =
		fmt::format("[PAD {}] Raw Bytes: Pressed = [{}]\n", port + 1, pressedBytes) +
		fmt::format("[PAD {}] Raw Bytes: Right Analog = [{}]\n", port + 1, rightAnalogBytes) +
		fmt::format("[PAD {}] Raw Bytes: Left Analog = [{}]\n", port + 1, leftAnalogBytes) +
		fmt::format("[PAD {}] Raw Bytes: Pressure = [{}]\n", port + 1, pressureBytes);
	controlLog(fullLog);
}

#endif