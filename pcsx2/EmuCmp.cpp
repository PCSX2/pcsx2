/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2020  PCSX2 Dev Team
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

#include "EmuCmp.h"
#include "PrecompiledHeader.h"
#include "Assertions.h"
#include "R5900.h"
#include "DebugTools/Debug.h"
#ifdef __POSIX__
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#endif

using namespace EmuCmp;

Config::Mode EmuCmp::mode = Config::Mode::Off;

// TODO: Windows support

// Note: communications are one-way so we can use buffered IO
#ifdef __POSIX__
static FILE *comms = nullptr;
#endif

void EmuCmp::init() {
#ifdef __POSIX__
	if (comms) { return; } // Already initted
#endif
	mode = Config::Mode::Off;
	if (EmuConfig.Debugger.EmuCmpHost.IsEmpty()) { return; }
	wxString host;
	Config::Mode attemptMode;
	auto port = EmuConfig.Debugger.EmuCmpHost.AfterLast(':');
	if (port == EmuConfig.Debugger.EmuCmpHost) {
		attemptMode = Config::Mode::Server;
	} else {
		host = EmuConfig.Debugger.EmuCmpHost.BeforeLast(':');
		attemptMode = Config::Mode::Client;
	}
	long portnum;
	if (!port.ToLong(&portnum) && portnum == (u16)portnum) {
		Console.Error("EmuCmp: Invalid port %s", WX_STR(port));
		return;
	}

#ifdef __POSIX__
	if (attemptMode == Config::Mode::Server) {
		signal(SIGPIPE, SIG_IGN); // I don't think anyone needs these
		struct sockaddr_in addr = {0};
		addr.sin_family = AF_INET;
		((u8*)&addr.sin_port)[0] = portnum >> 8;
		((u8*)&addr.sin_port)[1] = portnum & 0xFF;
		int fd = socket(PF_INET, SOCK_STREAM, 0);
		if (fd >= 0 && bind(fd, (struct sockaddr *)&addr, sizeof(addr)) == 0 && listen(fd, 1) == 0) {
			Console.WriteLn("EmuCmp: Awaiting client on port %s", WX_STR(port));
			int newfd = accept(fd, nullptr, nullptr);
			if (newfd >= 0) {
				close(fd);
				comms = fdopen(newfd, "w");
				mode = Config::Mode::Server;
				goto done;
			}
		}
		Console.Error("EmuCmp: Failed to listen on %s: %s", WX_STR(port), strerror(errno));
	} else {
		struct addrinfo hints = {0};
		hints.ai_socktype = SOCK_STREAM;
		struct addrinfo *res;

		int error = getaddrinfo(host.IsEmpty() ? nullptr : WX_STR(host), port.c_str(), &hints, &res);
		if (error) {
			Console.Error("EmuCmp: Failed to resolve %s:%s: %s", WX_STR(host), WX_STR(port), gai_strerror(error));
			return;
		}

		for (auto info = res; info; info = info->ai_next) {
			int fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
			if (fd < 0) { continue; }
			if (connect(fd, info->ai_addr, info->ai_addrlen) == 0) {
				mode = Config::Mode::Client;
				comms = fdopen(fd, "r");
				goto cleanup;
			}
			close(fd);
		}
		Console.Error("EmuCmp: Failed to connect to %s:%s: %s", WX_STR(host), WX_STR(port), strerror(errno));
	cleanup:
		freeaddrinfo(res);
	}
done:
#endif
	return;
}

void EmuCmp::shutdown() {
	if (comms) {
		fclose(comms);
		comms = nullptr;
	}
	mode = Config::Mode::Off;
}

static void send(const void *buffer, std::size_t size) {
	if (mode != Config::Mode::Server) { return; }
#ifdef __POSIX__
	if (fwrite(buffer, size, 1, comms) != 1) {
		// Client quit
		Console.Error("EmuCmp: Client quit!");
		shutdown();
	}
#endif
}

static void receive(void *buffer, std::size_t size) {
	if (mode != Config::Mode::Client) { return; }
#ifdef __POSIX__
	if (fread(buffer, size, 1, comms) != 1) {
		// Server quit
		Console.Error("EmuCmp: Server quit!");
		shutdown();
	}
#endif
}

template <typename T, typename = typename std::enable_if<std::is_trivial<T>::value>::type>
void send(const T& item) {
	send(&item, sizeof(T));
}

template <typename T, typename = typename std::enable_if<std::is_trivial<T>::value>::type>
T receive() {
	T out;
	receive(&out, sizeof(T));
	return out;
}

void EmuCmp::detail::syncData(void *data, std::size_t size) {
	if (mode == Config::Mode::Server) {
		send(data, size);
	} else if (mode == Config::Mode::Client) {
		receive(data, size);
	}
}

#include <sstream>

template <typename T>
static void Compare(const T& server, T& client, const char *name, bool& found, u32 pc) {
	if (server != client) {
		std::stringstream out;
		out << "Server client mismatch of " << name << " at " << std::hex << pc << " (server was " << std::hex << server << " but client was " << std::hex << client << ")";
		pxAssertRel(0, out.str().c_str());
		if (Config::corrections) {
			client = server;
		}
		found = true;
	}
}

static void Compare(const GPR_reg& server, GPR_reg& client, const char *name, bool& found, u32 pc) {
	Compare(server.SD[0], client.SD[0], name, found, pc);
	char hiname[16];
	sprintf(hiname, "%s High", name);
	Compare(server.SD[1], client.SD[1], hiname, found, pc);
}

