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

#include "PrecompiledHeader.h"
#include "common/Assertions.h"
#include "common/StringUtil.h"

#ifdef __POSIX__
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#ifdef __linux__
#include <sys/ioctl.h>
#endif
#endif

#include "sockets.h"
#include "DEV9.h"

#include "Sessions/ICMP_Session/ICMP_Session.h"
#include "Sessions/TCP_Session/TCP_Session.h"
#include "Sessions/UDP_Session/UDP_FixedPort.h"
#include "Sessions/UDP_Session/UDP_Session.h"

#include "PacketReader/EthernetFrame.h"
#include "PacketReader/ARP/ARP_Packet.h"
#include "PacketReader/IP/ICMP/ICMP_Packet.h"
#include "PacketReader/IP/TCP/TCP_Packet.h"
#include "PacketReader/IP/UDP/UDP_Packet.h"

using namespace Sessions;
using namespace PacketReader;
using namespace PacketReader::ARP;
using namespace PacketReader::IP;
using namespace PacketReader::IP::ICMP;
using namespace PacketReader::IP::TCP;
using namespace PacketReader::IP::UDP;

std::vector<AdapterEntry> SocketAdapter::GetAdapters()
{
	std::vector<AdapterEntry> nic;
	AdapterEntry autoEntry;
	autoEntry.type = Pcsx2Config::DEV9Options::NetApi::Sockets;
	autoEntry.name = "Auto";
	autoEntry.guid = "Auto";
	nic.push_back(autoEntry);

#ifdef _WIN32
	int neededSize = 128;
	std::unique_ptr<IP_ADAPTER_ADDRESSES[]> AdapterInfo = std::make_unique<IP_ADAPTER_ADDRESSES[]>(neededSize);
	ULONG dwBufLen = sizeof(IP_ADAPTER_ADDRESSES) * neededSize;

	PIP_ADAPTER_ADDRESSES pAdapterInfo;

	DWORD dwStatus = GetAdaptersAddresses(
		AF_UNSPEC,
		GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS,
		NULL,
		AdapterInfo.get(),
		&dwBufLen);

	if (dwStatus == ERROR_BUFFER_OVERFLOW)
	{
		DevCon.WriteLn("DEV9: PCAPGetWin32Adapter() buffer too small, resizing");
		//
		neededSize = dwBufLen / sizeof(IP_ADAPTER_ADDRESSES) + 1;
		AdapterInfo = std::make_unique<IP_ADAPTER_ADDRESSES[]>(neededSize);
		dwBufLen = sizeof(IP_ADAPTER_ADDRESSES) * neededSize;
		DevCon.WriteLn("DEV9: New size %i", neededSize);

		dwStatus = GetAdaptersAddresses(
			AF_UNSPEC,
			GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS,
			NULL,
			AdapterInfo.get(),
			&dwBufLen);
	}

	if (dwStatus != ERROR_SUCCESS)
		return nic;

	pAdapterInfo = AdapterInfo.get();

	do
	{
		if (pAdapterInfo->IfType != IF_TYPE_SOFTWARE_LOOPBACK &&
			pAdapterInfo->OperStatus == IfOperStatusUp)
		{
			AdapterEntry entry;
			entry.type = Pcsx2Config::DEV9Options::NetApi::Sockets;
			entry.name = StringUtil::WideStringToUTF8String(pAdapterInfo->FriendlyName);
			entry.guid = pAdapterInfo->AdapterName;

			nic.push_back(entry);
		}

		pAdapterInfo = pAdapterInfo->Next;
	} while (pAdapterInfo);

#elif defined(__POSIX__)
	ifaddrs* adapterInfo;
	ifaddrs* pAdapter;

	int error = getifaddrs(&adapterInfo);
	if (error)
		return nic;

	pAdapter = adapterInfo;

	do
	{
		if ((pAdapter->ifa_flags & IFF_LOOPBACK) == 0 &&
			(pAdapter->ifa_flags & IFF_UP) != 0 &&
			pAdapter->ifa_addr != nullptr &&
			pAdapter->ifa_addr->sa_family == AF_INET)
		{
			AdapterEntry entry;
			entry.type = Pcsx2Config::DEV9Options::NetApi::Sockets;
			entry.name = pAdapter->ifa_name;
			entry.guid = pAdapter->ifa_name;

			nic.push_back(entry);
		}

		pAdapter = pAdapter->ifa_next;
	} while (pAdapter);

	freeifaddrs(adapterInfo);
#endif

	return nic;
}

