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

#pragma once

#include "PrecompiledHeader.h"
#include <cstddef>

/**
 * EmuCmp: Compare two versions of the emulator
 *
 * If you're working on a new version of the recompiler and it keeps hitting a kernel panic somewhere in the bios, use this to compare it to a working version!
 * Just edit the configuration here based on where your bug is, and run the emulator:
 */
namespace EmuCmp {

namespace detail {
	void syncData(void *data, std::size_t size);
	void cmpMem(void *mem, int length, const char *description);
	void verifySync(u16 syncID);
}

namespace Config {
	enum class Mode {
		/// EmuCmp is disabled
		Off,
		/// EmuCmp is sending all values to a client
		Server,
		/// EmuCmp is receiving values from a server and verifying them
		Client,
	};

	enum class Granularity {
		/// Synchronize every instruction (catches more inconsistencies, won't work if e.g. constant propagation is different between the client and server)
		Instruction = 0,
		/// Synchronize every basic block (catches less inconsistencies, works as long as both emulators flush all registers between basic blocks)
		BasicBlock = 1,
	};

	// ======== EmuCmp Configuration ========
	// Edit these to change EmuCmp settings
	// You may want to modify compare routines in EmuCmp.cpp as well to e.g. compare memory regions that are getting garbage written to them

	/// Is EmuCmp enabled?  If false, EmuCmp will be completely disabled and nothing below matters
	const bool enabled = false;
	/// If EmuCmp discovers a desync, should it attempt to fix it?
	const bool corrections = true;
	/// When should EmuCmp synchronize?
	const Config::Granularity granularity = Config::Granularity::Instruction;
	/// Should EmuCmp compare registers after R5900 instructions?  If your issue isn't in the R5900 you can turn this off to reduce EmuCmp overhead
	const bool shouldCompareR5900 = true;
	/// Should EmuCmp compare registers after R3000A instructions?  If your issue isn't in the R3000A you can turn this off to reduce EmuCmp overhead
	const bool shouldCompareR3000A = true;
	/// Should EmuCmp compare registers after VU instructions?  If your issue isn't in the VU you can turn this off to reduce EmuCmp overhead
	const bool shouldCompareVU = true;
};

/// The current mode
extern Config::Mode mode;

static bool isRunning() { return Config::enabled && mode != Config::Mode::Off; }
static bool shouldCompareR5900() { return isRunning() && Config::shouldCompareR5900; }
static bool shouldCompareR3000A() { return isRunning() && Config::shouldCompareR3000A; }
static bool shouldCompareVU() { return isRunning() && Config::shouldCompareVU; }
static bool shouldEmitAfterInstr() { return Config::granularity == Config::Granularity::Instruction; }
static bool shouldEmitAfterBB() { return Config::granularity == Config::Granularity::BasicBlock; }

/// Attempt to initialize emucmp
extern void init();
/// Quit emucmp
extern void shutdown();

/// Compare R5900 registers
extern void __fastcall cmpR5900(u32 pc);

/// Compare VU registers
extern void __fastcall cmpVU(u32 idx, u32 pc);

/// Compare an arbitrary memory buffer
/// (Does not perform correction, meant as a verification for e.g. memory card data which probably shouldn't be synced)
static void cmpMem(void *mem, int length, const char *description) {
	if (isRunning()) {
		detail::cmpMem(mem, length, description);
	}
}

/// If you're not sure both emulators are taking the same codepaths, add one of these
/// It will synchronize `0xaaaa0000 | syncID` and make sure both sides see it.  If you see `0xaaaa####` coming through somewhere else, it's probably due to a desync
static void __fastcall verifySync(u16 syncID) {
	if (isRunning()) {
		detail::verifySync(syncID);
	}
}

/// Synchronize a value between the emucmp client and server
/// (The server will send the value to the client, who overwrites its value with the server's)
template <typename T>
void syncValue(T& value) {
	if (isRunning()) {
		detail::syncData(&value, sizeof(T));
	}
}

} // namespace EmuCmp
