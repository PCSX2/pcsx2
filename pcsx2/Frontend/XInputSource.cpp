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
#include "Frontend/XInputSource.h"
#include "Frontend/InputManager.h"
#include "common/Assertions.h"
#include "common/StringUtil.h"
#include "common/Console.h"
#include <cmath>

const char* XInputSource::s_axis_names[XInputSource::NUM_AXES] = {
	"LeftX", // AXIS_LEFTX
	"LeftY", // AXIS_LEFTY
	"RightX", // AXIS_RIGHTX
	"RightY", // AXIS_RIGHTY
	"LeftTrigger", // AXIS_TRIGGERLEFT
	"RightTrigger", // AXIS_TRIGGERRIGHT
};

const char* XInputSource::s_button_names[XInputSource::NUM_BUTTONS] = {
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
const u16 XInputSource::s_button_masks[XInputSource::NUM_BUTTONS] = {
	XINPUT_GAMEPAD_DPAD_UP,
	XINPUT_GAMEPAD_DPAD_DOWN,
	XINPUT_GAMEPAD_DPAD_LEFT,
	XINPUT_GAMEPAD_DPAD_RIGHT,
	XINPUT_GAMEPAD_START,
	XINPUT_GAMEPAD_BACK,
	XINPUT_GAMEPAD_LEFT_THUMB,
	XINPUT_GAMEPAD_RIGHT_THUMB,
	XINPUT_GAMEPAD_LEFT_SHOULDER,
	XINPUT_GAMEPAD_RIGHT_SHOULDER,
	XINPUT_GAMEPAD_A,
	XINPUT_GAMEPAD_B,
	XINPUT_GAMEPAD_X,
	XINPUT_GAMEPAD_Y,
	0x400, // XINPUT_GAMEPAD_GUIDE
};

XInputSource::XInputSource() = default;

XInputSource::~XInputSource() = default;

bool XInputSource::Initialize(SettingsInterface& si)
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
	m_xinput_module = LoadLibraryW(L"xinput1_4");
	if (!m_xinput_module)
	{
		m_xinput_module = LoadLibraryW(L"xinput1_3");
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
	m_xinput_get_state =
		reinterpret_cast<decltype(m_xinput_get_state)>(GetProcAddress(m_xinput_module, reinterpret_cast<LPCSTR>(100)));
	if (!m_xinput_get_state)
		reinterpret_cast<decltype(m_xinput_get_state)>(GetProcAddress(m_xinput_module, "XInputGetState"));
	m_xinput_set_state =
		reinterpret_cast<decltype(m_xinput_set_state)>(GetProcAddress(m_xinput_module, "XInputSetState"));
	m_xinput_get_capabilities =
		reinterpret_cast<decltype(m_xinput_get_capabilities)>(GetProcAddress(m_xinput_module, "XInputGetCapabilities"));
#else
	m_xinput_get_state = XInputGetState;
	m_xinput_set_state = XInputSetState;
	m_xinput_get_capabilities = XInputGetCapabilities;
#endif
	if (!m_xinput_get_state || !m_xinput_set_state || !m_xinput_get_capabilities)
	{
		Console.Error("Failed to get XInput function pointers.");
		return false;
	}

	return true;
}

void XInputSource::UpdateSettings(SettingsInterface& si)
{
}

void XInputSource::Shutdown()
{
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
	if (m_xinput_module)
	{
		FreeLibrary(m_xinput_module);
		m_xinput_module = nullptr;
	}
#endif

	m_xinput_get_state = nullptr;
	m_xinput_set_state = nullptr;
	m_xinput_get_capabilities = nullptr;
}

void XInputSource::PollEvents()
{
	for (u32 i = 0; i < NUM_CONTROLLERS; i++)
	{
		XINPUT_STATE new_state;
		const DWORD result = m_xinput_get_state(i, &new_state);
		const bool was_connected = m_controllers[i].connected;
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

std::optional<InputBindingKey> XInputSource::ParseKeyString(
	const std::string_view& device, const std::string_view& binding)
{
	if (!StringUtil::StartsWith(device, "XInput-") || binding.empty())
		return std::nullopt;

	const std::optional<s32> player_id = StringUtil::FromChars<s32>(device.substr(7));
	if (!player_id.has_value() || player_id.value() < 0)
		return std::nullopt;

	InputBindingKey key = {};
	key.source_type = InputSourceType::XInput;
	key.source_index = static_cast<u32>(player_id.value());

	if (StringUtil::EndsWith(binding, "Motor"))
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
				key.negative = (binding[0] == '-');
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

std::string XInputSource::ConvertKeyToString(InputBindingKey key)
{
	std::string ret;

	if (key.source_type == InputSourceType::XInput)
	{
		if (key.source_subtype == InputSubclass::ControllerAxis && key.data < std::size(s_axis_names))
		{
			ret = StringUtil::StdStringFromFormat(
				"XInput-%u/%c%s", key.source_index, key.negative ? '-' : '+', s_axis_names[key.data]);
		}
		else if (key.source_subtype == InputSubclass::ControllerButton && key.data < std::size(s_button_names))
		{
			ret = StringUtil::StdStringFromFormat("XInput-%u/%s", key.source_index, s_button_names[key.data]);
		}
		else if (key.source_subtype == InputSubclass::ControllerMotor)
		{
			ret = StringUtil::StdStringFromFormat("XInput-%u/%sMotor", key.source_index, key.data ? "Large" : "Small");
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
}

void XInputSource::HandleControllerDisconnection(u32 index)
{
	Console.WriteLn("XInput controller %u disconnected.", index);
	m_controllers[index] = {};
}

void XInputSource::CheckForStateChanges(u32 index, const XINPUT_STATE& new_state)
{
	ControllerData& cd = m_controllers[index];
	if (new_state.dwPacketNumber == cd.last_state.dwPacketNumber)
		return;

	cd.last_state.dwPacketNumber = new_state.dwPacketNumber;

	XINPUT_GAMEPAD& ogp = cd.last_state.Gamepad;
	const XINPUT_GAMEPAD& ngp = new_state.Gamepad;

#define CHECK_AXIS(field, axis, min_value, max_value) \
	if (ogp.field != ngp.field) \
	{ \
		InputManager::InvokeEvents( \
			MakeGenericControllerAxisKey(InputSourceType::XInput, index, axis), \
			static_cast<float>(ngp.field) / ((ngp.field < 0) ? min_value : max_value)); \
		ogp.field = ngp.field; \
	}

	CHECK_AXIS(sThumbLX, AXIS_LEFTX, -32768, 32767);
	CHECK_AXIS(sThumbLY, AXIS_LEFTY, -32768, 32767);
	CHECK_AXIS(sThumbRX, AXIS_RIGHTX, -32768, 32767);
	CHECK_AXIS(sThumbRY, AXIS_RIGHTY, -32768, 32767);
	CHECK_AXIS(bLeftTrigger, AXIS_LEFTTRIGGER, -128, 127);
	CHECK_AXIS(bRightTrigger, AXIS_RIGHTTRIGGER, -128, 127);

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
				InputManager::InvokeEvents(
					MakeGenericControllerButtonKey(InputSourceType::XInput, index, button),
					(new_button_bits & button_mask) != 0);
			}

			ogp.wButtons = ngp.wButtons;
		}
	}
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
