// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// USB Modem (Omron ME56PS2) emulation for PCSX2
// Based on https://github.com/msawahara/me56ps2-emulator
// Emulates FTDI-based USB modem with AT command handling over TCP/IP

#include "USB/usb-modem/usb-modem.h"
#include "USB/qemu-usb/qusb.h"
#include "USB/qemu-usb/USBinternal.h"
#include "USB/USB.h"
#include "common/Console.h"
#include "common/StringUtil.h"
#include "Config.h"
#include "StateWrapper.h"
#include "Host.h"
#include "IconsFontAwesome.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
typedef SOCKET socket_t;
#define SOCKET_INVALID INVALID_SOCKET
#define SOCKET_ERROR_VAL SOCKET_ERROR
#define CLOSE_SOCKET closesocket
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
typedef int socket_t;
#define SOCKET_INVALID (-1)
#define SOCKET_ERROR_VAL (-1)
#define CLOSE_SOCKET close
#endif

#include <algorithm>
#include <cstring>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <atomic>
#include <vector>
#include <cstdio>
#include <ctime>
#include <cstdarg>
#include <chrono>

namespace usb_modem
{
	// FTDI status constants (matches me56ps2-emulator exactly)
	// IN packet byte 0: modem status (CTS+DSR always high, DCD set when connected)
	static constexpr uint8_t FTDI_MODEM_STATUS_IDLE = 0x31;    // CTS+DSR high, no carrier
	static constexpr uint8_t FTDI_MODEM_STATUS_ONLINE = 0xB1;  // CTS+DSR high + DCD (carrier detect)
	// IN packet byte 1: line status (THRE+TEMT = transmitter ready) — ALWAYS 0x60
	static constexpr uint8_t FTDI_LINE_STATUS = 0x60;

	// Ring buffer for TCP -> USB data
	static constexpr size_t TX_BUFFER_SIZE = 512 * 1024;

	// FTDI vendor request codes
	static constexpr uint8_t FTDI_SET_MODEM_CTRL = 0x01;
	static constexpr uint8_t FTDI_GET_MODEM_STATUS = 0x05;

	// Note: me56ps2-emulator sends IN status every 40ms but does not
	// NAK at the USB level — it always has a response ready.

	struct RingBuffer
	{
		uint8_t data[TX_BUFFER_SIZE];
		size_t read_pos = 0;
		size_t write_pos = 0;
		size_t count = 0;
		mutable std::mutex mutex;

		void clear()
		{
			std::lock_guard<std::mutex> lock(mutex);
			read_pos = 0;
			write_pos = 0;
			count = 0;
		}

		size_t write(const uint8_t* buf, size_t len)
		{
			std::lock_guard<std::mutex> lock(mutex);
			size_t space = TX_BUFFER_SIZE - count;
			size_t to_write = std::min(len, space);
			if (to_write == 0)
				return 0;

			// Copy in 1-2 chunks to handle wrap-around
			size_t first = std::min(to_write, TX_BUFFER_SIZE - write_pos);
			std::memcpy(data + write_pos, buf, first);
			if (to_write > first)
				std::memcpy(data, buf + first, to_write - first);

			write_pos = (write_pos + to_write) % TX_BUFFER_SIZE;
			count += to_write;
			return to_write;
		}

		size_t read(uint8_t* buf, size_t max_len)
		{
			std::lock_guard<std::mutex> lock(mutex);
			size_t to_read = std::min(max_len, count);
			if (to_read == 0)
				return 0;

			// Copy in 1-2 chunks to handle wrap-around
			size_t first = std::min(to_read, TX_BUFFER_SIZE - read_pos);
			std::memcpy(buf, data + read_pos, first);
			if (to_read > first)
				std::memcpy(buf + first, data, to_read - first);

			read_pos = (read_pos + to_read) % TX_BUFFER_SIZE;
			count -= to_read;
			return to_read;
		}

		size_t available() const
		{
			std::lock_guard<std::mutex> lock(mutex);
			return count;
		}
	};

	struct ModemState
	{
		USBDevice dev{};
		USBDesc desc{};
		USBDescDevice desc_dev{};

		// Network settings
		std::string remote_host;
		int remote_port = 0;
		bool server_mode = false;

		// Modem state
		bool connected = false;   // Online mode (data passthrough)
		bool dtr_high = false;    // DTR signal state
		bool echo_enabled = false; // AT command echo (OFF by default, matches me56ps2-emulator)

		// AT command buffer
		char at_cmd_buf[256] = {};
		int at_cmd_len = 0;

		// USB TX buffer (TCP -> PS2)
		RingBuffer tx_buffer;

		// Socket state
		socket_t listen_sock = SOCKET_INVALID;
		socket_t comm_sock = SOCKET_INVALID;
		std::thread recv_thread;
		std::atomic<bool> recv_thread_running{false};
		std::mutex conn_mutex;
		std::condition_variable conn_cv;

		// Pending RING notification
		bool ring_pending = false;

		// USB polling counters (for diagnostics)
		int in_poll_counter = 0;
		int out_counter = 0;

		// Compatibility mode (selected via USB SubType dropdown)
		ModemVariant mode = MOD_BALANCED;
		// Mode-driven tuning knobs (initialized in CreateDevice).
		int recv_chunk = 512;          // TCP recv() chunk size
		int select_timeout_us = 10000; // recv thread select() timeout
		int in_min_interval_ms = 20;   // Min gap between data-bearing IN responses (0=off)
		int socket_buf_size = 65536;   // SO_SNDBUF/SO_RCVBUF (0 = leave at OS default)
		// Timestamp of last IN response that actually delivered data.
		std::chrono::steady_clock::time_point last_in_data_time{};
	};

	// ---- Socket helpers ----

