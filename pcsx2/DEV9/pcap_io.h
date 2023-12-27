// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "pcap.h"
#include "net.h"
#include "PacketReader/MAC_Address.h"

#ifdef _WIN32
bool load_pcap();
void unload_pcap();
#endif

class PCAPAdapter : public NetAdapter
{
private:
	pcap_t* hpcap = nullptr;

	bool switched;
	bool blocking;

	PacketReader::IP::IP_Address ps2IP{};
	PacketReader::MAC_Address hostMAC;

public:
	PCAPAdapter();
	virtual bool blocks();
	virtual bool isInitialised();
	//gets a packet.rv :true success
	virtual bool recv(NetPacket* pkt);
	//sends the packet and deletes it when done (if successful).rv :true success
	virtual bool send(NetPacket* pkt);
	virtual void reloadSettings();
	virtual ~PCAPAdapter();
	static std::vector<AdapterEntry> GetAdapters();
	static AdapterOptions GetAdapterOptions();

private:
	bool InitPCAP(const std::string& adapter, bool promiscuous);
	void InitPCAPDumper();
	bool SetMACSwitchedFilter(PacketReader::MAC_Address mac);

	void SetMACBridgedRecv(NetPacket* pkt);
	void SetMACBridgedSend(NetPacket* pkt);
};
