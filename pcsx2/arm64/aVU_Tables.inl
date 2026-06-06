// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// ARM64 microVU — opcode dispatch tables (Phase 7, Tables/Compile big-bang).
//
// VIXL port of pcsx2/x86/microVU_Tables.inl, but **minimal**: only the handlers
// that exist today (NOP in the Upper, B/BAL in the Lower) are wired; every other
// table slot routes to mVUunknown. As the real opcode handlers land (7.5a Upper,
// 7.5b Lower) their table slots get swapped from mVUunknown to the real fn, and
// the FD_xx / LowerOP / T3_xx sub-dispatch tables get filled in then.
//
// The two top-level dispatchers mVUopU / mVUopL are what the flag read-scan
// (_mVUflagPass, aVU_Flags.inl) and the compile driver (doUpperOp/doLowerOp,
// mVUcompile) call. They live in the same aVU.cpp TU, so static linkage is fine,
// but this .inl must be #included *before* aVU_Flags.inl's read-scan.

//------------------------------------------------------------------
// Branch-attribute setup (x86: microVU_Lower.inl setBranchA)
//------------------------------------------------------------------
// Records the branch type (x) on the lower op + handles the "branch to next
// instruction" NOP optimization. No emit — pure IR bookkeeping across all passes.
// Ported here (rather than a future aVU_Lower.inl) because B/BAL need it now; it
// moves out when 7.5b ports the full Lower ISA.
void setBranchA(mP, int x, int _x_)
{
	bool isBranchDelaySlot = false;

	incPC(-2);
	if (mVUlow.branch)
		isBranchDelaySlot = true;
	incPC(2);

	pass1
	{
		if (_Imm11_ == 1 && !_x_ && !isBranchDelaySlot)
		{
			DevCon.WriteLn(Color_Green, "microVU%d: Branch Optimization", mVU.index);
			mVUlow.isNOP = true;
			return;
		}
		mVUbranch     = x;
		mVUlow.branch = x;
	}
	pass2 { if (_Imm11_ == 1 && !_x_ && !isBranchDelaySlot) { return; } mVUbranch = x; }
	pass3 { mVUbranch = x; }
	pass4 { if (_Imm11_ == 1 && !_x_ && !isBranchDelaySlot) { return; } mVUbranch = x; }
}

//------------------------------------------------------------------
// Implemented opcode handlers (mVU_NOP + the full Upper ISA live in aVU_Upper.inl,
// included before this file; the Lower ISA is still NOP/B-only until task 7.5b)
//------------------------------------------------------------------

mVUop(mVU_B)
{
	setBranchA(mX, 1, 0);
	pass1 { mVUanalyzeNormBranch(mVU, 0, false); }
	pass2
	{
		if (mVUlow.badBranch)  { mvuStrImm32(&mVU.badBranch, branchAddr(mVU), gprT1); }
		if (mVUlow.evilBranch) { if (isEvilBlock) mvuStrImm32(&mVU.evilevilBranch, branchAddr(mVU), gprT1); else mvuStrImm32(&mVU.evilBranch, branchAddr(mVU), gprT1); }
		mVU.profiler.EmitOp(opB);
	}
	pass3 { mVUlog("B [<a href=\"#addr%04x\">%04x</a>]", branchAddr(mVU), branchAddr(mVU)); }
}

mVUop(mVU_BAL)
{
	setBranchA(mX, 2, _It_);
	pass1 { mVUanalyzeNormBranch(mVU, _It_, true); }
	pass2
	{
		if (!mVUlow.evilBranch)
		{
			const a64::Register regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
			armAsm->Mov(regT, bSaveAddr);
			mVU.regAlloc->clearNeeded(regT);
		}
		else
		{
			incPC(-2);
			DevCon.Warning("Linking BAL from %s branch taken/not taken target! - If game broken report to PCSX2 Team", branchSTR[mVUlow.branch & 0xf]);
			incPC(2);

			const a64::Register regT = mVU.regAlloc->allocGPR(-1, _It_, mVUlow.backupVI);
			if (isEvilBlock)
				mvuLdr32(regT, &mVU.evilBranch);
			else
				mvuLdr32(regT, &mVU.badBranch);

			armAsm->Add(regT.W(), regT.W(), 8);
			armAsm->Lsr(regT.W(), regT.W(), 3);
			mVU.regAlloc->clearNeeded(regT);
		}

		if (mVUlow.badBranch)  { mvuStrImm32(&mVU.badBranch, branchAddr(mVU), gprT1); }
		if (mVUlow.evilBranch) { if (isEvilBlock) mvuStrImm32(&mVU.evilevilBranch, branchAddr(mVU), gprT1); else mvuStrImm32(&mVU.evilBranch, branchAddr(mVU), gprT1); }
		mVU.profiler.EmitOp(opBAL);
	}
	pass3 { mVUlog("BAL vi%02d [<a href=\"#addr%04x\">%04x</a>]", _Ft_, branchAddr(mVU), branchAddr(mVU)); }
}

mVUop(mVUunknown)
{
	pass1
	{
		if (mVU.code != 0x8000033c)
			mVUinfo.isBadOp = true;
	}
	pass2
	{
		if (mVU.code != 0x8000033c)
			Console.Error("microVU%d: Unknown Micro VU opcode called (%x) [%04x]\n", getIndex, mVU.code, xPC);
	}
	pass3 { mVUlog("Unknown", mVU.code); }
}

