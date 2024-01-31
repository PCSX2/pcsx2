// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#define INITGUID

#include "Input/DInputSource.h"
#include "Input/InputManager.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/StringUtil.h"

#include "fmt/format.h"
#include <cmath>
#include <limits>

using PFNDIRECTINPUT8CREATE = HRESULT(WINAPI*)(HINSTANCE hinst, DWORD dwVersion, REFIID riidltf, LPVOID* ppvOut, LPUNKNOWN punkOuter);
using PFNGETDFDIJOYSTICK = LPCDIDATAFORMAT(WINAPI*)();

DInputSource::DInputSource() = default;

DInputSource::~DInputSource() = default;

std::array<bool, DInputSource::NUM_HAT_DIRECTIONS> DInputSource::GetHatButtons(DWORD hat)
{
	std::array<bool, NUM_HAT_DIRECTIONS> buttons = {};

	const WORD hv = LOWORD(hat);
	if (hv != 0xFFFF)
	{
		if ((hv >= 0 && hv < 9000) || hv >= 31500)
			buttons[HAT_DIRECTION_UP] = true;
		if (hv >= 4500 && hv < 18000)
			buttons[HAT_DIRECTION_RIGHT] = true;
		if (hv >= 13500 && hv < 27000)
			buttons[HAT_DIRECTION_DOWN] = true;
		if (hv >= 22500)
			buttons[HAT_DIRECTION_LEFT] = true;
	}

	return buttons;
}

std::string DInputSource::GetDeviceIdentifier(u32 index)
{
	return fmt::format("DInput-{}", index);
}

static constexpr std::array<const char*, DInputSource::NUM_HAT_DIRECTIONS> s_hat_directions = {{"Up", "Down", "Left", "Right"}};

bool DInputSource::Initialize(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock)
{
	m_dinput_module.reset(LoadLibraryW(L"dinput8"));
	if (!m_dinput_module)
	{
		Console.Error("Failed to load DInput module.");
		return false;
	}

	PFNDIRECTINPUT8CREATE create = reinterpret_cast<PFNDIRECTINPUT8CREATE>(GetProcAddress(m_dinput_module.get(), "DirectInput8Create"));
	if (!create)
	{
		Console.Error("Failed to get DInput function pointers.");
		return false;
	}

	HRESULT hr =
		create(GetModuleHandleA(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8W, reinterpret_cast<LPVOID*>(m_dinput.put()), nullptr);
	if (FAILED(hr))
	{
		Console.Error("DirectInput8Create() failed: %08X", hr);
		return false;
	}

	// need to release the lock while we're enumerating, because we call winId().
	settings_lock.unlock();
	const std::optional<WindowInfo> toplevel_wi(Host::GetTopLevelWindowInfo());
	settings_lock.lock();

	if (!toplevel_wi.has_value() || toplevel_wi->type != WindowInfo::Type::Win32)
	{
		Console.Error("Missing top level window, cannot add DInput devices.");
		return false;
	}

	m_toplevel_window = static_cast<HWND>(toplevel_wi->window_handle);
	ReloadDevices();
	return true;
}

void DInputSource::UpdateSettings(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock)
{
}

static BOOL CALLBACK EnumCallback(LPCDIDEVICEINSTANCEW lpddi, LPVOID pvRef)
{
	static_cast<std::vector<DIDEVICEINSTANCEW>*>(pvRef)->push_back(*lpddi);
	return DIENUM_CONTINUE;
}

bool DInputSource::ReloadDevices()
{
	// detect any removals
	PollEvents();

	// look for new devices
	std::vector<DIDEVICEINSTANCEW> devices;
	m_dinput->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumCallback, &devices, DIEDFL_ATTACHEDONLY);

	DevCon.WriteLn("Enumerated %zu devices", devices.size());

	bool changed = false;
	for (DIDEVICEINSTANCEW inst : devices)
	{
		// do we already have this one?
		if (std::any_of(
				m_controllers.begin(), m_controllers.end(), [&inst](const ControllerData& cd) { return inst.guidInstance == cd.guid; }))
		{
			// yup, so skip it
			continue;
		}

		ControllerData cd;
		cd.guid = inst.guidInstance;
		HRESULT hr = m_dinput->CreateDevice(inst.guidInstance, cd.device.put(), nullptr);
		if (FAILED(hr))
		{
			Console.Warning("Failed to create instance of device [%s, %s]", inst.tszProductName, inst.tszInstanceName);
			continue;
		}

		const std::string name(StringUtil::WideStringToUTF8String(inst.tszProductName));
		if (AddDevice(cd, name))
		{
			const u32 index = static_cast<u32>(m_controllers.size());
			m_controllers.push_back(std::move(cd));
			InputManager::OnInputDeviceConnected(GetDeviceIdentifier(index), name);
			changed = true;
		}
	}

	return changed;
}

