// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "SIO/Pad/PadNegcon.h"
#include "SIO/Pad/Pad.h"
#include "SIO/Sio.h"

#include "Common.h"
#include "Input/InputManager.h"
#include "Host.h"

#include "IconsPromptFont.h"

static const InputBindingInfo s_bindings[] = {
	// clang-format off
	{"Up", TRANSLATE_NOOP("Pad", "D-Pad Up"), ICON_PF_DPAD_UP, InputBindingInfo::Type::Button, PadNegcon::Inputs::PAD_UP, GenericInputBinding::DPadUp},
	{"Right", TRANSLATE_NOOP("Pad", "D-Pad Right"), ICON_PF_DPAD_RIGHT, InputBindingInfo::Type::Button, PadNegcon::Inputs::PAD_RIGHT, GenericInputBinding::DPadRight},
	{"Down", TRANSLATE_NOOP("Pad", "D-Pad Down"), ICON_PF_DPAD_DOWN, InputBindingInfo::Type::Button, PadNegcon::Inputs::PAD_DOWN, GenericInputBinding::DPadDown},
	{"Left", TRANSLATE_NOOP("Pad", "D-Pad Left"), ICON_PF_DPAD_LEFT, InputBindingInfo::Type::Button, PadNegcon::Inputs::PAD_LEFT, GenericInputBinding::DPadLeft},
	{"B", TRANSLATE_NOOP("Pad", "B Button"), ICON_PF_BUTTON_B, InputBindingInfo::Type::Button, PadNegcon::Inputs::PAD_B, GenericInputBinding::Triangle},
	{"A", TRANSLATE_NOOP("Pad", "A Button"), ICON_PF_BUTTON_A, InputBindingInfo::Type::Button, PadNegcon::Inputs::PAD_A, GenericInputBinding::Circle},
	{"I", TRANSLATE_NOOP("Pad", "I Button"), nullptr, InputBindingInfo::Type::Button, PadNegcon::Inputs::PAD_I, GenericInputBinding::Cross},
	{"II", TRANSLATE_NOOP("Pad", "II Button"), nullptr, InputBindingInfo::Type::Button, PadNegcon::Inputs::PAD_II, GenericInputBinding::Square},
	{},
	{"Start", TRANSLATE_NOOP("Pad", "Start"), ICON_PF_START, InputBindingInfo::Type::Button, PadNegcon::Inputs::PAD_START, GenericInputBinding::Start},
	{"L", TRANSLATE_NOOP("Pad", "L (Left Bumper)"), ICON_PF_LEFT_SHOULDER_L1, InputBindingInfo::Type::Button, PadNegcon::Inputs::PAD_L, GenericInputBinding::L2},
	{"R", TRANSLATE_NOOP("Pad", "R (Right Bumper)"), ICON_PF_RIGHT_SHOULDER_R1, InputBindingInfo::Type::Button, PadNegcon::Inputs::PAD_R, GenericInputBinding::R2},
	{"TwistLeft", TRANSLATE_NOOP("Pad", "Twist (Left)"), nullptr, InputBindingInfo::Type::HalfAxis, PadNegcon::Inputs::PAD_TWIST_LEFT, GenericInputBinding::LeftStickLeft},
	{"TwistRight", TRANSLATE_NOOP("Pad", "Twist (Right)"), nullptr, InputBindingInfo::Type::HalfAxis, PadNegcon::Inputs::PAD_TWIST_RIGHT, GenericInputBinding::LeftStickRight},
	{"LargeMotor", TRANSLATE_NOOP("Pad", "Large (Low Frequency) Motor"), nullptr, InputBindingInfo::Type::Motor, 0, GenericInputBinding::LargeMotor},
	{"SmallMotor", TRANSLATE_NOOP("Pad", "Small (High Frequency) Motor"), nullptr, InputBindingInfo::Type::Motor, 0, GenericInputBinding::SmallMotor},
	// clang-format on
};

static const SettingInfo s_settings[] = {
	{SettingInfo::Type::Float, "Deadzone", TRANSLATE_NOOP("Pad", "Twist Deadzone"),
		TRANSLATE_NOOP("Pad", "Sets the twist deadzone. Inputs below this value will not be sent to the PS2."),
		"0.00", "0.00", "1.00", "0.01", "%.0f%%", nullptr, nullptr, 100.0f},
	{SettingInfo::Type::Float, "AxisScale", TRANSLATE_NOOP("Pad", "Twist Sensitivity"),
		TRANSLATE_NOOP("Pad", "Sets the twist scaling factor."),
		"1.0", "0.01", "2.00", "0.01", "%.0f%%", nullptr, nullptr, 100.0f},
};

