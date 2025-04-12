// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <bit>

#include "common/Console.h"

#ifdef __POSIX__
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <netinet/in.h>
#endif

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <winsock2.h>
#else
#include <unistd.h>
#endif

#include "UDP_Common.h"
#include "DEV9/PacketReader/IP/UDP/UDP_Packet.h"

using namespace PacketReader;
using namespace PacketReader::IP;
using namespace PacketReader::IP::UDP;

namespace Sessions::UDP_Common
{
#ifdef _WIN32
	SOCKET CreateSocket(IP_Address adapterIP, std::optional<u16> port)
	{
		SOCKET client = INVALID_SOCKET;
#elif defined(__POSIX__)
	int CreateSocket(IP_Address adapterIP, std::optional<u16> port)
	{
		int client = INVALID_SOCKET;
#endif

		int ret;
		client = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
		if (client == INVALID_SOCKET)
		{
			Console.Error("DEV9: UDP: Failed to open socket. Error: %d",
#ifdef _WIN32
				WSAGetLastError());
#elif defined(__POSIX__)
				errno);
#endif
			return INVALID_SOCKET;
		}

		constexpr int reuseAddress = true; // BOOL on Windows
		ret = setsockopt(client, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuseAddress), sizeof(reuseAddress));

		if (ret == SOCKET_ERROR)
			Console.Error("DEV9: UDP: Failed to set SO_REUSEADDR. Error: %d",
#ifdef _WIN32
				WSAGetLastError());
#elif defined(__POSIX__)
				errno);
#endif

		if (port.has_value() || adapterIP != IP_Address{{{0, 0, 0, 0}}})
		{
			sockaddr_in endpoint{};
			endpoint.sin_family = AF_INET;
			endpoint.sin_addr = std::bit_cast<in_addr>(adapterIP);
			if (port.has_value())
				endpoint.sin_port = htons(port.value());

			ret = bind(client, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint));

			if (ret == SOCKET_ERROR)
				Console.Error("DEV9: UDP: Failed to bind socket. Error: %d",
#ifdef _WIN32
					WSAGetLastError());
#elif defined(__POSIX__)
					errno);
#endif
		}
		return client;
	}

#ifdef _WIN32
	std::tuple<std::optional<ReceivedPayload>, bool> RecvFrom(SOCKET client, u16 port)
#elif defined(__POSIX__)
	std::tuple<std::optional<ReceivedPayload>, bool> RecvFrom(int client, u16 port)
#endif
	{
		int ret;
		fd_set sReady;
		fd_set sExcept;

		// not const Linux
		timeval nowait{};
		FD_ZERO(&sReady);
		FD_ZERO(&sExcept);
		FD_SET(client, &sReady);
		FD_SET(client, &sExcept);
		ret = select(client + 1, &sReady, nullptr, &sExcept, &nowait);

		if (ret == SOCKET_ERROR)
		{
			Console.Error("DEV9: UDP: select failed. Error code: %d",
#ifdef _WIN32
				WSAGetLastError());
#elif defined(__POSIX__)
				errno);
#endif
			return {std::nullopt, true};
		}
		else if (FD_ISSET(client, &sExcept))
		{
#ifdef _WIN32
			int len = sizeof(ret);
			if (getsockopt(client, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&ret), &len) < 0)
			{
				Console.Error("DEV9: UDP: Unknown UDP connection error (getsockopt error: %d)", WSAGetLastError());
			}
#elif defined(__POSIX__)
			socklen_t len = sizeof(ret);
			if (getsockopt(client, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&ret), &len) < 0)
				Console.Error("DEV9: UDP: Unknown UDP connection error (getsockopt error: %d)", errno);
#endif
			else
				Console.Error("DEV9: UDP: Socket error: %d", ret);

			// All socket errors assumed fatal.
			return {std::nullopt, false};
		}
		else if (FD_ISSET(client, &sReady))
		{
			unsigned long available = 0;
			PayloadData* recived = nullptr;
			std::unique_ptr<u8[]> buffer;
			sockaddr_in endpoint{};

			// FIONREAD returns total size of all available messages
			// however, we only read one message at a time
#ifdef _WIN32
			ret = ioctlsocket(client, FIONREAD, &available);
#elif defined(__POSIX__)
			ret = ioctl(client, FIONREAD, &available);
#endif
			if (ret != SOCKET_ERROR)
			{
				buffer = std::make_unique<u8[]>(available);

#ifdef _WIN32
				int fromlen = sizeof(endpoint);
#elif defined(__POSIX__)
				socklen_t fromlen = sizeof(endpoint);
#endif
				ret = recvfrom(client, reinterpret_cast<char*>(buffer.get()), available, 0, reinterpret_cast<sockaddr*>(&endpoint), &fromlen);
			}

			if (ret == SOCKET_ERROR)
			{
#ifdef _WIN32
				ret = WSAGetLastError();
#elif defined(__POSIX__)
				ret = errno;
#endif
				Console.Error("DEV9: UDP: recvfrom error: %d", ret);

				/*
				 * We can receive an ICMP Port Unreacable error as a WSAECONNRESET/ECONNREFUSED error
				 * Ignore the error, recv will be retried next loop
				 */
				return {std::nullopt,
#ifdef _WIN32
					ret == WSAECONNRESET};
#elif defined(__POSIX__)
					ret == ECONNREFUSED};
#endif
			}

			recived = new PayloadData(ret);
			memcpy(recived->data.get(), buffer.get(), ret);

			std::unique_ptr<UDP_Packet> iRet = std::make_unique<UDP_Packet>(recived);
			iRet->destinationPort = port;
			iRet->sourcePort = ntohs(endpoint.sin_port);

			return {ReceivedPayload{std::bit_cast<IP_Address>(endpoint.sin_addr), std::move(iRet)}, true};
		}
		return {std::nullopt, true};
	}
} // namespace Sessions::UDP_Common
