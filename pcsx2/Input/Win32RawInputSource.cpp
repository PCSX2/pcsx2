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

#include "PrecompiledHeader.h"

#include "Input/Win32RawInputSource.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/StringUtil.h"

#include "fmt/format.h"

#include <cmath>
#include <hidusage.h>
#include <hidsdi.h>
#include <malloc.h>

static const wchar_t* WINDOW_CLASS_NAME = L"Win32RawInputSource";
static bool s_window_class_registered = false;

static constexpr const u32 ALL_BUTTON_MASKS = RI_MOUSE_BUTTON_1_DOWN | RI_MOUSE_BUTTON_1_UP | RI_MOUSE_BUTTON_2_DOWN |
											  RI_MOUSE_BUTTON_2_UP | RI_MOUSE_BUTTON_3_DOWN | RI_MOUSE_BUTTON_3_UP |
											  RI_MOUSE_BUTTON_4_DOWN | RI_MOUSE_BUTTON_4_UP | RI_MOUSE_BUTTON_5_DOWN | RI_MOUSE_BUTTON_5_UP;

Win32RawInputSource::Win32RawInputSource() = default;

Win32RawInputSource::~Win32RawInputSource() = default;

bool Win32RawInputSource::Initialize(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock)
{
	if (!RegisterDummyClass())
	{
		Console.Error("(Win32RawInputSource) Failed to register dummy window class");
		return false;
	}

	if (!CreateDummyWindow())
	{
		Console.Error("(Win32RawInputSource) Failed to create dummy window");
		return false;
	}

	if (!OpenDevices())
	{
		Console.Error("(Win32RawInputSource) Failed to open devices");
		return false;
	}

	return true;
}

void Win32RawInputSource::UpdateSettings(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock)
{
}

bool Win32RawInputSource::ReloadDevices()
{
	CloseDevices();
	OpenDevices();
	return false;
}

void Win32RawInputSource::Shutdown()
{
	CloseDevices();
	DestroyDummyWindow();
}

void Win32RawInputSource::PollEvents()
{
	// noop, handled by message pump
}

std::vector<std::pair<std::string, std::string>> Win32RawInputSource::EnumerateDevices()
{
	std::vector<std::pair<std::string, std::string>> ret;

	u32 pointer_index = 0;
	for (const MouseState& ms : m_mice)
	{
		ret.emplace_back(InputManager::GetPointerDeviceName(pointer_index), GetMouseDeviceName(pointer_index));
		pointer_index++;
	}

	return ret;
}

void Win32RawInputSource::UpdateMotorState(InputBindingKey key, float intensity)
{
}

void Win32RawInputSource::UpdateMotorState(
	InputBindingKey large_key, InputBindingKey small_key, float large_intensity, float small_intensity)
{
}

std::optional<InputBindingKey> Win32RawInputSource::ParseKeyString(const std::string_view& device, const std::string_view& binding)
{
	return std::nullopt;
}

std::string Win32RawInputSource::ConvertKeyToString(InputBindingKey key)
{
	return {};
}

std::vector<InputBindingKey> Win32RawInputSource::EnumerateMotors()
{
	return {};
}

bool Win32RawInputSource::GetGenericBindingMapping(const std::string_view& device, InputManager::GenericInputBindingMapping* mapping)
{
	return {};
}

bool Win32RawInputSource::RegisterDummyClass()
{
	if (s_window_class_registered)
		return true;

	WNDCLASSW wc = {};
	wc.hInstance = GetModuleHandleW(nullptr);
	wc.lpfnWndProc = DummyWindowProc;
	wc.lpszClassName = WINDOW_CLASS_NAME;
	s_window_class_registered = (RegisterClassW(&wc) != 0);
	return s_window_class_registered;
}

bool Win32RawInputSource::CreateDummyWindow()
{
	m_dummy_window = CreateWindowExW(0, WINDOW_CLASS_NAME, WINDOW_CLASS_NAME, WS_OVERLAPPED, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
		CW_USEDEFAULT, HWND_MESSAGE, NULL, GetModuleHandleW(nullptr), NULL);
	if (!m_dummy_window)
		return false;

	SetWindowLongPtrW(m_dummy_window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
	return true;
}

void Win32RawInputSource::DestroyDummyWindow()
{
	if (!m_dummy_window)
		return;

	DestroyWindow(m_dummy_window);
	m_dummy_window = {};
}

LRESULT CALLBACK Win32RawInputSource::DummyWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg != WM_INPUT)
		return DefWindowProcW(hwnd, msg, wParam, lParam);

	UINT size = 0;
	GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER));

	PRAWINPUT data = static_cast<PRAWINPUT>(_alloca(size));
	GetRawInputData((HRAWINPUT)lParam, RID_INPUT, data, &size, sizeof(RAWINPUTHEADER));

	// we shouldn't get any WM_INPUT messages prior to SetWindowLongPtr(), so this'll be fine
	Win32RawInputSource* ris = reinterpret_cast<Win32RawInputSource*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
	if (ris->ProcessRawInputEvent(data))
		return 0;

	// forward through to normal message processing
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

