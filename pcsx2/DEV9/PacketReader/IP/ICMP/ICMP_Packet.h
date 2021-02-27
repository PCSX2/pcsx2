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
