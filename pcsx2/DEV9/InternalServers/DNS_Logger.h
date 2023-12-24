// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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
