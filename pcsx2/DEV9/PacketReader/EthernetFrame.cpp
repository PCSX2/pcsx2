// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
		NetLib::ReadMACAddress((u8*)pkt->buffer, &offset, &destinationMAC);
		NetLib::ReadMACAddress((u8*)pkt->buffer, &offset, &sourceMAC);

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
		NetLib::WriteMACAddress((u8*)pkt->buffer, &counter, destinationMAC);
		NetLib::WriteMACAddress((u8*)pkt->buffer, &counter, sourceMAC);
		//
		NetLib::WriteUInt16((u8*)pkt->buffer, &counter, protocol);
		//
		payload->WriteBytes((u8*)pkt->buffer, &counter);
	}
} // namespace PacketReader
