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

#include "PrecompiledHeader.h"
#include "Frontend/InputManager.h"
#include "Frontend/InputSource.h"
#include "PAD/Host/PAD.h"
#include "common/StringUtil.h"
#include "common/Timer.h"
#include "VMManager.h"
#include <array>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_map>
#include <variant>
#include <vector>

// ------------------------------------------------------------------------
// Constants
// ------------------------------------------------------------------------

enum : u32
{
	MAX_KEYS_PER_BINDING = 4,
	MAX_MOTORS_PER_PAD = 2,
	FIRST_EXTERNAL_INPUT_SOURCE = static_cast<u32>(InputSourceType::Mouse) + 1u,
	LAST_EXTERNAL_INPUT_SOURCE = static_cast<u32>(InputSourceType::Count),
};

// ------------------------------------------------------------------------
// Event Handler Type
// ------------------------------------------------------------------------
// This class acts as an adapter to convert from normalized values to
// binary values when the callback is a binary/button handler. That way
// you don't need to convert float->bool in your callbacks.
using InputEventHandler = std::variant<InputAxisEventHandler, InputButtonEventHandler>;

// ------------------------------------------------------------------------
// Binding Type
// ------------------------------------------------------------------------
// This class tracks both the keys which make it up (for chords), as well
// as the state of all buttons. For button callbacks, it's fired when
// all keys go active, and for axis callbacks, when all are active and
// the value changes.

struct InputBinding
{
	InputBindingKey keys[MAX_KEYS_PER_BINDING] = {};
	InputEventHandler handler;
	u8 num_keys = 0;
	u8 full_mask = 0;
	u8 current_mask = 0;
};

struct PadVibrationBinding
{
	struct Motor
	{
		InputBindingKey binding;
		u64 last_update_time;
		InputSource* source;
		float last_intensity;
	};

	u32 pad_index = 0;
	Motor motors[MAX_MOTORS_PER_PAD] = {};

	/// Returns true if the two motors are bound to the same host motor.
	__fi bool AreMotorsCombined() const { return motors[0].binding == motors[1].binding; }

	/// Returns the intensity when both motors are combined.
	__fi float GetCombinedIntensity() const { return std::max(motors[0].last_intensity, motors[1].last_intensity); }
};

// ------------------------------------------------------------------------
// Forward Declarations (for static qualifier)
// ------------------------------------------------------------------------
namespace InputManager
{
	static std::optional<InputBindingKey> ParseHostKeyboardKey(
		const std::string_view& source, const std::string_view& sub_binding);
	static std::optional<InputBindingKey> ParseHostMouseKey(
		const std::string_view& source, const std::string_view& sub_binding);

	static std::vector<std::string_view> SplitChord(const std::string_view& binding);
	static bool SplitBinding(const std::string_view& binding, std::string_view* source, std::string_view* sub_binding);
	static void AddBindings(const std::vector<std::string>& bindings, const InputEventHandler& handler);
	static bool ParseBindingAndGetSource(const std::string_view& binding, InputBindingKey* key, InputSource** source);

	static bool IsAxisHandler(const InputEventHandler& handler);

	static void AddHotkeyBindings(SettingsInterface& si);
	static void AddPadBindings(SettingsInterface& si, u32 pad, const char* default_type);
	static void UpdateContinuedVibration();

	static bool DoEventHook(InputBindingKey key, float value);
} // namespace InputManager

// ------------------------------------------------------------------------
// Local Variables
// ------------------------------------------------------------------------

// This is a multimap containing any binds related to the specified key.
using BindingMap = std::unordered_multimap<InputBindingKey, std::shared_ptr<InputBinding>, InputBindingKeyHash>;
using VibrationBindingArray = std::vector<PadVibrationBinding>;
static BindingMap s_binding_map;
static VibrationBindingArray s_pad_vibration_array;
static std::mutex s_binding_map_write_lock;

// Hooks/intercepting (for setting bindings)
static std::mutex m_event_intercept_mutex;
static InputInterceptHook::Callback m_event_intercept_callback;

// Input sources. Keyboard/mouse don't exist here.
static std::array<std::unique_ptr<InputSource>, static_cast<u32>(InputSourceType::Count)> s_input_sources;

// ------------------------------------------------------------------------
// Hotkeys
// ------------------------------------------------------------------------
static const HotkeyInfo* const s_hotkey_list[] = {g_vm_manager_hotkeys, g_gs_hotkeys, g_host_hotkeys};

