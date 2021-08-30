/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2015  PCSX2 Dev Team
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
#include "common/Pcsx2Defs.h"

// Keep my nice alignment please!
#define MOVZ MOVZtemp
#define MOVN MOVNtemp

enum class eeOpcode
{
	// Core
	special , regimm , J    , JAL   , BEQ  , BNE  , BLEZ  , BGTZ  ,
	ADDI    , ADDIU  , SLTI , SLTIU , ANDI , ORI  , XORI  , LUI   ,
	cop0    , cop1   , cop2 , /*,*/   BEQL , BNEL , BLEZL , BGTZL ,
	DADDI   , DADDIU , LDL  , LDR   , mmi  , /*,*/  LQ    , SQ    ,
	LB      , LH     , LWL  , LW    , LBU  , LHU  , LWR   , LWU   ,
	SB      , SH     , SWL  , SW    , SDL  , SDR  , SWR   , CACHE ,
	/*,*/     LWC1   , /*,*/  PREF  , /*,*/  /*,*/  LQC2  , LD    ,
	/*,*/     SWC1   , /*,*/  /*,*/   /*,*/  /*,*/  SQC2  , SD    ,

	// Special
	SLL  , /*,*/   SRL  , SRA  , SLLV    , /*,*/   SRLV   , SRAV   ,
	JR   , JALR  , MOVZ , MOVN , SYSCALL , BREAK , /*,*/    SYNC   ,
	MFHI , MTHI  , MFLO , MTLO , DSLLV   , /*,*/   DSRLV  , DSRAV  ,
	MULT , MULTU , DIV  , DIVU , /*,*/     /*,*/   /*,*/    /*,*/
	ADD  , ADDU  , SUB  , SUBU , AND     , OR    , XOR    , NOR    ,
	MFSA , MTSA  , SLT  , SLTU , DADD    , DADDU , DSUB   , DSUBU  ,
	TGE  , TGEU  , TLT  , TLTU , TEQ     , /*,*/   TNE    , /*,*/
	DSLL , /*,*/   DSRL , DSRA , DSLL32  , /*,*/   DSRL32 , DSRA32 ,

	// Regimm
	BLTZ   , BGEZ   , BLTZL   , BGEZL   , /*,*/  /*,*/  /*,*/  /*,*/
	TGEI   , TGEIU  , TLTI    , TLTIU   , TEQI , /*,*/  TNEI , /*,*/
	BLTZAL , BGEZAL , BLTZALL , BGEZALL , /*,*/  /*,*/  /*,*/  /*,*/
	MTSAB  , MTSAH  , /*,*/     /*,*/     /*,*/  /*,*/  /*,*/  /*,*/

	// MMI
	MADD  , MADDU  , /*,*/   /*,*/   PLZCW , /*,*/  /*,*/   /*,*/
	MMI0  , MMI2   , /*,*/   /*,*/   /*,*/   /*,*/  /*,*/   /*,*/
	MFHI1 , MTHI1  , MFLO1 , MTLO1 , /*,*/   /*,*/  /*,*/   /*,*/
	MULT1 , MULTU1 , DIV1  , DIVU1 , /*,*/   /*,*/  /*,*/   /*,*/
	MADD1 , MADDU1 , /*,*/   /*,*/   /*,*/   /*,*/  /*,*/   /*,*/
	MMI1  , MMI3   , /*,*/   /*,*/   /*,*/   /*,*/  /*,*/   /*,*/
	PMFHL , PMTHL  , /*,*/   /*,*/   PSLLH , /*,*/  PSRLH , PSRAH ,
	/*,*/   /*,*/    /*,*/   /*,*/   PSLLW , /*,*/  PSRLW , PSRAW ,

