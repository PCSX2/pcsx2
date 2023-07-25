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
#include "Host.h"

namespace Dualshock2
{
	enum Inputs
	{
		PAD_UP, // Directional pad up
		PAD_RIGHT, // Directional pad right
		PAD_DOWN, // Directional pad down
		PAD_LEFT, // Directional pad left
		PAD_TRIANGLE, // Triangle button 
		PAD_CIRCLE, // Circle button 
		PAD_CROSS, // Cross button 
		PAD_SQUARE, // Square button 
		PAD_SELECT, // Select button
		PAD_START, // Start button
		PAD_L1, // L1 button
		PAD_L2, // L2 button
		PAD_R1, // R1 button
		PAD_R2, // R2 button
		PAD_L3, // Left joystick button (L3)
		PAD_R3, // Right joystick button (R3)
		PAD_ANALOG, // Analog mode toggle
		PAD_PRESSURE, // Pressure modifier
		PAD_L_UP, // Left joystick (Up) 
		PAD_L_RIGHT, // Left joystick (Right) 
		PAD_L_DOWN, // Left joystick (Down) 
		PAD_L_LEFT, // Left joystick (Left) 
		PAD_R_UP, // Right joystick (Up) 
		PAD_R_RIGHT, // Right joystick (Right) 
		PAD_R_DOWN, // Right joystick (Down) 
		PAD_R_LEFT, // Right joystick (Left) 
		LENGTH,
	};

	static constexpr u32 PRESSURE_BUTTONS = 12;
	static constexpr u8 VIBRATION_MOTORS = 2;

	struct Analogs
	{
		u8 lx = 0x7f;
		u8 ly = 0x7f;
		u8 rx = 0x7f;
		u8 ry = 0x7f;
		u8 lxInvert = 0x7f;
		u8 lyInvert = 0x7f;
		u8 rxInvert = 0x7f;
		u8 ryInvert = 0x7f;
	};

	static const InputBindingInfo defaultBindings[] = {
		{"Up", TRANSLATE_NOOP("Pad", "D-Pad Up"), InputBindingInfo::Type::Button, Dualshock2::Inputs::PAD_UP, GenericInputBinding::DPadUp},
		{"Right", TRANSLATE_NOOP("Pad", "D-Pad Right"), InputBindingInfo::Type::Button, Dualshock2::Inputs::PAD_RIGHT, GenericInputBinding::DPadRight},
		{"Down", TRANSLATE_NOOP("Pad", "D-Pad Down"), InputBindingInfo::Type::Button, Dualshock2::Inputs::PAD_DOWN, GenericInputBinding::DPadDown},
		{"Left", TRANSLATE_NOOP("Pad", "D-Pad Left"), InputBindingInfo::Type::Button, Dualshock2::Inputs::PAD_LEFT, GenericInputBinding::DPadLeft},
		{"Triangle", TRANSLATE_NOOP("Pad", "Triangle"), InputBindingInfo::Type::Button, Dualshock2::Inputs::PAD_TRIANGLE, GenericInputBinding::Triangle},
		{"Circle", TRANSLATE_NOOP("Pad", "Circle"), InputBindingInfo::Type::Button, Dualshock2::Inputs::PAD_CIRCLE, GenericInputBinding::Circle},
		{"Cross", TRANSLATE_NOOP("Pad", "Cross"), InputBindingInfo::Type::Button, Dualshock2::Inputs::PAD_CROSS, GenericInputBinding::Cross},
		{"Square", TRANSLATE_NOOP("Pad", "Square"), InputBindingInfo::Type::Button, Dualshock2::Inputs::PAD_SQUARE, GenericInputBinding::Square},
		{"Select", TRANSLATE_NOOP("Pad", "Select"), InputBindingInfo::Type::Button, Dualshock2::Inputs::PAD_SELECT, GenericInputBinding::Select},
		{"Start", TRANSLATE_NOOP("Pad", "Start"), InputBindingInfo::Type::Button, Dualshock2::Inputs::PAD_START, GenericInputBinding::Start},
		{"L1", TRANSLATE_NOOP("Pad", "L1 (Left Bumper)"), InputBindingInfo::Type::Button, Dualshock2::Inputs::PAD_L1, GenericInputBinding::L1},
		{"L2", TRANSLATE_NOOP("Pad", "L2 (Left Trigger)"), InputBindingInfo::Type::HalfAxis, Dualshock2::Inputs::PAD_L2, GenericInputBinding::L2},
		{"R1", TRANSLATE_NOOP("Pad", "R1 (Right Bumper)"), InputBindingInfo::Type::Button, Dualshock2::Inputs::PAD_R1, GenericInputBinding::R1},
		{"R2", TRANSLATE_NOOP("Pad", "R2 (Right Trigger)"), InputBindingInfo::Type::HalfAxis, Dualshock2::Inputs::PAD_R2, GenericInputBinding::R2},
		{"L3", TRANSLATE_NOOP("Pad", "L3 (Left Stick Button)"), InputBindingInfo::Type::Button, Dualshock2::Inputs::PAD_L3, GenericInputBinding::L3},
		{"R3", TRANSLATE_NOOP("Pad", "R3 (Right Stick Button)"), InputBindingInfo::Type::Button, Dualshock2::Inputs::PAD_R3, GenericInputBinding::R3},
		{"Analog", TRANSLATE_NOOP("Pad", "Analog Toggle"), InputBindingInfo::Type::Button, Dualshock2::Inputs::PAD_ANALOG, GenericInputBinding::System},
		{"Pressure", TRANSLATE_NOOP("Pad", "Apply Pressure"), InputBindingInfo::Type::Button, Dualshock2::Inputs::PAD_PRESSURE, GenericInputBinding::Unknown},
		{"LUp", TRANSLATE_NOOP("Pad", "Left Stick Up"), InputBindingInfo::Type::HalfAxis, Dualshock2::Inputs::PAD_L_UP, GenericInputBinding::LeftStickUp},
		{"LRight", TRANSLATE_NOOP("Pad", "Left Stick Right"), InputBindingInfo::Type::HalfAxis, Dualshock2::Inputs::PAD_L_RIGHT, GenericInputBinding::LeftStickRight},
		{"LDown", TRANSLATE_NOOP("Pad", "Left Stick Down"), InputBindingInfo::Type::HalfAxis, Dualshock2::Inputs::PAD_L_DOWN, GenericInputBinding::LeftStickDown},
		{"LLeft", TRANSLATE_NOOP("Pad", "Left Stick Left"), InputBindingInfo::Type::HalfAxis, Dualshock2::Inputs::PAD_L_LEFT, GenericInputBinding::LeftStickLeft},
		{"RUp", TRANSLATE_NOOP("Pad", "Right Stick Up"), InputBindingInfo::Type::HalfAxis, Dualshock2::Inputs::PAD_R_UP, GenericInputBinding::RightStickUp},
		{"RRight", TRANSLATE_NOOP("Pad", "Right Stick Right"), InputBindingInfo::Type::HalfAxis, Dualshock2::Inputs::PAD_R_RIGHT, GenericInputBinding::RightStickRight},
		{"RDown", TRANSLATE_NOOP("Pad", "Right Stick Down"), InputBindingInfo::Type::HalfAxis, Dualshock2::Inputs::PAD_R_DOWN, GenericInputBinding::RightStickDown},
		{"RLeft", TRANSLATE_NOOP("Pad", "Right Stick Left"), InputBindingInfo::Type::HalfAxis, Dualshock2::Inputs::PAD_R_LEFT, GenericInputBinding::RightStickLeft},
		{"LargeMotor", TRANSLATE_NOOP("Pad", "Large (Low Frequency) Motor"), InputBindingInfo::Type::Motor, 0, GenericInputBinding::LargeMotor},
		{"SmallMotor", TRANSLATE_NOOP("Pad", "Small (High Frequency) Motor"), InputBindingInfo::Type::Motor, 0, GenericInputBinding::SmallMotor},
	};

