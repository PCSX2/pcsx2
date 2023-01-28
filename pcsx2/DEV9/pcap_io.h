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
