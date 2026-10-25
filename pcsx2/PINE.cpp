// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "BuildVersion.h"
#include "Common.h"
#include "Host.h"
#include "Memory.h"
#include "Elfheader.h"
#include "SaveState.h"
#include "PINE.h"
#include "VMManager.h"
#include "DebugTools/Breakpoints.h"
#include "DebugTools/MIPSAnalyst.h"
#include "DebugTools/MipsStackWalk.h"
#include "GS/GS.h"
#include "MTGS.h"
#include "R5900.h"
#include "SIO/Pad/Pad.h"
#include "SIO/Pad/PadBase.h"
#include "SIO/Pad/PadDualshock2.h"
#include "SPU2/spu2.h"
#include "DEV9/net.h"
#include "DEV9/NetCapture.h"
#include "GSStreamServer.h"
#include "common/Error.h"
#include "common/Threading.h"

#include <atomic>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <mutex>
#include <span>
#include <sys/types.h>
#include <thread>
#include <unordered_set>

#include "fmt/format.h"

#if defined(_WIN32)
#define read_portable(a, b, c) (recv(a, (char*)b, c, 0))
#define write_portable(a, b, c) (send(a, (const char*)b, c, 0))
#define safe_close_portable(a) \
	do \
	{ \
		if ((a) >= 0) \
		{ \
			closesocket((a)); \
			(a) = INVALID_SOCKET; \
		} \
	} while (0)
#include "common/RedtapeWindows.h"
#include <WinSock2.h>
#elif defined(__linux__) || defined(__FreeBSD__)
#define read_portable(a, b, c) (read(a, b, c))
#define write_portable(a, b, c) (send(a, b, c, MSG_NOSIGNAL))
#define safe_close_portable(a) \
	do \
	{ \
		if ((a) >= 0) \
		{ \
			close((a)); \
			(a) = -1; \
		} \
	} while (0)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#else
#define read_portable(a, b, c) (read(a, b, c))
#define write_portable(a, b, c) (write(a, b, c))
#define safe_close_portable(a) \
	do \
	{ \
		if ((a) >= 0) \
		{ \
			close((a)); \
			(a) = -1; \
		} \
	} while (0)
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

#define PINE_EMULATOR_NAME "pcsx2"

#ifdef _WIN32

static bool InitializeWinsock()
{
	static bool initialized = false;
	if (initialized)
		return true;

	WSADATA wsa = {};
	if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
		return false;

	initialized = true;
	std::atexit([]() { WSACleanup(); });
	return true;
}

#endif

namespace PINEServer
{
	static std::thread s_thread;
	static int s_slot;

#ifdef _WIN32
	// windows claim to have support for AF_UNIX sockets but that is a blatant lie,
	// their SDK won't even run their own examples, so we go on TCP sockets.
	static SOCKET s_sock = INVALID_SOCKET;
	// the message socket used in thread's accept().
	static SOCKET s_msgsock = INVALID_SOCKET;
#else
	// absolute path of the socket. Stored in XDG_RUNTIME_DIR, if unset /tmp
	static std::string s_socket_name;
	static int s_sock = -1;
	// the message socket used in thread's accept().
	static int s_msgsock = -1;
#endif

	// Whether the socket processing thread should stop executing/is stopped.
	static std::atomic_bool s_end{true};

	/**
	 * Maximum memory used by an IPC message request.
	 * Equivalent to 50,000 Write64 requests.
	 */
#define MAX_IPC_SIZE 650000

	/**
	 * Maximum memory used by an IPC message reply.
	 * Equivalent to 50,000 Read64 replies.
	 */
#define MAX_IPC_RETURN_SIZE 450000

	/**
	 * IPC return buffer.
	 * A preallocated buffer used to store all IPC replies.
	 * to the size of 50.000 MsgWrite64 IPC calls.
	 */
	static std::vector<u8> s_ret_buffer;

	/**
	 * IPC messages buffer.
	 * A preallocated buffer used to store all IPC messages.
	 */
	static std::vector<u8> s_ipc_buffer;

	/**
	 * Set of EE PC addresses registered as PINE breakpoints.
	 * Guarded by s_bp_mutex.
	 */
	static std::unordered_set<u32> s_breakpoints;
	static std::mutex s_bp_mutex;

	/**
	 * IPC Command messages opcodes.
	 * A list of possible operations possible by the IPC.
	 * Each one of them is what we call an "opcode" and is the first
	 * byte sent by the IPC to differentiate between commands.
	 */
	enum IPCCommand : unsigned char
	{
		MsgRead8 = 0, /**< Read 8 bit value to memory. */
		MsgRead16 = 1, /**< Read 16 bit value to memory. */
		MsgRead32 = 2, /**< Read 32 bit value to memory. */
		MsgRead64 = 3, /**< Read 64 bit value to memory. */
		MsgWrite8 = 4, /**< Write 8 bit value to memory. */
		MsgWrite16 = 5, /**< Write 16 bit value to memory. */
		MsgWrite32 = 6, /**< Write 32 bit value to memory. */
		MsgWrite64 = 7, /**< Write 64 bit value to memory. */
		MsgVersion = 8, /**< Returns PCSX2 version. */
		MsgSaveState = 9, /**< Saves a savestate. */
		MsgLoadState = 0xA, /**< Loads a savestate. */
		MsgTitle = 0xB, /**< Returns the game title. */
		MsgID = 0xC, /**< Returns the game ID. */
		MsgUUID = 0xD, /**< Returns the game UUID. */
		MsgGameVersion = 0xE, /**< Returns the game verion. */
		MsgStatus = 0xF, /**< Returns the emulator status. */
		MsgGetProgramCounter   = 0x10, /**< Returns EE PC and pause state. */
		MsgPause               = 0x11, /**< Pauses the emulator. */
		MsgResume              = 0x12, /**< Resumes the emulator. */
		MsgStep                = 0x13, /**< Steps one EE instruction. */
		MsgSetBreakpoint       = 0x14, /**< Sets a breakpoint at the given EE address. */
		MsgClearBreakpoint     = 0x15, /**< Clears a breakpoint at the given EE address. */
		MsgClearAllBreakpoints = 0x16, /**< Clears all EE breakpoints. */
		MsgGetRegisters        = 0x17, /**< Returns EE GPRs, PC, HI, LO. */
		MsgGetRegister         = 0x18, /**< Read one register: CPU type, category, index → u128. */
		MsgSetRegister         = 0x19, /**< Write one register: CPU type, category, index, u128. */
		MsgGetEEThreads        = 0x1A, /**< List all EE (R5900) threads. */
		MsgGetIOPThreads       = 0x1B, /**< List all IOP (R3000) threads. */
		MsgGetModules          = 0x1C, /**< List all IOP modules. */
		MsgGetStack            = 0x1D, /**< Walk the EE call stack. */
		MsgStepInto            = 0x1E, /**< Step into next instruction (non-blocking). */
		MsgStepOver            = 0x1F, /**< Step over function calls (non-blocking). */
		MsgStepOut             = 0x20, /**< Step out of current function (non-blocking). */
		MsgGetSymbol           = 0x21, /**< Look up EE function symbol by address. */
		MsgSaveStateFile       = 0x22, /**< Save state to a named file path. */
		MsgLoadStateFile       = 0x23, /**< Load state from a named file path. */
		MsgReset               = 0x24, /**< Cold-boot reset. */
		MsgFrameAdvance        = 0x25, /**< Advance N video frames then pause. */
		MsgGetFPS              = 0x26, /**< Current emulated framerate as f32. */
		MsgSetLimiterMode      = 0x27, /**< Set speed limiter mode (0=Nominal, 1=Turbo, 2=Slomo, 3=Unlimited). */
		MsgListBreakpoints     = 0x28, /**< List all active EE and/or IOP breakpoints. */
		MsgDisassemble         = 0x29, /**< Disassemble N instructions at an address. */
		MsgListFunctions       = 0x2A, /**< Paginated list of EE function symbols. */
		MsgGetSymbolByName     = 0x2B, /**< Look up any EE symbol by name → address. */
		MsgListGlobals         = 0x2C, /**< Paginated list of EE global variables. */
		MsgGetLocals           = 0x2D, /**< List locals/params for the function containing an EE address. */
		MsgAddWatch            = 0x2E, /**< Add a memory watchpoint (cpu, start, end, condition). */
		MsgRemoveWatch         = 0x2F, /**< Remove a memory watchpoint (cpu, start, end). */
		MsgListWatches         = 0x30, /**< List all active memory watchpoints. */
		MsgClearAllWatches     = 0x31, /**< Clear all memory watchpoints. */
		MsgSaveSnapshot        = 0x32, /**< Save a screenshot of the current game frame to disk. */
		MsgPadGetState         = 0x33, /**< Read full pad state (buttons, analogs, pressures). */
		MsgPadSetButton        = 0x34, /**< Set a single button on a pad by index and value. */
		MsgPadSetAnalog        = 0x35, /**< Set analog stick positions on a pad directly. */
		MsgPadSetState         = 0x36, /**< Set the complete pad state in one message. */
		MsgPadGetType          = 0x37, /**< Query the controller type of a pad slot. */
		MsgNetGetStatus        = 0x38, /**< Query DEV9 adapter status (enabled, type, MAC, IP). */
		MsgNetCapture          = 0x39, /**< Enable/disable packet capture with direction and queue size. */
		MsgNetRecvCapture      = 0x3A, /**< Drain captured packets from the queue. */
		MsgNetInject           = 0x3B, /**< Inject a raw Ethernet frame into the PS2 RX path. */
		MsgNetInjectTx         = 0x3C, /**< Inject a raw Ethernet frame out through the TX adapter. */
		MsgNetDisassemble      = 0x3D, /**< Dissect a raw Ethernet frame and return a text summary. */
		MsgNetSetFilter        = 0x3E, /**< Set the capture filter (by protocol, port, or IP). */
		MsgNetGetStats         = 0x3F, /**< Return TX/RX counters and capture queue statistics. */
		MsgGSStreamGetPort     = 0x40, /**< Return the GS stream listener port (0 if disabled). */
		MsgGSStreamGetStatus   = 0x41, /**< Return GS stream stats: active, port, subscribers, delivered, dropped. */
		MsgGSStreamPing        = 0x42, /**< Reserved: lightweight liveness probe for the GS stream channel. */
		MsgReadMemBatch        = 0x43, /**< Read fixed-size memory slices from multiple addresses. */
		MsgWriteMemBatch       = 0x44, /**< Write fixed-size memory slices to multiple addresses. */
		MsgSetAudioMuted       = 0x45, /**< Mute or unmute audio output (same as the Toggle Mute hotkey). */
		MsgGetAudioState       = 0x46, /**< Query audio output mute state and volume. */
		MsgUnimplemented = 0xFF /**< Unimplemented IPC message. */
	};

	enum BatchResult : u8
	{
		BatchSuccess = 0,
		BatchPartial = 1,
	};

	enum BatchItemResult : u8
	{
		BatchItemOk = 0,
	};

