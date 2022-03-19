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
extern void _vu0WaitMicro();
extern void _vu0FinishMicro();

static VURegs& vu0Regs = vuRegs[0];

//------------------------------------------------------------------
// Macro VU - Helper Macros / Functions
//------------------------------------------------------------------

using namespace R5900::Dynarec;

#define printCOP2(...) (void)0
//#define printCOP2 DevCon.Status

void setupMacroOp(int mode, const char* opName)
{
	// Set up reg allocation
	microVU0.regAlloc->reset(true);

	if (mode & 0x110) // X86 regs are modified, or flags modified
	{
		_freeX86reg(eax);
		_freeX86reg(ecx);
		_freeX86reg(edx);
	}

	if (mode & 0x03) // Q will be read/written
		_freeXMMreg(xmmPQ.Id);

	// Set up MicroVU ready for new op
	printCOP2(opName);
	microVU0.cop2 = 1;
	microVU0.prog.IRinfo.curPC = 0;
	microVU0.code = cpuRegs.code;
	memset(&microVU0.prog.IRinfo.info[0], 0, sizeof(microVU0.prog.IRinfo.info[0]));
	
	if (mode & 0x01) // Q-Reg will be Read
	{
		xMOVSSZX(xmmPQ, ptr32[&vu0Regs.VI[REG_Q].UL]);
	}
	if (mode & 0x08 && (!CHECK_VU_FLAGHACK || g_pCurInstInfo->info & EEINST_COP2_CLIP_FLAG)) // Clip Instruction
	{
		microVU0.prog.IRinfo.info[0].cFlag.write     = 0xff;
		microVU0.prog.IRinfo.info[0].cFlag.lastWrite = 0xff;
	}
	if (mode & 0x10 && (!CHECK_VU_FLAGHACK || g_pCurInstInfo->info & EEINST_COP2_STATUS_FLAG)) // Update Status Flag
	{
		microVU0.prog.IRinfo.info[0].sFlag.doFlag      = true;
		microVU0.prog.IRinfo.info[0].sFlag.doNonSticky = true;
		microVU0.prog.IRinfo.info[0].sFlag.write       = 0;
		microVU0.prog.IRinfo.info[0].sFlag.lastWrite   = 0;
	}
	if (mode & 0x10 && (!CHECK_VU_FLAGHACK || g_pCurInstInfo->info & EEINST_COP2_MAC_FLAG)) // Update Mac Flags
	{
		microVU0.prog.IRinfo.info[0].mFlag.doFlag      = true;
		microVU0.prog.IRinfo.info[0].mFlag.write       = 0xff;
	}
	if (mode & 0x10)
	{
		_freeX86reg(gprF0);

		if (!CHECK_VU_FLAGHACK || (g_pCurInstInfo->info & EEINST_COP2_DENORMALIZE_STATUS_FLAG))
		{
			// flags are normalized, so denormalize before running the first instruction
			mVUallocSFLAGd(&vu0Regs.VI[REG_STATUS_FLAG].UL, gprF0, eax, ecx);
		}
		else
		{
			// load denormalized status flag
			// ideally we'd keep this in a register, but 32-bit...
			xMOV(gprF0, ptr32[&vuRegs->VI[REG_STATUS_FLAG].UL]);
		}
	}
}

void endMacroOp(int mode)
{
	if (mode & 0x02) // Q-Reg was Written To
	{
		xMOVSS(ptr32[&vu0Regs.VI[REG_Q].UL], xmmPQ);
	}

	microVU0.regAlloc->flushAll();
	_clearNeededCOP2Regs();

	if (mode & 0x10)
	{
		if (!CHECK_VU_FLAGHACK || g_pCurInstInfo->info & EEINST_COP2_NORMALIZE_STATUS_FLAG)
		{
			// Normalize
			mVUallocSFLAGc(eax, gprF0, 0);
			xMOV(ptr32[&vu0Regs.VI[REG_STATUS_FLAG].UL], eax);
		}
		else
		{
			// backup denormalized flags for the next instruction
			// this is fine, because we'll normalize them again before this reg is accessed
			xMOV(ptr32[&vuRegs->VI[REG_STATUS_FLAG].UL], gprF0);
		}
	}

	microVU0.cop2 = 0;
	microVU0.regAlloc->reset(false);
}

