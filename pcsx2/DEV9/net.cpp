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

#include <chrono>
#include <thread>
#include <mutex>
#if defined(__POSIX__)
#include <pthread.h>
#endif

#include "net.h"
#include "DEV9.h"
#ifdef _WIN32
#include "Win32/tap.h"
#endif
#include "pcap_io.h"
#include "sockets.h"

#include "PacketReader/EthernetFrame.h"
#include "PacketReader/IP/IP_Packet.h"
#include "PacketReader/IP/UDP/UDP_Packet.h"

NetAdapter* nif;
std::thread rx_thread;

std::mutex rx_mutex;

volatile bool RxRunning = false;
//rx thread
void NetRxThread()
{
	NetPacket tmp;
	while (RxRunning)
	{
		while (rx_fifo_can_rx() && nif->recv(&tmp))
		{
			std::lock_guard rx_lock(rx_mutex);
			//Check if we can still rx
			if (rx_fifo_can_rx())
				rx_process(&tmp);
			else
				Console.Error("DEV9: rx_fifo_can_rx() false after nif->recv(), dropping");
		}

		using namespace std::chrono_literals;
		std::this_thread::sleep_for(1ms);
	}
}

void tx_put(NetPacket* pkt)
{
	if (nif != nullptr)
		nif->send(pkt);
	//pkt must be copied if its not processed by here, since it can be allocated on the callers stack
}

void ad_reset()
{
	if (nif != nullptr)
		nif->reset();
}

NetAdapter* GetNetAdapter()
{
	NetAdapter* na = nullptr;

	switch (EmuConfig.DEV9.EthApi)
	{
#ifdef _WIN32
		case Pcsx2Config::DEV9Options::NetApi::TAP:
			na = static_cast<NetAdapter*>(new TAPAdapter());
			break;
#endif
		case Pcsx2Config::DEV9Options::NetApi::PCAP_Bridged:
		case Pcsx2Config::DEV9Options::NetApi::PCAP_Switched:
			na = static_cast<NetAdapter*>(new PCAPAdapter());
			break;
		case Pcsx2Config::DEV9Options::NetApi::Sockets:
			na = static_cast<NetAdapter*>(new SocketAdapter());
			break;
		default:
			return 0;
	}

	if (!na->isInitialised())
	{
		delete na;
		return 0;
	}
	return na;
}

void InitNet()
{
	NetAdapter* na = GetNetAdapter();

	if (!na)
	{
		Console.Error("DEV9: Failed to GetNetAdapter()");
		EmuConfig.DEV9.EthEnable = false;
		return;
	}

	nif = na;
	RxRunning = true;

	rx_thread = std::thread(NetRxThread);

#ifdef _WIN32
	SetThreadPriority(rx_thread.native_handle(), THREAD_PRIORITY_HIGHEST);
#elif defined(__POSIX__)
	int policy = 0;
	sched_param param;

	pthread_getschedparam(rx_thread.native_handle(), &policy, &param);
	param.sched_priority = sched_get_priority_max(policy);

	pthread_setschedparam(rx_thread.native_handle(), policy, &param);
#endif
}

void ReconfigureLiveNet(const Pcsx2Config& old_config)
{
	//Eth
	if (EmuConfig.DEV9.EthEnable)
	{
		if (old_config.DEV9.EthEnable)
		{
			//Reload Net if adapter changed
			if (EmuConfig.DEV9.EthDevice != old_config.DEV9.EthDevice ||
				EmuConfig.DEV9.EthApi != old_config.DEV9.EthApi)
			{
				TermNet();
				InitNet();
				return;
			}
			else
				nif->reloadSettings();
		}
		else
			InitNet();
	}
	else if (old_config.DEV9.EthEnable)
		TermNet();
}

void TermNet()
{
	if (RxRunning)
	{
		RxRunning = false;
		nif->close();
		Console.WriteLn("DEV9: Waiting for RX-net thread to terminate..");
		rx_thread.join();
		Console.WriteLn("DEV9: Done");

		delete nif;
		nif = nullptr;
	}
}

using namespace PacketReader;
using namespace PacketReader::IP;
using namespace PacketReader::IP::UDP;

