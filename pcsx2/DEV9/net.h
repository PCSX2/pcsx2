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

#include "Config.h"

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

struct AdapterEntry
{
	Pcsx2Config::DEV9Options::NetApi type;
	//UTF8
	std::string name;
	std::string guid;
};

enum struct AdapterOptions : int
{
	None = 0,
	DHCP_ForcedOn = 1 << 0,
	DHCP_OverrideIP = 1 << 1,
	DHCP_OverideSubnet = 1 << 2,
	DHCP_OverideGateway = 1 << 3,
};

constexpr enum AdapterOptions operator|(const enum AdapterOptions selfValue, const enum AdapterOptions inValue)
{
	return (enum AdapterOptions)(int(selfValue) | int(inValue));
}
constexpr enum AdapterOptions operator&(const enum AdapterOptions selfValue, const enum AdapterOptions inValue)
{
	return (enum AdapterOptions)(int(selfValue) & int(inValue));
}

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

	bool dhcpOn = false;

protected:
	InternalServers::DHCP_Server dhcpServer = InternalServers::DHCP_Server([&] { InternalSignalReceived(); });
	InternalServers::DNS_Logger dnsLogger;
	InternalServers::DNS_Server dnsServer = InternalServers::DNS_Server([&] { InternalSignalReceived(); });

public:
	NetAdapter();
	virtual bool blocks() = 0;
	virtual bool isInitialised() = 0;
	virtual bool recv(NetPacket* pkt); //gets a packet
	virtual bool send(NetPacket* pkt); //sends the packet and deletes it when done
	virtual void reset(){};
	virtual void reloadSettings() = 0;
	virtual void close(){};
	virtual ~NetAdapter();

protected:
	void SetMACAddress(u8* mac);
	bool VerifyPkt(NetPacket* pkt, int read_size);

	void InspectRecv(NetPacket* pkt);
	void InspectSend(NetPacket* pkt);

#ifdef _WIN32
	void InitInternalServer(PIP_ADAPTER_ADDRESSES adapter, bool dhcpForceEnable = false, PacketReader::IP::IP_Address ipOverride = {0}, PacketReader::IP::IP_Address subnetOverride = {0}, PacketReader::IP::IP_Address gatewayOveride = {0});
	void ReloadInternalServer(PIP_ADAPTER_ADDRESSES adapter, bool dhcpForceEnable = false, PacketReader::IP::IP_Address ipOverride = {0}, PacketReader::IP::IP_Address subnetOverride = {0}, PacketReader::IP::IP_Address gatewayOveride = {0});
#elif defined(__POSIX__)
	void InitInternalServer(ifaddrs* adapter, bool dhcpForceEnable = false, PacketReader::IP::IP_Address ipOverride = {0}, PacketReader::IP::IP_Address subnetOverride = {0}, PacketReader::IP::IP_Address gatewayOveride = {0});
	void ReloadInternalServer(ifaddrs* adapter, bool dhcpForceEnable = false, PacketReader::IP::IP_Address ipOverride = {0}, PacketReader::IP::IP_Address subnetOverride = {0}, PacketReader::IP::IP_Address gatewayOveride = {0});
#endif

private:
	bool InternalServerRecv(NetPacket* pkt);
	bool InternalServerSend(NetPacket* pkt);

	void InternalSignalReceived();
	void InternalServerThread();
};

void tx_put(NetPacket* ptr);
void ad_reset();

void InitNet();
void ReconfigureLiveNet(const Pcsx2Config& old_config);
void TermNet();
