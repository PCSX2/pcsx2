// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Input/InputSource.h"

#include <SDL3/SDL.h>

#include <array>
#include <functional>
#include <mutex>
#include <vector>

class SettingsInterface;

class SDLInputSource final : public InputSource
{
public:
	static constexpr u32 MAX_LED_COLORS = 4;

	SDLInputSource();
	~SDLInputSource();

	bool Initialize(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock) override;
	void UpdateSettings(SettingsInterface& si, std::unique_lock<std::mutex>& settings_lock) override;
	bool ReloadDevices() override;
	void Shutdown() override;
	bool IsInitialized() override;

	void PollEvents() override;
	std::vector<std::pair<std::string, std::string>> EnumerateDevices() override;
	std::vector<InputBindingKey> EnumerateMotors() override;
	bool GetGenericBindingMapping(const std::string_view device, InputManager::GenericInputBindingMapping* mapping) override;
	InputLayout GetControllerLayout(u32 index) override;
	void UpdateMotorState(InputBindingKey key, float intensity) override;
	void UpdateMotorState(InputBindingKey large_key, InputBindingKey small_key, float large_intensity, float small_intensity) override;

	std::optional<InputBindingKey> ParseKeyString(const std::string_view device, const std::string_view binding) override;
	TinyString ConvertKeyToString(InputBindingKey key, bool display = false, bool migration = false) override;
	TinyString ConvertKeyToIcon(InputBindingKey key) override;

	bool ProcessSDLEvent(const SDL_Event* event);

	SDL_Joystick* GetJoystickForDevice(const std::string_view device);

	static u32 GetRGBForPlayerId(SettingsInterface& si, u32 player_id);
	static u32 ParseRGBForPlayerId(const std::string_view str, u32 player_id);
	static void ResetRGBForAllPlayers(SettingsInterface& si);

private:
	struct ControllerData
	{
		SDL_Haptic* haptic;
		SDL_Gamepad* gamepad;
		SDL_Joystick* joystick;
		u16 rumble_intensity[2];
		int haptic_left_right_effect;
		SDL_JoystickID joystick_id;
		int player_id;
		bool use_gamepad_rumble;

		// Used to disable Joystick controls that are used in Gamepad inputs so we don't get double events
		std::vector<bool> joy_button_used_in_pad;
		std::vector<bool> joy_axis_used_in_pad;

		// Track last hat state so we can send "unpressed" events.
		std::vector<u8> last_hat_state;
	};

	using ControllerDataVector = std::vector<ControllerData>;

	bool InitializeSubsystem();
	void ShutdownSubsystem();
	void LoadSettings(SettingsInterface& si);
	void SetHints();

	ControllerDataVector::iterator GetControllerDataForJoystickId(SDL_JoystickID id);
	ControllerDataVector::iterator GetControllerDataForPlayerId(int id);
	int GetFreePlayerId() const;

	bool OpenDevice(SDL_JoystickID index, bool is_gamepad);
	bool CloseDevice(SDL_JoystickID joystick_index);
	bool HandleGamepadAxisEvent(const SDL_GamepadAxisEvent* ev);
	bool HandleGamepadButtonEvent(const SDL_GamepadButtonEvent* ev);
	bool HandleJoystickAxisEvent(const SDL_JoyAxisEvent* ev);
	bool HandleJoystickButtonEvent(const SDL_JoyButtonEvent* ev);
	bool HandleJoystickHatEvent(const SDL_JoyHatEvent* ev);
	void SendRumbleUpdate(ControllerData* cd);

	ControllerDataVector m_controllers;

	// ConvertKeyToString and ConvertKeyToIcon can inspect the
	// currently connected gamepad to provide matching labels
	// ParseKeyString can also inspect the gamepad for migrations
	// Those functions can be called on the main thread, while
	// gamepad addition/removal is done on the CPU thread
	std::mutex m_controllers_key_mutex;

	std::vector<u32> m_gamepads_needing_migration;

	std::array<u32, MAX_LED_COLORS> m_led_colors{};
	std::vector<std::pair<std::string, std::string>> m_sdl_hints;

	bool m_sdl_subsystem_initialized = false;
	bool m_enable_enhanced_reports = false;
	bool m_use_raw_input = false;
	bool m_enable_ps5_player_leds = false;

#ifdef __APPLE__
	bool m_enable_iokit_driver = false;
	bool m_enable_mfi_driver = false;
#endif
};
