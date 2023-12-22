// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "VU.h"
#include "VUflags.h"

#define float_to_int4(x)	((float)x * (1.0f / 0.0625f))
#define float_to_int12(x)	((float)x * (1.0f / 0.000244140625f))
#define float_to_int15(x)	((float)x * (1.0f / 0.000030517578125))

#define int4_to_float(x)	(float)((float)x * 0.0625f)
#define int12_to_float(x)	(float)((float)x * 0.000244140625f)
#define int15_to_float(x)	(float)((float)x * 0.000030517578125)

struct _VURegsNum {
	u8 pipe; // if 0xff, COP2
	u8 VFwrite;
	u8 VFwxyzw;
	u8 VFr0xyzw;
	u8 VFr1xyzw;
	u8 VFread0;
	u8 VFread1;
	u32 VIwrite;
	u32 VIread;
	int cycles;
};

using FnPtr_VuVoid = void (*)();
using FnPtr_VuRegsN = void(*)(_VURegsNum *VUregsn);

alignas(16) extern const FnPtr_VuVoid VU0_LOWER_OPCODE[128];
alignas(16) extern const FnPtr_VuVoid VU0_UPPER_OPCODE[64];
alignas(16) extern const FnPtr_VuRegsN VU0regs_LOWER_OPCODE[128];
alignas(16) extern const FnPtr_VuRegsN VU0regs_UPPER_OPCODE[64];

alignas(16) extern const FnPtr_VuVoid VU1_LOWER_OPCODE[128];
alignas(16) extern const FnPtr_VuVoid VU1_UPPER_OPCODE[64];
alignas(16) extern const FnPtr_VuRegsN VU1regs_LOWER_OPCODE[128];
alignas(16) extern const FnPtr_VuRegsN VU1regs_UPPER_OPCODE[64];
extern void _vuClearFMAC(VURegs * VU);
extern void _vuTestPipes(VURegs * VU);
extern void _vuTestUpperStalls(VURegs * VU, _VURegsNum *VUregsn);
extern void _vuTestLowerStalls(VURegs * VU, _VURegsNum *VUregsn);
extern void _vuAddUpperStalls(VURegs * VU, _VURegsNum *VUregsn);
extern void _vuAddLowerStalls(VURegs * VU, _VURegsNum *VUregsn);
extern void _vuXGKICKTransfer(s32 cycles, bool flush);