// ------------------------------------------------------------------------
// Binding Parsing
// ------------------------------------------------------------------------

std::vector<std::string_view> InputManager::SplitChord(const std::string_view& binding)
{
	std::vector<std::string_view> parts;

	// under an if for RVO
	if (!binding.empty())
	{
		std::string_view::size_type last = 0;
		std::string_view::size_type next;
		while ((next = binding.find('&', last)) != std::string_view::npos)
		{
			if (last != next)
			{
				std::string_view part(StringUtil::StripWhitespace(binding.substr(last, next - last)));
				if (!part.empty())
					parts.push_back(std::move(part));
			}
			last = next + 1;
		}
		if (last < (binding.size() - 1))
		{
			std::string_view part(StringUtil::StripWhitespace(binding.substr(last)));
			if (!part.empty())
				parts.push_back(std::move(part));
		}
	}

	return parts;
}

bool InputManager::SplitBinding(
	const std::string_view& binding, std::string_view* source, std::string_view* sub_binding)
{
	const std::string_view::size_type slash_pos = binding.find('/');
	if (slash_pos == std::string_view::npos)
	{
		Console.Warning("Malformed binding: '%.*s'", static_cast<int>(binding.size()), binding.data());
		return false;
	}

	*source = std::string_view(binding).substr(0, slash_pos);
	*sub_binding = std::string_view(binding).substr(slash_pos + 1);
	return true;
}

std::optional<InputBindingKey> InputManager::ParseInputBindingKey(const std::string_view& binding)
{
	std::string_view source, sub_binding;
	if (!SplitBinding(binding, &source, &sub_binding))
		return std::nullopt;

	// lameee, string matching
	if (StringUtil::StartsWith(source, "Keyboard"))
	{
		return ParseHostKeyboardKey(source, sub_binding);
	}
	else if (StringUtil::StartsWith(source, "Mouse"))
	{
		return ParseHostMouseKey(source, sub_binding);
	}
	else
	{
		for (u32 i = FIRST_EXTERNAL_INPUT_SOURCE; i < LAST_EXTERNAL_INPUT_SOURCE; i++)
		{
			if (s_input_sources[i])
			{
				std::optional<InputBindingKey> key = s_input_sources[i]->ParseKeyString(source, sub_binding);
				if (key.has_value())
					return key;
			}
		}
	}

	return std::nullopt;
}

bool InputManager::ParseBindingAndGetSource(const std::string_view& binding, InputBindingKey* key, InputSource** source)
{
	std::string_view source_string, sub_binding;
	if (!SplitBinding(binding, &source_string, &sub_binding))
		return false;

	for (u32 i = FIRST_EXTERNAL_INPUT_SOURCE; i < LAST_EXTERNAL_INPUT_SOURCE; i++)
	{
		if (s_input_sources[i])
		{
			std::optional<InputBindingKey> parsed_key = s_input_sources[i]->ParseKeyString(source_string, sub_binding);
			if (parsed_key.has_value())
			{
				*key = parsed_key.value();
				*source = s_input_sources[i].get();
				return true;
			}
		}
	}

	return false;
}

std::string InputManager::ConvertInputBindingKeyToString(InputBindingKey key)
{
	if (key.source_type == InputSourceType::Keyboard)
	{
		const std::optional<std::string> str(ConvertHostKeyboardCodeToString(key.data));
		if (str.has_value() && !str->empty())
			return StringUtil::StdStringFromFormat("Keyboard/%s", str->c_str());
	}
	else if (key.source_type == InputSourceType::Mouse)
	{
		if (key.source_subtype == InputSubclass::MouseButton)
			return StringUtil::StdStringFromFormat("Mouse%u/Button%u", key.source_index, key.data);
		else if (key.source_subtype == InputSubclass::MousePointer)
			return StringUtil::StdStringFromFormat("Mouse%u/Pointer%u", key.source_index, key.data);
		else if (key.source_subtype == InputSubclass::MouseWheel)
			return StringUtil::StdStringFromFormat(
				"Mouse%u/Wheel%u%c", key.source_index, key.data, key.negative ? '-' : '+');
	}
	else if (key.source_type < InputSourceType::Count && s_input_sources[static_cast<u32>(key.source_type)])
	{
		return s_input_sources[static_cast<u32>(key.source_type)]->ConvertKeyToString(key);
	}

	return {};
}

