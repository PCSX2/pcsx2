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

#include "SIO/Pad/PadDualshock2.h"

#include "SIO/Pad/PadManager.h"
#include "SIO/Sio.h"
#include "SIO/Sio0.h"

#include "Common.h"
#include "Input/InputManager.h"
#include "Host.h"

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
}

u8 PadDualshock2::Poll(u8 commandByte)
{
	const u32 buttons = GetButtons();

	switch (commandBytesReceived)
	{
		case 3:
			this->vibrationMotors.at(0) = commandByte;
			return (buttons >> 8) & 0xff;
		case 4:
			this->vibrationMotors.at(1) = commandByte;
			InputManager::SetPadVibrationIntensity(this->unifiedSlot,
				std::min(static_cast<float>(this->vibrationMotors.at(0)) * GetVibrationScale(0) * (1.0f / 255.0f), 1.0f),
				std::min(static_cast<float>(this->vibrationMotors.at(1)) * GetVibrationScale(1) * (1.0f / 255.0f), 1.0f)
			);

			// PS1 mode: If the controller is still in digital mode, it is time to stop acknowledging.
			if (this->currentMode == Pad::Mode::DIGITAL)
			{
				g_Sio0.SetAcknowledge(false);
			}

			return buttons & 0xff;
		case 5:
			return GetPressure(Dualshock2::Inputs::PAD_R_RIGHT);
		case 6:
			return GetPressure(Dualshock2::Inputs::PAD_R_UP);
		case 7:
			return GetPressure(Dualshock2::Inputs::PAD_L_RIGHT);
		case 8:
			// PS1 mode: If the controller reaches this byte, it is in analog mode and has irrefutably reached the last byte.
			// There's simply nothing to check, we know it's done and time to stop acknowledgements.
			g_Sio0.SetAcknowledge(false);
			return GetPressure(Dualshock2::Inputs::PAD_L_UP);
		case 9:
			return IsButtonBitSet(buttons, 13) ? GetPressure(Dualshock2::Inputs::PAD_RIGHT) : 0;
		case 10:
			return IsButtonBitSet(buttons, 15) ? GetPressure(Dualshock2::Inputs::PAD_LEFT) : 0;
		case 11:
			return IsButtonBitSet(buttons, 12) ? GetPressure(Dualshock2::Inputs::PAD_UP) : 0;
		case 12:
			return IsButtonBitSet(buttons, 14) ? GetPressure(Dualshock2::Inputs::PAD_DOWN) : 0;
		case 13:
			return IsButtonBitSet(buttons, 4) ? GetPressure(Dualshock2::Inputs::PAD_TRIANGLE) : 0;
		case 14:
			return IsButtonBitSet(buttons, 5) ? GetPressure(Dualshock2::Inputs::PAD_CIRCLE) : 0;
		case 15:
			return IsButtonBitSet(buttons, 6) ? GetPressure(Dualshock2::Inputs::PAD_CROSS) : 0;
		case 16:
			return IsButtonBitSet(buttons, 7) ? GetPressure(Dualshock2::Inputs::PAD_SQUARE) : 0;
		case 17:
			return IsButtonBitSet(buttons, 2) ? GetPressure(Dualshock2::Inputs::PAD_L1) : 0;
		case 18:
			return IsButtonBitSet(buttons, 3) ? GetPressure(Dualshock2::Inputs::PAD_R1) : 0;
		case 19:
			return IsButtonBitSet(buttons, 0) ? GetPressure(Dualshock2::Inputs::PAD_L2) : 0;
		case 20:
			return IsButtonBitSet(buttons, 1) ? GetPressure(Dualshock2::Inputs::PAD_R2) : 0;
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
				const auto [port, slot] = sioConvertPadToPortAndSlot(unifiedSlot);
				Console.WriteLn(StringUtil::StdStringFromFormat("[Pad] Game finished pad setup for port %d / slot %d - Analogs: %s - Analog Button: %s - Pressure: %s",
					port + 1,
					slot + 1,
					(this->analogLight ? "On" : "Off"),
					(this->analogLocked ? "Locked" : "Usable"),
					(this->responseBytes == static_cast<u32>(Pad::ResponseBytes::DUALSHOCK2) ? "On" : "Off")));
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

u8 PadDualshock2::VibrationMap(u8 commandByte)
{
	switch (commandBytesReceived)
	{
		case 3:
			return 0x00;
		case 4:
			return 0x01;
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

PadDualshock2::PadDualshock2(u8 unifiedSlot)
	: PadBase(unifiedSlot)
{
	this->currentMode = Pad::Mode::DIGITAL;
	Init();
}

PadDualshock2::~PadDualshock2() = default;

void PadDualshock2::Init()
{
	this->buttons = 0xffffffff;
	this->analogs.lx = Pad::ANALOG_NEUTRAL_POSITION;
	this->analogs.ly = Pad::ANALOG_NEUTRAL_POSITION;
	this->analogs.rx = Pad::ANALOG_NEUTRAL_POSITION;
	this->analogs.ry = Pad::ANALOG_NEUTRAL_POSITION;
	this->analogs.lxInvert = 0;
	this->analogs.lyInvert = 0;
	this->analogs.rxInvert = 0;
	this->analogs.ryInvert = 0;
	this->analogLight = false;
	this->analogLocked = false;
	this->analogPressed = false;
	this->responseBytes = 0;

	for (u8 i = 0; i < this->rawInputs.size(); i++)
	{
		this->rawInputs.at(i) = 0;
	}

	for (u8 i = 0; i < this->pressures.size(); i++)
	{
		this->pressures.at(i) = 0;
	}

	this->axisScale = 1.0f;
	this->axisDeadzone = 0.0f;
	
	this->vibrationScale.at(0) = 0.0f;
	this->vibrationScale.at(1) = 1.0f;

	this->pressureModifier = 0.5f;
	this->buttonDeadzone = 0.0f;
}

Pad::ControllerType PadDualshock2::GetType() 
{
	return Pad::ControllerType::DualShock2;
}

void PadDualshock2::Set(u32 index, float value)
{
	if (index > Dualshock2::Inputs::LENGTH)
	{
		return;
	}

	// Since we reordered the buttons for better UI, we need to remap them here.
	static constexpr std::array<u8, Dualshock2::Inputs::LENGTH> bitmaskMapping = {{
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
		if (index <= Dualshock2::Inputs::PAD_L_LEFT)
		{
			// Left Stick
			this->analogs.lx = this->analogs.lxInvert ? MERGE(Dualshock2::Inputs::PAD_L_LEFT, Dualshock2::Inputs::PAD_L_RIGHT) : MERGE(Dualshock2::Inputs::PAD_L_RIGHT, Dualshock2::Inputs::PAD_L_LEFT);
			this->analogs.ly = this->analogs.lyInvert ? MERGE(Dualshock2::Inputs::PAD_L_UP, Dualshock2::Inputs::PAD_L_DOWN) : MERGE(Dualshock2::Inputs::PAD_L_DOWN, Dualshock2::Inputs::PAD_L_UP);
		}
		else
		{
			// Right Stick
			this->analogs.rx = this->analogs.rxInvert ? MERGE(Dualshock2::Inputs::PAD_R_LEFT, Dualshock2::Inputs::PAD_R_RIGHT) : MERGE(Dualshock2::Inputs::PAD_R_RIGHT, Dualshock2::Inputs::PAD_R_LEFT);
			this->analogs.ry = this->analogs.ryInvert ? MERGE(Dualshock2::Inputs::PAD_R_UP, Dualshock2::Inputs::PAD_R_DOWN) : MERGE(Dualshock2::Inputs::PAD_R_DOWN, Dualshock2::Inputs::PAD_R_UP);
		}
#undef MERGE

		// Deadzone computation.
		const float dz = this->axisDeadzone;

		if (dz > 0.0f)
		{
#define MERGE_F(pos, neg) ((this->rawInputs[pos] != 0) ? (static_cast<float>(this->rawInputs[pos]) / 255.0f) : (static_cast<float>(this->rawInputs[neg]) / -255.0f))
			float posX, posY;
			if (index <= Dualshock2::Inputs::PAD_L_LEFT)
			{
				posX = this->analogs.lxInvert ? MERGE_F(Dualshock2::Inputs::PAD_L_LEFT, Dualshock2::Inputs::PAD_L_RIGHT) : MERGE_F(Dualshock2::Inputs::PAD_L_RIGHT, Dualshock2::Inputs::PAD_L_LEFT);
				posY = this->analogs.lyInvert ? MERGE_F(Dualshock2::Inputs::PAD_L_UP, Dualshock2::Inputs::PAD_L_DOWN) : MERGE_F(Dualshock2::Inputs::PAD_L_DOWN, Dualshock2::Inputs::PAD_L_UP);
			}
			else
			{
				posX = this->analogs.rxInvert ? MERGE_F(Dualshock2::Inputs::PAD_R_LEFT, Dualshock2::Inputs::PAD_R_RIGHT) : MERGE_F(Dualshock2::Inputs::PAD_R_RIGHT, Dualshock2::Inputs::PAD_R_LEFT);
				posY = this->analogs.ryInvert ? MERGE_F(Dualshock2::Inputs::PAD_R_UP, Dualshock2::Inputs::PAD_R_DOWN) : MERGE_F(Dualshock2::Inputs::PAD_R_DOWN, Dualshock2::Inputs::PAD_R_UP);
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
					if (index <= Dualshock2::Inputs::PAD_L_LEFT)
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
		const float s_value = std::clamp(value * this->triggerScale, 0.0f, 1.0f);
		const float dz_value = (this->triggerDeadzone > 0.0f && s_value < this->triggerDeadzone) ? 0.0f : s_value;
		this->rawInputs[index] = static_cast<u8>(dz_value * 255.0f);
		if (dz_value > 0.0f)
			this->buttons &= ~(1u << bitmaskMapping[index]);
		else
			this->buttons |= (1u << bitmaskMapping[index]);
	}
	else
	{
		// Don't affect L2/R2, since they are analog on most pads.
		const float pMod = ((this->buttons & (1u << Dualshock2::Inputs::PAD_PRESSURE)) == 0 && !IsTriggerKey(index)) ? this->pressureModifier : 1.0f;
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
		if (index == Dualshock2::Inputs::PAD_PRESSURE)
		{
			const float adjustPMod = ((this->buttons & (1u << Dualshock2::Inputs::PAD_PRESSURE)) == 0) ? this->pressureModifier : (1.0f / this->pressureModifier);

			for (u32 i = 0; i < Dualshock2::Inputs::LENGTH; i++)
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

		if (index == Dualshock2::Inputs::PAD_ANALOG && !this->analogPressed && value > 0)
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
					fmt::format(TRANSLATE_FS("Pad", "Analog light is now {} for port {} / slot {}"),
						(this->analogLight ? "On" : "Off"),
						port + 1,
						slot + 1),
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

void PadDualshock2::SetTriggerScale(float deadzone, float scale)
{
	this->triggerDeadzone = deadzone;
	this->triggerScale = scale;
}

float PadDualshock2::GetVibrationScale(u32 motor)
{
	return this->vibrationScale[motor];
}

void PadDualshock2::SetVibrationScale(u32 motor, float scale)
{
	this->vibrationScale[motor] = scale;
}

float PadDualshock2::GetPressureModifier()
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

u8 PadDualshock2::GetRawInput(u32 index)
{
	return this->rawInputs[index];
}

std::tuple<u8, u8> PadDualshock2::GetRawLeftAnalog()
{
	return {this->analogs.lx, this->analogs.ly};
}

std::tuple<u8, u8> PadDualshock2::GetRawRightAnalog()
{
	return {this->analogs.rx, this->analogs.ry};
}

u32 PadDualshock2::GetButtons()
{
	return this->buttons;
}

u8 PadDualshock2::GetPressure(u32 index)
{
	switch (index)
	{
		case Dualshock2::Inputs::PAD_R_LEFT:
		case Dualshock2::Inputs::PAD_R_RIGHT:
			return this->analogs.rx;
		case Dualshock2::Inputs::PAD_R_DOWN:
		case Dualshock2::Inputs::PAD_R_UP:
			return this->analogs.ry;
		case Dualshock2::Inputs::PAD_L_LEFT:
		case Dualshock2::Inputs::PAD_L_RIGHT:
			return this->analogs.lx;
		case Dualshock2::Inputs::PAD_L_DOWN:
		case Dualshock2::Inputs::PAD_L_UP:
			return this->analogs.ly;
		default:
			return this->rawInputs.at(index);
	}
}

void PadDualshock2::Freeze(StateWrapper& sw)
{
	// Protected PadBase members
	sw.Do(&rawInputs);
	sw.Do(&unifiedSlot);
	sw.Do(&isInConfig);
	sw.Do(&currentMode);
	sw.Do(&currentCommand);
	sw.Do(&commandBytesReceived);

	// Private PadDualshock2 members
	sw.Do(&buttons);
	sw.DoBytes(&analogs, sizeof(Dualshock2::Analogs));
	sw.Do(&analogLight);
	sw.Do(&analogLocked);
	sw.Do(&analogPressed);
	sw.Do(&commandStage);
	sw.Do(&responseBytes);
	sw.Do(&pressures);
	sw.Do(&vibrationMotors);
	sw.Do(&axisScale);
	sw.Do(&axisDeadzone);
	sw.Do(&triggerScale);
	sw.Do(&triggerDeadzone);
	sw.Do(&vibrationScale);
	sw.Do(&pressureModifier);
	sw.Do(&buttonDeadzone);
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
