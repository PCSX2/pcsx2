/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "common/Assertions.h"

#ifdef __POSIX__
#define SOCKET_ERROR -1
#include <errno.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#endif

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <winsock2.h>
#else
#include <unistd.h>
#endif

#include "UDP_FixedPort.h"
#include "DEV9/PacketReader/IP/UDP/UDP_Packet.h"

using namespace PacketReader;
using namespace PacketReader::IP;
using namespace PacketReader::IP::UDP;

namespace Sessions
{
	UDP_FixedPort::UDP_FixedPort(ConnectionKey parKey, IP_Address parAdapterIP, u16 parPort)
		: BaseSession(parKey, parAdapterIP)
		, client{socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)}
		, port(parPort)
	{
		int ret;
		if (client == INVALID_SOCKET)
		{
			Console.Error("DEV9: UDP: Failed to open socket. Error: %d",
#ifdef _WIN32
				WSAGetLastError());
#elif defined(__POSIX__)
				errno);
#endif
			//RaiseEventConnectionClosed(); //TODO
			return;
		}

		const int reuseAddress = true; //BOOL
		ret = setsockopt(client, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuseAddress, sizeof(reuseAddress));

		if (ret == SOCKET_ERROR)
			Console.Error("DEV9: UDP: Failed to set SO_REUSEADDR. Error: %d",
#ifdef _WIN32
				WSAGetLastError());
#elif defined(__POSIX__)
				errno);
#endif

		const int broadcastEnable = true; //BOOL
		ret = setsockopt(client, SOL_SOCKET, SO_BROADCAST, (const char*)&broadcastEnable, sizeof(broadcastEnable));

		if (ret == SOCKET_ERROR)
			Console.Error("DEV9: UDP: Failed to set SO_BROADCAST. Error: %d",
#ifdef _WIN32
				WSAGetLastError());
#elif defined(__POSIX__)
				errno);
#endif
	}

	IP_Payload* UDP_FixedPort::Recv()
	{
		if (!open.load())
			return nullptr;

		int ret;
		fd_set sReady;
		fd_set sExcept;

		timeval nowait{0};
		FD_ZERO(&sReady);
		FD_ZERO(&sExcept);
		FD_SET(client, &sReady);
		FD_SET(client, &sExcept);
		ret = select(client + 1, &sReady, nullptr, &sExcept, &nowait);

		bool hasData;
		if (ret == SOCKET_ERROR)
		{
			hasData = false;
			Console.WriteLn("DEV9: UDP: select failed. Error Code: %d",
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
			if (getsockopt(client, SOL_SOCKET, SO_ERROR, (char*)&error, &len) < 0)
				Console.Error("DEV9: UDP: Unkown UDP Connection Error (getsockopt Error: %d)", WSAGetLastError());
#elif defined(__POSIX__)
			socklen_t len = sizeof(error);
			if (getsockopt(client, SOL_SOCKET, SO_ERROR, (char*)&error, &len) < 0)
				Console.Error("DEV9: UDP: Unkown UDP Connection Error (getsockopt Error: %d)", errno);
#endif
			else
				Console.Error("DEV9: UDP: Recv Error: %d", error);
		}
		else
			hasData = FD_ISSET(client, &sReady);

		if (hasData)
		{
			u_long available = 0;
			PayloadData* recived = nullptr;
			std::unique_ptr<u8[]> buffer;
			sockaddr endpoint{0};

			//FIONREAD returns total size of all available messages
			//but we will read one message at a time
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
				ret = recvfrom(client, (char*)buffer.get(), available, 0, &endpoint, &fromlen);
			}

			if (ret == SOCKET_ERROR)
			{
				Console.Error("UDP Recv Error: %d",
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
			iRet->destinationPort = port;

			sockaddr_in* sockaddr = (sockaddr_in*)&endpoint;
			destIP = *(IP_Address*)&sockaddr->sin_addr;
			iRet->sourcePort = ntohs(sockaddr->sin_port);
			{
				std::lock_guard numberlock(connectionSentry);

				for (size_t i = 0; i < connections.size(); i++)
				{
					UDP_BaseSession* s = connections[i];
					if (s->WillRecive(destIP))
						return iRet;
				}
			}
			Console.Error("DEV9: UDP: Unexpected packet, dropping");
			delete iRet;
		}
		return nullptr;
	}

	bool UDP_FixedPort::Send(PacketReader::IP::IP_Payload* payload)
	{
		pxAssert(false);
		return false;
	}

	void UDP_FixedPort::Reset()
	{
		std::lock_guard numberlock(connectionSentry);

		for (size_t i = 0; i < connections.size(); i++)
			connections[i]->Reset();
	}

	UDP_Session* UDP_FixedPort::NewClientSession(ConnectionKey parNewKey, bool parIsBrodcast, bool parIsMulticast)
	{
		UDP_Session* s = new UDP_Session(parNewKey, adapterIP, parIsBrodcast, parIsMulticast, client);

		s->AddConnectionClosedHandler([&](BaseSession* session) { HandleChildConnectionClosed(session); });

		{
			std::lock_guard numberlock(connectionSentry);
			connections.push_back(s);
		}
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
				RaiseEventConnectionClosed();
		}
	}

	UDP_FixedPort::~UDP_FixedPort()
	{
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
