// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "DEV9/PacketReader/IP/IP_Packet.h"

namespace PacketReader::IP::ICMP
{
	class ICMP_Packet : public IP_Payload
	{
	public:
		u8 type;
		u8 code;

	private:
		u16 checksum;

	public:
		u8 headerData[4];

	private:
		const static int headerLength = 8;
		const static IP_Type protocol = IP_Type::ICMP;

		std::unique_ptr<Payload> payload;

	public:
		//Takes ownership of payload
		ICMP_Packet(Payload* data);
		ICMP_Packet(u8* buffer, int bufferSize);
		ICMP_Packet(const ICMP_Packet&);

		Payload* GetPayload();

		virtual int GetLength();
		virtual void WriteBytes(u8* buffer, int* offset);
		virtual ICMP_Packet* Clone() const;

		virtual u8 GetProtocol();

		virtual bool VerifyChecksum(IP_Address srcIP, IP_Address dstIP);
		virtual void CalculateChecksum(IP_Address srcIP, IP_Address dstIP);
	};

	//Helper Classes
	//Do we want this? or do we do the same as with options?
	class ICMP_HeaderDataIdentifier
	{
	public:
		u16 identifier;
		u16 sequenceNumber;

		ICMP_HeaderDataIdentifier(u16 id, u16 seq);
		ICMP_HeaderDataIdentifier(u8* headerData);
		void WriteHeaderData(u8* headerData);
	};
} // namespace PacketReader::IP::ICMP
