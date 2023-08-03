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

#include "SIO/Pad/PadBase.h"

PadBase::PadBase(u8 unifiedSlot)
{
	this->unifiedSlot = unifiedSlot;
}

PadBase::~PadBase() = default;

void PadBase::SoftReset()
{
	commandBytesReceived = 1;
}

void PadBase::FullReset()
{
	this->isInConfig = false;
	this->currentMode = Pad::Mode::DIGITAL;
}

bool PadBase::Freeze(StateWrapper& sw)
{
	if (!sw.DoMarker("PadBase"))
		return false;

	// Protected PadBase members
	sw.Do(&rawInputs);
	sw.Do(&unifiedSlot);
	sw.Do(&isInConfig);
	sw.Do(&currentMode);
	sw.Do(&currentCommand);
	sw.Do(&commandBytesReceived);
	return !sw.HasError();
}