	// MMI0
	PADDW  , PSUBW  , PCGTW  , PMAXW ,
	PADDH  , PSUBH  , PCGTH  , PMAXH ,
	PADDB  , PSUBB  , PCGTB  , /*,*/
	/*,*/    /*,*/    /*,*/    /*,*/
	PADDSW , PSUBSW , PEXTLW , PPACW ,
	PADDSH , PSUBSH , PEXTLH , PPACH ,
	PADDSB , PSUBSB , PEXTLB , PPACB ,
	/*,*/    /*,*/    PEXT5  , PPAC5 ,

	// MMI1
	/*,*/    PABSW  , PCEQW  , PMINW ,
	PADSBH , PABSH  , PCEQH  , PMINH ,
	/*,*/    /*,*/    PCEQB  , /*,*/
	/*,*/    /*,*/    /*,*/    /*,*/
	PADDUW , PSUBUW , PEXTUW , /*,*/
	PADDUH , PSUBUH , PEXTUH , /*,*/
	PADDUB , PSUBUB , PEXTUB , QFSRV ,
	/*,*/    /*,*/    /*,*/    /*,*/

	// MMI2
	PMADDW , /*,*/    PSLLVW , PSRLVW ,
	PMSUBW , /*,*/    /*,*/    /*,*/
	PMFHI  , PMFLO  , PINTH  , /*,*/
	PMULTW , PDIVW  , PCPYLD , /*,*/
	PMADDH , PHMADH , PAND   , PXOR   ,
	PMSUBH , PHMSBH , /*,*/    /*,*/
	/*,*/    /*,*/    PEXEH  , PREVH  ,
	PMULTH , PDIVBW , PEXEW  , PROT3W ,

	// MMI3
	PMADDUW , /*,*/    /*,*/    PSRAVW ,
	/*,*/     /*,*/    /*,*/    /*,*/
	PMTHI   , PMTLO  , PINTEH , /*,*/
	PMULTUW , PDIVUW , PCPYUD , /*,*/
	/*,*/     /*,*/    POR    , PNOR   ,
	/*,*/     /*,*/    /*,*/    /*,*/
	/*,*/     /*,*/    PEXCH  , PCPYH  ,
	/*,*/     /*,*/    PEXCW  , /*,*/

	// ADD COP0 ??

	// "COP1"
	MFC1   , /*,*/    CFC1   , /*,*/   MTC1   , /*,*/    CTC1    , /*,*/

	// "COP1 BC1"
	BC1F   , BC1T   , BC1FL  , BC1TL , /*,*/    /*,*/    /*,*/     /*,*/

	// "COP1 S"
	ADD_F  , SUB_F  , MUL_F  , DIV_F , SQRT_F , ABS_F  , MOV_F   , NEG_F   ,
	/*,*/    /*,*/    /*,*/    /*,*/   /*,*/    /*,*/    /*,*/     /*,*/
	/*,*/    /*,*/    /*,*/    /*,*/   /*,*/    /*,*/    RSQRT_F , /*,*/
	ADDA_F , SUBA_F , MULA_F , /*,*/   MADD_F , MSUB_F , MADDA_F , MSUBA_F ,
	/*,*/    /*,*/    /*,*/    /*,*/   CVTW   , /*,*/    /*,*/     /*,*/
	MAX_F  , MIN_F  , /*,*/    /*,*/   /*,*/    /*,*/    /*,*/     /*,*/
	CF_F,    /*,*/    CEQ_F  , /*,*/   CLT_F  , /*,*/    CLE_F   , /*,*/

	// "COP1 W"
	CVTS_F,  /*,*/    /*,*/    /*,*/   /*,*/    /*,*/    /*,*/     /*,*/

	LAST
};

#undef MOVZ
#undef MOVN

