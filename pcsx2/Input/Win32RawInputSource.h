/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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
#include "common/RedtapeWindows.h"
#include "Input/InputSource.h"
#include <array>
#include <functional>
#include <mutex>
#include <vector>

class SettingsInterface;

class Win32RawInputSource final : public InputSource
{
public:
	Win32RawInputSource();
	~Win32RawInputSource();

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
	struct MouseState
	{
		HANDLE device;
		u32 button_state;
		s32 last_x;
		s32 last_y;
	};

	static bool RegisterDummyClass();
	static LRESULT CALLBACK DummyWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	static std::string GetMouseDeviceName(u32 index);

	bool CreateDummyWindow();
	void DestroyDummyWindow();
	bool OpenDevices();
	void CloseDevices();

	bool ProcessRawInputEvent(const RAWINPUT* event);

	HWND m_dummy_window = {};

	std::vector<MouseState> m_mice;
};