/// If you're having trouble with load instructions loading incorrect data from memory, use this to compare the section of memory containing the incorrect data during the appropriate cmpR#### routine to catch the bad store in action!
static void CompareBuffer(void *buffer, int startOffset, int length, const char *name, u32 pc) {
	u8 *local = (u8*)buffer + startOffset;
	if (mode == Config::Mode::Server) {
		send(local, length);
	} else if (mode == Config::Mode::Client) {
		const int maxStackBuffer = _64kb;
		u8 srv_[std::min(length, maxStackBuffer)];
		u8 *srv = length > maxStackBuffer ? (u8*)malloc(length) : srv_;
		receive(srv, length);

		if (0 != memcmp(srv, local, length)) {
			bool ignore;
			if (length % 4 == 0) { // Compare ints
				u32* srv32 = (u32*)srv;
				u32* local32 = (u32*)local;
				for (int i = 0; i < length/4; i++) {
					char nm[128];
					sprintf(nm, "%s[0x%x]", name, i*4+startOffset);
					Compare(srv32[i], local32[i], nm, ignore, pc);
				}
			} else {
				for (int i = 0; i < length; i++) {
					char nm[128];
					sprintf(nm, "%s[0x%x]", name, i+startOffset);
					Compare(srv[i], local[i], nm, ignore, pc);
				}
			}
		}

		if (length > maxStackBuffer) { free(srv); }
	}
}

#define COMPARE(reg) Compare(servCPU.reg, cpuRegs.reg, #reg, found, pc)

void EmuCmp::cmpR5900(u32 pc) {
	static u32 lastPC = 0;
	// Example use of CompareBuffer to find bad memory around 0x70001000
	// CompareBuffer(eeMem->Scratch, 0x1000, 0x500, "Scratch", lastPC);
	if (mode == Config::Mode::Server) {
		send(cpuRegs);
		send(fpuRegs);
	} else if (mode == Config::Mode::Client) {
		const cpuRegisters servCPU = receive<cpuRegisters>();
		const fpuRegisters servFPU = receive<fpuRegisters>();
		if (0 != memcmp(&servCPU, &cpuRegs, sizeof(cpuRegisters))) {
			bool found = false;
			for (int i = 0; i < 32; i++) {
				Compare(servCPU.GPR.r[i], cpuRegs.GPR.r[i], R5900::GPR_REG[i], found, lastPC);
				Compare(servCPU.CP0.r[i], cpuRegs.CP0.r[i], R5900::COP0_REG[i], found, lastPC);
				Compare(servCPU.eCycle[i], cpuRegs.eCycle[i], "eCycle", found, lastPC);
				Compare(servCPU.sCycle[i], cpuRegs.sCycle[i], "sCycle", found, lastPC);
			}
			COMPARE(HI);
			COMPARE(LO);
			COMPARE(sa);
			COMPARE(IsDelaySlot);
			COMPARE(pc);
			COMPARE(cycle);
			COMPARE(interrupt);
			COMPARE(branch);
			COMPARE(opmode);
			COMPARE(tempcycles);
			Compare(servCPU.PERF.n.pccr.val, cpuRegs.PERF.n.pccr.val, "pccr", found, lastPC);
			Compare(servCPU.PERF.n.pad, cpuRegs.PERF.n.pad, "pccr", found, lastPC);
			Compare(servCPU.PERF.n.pcr0, cpuRegs.PERF.n.pcr0, "pcr0", found, lastPC);
			Compare(servCPU.PERF.n.pcr1, cpuRegs.PERF.n.pcr1, "pcr1", found, lastPC);
			if (!found) {
				std::stringstream out;
				out << "Server client mismatch of unknown CPU register at " << std::hex << lastPC;
				pxAssertRel(0, out.str().c_str());
			}
		}
		if (0 != memcmp(&servFPU, &fpuRegs, sizeof(fpuRegisters))) {
			bool found = false;
			for (int i = 0; i < 32; i++) {
				Compare(servFPU.fpr[i].f, fpuRegs.fpr[i].f, R5900::COP1_REG_FP[i], found, lastPC);
				Compare(servFPU.fprc[i], fpuRegs.fprc[i], R5900::COP1_REG_FCR[i], found, lastPC);
			}
			Compare(servFPU.ACC.f, fpuRegs.ACC.f, "FP ACC", found, lastPC);
			if (!found) {
				std::stringstream out;
				out << "Server client mismatch of unknown FPU register at " << std::hex << lastPC;
				pxAssertRel(0, out.str().c_str());
			}
		}
	}
	lastPC = pc;
}

void EmuCmp::detail::cmpMem(void *mem, int length, const char *description) {
	if (mode == Config::Mode::Server) {
		send(mem, length);
	} else {
		const int maxStackBuffer = _64kb;
		u8 srv_[std::min(length, maxStackBuffer)];
		u8 *srv = length > maxStackBuffer ? (u8*)malloc(length) : srv_;

		if (0 != memcmp(srv, mem, length)) {
			char err[1024];
			sprintf(err, "Mismatch in %s", description);
			pxAssertRel(0, err);
		}

		if (length > maxStackBuffer) { free(srv); }
	}
}

void EmuCmp::detail::verifySync(u16 syncID) {
	u32 val = 0xaaaa0000 | syncID;
	syncValue(val);
	pxAssertRel(val == 0xaaaa0000 | syncID, "Emulators desynced!");
}
