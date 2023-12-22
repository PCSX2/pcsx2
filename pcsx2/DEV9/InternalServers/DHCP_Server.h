// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <functional>

#include "DEV9/SimpleQueue.h"
#include "DEV9/PacketReader/IP/IP_Address.h"
#include "DEV9/PacketReader/IP/UDP/UDP_Packet.h"

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#elif defined(__POSIX__)
#include <sys/types.h>
#include <ifaddrs.h>
#endif

namespace InternalServers
{
	class DHCP_Server
	{
	public:
		PacketReader::IP::IP_Address ps2IP;
		PacketReader::IP::IP_Address gateway;
		PacketReader::IP::IP_Address broadcastIP;

	private:
		std::function<void()> callback;

		PacketReader::IP::IP_Address dns1{};
		PacketReader::IP::IP_Address dns2{};
		PacketReader::IP::IP_Address netmask{};

		SimpleQueue<PacketReader::IP::UDP::UDP_Packet*> recvBuff;

		u16 maxMs = 576;

	public:
		DHCP_Server(std::function<void()> receivedcallback);

#ifdef _WIN32
		void Init(PIP_ADAPTER_ADDRESSES adapter, PacketReader::IP::IP_Address ipOverride = {}, PacketReader::IP::IP_Address subnetOverride = {}, PacketReader::IP::IP_Address gatewayOvveride = {});
#elif defined(__POSIX__)
		void Init(ifaddrs* adapter, PacketReader::IP::IP_Address ipOverride = {}, PacketReader::IP::IP_Address subnetOverride = {}, PacketReader::IP::IP_Address gatewayOvveride = {});
#endif

		PacketReader::IP::UDP::UDP_Packet* Recv();
		bool Send(PacketReader::IP::UDP::UDP_Packet* payload);

		~DHCP_Server();

	private:
#ifdef _WIN32
		void AutoNetmask(PIP_ADAPTER_ADDRESSES adapter);
		void AutoGateway(PIP_ADAPTER_ADDRESSES adapter);
		void AutoDNS(PIP_ADAPTER_ADDRESSES adapter, bool autoDNS1, bool autoDNS2);
#elif defined(__POSIX__)
		void AutoNetmask(ifaddrs* adapter);
		void AutoGateway(ifaddrs* adapter);
		void AutoDNS(ifaddrs* adapter, bool autoDNS1, bool autoDNS2);
#endif
		void AutoBroadcast(PacketReader::IP::IP_Address parPS2IP, PacketReader::IP::IP_Address parNetmask);
	};
} // namespace InternalServers