const IP_Address NetAdapter::internalIP{192, 0, 2, 1};
const u8 NetAdapter::broadcastMAC[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
const u8 NetAdapter::internalMAC[6] = {0x76, 0x6D, 0xF4, 0x63, 0x30, 0x31};

NetAdapter::NetAdapter()
{
	//Ensure eeprom matches our default
	SetMACAddress(nullptr);
}

bool NetAdapter::recv(NetPacket* pkt)
{
	if (!internalRxThreadRunning.load())
		return InternalServerRecv(pkt);
	return false;
}

bool NetAdapter::send(NetPacket* pkt)
{
	return InternalServerSend(pkt);
}

//RxRunning must be set false before this
NetAdapter::~NetAdapter()
{
	//unblock InternalServerRX thread
	if (internalRxThreadRunning.load())
	{
		internalRxThreadRunning.store(false);

		{
			std::lock_guard srvlock(internalRxMutex);
			internalRxHasData = true;
		}

		internalRxCV.notify_all();
		internalRxThread.join();
	}
}

void NetAdapter::InspectSend(NetPacket* pkt)
{
	if (EmuConfig.DEV9.EthLogDNS)
	{
		EthernetFrame frame(pkt);
		if (frame.protocol == (u16)EtherType::IPv4)
		{
			PayloadPtr* payload = static_cast<PayloadPtr*>(frame.GetPayload());
			IP_Packet ippkt(payload->data, payload->GetLength());

			if (ippkt.protocol == (u16)IP_Type::UDP)
			{
				IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(ippkt.GetPayload());
				UDP_Packet udppkt(ipPayload->data, ipPayload->GetLength());

				if (udppkt.destinationPort == 53)
				{
					Console.WriteLn("DEV9: DNS: Packet Sent To %i.%i.%i.%i",
						ippkt.destinationIP.bytes[0], ippkt.destinationIP.bytes[1], ippkt.destinationIP.bytes[2], ippkt.destinationIP.bytes[3]);
					dnsLogger.InspectSend(&udppkt);
				}
			}
		}
	}
}
void NetAdapter::InspectRecv(NetPacket* pkt)
{
	if (EmuConfig.DEV9.EthLogDNS)
	{
		EthernetFrame frame(pkt);
		if (frame.protocol == (u16)EtherType::IPv4)
		{
			PayloadPtr* payload = static_cast<PayloadPtr*>(frame.GetPayload());
			IP_Packet ippkt(payload->data, payload->GetLength());

			if (ippkt.protocol == (u16)IP_Type::UDP)
			{
				IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(ippkt.GetPayload());
				UDP_Packet udppkt(ipPayload->data, ipPayload->GetLength());

				if (udppkt.sourcePort == 53)
				{
					Console.WriteLn("DEV9: DNS: Packet Sent From %i.%i.%i.%i",
						ippkt.sourceIP.bytes[0], ippkt.sourceIP.bytes[1], ippkt.sourceIP.bytes[2], ippkt.sourceIP.bytes[3]);
					dnsLogger.InspectRecv(&udppkt);
				}
			}
		}
	}
}

void NetAdapter::SetMACAddress(u8* mac)
{
	if (mac == nullptr)
		memcpy(ps2MAC, defaultMAC, 6);
	else
		memcpy(ps2MAC, mac, 6);

	for (int i = 0; i < 3; i++)
		dev9.eeprom[i] = ((u16*)ps2MAC)[i];

	//The checksum seems to be all the values of the mac added up in 16bit chunks
	dev9.eeprom[3] = (dev9.eeprom[0] + dev9.eeprom[1] + dev9.eeprom[2]) & 0xffff;
}

bool NetAdapter::VerifyPkt(NetPacket* pkt, int read_size)
{
	if ((memcmp(pkt->buffer, ps2MAC, 6) != 0) && (memcmp(pkt->buffer, &broadcastMAC, 6) != 0))
	{
		//ignore strange packets
		return false;
	}

	if (memcmp(pkt->buffer + 6, ps2MAC, 6) == 0)
	{
		//avoid pcap looping packets
		return false;
	}
	pkt->size = read_size;
	return true;
}

#ifdef _WIN32
void NetAdapter::InitInternalServer(PIP_ADAPTER_ADDRESSES adapter, bool dhcpForceEnable, IP_Address ipOverride, IP_Address subnetOverride, IP_Address gatewayOvveride)
#elif defined(__POSIX__)
void NetAdapter::InitInternalServer(ifaddrs* adapter, bool dhcpForceEnable, IP_Address ipOverride, IP_Address subnetOverride, IP_Address gatewayOvveride)
#endif
{
	if (adapter == nullptr)
		Console.Error("DEV9: InitInternalServer() got nullptr for adapter");

	dhcpOn = EmuConfig.DEV9.InterceptDHCP || dhcpForceEnable;
	if (dhcpOn)
		dhcpServer.Init(adapter, ipOverride, subnetOverride, gatewayOvveride);

	dnsServer.Init(adapter);

	if (blocks())
	{
		internalRxThreadRunning.store(true);
		internalRxThread = std::thread(&NetAdapter::InternalServerThread, this);
	}
}

#ifdef _WIN32
void NetAdapter::ReloadInternalServer(PIP_ADAPTER_ADDRESSES adapter, bool dhcpForceEnable, IP_Address ipOverride, IP_Address subnetOverride, IP_Address gatewayOveride)
#elif defined(__POSIX__)
void NetAdapter::ReloadInternalServer(ifaddrs* adapter, bool dhcpForceEnable, IP_Address ipOverride, IP_Address subnetOverride, IP_Address gatewayOveride)
#endif
{
	if (adapter == nullptr)
		Console.Error("DEV9: ReloadInternalServer() got nullptr for adapter");

	dhcpOn = EmuConfig.DEV9.InterceptDHCP || dhcpForceEnable;
	if (dhcpOn)
		dhcpServer.Init(adapter, ipOverride, subnetOverride, gatewayOveride);

	dnsServer.Init(adapter);
}

bool NetAdapter::InternalServerRecv(NetPacket* pkt)
{
	IP_Payload* ippay;
	ippay = dhcpServer.Recv();
	if (ippay != nullptr)
	{
		IP_Packet* ippkt = new IP_Packet(ippay);
		ippkt->destinationIP = {255, 255, 255, 255};
		ippkt->sourceIP = internalIP;
		EthernetFrame frame(ippkt);
		memcpy(frame.sourceMAC, internalMAC, 6);
		memcpy(frame.destinationMAC, ps2MAC, 6);
		frame.protocol = (u16)EtherType::IPv4;
		frame.WritePacket(pkt);
		return true;
	}

	ippay = dnsServer.Recv();
	if (ippay != nullptr)
	{
		IP_Packet* ippkt = new IP_Packet(ippay);
		ippkt->destinationIP = ps2IP;
		ippkt->sourceIP = internalIP;
		EthernetFrame frame(ippkt);
		memcpy(frame.sourceMAC, internalMAC, 6);
		memcpy(frame.destinationMAC, ps2MAC, 6);
		frame.protocol = (u16)EtherType::IPv4;
		frame.WritePacket(pkt);
		InspectRecv(pkt);
		return true;
	}

	return false;
}

bool NetAdapter::InternalServerSend(NetPacket* pkt)
{
	EthernetFrame frame(pkt);
	if (frame.protocol == (u16)EtherType::IPv4)
	{
		PayloadPtr* payload = static_cast<PayloadPtr*>(frame.GetPayload());
		IP_Packet ippkt(payload->data, payload->GetLength());

		if (ippkt.protocol == (u16)IP_Type::UDP)
		{
			IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(ippkt.GetPayload());
			UDP_Packet udppkt(ipPayload->data, ipPayload->GetLength());

			if (udppkt.destinationPort == 67)
			{
				//Send DHCP
				if (dhcpOn)
					return dhcpServer.Send(&udppkt);
			}
		}

		if (ippkt.destinationIP == internalIP)
		{
			if (ippkt.protocol == (u16)IP_Type::UDP)
			{
				ps2IP = ippkt.sourceIP;

				IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(ippkt.GetPayload());
				UDP_Packet udppkt(ipPayload->data, ipPayload->GetLength());

				if (udppkt.destinationPort == 53)
				{
					//Send DNS
					return dnsServer.Send(&udppkt);
				}
			}
			return true;
		}
	}
	return false;
}

void NetAdapter::InternalSignalReceived()
{
	//Signal internal server thread to read
	if (internalRxThreadRunning.load())
	{
		{
			std::lock_guard srvlock(internalRxMutex);
			internalRxHasData = true;
		}

		internalRxCV.notify_all();
	}
}

void NetAdapter::InternalServerThread()
{
	NetPacket tmp;
	while (internalRxThreadRunning.load())
	{
		std::unique_lock srvLock(internalRxMutex);
		internalRxCV.wait(srvLock, [&] { return internalRxHasData; });

		{
			std::lock_guard rx_lock(rx_mutex);
			while (rx_fifo_can_rx() && InternalServerRecv(&tmp))
				rx_process(&tmp);
		}

		internalRxHasData = false;
	}
}
