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

#pragma once

#include "PAD/Host/Global.h"

namespace PAD
{
enum class ControllerType : u8;
}

class KeyStatus
{
private:
	static constexpr u8 m_analog_released_val = 0x7F;

	struct PADAnalog
	{
		u8 lx, ly;
		u8 rx, ry;
	};

	PAD::ControllerType m_type[GAMEPAD_NUMBER] = {};
	u32 m_button[GAMEPAD_NUMBER];
	u8 m_button_pressure[GAMEPAD_NUMBER][MAX_KEYS];
	PADAnalog m_analog[GAMEPAD_NUMBER];
	float m_axis_scale[GAMEPAD_NUMBER];
	float m_vibration_scale[GAMEPAD_NUMBER][2];

public:
	KeyStatus();
	void Init();

	void Set(u32 pad, u32 index, float value);

	__fi PAD::ControllerType GetType(u32 pad) { return m_type[pad]; }
	__fi void SetType(u32 pad, PAD::ControllerType type) { m_type[pad] = type; }

	__fi void SetAxisScale(u32 pad, float scale) { m_axis_scale[pad] = scale; }
	__fi float GetVibrationScale(u32 pad, u32 motor) const { return m_vibration_scale[pad][motor]; }
	__fi void SetVibrationScale(u32 pad, u32 motor, float scale) { m_vibration_scale[pad][motor] = scale; }

	u32 GetButtons(u32 pad);
	u8 GetPressure(u32 pad, u32 index);
};

extern KeyStatus g_key_status;
