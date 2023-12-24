// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include <vector>

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
