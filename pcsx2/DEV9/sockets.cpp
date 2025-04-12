// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/Assertions.h"
#include "common/StringUtil.h"
#include "common/ScopedGuard.h"

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#elif defined(__POSIX__)
#include <sys/types.h>
#include <ifaddrs.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#endif

#include "sockets.h"
#include "AdapterUtils.h"
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
	AdapterUtils::AdapterBuffer adapterInfo;
	PIP_ADAPTER_ADDRESSES pAdapter = AdapterUtils::GetAllAdapters(&adapterInfo);
	if (pAdapter == nullptr)
		return nic;

	do
	{
		if (pAdapter->IfType != IF_TYPE_SOFTWARE_LOOPBACK &&
			pAdapter->OperStatus == IfOperStatusUp)
		{
			AdapterEntry entry;
			entry.type = Pcsx2Config::DEV9Options::NetApi::Sockets;
			entry.name = StringUtil::WideStringToUTF8String(pAdapter->FriendlyName);
			entry.guid = pAdapter->AdapterName;

			nic.push_back(entry);
		}

		pAdapter = pAdapter->Next;
	} while (pAdapter);

#elif defined(__POSIX__)
	AdapterUtils::AdapterBuffer adapterInfo;
	ifaddrs* pAdapter = GetAllAdapters(&adapterInfo);
	if (pAdapter == nullptr)
		return nic;

	do
	{
		if ((pAdapter->ifa_flags & IFF_LOOPBACK) == 0 &&
			(pAdapter->ifa_flags & IFF_UP) != 0 &&
			pAdapter->ifa_addr != nullptr &&
			AdapterUtils::ReadAddressFamily(pAdapter->ifa_addr) == AF_INET)
		{
			AdapterEntry entry;
			entry.type = Pcsx2Config::DEV9Options::NetApi::Sockets;
			entry.name = pAdapter->ifa_name;
			entry.guid = pAdapter->ifa_name;

			nic.push_back(entry);
		}

		pAdapter = pAdapter->ifa_next;
	} while (pAdapter);
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

	AdapterUtils::Adapter adapter;
	AdapterUtils::AdapterBuffer buffer;

	if (strcmp(EmuConfig.DEV9.EthDevice.c_str(), "Auto") != 0)
	{
		foundAdapter = AdapterUtils::GetAdapter(EmuConfig.DEV9.EthDevice, &adapter, &buffer);

		if (!foundAdapter)
		{
			Console.Error("DEV9: Socket: Failed to Get Adapter");
			return;
		}

		std::optional<IP_Address> adIP = AdapterUtils::GetAdapterIP(&adapter);
		if (adIP.has_value())
			adapterIP = adIP.value();
		else
		{
			Console.Error("DEV9: Socket: Failed To Get Adapter IP");
			return;
		}
	}
	else
	{
		foundAdapter = AdapterUtils::GetAdapterAuto(&adapter, &buffer);
		adapterIP = {};

		if (!foundAdapter)
		{
			Console.Error("DEV9: Socket: Auto Selection Failed, Check You Connection or Manually Specify Adapter");
			return;
		}
	}

	//For DHCP, we need to override some settings
	//DNS settings as per direct adapters

	const IP_Address ps2IP{{{internalIP.bytes[0], internalIP.bytes[1], internalIP.bytes[2], 100}}};
	const IP_Address subnet{{{255, 255, 255, 0}}};
	const IP_Address gateway = internalIP;

	InitInternalServer(&adapter, true, ps2IP, subnet, gateway);

	std::optional<MAC_Address> adMAC = AdapterUtils::GetAdapterMAC(&adapter);
	if (adMAC.has_value())
	{
		MAC_Address hostMAC = adMAC.value();
		MAC_Address newMAC = ps2MAC;

		//Lets take the hosts last 2 bytes to make it unique on Xlink
		newMAC.bytes[5] = hostMAC.bytes[4];
		newMAC.bytes[4] = hostMAC.bytes[5];

		SetMACAddress(&newMAC);
	}
	else
		Console.Error("DEV9: Socket: Failed to get MAC address for adapter");

