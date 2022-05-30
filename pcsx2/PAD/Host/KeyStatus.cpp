/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "PAD/Host/KeyStatus.h"

KeyStatus::KeyStatus()
{
	Init();

	for (u32 pad = 0; pad < GAMEPAD_NUMBER; pad++)
	{
		m_axis_scale[pad] = 1.0f;
	}
}

void KeyStatus::Init()
{
	for (u32 pad = 0; pad < GAMEPAD_NUMBER; pad++)
	{
		m_button[pad] = 0xFFFFFFFF;

		for (u32 index = 0; index < MAX_KEYS; index++)
			m_button_pressure[pad][index] = 0;

		m_analog[pad].lx = m_analog_released_val;
		m_analog[pad].ly = m_analog_released_val;
		m_analog[pad].rx = m_analog_released_val;
		m_analog[pad].ry = m_analog_released_val;
	}
}

void KeyStatus::Set(u32 pad, u32 index, float value)
{
	m_button_pressure[pad][index] = static_cast<u8>(std::clamp(value * m_axis_scale[pad] * 255.0f, 0.0f, 255.0f));

	if (IsAnalogKey(index))
	{
		//                          Left -> -- -> Right
		// Value range :        FFFF8002 -> 0  -> 7FFE
		// Force range :			  80 -> 0  -> 7F
		// Normal mode : expect value 0  -> 80 -> FF
		// Reverse mode: expect value FF -> 7F -> 0

		// merge left/right or up/down into rx or ry

#define MERGE(pad, pos, neg) ((m_button_pressure[pad][pos] != 0) ? (127u + ((m_button_pressure[pad][pos] + 1u) / 2u)) : (127u - (m_button_pressure[pad][neg] / 2u)))

		switch (index)
		{
			case PAD_R_LEFT:
			case PAD_R_RIGHT:
				m_analog[pad].rx = MERGE(pad, PAD_R_RIGHT, PAD_R_LEFT);
				break;

			case PAD_R_DOWN:
			case PAD_R_UP:
				m_analog[pad].ry = MERGE(pad, PAD_R_DOWN, PAD_R_UP);
				break;

			case PAD_L_LEFT:
			case PAD_L_RIGHT:
				m_analog[pad].lx = MERGE(pad, PAD_L_RIGHT, PAD_L_LEFT);
				break;

			case PAD_L_DOWN:
			case PAD_L_UP:
				m_analog[pad].ly = MERGE(pad, PAD_L_DOWN, PAD_L_UP);
				break;

			default:
				break;
		}

#undef MERGE
	}
	else
	{
		// Since we reordered the buttons for better UI, we need to remap them here.
		static constexpr std::array<u8, MAX_KEYS> bitmask_mapping = { {
			12, // PAD_UP
			13, // PAD_RIGHT
			14, // PAD_DOWN
			15, // PAD_LEFT
			4, // PAD_TRIANGLE
			5, // PAD_CIRCLE
			6, // PAD_CROSS
			7, // PAD_SQUARE
			8, // PAD_SELECT
			11, // PAD_START
			2, // PAD_L1
			0, // PAD_L2
			3, // PAD_R1
			1, // PAD_R2
			9, // PAD_L3
			10, // PAD_R3
			16, // Analog
			// remainder are analogs and not used here
		} };

		// TODO: Deadzone here?
		if (value > 0.0f)
			m_button[pad] &= ~(1u << bitmask_mapping[index]);
		else
			m_button[pad] |= (1u << bitmask_mapping[index]);
	}
}

u32 KeyStatus::GetButtons(u32 pad)
{
	return m_button[pad];
}

u8 KeyStatus::GetPressure(u32 pad, u32 index)
{
	switch (index)
	{
		case PAD_R_LEFT:
		case PAD_R_RIGHT:
			return m_analog[pad].rx;

		case PAD_R_DOWN:
		case PAD_R_UP:
			return m_analog[pad].ry;

		case PAD_L_LEFT:
		case PAD_L_RIGHT:
			return m_analog[pad].lx;

		case PAD_L_DOWN:
		case PAD_L_UP:
			return m_analog[pad].ly;

		default:
			return m_button_pressure[pad][index];
	}
}
