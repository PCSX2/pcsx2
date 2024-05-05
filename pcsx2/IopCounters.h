// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

struct CounterIRQBehaviour
{
	bool repeatInterrupt;
	bool toggleInterrupt;
};

union psxCounterMode
{
	struct
	{
		u32 gateEnable : 1;
		u32 gateMode : 2;
		u32 zeroReturn : 1;
		u32 targetIntr : 1;
		u32 overflIntr : 1;
		u32 repeatIntr : 1;
		u32 toggleIntr : 1;
		u32 extSignal : 1;
		u32 t2Prescale : 1;
		u32 intrEnable : 1;
		u32 targetFlag : 1;
		u32 overflowFlag : 1;
		u32 t4_5Prescale : 2;
		u32 stopped : 1;
	};

	u32 modeval;
};

struct psxCounter {
	u64 count, target;
	
	u32 rate, interrupt;
	u32 startCycle;
	s32 deltaCycles;

	psxCounterMode mode;
	CounterIRQBehaviour currentIrqMode;
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
extern void psxRcntSetNewIntrMode(int index);
extern u16  psxRcntRcount16(int index);
extern u32  psxRcntRcount32(int index);
extern u64  psxRcntCycles(int index);

extern void psxHBlankStart();
extern void psxHBlankEnd();
extern void psxVBlankStart();
extern void psxVBlankEnd();
