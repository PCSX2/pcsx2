// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
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
// Implemented opcode handlers
//------------------------------------------------------------------
// The full Upper ISA (+ mVU_NOP) lives in aVU_Upper.inl and the full Lower ISA
// (incl. setBranchA, mVU_B/mVU_BAL, the FMAC-table sub-dispatch, and every
// integer/load-store/EFU/flag/branch handler) lives in aVU_Lower.inl — both are
// #included before this file. This file only owns the dispatch tables.

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
mVUop(mVULowerOP);
mVUop(mVULowerOP_T3_00);
mVUop(mVULowerOP_T3_01);
mVUop(mVULowerOP_T3_10);
mVUop(mVULowerOP_T3_11);

//------------------------------------------------------------------
// Opcode tables — every unimplemented slot is mVUunknown
//------------------------------------------------------------------
#define U mVUunknown

static const Fnptr_mVUrecInst mVULOWER_OPCODE[128] = {
	mVU_LQ     , mVU_SQ     , mVUunknown , mVUunknown,
	mVU_ILW    , mVU_ISW    , mVUunknown , mVUunknown,
	mVU_IADDIU , mVU_ISUBIU , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVU_FCEQ   , mVU_FCSET  , mVU_FCAND  , mVU_FCOR,
	mVU_FSEQ   , mVU_FSSET  , mVU_FSAND  , mVU_FSOR,
	mVU_FMEQ   , mVUunknown , mVU_FMAND  , mVU_FMOR,
	mVU_FCGET  , mVUunknown , mVUunknown , mVUunknown,
	mVU_B      , mVU_BAL    , mVUunknown , mVUunknown,
	mVU_JR     , mVU_JALR   , mVUunknown , mVUunknown,
	mVU_IBEQ   , mVU_IBNE   , mVUunknown , mVUunknown,
	mVU_IBLTZ  , mVU_IBGTZ  , mVU_IBLEZ  , mVU_IBGEZ,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVULowerOP , mVUunknown , mVUunknown , mVUunknown,
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

static const Fnptr_mVUrecInst mVULowerOP_T3_00_OPCODE[32] = {
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVU_MOVE   , mVU_LQI    , mVU_DIV    , mVU_MTIR,
	mVU_RNEXT  , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVU_MFP    , mVU_XTOP   , mVU_XGKICK,
	mVU_ESADD  , mVU_EATANxy, mVU_ESQRT  , mVU_ESIN,
};

static const Fnptr_mVUrecInst mVULowerOP_T3_01_OPCODE[32] = {
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVU_MR32   , mVU_SQI    , mVU_SQRT   , mVU_MFIR,
	mVU_RGET   , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVU_XITOP  , mVUunknown,
	mVU_ERSADD , mVU_EATANxz, mVU_ERSQRT , mVU_EATAN,
};

static const Fnptr_mVUrecInst mVULowerOP_T3_10_OPCODE[32] = {
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVU_LQD    , mVU_RSQRT  , mVU_ILWR,
	mVU_RINIT  , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVU_ELENG  , mVU_ESUM   , mVU_ERCPR  , mVU_EEXP,
};

static const Fnptr_mVUrecInst mVULowerOP_T3_11_OPCODE[32] = {
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVU_SQD    , mVU_WAITQ  , mVU_ISWR,
	mVU_RXOR   , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVU_ERLENG , mVUunknown , mVU_WAITP  , mVUunknown,
};

static const Fnptr_mVUrecInst mVULowerOP_OPCODE[64] = {
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVU_IADD   , mVU_ISUB   , mVU_IADDI  , mVUunknown,
	mVU_IAND   , mVU_IOR    , mVUunknown , mVUunknown,
	mVUunknown , mVUunknown , mVUunknown , mVUunknown,
	mVULowerOP_T3_00, mVULowerOP_T3_01, mVULowerOP_T3_10, mVULowerOP_T3_11,
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
mVUop(mVULowerOP)       { mVULowerOP_OPCODE       [ (mVU.code & 0x3f) ](mX); }
mVUop(mVULowerOP_T3_00) { mVULowerOP_T3_00_OPCODE [((mVU.code >> 6) & 0x1f)](mX); }
mVUop(mVULowerOP_T3_01) { mVULowerOP_T3_01_OPCODE [((mVU.code >> 6) & 0x1f)](mX); }
mVUop(mVULowerOP_T3_10) { mVULowerOP_T3_10_OPCODE [((mVU.code >> 6) & 0x1f)](mX); }
mVUop(mVULowerOP_T3_11) { mVULowerOP_T3_11_OPCODE [((mVU.code >> 6) & 0x1f)](mX); }
mVUop(mVUopU)          { mVU_UPPER_OPCODE     [ (mVU.code & 0x3f) ](mX); } // Upper Opcode
mVUop(mVUopL)          { mVULOWER_OPCODE      [ (mVU.code >>  25) ](mX); } // Lower Opcode