AdapterOptions SocketAdapter::GetAdapterOptions()
{
	return (AdapterOptions::DHCP_ForcedOn | AdapterOptions::DHCP_OverrideIP | AdapterOptions::DHCP_OverideSubnet | AdapterOptions::DHCP_OverideGateway);
}

SocketAdapter::SocketAdapter()
{
	bool foundAdapter;

#ifdef _WIN32
	IP_ADAPTER_ADDRESSES adapter;
	std::unique_ptr<IP_ADAPTER_ADDRESSES[]> buffer;

	if (strcmp(EmuConfig.DEV9.EthDevice.c_str(), "Auto") != 0)
	{
		foundAdapter = GetWin32SelectedAdapter(EmuConfig.DEV9.EthDevice, &adapter, &buffer);

		if (!foundAdapter)
		{
			Console.Error("DEV9: Socket: Failed to Get Adapter");
			return;
		}

		PIP_ADAPTER_UNICAST_ADDRESS address = nullptr;

		address = adapter.FirstUnicastAddress;
		while (address != nullptr && address->Address.lpSockaddr->sa_family != AF_INET)
			address = address->Next;

		if (address != nullptr)
		{
			sockaddr_in* sockaddr = (sockaddr_in*)address->Address.lpSockaddr;
			adapterIP = *(IP_Address*)&sockaddr->sin_addr;
		}
		else
		{
			Console.Error("DEV9: Socket: Failed To Get Adapter IP");
			return;
		}
	}
	else
	{
		foundAdapter = GetWin32AutoAdapter(&adapter, &buffer);
		adapterIP = {0};

		if (!foundAdapter)
		{
			Console.Error("DEV9: Socket: Auto Selection Failed, Check You Connection or Manually Specify Adapter");
			return;
		}
	}
#elif defined(__POSIX__)
	ifaddrs adapter;
	ifaddrs* buffer;

	if (strcmp(EmuConfig.DEV9.EthDevice.c_str(), "Auto") != 0)
	{
		foundAdapter = GetIfSelectedAdapter(EmuConfig.DEV9.EthDevice, &adapter, &buffer);

		if (!foundAdapter)
		{
			Console.Error("DEV9: Socket: Failed to Get Adapter");
			return;
		}

		sockaddr* address = nullptr;

		if (adapter.ifa_addr != nullptr && adapter.ifa_addr->sa_family == AF_INET)
			address = adapter.ifa_addr;

		if (address != nullptr)
		{
			sockaddr_in* sockaddr = (sockaddr_in*)address;
			adapterIP = *(IP_Address*)&sockaddr->sin_addr;
		}
		else
		{
			Console.Error("DEV9: Socket: Failed To Get Adapter IP");
			freeifaddrs(buffer);
			return;
		}
	}
	else
	{
		foundAdapter = GetIfAutoAdapter(&adapter, &buffer);
		adapterIP = {0};

		if (!foundAdapter)
		{
			Console.Error("DEV9: Socket: Auto Selection Failed, Check You Connection or Manually Specify Adapter");
			return;
		}
	}
#endif

	//For DHCP, we need to override some settings
	//DNS settings as per direct adapters

	const IP_Address ps2IP{internalIP.bytes[0], internalIP.bytes[1], internalIP.bytes[2], 100};
	const IP_Address subnet{255, 255, 255, 0};
	const IP_Address gateway = internalIP;

	InitInternalServer(&adapter, true, ps2IP, subnet, gateway);
#ifdef __POSIX__
	freeifaddrs(buffer);
#endif

	u8 hostMAC[6];
	u8 newMAC[6];

#ifdef _WIN32
	memcpy(hostMAC, adapter.PhysicalAddress, 6);
#elif defined(__linux__)
	struct ifreq ifr;
	int fd = socket(AF_INET, SOCK_DGRAM, 0);
	strcpy(ifr.ifr_name, adapter.ifa_name);
	if (0 == ioctl(fd, SIOCGIFHWADDR, &ifr))
		memcpy(hostMAC, ifr.ifr_hwaddr.sa_data, 6);
	else
	{
		memcpy(hostMAC, ps2MAC, 6);
		Console.Error("Could not get MAC address for adapter: %s", adapter.ifa_name);
	}
	::close(fd);
#else
	memcpy(hostMAC, ps2MAC, 6);
	Console.Error("Could not get MAC address for adapter, OS not supported");
#endif
	memcpy(newMAC, ps2MAC, 6);

	//Lets take the hosts last 2 bytes to make it unique on Xlink
	newMAC[5] = hostMAC[4];
	newMAC[4] = hostMAC[5];

	SetMACAddress(newMAC);

#ifdef _WIN32
	/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
	WORD wVersionRequested = MAKEWORD(2, 2);

	WSADATA wsaData{0};
	const int err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0)
	{
		Console.Error("DEV9: WSAStartup failed with error: %d\n", err);
		return;
	}
	else
		wsa_init = true;