static const char eeOpcodeName[][16] = {
	// "Core"
	"special" , "regimm" , "J"    , "JAL"   , "BEQ"  , "BNE"  , "BLEZ"  , "BGTZ"  ,
	"ADDI"    , "ADDIU"  , "SLTI" , "SLTIU" , "ANDI" , "ORI"  , "XORI"  , "LUI"   ,
	"cop0"    , "cop1"   , "cop2" , /* , */   "BEQL" , "BNEL" , "BLEZL" , "BGTZL" ,
	"DADDI"   , "DADDIU" , "LDL"  , "LDR"   , "mmi"  , /* , */  "LQ"    , "SQ"    ,
	"LB"      , "LH"     , "LWL"  , "LW"    , "LBU"  , "LHU"  , "LWR"   , "LWU"   ,
	"SB"      , "SH"     , "SWL"  , "SW"    , "SDL"  , "SDR"  , "SWR"   , "CACHE" ,
	/* , */     "LWC1"   , /* , */  "PREF"  , /* , */  /* , */  "LQC2"  , "LD"    ,
	/* , */     "SWC1"   , /* , */  /* , */   /* , */  /* , */  "SQC2"  , "SD"    ,

	// "Special"
	"SLL"  , /* , */   "SRL"  , "SRA"  , "SLLV"    , /* , */   "SRLV"   , "SRAV"   ,
	"JR"   , "JALR"  , "MOVZ" , "MOVN" , "SYSCALL" , "BREAK" , /* , */    "SYNC"   ,
	"MFHI" , "MTHI"  , "MFLO" , "MTLO" , "DSLLV"   , /* , */   "DSRLV"  , "DSRAV"  ,
	"MULT" , "MULTU" , "DIV"  , "DIVU" , /* , */     /* , */   /* , */    /* , */
	"ADD"  , "ADDU"  , "SUB"  , "SUBU" , "AND"     , "OR"    , "XOR"    , "NOR"    ,
	"MFSA" , "MTSA"  , "SLT"  , "SLTU" , "DADD"    , "DADDU" , "DSUB"   , "DSUBU"  ,
	"TGE"  , "TGEU"  , "TLT"  , "TLTU" , "TEQ"     , /* , */   "TNE"    , /* , */
	"DSLL" , /* , */   "DSRL" , "DSRA" , "DSLL32"  , /* , */   "DSRL32" , "DSRA32" ,

	// "Regimm"
	"BLTZ"   , "BGEZ"   , "BLTZL"   , "BGEZL"   , /* , */  /* , */  /* , */  /* , */
	"TGEI"   , "TGEIU"  , "TLTI"    , "TLTIU"   , "TEQI" , /* , */  "TNEI" , /* , */
	"BLTZAL" , "BGEZAL" , "BLTZALL" , "BGEZALL" , /* , */  /* , */  /* , */  /* , */
	"MTSAB"  , "MTSAH"  , /* , */     /* , */     /* , */  /* , */  /* , */  /* , */

	// "MMI"
	"MADD"  , "MADDU"  , /* , */   /* , */   "PLZCW" , /* , */  /* , */   /* , */
	"MMI0"  , "MMI2"   , /* , */   /* , */   /* , */   /* , */  /* , */   /* , */
	"MFHI1" , "MTHI1"  , "MFLO1" , "MTLO1" , /* , */   /* , */  /* , */   /* , */
	"MULT1" , "MULTU1" , "DIV1"  , "DIVU1" , /* , */   /* , */  /* , */   /* , */
	"MADD1" , "MADDU1" , /* , */   /* , */   /* , */   /* , */  /* , */   /* , */
	"MMI1"  , "MMI3"   , /* , */   /* , */   /* , */   /* , */  /* , */   /* , */
	"PMFHL" , "PMTHL"  , /* , */   /* , */   "PSLLH" , /* , */  "PSRLH" , "PSRAH" ,
	/* , */   /* , */    /* , */   /* , */   "PSLLW" , /* , */  "PSRLW" , "PSRAW" ,

	// "MMI0"
	"PADDW"  , "PSUBW"  , "PCGTW"  , "PMAXW" ,
	"PADDH"  , "PSUBH"  , "PCGTH"  , "PMAXH" ,
	"PADDB"  , "PSUBB"  , "PCGTB"  , /* , */
	/* , */    /* , */    /* , */    /* , */
	"PADDSW" , "PSUBSW" , "PEXTLW" , "PPACW" ,
	"PADDSH" , "PSUBSH" , "PEXTLH" , "PPACH" ,
	"PADDSB" , "PSUBSB" , "PEXTLB" , "PPACB" ,
	/* , */    /* , */    "PEXT5"  , "PPAC5" ,

	// "MMI1"
	/* , */    "PABSW"  , "PCEQW"  , "PMINW" ,
	"PADSBH" , "PABSH"  , "PCEQH"  , "PMINH" ,
	/* , */    /* , */    "PCEQB"  , /* , */
	/* , */    /* , */    /* , */    /* , */
	"PADDUW" , "PSUBUW" , "PEXTUW" , /* , */
	"PADDUH" , "PSUBUH" , "PEXTUH" , /* , */
	"PADDUB" , "PSUBUB" , "PEXTUB" , "QFSRV" ,
	/* , */    /* , */    /* , */    /* , */

	// "MMI2"
	"PMADDW" , /* , */    "PSLLVW" , "PSRLVW" ,
	"PMSUBW" , /* , */    /* , */    /* , */
	"PMFHI"  , "PMFLO"  , "PINTH"  , /* , */
	"PMULTW" , "PDIVW"  , "PCPYLD" , /* , */
	"PMADDH" , "PHMADH" , "PAND"   , "PXOR"   ,
	"PMSUBH" , "PHMSBH" , /* , */    /* , */
	/* , */    /* , */    "PEXEH"  , "PREVH"  ,
	"PMULTH" , "PDIVBW" , "PEXEW"  , "PROT3W" ,

	// "MMI3"
	"PMADDUW" , /* , */    /* , */    "PSRAVW" ,
	/* , */     /* , */    /* , */    /* , */
	"PMTHI"   , "PMTLO"  , "PINTEH" , /* , */
	"PMULTUW" , "PDIVUW" , "PCPYUD" , /* , */
	/* , */     /* , */    "POR"    , "PNOR"   ,
	/* , */     /* , */    /* , */    /* , */
	/* , */     /* , */    "PEXCH"  , "PCPYH"  ,
	/* , */     /* , */    "PEXCW"  , /* , */

	// "COP1"
	"MFC1"   , /* , */     "CFC1"    , /* , */   "MTC1"   , /* , */    "CTC1"    , /* , */
	
	// "COP1 BC1"
	"BC1F"   , "BC1T"    , "BC1FL"   , "BC1TL" , /* , */    /* , */    /* , */     /* , */

	// "COP1 S"
	"ADD_F"  , "SUB_F"   , "MUL_F"   , "DIV_F" , "SQRT_F" , "ABS_F"  , "MOV_F"   , "NEG_F"   ,
	/* , */    /* , */     /* , */     /* , */   /* , */    /* , */    /* , */     /* , */
	/* , */    /* , */     /* , */     /* , */   /* , */    /* , */    "RSQRT_F" , /* , */
	"ADDA_F" ,  "SUBA_F" ,  "MULA_F" , /* , */   "MADD_F" , "MSUB_F" , "MADDA_F" , "MSUBA_F" ,
	/* , */    /* , */     /* , */     /* , */   "CVTW"   , /* , */    /* , */     /* , */
	"MAX_F"  , "MIN_F"   , /* , */     /* , */   /* , */    /* , */    /* , */     /* , */
	"C.F"    , /* , */     "C.EQ"    , /* , */   "C.LT"   , /* , */    "C.LE"    , /* , */

	// "COP1 W"
	"CVTS_F" , /* , */     /* , */     /* , */   /* , */    /* , */    /* , */     /* , */

	"!"
};

