// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SIO/Pad/PadPopn.h"
#include "SIO/Pad/Pad.h"
#include "SIO/Sio.h"
#include "SIO/Sio0.h"

#include "Common.h"
#include "Input/InputManager.h"
#include "Host.h"

#include "IconsPromptFont.h"

static const InputBindingInfo s_bindings[] = {
	// clang-format off
	{"YellowL", TRANSLATE_NOOP("Pad", "Yellow (Left)"), ICON_PF_POPN_YELLOW_LEFT, InputBindingInfo::Type::Button, PadPopn::Inputs::PAD_YELLOW_LEFT, GenericInputBinding::Circle},
	{"YellowR", TRANSLATE_NOOP("Pad", "Yellow (Right)"), ICON_PF_POPN_YELLOW_RIGHT, InputBindingInfo::Type::Button, PadPopn::Inputs::PAD_YELLOW_RIGHT, GenericInputBinding::DPadUp},
	{"BlueL", TRANSLATE_NOOP("Pad", "Blue (Left)"), ICON_PF_POPN_BLUE_LEFT, InputBindingInfo::Type::Button, PadPopn::Inputs::PAD_BLUE_LEFT, GenericInputBinding::Cross},
	{"BlueR", TRANSLATE_NOOP("Pad", "Blue (Right)"), ICON_PF_POPN_BLUE_RIGHT, InputBindingInfo::Type::Button, PadPopn::Inputs::PAD_BLUE_RIGHT, GenericInputBinding::Square},
	{"WhiteL", TRANSLATE_NOOP("Pad", "White (Left)"), ICON_PF_POPN_WHITE_LEFT, InputBindingInfo::Type::Button, PadPopn::Inputs::PAD_WHITE_LEFT, GenericInputBinding::Triangle},
	{"WhiteR", TRANSLATE_NOOP("Pad", "White (Right)"), ICON_PF_POPN_WHITE_RIGHT, InputBindingInfo::Type::Button, PadPopn::Inputs::PAD_WHITE_RIGHT, GenericInputBinding::L2},
	{"GreenL", TRANSLATE_NOOP("Pad", "Green (Left)"), ICON_PF_POPN_GREEN_LEFT, InputBindingInfo::Type::Button, PadPopn::Inputs::PAD_GREEN_LEFT, GenericInputBinding::R1},
	{"GreenR", TRANSLATE_NOOP("Pad", "Green (Right)"), ICON_PF_POPN_GREEN_RIGHT, InputBindingInfo::Type::Button, PadPopn::Inputs::PAD_GREEN_RIGHT, GenericInputBinding::R2},
	{"Red", TRANSLATE_NOOP("Pad", "Red"), ICON_PF_POPN_RED, InputBindingInfo::Type::Button, PadPopn::Inputs::PAD_RED, GenericInputBinding::L1},
	{"Start", TRANSLATE_NOOP("Pad", "Start"), ICON_PF_START, InputBindingInfo::Type::Button, PadPopn::Inputs::PAD_START, GenericInputBinding::Start},
	{"Select", TRANSLATE_NOOP("Pad", "Select"), ICON_PF_SELECT_SHARE, InputBindingInfo::Type::Button, PadPopn::Inputs::PAD_SELECT, GenericInputBinding::Select},
	// clang-format on
};

const Pad::ControllerInfo PadPopn::ControllerInfo = {Pad::ControllerType::Popn, "Popn",
	TRANSLATE_NOOP("Pad", "Pop'n Music"), ICON_PF_POPN_CONTROLLER, s_bindings, {}, Pad::VibrationCapabilities::NoVibration};

void PadPopn::ConfigLog()
{
	const auto [port, slot] = sioConvertPadToPortAndSlot(unifiedSlot);
	
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
	// RB: Response Bytes (what data is included in the controller's responses - D = Digital, A = Analog, P = Pressure)
	Console.WriteLn(fmt::format("[Pad] Pop'n Config Finished - P{0}/S{1} - AL: {2} - AB: {3} - RB: {4} (0x{5:08X})",
		port + 1,
		slot + 1,
		(this->analogLight ? "On" : "Off"),
		(this->analogLocked ? "Locked" : "Usable"),
		pressureStr,
		this->responseBytes));
}

u8 PadPopn::Mystery(u8 commandByte)
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

u8 PadPopn::ButtonQuery(u8 commandByte)
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

u8 PadPopn::Poll(u8 commandByte)
{
	const u32 buttons = GetButtons();

	switch (commandBytesReceived)
	{
		case 3:
			return (buttons >> 8) & 0xff;
		case 4:
			// PS1 mode: If the controller is still in digital mode, it is time to stop acknowledging.
			if (this->currentMode == Pad::Mode::DIGITAL)
			{
				g_Sio0.SetAcknowledge(false);
			}

			return buttons & 0xff;
	}

	Console.Warning("%s(%02X) Did not reach a valid return path! Returning zero as a failsafe!", __FUNCTION__, commandByte);
	return 0x00;
}

