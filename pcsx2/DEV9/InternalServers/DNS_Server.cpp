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

#include <chrono>
#include <thread>

#ifdef _WIN32
#include <ws2tcpip.h>
#elif defined(__POSIX__)
//Note that getaddrinfo_a() exists which allows asynchronous operation
//however, that function is not standard POSIX, and is instead part of glibc
//So we will run with getaddrinfo() in a thread ourself
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#endif

#if defined(__FreeBSD__)
#include <netinet/in.h>
#endif

#include "DNS_Server.h"
#include "DEV9/PacketReader/IP/UDP/UDP_Packet.h"
#include "DEV9/PacketReader/IP/UDP/DNS/DNS_Packet.h"

#include "DEV9/DEV9.h"

using namespace PacketReader;
using namespace PacketReader::IP;
using namespace PacketReader::IP::UDP;
using namespace PacketReader::IP::UDP::DNS;

namespace InternalServers
{
	DNS_Server::DNS_State::DNS_State(int count, std::vector<std::string> dnsQuestions, DNS_Packet* dnsPacket, u16 port)
	{
		dns = dnsPacket;
		counter.store(count);
		questions = dnsQuestions;
		clientPort = port;

		//Prefill unordered_map, allowing use to modify it from seperate threads
		//See https://en.cppreference.com/w/cpp/container#Thread_safety
		//Different elements in the same container can be modified concurrently by different threads
		for (size_t i = 0; i < dnsQuestions.size(); i++)
			answers[dnsQuestions[i]] = {0};
	}

	int DNS_Server::DNS_State::AddAnswer(const std::string& answer, IP_Address address)
	{
		answers[answer] = address;
		return --counter;
	}
	int DNS_Server::DNS_State::AddNoAnswer(const std::string& answer)
	{
		return --counter;
	}

	std::unordered_map<std::string, IP_Address> DNS_Server::DNS_State::GetAnswers()
	{
		return answers;
	}

	DNS_Server::DNS_Server(std::function<void()> receivedcallback)
		: callback{receivedcallback}
	{
#ifdef _WIN32
		/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
		WORD wVersionRequested = MAKEWORD(2, 2);

		WSADATA wsaData{0};
		const int err = WSAStartup(wVersionRequested, &wsaData);
		if (err != 0)
			Console.Error("DEV9: WSAStartup failed with error: %d\n", err);
		else
			wsa_init = true;
#endif
	}

	//We remap 127.0.0.1 to the PC's IP address
	//We specificly use the address assigned to
	//the adapter we are using
#ifdef _WIN32
	void DNS_Server::Init(PIP_ADAPTER_ADDRESSES adapter)
#elif defined(__POSIX__)
	void DNS_Server::Init(ifaddrs* adapter)
#endif
	{
		localhostIP = {127, 0, 0, 1};
		//Find IPv4 Address
#ifdef _WIN32
		PIP_ADAPTER_UNICAST_ADDRESS address = nullptr;
		if (adapter != nullptr)
		{
			PIP_ADAPTER_ADDRESSES info = (PIP_ADAPTER_ADDRESSES)adapter;
			address = info->FirstUnicastAddress;
			while (address != nullptr && address->Address.lpSockaddr->sa_family != AF_INET)
				address = address->Next;
		}

		if (address != nullptr)
		{
			sockaddr_in* sockaddr = (sockaddr_in*)address->Address.lpSockaddr;
			localhostIP = *(IP_Address*)&sockaddr->sin_addr;
		}
		else
			Console.Error("DEV9: Failed To Get Adapter IP");
#elif defined(__POSIX__)
		sockaddr* address = nullptr;
		if (adapter != nullptr)
		{
			ifaddrs* info = (ifaddrs*)adapter;
			if (info->ifa_addr != nullptr && info->ifa_addr->sa_family == AF_INET)
				address = info->ifa_addr;
		}

		if (address != nullptr)
		{
			sockaddr_in* sockaddr = (sockaddr_in*)address;
			localhostIP = *(IP_Address*)&sockaddr->sin_addr;
		}
		else
			Console.Error("DEV9: Failed To Get Adapter IP");
#endif

		LoadHostList();
	}

	void DNS_Server::LoadHostList()
	{
		hosts.clear();
#ifndef PCSX2_CORE
		for (const ConfigHost& entry : config.EthHosts)
		{
			if (entry.Enabled)
				hosts.insert_or_assign(entry.Url, *(IP_Address*)entry.Address);
		}
#else
		for (const Pcsx2Config::DEV9Options::HostEntry& entry : EmuConfig.DEV9.EthHosts)
		{
			if (entry.Enabled)
				hosts.insert_or_assign(entry.Url, *(IP_Address*)entry.Address);
		}
#endif
	}

