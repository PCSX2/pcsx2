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
#include "Input/InputSource.h"

#include "common/RedtapeWindows.h"

#include <array>
#include <vector>
#include <string>

class DualShock3InputSource final : public InputSource
{
public:
	DualShock3InputSource()	noexcept;
	~DualShock3InputSource() override;

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

	//Sixaxis driver commands.
	//All commands must be sent via WriteFile with 49-byte buffer containing output report.
	//Byte 0 indicates reportId and must always be 0.
	//Byte 1 indicates some command, supported values are specified below.
	enum SIXASIS_DRIVER_COMMANDS : u8
	{
		//This command allows to set user LEDs.
		//Bytes 5,6.7.8 contain mode for corresponding LED: 0 value means LED is OFF, 1 means LEDs in ON and 2 means LEDs is flashing.
		//Bytes 9-16 specify 64-bit LED flash period in 100 ns units if some LED is flashing, otherwise not used.
		SIXAXIS_COMMAND_SET_LEDS = 1,

		//This command allows to set left and right motors.
		//Byte 5 is right motor duration (0-255) and byte 6, if not zero, activates right motor. Zero value disables right motor.
		//Byte 7 is left motor duration (0-255) and byte 8 is left motor amplitude (0-255).
		SIXAXIS_COMMAND_SET_MOTORS = 2, 

		//This command allows to block/unblock setting device LEDs by applications.
		//Byte 5 is used as parameter - any non-zero value blocks LEDs, zero value will unblock LEDs.
		SIXAXIS_COMMAND_BLOCK_LEDS = 3,

		//This command refreshes driver settings. No parameters used.
		//When sixaxis driver loads it reads 'CurrentDriverSetting' binary value from 'HKLM\System\CurrentControlSet\Services\sixaxis\Parameters' registry key.
		//If the key is not present then default values are used. Sending this command forces sixaxis driver to re-read the registry and update driver settings.
		SIXAXIS_COMMAND_REFRESH_DRIVER_SETTING = 9,

		//This command clears current bluetooth pairing. No parameters used.
		SIXAXIS_COMMAND_CLEAR_PAIRING = 10
	};
	
	struct DS3ControllerData
	{
		int player_id;
		std::wstring device_path; 

		HANDLE hFile = INVALID_HANDLE_VALUE;		

		bool active = false;

		u8 SmallMotorOn = 0; // 0 or 1 (off/on)
		u8 LargeMotorForce = 0; // range [0, 255]
		
		std::array<bool, 17> physicalButtonState = {}; 
		std::array<bool, 17> lastPhysicalButtonState = {};

		//If we wever want to support pressure sensitive buttons
		//std::array<u8, 12> physicalButtonPressureState = {};
		//std::array<u8, 12> lastPhysicalButtonPressureState = {};

		std::array<u8, 4> physicalAxisState = {};
		std::array<u8, 4> lastPhysicalAxisState = {};		

	public:		
		bool Activate();
		void Deactivate();
	};		
	
	using DS3ControllerDataVector = std::vector<DS3ControllerData>;
	DS3ControllerDataVector m_controllers;

	float ConvertDS3Axis(u8 axis_val) const;

	DS3ControllerDataVector::iterator GetDS3ControllerDataForPlayerId(int id);
	int GetFreePlayerId() const; //borrowed from SDLInputSource
	
	static constexpr u16 DS3_VID = 0x054c; // Sony Corp.
	static constexpr u16 DS3_PID = 0x0268; // PlayStation 3 Controller

	static constexpr const char* DualShock3AxisNames[] = {
		"LeftX", 
		"LeftY", 
		"RightX", 
		"RightY" 
	};

	static constexpr GenericInputBinding DualShock3GenericAxisMapping[][2] = {
		{GenericInputBinding::LeftStickLeft, GenericInputBinding::LeftStickRight},
		{GenericInputBinding::LeftStickUp, GenericInputBinding::LeftStickDown}, 
		{GenericInputBinding::RightStickLeft, GenericInputBinding::RightStickRight}, 
		{GenericInputBinding::RightStickUp, GenericInputBinding::RightStickDown} 
	};

	static constexpr const char* DualShock3ButtonNames[] = {
		"Select",		
		"L3",
		"R3",
		"Start",
		"Up",
		"Right",
		"Down",
		"Left",
		"L2",
		"R2",
		"L1",
		"R1",
		"Triangle",
		"Circle",
		"Cross",
		"Square",
		"PS Button"
	};

	static constexpr GenericInputBinding DualShock3GenericButtonMapping[] = {
		GenericInputBinding::Select,
		GenericInputBinding::L3,		
		GenericInputBinding::R3,
		GenericInputBinding::Start,
		GenericInputBinding::DPadUp,
		GenericInputBinding::DPadRight,
		GenericInputBinding::DPadDown,
		GenericInputBinding::DPadLeft,
		GenericInputBinding::L2,
		GenericInputBinding::R2,
		GenericInputBinding::L1,
		GenericInputBinding::R1,
		GenericInputBinding::Triangle,
		GenericInputBinding::Circle,
		GenericInputBinding::Cross,
		GenericInputBinding::Square,		
		GenericInputBinding::System
	};
};
