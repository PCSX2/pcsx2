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

#include "SIO/Pad/PadNotConnected.h"
#include "SIO/Pad/PadDualshock2.h"
#include "SIO/Pad/PadGuitar.h"

PadManager g_PadManager;

// Convert the PS2's port/slot addressing to a single value.
// Physical ports 0 and 1 still correspond to unified slots 0 and 1.
// The remaining unified slots are for multitapped slots.
// Port 0's three multitap slots then occupy unified slots 2, 3 and 4.
// Port 1's three multitap slots then occupy unified slots 5, 6 and 7.
u8 PadManager::GetUnifiedSlot(u8 port, u8 slot)
{
	if (slot == 0)
	{
		return port;
	}
	else if (port == 0) // slot=[0,1]
	{
		return slot + 1;
	}
	else
	{
		return slot + 4;
	}
}

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
		this->ps2Controllers.at(i) = nullptr;
	}

	return true;
}

PadBase* PadManager::ChangePadType(u8 unifiedSlot, Pad::ControllerType controllerType)
{
	switch (controllerType)
	{
		case Pad::ControllerType::DualShock2:
			this->ps2Controllers.at(unifiedSlot) = std::make_unique<PadDualshock2>(unifiedSlot);
			break;
		case Pad::ControllerType::Guitar:
			this->ps2Controllers.at(unifiedSlot) = std::make_unique<PadGuitar>(unifiedSlot);
			break;
		default:
			this->ps2Controllers.at(unifiedSlot) = std::make_unique<PadNotConnected>(unifiedSlot);
			break;
	}

	return this->ps2Controllers.at(unifiedSlot).get();
}

PadBase* PadManager::GetPad(u8 port, u8 slot)
{
	const u8 unifiedSlot = this->GetUnifiedSlot(port, slot);
	return this->ps2Controllers.at(unifiedSlot).get();
}

PadBase* PadManager::GetPad(const u8 unifiedSlot)
{
	return this->ps2Controllers.at(unifiedSlot).get();
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

			PadBase* pad = this->ChangePadType(unifiedSlot, type);
			pad->Freeze(sw);
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
			pad->Freeze(sw);
		}
	}

	return true;
}
