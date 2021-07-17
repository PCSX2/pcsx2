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

#pragma once
#include <atomic>
#include <unordered_map>
#include <functional>

#ifdef _WIN32
#include <minwinbase.h>
#endif

#include "DEV9/SimpleQueue.h"

#include "DEV9/PacketReader/IP/IP_Packet.h"
#include "DEV9/PacketReader/IP/UDP/UDP_Packet.h"
#include "DEV9/PacketReader/IP/UDP/DNS/DNS_Packet.h"

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#elif defined(__POSIX__)
#include <sys/types.h>
#include <ifaddrs.h>
#endif

namespace InternalServers
{
	class DNS_Server
	{
	private:
		class DNS_State
		{
		public:
			std::atomic<int> counter;

			std::vector<std::string> questions;
			PacketReader::IP::UDP::DNS::DNS_Packet* dns;
			u16 clientPort;

		private:
			std::unordered_map<std::string, PacketReader::IP::IP_Address> answers;

		public:
			DNS_State(int count, std::vector<std::string> dnsQuestions, PacketReader::IP::UDP::DNS::DNS_Packet* dnsPacket, u16 port);

			int AddAnswer(const std::string& answer, PacketReader::IP::IP_Address address);
			int AddNoAnswer(const std::string& answer);

			std::unordered_map<std::string, PacketReader::IP::IP_Address> GetAnswers();
		};

#ifdef _WIN32
		bool wsa_init = false;
#endif

		std::function<void()> callback;

		PacketReader::IP::IP_Address localhostIP{127, 0, 0, 1};
		std::unordered_map<std::string, PacketReader::IP::IP_Address> hosts;
		std::atomic<int> outstandingQueries{0};
		SimpleQueue<PacketReader::IP::UDP::UDP_Packet*> dnsQueue;

	public:
		DNS_Server(std::function<void()> receivedcallback);

#ifdef _WIN32
		void Init(PIP_ADAPTER_ADDRESSES adapter);
#elif defined(__POSIX__)
		void Init(ifaddrs* adapter);
#endif

		//Recv
		PacketReader::IP::UDP::UDP_Packet* Recv();
		//Expects a UDP_payload
		bool Send(PacketReader::IP::UDP::UDP_Packet* payload);

		//This might block for a large amount of time
		//if destruction takes place during DNS request
		//and the OS configured DNS server is unreachable
		~DNS_Server();

	private:
		void LoadHostList();
		bool CheckHostList(std::string url, DNS_State* state);
		void GetHost(std::string url, DNS_State* state);
		void FinaliseDNS(DNS_State* state);

#ifdef _WIN32
		struct GetAddrInfoExCallbackData
		{
			OVERLAPPED overlapped{0};
			void* result = nullptr;
			HANDLE cancelHandle = nullptr;
			DNS_State* state;
			DNS_Server* session;
			std::string url;
		};

		static void __stdcall GetAddrInfoExCallback(DWORD dwError, DWORD dwBytes, OVERLAPPED* lpOverlapped);
#elif defined(__POSIX__)
		void GetAddrInfoThread(std::string url, DNS_State* state);
#endif
	};
} // namespace InternalServers