const Pad::ControllerInfo PadNegcon::ControllerInfo = {Pad::ControllerType::Negcon, "Negcon",
	TRANSLATE_NOOP("Pad", "Negcon"), ICON_PF_GAMEPAD_ALT, s_bindings, s_settings, Pad::VibrationCapabilities::LargeSmallMotors};

void PadNegcon::ConfigLog()
{
	const auto [port, slot] = sioConvertPadToPortAndSlot(unifiedSlot);

	// AL: Analog Light (is it turned on right now)
	// AB: Analog Button (is it useable or is it locked in its current state)
	Console.WriteLn(fmt::format("[Pad] Negcon Config Finished - P{0}/S{1} - AL: {2} - AB: {3}",
		port + 1,
		slot + 1,
		(this->analogLight ? "On" : "Off"),
		(this->analogLocked ? "Locked" : "Usable")));
}

u8 PadNegcon::Mystery(u8 commandByte)
{
	switch (this->commandBytesReceived)
	{
		case 5:
			return 0x02;
		case 8:
			return 0x5a;
		default:
			return 0x00;
	}
}

u8 PadNegcon::ButtonQuery(u8 commandByte)
{
	switch (this->commandBytesReceived)
	{
		case 3:
		case 4:
			return 0xff;
		case 5:
			return 0x03;
		case 8:
			return 0x5a;
		default:
			return 0x00;
	}
}

u8 PadNegcon::Poll(u8 commandByte)
{
	const u32 buttons = GetButtons();
	u8 largeMotor = 0x00;
	u8 smallMotor = 0x00;

	switch (this->commandBytesReceived)
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
				std::min(static_cast<float>((smallMotor ? 0xff : 0)) * GetVibrationScale(0) * (1.0f / 255.0f), 1.0f));

			return buttons & 0xff;
		case 5:
			return this->analogs.twist;
		case 6:
			return this->analogs.i;
		case 7:
			return this->analogs.ii;
		case 8:
			return this->analogs.l;
	}

	Console.Warning("%s(%02X) Did not reach a valid return path! Returning zero as a failsafe!", __FUNCTION__, commandByte);
	return 0x00;
}

u8 PadNegcon::Config(u8 commandByte)
{
	if (this->commandBytesReceived == 3)
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

	return 0x00;
}

// Changes the mode of the controller between digital and analog, and adjusts the analog LED accordingly.
u8 PadNegcon::ModeSwitch(u8 commandByte)
{
	switch (this->commandBytesReceived)
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
		default:
			break;
	}

	return 0x00;
}

u8 PadNegcon::StatusInfo(u8 commandByte)
{
	switch (this->commandBytesReceived)
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
		default:
			return 0x00;
	}
}

u8 PadNegcon::Constant1(u8 commandByte)
{
	switch (this->commandBytesReceived)
	{
		case 3:
			commandStage = (commandByte != 0);
			return 0x00;
		case 5:
			return 0x01;
		case 6:
			return (!commandStage ? 0x02 : 0x01);
		case 7:
			return (!commandStage ? 0x00 : 0x01);
		case 8:
			return (commandStage ? 0x0a : 0x14);
		default:
			return 0x00;
	}
}

u8 PadNegcon::Constant2(u8 commandByte)
{
	switch (this->commandBytesReceived)
	{
		case 5:
			return 0x02;
		case 7:
			return 0x01;
		default:
			return 0x00;
	}
}

u8 PadNegcon::Constant3(u8 commandByte)
{
	switch (this->commandBytesReceived)
	{
		case 3:
			commandStage = (commandByte != 0);
			return 0x00;
		case 6:
			return (!commandStage ? 0x04 : 0x07);
		default:
			return 0x00;
	}
}

u8 PadNegcon::VibrationMap(u8 commandByte)
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
			return 0xff;
		default:
			return 0xff;
	}
}

PadNegcon::PadNegcon(u8 unifiedSlot, size_t ejectTicks)
	: PadBase(unifiedSlot, ejectTicks)
{
	currentMode = Pad::Mode::NEGCON;
}

PadNegcon::~PadNegcon() = default;

Pad::ControllerType PadNegcon::GetType() const
{
	return Pad::ControllerType::Negcon;
}

const Pad::ControllerInfo& PadNegcon::GetInfo() const
{
	return ControllerInfo;
}

