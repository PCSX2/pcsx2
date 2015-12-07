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

enum class eeOpcode {
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

	// ADD COP0/1 ??

	LAST
};

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

	"!"
};

//#define eeProfileProg

#ifdef eeProfileProg
#include <utility>
#include <algorithm>

struct eeProfiler {
	u64 opStats[static_cast<int>(eeOpcode::LAST)];

	void Reset() {
		memzero(*this);
		pxAssert(eeOpcodeName[static_cast<int>(eeOpcode::LAST)][0] == '!');
	}

	void EmitOp(eeOpcode opcode) {
		int op = static_cast<int>(opcode);
		x86Emitter::xADD(x86Emitter::ptr32[&(((u32*)opStats)[op*2+0])], 1);
		x86Emitter::xADC(x86Emitter::ptr32[&(((u32*)opStats)[op*2+1])], 0);
	}

	void Print() {
		u64 total = 0;
		std::vector< std::pair<u32, u32> > v;
		for(int i = 0; i < static_cast<int>(eeOpcode::LAST); i++) {
			total += opStats[i];
			v.push_back(std::make_pair(opStats[i], i));
		}
		std::sort   (v.begin(), v.end());
		std::reverse(v.begin(), v.end());

		DevCon.WriteLn("EE Profiler:");
		for(u32 i = 0; i < v.size(); i++) {
			u64    count = v[i].first;
			double stat  = (double)count / (double)total * 100.0;
			DevCon.WriteLn("%-8s - [%3.4f%%][count=%u]",
					eeOpcodeName[v[i].second], stat, (u32)count);
		}
		DevCon.WriteLn("Total = 0x%x_%x\n\n", (u32)(u64)(total>>32),(u32)total);
	}
};
#else
struct eeProfiler {
	__fi void Reset() {}
	__fi void EmitOp(eeOpcode op) {}
	__fi void Print() {}
};
#endif

namespace EE {
	extern eeProfiler Profiler;
}
