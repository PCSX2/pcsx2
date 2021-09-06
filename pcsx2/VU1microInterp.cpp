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
#include "GS.h"
#include "Gif_Unit.h"
#include "MTVU.h"

#include <cfenv>

extern void _vuFlushAll(VURegs* VU);
extern void _vuXGKICKFlush(VURegs* VU);

void _vu1ExecUpper(VURegs* VU, u32* ptr)
{
	VU->code = ptr[1];
	IdebugUPPER(VU1);
	VU1_UPPER_OPCODE[VU->code & 0x3f]();
}

void _vu1ExecLower(VURegs* VU, u32* ptr)
{
	VU->code = ptr[0];
	IdebugLOWER(VU1);
	VU1_LOWER_OPCODE[VU->code >> 25]();
}

int vu1branch = 0;

static void _vu1Exec(VURegs* VU)
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
	if (ptr[1] & 0x10000000) // D flag
	{
		if (VU0.VI[REG_FBRST].UL & 0x400)
		{
			VU0.VI[REG_VPU_STAT].UL |= 0x200;
			hwIntcIrq(INTC_VU1);
			VU->ebit = 1;
		}
	}
	if (ptr[1] & 0x08000000) // T flag
	{
		if (VU0.VI[REG_FBRST].UL & 0x800)
		{
			VU0.VI[REG_VPU_STAT].UL |= 0x400;
			hwIntcIrq(INTC_VU1);
			VU->ebit = 1;
		}
	}

	//VUM_LOG("VU->cycle = %d (flags st=%x;mac=%x;clip=%x,q=%f)", VU->cycle, VU->statusflag, VU->macflag, VU->clipflag, VU->q.F);

	VU->code = ptr[1];
	VU1regs_UPPER_OPCODE[VU->code & 0x3f](&uregs);

	u32 cyclesBeforeOp = VU1.cycle-1;
	
	_vuTestUpperStalls(VU, &uregs);

	/* check upper flags */
	if (ptr[1] & 0x80000000) // I Flag (Lower op is a float)
	{
		_vuTestPipes(VU);

		if (VU->VIBackupCycles > 0)
			VU->VIBackupCycles -= std::min((u8)(VU1.cycle - cyclesBeforeOp), VU->VIBackupCycles);

		_vu1ExecUpper(VU, ptr);

		VU->VI[REG_I].UL = ptr[0];
		//Lower not used, set to 0 to fill in the FMAC stall gap
		//Could probably get away with just running upper stalls, but lets not tempt fate.
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
		VU1regs_LOWER_OPCODE[VU->code >> 25](&lregs);

		_vuTestLowerStalls(VU, &lregs);
		_vuTestPipes(VU);

		if (VU->VIBackupCycles > 0)
			VU->VIBackupCycles-= std::min((u8)(VU1.cycle- cyclesBeforeOp), VU->VIBackupCycles);

		if (uregs.VFwrite)
		{
			if (lregs.VFwrite == uregs.VFwrite)
			{
				//Console.Warning("*PCSX2*: Warning, VF write to the same reg in both lower/upper cycle pc=%x", VU->VI[REG_TPC].UL);
				discard = 1;
			}
			if (lregs.VFread0 == uregs.VFwrite ||
				lregs.VFread1 == uregs.VFwrite)
			{
				//Console.WriteLn("saving reg %d at pc=%x", uregs.VFwrite, VU->VI[REG_TPC].UL);
				_VF = VU->VF[uregs.VFwrite];
				vfreg = uregs.VFwrite;
			}
		}
		if (uregs.VIwrite & (1 << REG_CLIP_FLAG))
		{
			if (lregs.VIwrite & (1 << REG_CLIP_FLAG))
			{
				//Console.Warning("*PCSX2*: Warning, VI write to the same reg in both lower/upper cyclepc=%x", VU->VI[REG_TPC].UL);
				discard = 1;
			}
			if (lregs.VIread & (1 << REG_CLIP_FLAG))
			{
				//Console.Warning("*PCSX2*: Warning, VI read same cycle as write pc=%x", VU->VI[REG_TPC].UL);
				_VI = VU->VI[REG_CLIP_FLAG];
				vireg = REG_CLIP_FLAG;
			}
		}

		_vu1ExecUpper(VU, ptr);

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

			_vu1ExecLower(VU, ptr);

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
	// Clear an FMAC read for use
	if (uregs.pipe == VUPIPE_FMAC || lregs.pipe == VUPIPE_FMAC)
		_vuClearFMAC(VU);

	_vuAddUpperStalls(VU, &uregs);
	_vuAddLowerStalls(VU, &lregs);

	if (VU->branch > 0)
	{
		if (VU->branch-- == 1)
		{
			VU->VI[REG_TPC].UL = VU->branchpc;

			if (VU->takedelaybranch)
			{
				//DevCon.Warning("VU1 - Branch/Jump in Delay Slot");
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
			VU0.VI[REG_VPU_STAT].UL &= ~0x100;
			vif1Regs.stat.VEW = false;
		}
	}

	if (VU->xgkickenable && (VU1.cycle - VU->xgkicklastcycle) >= 2)
	{
		if (VU->xgkicksizeremaining == 0)
		{
			u32 size = gifUnit.GetGSPacketSize(GIF_PATH_1, VU->Mem, VU->xgkickaddr);
			VU->xgkicksizeremaining = size & 0xFFFF;
			VU->xgkickendpacket = size >> 31;

			if (VU->xgkicksizeremaining == 0)
			{
				VUM_LOG("Invalid GS packet size returned, cancelling XGKick");
				VU->xgkickenable = false;
			}
			else
				VUM_LOG("XGKICK New tag size %d bytes EOP %d", VU->xgkicksizeremaining, VU->xgkickendpacket);
		}
		u32 transfersize = std::min(VU->xgkicksizeremaining / 0x10, (VU1.cycle - VU->xgkicklastcycle) / 2);
		transfersize = std::min(transfersize, VU->xgkickdiff / 0x10);

		if (transfersize)
		{
			VUM_LOG("XGKICK Transferring %x bytes from %x size left %x", transfersize * 0x10, VU->xgkickaddr, VU->xgkicksizeremaining);
			if ((transfersize * 0x10) > VU->xgkicksizeremaining)
				gifUnit.gifPath[GIF_PATH_1].CopyGSPacketData(&VU->Mem[VU->xgkickaddr], transfersize * 0x10, true);
			else
				gifUnit.TransferGSPacketData(GIF_TRANS_XGKICK, &VU->Mem[VU->xgkickaddr], transfersize * 0x10, true);

			VU->xgkickaddr = (VU->xgkickaddr + (transfersize * 0x10)) & 0x3FFF;
			VU->xgkicksizeremaining -= (transfersize * 0x10);
			VU->xgkickdiff = 0x4000 - VU->xgkickaddr;
			VU->xgkicklastcycle += std::max(transfersize * 2, 2U);

			if (VU->xgkicksizeremaining || !VU->xgkickendpacket)
				VUM_LOG("XGKICK next addr %x left size %x", VU->xgkickaddr, VU->xgkicksizeremaining);
			else
			{
				VUM_LOG("XGKICK transfer finished");
				VU->xgkickenable = false;
			}
		}
	}
	
	// Progress the write position of the FMAC pipeline by one place
	if (uregs.pipe == VUPIPE_FMAC || lregs.pipe == VUPIPE_FMAC)
		VU->fmacwritepos = ++VU->fmacwritepos & 3;
}

void vu1Exec(VURegs* VU)
{
	VU->cycle++;
	_vu1Exec(VU);

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

InterpVU1::InterpVU1()
{
	m_Idx = 1;
	IsInterpreter = true;
}

void InterpVU1::Reset()
{
	DevCon.Warning("VU1 Int Reset");
	VU1.fmacwritepos = 0;
	VU1.fmacreadpos = 0;
	VU1.fmaccount = 0;
	VU1.ialuwritepos = 0;
	VU1.ialureadpos = 0;
	VU1.ialucount = 0;
	vu1Thread.WaitVU();
}

void InterpVU1::Shutdown() noexcept
{
	vu1Thread.WaitVU();
}

void InterpVU1::SetStartPC(u32 startPC)
{
	VU1.start_pc = startPC;
}

void InterpVU1::Step()
{
	VU1.VI[REG_TPC].UL &= VU1_PROGMASK;
	vu1Exec(&VU1);
}

void InterpVU1::Execute(u32 cycles)
{
	const int originalRounding = fegetround();
	fesetround(g_sseVUMXCSR.RoundingControl << 8);

	VU1.VI[REG_TPC].UL <<= 3;
	u32 startcycles = VU1.cycle;
	while ((VU1.cycle - startcycles) < cycles)
	{
		if (!(VU0.VI[REG_VPU_STAT].UL & 0x100))
		{
			if (VU1.branch || VU1.ebit)
			{
				Step(); // run branch delay slot?
			}
			break;
		}
		Step();
	}
	VU1.VI[REG_TPC].UL >>= 3;

	fesetround(originalRounding);
}