	/**
	 * Emulator status enum.
	 * A list of possible emulator statuses.
	 */
	enum EmuStatus : uint32_t
	{
		Running = 0, /**< Game is running */
		Paused = 1, /**< Game is paused */
		Shutdown = 2 /**< Game is shutdown */
	};

	/**
	 * IPC message buffer.
	 * A list of all needed fields to store an IPC message.
	 */
	struct IPCBuffer
	{
		int size; /**< Size of the buffer. */
		std::vector<u8> buffer; /**< Buffer. */
	};

	/**
	 * IPC result codes.
	 * A list of possible result codes the IPC can send back.
	 * Each one of them is what we call an "opcode" or "tag" and is the
	 * first byte sent by the IPC to differentiate between results.
	 */
	enum IPCResult : unsigned char
	{
		IPC_OK = 0, /**< IPC command successfully completed. */
		IPC_FAIL = 0xFF /**< IPC command failed to complete. */
	};

	// Thread used to relay IPC commands.
	void MainLoop();
	void ClientLoop();

	/**
	 * Internal function, Parses an IPC command.
	 * buf: buffer containing the IPC command.
	 * buf_size: size of the buffer announced.
	 * ret_buffer: buffer that will be used to send the reply.
	 * return value: IPCBuffer containing a buffer with the result
	 *               of the command and its size.
	 */
	static IPCBuffer ParseCommand(std::span<u8> buf, std::vector<u8>& ret_buffer, u32 buf_size);

	/**
	 * Formats an IPC buffer
	 * ret_buffer: return buffer to use.
	 * size: size of the IPC buffer.
	 * return value: buffer containing the status code allocated of size
	 */
	static std::vector<u8>& MakeOkIPC(std::vector<u8>& ret_buffer, uint32_t size);
	static std::vector<u8>& MakeFailIPC(std::vector<u8>& ret_buffer, uint32_t size);

	/**
	 * Initializes an open socket for IPC communication.
	 */
	bool AcceptClient();

	/**
	 * Converts a primitive value to bytes in little endian
	 * res_vector: the vector to modify
	 * res: the value to convert
	 * i: where to insert it into the vector
	 * NB: implicitely inlined
	 */
	template <typename T>
	static void ToResultVector(std::vector<u8>& res_vector, T res, int i)
	{
		memcpy(&res_vector[i], (char*)&res, sizeof(T));
	}

	/**
	 * Converts bytes in little endian to a primitive value
	 * span: the span to convert
	 * i: where to load it from the span
	 * return value: the converted value
	 * NB: implicitely inlined
	 */
	template <typename T>
	static T FromSpan(std::span<u8> span, int i)
	{
		return *(T*)(&span[i]);
	}

	/**
	 * Ensures an IPC message isn't too big.
	 * return value: false if checks failed, true otherwise.
	 */
	static inline bool SafetyChecks(u32 command_len, int command_size, u32 reply_len, int reply_size = 0, u32 buf_size = MAX_IPC_SIZE - 1)
	{
		return !((command_len + command_size) > buf_size ||
				 (reply_len + reply_size) >= MAX_IPC_RETURN_SIZE);
	}
}

	static inline u32 NormalizePcKseg0(u32 vaddr)
	{
		if ((vaddr & 0xE0000000u) == 0xA0000000u)
			return (vaddr & 0x1FFFFFFFu) | 0x80000000u;
		return vaddr;
	}
 // namespace PINEServer