void DInputSource::Shutdown()
{
	while (!m_controllers.empty())
	{
		InputManager::OnInputDeviceDisconnected(GetDeviceIdentifier(static_cast<u32>(m_controllers.size() - 1)));
		m_controllers.pop_back();
	}
}

bool DInputSource::AddDevice(ControllerData& cd, const std::string& name)
{
	HRESULT hr = cd.device->SetCooperativeLevel(m_toplevel_window, DISCL_BACKGROUND | DISCL_EXCLUSIVE);
	if (FAILED(hr))
	{
		hr = cd.device->SetCooperativeLevel(m_toplevel_window, DISCL_BACKGROUND | DISCL_NONEXCLUSIVE);
		if (FAILED(hr))
		{
			Console.Error("Failed to set cooperative level for '%s'", name.c_str());
			return false;
		}

		Console.Warning("Failed to set exclusive mode for '%s'", name.c_str());
	}

	hr = cd.device->SetDataFormat(&c_dfDIJoystick2);
	if (FAILED(hr))
	{
		Console.Error("Failed to set data format for '%s'", name.c_str());
		return false;
	}

	hr = cd.device->Acquire();
	if (FAILED(hr))
	{
		Console.Error("Failed to acquire device '%s'", name.c_str());
		return false;
	}

	DIDEVCAPS caps = {};
	caps.dwSize = sizeof(caps);
	hr = cd.device->GetCapabilities(&caps);
	if (FAILED(hr))
	{
		Console.Error("Failed to get capabilities for '%s'", name.c_str());
		return false;
	}

	static constexpr const u32 axis_offsets[] = {offsetof(DIJOYSTATE2, lX), offsetof(DIJOYSTATE2, lY), offsetof(DIJOYSTATE2, lZ),
		offsetof(DIJOYSTATE2, lRz), offsetof(DIJOYSTATE2, lRx), offsetof(DIJOYSTATE2, lRy), offsetof(DIJOYSTATE2, rglSlider[0]),
		offsetof(DIJOYSTATE2, rglSlider[1])};
	for (const u32 offset : axis_offsets)
	{
		// ask for 16 bits of axis range
		DIPROPRANGE range = {};
		range.diph.dwSize = sizeof(range);
		range.diph.dwHeaderSize = sizeof(range.diph);
		range.diph.dwHow = DIPH_BYOFFSET;
		range.diph.dwObj = static_cast<DWORD>(offset);
		range.lMin = std::numeric_limits<s16>::min();
		range.lMax = std::numeric_limits<s16>::max();
		hr = cd.device->SetProperty(DIPROP_RANGE, &range.diph);

		// did it apply?
		if (SUCCEEDED(cd.device->GetProperty(DIPROP_RANGE, &range.diph)))
			cd.axis_offsets.push_back(offset);
	}

	hr = cd.device->Poll();
	if (hr == DI_NOEFFECT)
		cd.needs_poll = false;
	else if (hr != DI_OK)
		Console.Warning("Polling device '%s' failed: %08X", name.c_str(), hr);

	hr = cd.device->GetDeviceState(sizeof(cd.last_state), &cd.last_state);
	if (hr != DI_OK)
		Console.Warning("GetDeviceState() for '%s' failed: %08X", name.c_str(), hr);

	cd.num_buttons = std::min<u32>(caps.dwButtons, std::size(cd.last_state.rgbButtons));
	cd.num_hats = std::min<u32>(caps.dwPOVs, std::size(cd.last_state.rgdwPOV));

	Console.WriteLn(
		"%s has %u buttons, %u axes, %u hats", name.c_str(), cd.num_buttons, static_cast<u32>(cd.axis_offsets.size()), cd.num_hats);

	return (cd.num_buttons > 0 || !cd.axis_offsets.empty() || cd.num_hats > 0);
}

