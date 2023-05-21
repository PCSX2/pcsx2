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
#include "PrecompiledHeader.h"
#include "DebugServer.h"
#include "common/Threading.h"

DebugNetworkServer debugNetworkServer;

#if _WIN32
#define would_block() (WSAGetLastError() == WSAEWOULDBLOCK)
#define read_portable(a, b, c) (recv(a, (char*)b, c, 0))
#define write_portable(a, b, c) (send(a, (const char*)b, c, 0))
#define close_portable(a) (closesocket(a))
#include <WinSock2.h>
#include <windows.h>
#else
#define would_block() (errno == EWOULDBLOCK)
#define read_portable(a, b, c) (read(a, b, c))
#define write_portable(a, b, c) (write(a, b, c))
#define close_portable(a) (close(a))
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

void 
DebugServerInterface::setDebugInterface(DebugInterface* debugInterface)
{
	m_debugInterface = debugInterface;
}

DebugNetworkServer::DebugNetworkServer()
{

}

DebugNetworkServer::~DebugNetworkServer()
{
}

bool 
DebugNetworkServer::init(
	std::unique_ptr<DebugServerInterface>& debugServerInterface,
	u16 port,
	const char* address
)
{
	if (debugServerInterface == nullptr)
	{
		Console.WriteLn(Color_Red, "DebugNetworkServer: debug server interface is null! Shutting down...");
		return false;
	}

	// transfer exclusive pointer
	m_debugServerInterface = std::move(debugServerInterface);

#ifdef _WIN32
	WSADATA wsa;
	struct sockaddr_in server;

	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		Console.WriteLn(Color_Red, "DebugNetworkServer: Cannot initialize winsock! Shutting down...");
		return false;
	}
	
	m_sock = socket(AF_INET, SOCK_STREAM, 0);
	if ((m_sock == INVALID_SOCKET) || port > 65536)
	{
		Console.WriteLn(Color_Red, "DebugNetworkServer: Cannot open socket! Shutting down...");
		shutdown();
		return false;
	}

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(address);
	server.sin_port = htons(port);
#else
	struct sockaddr_un server;
	m_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (m_sock < 0)
	{
		Console.WriteLn(Color_Red, "DebugNetworkServer: Cannot open socket! Shutting down...");
		shutdown();
		return false;
	}
	
	server.sun_family = AF_UNIX;
	if (bind(m_sock, (struct sockaddr*)&server, sizeof(struct sockaddr_un)))
	{
		Console.WriteLn(Color_Red, "DebugNetworkServer: Error while binding to socket! Shutting down...");
		shutdown();
		return false;
	}
#endif
	// Don't need more than 5 in this case, GDB or WinDbg can't handle so much packets anyway
	if (listen(m_sock, 2))
	{
		Console.WriteLn(Color_Red, "DebugNetworkServer: Cannot listen for connections! Shutting down...");
		shutdown();
		return false;
	}

	m_recv_buffer.resize(MAX_DEBUG_PACKET_SIZE);
	m_send_buffer.resize(MAX_DEBUG_PACKET_SIZE);

	m_thread = std::thread(&DebugNetworkServer::serverLoop, this);
	return true;
}

void 
DebugNetworkServer::shutdown()
{
	m_end.store(true, std::memory_order_release);

#ifdef _WIN32
	WSACleanup();
#else
	unlink(m_socket_name.c_str());
#endif

	close_portable(m_sock);
	close_portable(m_msgsock);

	if (std::this_thread::get_id() != m_thread.get_id() && m_thread.joinable())
	{
		m_thread.join();
	}

	m_debugServerInterface.reset();
}

void DebugNetworkServer::signal(int signal)
{
	std::scoped_lock<std::mutex> sc(m_debugMutex);
	if (m_signalCount + 1 == MAX_SIGNALS)
	{
		return;
	}

	m_signals[m_signalCount++] = signal;
}

int 
DebugNetworkServer::getNetworkStatus() const
{
	return 0;
}

bool 
DebugNetworkServer::reviveConnection()
{
	if (m_msgsock != 0)
	{
		closesocket(m_msgsock);
		m_msgsock = 0;
	}

	m_msgsock = accept(m_sock, 0, 0);

	if (m_msgsock == -1)
	{
		// everything else is non recoverable in our scope
		// we also mark as recoverable socket errors where it would block a
		// non blocking socket, even though our socket is blocking, in case
		// we ever have to implement a non blocking socket.
#ifdef _WIN32
		int errno_w = WSAGetLastError();
		if (!(errno_w == WSAECONNRESET || errno_w == WSAEINTR || errno_w == WSAEINPROGRESS || errno_w == WSAEMFILE || errno_w == WSAEWOULDBLOCK))
		{
#else
		if (!(errno == ECONNABORTED || errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
		{
#endif
			  
			return false;
		}
	}

	u_long mode = 1;
#ifdef _WIN32
	if (ioctlsocket(m_msgsock, FIONBIO, &mode) < 0)
#else
	if (fcntl(m_msgsock, O_NONBLOCK, &mode) < 0)
#endif
	{
		Console.WriteLn(Color_Red, "DebugNetworkServer: unnable to set socket as non-blocking! Shutting down...");
		shutdown();
		return false;
	}

	return true;
}

void 
DebugNetworkServer::serverLoop()
{
	if (!reviveConnection())
	{
		shutdown();
		return;
	}

	while (!m_end.load(std::memory_order_acquire))
	{
		if (!receiveAndSendPacket())
		{
			break;
		}
	}
}

bool DebugNetworkServer::receiveAndSendPacket()
{
	const auto receive_length = read_portable(m_msgsock, &m_recv_buffer[0], MAX_DEBUG_PACKET_SIZE);

	// we recreate the socket if an error happens
	if (receive_length <= 0)
	{
		if (would_block())
		{
			// that's ok, just wait until our packet is ready
			Threading::Sleep(1);
			return true;
		}

		if (!reviveConnection())
		{
			shutdown();
			return true;
		}

		return false;
	}

	std::size_t outSize = 0;
	std::size_t offset = 0;
	do
	{
		const std::size_t localOffset = m_debugServerInterface->processPacket((char*)&m_recv_buffer.at(offset), receive_length, m_send_buffer.data(), outSize);
		if (localOffset == std::size_t(-1))
		{
			Console.WriteLn(Color_Red, "DebugNetworkServer: invalid packet passed! Shutting down...");
			shutdown();
			return false;
		}

		offset += localOffset;
		while (true)
		{
			if (write_portable(m_msgsock, m_send_buffer.data(), outSize) >= 0)
				break;

			if (would_block())
			{
				Threading::Sleep(1);
				continue;
			}

			if (!reviveConnection())
			{
				shutdown();
				return false;
			}
		}
	} while (offset < receive_length);

	return true;
}