#endif

	initialized = true;
}

#ifdef _WIN32
bool SocketAdapter::GetWin32SelectedAdapter(const std::string& name, PIP_ADAPTER_ADDRESSES adapter, std::unique_ptr<IP_ADAPTER_ADDRESSES[]>* buffer)
{
	int neededSize = 128;
	std::unique_ptr<IP_ADAPTER_ADDRESSES[]> AdapterInfo = std::make_unique<IP_ADAPTER_ADDRESSES[]>(neededSize);
	ULONG dwBufLen = sizeof(IP_ADAPTER_ADDRESSES) * neededSize;

	PIP_ADAPTER_ADDRESSES pAdapterInfo;

	DWORD dwStatus = GetAdaptersAddresses(
		AF_UNSPEC,
		GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS,
		NULL,
		AdapterInfo.get(),
		&dwBufLen);

	if (dwStatus == ERROR_BUFFER_OVERFLOW)
	{
		DevCon.WriteLn("DEV9: GetWin32Adapter() buffer too small, resizing");
		neededSize = dwBufLen / sizeof(IP_ADAPTER_ADDRESSES) + 1;
		AdapterInfo = std::make_unique<IP_ADAPTER_ADDRESSES[]>(neededSize);
		dwBufLen = sizeof(IP_ADAPTER_ADDRESSES) * neededSize;
		DevCon.WriteLn("DEV9: New size %i", neededSize);

		dwStatus = GetAdaptersAddresses(
			AF_UNSPEC,
			GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS,
			NULL,
			AdapterInfo.get(),
			&dwBufLen);
	}
	if (dwStatus != ERROR_SUCCESS)
		return false;

	pAdapterInfo = AdapterInfo.get();

	do
	{
		if (strcmp(pAdapterInfo->AdapterName, name.c_str()) == 0)
		{
			*adapter = *pAdapterInfo;
			buffer->swap(AdapterInfo);
			return true;
		}

		pAdapterInfo = pAdapterInfo->Next;
	} while (pAdapterInfo);
	return false;
}
bool SocketAdapter::GetWin32AutoAdapter(PIP_ADAPTER_ADDRESSES adapter, std::unique_ptr<IP_ADAPTER_ADDRESSES[]>* buffer)
{
	int neededSize = 128;
	std::unique_ptr<IP_ADAPTER_ADDRESSES[]> AdapterInfo = std::make_unique<IP_ADAPTER_ADDRESSES[]>(neededSize);
	ULONG dwBufLen = sizeof(IP_ADAPTER_ADDRESSES) * neededSize;

	PIP_ADAPTER_ADDRESSES pAdapterInfo;

	DWORD dwStatus = GetAdaptersAddresses(
		AF_UNSPEC,
		GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS,
		NULL,
		AdapterInfo.get(),
		&dwBufLen);

	if (dwStatus == ERROR_BUFFER_OVERFLOW)
	{
		DevCon.WriteLn("DEV9: PCAPGetWin32Adapter() buffer too small, resizing");
		//
		neededSize = dwBufLen / sizeof(IP_ADAPTER_ADDRESSES) + 1;
		AdapterInfo = std::make_unique<IP_ADAPTER_ADDRESSES[]>(neededSize);
		dwBufLen = sizeof(IP_ADAPTER_ADDRESSES) * neededSize;
		DevCon.WriteLn("DEV9: New size %i", neededSize);

		dwStatus = GetAdaptersAddresses(
			AF_UNSPEC,
			GAA_FLAG_INCLUDE_PREFIX | GAA_FLAG_INCLUDE_GATEWAYS,
			NULL,
			AdapterInfo.get(),
			&dwBufLen);
	}

	if (dwStatus != ERROR_SUCCESS)
		return 0;

	pAdapterInfo = AdapterInfo.get();

	do
	{
		if (pAdapterInfo->IfType != IF_TYPE_SOFTWARE_LOOPBACK &&
			pAdapterInfo->OperStatus == IfOperStatusUp)
		{
			//Search for an adapter with;
			//IPv4 Address
			//DNS
			//Gateway

			bool hasIPv4 = false;
			bool hasDNS = false;
			bool hasGateway = false;

			//IPv4
			PIP_ADAPTER_UNICAST_ADDRESS ipAddress = nullptr;
			ipAddress = pAdapterInfo->FirstUnicastAddress;
			while (ipAddress != nullptr && ipAddress->Address.lpSockaddr->sa_family != AF_INET)
				ipAddress = ipAddress->Next;
			if (ipAddress != nullptr)
				hasIPv4 = true;

			//DNS
			PIP_ADAPTER_DNS_SERVER_ADDRESS dnsAddress = pAdapterInfo->FirstDnsServerAddress;
			while (dnsAddress != nullptr && dnsAddress->Address.lpSockaddr->sa_family != AF_INET)
				dnsAddress = dnsAddress->Next;
			if (dnsAddress != nullptr)
				hasDNS = true;

			//Gateway
			PIP_ADAPTER_GATEWAY_ADDRESS gatewayAddress = pAdapterInfo->FirstGatewayAddress;
			while (gatewayAddress != nullptr && gatewayAddress->Address.lpSockaddr->sa_family != AF_INET)
				gatewayAddress = gatewayAddress->Next;
			if (gatewayAddress != nullptr)
				hasGateway = true;

			if (hasIPv4 && hasDNS && hasGateway)
			{
				*adapter = *pAdapterInfo;
				buffer->swap(AdapterInfo);
				return true;
			}
		}

		pAdapterInfo = pAdapterInfo->Next;
	} while (pAdapterInfo);

	return false;
}
#elif defined(__POSIX__)
bool SocketAdapter::GetIfSelectedAdapter(const std::string& name, ifaddrs* adapter, ifaddrs** buffer)
{
	ifaddrs* adapterInfo;
	ifaddrs* pAdapter;

	int error = getifaddrs(&adapterInfo);
	if (error)
		return false;

	pAdapter = adapterInfo;

	do
	{
		if (pAdapter->ifa_addr->sa_family == AF_INET && strcmp(pAdapter->ifa_name, name.c_str()) == 0)
			break;

		pAdapter = pAdapter->ifa_next;
	} while (pAdapter);

	if (pAdapter != nullptr)
	{
		*adapter = *pAdapter;
		*buffer = adapterInfo;
		return true;
	}

	freeifaddrs(adapterInfo);
	return false;
}
bool SocketAdapter::GetIfAutoAdapter(ifaddrs* adapter, ifaddrs** buffer)
{
	ifaddrs* adapterInfo;
	ifaddrs* pAdapter;

	int error = getifaddrs(&adapterInfo);
	if (error)
		return false;

	pAdapter = adapterInfo;

	do
	{
		if ((pAdapter->ifa_flags & IFF_LOOPBACK) == 0 &&
			(pAdapter->ifa_flags & IFF_UP) != 0)
		{
			//Search for an adapter with;
			//IPv4 Address
			//Gateway

			bool hasIPv4 = false;
			bool hasGateway = false;

			if (pAdapter->ifa_addr->sa_family == AF_INET)
				hasIPv4 = true;

#ifdef __linux__
			std::vector<IP_Address> gateways = InternalServers::DHCP_Server::GetGatewaysLinux(pAdapter->ifa_name);

			if (gateways.size() > 0)
				hasGateway = true;

#elif defined(__FreeBSD__) || (__APPLE__)
			std::vector<IP_Address> gateways = InternalServers::DHCP_Server::GetGatewaysBSD(pAdapter->ifa_name);

			if (gateways.size() > 0)
				hasGateway = true;

#else
			Console.Error("DHCP: Unsupported OS, can't find Gateway");
#endif
			if (hasIPv4 && hasGateway)
			{
				*adapter = *pAdapter;
				*buffer = adapterInfo;
				return true;
			}
		}

		pAdapter = pAdapter->ifa_next;
	} while (pAdapter);

	freeifaddrs(adapterInfo);
	return false;
}
#endif

