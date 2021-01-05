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
#include <string.h> //uh isnt memcpy @ stdlib ?

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

class NetAdapter
{
protected:
	u8 ps2MAC[6];

public:
	NetAdapter();
	virtual bool blocks() = 0;
	virtual bool isInitialised() = 0;
	virtual bool recv(NetPacket* pkt) = 0; //gets a packet
	virtual bool send(NetPacket* pkt) = 0; //sends the packet and deletes it when done
	virtual void close() {}
	virtual ~NetAdapter() {}

protected:
	void SetMACAddress(u8* mac);
};

void tx_put(NetPacket* ptr);
void InitNet(NetAdapter* adapter);
void TermNet();