std::string InputManager::ConvertInputBindingKeysToString(const InputBindingKey* keys, size_t num_keys)
{
	std::stringstream ss;
	for (size_t i = 0; i < num_keys; i++)
	{
		const std::string keystr(ConvertInputBindingKeyToString(keys[i]));
		if (keystr.empty())
			return std::string();

		if (i > 0)
			ss << " & ";

		ss << keystr;
	}

	return ss.str();
}

void InputManager::AddBindings(const std::vector<std::string>& bindings, const InputEventHandler& handler)
{
	for (const std::string& binding : bindings)
	{
		std::shared_ptr<InputBinding> ibinding;
		const std::vector<std::string_view> chord_bindings(SplitChord(binding));

		for (const std::string_view& chord_binding : chord_bindings)
		{
			std::optional<InputBindingKey> key = ParseInputBindingKey(chord_binding);
			if (!key.has_value())
			{
				Console.WriteLn("Invalid binding: '%s'", binding.c_str());
				ibinding.reset();
				break;
			}

			if (!ibinding)
			{
				ibinding = std::make_shared<InputBinding>();
				ibinding->handler = handler;
			}

			if (ibinding->num_keys == MAX_KEYS_PER_BINDING)
			{
				Console.WriteLn("Too many chord parts, max is %u (%s)", MAX_KEYS_PER_BINDING, binding.c_str());
				ibinding.reset();
				break;
			}

			ibinding->keys[ibinding->num_keys] = key.value();
			ibinding->full_mask |= (static_cast<u8>(1) << ibinding->num_keys);
			ibinding->num_keys++;
		}

		if (!ibinding)
			continue;

		// plop it in the input map for all the keys
		for (u32 i = 0; i < ibinding->num_keys; i++)
			s_binding_map.emplace(ibinding->keys[i].MaskDirection(), ibinding);
	}
}

// ------------------------------------------------------------------------
// Key Decoders
// ------------------------------------------------------------------------

InputBindingKey InputManager::MakeHostKeyboardKey(s32 key_code)
{
	InputBindingKey key = {};
	key.source_type = InputSourceType::Keyboard;
	key.data = static_cast<u32>(key_code);
	return key;
}

InputBindingKey InputManager::MakeHostMouseButtonKey(s32 button_index)
{
	InputBindingKey key = {};
	key.source_type = InputSourceType::Mouse;
	key.source_subtype = InputSubclass::MouseButton;
	key.data = static_cast<u32>(button_index);
	return key;
}

InputBindingKey InputManager::MakeHostMouseWheelKey(s32 axis_index)
{
	InputBindingKey key = {};
	key.source_type = InputSourceType::Mouse;
	key.source_subtype = InputSubclass::MouseWheel;
	key.data = static_cast<u32>(axis_index);
	return key;
}

// ------------------------------------------------------------------------
// Bind Encoders
// ------------------------------------------------------------------------

static std::array<const char*, static_cast<u32>(InputSourceType::Count)> s_input_class_names = {{
	"Keyboard",
	"Mouse",
#ifdef _WIN32
	"XInput",
#endif
#ifdef SDL_BUILD
	"SDL",
#endif
}};

InputSource* InputManager::GetInputSourceInterface(InputSourceType type)
{
	return s_input_sources[static_cast<u32>(type)].get();
}

const char* InputManager::InputSourceToString(InputSourceType clazz)
{
	return s_input_class_names[static_cast<u32>(clazz)];
}

std::optional<InputSourceType> InputManager::ParseInputSourceString(const std::string_view& str)
{
	for (u32 i = 0; i < static_cast<u32>(InputSourceType::Count); i++)
	{
		if (str == s_input_class_names[i])
			return static_cast<InputSourceType>(i);
	}

	return std::nullopt;
}

std::optional<InputBindingKey> InputManager::ParseHostKeyboardKey(
	const std::string_view& source, const std::string_view& sub_binding)
{
	if (source != "Keyboard")
		return std::nullopt;

	const std::optional<s32> code = ConvertHostKeyboardStringToCode(sub_binding);
	if (!code.has_value())
		return std::nullopt;

	InputBindingKey key = {};
	key.source_type = InputSourceType::Keyboard;
	key.data = static_cast<u32>(code.value());
	return key;
}

