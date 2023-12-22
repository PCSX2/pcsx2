// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#define DIRECTINPUT_VERSION 0x0800
#include "common/RedtapeWindows.h"
#include "common/RedtapeWilCom.h"
#include "Input/InputSource.h"
#include <array>
#include <dinput.h>
#include <functional>
#include <mutex>
#include <vector>

#include <wil/resource.h>

class DInputSource final : public InputSource
{
public:
	enum HAT_DIRECTION : u32
	{
		HAT_DIRECTION_UP = 0,
		HAT_DIRECTION_DOWN = 1,
		HAT_DIRECTION_LEFT = 2,
		HAT_DIRECTION_RIGHT = 3,
		NUM_HAT_DIRECTIONS = 4,
	};

	enum : u32
	{
		MAX_NUM_BUTTONS = 128,
	};

	DInputSource();
	~DInputSource() override;

	bool Initialize(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock) override;
	void UpdateSettings(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock) override;
	void LoadSettings(SettingsInterface& si);
	bool ReloadDevices() override;
	void Shutdown() override;

	void PollEvents() override;
	std::vector<std::pair<std::string, std::string>> EnumerateDevices() override;
	std::vector<InputBindingKey> EnumerateMotors() override;
	bool GetGenericBindingMapping(const std::string_view& device, InputManager::GenericInputBindingMapping* mapping) override;
	void UpdateMotorState(InputBindingKey key, float intensity) override;
	void UpdateMotorState(InputBindingKey large_key, InputBindingKey small_key, float large_intensity, float small_intensity) override;

	std::optional<InputBindingKey> ParseKeyString(const std::string_view& device, const std::string_view& binding) override;
	TinyString ConvertKeyToString(InputBindingKey key) override;
	TinyString ConvertKeyToIcon(InputBindingKey key) override;

private:
	struct ControllerData
	{
		wil::com_ptr_nothrow<IDirectInputDevice8W> device;
		DIJOYSTATE2 last_state = {};
		GUID guid = {};
		std::vector<u32> axis_offsets;
		u32 num_buttons = 0;

		// NOTE: We expose hats as num_buttons + (hat_index * 4) + direction.
		u32 num_hats = 0;

		bool needs_poll = true;
	};

	using ControllerDataArray = std::vector<ControllerData>;

	static std::array<bool, NUM_HAT_DIRECTIONS> GetHatButtons(DWORD hat);
	static std::string GetDeviceIdentifier(u32 index);

	bool AddDevice(ControllerData& cd, const std::string& name);

	void CheckForStateChanges(size_t index, const DIJOYSTATE2& new_state);

	// Those must go first in the class so they are destroyed last
	wil::unique_hmodule m_dinput_module;
	wil::com_ptr_nothrow<IDirectInput8W> m_dinput;
	HWND m_toplevel_window = nullptr;

	ControllerDataArray m_controllers;
	bool m_ignore_inversion = false;
};
