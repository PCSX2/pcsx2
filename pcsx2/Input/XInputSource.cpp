// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Input/XInputSource.h"
#include "Input/InputManager.h"

#include "common/Assertions.h"
#include "common/StringUtil.h"
#include "common/Console.h"

#include "IconsPromptFont.h"

#include <cmath>

static const char* s_axis_names[XInputSource::NUM_AXES] = {
	"LeftX", // AXIS_LEFTX
	"LeftY", // AXIS_LEFTY
	"RightX", // AXIS_RIGHTX
	"RightY", // AXIS_RIGHTY
	"LeftTrigger", // AXIS_TRIGGERLEFT
	"RightTrigger", // AXIS_TRIGGERRIGHT
};
static constexpr const char* s_axis_icons[][2] = {
	{ICON_PF_LEFT_ANALOG_LEFT, ICON_PF_LEFT_ANALOG_RIGHT}, // AXIS_LEFTX
	{ICON_PF_LEFT_ANALOG_UP, ICON_PF_LEFT_ANALOG_DOWN}, // AXIS_LEFTY
	{ICON_PF_RIGHT_ANALOG_LEFT, ICON_PF_RIGHT_ANALOG_RIGHT}, // AXIS_RIGHTX
	{ICON_PF_RIGHT_ANALOG_UP, ICON_PF_RIGHT_ANALOG_DOWN}, // AXIS_RIGHTY
	{nullptr, ICON_PF_LEFT_TRIGGER_PULL}, // AXIS_TRIGGERLEFT
	{nullptr, ICON_PF_RIGHT_TRIGGER_PULL}, // AXIS_TRIGGERRIGHT
};
static const GenericInputBinding s_xinput_generic_binding_axis_mapping[][2] = {
	{GenericInputBinding::LeftStickLeft, GenericInputBinding::LeftStickRight}, // AXIS_LEFTX
	{GenericInputBinding::LeftStickUp, GenericInputBinding::LeftStickDown}, // AXIS_LEFTY
	{GenericInputBinding::RightStickLeft, GenericInputBinding::RightStickRight}, // AXIS_RIGHTX
	{GenericInputBinding::RightStickUp, GenericInputBinding::RightStickDown}, // AXIS_RIGHTY
	{GenericInputBinding::Unknown, GenericInputBinding::L2}, // AXIS_TRIGGERLEFT
	{GenericInputBinding::Unknown, GenericInputBinding::R2}, // AXIS_TRIGGERRIGHT
};