#define REC_COP2_mVU0(f, opName, mode) \
	void recV##f() \
	{ \
		int _mode = (mode); \
		setupMacroOp(_mode, opName); \
		if (_mode & 4) \
		{ \
			mVU_##f(microVU0, 0); \
			if (!microVU0.prog.IRinfo.info[0].lOp.isNOP) \
			{ \
				mVU_##f(microVU0, 1); \
			} \
		} \
		else \
		{ \
			mVU_##f(microVU0, 1); \
		} \
		endMacroOp(_mode); \
	}

#define INTERPRETATE_COP2_FUNC(f) \
	void recV##f() \
	{ \
		_freeX86reg(eax); \
		xMOV(eax, ptr32[&cpuRegs.cycle]); \
		xADD(eax, scaleblockcycles_clear()); \
		xMOV(ptr32[&cpuRegs.cycle], eax); \
		_cop2BackupRegs(); \
		recCall(V##f); \
		_cop2RestoreRegs(); \
	}

//------------------------------------------------------------------
// Macro VU - Instructions
//------------------------------------------------------------------

//------------------------------------------------------------------
// Macro VU - Redirect Upper Instructions
//------------------------------------------------------------------

/* Mode information
0x1  reads Q reg
0x2  writes Q reg
0x4  requires analysis pass
0x8  write CLIP
0x10 writes status/mac
0x100 requires x86 regs
*/

REC_COP2_mVU0(ABS,    "ABS",    0x0);
REC_COP2_mVU0(ITOF0,  "ITOF0",  0x0);
REC_COP2_mVU0(ITOF4,  "ITOF4",  0x0);
REC_COP2_mVU0(ITOF12, "ITOF12", 0x0);
REC_COP2_mVU0(ITOF15, "ITOF15", 0x0);
REC_COP2_mVU0(FTOI0,  "FTOI0",  0x0);
REC_COP2_mVU0(FTOI4,  "FTOI4",  0x0);
REC_COP2_mVU0(FTOI12, "FTOI12", 0x0);
REC_COP2_mVU0(FTOI15, "FTOI15", 0x0);
REC_COP2_mVU0(ADD,    "ADD",    0x110);
REC_COP2_mVU0(ADDi,   "ADDi",   0x110);
REC_COP2_mVU0(ADDq,   "ADDq",   0x111);
REC_COP2_mVU0(ADDx,   "ADDx",   0x110);
REC_COP2_mVU0(ADDy,   "ADDy",   0x110);
REC_COP2_mVU0(ADDz,   "ADDz",   0x110);
REC_COP2_mVU0(ADDw,   "ADDw",   0x110);
REC_COP2_mVU0(ADDA,   "ADDA",   0x110);
REC_COP2_mVU0(ADDAi,  "ADDAi",  0x110);
REC_COP2_mVU0(ADDAq,  "ADDAq",  0x111);
REC_COP2_mVU0(ADDAx,  "ADDAx",  0x110);
REC_COP2_mVU0(ADDAy,  "ADDAy",  0x110);
REC_COP2_mVU0(ADDAz,  "ADDAz",  0x110);
REC_COP2_mVU0(ADDAw,  "ADDAw",  0x110);
REC_COP2_mVU0(SUB,    "SUB",    0x110);
REC_COP2_mVU0(SUBi,   "SUBi",   0x110);
REC_COP2_mVU0(SUBq,   "SUBq",   0x111);
REC_COP2_mVU0(SUBx,   "SUBx",   0x110);
REC_COP2_mVU0(SUBy,   "SUBy",   0x110);
REC_COP2_mVU0(SUBz,   "SUBz",   0x110);
REC_COP2_mVU0(SUBw,   "SUBw",   0x110);
REC_COP2_mVU0(SUBA,   "SUBA",   0x110);
REC_COP2_mVU0(SUBAi,  "SUBAi",  0x110);
REC_COP2_mVU0(SUBAq,  "SUBAq",  0x111);
REC_COP2_mVU0(SUBAx,  "SUBAx",  0x110);
REC_COP2_mVU0(SUBAy,  "SUBAy",  0x110);
REC_COP2_mVU0(SUBAz,  "SUBAz",  0x110);
REC_COP2_mVU0(SUBAw,  "SUBAw",  0x110);
REC_COP2_mVU0(MUL,    "MUL",    0x110);
REC_COP2_mVU0(MULi,   "MULi",   0x110);
REC_COP2_mVU0(MULq,   "MULq",   0x111);
REC_COP2_mVU0(MULx,   "MULx",   0x110);
REC_COP2_mVU0(MULy,   "MULy",   0x110);
REC_COP2_mVU0(MULz,   "MULz",   0x110);
REC_COP2_mVU0(MULw,   "MULw",   0x110);
REC_COP2_mVU0(MULA,   "MULA",   0x110);
REC_COP2_mVU0(MULAi,  "MULAi",  0x110);
REC_COP2_mVU0(MULAq,  "MULAq",  0x111);
REC_COP2_mVU0(MULAx,  "MULAx",  0x110);
REC_COP2_mVU0(MULAy,  "MULAy",  0x110);
REC_COP2_mVU0(MULAz,  "MULAz",  0x110);
REC_COP2_mVU0(MULAw,  "MULAw",  0x110);
REC_COP2_mVU0(MAX,    "MAX",    0x0);
REC_COP2_mVU0(MAXi,   "MAXi",   0x0);
REC_COP2_mVU0(MAXx,   "MAXx",   0x0);
REC_COP2_mVU0(MAXy,   "MAXy",   0x0);
REC_COP2_mVU0(MAXz,   "MAXz",   0x0);
REC_COP2_mVU0(MAXw,   "MAXw",   0x0);
REC_COP2_mVU0(MINI,   "MINI",   0x0);
REC_COP2_mVU0(MINIi,  "MINIi",  0x0);
REC_COP2_mVU0(MINIx,  "MINIx",  0x0);
REC_COP2_mVU0(MINIy,  "MINIy",  0x0);
REC_COP2_mVU0(MINIz,  "MINIz",  0x0);
REC_COP2_mVU0(MINIw,  "MINIw",  0x0);
REC_COP2_mVU0(MADD,   "MADD",   0x110);
REC_COP2_mVU0(MADDi,  "MADDi",  0x110);
REC_COP2_mVU0(MADDq,  "MADDq",  0x111);
REC_COP2_mVU0(MADDx,  "MADDx",  0x110);
REC_COP2_mVU0(MADDy,  "MADDy",  0x110);
REC_COP2_mVU0(MADDz,  "MADDz",  0x110);
REC_COP2_mVU0(MADDw,  "MADDw",  0x110);
REC_COP2_mVU0(MADDA,  "MADDA",  0x110);
REC_COP2_mVU0(MADDAi, "MADDAi", 0x110);
REC_COP2_mVU0(MADDAq, "MADDAq", 0x111);
REC_COP2_mVU0(MADDAx, "MADDAx", 0x110);
REC_COP2_mVU0(MADDAy, "MADDAy", 0x110);
REC_COP2_mVU0(MADDAz, "MADDAz", 0x110);
REC_COP2_mVU0(MADDAw, "MADDAw", 0x110);
REC_COP2_mVU0(MSUB,   "MSUB",   0x110);
REC_COP2_mVU0(MSUBi,  "MSUBi",  0x110);
REC_COP2_mVU0(MSUBq,  "MSUBq",  0x111);
REC_COP2_mVU0(MSUBx,  "MSUBx",  0x110);
REC_COP2_mVU0(MSUBy,  "MSUBy",  0x110);
REC_COP2_mVU0(MSUBz,  "MSUBz",  0x110);
REC_COP2_mVU0(MSUBw,  "MSUBw",  0x110);
REC_COP2_mVU0(MSUBA,  "MSUBA",  0x110);
REC_COP2_mVU0(MSUBAi, "MSUBAi", 0x110);
REC_COP2_mVU0(MSUBAq, "MSUBAq", 0x111);
REC_COP2_mVU0(MSUBAx, "MSUBAx", 0x110);
REC_COP2_mVU0(MSUBAy, "MSUBAy", 0x110);
REC_COP2_mVU0(MSUBAz, "MSUBAz", 0x110);
REC_COP2_mVU0(MSUBAw, "MSUBAw", 0x110);
REC_COP2_mVU0(OPMULA, "OPMULA", 0x110);
REC_COP2_mVU0(OPMSUB, "OPMSUB", 0x110);
REC_COP2_mVU0(CLIP,   "CLIP",   0x108);

//------------------------------------------------------------------
// Macro VU - Redirect Lower Instructions
//------------------------------------------------------------------

REC_COP2_mVU0(DIV,   "DIV",   0x112);
REC_COP2_mVU0(SQRT,  "SQRT",  0x112);
REC_COP2_mVU0(RSQRT, "RSQRT", 0x112);
REC_COP2_mVU0(IADD,  "IADD",  0x104);
REC_COP2_mVU0(IADDI, "IADDI", 0x104);
REC_COP2_mVU0(IAND,  "IAND",  0x104);
REC_COP2_mVU0(IOR,   "IOR",   0x104);
REC_COP2_mVU0(ISUB,  "ISUB",  0x104);
REC_COP2_mVU0(ILWR,  "ILWR",  0x104);
REC_COP2_mVU0(ISWR,  "ISWR",  0x100);
REC_COP2_mVU0(LQI,   "LQI",   0x104);
REC_COP2_mVU0(LQD,   "LQD",   0x104);
REC_COP2_mVU0(SQI,   "SQI",   0x100);
REC_COP2_mVU0(SQD,   "SQD",   0x100);
REC_COP2_mVU0(MFIR,  "MFIR",  0x104);
REC_COP2_mVU0(MTIR,  "MTIR",  0x104);
REC_COP2_mVU0(MOVE,  "MOVE",  0x0);
REC_COP2_mVU0(MR32,  "MR32",  0x0);
REC_COP2_mVU0(RINIT, "RINIT", 0x100);
REC_COP2_mVU0(RGET,  "RGET",  0x104);
REC_COP2_mVU0(RNEXT, "RNEXT", 0x104);
REC_COP2_mVU0(RXOR,  "RXOR",  0x100);

//------------------------------------------------------------------
// Macro VU - Misc...
//------------------------------------------------------------------

void recVNOP() {}
void recVWAITQ() {}
INTERPRETATE_COP2_FUNC(CALLMS);
INTERPRETATE_COP2_FUNC(CALLMSR);

//------------------------------------------------------------------
// Macro VU - Branches
//------------------------------------------------------------------

void _setupBranchTest(u32*(jmpType)(u32), bool isLikely)
{
	printCOP2("COP2 Branch");
	_eeFlushAllUnused();
	//xTEST(ptr32[&vif1Regs.stat._u32], 0x4);
	xTEST(ptr32[&VU0.VI[REG_VPU_STAT].UL], 0x100);
	recDoBranchImm(jmpType(0), isLikely);
}

void recBC2F()  { _setupBranchTest(JNZ32, false); }
void recBC2T()  { _setupBranchTest(JZ32,  false); }
void recBC2FL() { _setupBranchTest(JNZ32, true);  }
void recBC2TL() { _setupBranchTest(JZ32,  true);  }

//------------------------------------------------------------------
// Macro VU - COP2 Transfer Instructions
//------------------------------------------------------------------

void COP2_Interlock(bool mBitSync)
{

	if (cpuRegs.code & 1)
	{
		s_nBlockInterlocked = true;
		_freeX86reg(eax);
		xMOV(eax, ptr32[&cpuRegs.cycle]);
		xADD(eax, scaleblockcycles_clear());
		xMOV(ptr32[&cpuRegs.cycle], eax); // update cycles

		xTEST(ptr32[&VU0.VI[REG_VPU_STAT].UL], 0x1);
		xForwardJZ32 skipvuidle;
		_cop2BackupRegs();
		if (mBitSync)
		{
			xSUB(eax, ptr32[&VU0.cycle]);
			xSUB(eax, ptr32[&VU0.nextBlockCycles]);
			xCMP(eax, 4);
			xForwardJL32 skip;
			xLoadFarAddr(arg1reg, CpuVU0);
			xMOV(arg2reg, s_nBlockInterlocked);
			xFastCall((void*)BaseVUmicroCPU::ExecuteBlockJIT, arg1reg, arg2reg);
			skip.SetTarget();

			xFastCall((void*)_vu0WaitMicro);
		}
		else
			xFastCall((void*)_vu0FinishMicro);
		_cop2RestoreRegs();
		skipvuidle.SetTarget();
	}
}

void TEST_FBRST_RESET(FnType_Void* resetFunct, int vuIndex)
{
	xTEST(eax, (vuIndex) ? 0x200 : 0x002);
	xForwardJZ8 skip;
		xFastCall((void*)resetFunct);
		xMOV(eax, ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
	skip.SetTarget();
}

static void recCFC2()
{
	printCOP2("CFC2");

	COP2_Interlock(false);

	if (!_Rt_)
		return;

	if (!(cpuRegs.code & 1))
	{
		_freeX86reg(eax);
		xMOV(eax, ptr32[&cpuRegs.cycle]);
		xADD(eax, scaleblockcycles_clear());
		xMOV(ptr32[&cpuRegs.cycle], eax); // update cycles

		xTEST(ptr32[&VU0.VI[REG_VPU_STAT].UL], 0x1);
		xForwardJZ32 skipvuidle;
		xSUB(eax, ptr32[&VU0.cycle]);
		xSUB(eax, ptr32[&VU0.nextBlockCycles]);
		xCMP(eax, 4);
		xForwardJL32 skip;
		_cop2BackupRegs();
		xLoadFarAddr(arg1reg, CpuVU0);
		xMOV(arg2reg, s_nBlockInterlocked);
		xFastCall((void*)BaseVUmicroCPU::ExecuteBlockJIT, arg1reg, arg2reg);
		_cop2RestoreRegs();
		skip.SetTarget();
		skipvuidle.SetTarget();
	}

	_flushEEreg(_Rt_, true);

	if (_Rd_ == REG_STATUS_FLAG) // Normalize Status Flag
		xMOV(eax, ptr32[&vu0Regs.VI[REG_STATUS_FLAG].UL]);
	else
		xMOV(eax, ptr32[&vu0Regs.VI[_Rd_].UL]);

	// FixMe: Should R-Reg have upper 9 bits 0?
	if (_Rd_ >= 16)
		xCDQE(); // Sign Extend

	xMOV(ptr64[&cpuRegs.GPR.r[_Rt_].UD[0]], rax);

	// FixMe: I think this is needed, but not sure how it works
	// Update Refraction 20/09/2021: This is needed because Const Prop is broken
	// the Flushed flag isn't being cleared when it's not flushed. TODO I guess
	_eeOnWriteReg(_Rt_, 0);
}

static void recCTC2()
{
	printCOP2("CTC2");

	COP2_Interlock(1);

	if (!_Rd_)
		return;

	if (!(cpuRegs.code & 1))
	{
		_freeX86reg(eax);
		xMOV(eax, ptr32[&cpuRegs.cycle]);
		xADD(eax, scaleblockcycles_clear());
		xMOV(ptr32[&cpuRegs.cycle], eax); // update cycles

		xTEST(ptr32[&VU0.VI[REG_VPU_STAT].UL], 0x1);
		xForwardJZ32 skipvuidle;
		xSUB(eax, ptr32[&VU0.cycle]);
		xSUB(eax, ptr32[&VU0.nextBlockCycles]);
		xCMP(eax, 4);
		xForwardJL32 skip;
		_cop2BackupRegs();
		xLoadFarAddr(arg1reg, CpuVU0);
		xMOV(arg2reg, s_nBlockInterlocked);
		xFastCall((void*)BaseVUmicroCPU::ExecuteBlockJIT, arg1reg, arg2reg);
		_cop2RestoreRegs();
		skip.SetTarget();
		skipvuidle.SetTarget();
	}

	_flushEEreg(_Rt_);

	switch (_Rd_)
	{
		case REG_MAC_FLAG:
		case REG_TPC:
		case REG_VPU_STAT:
			break; // Read Only Regs
		case REG_R:
			xMOV(eax, ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
			xOR(eax, 0x3f800000);
			xMOV(ptr32[&vu0Regs.VI[REG_R].UL], eax);
			break;
		case REG_STATUS_FLAG:
		{
			if (_Rt_)
			{
				xMOV(eax, ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
				xAND(eax, 0xFC0);
				xAND(ptr32[&vu0Regs.VI[REG_STATUS_FLAG].UL], 0x3F);
				xOR(ptr32[&vu0Regs.VI[REG_STATUS_FLAG].UL], eax);
			}
			else
				xAND(ptr32[&vu0Regs.VI[REG_STATUS_FLAG].UL], 0x3F);

			_freeXMMreg(xmmT1.Id);
			//Need to update the sticky flags for microVU
			mVUallocSFLAGd(&vu0Regs.VI[REG_STATUS_FLAG].UL);
			xMOVDZX(xmmT1, eax);
			xSHUF.PS(xmmT1, xmmT1, 0);
			// Make sure the values are everywhere the need to be
			xMOVAPS(ptr128[&vu0Regs.micro_statusflags], xmmT1);
			break;
		}
		case REG_CMSAR1: // Execute VU1 Micro SubRoutine
			_cop2BackupRegs();
			xMOV(ecx, 1);
			xFastCall((void*)vu1Finish, ecx);
			if (_Rt_)
			{
				xMOV(ecx, ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
			}
			else
				xXOR(ecx, ecx);
			xFastCall((void*)vu1ExecMicro, ecx);
			_cop2RestoreRegs();
			break;
		case REG_FBRST:
			if (!_Rt_)
			{
				xMOV(ptr32[&vu0Regs.VI[REG_FBRST].UL], 0);
				return;
			}
			else
				xMOV(eax, ptr32[&cpuRegs.GPR.r[_Rt_].UL[0]]);
			_cop2BackupRegs();
			TEST_FBRST_RESET(vu0ResetRegs, 0);
			TEST_FBRST_RESET(vu1ResetRegs, 1);
			_cop2RestoreRegs();
			xAND(eax, 0x0C0C);
			xMOV(ptr32[&vu0Regs.VI[REG_FBRST].UL], eax);
			break;
		default:
			// Executing vu0 block here fixes the intro of Ratchet and Clank
			// sVU's COP2 has a comment that "Donald Duck" needs this too...
			if (_Rd_)
				_eeMoveGPRtoM((uptr)&vu0Regs.VI[_Rd_].UL, _Rt_);
			break;
	}
}

static void recQMFC2()
{

	printCOP2("QMFC2");

	COP2_Interlock(false);

	if (!_Rt_)
		return;
	
	if (!(cpuRegs.code & 1))
	{
		_freeX86reg(eax);
		xMOV(eax, ptr32[&cpuRegs.cycle]);
		xADD(eax, scaleblockcycles_clear());
		xMOV(ptr32[&cpuRegs.cycle], eax); // update cycles

		xTEST(ptr32[&VU0.VI[REG_VPU_STAT].UL], 0x1);
		xForwardJZ32 skipvuidle;
		xSUB(eax, ptr32[&VU0.cycle]);
		xSUB(eax, ptr32[&VU0.nextBlockCycles]);
		xCMP(eax, 4);
		xForwardJL32 skip;
		_cop2BackupRegs();
		xLoadFarAddr(arg1reg, CpuVU0);
		xMOV(arg2reg, s_nBlockInterlocked);
		xFastCall((void*)BaseVUmicroCPU::ExecuteBlockJIT, arg1reg, arg2reg);
		_cop2RestoreRegs();
		skip.SetTarget();
		skipvuidle.SetTarget();
	}

	int rtreg = _allocGPRtoXMMreg(-1, _Rt_, MODE_WRITE);
	int t0reg = _allocTempXMMreg(XMMT_INT, -1);
	// Update Refraction 20/09/2021: This is needed because Const Prop is broken
	// the Flushed flag isn't being cleared when it's not flushed. TODO I guess
	_eeOnWriteReg(_Rt_, 0); // This is needed because Const Prop is broken

	xMOVAPS(xRegisterSSE(t0reg), ptr128[&vu0Regs.VF[_Rd_]]);
	xMOVAPS(xRegisterSSE(rtreg), xRegisterSSE(t0reg));
	_freeXMMreg(t0reg);
}

static void recQMTC2()
{
	printCOP2("QMTC2");
	COP2_Interlock(true);

	if (!_Rd_)
		return;
	
	if (!(cpuRegs.code & 1))
	{
		_freeX86reg(eax);
		xMOV(eax, ptr32[&cpuRegs.cycle]);
		xADD(eax, scaleblockcycles_clear());
		xMOV(ptr32[&cpuRegs.cycle], eax); // update cycles

		xTEST(ptr32[&VU0.VI[REG_VPU_STAT].UL], 0x1);
		xForwardJZ32 skipvuidle;
		xSUB(eax, ptr32[&VU0.cycle]);
		xSUB(eax, ptr32[&VU0.nextBlockCycles]);
		xCMP(eax, 4);
		xForwardJL32 skip;
		_cop2BackupRegs();
		xLoadFarAddr(arg1reg, CpuVU0);
		xMOV(arg2reg, s_nBlockInterlocked);
		xFastCall((void*)BaseVUmicroCPU::ExecuteBlockJIT, arg1reg, arg2reg);
		_cop2RestoreRegs();
		skip.SetTarget();
		skipvuidle.SetTarget();
	}

	int rtreg = _allocGPRtoXMMreg(-1, _Rt_, MODE_READ);
	int t0reg = _allocTempXMMreg(XMMT_INT, -1);

	xMOVAPS(xRegisterSSE(t0reg), xRegisterSSE(rtreg));
	xMOVAPS(ptr128[&vu0Regs.VF[_Rd_]], xRegisterSSE(t0reg));
	_freeXMMreg(t0reg);
}

//------------------------------------------------------------------
// Macro VU - Tables
//------------------------------------------------------------------

void recCOP2();
void recCOP2_BC2();
void recCOP2_SPEC1();
void recCOP2_SPEC2();
void rec_C2UNK()
{
	Console.Error("Cop2 bad opcode: %x", cpuRegs.code);
}

// Recompilation
void (*recCOP2t[32])() = {
	rec_C2UNK,     recQMFC2,      recCFC2,       rec_C2UNK,     rec_C2UNK,     recQMTC2,      recCTC2,       rec_C2UNK,
	recCOP2_BC2,   rec_C2UNK,     rec_C2UNK,     rec_C2UNK,     rec_C2UNK,     rec_C2UNK,     rec_C2UNK,     rec_C2UNK,
	recCOP2_SPEC1, recCOP2_SPEC1, recCOP2_SPEC1, recCOP2_SPEC1, recCOP2_SPEC1, recCOP2_SPEC1, recCOP2_SPEC1, recCOP2_SPEC1,
	recCOP2_SPEC1, recCOP2_SPEC1, recCOP2_SPEC1, recCOP2_SPEC1, recCOP2_SPEC1, recCOP2_SPEC1, recCOP2_SPEC1, recCOP2_SPEC1,
};

void (*recCOP2_BC2t[32])() = {
	recBC2F,   recBC2T,   recBC2FL,  recBC2TL,  rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK,
	rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK,
	rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK,
	rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK,
};

void (*recCOP2SPECIAL1t[64])() = {
	recVADDx,   recVADDy,   recVADDz,  recVADDw,  recVSUBx,      recVSUBy,      recVSUBz,      recVSUBw,
	recVMADDx,  recVMADDy,  recVMADDz, recVMADDw, recVMSUBx,     recVMSUBy,     recVMSUBz,     recVMSUBw,
	recVMAXx,   recVMAXy,   recVMAXz,  recVMAXw,  recVMINIx,     recVMINIy,     recVMINIz,     recVMINIw,
	recVMULx,   recVMULy,   recVMULz,  recVMULw,  recVMULq,      recVMAXi,      recVMULi,      recVMINIi,
	recVADDq,   recVMADDq,  recVADDi,  recVMADDi, recVSUBq,      recVMSUBq,     recVSUBi,      recVMSUBi,
	recVADD,    recVMADD,   recVMUL,   recVMAX,   recVSUB,       recVMSUB,      recVOPMSUB,    recVMINI,
	recVIADD,   recVISUB,   recVIADDI, rec_C2UNK, recVIAND,      recVIOR,       rec_C2UNK,     rec_C2UNK,
	recVCALLMS, recVCALLMSR,rec_C2UNK, rec_C2UNK, recCOP2_SPEC2, recCOP2_SPEC2, recCOP2_SPEC2, recCOP2_SPEC2,
};

void (*recCOP2SPECIAL2t[128])() = {
	recVADDAx,  recVADDAy, recVADDAz,  recVADDAw,  recVSUBAx,  recVSUBAy,  recVSUBAz,  recVSUBAw,
	recVMADDAx,recVMADDAy, recVMADDAz, recVMADDAw, recVMSUBAx, recVMSUBAy, recVMSUBAz, recVMSUBAw,
	recVITOF0,  recVITOF4, recVITOF12, recVITOF15, recVFTOI0,  recVFTOI4,  recVFTOI12, recVFTOI15,
	recVMULAx,  recVMULAy, recVMULAz,  recVMULAw,  recVMULAq,  recVABS,    recVMULAi,  recVCLIP,
	recVADDAq,  recVMADDAq,recVADDAi,  recVMADDAi, recVSUBAq,  recVMSUBAq, recVSUBAi,  recVMSUBAi,
	recVADDA,   recVMADDA, recVMULA,   rec_C2UNK,  recVSUBA,   recVMSUBA,  recVOPMULA, recVNOP,
	recVMOVE,   recVMR32,  rec_C2UNK,  rec_C2UNK,  recVLQI,    recVSQI,    recVLQD,    recVSQD,
	recVDIV,    recVSQRT,  recVRSQRT,  recVWAITQ,  recVMTIR,   recVMFIR,   recVILWR,   recVISWR,
	recVRNEXT,  recVRGET,  recVRINIT,  recVRXOR,   rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,
	rec_C2UNK,  rec_C2UNK, rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,
	rec_C2UNK,  rec_C2UNK, rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,
	rec_C2UNK,  rec_C2UNK, rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,
	rec_C2UNK,  rec_C2UNK, rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,
	rec_C2UNK,  rec_C2UNK, rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,
	rec_C2UNK,  rec_C2UNK, rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,
	rec_C2UNK,  rec_C2UNK, rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,
};

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {
	void recCOP2() { recCOP2t[_Rs_](); }
} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
void recCOP2_BC2() { recCOP2_BC2t[_Rt_](); }
void recCOP2_SPEC1()
{
	xTEST(ptr32[&VU0.VI[REG_VPU_STAT].UL], 0x1);
	xForwardJZ32 skipvuidle;
	_cop2BackupRegs();
	xFastCall((void*)_vu0FinishMicro);
	_cop2RestoreRegs();
	skipvuidle.SetTarget();

	recCOP2SPECIAL1t[_Funct_]();

}
void recCOP2_SPEC2() { recCOP2SPECIAL2t[(cpuRegs.code & 3) | ((cpuRegs.code >> 4) & 0x7c)](); }
