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
#include <mutex>
#include <optional>
#include <string_view>
#include <variant>
#include <utility>

#include "common/Pcsx2Types.h"
#include "common/SettingsInterface.h"
#include "common/WindowInfo.h"

#include "pcsx2/Config.h"

/// Class, or source of an input event.
enum class InputSourceType : u32
{
	Keyboard,
	Pointer,
	SDL,
#ifdef _WIN32
	DInput,
	XInput,
#endif
	Count,
};

/// Subtype of a key for an input source.
enum class InputSubclass : u32
{
	None = 0,

	PointerButton = 0,
	PointerAxis = 1,

	ControllerButton = 0,
	ControllerAxis = 1,
	ControllerHat = 2,
	ControllerMotor = 3,
	ControllerHaptic = 4,
};

enum class InputModifier : u32
{
	None = 0,
	Negate, ///< Input * -1, gets the negative side of the axis
	FullAxis, ///< (Input * 0.5) + 0.5, uses both the negative and positive side of the axis together
};

/// A composite type representing a full input key which is part of an event.
union InputBindingKey
{
	struct
	{
		InputSourceType source_type : 4;
		u32 source_index : 8; ///< controller number
		InputSubclass source_subtype : 3; ///< if 1, binding is for an axis and not a button (used for controllers)
		InputModifier modifier : 2;
		u32 invert : 1; ///< if 1, value is inverted prior to being sent to the sink
		u32 unused : 14;
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
		r.modifier = InputModifier::None;
		r.invert = 0;
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
using InputButtonEventHandler = std::function<void(s32 value)>;

/// Callback types for a normalized event. Usually used for pads.
using InputAxisEventHandler = std::function<void(float value)>;

/// Input monitoring for external access.
struct InputInterceptHook
{
	enum class CallbackResult
	{
		StopProcessingEvent,
		ContinueProcessingEvent,
		RemoveHookAndStopProcessingEvent,
		RemoveHookAndContinueProcessingEvent,
	};

	using Callback = std::function<CallbackResult(InputBindingKey key, float value)>;
};

/// Hotkeys are actions (e.g. toggle frame limit) which can be bound to keys or chords.
/// The handler is called with an integer representing the key state, where 0 means that
/// one or more keys were released, 1 means all the keys were pressed, and -1 means that
/// the hotkey was cancelled due to a chord with more keys being activated.
struct HotkeyInfo
{
	const char* name;
	const char* category;
	const char* display_name;
	void (*handler)(s32 pressed);
};
#define DECLARE_HOTKEY_LIST(name) extern const HotkeyInfo name[]
#define BEGIN_HOTKEY_LIST(name) const HotkeyInfo name[] = {
#define DEFINE_HOTKEY(name, category, display_name, handler) {(name), (category), (display_name), (handler)},
#define END_HOTKEY_LIST() \
	{ \
		nullptr, nullptr, nullptr, nullptr \
	} \
	} \
	;

DECLARE_HOTKEY_LIST(g_common_hotkeys);
DECLARE_HOTKEY_LIST(g_gs_hotkeys);
DECLARE_HOTKEY_LIST(g_host_hotkeys);

/// Host mouse relative axes are X, Y, wheel horizontal, wheel vertical.
enum class InputPointerAxis : u8
{
	X,
	Y,
	WheelX,
	WheelY,
	Count
};

/// External input source class.
class InputSource;

namespace InputManager
{
	/// Minimum interval between vibration updates when the effect is continuous.
	static constexpr double VIBRATION_UPDATE_INTERVAL_SECONDS = 0.5; // 500ms

	/// Maximum number of host mouse devices.
	static constexpr u32 MAX_POINTER_DEVICES = 1;
	static constexpr u32 MAX_POINTER_BUTTONS = 3;

	/// Maximum number of software cursors. We allocate an extra two for USB devices with
	/// positioning data from the controller instead of a mouse.
	static constexpr u32 MAX_SOFTWARE_CURSORS = MAX_POINTER_BUTTONS + 2;

	/// Returns a pointer to the external input source class, if present.
	InputSource* GetInputSourceInterface(InputSourceType type);

	/// Converts an input class to a string.
	const char* InputSourceToString(InputSourceType clazz);

	/// Returns the default state for an input source.
	bool GetInputSourceDefaultEnabled(InputSourceType type);

	/// Parses an input class string.
	std::optional<InputSourceType> ParseInputSourceString(const std::string_view& str);

	/// Parses a pointer device string, i.e. tells you which pointer is specified.
	std::optional<u32> GetIndexFromPointerBinding(const std::string_view& str);

	/// Returns the device name for a pointer index (e.g. Pointer-0).
	std::string GetPointerDeviceName(u32 pointer_index);

