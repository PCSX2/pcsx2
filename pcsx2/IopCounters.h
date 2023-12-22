// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

struct psxCounter {
	u64 count, target;
    u32 mode;
	u32 rate, interrupt;
	u32 sCycleT;
	s32 CycleT;
};

#define NUM_COUNTERS 8

extern psxCounter psxCounters[NUM_COUNTERS];

extern void psxRcntInit();
extern void psxRcntUpdate();
extern void psxRcntWcount16(int index, u16 value);
extern void psxRcntWcount32(int index, u32 value);
extern void psxRcntWmode16(int index, u32 value);
extern void psxRcntWmode32(int index, u32 value);
extern void psxRcntWtarget16(int index, u32 value);
extern void psxRcntWtarget32(int index, u32 value);
extern u16  psxRcntRcount16(int index);
extern u32  psxRcntRcount32(int index);
extern u64  psxRcntCycles(int index);

extern void psxVBlankStart();
extern void psxVBlankEnd();
extern void psxCheckStartGate16(int i);
extern void psxCheckEndGate16(int i);
