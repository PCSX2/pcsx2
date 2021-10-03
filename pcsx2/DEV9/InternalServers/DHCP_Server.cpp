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

#include "PrecompiledHeader.h"

#include <algorithm>
#ifdef __POSIX__
#include <string>
#include <vector>
#include <fstream>
#include <arpa/inet.h>
#endif

#if defined(__FreeBSD__)
#include <sys/socket.h>
#include <netinet/in.h>
#endif

#include "DHCP_Server.h"
#include "DEV9/PacketReader/IP/UDP/UDP_Packet.h"
#include "DEV9/PacketReader/IP/UDP/DHCP/DHCP_Packet.h"

#include "DEV9/DEV9.h"

using namespace PacketReader;
using namespace PacketReader::IP;
using namespace PacketReader::IP::UDP;
using namespace PacketReader::IP::UDP::DHCP;

namespace InternalServers
{
	DHCP_Server::DHCP_Server(std::function<void()> receivedcallback)
		: callback{receivedcallback}
	{
	}

#ifdef _WIN32
	void DHCP_Server::Init(PIP_ADAPTER_ADDRESSES adapter)
#elif defined(__POSIX__)
	void DHCP_Server::Init(ifaddrs* adapter)
#endif
	{
		ps2IP = config.PS2IP;
		netmask = {0};
		gateway = {0};
		dns1 = {0};
		dns2 = {0};
		broadcastIP = {0};

		if (config.AutoMask)
			AutoNetmask(adapter);
		else
			netmask = config.Mask;

		if (config.AutoGateway)
			AutoGateway(adapter);
		else
			gateway = config.Gateway;

		if (!config.AutoDNS1)
			dns1 = config.DNS1;

		if (!config.AutoDNS2)
			dns2 = config.DNS2;

		AutoDNS(adapter, config.AutoDNS1, config.AutoDNS2);
		AutoBroadcast(ps2IP, netmask);
	}

#ifdef __POSIX__
	//skipsEmpty
	std::vector<std::string> DHCP_Server::SplitString(std::string str, char delimiter)
	{
		std::vector<std::string> ret;
		size_t last = 0;
		size_t next = 0;
		std::string token;
		while ((next = str.find(delimiter, last)) != std::string::npos)
		{
			token = str.substr(last, next - last);
			if (next != last)
				ret.push_back(token);
			last = next + 1;
		}
		token = str.substr(last);
		if (next != last)
			ret.push_back(token);

		return ret;
	}
#ifdef __linux__
	std::vector<IP_Address> DHCP_Server::GetGatewaysLinux(char* interfaceName)
	{
		///proc/net/route contains some information about gateway addresses,
		//and separates the information about by each interface.
		std::vector<IP_Address> collection;
		std::vector<std::string> routeLines;
		std::fstream route("/proc/net/route", std::ios::in);
		if (route.fail())
		{
			route.close();
			Console.Error("DHCP: Failed to open /proc/net/route");
			return collection;
		}

		std::string line;
		while (std::getline(route, line))
			routeLines.push_back(line);
		route.close();

		//Columns are as follows (first-line header):
		//Iface  Destination  Gateway  Flags  RefCnt  Use  Metric  Mask  MTU  Window  IRTT
		for (size_t i = 1; i < routeLines.size(); i++)
		{
			std::string line = routeLines[i];
			if (line.rfind(interfaceName, 0) == 0)
			{
				std::vector<std::string> split = SplitString(line, '\t');
				std::string gatewayIPHex = split[2];
				int addressValue = std::stoi(gatewayIPHex, 0, 16);
				//Skip device routes without valid NextHop IP address
				if (addressValue != 0)
				{
					IP_Address gwIP = *(IP_Address*)&addressValue;
					collection.push_back(gwIP);
				}
			}
		}
		return collection;
	}
#endif
	std::vector<IP_Address> DHCP_Server::GetDNSUnix()
	{
		//On Linux and OSX, DNS is system wide, not adapter specific, so we can ignore adapter

		// Parse /etc/resolv.conf for all of the "nameserver" entries.
		// These are the DNS servers the machine is configured to use.
		// On OSX, this file is not directly used by most processes for DNS
		// queries/routing, but it is automatically generated instead, with
		// the machine's DNS servers listed in it.
		std::vector<IP_Address> collection;

		std::fstream servers("/etc/resolv.conf", std::ios::in);
		if (servers.fail())
		{
			servers.close();
			Console.Error("DHCP: Failed to open /etc/resolv.conf");
			return collection;
		}

		std::string line;
		std::vector<std::string> serversLines;
		while (std::getline(servers, line))
			serversLines.push_back(line);
		servers.close();

		const IP_Address systemdDNS{127, 0, 0, 53};
		for (size_t i = 1; i < serversLines.size(); i++)
		{
			std::string line = serversLines[i];
			if (line.rfind("nameserver", 0) == 0)
			{
				std::vector<std::string> split = SplitString(line, '\t');
				if (split.size() == 1)
					split = SplitString(line, ' ');
				std::string dns = split[1];

				IP_Address address;
				if (inet_pton(AF_INET, dns.c_str(), &address) != 1)
					continue;

				if (address == systemdDNS)
					Console.Error("DHCP: systemd-resolved DNS server is not supported");

				collection.push_back(address);
			}
		}
		return collection;
	}
#endif

#ifdef _WIN32
	void DHCP_Server::AutoNetmask(PIP_ADAPTER_ADDRESSES adapter)
	{
		if (adapter != nullptr)
		{
			PIP_ADAPTER_UNICAST_ADDRESS address = adapter->FirstUnicastAddress;
			while (address != nullptr && address->Address.lpSockaddr->sa_family != AF_INET)
				address = address->Next;

			if (address != nullptr)
			{
				ULONG mask;
				if (ConvertLengthToIpv4Mask(address->OnLinkPrefixLength, &mask) == NO_ERROR)
					netmask.integer = mask;
			}
		}
	}

