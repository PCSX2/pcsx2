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

#pragma once
#include <vector>

#include "../PacketReader/IP/IP_Packet.h"
#include "../PacketReader/IP/UDP/DHCP/DHCP_Packet.h"

//using PacketReader::Payload;
using PacketReader::IP::IP_Address;
using PacketReader::IP::IP_Payload;

namespace Sessions
{
	class DHCP_Patcher
	{
	private:
		std::vector<u8> reqList;
		u16 maxMessage = 576;

	public:
		IP_Payload* InspectSent(IP_Payload* ipPayload);
		IP_Payload* InspectRecv(IP_Payload* ipPayload);
	};
}
