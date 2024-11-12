// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include "DEV9/PacketReader/Payload.h"
#include "DEV9/PacketReader/ARP/ARP_Packet.h"

namespace InternalServers
{
	class ARP_Logger
	{
	public:
		ARP_Logger(){};

		//Expects a ARP_payload
		void InspectRecv(PacketReader::Payload* payload);
		//Expects a ARP_payload
		void InspectSend(PacketReader::Payload* payload);

	private:
		std::string ArrayToString(const std::unique_ptr<u8[]>& data, int length);
		const char* HardwareTypeToString(u8 op); // Same as DHCP
		const char* ProtocolToString(u16 protocol);
		const char* OperationToString(u16 op);
		void LogPacket(PacketReader::ARP::ARP_Packet* payload);
	};
} // namespace InternalServers
