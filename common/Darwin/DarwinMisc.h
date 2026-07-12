// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once
#ifdef __APPLE__

#include <string>
#include <vector>

#include "common/Pcsx2Types.h"

namespace DarwinMisc {

struct CPUClass {
	std::string name;
	u32 num_physical;
	u32 num_logical;
};

std::vector<CPUClass> GetCPUClasses();

// iOS JIT availability + diagnostics stubs.
// Phase 3 will replace these with the full iOS W^X/JIT implementation.
#if TARGET_OS_IPHONE
extern bool iPSX2_FORCE_EE_INTERP;
extern int iPSX2_FORCE_JIT_VERIFY;
extern int iPSX2_CALL_TGT_X9;
extern int iPSX2_CRASH_PACK;
extern int iPSX2_WX_TRACE;
extern int iPSX2_CALLPROBE;
extern int iPSX2_JIT_HLE;
extern int iPSX2_BISECT_COP1_EVERYTHING_ONLY;
extern int iPSX2_BISECT_COP1_EVERYTHING_PLUS_LOADSTORE;
extern int iPSX2_BISECT_COP1_EVERYTHING_PLUS_MMI;
extern int iPSX2_BISECT_COP1_EVERYTHING_PLUS_COP2_VU;
extern int iPSX2_BISECT_COP1_EVERYTHING_PLUS_MULTDIV;
extern int iPSX2_BISECT_COP1_EVERYTHING_PLUS_SHIFTS;
extern int iPSX2_BISECT_COP1_EVERYTHING_PLUS_MOVES;
extern int iPSX2_BISECT_COP1_EVERYTHING_PLUS_INTEGER_ALU;
extern int iPSX2_BISECT_COP1_EVERYTHING_PLUS_BRANCHES;
bool IsJITAvailable();
void SetCrashLogFD(int fd);
#endif

}

#endif
