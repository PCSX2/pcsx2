// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SIO/Pad/PadGuitar.h"
#include "SIO/Pad/Pad.h"
#include "SIO/Sio.h"

#include "Host.h"

#include "IconsPromptFont.h"

#include "common/Console.h"

// The generic input bindings on this might seem bizarre, but they are intended to match what DS2 buttons
// would do what actions, if you played Guitar Hero on a PS2 with a DS2 instead of a controller.
static const InputBindingInfo s_bindings[] = {
	// clang-format off
	{"Up", TRANSLATE_NOOP("Pad", "Strum Up"), nullptr, InputBindingInfo::Type::Button, PadGuitar::Inputs::STRUM_UP, GenericInputBinding::DPadUp},
	{"Down", TRANSLATE_NOOP("Pad", "Strum Down"), nullptr, InputBindingInfo::Type::Button, PadGuitar::Inputs::STRUM_DOWN, GenericInputBinding::DPadDown},
	{"Select", TRANSLATE_NOOP("Pad", "Select"), nullptr, InputBindingInfo::Type::Button, PadGuitar::Inputs::SELECT, GenericInputBinding::Select},
	{"Start", TRANSLATE_NOOP("Pad", "Start"), nullptr, InputBindingInfo::Type::Button, PadGuitar::Inputs::START, GenericInputBinding::Start},
	{"Green", TRANSLATE_NOOP("Pad", "Green Fret"), nullptr, InputBindingInfo::Type::Button, PadGuitar::Inputs::GREEN, GenericInputBinding::R2},
	{"Red", TRANSLATE_NOOP("Pad", "Red Fret"), nullptr, InputBindingInfo::Type::Button, PadGuitar::Inputs::RED, GenericInputBinding::Circle},
	{"Yellow", TRANSLATE_NOOP("Pad", "Yellow Fret"), nullptr, InputBindingInfo::Type::Button, PadGuitar::Inputs::YELLOW, GenericInputBinding::Triangle},
	{"Blue", TRANSLATE_NOOP("Pad", "Blue Fret"), nullptr, InputBindingInfo::Type::Button, PadGuitar::Inputs::BLUE, GenericInputBinding::Cross},
	{"Orange", TRANSLATE_NOOP("Pad", "Orange Fret"), nullptr, InputBindingInfo::Type::Button, PadGuitar::Inputs::ORANGE, GenericInputBinding::Square},
	{"Whammy", TRANSLATE_NOOP("Pad", "Whammy Bar"), nullptr, InputBindingInfo::Type::HalfAxis, PadGuitar::Inputs::WHAMMY, GenericInputBinding::LeftStickUp},
	{"Tilt", TRANSLATE_NOOP("Pad", "Tilt Up"), nullptr, InputBindingInfo::Type::Button, PadGuitar::Inputs::TILT, GenericInputBinding::L2},
	// clang-format on
};

static const SettingInfo s_settings[] = {
	{SettingInfo::Type::Float, "Deadzone", TRANSLATE_NOOP("Pad", "Whammy Bar Deadzone"),
		TRANSLATE_NOOP("Pad", "Sets the whammy bar deadzone. Inputs below this value will not be sent to the PS2."),
		"0.00", "0.00", "1.00", "0.01", "%.0f%%", nullptr, nullptr, 100.0f},
	{SettingInfo::Type::Float, "AxisScale", TRANSLATE_NOOP("Pad", "Whammy Bar Sensitivity"),
		TRANSLATE_NOOP("Pad", "Sets the whammy bar axis scaling factor."), "1.0", "0.01", "2.00", "0.01", "%.0f%%",
		nullptr, nullptr, 100.0f},
};

const Pad::ControllerInfo PadGuitar::ControllerInfo = {Pad::ControllerType::Guitar, "Guitar",
	TRANSLATE_NOOP("Pad", "Guitar"), ICON_PF_GUITAR, s_bindings, s_settings, Pad::VibrationCapabilities::NoVibration};

void PadGuitar::ConfigLog()
{
	const auto [port, slot] = sioConvertPadToPortAndSlot(unifiedSlot);

	// AL: Analog Light (is it turned on right now)
	// AB: Analog Button (is it useable or is it locked in its current state)
	Console.WriteLn(fmt::format("[Pad] Guitar Config Finished - P{0}/S{1} - AL: {2} - AB: {3}",
		port + 1,
		slot + 1,
		(this->analogLight ? "On" : "Off"),
		(this->analogLocked ? "Locked" : "Usable")));
}

u8 PadGuitar::Mystery(u8 commandByte)
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

u8 PadGuitar::ButtonQuery(u8 commandByte)
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

u8 PadGuitar::Poll(u8 commandByte)
{
	const u32 buttons = GetButtons();

	switch (this->commandBytesReceived)
	{
		case 3:
			return (buttons >> 8) & 0x7f;
		case 4:
			return buttons & 0xff;
		case 5:
			return 0x7f;
		case 6:
			return 0x7f;
		case 7:
			return 0x7f;
		case 8:
			return GetPressure(Inputs::WHAMMY);
	}

	Console.Warning("%s(%02X) Did not reach a valid return path! Returning zero as a failsafe!", __FUNCTION__, commandByte);
	return 0x00;
}