#ifdef _WIN32
	/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
	const WORD wVersionRequested = MAKEWORD(2, 2);

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

	sendThreadId = std::this_thread::get_id();

	initialized = true;
}

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

	ScopedGuard cleanup([&]() {
		// Garbage collect closed connections
		if (deleteQueueRecvThread.size() != 0)
		{
			std::lock_guard deletelock(deleteRecvSentry);
			for (BaseSession* s : deleteQueueRecvThread)
				delete s;
			deleteQueueRecvThread.clear();
		}
	});

	EthernetFrame* bFrame;
	if (!vRecBuffer.Dequeue(&bFrame))
	{
		std::lock_guard deletelock(deleteSendSentry);
		std::vector<ConnectionKey> keys = connections.GetKeys();
		for (size_t i = 0; i < keys.size(); i++)
		{
			const ConnectionKey key = keys[i];

			BaseSession* session;
			if (!connections.TryGetValue(key, &session))
				continue;

			std::optional<ReceivedPayload> pl = session->Recv();

			if (pl.has_value())
			{
				IP_Packet* ipPkt = new IP_Packet(pl->payload.release());
				ipPkt->destinationIP = session->sourceIP;
				ipPkt->sourceIP = pl->sourceIP;

				EthernetFrame frame(ipPkt);
				frame.sourceMAC = internalMAC;
				frame.destinationMAC = ps2MAC;
				frame.protocol = static_cast<u16>(EtherType::IPv4);

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

	pxAssert(std::this_thread::get_id() == sendThreadId);
	ScopedGuard cleanup([&]() {
		// Garbage collect closed connections
		if (deleteQueueSendThread.size() != 0)
		{
			std::lock_guard deletelock(deleteSendSentry);
			for (BaseSession* s : deleteQueueSendThread)
				delete s;
			deleteQueueSendThread.clear();
		}
	});

	EthernetFrame frame(pkt);

	switch (frame.protocol)
	{
		case static_cast<u16>(EtherType::null):
		case 0x0C00:
			//Packets with the above ethertypes get sent when the adapter is reset
			//Catch them here instead of printing an error
			return true;
		case static_cast<int>(EtherType::IPv4):
		{
			PayloadPtr* payload = static_cast<PayloadPtr*>(frame.GetPayload());
			IP_Packet ippkt(payload->data, payload->GetLength());

			return SendIP(&ippkt);
		}
		case static_cast<u16>(EtherType::ARP):
		{
			PayloadPtr* payload = static_cast<PayloadPtr*>(frame.GetPayload());
			ARP_Packet arpPkt(payload->data, payload->GetLength());

			if (arpPkt.protocol == static_cast<u16>(EtherType::IPv4))
			{
				if (arpPkt.op == 1) //ARP request
				{
					if (*(IP_Address*)arpPkt.targetProtocolAddress.get() != dhcpServer.ps2IP)
					//it's trying to resolve the virtual gateway's mac addr
					{
						ARP_Packet* arpRet = new ARP_Packet(6, 4);
						*(MAC_Address*)arpRet->targetHardwareAddress.get() = *(MAC_Address*)arpPkt.senderHardwareAddress.get();
						*(MAC_Address*)arpRet->senderHardwareAddress.get() = internalMAC;
						*(IP_Address*)arpRet->targetProtocolAddress.get() = *(IP_Address*)arpPkt.senderProtocolAddress.get();
						*(IP_Address*)arpRet->senderProtocolAddress.get() = *(IP_Address*)arpPkt.targetProtocolAddress.get();
						arpRet->op = 2,
						arpRet->protocol = arpPkt.protocol;
						arpRet->hardwareType = arpPkt.hardwareType;

						EthernetFrame* retARP = new EthernetFrame(arpRet);
						retARP->destinationMAC = ps2MAC;
						retARP->sourceMAC = internalMAC;
						retARP->protocol = static_cast<u16>(EtherType::ARP);

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

	return false;
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

	AdapterUtils::Adapter adapter;
	AdapterUtils::AdapterBuffer buffer;

	if (strcmp(EmuConfig.DEV9.EthDevice.c_str(), "Auto") != 0)
		foundAdapter = AdapterUtils::GetAdapter(EmuConfig.DEV9.EthDevice, &adapter, &buffer);
	else
		foundAdapter = AdapterUtils::GetAdapterAuto(&adapter, &buffer);

	const IP_Address ps2IP = {{{internalIP.bytes[0], internalIP.bytes[1], internalIP.bytes[2], 100}}};
	const IP_Address subnet{{{255, 255, 255, 0}}};
	const IP_Address gateway = internalIP;

	if (foundAdapter)
		ReloadInternalServer(&adapter, true, ps2IP, subnet, gateway);
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

	ConnectionKey Key{};
	Key.ip = ipPkt->destinationIP;
	Key.protocol = ipPkt->protocol;

	std::lock_guard deletelock(deleteRecvSentry);
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

	const int res = SendFromConnection(Key, ipPkt);
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
		// Always bind the UDP source port
		// PS2 software can run into issues if the source port is not preserved
		UDP_FixedPort* fPort = nullptr;
		BaseSession* fSession;
		if (fixedUDPPorts.TryGetValue(udp.sourcePort, &fSession))
		{
			fPort = static_cast<UDP_FixedPort*>(fSession);
		}
		else
		{
			ConnectionKey fKey{};
			fKey.protocol = static_cast<u8>(IP_Type::UDP);
			fKey.ps2Port = udp.sourcePort;
			fKey.srvPort = 0;

			Console.WriteLn("DEV9: Socket: Binding UDP fixed port %d", udp.sourcePort);

			fPort = new UDP_FixedPort(fKey, adapterIP, udp.sourcePort);
			fPort->AddConnectionClosedHandler([&](BaseSession* session) { HandleFixedPortClosed(session); });

			fPort->destIP = {};
			fPort->sourceIP = dhcpServer.ps2IP;

			connections.Add(fKey, fPort);
			fixedUDPPorts.Add(udp.sourcePort, fPort);

			fPort->Init();
		}

		Console.WriteLn("DEV9: Socket: Creating New UDP Connection from fixed port %d to %d", udp.sourcePort, udp.destinationPort);
		UDP_Session* s = fPort->NewClientSession(Key,
			ipPkt->destinationIP == dhcpServer.broadcastIP || ipPkt->destinationIP == IP_Address{{{255, 255, 255, 255}}},
			(ipPkt->destinationIP.bytes[0] & 0xF0) == 0xE0);

		// If we are unable to bind to the port, fall back to a dynamic port
		if (s == nullptr)
		{
			Console.Error("DEV9: Socket: Failed to Create New UDP Connection from fixed port");
			Console.WriteLn("DEV9: Socket: Retrying with dynamic port to %d", udp.destinationPort);
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
	const ConnectionKey key = sender->key;
	if (!connections.Remove(key))
		return;

	// Defer deleting the connection untill we have left the calling session's callstack
	if (std::this_thread::get_id() == sendThreadId)
		deleteQueueSendThread.push_back(sender);
	else
		deleteQueueRecvThread.push_back(sender);

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
	const ConnectionKey key = sender->key;
	if (!connections.Remove(key))
		return;
	fixedUDPPorts.Remove(key.ps2Port);

	// Defer deleting the connection untill we have left the calling session's callstack
	if (std::this_thread::get_id() == sendThreadId)
		deleteQueueSendThread.push_back(sender);
	else
		deleteQueueRecvThread.push_back(sender);

	Console.WriteLn("DEV9: Socket: Unbound fixed port %d", key.ps2Port);
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

	//Clear out any delete queues
	DevCon.WriteLn("DEV9: Socket: Found %d Connections in send delete queue", deleteQueueSendThread.size());
	DevCon.WriteLn("DEV9: Socket: Found %d Connections in recv delete queue", deleteQueueRecvThread.size());
	for (BaseSession* s : deleteQueueSendThread)
		delete s;
	for (BaseSession* s : deleteQueueRecvThread)
		delete s;
	deleteQueueSendThread.clear();
	deleteQueueRecvThread.clear();

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
