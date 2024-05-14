// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "common/Pcsx2Defs.h"
#include "common/SmallString.h"
#include "Input/InputManager.h"

class SettingsInterface;

class InputSource
{
public:
	InputSource();
	virtual ~InputSource();

	virtual bool Initialize(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock) = 0;
	virtual void UpdateSettings(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock) = 0;
	virtual bool ReloadDevices() = 0;
	virtual void Shutdown() = 0;

	virtual void PollEvents() = 0;

	virtual std::optional<InputBindingKey> ParseKeyString(const std::string_view device, const std::string_view binding) = 0;
	virtual TinyString ConvertKeyToString(InputBindingKey key) = 0;
	virtual TinyString ConvertKeyToIcon(InputBindingKey key) = 0;

	/// Enumerates available devices. Returns a pair of the prefix (e.g. SDL-0) and the device name.
	virtual std::vector<std::pair<std::string, std::string>> EnumerateDevices() = 0;

	/// Enumerates available vibration motors at the time of call.
	virtual std::vector<InputBindingKey> EnumerateMotors() = 0;

	/// Retrieves bindings that match the generic bindings for the specified device.
	/// Returns false if it's not one of our devices.
	virtual bool GetGenericBindingMapping(const std::string_view device, InputManager::GenericInputBindingMapping* mapping) = 0;

	/// Informs the source of a new vibration motor state. Changes may not take effect until the next PollEvents() call.
	virtual void UpdateMotorState(InputBindingKey key, float intensity) = 0;

	/// Concurrently update both motors where possible, to avoid redundant packets.
	virtual void UpdateMotorState(InputBindingKey large_key, InputBindingKey small_key, float large_intensity, float small_intensity);

	/// Creates a key for a generic controller axis event.
	static InputBindingKey MakeGenericControllerAxisKey(InputSourceType clazz, u32 controller_index, s32 axis_index);

	/// Creates a key for a generic controller button event.
	static InputBindingKey MakeGenericControllerButtonKey(InputSourceType clazz, u32 controller_index, s32 button_index);

	/// Creates a key for a generic controller hat event.
	static InputBindingKey MakeGenericControllerHatKey(
		InputSourceType clazz, u32 controller_index, s32 hat_index, u8 hat_direction, u32 num_directions);

	/// Creates a key for a generic controller motor event.
	static InputBindingKey MakeGenericControllerMotorKey(InputSourceType clazz, u32 controller_index, s32 motor_index);

	/// Parses a generic controller key string.
	static std::optional<InputBindingKey> ParseGenericControllerKey(
		InputSourceType clazz, const std::string_view source, const std::string_view sub_binding);

	/// Converts a generic controller key to a string.
	static std::string ConvertGenericControllerKeyToString(InputBindingKey key);

	/// Returns true if inversion/negation should be ignored when binding axes.
	static bool ShouldIgnoreInversion();
};
