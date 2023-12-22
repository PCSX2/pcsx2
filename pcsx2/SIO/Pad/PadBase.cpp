// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SIO/Pad/PadBase.h"

PadBase::PadBase(u8 unifiedSlot, size_t ejectTicks)
{
	this->unifiedSlot = unifiedSlot;
	this->ejectTicks = ejectTicks;
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
	sw.Do(&unifiedSlot);
	sw.Do(&isInConfig);
	sw.Do(&currentMode);
	sw.Do(&currentCommand);
	sw.Do(&commandBytesReceived);
	return !sw.HasError();
}
