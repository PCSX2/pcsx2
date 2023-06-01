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

DebugNetworkServer EEDebugNetworkServer;
DebugNetworkServer IOPDebugNetworkServer;
DebugNetworkServer VU0DebugNetworkServer;
DebugNetworkServer VU1DebugNetworkServer;

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
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <poll.h>
#endif

DebugNetworkServer::DebugNetworkServer()
{
#if _WIN32
	WSADATA wsa;
	
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		Console.WriteLn(Color_Red, "DebugNetworkServer: Cannot initialize winsock! Shutting down...");
	}
#endif
}

DebugNetworkServer::~DebugNetworkServer()
{
	shutdown();

#if _WIN32
	WSACleanup();
#endif
}

bool 
DebugNetworkServer::init(
	std::string_view name, 
	std::unique_ptr<DebugServerInterface> debugServerInterface,
	u16 port,
	const char* address
)
{
	m_end.store(false, std::memory_order_release);

	if (debugServerInterface == nullptr)
	{
		Console.WriteLn(Color_Red, "DebugNetworkServer: debug server interface is null! Shutting down...");
		return false;
	}

	// transfer exclusive pointer
	m_debugServerInterface = std::move(debugServerInterface);

	m_recv_buffer.resize(MAX_DEBUG_PACKET_SIZE);
	m_send_buffer.resize(MAX_DEBUG_PACKET_SIZE);

	m_port = port;
	m_name = name;
	m_address = address;
	m_thread = std::thread(&DebugNetworkServer::serverLoop, this);
	return true;
}

void 
DebugNetworkServer::shutdown()
{
	m_end.store(true, std::memory_order_release);

	if (std::this_thread::get_id() == m_thread.get_id())
	{
		m_connected = false;
		close_portable(m_sock);
		close_portable(m_msgsock);
	} 
	else
	{
		if (m_thread.joinable())
			m_thread.join();

		m_debugServerInterface.reset();
	}
}

bool
DebugNetworkServer::isConnected() const
{
	return m_connected;
}

bool
DebugNetworkServer::isRunning() const
{
	return m_thread.joinable();
}

int 
DebugNetworkServer::getNetworkStatus() const
{
	return 0;
}

int 
DebugNetworkServer::getPort() const
{
	return m_port;
}