u8 PadGuitar::Config(u8 commandByte)
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
u8 PadGuitar::ModeSwitch(u8 commandByte)
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

u8 PadGuitar::StatusInfo(u8 commandByte)
{
	switch (this->commandBytesReceived)
	{
		case 3:
			return static_cast<u8>(Pad::PhysicalType::GUITAR);
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

u8 PadGuitar::Constant1(u8 commandByte)
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

u8 PadGuitar::Constant2(u8 commandByte)
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

u8 PadGuitar::Constant3(u8 commandByte)
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

u8 PadGuitar::VibrationMap(u8 commandByte)
{
	return 0xff;
}

PadGuitar::PadGuitar(u8 unifiedSlot, size_t ejectTicks)
	: PadBase(unifiedSlot, ejectTicks)
{
	currentMode = Pad::Mode::DIGITAL;
}

PadGuitar::~PadGuitar() = default;

Pad::ControllerType PadGuitar::GetType() const
{
	return Pad::ControllerType::Guitar;
}

const Pad::ControllerInfo& PadGuitar::GetInfo() const
{
	return ControllerInfo;
}

void PadGuitar::Set(u32 index, float value)
{
	if (index > Inputs::LENGTH)
	{
		return;
	}

	// The whammy bar is a special kind of weird in that rather than resting at 0 and going to 255,
	// they chose to rest it at 127 like a normal analog, but then also make its full press 0, as if
	// it were the negative Y component of a normal analog. Fun!
	if (index == Inputs::WHAMMY)
	{
		this->whammy = static_cast<u8>(std::clamp(127 - (value * this->whammyAxisScale) * 255.0f, 0.0f, 127.0f));

		if (this->whammyDeadzone > 0.0f)
		{
			// Whammy has a range of 0x7f to 0x00, since it is only half of an axis with no ability to go the
			// other direction. So whatever we get in, we basically need to cut half of that off in order to
			// figure out where our deadzone truly lives. I think.
			const float whammyF = (static_cast<float>(this->whammy - 127.0f) / 127.0f);

			if (whammyF != 0.0f && whammyF <= this->whammyDeadzone)
			{
				this->whammy = 0x7f;
			}
		}
	}
	else
	{
		// Don't affect L2/R2, since they are analog on most pads.
		const float dzValue = (value < this->buttonDeadzone) ? 0.0f : value;
		this->rawInputs[index] = static_cast<u8>(std::clamp(dzValue * 255.0f, 0.0f, 255.0f));

		// Since we reordered the buttons for better UI, we need to remap them here.
		static constexpr std::array<u8, Inputs::LENGTH> bitmaskMapping = {{
			12, // STRUM_UP
			14, // STRUM_DOWN
			8, // SELECT
			11, // START
			1, // GREEN
			5, // RED
			4, // YELLOW
			6, // BLUE
			7, // ORANGE
			0 // TILT
		}};

		if (dzValue > 0.0f)
		{
			this->buttons &= ~(1u << bitmaskMapping[index]);
		}
		else
		{
			this->buttons |= (1u << bitmaskMapping[index]);
		}
	}
}

void PadGuitar::SetRawAnalogs(const std::tuple<u8, u8> left, const std::tuple<u8, u8> right)
{
}

void PadGuitar::SetAxisScale(float deadzone, float scale)
{
	this->whammyDeadzone = deadzone;
	this->whammyAxisScale = scale;
}

float PadGuitar::GetVibrationScale(u32 motor) const
{
	return 0;
}

void PadGuitar::SetVibrationScale(u32 motor, float scale)
{
}

float PadGuitar::GetPressureModifier() const
{
	return 0;
}

void PadGuitar::SetPressureModifier(float mod)
{
}

void PadGuitar::SetButtonDeadzone(float deadzone)
{
	this->buttonDeadzone = deadzone;
}

void PadGuitar::SetAnalogInvertL(bool x, bool y)
{
}

void PadGuitar::SetAnalogInvertR(bool x, bool y)
{
}

float PadGuitar::GetEffectiveInput(u32 index) const
{
	return GetRawInput(index) / 255.0f;
}

u8 PadGuitar::GetRawInput(u32 index) const
{
	return rawInputs[index];
}

std::tuple<u8, u8> PadGuitar::GetRawLeftAnalog() const
{
	return std::tuple<u8, u8>{0x7f, 0x7f};
}

std::tuple<u8, u8> PadGuitar::GetRawRightAnalog() const
{
	return std::tuple<u8, u8>{0x7f, 0x7f};
}

u32 PadGuitar::GetButtons() const
{
	return buttons;
}

u8 PadGuitar::GetPressure(u32 index) const
{
	if (index == Inputs::WHAMMY)
		return whammy;
	
	return 0;
}

bool PadGuitar::Freeze(StateWrapper& sw)
{
	if (!PadBase::Freeze(sw) || !sw.DoMarker("PadGuitar"))
		return false;

	// Private PadGuitar members
	sw.Do(&whammy);
	sw.Do(&analogLight);
	sw.Do(&analogLocked);
	sw.Do(&commandStage);
	sw.Do(&whammyAxisScale);
	sw.Do(&whammyDeadzone);
	sw.Do(&buttonDeadzone);
	return !sw.HasError();
}

u8 PadGuitar::SendCommandByte(u8 commandByte)
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
