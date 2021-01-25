/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include "UDP_Packet.h"
#include "../../NetLib.h"

namespace PacketReader::IP::UDP
{
	UDP_Packet::UDP_Packet(u8* buffer, int bufferSize)
	{
		int offset = 0;
		//Bits 0-31
		NetLib::ReadUInt16(buffer, &offset, &sourcePort);
		NetLib::ReadUInt16(buffer, &offset, &destinationPort);

		//Bits 32-63
		u16 length;
		NetLib::ReadUInt16(buffer, &offset, &length); //includes header length
		NetLib::ReadUInt16(buffer, &offset, &checksum);

		if (length > bufferSize)
		{
			Console.Error("UDP Unexpected Length");
			length = (u16)bufferSize;
		}

		//Bits 64+
		payload = new PayloadPtr(&buffer[offset], length - offset);
		//AllDone
	}

	Payload* UDP_Packet::GetPayload()
	{
		return payload;
	}
	void UDP_Packet::SetPayload(Payload* value, bool takeOwnership)
	{
		delete payload;
		payload = value;
		ownsPayload = takeOwnership;
	}

	int UDP_Packet::GetLength()
	{
		return headerLength + payload->GetLength();
	}

	void UDP_Packet::WriteBytes(u8* buffer, int* offset)
	{
		NetLib::WriteUInt16(buffer, offset, sourcePort);
		NetLib::WriteUInt16(buffer, offset, destinationPort);
		NetLib::WriteUInt16(buffer, offset, GetLength());
		NetLib::WriteUInt16(buffer, offset, checksum);

		payload->WriteBytes(buffer, offset);
	}

	void UDP_Packet::CalculateChecksum(IP_Address srcIP, IP_Address dstIP)
	{
		int pHeaderLen = (12) + headerLength + payload->GetLength();
		if ((pHeaderLen & 1) != 0)
		{
			pHeaderLen += 1;
		}

		u8* headerSegment = new u8[pHeaderLen];
		int counter = 0;

		NetLib::WriteByteArray(headerSegment, &counter, 4, (u8*)&srcIP);
		NetLib::WriteByteArray(headerSegment, &counter, 4, (u8*)&dstIP);
		NetLib::WriteByte08(headerSegment, &counter, 0);
		NetLib::WriteByte08(headerSegment, &counter, (u8)protocol);
		NetLib::WriteUInt16(headerSegment, &counter, GetLength());

		//Pseudo Header added
		//Rest of data is normal Header+data (with zerored checksum feild)
		NetLib::WriteUInt16(headerSegment, &counter, sourcePort);
		NetLib::WriteUInt16(headerSegment, &counter, destinationPort);
		NetLib::WriteUInt16(headerSegment, &counter, GetLength());
		NetLib::WriteUInt16(headerSegment, &counter, 0); //Zeroed checksum

		payload->WriteBytes(headerSegment, &counter);

		checksum = IP_Packet::InternetChecksum(headerSegment, pHeaderLen);
		delete[] headerSegment;
	}
	bool UDP_Packet::VerifyChecksum(IP_Address srcIP, IP_Address dstIP)
	{
		int pHeaderLen = (12) + headerLength + payload->GetLength();
		if ((pHeaderLen & 1) != 0)
		{
			pHeaderLen += 1;
		}

		u8* headerSegment = new u8[pHeaderLen];
		int counter = 0;

		NetLib::WriteByteArray(headerSegment, &counter, 4, (u8*)&srcIP);
		NetLib::WriteByteArray(headerSegment, &counter, 4, (u8*)&dstIP);
		counter += 1; //[8] = 0
		NetLib::WriteByte08(headerSegment, &counter, (u8)protocol);
		NetLib::WriteUInt16(headerSegment, &counter, GetLength());

		//Pseudo Header added
		//Rest of data is normal Header+data (with zerored checksum feild)
		NetLib::WriteUInt16(headerSegment, &counter, sourcePort);
		NetLib::WriteUInt16(headerSegment, &counter, destinationPort);
		NetLib::WriteUInt16(headerSegment, &counter, GetLength());
		NetLib::WriteUInt16(headerSegment, &counter, checksum);

		payload->WriteBytes(headerSegment, &counter);

		u16 csumCal = IP_Packet::InternetChecksum(headerSegment, pHeaderLen);
		delete[] headerSegment;

		return (csumCal == 0);
	}

	UDP_Packet::~UDP_Packet()
	{
		delete payload;
	}
} // namespace PacketReader::IP::UDP