std::string Win32RawInputSource::GetMouseDeviceName(u32 index)
{
#if 0
	// Doesn't work for mice :(
	const HANDLE device = m_mice[index].device;
	std::wstring wdevice_name;

	UINT size = 0;
	if (GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, nullptr, &size) == static_cast<UINT>(-1))
		goto error;

	wdevice_name.resize(size);

	UINT written_size = GetRawInputDeviceInfoW(device, RIDI_DEVICENAME, wdevice_name.data(), &size);
	if (written_size == static_cast<UINT>(-1))
		goto error;

	wdevice_name.resize(written_size);
	if (wdevice_name.empty())
		goto error;

	const HANDLE hFile = CreateFileW(wdevice_name.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		goto error;

	wchar_t product_string[256];
	if (!HidD_GetProductString(hFile, product_string, sizeof(product_string)))
	{
		CloseHandle(hFile);
		goto error;
	}

	CloseHandle(hFile);

	return StringUtil::WideStringToUTF8String(product_string);

error:
	return "Unknown Device";
#else
	return fmt::format("Raw Input Pointer {}", index);
#endif
}

bool Win32RawInputSource::OpenDevices()
{
	UINT num_devices = 0;
	if (GetRawInputDeviceList(nullptr, &num_devices, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1) || num_devices == 0)
		return false;

	std::vector<RAWINPUTDEVICELIST> devices(num_devices);
	if (GetRawInputDeviceList(devices.data(), &num_devices, sizeof(RAWINPUTDEVICELIST)) == static_cast<UINT>(-1))
		return false;
	devices.resize(num_devices);

	for (const RAWINPUTDEVICELIST& rid : devices)
	{
		if (rid.dwType == RIM_TYPEMOUSE)
			m_mice.push_back({rid.hDevice});
	}

	Console.WriteLn("(Win32RawInputSource) Found %zu mice", m_mice.size());

	// Grab all mouse input.
	if (!m_mice.empty())
	{
		const RAWINPUTDEVICE rrid = {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_MOUSE, 0, m_dummy_window};
		if (!RegisterRawInputDevices(&rrid, 1, sizeof(rrid)))
			return false;

		for (u32 i = 0; i < static_cast<u32>(m_mice.size()); i++)
			InputManager::OnInputDeviceConnected(InputManager::GetPointerDeviceName(i), GetMouseDeviceName(i));
	}

	return true;
}

void Win32RawInputSource::CloseDevices()
{
	if (!m_mice.empty())
	{
		const RAWINPUTDEVICE rrid = {HID_USAGE_PAGE_GENERIC, HID_USAGE_GENERIC_KEYBOARD, RIDEV_REMOVE, m_dummy_window};
		RegisterRawInputDevices(&rrid, 1, sizeof(rrid));

		for (u32 i = 0; i < static_cast<u32>(m_mice.size()); i++)
			InputManager::OnInputDeviceDisconnected(InputManager::GetPointerDeviceName(i));
		m_mice.clear();
	}
}

bool Win32RawInputSource::ProcessRawInputEvent(const RAWINPUT* event)
{
	if (event->header.dwType == RIM_TYPEMOUSE)
	{
		for (u32 pointer_index = 0; pointer_index < static_cast<u32>(m_mice.size()); pointer_index++)
		{
			MouseState& state = m_mice[pointer_index];
			if (state.device != event->header.hDevice)
				continue;

			const RAWMOUSE& rm = event->data.mouse;

			s32 dx = rm.lLastX;
			s32 dy = rm.lLastY;

			// handle absolute positioned devices
			if ((rm.usFlags & MOUSE_MOVE_ABSOLUTE) == MOUSE_MOVE_ABSOLUTE)
			{
				dx -= std::exchange(dx, state.last_x);
				dy -= std::exchange(dy, state.last_y);
			}

			unsigned long button_mask =
				(rm.usButtonFlags & (rm.usButtonFlags ^ std::exchange(state.button_state, rm.usButtonFlags))) & ALL_BUTTON_MASKS;

			while (button_mask != 0)
			{
				unsigned long bit_index;
				_BitScanForward(&bit_index, button_mask);

				// these are ordered down..up for each button
				const u32 button_number = bit_index >> 1;
				const bool button_pressed = (bit_index & 1u) == 0;
				InputManager::InvokeEvents(InputManager::MakePointerButtonKey(pointer_index, button_number),
					static_cast<float>(button_pressed), GenericInputBinding::Unknown);

				button_mask &= ~(1u << bit_index);
			}

			if (dx != 0)
				InputManager::UpdatePointerRelativeDelta(pointer_index, InputPointerAxis::X, static_cast<float>(dx), true);
			if (dy != 0)
				InputManager::UpdatePointerRelativeDelta(pointer_index, InputPointerAxis::Y, static_cast<float>(dy), true);

			return true;
		}
	}

	return false;
}
