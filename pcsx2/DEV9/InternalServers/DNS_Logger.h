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
#include "DEV9/PacketReader/IP/UDP/DNS/DNS_Packet.h"

namespace InternalServers
{
	class DNS_Logger
	{
	public:
		DNS_Logger(){};

		//Expects a UDP_payload
		void InspectRecv(PacketReader::IP::IP_Payload* payload);
		//Expects a UDP_payload
		void InspectSend(PacketReader::IP::IP_Payload* payload);

	private:
		std::string VectorToString(const std::vector<u8>& data);
		const char* OpCodeToString(PacketReader::IP::UDP::DNS::DNS_OPCode opcode);
		const char* RCodeToString(PacketReader::IP::UDP::DNS::DNS_RCode rcode);
		void LogPacket(PacketReader::IP::UDP::DNS::DNS_Packet* payload);
	};
} // namespace InternalServers