bool SocketAdapter::blocks()
{
	return false;
}

bool SocketAdapter::isInitialised()
{
	return initialized;
}

bool SocketAdapter::recv(NetPacket* pkt)
{
	if (NetAdapter::recv(pkt))
		return true;

	EthernetFrame* bFrame;
	if (!vRecBuffer.Dequeue(&bFrame))
	{
		std::vector<ConnectionKey> keys = connections.GetKeys();
		for (size_t i = 0; i < keys.size(); i++)
		{
			const ConnectionKey key = keys[i];

			BaseSession* session;
			if (!connections.TryGetValue(key, &session))
				continue;

			IP_Payload* pl = session->Recv();

			if (pl != nullptr)
			{
				IP_Packet* ipPkt = new IP_Packet(pl);
				ipPkt->destinationIP = session->sourceIP;
				ipPkt->sourceIP = session->destIP;

				EthernetFrame frame(ipPkt);
				memcpy(frame.sourceMAC, internalMAC, 6);
				memcpy(frame.destinationMAC, ps2MAC, 6);
				frame.protocol = (u16)EtherType::IPv4;

				frame.WritePacket(pkt);
				InspectRecv(pkt);
				return true;
			}
		}
	}
	else
	{
		bFrame->WritePacket(pkt);
		InspectRecv(pkt);

		delete bFrame;
		return true;
	}
	return false;
}

