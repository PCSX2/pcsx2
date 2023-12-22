// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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

// For now, we need to free all XMMs. Because we're not saving the nonvolatile registers when
// we enter micro mode, they will get overriden otherwise...
#define FLUSH_FOR_POSSIBLE_MICRO_EXEC (FLUSH_FREE_XMM | FLUSH_FREE_VU0)

void setupMacroOp(int mode, const char* opName)
{
	// Set up reg allocation
	microVU0.regAlloc->reset(true);

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
	if (mode & 0x10 && (!CHECK_VU_FLAGHACK || g_pCurInstInfo->info & (EEINST_COP2_STATUS_FLAG | EEINST_COP2_DENORMALIZE_STATUS_FLAG)))
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

	microVU0.regAlloc->flushPartialForCOP2();

	if (mode & 0x10)
	{
		if (!CHECK_VU_FLAGHACK || g_pCurInstInfo->info & EEINST_COP2_NORMALIZE_STATUS_FLAG)
		{
			// Normalize
			mVUallocSFLAGc(eax, gprF0, 0);
			xMOV(ptr32[&vu0Regs.VI[REG_STATUS_FLAG].UL], eax);
		}
		else if (g_pCurInstInfo->info & (EEINST_COP2_STATUS_FLAG | EEINST_COP2_DENORMALIZE_STATUS_FLAG))
		{
			// backup denormalized flags for the next instruction
			// this is fine, because we'll normalize them again before this reg is accessed
			xMOV(ptr32[&vuRegs->VI[REG_STATUS_FLAG].UL], gprF0);
		}
	}

	microVU0.cop2 = 0;
	microVU0.regAlloc->reset(false);
}

void mVUFreeCOP2XMMreg(int hostreg)
{
	microVU0.regAlloc->clearRegCOP2(hostreg);
}

void mVUFreeCOP2GPR(int hostreg)
{
	microVU0.regAlloc->clearGPRCOP2(hostreg);
}

