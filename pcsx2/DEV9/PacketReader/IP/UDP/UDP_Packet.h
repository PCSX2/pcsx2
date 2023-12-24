// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "DEV9/PacketReader/IP/IP_Packet.h"

namespace PacketReader::IP::UDP
{
	class UDP_Packet : public IP_Payload
	{
	public:
		u16 sourcePort;
		u16 destinationPort;

	private:
		//u16 length;
		u16 checksum;

		const static int headerLength = 8;
		const static IP_Type protocol = IP_Type::UDP;

		std::unique_ptr<Payload> payload;

	public:
		//Takes ownership of payload
		UDP_Packet(Payload* data);
		UDP_Packet(u8* buffer, int bufferSize);
		UDP_Packet(const UDP_Packet&);

		Payload* GetPayload();

		virtual int GetLength();
		virtual void WriteBytes(u8* buffer, int* offset);
		virtual UDP_Packet* Clone() const;

		virtual u8 GetProtocol();

		virtual bool VerifyChecksum(IP_Address srcIP, IP_Address dstIP);
		virtual void CalculateChecksum(IP_Address srcIP, IP_Address dstIP);
	};
} // namespace PacketReader::IP::UDP