bool SocketAdapter::send(NetPacket* pkt)
{
	InspectSend(pkt);
	if (NetAdapter::send(pkt))
		return true;

	bool result = false;

	EthernetFrame frame(pkt);

	switch (frame.protocol)
	{
		case (u16)EtherType::null:
		case 0x0C00:
			//Packets with the above ethertypes get sent when the adapter is reset
			//Catch them here instead of printing an error
			return true;
		case (int)EtherType::IPv4:
		{
			PayloadPtr* payload = static_cast<PayloadPtr*>(frame.GetPayload());
			IP_Packet ippkt(payload->data, payload->GetLength());

			return SendIP(&ippkt);
		}
		case (u16)EtherType::ARP:
		{
			PayloadPtr* payload = static_cast<PayloadPtr*>(frame.GetPayload());
			ARP_Packet arpPkt(payload->data, payload->GetLength());

			if (arpPkt.protocol == (u16)EtherType::IPv4)
			{
				if (arpPkt.op == 1) //ARP request
				{
					if (*(IP_Address*)arpPkt.targetProtocolAddress.get() != dhcpServer.ps2IP)
					//it's trying to resolve the virtual gateway's mac addr
					{
						ARP_Packet* arpRet = new ARP_Packet(6, 4);
						memcpy(arpRet->targetHardwareAddress.get(), arpPkt.senderHardwareAddress.get(), 6);
						memcpy(arpRet->senderHardwareAddress.get(), internalMAC, 6);
						memcpy(arpRet->targetProtocolAddress.get(), arpPkt.senderProtocolAddress.get(), 4);
						memcpy(arpRet->senderProtocolAddress.get(), arpPkt.targetProtocolAddress.get(), 4);
						arpRet->op = 2,
						arpRet->protocol = arpPkt.protocol;

						EthernetFrame* retARP = new EthernetFrame(arpRet);
						memcpy(retARP->destinationMAC, ps2MAC, 6);
						memcpy(retARP->sourceMAC, internalMAC, 6);
						retARP->protocol = (u16)EtherType::ARP;

						vRecBuffer.Enqueue(retARP);
					}
				}
			}
			return true;
		}
		default:
			Console.Error("DEV9: Socket: Unkown EtherframeType %X", frame.protocol);
			return false;
	}

	return result;
}