bool DebugNetworkServer::setupSockets()
{
	m_end.store(false, std::memory_order_release);

#ifdef _WIN32
	struct sockaddr_in server;

	if (!m_wsaInited)
	{
		m_wsaInited = true;
	}

	m_sock = socket(AF_INET, SOCK_STREAM, 0);
	if ((m_sock == INVALID_SOCKET) || m_port > 65536)
	{
		Console.WriteLn(Color_Red, "DebugNetworkServer: Cannot open socket (%i)! Shutting down...", WSAGetLastError());
		shutdown();
		return false;
	}

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = m_address.empty() ? 0 : inet_addr(m_address.data());
	server.sin_port = htons(m_port);
	if (bind(m_sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR)
	{
		Console.WriteLn(Color_Red, "DebugNetworkServer: Error while binding to socket! Shutting down...");
		shutdown();
		return false;
	}
#else
	struct sockaddr_in server;
	m_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (m_sock < 0)
	{
		Console.WriteLn(Color_Red, "DebugNetworkServer: Cannot open socket! Shutting down...");
		shutdown();
		return false;
	}

	server.sin_family = AF_INET;
	server.sin_addr.s_addr = 0;
	if (!m_address.empty())
	{
		inet_pton(AF_INET, m_address.data(), &(server.sin_addr));
	}
	server.sin_port = htons(m_port);

	if (bind(m_sock, (struct sockaddr*)&server, sizeof(struct sockaddr_un)))
	{
		Console.WriteLn(Color_Red, "DebugNetworkServer: Error while binding to socket! Shutting down...");
		shutdown();
		return false;
	}
#endif
#ifdef _WIN32
	u_long mode = 1;
	if (ioctlsocket(m_sock, FIONBIO, &mode) < 0)
#else
	if (fcntl(m_sock, F_SETFL, fcntl(m_sock, F_GETFL) | O_NONBLOCK) < 0)
#endif
	{
		Console.WriteLn(Color_Red, "DebugNetworkServer: unnable to set socket as non-blocking! Shutting down...");
	}

	// Don't need more than 5 in this case, GDB or WinDbg can't handle so much packets anyway
	if (listen(m_sock, 5))
	{
		Console.WriteLn(Color_Red, "DebugNetworkServer: Cannot listen for connections! Shutting down...");
		shutdown();
		return false;
	}

	return true;
}

bool DebugNetworkServer::reviveConnection()
{
	if (m_msgsock != 0)
		shutdown();
	
	if (!setupSockets())
		return false;

	Console.WriteLn(Color_Green, "DebugNetworkServer: [%s] waiting for any connection on port %u...", m_name.data(), m_port);
	#ifdef _WIN32
	do
	{
		if (m_end.load())
			return false;

		m_msgsock = accept(m_sock, 0, 0);
		int errno_w = WSAGetLastError();
		if (errno_w == 0)
		{
			break;
		}

		if (errno_w == WSAECONNRESET || errno_w == WSAEINTR || errno_w == WSAEINPROGRESS || errno_w == WSAEMFILE || errno_w == WSAEWOULDBLOCK)
		{
			Threading::Sleep(1);
		}
		else
		{
			Console.WriteLn(Color_Red, "DebugNetworkServer: unnable to create socket (internal error, %i)! Shutting down...", errno_w);
			return false;
		}

	} while (m_msgsock == (u64)-1);
#else
	pollfd fd = {};
	fd.fd = m_sock;
	fd.events = POLLIN;
	int rc = 0;
	m_msgsock = -1;

	do 
	{
		if (m_end.load())
			return false;
			
		rc = poll(&fd, 1, 1);
		if (rc < 0)
		{
			Console.WriteLn(Color_Red, "DebugNetworkServer: unnable to create socket (internal error, %i)! Shutting down...", errno);
			return false;
		}

		if (rc > 0)
		{
			m_msgsock = accept(m_sock, 0, 0);
			if (errno != 0 && !(errno == ECONNABORTED || errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
				return false;
		}
	} while (m_msgsock < 0);
#endif

#ifdef _WIN32
	u_long mode = 1;
	if (ioctlsocket(m_msgsock, FIONBIO, &mode) < 0)
#else
	if (fcntl(m_msgsock, F_SETFL, fcntl(m_msgsock, F_GETFL) | O_NONBLOCK) < 0)
#endif
	{
		Console.WriteLn(Color_Red, "DebugNetworkServer: unnable to set socket as non-blocking! Shutting down...");
		return false;
	}

	m_connected = true;
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

	Console.WriteLn(Color_Green, "DebugNetworkServer: connection for %s on port %u has been opened", m_name.data(), m_port);
	while (!m_end.load(std::memory_order_acquire))
	{
		if (!receiveAndSendPacket())
		{
			break;
		}
	}

	shutdown();
}

bool DebugNetworkServer::receiveAndSendPacket()
{
	const auto receive_length = read_portable(m_msgsock, &m_recv_buffer[0], MAX_DEBUG_PACKET_SIZE - 1);
	std::size_t outSize = 0;
	std::int64_t offset = 0;

	// we recreate the socket if an error happens
	if (receive_length <= 0)
	{
		if (would_block())
		{
			if (!m_debugServerInterface->replyPacket(m_send_buffer.data(), outSize))
				return false;

			if (outSize > 0)
			{
				while (true)
				{
					if (write_portable(m_msgsock, m_send_buffer.data(), outSize) >= 0)
						break;

					if (would_block())
					{
						Threading::Sleep(1);
						continue;
					}

					return reviveConnection();
				}
			}
			else
			{
				// that's ok, just wait until our packet is ready
				Threading::Sleep(1);
			}

			return true;
		}

		return reviveConnection();
	}

	m_recv_buffer[receive_length] = 0;
	Console.WriteLn(Color_Orange, "recv");
	Console.WriteLn(Color_Gray, "%s", (char*)&m_recv_buffer[0]);

	const std::size_t localOffset = m_debugServerInterface->processPacket((char*)&m_recv_buffer.at(offset), receive_length, m_send_buffer.data(), outSize);
	if (localOffset == std::size_t(-1))
	{
		return reviveConnection();
	}

	if (localOffset == 0 || outSize == 0)
	{
		Threading::Sleep(1);
		return true;
	}

	m_send_buffer[outSize] = 0;
	Console.WriteLn(Color_Orange, "send");
	Console.WriteLn(Color_Gray, "%s", m_send_buffer.data());

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

		return reviveConnection();
	}

	return true;
}
