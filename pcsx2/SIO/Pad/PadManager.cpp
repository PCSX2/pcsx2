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

#include "SIO/Pad/PadConfig.h"
#include "SIO/Pad/PadDualshock2.h"
#include "SIO/Pad/PadGuitar.h"
#include "SIO/Pad/PadManager.h"
#include "SIO/Pad/PadNotConnected.h"
#include "SIO/Sio.h"

#include "Host.h"
#include "IconsFontAwesome5.h"

#include "fmt/format.h"

namespace Pad
{
	static std::array<std::unique_ptr<PadBase>, NUM_CONTROLLER_PORTS> s_controllers;
}

bool Pad::Initialize()
{
	return true;
}

void Pad::Shutdown()
{
	for (auto& port : s_controllers)
		port.reset();
}

std::unique_ptr<PadBase> Pad::CreatePad(u8 unifiedSlot, ControllerType controllerType)
{
	switch (controllerType)
	{
		case ControllerType::DualShock2:
			return std::make_unique<PadDualshock2>(unifiedSlot);
		case ControllerType::Guitar:
			return std::make_unique<PadGuitar>(unifiedSlot);
		default:
			return std::make_unique<PadNotConnected>(unifiedSlot);
	}
}

PadBase* Pad::ChangePadType(u8 unifiedSlot, ControllerType controllerType)
{
	s_controllers[unifiedSlot] = CreatePad(unifiedSlot, controllerType);
	return s_controllers[unifiedSlot].get();
}

bool Pad::HasConnectedPad(u8 unifiedSlot)
{
	return (
		unifiedSlot < NUM_CONTROLLER_PORTS && s_controllers[unifiedSlot]->GetType() != ControllerType::NotConnected);
}

PadBase* Pad::GetPad(u8 port, u8 slot)
{
	const u8 unifiedSlot = sioConvertPortAndSlotToPad(port, slot);
	return s_controllers[unifiedSlot].get();
}

PadBase* Pad::GetPad(const u8 unifiedSlot)
{
	return s_controllers[unifiedSlot].get();
}

void Pad::SetControllerState(u32 controller, u32 bind, float value)
{
	if (controller >= NUM_CONTROLLER_PORTS)
		return;

	s_controllers[controller]->Set(bind, value);
}

bool Pad::Freeze(StateWrapper& sw)
{
	if (sw.IsReading())
	{
		if (!sw.DoMarker("PAD"))
		{
			Console.Error("PAD state is invalid! Leaving the current state in place.");
			return false;
		}

		for (u32 unifiedSlot = 0; unifiedSlot < NUM_CONTROLLER_PORTS; unifiedSlot++)
		{
			ControllerType type;
			sw.Do(&type);
			if (sw.HasError())
				return false;

			std::unique_ptr<PadBase> tempPad;
			PadBase* pad = GetPad(unifiedSlot);
			if (!pad || pad->GetType() != type)
			{
				const auto& [port, slot] = sioConvertPadToPortAndSlot(unifiedSlot);
				Host::AddIconOSDMessage(fmt::format("UnfreezePad{}Changed", unifiedSlot), ICON_FA_GAMEPAD,
					fmt::format(TRANSLATE_FS("Pad",
									"Controller port {}, slot {} has a {} connected, but the save state has a "
									"{}.\nLeaving the original controller type connected, but this may cause issues."),
						port, slot,
						Pad::GetControllerTypeName(pad ? pad->GetType() : Pad::ControllerType::NotConnected),
						Pad::GetControllerTypeName(type)));

				// Reset the transfer etc state of the pad, at least it has a better chance of surviving.
				pad->SoftReset();

				// But we still need to pull the data from the state..
				tempPad = CreatePad(unifiedSlot, type);
				pad = tempPad.get();
			}

			if (!pad->Freeze(sw))
				return false;
		}
	}
	else
	{
		if (!sw.DoMarker("PAD"))
			return false;

		for (u32 unifiedSlot = 0; unifiedSlot < NUM_CONTROLLER_PORTS; unifiedSlot++)
		{
			PadBase* pad = GetPad(unifiedSlot);
			ControllerType type = pad->GetType();
			sw.Do(&type);
			if (sw.HasError() || !pad->Freeze(sw))
				return false;
		}
	}

	return !sw.HasError();
}