	UDP_Packet* DNS_Server::Recv()
	{
		UDP_Packet* retPay;
		if (dnsQueue.Dequeue(&retPay))
		{
			outstandingQueries--;
			return retPay;
		}
		return nullptr;
	}

	bool DNS_Server::Send(UDP_Packet* payload)
	{
		PayloadPtr* udpPayload = static_cast<PayloadPtr*>(payload->GetPayload());
		DNS_Packet dns(udpPayload->data, udpPayload->GetLength());

		if (dns.GetOpCode() == (u8)DNS_OPCode::Query && dns.questions.size() > 0 && dns.GetQR() == false)
		{
			std::vector<std::string> reqs;

			for (size_t i = 0; i < dns.questions.size(); i++)
			{
				DNS_QuestionEntry q = dns.questions[i];
				if (q.entryType == 1 && q.entryClass == 1)
					reqs.push_back(q.name);
				else
					Console.Error("DEV9: Unexpected question type of class, T: %d C: %d", q.entryType, q.entryClass);
			}
			if (reqs.size() == 0)
				return true;
			if (dns.GetTC() == true)
			{
				Console.Error("DEV9: Truncated DNS packet Not Supported");
				return true;
			}

			DNS_Packet* ret = new DNS_Packet();
			ret->id = dns.id; //TODO, drop duplicate requests based on ID
			ret->SetQR(true);
			ret->SetOpCode((u8)DNS_OPCode::Query);
			ret->SetAA(false);
			ret->SetTC(false);
			ret->SetRD(true);
			ret->SetRA(true);
			ret->SetAD(false);
			ret->SetCD(false);
			ret->SetRCode((u8)DNS_RCode::NoError);
			//Counts
			ret->questions = dns.questions;

			DNS_State* state = new DNS_State(reqs.size(), reqs, ret, payload->sourcePort);
			outstandingQueries++;

			for (size_t i = 0; i < reqs.size(); i++)
			{
				if (CheckHostList(reqs[i], state))
					continue;
				GetHost(reqs[i], state);
			}
			return true;
		}
		else
		{
			Console.Error("DEV9: Unexpected DNS OPCode, Code: %s", dns.GetOpCode());
			return true;
		}
	}

	bool DNS_Server::CheckHostList(std::string url, DNS_State* state)
	{
		std::transform(url.begin(), url.end(), url.begin(),
					   [](unsigned char c) { return std::tolower(c); });

		auto f = hosts.find(url);
		if (f != hosts.end())
		{
			const int remaining = state->AddAnswer(url, hosts[url]);
			Console.WriteLn("DEV9: DNS: %s found in hosts", url.c_str());
			if (remaining == 0)
				FinaliseDNS(state);
			return true;
		}
		return false;
	}

	void DNS_Server::FinaliseDNS(DNS_State* state)
	{
		DNS_Packet* retPay = state->dns;
		std::vector<std::string> reqs = state->questions;
		std::unordered_map<std::string, IP_Address> answers = state->GetAnswers();

		for (size_t i = 0; i < reqs.size(); i++)
		{
			IP_Address ans = answers[reqs[i]];
			if (ans.integer != 0)
			{
				//TODO, might not be effective on pcap
				const IP_Address local{127, 0, 0, 1};
				if (ans == local)
					ans = localhostIP;

				std::vector<u8> ansVector;
				ansVector.resize(4);
				*(IP_Address*)&ansVector[0] = ans;
				DNS_ResponseEntry ansEntry(reqs[i], 1, 1, ansVector, 10800);
				retPay->answers.push_back(ansEntry);
			}
			else
				retPay->SetRCode(2); //ServerFailure
		}

		const u16 clientPort = state->clientPort;
		delete state;

		if (retPay->GetLength() > 512)
		{
			Console.Error("DEV9: Generated DNS response too large, dropping");
			delete retPay;
			outstandingQueries--;
			return;
		}

		UDP_Packet* retUdp = new UDP_Packet(retPay);
		retUdp->sourcePort = 53;
		retUdp->destinationPort = clientPort;
		dnsQueue.Enqueue(retUdp);
		callback();
	}

	DNS_Server::~DNS_Server()
	{
		//Block untill DNS finished &
		//Delete entries in queue
		while (outstandingQueries != 0)
		{
			UDP_Packet* retPay = nullptr;
			if (!dnsQueue.Dequeue(&retPay))
			{
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(10ms);
				continue;
			}

			delete retPay;
			outstandingQueries--;
		}

#ifdef _WIN32
		if (wsa_init)
			WSACleanup();
#endif
	}

#ifdef _WIN32
	void DNS_Server::GetHost(std::string url, DNS_State* state)
	{
		//Need to convert to UTF16
		const int size = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
		std::vector<wchar_t> converted_string(size);
		MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, converted_string.data(), converted_string.size());