static const char* s_button_names[XInputSource::NUM_BUTTONS] = {
	"DPadUp", // XINPUT_GAMEPAD_DPAD_UP
	"DPadDown", // XINPUT_GAMEPAD_DPAD_DOWN
	"DPadLeft", // XINPUT_GAMEPAD_DPAD_LEFT
	"DPadRight", // XINPUT_GAMEPAD_DPAD_RIGHT
	"Start", // XINPUT_GAMEPAD_START
	"Back", // XINPUT_GAMEPAD_BACK
	"LeftStick", // XINPUT_GAMEPAD_LEFT_THUMB
	"RightStick", // XINPUT_GAMEPAD_RIGHT_THUMB
	"LeftShoulder", // XINPUT_GAMEPAD_LEFT_SHOULDER
	"RightShoulder", // XINPUT_GAMEPAD_RIGHT_SHOULDER
	"A", // XINPUT_GAMEPAD_A
	"B", // XINPUT_GAMEPAD_B
	"X", // XINPUT_GAMEPAD_X
	"Y", // XINPUT_GAMEPAD_Y
	"Guide", // XINPUT_GAMEPAD_GUIDE
};
static const u16 s_button_masks[XInputSource::NUM_BUTTONS] = {
	XINPUT_GAMEPAD_DPAD_UP, XINPUT_GAMEPAD_DPAD_DOWN, XINPUT_GAMEPAD_DPAD_LEFT, XINPUT_GAMEPAD_DPAD_RIGHT, XINPUT_GAMEPAD_START,
	XINPUT_GAMEPAD_BACK, XINPUT_GAMEPAD_LEFT_THUMB, XINPUT_GAMEPAD_RIGHT_THUMB, XINPUT_GAMEPAD_LEFT_SHOULDER, XINPUT_GAMEPAD_RIGHT_SHOULDER,
	XINPUT_GAMEPAD_A, XINPUT_GAMEPAD_B, XINPUT_GAMEPAD_X, XINPUT_GAMEPAD_Y,
	0x400, // XINPUT_GAMEPAD_GUIDE
};
static constexpr const char* s_button_icons[] = {
	ICON_PF_XBOX_DPAD_UP, // XINPUT_GAMEPAD_DPAD_UP
	ICON_PF_XBOX_DPAD_DOWN, // XINPUT_GAMEPAD_DPAD_DOWN
	ICON_PF_XBOX_DPAD_LEFT, // XINPUT_GAMEPAD_DPAD_LEFT
	ICON_PF_XBOX_DPAD_RIGHT, // XINPUT_GAMEPAD_DPAD_RIGHT
	ICON_PF_BURGER_MENU, // XINPUT_GAMEPAD_START
	ICON_PF_SHARE_CAPTURE, // XINPUT_GAMEPAD_BACK
	ICON_PF_LEFT_ANALOG_CLICK, // XINPUT_GAMEPAD_LEFT_THUMB
	ICON_PF_RIGHT_ANALOG_CLICK, // XINPUT_GAMEPAD_RIGHT_THUMB
	ICON_PF_LEFT_SHOULDER_LB, // XINPUT_GAMEPAD_LEFT_SHOULDER
	ICON_PF_RIGHT_SHOULDER_RB, // XINPUT_GAMEPAD_RIGHT_SHOULDER
	ICON_PF_BUTTON_A, // XINPUT_GAMEPAD_A
	ICON_PF_BUTTON_B, // XINPUT_GAMEPAD_B
	ICON_PF_BUTTON_X, // XINPUT_GAMEPAD_X
	ICON_PF_BUTTON_Y, // XINPUT_GAMEPAD_Y
	ICON_PF_XBOX, // XINPUT_GAMEPAD_GUIDE
};
static const GenericInputBinding s_xinput_generic_binding_button_mapping[] = {
	GenericInputBinding::DPadUp, // XINPUT_GAMEPAD_DPAD_UP
	GenericInputBinding::DPadDown, // XINPUT_GAMEPAD_DPAD_DOWN
	GenericInputBinding::DPadLeft, // XINPUT_GAMEPAD_DPAD_LEFT
	GenericInputBinding::DPadRight, // XINPUT_GAMEPAD_DPAD_RIGHT
	GenericInputBinding::Start, // XINPUT_GAMEPAD_START
	GenericInputBinding::Select, // XINPUT_GAMEPAD_BACK
	GenericInputBinding::L3, // XINPUT_GAMEPAD_LEFT_THUMB
	GenericInputBinding::R3, // XINPUT_GAMEPAD_RIGHT_THUMB
	GenericInputBinding::L1, // XINPUT_GAMEPAD_LEFT_SHOULDER
	GenericInputBinding::R1, // XINPUT_GAMEPAD_RIGHT_SHOULDER
	GenericInputBinding::Cross, // XINPUT_GAMEPAD_A
	GenericInputBinding::Circle, // XINPUT_GAMEPAD_B
	GenericInputBinding::Square, // XINPUT_GAMEPAD_X
	GenericInputBinding::Triangle, // XINPUT_GAMEPAD_Y
	GenericInputBinding::System, // XINPUT_GAMEPAD_GUIDE
};

XInputSource::XInputSource() = default;

XInputSource::~XInputSource() = default;

