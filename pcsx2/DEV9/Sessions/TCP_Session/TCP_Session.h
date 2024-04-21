// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include <atomic>
#include <chrono>
#include <mutex>
#include <tuple>
#include <vector>
#ifdef _WIN32
#include <winsock2.h>
#elif defined(__POSIX__)
#define INVALID_SOCKET -1
#endif

#include "DEV9/SimpleQueue.h"
#include "DEV9/Sessions/BaseSession.h"
#include "DEV9/PacketReader/IP/TCP/TCP_Packet.h"

namespace Sessions
{
	class TCP_Session : public BaseSession
	{
	private:
		enum struct TCP_State
		{
			None,
			SendingSYN_ACK,
			SentSYN_ACK,
			Connected,
			Closing_ClosedByPS2,
			Closing_ClosedByPS2ThenRemote_WaitingForAck,
			Closing_ClosedByRemote,
			Closing_ClosedByRemoteThenPS2_WaitingForAck,
			CloseCompletedFlushBuffer, // Send any remaining packets in recvBuff
			CloseCompleted,
		};
		enum struct NumCheckResult
		{
			OK,
			OldSeq,
			Bad
		};

		SimpleQueue<PacketReader::IP::TCP::TCP_Packet*> _recvBuff;

#ifdef _WIN32
		SOCKET client = INVALID_SOCKET;
#elif defined(__POSIX__)
		int client = INVALID_SOCKET;
#endif

		TCP_State state = TCP_State::None;

		u16 srcPort = 0;
		u16 destPort = 0;

		u16 maxSegmentSize = 1460; // Accesed by both in and out threads, but set only on connect thread
		int windowScale = 0;
		std::atomic<int> windowSize{1460};

		u32 lastRecivedTimeStamp; // Accesed by both in and out threads
		std::chrono::steady_clock::time_point timeStampStart; // Set by in thread on connect, read by in and out threads
		bool sendTimeStamps = false; // Accesed by out thread only

		const int receivedPS2SeqNumberCount = 5;
		u32 expectedSeqNumber; // Accesed by out thread only
		std::vector<u32> receivedPS2SeqNumbers; // Accesed by out thread only

		std::mutex myNumberSentry;
		const int oldMyNumCount = 64;
		u32 _MySequenceNumber = 1;
		std::vector<u32> _OldMyNumbers;
		u32 _ReceivedAckNumber = 1;
		std::atomic<bool> myNumberACKed{true};

	public:
		TCP_Session(ConnectionKey parKey, PacketReader::IP::IP_Address parAdapterIP);

		virtual PacketReader::IP::IP_Payload* Recv();
		virtual bool Send(PacketReader::IP::IP_Payload* payload);
		virtual void Reset();

		virtual ~TCP_Session();

	private:
		// Async functions
		void PushRecvBuff(PacketReader::IP::TCP::TCP_Packet* tcp);
		PacketReader::IP::TCP::TCP_Packet* PopRecvBuff();

		void IncrementMyNumber(u32 amount);
		void UpdateReceivedAckNumber(u32 ack);
		u32 GetMyNumber();
		u32 GetOutstandingSequenceLength();
		bool ShouldWaitForAck();
		std::tuple<u32, std::vector<u32>> GetAllMyNumbers();
		void ResetMyNumbers();

		NumCheckResult CheckRepeatSYNNumbers(PacketReader::IP::TCP::TCP_Packet* tcp);
		NumCheckResult CheckNumbers(PacketReader::IP::TCP::TCP_Packet* tcp, bool rejectOldSeq = false);
		// Returns a - b, accounting for overflow
		s32 GetDelta(u32 a, u32 b);
		// Returns true if errored
		bool ValidateEmptyPacket(PacketReader::IP::TCP::TCP_Packet* tcp, bool ignoreOld = true);

		// PS2 sent SYN
		PacketReader::IP::TCP::TCP_Packet* ConnectTCPComplete(bool success);
		bool SendConnect(PacketReader::IP::TCP::TCP_Packet* tcp);
		bool SendConnected(PacketReader::IP::TCP::TCP_Packet* tcp);

		bool SendData(PacketReader::IP::TCP::TCP_Packet* tcp);
		bool SendNoData(PacketReader::IP::TCP::TCP_Packet* tcp);

		/*
		 * On close by PS2
		 * S1: PS2 Sends FIN+ACK
		 * S2: CloseByPS2Stage1_2 sends ACK, state set to Closing_ClosedByPS2
		 * S3: When server closes socket, we send FIN in CloseByPS2Stage3
		 * and set state to Closing_ClosedByPS2ThenRemote_WaitingForAck
		 * S4: PS2 then Sends ACK
		 */
		bool CloseByPS2Stage1_2(PacketReader::IP::TCP::TCP_Packet* tcp);
		PacketReader::IP::TCP::TCP_Packet* CloseByPS2Stage3();
		bool CloseByPS2Stage4(PacketReader::IP::TCP::TCP_Packet* tcp);

		/*
		 * On close By Server
		 * S1: CloseByRemoteStage1 sends FIN+ACK, state set to Closing_ClosedByRemote
		 * S2: PS2 Will then sends ACK, this is only checked after stage4
		 * S3: PS2 Will send FIN, possible in the previous ACK packet
		 * S4: CloseByRemoteStage3_4 sends ACK, state set to
		 * Closing_ClosedByRemoteThenPS2_WaitingForAck
		 * we then check if S3 has been completed
		 */
		PacketReader::IP::TCP::TCP_Packet* CloseByRemoteStage1();
		bool CloseByRemoteStage2_ButAfter4(PacketReader::IP::TCP::TCP_Packet* tcp);
		bool CloseByRemoteStage3_4(PacketReader::IP::TCP::TCP_Packet* tcp);

		// Error on sending data
		void CloseByRemoteRST();

		// Returned TCP_Packet takes ownership of data
		PacketReader::IP::TCP::TCP_Packet* CreateBasePacket(PacketReader::PayloadData* data = nullptr);

		void CloseSocket();
	};
} // namespace Sessions
