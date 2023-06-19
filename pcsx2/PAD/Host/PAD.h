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
#include <tuple>
#include <utility>
#include <vector>

#include "Config.h"
#include "PAD/Host/Global.h"
#include "SaveState.h"

class SettingsInterface;
struct WindowInfo;
enum class GenericInputBinding : u8;

s32 PADinit();
void PADshutdown();
s32 PADopen();
void PADclose();
s32 PADsetSlot(u8 port, u8 slot);
s32 PADfreeze(FreezeAction mode, freezeData* data);
u8 PADstartPoll(int _port, int _slot);
u8 PADpoll(u8 value);
bool PADcomplete();

namespace PAD
{
	enum class ControllerType: u8
	{
		NotConnected,
		DualShock2,
		Count
	};

	enum class VibrationCapabilities : u8
	{
		NoVibration,
		LargeSmallMotors,
		SingleMotor,
		Count
	};

	struct ControllerInfo
	{
		ControllerType type;
		const char* name;
		const char* display_name;
		const InputBindingInfo* bindings;
		u32 num_bindings;
		const SettingInfo* settings;
		u32 num_settings;
		VibrationCapabilities vibration_caps;
	};

	/// Total number of pad ports, across both multitaps.
	static constexpr u32 NUM_CONTROLLER_PORTS = 8;

	/// Number of macro buttons per controller.
	static constexpr u32 NUM_MACRO_BUTTONS_PER_CONTROLLER = 16;

	/// Default stick deadzone/sensitivity.
	static constexpr float DEFAULT_STICK_DEADZONE = 0.0f;
	static constexpr float DEFAULT_STICK_SCALE = 1.33f;
	static constexpr float DEFAULT_TRIGGER_DEADZONE = 0.0f;
	static constexpr float DEFAULT_TRIGGER_SCALE = 1.0f;
	static constexpr float DEFAULT_MOTOR_SCALE = 1.0f;
	static constexpr float DEFAULT_PRESSURE_MODIFIER = 0.5f;
	static constexpr float DEFAULT_BUTTON_DEADZONE = 0.0f;

	/// Returns the default type for the specified port.
	const char* GetDefaultPadType(u32 pad);

	/// Reloads configuration.
	void LoadConfig(const SettingsInterface& si);

	/// Restores default configuration.
	void SetDefaultControllerConfig(SettingsInterface& si);
	void SetDefaultHotkeyConfig(SettingsInterface& si);

	/// Clears all bindings for a given port.
	void ClearPortBindings(SettingsInterface& si, u32 port);

	/// Copies pad configuration from one interface (ini) to another.
	void CopyConfiguration(SettingsInterface* dest_si, const SettingsInterface& src_si,
		bool copy_pad_config = true, bool copy_pad_bindings = true, bool copy_hotkey_bindings = true);

	/// Updates vibration and other internal state. Called at the *end* of a frame.
	void Update();

	/// Returns a list of controller type names. Pair of [name, display name].
	std::vector<std::pair<const char*, const char*>> GetControllerTypeNames();

	/// Returns the list of binds for the specified controller type.
	std::vector<std::string> GetControllerBinds(const std::string_view& type);

	/// Returns general information for the specified controller type.
	const ControllerInfo* GetControllerInfo(ControllerType type);
	const ControllerInfo* GetControllerInfo(const std::string_view& name);

	/// Performs automatic controller mapping with the provided list of generic mappings.
	bool MapController(SettingsInterface& si, u32 controller,
		const std::vector<std::pair<GenericInputBinding, std::string>>& mapping);

	/// Sets the specified bind on a controller to the specified pressure (normalized to 0..1).
	void SetControllerState(u32 controller, u32 bind, float value);

	/// Sets the state of the specified macro button.
	void SetMacroButtonState(u32 pad, u32 index, bool state);

	/// Returns a list of input profiles available.
	std::vector<std::string> GetInputProfileNames();
} // namespace PAD
