/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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
#include "DebugInterface.h"
#include "common/Threading.h"
#include <gsl/span>
#ifdef _WIN32
#include <WinSock2.h>
#include <windows.h>
#endif

enum
{
	DEBUG_CONNECTION_INVALID,
	DEBUG_CONNECTION_ESTABLISHED
};

class DebugServerInterface
{
public:
	virtual void setDebugInterface(DebugInterface* debugInterface);
	virtual std::size_t processPacket(const char* inData, std::size_t inSize, void* outData, std::size_t& outSize) = 0;

protected:
	DebugInterface* m_debugInterface = nullptr;
};

class DebugNetworkServer
{
public:
	DebugNetworkServer();
	~DebugNetworkServer();

public:
	bool init(std::unique_ptr<DebugServerInterface>& debugServerInterface, u16 port, const char* address);
	void shutdown();

public:
	void signal(int signal);

public:
	int getNetworkStatus() const;
	int getPort() const;
	
protected:
	bool reviveConnection();
	void serverLoop();

	bool receiveAndSendPacket();

private:
#define MAX_DEBUG_PACKET_SIZE 10240
#define MAX_SIGNALS 16

#ifdef _WIN32
	SOCKET m_sock = INVALID_SOCKET;
	SOCKET m_msgsock = INVALID_SOCKET;
#else
	int m_sock = 0;
	int m_msgsock = 0;
#endif
	int m_port = -1;
	u32 m_listenAddress = 0;
	u32 m_signalCount = 0;
	int m_signals[MAX_SIGNALS] = {};

	std::mutex m_debugMutex;
	std::atomic_bool m_end = false;
	std::thread m_thread;

	std::vector<u8> m_recv_buffer;
	std::vector<u8> m_send_buffer;

	std::unique_ptr<DebugServerInterface> m_debugServerInterface;
};

extern DebugNetworkServer debugNetworkServer;