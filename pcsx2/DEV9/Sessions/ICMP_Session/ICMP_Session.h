// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include <atomic>
#include <chrono>
#include <mutex>
#include <vector>

#include "DEV9/SimpleQueue.h"
#include "DEV9/ThreadSafeMap.h"
#include "DEV9/Sessions/BaseSession.h"
#include "DEV9/PacketReader/IP/ICMP/ICMP_Packet.h"

namespace Sessions
{
	class ICMP_Session : public BaseSession
	{
	private:
		struct PingResult
		{
			PacketReader::IP::IP_Address address;
			int type;
			int code;
			int dataLength;
			u8* data;
		};

		class Ping
		{
		public:
			u8 headerData[4];
			std::unique_ptr<PacketReader::IP::IP_Packet> originalPacket;

		private:
			const static std::chrono::duration<std::chrono::steady_clock::rep, std::chrono::steady_clock::period> ICMP_TIMEOUT;

#ifdef _WIN32
			HANDLE icmpFile{INVALID_HANDLE_VALUE};
			HANDLE icmpEvent{NULL};
#elif defined(__POSIX__)
			enum struct PingType
			{
				ICMP,
				RAW,
			};

			static PingType icmpConnectionKind;

			//Sockets
			int icmpSocket{-1};
			std::chrono::steady_clock::time_point icmpDeathClockStart;
			u16 icmpId;

#endif

			//Return buffers
			PingResult result{};
			int icmpResponseBufferLen{0};
			std::unique_ptr<u8[]> icmpResponseBuffer;

		public:
			Ping(int requestSize);
			bool IsInitialised();
			PingResult* Recv();
			bool Send(PacketReader::IP::IP_Address parAdapterIP, PacketReader::IP::IP_Address parDestIP, int parTimeToLive, PacketReader::PayloadPtr* parPayload);

			~Ping();
		};

		SimpleQueue<PacketReader::IP::ICMP::ICMP_Packet*> _recvBuff;
		std::mutex ping_mutex;
		std::vector<Ping*> pings;
		ThreadSafeMap<Sessions::ConnectionKey, Sessions::BaseSession*>* connections;

		std::atomic<int> open{0};

	public:
		ICMP_Session(ConnectionKey parKey, PacketReader::IP::IP_Address parAdapterIP, ThreadSafeMap<Sessions::ConnectionKey, Sessions::BaseSession*>* parConnections);

		virtual PacketReader::IP::IP_Payload* Recv();
		virtual bool Send(PacketReader::IP::IP_Payload* payload);
		bool Send(PacketReader::IP::IP_Payload* payload, PacketReader::IP::IP_Packet* packet);
		virtual void Reset();

		virtual ~ICMP_Session();
	};
} // namespace Sessions
