// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Common.h"
#include "Memory.h"
#include "Elfheader.h"
#include "SysForwardDefs.h"
#include "PINE.h"
#include "VMManager.h"
#include "svnrev.h"

#include <cstdio>
#include <cstdlib>
#include <sys/types.h>
#if _WIN32
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
#define bzero(b, len) (memset((b), '\0', (len)), (void)0)
#include "common/RedtapeWindows.h"
#include <WinSock2.h>
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

#include "fmt/format.h"

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

PINEServer::PINEServer() = default;

PINEServer::~PINEServer()
{
	// Should be shut down by VMManager.
	pxAssert(!IsInitialized());
}

bool PINEServer::Initialize(int slot)
{
	m_end.store(false, std::memory_order_release);
	m_slot = slot;

#ifdef _WIN32
	if (!InitializeWinsock())
	{
		Console.WriteLn(Color_Red, "PINE: Cannot initialize winsock! Shutting down...");
		Deinitialize();
		return false;
	}

	m_sock = socket(AF_INET, SOCK_STREAM, 0);
	if ((m_sock == INVALID_SOCKET) || slot > 65536)
	{
		Console.WriteLn(Color_Red, "PINE: Cannot open socket! Shutting down...");
		Deinitialize();
		return false;
	}

	sockaddr_in server = {};
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // localhost only
	server.sin_port = htons(slot);

	if (bind(m_sock, (struct sockaddr*)&server, sizeof(server)) == SOCKET_ERROR)
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
		m_socket_name = "/tmp/" PINE_EMULATOR_NAME ".sock";
	else
	{
		m_socket_name = runtime_dir;
		m_socket_name += "/" PINE_EMULATOR_NAME ".sock";
	}

	if (slot != PINE_DEFAULT_SLOT)
		m_socket_name += "." + std::to_string(slot);

	struct sockaddr_un server;

	m_sock = socket(AF_UNIX, SOCK_STREAM, 0);
	if (m_sock < 0)
	{
		Console.WriteLn(Color_Red, "PINE: Cannot open socket! Shutting down...");
		Deinitialize();
		return false;
	}
	server.sun_family = AF_UNIX;
	strcpy(server.sun_path, m_socket_name.c_str());

	// we unlink the socket so that when releasing this thread the socket gets
	// freed even if we didn't close correctly the loop
	unlink(m_socket_name.c_str());
	if (bind(m_sock, (struct sockaddr*)&server, sizeof(struct sockaddr_un)))
	{
		Console.WriteLn(Color_Red, "PINE: Error while binding to socket! Shutting down...");
		Deinitialize();
		return false;
	}
#endif

	// maximum queue of 4096 commands before refusing, approximated to the
	// nearest legal value. We do not use SOMAXCONN as windows have this idea
	// that a "reasonable" value is 5, which is not.
	if (listen(m_sock, 4096))
	{
		Console.WriteLn(Color_Red, "PINE: Cannot listen for connections! Shutting down...");
		Deinitialize();
		return false;
	}

	// we allocate once buffers to not have to do mallocs for each IPC
	// request, as malloc is expansive when we optimize for µs.
	m_ret_buffer.resize(MAX_IPC_RETURN_SIZE);
	m_ipc_buffer.resize(MAX_IPC_SIZE);

	// we start the thread
	m_thread = std::thread(&PINEServer::MainLoop, this);

	return true;
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
	m_msgsock = accept(m_sock, 0, 0);
	if (m_msgsock >= 0)
	{
		Console.WriteLn("PINE: New client with FD %d connected.", static_cast<int>(m_msgsock));
		return true;
	}

	// everything else is non recoverable in our scope
	// we also mark as recoverable socket errors where it would block a
	// non blocking socket, even though our socket is blocking, in case
	// we ever have to implement a non blocking socket.
#ifdef _WIN32
	const int errno_w = WSAGetLastError();
	if (!(errno_w == WSAECONNRESET || errno_w == WSAEINTR || errno_w == WSAEINPROGRESS || errno_w == WSAEMFILE || errno_w == WSAEWOULDBLOCK) && m_sock != INVALID_SOCKET)
		Console.Error("PINE: accept() returned error %d", errno_w);
#else
	if (!(errno == ECONNABORTED || errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK) && m_sock >= 0)
		Console.Error("PINE: accept() returned error %d", errno);
#endif

	return false;
}

