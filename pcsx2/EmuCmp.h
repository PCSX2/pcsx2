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
 */
namespace EmuCmp {

namespace detail {
	void syncData(void *data, std::size_t size);
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
};

extern Config::Mode mode;
extern Config::Granularity granularity;

/// Attempt to initialize emucmp
void init();
/// Quit emucmp
void shutdown();

/// Compare R5900 registers
void __fastcall cmpR5900(u32 pc);

/// Synchronize a value between the emucmp client and server
/// (The server will send the value to the client, who overwrites its value with the server's)
template <typename T>
void syncValue(T& value) {
	if (mode != Config::Mode::Off) {
		detail::syncData(&value, sizeof(T));
	}
}

} // namespace EmuCmp