	void DHCP_Server::AutoGateway(PIP_ADAPTER_ADDRESSES adapter)
	{
		if (adapter != nullptr)
		{
			PIP_ADAPTER_GATEWAY_ADDRESS address = adapter->FirstGatewayAddress;
			while (address != nullptr && address->Address.lpSockaddr->sa_family != AF_INET)
				address = address->Next;

			if (address != nullptr)
			{
				sockaddr_in* sockaddr = (sockaddr_in*)address->Address.lpSockaddr;
				gateway = *(IP_Address*)&sockaddr->sin_addr;
			}
		}
	}

#elif defined(__POSIX__)
	void DHCP_Server::AutoNetmask(ifaddrs* adapter)
	{
		if (adapter != nullptr)
		{
			if (adapter->ifa_netmask != nullptr && adapter->ifa_netmask->sa_family == AF_INET)
			{
				sockaddr_in* sockaddr = (sockaddr_in*)adapter->ifa_netmask;
				netmask = *(IP_Address*)&sockaddr->sin_addr;
			}
		}
	}

	void DHCP_Server::AutoGateway(ifaddrs* adapter)
	{
		if (adapter != nullptr)
		{
#ifdef __linux__
			std::vector<IP_Address> gateways = GetGatewaysLinux(adapter->ifa_name);

			if (gateways.size() > 0)
				gateway = gateways[0];
#else
			Console.Error("DHCP: Unsupported OS, can't find Gateway");
#endif
		}
	}
#endif

#ifdef _WIN32
	void DHCP_Server::AutoDNS(PIP_ADAPTER_ADDRESSES adapter, bool autoDNS1, bool autoDNS2)
	{
		std::vector<IP_Address> dnsIPs;

		if (adapter != nullptr)
		{
			PIP_ADAPTER_DNS_SERVER_ADDRESS address = adapter->FirstDnsServerAddress;
			while (address != nullptr)
			{
				if (address->Address.lpSockaddr->sa_family == AF_INET)
				{
					sockaddr_in* sockaddr = (sockaddr_in*)address->Address.lpSockaddr;
					dnsIPs.push_back(*(IP_Address*)&sockaddr->sin_addr);
				}
				address = address->Next;
			}
		}
#elif defined(__POSIX__)
	void DHCP_Server::AutoDNS(ifaddrs* adapter, bool autoDNS1, bool autoDNS2)
	{
		std::vector<IP_Address> dnsIPs = GetDNSUnix();
#endif
		if (autoDNS1)
		{
			//Auto DNS1
			//If Adapter has DNS, add 1st entry
			if (dnsIPs.size() >= 1)
				dns1 = dnsIPs[0];

			//Auto DNS1 & AutoDNS2
			if (autoDNS2 && dnsIPs.size() >= 2)
				dns2 = dnsIPs[1];
		}
		else if (autoDNS2)
		{
			//Manual DNS1 & Auto DNS2

			//Use adapter's DNS2 if it has one
			//otherwise use adapter's DNS1

			if (!dnsIPs.empty())
				dns2 = dnsIPs[std::min<size_t>(1, dnsIPs.size() - 1)];
		}
		if (dns1.integer == 0 && dns2.integer != 0)
		{
			Console.Error("DHCP: DNS1 is zero, but DNS2 is valid, using DNS2 as DNS1");
			//no value for DNS1, but we have a value for DNS2
			//set DNS1 to DNS2 and zero DNS2
			dns1 = dns2;
			dns2 = {0, 0, 0, 0};
		}
	}

	void DHCP_Server::AutoBroadcast(IP_Address parPS2IP, IP_Address parNetmask)
	{
		if (parNetmask.integer != 0)
		{
			for (int i = 0; i < 4; i++)
				broadcastIP.bytes[i] = ((parPS2IP.bytes[i]) | (~parNetmask.bytes[i]));
		}
	}

	UDP_Packet* DHCP_Server::Recv()
	{
		UDP_Packet* payload;
		if (recvBuff.Dequeue(&payload))
			return payload;
		return nullptr;
	}

	bool DHCP_Server::Send(UDP_Packet* payload)
	{
		UDP_Packet* udpPacket = static_cast<UDP_Packet*>(payload);
		PayloadPtr* udpPayload = static_cast<PayloadPtr*>(udpPacket->GetPayload());
		DHCP_Packet dhcp = DHCP_Packet(udpPayload->data, udpPayload->GetLength());

		//State
		u8 hType = dhcp.hardwareType;
		u8 hLen = dhcp.hardwareAddressLength;
		u32 xID = dhcp.transactionID;
		u8* cMac = dhcp.clientHardwareAddress;
		u32 cookie = dhcp.magicCookie;

		u8 msg = 0;
		std::vector<u8> reqList;

		uint leaseTime = 86400;

		for (size_t i = 0; i < dhcp.options.size(); i++)
		{
			switch (dhcp.options[i]->GetCode())
			{
				case 0:
					continue;
				case 1:
					if (netmask != ((DHCPopSubnet*)dhcp.options[i])->subnetMask)
						Console.Error("DHCP: SubnetMask missmatch");
					break;
				case 3:
					if (((DHCPopRouter*)dhcp.options[i])->routers.size() != 1)
						Console.Error("DHCP: Routers count missmatch");

					if (gateway != ((DHCPopRouter*)dhcp.options[i])->routers[0])
						Console.Error("DHCP: RouterIP missmatch");
					break;
				case 6:
					// clang-format off
					if (((((DHCPopDNS*)dhcp.options[i])->dnsServers.size() == 0 && dns1.integer == 0) ||
						 (((DHCPopDNS*)dhcp.options[i])->dnsServers.size() == 1 && dns1.integer != 0 && dns2.integer == 0) ||
						 (((DHCPopDNS*)dhcp.options[i])->dnsServers.size() == 2 && dns2.integer != 0)) == false)
						Console.Error("DHCP: DNS count missmatch");
					// clang-format on

					if ((((DHCPopDNS*)dhcp.options[i])->dnsServers.size() > 0 && dns1 != ((DHCPopDNS*)dhcp.options[i])->dnsServers[0]) ||
						(((DHCPopDNS*)dhcp.options[i])->dnsServers.size() > 1 && dns2 != ((DHCPopDNS*)dhcp.options[i])->dnsServers[1]))
						Console.Error("DHCP: DNS missmatch");
					break;
				case 12:
					//TODO use name?
					break;
				case 50:
					if (ps2IP != ((DHCPopREQIP*)dhcp.options[i])->requestedIP)
						Console.Error("DHCP: ReqIP missmatch");
					break;
				case 51:
					leaseTime = ((DHCPopIPLT*)(dhcp.options[i]))->ipLeaseTime;
					break;
				case 53:
					msg = ((DHCPopMSG*)(dhcp.options[i]))->message;
					break;
				case 54:
					if (NetAdapter::internalIP != ((DHCPopSERVIP*)dhcp.options[i])->serverIP)
						Console.Error("DHCP: ServIP missmatch");
					break;
				case 55:
					reqList = ((DHCPopREQLIST*)(dhcp.options[i]))->requests;
					break;
				case 56: //String message
					break;
				case 57:
					maxMs = ((DHCPopMMSGS*)(dhcp.options[i]))->maxMessageSize;
					break;
				case 60: //ClassID
				case 61: //ClientID
				case 255: //End
					break;
				default:
					Console.Error("DHCP: Got Unhandled Option %d", dhcp.options[i]->GetCode());
					break;
			}
		}

		DHCP_Packet* retPay = new DHCP_Packet();
		retPay->op = 2,
		retPay->hardwareType = hType,
		retPay->hardwareAddressLength = hLen,
		retPay->transactionID = xID,

		retPay->yourIP = ps2IP;
		retPay->serverIP = NetAdapter::internalIP;

		memcpy(retPay->clientHardwareAddress, cMac, 6);
		retPay->magicCookie = cookie;

		if (msg == 1 || msg == 3) //Fill out Requests
		{
			if (msg == 1)
				retPay->options.push_back(new DHCPopMSG(2));
			if (msg == 3)
				retPay->options.push_back(new DHCPopMSG(5));

			for (size_t i = 0; i < reqList.size(); i++)
			{
				switch (reqList[i])
				{
					case 1:
						retPay->options.push_back(new DHCPopSubnet(netmask));
						break;
					case 3:
						if (gateway.integer != 0)
						{
							std::vector<IP_Address> routers;
							routers.push_back(gateway);
							retPay->options.push_back(new DHCPopRouter(routers));
						}
						break;
					case 6:
						if (dns1.integer != 0)
						{
							std::vector<IP_Address> dns;
							dns.push_back(dns1);
							if (dns2.integer != 0)
								dns.push_back(dns2);
							retPay->options.push_back(new DHCPopDNS(dns));
						}
						break;
					case 15:
						retPay->options.push_back(new DHCPopDnsName("PCSX2"));
						break;
					case 28:
						retPay->options.push_back(new DHCPopBCIP(broadcastIP));
						break;
					case 50:
						retPay->options.push_back(new DHCPopREQIP(ps2IP));
						break;
					case 53: //Msg (Already added)
					case 54: //Server Identifier (Already Added)
						break;
					default:
						Console.Error("DHCP: Got Unhandled Request %d", reqList[i]);
						break;
				}
			}
			retPay->options.push_back(new DHCPopIPLT(leaseTime));
		}
		else if (msg == 7)
			return true;

		retPay->options.push_back(new DHCPopSERVIP(NetAdapter::internalIP));
		retPay->options.push_back(new DHCPopEND());

		retPay->maxLenth = maxMs;
		UDP_Packet* retUdp = new UDP_Packet(retPay);
		retUdp->sourcePort = 67;
		retUdp->destinationPort = 68;

		recvBuff.Enqueue(retUdp);
		callback();
		return true;
	}

	DHCP_Server::~DHCP_Server()
	{
		//Delete entries in queue
		while (!recvBuff.IsQueueEmpty())
		{
			UDP_Packet* retPay = nullptr;
			if (!recvBuff.Dequeue(&retPay))
			{
				std::this_thread::yield();
				continue;
			}

			delete retPay;
		}
	}
} // namespace InternalServers
