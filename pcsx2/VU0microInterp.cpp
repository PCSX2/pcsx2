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


#include "PrecompiledHeader.h"
#include "Common.h"

#include "VUmicro.h"

#include <cfenv>

extern void _vuFlushAll(VURegs* VU);

static void _vu0ExecUpper(VURegs* VU, u32* ptr)
{
	VU->code = ptr[1];
	IdebugUPPER(VU0);
	VU0_UPPER_OPCODE[VU->code & 0x3f]();
}

static void _vu0ExecLower(VURegs* VU, u32* ptr)
{
	VU->code = ptr[0];
	IdebugLOWER(VU0);
	VU0_LOWER_OPCODE[VU->code >> 25]();
}

int vu0branch = 0;
static void _vu0Exec(VURegs* VU)
{
	_VURegsNum lregs;
	_VURegsNum uregs;
	u32* ptr;

	ptr = (u32*)&VU->Micro[VU->VI[REG_TPC].UL];
	VU->VI[REG_TPC].UL += 8;

	if (ptr[1] & 0x40000000) // E flag
	{
		VU->ebit = 2;
	}
	if (ptr[1] & 0x20000000 && VU == &VU0) // M flag
	{
		VU->flags |= VUFLAG_MFLAGSET;
		VU0.blockhasmbit = true;
		//		Console.WriteLn("fixme: M flag set");
	}
	if (ptr[1] & 0x10000000) // D flag
	{
		if (VU0.VI[REG_FBRST].UL & 0x4)
		{
			VU0.VI[REG_VPU_STAT].UL |= 0x2;
			hwIntcIrq(INTC_VU0);
			VU->ebit = 1;
		}
	}
	if (ptr[1] & 0x08000000) // T flag
	{
		if (VU0.VI[REG_FBRST].UL & 0x8)
		{
			VU0.VI[REG_VPU_STAT].UL |= 0x4;
			hwIntcIrq(INTC_VU0);
			VU->ebit = 1;
		}
	}

	VU->code = ptr[1];
	VU0regs_UPPER_OPCODE[VU->code & 0x3f](&uregs);

	u32 cyclesBeforeOp = VU0.cycle - 1;

	_vuTestUpperStalls(VU, &uregs);

	/* check upper flags */
	if (ptr[1] & 0x80000000) // I flag
	{
		_vuTestPipes(VU);

		if (VU->VIBackupCycles > 0)
			VU->VIBackupCycles -= std::min((u8)(VU0.cycle - cyclesBeforeOp), VU->VIBackupCycles);

		_vu0ExecUpper(VU, ptr);

		VU->VI[REG_I].UL = ptr[0];
		memset(&lregs, 0, sizeof(lregs));
	}
	else
	{
		VECTOR _VF;
		VECTOR _VFc;
		REG_VI _VI;
		REG_VI _VIc;
		int vfreg = 0;
		int vireg = 0;
		int discard = 0;

		VU->code = ptr[0];
		lregs.cycles = 0;
		VU0regs_LOWER_OPCODE[VU->code >> 25](&lregs);
		_vuTestLowerStalls(VU, &lregs);

		_vuTestPipes(VU);
		if (VU->VIBackupCycles > 0)
			VU->VIBackupCycles -= std::min((u8)(VU0.cycle - cyclesBeforeOp), VU->VIBackupCycles);
		vu0branch = lregs.pipe == VUPIPE_BRANCH;

		if (uregs.VFwrite)
		{
			if (lregs.VFwrite == uregs.VFwrite)
			{
				//				Console.Warning("*PCSX2*: Warning, VF write to the same reg in both lower/upper cycle");
				discard = 1;
			}
			if (lregs.VFread0 == uregs.VFwrite ||
				lregs.VFread1 == uregs.VFwrite)
			{
				//				Console.WriteLn("saving reg %d at pc=%x", i, VU->VI[REG_TPC].UL);
				_VF = VU->VF[uregs.VFwrite];
				vfreg = uregs.VFwrite;
			}
		}
		if (uregs.VIread & (1 << REG_CLIP_FLAG))
		{
			if (lregs.VIwrite & (1 << REG_CLIP_FLAG))
			{
				//Console.Warning("*PCSX2*: Warning, VI write to the same reg in both lower/upper cycle");
				discard = 1;
			}
			if (lregs.VIread & (1 << REG_CLIP_FLAG))
			{
				_VI = VU0.VI[REG_CLIP_FLAG];
				vireg = REG_CLIP_FLAG;
			}
		}

		_vu0ExecUpper(VU, ptr);

		if (discard == 0)
		{
			if (vfreg)
			{
				_VFc = VU->VF[vfreg];
				VU->VF[vfreg] = _VF;
			}
			if (vireg)
			{
				_VIc = VU->VI[vireg];
				VU->VI[vireg] = _VI;
			}

			_vu0ExecLower(VU, ptr);

			if (vfreg)
			{
				VU->VF[vfreg] = _VFc;
			}
			if (vireg)
			{
				VU->VI[vireg] = _VIc;
			}
		}
	}

	if (uregs.pipe == VUPIPE_FMAC || lregs.pipe == VUPIPE_FMAC)
		_vuClearFMAC(VU);

	_vuAddUpperStalls(VU, &uregs);
	_vuAddLowerStalls(VU, &lregs);

	if (VU->branch > 0)
	{
		if (VU->branch-- == 1)
		{
			VU->VI[REG_TPC].UL = VU->branchpc;

			VU->blockhasmbit = false;

			if (VU->takedelaybranch)
			{
				DevCon.Warning("VU0 - Branch/Jump in Delay Slot");
				VU->branch = 1;
				VU->branchpc = VU->delaybranchpc;
				VU->takedelaybranch = false;
			}
		}
	}

	if (VU->ebit > 0)
	{
		if (VU->ebit-- == 1)
		{
			VU->VIBackupCycles = 0;
			_vuFlushAll(VU);
			VU0.VI[REG_VPU_STAT].UL &= ~0x1; /* E flag */
			vif0Regs.stat.VEW = false;

			VU->blockhasmbit = false;
		}
	}

	// Progress the write position of the FMAC pipeline by one place
	if (uregs.pipe == VUPIPE_FMAC || lregs.pipe == VUPIPE_FMAC)
		VU->fmacwritepos = (VU->fmacwritepos + 1) & 3;
}

