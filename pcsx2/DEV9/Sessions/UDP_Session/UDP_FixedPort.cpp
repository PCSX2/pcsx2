// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <bit>

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

#include "UDP_Common.h"
#include "UDP_FixedPort.h"
#include "DEV9/PacketReader/IP/UDP/UDP_Packet.h"

using namespace PacketReader;
using namespace PacketReader::IP;
using namespace PacketReader::IP::UDP;

namespace Sessions
{
	/*
	 * The default UDP_Session backend don't bind to the src port the PS2 uses.
	 * Some games, however, sends the response to a set port, rather than the message source port 
	 * A set of heuristics are used to determine when we should bind the port, these are;
	 * Any broadcast & multicast packet, and any packet where the src and dst ports are close to each other
	 * UDP_FixedPort manages the lifetime of socket bound to a specific port, and shares that socket
	 * with any UDP_Sessions created from it.
	 * For a UDP_Session with a parent UDP_FixedPort, packets are sent from the UDP_Session, but received
	 * by the UDP_FixedPort, with the UDP_FixedPort asking each UDP_Session associated with it whether 
	 * it can accept the received packet, broadcast/multicast will accept eveything, while unicast sessions
	 * only accept packets from the address it sent to
	 */

	UDP_FixedPort::UDP_FixedPort(ConnectionKey parKey, IP_Address parAdapterIP, u16 parPort)
		: BaseSession(parKey, parAdapterIP)
		, port{parPort}
	{
	}

	void UDP_FixedPort::Init()
	{
		client = UDP_Common::CreateSocket(adapterIP, port);
		if (client == INVALID_SOCKET)
		{
			RaiseEventConnectionClosed();
			return;
		}

		constexpr int broadcastEnable = true; // BOOL on Windows
		const int ret = setsockopt(client, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcastEnable), sizeof(broadcastEnable));

		if (ret == SOCKET_ERROR)
			Console.Error("DEV9: UDP: Failed to set SO_BROADCAST. Error: %d",
#ifdef _WIN32
				WSAGetLastError());
#elif defined(__POSIX__)
				errno);
#endif

		open.store(true);
	}

	std::optional<ReceivedPayload> UDP_FixedPort::Recv()
	{
		if (!open.load())
			return std::nullopt;

		std::optional<ReceivedPayload> ret;
		bool success;
		std::tie(ret, success) = UDP_Common::RecvFrom(client, port);

		if (!success)
		{
			// See Reset() for why we copy the vector.
			std::vector<UDP_BaseSession*> connectionsCopy;
			{
				std::lock_guard numberlock(connectionSentry);
				open.store(false);
				connectionsCopy = connections;
			}

			if (connectionsCopy.size() == 0)
			{
				// Can close immediately.
				RaiseEventConnectionClosed();
				return std::nullopt;
			}
			else
			{
				// Need to wait for child connections to close.
				for (size_t i = 0; i < connectionsCopy.size(); i++)
					connectionsCopy[i]->ForceClose();

				return std::nullopt;
			}
		}
		else if (ret.has_value())
		{
			{
				std::lock_guard numberlock(connectionSentry);

				for (size_t i = 0; i < connections.size(); i++)
				{
					UDP_BaseSession* s = connections[i];
					if (s->WillRecive(ret.value().sourceIP))
						return ret;
				}
			}
			Console.Error("DEV9: UDP: Unexpected packet, dropping");
		}
		return std::nullopt;
	}

	bool UDP_FixedPort::Send(PacketReader::IP::IP_Payload* payload)
	{
		pxAssert(false);
		return false;
	}

	void UDP_FixedPort::Reset()
	{
		/*
		 * Reseting a session may cause that session to close itself,
		 * when that happens, the connections vector gets modified via our close handler.
		 * Duplicate the vector to avoid iterating over a modified collection,
		 * this also avoids the issue of recursive locking when our close handler takes a lock.
		 */
		std::vector<UDP_BaseSession*> connectionsCopy;
		{
			std::lock_guard numberlock(connectionSentry);
			connectionsCopy = connections;
		}

		for (size_t i = 0; i < connectionsCopy.size(); i++)
			connectionsCopy[i]->Reset();
	}

	UDP_Session* UDP_FixedPort::NewClientSession(ConnectionKey parNewKey, bool parIsBrodcast, bool parIsMulticast)
	{
		// Lock the whole function so we can't race between the open check and creating the session
		std::lock_guard numberlock(connectionSentry);
		if (!open.load())
			return nullptr;

		UDP_Session* s = new UDP_Session(parNewKey, adapterIP, parIsBrodcast, parIsMulticast, client);

		s->AddConnectionClosedHandler([&](BaseSession* session) { HandleChildConnectionClosed(session); });
		connections.push_back(s);
		return s;
	}

	void UDP_FixedPort::HandleChildConnectionClosed(BaseSession* sender)
	{
		std::lock_guard numberlock(connectionSentry);

		auto index = std::find(connections.begin(), connections.end(), sender);
		if (index != connections.end())
		{
			connections.erase(index);
			if (connections.size() == 0)
			{
				open.store(false);
				RaiseEventConnectionClosed();
			}
		}
	}

	UDP_FixedPort::~UDP_FixedPort()
	{
		DevCon.WriteLn("DEV9: Socket: UDPFixedPort %d had %d child connections", port, connections.size());

		open.store(false);
		if (client != INVALID_SOCKET)
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
