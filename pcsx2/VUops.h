/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

typedef void FnType_VuRegsN(_VURegsNum *VUregsn);
typedef FnType_VuRegsN* Fnptr_VuRegsN;

alignas(16) extern const Fnptr_Void VU0_LOWER_OPCODE[128];
alignas(16) extern const Fnptr_Void VU0_UPPER_OPCODE[64];
alignas(16) extern const Fnptr_VuRegsN VU0regs_LOWER_OPCODE[128];
alignas(16) extern const Fnptr_VuRegsN VU0regs_UPPER_OPCODE[64];

alignas(16) extern const Fnptr_Void VU1_LOWER_OPCODE[128];
alignas(16) extern const Fnptr_Void VU1_UPPER_OPCODE[64];
alignas(16) extern const Fnptr_VuRegsN VU1regs_LOWER_OPCODE[128];
alignas(16) extern const Fnptr_VuRegsN VU1regs_UPPER_OPCODE[64];
extern void _vuClearFMAC(VURegs * VU);
extern void _vuTestPipes(VURegs * VU);
extern void _vuTestUpperStalls(VURegs * VU, _VURegsNum *VUregsn);
extern void _vuTestLowerStalls(VURegs * VU, _VURegsNum *VUregsn);
extern void _vuAddUpperStalls(VURegs * VU, _VURegsNum *VUregsn);
extern void _vuAddLowerStalls(VURegs * VU, _VURegsNum *VUregsn);
extern void _vuXGKICKTransfer(s32 cycles, bool flush);
