// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SIO/Pad/PadDualshock2.h"
#include "SIO/Pad/Pad.h"
#include "SIO/Sio.h"
#include "SIO/Sio0.h"

#include "Common.h"
#include "Input/InputManager.h"
#include "Host.h"

#include "IconsPromptFont.h"

static const InputBindingInfo s_bindings[] = {
	// clang-format off
	{"Up", TRANSLATE_NOOP("Pad", "D-Pad Up"), ICON_PF_DPAD_UP, InputBindingInfo::Type::Button, PadDualshock2::Inputs::PAD_UP, GenericInputBinding::DPadUp},
	{"Right", TRANSLATE_NOOP("Pad", "D-Pad Right"), ICON_PF_DPAD_RIGHT, InputBindingInfo::Type::Button, PadDualshock2::Inputs::PAD_RIGHT, GenericInputBinding::DPadRight},
	{"Down", TRANSLATE_NOOP("Pad", "D-Pad Down"), ICON_PF_DPAD_DOWN, InputBindingInfo::Type::Button, PadDualshock2::Inputs::PAD_DOWN, GenericInputBinding::DPadDown},
	{"Left", TRANSLATE_NOOP("Pad", "D-Pad Left"), ICON_PF_DPAD_LEFT, InputBindingInfo::Type::Button, PadDualshock2::Inputs::PAD_LEFT, GenericInputBinding::DPadLeft},
	{"Triangle", TRANSLATE_NOOP("Pad", "Triangle"), ICON_PF_BUTTON_TRIANGLE, InputBindingInfo::Type::Button, PadDualshock2::Inputs::PAD_TRIANGLE, GenericInputBinding::Triangle},
	{"Circle", TRANSLATE_NOOP("Pad", "Circle"), ICON_PF_BUTTON_CIRCLE, InputBindingInfo::Type::Button, PadDualshock2::Inputs::PAD_CIRCLE, GenericInputBinding::Circle},
	{"Cross", TRANSLATE_NOOP("Pad", "Cross"), ICON_PF_BUTTON_CROSS, InputBindingInfo::Type::Button, PadDualshock2::Inputs::PAD_CROSS, GenericInputBinding::Cross},
	{"Square", TRANSLATE_NOOP("Pad", "Square"), ICON_PF_BUTTON_SQUARE, InputBindingInfo::Type::Button, PadDualshock2::Inputs::PAD_SQUARE, GenericInputBinding::Square},
	{"Select", TRANSLATE_NOOP("Pad", "Select"), ICON_PF_SELECT_SHARE, InputBindingInfo::Type::Button, PadDualshock2::Inputs::PAD_SELECT, GenericInputBinding::Select},
	{"Start", TRANSLATE_NOOP("Pad", "Start"), ICON_PF_START, InputBindingInfo::Type::Button, PadDualshock2::Inputs::PAD_START, GenericInputBinding::Start},
	{"L1", TRANSLATE_NOOP("Pad", "L1 (Left Bumper)"), ICON_PF_LEFT_SHOULDER_L1, InputBindingInfo::Type::Button, PadDualshock2::Inputs::PAD_L1, GenericInputBinding::L1},
	{"L2", TRANSLATE_NOOP("Pad", "L2 (Left Trigger)"), ICON_PF_LEFT_TRIGGER_L2, InputBindingInfo::Type::HalfAxis, PadDualshock2::Inputs::PAD_L2, GenericInputBinding::L2},
	{"R1", TRANSLATE_NOOP("Pad", "R1 (Right Bumper)"), ICON_PF_RIGHT_SHOULDER_R1, InputBindingInfo::Type::Button, PadDualshock2::Inputs::PAD_R1, GenericInputBinding::R1},
	{"R2", TRANSLATE_NOOP("Pad", "R2 (Right Trigger)"), ICON_PF_RIGHT_TRIGGER_R2, InputBindingInfo::Type::HalfAxis, PadDualshock2::Inputs::PAD_R2, GenericInputBinding::R2},
	{"L3", TRANSLATE_NOOP("Pad", "L3 (Left Stick Button)"), ICON_PF_LEFT_ANALOG_CLICK, InputBindingInfo::Type::Button, PadDualshock2::Inputs::PAD_L3, GenericInputBinding::L3},
	{"R3", TRANSLATE_NOOP("Pad", "R3 (Right Stick Button)"), ICON_PF_RIGHT_ANALOG_CLICK, InputBindingInfo::Type::Button, PadDualshock2::Inputs::PAD_R3, GenericInputBinding::R3},
	{"Analog", TRANSLATE_NOOP("Pad", "Analog Toggle"), ICON_PF_ANALOG_LEFT_RIGHT, InputBindingInfo::Type::Button, PadDualshock2::Inputs::PAD_ANALOG, GenericInputBinding::System},
	{"Pressure", TRANSLATE_NOOP("Pad", "Apply Pressure"), ICON_PF_ANALOG_ANY, InputBindingInfo::Type::Button, PadDualshock2::Inputs::PAD_PRESSURE, GenericInputBinding::Unknown},
	{"LUp", TRANSLATE_NOOP("Pad", "Left Stick Up"), ICON_PF_LEFT_ANALOG_UP, InputBindingInfo::Type::HalfAxis, PadDualshock2::Inputs::PAD_L_UP, GenericInputBinding::LeftStickUp},
	{"LRight", TRANSLATE_NOOP("Pad", "Left Stick Right"), ICON_PF_LEFT_ANALOG_RIGHT, InputBindingInfo::Type::HalfAxis, PadDualshock2::Inputs::PAD_L_RIGHT, GenericInputBinding::LeftStickRight},
	{"LDown", TRANSLATE_NOOP("Pad", "Left Stick Down"), ICON_PF_LEFT_ANALOG_DOWN, InputBindingInfo::Type::HalfAxis, PadDualshock2::Inputs::PAD_L_DOWN, GenericInputBinding::LeftStickDown},
	{"LLeft", TRANSLATE_NOOP("Pad", "Left Stick Left"), ICON_PF_LEFT_ANALOG_LEFT, InputBindingInfo::Type::HalfAxis, PadDualshock2::Inputs::PAD_L_LEFT, GenericInputBinding::LeftStickLeft},
	{"RUp", TRANSLATE_NOOP("Pad", "Right Stick Up"), ICON_PF_RIGHT_ANALOG_UP, InputBindingInfo::Type::HalfAxis, PadDualshock2::Inputs::PAD_R_UP, GenericInputBinding::RightStickUp},
	{"RRight", TRANSLATE_NOOP("Pad", "Right Stick Right"), ICON_PF_RIGHT_ANALOG_RIGHT, InputBindingInfo::Type::HalfAxis, PadDualshock2::Inputs::PAD_R_RIGHT, GenericInputBinding::RightStickRight},
	{"RDown", TRANSLATE_NOOP("Pad", "Right Stick Down"), ICON_PF_RIGHT_ANALOG_DOWN, InputBindingInfo::Type::HalfAxis, PadDualshock2::Inputs::PAD_R_DOWN, GenericInputBinding::RightStickDown},
	{"RLeft", TRANSLATE_NOOP("Pad", "Right Stick Left"), ICON_PF_RIGHT_ANALOG_LEFT, InputBindingInfo::Type::HalfAxis, PadDualshock2::Inputs::PAD_R_LEFT, GenericInputBinding::RightStickLeft},
	{"LargeMotor", TRANSLATE_NOOP("Pad", "Large (Low Frequency) Motor"), nullptr, InputBindingInfo::Type::Motor, 0, GenericInputBinding::LargeMotor},
	{"SmallMotor", TRANSLATE_NOOP("Pad", "Small (High Frequency) Motor"), nullptr, InputBindingInfo::Type::Motor, 0, GenericInputBinding::SmallMotor},
	// clang-format on
};

