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
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#elif defined(__POSIX__)
#include <sys/types.h>
#include <ifaddrs.h>
#endif

#include "net.h"

#include "PacketReader/IP/IP_Packet.h"
#include "PacketReader/EthernetFrame.h"
#include "Sessions/BaseSession.h"
#include "SimpleQueue.h"
#include "ThreadSafeMap.h"

class SocketAdapter : public NetAdapter
{
	//SimpleQueue for ARP packages
	SimpleQueue<PacketReader::EthernetFrame*> vRecBuffer;

#ifdef _WIN32
	bool wsa_init = false;
#endif
	bool initialized = false;
	PacketReader::IP::IP_Address adapterIP;

	//Sentrys replaced by the requirment for each session class to have thread safe destructor

	ThreadSafeMap<Sessions::ConnectionKey, Sessions::BaseSession*> connections;
	ThreadSafeMap<u16, Sessions::BaseSession*> fixedUDPPorts;

public:
	SocketAdapter();
	virtual bool blocks();
	virtual bool isInitialised();
	//gets a packet.rv :true success
	virtual bool recv(NetPacket* pkt);
	//sends the packet and deletes it when done (if successful).rv :true success
	virtual bool send(NetPacket* pkt);
	virtual void reset();
	virtual void reloadSettings();
	virtual void close();
	virtual ~SocketAdapter();
	static std::vector<AdapterEntry> GetAdapters();
	static AdapterOptions GetAdapterOptions();

private:
#ifdef _WIN32
	bool GetWin32SelectedAdapter(const std::string& name, PIP_ADAPTER_ADDRESSES adapter, std::unique_ptr<IP_ADAPTER_ADDRESSES[]>* buffer);
	bool GetWin32AutoAdapter(PIP_ADAPTER_ADDRESSES adapter, std::unique_ptr<IP_ADAPTER_ADDRESSES[]>* buffer);
#elif defined(__POSIX__)
	bool GetIfSelectedAdapter(const std::string& name, ifaddrs* adapter, ifaddrs** buffer);
	bool GetIfAutoAdapter(ifaddrs* adapter, ifaddrs** buffer);
#endif // _WIN32

	bool SendIP(PacketReader::IP::IP_Packet* ipPkt);
	bool SendICMP(Sessions::ConnectionKey Key, PacketReader::IP::IP_Packet* ipPkt);
	bool SendIGMP(Sessions::ConnectionKey Key, PacketReader::IP::IP_Packet* ipPkt);
	bool SendTCP(Sessions::ConnectionKey Key, PacketReader::IP::IP_Packet* ipPkt);
	bool SendUDP(Sessions::ConnectionKey Key, PacketReader::IP::IP_Packet* ipPkt);

	int SendFromConnection(Sessions::ConnectionKey Key, PacketReader::IP::IP_Packet* ipPkt);

	//Event must only be raised once per connection
	void HandleConnectionClosed(Sessions::BaseSession* sender);
	void HandleFixedPortClosed(Sessions::BaseSession* sender);
};
