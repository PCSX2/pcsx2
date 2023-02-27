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
#include "PAD/Host/Global.h"

#include <array>
#include <cmath>

using namespace PAD;

KeyStatus::KeyStatus()
{
	std::memset(&m_analog, 0, sizeof(m_analog));

	for (u32 pad = 0; pad < NUM_CONTROLLER_PORTS; pad++)
	{
		m_axis_scale[pad][0] = 0.0f;
		m_axis_scale[pad][1] = 1.0f;
		m_pressure_modifier[pad] = 0.5f;
	}

	Init();
}

void KeyStatus::Init()
{
	for (u32 pad = 0; pad < NUM_CONTROLLER_PORTS; pad++)
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
	// Since we reordered the buttons for better UI, we need to remap them here.
	static constexpr std::array<u8, MAX_KEYS> bitmask_mapping = {{
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
		16, // PAD_ANALOG
		17, // PAD_PRESSURE
		// remainder are analogs and not used here
	}};

	if (IsAnalogKey(index))
	{
		m_button_pressure[pad][index] = static_cast<u8>(std::clamp(value * m_axis_scale[pad][1] * 255.0f, 0.0f, 255.0f));

		//                          Left -> -- -> Right
		// Value range :        FFFF8002 -> 0  -> 7FFE
		// Force range :			  80 -> 0  -> 7F
		// Normal mode : expect value 0  -> 80 -> FF
		// Reverse mode: expect value FF -> 7F -> 0

		// merge left/right or up/down into rx or ry

#define MERGE(pad, pos, neg) ((m_button_pressure[pad][pos] != 0) ? (127u + ((m_button_pressure[pad][pos] + 1u) / 2u)) : (127u - (m_button_pressure[pad][neg] / 2u)))
		if (index <= PAD_L_LEFT)
		{
			// Left Stick
			m_analog[pad].lx = m_analog[pad].invert_lx ? MERGE(pad, PAD_L_LEFT, PAD_L_RIGHT) : MERGE(pad, PAD_L_RIGHT, PAD_L_LEFT);
			m_analog[pad].ly = m_analog[pad].invert_ly ? MERGE(pad, PAD_L_UP, PAD_L_DOWN) : MERGE(pad, PAD_L_DOWN, PAD_L_UP);
		}
		else
		{
			// Right Stick
			m_analog[pad].rx = m_analog[pad].invert_rx ? MERGE(pad, PAD_R_LEFT, PAD_R_RIGHT) : MERGE(pad, PAD_R_RIGHT, PAD_R_LEFT);
			m_analog[pad].ry = m_analog[pad].invert_ry ? MERGE(pad, PAD_R_UP, PAD_R_DOWN) : MERGE(pad, PAD_R_DOWN, PAD_R_UP);
		}
#undef MERGE

		// Deadzone computation.
		const float dz = m_axis_scale[pad][0];
		if (dz > 0.0f)
		{
#define MERGE_F(pad, pos, neg) ((m_button_pressure[pad][pos] != 0) ? (static_cast<float>(m_button_pressure[pad][pos]) / 255.0f) : (static_cast<float>(m_button_pressure[pad][neg]) / -255.0f))
			float pos_x, pos_y;
			if (index <= PAD_L_LEFT)
			{
				pos_x = m_analog[pad].invert_lx ? MERGE_F(pad, PAD_L_LEFT, PAD_L_RIGHT) : MERGE_F(pad, PAD_L_RIGHT, PAD_L_LEFT);
				pos_y = m_analog[pad].invert_ly ? MERGE_F(pad, PAD_L_UP, PAD_L_DOWN) : MERGE_F(pad, PAD_L_DOWN, PAD_L_UP);
			}
			else
			{
				pos_x = m_analog[pad].invert_rx ? MERGE_F(pad, PAD_R_LEFT, PAD_R_RIGHT) : MERGE_F(pad, PAD_R_RIGHT, PAD_R_LEFT);
				pos_y = m_analog[pad].invert_ry ? MERGE_F(pad, PAD_R_UP, PAD_R_DOWN) : MERGE_F(pad, PAD_R_DOWN, PAD_R_UP);
			}

			// No point checking if we're at dead center (usually keyboard with no buttons pressed).
			if (pos_x != 0.0f || pos_y != 0.0f)
			{
				// Compute the angle at the given position in the stick's square bounding box.
				const float theta = std::atan2(pos_y, pos_x);

				// Compute the position that the edge of the circle would be at, given the angle.
				const float dz_x = std::cos(theta) * dz;
				const float dz_y = std::sin(theta) * dz;

				// We're in the deadzone if our position is less than the circle edge.
				const bool in_x = (pos_x < 0.0f) ? (pos_x > dz_x) : (pos_x <= dz_x);
				const bool in_y = (pos_y < 0.0f) ? (pos_y > dz_y) : (pos_y <= dz_y);
				if (in_x && in_y)
				{
					// In deadzone. Set to 127 (center).
					if (index <= PAD_L_LEFT)
						m_analog[pad].lx = m_analog[pad].ly = 127;
					else
						m_analog[pad].rx = m_analog[pad].ry = 127;
				}
			}
#undef MERGE_F
		}
	}
	else if (IsTriggerKey(index))
	{
		const float s_value = std::clamp(value * m_trigger_scale[pad][1], 0.0f, 1.0f);
		const float dz_value = (m_trigger_scale[pad][0] > 0.0f && s_value < m_trigger_scale[pad][0]) ? 0.0f : s_value;
		m_button_pressure[pad][index] = static_cast<u8>(dz_value * 255.0f);
		if (dz_value > 0.0f)
			m_button[pad] &= ~(1u << bitmask_mapping[index]);
		else
			m_button[pad] |= (1u << bitmask_mapping[index]);
	}
	else
	{
		// Don't affect L2/R2, since they are analog on most pads.
		const float pmod = ((m_button[pad] & (1u << PAD_PRESSURE)) == 0) ? m_pressure_modifier[pad] : 1.0f;
		const float dz_value = (value < m_button_deadzone[pad]) ? 0.0f : value;
		m_button_pressure[pad][index] = static_cast<u8>(std::clamp(dz_value * pmod * 255.0f, 0.0f, 255.0f));

		if (dz_value > 0.0f)
			m_button[pad] &= ~(1u << bitmask_mapping[index]);
		else
			m_button[pad] |= (1u << bitmask_mapping[index]);

		// Adjust pressure of all other face buttons which are active when pressure modifier is pressed..
		if (index == PAD_PRESSURE)
		{
			const float adjust_pmod = ((m_button[pad] & (1u << PAD_PRESSURE)) == 0) ? m_pressure_modifier[pad] : (1.0f / m_pressure_modifier[pad]);
			for (u32 i = 0; i < MAX_KEYS; i++)
			{
				if (i == index || IsAnalogKey(i) || IsTriggerKey(i))
					continue;

				// We add 0.5 here so that the round trip between 255->127->255 when applying works as expected.
				const float add = (m_button_pressure[pad][i] != 0) ? 0.5f : 0.0f;
				m_button_pressure[pad][i] = static_cast<u8>(std::clamp((static_cast<float>(m_button_pressure[pad][i]) + add) * adjust_pmod, 0.0f, 255.0f));
			}
		}
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
