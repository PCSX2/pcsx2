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

#include "SIO/Pad/PadBase.h"

#include <array>

namespace Pad
{
	bool Initialize();
	void Shutdown();
	
	std::unique_ptr<PadBase> CreatePad(u8 unifiedSlot, Pad::ControllerType controllerType);
	PadBase* ChangePadType(u8 unifiedSlot, Pad::ControllerType controllerType);
	bool HasConnectedPad(u8 unifiedSlot);

	PadBase* GetPad(u8 port, u8 slot);
	PadBase* GetPad(const u8 unifiedSlot);

	// Sets the specified bind on a controller to the specified pressure (normalized to 0..1).
	void SetControllerState(u32 controller, u32 bind, float value);
	
	bool Freeze(StateWrapper& sw);
};