static const char* s_invert_options[] = {TRANSLATE_NOOP("Pad", "Not Inverted"),
	TRANSLATE_NOOP("Pad", "Invert Left/Right"), TRANSLATE_NOOP("Pad", "Invert Up/Down"),
	TRANSLATE_NOOP("Pad", "Invert Left/Right + Up/Down"), nullptr};

static const SettingInfo s_settings[] = {
	{SettingInfo::Type::IntegerList, "InvertL", TRANSLATE_NOOP("Pad", "Invert Left Stick"),
		TRANSLATE_NOOP("Pad", "Inverts the direction of the left analog stick."), "0", "0", "3", nullptr, nullptr,
		s_invert_options, nullptr, 0.0f},
	{SettingInfo::Type::IntegerList, "InvertR", TRANSLATE_NOOP("Pad", "Invert Right Stick"),
		TRANSLATE_NOOP("Pad", "Inverts the direction of the right analog stick."), "0", "0", "3", nullptr, nullptr,
		s_invert_options, nullptr, 0.0f},
	{SettingInfo::Type::Float, "Deadzone", TRANSLATE_NOOP("Pad", "Analog Deadzone"),
		TRANSLATE_NOOP(
			"Pad", "Sets the analog stick deadzone, i.e. the fraction of the stick movement which will be ignored."),
		"0.00", "0.00", "1.00", "0.01", TRANSLATE_NOOP("Pad", "%.0f%%"), nullptr, nullptr, 100.0f},
	{SettingInfo::Type::Float, "AxisScale", TRANSLATE_NOOP("Pad", "Analog Sensitivity"),
		TRANSLATE_NOOP("Pad",
			"Sets the analog stick axis scaling factor. A value between 130% and 140% is recommended when using recent "
			"controllers, e.g. DualShock 4, Xbox One Controller."),
		"1.33", "0.01", "2.00", "0.01", TRANSLATE_NOOP("Pad", "%.0f%%"), nullptr, nullptr, 100.0f},
	{SettingInfo::Type::Float, "LargeMotorScale", TRANSLATE_NOOP("Pad", "Large Motor Vibration Scale"),
		TRANSLATE_NOOP("Pad", "Increases or decreases the intensity of low frequency vibration sent by the game."),
		"1.00", "0.00", "2.00", "0.01", TRANSLATE_NOOP("Pad", "%.0f%%"), nullptr, nullptr, 100.0f},
	{SettingInfo::Type::Float, "SmallMotorScale", TRANSLATE_NOOP("Pad", "Small Motor Vibration Scale"),
		TRANSLATE_NOOP("Pad", "Increases or decreases the intensity of high frequency vibration sent by the game."),
		"1.00", "0.00", "2.00", "0.01", TRANSLATE_NOOP("Pad", "%.0f%%"), nullptr, nullptr, 100.0f},
	{SettingInfo::Type::Float, "ButtonDeadzone", TRANSLATE_NOOP("Pad", "Button/Trigger Deadzone"),
		TRANSLATE_NOOP("Pad", "Sets the deadzone for activating buttons/triggers, i.e. the fraction of the trigger "
							  "which will be ignored."),
		"0.00", "0.00", "1.00", "0.01", TRANSLATE_NOOP("Pad", "%.0f%%"), nullptr, nullptr, 100.0f},
	{SettingInfo::Type::Float, "PressureModifier", TRANSLATE_NOOP("Pad", "Modifier Pressure"),
		TRANSLATE_NOOP("Pad", "Sets the pressure when the modifier button is held."), "0.50", "0.01", "1.00", "0.01",
		TRANSLATE_NOOP("Pad", "%.0f%%"), nullptr, nullptr, 100.0f},
};