std::optional<InputBindingKey> InputManager::ParseHostMouseKey(
	const std::string_view& source, const std::string_view& sub_binding)
{
	if (source != "Mouse")
		return std::nullopt;

	InputBindingKey key = {};
	key.source_type = InputSourceType::Mouse;

	if (StringUtil::StartsWith(sub_binding, "Button"))
	{
		const std::optional<s32> button_number = StringUtil::FromChars<s32>(sub_binding.substr(6));
		if (!button_number.has_value() || button_number.value() < 0)
			return std::nullopt;

		key.source_subtype = InputSubclass::MouseButton;
		key.data = static_cast<u32>(button_number.value());
	}
	else
	{
		return std::nullopt;
	}

	return key;
}

// ------------------------------------------------------------------------
// Binding Enumeration
// ------------------------------------------------------------------------

std::vector<const HotkeyInfo*> InputManager::GetHotkeyList()
{
	std::vector<const HotkeyInfo*> ret;
	for (const HotkeyInfo* hotkey_list : s_hotkey_list)
	{
		for (const HotkeyInfo* hotkey = hotkey_list; hotkey->name != nullptr; hotkey++)
			ret.push_back(hotkey);
	}
	std::sort(ret.begin(), ret.end(), [](const HotkeyInfo* left, const HotkeyInfo* right) {
		// category -> display name
		if (const int res = StringUtil::Strcasecmp(left->category, right->category); res != 0)
			return (res < 0);
		return (StringUtil::Strcasecmp(left->display_name, right->display_name) < 0);
	});
	return ret;
}

void InputManager::AddHotkeyBindings(SettingsInterface& si)
{
	for (const HotkeyInfo* hotkey_list : s_hotkey_list)
	{
		for (const HotkeyInfo* hotkey = hotkey_list; hotkey->name != nullptr; hotkey++)
		{
			const std::vector<std::string> bindings(si.GetStringList("Hotkeys", hotkey->name));
			if (bindings.empty())
				continue;

			AddBindings(bindings, InputButtonEventHandler{hotkey->handler});
		}
	}
}

void InputManager::AddPadBindings(SettingsInterface& si, u32 pad_index, const char* default_type)
{
	const std::string section(StringUtil::StdStringFromFormat("Pad%u", pad_index + 1));
	const std::string type(si.GetStringValue(section.c_str(), "Type", default_type));
	if (type.empty() || type == "None")
		return;

	const std::vector<std::string> bind_names = PAD::GetControllerBinds(type);
	if (!bind_names.empty())
	{
		for (u32 bind_index = 0; bind_index < static_cast<u32>(bind_names.size()); bind_index++)
		{
			const std::string& bind_name = bind_names[bind_index];
			const std::vector<std::string> bindings(si.GetStringList(section.c_str(), bind_name.c_str()));
			if (!bindings.empty())
			{
				// we use axes for all pad bindings to simplify things, and because they are pressure sensitive
				AddBindings(bindings, InputAxisEventHandler{[pad_index, bind_index, bind_names](float value) {
					PAD::SetControllerState(pad_index, bind_index, value);
				}});
			}
		}
	}

	for (u32 macro_button_index = 0; macro_button_index < PAD::NUM_MACRO_BUTTONS_PER_CONTROLLER; macro_button_index++)
	{
		const std::vector<std::string> bindings(si.GetStringList(section.c_str(),
			StringUtil::StdStringFromFormat("Macro%u", macro_button_index + 1).c_str()));
		if (!bindings.empty())
		{
			AddBindings(bindings, InputButtonEventHandler{[pad_index, macro_button_index](bool state) {
				PAD::SetMacroButtonState(pad_index, macro_button_index, state);
			}});
		}
	}

	const PAD::VibrationCapabilities vibcaps = PAD::GetControllerVibrationCapabilities(type);
	if (vibcaps != PAD::VibrationCapabilities::NoVibration)
	{
		PadVibrationBinding vib;
		vib.pad_index = pad_index;

		bool has_any_bindings = false;
		switch (vibcaps)
		{
			case PAD::VibrationCapabilities::LargeSmallMotors:
			{
				if (const std::string large_binding(si.GetStringValue(section.c_str(), "LargeMotor")); !large_binding.empty())
					has_any_bindings |= ParseBindingAndGetSource(large_binding, &vib.motors[0].binding, &vib.motors[0].source);
				if (const std::string small_binding(si.GetStringValue(section.c_str(), "SmallMotor")); !small_binding.empty())
					has_any_bindings |= ParseBindingAndGetSource(small_binding, &vib.motors[1].binding, &vib.motors[1].source);
			}
			break;

			case PAD::VibrationCapabilities::SingleMotor:
			{
				if (const std::string binding(si.GetStringValue(section.c_str(), "Motor")); !binding.empty())
					has_any_bindings |= ParseBindingAndGetSource(binding, &vib.motors[0].binding, &vib.motors[0].source);
			}
			break;

			default:
				break;
		}

		if (has_any_bindings)
			s_pad_vibration_array.push_back(std::move(vib));
	}
}

