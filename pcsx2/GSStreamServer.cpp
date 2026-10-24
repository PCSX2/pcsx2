// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "GSStreamServer.h"

#include "common/Console.h"
#include "common/Image.h"
#include "common/Threading.h"

#include <atomic>
#include <condition_variable>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include "common/RedtapeWindows.h"
#include <WinSock2.h>
#include <ws2tcpip.h>
using socket_t = SOCKET;
#define gs_send(s, b, n)  (send((s), reinterpret_cast<const char*>(b), static_cast<int>(n), 0))
#define gs_close(s)       do { if ((s) != INVALID_SOCKET) { closesocket((s)); (s) = INVALID_SOCKET; } } while (0)
#define GS_INVALID_SOCK   INVALID_SOCKET
#define gs_last_err()     (WSAGetLastError())
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_t = int;
#define gs_send(s, b, n)  (send((s), (b), (n), MSG_NOSIGNAL))
#define gs_close(s)       do { if ((s) >= 0) { close((s)); (s) = -1; } } while (0)
#define GS_INVALID_SOCK   (-1)
#define gs_last_err()     (errno)
#endif

namespace
{
	// Wire constants
	constexpr u8  kMsgTypeFrame = 1;
	constexpr u8  kCodecMJPEG   = 1;
	constexpr u8  kJpegQuality  = 80;

	// Header (after the leading u32 size field):
	//   u8 msg_type, u32 frame_idx, u16 width, u16 height, u8 codec, u8 flags = 11 bytes
	constexpr size_t kHeaderAfterSize = 1 + 4 + 2 + 2 + 1 + 1;

	std::atomic_bool s_initialized{false};
	std::atomic_bool s_end{true};
	std::atomic<u32> s_subscriber_count{0};
	std::atomic<u32> s_frames_delivered{0};
	std::atomic<u32> s_frames_dropped{0};
	std::atomic<int> s_bound_port{-1};

	socket_t s_listen_sock = GS_INVALID_SOCK;

	std::thread s_accept_thread;
	std::thread s_encoder_thread;

	std::mutex s_subscribers_mutex;
	std::vector<socket_t> s_subscribers;

	// Single-slot pending frame from GS thread → encoder thread.
	// Drop-newest: GS thread overwrites without blocking; encoder takes whatever
	// is latest when it wakes up. This keeps GS perf flat under load.
	std::mutex s_pending_mutex;
	std::condition_variable s_pending_cv;
	bool s_pending_has_frame = false;
	u32  s_pending_width = 0;
	u32  s_pending_height = 0;
	std::vector<u32> s_pending_pixels; // tightly packed RGBA8

	// Reusable encode buffers (encoder thread only — no locking needed).
	RGBA8Image s_encode_image;
	u32 s_next_frame_idx = 0;

	void ConfigureClientSocket(socket_t s)
	{
		// Keep subscriber sockets blocking so WriteAll() can reliably drain
		// the full frame header+payload; a send timeout bounds stalls.
#if defined(_WIN32)
		DWORD send_timeout_ms = 250;
		setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&send_timeout_ms), sizeof(send_timeout_ms));
#else
		const timeval send_timeout = {0, 250 * 1000};
		setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout));