void PadNegcon::Set(u32 index, float value)
{
	if (index > Inputs::LENGTH)
	{
		return;
	}

	if (IsTwistKey(index))
	{
		const float dz_value = (this->twistDeadzone > 0.0f && value < this->twistDeadzone) ? 0.0f : value;
		this->rawInputs[index] = static_cast<u8>(std::clamp(dz_value * this->twistScale * 255.0f, 0.0f, 255.0f));

		this->analogs.twist = this->rawInputs[Inputs::PAD_TWIST_RIGHT] != 0 ? 127u + (this->rawInputs[Inputs::PAD_TWIST_RIGHT] + 1u) / 2u : 127u - (this->rawInputs[Inputs::PAD_TWIST_LEFT] - 1u) / 2;
		return;
	}

	this->rawInputs[index] = static_cast<u8>(std::clamp(value * 255.0f, 0.0f, 255.0f));
	if (IsAnalogKey(index))
	{
		switch (index)
		{
			case PAD_I:
				this->analogs.i = this->rawInputs[index];
				break;
			case PAD_II:
				this->analogs.ii = this->rawInputs[index];
				break;
			case PAD_L:
				this->analogs.l = this->rawInputs[index];
				break;
		}
	}

	if (this->rawInputs[index] > 0.0f)
	{
		this->buttons &= ~(1u << bitmaskMapping[index]);
	}
	else
	{
		this->buttons |= (1u << bitmaskMapping[index]);
	}
}

void PadNegcon::SetRawAnalogs(const std::tuple<u8, u8> left, const std::tuple<u8, u8> right)
{
	this->analogs.i = std::get<0>(left);
	this->analogs.ii = std::get<1>(left);
	this->analogs.l = std::get<0>(right);
	this->analogs.twist = std::get<1>(right);
}

void PadNegcon::SetRawPressureButton(u32 index, const std::tuple<bool, u8> value)
{
	this->rawInputs[index] = std::get<1>(value);
	if (std::get<0>(value))
	{
		this->buttons &= ~(1u << bitmaskMapping[index]);
	}
	else
	{
		this->buttons |= (1u << bitmaskMapping[index]);
	}
}

void PadNegcon::SetAxisScale(float deadzone, float scale)
{
	this->twistDeadzone = deadzone;
	this->twistScale = scale;
}

float PadNegcon::GetVibrationScale(u32 motor) const
{
	return this->vibrationScale[motor];
}

void PadNegcon::SetVibrationScale(u32 motor, float scale)
{
	this->vibrationScale[motor] = scale;
}

float PadNegcon::GetPressureModifier() const
{
	return 0;
}

void PadNegcon::SetPressureModifier(float mod)
{
}

void PadNegcon::SetButtonDeadzone(float deadzone)
{
}

void PadNegcon::SetAnalogInvertL(bool x, bool y)
{
}

void PadNegcon::SetAnalogInvertR(bool x, bool y)
{
}

float PadNegcon::GetEffectiveInput(u32 index) const
{
	if (!IsAnalogKey(index))
		return GetRawInput(index);

	switch (index)
	{
		case Inputs::PAD_I:
			return analogs.i;

		case Inputs::PAD_II:
			return analogs.ii;

		case Inputs::PAD_L:
			return analogs.l;

		case Inputs::PAD_TWIST_LEFT:
			return (analogs.twist < 127) ? ((127 - analogs.twist) / 127.0f) : 0;

		case Inputs::PAD_TWIST_RIGHT:
			return (analogs.twist > 128) ? ((analogs.twist - 128) / 127.0f) : 0;

		default:
			return 0;
	}
}

u8 PadNegcon::GetRawInput(u32 index) const
{
	return rawInputs[index];
}

std::tuple<u8, u8> PadNegcon::GetRawLeftAnalog() const
{
	return std::tuple<u8, u8>{analogs.i, analogs.ii};
}

std::tuple<u8, u8> PadNegcon::GetRawRightAnalog() const
{
	return std::tuple<u8, u8>{analogs.l, analogs.twist};
}

u32 PadNegcon::GetButtons() const
{
	return buttons;
}

u8 PadNegcon::GetPressure(u32 index) const
{
	return 0;
}

bool PadNegcon::Freeze(StateWrapper& sw)
{
	if (!PadBase::Freeze(sw) || !sw.DoMarker("PadNegcon"))
		return false;

	// Private PadNegcon members
	sw.Do(&analogLight);
	sw.Do(&analogLocked);
	sw.Do(&commandStage);
	sw.Do(&vibrationMotors);
	sw.Do(&smallMotorLastConfig);
	sw.Do(&largeMotorLastConfig);
	return !sw.HasError();
}

u8 PadNegcon::SendCommandByte(u8 commandByte)
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
				default:
					ret = 0x00;
					break;
			}
	}

	this->commandBytesReceived++;
	return ret;
}
