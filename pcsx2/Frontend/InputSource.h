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

#pragma once

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "common/Pcsx2Defs.h"
#include "Frontend/InputManager.h"

class SettingsInterface;

class InputSource
{
public:
	InputSource();
	virtual ~InputSource();

	virtual bool Initialize(SettingsInterface& si) = 0;
	virtual void UpdateSettings(SettingsInterface& si) = 0;
	virtual void Shutdown() = 0;

	virtual void PollEvents() = 0;

	virtual std::optional<InputBindingKey> ParseKeyString(
		const std::string_view& device, const std::string_view& binding) = 0;
	virtual std::string ConvertKeyToString(InputBindingKey key) = 0;

	/// Enumerates available devices. Returns a pair of the prefix (e.g. SDL-0) and the device name.
	virtual std::vector<std::pair<std::string, std::string>> EnumerateDevices() = 0;

	/// Enumerates available vibration motors at the time of call.
	virtual std::vector<InputBindingKey> EnumerateMotors() = 0;

	/// Retrieves bindings that match the generic bindings for the specified device.
	/// Returns false if it's not one of our devices.
	virtual bool GetGenericBindingMapping(const std::string_view& device, GenericInputBindingMapping* mapping) = 0;

	/// Informs the source of a new vibration motor state. Changes may not take effect until the next PollEvents() call.
	virtual void UpdateMotorState(InputBindingKey key, float intensity) = 0;

	/// Concurrently update both motors where possible, to avoid redundant packets.
	virtual void UpdateMotorState(InputBindingKey large_key, InputBindingKey small_key, float large_intensity, float small_intensity);

	/// Creates a key for a generic controller axis event.
	static InputBindingKey MakeGenericControllerAxisKey(InputSourceType clazz, u32 controller_index, s32 axis_index);

	/// Creates a key for a generic controller button event.
	static InputBindingKey MakeGenericControllerButtonKey(InputSourceType clazz, u32 controller_index, s32 button_index);

	/// Creates a key for a generic controller motor event.
	static InputBindingKey MakeGenericControllerMotorKey(InputSourceType clazz, u32 controller_index, s32 motor_index);

	/// Parses a generic controller key string.
	static std::optional<InputBindingKey> ParseGenericControllerKey(
		InputSourceType clazz, const std::string_view& source, const std::string_view& sub_binding);

	/// Converts a generic controller key to a string.
	static std::string ConvertGenericControllerKeyToString(InputBindingKey key);
};