void SocketAdapter::reset()
{
	//Adapter Reset
	std::vector<ConnectionKey> keys = connections.GetKeys();
	DevCon.WriteLn("DEV9: Socket: Reset %d Connections", keys.size());
	for (size_t i = 0; i < keys.size(); i++)
	{
		ConnectionKey key = keys[i];

		BaseSession* session;
		if (!connections.TryGetValue(key, &session))
			continue;

		session->Reset();
	}
}

void SocketAdapter::reloadSettings()
{
	bool foundAdapter = false;
#ifdef _WIN32
	IP_ADAPTER_ADDRESSES adapter;
	std::unique_ptr<IP_ADAPTER_ADDRESSES[]> buffer;

	if (strcmp(EmuConfig.DEV9.EthDevice.c_str(), "Auto") != 0)
		foundAdapter = GetWin32SelectedAdapter(EmuConfig.DEV9.EthDevice, &adapter, &buffer);
	else
		foundAdapter = GetWin32AutoAdapter(&adapter, &buffer);

#elif defined(__POSIX__)
	ifaddrs adapter;
	ifaddrs* buffer;

	if (strcmp(EmuConfig.DEV9.EthDevice.c_str(), "Auto") != 0)
		foundAdapter = GetIfSelectedAdapter(EmuConfig.DEV9.EthDevice, &adapter, &buffer);
	else
		foundAdapter = GetIfAutoAdapter(&adapter, &buffer);
#endif

	const IP_Address ps2IP = {internalIP.bytes[0], internalIP.bytes[1], internalIP.bytes[2], 100};
	const IP_Address subnet{255, 255, 255, 0};
	const IP_Address gateway = internalIP;

	if (foundAdapter)
	{
		ReloadInternalServer(&adapter, true, ps2IP, subnet, gateway);
#ifdef __POSIX__
		freeifaddrs(buffer);
#endif
	}
	else
	{
		pxAssert(false);
		ReloadInternalServer(nullptr, true, ps2IP, subnet, gateway);
	}
}

bool SocketAdapter::SendIP(IP_Packet* ipPkt)
{
	if (ipPkt->VerifyChecksum() == false)
	{
		Console.Error("DEV9: Socket: IP packet with bad CSUM");
		return false;
	}
	//Do Checksum in sub functions

	ConnectionKey Key{0};
	Key.ip = ipPkt->destinationIP;
	Key.protocol = ipPkt->protocol;

	switch (ipPkt->protocol) //(Prase Payload)
	{
		case (u8)IP_Type::ICMP:
			return SendICMP(Key, ipPkt);
		case (u8)IP_Type::IGMP:
			return SendIGMP(Key, ipPkt);
		case (u8)IP_Type::TCP:
			return SendTCP(Key, ipPkt);
		case (u8)IP_Type::UDP:
			return SendUDP(Key, ipPkt);
		default:
			//Log_Error("Unkown Protocol");
			Console.Error("DEV9: Socket: Unkown IPv4 Protocol %X", ipPkt->protocol);
			return false;
	}
}