void DInputSource::PollEvents()
{
	for (size_t i = 0; i < m_controllers.size();)
	{
		ControllerData& cd = m_controllers[i];
		if (cd.needs_poll)
			cd.device->Poll();

		DIJOYSTATE2 js;
		HRESULT hr = cd.device->GetDeviceState(sizeof(js), &js);
		if (hr == DIERR_INPUTLOST || hr == DIERR_NOTACQUIRED)
		{
			hr = cd.device->Acquire();
			if (hr == DI_OK)
				hr = cd.device->GetDeviceState(sizeof(js), &js);

			if (hr != DI_OK)
			{
				InputManager::OnInputDeviceDisconnected(GetDeviceIdentifier(static_cast<u32>(i)));
				m_controllers.erase(m_controllers.begin() + i);
				continue;
			}
		}
		else if (hr != DI_OK)
		{
			Console.Warning("GetDeviceState() failed: %08X", hr);
			i++;
			continue;
		}

		CheckForStateChanges(i, js);
		i++;
	}
}

std::vector<std::pair<std::string, std::string>> DInputSource::EnumerateDevices()
{
	std::vector<std::pair<std::string, std::string>> ret;
	for (size_t i = 0; i < m_controllers.size(); i++)
	{
		DIDEVICEINSTANCEW dii = {sizeof(DIDEVICEINSTANCEW)};
		std::string name;
		if (SUCCEEDED(m_controllers[i].device->GetDeviceInfo(&dii)))
			name = StringUtil::WideStringToUTF8String(dii.tszProductName);

		if (name.empty())
			name = "Unknown";

		ret.emplace_back(GetDeviceIdentifier(static_cast<u32>(i)), std::move(name));
	}

	return ret;
}

std::vector<InputBindingKey> DInputSource::EnumerateMotors()
{
	return {};
}

bool DInputSource::GetGenericBindingMapping(const std::string_view& device, InputManager::GenericInputBindingMapping* mapping)
{
	return {};
}

void DInputSource::UpdateMotorState(InputBindingKey key, float intensity)
{
	// not supported
}

void DInputSource::UpdateMotorState(InputBindingKey large_key, InputBindingKey small_key, float large_intensity, float small_intensity)
{
	// not supported
}

std::optional<InputBindingKey> DInputSource::ParseKeyString(const std::string_view& device, const std::string_view& binding)
{
	if (!device.starts_with("DInput-") || binding.empty())
		return std::nullopt;

	const std::optional<s32> player_id = StringUtil::FromChars<s32>(device.substr(7));
	if (!player_id.has_value() || player_id.value() < 0)
		return std::nullopt;

	InputBindingKey key = {};
	key.source_type = InputSourceType::DInput;
	key.source_index = static_cast<u32>(player_id.value());

	if (binding.starts_with("+Axis") || binding.starts_with("-Axis"))
	{
		std::string_view end;
		const std::optional<u32> axis_index = StringUtil::FromChars<u32>(binding.substr(5), 10, &end);
		if (!axis_index.has_value())
			return std::nullopt;

		key.source_subtype = InputSubclass::ControllerAxis;
		key.data = axis_index.value();
		key.modifier = (binding[0] == '-') ? InputModifier::Negate : InputModifier::None;
		key.invert = (end == "~");
		return key;
	}
	else if (binding.starts_with("FullAxis"))
	{
		std::string_view end;
		const std::optional<u32> axis_index = StringUtil::FromChars<u32>(binding.substr(8), 10, &end);
		if (!axis_index.has_value())
			return std::nullopt;

		key.source_subtype = InputSubclass::ControllerAxis;
		key.data = axis_index.value();
		key.modifier = InputModifier::FullAxis;
		key.invert = (end == "~");
		return key;
	}
	else if (binding.starts_with("Hat"))
	{
		if (binding[3] < '0' || binding[3] > '9' || binding.length() < 5)
			return std::nullopt;

		const u32 hat_index = binding[3] - '0';
		const std::string_view hat_dir(binding.substr(4));
		for (u32 i = 0; i < NUM_HAT_DIRECTIONS; i++)
		{
			if (hat_dir == s_hat_directions[i])
			{
				key.source_subtype = InputSubclass::ControllerButton;
				key.data = MAX_NUM_BUTTONS + hat_index * NUM_HAT_DIRECTIONS + i;
				return key;
			}
		}

		// bad direction
		return std::nullopt;
	}
	else if (binding.starts_with("Button"))
	{
		const std::optional<u32> button_index = StringUtil::FromChars<u32>(binding.substr(6));
		if (!button_index.has_value())
			return std::nullopt;

		key.source_subtype = InputSubclass::ControllerButton;
		key.data = button_index.value();
		return key;
	}

	// unknown axis/button
	return std::nullopt;
}

