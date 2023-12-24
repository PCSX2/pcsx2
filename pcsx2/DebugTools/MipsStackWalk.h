// SPDX-FileCopyrightText: 2014 PPSSPP Project, 2014-2023 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-2.0+

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
