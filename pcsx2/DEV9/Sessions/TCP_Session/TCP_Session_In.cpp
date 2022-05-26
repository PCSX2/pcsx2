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

#include <algorithm>

#ifdef __POSIX__
#define SOCKET_ERROR -1
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/select.h>
#define SD_RECEIVE SHUT_RD
#endif

#include "TCP_Session.h"

using namespace PacketReader;
using namespace PacketReader::IP;
using namespace PacketReader::IP::TCP;

namespace Sessions
{
	PacketReader::IP::IP_Payload* TCP_Session::Recv()
	{
		TCP_Packet* ret = PopRecvBuff();
		if (ret != nullptr)
			return ret;

		switch (state)
		{
			case TCP_State::SendingSYN_ACK:
			{
				fd_set writeSet;
				fd_set exceptSet;

				FD_ZERO(&writeSet);
				FD_ZERO(&exceptSet);

				FD_SET(client, &writeSet);
				FD_SET(client, &exceptSet);

				timeval nowait{0};
				select(client + 1, nullptr, &writeSet, &exceptSet, &nowait);

				if (FD_ISSET(client, &writeSet))
					return ConnectTCPComplete(true);
				if (FD_ISSET(client, &exceptSet))
					return ConnectTCPComplete(false);

				return nullptr;
			}
			case TCP_State::SentSYN_ACK:
				//Don't read data untill PS2 ACKs connection
				return nullptr;
			case TCP_State::CloseCompletedFlushBuffer:
				//When TCP connection is closed by the server
				//the server is the last to send a packet
				//so the event must be raised here
				state = TCP_State::CloseCompleted;
				RaiseEventConnectionClosed();
				return nullptr;
			case TCP_State::Connected:
			case TCP_State::Closing_ClosedByPS2:
				//Only accept data in above two states
				break;
			default:
				return nullptr;
		}

		uint maxSize = 0;
		if (sendTimeStamps)
			maxSize = std::min<uint>(maxSegmentSize - 12, windowSize.load());
		else
			maxSize = std::min<uint>(maxSegmentSize, windowSize.load());

		if (maxSize != 0 &&
			myNumberACKed.load())
		{
			std::unique_ptr<u8[]> buffer;
			int err = 0;
			int recived;

			u_long available;
#ifdef _WIN32
			err = ioctlsocket(client, FIONREAD, &available);
#elif defined(__POSIX__)
			err = ioctl(client, FIONREAD, &available);
#endif
			if (err != SOCKET_ERROR)
			{
				if (available > maxSize)
					Console.WriteLn("DEV9: TCP: Got a lot of data: %d Using: %d", available, maxSize);

				buffer = std::make_unique<u8[]>(maxSize);
				recived = recv(client, (char*)buffer.get(), maxSize, 0);
				if (recived == -1)
#ifdef _WIN32
					err = WSAGetLastError();
#elif defined(__POSIX__)
					err = errno;
#endif

				switch (err)
				{
#ifdef _WIN32
					case WSAEINVAL:
					case WSAESHUTDOWN:
						//In theory, this should only occur when the PS2 has RST the connection
						//and the call to TCPSession.Recv() occurs at just the right time.

						//Console.WriteLn("DEV9: TCP: Recv() on shutdown socket");
						return nullptr;
					case WSAEWOULDBLOCK:
						return nullptr;
#elif defined(__POSIX__)
					case EINVAL:
					case ESHUTDOWN:
						//See WSAESHUTDOWN
						//Console.WriteLn("DEV9: TCP: Recv() on shutdown socket");
						return nullptr;
					case EWOULDBLOCK:
						return nullptr;
#endif
					case 0:
						break;
					default:
						CloseByRemoteRST();
						Console.Error("DEV9: TCP: Recv Error: %d", err);
						return nullptr;
				}

				//Server Closed Socket
				if (recived == 0)
				{
					int result = shutdown(client, SD_RECEIVE);
					if (result == SOCKET_ERROR)
						Console.Error("DEV9: TCP: Shutdown SD_RECEIVE Error: %d",
#ifdef _WIN32
							WSAGetLastError());
#elif defined(__POSIX__)
							errno);
#endif

					switch (state)
					{
						case TCP_State::Connected:
							return CloseByRemoteStage1();
						case TCP_State::Closing_ClosedByPS2:
							return CloseByPS2Stage3();
						default:
							CloseByRemoteRST();
							Console.Error("DEV9: TCP: Remote Close In Invalid State");
							break;
					}
					return nullptr;
				}
				DevCon.WriteLn("DEV9: TCP: [SRV]Sending %d bytes", recived);

				PayloadData* recivedData = new PayloadData(recived);
				memcpy(recivedData->data.get(), buffer.get(), recived);

				TCP_Packet* iRet = CreateBasePacket(recivedData);
				IncrementMyNumber((u32)recived);

				iRet->SetACK(true);
				iRet->SetPSH(true);

				myNumberACKed.store(false);
				//DevCon.WriteLn("DEV9: TCP: myNumberACKed Reset");
				return iRet;
			}
		}