TinyString DInputSource::ConvertKeyToString(InputBindingKey key)
{
	TinyString ret;

	if (key.source_type == InputSourceType::DInput)
	{
		if (key.source_subtype == InputSubclass::ControllerAxis)
		{
			const char* modifier = (key.modifier == InputModifier::FullAxis ? "Full" : (key.modifier == InputModifier::Negate ? "-" : "+"));
			ret.fmt("DInput-{}/{}Axis{}{}", u32(key.source_index), modifier, u32(key.data), (key.invert && !ShouldIgnoreInversion()) ? "~" : "");
		}
		else if (key.source_subtype == InputSubclass::ControllerButton && key.data >= MAX_NUM_BUTTONS)
		{
			const u32 hat_num = (key.data - MAX_NUM_BUTTONS) / NUM_HAT_DIRECTIONS;
			const u32 hat_dir = (key.data - MAX_NUM_BUTTONS) % NUM_HAT_DIRECTIONS;
			ret.fmt("DInput-{}/Hat{}{}", u32(key.source_index), hat_num, s_hat_directions[hat_dir]);
		}
		else if (key.source_subtype == InputSubclass::ControllerButton)
		{
			ret.fmt("DInput-{}/Button{}", u32(key.source_index), u32(key.data));
		}
	}

	return ret;
}


TinyString DInputSource::ConvertKeyToIcon(InputBindingKey key)
{
	return {};
}

void DInputSource::CheckForStateChanges(size_t index, const DIJOYSTATE2& new_state)
{
	ControllerData& cd = m_controllers[index];
	DIJOYSTATE2& last_state = cd.last_state;

	for (size_t i = 0; i < cd.axis_offsets.size(); i++)
	{
		LONG new_value;
		LONG old_value;
		std::memcpy(&old_value, reinterpret_cast<const u8*>(&cd.last_state) + cd.axis_offsets[i], sizeof(old_value));
		std::memcpy(&new_value, reinterpret_cast<const u8*>(&new_state) + cd.axis_offsets[i], sizeof(new_value));
		if (old_value != new_value)
		{
			std::memcpy(reinterpret_cast<u8*>(&cd.last_state) + cd.axis_offsets[i], &new_value, sizeof(new_value));

			// TODO: Use the range from caps?
			const float value = static_cast<float>(new_value) / (new_value < 0 ? 32768.0f : 32767.0f);
			InputManager::InvokeEvents(MakeGenericControllerAxisKey(InputSourceType::DInput, static_cast<u32>(index), static_cast<u32>(i)),
				value, GenericInputBinding::Unknown);
		}
	}

	for (u32 i = 0; i < cd.num_buttons; i++)
	{
		if (last_state.rgbButtons[i] != new_state.rgbButtons[i])
		{
			last_state.rgbButtons[i] = new_state.rgbButtons[i];

			const float value = (new_state.rgbButtons[i] != 0) ? 1.0f : 0.0f;
			InputManager::InvokeEvents(
				MakeGenericControllerButtonKey(InputSourceType::DInput, static_cast<u32>(index), i), value, GenericInputBinding::Unknown);
		}
	}

	for (u32 i = 0; i < cd.num_hats; i++)
	{
		if (last_state.rgdwPOV[i] != new_state.rgdwPOV[i])
		{
			// map hats to the last buttons
			const std::array<bool, NUM_HAT_DIRECTIONS> old_buttons(GetHatButtons(last_state.rgdwPOV[i]));
			const std::array<bool, NUM_HAT_DIRECTIONS> new_buttons(GetHatButtons(new_state.rgdwPOV[i]));
			last_state.rgdwPOV[i] = new_state.rgdwPOV[i];

			for (u32 j = 0; j < NUM_HAT_DIRECTIONS; j++)
			{
				if (old_buttons[j] != new_buttons[j])
				{
					const float value = (new_buttons[j] ? 1.0f : 0.0f);
					InputManager::InvokeEvents(MakeGenericControllerButtonKey(InputSourceType::DInput, static_cast<u32>(index),
												   cd.num_buttons + (i * NUM_HAT_DIRECTIONS) + j),
						value, GenericInputBinding::Unknown);
				}
			}
		}
	}
}
