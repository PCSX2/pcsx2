// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "DEV9/PacketReader/IP/IP_Packet.h"
#include "DEV9/PacketReader/IP/UDP/DHCP/DHCP_Packet.h"

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#elif defined(__POSIX__)
#include <sys/types.h>
#include <ifaddrs.h>
#endif

namespace InternalServers
{
	class DHCP_Logger
	{
	public:
		DHCP_Logger(){};

#ifdef _WIN32
		void Init(PIP_ADAPTER_ADDRESSES adapter);
#elif defined(__POSIX__)
		void Init(ifaddrs* adapter);
#endif

		// Expects a UDP_payload
		void InspectRecv(PacketReader::IP::IP_Payload* payload);
		// Expects a UDP_payload
		void InspectSend(PacketReader::IP::IP_Payload* payload);

	private:
		PacketReader::IP::IP_Address pcIP{};

		std::string IpToString(PacketReader::IP::IP_Address ip);
		std::string HardwareAddressToString(u8* data, size_t len);
		std::string ClientIdToString(const std::vector<u8>& data);
		const char* OpToString(u8 op);
		const char* HardwareTypeToString(u8 op);
		const char* OptionToString(u8 option);
		const char* MessageCodeToString(u8 msg);
		void LogPacket(PacketReader::IP::UDP::DHCP::DHCP_Packet* payload);
	};
} // namespace InternalServers