bool XInputSource::Initialize(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock)
{
	// xinput1_3.dll is flawed and obsolete, but it's also commonly used by wrappers.
	// For this reason, try to load it *only* from the application directory, and not system32.
	m_xinput_module = LoadLibraryExW(L"xinput1_3", nullptr, LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
	if (!m_xinput_module)
	{
		m_xinput_module = LoadLibraryW(L"xinput1_4");
	}
	if (!m_xinput_module)
	{
		m_xinput_module = LoadLibraryW(L"xinput9_1_0");
	}
	if (!m_xinput_module)
	{
		Console.Error("Failed to load XInput module.");
		return false;
	}

	// Try the hidden version of XInputGetState(), which lets us query the guide button.
	m_xinput_get_state = reinterpret_cast<decltype(m_xinput_get_state)>(GetProcAddress(m_xinput_module, reinterpret_cast<LPCSTR>(100)));
	if (!m_xinput_get_state)
		m_xinput_get_state = reinterpret_cast<decltype(m_xinput_get_state)>(GetProcAddress(m_xinput_module, "XInputGetState"));
	m_xinput_set_state = reinterpret_cast<decltype(m_xinput_set_state)>(GetProcAddress(m_xinput_module, "XInputSetState"));
	m_xinput_get_capabilities =
		reinterpret_cast<decltype(m_xinput_get_capabilities)>(GetProcAddress(m_xinput_module, "XInputGetCapabilities"));

	// SCP extension, only exists when the bridge xinput1_3.dll is in use
	m_xinput_get_extended = reinterpret_cast<decltype(m_xinput_get_extended)>(GetProcAddress(m_xinput_module, "XInputGetExtended"));
	if (!m_xinput_get_state || !m_xinput_set_state || !m_xinput_get_capabilities)
	{
		Console.Error("Failed to get XInput function pointers.");
		return false;
	}

	ReloadDevices();
	return true;
}

void XInputSource::UpdateSettings(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock)
{
}

bool XInputSource::ReloadDevices()
{
	bool changed = false;
	for (u32 i = 0; i < NUM_CONTROLLERS; i++)
	{
		XINPUT_STATE new_state;
		SCP_EXTN new_state_scp;
		DWORD result = m_xinput_get_extended ? m_xinput_get_extended(i, &new_state_scp) : ERROR_NOT_SUPPORTED;
		if (result != ERROR_SUCCESS)
			result = m_xinput_get_state(i, &new_state);

		if (result == ERROR_SUCCESS)
		{
			if (m_controllers[i].connected)
				continue;

			HandleControllerConnection(i);
			changed = true;
		}
		else if (result == ERROR_DEVICE_NOT_CONNECTED)
		{
			if (!m_controllers[i].connected)
				continue;

			HandleControllerDisconnection(i);
			changed = true;
		}
	}

	return changed;
}

void XInputSource::Shutdown()
{
	for (u32 i = 0; i < NUM_CONTROLLERS; i++)
	{
		if (m_controllers[i].connected)
			HandleControllerDisconnection(i);
	}

	if (m_xinput_module)
	{
		FreeLibrary(m_xinput_module);
		m_xinput_module = nullptr;
	}

	m_xinput_get_state = nullptr;
	m_xinput_set_state = nullptr;
	m_xinput_get_capabilities = nullptr;
	m_xinput_get_extended = nullptr;
}

void XInputSource::PollEvents()
{
	for (u32 i = 0; i < NUM_CONTROLLERS; i++)
	{
		const bool was_connected = m_controllers[i].connected;
		if (!was_connected)
			continue;

		SCP_EXTN new_state_scp;
		DWORD result = m_xinput_get_extended ? m_xinput_get_extended(i, &new_state_scp) : ERROR_NOT_SUPPORTED;
		if (result == ERROR_SUCCESS)
		{
			if (!was_connected)
				HandleControllerConnection(i);

			CheckForStateChangesSCP(i, new_state_scp);
		}
		else
		{
			XINPUT_STATE new_state;
			result = m_xinput_get_state(i, &new_state);

			if (result == ERROR_SUCCESS)
			{
				if (!was_connected)
					HandleControllerConnection(i);

				CheckForStateChanges(i, new_state);
			}
			else
			{
				if (result != ERROR_DEVICE_NOT_CONNECTED)
					Console.Warning("XInputGetState(%u) failed: 0x%08X / 0x%08X", i, result, GetLastError());

				if (was_connected)
					HandleControllerDisconnection(i);
			}
		}
	}
}

std::vector<std::pair<std::string, std::string>> XInputSource::EnumerateDevices()
{
	std::vector<std::pair<std::string, std::string>> ret;

	for (u32 i = 0; i < NUM_CONTROLLERS; i++)
	{
		if (!m_controllers[i].connected)
			continue;

		ret.emplace_back(StringUtil::StdStringFromFormat("XInput-%u", i), StringUtil::StdStringFromFormat("XInput Controller %u", i));
	}

	return ret;
}

std::optional<InputBindingKey> XInputSource::ParseKeyString(const std::string_view& device, const std::string_view& binding)
{
	if (!device.starts_with("XInput-") || binding.empty())
		return std::nullopt;

	const std::optional<s32> player_id = StringUtil::FromChars<s32>(device.substr(7));
	if (!player_id.has_value() || player_id.value() < 0)
		return std::nullopt;

	InputBindingKey key = {};
	key.source_type = InputSourceType::XInput;
	key.source_index = static_cast<u32>(player_id.value());

	if (binding.ends_with("Motor"))
	{
		key.source_subtype = InputSubclass::ControllerMotor;
		if (binding == "LargeMotor")
		{
			key.data = 0;
			return key;
		}
		else if (binding == "SmallMotor")
		{
			key.data = 1;
			return key;
		}
		else
		{
			return std::nullopt;
		}
	}
	else if (binding[0] == '+' || binding[0] == '-')
	{
		// likely an axis
		const std::string_view axis_name(binding.substr(1));
		for (u32 i = 0; i < std::size(s_axis_names); i++)
		{
			if (axis_name == s_axis_names[i])
			{
				// found an axis!
				key.source_subtype = InputSubclass::ControllerAxis;
				key.data = i;
				key.modifier = binding[0] == '-' ? InputModifier::Negate : InputModifier::None;
				return key;
			}
		}
	}
	else
	{
		// must be a button
		for (u32 i = 0; i < std::size(s_button_names); i++)
		{
			if (binding == s_button_names[i])
			{
				key.source_subtype = InputSubclass::ControllerButton;
				key.data = i;
				return key;
			}
		}
	}

	// unknown axis/button
	return std::nullopt;
}

TinyString XInputSource::ConvertKeyToString(InputBindingKey key)
{
	TinyString ret;

	if (key.source_type == InputSourceType::XInput)
	{
		if (key.source_subtype == InputSubclass::ControllerAxis && key.data < std::size(s_axis_names))
		{
			const char modifier = key.modifier == InputModifier::Negate ? '-' : '+';
			ret.fmt("XInput-{}/{}{}", static_cast<u32>(key.source_index), modifier, s_axis_names[key.data]);
		}
		else if (key.source_subtype == InputSubclass::ControllerButton && key.data < std::size(s_button_names))
		{
			ret.fmt("XInput-{}/{}", static_cast<u32>(key.source_index), s_button_names[key.data]);
		}
		else if (key.source_subtype == InputSubclass::ControllerMotor)
		{
			ret.fmt("XInput-{}/{}Motor", static_cast<u32>(key.source_index), key.data ? "Large" : "Small");
		}
	}

	return ret;
}

TinyString XInputSource::ConvertKeyToIcon(InputBindingKey key)
{
	TinyString ret;

	if (key.source_type == InputSourceType::SDL)
	{
		if (key.source_subtype == InputSubclass::ControllerAxis)
		{
			if (key.data < std::size(s_axis_icons) && key.modifier != InputModifier::FullAxis)
			{
				ret.fmt("XInput-{}  {}", static_cast<u32>(key.source_index),
					s_axis_icons[key.data][key.modifier == InputModifier::None]);
			}
		}
		else if (key.source_subtype == InputSubclass::ControllerButton)
		{
			if (key.data < std::size(s_button_icons))
				ret.fmt("XInput-{}  {}", static_cast<u32>(key.source_index), s_button_icons[key.data]);
		}
	}

	return ret;
}

std::vector<InputBindingKey> XInputSource::EnumerateMotors()
{
	std::vector<InputBindingKey> ret;

	for (u32 i = 0; i < NUM_CONTROLLERS; i++)
	{
		const ControllerData& cd = m_controllers[i];
		if (!cd.connected)
			continue;

		if (cd.has_large_motor)
			ret.push_back(MakeGenericControllerMotorKey(InputSourceType::XInput, i, 0));

		if (cd.has_small_motor)
			ret.push_back(MakeGenericControllerMotorKey(InputSourceType::XInput, i, 1));
	}

	return ret;
}

bool XInputSource::GetGenericBindingMapping(const std::string_view& device, InputManager::GenericInputBindingMapping* mapping)
{
	if (!device.starts_with("XInput-"))
		return false;

	const std::optional<s32> player_id = StringUtil::FromChars<s32>(device.substr(7));
	if (!player_id.has_value() || player_id.value() < 0)
		return false;

	if (player_id.value() < 0 || player_id.value() >= static_cast<s32>(XUSER_MAX_COUNT))
		return false;

	// assume all buttons are present.
	const s32 pid = player_id.value();
	for (u32 i = 0; i < std::size(s_xinput_generic_binding_axis_mapping); i++)
	{
		const GenericInputBinding negative = s_xinput_generic_binding_axis_mapping[i][0];
		const GenericInputBinding positive = s_xinput_generic_binding_axis_mapping[i][1];
		if (negative != GenericInputBinding::Unknown)
			mapping->emplace_back(negative, StringUtil::StdStringFromFormat("XInput-%d/-%s", pid, s_axis_names[i]));

		if (positive != GenericInputBinding::Unknown)
			mapping->emplace_back(positive, StringUtil::StdStringFromFormat("XInput-%d/+%s", pid, s_axis_names[i]));
	}
	for (u32 i = 0; i < std::size(s_xinput_generic_binding_button_mapping); i++)
	{
		const GenericInputBinding binding = s_xinput_generic_binding_button_mapping[i];
		if (binding != GenericInputBinding::Unknown)
			mapping->emplace_back(binding, StringUtil::StdStringFromFormat("XInput-%d/%s", pid, s_button_names[i]));
	}

	if (m_controllers[pid].has_small_motor)
		mapping->emplace_back(GenericInputBinding::SmallMotor, StringUtil::StdStringFromFormat("XInput-%d/SmallMotor", pid));
	if (m_controllers[pid].has_large_motor)
		mapping->emplace_back(GenericInputBinding::LargeMotor, StringUtil::StdStringFromFormat("XInput-%d/LargeMotor", pid));

	return true;
}

void XInputSource::HandleControllerConnection(u32 index)
{
	Console.WriteLn("XInput controller %u connected.", index);

	XINPUT_CAPABILITIES caps = {};
	if (m_xinput_get_capabilities(index, 0, &caps) != ERROR_SUCCESS)
		Console.Warning("Failed to get XInput capabilities for controller %u", index);

	ControllerData& cd = m_controllers[index];
	cd.connected = true;
	cd.has_large_motor = caps.Vibration.wLeftMotorSpeed != 0;
	cd.has_small_motor = caps.Vibration.wRightMotorSpeed != 0;
	cd.last_state = {};
	cd.last_state_scp = {};

	InputManager::OnInputDeviceConnected(
		StringUtil::StdStringFromFormat("XInput-%u", index), StringUtil::StdStringFromFormat("XInput Controller %u", index));
}

void XInputSource::HandleControllerDisconnection(u32 index)
{
	Console.WriteLn("XInput controller %u disconnected.", index);
	InputManager::OnInputDeviceDisconnected(StringUtil::StdStringFromFormat("XInput-%u", index));
	m_controllers[index] = {};
}

void XInputSource::CheckForStateChanges(u32 index, const XINPUT_STATE& new_state)
{
	ControllerData& cd = m_controllers[index];
	if (new_state.dwPacketNumber == cd.last_state.dwPacketNumber)
		return;

	XINPUT_GAMEPAD& ogp = cd.last_state.Gamepad;
	const XINPUT_GAMEPAD& ngp = new_state.Gamepad;

#define CHECK_AXIS(field, axis, min_value, max_value) \
	if (ogp.field != ngp.field) \
	{ \
		InputManager::InvokeEvents(MakeGenericControllerAxisKey(InputSourceType::XInput, index, axis), \
			static_cast<float>(ngp.field) / ((ngp.field < 0) ? min_value : max_value)); \
	}

	// Y axes is inverted in XInput when compared to SDL.
	CHECK_AXIS(sThumbLX, AXIS_LEFTX, 32768, 32767);
	CHECK_AXIS(sThumbLY, AXIS_LEFTY, -32768, -32767);
	CHECK_AXIS(sThumbRX, AXIS_RIGHTX, 32768, 32767);
	CHECK_AXIS(sThumbRY, AXIS_RIGHTY, -32768, -32767);
	CHECK_AXIS(bLeftTrigger, AXIS_LEFTTRIGGER, 0, 255);
	CHECK_AXIS(bRightTrigger, AXIS_RIGHTTRIGGER, 0, 255);

#undef CHECK_AXIS

	const u16 old_button_bits = ogp.wButtons;
	const u16 new_button_bits = ngp.wButtons;
	if (old_button_bits != new_button_bits)
	{
		for (u32 button = 0; button < NUM_BUTTONS; button++)
		{
			const u16 button_mask = s_button_masks[button];
			if ((old_button_bits & button_mask) != (new_button_bits & button_mask))
			{
				const GenericInputBinding generic_key = (button < std::size(s_xinput_generic_binding_button_mapping)) ?
															s_xinput_generic_binding_button_mapping[button] :
                                                            GenericInputBinding::Unknown;
				const float value = ((new_button_bits & button_mask) != 0) ? 1.0f : 0.0f;
				InputManager::InvokeEvents(MakeGenericControllerButtonKey(InputSourceType::XInput, index, button), value, generic_key);
			}
		}
	}

	cd.last_state = new_state;
}

void XInputSource::CheckForStateChangesSCP(u32 index, const SCP_EXTN& new_state)
{
	ControllerData& cd = m_controllers[index];

	SCP_EXTN& ogp = cd.last_state_scp;
	const SCP_EXTN& ngp = new_state;

	s32 axis = 0, button = 0;

#define CHECK_AXIS(field, mult) \
	if (ogp.field != ngp.field) \
	{ \
		InputManager::InvokeEvents(MakeGenericControllerAxisKey(InputSourceType::XInput, index, axis), ngp.field* mult); \
	} \
	axis++;

#define CHECK_BUTTON(field) \
	if (ogp.field != ngp.field) \
	{ \
		InputManager::InvokeEvents(MakeGenericControllerButtonKey(InputSourceType::XInput, index, button), ngp.field); \
	} \
	button++;

	CHECK_AXIS(SCP_LX, 1.0f);
	CHECK_AXIS(SCP_LY, -1.0f);
	CHECK_AXIS(SCP_RX, 1.0f);
	CHECK_AXIS(SCP_RY, -1.0f);
	CHECK_AXIS(SCP_L2, 1.0f);
	CHECK_AXIS(SCP_R2, 1.0f);

	CHECK_BUTTON(SCP_UP);
	CHECK_BUTTON(SCP_DOWN);
	CHECK_BUTTON(SCP_LEFT);
	CHECK_BUTTON(SCP_RIGHT);

	CHECK_BUTTON(SCP_START);
	CHECK_BUTTON(SCP_SELECT);

	CHECK_BUTTON(SCP_L3);
	CHECK_BUTTON(SCP_R3);
	CHECK_BUTTON(SCP_L1);
	CHECK_BUTTON(SCP_R1);

	CHECK_BUTTON(SCP_X);
	CHECK_BUTTON(SCP_C);
	CHECK_BUTTON(SCP_S);
	CHECK_BUTTON(SCP_T);

	CHECK_BUTTON(SCP_PS);

#undef CHECK_BUTTON
#undef CHECK_AXIS

	cd.last_state_scp = new_state;
}

void XInputSource::UpdateMotorState(InputBindingKey key, float intensity)
{
	if (key.source_subtype != InputSubclass::ControllerMotor || key.source_index >= NUM_CONTROLLERS)
		return;

	ControllerData& cd = m_controllers[key.source_index];
	if (!cd.connected)
		return;

	const u16 i_intensity = static_cast<u16>(intensity * 65535.0f);
	if (key.data != 0)
		cd.last_vibration.wRightMotorSpeed = i_intensity;
	else
		cd.last_vibration.wLeftMotorSpeed = i_intensity;

	m_xinput_set_state(key.source_index, &cd.last_vibration);
}

void XInputSource::UpdateMotorState(InputBindingKey large_key, InputBindingKey small_key, float large_intensity, float small_intensity)
{
	if (large_key.source_index != small_key.source_index || large_key.source_subtype != InputSubclass::ControllerMotor ||
		small_key.source_subtype != InputSubclass::ControllerMotor)
	{
		// bonkers config where they're mapped to different controllers... who would do such a thing?
		UpdateMotorState(large_key, large_intensity);
		UpdateMotorState(small_key, small_intensity);
		return;
	}

	ControllerData& cd = m_controllers[large_key.source_index];
	if (!cd.connected)
		return;

	cd.last_vibration.wLeftMotorSpeed = static_cast<u16>(large_intensity * 65535.0f);
	cd.last_vibration.wRightMotorSpeed = static_cast<u16>(small_intensity * 65535.0f);
	m_xinput_set_state(large_key.source_index, &cd.last_vibration);
}