void PINEServer::MainLoop()
{
	while (!m_end.load(std::memory_order_acquire))
	{
		if (!AcceptClient())
			continue;

		ClientLoop();

		Console.WriteLn("PINE: Client disconnected.");
		safe_close_portable(m_msgsock);
	}
}

void PINEServer::ClientLoop()
{
	while (!m_end.load(std::memory_order_acquire))
	{
		// either int or ssize_t depending on the platform, so we have to
		// use a bunch of auto
		auto receive_length = 0;
		auto end_length = 4;
		const std::span<u8> ipc_buffer_span(m_ipc_buffer);

		// while we haven't received the entire packet, maybe due to
		// socket datagram splittage, we continue to read
		while (receive_length < end_length)
		{
			const auto tmp_length = read_portable(m_msgsock, &ipc_buffer_span[receive_length], MAX_IPC_SIZE - receive_length);

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
			res = ParseCommand(ipc_buffer_span.subspan(4), m_ret_buffer, (u32)end_length - 4);

			// if we cannot send back our answer restart the socket
			if (write_portable(m_msgsock, res.buffer.data(), res.size) < 0)
				return;
		}
	}
}

void PINEServer::Deinitialize()
{
	m_end.store(true, std::memory_order_release);

#ifndef _WIN32
	if (!m_socket_name.empty())
	{
		unlink(m_socket_name.c_str());
		m_socket_name = {};
	}
#endif

	// shutdown() is needed, otherwise accept() will still block.
#ifdef _WIN32
	if (m_sock != INVALID_SOCKET)
		shutdown(m_sock, SD_BOTH);
#else
	if (m_sock >= 0)
		shutdown(m_sock, SHUT_RDWR);
#endif

	safe_close_portable(m_sock);
	safe_close_portable(m_msgsock);

	if (m_thread.joinable())
		m_thread.join();
}

PINEServer::IPCBuffer PINEServer::ParseCommand(std::span<u8> buf, std::vector<u8>& ret_buffer, u32 buf_size)
{
	u32 ret_cnt = 5;
	u32 buf_cnt = 0;

	while (buf_cnt < buf_size)
	{
		if (!SafetyChecks(buf_cnt, 1, ret_cnt, 0, buf_size)) [[unlikely]]
			return IPCBuffer{5, MakeFailIPC(ret_buffer)};
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
			case MsgVersion:
			{
				if (!VMManager::HasValidVM())
					goto error;
				std::string version;
				if (GIT_TAGGED_COMMIT) // Nightly builds
				{
					// tagged commit - more modern implementation of dev build versioning
					// - there is no need to include the commit - that is associated with the tag, git is implied
					version = fmt::format("PCSX2 Nightly - {}", GIT_TAG);
				}
				else
				{
					version = fmt::format("PCSX2 {}.{}.{}-{}", PCSX2_VersionHi, PCSX2_VersionMid, PCSX2_VersionLo, SVN_REV);
				}
				const u32 size = version.size() + 1;
				if (!SafetyChecks(buf_cnt, 0, ret_cnt, size + 4, buf_size)) [[unlikely]]
					goto error;
				ToResultVector(ret_buffer, size, ret_cnt);
				ret_cnt += 4;
				memcpy(&ret_buffer[ret_cnt], version.c_str(), size);
				ret_cnt += size;
				break;
			}
			case MsgSaveState:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 1, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;
				VMManager::SaveStateToSlot(FromSpan<u8>(buf, buf_cnt));
				buf_cnt += 1;
				break;
			}
			case MsgLoadState:
			{
				if (!VMManager::HasValidVM())
					goto error;
				if (!SafetyChecks(buf_cnt, 1, ret_cnt, 0, buf_size)) [[unlikely]]
					goto error;
				VMManager::LoadStateFromSlot(FromSpan<u8>(buf, buf_cnt));
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
			default:
			{
			error:
				return IPCBuffer{5, MakeFailIPC(ret_buffer)};
			}
		}
	}
	return IPCBuffer{(int)ret_cnt, MakeOkIPC(ret_buffer, ret_cnt)};
}