const Pad::ControllerInfo PadDualshock2::ControllerInfo = {Pad::ControllerType::DualShock2, "DualShock2",
	TRANSLATE_NOOP("Pad", "DualShock 2"), ICON_PF_GAMEPAD, s_bindings, s_settings, Pad::VibrationCapabilities::LargeSmallMotors};

void PadDualshock2::ConfigLog()
{
	const auto [port, slot] = sioConvertPadToPortAndSlot(unifiedSlot);
	std::string_view smallMotorStr;

	switch (this->smallMotorLastConfig)
	{
		case 0xff:
			smallMotorStr = "Disabled";
			break;
		case 0x00:
			smallMotorStr = "Normal";
			break;
		case 0x01:
			smallMotorStr = "Inverted";
			break;
		default:
			smallMotorStr = "Undefined";
			break;
	}

	std::string_view largeMotorStr;

	switch (this->largeMotorLastConfig)
	{
		case 0xff:
			largeMotorStr = "Disabled";
			break;
		case 0x00:
			largeMotorStr = "Inverted";
			break;
		case 0x01:
			largeMotorStr = "Normal";
			break;
		default:
			largeMotorStr = "Undefined";
			break;
	}

	std::string_view pressureStr;

	switch (this->responseBytes)
	{
		case static_cast<u32>(Pad::ResponseBytes::DUALSHOCK2):
			pressureStr = "D+A+P";
			break;
		case static_cast<u32>(Pad::ResponseBytes::ANALOG):
			pressureStr = "D+A";
			break;
		case static_cast<u32>(Pad::ResponseBytes::DIGITAL):
			pressureStr = "D";
			break;
		default:
			pressureStr = "U";
			break;
	}

	// AL: Analog Light (is it turned on right now)
	// AB: Analog Button (is it useable or is it locked in its current state)
	// VS: Vibration Small (how is the small vibration motor mapped)
	// VL: Vibration Large (how is the large vibration motor mapped)
	// RB: Response Bytes (what data is included in the controller's responses - D = Digital, A = Analog, P = Pressure)
	Console.WriteLn(fmt::format("[Pad] DS2 Config Finished - P{0}/S{1} - AL: {2} - AB: {3} - VS: {4} - VL: {5} - RB: {6} (0x{7:08X})",
		port + 1,
		slot + 1,
		(this->analogLight ? "On" : "Off"),
		(this->analogLocked ? "Locked" : "Usable"),
		smallMotorStr,
		largeMotorStr,
		pressureStr,
		this->responseBytes));
}

u8 PadDualshock2::Mystery(u8 commandByte)
{
	switch (commandBytesReceived)
	{
		case 5:
			return 0x02;
		case 8:
			return 0x5a;
		default:
			return 0x00;
	}
}

u8 PadDualshock2::ButtonQuery(u8 commandByte)
{
	switch (this->currentMode)
	{
		case Pad::Mode::DUALSHOCK2:
		case Pad::Mode::ANALOG:
			switch (commandBytesReceived)
			{
				case 3:
				case 4:
					return 0xff;
				case 5:
					return 0x03;
				case 8:
					g_Sio0.SetAcknowledge(false);
					return 0x5a;
				default:
					return 0x00;
			}
		default:
			switch (commandBytesReceived)
			{
				case 8:
					g_Sio0.SetAcknowledge(false);
					return 0x00;
				default:
					return 0x00;
			}
	}
}

