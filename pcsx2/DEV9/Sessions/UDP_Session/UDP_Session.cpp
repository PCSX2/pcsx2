// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/Assertions.h"
#include "common/Console.h"

#ifdef __POSIX__
#define SOCKET_ERROR -1
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

#include "UDP_Session.h"
#include "DEV9/PacketReader/IP/UDP/UDP_Packet.h"

using namespace PacketReader;
using namespace PacketReader::IP;
using namespace PacketReader::IP::UDP;

using namespace std::chrono_literals;

namespace Sessions
{
	const std::chrono::duration<std::chrono::steady_clock::rep, std::chrono::steady_clock::period>
		UDP_Session::MAX_IDLE = 120s; // See RFC 4787 section 4.3

	UDP_Session::UDP_Session(ConnectionKey parKey, IP_Address parAdapterIP)
		: UDP_BaseSession(parKey, parAdapterIP)
		, isBroadcast(false)
		, isMulticast(false)
		, isFixedPort(false)
		, deathClockStart(std::chrono::steady_clock::now())
	{
	}

#ifdef _WIN32
	UDP_Session::UDP_Session(ConnectionKey parKey, IP_Address parAdapterIP, bool parIsBroadcast, bool parIsMulticast, SOCKET parClient)
#elif defined(__POSIX__)
	UDP_Session::UDP_Session(ConnectionKey parKey, IP_Address parAdapterIP, bool parIsBroadcast, bool parIsMulticast, int parClient)
#endif
		: UDP_BaseSession(parKey, parAdapterIP)
		, open(true)
		, client(parClient)
		, srcPort(parKey.ps2Port)
		, destPort(parKey.srvPort)
		, isBroadcast(parIsBroadcast)
		, isMulticast(parIsMulticast)
		, isFixedPort(true)
		, deathClockStart(std::chrono::steady_clock::now())
	{
	}

	IP_Payload* UDP_Session::Recv()
	{
		if (!open.load())
			return nullptr;

		if (isFixedPort)
		{
			if (std::chrono::steady_clock::now() - deathClockStart.load() > MAX_IDLE)
			{
				Console.WriteLn("DEV9: UDP: Fixed port max idle reached");
				open.store(false);
				RaiseEventConnectionClosed();
			}
			return nullptr;
		}

		int ret;
		fd_set sReady;
		fd_set sExcept;

		timeval nowait{};
		FD_ZERO(&sReady);
		FD_ZERO(&sExcept);
		FD_SET(client, &sReady);
		FD_SET(client, &sExcept);
		ret = select(client + 1, &sReady, nullptr, &sExcept, &nowait);

		bool hasData;
		if (ret == SOCKET_ERROR)
		{
			hasData = false;
			Console.Error("DEV9: UDP: Select failed. Error code: %d",
#ifdef _WIN32
				WSAGetLastError());
#elif defined(__POSIX__)
				errno);
#endif
		}
		else if (FD_ISSET(client, &sExcept))
		{
			hasData = false;

			int error = 0;
#ifdef _WIN32
			int len = sizeof(error);
			if (getsockopt(client, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len) < 0)
				Console.Error("DEV9: UDP: Unknown UDP connection error (getsockopt error: %d)", WSAGetLastError());
#elif defined(__POSIX__)
			socklen_t len = sizeof(error);
			if (getsockopt(client, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &len) < 0)
				Console.Error("DEV9: UDP: Unknown UDP connection error (getsockopt error: %d)", errno);
#endif
			else
				Console.Error("DEV9: UDP: Recv error: %d", error);
		}
		else
			hasData = FD_ISSET(client, &sReady);

		if (hasData)
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
				Console.Error("DEV9: UDP: Recv error: %d",
#ifdef _WIN32
					WSAGetLastError());
#elif defined(__POSIX__)
					errno);
#endif
				RaiseEventConnectionClosed();
				return nullptr;
			}

			recived = new PayloadData(ret);
			memcpy(recived->data.get(), buffer.get(), ret);

			UDP_Packet* iRet = new UDP_Packet(recived);
			iRet->destinationPort = srcPort;
			iRet->sourcePort = destPort;

			deathClockStart.store(std::chrono::steady_clock::now());

			return iRet;
		}

		if (std::chrono::steady_clock::now() - deathClockStart.load() > MAX_IDLE)
		{
			Console.WriteLn("DEV9: UDP: Max idle reached");
			RaiseEventConnectionClosed();
		}

		return nullptr;
	}

	bool UDP_Session::WillRecive(IP_Address parDestIP)
	{
		if (!open.load())
			return false;

		if (isBroadcast || isMulticast || (parDestIP == destIP))
		{
			deathClockStart.store(std::chrono::steady_clock::now());
			return true;
		}
		return false;
	}

	bool UDP_Session::Send(PacketReader::IP::IP_Payload* payload)
	{
		deathClockStart.store(std::chrono::steady_clock::now());

		IP_PayloadPtr* ipPayload = static_cast<IP_PayloadPtr*>(payload);
		UDP_Packet udp(ipPayload->data, ipPayload->GetLength());

		if (destPort != 0)
		{
			// Already created client!?
			if (!(udp.destinationPort == destPort && udp.sourcePort == srcPort))
			{
				Console.Error("DEV9: UDP: Packet invalid for current session (duplicate key?)");
				return false;
			}
		}
		else
		{
			// Create client
			destPort = udp.destinationPort;
			srcPort = udp.sourcePort;

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
				RaiseEventConnectionClosed();
				return false;
			}

			const int reuseAddress = true; // BOOL on Windows
			ret = setsockopt(client, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuseAddress), sizeof(reuseAddress));

			if (ret == SOCKET_ERROR)
				Console.Error("DEV9: UDP: Failed to set SO_REUSEADDR. Error: %d",
#ifdef _WIN32
					WSAGetLastError());