#endif
	}


	void WriteLE16(u8* dst, u16 value)
	{
		dst[0] = static_cast<u8>(value & 0xFF);
		dst[1] = static_cast<u8>((value >> 8) & 0xFF);
	}

	void WriteLE32(u8* dst, u32 value)
	{
		dst[0] = static_cast<u8>(value & 0xFF);
		dst[1] = static_cast<u8>((value >> 8) & 0xFF);
		dst[2] = static_cast<u8>((value >> 16) & 0xFF);
		dst[3] = static_cast<u8>((value >> 24) & 0xFF);
	}

	void DropSubscriberLocked(size_t idx)
	{
		gs_close(s_subscribers[idx]);
		s_subscribers.erase(s_subscribers.begin() + idx);
		s_subscriber_count.store(static_cast<u32>(s_subscribers.size()), std::memory_order_release);
	}

	bool WriteAll(socket_t s, const void* data, size_t len)
	{
		const char* p = reinterpret_cast<const char*>(data);
		size_t remaining = len;
		while (remaining > 0)
		{
			const auto n = gs_send(s, p, remaining);
			if (n <= 0)
				return false;
			p += n;
			remaining -= static_cast<size_t>(n);
		}
		return true;
	}

	void BroadcastFrame(const std::vector<u8>& jpeg, u32 width, u32 height, u32 frame_idx)
	{
		// Build header in a small stack buffer, then header + payload per subscriber.
		const u32 payload_size = static_cast<u32>(jpeg.size());
		const u32 size_field = static_cast<u32>(kHeaderAfterSize + payload_size);

		u8 hdr[4 + kHeaderAfterSize];
		WriteLE32(hdr + 0, size_field);
		hdr[4] = kMsgTypeFrame;
		WriteLE32(hdr + 5, frame_idx);
		const u16 w = static_cast<u16>(width);
		const u16 h = static_cast<u16>(height);
		WriteLE16(hdr + 9, w);
		WriteLE16(hdr + 11, h);
		hdr[13] = kCodecMJPEG;
		hdr[14] = 0;

		std::lock_guard<std::mutex> lock(s_subscribers_mutex);
		for (size_t i = 0; i < s_subscribers.size(); )
		{
			const socket_t s = s_subscribers[i];
			if (!WriteAll(s, hdr, sizeof(hdr)) ||
				(payload_size > 0 && !WriteAll(s, jpeg.data(), payload_size)))
			{
				Console.WriteLn("GSStream: subscriber dropped (write error %d).", gs_last_err());
				DropSubscriberLocked(i);
				continue;
			}
			++i;
		}
	}

	void AcceptLoop()
	{
		Threading::SetNameOfCurrentThread("GS Stream Accept");

		while (!s_end.load(std::memory_order_acquire))
		{
			sockaddr_in peer = {};
#if defined(_WIN32)
			int peer_len = sizeof(peer);
#else
			socklen_t peer_len = sizeof(peer);
#endif
			const socket_t client = accept(s_listen_sock, reinterpret_cast<sockaddr*>(&peer), &peer_len);
			if (client == GS_INVALID_SOCK)
			{
				if (s_end.load(std::memory_order_acquire))
					break;
				continue;
			}

			// Disable Nagle: we send one fully-formed frame per call and want
			// it to hit the wire immediately, not wait for more bytes.
			int one = 1;
			setsockopt(client, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&one), sizeof(one));

			ConfigureClientSocket(client);

			{
				std::lock_guard<std::mutex> lock(s_subscribers_mutex);
				s_subscribers.push_back(client);
				s_subscriber_count.store(static_cast<u32>(s_subscribers.size()), std::memory_order_release);
			}
			Console.WriteLn("GSStream: subscriber connected (now %u total).", s_subscriber_count.load());
		}
	}

	void EncoderLoop()
	{
		Threading::SetNameOfCurrentThread("GS Stream Encoder");

		std::vector<u32> local_pixels;

		for (;;)
		{
			u32 width = 0;
			u32 height = 0;

			{
				std::unique_lock<std::mutex> lock(s_pending_mutex);
				s_pending_cv.wait(lock, []() {
					return s_end.load(std::memory_order_acquire) || s_pending_has_frame;
				});

				if (s_end.load(std::memory_order_acquire))
					break;

				width  = s_pending_width;
				height = s_pending_height;
				local_pixels = std::move(s_pending_pixels);
				s_pending_pixels.clear();
				s_pending_has_frame = false;
			}

			if (width == 0 || height == 0 || local_pixels.empty())
				continue;

			// Re-create the image only if the geometry changed; pixels copy unavoidable.
			if (s_encode_image.GetWidth() != width || s_encode_image.GetHeight() != height)
				s_encode_image = RGBA8Image(width, height);

			std::memcpy(s_encode_image.GetPixels(), local_pixels.data(), local_pixels.size() * sizeof(u32));

			auto encoded = s_encode_image.SaveToBuffer("frame.jpg", kJpegQuality);
			if (!encoded.has_value() || encoded->size() < 4 || (*encoded)[0] != 0xFF || (*encoded)[1] != 0xD8 ||
				(*encoded)[encoded->size() - 2] != 0xFF || (*encoded)[encoded->size() - 1] != 0xD9)
			{
				Console.Warning("GSStream: JPEG encode invalid for %ux%u frame (size=%zu).", width, height, encoded.has_value() ? encoded->size() : 0u);
				continue;
			}

			const u32 idx = s_next_frame_idx++;
			BroadcastFrame(*encoded, width, height, idx);
			s_frames_delivered.fetch_add(1, std::memory_order_relaxed);
		}
	}
} // namespace

bool GSStreamServer::IsInitialized()
{
	return s_initialized.load(std::memory_order_acquire);
}

int GSStreamServer::GetPort()
{
	return s_bound_port.load(std::memory_order_acquire);
}

bool GSStreamServer::HasSubscribers()
{
	return s_subscriber_count.load(std::memory_order_acquire) > 0;
}

