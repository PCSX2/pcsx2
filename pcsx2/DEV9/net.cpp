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
#if defined(__POSIX__)
#include <pthread.h>
#endif
#include "net.h"
#include "DEV9.h"
#include "PacketReader/EthernetFrame.h"
#include "PacketReader/ARP/ARP_Packet.h"
#include "PacketReader/IP/IP_Packet.h"
#include "PacketReader/IP/UDP/UDP_Packet.h"
#include "PacketReader/IP/UDP/DHCP/DHCP_Packet.h"

NetAdapter* nif;
std::thread rx_thread;

volatile bool RxRunning = false;
//rx thread
void NetRxThread()
{
	NetPacket tmp;
	while (RxRunning)
	{
		while (rx_fifo_can_rx() && nif->recv(&tmp))
		{
			rx_process(&tmp);
		}
		std::this_thread::yield();
	}
}

void tx_put(NetPacket* pkt)
{
	if (nif != nullptr)
		nif->send(pkt);
	//pkt must be copied if its not processed by here, since it can be allocated on the callers stack
}

void InitNet(NetAdapter* ad)
{
	nif = ad;
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

void TermNet()
{
	if (RxRunning)
	{
		RxRunning = false;
		nif->close();
		Console.WriteLn("Waiting for RX-net thread to terminate..");
		rx_thread.join();
		Console.WriteLn("Done");

		delete nif;
		nif = nullptr;
	}
}

NetAdapter::NetAdapter()
{
	//Ensure eeprom matches our default 
	SetMACAddress(nullptr);
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

using namespace PacketReader;
using namespace PacketReader::ARP;
using namespace PacketReader::IP;
using namespace PacketReader::IP::UDP;
using namespace PacketReader::IP::UDP::DHCP;

void NetAdapter::InspectSentPacket(NetPacket* pkt)
{
	EthernetFrame frame(pkt);
	if (frame.protocol == (u16)EtherType::IPv4)
	{
		PayloadPtr* payload = static_cast<PayloadPtr*>(frame.GetPayload());
		IP_Packet* ippkt = new IP_Packet(payload->data, payload->GetLength());

		if (ippkt->protocol == (u16)IP_Type::UDP)
		{
			//Pass to DHCP patcher
			IP_Payload* newPayload = dhcpPatcher.InspectSent(ippkt->GetPayload());

			if (newPayload == nullptr)
			{
				delete ippkt;
				return;
			}

			//Assign back
			ippkt->SetPayload(newPayload, true);
			frame.SetPayload(ippkt, false);
			//Write back Packet
			frame.WritePacket(pkt);
			//Check readback
			Console.Error("DHCP Patch Sent Done");
		}
		delete ippkt;
	}
	else if (frame.protocol == (u16)EtherType::ARP)
	{
		Console.Error("ARP Sent;");
		PayloadPtr* payload = static_cast<PayloadPtr*>(frame.GetPayload());
		ARP_Packet arp(payload->data, payload->GetLength());
		Console.Error("");
	}
}


//void InspectRecvPacketTest(NetPacket* pkt)
//{
//	EthernetFrame frame(pkt);
//	if (frame.protocol == (u16)EtherType::IPv4)
//	{
//		PayloadPtr* payload = static_cast<PayloadPtr*>(frame.GetPayload());
//		IP_Packet* ippkt = new IP_Packet(payload->data, payload->GetLength());
//
//		if (ippkt->protocol == (u16)IP_Type::UDP)
//		{
//			IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(ippkt->GetPayload());
//			UDP_Packet* udppkt = new UDP_Packet(ipPayload->data, ipPayload->GetLength());
//
//			if (udppkt->destinationPort == 68 && udppkt->sourcePort == 67)
//			{
//				PayloadPtr* payload = static_cast<PayloadPtr*>(udppkt->GetPayload());
//				DHCP_Packet* dhcppkt = new DHCP_Packet(payload->data, payload->GetLength());
//				delete dhcppkt;
//			}
//			delete udppkt;
//		}
//		delete ippkt;
//	}
//}

void NetAdapter::InspectRecvPacket(NetPacket* pkt)
{
	EthernetFrame frame(pkt);
	if (frame.protocol == (u16)EtherType::IPv4)
	{
		PayloadPtr* payload = static_cast<PayloadPtr*>(frame.GetPayload());
		IP_Packet* ippkt = new IP_Packet(payload->data, payload->GetLength());

		if (ippkt->protocol == (u16)IP_Type::UDP)
		{
			//Pass to DHCP patcher
			IP_Payload* newPayload = dhcpPatcher.InspectRecv(ippkt->GetPayload());

			if (newPayload == nullptr)
			{
				delete ippkt;
				return;
			}

			//Assign back
			ippkt->SetPayload(newPayload, true);
			frame.SetPayload(ippkt, false);
			//Write back Packet
			frame.WritePacket(pkt);
			//Check readback
			Console.Error("DHCP Patch Recv Done");
		}
		delete ippkt;
	}
	else if (frame.protocol == (u16)EtherType::ARP)
	{
		Console.Error("ARP Recv;");
		PayloadPtr* payload = static_cast<PayloadPtr*>(frame.GetPayload());
		ARP_Packet arp(payload->data, payload->GetLength());
	}
}
