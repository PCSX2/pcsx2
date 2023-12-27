// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include <algorithm>
#ifdef __POSIX__
#include <string>
#include <vector>
#include <fstream>
#include <netinet/in.h>
#include <arpa/inet.h>

#if defined(__FreeBSD__) || (__APPLE__)
#include <sys/param.h>
#include <sys/sysctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <net/route.h>
#endif
#endif

#include "DHCP_Server.h"
#include "DEV9/PacketReader/IP/UDP/UDP_Packet.h"
#include "DEV9/PacketReader/IP/UDP/DHCP/DHCP_Packet.h"

#include "DEV9/DEV9.h"
#include "DEV9/AdapterUtils.h"

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
	void DHCP_Server::Init(PIP_ADAPTER_ADDRESSES adapter, IP_Address ipOverride, IP_Address subnetOverride, IP_Address gatewayOverride)
#elif defined(__POSIX__)
	void DHCP_Server::Init(ifaddrs* adapter, IP_Address ipOverride, IP_Address subnetOverride, IP_Address gatewayOverride)
#endif
	{
		netmask = {};
		gateway = {};
		dns1 = {};
		dns2 = {};
		broadcastIP = {};

		if (ipOverride.integer != 0)
			ps2IP = ipOverride;
		else
			ps2IP = *(IP_Address*)&EmuConfig.DEV9.PS2IP;

		if (subnetOverride.integer != 0)
			netmask = subnetOverride;
		else if (EmuConfig.DEV9.AutoMask)
			AutoNetmask(adapter);
		else
			netmask = *(IP_Address*)EmuConfig.DEV9.Mask;

		if (gatewayOverride.integer != 0)
			gateway = gatewayOverride;
		else if (EmuConfig.DEV9.AutoGateway)
			AutoGateway(adapter);
		else
			gateway = *(IP_Address*)EmuConfig.DEV9.Gateway;

		switch (EmuConfig.DEV9.ModeDNS1)
		{
			case Pcsx2Config::DEV9Options::DnsMode::Manual:
				dns1 = *(IP_Address*)EmuConfig.DEV9.DNS1;
				break;
			case Pcsx2Config::DEV9Options::DnsMode::Internal:
				dns1 = {{{192, 0, 2, 1}}};
				break;
			default:
				break;
		}

		switch (EmuConfig.DEV9.ModeDNS2)
		{
			case Pcsx2Config::DEV9Options::DnsMode::Manual:
				dns2 = *(IP_Address*)EmuConfig.DEV9.DNS2;
				break;
			case Pcsx2Config::DEV9Options::DnsMode::Internal:
				dns2 = {{{192, 0, 2, 1}}};
				break;
			default:
				break;
		}

		AutoDNS(adapter, EmuConfig.DEV9.ModeDNS1 == Pcsx2Config::DEV9Options::DnsMode::Auto, EmuConfig.DEV9.ModeDNS2 == Pcsx2Config::DEV9Options::DnsMode::Auto);
		AutoBroadcast(ps2IP, netmask);
	}

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
#endif

#ifdef _WIN32
	void DHCP_Server::AutoGateway(PIP_ADAPTER_ADDRESSES adapter)
#elif defined(__POSIX__)
	void DHCP_Server::AutoGateway(ifaddrs* adapter)
#endif
	{
		std::vector<IP_Address> gateways = AdapterUtils::GetGateways(adapter);

		if (gateways.size() > 0)
			gateway = gateways[0];
	}

#ifdef _WIN32
	void DHCP_Server::AutoDNS(PIP_ADAPTER_ADDRESSES adapter, bool autoDNS1, bool autoDNS2)
#elif defined(__POSIX__)
	void DHCP_Server::AutoDNS(ifaddrs* adapter, bool autoDNS1, bool autoDNS2)
#endif
	{
		std::vector<IP_Address> dnsIPs = AdapterUtils::GetDNS(adapter);

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
			dns2 = {};
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

		retPay->maxLength = maxMs;
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
