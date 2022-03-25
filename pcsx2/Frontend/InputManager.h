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

#include <functional>
#include <optional>
#include <string_view>
#include <variant>
#include <utility>

#include "common/Pcsx2Types.h"
#include "common/SettingsInterface.h"

/// Class, or source of an input event.
enum class InputSourceType : u32
{
	Keyboard,
	Mouse,
#ifdef _WIN32
	//DInput,
	XInput,
#endif
#ifdef SDL_BUILD
	SDL,
#endif
	Count,
};

/// Subtype of a key for an input source.
enum class InputSubclass : u32
{
	None = 0,

	MouseButton = 0,
	MousePointer = 1,
	MouseWheel = 2,

	ControllerButton = 0,
	ControllerAxis = 1,
	ControllerMotor = 2,
	ControllerHaptic = 3,
};

/// A composite type representing a full input key which is part of an event.
union InputBindingKey
{
	struct
	{
		InputSourceType source_type : 4;
		u32 source_index : 8; ///< controller number
		InputSubclass source_subtype : 2; ///< if 1, binding is for an axis and not a button (used for controllers)
		u32 negative : 1; ///< if 1, binding is for the negative side of the axis
		u32 unused : 17;
		u32 data;
	};

	u64 bits;

	bool operator==(const InputBindingKey& k) const { return bits == k.bits; }
	bool operator!=(const InputBindingKey& k) const { return bits != k.bits; }

	/// Removes the direction bit from the key, which is used to look up the bindings for it.
	/// This is because negative bindings should still fire when they reach zero again.
	InputBindingKey MaskDirection() const
	{
		InputBindingKey r;
		r.bits = bits;
		r.negative = false;
		return r;
	}
};
static_assert(sizeof(InputBindingKey) == sizeof(u64), "Input binding key is 64 bits");

/// Hashability for InputBindingKey
struct InputBindingKeyHash
{
	std::size_t operator()(const InputBindingKey& k) const { return std::hash<u64>{}(k.bits); }
};

/// Callback type for a binary event. Usually used for hotkeys.
using InputButtonEventHandler = std::function<void(bool value)>;

/// Callback types for a normalized event. Usually used for pads.
using InputAxisEventHandler = std::function<void(float value)>;

/// Input monitoring for external access.
struct InputInterceptHook
{
	enum class CallbackResult
	{
		StopProcessingEvent,
		ContinueProcessingEvent
	};

	using Callback = std::function<CallbackResult(InputBindingKey key, float value)>;
};

/// Hotkeys are actions (e.g. toggle frame limit) which can be bound to keys or chords.
struct HotkeyInfo
{
	const char* name;
	const char* category;
	const char* display_name;
	void(*handler)(bool pressed);
};
#define DECLARE_HOTKEY_LIST(name) extern const HotkeyInfo name[]
#define BEGIN_HOTKEY_LIST(name) const HotkeyInfo name[] = {
#define DEFINE_HOTKEY(name, category, display_name, handler) {(name), (category), (display_name), (handler)},
#define END_HOTKEY_LIST() {nullptr, nullptr, nullptr, nullptr} };

DECLARE_HOTKEY_LIST(g_vm_manager_hotkeys);
DECLARE_HOTKEY_LIST(g_gs_hotkeys);
DECLARE_HOTKEY_LIST(g_host_hotkeys);

/// Generic input bindings. These roughly match a DualShock 4 or XBox One controller.
/// They are used for automatic binding to PS2 controller types.
enum class GenericInputBinding : u8
{
	Unknown,

	DPadUp,
	DPadRight,
	DPadLeft,
	DPadDown,

	LeftStickUp,
	LeftStickRight,
	LeftStickDown,
	LeftStickLeft,
	L3,

	RightStickUp,
	RightStickRight,
	RightStickDown,
	RightStickLeft,
	R3,

	Triangle, // Y on XBox pads.
	Circle, // B on XBox pads.
	Cross, // A on XBox pads.
	Square, // X on XBox pads.

	Select, // Share on DS4, View on XBox pads.
	Start, // Options on DS4, Menu on XBox pads.
	System, // PS button on DS4, Guide button on XBox pads.

	L1, // LB on Xbox pads.
	L2, // Left trigger on XBox pads.
	R1, // RB on XBox pads.
	R2, // Right trigger on Xbox pads.