// ------------------------------------------------------------------------
// Event Handling
// ------------------------------------------------------------------------

bool InputManager::HasAnyBindingsForKey(InputBindingKey key)
{
	std::unique_lock lock(s_binding_map_write_lock);
	return (s_binding_map.find(key.MaskDirection()) != s_binding_map.end());
}

bool InputManager::IsAxisHandler(const InputEventHandler& handler)
{
	return std::holds_alternative<InputAxisEventHandler>(handler);
}

bool InputManager::InvokeEvents(InputBindingKey key, float value)
{
	if (DoEventHook(key, value))
		return true;

	// find all the bindings associated with this key
	const InputBindingKey masked_key = key.MaskDirection();
	const auto range = s_binding_map.equal_range(masked_key);
	if (range.first == s_binding_map.end())
		return false;

	// Workaround for modifier keys. Basically, if we bind say, F1 and Shift+F1, and press shift
	// and then F1, we'll fire bindings for both F1 and Shift+F1, when we really only want to fire
	// the binding for Shift+F1. So, let's search through the binding list, and see if there's a
	// "longer" binding (more keys), and if so, only activate that and not the shorter binding(s).
	const InputBinding* longest_hotkey_binding = nullptr;
	for (auto it = range.first; it != range.second; ++it)
	{
		InputBinding* binding = it->second.get();
		if (IsAxisHandler(binding->handler))
			continue;

		// find the key which matches us
		for (u32 i = 0; i < binding->num_keys; i++)
		{
			if (binding->keys[i].MaskDirection() != masked_key)
				continue;

			const u8 bit = static_cast<u8>(1) << i;
			const bool negative = binding->keys[i].negative;
			const bool new_state = (negative ? (value < 0.0f) : (value > 0.0f));
			const u8 new_mask = (new_state ? (binding->current_mask | bit) : (binding->current_mask & ~bit));
			const bool prev_full_state = (binding->current_mask == binding->full_mask);
			const bool new_full_state = (new_mask == binding->full_mask);

			// If we're activating this chord, block activation of other bindings with fewer keys.
			if (prev_full_state || new_full_state)
			{
				if (!longest_hotkey_binding || longest_hotkey_binding->num_keys < binding->num_keys)
					longest_hotkey_binding = binding;
			}

			break;
		}
	}

	// Now we can actually fire/activate bindings.
	for (auto it = range.first; it != range.second; ++it)
	{
		InputBinding* binding = it->second.get();

		// find the key which matches us
		for (u32 i = 0; i < binding->num_keys; i++)
		{
			if (binding->keys[i].MaskDirection() != masked_key)
				continue;

			const u8 bit = static_cast<u8>(1) << i;
			const bool negative = binding->keys[i].negative;
			const bool new_state = (negative ? (value < 0.0f) : (value > 0.0f));

			// Don't register the key press when we're part of a longer chord. That way,
			// the state won't change, and it won't get the released event either.
			if (longest_hotkey_binding && new_state && !IsAxisHandler(binding->handler) &&
				binding->num_keys != longest_hotkey_binding->num_keys)
			{
				continue;
			}

			// update state based on whether the whole chord was activated
			const u8 new_mask = (new_state ? (binding->current_mask | bit) : (binding->current_mask & ~bit));
			const bool prev_full_state = (binding->current_mask == binding->full_mask);
			const bool new_full_state = (new_mask == binding->full_mask);
			binding->current_mask = new_mask;

			// invert if we're negative, since the handler expects 0..1
			const float value_to_pass = (negative ? ((value < 0.0f) ? -value : 0.0f) : (value > 0.0f) ? value :
                                                                                                        0.0f);

			// axes are fired regardless of a state change, unless they're zero
			// (but going from not-zero to zero will still fire, because of the full state)
			// for buttons, we can use the state of the last chord key, because it'll be 1 on press,
			// and 0 on release (when the full state changes).
			if (IsAxisHandler(binding->handler))
			{
				if (prev_full_state != new_full_state || value_to_pass >= 0.0f)
					std::get<InputAxisEventHandler>(binding->handler)(value_to_pass);
			}
			else
			{
				if (prev_full_state != new_full_state)
					std::get<InputButtonEventHandler>(binding->handler)(value_to_pass > 0.0f);
			}

			// bail out, since we shouldn't have the same key twice in the chord
			break;
		}
	}

	return true;
}

