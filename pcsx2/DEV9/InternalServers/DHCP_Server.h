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

		PacketReader::IP::IP_Address dns1{0};
		PacketReader::IP::IP_Address dns2{0};
		PacketReader::IP::IP_Address netmask{0};

		SimpleQueue<PacketReader::IP::UDP::UDP_Packet*> recvBuff;

		u16 maxMs = 576;

	public:
		DHCP_Server(std::function<void()> receivedcallback);

#ifdef _WIN32
		void Init(PIP_ADAPTER_ADDRESSES adapter, PacketReader::IP::IP_Address ipOverride = {0}, PacketReader::IP::IP_Address subnetOverride = {0}, PacketReader::IP::IP_Address gatewayOvveride = {0});
#elif defined(__POSIX__)
		void Init(ifaddrs* adapter, PacketReader::IP::IP_Address ipOverride = {0}, PacketReader::IP::IP_Address subnetOverride = {0}, PacketReader::IP::IP_Address gatewayOvveride = {0});
#endif

		PacketReader::IP::UDP::UDP_Packet* Recv();
		bool Send(PacketReader::IP::UDP::UDP_Packet* payload);

#ifdef __linux__
		static std::vector<PacketReader::IP::IP_Address> GetGatewaysLinux(char* interfaceName);
#elif defined(__FreeBSD__) || (__APPLE__)
		static std::vector<PacketReader::IP::IP_Address> GetGatewaysBSD(char* interfaceName);
#endif

		~DHCP_Server();

	private:
#ifdef __POSIX__
		static std::vector<std::string> SplitString(std::string str, char delimiter);
		static std::vector<PacketReader::IP::IP_Address> GetDNSUnix();
#endif

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