bool SocketAdapter::SendICMP(ConnectionKey Key, IP_Packet* ipPkt)
{
	//IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(ipPkt->GetPayload());

	ICMP_Session* s;

	//Need custom SendFromConnection
	BaseSession* existingSession = nullptr;
	connections.TryGetValue(Key, &existingSession);
	if (existingSession != nullptr)
	{
		s = static_cast<ICMP_Session*>(existingSession);
		return s->Send(ipPkt->GetPayload(), ipPkt);
	}

	DevCon.WriteLn("DEV9: Socket: Creating New ICMP Connection");
	s = new ICMP_Session(Key, adapterIP, &connections);

	s->AddConnectionClosedHandler([&](BaseSession* session) { HandleConnectionClosed(session); });
	s->destIP = ipPkt->destinationIP;
	s->sourceIP = dhcpServer.ps2IP;
	connections.Add(Key, s);
	return s->Send(ipPkt->GetPayload(), ipPkt);
}

bool SocketAdapter::SendIGMP(ConnectionKey Key, IP_Packet* ipPkt)
{
	Console.Error("DEV9: Socket: IGMP Packets not supported in socket mode");
	return false;
}

bool SocketAdapter::SendTCP(ConnectionKey Key, IP_Packet* ipPkt)
{
	IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(ipPkt->GetPayload());
	TCP_Packet tcp(ipPayload->data, ipPayload->GetLength());

	Key.ps2Port = tcp.sourcePort;
	Key.srvPort = tcp.destinationPort;

	int res = SendFromConnection(Key, ipPkt);
	if (res == 1)
		return true;
	else if (res == 0)
		return false;
	else
	{
		Console.WriteLn("DEV9: Socket: Creating New TCP Connection to %d", tcp.destinationPort);
		TCP_Session* s = new TCP_Session(Key, adapterIP);

		s->AddConnectionClosedHandler([&](BaseSession* session) { HandleConnectionClosed(session); });
		s->destIP = ipPkt->destinationIP;
		s->sourceIP = dhcpServer.ps2IP;
		connections.Add(Key, s);
		return s->Send(ipPkt->GetPayload());
	}
}

bool SocketAdapter::SendUDP(ConnectionKey Key, IP_Packet* ipPkt)
{
	IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(ipPkt->GetPayload());
	UDP_Packet udp(ipPayload->data, ipPayload->GetLength());

	Key.ps2Port = udp.sourcePort;
	Key.srvPort = udp.destinationPort;

	const int res = SendFromConnection(Key, ipPkt);
	if (res == 1)
		return true;
	else if (res == 0)
		return false;
	else
	{
		UDP_Session* s = nullptr;

		if (udp.sourcePort == udp.destinationPort || //Used for LAN games that assume the destination port
			ipPkt->destinationIP == dhcpServer.broadcastIP || //Broadcast packets
			ipPkt->destinationIP == IP_Address{255, 255, 255, 255} || //Limited Broadcast packets
			(ipPkt->destinationIP.bytes[0] & 0xF0) == 0xE0) //Multicast address start with 0b1110
		{
			UDP_FixedPort* fPort = nullptr;
			BaseSession* fSession;
			if (fixedUDPPorts.TryGetValue(udp.sourcePort, &fSession))
			{
				//DevCon.WriteLn("DEV9: Socket: Using Existing UDPFixedPort");
				fPort = static_cast<UDP_FixedPort*>(fSession);
			}
			else
			{
				ConnectionKey fKey{0};
				fKey.protocol = (u8)IP_Type::UDP;
				fKey.ps2Port = udp.sourcePort;
				fKey.srvPort = 0;

				Console.WriteLn("DEV9: Socket: Creating New UDPFixedPort with port %d", udp.sourcePort);

				fPort = new UDP_FixedPort(fKey, adapterIP, udp.sourcePort);
				fPort->AddConnectionClosedHandler([&](BaseSession* session) { HandleFixedPortClosed(session); });

				fPort->destIP = {0, 0, 0, 0};
				fPort->sourceIP = dhcpServer.ps2IP;

				connections.Add(fKey, fPort);
				fixedUDPPorts.Add(udp.sourcePort, fPort);
			}

			Console.WriteLn("DEV9: Socket: Creating New UDP Connection from FixedPort %d", udp.sourcePort);
			s = fPort->NewClientSession(Key,
				ipPkt->destinationIP == dhcpServer.broadcastIP || ipPkt->destinationIP == IP_Address{255, 255, 255, 255},
				(ipPkt->destinationIP.bytes[0] & 0xF0) == 0xE0);
		}
		else
		{
			Console.WriteLn("DEV9: Socket: Creating New UDP Connection to %d", udp.sourcePort);
			s = new UDP_Session(Key, adapterIP);
		}

		s->AddConnectionClosedHandler([&](BaseSession* session) { HandleConnectionClosed(session); });
		s->destIP = ipPkt->destinationIP;
		s->sourceIP = dhcpServer.ps2IP;
		connections.Add(Key, s);
		return s->Send(ipPkt->GetPayload());
	}
}