// ------------------------------------------------------------------------
// Vibration
// ------------------------------------------------------------------------

void InputManager::SetPadVibrationIntensity(u32 pad_index, float large_or_single_motor_intensity, float small_motor_intensity)
{
	for (PadVibrationBinding& pad : s_pad_vibration_array)
	{
		if (pad.pad_index != pad_index)
			continue;

		PadVibrationBinding::Motor& large_motor = pad.motors[0];
		PadVibrationBinding::Motor& small_motor = pad.motors[1];
		if (large_motor.last_intensity == large_or_single_motor_intensity && small_motor.last_intensity == small_motor_intensity)
			continue;

		if (pad.AreMotorsCombined())
		{
			// if the motors are combined, we need to adjust to the maximum of both
			const float report_intensity = std::max(large_or_single_motor_intensity, small_motor_intensity);
			if (large_motor.source)
			{
				large_motor.last_update_time = Common::Timer::GetCurrentValue();
				large_motor.source->UpdateMotorState(large_motor.binding, report_intensity);
			}
		}
		else if (large_motor.source == small_motor.source)
		{
			// both motors are bound to the same source, do an optimal update
			large_motor.last_update_time = Common::Timer::GetCurrentValue();
			large_motor.source->UpdateMotorState(large_motor.binding, small_motor.binding, large_or_single_motor_intensity, small_motor_intensity);
		}
		else
		{
			// update motors independently
			if (large_motor.source && large_motor.last_intensity != large_or_single_motor_intensity)
			{
				large_motor.last_update_time = Common::Timer::GetCurrentValue();
				large_motor.source->UpdateMotorState(large_motor.binding, large_or_single_motor_intensity);
			}
			if (small_motor.source && small_motor.last_intensity != small_motor_intensity)
			{
				small_motor.last_update_time = Common::Timer::GetCurrentValue();
				small_motor.source->UpdateMotorState(small_motor.binding, small_motor_intensity);
			}
		}

		large_motor.last_intensity = large_or_single_motor_intensity;
		small_motor.last_intensity = small_motor_intensity;
	}
}

void InputManager::PauseVibration()
{
	for (PadVibrationBinding& binding : s_pad_vibration_array)
	{
		for (u32 motor_index = 0; motor_index < MAX_MOTORS_PER_PAD; motor_index++)
		{
			PadVibrationBinding::Motor& motor = binding.motors[motor_index];
			if (!motor.source || motor.last_intensity == 0.0f)
				continue;

			// we deliberately don't zero the intensity here, so it can resume later
			motor.last_update_time = 0;
			motor.source->UpdateMotorState(motor.binding, 0.0f);
		}
	}
}

void InputManager::UpdateContinuedVibration()
{
	// update vibration intensities, so if the game does a long effect, it continues
	const u64 current_time = Common::Timer::GetCurrentValue();
	for (PadVibrationBinding& pad : s_pad_vibration_array)
	{
		if (pad.AreMotorsCombined())
		{
			// motors are combined
			PadVibrationBinding::Motor& large_motor = pad.motors[0];
			if (!large_motor.source)
				continue;

			// so only check the first one
			const double dt = Common::Timer::ConvertValueToSeconds(current_time - large_motor.last_update_time);
			if (dt < VIBRATION_UPDATE_INTERVAL_SECONDS)
				continue;

			// but take max of both motors for the intensity
			const float intensity = pad.GetCombinedIntensity();
			if (intensity == 0.0f)
				continue;

			large_motor.last_update_time = current_time;
			large_motor.source->UpdateMotorState(large_motor.binding, intensity);
		}
		else
		{
			// independent motor control
			for (u32 i = 0; i < MAX_MOTORS_PER_PAD; i++)
			{
				PadVibrationBinding::Motor& motor = pad.motors[i];
				if (!motor.source || motor.last_intensity == 0.0f)
					continue;

				const double dt = Common::Timer::ConvertValueToSeconds(current_time - motor.last_update_time);
				if (dt < VIBRATION_UPDATE_INTERVAL_SECONDS)
					continue;

				// re-notify the source of the continued effect
				motor.last_update_time = current_time;
				motor.source->UpdateMotorState(motor.binding, motor.last_intensity);
			}
		}
	}
}

