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

/* Client code example for interfacing with the IPC interface is available 
 * here: https://code.govanify.com/govanify/pcsx2_ipc/ */

#pragma once

#include "Utilities/PersistentThread.h"
#include "System/SysThreads.h"

using namespace Threading;

class SocketIPC : public pxThread
{

	typedef pxThread _parent;

protected:
#ifdef _WIN32
	// windows claim to have support for AF_UNIX sockets but that is a blatant lie,
	// their SDK won't even run their own examples, so we go on TCP sockets.
#define PORT 28011
#else
	// absolute path of the socket. Stored in the temporary directory in linux since
	// /run requires superuser permission
	const char* SOCKET_NAME = "/tmp/pcsx2.sock";
#endif

	// socket handlers
#ifdef _WIN32
	SOCKET m_sock = INVALID_SOCKET;
#else
	int m_sock = 0;
#endif

	// buffers that store the ipc request and reply messages.
	char* ret_buffer;
	char* ipc_buffer;

	// possible command messages
	enum IPCCommand
	{
		MsgRead8 = 0,
		MsgRead16 = 1,
		MsgRead32 = 2,
		MsgRead64 = 3,
		MsgWrite8 = 4,
		MsgWrite16 = 5,
		MsgWrite32 = 6,
		MsgWrite64 = 7
	};

	// possible result codes
	enum IPCResult
	{
		IPC_OK = 0,
		IPC_FAIL = 0xFF
	};

	// handle to the main vm thread
	SysCoreThread* m_vm;

	/* Thread used to relay IPC commands. */
	void ExecuteTaskInThread();

	/* Internal function, Parses an IPC command.
         * buf: buffer containing the IPC command.
         * ret_buffer: buffer that will be used to send the reply.
         * return value: pair containing a buffer with the result 
         *               of the command and its size. */
	std::pair<int, char*> ParseCommand(char* buf, char* ret_buffer);

	/* Formats an IPC buffer
         * ret_buffer: return buffer to use. 
         * return value: buffer containing the status code allocated of size
         *               size */
	static inline char* MakeOkIPC(char* ret_buffer);
	static inline char* MakeFailIPC(char* ret_buffer);

	/* Converts an uint to an char* in little endian 
         * res_array: the array to modify 
         * res: the value to convert
         * i: when to insert it into the array 
         * return value: res_array 
         * NB: implicitely inlined */
	template <typename T>
	static char* ToArray(char* res_array, T res, int i)
	{
		memcpy((res_array + i), (char*)&res, sizeof(T));
		return res_array;
	}

	/* Converts a char* to an uint in little endian 
         * arr: the array to convert
         * i: when to load it from the array 
         * return value: the converted value 
         * NB: implicitely inlined */
	template <typename T>
	static T FromArray(char* arr, int i)
	{
		return *(T*)(arr + i);
	}

public:
	/* Initializers */
	SocketIPC(SysCoreThread* vm);
	virtual ~SocketIPC();

}; // class SocketIPC
