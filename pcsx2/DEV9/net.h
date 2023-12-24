// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include <stdlib.h>
#include <string>

#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <winsock2.h>
#include <iphlpapi.h>
#elif defined(__POSIX__)
#include <sys/types.h>
#include <ifaddrs.h>
#endif

#include "Config.h"

#include "PacketReader/MAC_Address.h"
#include "PacketReader/IP/IP_Address.h"
#include "InternalServers/DHCP_Server.h"
#include "InternalServers/DNS_Logger.h"
#include "InternalServers/DNS_Server.h"

struct ConfigDEV9;

// first three recognized by Xlink as Sony PS2
const PacketReader::MAC_Address defaultMAC = {{{0x00, 0x04, 0x1F, 0x82, 0x30, 0x31}}};

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
	PacketReader::MAC_Address ps2MAC;
	static const PacketReader::MAC_Address broadcastMAC;
	static const PacketReader::MAC_Address internalMAC;

private:
	//Only set if packet sent to the internal IP address
	PacketReader::IP::IP_Address ps2IP{};
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
	void SetMACAddress(PacketReader::MAC_Address* mac);
	bool VerifyPkt(NetPacket* pkt, int read_size);

	void InspectRecv(NetPacket* pkt);
	void InspectSend(NetPacket* pkt);

#ifdef _WIN32
	void InitInternalServer(PIP_ADAPTER_ADDRESSES adapter, bool dhcpForceEnable = false, PacketReader::IP::IP_Address ipOverride = {}, PacketReader::IP::IP_Address subnetOverride = {}, PacketReader::IP::IP_Address gatewayOveride = {});
	void ReloadInternalServer(PIP_ADAPTER_ADDRESSES adapter, bool dhcpForceEnable = false, PacketReader::IP::IP_Address ipOverride = {}, PacketReader::IP::IP_Address subnetOverride = {}, PacketReader::IP::IP_Address gatewayOveride = {});
#elif defined(__POSIX__)
	void InitInternalServer(ifaddrs* adapter, bool dhcpForceEnable = false, PacketReader::IP::IP_Address ipOverride = {}, PacketReader::IP::IP_Address subnetOverride = {}, PacketReader::IP::IP_Address gatewayOveride = {});
	void ReloadInternalServer(ifaddrs* adapter, bool dhcpForceEnable = false, PacketReader::IP::IP_Address ipOverride = {}, PacketReader::IP::IP_Address subnetOverride = {}, PacketReader::IP::IP_Address gatewayOveride = {});
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
