// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "ICMP_Packet.h"
#include "DEV9/PacketReader/NetLib.h"

namespace PacketReader::IP::ICMP
{
	ICMP_Packet::ICMP_Packet(Payload* data)
		: payload{data}
	{
	}
	ICMP_Packet::ICMP_Packet(u8* buffer, int bufferSize)
	{
		int offset = 0;
		//Bits 0-31
		NetLib::ReadByte08(buffer, &offset, &type);
		NetLib::ReadByte08(buffer, &offset, &code);
		NetLib::ReadUInt16(buffer, &offset, &checksum);

		//Bits 32-63
		NetLib::ReadByteArray(buffer, &offset, 4, headerData);

		//Bits 64+
		payload = std::make_unique<PayloadPtr>(&buffer[offset], bufferSize - offset);
		//AllDone
	}
	ICMP_Packet::ICMP_Packet(const ICMP_Packet& original)
		: type{original.type}
		, code{original.code}
		, checksum{original.checksum}
		, payload{original.payload->Clone()}
	{
		memcpy(headerData, original.headerData, 4);
	}

	Payload* ICMP_Packet::GetPayload()
	{
		return payload.get();
	}

	int ICMP_Packet::GetLength()
	{
		return headerLength + payload->GetLength();
	}

	void ICMP_Packet::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteByte08(buffer, offset, type);
		NetLib::WriteByte08(buffer, offset, code);
		NetLib::WriteUInt16(buffer, offset, checksum);
		NetLib::WriteByteArray(buffer, offset, 4, headerData);

		payload->WriteBytes(buffer, offset);
	}

	ICMP_Packet* ICMP_Packet::Clone() const
	{
		return new ICMP_Packet(*this);
	}

	u8 ICMP_Packet::GetProtocol()
	{
		return (u8)protocol;
	}

	void ICMP_Packet::CalculateChecksum(IP_Address srcIP, IP_Address dstIP)
	{
		int pHeaderLen = headerLength + payload->GetLength();
		if ((pHeaderLen & 1) != 0)
		{
			pHeaderLen += 1;
		}

		u8* segment = new u8[pHeaderLen];
		int counter = 0;

		checksum = 0;
		WriteBytes(segment, &counter);

		//Zero alignment byte
		if (counter != pHeaderLen)
			NetLib::WriteByte08(segment, &counter, 0);

		checksum = IP_Packet::InternetChecksum(segment, pHeaderLen);
		delete[] segment;
	}
	bool ICMP_Packet::VerifyChecksum(IP_Address srcIP, IP_Address dstIP)
	{
		int pHeaderLen = headerLength + payload->GetLength();
		if ((pHeaderLen & 1) != 0)
		{
			pHeaderLen += 1;
		}

		u8* segment = new u8[pHeaderLen];
		int counter = 0;

		WriteBytes(segment, &counter);

		//Zero alignment byte
		if (counter != pHeaderLen)
			NetLib::WriteByte08(segment, &counter, 0);

		u16 csumCal = IP_Packet::InternetChecksum(segment, pHeaderLen);
		delete[] segment;

		return (csumCal == 0);
	}

	ICMP_HeaderDataIdentifier::ICMP_HeaderDataIdentifier(u16 id, u16 seq)
		: identifier{id}
		, sequenceNumber{seq}
	{
	}
	ICMP_HeaderDataIdentifier::ICMP_HeaderDataIdentifier(u8* headerData)
	{
		int offset = 0;
		NetLib::ReadUInt16(headerData, &offset, &identifier);
		NetLib::ReadUInt16(headerData, &offset, &sequenceNumber);
	}
	void ICMP_HeaderDataIdentifier::WriteHeaderData(u8* headerData)
	{
		int offset = 0;
		NetLib::WriteUInt16(headerData, &offset, identifier);
		NetLib::WriteUInt16(headerData, &offset, sequenceNumber);
	}
} // namespace PacketReader::IP::ICMP
