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