	SmallMotor, // High frequency vibration.
	LargeMotor, // Low frequency vibration.

	Count,
};
using GenericInputBindingMapping = std::vector<std::pair<GenericInputBinding, std::string>>;

/// External input source class.
class InputSource;

namespace InputManager
{
	/// Number of emulated pads. TODO: Multitap support.
	static constexpr u32 MAX_PAD_NUMBER = 2;

	/// Minimum interval between vibration updates when the effect is continuous.
	static constexpr double VIBRATION_UPDATE_INTERVAL_SECONDS = 0.5;		// 500ms

	/// Returns a pointer to the external input source class, if present.
	InputSource* GetInputSourceInterface(InputSourceType type);

	/// Converts an input class to a string.
	const char* InputSourceToString(InputSourceType clazz);

	/// Parses an input class string.
	std::optional<InputSourceType> ParseInputSourceString(const std::string_view& str);

	/// Converts a key code from a human-readable string to an identifier.
	std::optional<u32> ConvertHostKeyboardStringToCode(const std::string_view& str);

	/// Converts a key code from an identifier to a human-readable string.
	std::optional<std::string> ConvertHostKeyboardCodeToString(u32 code);

	/// Creates a key for a host-specific key code.
	InputBindingKey MakeHostKeyboardKey(s32 key_code);

	/// Creates a key for a host-specific button.
	InputBindingKey MakeHostMouseButtonKey(s32 button_index);

	/// Creates a key for a host-specific mouse wheel axis (0 = vertical, 1 = horizontal).
	InputBindingKey MakeHostMouseWheelKey(s32 axis_index);

	/// Parses an input binding key string.
	std::optional<InputBindingKey> ParseInputBindingKey(const std::string_view& binding);

	/// Converts a input key to a string.
	std::string ConvertInputBindingKeyToString(InputBindingKey key);

	/// Converts a chord of binding keys to a string.
	std::string ConvertInputBindingKeysToString(const InputBindingKey* keys, size_t num_keys);

	/// Returns a list of all hotkeys.
	std::vector<const HotkeyInfo*> GetHotkeyList();

	/// Enumerates available devices. Returns a pair of the prefix (e.g. SDL-0) and the device name.
	std::vector<std::pair<std::string, std::string>> EnumerateDevices();

	/// Enumerates available vibration motors at the time of call.
	std::vector<InputBindingKey> EnumerateMotors();

	/// Retrieves bindings that match the generic bindings for the specified device.
	GenericInputBindingMapping GetGenericBindingMapping(const std::string_view& device);

	/// Re-parses the config and registers all hotkey and pad bindings.
	void ReloadBindings(SettingsInterface& si);

	/// Re-parses the sources part of the config and initializes any backends.
	void ReloadSources(SettingsInterface& si);

	/// Shuts down any enabled input sources.
	void CloseSources();

	/// Polls input sources for events (e.g. external controllers).
	void PollSources();

	/// Returns true if any bindings exist for the specified key.
	/// This is the only function which can be safely called on another thread.
	bool HasAnyBindingsForKey(InputBindingKey key);

	/// Updates internal state for any binds for this key, and fires callbacks as needed.
	/// Returns true if anything was bound to this key, otherwise false.
	bool InvokeEvents(InputBindingKey key, float value);

	/// Sets a hook which can be used to intercept events before they're processed by the normal bindings.
	/// This is typically used when binding new controls to detect what gets pressed.
	void SetHook(InputInterceptHook::Callback callback);

	/// Removes any currently-active interception hook.
	void RemoveHook();

	/// Returns true if there is an interception hook present.
	bool HasHook();

	/// Internal method used by pads to dispatch vibration updates to input sources.
	/// Intensity is normalized from 0 to 1.
	void SetPadVibrationIntensity(u32 pad_index, float large_or_single_motor_intensity, float small_motor_intensity);

	/// Zeros all vibration intensities. Call when pausing.
	/// The pad vibration state will internally remain, so that when emulation is unpaused, the effect resumes.
	void PauseVibration();
} // namespace InputManager

namespace Host
{
	/// Called when a new input device is connected.
	void OnInputDeviceConnected(const std::string_view& identifier, const std::string_view& device_name);

	/// Called when an input device is disconnected.
	void OnInputDeviceDisconnected(const std::string_view& identifier);
}