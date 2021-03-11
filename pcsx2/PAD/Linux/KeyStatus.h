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

#pragma once

#include "Global.h"

typedef struct
{
	u8 lx, ly;
	u8 rx, ry;
} PADAnalog;

#define MAX_ANALOG_VALUE 32766

class KeyStatus
{
private:
	const u8 m_analog_released_val;

	u16 m_button[GAMEPAD_NUMBER];
	u16 m_internal_button_kbd[GAMEPAD_NUMBER];
	u16 m_internal_button_joy[GAMEPAD_NUMBER];

	u8 m_button_pressure[GAMEPAD_NUMBER][MAX_KEYS];
	u8 m_internal_button_pressure[GAMEPAD_NUMBER][MAX_KEYS];

	bool m_state_acces[GAMEPAD_NUMBER];

	PADAnalog m_analog[GAMEPAD_NUMBER];
	PADAnalog m_internal_analog_kbd[GAMEPAD_NUMBER];
	PADAnalog m_internal_analog_joy[GAMEPAD_NUMBER];

	void analog_set(u32 pad, u32 index, u8 value);
	bool analog_is_reversed(u32 pad, u32 index);
	u8 analog_merge(u8 kbd, u8 joy);

public:
	KeyStatus()
		: m_analog_released_val(0x7F)
	{
		Init();
	}
	void Init();

	void keyboard_state_acces(u32 pad) { m_state_acces[pad] = true; }
	void joystick_state_acces(u32 pad) { m_state_acces[pad] = false; }

	void press(u32 pad, u32 index, s32 value = 0xFF);
	void release(u32 pad, u32 index);

	u16 get(u32 pad);
	u8 get(u32 pad, u32 index);


	void commit_status(u32 pad);
};

extern KeyStatus g_key_status;
