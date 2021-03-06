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
#include <vector>

#include "DEV9/PacketReader/Payload.h"

namespace PacketReader::ARP
{
	class ARP_Packet : public Payload
	{
	public:
		u16 hardwareType;
		u16 protocol;
		u8 hardwareAddressLength = 6;
		u8 protocolAddressLength = 4;
		u16 op;
		std::unique_ptr<u8[]> senderHardwareAddress;
		std::unique_ptr<u8[]> senderProtocolAddress;
		std::unique_ptr<u8[]> targetHardwareAddress;
		std::unique_ptr<u8[]> targetProtocolAddress;

		ARP_Packet(u8 hwAddrLen, u8 procAddrLen);
		ARP_Packet(u8* buffer, int bufferSize);
		ARP_Packet(const ARP_Packet&);

		virtual int GetLength();
		virtual void WriteBytes(u8* buffer, int* offset);
		virtual ARP_Packet* Clone() const;
	};
} // namespace PacketReader::ARP
