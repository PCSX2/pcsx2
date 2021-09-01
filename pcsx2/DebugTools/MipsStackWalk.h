/*
 * Copyright (C) 2014-2014  PCSX2 Dev Team
 *
 * Imported from PPSSPP
 *
 * PCSX2 is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * PCSX2 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#pragma once

#include <vector>
#include "common/Pcsx2Types.h"

class DebugInterface;

namespace MipsStackWalk {
	struct StackFrame {
		// Beginning of function symbol (may be estimated.)
		u32 entry;
		// Next position within function.
		u32 pc;
		// Value of SP inside this function (assuming no alloca()...)
		u32 sp;
		// Size of stack frame in bytes.
		int stackSize;
	};

	std::vector<StackFrame> Walk(DebugInterface* cpu, u32 pc, u32 ra, u32 sp, u32 threadEntry, u32 threadStackTop);
};
