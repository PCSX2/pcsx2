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
#ifdef _WIN32
#include "Win32/tap.h"
#endif
#include "pcap_io.h"

#ifdef _WIN32
#include "Win32\tap.h"
#endif
#include "pcap_io.h"

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

NetAdapter* GetNetAdapter()
{
	NetAdapter* na = nullptr;

	switch (config.EthApi)
	{
#ifdef _WIN32
		case NetApi::TAP:
			na = static_cast<NetAdapter*>(new TAPAdapter());
			break;
#endif
		case NetApi::PCAP_Bridged:
		case NetApi::PCAP_Switched:
			na = static_cast<NetAdapter*>(new PCAPAdapter());
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
		Console.Error("Failed to GetNetAdapter()");
		config.ethEnable = false;
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

const char* NetApiToString(NetApi api)
{
	switch (api)
	{
		case NetApi::PCAP_Bridged:
			return "PCAP (Bridged)";
		case NetApi::PCAP_Switched:
			return "PCAP (Switched)";
		case NetApi::TAP:
			return "TAP";
		default:
			return "UNK";
	}
}

const wchar_t* NetApiToWstring(NetApi api)
{
	switch (api)
	{
		case NetApi::PCAP_Bridged:
			return L"PCAP (Bridged)";
		case NetApi::PCAP_Switched:
			return L"PCAP (Switched)";
		case NetApi::TAP:
			return L"TAP";
		default:
			return L"UNK";
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