//------------------------------------------------------------------
// Sub-dispatch declarations
//------------------------------------------------------------------
mVUop(mVU_UPPER_FD_00);
mVUop(mVU_UPPER_FD_01);
mVUop(mVU_UPPER_FD_10);
mVUop(mVU_UPPER_FD_11);

//------------------------------------------------------------------
// Opcode tables — every unimplemented slot is mVUunknown (filled in 7.5)
//------------------------------------------------------------------
#define U mVUunknown

static const Fnptr_mVUrecInst mVULOWER_OPCODE[128] = {
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	mVU_B , mVU_BAL, U     , U,   // 0x20: B, BAL
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
	U     , U     , U     , U,
};

static const Fnptr_mVUrecInst mVU_UPPER_OPCODE[64] = {
	mVU_ADDx   , mVU_ADDy   , mVU_ADDz   , mVU_ADDw,
	mVU_SUBx   , mVU_SUBy   , mVU_SUBz   , mVU_SUBw,
	mVU_MADDx  , mVU_MADDy  , mVU_MADDz  , mVU_MADDw,
	mVU_MSUBx  , mVU_MSUBy  , mVU_MSUBz  , mVU_MSUBw,
	mVU_MAXx   , mVU_MAXy   , mVU_MAXz   , mVU_MAXw,
	mVU_MINIx  , mVU_MINIy  , mVU_MINIz  , mVU_MINIw,
	mVU_MULx   , mVU_MULy   , mVU_MULz   , mVU_MULw,
	mVU_MULq   , mVU_MAXi   , mVU_MULi   , mVU_MINIi,
	mVU_ADDq   , mVU_MADDq  , mVU_ADDi   , mVU_MADDi,
	mVU_SUBq   , mVU_MSUBq  , mVU_SUBi   , mVU_MSUBi,
	mVU_ADD    , mVU_MADD   , mVU_MUL    , mVU_MAX,
	mVU_SUB    , mVU_MSUB   , mVU_OPMSUB , mVU_MINI,
	U          , U          , U          , U,
	U          , U          , U          , U,
	U          , U          , U          , U,
	mVU_UPPER_FD_00, mVU_UPPER_FD_01, mVU_UPPER_FD_10, mVU_UPPER_FD_11,
};

static const Fnptr_mVUrecInst mVU_UPPER_FD_00_TABLE[32] = {
	mVU_ADDAx  , mVU_SUBAx  , mVU_MADDAx , mVU_MSUBAx,
	mVU_ITOF0  , mVU_FTOI0  , mVU_MULAx  , mVU_MULAq,
	mVU_ADDAq  , mVU_SUBAq  , mVU_ADDA   , mVU_SUBA,
	U          , U          , U          , U,
	U          , U          , U          , U,
	U          , U          , U          , U,
	U          , U          , U          , U,
	U          , U          , U          , U,
};

static const Fnptr_mVUrecInst mVU_UPPER_FD_01_TABLE[32] = {
	mVU_ADDAy  , mVU_SUBAy  , mVU_MADDAy , mVU_MSUBAy,
	mVU_ITOF4  , mVU_FTOI4  , mVU_MULAy  , mVU_ABS,
	mVU_MADDAq , mVU_MSUBAq , mVU_MADDA  , mVU_MSUBA,
	U          , U          , U          , U,
	U          , U          , U          , U,
	U          , U          , U          , U,
	U          , U          , U          , U,
	U          , U          , U          , U,
};

static const Fnptr_mVUrecInst mVU_UPPER_FD_10_TABLE[32] = {
	mVU_ADDAz  , mVU_SUBAz  , mVU_MADDAz , mVU_MSUBAz,
	mVU_ITOF12 , mVU_FTOI12 , mVU_MULAz  , mVU_MULAi,
	mVU_ADDAi  , mVU_SUBAi  , mVU_MULA   , mVU_OPMULA,
	U          , U          , U          , U,
	U          , U          , U          , U,
	U          , U          , U          , U,
	U          , U          , U          , U,
	U          , U          , U          , U,
};

static const Fnptr_mVUrecInst mVU_UPPER_FD_11_TABLE[32] = {
	mVU_ADDAw  , mVU_SUBAw  , mVU_MADDAw , mVU_MSUBAw,
	mVU_ITOF15 , mVU_FTOI15 , mVU_MULAw  , mVU_CLIP,
	mVU_MADDAi , mVU_MSUBAi , U          , mVU_NOP,
	U          , U          , U          , U,
	U          , U          , U          , U,
	U          , U          , U          , U,
	U          , U          , U          , U,
	U          , U          , U          , U,
};

#undef U

//------------------------------------------------------------------
// Table dispatch functions
//------------------------------------------------------------------
mVUop(mVU_UPPER_FD_00) { mVU_UPPER_FD_00_TABLE[((mVU.code >> 6) & 0x1f)](mX); }
mVUop(mVU_UPPER_FD_01) { mVU_UPPER_FD_01_TABLE[((mVU.code >> 6) & 0x1f)](mX); }
mVUop(mVU_UPPER_FD_10) { mVU_UPPER_FD_10_TABLE[((mVU.code >> 6) & 0x1f)](mX); }
mVUop(mVU_UPPER_FD_11) { mVU_UPPER_FD_11_TABLE[((mVU.code >> 6) & 0x1f)](mX); }
mVUop(mVUopU)          { mVU_UPPER_OPCODE     [ (mVU.code & 0x3f) ](mX); } // Upper Opcode
mVUop(mVUopL)          { mVULOWER_OPCODE      [ (mVU.code >>  25) ](mX); } // Lower Opcode