// ------------------------------------------------------------------------
// Hooks/Event Intercepting
// ------------------------------------------------------------------------

void InputManager::SetHook(InputInterceptHook::Callback callback)
{
	std::unique_lock<std::mutex> lock(m_event_intercept_mutex);
	pxAssert(!m_event_intercept_callback);
	m_event_intercept_callback = std::move(callback);
}

void InputManager::RemoveHook()
{
	std::unique_lock<std::mutex> lock(m_event_intercept_mutex);
	if (m_event_intercept_callback)
		m_event_intercept_callback = {};
}

bool InputManager::HasHook()
{
	std::unique_lock<std::mutex> lock(m_event_intercept_mutex);
	return (bool)m_event_intercept_callback;
}

bool InputManager::DoEventHook(InputBindingKey key, float value)
{
	std::unique_lock<std::mutex> lock(m_event_intercept_mutex);
	if (!m_event_intercept_callback)
		return false;

	const InputInterceptHook::CallbackResult action = m_event_intercept_callback(key, value);
	return (action == InputInterceptHook::CallbackResult::StopProcessingEvent);
}

// ------------------------------------------------------------------------
// Binding Updater
// ------------------------------------------------------------------------

// TODO(Stenzek): Find a better place for this. Maybe in PAD?
static constexpr std::array<const char*, InputManager::MAX_PAD_NUMBER> s_default_pad_types = {{
	"DualShock2", // Pad 1
	"None" // Pad 2
}};

void InputManager::ReloadBindings(SettingsInterface& si)
{
	PauseVibration();

	std::unique_lock lock(s_binding_map_write_lock);

	s_binding_map.clear();
	s_pad_vibration_array.clear();

	AddHotkeyBindings(si);

	for (u32 pad = 0; pad < MAX_PAD_NUMBER; pad++)
		AddPadBindings(si, pad, s_default_pad_types[pad]);
}

// ------------------------------------------------------------------------
// Source Management
// ------------------------------------------------------------------------

void InputManager::CloseSources()
{
	for (u32 i = FIRST_EXTERNAL_INPUT_SOURCE; i < LAST_EXTERNAL_INPUT_SOURCE; i++)
	{
		if (s_input_sources[i])
		{
			s_input_sources[i]->Shutdown();
			s_input_sources[i].reset();
		}
	}
}

void InputManager::PollSources()
{
	for (u32 i = FIRST_EXTERNAL_INPUT_SOURCE; i < LAST_EXTERNAL_INPUT_SOURCE; i++)
	{
		if (s_input_sources[i])
			s_input_sources[i]->PollEvents();
	}

	if (VMManager::GetState() == VMState::Running && !s_pad_vibration_array.empty())
		UpdateContinuedVibration();
}


std::vector<std::pair<std::string, std::string>> InputManager::EnumerateDevices()
{
	std::vector<std::pair<std::string, std::string>> ret;

	ret.emplace_back("Keyboard", "Keyboard");
	ret.emplace_back("Mouse", "Mouse");

	for (u32 i = FIRST_EXTERNAL_INPUT_SOURCE; i < LAST_EXTERNAL_INPUT_SOURCE; i++)
	{
		if (s_input_sources[i])
		{
			std::vector<std::pair<std::string, std::string>> devs(s_input_sources[i]->EnumerateDevices());
			if (ret.empty())
				ret = std::move(devs);
			else
				std::move(devs.begin(), devs.end(), std::back_inserter(ret));
		}
	}

	return ret;
}

std::vector<InputBindingKey> InputManager::EnumerateMotors()
{
	std::vector<InputBindingKey> ret;

	for (u32 i = FIRST_EXTERNAL_INPUT_SOURCE; i < LAST_EXTERNAL_INPUT_SOURCE; i++)
	{
		if (s_input_sources[i])
		{
			std::vector<InputBindingKey> devs(s_input_sources[i]->EnumerateMotors());
			if (ret.empty())
				ret = std::move(devs);
			else
				std::move(devs.begin(), devs.end(), std::back_inserter(ret));
		}
	}

	return ret;
}

