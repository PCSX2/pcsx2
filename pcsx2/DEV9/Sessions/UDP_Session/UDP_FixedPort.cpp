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

		sockaddr_in endpoint{0};
		endpoint.sin_family = AF_INET;
		*(IP_Address*)&endpoint.sin_addr = adapterIP;
		endpoint.sin_port = htons(parPort);

		ret = bind(client, (const sockaddr*)&endpoint, sizeof(endpoint));

		if (ret == SOCKET_ERROR)
			Console.Error("DEV9: UDP: Failed to bind socket. Error: %d",
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
			Console.Error("DEV9: UDP: select failed. Error Code: %d",
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
			unsigned long available = 0;
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
