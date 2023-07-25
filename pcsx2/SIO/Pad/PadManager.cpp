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

#include "SIO/Pad/PadManager.h"
#include "SIO/Pad/PadConfig.h"
#include "SIO/Pad/PadNotConnected.h"
#include "SIO/Pad/PadDualshock2.h"
#include "SIO/Pad/PadGuitar.h"
#include "SIO/Sio.h"

#include "Host.h"
#include "IconsFontAwesome5.h"

#include "fmt/format.h"

PadManager g_PadManager;

PadManager::PadManager() = default;
PadManager::~PadManager() = default;

bool PadManager::Initialize()
{
	return true;
}

bool PadManager::Shutdown()
{
	for (u8 i = 0; i < 8; i++)
	{
		this->ps2Controllers[i] = nullptr;
	}

	return true;
}

std::unique_ptr<PadBase> PadManager::CreatePad(u8 unifiedSlot, Pad::ControllerType controllerType)
{
	switch (controllerType)
	{
		case Pad::ControllerType::DualShock2:
			return std::make_unique<PadDualshock2>(unifiedSlot);
		case Pad::ControllerType::Guitar:
			return std::make_unique<PadGuitar>(unifiedSlot);
		default:
			return std::make_unique<PadNotConnected>(unifiedSlot);
	}
}

PadBase* PadManager::ChangePadType(u8 unifiedSlot, Pad::ControllerType controllerType)
{
	this->ps2Controllers[unifiedSlot] = CreatePad(unifiedSlot, controllerType);
	return this->ps2Controllers[unifiedSlot].get();
}

PadBase* PadManager::GetPad(u8 port, u8 slot)
{
	const u8 unifiedSlot = sioConvertPortAndSlotToPad(port, slot);
	return this->ps2Controllers[unifiedSlot].get();
}

PadBase* PadManager::GetPad(const u8 unifiedSlot)
{
	return this->ps2Controllers[unifiedSlot].get();
}

void PadManager::SetControllerState(u32 controller, u32 bind, float value)
{
	if (controller >= Pad::NUM_CONTROLLER_PORTS)
		return;

	PadBase* pad = g_PadManager.GetPad(controller);
	pad->Set(bind, value);
}

bool PadManager::PadFreeze(StateWrapper& sw)
{
	if (sw.IsReading())
	{
		if (!sw.DoMarker("PAD"))
		{
			Console.Error("PAD state is invalid! Leaving the current state in place.");
			return false;
		}

		for (u32 unifiedSlot = 0; unifiedSlot < Pad::NUM_CONTROLLER_PORTS; unifiedSlot++)
		{
			Pad::ControllerType type;
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
						g_PadConfig.GetControllerTypeName(pad ? pad->GetType() : Pad::ControllerType::NotConnected),
						g_PadConfig.GetControllerTypeName(type)));

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
		{
			return false;
		}

		for (u32 unifiedSlot = 0; unifiedSlot < Pad::NUM_CONTROLLER_PORTS; unifiedSlot++)
		{
			PadBase* pad = this->GetPad(unifiedSlot);
			Pad::ControllerType type = pad->GetType();
			sw.Do(&type);
			if (sw.HasError() || !pad->Freeze(sw))
				return false;
		}
	}

	return !sw.HasError();
}
