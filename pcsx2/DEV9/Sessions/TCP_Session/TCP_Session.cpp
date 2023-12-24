// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "TCP_Session.h"

#include <thread>

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#include <winsock2.h>
#else
#include <unistd.h>
#endif

using namespace PacketReader;
using namespace PacketReader::IP;
using namespace PacketReader::IP::TCP;

namespace Sessions
{
	void TCP_Session::PushRecvBuff(TCP_Packet* tcp)
	{
		_recvBuff.Enqueue(tcp);
	}
	TCP_Packet* TCP_Session::PopRecvBuff()
	{
		TCP_Packet* ret;
		if (_recvBuff.Dequeue(&ret))
			return ret;
		else
			return nullptr;
	}

	void TCP_Session::IncrementMyNumber(u32 amount)
	{
		std::lock_guard numberlock(myNumberSentry);
		_OldMyNumbers.push_back(_MySequenceNumber);
		_OldMyNumbers.erase(_OldMyNumbers.begin());

		_MySequenceNumber += amount;
	}
	u32 TCP_Session::GetMyNumber()
	{
		std::lock_guard numberlock(myNumberSentry);
		return _MySequenceNumber;
	}
	std::tuple<u32, std::vector<u32>> TCP_Session::GetAllMyNumbers()
	{
		std::lock_guard numberlock(myNumberSentry);

		std::vector<u32> old;
		old.reserve(_OldMyNumbers.size());
		old.insert(old.end(), _OldMyNumbers.begin(), _OldMyNumbers.end());

		return {_MySequenceNumber, old};
	}
	void TCP_Session::ResetMyNumbers()
	{
		std::lock_guard numberlock(myNumberSentry);
		_MySequenceNumber = 1;
		_OldMyNumbers.clear();
		for (int i = 0; i < oldMyNumCount; i++)
			_OldMyNumbers.push_back(1);
	}

	TCP_Session::TCP_Session(ConnectionKey parKey, IP_Address parAdapterIP)
		: BaseSession(parKey, parAdapterIP)
	{
	}

	TCP_Packet* TCP_Session::CreateBasePacket(PayloadData* data)
	{
		//DevCon.WriteLn("Creating Base Packet");
		if (data == nullptr)
			data = new PayloadData(0);

		TCP_Packet* ret = new TCP_Packet(data);

		//and now to setup THE ENTIRE THING
		ret->sourcePort = destPort;
		ret->destinationPort = srcPort;

		ret->sequenceNumber = GetMyNumber();
		//DevCon.WriteLn("With MySeq: %d", ret->sequenceNumber);
		ret->acknowledgementNumber = expectedSeqNumber;
		//DevCon.WriteLn("With MyAck: %d", ret->acknowledgementNumber);

		ret->windowSize = 2 * maxSegmentSize;

		if (sendTimeStamps)
		{
			ret->options.push_back(new TCPopNOP());
			ret->options.push_back(new TCPopNOP());

			const auto timestampChrono = std::chrono::steady_clock::now() - timeStampStart;
			const u32 timestampSeconds = std::chrono::duration_cast<std::chrono::seconds>(timestampChrono).count() % UINT_MAX;

			ret->options.push_back(new TCPopTS(timestampSeconds, lastRecivedTimeStamp));
		}
		return ret;
	}

	void TCP_Session::CloseSocket()
	{
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

	void TCP_Session::Reset()
	{
		//CloseSocket();
		RaiseEventConnectionClosed();
	}

	TCP_Session::~TCP_Session()
	{
		CloseSocket();

		//Clear out _recvBuff
		while (!_recvBuff.IsQueueEmpty())
		{
			TCP_Packet* retPay;
			if (!_recvBuff.Dequeue(&retPay))
			{
				using namespace std::chrono_literals;
				std::this_thread::sleep_for(1ms);
				continue;
			}

			delete retPay;
		}
	}
} // namespace Sessions