bool GSStreamServer::Initialize(int port)
{
	if (s_initialized.load(std::memory_order_acquire))
	{
		Console.WriteLn(Color_Yellow, "GSStream: already initialized.");
		return true;
	}

	s_end.store(false, std::memory_order_release);
	s_subscriber_count.store(0, std::memory_order_release);
	s_frames_delivered.store(0, std::memory_order_release);
	s_frames_dropped.store(0, std::memory_order_release);
	s_next_frame_idx = 0;
	{
		std::lock_guard<std::mutex> lock(s_pending_mutex);
		s_pending_has_frame = false;
		s_pending_pixels.clear();
		s_pending_width = 0;
		s_pending_height = 0;
	}

#if defined(_WIN32)
	// Winsock is initialized by PINEServer when present; we don't double-init here
	// because PINE always comes up before us and atexit-registers WSACleanup.
#endif

	s_listen_sock = socket(AF_INET, SOCK_STREAM, 0);
	if (s_listen_sock == GS_INVALID_SOCK)
	{
		Console.Error("GSStream: socket() failed (err %d).", gs_last_err());
		s_end.store(true, std::memory_order_release);
		return false;
	}

	int one = 1;
	setsockopt(s_listen_sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&one), sizeof(one));

	sockaddr_in addr = {};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	addr.sin_port = htons(static_cast<u_short>(port));

	if (bind(s_listen_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
	{
		Console.Error("GSStream: bind(port=%d) failed (err %d).", port, gs_last_err());
		gs_close(s_listen_sock);
		s_end.store(true, std::memory_order_release);
		return false;
	}

	// Recover the actually-bound port (matters when port == 0).
	sockaddr_in bound = {};
#if defined(_WIN32)
	int bound_len = sizeof(bound);
#else
	socklen_t bound_len = sizeof(bound);
#endif
	if (getsockname(s_listen_sock, reinterpret_cast<sockaddr*>(&bound), &bound_len) != 0)
	{
		Console.Error("GSStream: getsockname() failed (err %d).", gs_last_err());
		gs_close(s_listen_sock);
		s_end.store(true, std::memory_order_release);
		return false;
	}
	s_bound_port.store(ntohs(bound.sin_port), std::memory_order_release);

	if (listen(s_listen_sock, 16) != 0)
	{
		Console.Error("GSStream: listen() failed (err %d).", gs_last_err());
		gs_close(s_listen_sock);
		s_end.store(true, std::memory_order_release);
		return false;
	}

	s_accept_thread  = std::thread(AcceptLoop);
	s_encoder_thread = std::thread(EncoderLoop);

	s_initialized.store(true, std::memory_order_release);
	Console.WriteLn("GSStream: listening on 127.0.0.1:%d", s_bound_port.load());
	return true;
}

void GSStreamServer::Deinitialize()
{
	if (!s_initialized.load(std::memory_order_acquire))
		return;

	s_end.store(true, std::memory_order_release);

	// Unblock accept() by shutting the listener down.
	if (s_listen_sock != GS_INVALID_SOCK)
	{
#if defined(_WIN32)
		shutdown(s_listen_sock, SD_BOTH);
#else
		shutdown(s_listen_sock, SHUT_RDWR);
#endif
	}

	// Unblock the encoder.
	{
		std::lock_guard<std::mutex> lock(s_pending_mutex);
		s_pending_cv.notify_all();
	}

	if (s_accept_thread.joinable())
		s_accept_thread.join();
	if (s_encoder_thread.joinable())
		s_encoder_thread.join();

	{
		std::lock_guard<std::mutex> lock(s_pending_mutex);
		s_pending_has_frame = false;
		s_pending_width = 0;
		s_pending_height = 0;
		s_pending_pixels.clear();
		s_pending_pixels.shrink_to_fit();
	}

	{
		std::lock_guard<std::mutex> lock(s_subscribers_mutex);
		for (socket_t s : s_subscribers)
			gs_close(s);
		s_subscribers.clear();
		s_subscriber_count.store(0, std::memory_order_release);
	}

	gs_close(s_listen_sock);
	s_encode_image = RGBA8Image();
	s_bound_port.store(-1, std::memory_order_release);
	s_initialized.store(false, std::memory_order_release);
}

void GSStreamServer::DeliverRGBA(u32 width, u32 height, const u32* pixels)
{
	if (!s_initialized.load(std::memory_order_acquire) || width == 0 || height == 0 || pixels == nullptr)
		return;

	const size_t pixel_count = static_cast<size_t>(width) * static_cast<size_t>(height);

	std::lock_guard<std::mutex> lock(s_pending_mutex);
	if (s_pending_has_frame)
		s_frames_dropped.fetch_add(1, std::memory_order_relaxed);

	s_pending_pixels.assign(pixels, pixels + pixel_count);
	s_pending_width  = width;
	s_pending_height = height;
	s_pending_has_frame = true;
	s_pending_cv.notify_one();
}

GSStreamServer::Stats GSStreamServer::GetStats()
{
	return Stats{
		s_frames_delivered.load(std::memory_order_acquire),
		s_frames_dropped.load(std::memory_order_acquire),
		s_subscriber_count.load(std::memory_order_acquire),
		s_bound_port.load(std::memory_order_acquire) >= 0 ? static_cast<u32>(s_bound_port.load()) : 0u,
	};
}
