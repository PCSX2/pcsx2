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

#include "SIO/Pad/PadGuitar.h"

#include "SIO/Pad/PadManager.h"
#include "SIO/Pad/PadGuitarTypes.h"
#include "SIO/Sio.h"

#include "Common.h"

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
			return GetPressure(Guitar::Inputs::WHAMMY);
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
				const auto [port, slot] = sioConvertPadToPortAndSlot(unifiedSlot);
				Console.WriteLn(StringUtil::StdStringFromFormat("[Pad] Game finished pad setup for port %d / slot %d - Analogs: %s - Analog Button: %s - Pressure: Not available on guitars",
					port + 1,
					slot + 1,
					(this->analogLight ? "On" : "Off"),
					(this->analogLocked ? "Locked" : "Usable")));
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

PadGuitar::PadGuitar(u8 unifiedSlot)
	: PadBase(unifiedSlot)
{
	this->currentMode = Pad::Mode::DIGITAL;
	Init();
}

PadGuitar::~PadGuitar() = default;

void PadGuitar::Init()
{
	this->buttons = 0xffffffff;
	this->whammy = Pad::ANALOG_NEUTRAL_POSITION;
	this->analogLight = false;
	this->analogLocked = false;
	this->whammyAxisScale = 1.0f;
	this->whammyDeadzone = 0.0f;
}

Pad::ControllerType PadGuitar::GetType()
{
	return Pad::ControllerType::Guitar;
}

void PadGuitar::Set(u32 index, float value)
{
	if (index > Guitar::Inputs::LENGTH)
	{
		return;
	}

	// The whammy bar is a special kind of weird in that rather than resting at 0 and going to 255,
	// they chose to rest it at 127 like a normal analog, but then also make its full press 0, as if
	// it were the negative Y component of a normal analog. Fun!
	if (index == Guitar::Inputs::WHAMMY)
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
		static constexpr std::array<u8, Guitar::Inputs::LENGTH> bitmaskMapping = {{
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

void PadGuitar::SetTriggerScale(float deadzone, float scale)
{

}

float PadGuitar::GetVibrationScale(u32 motor)
{
	return 0;
}

void PadGuitar::SetVibrationScale(u32 motor, float scale)
{
}

float PadGuitar::GetPressureModifier()
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

u8 PadGuitar::GetRawInput(u32 index)
{
	return this->rawInputs[index];
}

std::tuple<u8, u8> PadGuitar::GetRawLeftAnalog()
{
	return std::tuple<u8, u8>{0x7f, 0x7f};
}

std::tuple<u8, u8> PadGuitar::GetRawRightAnalog()
{
	return std::tuple<u8, u8>{0x7f, 0x7f};
}

u32 PadGuitar::GetButtons()
{
	return this->buttons;
}

u8 PadGuitar::GetPressure(u32 index)
{
	if (index == Guitar::Inputs::WHAMMY)
	{
		return this->whammy;
	}
	
	return 0;
}

void PadGuitar::Freeze(StateWrapper& sw)
{
	// Protected PadBase members
	sw.Do(&rawInputs);
	sw.Do(&unifiedSlot);
	sw.Do(&isInConfig);
	sw.Do(&currentMode);
	sw.Do(&currentCommand);
	sw.Do(&commandBytesReceived);

	// Private PadGuitar members
	sw.Do(&buttons);
	sw.Do(&whammy);
	sw.Do(&analogLight);
	sw.Do(&analogLocked);
	sw.Do(&commandStage);
	sw.Do(&whammyAxisScale);
	sw.Do(&whammyDeadzone);
	sw.Do(&buttonDeadzone);
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