static void GetKeyboardGenericBindingMapping(std::vector<std::pair<GenericInputBinding, std::string>>* mapping)
{
	mapping->emplace_back(GenericInputBinding::DPadUp, "Keyboard/Up");
	mapping->emplace_back(GenericInputBinding::DPadRight, "Keyboard/Right");
	mapping->emplace_back(GenericInputBinding::DPadDown, "Keyboard/Down");
	mapping->emplace_back(GenericInputBinding::DPadLeft, "Keyboard/Left");
	mapping->emplace_back(GenericInputBinding::LeftStickUp, "Keyboard/W");
	mapping->emplace_back(GenericInputBinding::LeftStickRight, "Keyboard/D");
	mapping->emplace_back(GenericInputBinding::LeftStickDown, "Keyboard/S");
	mapping->emplace_back(GenericInputBinding::LeftStickLeft, "Keyboard/A");
	mapping->emplace_back(GenericInputBinding::RightStickUp, "Keyboard/T");
	mapping->emplace_back(GenericInputBinding::RightStickRight, "Keyboard/H");
	mapping->emplace_back(GenericInputBinding::RightStickDown, "Keyboard/G");
	mapping->emplace_back(GenericInputBinding::RightStickLeft, "Keyboard/F");
	mapping->emplace_back(GenericInputBinding::Start, "Keyboard/Return");
	mapping->emplace_back(GenericInputBinding::Select, "Keyboard/Backspace");
	mapping->emplace_back(GenericInputBinding::Triangle, "Keyboard/I");
	mapping->emplace_back(GenericInputBinding::Circle, "Keyboard/L");
	mapping->emplace_back(GenericInputBinding::Cross, "Keyboard/K");
	mapping->emplace_back(GenericInputBinding::Square, "Keyboard/J");
	mapping->emplace_back(GenericInputBinding::L1, "Keyboard/Q");
	mapping->emplace_back(GenericInputBinding::L2, "Keyboard/1");
	mapping->emplace_back(GenericInputBinding::L3, "Keyboard/2");
	mapping->emplace_back(GenericInputBinding::R1, "Keyboard/E");
	mapping->emplace_back(GenericInputBinding::R2, "Keyboard/2");
	mapping->emplace_back(GenericInputBinding::R3, "Keyboard/4");
}

static bool GetInternalGenericBindingMapping(const std::string_view& device, GenericInputBindingMapping* mapping)
{
	if (device == "Keyboard")
	{
		GetKeyboardGenericBindingMapping(mapping);
		return true;
	}

	return false;
}

GenericInputBindingMapping InputManager::GetGenericBindingMapping(const std::string_view& device)
{
	GenericInputBindingMapping mapping;

	if (!GetInternalGenericBindingMapping(device, &mapping))
	{
		for (u32 i = FIRST_EXTERNAL_INPUT_SOURCE; i < LAST_EXTERNAL_INPUT_SOURCE; i++)
		{
			if (s_input_sources[i] && s_input_sources[i]->GetGenericBindingMapping(device, &mapping))
				break;
		}
	}

	return mapping;
}

template <typename T>
static void UpdateInputSourceState(SettingsInterface& si, InputSourceType type, bool default_state)
{
	const bool enabled = si.GetBoolValue("InputSources", InputManager::InputSourceToString(type), default_state);
	if (enabled)
	{
		if (s_input_sources[static_cast<u32>(type)])
		{
			s_input_sources[static_cast<u32>(type)]->UpdateSettings(si);
		}
		else
		{
			std::unique_ptr<InputSource> source = std::make_unique<T>();
			if (!source->Initialize(si))
			{
				Console.Error("(InputManager) Source '%s' failed to initialize.", InputManager::InputSourceToString(type));
				return;
			}

			s_input_sources[static_cast<u32>(type)] = std::move(source);
		}
	}
	else
	{
		if (s_input_sources[static_cast<u32>(type)])
		{
			s_input_sources[static_cast<u32>(type)]->Shutdown();
			s_input_sources[static_cast<u32>(type)].reset();
		}
	}
}

#ifdef _WIN32
#include "Frontend/XInputSource.h"
#endif

#ifdef SDL_BUILD
#include "Frontend/SDLInputSource.h"
#endif

void InputManager::ReloadSources(SettingsInterface& si)
{
#ifdef _WIN32
	UpdateInputSourceState<XInputSource>(si, InputSourceType::XInput, false);
#endif
#ifdef SDL_BUILD
	UpdateInputSourceState<SDLInputSource>(si, InputSourceType::SDL, true);
#endif
}