u8 PadDualshock2::Poll(u8 commandByte)
{
	const u32 buttons = GetButtons();
	u8 largeMotor = 0x00;
	u8 smallMotor = 0x00;

	switch (commandBytesReceived)
	{
		case 3:
			this->vibrationMotors[0] = commandByte;
			return (buttons >> 8) & 0xff;
		case 4:
			this->vibrationMotors[1] = commandByte;

			// Apply the vibration mapping to the motors
			switch (this->largeMotorLastConfig)
			{
				case 0x00:
					largeMotor = this->vibrationMotors[0];
					break;
				case 0x01:
					largeMotor = this->vibrationMotors[1];
					break;
				default:
					break;
			}

			// Small motor on the controller is only controlled by the LSB.
			// Any value can be sent by the software, but only odd numbers
			// (LSB set) will turn on the motor.
			switch (this->smallMotorLastConfig)
			{
				case 0x00:
					smallMotor = this->vibrationMotors[0] & 0x01;
					break;
				case 0x01:
					smallMotor = this->vibrationMotors[1] & 0x01;
					break;
				default:
					break;
			}

			// Order is reversed here - SetPadVibrationIntensity takes large motor first, then small. PS2 orders small motor first, large motor second.
			InputManager::SetPadVibrationIntensity(this->unifiedSlot,
				std::min(static_cast<float>(largeMotor) * GetVibrationScale(1) * (1.0f / 255.0f), 1.0f),
				// Small motor on the PS2 is either on full power or zero power, it has no variable speed. If the game supplies any value here at all,
				// the pad in turn supplies full power to the motor, or no power at all if zero.
				std::min(static_cast<float>((smallMotor ? 0xff : 0)) * GetVibrationScale(0) * (1.0f / 255.0f), 1.0f)
			);

			// PS1 mode: If the controller is still in digital mode, it is time to stop acknowledging.
			if (this->currentMode == Pad::Mode::DIGITAL)
			{
				g_Sio0.SetAcknowledge(false);
			}

			return buttons & 0xff;
		case 5:
			return GetPressure(Inputs::PAD_R_RIGHT);
		case 6:
			return GetPressure(Inputs::PAD_R_UP);
		case 7:
			return GetPressure(Inputs::PAD_L_RIGHT);
		case 8:
			// PS1 mode: If the controller reaches this byte, it is in analog mode and has irrefutably reached the last byte.
			// There's simply nothing to check, we know it's done and time to stop acknowledgements.
			g_Sio0.SetAcknowledge(false);
			return GetPressure(Inputs::PAD_L_UP);
		case 9:
			return IsButtonBitSet(buttons, 13) ? GetPressure(Inputs::PAD_RIGHT) : 0;
		case 10:
			return IsButtonBitSet(buttons, 15) ? GetPressure(Inputs::PAD_LEFT) : 0;
		case 11:
			return IsButtonBitSet(buttons, 12) ? GetPressure(Inputs::PAD_UP) : 0;
		case 12:
			return IsButtonBitSet(buttons, 14) ? GetPressure(Inputs::PAD_DOWN) : 0;
		case 13:
			return IsButtonBitSet(buttons, 4) ? GetPressure(Inputs::PAD_TRIANGLE) : 0;
		case 14:
			return IsButtonBitSet(buttons, 5) ? GetPressure(Inputs::PAD_CIRCLE) : 0;
		case 15:
			return IsButtonBitSet(buttons, 6) ? GetPressure(Inputs::PAD_CROSS) : 0;
		case 16:
			return IsButtonBitSet(buttons, 7) ? GetPressure(Inputs::PAD_SQUARE) : 0;
		case 17:
			return IsButtonBitSet(buttons, 2) ? GetPressure(Inputs::PAD_L1) : 0;
		case 18:
			return IsButtonBitSet(buttons, 3) ? GetPressure(Inputs::PAD_R1) : 0;
		case 19:
			return IsButtonBitSet(buttons, 0) ? GetPressure(Inputs::PAD_L2) : 0;
		case 20:
			return IsButtonBitSet(buttons, 1) ? GetPressure(Inputs::PAD_R2) : 0;
	}
	
	Console.Warning("%s(%02X) Did not reach a valid return path! Returning zero as a failsafe!", __FUNCTION__, commandByte);
	return 0x00;
}

u8 PadDualshock2::Config(u8 commandByte)
{
	if (commandBytesReceived == 3)
	{
		if (commandByte)
		{
			if (!this->isInConfig)
			{
				this->isInConfig = true;
			}
			else
			{
				Console.Warning("%s(%02X) Unexpected enter while already in config mode", __FUNCTION__, commandByte);
			}
		}
		else
		{
			if (this->isInConfig)
			{
				this->isInConfig = false;
				this->ConfigLog();
			}
			else
			{
				Console.Warning("%s(%02X) Unexpected exit while not in config mode", __FUNCTION__, commandByte);
			}
		}
	}
	
	// PS1 mode: Config mode would have been triggered by a prior byte in this command sequence;
	// if we are now in config mode, check the current mode and if this is the last byte. If so,
	// don't acknowledge.
	if (this->isInConfig)
	{
		if ((this->currentMode == Pad::Mode::DIGITAL && this->commandBytesReceived == 4) || (this->currentMode == Pad::Mode::ANALOG && this->commandBytesReceived == 8))
		{
			g_Sio0.SetAcknowledge(false);
		}
	}

	return 0x00;
}