//#define eeProfileProg

#ifdef eeProfileProg
#include <utility>
#include <algorithm>

using namespace x86Emitter;

struct eeProfiler
{
	static const u32 memSpace = 1 << 19;

	u64 opStats[static_cast<int>(eeOpcode::LAST)];
	u32 memStats[memSpace];
	u32 memStatsConst[memSpace];
	u64 memStatsSlow;
	u64 memStatsFast;
	u32 memMask;

	void Reset()
	{
		memzero(opStats);
		memzero(memStats);
		memzero(memStatsConst);
		memStatsSlow = 0;
		memStatsFast = 0;
		memMask = 0xF700FFF0;
		pxAssert(eeOpcodeName[static_cast<int>(eeOpcode::LAST)][0] == '!');
	}

	void EmitOp(eeOpcode opcode)
	{
		int op = static_cast<int>(opcode);
		xADD(ptr32[&(((u32*)opStats)[op * 2 + 0])], 1);
		xADC(ptr32[&(((u32*)opStats)[op * 2 + 1])], 0);
	}

	double per(u64 part, u64 total)
	{
		return (double)part / (double)total * 100.0;
	}

	void Print()
	{
		// Compute opcode stat
		u64 total = 0;
		std::vector<std::pair<u32, u32>> v;
		std::vector<std::pair<u32, u32>> vc;
		for (int i = 0; i < static_cast<int>(eeOpcode::LAST); i++)
		{
			total += opStats[i];
			v.push_back(std::make_pair(opStats[i], i));
		}
		std::sort(v.begin(), v.end());
		std::reverse(v.begin(), v.end());

		DevCon.WriteLn("EE Profiler:");
		for (u32 i = 0; i < v.size(); i++)
		{
			u64 count = v[i].first;
			double stat = (double)count / (double)total * 100.0;
			DevCon.WriteLn("%-8s - [%3.4f%%][count=%u]",
				eeOpcodeName[v[i].second], stat, (u32)count);
			if (stat < 0.01)
				break;
		}
		//DevCon.WriteLn("Total = 0x%x_%x", (u32)(u64)(total>>32),(u32)total);

		// Compute memory stat
		total = 0;
		u64 reg = 0;
		u64 gs  = 0;
		u64 vu  = 0;
		// FIXME: MAYBE count the scratch pad
		for (size_t i = 0; i < memSpace; i++)
			total += memStats[i];

		int ou = 32 * _1kb; // user segment (0x10000000)
		int ok = 352 * _1kb; // kernel segment (0xB0000000)
		for (int i = 0; i < 4 * _1kb; i++) reg += memStats[ou + 0 * _1kb + i] + memStats[ok + 0 * _1kb + i];
		for (int i = 0; i < 4 * _1kb; i++) gs  += memStats[ou + 4 * _1kb + i] + memStats[ok + 4 * _1kb + i];
		for (int i = 0; i < 4 * _1kb; i++) vu  += memStats[ou + 8 * _1kb + i] + memStats[ok + 8 * _1kb + i];


		u64 ram = total - reg - gs - vu;
		double ram_p = per(ram, total);
		double reg_p = per(reg, total);
		double gs_p  = per(gs, total);
		double vu_p  = per(vu, total);

		// Compute const memory stat
		u64 total_const = 0;
		u64 reg_const = 0;
		for (size_t i = 0; i < memSpace; i++)
			total_const += memStatsConst[i];

		for (int i = 0; i < 4 * _1kb; i++)
			reg_const += memStatsConst[ou + i] + memStatsConst[ok + i];
		u64 ram_const = total_const - reg_const; // value is slightly wrong but good enough

		double ram_const_p = per(ram_const, ram);
		double reg_const_p = per(reg_const, reg);

		DevCon.WriteLn("\nEE Memory Profiler:");
		DevCon.WriteLn("Total = 0x%08x_%08x", (u32)(u64)(total >> 32), (u32)total);
		DevCon.WriteLn("  RAM = 0x%08x_%08x [%3.4f%%] Const[%3.4f%%]", (u32)(u64)(ram >> 32), (u32)ram, ram_p, ram_const_p);
		DevCon.WriteLn("  REG = 0x%08x_%08x [%3.4f%%] Const[%3.4f%%]", (u32)(u64)(reg >> 32), (u32)reg, reg_p, reg_const_p);
		DevCon.WriteLn("  GS  = 0x%08x_%08x [%3.4f%%]", (u32)(u64)(gs >> 32), (u32)gs, gs_p);
		DevCon.WriteLn("  VU  = 0x%08x_%08x [%3.4f%%]", (u32)(u64)(vu >> 32), (u32)vu, vu_p);

		u64 total_ram = memStatsSlow + memStatsFast;
		DevCon.WriteLn("\n  RAM Fast [%3.4f%%] RAM Slow [%3.4f%%]. Total 0x%08x_%08x [%3.4f%%]",
			per(memStatsFast, total_ram), per(memStatsSlow, total_ram), (u32)(u64)(total_ram >> 32), (u32)total_ram, per(total_ram, total));

		v.clear();
		vc.clear();
		for (int i = 0; i < 4 * _1kb; i++)
		{
			u32 reg_c = memStatsConst[ou + i] + memStatsConst[ok + i];
			u32 reg   = memStats[ok + i] + memStats[ou + i] - reg_c;
			if (reg)
				v.push_back(std::make_pair(reg, i * 16));
			if (reg_c)
				vc.push_back(std::make_pair(reg_c, i * 16));
		}
		std::sort(v.begin(), v.end());
		std::reverse(v.begin(), v.end());

		std::sort(vc.begin(), vc.end());
		std::reverse(vc.begin(), vc.end());

		DevCon.WriteLn("\nEE Reg Profiler:");
		for (u32 i = 0; i < v.size(); i++)
		{
			u64    count = v[i].first;
			double stat  = (double)count / (double)(reg - reg_const) * 100.0;
			DevCon.WriteLn("%04x - [%3.4f%%][count=%u]",
				v[i].second, stat, (u32)count);
			if (stat < 0.01)
				break;
		}

		DevCon.WriteLn("\nEE Const Reg Profiler:");
		for (u32 i = 0; i < vc.size(); i++)
		{
			u64    count = vc[i].first;
			double stat  = (double)count / (double)reg_const * 100.0;
			DevCon.WriteLn("%04x - [%3.4f%%][count=%u]",
				vc[i].second, stat, (u32)count);
			if (stat < 0.01)
				break;
		}
	}