	static void set_socket_nonblocking(socket_t sock)
	{
#ifdef _WIN32
		u_long mode = 1;
		ioctlsocket(sock, FIONBIO, &mode);
#else
		int flags = fcntl(sock, F_GETFL, 0);
		fcntl(sock, F_SETFL, flags | O_NONBLOCK);
#endif
	}

	static void set_socket_nodelay(socket_t sock)
	{
		int flag = 1;
		setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&flag, sizeof(flag));
	}

	static void set_socket_buffers(socket_t sock, int size)
	{
		// size == 0 means "leave OS defaults alone" (Compatible mode).
		if (size <= 0)
			return;
		setsockopt(sock, SOL_SOCKET, SO_SNDBUF, (const char*)&size, sizeof(size));
		setsockopt(sock, SOL_SOCKET, SO_RCVBUF, (const char*)&size, sizeof(size));
	}

	static void init_winsock()
	{
#ifdef _WIN32
		static bool initialized = false;
		if (!initialized)
		{
			WSADATA wsaData;
			WSAStartup(MAKEWORD(2, 2), &wsaData);
			initialized = true;
		}
#endif
	}

	// ---- File-based debug logging ----

	static FILE* g_modem_log = nullptr;
	static std::mutex g_log_mutex;

	static void modem_log_init()
	{
		// File logging disabled for performance.
		// To re-enable: uncomment the fopen below.
		// std::lock_guard<std::mutex> lock(g_log_mutex);
		// if (g_modem_log)
		// 	return;
		// g_modem_log = std::fopen("modem_debug.log", "w");
		// if (g_modem_log)
		// 	std::setvbuf(g_modem_log, nullptr, _IONBF, 0);
	}

	static void modem_log_close()
	{
		std::lock_guard<std::mutex> lock(g_log_mutex);
		if (g_modem_log)
		{
			std::fclose(g_modem_log);
			g_modem_log = nullptr;
		}
	}

	static void modem_log(const char* fmt, ...)
	{
		std::lock_guard<std::mutex> lock(g_log_mutex);
		if (!g_modem_log)
			return;

		auto now = std::chrono::system_clock::now();
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
		std::time_t t = std::chrono::system_clock::to_time_t(now);
		struct std::tm tm_buf = {};
#ifdef _WIN32
		localtime_s(&tm_buf, &t);
#else
		localtime_r(&t, &tm_buf);
#endif

		std::fprintf(g_modem_log, "[%02d:%02d:%02d.%03d] ",
			tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, (int)ms.count());

		va_list args;
		va_start(args, fmt);
		std::vfprintf(g_modem_log, fmt, args);
		va_end(args);

		std::fprintf(g_modem_log, "\n");
	}

	static void modem_log_hex(const char* prefix, const uint8_t* data, size_t len)
	{
		std::lock_guard<std::mutex> lock(g_log_mutex);
		if (!g_modem_log)
			return;

		auto now = std::chrono::system_clock::now();
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
		std::time_t t = std::chrono::system_clock::to_time_t(now);
		struct std::tm tm_buf = {};
#ifdef _WIN32
		localtime_s(&tm_buf, &t);
#else
		localtime_r(&t, &tm_buf);
#endif

		std::fprintf(g_modem_log, "[%02d:%02d:%02d.%03d] %s (%zu bytes):",
			tm_buf.tm_hour, tm_buf.tm_min, tm_buf.tm_sec, (int)ms.count(),
			prefix, len);

		for (size_t i = 0; i < len && i < 128; i++)
			std::fprintf(g_modem_log, " %02X", data[i]);

		if (len > 128)
			std::fprintf(g_modem_log, " ...(+%zu more)", len - 128);

		// ASCII representation for printable chars
		std::fprintf(g_modem_log, "  |");
		for (size_t i = 0; i < len && i < 128; i++)
		{
			char c = (char)data[i];
			std::fprintf(g_modem_log, "%c", (c >= 0x20 && c < 0x7F) ? c : '.');
		}
		std::fprintf(g_modem_log, "|\n");
	}

	// ---- TCP recv thread ----

	static void tcp_recv_thread(ModemState* s)
	{
		modem_log("RECV_THREAD: started (mode=%d recv_chunk=%d select_us=%d)",
			(int)s->mode, s->recv_chunk, s->select_timeout_us);
		uint8_t buf[4096]; // max-size stack buffer; we only read up to s->recv_chunk bytes
		while (s->recv_thread_running.load())
		{
			if (s->comm_sock == SOCKET_INVALID)
			{
				std::unique_lock<std::mutex> lk(s->conn_mutex);
				s->conn_cv.wait_for(lk, std::chrono::milliseconds(100), [&] {
					return s->comm_sock != SOCKET_INVALID || !s->recv_thread_running.load();
				});
				continue;
			}

			fd_set readfds;
			FD_ZERO(&readfds);
			FD_SET(s->comm_sock, &readfds);

			struct timeval tv;
			tv.tv_sec = 0;
			tv.tv_usec = s->select_timeout_us;

			int ret = select((int)s->comm_sock + 1, &readfds, nullptr, nullptr, &tv);
			if (ret > 0 && FD_ISSET(s->comm_sock, &readfds))
			{
				// Mode-dependent chunk: smaller chunks preserve TCP packet boundaries
				// closer to the PPP/game frames, at some CPU cost.
				int to_read = s->recv_chunk;
				if (to_read <= 0 || to_read > (int)sizeof(buf))
					to_read = sizeof(buf);
				int n = recv(s->comm_sock, (char*)buf, to_read, 0);
				if (n > 0)
				{
					modem_log("RECV_THREAD: received %d bytes from TCP", n);
					modem_log_hex("RECV_THREAD: TCP->RingBuf", buf, n);
					s->tx_buffer.write(buf, n);
					modem_log("RECV_THREAD: ringbuf count=%zu after write", s->tx_buffer.available());
				}
				else if (n == 0)
				{
					modem_log("RECV_THREAD: remote peer closed connection");
					Console.WriteLn("USB Modem: Remote disconnected");
					CLOSE_SOCKET(s->comm_sock);
					s->comm_sock = SOCKET_INVALID;
					s->connected = false;
				}
				else
				{
#ifdef _WIN32
					int err = WSAGetLastError();
					if (err != WSAEWOULDBLOCK)
					{
						modem_log("RECV_THREAD: recv error %d, disconnecting", err);
#else
					if (errno != EAGAIN && errno != EWOULDBLOCK)
					{
						modem_log("RECV_THREAD: recv error %d, disconnecting", errno);
#endif
						Console.WriteLn("USB Modem: recv error, disconnecting");
						CLOSE_SOCKET(s->comm_sock);
						s->comm_sock = SOCKET_INVALID;
						s->connected = false;
					}
				}
			}
		}
		modem_log("RECV_THREAD: exiting");
	}

	// ---- Server accept thread ----

	static void server_accept_check(ModemState* s)
	{
		if (!s->server_mode || s->listen_sock == SOCKET_INVALID || s->comm_sock != SOCKET_INVALID)
			return;

		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(s->listen_sock, &readfds);

		struct timeval tv = {};
		tv.tv_sec = 0;
		tv.tv_usec = 0; // Non-blocking check

		if (select((int)s->listen_sock + 1, &readfds, nullptr, nullptr, &tv) > 0)
		{
			struct sockaddr_in addr;
			socklen_t addrlen = sizeof(addr);
			socket_t new_sock = accept(s->listen_sock, (struct sockaddr*)&addr, &addrlen);
			if (new_sock != SOCKET_INVALID)
			{
				char ip_str[INET_ADDRSTRLEN];
				inet_ntop(AF_INET, &addr.sin_addr, ip_str, sizeof(ip_str));
				modem_log("SERVER: accepted connection from %s:%d (sock=%d)", ip_str, ntohs(addr.sin_port), (int)new_sock);
				Console.WriteLn("USB Modem: Incoming connection from %s:%d", ip_str, ntohs(addr.sin_port));

				set_socket_nonblocking(new_sock);
				set_socket_nodelay(new_sock);
				set_socket_buffers(new_sock, s->socket_buf_size);
				s->comm_sock = new_sock;
				s->ring_pending = true;
				s->conn_cv.notify_one();
				modem_log("SERVER: ring_pending=true, waiting for ATA");
			}
		}
	}

	// ---- AT command parser ----

	// Parse ATD dial string: "192-168-100-1#12345" -> ip:port
	// Returns false for non-IP dial strings (e.g. "0528#0528") so the
	// caller falls back to the configured RemoteHost/RemotePort.
	static bool parse_dial_address(const char* dial_str, std::string& host, int& port)
	{
		// Skip 'T' or 'P' (tone/pulse dial prefix)
		if (*dial_str == 'T' || *dial_str == 'P' || *dial_str == 't' || *dial_str == 'p')
			dial_str++;

		std::string addr(dial_str);

		// Find port separator '#'
		auto hash_pos = addr.find('#');
		if (hash_pos == std::string::npos)
			return false;

		std::string ip_part = addr.substr(0, hash_pos);
		std::string port_part = addr.substr(hash_pos + 1);

		// Replace '-' with '.'
		for (char& c : ip_part)
		{
			if (c == '-')
				c = '.';
		}

		// Validate as a real IPv4 address
		struct in_addr tmp;
		if (inet_pton(AF_INET, ip_part.c_str(), &tmp) != 1)
			return false;

		host = ip_part;
		port = std::atoi(port_part.c_str());
		return port > 0 && port <= 65535;
	}

	static void send_at_response(ModemState* s, const char* response)
	{
		size_t len = std::strlen(response);
		// Log without \r\n for readability
		char clean[256] = {};
		size_t ci = 0;
		for (size_t i = 0; i < len && ci < sizeof(clean) - 1; i++)
		{
			if (response[i] == '\r') { clean[ci++] = '\\'; clean[ci++] = 'r'; }
			else if (response[i] == '\n') { clean[ci++] = '\\'; clean[ci++] = 'n'; }
			else clean[ci++] = response[i];
		}
		modem_log("AT_RESP: -> \"%s\" (%zu bytes)", clean, len);
		s->tx_buffer.write((const uint8_t*)response, len);
	}

	static void modem_connect(ModemState* s, const std::string& host, int port)
	{
		modem_log("CONNECT: attempting TCP connection to %s:%d", host.c_str(), port);
		init_winsock();

		struct sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);

		if (inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1)
		{
			modem_log("CONNECT: FAILED - invalid address '%s'", host.c_str());
			Console.WriteLn("USB Modem: Invalid address: %s", host.c_str());
			send_at_response(s, "BUSY\r\n");
			return;
		}

		socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock == SOCKET_INVALID)
		{
			modem_log("CONNECT: FAILED - socket creation failed");
			Console.WriteLn("USB Modem: Failed to create socket");
			send_at_response(s, "BUSY\r\n");
			return;
		}

		// Use blocking connect with a timeout
		struct timeval tv;
		tv.tv_sec = 5;
		tv.tv_usec = 0;
		setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof(tv));
		setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof(tv));

		modem_log("CONNECT: calling connect() to %s:%d (5s timeout)...", host.c_str(), port);
		if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) != 0)
		{
#ifdef _WIN32
			modem_log("CONNECT: FAILED - connect() error %d", WSAGetLastError());
#else
			modem_log("CONNECT: FAILED - connect() error %d", errno);
#endif
			Console.WriteLn("USB Modem: Failed to connect to %s:%d", host.c_str(), port);
			CLOSE_SOCKET(sock);
			send_at_response(s, "BUSY\r\n");
			return;
		}

		modem_log("CONNECT: SUCCESS - connected to %s:%d (sock=%d)", host.c_str(), port, (int)sock);
		Console.WriteLn("USB Modem: Connected to %s:%d", host.c_str(), port);

		set_socket_nonblocking(sock);
		set_socket_nodelay(sock);
		set_socket_buffers(sock, s->socket_buf_size);
		s->comm_sock = sock;
		s->connected = true;
		s->tx_buffer.clear();
		s->conn_cv.notify_one();

		send_at_response(s, "CONNECT 57600 V42\r\n");
	}

	static void modem_disconnect(ModemState* s)
	{
		modem_log("DISCONNECT: connected=%d comm_sock=%d", s->connected, (int)s->comm_sock);
		if (s->comm_sock != SOCKET_INVALID)
		{
			CLOSE_SOCKET(s->comm_sock);
			s->comm_sock = SOCKET_INVALID;
		}
		s->connected = false;
		s->ring_pending = false;
		modem_log("DISCONNECT: done");
		Console.WriteLn("USB Modem: Disconnected");
	}

	static void process_at_command(ModemState* s, const char* cmd)
	{
		modem_log("AT_CMD: received '%s' (len=%d)", cmd, (int)std::strlen(cmd));
		Console.WriteLn("USB Modem: AT cmd: '%s'", cmd);

		// Normalize: skip "AT" prefix
		if ((cmd[0] == 'A' || cmd[0] == 'a') && (cmd[1] == 'T' || cmd[1] == 't'))
			cmd += 2;
		else
			return; // Not an AT command

		if (cmd[0] == '\0')
		{
			// Bare "AT" - just respond OK
			send_at_response(s, "OK\r\n");
		}
		else if (cmd[0] == '&' && (cmd[1] == 'F' || cmd[1] == 'f'))
		{
			// AT&F - Factory reset
			s->echo_enabled = true;
			send_at_response(s, "OK\r\n");
		}
		else if ((cmd[0] == 'E' || cmd[0] == 'e') && cmd[1] == '0')
		{
			// ATE0 - Disable echo
			s->echo_enabled = false;
			send_at_response(s, "OK\r\n");
		}
		else if ((cmd[0] == 'E' || cmd[0] == 'e') && cmd[1] == '1')
		{
			// ATE1 - Enable echo
			s->echo_enabled = true;
			send_at_response(s, "OK\r\n");
		}
		else if (cmd[0] == 'D' || cmd[0] == 'd')
		{
			// ATD - Dial
			std::string host;
			int port;
			if (parse_dial_address(cmd + 1, host, port))
			{
				Console.WriteLn("USB Modem: Dialing %s:%d", host.c_str(), port);
				modem_connect(s, host, port);
			}
			else
			{
				// Try using configured remote host/port
				if (!s->remote_host.empty() && s->remote_port > 0)
				{
					Console.WriteLn("USB Modem: Dialing configured %s:%d", s->remote_host.c_str(), s->remote_port);
					modem_connect(s, s->remote_host, s->remote_port);
				}
				else
				{
					Console.WriteLn("USB Modem: Invalid dial string and no configured host");
					send_at_response(s, "BUSY\r\n");
				}
			}
		}
		else if (cmd[0] == 'A' || cmd[0] == 'a')
		{
			// ATA - Answer
			if (s->comm_sock != SOCKET_INVALID)
			{
				Console.WriteLn("USB Modem: Answering call");
				s->connected = true;
				s->ring_pending = false;
				s->tx_buffer.clear();
				send_at_response(s, "CONNECT 57600 V42\r\n");
			}
			else
			{
				send_at_response(s, "NO CARRIER\r\n");
			}
		}
		else if (cmd[0] == 'H' || cmd[0] == 'h')
		{
			// ATH - Hang up
			modem_disconnect(s);
			send_at_response(s, "OK\r\n");
		}
		else
		{
			// All other AT commands - dummy OK
			send_at_response(s, "OK\r\n");
		}
	}

	// ---- USB handlers ----

	static void usb_modem_handle_reset(USBDevice* dev)
	{
		ModemState* s = USB_CONTAINER_OF(dev, ModemState, dev);
		modem_log("USB_RESET: device reset (in_polls=%d, outs=%d)", s->in_poll_counter, s->out_counter);
		s->at_cmd_len = 0;
		s->in_poll_counter = 0;
		s->out_counter = 0;
		s->tx_buffer.clear();
	}

	static void usb_modem_handle_control(USBDevice* dev, USBPacket* p, int request, int value,
		int index, int length, uint8_t* data)
	{
		ModemState* s = USB_CONTAINER_OF(dev, ModemState, dev);

		modem_log("USB_CTRL: request=0x%04X value=0x%04X index=0x%04X length=%d",
			request, value, index, length);

		int ret = usb_desc_handle_control(dev, p, request, value, index, length, data);
		if (ret >= 0)
		{
			modem_log("USB_CTRL: handled by usb_desc (ret=%d)", ret);
			return;
		}

		// request format: (bmRequestType << 8) | bRequest
		// VendorDeviceOutRequest = 0x4000, VendorDeviceRequest = 0xC000
		switch (request)
		{
			// FTDI SET_MODEM_CTRL (0x01) - DTR/RTS signaling
			case VendorDeviceOutRequest | FTDI_SET_MODEM_CTRL:
			{
				uint16_t dtr_bits = value & 0x0101;
				modem_log("USB_CTRL: FTDI SET_MODEM_CTRL value=0x%04X dtr_bits=0x%04X", value, dtr_bits);
				if (dtr_bits == 0x0100)
				{
					modem_log("USB_CTRL: DTR LOW (on-hook) -> disconnect");
					Console.WriteLn("USB Modem: DTR low (on-hook)");
					s->dtr_high = false;
					modem_disconnect(s);
				}
				else if (dtr_bits == 0x0101)
				{
					modem_log("USB_CTRL: DTR HIGH (off-hook) was_low=%d", !s->dtr_high);
					Console.WriteLn("USB Modem: DTR high (off-hook)");
					s->dtr_high = true;
					// me56ps2-emulator does NOT send any response on DTR toggle.
					// Only AT command processing generates IN data.
				}
				p->actual_length = 0;
				p->status = USB_RET_SUCCESS;
				break;
			}

			// FTDI GET_MODEM_STATUS (0x05) - return modem + line status bytes
			// Same 2 bytes as FTDI IN header. IOP may use this for modem detection.
			case (int)VendorDeviceRequest | FTDI_GET_MODEM_STATUS:
			{
				if (length >= 2 && data)
				{
					data[0] = s->connected ? FTDI_MODEM_STATUS_ONLINE : FTDI_MODEM_STATUS_IDLE;
					data[1] = FTDI_LINE_STATUS;
					p->actual_length = 2;
					modem_log("USB_CTRL: GET_MODEM_STATUS -> [0x%02X 0x%02X]", data[0], data[1]);
				}
				else
				{
					p->actual_length = 0;
				}
				p->status = USB_RET_SUCCESS;
				break;
			}

			default:
			{
				// Handle specific FTDI vendor requests, then catch-all for the rest.
				int req_type = request & 0xFF00;
				int bRequest = request & 0x00FF;
				modem_log("USB_CTRL: vendor request=0x%04X bRequest=0x%02X value=0x%04X", request, bRequest, value);

				if (req_type == VendorDeviceOutRequest || req_type == (int)VendorDeviceRequest)
				{
					// FTDI SIO_RESET (0x00): handle purge/reset commands
					if (bRequest == 0x00)
					{
						if (value == 0)
						{
							// Full device reset: clear all buffers and state
							modem_log("USB_CTRL: FTDI RESET (full) — clearing buffers");
							s->tx_buffer.clear();
							s->at_cmd_len = 0;
						}
						else if (value == 1)
						{
							// Purge RX buffer (FTDI → host direction, i.e. our tx_buffer)
							modem_log("USB_CTRL: FTDI PURGE_RX — clearing tx_buffer");
							s->tx_buffer.clear();
						}
						else if (value == 2)
						{
							// Purge TX buffer (host → FTDI direction)
							modem_log("USB_CTRL: FTDI PURGE_TX");
							// Nothing to clear on our side for host→device direction
						}
					}

					// All vendor requests: zero-length ACK (matches me56ps2-emulator)
					p->actual_length = 0;
					p->status = USB_RET_SUCCESS;
				}
				else
				{
					modem_log("USB_CTRL: STALL (unknown request type)");
					p->status = USB_RET_STALL;
				}
				break;
			}
		}
	}

	static void usb_modem_handle_data(USBDevice* dev, USBPacket* p)
	{
		ModemState* s = USB_CONTAINER_OF(dev, ModemState, dev);

		switch (p->pid)
		{
			case USB_TOKEN_OUT:
			{
				// Host -> Device (PS2 -> Modem)
				// FTDI OUT format: [1-byte header][payload...]
				// Header byte: upper 6 bits = payload length (header >> 2)
				// Confirmed by me56ps2-emulator source code.
				uint8_t buf[64];
				size_t size = std::min<size_t>(p->buffer_size, sizeof(buf));
				usb_packet_copy(p, buf, size);

				if (size < 1)
					break;

				s->out_counter++;

				// Strip FTDI OUT header (1 byte) — matches me56ps2-emulator:
				// payload_length = pkt.data[0] >> 2; buffer.append(&pkt.data[1], payload_length);
				uint8_t header = buf[0];
				int payload_len = header >> 2;
				if (payload_len > (int)size - 1)
					payload_len = (int)size - 1;
				if (payload_len < 0)
					payload_len = 0;

				uint8_t* payload = buf + 1;

				// Log first 20 OUT packets in detail, then summary every 10000th
				if (s->out_counter <= 20)
				{
					modem_log_hex("USB_OUT: raw packet", buf, size);
					modem_log("USB_OUT: #%d hdr=0x%02X payload_len=%d raw_size=%zu connected=%d",
						s->out_counter, header, payload_len, size, s->connected);
				}
				else if (s->out_counter % 10000 == 0)
				{
					modem_log("USB_OUT: #%d (summary) hdr=0x%02X payload_len=%d raw_size=%zu connected=%d",
						s->out_counter, header, payload_len, size, s->connected);
				}

				if (s->connected)
				{
					// Online mode: forward directly to TCP.
					// send() may do a partial write (returns < payload_len) when the
					// kernel send buffer fills up, especially with small SO_SNDBUF.
					// Dropping the remainder corrupts PPP frame boundaries on the
					// peer and causes AP desync, so loop until fully sent.
					if (s->comm_sock != SOCKET_INVALID && payload_len > 0)
					{
						modem_log_hex("USB_OUT: sending to TCP", payload, payload_len);
						int total_sent = 0;
						int retries = 0;
						while (total_sent < payload_len)
						{
							int n = send(s->comm_sock, (const char*)payload + total_sent,
								payload_len - total_sent, 0);
							if (n > 0)
							{
								total_sent += n;
								continue;
							}
							// n <= 0 path: EAGAIN retries briefly, other errors break out.
#ifdef _WIN32
							int err = (n < 0) ? WSAGetLastError() : 0;
							const bool would_block = (err == WSAEWOULDBLOCK);
#else
							int err = (n < 0) ? errno : 0;
							const bool would_block = (err == EAGAIN || err == EWOULDBLOCK);
#endif
							if (would_block && retries < 10)
							{
								retries++;
								std::this_thread::sleep_for(std::chrono::microseconds(200));
								continue;
							}
							modem_log("USB_OUT: TCP send error %d (partial %d/%d)",
								err, total_sent, payload_len);
							break;
						}
						modem_log("USB_OUT: TCP send completed %d/%d", total_sent, payload_len);
					}
				}
				else
				{
					// Command mode: AT command parsing only.
					// Binary probe data from the IOP FTDI driver is NOT echoed back.
					// A real modem receives this data on its UART but only echoes
					// recognized AT commands (when ATE1). The me56ps2-emulator also
					// does not echo raw OUT data — only AT command responses go to IN.
					if (payload_len > 0 && s->out_counter <= 20)
					{
						modem_log_hex("USB_OUT: AT mode payload", payload, payload_len);
					}

					for (int i = 0; i < payload_len; i++)
					{
						char c = (char)payload[i];

						if (c == '\r')
						{
							s->at_cmd_buf[s->at_cmd_len] = '\0';
							if (s->at_cmd_len > 0)
							{
								process_at_command(s, s->at_cmd_buf);
							}
							s->at_cmd_len = 0;
						}
						else if (c == '\n' || c == '\0')
						{
							// Ignore LF and NUL
						}
						else if (c == '\b' || c == 0x7F)
						{
							// Backspace
							if (s->at_cmd_len > 0)
								s->at_cmd_len--;
						}
						else
						{
							if (s->at_cmd_len < (int)sizeof(s->at_cmd_buf) - 1)
								s->at_cmd_buf[s->at_cmd_len++] = c;
						}
					}
				}
				break;
			}

			case USB_TOKEN_IN:
			{
				// Device -> Host (Modem -> PS2)
				// FTDI IN format: [modem_status][line_status][optional data...]
				// me56ps2-emulator sends exactly 2+payload_len bytes (NOT padded to 64).
				// The IOP FTDI driver uses transfer_length-2 to count serial data bytes.

				s->in_poll_counter++;

				// Always respond immediately to IN tokens — the me56ps2-emulator
				// always has a response ready (sends status every 40ms).
				// No latency timer: let the OHCI handle retry timing.

				// Log periodically
				if (s->in_poll_counter <= 50 || s->in_poll_counter % 5000 == 0)
				{
					modem_log("USB_IN: poll #%d RESPOND buf_size=%zu tx_avail=%zu connected=%d out_count=%d",
						s->in_poll_counter, (size_t)p->buffer_size,
						s->tx_buffer.available(), s->connected, s->out_counter);
				}

				// Check for incoming connections in server mode
				if (!s->connected)
					server_accept_check(s);

				// Send RING if pending
				if (s->ring_pending && !s->connected)
				{
					modem_log("USB_IN: sending RING notification");
					send_at_response(s, "RING\r\n");
					s->ring_pending = false;
				}

				uint8_t pkt[64];
				memset(pkt, 0, sizeof(pkt));

				// FTDI IN header:
				//   byte 0: modem status (0x31 idle, 0xB1 connected)
				//   byte 1: line status  (0x60 = THRE+TEMT, bit 0 = DR when data ready)
				pkt[0] = s->connected ? FTDI_MODEM_STATUS_ONLINE : FTDI_MODEM_STATUS_IDLE;

				// Read available data from tx_buffer (max 62 bytes per packet).
				//
				// Pacing: the original me56ps2-emulator emits IN packets on a ~40ms
				// timer, which approximates 14.4kbps serial pacing. Responding
				// instantly on every OHCI IN poll can overrun the IOP's UART FIFO
				// and also coalesce multiple PPP frames into single USB reads, which
				// appears to cause AP desync against real PS2 hardware.
				// When in_min_interval_ms > 0, skip delivering data if insufficient
				// time has elapsed since the last data-bearing response; the IN still
				// returns the 2-byte status so OHCI stays happy.
				size_t buf_size = std::min<size_t>(p->buffer_size, sizeof(pkt));
				size_t max_data = (buf_size > 2) ? buf_size - 2 : 0;
				size_t data_len = 0;
				bool throttled = false;
				if (max_data > 0)
				{
					if (s->in_min_interval_ms > 0)
					{
						auto now = std::chrono::steady_clock::now();
						auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
							now - s->last_in_data_time).count();
						if (elapsed_ms < s->in_min_interval_ms)
							throttled = true;
					}
					if (!throttled)
					{
						data_len = s->tx_buffer.read(pkt + 2, max_data);
						if (data_len > 0)
							s->last_in_data_time = std::chrono::steady_clock::now();
					}
				}

				// Line status: THRE+TEMT (0x60) always set.
				// DR (Data Ready, bit 0) = 1 when RX FIFO has data.
				pkt[1] = FTDI_LINE_STATUS | (data_len > 0 ? 0x01 : 0x00);

				if (data_len > 0)
				{
					modem_log("USB_IN: returning %zu bytes data, hdr=[0x%02X 0x%02X] connected=%d",
						data_len, pkt[0], pkt[1], s->connected);
					modem_log_hex("USB_IN: full packet", pkt, 2 + data_len);
				}

				// Send exactly 2 + data_len bytes, matching me56ps2-emulator behavior.
				// The IOP FTDI driver uses transfer length to determine serial data count:
				//   data_bytes = transfer_length - 2 (subtract status header)
				// Padding with zeros would make the IOP think it received NUL serial data.
				size_t total = 2 + data_len;
				usb_packet_copy(p, pkt, total);

				modem_log("USB_IN: poll #%d returned %zu bytes (data=%zu) hdr=[0x%02X,0x%02X]",
					s->in_poll_counter, total, data_len, pkt[0], pkt[1]);
				break;
			}

			default:
				modem_log("USB_DATA: unknown pid=0x%02X -> STALL", p->pid);
				p->status = USB_RET_STALL;
				break;
		}
	}

	static void usb_modem_handle_destroy(USBDevice* dev)
	{
		ModemState* s = USB_CONTAINER_OF(dev, ModemState, dev);

		modem_log("DESTROY: shutting down modem device");

		// Stop recv thread
		s->recv_thread_running.store(false);
		s->conn_cv.notify_one();
		if (s->recv_thread.joinable())
			s->recv_thread.join();
		modem_log("DESTROY: recv thread joined");

		// Close sockets
		if (s->comm_sock != SOCKET_INVALID)
		{
			CLOSE_SOCKET(s->comm_sock);
			s->comm_sock = SOCKET_INVALID;
		}
		if (s->listen_sock != SOCKET_INVALID)
		{
			CLOSE_SOCKET(s->listen_sock);
			s->listen_sock = SOCKET_INVALID;
		}
		modem_log("DESTROY: sockets closed");

		modem_log("=== Modem log closed ===");
		modem_log_close();

		delete s;
	}

	static bool modem_start_server(ModemState* s)
	{
		modem_log("SERVER: starting server on port %d", s->remote_port);
		init_winsock();

		s->listen_sock = socket(AF_INET, SOCK_STREAM, 0);
		if (s->listen_sock == SOCKET_INVALID)
		{
			modem_log("SERVER: FAILED - socket creation failed");
			Console.Error("USB Modem: Failed to create listen socket");
			return false;
		}

		int opt = 1;
		setsockopt(s->listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

		struct sockaddr_in addr = {};
		addr.sin_family = AF_INET;
		addr.sin_port = htons(s->remote_port);
		addr.sin_addr.s_addr = INADDR_ANY;

		if (bind(s->listen_sock, (struct sockaddr*)&addr, sizeof(addr)) != 0)
		{
#ifdef _WIN32
			modem_log("SERVER: FAILED - bind error %d on port %d", WSAGetLastError(), s->remote_port);
#else
			modem_log("SERVER: FAILED - bind error %d on port %d", errno, s->remote_port);
#endif
			Console.Error("USB Modem: Failed to bind to port %d", s->remote_port);
			CLOSE_SOCKET(s->listen_sock);
			s->listen_sock = SOCKET_INVALID;
			return false;
		}

		if (listen(s->listen_sock, 1) != 0)
		{
			modem_log("SERVER: FAILED - listen error on port %d", s->remote_port);
			Console.Error("USB Modem: Failed to listen on port %d", s->remote_port);
			CLOSE_SOCKET(s->listen_sock);
			s->listen_sock = SOCKET_INVALID;
			return false;
		}

		set_socket_nonblocking(s->listen_sock);
		modem_log("SERVER: SUCCESS - listening on 0.0.0.0:%d (sock=%d)", s->remote_port, (int)s->listen_sock);
		Console.WriteLn("USB Modem: Listening on port %d (server mode)", s->remote_port);
		return true;
	}

	// ---- Device Proxy implementation ----

	USBDevice* ModemDevice::CreateDevice(SettingsInterface& si, u32 port, u32 subtype) const
	{
		ModemState* s = new ModemState();

		modem_log_init();
		modem_log("=== Modem device creation started ===");

		s->remote_host = USB::GetConfigString(si, port, TypeName(), "RemoteHost", "");
		s->remote_port = USB::GetConfigInt(si, port, TypeName(), "RemotePort", 10023);
		s->server_mode = USB::GetConfigBool(si, port, TypeName(), "ServerMode", false);

		// Map SubType index to tuning knobs. Index 0 (MOD_BALANCED) is the default
		// for users with no prior modem_subtype entry in PCSX2.ini.
		if (subtype >= MOD_COUNT)
			subtype = MOD_BALANCED;
		s->mode = static_cast<ModemVariant>(subtype);
		const char* mode_name = "Balanced";
		switch (s->mode)
		{
			case MOD_FAST:
				// Preserves the pre-existing aggressive tuning for low-latency LAN.
				s->recv_chunk = 4096;
				s->select_timeout_us = 1000;
				s->in_min_interval_ms = 0;
				s->socket_buf_size = 4096;
				mode_name = "Fast";
				break;
			case MOD_BALANCED:
			default:
				s->recv_chunk = 512;
				s->select_timeout_us = 10000;
				s->in_min_interval_ms = 20;
				s->socket_buf_size = 65536;
				mode_name = "Balanced";
				break;
		}
		s->last_in_data_time = std::chrono::steady_clock::now();

		modem_log("CREATE: host='%s' port=%d server_mode=%d usb_port=%u mode=%s",
			s->remote_host.c_str(), s->remote_port, s->server_mode, port, mode_name);
		Console.WriteLn("USB Modem: Creating device - Host=%s Port=%d Server=%s Mode=%s",
			s->remote_host.c_str(), s->remote_port, s->server_mode ? "true" : "false", mode_name);

		s->dev.speed = USB_SPEED_FULL;
		s->desc.full = &s->desc_dev;
		s->desc.str = (const char* const*)nullptr;

		// Set up string descriptors
		static const char* modem_strings[] = {
			"",              // 0: (empty)
			"OMRON",         // 1: Manufacturer
			"ME56PS2",       // 2: Product
			"00000000",      // 3: Serial
		};
		s->desc.str = modem_strings;

		s->desc.id.idVendor = 0x0590;
		s->desc.id.idProduct = 0x001A;
		s->desc.id.bcdDevice = 0x0101;
		s->desc.id.iManufacturer = 1;
		s->desc.id.iProduct = 2;
		s->desc.id.iSerialNumber = 3;

		if (usb_desc_parse_dev(me56ps2_dev_descriptor, sizeof(me56ps2_dev_descriptor), s->desc, s->desc_dev) < 0)
			goto fail;
		if (usb_desc_parse_config(me56ps2_config_descriptor, sizeof(me56ps2_config_descriptor), s->desc_dev) < 0)
			goto fail;

		s->dev.klass.handle_attach = usb_desc_attach;
		s->dev.klass.handle_reset = usb_modem_handle_reset;
		s->dev.klass.handle_control = usb_modem_handle_control;
		s->dev.klass.handle_data = usb_modem_handle_data;
		s->dev.klass.unrealize = usb_modem_handle_destroy;
		s->dev.klass.usb_desc = &s->desc;
		s->dev.klass.product_desc = "ME56PS2 USB Modem";

		usb_desc_init(&s->dev);
		usb_ep_init(&s->dev);
		usb_modem_handle_reset(&s->dev);

		// Start TCP recv thread
		s->recv_thread_running.store(true);
		s->recv_thread = std::thread(tcp_recv_thread, s);

		// Start server if in server mode
		if (s->server_mode && s->remote_port > 0)
		{
			modem_start_server(s);
		}

		return &s->dev;

	fail:
		usb_modem_handle_destroy(&s->dev);
		return nullptr;
	}

	const char* ModemDevice::Name() const
	{
		return TRANSLATE_NOOP("USB", "ME56PS2 Modem");
	}

	const char* ModemDevice::TypeName() const
	{
		return "modem";
	}

	const char* ModemDevice::IconName() const
	{
		return ICON_FA_PHONE;
	}

	std::span<const char*> ModemDevice::SubTypes() const
	{
		// Order matches ModemVariant enum; index 0 becomes the default subtype
		// in PCSX2.ini for fresh users, which we want to be Balanced.
		static const char* subtypes[] = {
			TRANSLATE_NOOP("USB", "Balanced"),
			TRANSLATE_NOOP("USB", "Fast (low latency)"),
		};
		return subtypes;
	}

	std::span<const SettingInfo> ModemDevice::Settings(u32 subtype) const
	{
		(void)subtype; // All three modes share the same user-facing settings.
		static constexpr const SettingInfo info[] = {
			{SettingInfo::Type::String, "RemoteHost", TRANSLATE_NOOP("USB", "Remote Host"),
				TRANSLATE_NOOP("USB", "IP address of the remote player (client mode) or bind address (server mode)."),
				"", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0.0f},
			{SettingInfo::Type::Integer, "RemotePort", TRANSLATE_NOOP("USB", "Port"),
				TRANSLATE_NOOP("USB", "TCP port number for the modem connection."),
				"10023", "1", "65535", "1", nullptr, nullptr, nullptr, 0.0f},
			{SettingInfo::Type::Boolean, "ServerMode", TRANSLATE_NOOP("USB", "Server Mode"),
				TRANSLATE_NOOP("USB", "Enable to wait for incoming connections (answering side). Disable to dial out (calling side).\n\n"
				"Based on me56ps2-emulator by msawahara\n"
				"https://github.com/msawahara/me56ps2-emulator\n"
				"PCSX2 port by ChungSo"),
				"false", nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, 0.0f},
		};
		return info;
	}

	bool ModemDevice::Freeze(USBDevice* dev, StateWrapper& sw) const
	{
		ModemState* s = USB_CONTAINER_OF(dev, ModemState, dev);

		if (!sw.DoMarker("ModemDevice"))
			return false;

		sw.Do(&s->connected);
		sw.Do(&s->dtr_high);
		sw.Do(&s->echo_enabled);
		sw.DoBytes(&s->at_cmd_buf, sizeof(s->at_cmd_buf));
		sw.Do(&s->at_cmd_len);
		sw.Do(&s->ring_pending);
		sw.Do(&s->in_poll_counter);

		// On load, reset network state since we can't serialize sockets
		if (sw.IsReading())
		{
			s->connected = false;
			s->ring_pending = false;
			if (s->comm_sock != SOCKET_INVALID)
			{
				CLOSE_SOCKET(s->comm_sock);
				s->comm_sock = SOCKET_INVALID;
			}
			s->tx_buffer.clear();
		}

		return true;
	}

	void ModemDevice::UpdateSettings(USBDevice* dev, SettingsInterface& si) const
	{
		ModemState* s = USB_CONTAINER_OF(dev, ModemState, dev);

		std::string new_host = USB::GetConfigString(si, 0, TypeName(), "RemoteHost", "");
		int new_port = USB::GetConfigInt(si, 0, TypeName(), "RemotePort", 10023);
		bool new_server = USB::GetConfigBool(si, 0, TypeName(), "ServerMode", false);

		if (new_host != s->remote_host || new_port != s->remote_port || new_server != s->server_mode)
		{
			Console.WriteLn("USB Modem: Settings changed, reconnecting...");
			modem_disconnect(s);

			if (s->listen_sock != SOCKET_INVALID)
			{
				CLOSE_SOCKET(s->listen_sock);
				s->listen_sock = SOCKET_INVALID;
			}

			s->remote_host = new_host;
			s->remote_port = new_port;
			s->server_mode = new_server;

			if (s->server_mode && s->remote_port > 0)
			{
				modem_start_server(s);
			}
		}
	}

} // namespace usb_modem
