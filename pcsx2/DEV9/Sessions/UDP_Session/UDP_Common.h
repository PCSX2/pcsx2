// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#include <tuple>
#ifdef _WIN32
#include <winsock2.h>
#elif defined(__POSIX__)
#include <sys/socket.h>
#endif

#include "common/Pcsx2Defs.h"
#include "DEV9/Sessions/BaseSession.h"

namespace Sessions::UDP_Common
{
	// Binds the socket when provided with an IP
#ifdef _WIN32
	SOCKET CreateSocket(PacketReader::IP::IP_Address adapterIP, std::optional<u16> port);
#elif defined(__POSIX__)
	int CreateSocket(PacketReader::IP::IP_Address adapterIP, std::optional<u16> port);
#endif

	// Receives from the client and packages the data into ReceivedPayload
	// port is the local port to be written to the UDP header in ReceivedPayload
#ifdef _WIN32
	std::tuple<std::optional<ReceivedPayload>, bool> RecvFrom(SOCKET client, u16 port);
#elif defined(__POSIX__)
	std::tuple<std::optional<ReceivedPayload>, bool> RecvFrom(int client, u16 port);
#endif
} // namespace Sessions::UDP_Common