		ADDRINFOEX hints{0};
		hints.ai_family = AF_INET;

		GetAddrInfoExCallbackData* data = new GetAddrInfoExCallbackData();
		data->state = state;
		data->session = this;
		data->url = url;

		int ret = GetAddrInfoEx(converted_string.data(), nullptr, NS_ALL, 0, &hints, (ADDRINFOEX**)&data->result, nullptr, &data->overlapped, &DNS_Server::GetAddrInfoExCallback, &data->cancelHandle);

		if (ret == WSA_IO_PENDING)
			return;
		else
			GetAddrInfoExCallback(ret, -1, &data->overlapped);
	}

	void __stdcall DNS_Server::GetAddrInfoExCallback(DWORD dwError, DWORD dwBytes, OVERLAPPED* lpOverlapped)
	{
		GetAddrInfoExCallbackData* data = reinterpret_cast<GetAddrInfoExCallbackData*>(lpOverlapped);

		int remaining = -1;
		switch (dwError)
		{
			case NO_ERROR:
			{
				ADDRINFOEX* addrInfo = (ADDRINFOEX*)data->result;
				while (addrInfo != nullptr && addrInfo->ai_family != AF_INET)
					addrInfo = addrInfo->ai_next;

				if (addrInfo == nullptr)
				{
					Console.Error("DEV9: Internal DNS failed to find host %s", data->url.c_str());
					Console.Error("DEV9: with unexpected error code %d", -1);
					remaining = data->state->AddNoAnswer(data->url);
					break;
				}

				sockaddr_in* sockaddr = (sockaddr_in*)addrInfo->ai_addr;
				remaining = data->state->AddAnswer(data->url, *(IP_Address*)&sockaddr->sin_addr);
				break;
			}
			case WSAHOST_NOT_FOUND:
			case WSATRY_AGAIN: //Nonauthoritative host not found
				Console.Error("DEV9: Internal DNS failed to find host %s", data->url.c_str());
				remaining = data->state->AddNoAnswer(data->url);
				break;
			default:
				Console.Error("DEV9: Internal DNS failed to find host %s", data->url.c_str());
				Console.Error("DEV9: with unexpected error code %d", dwError);
				remaining = data->state->AddNoAnswer(data->url);
				break;
		}

		pxAssert(remaining != -1);

		if (remaining == 0)
			data->session->FinaliseDNS(data->state);

		//cleanup
		if (data->result != nullptr)
			FreeAddrInfoEx((ADDRINFOEX*)data->result);
		delete data;
	}
#elif defined(__POSIX__)
	void DNS_Server::GetHost(std::string url, DNS_State* state)
	{
		//Need to spin up thread, pass the parms to it

		std::thread GetHostThread(&DNS_Server::GetAddrInfoThread, this, url, state);
		//detatch thread so that it can clean up itself
		//we use another method of waiting for thread compleation
		GetHostThread.detach();
	}

	void DNS_Server::GetAddrInfoThread(std::string url, DNS_State* state)
	{
		addrinfo hints{0};
		hints.ai_family = AF_INET;
		addrinfo* result = nullptr;

		int error = getaddrinfo(url.c_str(), nullptr, &hints, &result);
		int remaining = -1;
		switch (error)
		{
			case 0:
			{
				addrinfo* retInfo = result;
				while (retInfo != nullptr && retInfo->ai_family != AF_INET)
					retInfo = retInfo->ai_next;

				if (retInfo == nullptr)
				{
					Console.Error("DEV9: Internal DNS failed to find host %s", url.c_str());
					Console.Error("DEV9: with unexpected error code %d", -1);
					remaining = state->AddNoAnswer(url);
					break;
				}

				sockaddr_in* sockaddr = (sockaddr_in*)retInfo->ai_addr;
				remaining = state->AddAnswer(url, *(IP_Address*)&sockaddr->sin_addr);
				break;
			}
			case EAI_NONAME:
			case EAI_AGAIN: //Nonauthoritative host not found
				Console.Error("DEV9: Internal DNS failed to find host %s", url.c_str());
				remaining = state->AddNoAnswer(url);
				break;
			default:
				Console.Error("DEV9: Internal DNS failed to find host %s", url.c_str());
				Console.Error("DEV9: with unexpected error code %d", error);
				remaining = state->AddNoAnswer(url);
				break;
		}

		pxAssert(remaining != -1);

		if (remaining == 0)
			FinaliseDNS(state);

		//cleanup
		if (result != nullptr)
			freeaddrinfo(result);
	}
#endif
} // namespace InternalServers
