// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "SIO/Sio.h"

#include "SIO/SioTypes.h"
#include "SIO/Memcard/MemoryCardProtocol.h"

#include "Host.h"
#include "IconsPromptFont.h"

#include <atomic>

_mcd mcds[2][4];
_mcd *mcd;

void sioNextFrame() {
	for ( uint port = 0; port < 2; ++port ) {
		for ( uint slot = 0; slot < 4; ++slot ) {
			mcds[port][slot].NextFrame();
		}
	}
}

void sioSetGameSerial( const std::string& serial ) {
	for ( uint port = 0; port < 2; ++port ) {
		for ( uint slot = 0; slot < 4; ++slot ) {
			if ( mcds[port][slot].ReIndex( serial ) ) {
				AutoEject::Set( port, slot );
			}
		}
	}
}

std::tuple<u32, u32> sioConvertPadToPortAndSlot(u32 index)
{
	if (index > 4) // [5,6,7]
		return std::make_tuple(1, index - 4); // 2B,2C,2D
	else if (index > 1) // [2,3,4]
		return std::make_tuple(0, index - 1); // 1B,1C,1D
	else // [0,1]
		return std::make_tuple(index, 0); // 1A,2A
}

u32 sioConvertPortAndSlotToPad(u32 port, u32 slot)
{
	if (slot == 0)
		return port;
	else if (port == 0) // slot=[0,1]
		return slot + 1; // 2,3,4
	else
		return slot + 4; // 5,6,7
}

bool sioPadIsMultitapSlot(u32 index)
{
	return (index >= 2);
}

bool sioPortAndSlotIsMultitap(u32 port, u32 slot)
{
	return (slot != 0);
}

void AutoEject::CountDownTicks()
{
	bool reinserted = false;
	for (size_t port = 0; port < SIO::PORTS; port++)
	{
		for (size_t slot = 0; slot < SIO::SLOTS; slot++)
		{
			if (mcds[port][slot].autoEjectTicks > 0)
			{
				if (--mcds[port][slot].autoEjectTicks == 0)
					reinserted |= EmuConfig.Mcd[sioConvertPortAndSlotToPad(port, slot)].Enabled;
			}
		}
	}

	if (reinserted)
	{
		Host::AddIconOSDMessage("AutoEjectAllSet", ICON_PF_MEMORY_CARD,
			TRANSLATE_SV("MemoryCard", "Memory Cards reinserted."), Host::OSD_INFO_DURATION);
	}
}

void AutoEject::Set(size_t port, size_t slot)
{
	if (mcds[port][slot].autoEjectTicks == 0)
	{
		mcds[port][slot].autoEjectTicks = 60; // 60 frames is enough.
		mcds[port][slot].term = Terminator::NOT_READY; // Reset terminator to NOT_READY (0x66), forces the PS2 to recheck the memcard.
	}
}

void AutoEject::Clear(size_t port, size_t slot)
{
	mcds[port][slot].autoEjectTicks = 0;
}

void AutoEject::SetAll()
{
	Host::AddIconOSDMessage("AutoEjectAllSet", ICON_PF_MEMORY_CARD,
		TRANSLATE_SV("MemoryCard", "Force ejecting all Memory Cards. Reinserting in 1 second."), Host::OSD_INFO_DURATION);

	for (size_t port = 0; port < SIO::PORTS; port++)
	{
		for (size_t slot = 0; slot < SIO::SLOTS; slot++)
		{
			AutoEject::Set(port, slot);
		}
	}
}

void AutoEject::ClearAll()
{
	for (size_t port = 0; port < SIO::PORTS; port++)
	{
		for (size_t slot = 0; slot < SIO::SLOTS; slot++)
		{
			AutoEject::Clear(port, slot);
		}
	}
}

// Decremented once per frame if nonzero, indicates how many more frames must pass before
// memcards are considered "no longer being written to". Used as a way to detect if it is
// unsafe to shutdown the VM due to memcard access.
static std::atomic_uint32_t currentBusyTicks = 0;

void MemcardBusy::Decrement()
{
	if (currentBusyTicks.load(std::memory_order_relaxed) == 0)
		return;

	currentBusyTicks.fetch_sub(1, std::memory_order_release);
}

void MemcardBusy::SetBusy()
{
	currentBusyTicks.store(300, std::memory_order_release);
}

bool MemcardBusy::IsBusy()
{
	return (currentBusyTicks.load(std::memory_order_acquire) > 0);
}

void MemcardBusy::ClearBusy()
{
	currentBusyTicks.store(0, std::memory_order_release);
}
