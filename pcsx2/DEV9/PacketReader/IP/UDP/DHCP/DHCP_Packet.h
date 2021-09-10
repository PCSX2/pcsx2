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

#include "DHCP_Options.h"
#include "DEV9/PacketReader/IP/IP_Packet.h"

namespace PacketReader::IP::UDP::DHCP
{
	class DHCP_Packet : public Payload
	{
	public:
		u8 op;
		u8 hardwareType;
		u8 hardwareAddressLength;
		u8 hops;
		u32 transactionID; //xid
		u16 seconds;
		u16 flags;
		IP_Address clientIP{0};
		IP_Address yourIP{0};
		IP_Address serverIP{0};
		IP_Address gatewayIP{0};
		u8 clientHardwareAddress[16]{0}; //always 16 bytes, regardless of HardwareAddressLength
		//192 bytes of padding
		u32 magicCookie;
		//Assumes ownership of ptrs assigned to it
		std::vector<BaseOption*> options;

		//used by GetLength & WriteBytes
		int maxLength = 576;

		DHCP_Packet() {}
		DHCP_Packet(u8* buffer, int bufferSize);
		DHCP_Packet(const DHCP_Packet&);

		virtual int GetLength();
		virtual void WriteBytes(u8* buffer, int* offset);
		virtual DHCP_Packet* Clone() const;

		virtual ~DHCP_Packet();
	};
} // namespace PacketReader::IP::UDP::DHCP
