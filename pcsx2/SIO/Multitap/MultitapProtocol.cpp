/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "Common.h"
#include "StateWrapper.h"

#include "SIO/Multitap/MultitapProtocol.h"

#include "SIO/Sio2.h"
#include "SIO/SioTypes.h"

#define MT_LOG_ENABLE 0
#define MT_LOG if (MT_LOG_ENABLE) DevCon

std::array<MultitapProtocol, SIO::PORTS> g_MultitapArr;

void MultitapProtocol::SupportCheck()
{
	MT_LOG.WriteLn("%s", __FUNCTION__);

	if (EmuConfig.Pad.IsMultitapPortEnabled(g_Sio2.port))
	{
		g_Sio2FifoOut.push_back(0xff);
		g_Sio2FifoOut.push_back(0x80);
		g_Sio2FifoOut.push_back(0x5a);
		g_Sio2FifoOut.push_back(0x04);
		g_Sio2FifoOut.push_back(0x00);
		g_Sio2FifoOut.push_back(0x5a);
	}
	else
	{
		g_Sio2FifoOut.push_back(0xff);
		g_Sio2FifoOut.push_back(0xff);
		g_Sio2FifoOut.push_back(0xff);
		g_Sio2FifoOut.push_back(0xff);
		g_Sio2FifoOut.push_back(0xff);
		g_Sio2FifoOut.push_back(0xff);
	}
}

void MultitapProtocol::Select(MultitapMode mode)
{
	MT_LOG.WriteLn("%s", __FUNCTION__);
	
	if (EmuConfig.Pad.IsMultitapPortEnabled(g_Sio2.port))
	{
		const u8 newSlot = g_Sio2FifoIn.front();
		g_Sio2FifoIn.pop_front();
		const bool isInBounds = (newSlot < SIO::SLOTS);

		if (isInBounds)
		{
			switch (mode)
			{
				case MultitapMode::SELECT_PAD:
					this->currentPadSlot = newSlot;
					break;
				case MultitapMode::SELECT_MEMCARD:
					this->currentMemcardSlot = newSlot;
					break;
				default:
					break;
			}

			MT_LOG.WriteLn("Slot changed to %d", newSlot);
		}

		g_Sio2FifoOut.push_back(0xff);
		g_Sio2FifoOut.push_back(0x80);
		g_Sio2FifoOut.push_back(0x5a);
		g_Sio2FifoOut.push_back(0x00);
		g_Sio2FifoOut.push_back(0x00);
		g_Sio2FifoOut.push_back(isInBounds ? newSlot : 0xff);
		g_Sio2FifoOut.push_back(isInBounds ? 0x5a : 0x66);
	}
	else
	{
		g_Sio2FifoOut.push_back(0xff);
		g_Sio2FifoOut.push_back(0xff);
		g_Sio2FifoOut.push_back(0xff);
		g_Sio2FifoOut.push_back(0xff);
		g_Sio2FifoOut.push_back(0xff);
		g_Sio2FifoOut.push_back(0xff);
		g_Sio2FifoOut.push_back(0xff);
	}
}

MultitapProtocol::MultitapProtocol() = default;
MultitapProtocol::~MultitapProtocol() = default;

void MultitapProtocol::SoftReset()
{
}

void MultitapProtocol::FullReset()
{
	SoftReset();

	this->currentPadSlot = 0;
	this->currentMemcardSlot = 0;
}

bool MultitapProtocol::DoState(StateWrapper& sw)
{
	if (!sw.DoMarker("Multitap"))
		return false;

	sw.Do(&currentPadSlot);
	sw.Do(&currentMemcardSlot);
	return true;
}

u8 MultitapProtocol::GetPadSlot()
{
	return this->currentPadSlot;
}

u8 MultitapProtocol::GetMemcardSlot()
{
	return this->currentMemcardSlot;
}

void MultitapProtocol::SendToMultitap()
{
	const u8 commandByte = g_Sio2FifoIn.front();
	g_Sio2FifoIn.pop_front();

	switch (static_cast<MultitapMode>(commandByte))
	{
		case MultitapMode::PAD_SUPPORT_CHECK:
		case MultitapMode::MEMCARD_SUPPORT_CHECK:
			SupportCheck();
			break;
		case MultitapMode::SELECT_PAD:
			Select(MultitapMode::SELECT_PAD);
			break;
		case MultitapMode::SELECT_MEMCARD:
			Select(MultitapMode::SELECT_MEMCARD);
			break;
		default:
			DevCon.Warning("%s() Unhandled MultitapMode (%02X)", __FUNCTION__, commandByte);
			break;
	}
}