// Changes the mode of the controller between digital and analog, and adjusts the analog LED accordingly.
u8 PadDualshock2::ModeSwitch(u8 commandByte)
{
	switch (commandBytesReceived)
	{
		case 3:
			this->analogLight = commandByte;

			if (this->analogLight)
			{
				this->currentMode = Pad::Mode::ANALOG;
			}
			else
			{
				this->currentMode = Pad::Mode::DIGITAL;
			}

			break;
		case 4:
			this->analogLocked = (commandByte == 0x03);
			break;
		case 8:
			g_Sio0.SetAcknowledge(false);
			break;
		default:
			break;
	}

	return 0x00;
}

u8 PadDualshock2::StatusInfo(u8 commandByte)
{
	switch (commandBytesReceived)
	{
		case 3:
			return static_cast<u8>(Pad::PhysicalType::STANDARD);
		case 4:
			return 0x02;
		case 5:
			return this->analogLight;
		case 6:
			return 0x02;
		case 7:
			return 0x01;
		case 8:
			g_Sio0.SetAcknowledge(false);
			return 0x00;
		default:
			return 0x00;
	}
}

u8 PadDualshock2::Constant1(u8 commandByte)
{
	switch (commandBytesReceived)
	{
		case 3:
			commandStage = commandByte != 0;
			return 0x00;
		case 5:
			return 0x01;
		case 6:
			if (commandStage)
			{
				return 0x01;
			}
			else
			{
				return 0x02;
			}
		case 7:
			if (commandStage)
			{
				return 0x01;
			}
			else
			{
				return 0x00;
			}
		case 8:
			g_Sio0.SetAcknowledge(false);
			return (commandStage ? 0x14 : 0x0a);
		default:
			return 0x00;
	}
}

u8 PadDualshock2::Constant2(u8 commandByte)
{
	switch (commandBytesReceived)
	{
		case 5:
			return 0x02;
		case 7:
			return 0x01;
		case 8:
			g_Sio0.SetAcknowledge(false);
			return 0x00;
		default:
			return 0x00;
	}
}

u8 PadDualshock2::Constant3(u8 commandByte)
{
	switch (commandBytesReceived)
	{
		case 3:
			commandStage = (commandByte != 0);
			return 0x00;
		case 6:
			if (commandStage)
			{
				return 0x07;
			}
			else
			{
				return 0x04;
			}
		case 8:
			g_Sio0.SetAcknowledge(false);
			return 0x00;
		default:
			return 0x00;
	}
}

// Set which byte of the poll command will correspond to a motor's power level.
// In all known cases, games never rearrange the motors. We've hard coded pad polls
// to always use the first vibration byte as small motor, and the second as big motor.
// There is no reason to rearrange these. Games never rearrange these. If someone does
// try to rearrange these, they should suffer.
//
// The return values for cases 3 and 4 are just to notify the pad module of what the mapping was, prior to this command.
u8 PadDualshock2::VibrationMap(u8 commandByte)
{
	u8 ret = 0xff;

	switch (commandBytesReceived)
	{
		case 3:
			ret = this->smallMotorLastConfig;
			this->smallMotorLastConfig = commandByte;
			return ret;
		case 4:
			ret = this->largeMotorLastConfig;
			this->largeMotorLastConfig = commandByte;
			return ret;
		case 8:
			g_Sio0.SetAcknowledge(false);
			return 0xff;
		default:
			return 0xff;
	}
}

u8 PadDualshock2::ResponseBytes(u8 commandByte)
{
	switch (commandBytesReceived)
	{
		case 3:
			this->responseBytes = commandByte;
			return 0x00;
		case 4:
			this->responseBytes |= (commandByte << 8);
			return 0x00;
		case 5:
			this->responseBytes |= (commandByte << 16);

			switch (static_cast<Pad::ResponseBytes>(this->responseBytes))
			{
				case Pad::ResponseBytes::ANALOG:
					this->analogLight = true;
					this->currentMode = Pad::Mode::ANALOG;
					break;
				case Pad::ResponseBytes::DUALSHOCK2:
					this->analogLight = true;
					this->currentMode = Pad::Mode::DUALSHOCK2;
					break;
				default:
					this->analogLight = false;
					this->currentMode = Pad::Mode::DIGITAL;
					break;
			}

			return 0x00;
		case 8:
			return 0x5a;
		default:
			return 0x00;
	}
}

PadDualshock2::PadDualshock2(u8 unifiedSlot, size_t ejectTicks)
	: PadBase(unifiedSlot, ejectTicks)
{
	currentMode = Pad::Mode::DIGITAL;
}

