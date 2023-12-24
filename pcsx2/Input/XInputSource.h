// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "Input/InputSource.h"

#include "common/RedtapeWindows.h"

#include <Xinput.h>
#include <array>
#include <functional>
#include <mutex>
#include <vector>

// SCP XInput extension
typedef struct
{
	float SCP_UP;
	float SCP_RIGHT;
	float SCP_DOWN;
	float SCP_LEFT;

	float SCP_LX;
	float SCP_LY;

	float SCP_L1;
	float SCP_L2;
	float SCP_L3;

	float SCP_RX;
	float SCP_RY;

	float SCP_R1;
	float SCP_R2;
	float SCP_R3;

	float SCP_T;
	float SCP_C;
	float SCP_X;
	float SCP_S;

	float SCP_SELECT;
	float SCP_START;

	float SCP_PS;
} SCP_EXTN;

class SettingsInterface;

class XInputSource final : public InputSource
{
public:
	enum : u32
	{
		NUM_CONTROLLERS = XUSER_MAX_COUNT, // 4
		NUM_BUTTONS = 15,
	};

	enum : u32
	{
		AXIS_LEFTX,
		AXIS_LEFTY,
		AXIS_RIGHTX,
		AXIS_RIGHTY,
		AXIS_LEFTTRIGGER,
		AXIS_RIGHTTRIGGER,
		NUM_AXES,
	};

	XInputSource();
	~XInputSource();

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
	TinyString ConvertKeyToString(InputBindingKey key) override;
	TinyString ConvertKeyToIcon(InputBindingKey key) override;

private:
	struct ControllerData
	{
		union
		{
			XINPUT_STATE last_state;
			SCP_EXTN last_state_scp;
		};
		XINPUT_VIBRATION last_vibration = {};
		bool connected = false;
		bool has_large_motor = false;
		bool has_small_motor = false;
	};

	using ControllerDataArray = std::array<ControllerData, NUM_CONTROLLERS>;

	void CheckForStateChanges(u32 index, const XINPUT_STATE& new_state);
	void CheckForStateChangesSCP(u32 index, const SCP_EXTN& new_state);
	void HandleControllerConnection(u32 index);
	void HandleControllerDisconnection(u32 index);

	ControllerDataArray m_controllers;

	HMODULE m_xinput_module{};
	DWORD(WINAPI* m_xinput_get_state)(DWORD, XINPUT_STATE*);
	DWORD(WINAPI* m_xinput_set_state)(DWORD, XINPUT_VIBRATION*);
	DWORD(WINAPI* m_xinput_get_capabilities)(DWORD, DWORD, XINPUT_CAPABILITIES*);
	DWORD(WINAPI* m_xinput_get_extended)(DWORD, SCP_EXTN*);
};
