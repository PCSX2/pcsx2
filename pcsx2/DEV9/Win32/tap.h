// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include <vector>
#include <string>
#include "..\net.h"
using namespace std;

class TAPAdapter : public NetAdapter
{
	HANDLE htap;
	OVERLAPPED read, write;
	HANDLE cancel;
	bool isActive = false;

public:
	TAPAdapter();
	virtual bool blocks();
	virtual bool isInitialised();
	//gets a packet.rv :true success
	virtual bool recv(NetPacket* pkt);
	//sends the packet and deletes it when done (if successful).rv :true success
	virtual bool send(NetPacket* pkt);
	virtual void reloadSettings();
	virtual void close();
	virtual ~TAPAdapter();
	static std::vector<AdapterEntry> GetAdapters();
	static AdapterOptions GetAdapterOptions();
};
