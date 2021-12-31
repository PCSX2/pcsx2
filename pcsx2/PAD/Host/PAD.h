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

#include <string>
#include <vector>

#include "PAD/Host/Global.h"
#include "SaveState.h"

class SettingsInterface;
struct WindowInfo;

s32 PADinit();
void PADshutdown();
s32 PADopen(const WindowInfo& wi);
void PADclose();
s32 PADsetSlot(u8 port, u8 slot);
s32 PADfreeze(FreezeAction mode, freezeData* data);
u8 PADstartPoll(int pad);
u8 PADpoll(u8 value);

namespace PAD
{
	enum class VibrationCapabilities
	{
		NoVibration,
		LargeSmallMotors,
		SingleMotor,
		Count
	};

	/// Reloads configuration.
	void LoadConfig(const SettingsInterface& si);

	/// Updates vibration and other internal state. Called at the *end* of a frame.
	void Update();

	/// Returns a list of controller type names.
	std::vector<std::string> GetControllerTypeNames();

	/// Returns the list of binds for the specified controller type.
	std::vector<std::string> GetControllerBinds(const std::string_view& type);

	/// Returns the vibration configuration for the specified controller type.
	VibrationCapabilities GetControllerVibrationCapabilities(const std::string_view& type);

	/// Sets the specified bind on a controller to the specified pressure (normalized to 0..1).
	void SetControllerState(u32 controller, u32 bind, float value);
} // namespace PAD