int SocketAdapter::SendFromConnection(ConnectionKey Key, IP_Packet* ipPkt)
{
	BaseSession* s = nullptr;
	connections.TryGetValue(Key, &s);
	if (s != nullptr)
		return s->Send(ipPkt->GetPayload()) ? 1 : 0;
	else
		return -1;
}

void SocketAdapter::HandleConnectionClosed(BaseSession* sender)
{
	ConnectionKey key = sender->key;
	connections.Remove(key);
	//Note, we delete something that is calling us
	//this is probably going to cause issues
	delete sender;

	switch (key.protocol)
	{
		case (int)IP_Type::UDP:
			Console.WriteLn("DEV9: Socket: Closed Dead UDP Connection to %d", key.srvPort);
			break;
		case (int)IP_Type::TCP:
			Console.WriteLn("DEV9: Socket: Closed Dead TCP Connection to %d", key.srvPort);
			break;
		case (int)IP_Type::ICMP:
			Console.WriteLn("DEV9: Socket: Closed Dead ICMP Connection");
			break;
		case (int)IP_Type::IGMP:
			Console.WriteLn("DEV9: Socket: Closed Dead ICMP Connection");
			break;
		default:
			Console.WriteLn("DEV9: Socket: Closed Dead Unk Connection");
			break;
	}
}

void SocketAdapter::HandleFixedPortClosed(BaseSession* sender)
{
	ConnectionKey key = sender->key;
	connections.Remove(key);
	fixedUDPPorts.Remove(key.ps2Port);
	//Note, we delete something that is calling us
	//this is probably going to cause issues
	delete sender;

	Console.WriteLn("DEV9: Socket: Closed Dead UDP Fixed Port to %d", key.ps2Port);
}

void SocketAdapter::close()
{
}

SocketAdapter::~SocketAdapter()
{
	//Force close all sessions
	std::vector<ConnectionKey> keys = connections.GetKeys();
	DevCon.WriteLn("DEV9: Socket: Closing %d Connections", keys.size());
	for (size_t i = 0; i < keys.size(); i++)
	{
		const ConnectionKey key = keys[i];
		BaseSession* session;
		if (!connections.TryGetValue(key, &session))
			continue;
		delete session;
	}
	connections.Clear();
	fixedUDPPorts.Clear(); //fixedUDP sessions already deleted via connections

	//Clear out vRecBuffer
	while (!vRecBuffer.IsQueueEmpty())
	{
		EthernetFrame* retPay;
		if (!vRecBuffer.Dequeue(&retPay))
		{
			using namespace std::chrono_literals;
			std::this_thread::sleep_for(1ms);
			continue;
		}

		delete retPay;
	}
}