u8 PadPopn::Config(u8 commandByte)
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
u8 PadPopn::ModeSwitch(u8 commandByte)
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

u8 PadPopn::StatusInfo(u8 commandByte)
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

u8 PadPopn::Constant1(u8 commandByte)
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

u8 PadPopn::Constant2(u8 commandByte)
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

u8 PadPopn::Constant3(u8 commandByte)
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
u8 PadPopn::VibrationMap(u8 commandByte)
{
	return 0xff;
}

u8 PadPopn::ResponseBytes(u8 commandByte)
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

PadPopn::PadPopn(u8 unifiedSlot, size_t ejectTicks)
	: PadBase(unifiedSlot, ejectTicks)
{
	currentMode = Pad::Mode::DIGITAL;
}

PadPopn::~PadPopn() = default;

Pad::ControllerType PadPopn::GetType() const
{
	return Pad::ControllerType::Popn;
}

const Pad::ControllerInfo& PadPopn::GetInfo() const
{
	return ControllerInfo;
}

void PadPopn::Set(u32 index, float value)
{
	if (index > Inputs::LENGTH)
	{
		return;
	}

	// Since we reordered the buttons for better UI, we need to remap them here.
	static constexpr std::array<u8, Inputs::LENGTH> bitmaskMapping = {{
		5, // PAD_YELLOW_LEFT
		12, // PAD_YELLOW_RIGHT
		6, // PAD_BLUE_LEFT
		7, // PAD_BLUE_RIGHT
		4, // PAD_WHITE_LEFT
		0, // PAD_WHITE_RIGHT
		3, // PAD_GREEN_LEFT
		1, // PAD_GREEN_RIGHT
		2, // PAD_RED
		11, // PAD_START
		8, // PAD_SELECT
	}};

	this->rawInputs[index] = static_cast<u8>(std::clamp(value * 255.0f, 0.0f, 255.0f));

	if (value)
	{
		this->buttons &= ~(1u << bitmaskMapping[index]);
	}
	else
	{
		this->buttons |= (1u << bitmaskMapping[index]);
	}
}

void PadPopn::SetRawAnalogs(const std::tuple<u8, u8> left, const std::tuple<u8, u8> right)
{
}

void PadPopn::SetAxisScale(float deadzone, float scale)
{
}

float PadPopn::GetVibrationScale(u32 motor) const
{
	return 0;
}

void PadPopn::SetVibrationScale(u32 motor, float scale)
{
}

float PadPopn::GetPressureModifier() const
{
	return 0;
}

void PadPopn::SetPressureModifier(float mod)
{
}

void PadPopn::SetButtonDeadzone(float deadzone)
{
}

void PadPopn::SetAnalogInvertL(bool x, bool y)
{
}

void PadPopn::SetAnalogInvertR(bool x, bool y)
{
}

float PadPopn::GetEffectiveInput(u32 index) const
{
	return GetRawInput(index) / 255.0f;
}

u8 PadPopn::GetRawInput(u32 index) const
{
	return rawInputs[index];
}

std::tuple<u8, u8> PadPopn::GetRawLeftAnalog() const
{
	return std::tuple<u8, u8>{0x7f, 0x7f};
}

std::tuple<u8, u8> PadPopn::GetRawRightAnalog() const
{
	return std::tuple<u8, u8>{0x7f, 0x7f};
}

u32 PadPopn::GetButtons() const
{
	// A quirk of the Pop'n controller, the "D-Pad" left right and down buttons are always pressed.
	// Likely this was a simple way to identify their controllers by always reporting this button combination,
	// since it was both impossible for a normal pad to do and quite impractical to imitate without
	// some level of hardware tampering. This also meant they could just have the pad identify as a seemingly
	// normal PS1 digital pad to work with the standard pad libraries across the board.
	return buttons & ~(0xE000);
}

u8 PadPopn::GetPressure(u32 index) const
{
	return 0;
}

bool PadPopn::Freeze(StateWrapper& sw)
{
	if (!PadBase::Freeze(sw) || !sw.DoMarker("PadPopn"))
		return false;

	// Private PadPopn members
	sw.Do(&analogLight);
	sw.Do(&analogLocked);
	sw.Do(&analogPressed);
	sw.Do(&commandStage);
	sw.Do(&responseBytes);
	return !sw.HasError();
}

u8 PadPopn::SendCommandByte(u8 commandByte)
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