void vu0Exec(VURegs* VU)
{
	VU0.VI[REG_TPC].UL &= VU0_PROGMASK;
	VU->cycle++;
	_vu0Exec(VU);

	if (VU->VI[0].UL != 0)
		DbgCon.Error("VI[0] != 0!!!!\n");
	if (VU->VF[0].f.x != 0.0f)
		DbgCon.Error("VF[0].x != 0.0!!!!\n");
	if (VU->VF[0].f.y != 0.0f)
		DbgCon.Error("VF[0].y != 0.0!!!!\n");
	if (VU->VF[0].f.z != 0.0f)
		DbgCon.Error("VF[0].z != 0.0!!!!\n");
	if (VU->VF[0].f.w != 1.0f)
		DbgCon.Error("VF[0].w != 1.0!!!!\n");
}

// --------------------------------------------------------------------------------------
//  VU0microInterpreter
// --------------------------------------------------------------------------------------
InterpVU0::InterpVU0()
{
	m_Idx = 0;
	IsInterpreter = true;
}

void InterpVU0::Reset()
{
	DevCon.Warning("VU0 Int Reset");
	VU0.fmacwritepos = 0;
	VU0.fmacreadpos = 0;
	VU0.fmaccount = 0;
	VU0.ialuwritepos = 0;
	VU0.ialureadpos = 0;
	VU0.ialucount = 0;
}
void InterpVU0::SetStartPC(u32 startPC)
{
	VU0.start_pc = startPC;
}

void InterpVU0::Step()
{
	vu0Exec(&VU0);
}

void InterpVU0::Execute(u32 cycles)
{
	const int originalRounding = fegetround();
	fesetround(g_sseVUMXCSR.RoundingControl << 8);

	VU0.VI[REG_TPC].UL <<= 3;
	VU0.flags &= ~VUFLAG_MFLAGSET;
	u32 startcycles = VU0.cycle;
	while ((VU0.cycle - startcycles) < cycles)
	{
		if (!(VU0.VI[REG_VPU_STAT].UL & 0x1))
		{
			// Branches advance the PC to the new location if there was a branch in the E-Bit delay slot
			if (VU0.branch)
			{
				VU0.VI[REG_TPC].UL = VU0.branchpc;
				VU0.branch = 0;
			}
			break;
		}
		if (VU0.flags & VUFLAG_MFLAGSET)
			break;

		vu0Exec(&VU0);
	}
	VU0.VI[REG_TPC].UL >>= 3;
	VU0.nextBlockCycles = (VU0.cycle - cpuRegs.cycle) + 1;
	fesetround(originalRounding);
}