PadDualshock2::~PadDualshock2() = default;

Pad::ControllerType PadDualshock2::GetType() const
{
	return Pad::ControllerType::DualShock2;
}

const Pad::ControllerInfo& PadDualshock2::GetInfo() const
{
	return ControllerInfo;
}

void PadDualshock2::Set(u32 index, float value)
{
	if (index > Inputs::LENGTH)
	{
		return;
	}

	// Since we reordered the buttons for better UI, we need to remap them here.
	static constexpr std::array<u8, Inputs::LENGTH> bitmaskMapping = {{
		12, // PAD_UP
		13, // PAD_RIGHT
		14, // PAD_DOWN
		15, // PAD_LEFT
		4, // PAD_TRIANGLE
		5, // PAD_CIRCLE
		6, // PAD_CROSS
		7, // PAD_SQUARE
		8, // PAD_SELECT
		11, // PAD_START
		2, // PAD_L1
		0, // PAD_L2
		3, // PAD_R1
		1, // PAD_R2
		9, // PAD_L3
		10, // PAD_R3
		16, // PAD_ANALOG
		17, // PAD_PRESSURE
		// remainder are analogs and not used here
	}};

	if (IsAnalogKey(index))
	{
		this->rawInputs[index] = static_cast<u8>(std::clamp(value * this->axisScale * 255.0f, 0.0f, 255.0f));

		//                          Left -> -- -> Right
		// Value range :        FFFF8002 -> 0  -> 7FFE
		// Force range :			  80 -> 0  -> 7F
		// Normal mode : expect value 0  -> 80 -> FF
		// Reverse mode: expect value FF -> 7F -> 0

		// merge left/right or up/down into rx or ry

#define MERGE(pos, neg) ((this->rawInputs[pos] != 0) ? (127u + ((this->rawInputs[pos] + 1u) / 2u)) : (127u - (this->rawInputs[neg] / 2u)))
		if (index <= Inputs::PAD_L_LEFT)
		{
			// Left Stick
			this->analogs.lx = this->analogs.lxInvert ? MERGE(Inputs::PAD_L_LEFT, Inputs::PAD_L_RIGHT) : MERGE(Inputs::PAD_L_RIGHT, Inputs::PAD_L_LEFT);
			this->analogs.ly = this->analogs.lyInvert ? MERGE(Inputs::PAD_L_UP, Inputs::PAD_L_DOWN) : MERGE(Inputs::PAD_L_DOWN, Inputs::PAD_L_UP);
		}
		else
		{
			// Right Stick
			this->analogs.rx = this->analogs.rxInvert ? MERGE(Inputs::PAD_R_LEFT, Inputs::PAD_R_RIGHT) : MERGE(Inputs::PAD_R_RIGHT, Inputs::PAD_R_LEFT);
			this->analogs.ry = this->analogs.ryInvert ? MERGE(Inputs::PAD_R_UP, Inputs::PAD_R_DOWN) : MERGE(Inputs::PAD_R_DOWN, Inputs::PAD_R_UP);
		}
#undef MERGE

		// Deadzone computation.
		const float dz = this->axisDeadzone;

		if (dz > 0.0f)
		{
#define MERGE_F(pos, neg) ((this->rawInputs[pos] != 0) ? (static_cast<float>(this->rawInputs[pos]) / 255.0f) : (static_cast<float>(this->rawInputs[neg]) / -255.0f))
			float posX, posY;
			if (index <= Inputs::PAD_L_LEFT)
			{
				posX = this->analogs.lxInvert ? MERGE_F(Inputs::PAD_L_LEFT, Inputs::PAD_L_RIGHT) : MERGE_F(Inputs::PAD_L_RIGHT, Inputs::PAD_L_LEFT);
				posY = this->analogs.lyInvert ? MERGE_F(Inputs::PAD_L_UP, Inputs::PAD_L_DOWN) : MERGE_F(Inputs::PAD_L_DOWN, Inputs::PAD_L_UP);
			}
			else
			{
				posX = this->analogs.rxInvert ? MERGE_F(Inputs::PAD_R_LEFT, Inputs::PAD_R_RIGHT) : MERGE_F(Inputs::PAD_R_RIGHT, Inputs::PAD_R_LEFT);
				posY = this->analogs.ryInvert ? MERGE_F(Inputs::PAD_R_UP, Inputs::PAD_R_DOWN) : MERGE_F(Inputs::PAD_R_DOWN, Inputs::PAD_R_UP);
			}

			// No point checking if we're at dead center (usually keyboard with no buttons pressed).
			if (posX != 0.0f || posY != 0.0f)
			{
				// Compute the angle at the given position in the stick's square bounding box.
				const float theta = std::atan2(posY, posX);

				// Compute the position that the edge of the circle would be at, given the angle.
				const float dzX = std::cos(theta) * dz;
				const float dzY = std::sin(theta) * dz;

				// We're in the deadzone if our position is less than the circle edge.
				const bool inX = (posX < 0.0f) ? (posX > dzX) : (posX <= dzX);
				const bool inY = (posY < 0.0f) ? (posY > dzY) : (posY <= dzY);
				
				if (inX && inY)
				{
					// In deadzone. Set to 127 (center).
					if (index <= Inputs::PAD_L_LEFT)
					{
						this->analogs.lx = this->analogs.ly = 127;
					}
					else
					{
						this->analogs.rx = this->analogs.ry = 127;
					}	
				}
			}
#undef MERGE_F
		}
	}
	else if (IsTriggerKey(index))
	{
		const float s_value = std::clamp(value, 0.0f, 1.0f);
		const float dz_value = (this->buttonDeadzone > 0.0f && s_value < this->buttonDeadzone) ? 0.0f : s_value;
		this->rawInputs[index] = static_cast<u8>(dz_value * 255.0f);
		if (dz_value > 0.0f)
			this->buttons &= ~(1u << bitmaskMapping[index]);
		else
			this->buttons |= (1u << bitmaskMapping[index]);
	}
	else
	{
		// Don't affect L2/R2, since they are analog on most pads.
		const float pMod = ((this->buttons & (1u << Inputs::PAD_PRESSURE)) == 0 && !IsTriggerKey(index)) ? this->pressureModifier : 1.0f;
		const float dzValue = (value < this->buttonDeadzone) ? 0.0f : value;
		this->rawInputs[index] = static_cast<u8>(std::clamp(dzValue * pMod * 255.0f, 0.0f, 255.0f));

		if (dzValue > 0.0f)
		{
			this->buttons &= ~(1u << bitmaskMapping[index]);
		}
		else
		{
			this->buttons |= (1u << bitmaskMapping[index]);
		}

		// Adjust pressure of all other face buttons which are active when pressure modifier is pressed..
		if (index == Inputs::PAD_PRESSURE)
		{
			const float adjustPMod = ((this->buttons & (1u << Inputs::PAD_PRESSURE)) == 0) ? this->pressureModifier : (1.0f / this->pressureModifier);

			for (u32 i = 0; i < Inputs::LENGTH; i++)
			{
				if (i == index || IsAnalogKey(i) || IsTriggerKey(i))
				{
					continue;
				}

				// We add 0.5 here so that the round trip between 255->127->255 when applying works as expected.
				const float add = (this->rawInputs[i] != 0) ? 0.5f : 0.0f;
				this->rawInputs[i] = static_cast<u8>(std::clamp((static_cast<float>(this->rawInputs[i]) + add) * adjustPMod, 0.0f, 255.0f));
			}
		}

		if (index == Inputs::PAD_ANALOG && !this->analogPressed && value > 0)
		{
			this->analogPressed = true;

			if (!this->analogLocked)
			{
				this->analogLight = !this->analogLight;

				if (this->analogLight)
				{
					this->currentMode = Pad::Mode::ANALOG;
				}
				else
				{
					this->currentMode = Pad::Mode::DIGITAL;
				}

				const auto [port, slot] = sioConvertPadToPortAndSlot(unifiedSlot);
				
				Host::AddKeyedOSDMessage(fmt::format("PadAnalogButtonChange{}{}", port, slot),
					this->analogLight ? fmt::format(TRANSLATE_FS("Pad", "Analog light is now on for port {0} / slot {1}"), port + 1, slot + 1) :
										fmt::format(TRANSLATE_FS("Pad", "Analog light is now off for port {0} / slot {1}"), port + 1, slot + 1),
					Host::OSD_INFO_DURATION);
			}
		}
		else
		{
			this->analogPressed = false;
		}
	}
}