	// Warning dirty ebx
	void EmitMem()
	{
		// Compact the 4GB virtual address to a 512KB virtual address
		if (x86caps.hasBMI2)
		{
			xPEXT(ebx, ecx, ptr[&memMask]);
			xADD(ptr32[(rbx * 4) + memStats], 1);
		}
	}

	void EmitConstMem(u32 add)
	{
		if (x86caps.hasBMI2)
		{
			u32 a = _pext_u32(add, memMask);
			xADD(ptr32[a + memStats], 1);
			xADD(ptr32[a + memStatsConst], 1);
		}
	}

	void EmitSlowMem()
	{
		xADD(ptr32[(u32*)&memStatsSlow], 1);
		xADC(ptr32[(u32*)&memStatsSlow + 1], 0);
	}

	void EmitFastMem()
	{
		xADD(ptr32[(u32*)&memStatsFast], 1);
		xADC(ptr32[(u32*)&memStatsFast + 1], 0);
	}
};
#else
struct eeProfiler
{
	__fi void Reset() {}
	__fi void EmitOp(eeOpcode op) {}
	__fi void Print() {}
	__fi void EmitMem() {}
	__fi void EmitConstMem(u32 add) {}
	__fi void EmitSlowMem() {}
	__fi void EmitFastMem() {}
};
#endif

namespace EE
{
	extern eeProfiler Profiler;
}
