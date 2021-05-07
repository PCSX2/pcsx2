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

#pragma once
#include <stdlib.h>
#include <string>

#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#elif defined(__POSIX__)
#include <sys/types.h>
#include <ifaddrs.h>
#endif

#include "PacketReader/IP/IP_Address.h"
#include "InternalServers/DHCP_Server.h"
#include "InternalServers/DNS_Logger.h"
#include "InternalServers/DNS_Server.h"

struct ConfigDEV9;

// first three recognized by Xlink as Sony PS2
const u8 defaultMAC[6] = {0x00, 0x04, 0x1F, 0x82, 0x30, 0x31};

struct NetPacket
{
	NetPacket() { size = 0; }
	NetPacket(void* ptr, int sz)
	{
		size = sz;
		memcpy(buffer, ptr, sz);
	}

	int size;
	char buffer[2048 - sizeof(int)]; //1536 is realy needed, just pad up to 2048 bytes :)
};
/*
extern mtfifo<NetPacket*> rx_fifo;
extern mtfifo<NetPacket*> tx_fifo;
*/

enum struct NetApi : int
{
	Unset = 0,
	PCAP_Bridged = 1,
	PCAP_Switched = 2,
	TAP = 3,
};

struct AdapterEntry
{
	NetApi type;
#ifdef _WIN32
	std::wstring name;
	std::wstring guid;
#else
	std::string name;
	std::string guid;
#endif
};

class NetAdapter
{
public:
	static const PacketReader::IP::IP_Address internalIP;

protected:
	u8 ps2MAC[6];
	static const u8 broadcastMAC[6];
	static const u8 internalMAC[6];

private:
	//Only set if packet sent to the internal IP address
	PacketReader::IP::IP_Address ps2IP{0};
	std::thread internalRxThread;
	std::atomic<bool> internalRxThreadRunning{false};

	std::mutex internalRxMutex;
	std::condition_variable internalRxCV;
	bool internalRxHasData = false;

	InternalServers::DHCP_Server dhcpServer = InternalServers::DHCP_Server([&] { InternalSignalReceived(); });
	InternalServers::DNS_Logger dnsLogger;
	InternalServers::DNS_Server dnsServer = InternalServers::DNS_Server([&] { InternalSignalReceived(); });

public:
	NetAdapter();
	virtual bool blocks() = 0;
	virtual bool isInitialised() = 0;
	virtual bool recv(NetPacket* pkt); //gets a packet
	virtual bool send(NetPacket* pkt); //sends the packet and deletes it when done
	virtual void reloadSettings() = 0;
	virtual void close(){};
	virtual ~NetAdapter();

protected:
	void SetMACAddress(u8* mac);
	bool VerifyPkt(NetPacket* pkt, int read_size);

	void InspectRecv(NetPacket* pkt);
	void InspectSend(NetPacket* pkt);

#ifdef _WIN32
	void InitInternalServer(PIP_ADAPTER_ADDRESSES adapter);
	void ReloadInternalServer(PIP_ADAPTER_ADDRESSES adapter);
#elif defined(__POSIX__)
	void InitInternalServer(ifaddrs* adapter);
	void ReloadInternalServer(ifaddrs* adapter);
#endif

private:
	bool InternalServerRecv(NetPacket* pkt);
	bool InternalServerSend(NetPacket* pkt);

	void InternalSignalReceived();
	void InternalServerThread();
};

void tx_put(NetPacket* ptr);
void InitNet();
void ReconfigureLiveNet(ConfigDEV9* oldConfig);
void TermNet();

const wxChar* NetApiToWxString(NetApi api);