void PadDualshock2::SetRawAnalogs(const std::tuple<u8, u8> left, const std::tuple<u8, u8> right)
{
	this->analogs.lx = std::get<0>(left);
	this->analogs.ly = std::get<1>(left);
	this->analogs.rx = std::get<0>(right);
	this->analogs.ry = std::get<1>(right);
}

void PadDualshock2::SetAxisScale(float deadzone, float scale)
{
	this->axisDeadzone = deadzone;
	this->axisScale = scale;
}

float PadDualshock2::GetVibrationScale(u32 motor) const
{
	return this->vibrationScale[motor];
}

void PadDualshock2::SetVibrationScale(u32 motor, float scale)
{
	this->vibrationScale[motor] = scale;
}

float PadDualshock2::GetPressureModifier() const
{
	return this->pressureModifier;
}

void PadDualshock2::SetPressureModifier(float mod)
{
	this->pressureModifier = mod;
}

void PadDualshock2::SetButtonDeadzone(float deadzone)
{
	this->buttonDeadzone = deadzone;
}

void PadDualshock2::SetAnalogInvertL(bool x, bool y)
{
	this->analogs.lxInvert = x;
	this->analogs.lyInvert = y;
}

void PadDualshock2::SetAnalogInvertR(bool x, bool y)
{
	this->analogs.rxInvert = x;
	this->analogs.ryInvert = y;
}