		return nullptr;
	}

	TCP_Packet* TCP_Session::ConnectTCPComplete(bool success)
	{
		if (success)
		{
			state = TCP_State::SentSYN_ACK;

			TCP_Packet* ret = new TCP_Packet(new PayloadData(0));
			//Return the fact we connected
			ret->sourcePort = destPort;
			ret->destinationPort = srcPort;

			ret->sequenceNumber = GetMyNumber();
			IncrementMyNumber(1);

			ret->acknowledgementNumber = expectedSeqNumber;

			ret->SetSYN(true);
			ret->SetACK(true);
			ret->windowSize = (2 * maxSegmentSize);
			ret->options.push_back(new TCPopMSS(maxSegmentSize));

			ret->options.push_back(new TCPopNOP());
			ret->options.push_back(new TCPopWS(0));

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
		else
		{
			int error = 0;
#ifdef _WIN32
			int len = sizeof(error);
			if (getsockopt(client, SOL_SOCKET, SO_ERROR, (char*)&error, &len) < 0)
				Console.Error("DEV9: TCP: Unkown TCP Connection Error (getsockopt Error: %d)", WSAGetLastError());
#elif defined(__POSIX__)
			socklen_t len = sizeof(error);
			if (getsockopt(client, SOL_SOCKET, SO_ERROR, (char*)&error, &len) < 0)
				Console.Error("DEV9: TCP: Unkown TCP Connection Error (getsockopt Error: %d)", errno);
#endif
			else
				Console.Error("DEV9: TCP: Send Error: %d", error);

			state = TCP_State::CloseCompleted;
			RaiseEventConnectionClosed();
			return nullptr;
		}
	}

	PacketReader::IP::TCP::TCP_Packet* TCP_Session::CloseByPS2Stage3()
	{
		//Console.WriteLn("DEV9: TCP: Remote has closed connection after PS2");

		TCP_Packet* ret = CreateBasePacket();
		IncrementMyNumber(1);

		ret->SetACK(true);
		ret->SetFIN(true);

		myNumberACKed.store(false);
		//DevCon.WriteLn("myNumberACKed Reset");

		state = TCP_State::Closing_ClosedByPS2ThenRemote_WaitingForAck;
		return ret;
	}

	PacketReader::IP::TCP::TCP_Packet* TCP_Session::CloseByRemoteStage1()
	{
		//Console.WriteLn("DEV9: TCP: Remote has closed connection");

		TCP_Packet* ret = CreateBasePacket();
		IncrementMyNumber(1);

		ret->SetACK(true);
		ret->SetFIN(true);

		myNumberACKed.store(false);
		//DevCon.WriteLn("myNumberACKed Reset");

		state = TCP_State::Closing_ClosedByRemote;
		return ret;
	}
} // namespace Sessions
