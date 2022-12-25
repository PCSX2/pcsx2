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
#define DIRECTINPUT_VERSION 0x0800
#include "common/RedtapeWindows.h"
#include "common/RedtapeWilCom.h"
#include "Frontend/InputSource.h"
#include <array>
#include <dinput.h>
#include <functional>
#include <mutex>
#include <vector>

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
		MAX_NUM_BUTTONS = 32,
	};

	DInputSource();
	~DInputSource() override;

	bool Initialize(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock) override;
	void UpdateSettings(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock) override;
	bool ReloadDevices() override;
	void Shutdown() override;

	void PollEvents() override;
	std::vector<std::pair<std::string, std::string>> EnumerateDevices() override;
	std::vector<InputBindingKey> EnumerateMotors() override;
	bool GetGenericBindingMapping(const std::string_view& device, InputManager::GenericInputBindingMapping* mapping) override;
	void UpdateMotorState(InputBindingKey key, float intensity) override;
	void UpdateMotorState(InputBindingKey large_key, InputBindingKey small_key, float large_intensity, float small_intensity) override;

	std::optional<InputBindingKey> ParseKeyString(const std::string_view& device, const std::string_view& binding) override;
	std::string ConvertKeyToString(InputBindingKey key) override;

private:
	struct ControllerData
	{
		wil::com_ptr_nothrow<IDirectInputDevice8W> device;
		DIJOYSTATE last_state = {};
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

	void CheckForStateChanges(size_t index, const DIJOYSTATE& new_state);

	ControllerDataArray m_controllers;

	HMODULE m_dinput_module{};
	wil::com_ptr_nothrow<IDirectInput8W> m_dinput;
	LPCDIDATAFORMAT m_joystick_data_format{};
	HWND m_toplevel_window = NULL;
};