float PadDualshock2::GetEffectiveInput(u32 index) const
{
	if (!IsAnalogKey(index))
		return GetRawInput(index);

	switch (index)
	{
	case Inputs::PAD_L_LEFT:
		return (analogs.lx < 127) ? -((127 - analogs.lx) / 127.0f) : 0;

	case Inputs::PAD_L_RIGHT:
		return (analogs.lx > 127) ? ((analogs.lx - 127) / 128.0f) : 0;

	case Inputs::PAD_L_UP:
		return (analogs.ly < 127) ? -((127 - analogs.ly) / 127.0f) : 0;

	case Inputs::PAD_L_DOWN:
		return (analogs.ly > 127) ? ((analogs.ly - 127) / 128.0f) : 0;

	case Inputs::PAD_R_LEFT:
		return (analogs.rx < 127) ? -((127 - analogs.rx) / 127.0f) : 0;

	case Inputs::PAD_R_RIGHT:
		return (analogs.rx > 127) ? ((analogs.rx - 127) / 128.0f) : 0;

	case Inputs::PAD_R_UP:
		return (analogs.ry < 127) ? -((127 - analogs.ry) / 127.0f) : 0;

	case Inputs::PAD_R_DOWN:
		return (analogs.ry > 127) ? ((analogs.ry - 127) / 128.0f) : 0;

	default:
		return 0;
	}
}

u8 PadDualshock2::GetRawInput(u32 index) const
{
	return rawInputs[index];
}

std::tuple<u8, u8> PadDualshock2::GetRawLeftAnalog() const
{
	return {analogs.lx, analogs.ly};
}

std::tuple<u8, u8> PadDualshock2::GetRawRightAnalog() const
{
	return {analogs.rx, analogs.ry};
}

u32 PadDualshock2::GetButtons() const
{
	return buttons;
}

u8 PadDualshock2::GetPressure(u32 index) const
{
	switch (index)
	{
		case Inputs::PAD_R_LEFT:
		case Inputs::PAD_R_RIGHT:
			return this->analogs.rx;
		case Inputs::PAD_R_DOWN:
		case Inputs::PAD_R_UP:
			return this->analogs.ry;
		case Inputs::PAD_L_LEFT:
		case Inputs::PAD_L_RIGHT:
			return this->analogs.lx;
		case Inputs::PAD_L_DOWN:
		case Inputs::PAD_L_UP:
			return this->analogs.ly;
		default:
			return this->rawInputs[index];
	}
}

bool PadDualshock2::Freeze(StateWrapper& sw)
{
	if (!PadBase::Freeze(sw) || !sw.DoMarker("PadDualshock2"))
		return false;

	// Private PadDualshock2 members
	sw.Do(&analogLight);
	sw.Do(&analogLocked);
	sw.Do(&analogPressed);
	sw.Do(&commandStage);
	sw.Do(&responseBytes);
	sw.Do(&vibrationMotors);
	sw.Do(&smallMotorLastConfig);
	sw.Do(&largeMotorLastConfig);
	return !sw.HasError();
}

u8 PadDualshock2::SendCommandByte(u8 commandByte)
{
	u8 ret = 0;

	switch (this->commandBytesReceived)
	{
		case 0:
			ret = 0x00;
			break;
		case 1:
			this->currentCommand = static_cast<Pad::Command>(commandByte);
			
			if (this->currentCommand != Pad::Command::POLL && this->currentCommand != Pad::Command::CONFIG && !this->isInConfig)
			{
				Console.Warning("%s(%02X) Config-only command was sent to a pad outside of config mode!", __FUNCTION__, commandByte);
			}

			ret = this->isInConfig ? static_cast<u8>(Pad::Mode::CONFIG) : static_cast<u8>(this->currentMode);
			break;
		case 2:
			ret = 0x5a;
			break;
		default:
			switch (this->currentCommand)
			{
				case Pad::Command::MYSTERY:
					ret = Mystery(commandByte);
					break;
				case Pad::Command::BUTTON_QUERY:
					ret = ButtonQuery(commandByte);
					break;
				case Pad::Command::POLL:
					ret = Poll(commandByte);
					break;
				case Pad::Command::CONFIG:
					ret = Config(commandByte);
					break;
				case Pad::Command::MODE_SWITCH:
					ret = ModeSwitch(commandByte);
					break;
				case Pad::Command::STATUS_INFO:
					ret = StatusInfo(commandByte);
					break;
				case Pad::Command::CONST_1:
					ret = Constant1(commandByte);
					break;
				case Pad::Command::CONST_2:
					ret = Constant2(commandByte);
					break;
				case Pad::Command::CONST_3:
					ret = Constant3(commandByte);
					break;
				case Pad::Command::VIBRATION_MAP:
					ret = VibrationMap(commandByte);
					break;
				case Pad::Command::RESPONSE_BYTES:
					ret = ResponseBytes(commandByte);
					break;
				default:
					ret = 0x00;
					break;
			}
	}

	this->commandBytesReceived++;
	return ret;
}
