/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include <sys/types.h>
#if _WIN32
#define read_portable(a, b, c) (recv(a, b, c, 0))
#define write_portable(a, b, c) (send(a, b, c, 0))
#define bzero(b, len) (memset((b), '\0', (len)), (void)0)
#include <windows.h>
#else
#define read_portable(a, b, c) (read(a, b, c))
#define write_portable(a, b, c) (write(a, b, c))
#include <sys/socket.h>
#include <sys/un.h>
#endif

#include "Common.h"
#include "Memory.h"
#include "System/SysThreads.h"
#include "IPC.h"

SocketIPC::SocketIPC(SysCoreThread* vm)
	: pxThread("IPC_Socket")
{
#ifdef _WIN32
	WSADATA wsa;
	SOCKET new_socket;
	struct sockaddr_in server, client;
	int c;


	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
	{
		Console.WriteLn(Color_Red, "IPC: Cannot initialize winsock! Shutting down...");
		return;
	}

	if ((m_sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET)
	{
		Console.WriteLn(Color_Red, "IPC: Cannot open socket! Shutting down...");
		return;
	}

	// yes very good windows s/sun/sin/g sure is fine
	server.sin_family = AF_INET;
	// localhost only
	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	server.sin_port = htons(PORT);

	if (bind(m_sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR)
	{
		Console.WriteLn(Color_Red, "IPC: Error while binding to socket! Shutting down...");
		return;
	}

#else
	struct sockaddr_un server;

	m_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (m_sock < 0)
	{
		Console.WriteLn(Color_Red, "IPC: Cannot open socket! Shutting down...");
		return;
	}
	server.sun_family = AF_UNIX;
	strcpy(server.sun_path, SOCKET_NAME);

	// we unlink the socket so that when releasing this thread the socket gets
	// freed even if we didn't close correctly the loop
	unlink(SOCKET_NAME);
	if (bind(m_sock, (struct sockaddr*)&server, sizeof(struct sockaddr_un)))
	{
		Console.WriteLn(Color_Red, "IPC: Error while binding to socket! Shutting down...");
		return;
	}
#endif

	// maximum queue of SOMAXCONN commands before refusing, which stops the thread
	listen(m_sock, SOMAXCONN);

	// we save a handle of the main vm object
	m_vm = vm;

	// we start the thread
	Start();
}

void SocketIPC::ExecuteTaskInThread()
{
	int msgsock = 0;

	// we allocate once buffers to not have to do mallocs for each IPC
	// request, as malloc is expansive when we optimize for µs.
	m_ret_buffer = new char[MAX_IPC_RETURN_SIZE];
	m_ipc_buffer = new char[MAX_IPC_SIZE];
	while (true)
	{
		msgsock = accept(m_sock, 0, 0);
		if (msgsock == -1)
		{
			return;
		}
		else
		{
			if (read_portable(msgsock, m_ipc_buffer, 650000) < 0)
			{
				return;
			}
			else
			{
				auto res = ParseCommand(m_ipc_buffer, m_ret_buffer);
				if (write_portable(msgsock, res.buffer, res.size) < 0)
				{
					return;
				}
			}
		}
	}
}

SocketIPC::~SocketIPC()
{
#ifdef _WIN32
	closesocket(m_sock);
	WSACleanup();
#else
	close(m_sock);
	unlink(SOCKET_NAME);
#endif
	delete[] m_ret_buffer;
	delete[] m_ipc_buffer;
	// destroy the thread
	try
	{
		pxThread::Cancel();
	}
	DESTRUCTOR_CATCHALL
}

char* SocketIPC::MakeOkIPC(char* ret_buffer)
{
	ret_buffer[0] = IPC_OK;
	return ret_buffer;
}

char* SocketIPC::MakeFailIPC(char* ret_buffer)
{
	ret_buffer[0] = IPC_FAIL;
	return ret_buffer;
}

SocketIPC::IPCBuffer SocketIPC::ParseCommand(char* buf, char* ret_buffer)
{
	// currently all our instructions require a running VM so we check once
	// here, slightly helps performance
	if (!m_vm->HasActiveMachine())
		return IPCBuffer{1, MakeFailIPC(ret_buffer)};


	u16 batch = 1;
	u32 ret_cnt = 1;
	u32 buf_cnt = 0;
	if ((IPCCommand)buf[0] == MsgMultiCommand)
	{
		batch = FromArray<u16>(&buf[buf_cnt], 1);
		buf_cnt += 3;
	}

	for (u16 i = 0; i < batch; i++)
	{
		// YY YY YY YY from schema below
		u32 a = FromArray<u32>(&buf[buf_cnt], 1);

		//         IPC Message event (1 byte)
		//         |  Memory address (4 byte)
		//         |  |           argument (VLE)
		//         |  |           |
		// format: XX YY YY YY YY ZZ ZZ ZZ ZZ
		//        reply code: 00 = OK, FF = NOT OK
		//        |  return value (VLE)
		//        |  |
		// reply: XX ZZ ZZ ZZ ZZ
		switch ((IPCCommand)buf[buf_cnt])
		{
			case MsgRead8:
			{
				if (!SafetyChecks(buf_cnt, 5, ret_cnt, 1))
					goto error;
				u8 res;
				res = memRead8(a);
				ToArray(ret_buffer, res, ret_cnt);
				ret_cnt += 1;
				buf_cnt += 5;
				break;
			}
			case MsgRead16:
			{
				if (!SafetyChecks(buf_cnt, 5, ret_cnt, 2))
					goto error;
				u16 res;
				res = memRead16(a);
				ToArray(ret_buffer, res, ret_cnt);
				ret_cnt += 2;
				buf_cnt += 5;
				break;
			}
			case MsgRead32:
			{
				if (!SafetyChecks(buf_cnt, 5, ret_cnt, 4))
					goto error;
				u32 res;
				res = memRead32(a);
				ToArray(ret_buffer, res, ret_cnt);
				ret_cnt += 4;
				buf_cnt += 5;
				break;
			}
			case MsgRead64:
			{
				if (!SafetyChecks(buf_cnt, 5, ret_cnt, 8))
					goto error;
				u64 res;
				memRead64(a, &res);
				ToArray(ret_buffer, res, ret_cnt);
				ret_cnt += 8;
				buf_cnt += 5;
				break;
			}
			case MsgWrite8:
			{
				if (!SafetyChecks(buf_cnt, 6, ret_cnt))
					goto error;
				memWrite8(a, FromArray<u8>(&buf[buf_cnt], 5));
				buf_cnt += 6;
				break;
			}
			case MsgWrite16:
			{
				if (!SafetyChecks(buf_cnt, 7, ret_cnt))
					goto error;
				memWrite16(a, FromArray<u16>(&buf[buf_cnt], 5));
				buf_cnt += 7;
				break;
			}
			case MsgWrite32:
			{
				if (!SafetyChecks(buf_cnt, 9, ret_cnt))
					goto error;
				memWrite32(a, FromArray<u32>(&buf[buf_cnt], 5));
				buf_cnt += 9;
				break;
			}
			case MsgWrite64:
			{
				if (!SafetyChecks(buf_cnt, 13, ret_cnt))
					goto error;
				memWrite64(a, FromArray<u64>(&buf[buf_cnt], 5));
				buf_cnt += 13;
				break;
			}
			default:
			{
			error:
				return IPCBuffer{1, MakeFailIPC(ret_buffer)};
			}
		}
	}
	return IPCBuffer{(int)ret_cnt, MakeOkIPC(ret_buffer)};
}