bool mVUIsReservedCOP2(int hostreg)
{
	// gprF1 through 3 is not correctly used in COP2 mode.
	return (hostreg == gprT1.GetId() || hostreg == gprT2.GetId() || hostreg == gprF0.GetId());
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
		iFlushCall(FLUSH_FOR_POSSIBLE_MICRO_EXEC); \
		xADD(ptr32[&cpuRegs.cycle], scaleblockcycles_clear()); \
		recCall(V##f); \
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

static void _setupBranchTest(u32*(jmpType)(u32), bool isLikely)
{
	printCOP2("COP2 Branch");
	const u32 branchTo = ((s32)_Imm_ * 4) + pc;
	const bool swap = isLikely ? false : TrySwapDelaySlot(0, 0, 0, false);
	_eeFlushAllDirty();
	//xTEST(ptr32[&vif1Regs.stat._u32], 0x4);
	xTEST(ptr32[&VU0.VI[REG_VPU_STAT].UL], 0x100);
	recDoBranchImm(branchTo, jmpType(0), isLikely, swap);
}

void recBC2F()  { _setupBranchTest(JNZ32, false); }
void recBC2T()  { _setupBranchTest(JZ32,  false); }
void recBC2FL() { _setupBranchTest(JNZ32, true);  }
void recBC2TL() { _setupBranchTest(JZ32,  true);  }

//------------------------------------------------------------------
// Macro VU - COP2 Transfer Instructions
//------------------------------------------------------------------

static void COP2_Interlock(bool mBitSync)
{
	if (cpuRegs.code & 1)
	{
		s_nBlockInterlocked = true;

		// We can safely skip the _vu0FinishMicro() call, when there's nothing
		// that can trigger a VU0 program between CFC2/CTC2/COP2 instructions.
		if (g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0)
		{
			iFlushCall(FLUSH_FOR_POSSIBLE_MICRO_EXEC);
			_freeX86reg(eax);
			xMOV(eax, ptr32[&cpuRegs.cycle]);
			xADD(eax, scaleblockcycles_clear());
			xMOV(ptr32[&cpuRegs.cycle], eax); // update cycles

			xTEST(ptr32[&VU0.VI[REG_VPU_STAT].UL], 0x1);
			xForwardJZ32 skipvuidle;
			if (mBitSync)
			{
				xSUB(eax, ptr32[&VU0.cycle]);

				// Why do we check this here? Ratchet games, maybe others end up with flickering polygons
				// when we use lazy COP2 sync, otherwise. The micro resumption getting deferred an extra
				// EE block is apparently enough to cause issues.
				if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
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
			skipvuidle.SetTarget();
		}
	}
}

static void mVUSyncVU0()
{
	iFlushCall(FLUSH_FOR_POSSIBLE_MICRO_EXEC);
	_freeX86reg(eax);
	xMOV(eax, ptr32[&cpuRegs.cycle]);
	xADD(eax, scaleblockcycles_clear());
	xMOV(ptr32[&cpuRegs.cycle], eax); // update cycles

	xTEST(ptr32[&VU0.VI[REG_VPU_STAT].UL], 0x1);
	xForwardJZ32 skipvuidle;
	xSUB(eax, ptr32[&VU0.cycle]);
	if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
		xSUB(eax, ptr32[&VU0.nextBlockCycles]);
	xCMP(eax, 4);
	xForwardJL32 skip;
	xLoadFarAddr(arg1reg, CpuVU0);
	xMOV(arg2reg, s_nBlockInterlocked);
	xFastCall((void*)BaseVUmicroCPU::ExecuteBlockJIT, arg1reg, arg2reg);
	skip.SetTarget();
	skipvuidle.SetTarget();
}

static void mVUFinishVU0()
{
	iFlushCall(FLUSH_FOR_POSSIBLE_MICRO_EXEC);
	xTEST(ptr32[&VU0.VI[REG_VPU_STAT].UL], 0x1);
	xForwardJZ32 skipvuidle;
	xFastCall((void*)_vu0FinishMicro);
	skipvuidle.SetTarget();
}

static void TEST_FBRST_RESET(int flagreg, void(*resetFunct)(), int vuIndex)
{
	xTEST(xRegister32(flagreg), (vuIndex) ? 0x200 : 0x002);
	xForwardJZ8 skip;
		xFastCall((void*)resetFunct);
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
		if (g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0)
			mVUSyncVU0();
		else if (g_pCurInstInfo->info & EEINST_COP2_FINISH_VU0)
			mVUFinishVU0();
	}

	const int regt = _allocX86reg(X86TYPE_GPR, _Rt_, MODE_WRITE);
	pxAssert(!GPR_IS_CONST1(_Rt_));

	if (_Rd_ == 0) // why would you read vi00?
	{
		xXOR(xRegister32(regt), xRegister32(regt));
	}
	else if (_Rd_ == REG_I)
	{
		const int xmmreg = _checkXMMreg(XMMTYPE_VFREG, 33, MODE_READ);
		if (xmmreg >= 0)
		{
			xMOVD(xRegister32(regt), xRegisterSSE(xmmreg));
			xMOVSX(xRegister64(regt), xRegister32(regt));
		}
		else
		{
			xMOVSX(xRegister64(regt), ptr32[&vu0Regs.VI[_Rd_].UL]);
		}
	}
	else if (_Rd_ == REG_R)
	{
		xMOVSX(xRegister64(regt), ptr32[&vu0Regs.VI[REG_R].UL]);
		xAND(xRegister64(regt), 0x7FFFFF);
	}
	else if (_Rd_ >= REG_STATUS_FLAG) // FixMe: Should R-Reg have upper 9 bits 0?
	{
		xMOVSX(xRegister64(regt), ptr32[&vu0Regs.VI[_Rd_].UL]);
	}
	else
	{
		const int vireg = _allocIfUsedVItoX86(_Rd_, MODE_READ);
		if (vireg >= 0)
			xMOVZX(xRegister32(regt), xRegister16(vireg));
		else
			xMOVZX(xRegister32(regt), ptr16[&vu0Regs.VI[_Rd_].UL]);
	}
}

static void recCTC2()
{
	printCOP2("CTC2");

	COP2_Interlock(1);

	if (!_Rd_)
		return;

	if (!(cpuRegs.code & 1))
	{
		if (g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0)
			mVUSyncVU0();
		else if (g_pCurInstInfo->info & EEINST_COP2_FINISH_VU0)
			mVUFinishVU0();
	}

	switch (_Rd_)
	{
		case REG_MAC_FLAG:
		case REG_TPC:
		case REG_VPU_STAT:
			break; // Read Only Regs
		case REG_R:
			_eeMoveGPRtoR(eax, _Rt_);
			xAND(eax, 0x7FFFFF);
			xOR(eax, 0x3f800000);
			xMOV(ptr32[&vu0Regs.VI[REG_R].UL], eax);
			break;
		case REG_STATUS_FLAG:
		{
			if (_Rt_)
			{
				_eeMoveGPRtoR(eax, _Rt_);
				xAND(eax, 0xFC0);
				xAND(ptr32[&vu0Regs.VI[REG_STATUS_FLAG].UL], 0x3F);
				xOR(ptr32[&vu0Regs.VI[REG_STATUS_FLAG].UL], eax);
			}
			else
				xAND(ptr32[&vu0Regs.VI[REG_STATUS_FLAG].UL], 0x3F);

			const int xmmtemp = _allocTempXMMreg(XMMT_INT);

			//Need to update the sticky flags for microVU
			mVUallocSFLAGd(&vu0Regs.VI[REG_STATUS_FLAG].UL);
			xMOVDZX(xRegisterSSE(xmmtemp), eax); // TODO(Stenzek): This can be a broadcast.
			xSHUF.PS(xRegisterSSE(xmmtemp), xRegisterSSE(xmmtemp), 0);
			// Make sure the values are everywhere the need to be
			xMOVAPS(ptr128[&vu0Regs.micro_statusflags], xRegisterSSE(xmmtemp));
			_freeXMMreg(xmmtemp);
			break;
		}
		case REG_CMSAR1: // Execute VU1 Micro SubRoutine
			iFlushCall(FLUSH_NONE);
			xMOV(arg1regd, 1);
			xFastCall((void*)vu1Finish);
			_eeMoveGPRtoR(arg1regd, _Rt_);
			iFlushCall(FLUSH_NONE);
			xFastCall((void*)vu1ExecMicro);
			break;
		case REG_FBRST:
			{
				if (!_Rt_)
				{
					xMOV(ptr32[&vu0Regs.VI[REG_FBRST].UL], 0);
					return;
				}

				const int flagreg = _allocX86reg(X86TYPE_TEMP, 0, MODE_CALLEESAVED);
				_eeMoveGPRtoR(xRegister32(flagreg), _Rt_);

				iFlushCall(FLUSH_FREE_VU0);
				TEST_FBRST_RESET(flagreg, vu0ResetRegs, 0);
				TEST_FBRST_RESET(flagreg, vu1ResetRegs, 1);

				xAND(xRegister32(flagreg), 0x0C0C);
				xMOV(ptr32[&vu0Regs.VI[REG_FBRST].UL], xRegister32(flagreg));
				_freeX86reg(flagreg);
			}
			break;
		case 0:
			// Ignore writes to vi00.
			break;
		default:
			// Executing vu0 block here fixes the intro of Ratchet and Clank
			// sVU's COP2 has a comment that "Donald Duck" needs this too...
			if (_Rd_ < REG_STATUS_FLAG)
			{
				// Little bit nasty, but optimal codegen.
				const int gprreg = _allocIfUsedGPRtoX86(_Rt_, MODE_READ);
				const int vireg = _allocIfUsedVItoX86(_Rd_, MODE_WRITE);
				if (vireg >= 0)
				{
					if (gprreg >= 0)
					{
						xMOVZX(xRegister32(vireg), xRegister16(gprreg));
					}
					else
					{
						// it could be in an xmm..
						const int gprxmmreg = _checkXMMreg(XMMTYPE_GPRREG, _Rt_, MODE_READ);
						if (gprxmmreg >= 0)
						{
							xMOVD(xRegister32(vireg), xRegisterSSE(gprxmmreg));
							xMOVZX(xRegister32(vireg), xRegister16(vireg));
						}
						else if (GPR_IS_CONST1(_Rt_))
						{
							if (_Rt_ != 0)
								xMOV(xRegister32(vireg), (g_cpuConstRegs[_Rt_].UL[0] & 0xFFFFu));
							else
								xXOR(xRegister32(vireg), xRegister32(vireg));
						}
						else
						{
							xMOVZX(xRegister32(vireg), ptr16[&cpuRegs.GPR.r[_Rt_].US[0]]);
						}
					}
				}
				else
				{
					if (gprreg >= 0)
					{
						xMOV(ptr16[&vu0Regs.VI[_Rd_].US[0]], xRegister16(gprreg));
					}
					else
					{
						const int gprxmmreg = _checkXMMreg(XMMTYPE_GPRREG, _Rt_, MODE_READ);
						if (gprxmmreg >= 0)
						{
							xMOVD(eax, xRegisterSSE(gprxmmreg));
							xMOV(ptr16[&vu0Regs.VI[_Rd_].US[0]], ax);
						}
						else if (GPR_IS_CONST1(_Rt_))
						{
							xMOV(ptr16[&vu0Regs.VI[_Rd_].US[0]], (g_cpuConstRegs[_Rt_].UL[0] & 0xFFFFu));
						}
						else
						{
							_eeMoveGPRtoR(eax, _Rt_);
							xMOV(ptr16[&vu0Regs.VI[_Rd_].US[0]], ax);
						}
					}
				}
			}
			else
			{
				// Move I direct to FPR if used.
				if (_Rd_ == REG_I)
				{
					const int xmmreg = _allocVFtoXMMreg(33, MODE_WRITE);
					if (_Rt_ == 0)
					{
						xPXOR(xRegisterSSE(xmmreg), xRegisterSSE(xmmreg));
					}
					else
					{
						const int xmmgpr = _checkXMMreg(XMMTYPE_GPRREG, _Rt_, MODE_READ);
						if (xmmgpr >= 0)
						{
							xPSHUF.D(xRegisterSSE(xmmreg), xRegisterSSE(xmmgpr), 0);
						}
						else
						{
							const int gprreg = _allocX86reg(X86TYPE_GPR, _Rt_, MODE_READ);
							if (gprreg >= 0)
								xMOVDZX(xRegisterSSE(xmmreg), xRegister32(gprreg));
							else
								xMOVSSZX(xRegisterSSE(xmmreg), ptr32[&cpuRegs.GPR.r[_Rt_].SD[0]]);
							xSHUF.PS(xRegisterSSE(xmmreg), xRegisterSSE(xmmreg), 0);
						}
					}
				}
				else
				{
					_eeMoveGPRtoM((uptr)&vu0Regs.VI[_Rd_].UL, _Rt_);
				}
			}
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
		if (g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0)
			mVUSyncVU0();
		else if (g_pCurInstInfo->info & EEINST_COP2_FINISH_VU0)
			mVUFinishVU0();
	}

	const bool vf_used = EEINST_VFUSEDTEST(_Rd_);
	const int ftreg = _allocVFtoXMMreg(_Rd_, MODE_READ);
	_deleteEEreg128(_Rt_);

	// const flag should've been cleared, but sanity check..
	pxAssert(!GPR_IS_CONST1(_Rt_));

	if (vf_used)
	{
		// store direct to state if rt is not used
		const int rtreg = _allocIfUsedGPRtoXMM(_Rt_, MODE_WRITE);
		if (rtreg >= 0)
			xMOVAPS(xRegisterSSE(rtreg), xRegisterSSE(ftreg));
		else
			xMOVAPS(ptr128[&cpuRegs.GPR.r[_Rt_].UQ], xRegisterSSE(ftreg));

		// don't cache vf00, microvu doesn't like it
		if (_Rd_ == 0)
			_freeXMMreg(ftreg);
	}
	else
	{
		_reallocateXMMreg(ftreg, XMMTYPE_GPRREG, _Rt_, MODE_WRITE, true);
	}
}

static void recQMTC2()
{
	printCOP2("QMTC2");
	COP2_Interlock(true);

	if (!_Rd_)
		return;
	
	if (!(cpuRegs.code & 1))
	{
		if (g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0)
			mVUSyncVU0();
		else if (g_pCurInstInfo->info & EEINST_COP2_FINISH_VU0)
			mVUFinishVU0();
	}

	if (_Rt_)
	{
		// if we have to flush to memory anyway (has a constant or is x86), force load.
		[[maybe_unused]] const bool vf_used = EEINST_VFUSEDTEST(_Rd_);
		const bool can_rename = EEINST_RENAMETEST(_Rt_);
		const int rtreg = (GPR_IS_DIRTY_CONST(_Rt_) || _hasX86reg(X86TYPE_GPR, _Rt_, MODE_WRITE)) ?
							  _allocGPRtoXMMreg(_Rt_, MODE_READ) :
                              _checkXMMreg(XMMTYPE_GPRREG, _Rt_, MODE_READ);
		
		// NOTE: can't transfer xmm15 to VF, it's reserved for PQ.
		int vfreg = _checkXMMreg(XMMTYPE_VFREG, _Rd_, MODE_WRITE);
		if (can_rename && rtreg >= 0 && rtreg != xmmPQ.GetId())
		{
			// rt is no longer needed, so transfer to VF.
			if (vfreg >= 0)
				_freeXMMregWithoutWriteback(vfreg);
			_reallocateXMMreg(rtreg, XMMTYPE_VFREG, _Rd_, MODE_WRITE, true);
		}
		else
		{
			// copy to VF.
			if (vfreg < 0)
				vfreg = _allocVFtoXMMreg(_Rd_, MODE_WRITE);
			if (rtreg >= 0)
				xMOVAPS(xRegisterSSE(vfreg), xRegisterSSE(rtreg));
			else
				xMOVAPS(xRegisterSSE(vfreg), ptr128[&cpuRegs.GPR.r[_Rt_].UQ]);
		}
	}
	else
	{
		const int vfreg = _allocVFtoXMMreg(_Rd_, MODE_WRITE);
		xPXOR(xRegisterSSE(vfreg), xRegisterSSE(vfreg));
	}
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

#if defined(LOADSTORE_RECOMPILE) && defined(CP2_RECOMPILE)

/*********************************************************
* Load and store for COP2 (VU0 unit)                     *
* Format:  OP rt, offset(base)                           *
*********************************************************/

void recLQC2()
{
	if (g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0)
		mVUSyncVU0();
	else if (g_pCurInstInfo->info & EEINST_COP2_FINISH_VU0)
		mVUFinishVU0();

	vtlb_ReadRegAllocCallback alloc_cb = nullptr;
	if (_Rt_)
	{
		// init regalloc after flush
		alloc_cb = []() { return _allocVFtoXMMreg(_Rt_, MODE_WRITE); };
	}

	int xmmreg;
	if (GPR_IS_CONST1(_Rs_))
	{
		const u32 addr = (g_cpuConstRegs[_Rs_].UL[0] + _Imm_) & ~0xFu;
		xmmreg = vtlb_DynGenReadQuad_Const(128, addr, alloc_cb);
	}
	else
	{
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);
		xAND(arg1regd, ~0xF);

		xmmreg = vtlb_DynGenReadQuad(128, arg1regd.GetId(), alloc_cb);
	}

	// toss away if loading to vf00
	if (!_Rt_)
		_freeXMMreg(xmmreg);

	EE::Profiler.EmitOp(eeOpcode::LQC2);
}

////////////////////////////////////////////////////

void recSQC2()
{
	if (g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0)
		mVUSyncVU0();
	else if (g_pCurInstInfo->info & EEINST_COP2_FINISH_VU0)
		mVUFinishVU0();

	// vf00 has to be special cased here, because of the microvu temps...
	const int ftreg = _Rt_ ? _allocVFtoXMMreg(_Rt_, MODE_READ) : _allocTempXMMreg(XMMT_FPS);
	if (!_Rt_)
		xMOVAPS(xRegisterSSE(ftreg), ptr128[&vu0Regs.VF[0].F]);

	if (GPR_IS_CONST1(_Rs_))
	{
		const u32 addr = (g_cpuConstRegs[_Rs_].UL[0] + _Imm_) & ~0xFu;
		vtlb_DynGenWrite_Const(128, true, addr, ftreg);
	}
	else
	{
		_eeMoveGPRtoR(arg1regd, _Rs_);
		if (_Imm_ != 0)
			xADD(arg1regd, _Imm_);
		xAND(arg1regd, ~0xF);

		vtlb_DynGenWrite(128, true, arg1regd.GetId(), ftreg);
	}

	if (!_Rt_)
		_freeXMMreg(ftreg);

	EE::Profiler.EmitOp(eeOpcode::SQC2);
}

#else
namespace Interp = R5900::Interpreter::OpcodeImpl;

REC_FUNC(LQC2);
REC_FUNC(SQC2);

#endif

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
void recCOP2_BC2() { recCOP2_BC2t[_Rt_](); }
void recCOP2_SPEC1()
{
	if (g_pCurInstInfo->info & (EEINST_COP2_SYNC_VU0 | EEINST_COP2_FINISH_VU0))
		mVUFinishVU0();

	recCOP2SPECIAL1t[_Funct_]();

}
void recCOP2_SPEC2() { recCOP2SPECIAL2t[(cpuRegs.code & 3) | ((cpuRegs.code >> 4) & 0x7c)](); }
