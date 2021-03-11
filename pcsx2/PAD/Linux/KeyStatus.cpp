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

#include "KeyStatus.h"
#include "Config.h"

void KeyStatus::Init()
{
	for (u32 pad = 0; pad < GAMEPAD_NUMBER; pad++)
	{
		m_button[pad] = 0xFFFF;
		m_internal_button_kbd[pad] = 0xFFFF;
		m_internal_button_joy[pad] = 0xFFFF;
		m_state_acces[pad] = false;

		for (u32 index = 0; index < MAX_KEYS; index++)
		{
			m_button_pressure[pad][index] = 0xFF;
			m_internal_button_pressure[pad][index] = 0xFF;
		}

		m_analog[pad].lx = m_analog_released_val;
		m_analog[pad].ly = m_analog_released_val;
		m_analog[pad].rx = m_analog_released_val;
		m_analog[pad].ry = m_analog_released_val;
		m_internal_analog_kbd[pad].lx = m_analog_released_val;
		m_internal_analog_kbd[pad].ly = m_analog_released_val;
		m_internal_analog_kbd[pad].rx = m_analog_released_val;
		m_internal_analog_kbd[pad].ry = m_analog_released_val;
		m_internal_analog_joy[pad].lx = m_analog_released_val;
		m_internal_analog_joy[pad].ly = m_analog_released_val;
		m_internal_analog_joy[pad].rx = m_analog_released_val;
		m_internal_analog_joy[pad].ry = m_analog_released_val;
	}
}

void KeyStatus::press(u32 pad, u32 index, s32 value)
{
	if (!IsAnalogKey(index))
	{
		m_internal_button_pressure[pad][index] = value;
		if (m_state_acces[pad])
			clear_bit(m_internal_button_kbd[pad], index);
		else
			clear_bit(m_internal_button_joy[pad], index);
	}
	else
	{
		// clamp value
		if (value > MAX_ANALOG_VALUE)
			value = MAX_ANALOG_VALUE;
		else if (value < -MAX_ANALOG_VALUE)
			value = -MAX_ANALOG_VALUE;

		//                          Left -> -- -> Right
		// Value range :        FFFF8002 -> 0  -> 7FFE
		// Force range :			  80 -> 0  -> 7F
		// Normal mode : expect value 0  -> 80 -> FF
		// Reverse mode: expect value FF -> 7F -> 0
		u8 force = (value / 256);
		if (analog_is_reversed(pad, index))
			analog_set(pad, index, m_analog_released_val - force);
		else
			analog_set(pad, index, m_analog_released_val + force);
	}
}

void KeyStatus::release(u32 pad, u32 index)
{
	if (!IsAnalogKey(index))
	{
		if (m_state_acces[pad])
			set_bit(m_internal_button_kbd[pad], index);
		else
			set_bit(m_internal_button_joy[pad], index);
	}
	else
	{
		analog_set(pad, index, m_analog_released_val);
	}
}

u16 KeyStatus::get(u32 pad)
{
	return m_button[pad];
}

void KeyStatus::analog_set(u32 pad, u32 index, u8 value)
{
	PADAnalog* m_internal_analog_ref;
	if (m_state_acces[pad])
		m_internal_analog_ref = &m_internal_analog_kbd[pad];
	else
		m_internal_analog_ref = &m_internal_analog_joy[pad];

	switch (index)
	{
		case PAD_R_LEFT:
		case PAD_R_RIGHT:
			m_internal_analog_ref->rx = value;
			break;

		case PAD_R_DOWN:
		case PAD_R_UP:
			m_internal_analog_ref->ry = value;
			break;

		case PAD_L_LEFT:
		case PAD_L_RIGHT:
			m_internal_analog_ref->lx = value;
			break;

		case PAD_L_DOWN:
		case PAD_L_UP:
			m_internal_analog_ref->ly = value;
			break;

		default:
			break;
	}
}

bool KeyStatus::analog_is_reversed(u32 pad, u32 index)
{
	switch (index)
	{
		case PAD_L_RIGHT:
		case PAD_L_LEFT:
			return (g_conf.pad_options[pad].reverse_lx);

		case PAD_R_LEFT:
		case PAD_R_RIGHT:
			return (g_conf.pad_options[pad].reverse_rx);

		case PAD_L_UP:
		case PAD_L_DOWN:
			return (g_conf.pad_options[pad].reverse_ly);

		case PAD_R_DOWN:
		case PAD_R_UP:
			return (g_conf.pad_options[pad].reverse_ry);

		default:
			return false;
	}
}

u8 KeyStatus::get(u32 pad, u32 index)
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

u8 KeyStatus::analog_merge(u8 kbd, u8 joy)
{
	if (kbd != m_analog_released_val)
		return kbd;
	else
		return joy;
}

void KeyStatus::commit_status(u32 pad)
{
	m_button[pad] = m_internal_button_kbd[pad] & m_internal_button_joy[pad];

	for (u32 index = 0; index < MAX_KEYS; index++)
		m_button_pressure[pad][index] = m_internal_button_pressure[pad][index];

	m_analog[pad].lx = analog_merge(m_internal_analog_kbd[pad].lx, m_internal_analog_joy[pad].lx);
	m_analog[pad].ly = analog_merge(m_internal_analog_kbd[pad].ly, m_internal_analog_joy[pad].ly);
	m_analog[pad].rx = analog_merge(m_internal_analog_kbd[pad].rx, m_internal_analog_joy[pad].rx);
	m_analog[pad].ry = analog_merge(m_internal_analog_kbd[pad].ry, m_internal_analog_joy[pad].ry);
}