#elif defined(__POSIX__)
					errno);
#endif

			if (adapterIP.integer != 0)
			{
				sockaddr_in endpoint{};
				endpoint.sin_family = AF_INET;
				endpoint.sin_addr = std::bit_cast<in_addr>(adapterIP);

				ret = bind(client, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint));

				if (ret == SOCKET_ERROR)
					Console.Error("DEV9: UDP: Failed to bind socket. Error: %d",
#ifdef _WIN32
						WSAGetLastError());
#elif defined(__POSIX__)
						errno);
#endif
			}

			sockaddr_in endpoint{};
			endpoint.sin_family = AF_INET;
			endpoint.sin_addr = std::bit_cast<in_addr>(destIP);
			endpoint.sin_port = htons(destPort);

			ret = connect(client, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint));

			if (ret == SOCKET_ERROR)
			{
				Console.Error("DEV9: UDP: Failed to connect socket. Error: %d",
#ifdef _WIN32
					WSAGetLastError());
#elif defined(__POSIX__)
					errno);
#endif
				RaiseEventConnectionClosed();
				return false;
			}

			if (srcPort != 0)
				open.store(true);
		}

		PayloadPtr* udpPayload = static_cast<PayloadPtr*>(udp.GetPayload());

		// Send Packet
		int ret = SOCKET_ERROR;
		if (isBroadcast)
		{
			sockaddr_in endpoint{};
			endpoint.sin_family = AF_INET;
			endpoint.sin_addr.s_addr = INADDR_BROADCAST;
			endpoint.sin_port = htons(destPort);

			ret = sendto(client, reinterpret_cast<const char*>(udpPayload->data), udpPayload->GetLength(), 0, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint));
		}
		else if (isFixedPort)
		{
			sockaddr_in endpoint{};
			endpoint.sin_family = AF_INET;
			endpoint.sin_addr = std::bit_cast<in_addr>(destIP);
			endpoint.sin_port = htons(destPort);

			ret = sendto(client, reinterpret_cast<const char*>(udpPayload->data), udpPayload->GetLength(), 0, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint));
		}
		else
			ret = send(client, reinterpret_cast<const char*>(udpPayload->data), udpPayload->GetLength(), 0);

		if (ret == SOCKET_ERROR)
		{
#ifdef _WIN32
			ret = WSAGetLastError();
#elif defined(__POSIX__)
			ret = errno;
#endif
			Console.Error("DEV9: UDP: Send error %d", ret);

			/*
			 * We can receive an ICMP Port Unreacable error, which can get raised in send (and maybe sendto?)
			 * On Windows this is an WSAECONNRESET error, although I've not been able to reproduce in testing
			 * On Linux this is an ECONNREFUSED error (Testing needed to confirm full behaviour)
			 * We ignore the error and resend to allow packet capture (i.e. wireshark) for server resurrection projects
			 */
#ifdef _WIN32
			if (ret == WSAECONNRESET)
#elif defined(__POSIX__)
			if (ret == ECONNREFUSED)
#endif
			{
				pxAssert(isBroadcast == false && isMulticast == false);
				if (isFixedPort)
				{
					sockaddr_in endpoint{};
					endpoint.sin_family = AF_INET;
					endpoint.sin_addr = std::bit_cast<in_addr>(destIP);
					endpoint.sin_port = htons(destPort);

					ret = sendto(client, reinterpret_cast<const char*>(udpPayload->data), udpPayload->GetLength(), 0, reinterpret_cast<const sockaddr*>(&endpoint), sizeof(endpoint));
				}
				else
					ret = send(client, reinterpret_cast<const char*>(udpPayload->data), udpPayload->GetLength(), 0);

				if (ret == SOCKET_ERROR)
				{
					Console.Error("DEV9: UDP: Send error (second attempt) %d",
#ifdef _WIN32
						WSAGetLastError());
#elif defined(__POSIX__)
						errno);
#endif
					return false;
				}
			}
			else
			{
				RaiseEventConnectionClosed();
				return false;
			}
		}
		pxAssert(ret == udpPayload->GetLength());

		if (srcPort == 0)
			RaiseEventConnectionClosed();

		return true;
	}

	void UDP_Session::Reset()
	{
		RaiseEventConnectionClosed();
	}

	UDP_Session::~UDP_Session()
	{
		open.store(false);
		if (!isFixedPort && client != INVALID_SOCKET)
		{
#ifdef _WIN32
			closesocket(client);
#elif defined(__POSIX__)
			::close(client);
#endif
			client = INVALID_SOCKET;
		}
	}
} // namespace Sessions
