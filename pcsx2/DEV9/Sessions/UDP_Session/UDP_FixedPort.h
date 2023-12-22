// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include <atomic>
#include <mutex>
#ifdef _WIN32
#include <winsock2.h>
#elif defined(__POSIX__)
#define INVALID_SOCKET -1
#include <sys/socket.h>
#endif

#include "DEV9/Sessions/BaseSession.h"
#include "UDP_BaseSession.h"
#include "UDP_Session.h"

namespace Sessions
{
	class UDP_FixedPort : public BaseSession
	{
	private:
		std::atomic<bool> open{true};

#ifdef _WIN32
		SOCKET client = INVALID_SOCKET;
#elif defined(__POSIX__)
		int client = INVALID_SOCKET;
#endif

	public:
		const u16 port = 0;

	private:
		std::mutex connectionSentry;
		std::vector<UDP_BaseSession*> connections;

	public:
		UDP_FixedPort(ConnectionKey parKey, PacketReader::IP::IP_Address parAdapterIP, u16 parPort);

		virtual PacketReader::IP::IP_Payload* Recv();
		virtual bool Send(PacketReader::IP::IP_Payload* payload);
		virtual void Reset();

		UDP_Session* NewClientSession(ConnectionKey parNewKey, bool parIsBrodcast, bool parIsMulticast);

		virtual ~UDP_FixedPort();

	private:
		void HandleChildConnectionClosed(BaseSession* sender);
	};
} // namespace Sessions
