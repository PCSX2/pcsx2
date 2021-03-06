/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "EthernetFrame.h"
#include "NetLib.h"

namespace PacketReader
{
	EthernetFrame::EthernetFrame(Payload* data)
		: payload{data}
	{
	}
	EthernetFrame::EthernetFrame(NetPacket* pkt)
	{
		int offset = 0;
		NetLib::ReadByteArray((u8*)pkt->buffer, &offset, 6, destinationMAC);
		NetLib::ReadByteArray((u8*)pkt->buffer, &offset, 6, sourceMAC);

		headerLength = 14; //(6+6+2)

		//Note: we don't have to worry about the Ethernet Frame CRC as it is not included in the packet

		NetLib::ReadUInt16((u8*)pkt->buffer, &offset, &protocol);

		//Note: We don't support tagged frames

		payload = std::make_unique<PayloadPtr>((u8*)&pkt->buffer[offset], pkt->size - headerLength);
	}

	Payload* EthernetFrame::GetPayload()
	{
		return payload.get();
	}

	void EthernetFrame::WritePacket(NetPacket* pkt)
	{
		int counter = 0;

		pkt->size = headerLength + payload->GetLength();
		NetLib::WriteByteArray((u8*)pkt->buffer, &counter, 6, destinationMAC);
		NetLib::WriteByteArray((u8*)pkt->buffer, &counter, 6, sourceMAC);
		//
		NetLib::WriteUInt16((u8*)pkt->buffer, &counter, protocol);
		//
		payload->WriteBytes((u8*)pkt->buffer, &counter);
	}
} // namespace PacketReader