bool PINEServer::Initialize(int slot)
{
	// Refuse to initialize if a server is already running in this process.
	if (IsInitialized())
	{
		Console.WriteLn(Color_Yellow, "PINE: A server is already running on slot %d. Skipping initialization.", slot);
		return false;
	}

	s_end.store(false, std::memory_order_release);
	s_slot = slot;

#ifdef _WIN32
	if (!InitializeWinsock())
	{
		Console.WriteLn(Color_Red, "PINE: Cannot initialize winsock! Shutting down...");
		Deinitialize();
		return false;
	}

	// Probe whether another process is already listening on this TCP port.
	{
		SOCKET probe = socket(AF_INET, SOCK_STREAM, 0);
		if (probe != INVALID_SOCKET)
		{
			sockaddr_in probe_addr = {};
			probe_addr.sin_family = AF_INET;
			probe_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
			probe_addr.sin_port = htons(static_cast<u_short>(slot));
			if (connect(probe, reinterpret_cast<sockaddr*>(&probe_addr), sizeof(probe_addr)) == 0)
			{
				closesocket(probe);
				Console.WriteLn(Color_Red, "PINE: A server is already listening on port %d. Shutting down...", slot);
				s_end.store(true, std::memory_order_release);
				return false;
			}
			closesocket(probe);
		}
	}

	s_sock = socket(AF_INET, SOCK_STREAM, 0);
	if ((s_sock == INVALID_SOCKET) || slot > 65535)
	{
		Console.WriteLn(Color_Red, "PINE: Cannot open socket! Shutting down...");
		Deinitialize();
		return false;
	}

	// Prevent address reuse so two instances cannot silently share the same port.
	{
		BOOL exclusive = TRUE;
		if (setsockopt(s_sock, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) == SOCKET_ERROR)
			Console.WriteLn(Color_Yellow, "PINE: Could not set SO_EXCLUSIVEADDRUSE (error %d); duplicate servers may go undetected.", WSAGetLastError());
	}

	sockaddr_in server = {};
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // localhost only
	server.sin_port = htons(slot);

	if (bind(s_sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR)
	{
		Console.WriteLn(Color_Red, "PINE: Error while binding to socket! Shutting down...");
		Deinitialize();
		return false;
	}

#else
	char* runtime_dir = nullptr;
#ifdef __APPLE__
	runtime_dir = std::getenv("TMPDIR");
#else
	runtime_dir = std::getenv("XDG_RUNTIME_DIR");
#endif
	// fallback in case macOS or other OSes don't implement the XDG base
	// spec
	if (runtime_dir == nullptr)
		s_socket_name = "/tmp/" PINE_EMULATOR_NAME ".sock";
	else
	{
		s_socket_name = runtime_dir;
		s_socket_name += "/" PINE_EMULATOR_NAME ".sock";
	}

	if (slot != PINE_DEFAULT_SLOT)
		s_socket_name += "." + std::to_string(slot);

	struct sockaddr_un server;

	// Probe whether another process is already listening on this Unix socket.
	// Only unlink a stale (dead) socket file; refuse to replace a live server.
	{
		int probe = socket(AF_UNIX, SOCK_STREAM, 0);
		if (probe >= 0)
		{
			struct sockaddr_un probe_addr;
			probe_addr.sun_family = AF_UNIX;
			StringUtil::Strlcpy(probe_addr.sun_path, s_socket_name, sizeof(probe_addr.sun_path));
			if (connect(probe, reinterpret_cast<struct sockaddr*>(&probe_addr), sizeof(probe_addr)) == 0)
			{
				close(probe);
				Console.WriteLn(Color_Red, "PINE: A server is already listening on %s. Shutting down...", s_socket_name.c_str());
				s_socket_name = {};
				s_end.store(true, std::memory_order_release);
				return false;
			}
			close(probe);
		}
		// No live server — safe to remove a stale socket file if it exists.
		unlink(s_socket_name.c_str());
	}

	s_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (s_sock < 0)
	{
		Console.WriteLn(Color_Red, "PINE: Cannot open socket! Shutting down...");
		Deinitialize();
		return false;
	}
	server.sun_family = AF_UNIX;
	StringUtil::Strlcpy(server.sun_path, s_socket_name, sizeof(server.sun_path));

	if (bind(s_sock, (struct sockaddr*)&server, sizeof(struct sockaddr_un)))
	{
		Console.WriteLn(Color_Red, "PINE: Error while binding to socket! Shutting down...");
		Deinitialize();
		return false;
	}
#endif

	// maximum queue of 4096 commands before refusing, approximated to the
	// nearest legal value. We do not use SOMAXCONN as windows have this idea
	// that a "reasonable" value is 5, which is not.
	if (listen(s_sock, 4096))
	{
		Console.WriteLn(Color_Red, "PINE: Cannot listen for connections! Shutting down...");
		Deinitialize();
		return false;
	}

	// we allocate once buffers to not have to do mallocs for each IPC
	// request, as malloc is expansive when we optimize for µs.
	s_ret_buffer.resize(MAX_IPC_RETURN_SIZE);
	s_ipc_buffer.resize(MAX_IPC_SIZE);

	// we start the thread
	s_thread = std::thread(&PINEServer::MainLoop);

	return true;
}

bool PINEServer::IsInitialized()
{
	return !s_end.load(std::memory_order_acquire);
}

int PINEServer::GetSlot()
{
	return s_slot;
}

std::vector<u8>& PINEServer::MakeOkIPC(std::vector<u8>& ret_buffer, uint32_t size = 5)
{
	ToResultVector<uint32_t>(ret_buffer, size, 0);
	ret_buffer[4] = IPC_OK;
	return ret_buffer;
}

std::vector<u8>& PINEServer::MakeFailIPC(std::vector<u8>& ret_buffer, uint32_t size = 5)
{
	ToResultVector<uint32_t>(ret_buffer, size, 0);
	ret_buffer[4] = IPC_FAIL;
	return ret_buffer;
}

bool PINEServer::AcceptClient()
{
	s_msgsock = accept(s_sock, 0, 0);
	if (s_msgsock >= 0)
	{
		// Gross C-style cast, but SOCKET is a handle on Windows.
		Console.WriteLn("PINE: New client with FD %d connected.", (int)s_msgsock);
#ifdef __APPLE__
		int nosigpipe = 1;
		setsockopt(s_msgsock, SOL_SOCKET, SO_NOSIGPIPE, &nosigpipe, sizeof(nosigpipe));
#endif
		return true;
	}

	// everything else is non recoverable in our scope
	// we also mark as recoverable socket errors where it would block a
	// non blocking socket, even though our socket is blocking, in case
	// we ever have to implement a non blocking socket.
#ifdef _WIN32
	const int errno_w = WSAGetLastError();
	if (!(errno_w == WSAECONNRESET || errno_w == WSAEINTR || errno_w == WSAEINPROGRESS || errno_w == WSAEMFILE || errno_w == WSAEWOULDBLOCK) && s_sock != INVALID_SOCKET)
		Console.Error("PINE: accept() returned error %d", errno_w);
#else
	if (!(errno == ECONNABORTED || errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) && s_sock >= 0)
		Console.Error("PINE: accept() returned error %d", errno);
#endif

	return false;
}

void PINEServer::MainLoop()
{
	Threading::SetNameOfCurrentThread("PINE Server");

	while (!s_end.load(std::memory_order_acquire))
	{
		if (!AcceptClient())
			continue;

		ClientLoop();

		Console.WriteLn("PINE: Client disconnected.");
		safe_close_portable(s_msgsock);
	}
}

void PINEServer::ClientLoop()
{
	while (!s_end.load(std::memory_order_acquire))
	{
		// either int or ssize_t depending on the platform, so we have to
		// use a bunch of auto
		auto receive_length = 0;
		auto end_length = 4;
		const std::span<u8> ipc_buffer_span(s_ipc_buffer);

		// while we haven't received the entire packet, maybe due to
		// socket datagram splittage, we continue to read
		while (receive_length < end_length)
		{
			const auto tmp_length = read_portable(s_msgsock, &ipc_buffer_span[receive_length], MAX_IPC_SIZE - receive_length);

			// we recreate the socket if an error happens
			if (tmp_length <= 0)
				return;

			receive_length += tmp_length;

			// if we got at least the final size then update
			if (end_length == 4 && receive_length >= 4)
			{
				end_length = FromSpan<u32>(ipc_buffer_span, 0);
				// we'd like to avoid a client trying to do OOB
				if (end_length > MAX_IPC_SIZE || end_length < 4)
				{
					receive_length = 0;
					break;
				}
			}
		}
		PINEServer::IPCBuffer res;

		// we remove 4 bytes to get the message size out of the IPC command
		// size in ParseCommand.
		// also, if we got a failed command, let's reset the state so we don't
		// end up deadlocking by getting out of sync, eg when a client
		// disconnects
		if (receive_length != 0)
		{
			res = ParseCommand(ipc_buffer_span.subspan(4), s_ret_buffer, (u32)end_length - 4);

			// if we cannot send back our answer restart the socket
			if (write_portable(s_msgsock, res.buffer.data(), res.size) < 0)
				return;
		}
	}
}

void PINEServer::Deinitialize()
{
	s_end.store(true, std::memory_order_release);

#ifndef _WIN32
	if (!s_socket_name.empty())
	{
		unlink(s_socket_name.c_str());
		s_socket_name = {};
	}
#endif

	// shutdown() is needed, otherwise accept() will still block.
#ifdef _WIN32
	if (s_sock != INVALID_SOCKET)
		shutdown(s_sock, SD_BOTH);
#else
	if (s_sock >= 0)
		shutdown(s_sock, SHUT_RDWR);
#endif

	safe_close_portable(s_sock);
	safe_close_portable(s_msgsock);

	if (s_thread.joinable())
		s_thread.join();
}

/* Forward declaration for IOP memory read used by the PINE IPC handlers.
   Some build configurations may not include the header that declares this symbol
   in this translation unit. Declaring it here resolves the compile-time lookup.
*/
u8 iopMemRead8(u32 addr);
// Insert a forward declaration for IOP memory write used by the PINE IPC handlers.
// Some build configurations may not include the header that declares this symbol
// in this translation unit. Declaring it here resolves the compile-time lookup.
void iopMemWrite8(u32 addr, u8 byte);

PINEServer::IPCBuffer PINEServer::ParseCommand(std::span<u8> buf, std::vector<u8>& ret_buffer, u32 buf_size)
{
	// Buffer-size constants for new opcodes
	// Breakpoints: addr(4) + enabled(1) + cpu(1) = 6 bytes each
	static constexpr int MAX_BREAKPOINTS_RESPONSE = 10000;
	static constexpr int BREAKPOINT_ENTRY_SIZE    = 6;
	// Disassembly: addr(4) + len(1) + text(255 max) = 260 bytes each
	static constexpr int MAX_DISASSEMBLY_LINES     = 1000;
	static constexpr int MAX_DISASSEMBLY_LINE_SIZE = 4 + 1 + 255;
	// Symbol listing: addr(4) + size(4) + name_len(1) + name(255 max) = 264 bytes each
	// Max batch is bounded so that (ret_cnt_initial + header + batch*entry) < MAX_IPC_RETURN_SIZE (450000)
	static constexpr int MAX_SYMBOL_BATCH          = 1500;
	static constexpr int MAX_SYMBOL_ENTRY_SIZE     = 4 + 4 + 1 + 255;
	// Locals: storage_type(1) + value(4) + name_len(1) + name(255 max) = 261 bytes each
	static constexpr int MAX_LOCALS_RESPONSE       = 512;
	static constexpr int MAX_LOCAL_ENTRY_SIZE      = 1 + 4 + 1 + 255;
	// Watches: start(4) + end(4) + cond(1) + result(1) + cpu(1) = 11 bytes each
	static constexpr int MAX_WATCHES_RESPONSE      = 10000;
	static constexpr int WATCH_ENTRY_SIZE          = 4 + 4 + 1 + 1 + 1;
	static constexpr u16 MAX_BATCH_COUNT           = 1024;
	static constexpr u16 MAX_BATCH_LENGTH_PER_ITEM = 256;
	static constexpr u32 MAX_BATCH_TOTAL_BYTES     = 1024 * 1024;

	u32 ret_cnt = 5;
	u32 buf_cnt = 0;

	while (buf_cnt < buf_size)
	{
		if (!SafetyChecks(buf_cnt, 1, ret_cnt, 0, buf_size)) [[unlikely]]
		{
			Console.Error("PINE: Safety check failed reading opcode at buffer offset %u (buf_size=%u).", buf_cnt, buf_size);
			return IPCBuffer{5, MakeFailIPC(ret_buffer)};
		}
		buf_cnt++;
		// example IPC messages: MsgRead/Write
		// refer to the client doc for more info on the format
		//         IPC Message event (1 byte)
		//         |  Memory address (4 byte)
		//         |  |           argument (VLE)
		//         |  |           |
		// format: XX YY YY YY YY ZZ ZZ ZZ ZZ
		//        reply code: 00 = OK, FF = NOT OK
		//        |  return value (VLE)
		//        |  |
		// reply: XX ZZ ZZ ZZ ZZ
		switch ((IPCCommand)buf[buf_cnt - 1])
		{
			case MsgRead8:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 4, ret_cnt, 1, buf_size)) [[unlikely]]
					goto error;
				const u32 a = FromSpan<u32>(buf, buf_cnt);
				const u8 res = memRead8(a);
				ToResultVector(ret_buffer, res, ret_cnt);
				ret_cnt += 1;
				buf_cnt += 4;
				break;
			}
			case MsgRead16:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 4, ret_cnt, 2, buf_size)) [[unlikely]]
					goto error;
				const u32 a = FromSpan<u32>(buf, buf_cnt);
				const u16 res = memRead16(a);
				ToResultVector(ret_buffer, res, ret_cnt);
				ret_cnt += 2;
				buf_cnt += 4;
				break;
			}
			case MsgRead32:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 4, ret_cnt, 4, buf_size)) [[unlikely]]
					goto error;
				const u32 a = FromSpan<u32>(buf, buf_cnt);
				const u32 res = memRead32(a);
				ToResultVector(ret_buffer, res, ret_cnt);
				ret_cnt += 4;
				buf_cnt += 4;
				break;
			}
			case MsgRead64:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 4, ret_cnt, 8, buf_size)) [[unlikely]]
					goto error;
				const u32 a = FromSpan<u32>(buf, buf_cnt);
				const u64 res = memRead64(a);
				ToResultVector(ret_buffer, res, ret_cnt);
				ret_cnt += 8;
				buf_cnt += 4;
				break;
			}
			case MsgWrite8:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 1 + 4, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;
				const u32 a = FromSpan<u32>(buf, buf_cnt);
				memWrite8(a, FromSpan<u8>(buf, buf_cnt + 4));
				buf_cnt += 5;
				break;
			}
			case MsgWrite16:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 2 + 4, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;
				const u32 a = FromSpan<u32>(buf, buf_cnt);
				memWrite16(a, FromSpan<u16>(buf, buf_cnt + 4));
				buf_cnt += 6;
				break;
			}
			case MsgWrite32:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 4 + 4, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;
				const u32 a = FromSpan<u32>(buf, buf_cnt);
				memWrite32(a, FromSpan<u32>(buf, buf_cnt + 4));
				buf_cnt += 8;
				break;
			}
			case MsgWrite64:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 8 + 4, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;
				const u32 a = FromSpan<u32>(buf, buf_cnt);
				memWrite64(a, FromSpan<u64>(buf, buf_cnt + 4));
				buf_cnt += 12;
				break;
			}
			case MsgReadMemBatch:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 1 + 2 + 2, ret_cnt, 1 + 2, buf_size)) [[unlikely]]
					goto error;

				const u8 cpu = FromSpan<u8>(buf, buf_cnt);
				const u16 item_size = FromSpan<u16>(buf, buf_cnt + 1);
				const u16 count = FromSpan<u16>(buf, buf_cnt + 3);
				buf_cnt += 5;

				if (count == 0 || count > MAX_BATCH_COUNT || item_size == 0 || item_size > MAX_BATCH_LENGTH_PER_ITEM)
					goto error;
				if (static_cast<u32>(count) * static_cast<u32>(item_size) > MAX_BATCH_TOTAL_BYTES)
					goto error;
				if (!SafetyChecks(buf_cnt, static_cast<int>(count) * 4, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				// The status + count header is patched in once the items are known.
				const u32 header_pos = ret_cnt;
				u8 batch_status = BatchSuccess;
				u16 returned_count = 0;
				ret_buffer[ret_cnt++] = batch_status;
				ToResultVector(ret_buffer, returned_count, ret_cnt);
				ret_cnt += 2;

				for (u16 i = 0; i < count; i++)
				{
					const u32 addr = FromSpan<u32>(buf, buf_cnt);
					buf_cnt += 4;

					if ((ret_cnt + 4 + 1 + 2 + item_size) > MAX_BATCH_TOTAL_BYTES || !SafetyChecks(buf_cnt, 0, ret_cnt, 4 + 1 + 2 + item_size, buf_size))
					{
						batch_status = BatchPartial;
						break;
					}

					ToResultVector(ret_buffer, addr, ret_cnt);
					ret_cnt += 4;
					ret_buffer[ret_cnt++] = BatchItemOk;
					ToResultVector(ret_buffer, item_size, ret_cnt);
					ret_cnt += 2;

					for (u16 b = 0; b < item_size; b++)
					{
						const u8 byte = (cpu == 1) ? iopMemRead8(addr + b) : memRead8(addr + b);
						ret_buffer[ret_cnt++] = byte;
					}
					returned_count++;
				}

				ret_buffer[header_pos] = batch_status;
				ToResultVector(ret_buffer, returned_count, header_pos + 1);
				break;
			}
			case MsgWriteMemBatch:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 1 + 2 + 2, ret_cnt, 1 + 2, buf_size)) [[unlikely]]
					goto error;

				const u8 cpu = FromSpan<u8>(buf, buf_cnt);
				const u16 item_size = FromSpan<u16>(buf, buf_cnt + 1);
				const u16 count = FromSpan<u16>(buf, buf_cnt + 3);
				buf_cnt += 5;

				if (count == 0 || count > MAX_BATCH_COUNT || item_size == 0 || item_size > MAX_BATCH_LENGTH_PER_ITEM)
					goto error;
				if (static_cast<u32>(count) * static_cast<u32>(item_size) > MAX_BATCH_TOTAL_BYTES)
					goto error;
				if (!SafetyChecks(buf_cnt, (static_cast<int>(count) * 4) + (static_cast<int>(count) * item_size), ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				// The status + count header is patched in once the items are known.
				const u32 header_pos = ret_cnt;
				u8 batch_status = BatchSuccess;
				u16 returned_count = 0;
				ret_buffer[ret_cnt++] = batch_status;
				ToResultVector(ret_buffer, returned_count, ret_cnt);
				ret_cnt += 2;

				for (u16 i = 0; i < count; i++)
				{
					const u32 addr = FromSpan<u32>(buf, buf_cnt);
					buf_cnt += 4;
					const u32 data_start = buf_cnt;
					buf_cnt += item_size;

					if (!SafetyChecks(buf_cnt, 0, ret_cnt, 4 + 1, buf_size))
					{
						batch_status = BatchPartial;
						break;
					}

					for (u16 b = 0; b < item_size; b++)
					{
						const u8 byte = FromSpan<u8>(buf, data_start + b);
						if (cpu == 1)
							iopMemWrite8(addr + b, byte);
						else
							memWrite8(addr + b, byte);
					}

					ToResultVector(ret_buffer, addr, ret_cnt);
					ret_cnt += 4;
					ret_buffer[ret_cnt++] = BatchItemOk;
					returned_count++;
				}

				ret_buffer[header_pos] = batch_status;
				ToResultVector(ret_buffer, returned_count, header_pos + 1);
				break;
			}
			case MsgVersion:
			{
				if (!VMManager::HasValidVM())
					goto error;
				u32 size = strlen(BuildVersion::GitRev) + 7;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, size + 4, buf_size)) [[unlikely]]
					goto error;
				ToResultVector(ret_buffer, size, ret_cnt);
				ret_cnt += 4;
				snprintf(reinterpret_cast<char*>(&ret_buffer[ret_cnt]), size, "PCSX2 %s", BuildVersion::GitRev);
				ret_cnt += size;
				break;
			}
			case MsgSaveState:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 1, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;
				Host::RunOnCPUThread([slot = FromSpan<u8>(buf, buf_cnt)] {
					VMManager::SaveStateToSlot(slot, true, [slot](const std::string& error) {
						SaveState_ReportSaveErrorOSD(error, slot);
					});
				});
				buf_cnt += 1;
				break;
			}
			case MsgLoadState:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 1, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;
				Host::RunOnCPUThread([slot = FromSpan<u8>(buf, buf_cnt)] {
					Error state_error;
					if (!VMManager::LoadStateFromSlot(slot, false, &state_error))
						SaveState_ReportLoadErrorOSD(state_error.GetDescription(), slot, false);
				});
				buf_cnt += 1;
				break;
			}
			case MsgTitle:
			{
				if (!VMManager::HasValidVM())
					goto error;
				const std::string gameName = VMManager::GetTitle(false);
				const u32 size = gameName.size() + 1;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, size + 4, buf_size)) [[unlikely]]
					goto error;
				ToResultVector(ret_buffer, size, ret_cnt);
				ret_cnt += 4;
				memcpy(&ret_buffer[ret_cnt], gameName.c_str(), size);
				ret_cnt += size;
				break;
			}
			case MsgID:
			{
				if (!VMManager::HasValidVM())
					goto error;
				const std::string gameSerial = VMManager::GetDiscSerial();
				const u32 size = gameSerial.size() + 1;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, size + 4, buf_size)) [[unlikely]]
					goto error;
				ToResultVector(ret_buffer, size, ret_cnt);
				ret_cnt += 4;
				memcpy(&ret_buffer[ret_cnt], gameSerial.c_str(), size);
				ret_cnt += size;
				break;
			}
			case MsgUUID:
			{
				if (!VMManager::HasValidVM())
					goto error;
				const std::string crc = fmt::format("{:08x}", VMManager::GetDiscCRC());
				const u32 size = crc.size() + 1;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, size + 4, buf_size)) [[unlikely]]
					goto error;
				ToResultVector(ret_buffer, size, ret_cnt);
				ret_cnt += 4;
				memcpy(&ret_buffer[ret_cnt], crc.c_str(), size);
				ret_cnt += size;
				break;
			}
			case MsgGameVersion:
			{
				if (!VMManager::HasValidVM())
					goto error;

				const std::string ElfVersion = VMManager::GetDiscVersion();
				const u32 size = ElfVersion.size() + 1;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, size + 4, buf_size)) [[unlikely]]
					goto error;
				ToResultVector(ret_buffer, size, ret_cnt);
				ret_cnt += 4;
				memcpy(&ret_buffer[ret_cnt], ElfVersion.c_str(), size);
				ret_cnt += size;
				break;
			}
			case MsgStatus:
			{
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 4, buf_size)) [[unlikely]]
					goto error;
				EmuStatus status;

				switch (VMManager::GetState())
				{
					case VMState::Running:
						status = EmuStatus::Running;
						break;
					case VMState::Paused:
						status = EmuStatus::Paused;
						break;
					default:
						status = EmuStatus::Shutdown;
						break;
				}

				ToResultVector(ret_buffer, status, ret_cnt);
				ret_cnt += 4;
				break;
			}
			case MsgGetProgramCounter:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 5, buf_size)) [[unlikely]]
					goto error;

				u32 pc = 0;
				u8 paused = 0;
				Host::RunOnCPUThread([&]() {
					pc = NormalizePcKseg0(cpuRegs.pc);
					paused = (VMManager::GetState() == VMState::Paused) ? 1 : 0;
				}, true);

				ToResultVector(ret_buffer, pc, ret_cnt);
				ret_cnt += 4;
				ret_buffer[ret_cnt++] = paused;
				break;
			}
			case MsgPause:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				Host::RunOnCPUThread([]() { VMManager::SetPaused(true); }, true);
				break;
			}
			case MsgResume:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				Host::RunOnCPUThread([]() { VMManager::SetPaused(false); }, true);
				break;
			}
			case MsgStep:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 4, buf_size)) [[unlikely]]
				goto error;

				bool stepped = false;
				u32 new_pc = 0;
				Host::RunOnCPUThread([&]() {
					if (VMManager::GetState() != VMState::Paused)
						return;
				 intCpu.Step();
				 new_pc = NormalizePcKseg0(cpuRegs.pc);
				 stepped = true;
				}, true);

				if (!stepped)
					goto error;

				ToResultVector(ret_buffer, new_pc, ret_cnt);
				ret_cnt += 4;
				break;
			}
			case MsgSetBreakpoint:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 4, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				const u32 addr = FromSpan<u32>(buf, buf_cnt);
				Host::RunOnCPUThread([addr]() {
					CBreakPoints::AddBreakPoint(BREAKPOINT_EE, addr, false, true, false);
				}, true);
				buf_cnt += 4;
				break;
			}
			case MsgClearBreakpoint:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 4, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				const u32 addr = FromSpan<u32>(buf, buf_cnt);
				bool had_bp = false;
				Host::RunOnCPUThread([addr, &had_bp]() {
					had_bp = CBreakPoints::IsAddressBreakPoint(BREAKPOINT_EE, addr);
					if (had_bp)
						CBreakPoints::RemoveBreakPoint(BREAKPOINT_EE, addr);
				}, true);
				buf_cnt += 4;

				if (!had_bp)
					goto error;

				break;
			}
			case MsgClearAllBreakpoints:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				Host::RunOnCPUThread([]() { CBreakPoints::ClearAllBreakPoints(); }, true);
				break;
			}
			case MsgGetRegisters:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 140, buf_size)) [[unlikely]]
					goto error;

				Host::RunOnCPUThread([&]() {
					for (int i = 0; i < 32; i++)
					{
						ToResultVector(ret_buffer, cpuRegs.GPR.r[i].UL[0], ret_cnt);
						ret_cnt += 4;
					}
					ToResultVector(ret_buffer, NormalizePcKseg0(cpuRegs.pc), ret_cnt);
					ret_cnt += 4;
					ToResultVector(ret_buffer, cpuRegs.HI.UL[0], ret_cnt);
					ret_cnt += 4;
					ToResultVector(ret_buffer, cpuRegs.LO.UL[0], ret_cnt);
					ret_cnt += 4;
				}, true);

				break;
			}
			case MsgGetRegister:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 3, ret_cnt, 16, buf_size)) [[unlikely]]
					goto error;

				const u8 cpu_sel = FromSpan<u8>(buf, buf_cnt);
				const u8 cat     = FromSpan<u8>(buf, buf_cnt + 1);
				const u8 num     = FromSpan<u8>(buf, buf_cnt + 2);
				DebugInterface& dbg = DebugInterface::get(cpu_sel == 0 ? BREAKPOINT_EE : BREAKPOINT_IOP);
				u128 val;
				Host::RunOnCPUThread([&]() { val = dbg.getRegister(cat, num); }, true);
				ToResultVector(ret_buffer, val, ret_cnt);
				ret_cnt += 16;
				buf_cnt += 3;
				break;
			}
			case MsgSetRegister:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (VMManager::GetState() != VMState::Paused)
					goto error;
				if (!SafetyChecks(buf_cnt, 3 + 16, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				const u8 cpu_sel = FromSpan<u8>(buf, buf_cnt);
				const u8 cat     = FromSpan<u8>(buf, buf_cnt + 1);
				const u8 num     = FromSpan<u8>(buf, buf_cnt + 2);
				const u128 val   = FromSpan<u128>(buf, buf_cnt + 3);
				DebugInterface& dbg = DebugInterface::get(cpu_sel == 0 ? BREAKPOINT_EE : BREAKPOINT_IOP);
				Host::RunOnCPUThread([&]() { dbg.setRegister(cat, num, val); }, true);
				buf_cnt += 3 + 16;
				break;
			}
			case MsgGetEEThreads:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (VMManager::GetState() != VMState::Paused)
					goto error;
				// count(4) + up to 256 threads × 22 bytes each
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 4 + 256 * 22, buf_size)) [[unlikely]]
					goto error;

				std::vector<std::unique_ptr<BiosThread>> threads;
				Host::RunOnCPUThread([&]() { threads = r5900Debug.GetThreadList(); }, true);

				const u32 count = static_cast<u32>(threads.size());
				ToResultVector(ret_buffer, count, ret_cnt);
				ret_cnt += 4;
				for (const auto& t : threads)
				{
					ToResultVector(ret_buffer, t->TID(), ret_cnt);           ret_cnt += 4;
					ToResultVector(ret_buffer, t->PC(), ret_cnt);            ret_cnt += 4;
					ret_buffer[ret_cnt++] = static_cast<u8>(t->Status());
					ret_buffer[ret_cnt++] = static_cast<u8>(t->Wait());
					ToResultVector(ret_buffer, t->Priority(), ret_cnt);      ret_cnt += 4;
					ToResultVector(ret_buffer, t->EntryPoint(), ret_cnt);    ret_cnt += 4;
					ToResultVector(ret_buffer, t->StackTop(), ret_cnt);      ret_cnt += 4;
				}
				break;
			}
			case MsgGetIOPThreads:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (VMManager::GetState() != VMState::Paused)
					goto error;
				// count(4) + up to 1000 threads × 22 bytes each
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 4 + 1000 * 22, buf_size)) [[unlikely]]
					goto error;

				std::vector<std::unique_ptr<BiosThread>> threads;
				Host::RunOnCPUThread([&]() { threads = r3000Debug.GetThreadList(); }, true);

				const u32 count = static_cast<u32>(threads.size());
				ToResultVector(ret_buffer, count, ret_cnt);
				ret_cnt += 4;
				for (const auto& t : threads)
				{
					ToResultVector(ret_buffer, t->TID(), ret_cnt);           ret_cnt += 4;
					ToResultVector(ret_buffer, t->PC(), ret_cnt);            ret_cnt += 4;
					ret_buffer[ret_cnt++] = static_cast<u8>(t->Status());
					ret_buffer[ret_cnt++] = static_cast<u8>(t->Wait());
					ToResultVector(ret_buffer, t->Priority(), ret_cnt);      ret_cnt += 4;
					ToResultVector(ret_buffer, t->EntryPoint(), ret_cnt);    ret_cnt += 4;
					ToResultVector(ret_buffer, t->StackTop(), ret_cnt);      ret_cnt += 4;
				}
				break;
			}
			case MsgGetModules:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (VMManager::GetState() != VMState::Paused)
					goto error;
				// count(4) + up to 1000 modules × 58 bytes each
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 4 + 1000 * 58, buf_size)) [[unlikely]]
					goto error;

				std::vector<IopMod> modules;
				Host::RunOnCPUThread([&]() { modules = r3000Debug.GetModuleList(); }, true);

				const u32 count = static_cast<u32>(modules.size());
				ToResultVector(ret_buffer, count, ret_cnt);
				ret_cnt += 4;
				for (const auto& m : modules)
				{
					// name: 32 bytes, NUL-padded
					const size_t name_len = std::min(m.name.size(), static_cast<size_t>(31));
					memset(&ret_buffer[ret_cnt], 0, 32);
					memcpy(&ret_buffer[ret_cnt], m.name.c_str(), name_len);
					ret_cnt += 32;
					ToResultVector(ret_buffer, m.version,   ret_cnt); ret_cnt += 2;
					ToResultVector(ret_buffer, m.text_addr, ret_cnt); ret_cnt += 4;
					ToResultVector(ret_buffer, m.entry,     ret_cnt); ret_cnt += 4;
					ToResultVector(ret_buffer, m.gp,        ret_cnt); ret_cnt += 4;
					ToResultVector(ret_buffer, m.text_size, ret_cnt); ret_cnt += 4;
					ToResultVector(ret_buffer, m.data_size, ret_cnt); ret_cnt += 4;
					ToResultVector(ret_buffer, m.bss_size,  ret_cnt); ret_cnt += 4;
				}
				break;
			}
			case MsgGetStack:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (VMManager::GetState() != VMState::Paused)
					goto error;
				// count(4) + up to MAX_DEPTH × 16 bytes per frame
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 4 + 1024 * 16, buf_size)) [[unlikely]]
					goto error;

				std::vector<MipsStackWalk::StackFrame> frames;
				Host::RunOnCPUThread([&]() {
					for (const auto& t : r5900Debug.GetThreadList())
					{
						if (t->Status() == ThreadStatus::THS_RUN)
						{
							frames = MipsStackWalk::Walk(
								&r5900Debug,
								r5900Debug.getPC(),
								static_cast<u32>(r5900Debug.getRegister(0, 31)),
								static_cast<u32>(r5900Debug.getRegister(0, 29)),
								t->EntryPoint(),
								t->StackTop());
							break;
						}
					}
				}, true);

				const u32 count = static_cast<u32>(frames.size());
				ToResultVector(ret_buffer, count, ret_cnt);
				ret_cnt += 4;
				for (const auto& f : frames)
				{
					ToResultVector(ret_buffer, f.entry,     ret_cnt); ret_cnt += 4;
					ToResultVector(ret_buffer, f.pc,        ret_cnt); ret_cnt += 4;
					ToResultVector(ret_buffer, f.sp,        ret_cnt); ret_cnt += 4;
					ToResultVector(ret_buffer, static_cast<u32>(f.stackSize), ret_cnt); ret_cnt += 4;
				}
				break;
			}
			case MsgStepInto:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!r5900Debug.isAlive() || !r5900Debug.isCpuPaused())
					goto error;

				const u32 pc   = r5900Debug.getPC();
				const MIPSAnalyst::MipsOpcodeInfo info = MIPSAnalyst::GetOpcodeInfo(&r5900Debug, pc);
				u32 bp_addr = pc + 4;
				if (info.isBranch)
				{
					if (!info.isConditional)
						bp_addr = info.branchTarget;
					else
						bp_addr = info.conditionMet ? info.branchTarget : pc + 8;
				}
				if (info.isSyscall)
					bp_addr = info.branchTarget;

				Host::RunOnCPUThread([pc, bp_addr]() {
					CBreakPoints::SetSkipFirst(BREAKPOINT_EE, pc);
					CBreakPoints::AddBreakPoint(BREAKPOINT_EE, bp_addr, true, true, true);
					r5900Debug.resumeCpu();
				});
				break;
			}
			case MsgStepOver:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!r5900Debug.isAlive() || !r5900Debug.isCpuPaused())
					goto error;

				const u32 pc   = r5900Debug.getPC();
				const MIPSAnalyst::MipsOpcodeInfo info = MIPSAnalyst::GetOpcodeInfo(&r5900Debug, pc);
				u32 bp_addr = pc + 4;
				if (info.isBranch)
				{
					if (!info.isConditional)
					{
						bp_addr = info.isLinkedBranch ? pc + 8 : info.branchTarget;
					}
					else
					{
						bp_addr = info.conditionMet ? info.branchTarget : pc + 8;
					}
				}

				Host::RunOnCPUThread([pc, bp_addr]() {
					CBreakPoints::SetSkipFirst(BREAKPOINT_EE, pc);
					CBreakPoints::AddBreakPoint(BREAKPOINT_EE, bp_addr, true, true, true);
					r5900Debug.resumeCpu();
				});
				break;
			}
			case MsgStepOut:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!r5900Debug.isAlive() || !r5900Debug.isCpuPaused())
					goto error;

				std::vector<MipsStackWalk::StackFrame> frames;
				const u32 current_pc = r5900Debug.getPC();
				for (const auto& t : r5900Debug.GetThreadList())
				{
					if (t->Status() == ThreadStatus::THS_RUN)
					{
						frames = MipsStackWalk::Walk(
							&r5900Debug,
							current_pc,
							static_cast<u32>(r5900Debug.getRegister(0, 31)),
							static_cast<u32>(r5900Debug.getRegister(0, 29)),
							t->EntryPoint(),
							t->StackTop());
						break;
					}
				}
				if (frames.size() < 2)
					goto error;

				{
					const u32 bp_addr = frames[1].pc;
					Host::RunOnCPUThread([current_pc, bp_addr]() {
						CBreakPoints::SetSkipFirst(BREAKPOINT_EE, current_pc);
						CBreakPoints::AddBreakPoint(BREAKPOINT_EE, bp_addr, true, true, true);
						r5900Debug.resumeCpu();
					});
				}
				break;
			}
			case MsgGetSymbol:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 4, ret_cnt, 4 + 256, buf_size)) [[unlikely]]
					goto error;

				const u32 addr = FromSpan<u32>(buf, buf_cnt);
				FunctionInfo info;
				Host::RunOnCPUThread([&]() {
					info = R5900SymbolGuardian.FunctionOverlappingAddress(addr);
				}, true);

				if (info.name.empty())
					goto error;

				const u32 sym_start = info.address.get_or_zero();
				ToResultVector(ret_buffer, sym_start, ret_cnt);
				ret_cnt += 4;
				const u32 name_len = static_cast<u32>(info.name.size()) + 1;
				memcpy(&ret_buffer[ret_cnt], info.name.c_str(), name_len);
				ret_cnt += name_len;
				buf_cnt += 4;
				break;
			}
			case MsgSaveStateFile:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 2, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				const u16 path_len = FromSpan<u16>(buf, buf_cnt);
				if (path_len == 0 || path_len > 512)
					goto error;
				if (!SafetyChecks(buf_cnt, 2 + path_len, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				std::string path(reinterpret_cast<const char*>(&buf[buf_cnt + 2]), path_len);
				buf_cnt += 2 + path_len;
				Host::RunOnCPUThread([path = std::move(path)]() {
					VMManager::SaveState(path.c_str(), true, false,
						[](const std::string&) {});
				});
				break;
			}
			case MsgLoadStateFile:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 2, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				const u16 path_len = FromSpan<u16>(buf, buf_cnt);
				if (path_len == 0 || path_len > 512)
					goto error;
				if (!SafetyChecks(buf_cnt, 2 + path_len, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				std::string path(reinterpret_cast<const char*>(&buf[buf_cnt + 2]), path_len);
				buf_cnt += 2 + path_len;
				bool ok = false;
				Host::RunOnCPUThread([&ok, path = std::move(path)]() {
					Error err;
					ok = VMManager::LoadState(path.c_str(), &err);
					if (!ok)
						Console.WriteLn(Color_Red, "PINE: LoadStateFile '%s' failed: %s", path.c_str(), err.GetDescription().c_str());
				}, true);
				if (!ok)
					goto error;
				break;
			}
			case MsgReset:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				Host::RunOnCPUThread([]() { VMManager::Reset(); });
				break;
			}
			case MsgFrameAdvance:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 1, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				const u8 num_frames = FromSpan<u8>(buf, buf_cnt);
				buf_cnt += 1;
				if (num_frames == 0)
					goto error;

				Host::RunOnCPUThread([num_frames]() { VMManager::FrameAdvance(num_frames); });
				break;
			}
			case MsgGetFPS:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 4, buf_size)) [[unlikely]]
					goto error;

				const float fps = VMManager::GetFrameRate();
				ToResultVector(ret_buffer, fps, ret_cnt);
				ret_cnt += 4;
				break;
			}
			case MsgSetLimiterMode:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 1, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				const u8 mode = FromSpan<u8>(buf, buf_cnt);
				buf_cnt += 1;
				if (mode > static_cast<u8>(LimiterModeType::Unlimited))
					goto error;

				VMManager::SetLimiterMode(static_cast<LimiterModeType>(mode));
				break;
			}
			case MsgListBreakpoints:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 1, ret_cnt, 4 + MAX_BREAKPOINTS_RESPONSE * BREAKPOINT_ENTRY_SIZE, buf_size)) [[unlikely]]
					goto error;

				const u8 cpu_sel = FromSpan<u8>(buf, buf_cnt);
				buf_cnt += 1;

				std::vector<BreakPoint> bps;
				Host::RunOnCPUThread([&]() {
					if (cpu_sel == 0 || cpu_sel == 0xFF)
					{
						auto ee = CBreakPoints::GetBreakpoints(BREAKPOINT_EE, false);
						bps.insert(bps.end(), ee.begin(), ee.end());
					}
					if (cpu_sel == 1 || cpu_sel == 0xFF)
					{
						auto iop = CBreakPoints::GetBreakpoints(BREAKPOINT_IOP, false);
						bps.insert(bps.end(), iop.begin(), iop.end());
					}
				}, true);

				const u32 count = static_cast<u32>(bps.size());
				ToResultVector(ret_buffer, count, ret_cnt);
				ret_cnt += 4;
				for (const auto& bp : bps)
				{
					ToResultVector(ret_buffer, bp.addr, ret_cnt);    ret_cnt += 4;
					ret_buffer[ret_cnt++] = bp.enabled ? 1 : 0;
					ret_buffer[ret_cnt++] = static_cast<u8>(bp.cpu);
				}
				break;
			}
			case MsgDisassemble:
			{
				if (!VMManager::HasValidVM())
					goto error;
				// cpu(1) + addr(4) + count(2) = 7; reply up to MAX_DISASSEMBLY_LINES × MAX_DISASSEMBLY_LINE_SIZE
				if (!SafetyChecks(buf_cnt, 7, ret_cnt, 2 + MAX_DISASSEMBLY_LINES * MAX_DISASSEMBLY_LINE_SIZE, buf_size)) [[unlikely]]
					goto error;

				const u8  cpu_sel = FromSpan<u8>(buf, buf_cnt);
				const u32 addr    = FromSpan<u32>(buf, buf_cnt + 1);
				const u16 count   = std::min<u16>(FromSpan<u16>(buf, buf_cnt + 5), MAX_DISASSEMBLY_LINES);
				buf_cnt += 7;

				DebugInterface& dbg = (cpu_sel == 0) ? static_cast<DebugInterface&>(r5900Debug)
				                                     : static_cast<DebugInterface&>(r3000Debug);

				std::vector<std::pair<u32, std::string>> lines;
				lines.reserve(count);
				Host::RunOnCPUThread([&]() {
					u32 cur = addr;
					for (u16 i = 0; i < count; ++i)
					{
						std::string text = dbg.disasm(cur, true);
						lines.emplace_back(cur, std::move(text));
						cur += 4;
					}
				}, true);

				const u16 returned = static_cast<u16>(lines.size());
				ToResultVector(ret_buffer, returned, ret_cnt);
				ret_cnt += 2;
				for (const auto& [line_addr, text] : lines)
				{
					ToResultVector(ret_buffer, line_addr, ret_cnt); ret_cnt += 4;
					const u8 text_len = static_cast<u8>(std::min(text.size(), static_cast<size_t>(255)));
					ret_buffer[ret_cnt++] = text_len;
					memcpy(&ret_buffer[ret_cnt], text.c_str(), text_len);
					ret_cnt += text_len;
				}
				break;
			}
			case MsgListFunctions:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 6, ret_cnt, 4 + 2 + MAX_SYMBOL_BATCH * MAX_SYMBOL_ENTRY_SIZE, buf_size)) [[unlikely]]
					goto error;

				const u32 offset    = FromSpan<u32>(buf, buf_cnt);
				const u16 max_count = FromSpan<u16>(buf, buf_cnt + 4);
				buf_cnt += 6;

				u32 total = 0;
				std::vector<std::tuple<u32, u32, std::string>> funcs; // addr, size, name
				R5900SymbolGuardian.Read([&](const ccc::SymbolDatabase& db) {
					total = static_cast<u32>(db.functions.size());
					u32 idx = 0;
					for (const ccc::Function& f : db.functions)
					{
						if (idx++ < offset)
							continue;
						if (funcs.size() >= max_count)
							break;
						funcs.emplace_back(f.address().get_or_zero(), f.size(), f.name());
					}
				});

				ToResultVector(ret_buffer, total, ret_cnt);
				ret_cnt += 4;
				const u16 returned = static_cast<u16>(funcs.size());
				ToResultVector(ret_buffer, returned, ret_cnt);
				ret_cnt += 2;
				for (const auto& [faddr, fsize, fname] : funcs)
				{
					ToResultVector(ret_buffer, faddr, ret_cnt); ret_cnt += 4;
					ToResultVector(ret_buffer, fsize, ret_cnt); ret_cnt += 4;
					const u8 name_len = static_cast<u8>(std::min(fname.size(), static_cast<size_t>(255)));
					ret_buffer[ret_cnt++] = name_len;
					memcpy(&ret_buffer[ret_cnt], fname.c_str(), name_len);
					ret_cnt += name_len;
				}
				break;
			}
			case MsgGetSymbolByName:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 1, ret_cnt, 4 + 4, buf_size)) [[unlikely]]
					goto error;

				const u8 name_len = FromSpan<u8>(buf, buf_cnt);
				if (name_len == 0)
					goto error;
				if (!SafetyChecks(buf_cnt, 1 + name_len, ret_cnt, 4 + 4, buf_size)) [[unlikely]]
					goto error;

				const std::string sym_name(reinterpret_cast<const char*>(&buf[buf_cnt + 1]), name_len);
				buf_cnt += 1 + name_len;

				u32 found_addr = 0;
				u32 found_size = 0;
				bool found = false;
				R5900SymbolGuardian.Read([&](const ccc::SymbolDatabase& db) {
					const ccc::Symbol* sym = db.symbol_with_name(sym_name);
					if (sym && sym->address().valid())
					{
						found      = true;
						found_addr = sym->address().value;
						found_size = sym->size();
					}
				});
				if (!found)
					goto error;

				ToResultVector(ret_buffer, found_addr, ret_cnt); ret_cnt += 4;
				ToResultVector(ret_buffer, found_size, ret_cnt); ret_cnt += 4;
				break;
			}
			case MsgListGlobals:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 6, ret_cnt, 4 + 2 + MAX_SYMBOL_BATCH * MAX_SYMBOL_ENTRY_SIZE, buf_size)) [[unlikely]]
					goto error;

				const u32 offset    = FromSpan<u32>(buf, buf_cnt);
				const u16 max_count = FromSpan<u16>(buf, buf_cnt + 4);
				buf_cnt += 6;

				u32 total = 0;
				std::vector<std::tuple<u32, u32, std::string>> globs; // addr, size, name
				R5900SymbolGuardian.Read([&](const ccc::SymbolDatabase& db) {
					total = static_cast<u32>(db.global_variables.size());
					u32 idx = 0;
					for (const ccc::GlobalVariable& gv : db.global_variables)
					{
						if (idx++ < offset)
							continue;
						if (globs.size() >= max_count)
							break;
						globs.emplace_back(gv.address().get_or_zero(), gv.size(), gv.name());
					}
				});

				ToResultVector(ret_buffer, total, ret_cnt);
				ret_cnt += 4;
				const u16 returned = static_cast<u16>(globs.size());
				ToResultVector(ret_buffer, returned, ret_cnt);
				ret_cnt += 2;
				for (const auto& [gaddr, gsize, gname] : globs)
				{
					ToResultVector(ret_buffer, gaddr, ret_cnt); ret_cnt += 4;
					ToResultVector(ret_buffer, gsize, ret_cnt); ret_cnt += 4;
					const u8 name_len = static_cast<u8>(std::min(gname.size(), static_cast<size_t>(255)));
					ret_buffer[ret_cnt++] = name_len;
					memcpy(&ret_buffer[ret_cnt], gname.c_str(), name_len);
					ret_cnt += name_len;
				}
				break;
			}
			case MsgGetLocals:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 4, ret_cnt, 4 + MAX_LOCALS_RESPONSE * MAX_LOCAL_ENTRY_SIZE, buf_size)) [[unlikely]]
					goto error;

				const u32 query_addr = FromSpan<u32>(buf, buf_cnt);
				buf_cnt += 4;

				struct LocalEntry { u8 storage_type; s32 value; std::string name; };
				std::vector<LocalEntry> locals;

				R5900SymbolGuardian.Read([&](const ccc::SymbolDatabase& db) {
					const ccc::Function* func = db.functions.symbol_overlapping_address(query_addr);
					if (!func)
						return;

					// Collect parameter variables
					if (func->parameter_variables().has_value())
					{
						for (const auto& pv_handle : *func->parameter_variables())
						{
							const ccc::ParameterVariable* pv = db.parameter_variables.symbol_from_handle(pv_handle);
							if (!pv)
								continue;
							LocalEntry e;
							e.name = pv->name();
							std::visit([&e](const auto& s) {
								using T = std::decay_t<decltype(s)>;
								if constexpr (std::is_same_v<T, ccc::RegisterStorage>)
								{
									e.storage_type = 1;
									e.value = static_cast<s32>(s.dbx_register_number);
								}
								else if constexpr (std::is_same_v<T, ccc::StackStorage>)
								{
									e.storage_type = 2;
									e.value = s.stack_pointer_offset;
								}
							}, pv->storage);
							locals.push_back(std::move(e));
						}
					}

					// Collect local variables
					if (func->local_variables().has_value())
					{
						for (const auto& lv_handle : *func->local_variables())
						{
							const ccc::LocalVariable* lv = db.local_variables.symbol_from_handle(lv_handle);
							if (!lv)
								continue;
							LocalEntry e;
							e.name = lv->name();
							std::visit([&e](const auto& s) {
								using T = std::decay_t<decltype(s)>;
								if constexpr (std::is_same_v<T, ccc::GlobalStorage>)
								{
									e.storage_type = 0;
									e.value = 0;
								}
								else if constexpr (std::is_same_v<T, ccc::RegisterStorage>)
								{
									e.storage_type = 1;
									e.value = static_cast<s32>(s.dbx_register_number);
								}
								else if constexpr (std::is_same_v<T, ccc::StackStorage>)
								{
									e.storage_type = 2;
									e.value = s.stack_pointer_offset;
								}
							}, lv->storage);
							locals.push_back(std::move(e));
						}
					}
				});

				if (locals.empty())
					goto error;

				const u32 count = static_cast<u32>(locals.size());
				ToResultVector(ret_buffer, count, ret_cnt);
				ret_cnt += 4;
				for (const auto& loc : locals)
				{
					ret_buffer[ret_cnt++] = loc.storage_type;
					ToResultVector(ret_buffer, loc.value, ret_cnt); ret_cnt += 4;
					const u8 name_len = static_cast<u8>(std::min(loc.name.size(), static_cast<size_t>(255)));
					ret_buffer[ret_cnt++] = name_len;
					memcpy(&ret_buffer[ret_cnt], loc.name.c_str(), name_len);
					ret_cnt += name_len;
				}
				break;
			}
			case MsgAddWatch:
			{
				if (!VMManager::HasValidVM())
					goto error;
				// cpu(1) + start(4) + end(4) + cond(1) = 10 bytes
				if (!SafetyChecks(buf_cnt, 10, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				const u8  cpu_sel  = FromSpan<u8>(buf, buf_cnt);
				const u32 start    = FromSpan<u32>(buf, buf_cnt + 1);
				const u32 end      = FromSpan<u32>(buf, buf_cnt + 5);
				const u8  cond_raw = FromSpan<u8>(buf, buf_cnt + 9);
				buf_cnt += 10;

				if (cpu_sel > 1)
					goto error;
				if (cond_raw == 0 || (cond_raw & ~static_cast<u8>(MEMCHECK_READWRITE)) != 0)
					goto error; // cond must be 0x01 (read), 0x02 (write), or 0x03 (read+write)
				if (end <= start)
					goto error;

				const BreakPointCpu cpu  = (cpu_sel == 0) ? BREAKPOINT_EE : BREAKPOINT_IOP;
				const auto          cond = static_cast<MemCheckCondition>(cond_raw);
				Host::RunOnCPUThread([cpu, start, end, cond]() {
					CBreakPoints::AddMemCheck(cpu, start, end, cond, MEMCHECK_BREAK);
				}, true);
				break;
			}
			case MsgRemoveWatch:
			{
				if (!VMManager::HasValidVM())
					goto error;
				// cpu(1) + start(4) + end(4) = 9 bytes
				if (!SafetyChecks(buf_cnt, 9, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				const u8  cpu_sel = FromSpan<u8>(buf, buf_cnt);
				const u32 start   = FromSpan<u32>(buf, buf_cnt + 1);
				const u32 end     = FromSpan<u32>(buf, buf_cnt + 5);
				buf_cnt += 9;

				if (cpu_sel > 1)
					goto error;
				if (end <= start)
					goto error;

				const BreakPointCpu cpu = (cpu_sel == 0) ? BREAKPOINT_EE : BREAKPOINT_IOP;
				bool found = false;
				Host::RunOnCPUThread([cpu, start, end, &found]() {
					const auto checks = CBreakPoints::GetMemChecks(cpu);
					for (const auto& mc : checks)
					{
						if (mc.start == start && mc.end == end)
						{
							found = true;
							break;
						}
					}
					if (found)
						CBreakPoints::RemoveMemCheck(cpu, start, end);
				}, true);
				if (!found)
					goto error;
				break;
			}
			case MsgListWatches:
			{
				if (!VMManager::HasValidVM())
					goto error;
				// cpu_sel(1); reply: count(4) + up to MAX_WATCHES_RESPONSE × WATCH_ENTRY_SIZE
				if (!SafetyChecks(buf_cnt, 1, ret_cnt, 4 + MAX_WATCHES_RESPONSE * WATCH_ENTRY_SIZE, buf_size)) [[unlikely]]
					goto error;

				const u8 cpu_sel = FromSpan<u8>(buf, buf_cnt);
				buf_cnt += 1;

				std::vector<MemCheck> watches;
				Host::RunOnCPUThread([&]() {
					if (cpu_sel == 0 || cpu_sel == 0xFF)
					{
						auto ee = CBreakPoints::GetMemChecks(BREAKPOINT_EE);
						watches.insert(watches.end(), ee.begin(), ee.end());
					}
					if (cpu_sel == 1 || cpu_sel == 0xFF)
					{
						auto iop = CBreakPoints::GetMemChecks(BREAKPOINT_IOP);
						watches.insert(watches.end(), iop.begin(), iop.end());
					}
				}, true);

				const u32 count = static_cast<u32>(watches.size());
				ToResultVector(ret_buffer, count, ret_cnt);
				ret_cnt += 4;
				for (const auto& w : watches)
				{
					ToResultVector(ret_buffer, w.start, ret_cnt);         ret_cnt += 4;
					ToResultVector(ret_buffer, w.end, ret_cnt);           ret_cnt += 4;
					ret_buffer[ret_cnt++] = static_cast<u8>(w.memCond);
					ret_buffer[ret_cnt++] = static_cast<u8>(w.result);
					ret_buffer[ret_cnt++] = static_cast<u8>(w.cpu);
				}
				break;
			}
			case MsgClearAllWatches:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				Host::RunOnCPUThread([]() { CBreakPoints::ClearAllMemChecks(); }, true);
				break;
			}
			case MsgSaveSnapshot:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				MTGS::RunOnGSThread([]() { GSQueueSnapshot(std::string(), 0); });
				break;
			}
			case MsgPadGetState:
			{
				// Request:  pad (u8)
				// Reply OK: buttons (u32 LE) + lx ly rx ry (4 × u8) + pressures[16] (16 × u8) = 24 bytes
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 1, ret_cnt, 4 + 4 + 16, buf_size)) [[unlikely]]
					goto error;

				const u8 pad_slot = FromSpan<u8>(buf, buf_cnt);
				buf_cnt += 1;
				if (pad_slot >= Pad::NUM_CONTROLLER_PORTS)
					goto error;

				PadBase* pad = Pad::GetPad(pad_slot);
				if (!pad || pad->GetType() == Pad::ControllerType::NotConnected)
					goto error;

				{
					const u32 buttons = pad->GetButtons();
					ToResultVector(ret_buffer, buttons, ret_cnt);
					ret_cnt += 4;

					const auto [lx, ly] = pad->GetRawLeftAnalog();
					const auto [rx, ry] = pad->GetRawRightAnalog();
					ret_buffer[ret_cnt++] = lx;
					ret_buffer[ret_cnt++] = ly;
					ret_buffer[ret_cnt++] = rx;
					ret_buffer[ret_cnt++] = ry;

					// Pressure values for buttons 0–15 (PAD_UP through PAD_R3).
					for (u32 i = 0; i < 16; i++)
					{
						ret_buffer[ret_cnt++] = pad->GetPressure(i);
					}
				}
				break;
			}
			case MsgPadSetButton:
			{
				// Request: pad (u8) + button_index (u8) + value (f32 LE)
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 1 + 1 + 4, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				const u8 pad_slot    = FromSpan<u8>(buf, buf_cnt);
				const u8 button      = FromSpan<u8>(buf, buf_cnt + 1);
				const float value    = std::clamp(FromSpan<float>(buf, buf_cnt + 2), 0.0f, 1.0f);
				buf_cnt += 1 + 1 + 4;

				if (pad_slot >= Pad::NUM_CONTROLLER_PORTS)
					goto error;
				if (button >= PadDualshock2::Inputs::LENGTH)
					goto error;

				Pad::SetControllerState(pad_slot, button, value);
				break;
			}
			case MsgPadSetAnalog:
			{
				// Request: pad (u8) + lx (u8) + ly (u8) + rx (u8) + ry (u8)
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 1 + 4, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				const u8 pad_slot = FromSpan<u8>(buf, buf_cnt);
				const u8 lx       = FromSpan<u8>(buf, buf_cnt + 1);
				const u8 ly       = FromSpan<u8>(buf, buf_cnt + 2);
				const u8 rx       = FromSpan<u8>(buf, buf_cnt + 3);
				const u8 ry       = FromSpan<u8>(buf, buf_cnt + 4);
				buf_cnt += 1 + 4;

				if (pad_slot >= Pad::NUM_CONTROLLER_PORTS)
					goto error;

				{
					PadBase* pad = Pad::GetPad(pad_slot);
					if (!pad || pad->GetType() == Pad::ControllerType::NotConnected)
						goto error;
					pad->SetRawAnalogs({lx, ly}, {rx, ry});
				}
				break;
			}
			case MsgPadSetState:
			{
				// Request: pad (u8) + buttons (u32 LE) + lx ly rx ry (4 × u8) + pressures[16] (16 × u8)
				// Total args: 1 + 4 + 4 + 16 = 25 bytes
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 1 + 4 + 4 + 16, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				const u8 pad_slot = FromSpan<u8>(buf, buf_cnt);
				buf_cnt += 1;
				if (pad_slot >= Pad::NUM_CONTROLLER_PORTS)
					goto error;

				{
					PadBase* pad = Pad::GetPad(pad_slot);
					if (!pad || pad->GetType() == Pad::ControllerType::NotConnected)
						goto error;

					const u8 lx = FromSpan<u8>(buf, buf_cnt + 4);
					const u8 ly = FromSpan<u8>(buf, buf_cnt + 5);
					const u8 rx = FromSpan<u8>(buf, buf_cnt + 6);
					const u8 ry = FromSpan<u8>(buf, buf_cnt + 7);
					pad->SetRawAnalogs({lx, ly}, {rx, ry});

					// buttons bitmask: bit clear means pressed (PS2 convention).
					// We derive pressed state and pressure from the payload rather than
					// the bitmask, matching PadData::OverrideActualController().
					for (u32 i = 0; i < 16; i++)
					{
						const u8 pressure = FromSpan<u8>(buf, buf_cnt + 8 + i);
						// A button is considered pressed when pressure > 0.
						pad->SetRawPressureButton(i, {pressure > 0, pressure});
					}
					buf_cnt += 4 + 4 + 16;
				}
				break;
			}
			case MsgPadGetType:
			{
				// Request:  pad (u8)
				// Reply OK: controller_type (u8)
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 1, ret_cnt, 1, buf_size)) [[unlikely]]
					goto error;

				const u8 pad_slot = FromSpan<u8>(buf, buf_cnt);
				buf_cnt += 1;
				if (pad_slot >= Pad::NUM_CONTROLLER_PORTS)
					goto error;

				{
					PadBase* pad = Pad::GetPad(pad_slot);
					if (!pad)
						goto error;
					ret_buffer[ret_cnt++] = static_cast<u8>(pad->GetType());
				}
				break;
			}
			case MsgNetGetStatus:
			{
				// Request:  (none)
				// Reply OK: enabled (u8) + adapter_type (u8) + ps2_mac (6 bytes) + ps2_ip (4 bytes)
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 1 + 1 + 6 + 4, buf_size)) [[unlikely]]
					goto error;

				{
					const NetAdapterStatus s = NetGetAdapterStatus();
					ret_buffer[ret_cnt++] = s.enabled ? 1u : 0u;
					ret_buffer[ret_cnt++] = static_cast<u8>(s.apiType);
					std::memcpy(&ret_buffer[ret_cnt], s.ps2MAC.bytes, 6);
					ret_cnt += 6;
					std::memcpy(&ret_buffer[ret_cnt], s.ps2IP.bytes, 4);
					ret_cnt += 4;
				}
				break;
			}
			case MsgNetCapture:
			{
				// Request:  enable (u8) + direction (u8: 0=both,1=TX-only,2=RX-only) + max_queue (u16 LE)
				// Reply OK: (none)
				if (!SafetyChecks(buf_cnt, 1 + 1 + 2, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				{
					const u8 enable        = FromSpan<u8>(buf, buf_cnt);
					const u8 direction     = FromSpan<u8>(buf, buf_cnt + 1);
					const u16 max_queue    = FromSpan<u16>(buf, buf_cnt + 2);
					buf_cnt += 1 + 1 + 2;

					if (enable)
						DEV9::g_netCapture.Enable(direction, max_queue);
					else
						DEV9::g_netCapture.Disable();
				}
				break;
			}
			case MsgNetRecvCapture:
			{
				// Request:  max_packets (u16 LE)
				// Reply OK: count (u16 LE) + per packet: direction (u8) + timestamp_us (u64 LE)
				//           + length (u16 LE) + data (length bytes)
				if (!SafetyChecks(buf_cnt, 2, ret_cnt, 2, buf_size)) [[unlikely]]
					goto error;

				{
					const u16 max_packets = FromSpan<u16>(buf, buf_cnt);
					buf_cnt += 2;

					auto captured = DEV9::g_netCapture.Drain(max_packets);

					// Write placeholder count; fill in after iterating.
					const u32 count_offset = ret_cnt;
					ToResultVector(ret_buffer, static_cast<u16>(0), ret_cnt);
					ret_cnt += 2;

					u16 written = 0;
					for (const auto& cp : captured)
					{
						// Per-packet: 1 + 8 + 2 + cp.packet.size bytes
						const int pkt_sz = cp.packet.size;
						const int overhead = 1 + 8 + 2;
						if (!SafetyChecks(0, 0, ret_cnt, overhead + pkt_sz, buf_size)) [[unlikely]]
							break;

						ret_buffer[ret_cnt++] = static_cast<u8>(cp.direction);
						ToResultVector(ret_buffer, cp.timestamp_us, ret_cnt);
						ret_cnt += 8;
						ToResultVector(ret_buffer, static_cast<u16>(pkt_sz), ret_cnt);
						ret_cnt += 2;
						std::memcpy(&ret_buffer[ret_cnt], cp.packet.buffer, static_cast<size_t>(pkt_sz));
						ret_cnt += static_cast<u32>(pkt_sz);
						written++;
					}

					// Back-fill the actual count.
					ToResultVector(ret_buffer, written, count_offset);
				}
				break;
			}
			case MsgNetInject:
			{
				// Request:  length (u16 LE) + data (length bytes)
				// Reply OK: success (u8)
				if (!SafetyChecks(buf_cnt, 2, ret_cnt, 1, buf_size)) [[unlikely]]
					goto error;

				{
					const u16 length = FromSpan<u16>(buf, buf_cnt);
					buf_cnt += 2;

					if (!SafetyChecks(buf_cnt, static_cast<int>(length), ret_cnt, 0, buf_size)) [[unlikely]]
						goto error;

					const bool ok = NetInjectRx(buf.data() + buf_cnt, static_cast<int>(length));
					buf_cnt += length;
					ret_buffer[ret_cnt++] = ok ? 1u : 0u;
				}
				break;
			}
			case MsgNetInjectTx:
			{
				// Request:  length (u16 LE) + data (length bytes)
				// Reply OK: success (u8)
				if (!SafetyChecks(buf_cnt, 2, ret_cnt, 1, buf_size)) [[unlikely]]
					goto error;

				{
					const u16 length = FromSpan<u16>(buf, buf_cnt);
					buf_cnt += 2;

					if (!SafetyChecks(buf_cnt, static_cast<int>(length), ret_cnt, 0, buf_size)) [[unlikely]]
						goto error;

					const bool ok = NetInjectTx(buf.data() + buf_cnt, static_cast<int>(length));
					buf_cnt += length;
					ret_buffer[ret_cnt++] = ok ? 1u : 0u;
				}
				break;
			}
			case MsgNetDisassemble:
			{
				// Request:  length (u16 LE) + data (length bytes)
				// Reply OK: text_len (u16 LE) + text (text_len bytes, not null-terminated)
				if (!SafetyChecks(buf_cnt, 2, ret_cnt, 2, buf_size)) [[unlikely]]
					goto error;

				{
					const u16 length = FromSpan<u16>(buf, buf_cnt);
					buf_cnt += 2;

					if (!SafetyChecks(buf_cnt, static_cast<int>(length), ret_cnt, 0, buf_size)) [[unlikely]]
						goto error;

					const std::string text = DEV9::NetCapture::DisassemblePacket(
						buf.data() + buf_cnt, static_cast<int>(length));
					buf_cnt += length;

					const u16 text_len = static_cast<u16>(
						std::min<size_t>(text.size(), 0xFFFFu));
					if (!SafetyChecks(0, 0, ret_cnt, 2 + text_len, buf_size)) [[unlikely]]
						goto error;

					ToResultVector(ret_buffer, text_len, ret_cnt);
					ret_cnt += 2;
					std::memcpy(&ret_buffer[ret_cnt], text.data(), text_len);
					ret_cnt += text_len;
				}
				break;
			}
			case MsgNetSetFilter:
			{
				// Request:  filter_type (u8) + [type-specific args]
				//   type 0: no extra args (clear filter)
				//   type 1: protocol (u8)     — 0=all,1=TCP,2=UDP,3=ICMP,4=ARP,5=DNS,6=DHCP
				//   type 2: port (u16 LE)
				//   type 3: ip (4 bytes)
				// Reply OK: (none)
				if (!SafetyChecks(buf_cnt, 1, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;

				{
					DEV9::NetCaptureFilter f{};
					f.type = FromSpan<u8>(buf, buf_cnt);
					buf_cnt += 1;

					if (f.type == 1)
					{
						if (!SafetyChecks(buf_cnt, 1, ret_cnt, 0, buf_size)) [[unlikely]]
							goto error;
						f.protocol = FromSpan<u8>(buf, buf_cnt);
						buf_cnt += 1;
					}
					else if (f.type == 2)
					{
						if (!SafetyChecks(buf_cnt, 2, ret_cnt, 0, buf_size)) [[unlikely]]
							goto error;
						f.port = FromSpan<u16>(buf, buf_cnt);
						buf_cnt += 2;
					}
					else if (f.type == 3)
					{
						if (!SafetyChecks(buf_cnt, 4, ret_cnt, 0, buf_size)) [[unlikely]]
							goto error;
						std::memcpy(f.ip, buf.data() + buf_cnt, 4);
						buf_cnt += 4;
					}

					DEV9::g_netCapture.SetFilter(f);
				}
				break;
			}
			case MsgNetGetStats:
			{
				// Request:  (none)
				// Reply OK: tx_packets (u32 LE) + rx_packets (u32 LE)
				//           + tx_bytes (u64 LE) + rx_bytes (u64 LE)
				//           + capture_queue_size (u16 LE) + dropped_count (u32 LE)
				// tx_packets(4) + rx_packets(4) + tx_bytes(8) + rx_bytes(8) + capture_queue_size(2) + dropped_count(4)
				constexpr int kStatSize = 4 + 4 + 8 + 8 + 2 + 4;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, kStatSize, buf_size)) [[unlikely]]
					goto error;

				{
					const DEV9::NetCaptureStats stats = DEV9::g_netCapture.GetStats();
					ToResultVector(ret_buffer, stats.tx_packets, ret_cnt);
					ret_cnt += 4;
					ToResultVector(ret_buffer, stats.rx_packets, ret_cnt);
					ret_cnt += 4;
					ToResultVector(ret_buffer, stats.tx_bytes, ret_cnt);
					ret_cnt += 8;
					ToResultVector(ret_buffer, stats.rx_bytes, ret_cnt);
					ret_cnt += 8;
					ToResultVector(ret_buffer, stats.capture_queue_size, ret_cnt);
					ret_cnt += 2;
					ToResultVector(ret_buffer, stats.dropped_count, ret_cnt);
					ret_cnt += 4;
				}
				break;
			}
			case MsgGSStreamGetPort:
			{
				// Request:  (none)
				// Reply OK: port (u16 LE)  — 0 when the stream listener is not initialized.
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 2, buf_size)) [[unlikely]]
					goto error;

				const int port = GSStreamServer::IsInitialized() ? GSStreamServer::GetPort() : 0;
				const u16 port_u16 = (port > 0 && port <= 0xFFFF) ? static_cast<u16>(port) : 0;
				ToResultVector(ret_buffer, port_u16, ret_cnt);
				ret_cnt += 2;
				break;
			}
			case MsgGSStreamGetStatus:
			{
				// Request:  (none)
				// Reply OK: active (u8) + port (u16 LE) + subscribers (u32 LE)
				//           + frames_delivered (u32 LE) + frames_dropped (u32 LE)
				constexpr int kStatusSize = 1 + 2 + 4 + 4 + 4;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, kStatusSize, buf_size)) [[unlikely]]
					goto error;

				const GSStreamServer::Stats stats = GSStreamServer::GetStats();
				const u8 active = GSStreamServer::IsInitialized() ? 1 : 0;
				const u16 port_u16 = stats.bound_port <= 0xFFFFu ? static_cast<u16>(stats.bound_port) : 0;
				ToResultVector(ret_buffer, active, ret_cnt);
				ret_cnt += 1;
				ToResultVector(ret_buffer, port_u16, ret_cnt);
				ret_cnt += 2;
				ToResultVector(ret_buffer, stats.subscribers, ret_cnt);
				ret_cnt += 4;
				ToResultVector(ret_buffer, stats.frames_delivered, ret_cnt);
				ret_cnt += 4;
				ToResultVector(ret_buffer, stats.frames_dropped, ret_cnt);
				ret_cnt += 4;
				break;
			}
			case MsgGSStreamPing:
			{
				// Request:  (none)
				// Reply OK: (empty) — used by the bridge to confirm the GS stream control plane is wired up.
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;
				break;
			}
			case MsgSetAudioMuted:
			case MsgGetAudioState:
			{
				// Request (0x45): muted (u8, 0 = unmute, 1 = mute)
				// Request (0x46): (none)
				// Reply OK: muted (u8) + muted_in_settings (u8)
				//   muted is the effective state; muted_in_settings means the user's
				//   "Mute All Sound" setting is on, which blocks unmuting from here
				//   just like it blocks the hotkey.
				const bool set = (buf[buf_cnt - 1] == MsgSetAudioMuted);
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, set ? 1 : 0, ret_cnt, 2, buf_size)) [[unlikely]]
					goto error;

				const u8 want = set ? FromSpan<u8>(buf, buf_cnt) : 0;
				if (set)
					buf_cnt += 1;

				u8 muted = 0;
				u8 muted_in_settings = 0;
				Host::RunOnCPUThread([&]() {
					if (set)
						SPU2::SetOutputMuted(want != 0);
					muted_in_settings = EmuConfig.SPU2.OutputMuted ? 1 : 0;
					muted = (SPU2::IsOutputMuted() || muted_in_settings) ? 1 : 0;
				}, true);

				ret_buffer[ret_cnt++] = muted;
				ret_buffer[ret_cnt++] = muted_in_settings;
				break;
			}
			default:
			{
			error:
				Console.Error("PINE: IPC command 0x%02X failed at buffer offset %u (buf_size=%u). "
				              "The emulator may have no game loaded, the buffer may be malformed, "
				              "or the opcode is unrecognised.",
				              buf_cnt > 0 ? (unsigned)buf[buf_cnt - 1] : 0xFFu, buf_cnt, buf_size);
				return IPCBuffer{5, MakeFailIPC(ret_buffer)};
			}
		}
	}
	return IPCBuffer{(int)ret_cnt, MakeOkIPC(ret_buffer, ret_cnt)};
}
