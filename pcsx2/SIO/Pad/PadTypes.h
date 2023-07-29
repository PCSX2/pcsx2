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

#include "Config.h"

#include <optional>
#include <span>

namespace Pad
{
	enum class Command : u8
	{
		NOT_SET = 0x00,
		MYSTERY = 0x40,
		BUTTON_QUERY = 0x41,
		POLL = 0x42,
		CONFIG = 0x43,
		MODE_SWITCH = 0x44,
		STATUS_INFO = 0x45,
		CONST_1 = 0x46,
		CONST_2 = 0x47,
		CONST_3 = 0x4c,
		VIBRATION_MAP = 0x4d,
		RESPONSE_BYTES = 0x4f
	};

	enum class Mode : u8
	{
		NOT_SET = 0x00,
		PS1_MOUSE = 0x12,
		NEGCON = 0x23,
		PS1_KONAMI_LIGHTGUN = 0x31,
		DIGITAL = 0x41,
		PS1_FLIGHT_STICK = 0x53,
		PS1_NAMCO_LIGHTGUN = 0x63,
		ANALOG = 0x73,
		DUALSHOCK2 = 0x79,
		PS1_MULTITAP = 0x80,
		PS1_JOGCON = 0xe3,
		CONFIG = 0xf3,
		DISCONNECTED = 0xff
	};

	enum class PhysicalType : u8
	{
		NOT_SET = 0x00,
		GUITAR = 0x01,
		STANDARD = 0x03
	};

	enum class ResponseBytes : u32
	{
		DIGITAL = 0x00000000,
		ANALOG = 0x0000003f,
		DUALSHOCK2 = 0x0003ffff
	};

	static constexpr u8 ANALOG_NEUTRAL_POSITION = 0x7f;

	enum class ControllerType : u8
	{
		NotConnected,
		DualShock2,
		Guitar,
		Count
	};

	enum class VibrationCapabilities : u8
	{
		NoVibration,
		LargeSmallMotors,
		SingleMotor,
		Count
	};

	struct ControllerInfo
	{
		ControllerType type;
		const char* name;
		const char* display_name;
		std::span<const InputBindingInfo> bindings;
		std::span<const SettingInfo> settings;
		VibrationCapabilities vibration_caps;

		/// Returns localized controller type name.
		const char* GetLocalizedName() const;

		/// Returns the index of the specified binding point, by name.
		std::optional<u32> GetBindIndex(const std::string_view& name) const;
	};

	// Total number of pad ports, across both multitaps.
	static constexpr u32 NUM_CONTROLLER_PORTS = Pcsx2Config::PadOptions::NUM_PORTS;

	// Default stick deadzone/sensitivity.
	static constexpr float DEFAULT_STICK_DEADZONE = 0.0f;
	static constexpr float DEFAULT_STICK_SCALE = 1.33f;
	static constexpr float DEFAULT_TRIGGER_DEADZONE = 0.0f;
	static constexpr float DEFAULT_TRIGGER_SCALE = 1.0f;
	static constexpr float DEFAULT_MOTOR_SCALE = 1.0f;
	static constexpr float DEFAULT_PRESSURE_MODIFIER = 0.5f;
	static constexpr float DEFAULT_BUTTON_DEADZONE = 0.0f;

	// Number of macro buttons per controller.
	static constexpr u32 NUM_MACRO_BUTTONS_PER_CONTROLLER = 16;
} // namespace Pad