	/// Converts a key code from a human-readable string to an identifier.
	std::optional<u32> ConvertHostKeyboardStringToCode(const std::string_view& str);

	/// Converts a key code from an identifier to a human-readable string.
	std::optional<std::string> ConvertHostKeyboardCodeToString(u32 code);

	/// Creates a key for a host-specific key code.
	InputBindingKey MakeHostKeyboardKey(u32 key_code);

	/// Creates a key for a host-specific button.
	InputBindingKey MakePointerButtonKey(u32 index, u32 button_index);

	/// Creates a key for a host-specific mouse relative event
	/// (axis 0 = horizontal, 1 = vertical, 2 = wheel horizontal, 3 = wheel vertical).
	InputBindingKey MakePointerAxisKey(u32 index, InputPointerAxis axis);

	/// Parses an input binding key string.
	std::optional<InputBindingKey> ParseInputBindingKey(const std::string_view& binding);

	/// Converts a input key to a string.
	std::string ConvertInputBindingKeyToString(InputBindingInfo::Type binding_type, InputBindingKey key);

	/// Converts a chord of binding keys to a string.
	std::string ConvertInputBindingKeysToString(InputBindingInfo::Type binding_type, const InputBindingKey* keys, size_t num_keys);

	/// Returns a list of all hotkeys.
	std::vector<const HotkeyInfo*> GetHotkeyList();

	/// Enumerates available devices. Returns a pair of the prefix (e.g. SDL-0) and the device name.
	std::vector<std::pair<std::string, std::string>> EnumerateDevices();

	/// Enumerates available vibration motors at the time of call.
	std::vector<InputBindingKey> EnumerateMotors();

	/// Retrieves bindings that match the generic bindings for the specified device.
	using GenericInputBindingMapping = std::vector<std::pair<GenericInputBinding, std::string>>;
	GenericInputBindingMapping GetGenericBindingMapping(const std::string_view& device);

	/// Returns whether a given input source is enabled.
	bool IsInputSourceEnabled(SettingsInterface& si, InputSourceType type);

	/// Re-parses the config and registers all hotkey and pad bindings.
	void ReloadBindings(SettingsInterface& si, SettingsInterface& binding_si);

	/// Re-parses the sources part of the config and initializes any backends.
	void ReloadSources(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock);

	/// Called when a device change is triggered by the system (DBT_DEVNODES_CHANGED on Windows).
	/// Returns true if any device changes are detected.
	bool ReloadDevices();

	/// Shuts down any enabled input sources.
	void CloseSources();

	/// Polls input sources for events (e.g. external controllers).
	void PollSources();

	/// Returns true if any bindings exist for the specified key.
	/// Can be safely called on another thread.
	bool HasAnyBindingsForKey(InputBindingKey key);

	/// Returns true if any bindings exist for the specified source + index.
	/// Can be safely called on another thread.
	bool HasAnyBindingsForSource(InputBindingKey key);

	/// Updates internal state for any binds for this key, and fires callbacks as needed.
	/// Returns true if anything was bound to this key, otherwise false.
	bool InvokeEvents(InputBindingKey key, float value, GenericInputBinding generic_key = GenericInputBinding::Unknown);

	/// Clears internal state for any binds with a matching source/index.
	void ClearBindStateFromSource(InputBindingKey key);

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

	/// Reads absolute pointer position.
	std::pair<float, float> GetPointerAbsolutePosition(u32 index);

	/// Updates absolute pointer position. Can call from UI thread, use when the host only reports absolute coordinates.
	void UpdatePointerAbsolutePosition(u32 index, float x, float y);

	/// Updates relative pointer position. Can call from the UI thread, use when host supports relative coordinate reporting.
	void UpdatePointerRelativeDelta(u32 index, InputPointerAxis axis, float d, bool raw_input = false);

	/// Updates host mouse mode (relative/cursor hiding).
	void UpdateHostMouseMode();

	/// Called when a new input device is connected.
	void OnInputDeviceConnected(const std::string_view& identifier, const std::string_view& device_name);

	/// Called when an input device is disconnected.
	void OnInputDeviceDisconnected(const std::string_view& identifier);
} // namespace InputManager

namespace Host
{
	/// Return the current window handle. Needed for DInput.
	std::optional<WindowInfo> GetTopLevelWindowInfo();

	/// Called when a new input device is connected.
	void OnInputDeviceConnected(const std::string_view& identifier, const std::string_view& device_name);

	/// Called when an input device is disconnected.
	void OnInputDeviceDisconnected(const std::string_view& identifier);

	/// Enables relative mouse mode in the host, and/or hides the cursor.
	void SetMouseMode(bool relative_mode, bool hide_cursor);
} // namespace Host