	static const char* invertOptions[] = {
		"Not Inverted",
		"Invert Left/Right",
		"Invert Up/Down",
		"Invert Left/Right + Up/Down",
		nullptr};

	static const SettingInfo defaultSettings[] = {
		{SettingInfo::Type::IntegerList, "InvertL", TRANSLATE_NOOP("Pad", "Invert Left Stick"),
			TRANSLATE_NOOP("Pad", "Inverts the direction of the left analog stick."),
			"0", "0", "3", nullptr, nullptr, invertOptions, nullptr, 0.0f},
		{SettingInfo::Type::IntegerList, "InvertR", TRANSLATE_NOOP("Pad", "Invert Right Stick"),
			TRANSLATE_NOOP("Pad", "Inverts the direction of the right analog stick."),
			"0", "0", "3", nullptr, nullptr, invertOptions, nullptr, 0.0f},
		{SettingInfo::Type::Float, "Deadzone", TRANSLATE_NOOP("Pad", "Analog Deadzone"),
			TRANSLATE_NOOP("Pad", "Sets the analog stick deadzone, i.e. the fraction of the stick movement which will be ignored."),
			"0.00", "0.00", "1.00", "0.01", "%.0f%%", nullptr, nullptr, 100.0f},
		{SettingInfo::Type::Float, "AxisScale", TRANSLATE_NOOP("Pad", "Analog Sensitivity"),
			TRANSLATE_NOOP("Pad", "Sets the analog stick axis scaling factor. A value between 1.30 and 1.40 is recommended when using recent "
			"controllers, e.g. DualShock 4, Xbox One Controller."),
			"1.33", "0.01", "2.00", "0.01", "%.0f%%", nullptr, nullptr, 100.0f},
		{SettingInfo::Type::Float, "LargeMotorScale", TRANSLATE_NOOP("Pad", "Large Motor Vibration Scale"),
			TRANSLATE_NOOP("Pad", "Increases or decreases the intensity of low frequency vibration sent by the game."),
			"1.00", "0.00", "2.00", "0.01", "%.0f%%", nullptr, nullptr, 100.0f},
		{SettingInfo::Type::Float, "SmallMotorScale", TRANSLATE_NOOP("Pad", "Small Motor Vibration Scale"),
			TRANSLATE_NOOP("Pad", "Increases or decreases the intensity of high frequency vibration sent by the game."),
			"1.00", "0.00", "2.00", "0.01", "%.0f%%", nullptr, nullptr, 100.0f},
		{SettingInfo::Type::Float, "ButtonDeadzone", TRANSLATE_NOOP("Pad", "Button/Trigger Deadzone"),
			TRANSLATE_NOOP("Pad", "Sets the deadzone for activating buttons/triggers, i.e. the fraction of the trigger which will be ignored."),
			"0.00", "0.00", "1.00", "0.01", "%.0f%%", nullptr, nullptr, 100.0f},
		{SettingInfo::Type::Float, "PressureModifier", TRANSLATE_NOOP("Pad", "Modifier Pressure"),
			TRANSLATE_NOOP("Pad", "Sets the pressure when the modifier button is held."),
			"0.50", "0.01", "1.00", "0.01", "%.0f%%", nullptr, nullptr, 100.0f},
	};
} // namespace Dualshock2
