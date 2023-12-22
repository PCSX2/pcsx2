// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

//all tables for R5900 are define here..

#include "R5900OpcodeTables.h"
#include "R5900.h"

#include "x86/iR5900AritImm.h"
#include "x86/iR5900Arit.h"
#include "x86/iR5900MultDiv.h"
#include "x86/iR5900Shift.h"
#include "x86/iR5900Branch.h"
#include "x86/iR5900Jump.h"
#include "x86/iR5900LoadStore.h"
#include "x86/iR5900Move.h"
#include "x86/iMMI.h"
#include "x86/iCOP0.h"
#include "x86/iFPU.h"

namespace R5900
{
	namespace Opcodes
	{
		// Generates an entry for the given opcode name.
		// Assumes the default function naming schemes for interpreter and recompiler  functions.
	#	define MakeOpcode( name, cycles, flags ) \
		static const OPCODE name = { \
			#name, \
			cycles, \
			flags, \
			NULL, \
			::R5900::Interpreter::OpcodeImpl::name, \
			::R5900::Dynarec::OpcodeImpl::rec##name, \
			::R5900::OpcodeDisasm::name \
		}

#	define MakeOpcodeM( name, cycles, flags ) \
		static const OPCODE name = { \
			#name, \
			cycles, \
			flags, \
			NULL, \
			::R5900::Interpreter::OpcodeImpl::MMI::name, \
			::R5900::Dynarec::OpcodeImpl::MMI::rec##name, \
			::R5900::OpcodeDisasm::name \
		}

#	define MakeOpcode0( name, cycles, flags ) \
		static const OPCODE name = { \
			#name, \
			cycles, \
			flags, \
			NULL, \
			::R5900::Interpreter::OpcodeImpl::COP0::name, \
			::R5900::Dynarec::OpcodeImpl::COP0::rec##name, \
			::R5900::OpcodeDisasm::name \
		}

	#	define MakeOpcode1( name, cycles, flags ) \
		static const OPCODE name = { \
			#name, \
			cycles, \
			flags, \
			NULL, \
			::R5900::Interpreter::OpcodeImpl::COP1::name, \
			::R5900::Dynarec::OpcodeImpl::COP1::rec##name, \
			::R5900::OpcodeDisasm::name \
		}

	#	define MakeOpcodeClass( name ) \
		static const OPCODE name = { \
			#name, \
			0, \
			0, \
			R5900::Opcodes::Class_##name, \
			NULL, \
			NULL, \
			NULL \
		}

		// We're working on new hopefully better cycle ratios, but they're still a WIP.
		// And yes this whole thing is an ugly hack.  I'll clean it up once we have
		// a better idea how exactly the cycle ratios will work best.

		namespace Cycles
		{
			static const int Default = 9;
			static const int Branch = 11;
			static const int CopDefault = 7;

			static const int Mult = 2*8;
			static const int Div = 14*8;
			static const int MMI_Mult = 3*8;
			static const int MMI_Div = 22*8;
			static const int MMI_Default = 14;

			static const int FPU_Mult = 4*8;

			static const int Store = 14;
			static const int Load = 14;
		}

		using namespace Cycles;

		MakeOpcode( Unknown, Default, 0 );
		MakeOpcode( MMI_Unknown, Default, 0 );
		MakeOpcode( COP0_Unknown, Default, 0 );
		MakeOpcode( COP1_Unknown, Default, 0 );

		// Class Subset Opcodes
		// (not really opcodes, but rather entire subsets of other opcode classes)

		MakeOpcodeClass( SPECIAL );
		MakeOpcodeClass( REGIMM );
		//MakeOpcodeClass( COP2 );
		MakeOpcodeClass( MMI );
		MakeOpcodeClass( MMI0 );
		MakeOpcodeClass( MMI2 );
		MakeOpcodeClass( MMI1 );
		MakeOpcodeClass( MMI3 );

		MakeOpcodeClass( COP0 );
		MakeOpcodeClass( COP1 );

		// Misc Junk

		MakeOpcode( COP2, Default, 0 );

		MakeOpcode( CACHE, Default, 0 );
		MakeOpcode( PREF, Default, 0 );
		MakeOpcode( SYSCALL, Default, IS_BRANCH|BRANCHTYPE_SYSCALL );
		MakeOpcode( BREAK, Default, 0 );
		MakeOpcode( SYNC, Default, 0 );

		// Branch/Jump Opcodes

		MakeOpcode( J ,   Default, IS_BRANCH|BRANCHTYPE_JUMP );
		MakeOpcode( JAL,  Default, IS_BRANCH|BRANCHTYPE_JUMP|IS_LINKED );
		MakeOpcode( JR,   Default, IS_BRANCH|BRANCHTYPE_REGISTER );
		MakeOpcode( JALR, Default, IS_BRANCH|BRANCHTYPE_REGISTER|IS_LINKED );

		MakeOpcode( BEQ,     Branch, IS_BRANCH|BRANCHTYPE_BRANCH|CONDTYPE_EQ );
		MakeOpcode( BNE,     Branch, IS_BRANCH|BRANCHTYPE_BRANCH|CONDTYPE_NE );
		MakeOpcode( BLEZ,    Branch, IS_BRANCH|BRANCHTYPE_BRANCH|CONDTYPE_LEZ );
		MakeOpcode( BGTZ,    Branch, IS_BRANCH|BRANCHTYPE_BRANCH|CONDTYPE_GTZ );
		MakeOpcode( BEQL,    Branch, IS_BRANCH|BRANCHTYPE_BRANCH|CONDTYPE_EQ|IS_LIKELY );
		MakeOpcode( BNEL,    Branch, IS_BRANCH|BRANCHTYPE_BRANCH|CONDTYPE_NE|IS_LIKELY );
		MakeOpcode( BLEZL,   Branch, IS_BRANCH|BRANCHTYPE_BRANCH|CONDTYPE_LEZ|IS_LIKELY );
		MakeOpcode( BGTZL,   Branch, IS_BRANCH|BRANCHTYPE_BRANCH|CONDTYPE_GTZ|IS_LIKELY );
		MakeOpcode( BLTZ,    Branch, IS_BRANCH|BRANCHTYPE_BRANCH|CONDTYPE_LTZ );
		MakeOpcode( BGEZ,    Branch, IS_BRANCH|BRANCHTYPE_BRANCH|CONDTYPE_GEZ );
		MakeOpcode( BLTZL,   Branch, IS_BRANCH|BRANCHTYPE_BRANCH|CONDTYPE_LTZ|IS_LIKELY );
		MakeOpcode( BGEZL,   Branch, IS_BRANCH|BRANCHTYPE_BRANCH|CONDTYPE_GEZ|IS_LIKELY );
		MakeOpcode( BLTZAL,  Branch, IS_BRANCH|BRANCHTYPE_BRANCH|CONDTYPE_LTZ|IS_LINKED );
		MakeOpcode( BGEZAL,  Branch, IS_BRANCH|BRANCHTYPE_BRANCH|CONDTYPE_GEZ|IS_LINKED );
		MakeOpcode( BLTZALL, Branch, IS_BRANCH|BRANCHTYPE_BRANCH|CONDTYPE_LTZ|IS_LINKED|IS_LIKELY );
		MakeOpcode( BGEZALL, Branch, IS_BRANCH|BRANCHTYPE_BRANCH|CONDTYPE_GEZ|IS_LINKED|IS_LIKELY );

		MakeOpcode( TGEI, Branch, 0 );
		MakeOpcode( TGEIU, Branch, 0 );
		MakeOpcode( TLTI, Branch, 0 );
		MakeOpcode( TLTIU, Branch, 0 );
		MakeOpcode( TEQI, Branch, 0 );
		MakeOpcode( TNEI, Branch, 0 );
		MakeOpcode( TGE, Branch, 0 );
		MakeOpcode( TGEU, Branch, 0 );
		MakeOpcode( TLT, Branch, 0 );
		MakeOpcode( TLTU, Branch, 0 );
		MakeOpcode( TEQ, Branch, 0 );
		MakeOpcode( TNE, Branch, 0 );

		// Arithmetic

		MakeOpcode( MULT, Mult, 0 );
		MakeOpcode( MULTU, Mult, 0 );
		MakeOpcode( MULT1, Mult, 0 );
		MakeOpcode( MULTU1, Mult, 0 );
		MakeOpcode( MADD, Mult, 0 );
		MakeOpcode( MADDU, Mult, 0 );
		MakeOpcode( MADD1, Mult, 0 );
		MakeOpcode( MADDU1, Mult, 0 );
		MakeOpcode( DIV, Div, 0 );
		MakeOpcode( DIVU, Div, 0 );
		MakeOpcode( DIV1, Div, 0 );
		MakeOpcode( DIVU1, Div, 0 );

		MakeOpcode( ADDI,   Default, IS_ALU|ALUTYPE_ADDI );
		MakeOpcode( ADDIU,  Default, IS_ALU|ALUTYPE_ADDI );
		MakeOpcode( DADDI,  Default, IS_ALU|ALUTYPE_ADDI|IS_64BIT );
		MakeOpcode( DADDIU, Default, IS_ALU|ALUTYPE_ADDI|IS_64BIT );
		MakeOpcode( DADD,   Default, IS_ALU|ALUTYPE_ADD|IS_64BIT );
		MakeOpcode( DADDU,  Default, IS_ALU|ALUTYPE_ADD|IS_64BIT );
		MakeOpcode( DSUB,   Default, IS_ALU|ALUTYPE_SUB|IS_64BIT );
		MakeOpcode( DSUBU,  Default, IS_ALU|ALUTYPE_SUB|IS_64BIT );
		MakeOpcode( ADD,    Default, IS_ALU|ALUTYPE_ADD );
		MakeOpcode( ADDU,   Default, IS_ALU|ALUTYPE_ADD );
		MakeOpcode( SUB,    Default, IS_ALU|ALUTYPE_SUB );
		MakeOpcode( SUBU,   Default, IS_ALU|ALUTYPE_SUB );

		MakeOpcode( ANDI, Default, 0 );
		MakeOpcode( ORI, Default, 0 );
		MakeOpcode( XORI, Default, 0 );
		MakeOpcode( AND, Default, 0 );
		MakeOpcode( OR, Default, 0 );
		MakeOpcode( XOR, Default, 0 );
		MakeOpcode( NOR, Default, 0 );
		MakeOpcode( SLTI, Default, 0 );
		MakeOpcode( SLTIU, Default, 0 );
		MakeOpcode( SLT, Default, 0 );
		MakeOpcode( SLTU, Default, 0 );
		MakeOpcode( LUI, Default, 0 );
		MakeOpcode( SLL, Default, 0 );
		MakeOpcode( SRL, Default, 0 );
		MakeOpcode( SRA, Default, 0 );
		MakeOpcode( SLLV, Default, 0 );
		MakeOpcode( SRLV, Default, 0 );
		MakeOpcode( SRAV, Default, 0 );
		MakeOpcode( MOVZ, Default, IS_ALU|ALUTYPE_CONDMOVE|CONDTYPE_EQ );
		MakeOpcode( MOVN, Default, IS_ALU|ALUTYPE_CONDMOVE|CONDTYPE_NE );
		MakeOpcode( DSLLV, Default, 0 );
		MakeOpcode( DSRLV, Default, 0 );
		MakeOpcode( DSRAV, Default, 0 );
		MakeOpcode( DSLL, Default, 0 );
		MakeOpcode( DSRL, Default, 0 );
		MakeOpcode( DSRA, Default, 0 );
		MakeOpcode( DSLL32, Default, 0 );
		MakeOpcode( DSRL32, Default, 0 );
		MakeOpcode( DSRA32, Default, 0 );

		MakeOpcode( MFHI, Default, 0 );
		MakeOpcode( MTHI, Default, 0 );
		MakeOpcode( MFLO, Default, 0 );
		MakeOpcode( MTLO, Default, 0 );
		MakeOpcode( MFSA, Default, 0 );
		MakeOpcode( MTSA, Default, 0 );
		MakeOpcode( MTSAB, Default, 0 );
		MakeOpcode( MTSAH, Default, 0 );
		MakeOpcode( MFHI1, Default, 0 );
		MakeOpcode( MTHI1, Default, 0 );
		MakeOpcode( MFLO1, Default, 0 );
		MakeOpcode( MTLO1, Default, 0 );

		// Loads!

		MakeOpcode( LDL,  Load, IS_MEMORY|IS_LOAD|MEMTYPE_DWORD|IS_LEFT );
		MakeOpcode( LDR,  Load, IS_MEMORY|IS_LOAD|MEMTYPE_DWORD|IS_RIGHT );
		MakeOpcode( LQ,   Load, IS_MEMORY|IS_LOAD|MEMTYPE_QWORD );
		MakeOpcode( LB,   Load, IS_MEMORY|IS_LOAD|MEMTYPE_BYTE );
		MakeOpcode( LH,   Load, IS_MEMORY|IS_LOAD|MEMTYPE_HALF );
		MakeOpcode( LWL,  Load, IS_MEMORY|IS_LOAD|MEMTYPE_WORD|IS_LEFT );
		MakeOpcode( LW,   Load, IS_MEMORY|IS_LOAD|MEMTYPE_WORD );
		MakeOpcode( LBU,  Load, IS_MEMORY|IS_LOAD|MEMTYPE_BYTE );
		MakeOpcode( LHU,  Load, IS_MEMORY|IS_LOAD|MEMTYPE_HALF );
		MakeOpcode( LWR,  Load, IS_MEMORY|IS_LOAD|MEMTYPE_WORD|IS_RIGHT );
		MakeOpcode( LWU,  Load, IS_MEMORY|IS_LOAD|MEMTYPE_WORD );
		MakeOpcode( LWC1, Load, IS_MEMORY|IS_LOAD|MEMTYPE_WORD );
		MakeOpcode( LQC2, Load, IS_MEMORY|IS_LOAD|MEMTYPE_QWORD );
		MakeOpcode( LD,   Load, IS_MEMORY|IS_LOAD|MEMTYPE_DWORD );

		// Stores!

		MakeOpcode( SQ,   Store, IS_MEMORY|IS_STORE|MEMTYPE_QWORD );
		MakeOpcode( SB,   Store, IS_MEMORY|IS_STORE|MEMTYPE_BYTE );
		MakeOpcode( SH,   Store, IS_MEMORY|IS_STORE|MEMTYPE_HALF );
		MakeOpcode( SWL,  Store, IS_MEMORY|IS_STORE|MEMTYPE_WORD|IS_LEFT );
		MakeOpcode( SW,   Store, IS_MEMORY|IS_STORE|MEMTYPE_WORD );
		MakeOpcode( SDL,  Store, IS_MEMORY|IS_STORE|MEMTYPE_DWORD|IS_LEFT );
		MakeOpcode( SDR,  Store, IS_MEMORY|IS_STORE|MEMTYPE_DWORD|IS_RIGHT );
		MakeOpcode( SWR,  Store, IS_MEMORY|IS_STORE|MEMTYPE_WORD|IS_RIGHT );
		MakeOpcode( SWC1, Store, IS_MEMORY|IS_STORE|MEMTYPE_WORD );
		MakeOpcode( SQC2, Store, IS_MEMORY|IS_STORE|MEMTYPE_QWORD );
		MakeOpcode( SD,   Store, IS_MEMORY|IS_STORE|MEMTYPE_DWORD );


		// Multimedia Instructions!

		MakeOpcodeM( PLZCW, MMI_Default, 0 );
		MakeOpcodeM( PMFHL, MMI_Default, 0 );
		MakeOpcodeM( PMTHL, MMI_Default, 0 );
		MakeOpcodeM( PSLLH, MMI_Default, 0 );
		MakeOpcodeM( PSRLH, MMI_Default, 0 );
		MakeOpcodeM( PSRAH, MMI_Default, 0 );
		MakeOpcodeM( PSLLW, MMI_Default, 0 );
		MakeOpcodeM( PSRLW, MMI_Default, 0 );
		MakeOpcodeM( PSRAW, MMI_Default, 0 );

		MakeOpcodeM( PADDW, MMI_Default, 0 );
		MakeOpcodeM( PADDH, MMI_Default, 0 );
		MakeOpcodeM( PADDB, MMI_Default, 0 );
		MakeOpcodeM( PADDSW, MMI_Default, 0 );
		MakeOpcodeM( PADDSH, MMI_Default, 0 );
		MakeOpcodeM( PADDSB, MMI_Default, 0 );
		MakeOpcodeM( PADDUW, MMI_Default, 0 );
		MakeOpcodeM( PADDUH, MMI_Default, 0 );
		MakeOpcodeM( PADDUB, MMI_Default, 0 );
		MakeOpcodeM( PSUBW, MMI_Default, 0 );
		MakeOpcodeM( PSUBH, MMI_Default, 0 );
		MakeOpcodeM( PSUBB, MMI_Default, 0 );
		MakeOpcodeM( PSUBSW, MMI_Default, 0 );
		MakeOpcodeM( PSUBSH, MMI_Default, 0 );
		MakeOpcodeM( PSUBSB, MMI_Default, 0 );
		MakeOpcodeM( PSUBUW, MMI_Default, 0 );
		MakeOpcodeM( PSUBUH, MMI_Default, 0 );
		MakeOpcodeM( PSUBUB, MMI_Default, 0 );

		MakeOpcodeM( PCGTW, MMI_Default, 0 );
		MakeOpcodeM( PMAXW, MMI_Default, 0 );
		MakeOpcodeM( PMAXH, MMI_Default, 0 );
		MakeOpcodeM( PCGTH, MMI_Default, 0 );
		MakeOpcodeM( PCGTB, MMI_Default, 0 );
		MakeOpcodeM( PEXTLW, MMI_Default, 0 );
		MakeOpcodeM( PEXTLH, MMI_Default, 0 );
		MakeOpcodeM( PEXTLB, MMI_Default, 0 );
		MakeOpcodeM( PEXT5, MMI_Default, 0 );
		MakeOpcodeM( PPACW, MMI_Default, 0 );
		MakeOpcodeM( PPACH, MMI_Default, 0 );
		MakeOpcodeM( PPACB, MMI_Default, 0 );
		MakeOpcodeM( PPAC5, MMI_Default, 0 );

		MakeOpcodeM( PABSW, MMI_Default, 0 );
		MakeOpcodeM( PABSH, MMI_Default, 0 );
		MakeOpcodeM( PCEQW, MMI_Default, 0 );
		MakeOpcodeM( PMINW, MMI_Default, 0 );
		MakeOpcodeM( PMINH, MMI_Default, 0 );
		MakeOpcodeM( PADSBH, MMI_Default, 0 );
		MakeOpcodeM( PCEQH, MMI_Default, 0 );
		MakeOpcodeM( PCEQB, MMI_Default, 0 );
		MakeOpcodeM( PEXTUW, MMI_Default, 0 );
		MakeOpcodeM( PEXTUH, MMI_Default, 0 );
		MakeOpcodeM( PEXTUB, MMI_Default, 0 );
		MakeOpcodeM( PSLLVW, MMI_Default, 0 );
		MakeOpcodeM( PSRLVW, MMI_Default, 0 );

		MakeOpcodeM( QFSRV, MMI_Default, 0 );

		MakeOpcodeM( PMADDH, MMI_Mult, 0 );
		MakeOpcodeM( PHMADH, MMI_Mult, 0 );
		MakeOpcodeM( PMSUBH, MMI_Mult, 0 );
		MakeOpcodeM( PHMSBH, MMI_Mult, 0 );
		MakeOpcodeM( PMULTH, MMI_Mult, 0 );
		MakeOpcodeM( PMADDW, MMI_Mult, 0 );
		MakeOpcodeM( PMSUBW, MMI_Mult, 0 );
		MakeOpcodeM( PMFHI, MMI_Mult, 0 );
		MakeOpcodeM( PMFLO, MMI_Mult, 0 );
		MakeOpcodeM( PMULTW, MMI_Mult, 0 );
		MakeOpcodeM( PMADDUW, MMI_Mult, 0 );
		MakeOpcodeM( PMULTUW, MMI_Mult, 0 );
		MakeOpcodeM( PDIVUW, MMI_Div, 0 );
		MakeOpcodeM( PDIVW, MMI_Div, 0 );
		MakeOpcodeM( PDIVBW, MMI_Div, 0 );

		MakeOpcodeM( PINTH, MMI_Default, 0 );
		MakeOpcodeM( PCPYLD, MMI_Default, 0 );
		MakeOpcodeM( PAND, MMI_Default, 0 );
		MakeOpcodeM( PXOR, MMI_Default, 0 );
		MakeOpcodeM( PEXEH, MMI_Default, 0 );
		MakeOpcodeM( PREVH, MMI_Default, 0 );
		MakeOpcodeM( PEXEW, MMI_Default, 0 );
		MakeOpcodeM( PROT3W, MMI_Default, 0 );

		MakeOpcodeM( PSRAVW, MMI_Default, 0 );
		MakeOpcodeM( PMTHI, MMI_Default, 0 );
		MakeOpcodeM( PMTLO, MMI_Default, 0 );
		MakeOpcodeM( PINTEH, MMI_Default, 0 );
		MakeOpcodeM( PCPYUD, MMI_Default, 0 );
		MakeOpcodeM( POR, MMI_Default, 0 );
		MakeOpcodeM( PNOR, MMI_Default, 0 );
		MakeOpcodeM( PEXCH, MMI_Default, 0 );
		MakeOpcodeM( PCPYH, MMI_Default, 0 );
		MakeOpcodeM( PEXCW, MMI_Default, 0 );

		//////////////////////////////////////////////////////////
		// COP0 Instructions

		MakeOpcodeClass( COP0_C0 );
		MakeOpcodeClass( COP0_BC0 );

		MakeOpcode0( MFC0, CopDefault, 0 );
		MakeOpcode0( MTC0, CopDefault, 0 );

		MakeOpcode0(BC0F, Branch, IS_BRANCH | BRANCHTYPE_BC0 | CONDTYPE_EQ);
		MakeOpcode0(BC0T, Branch, IS_BRANCH | BRANCHTYPE_BC0 | CONDTYPE_NE);
		MakeOpcode0(BC0FL, Branch, IS_BRANCH | BRANCHTYPE_BC0 | CONDTYPE_EQ | IS_LIKELY);
		MakeOpcode0(BC0TL, Branch, IS_BRANCH | BRANCHTYPE_BC0 | CONDTYPE_NE | IS_LIKELY);

		MakeOpcode0( TLBR, CopDefault, 0 );
		MakeOpcode0( TLBWI, CopDefault, 0 );
		MakeOpcode0( TLBWR, CopDefault, 0 );
		MakeOpcode0( TLBP, CopDefault, 0 );
		MakeOpcode0( ERET, CopDefault, IS_BRANCH|BRANCHTYPE_ERET );
		MakeOpcode0( EI, CopDefault, 0 );
		MakeOpcode0( DI, CopDefault, 0 );

		//////////////////////////////////////////////////////////
		// COP1 Instructions!

		MakeOpcodeClass( COP1_BC1 );
		MakeOpcodeClass( COP1_S );
		MakeOpcodeClass( COP1_W );		// contains CVT_S instruction *only*

		MakeOpcode1( MFC1, CopDefault, 0 );
		MakeOpcode1( CFC1, CopDefault, 0 );
		MakeOpcode1( MTC1, CopDefault, 0 );
		MakeOpcode1( CTC1, CopDefault, 0 );

		MakeOpcode1( BC1F,  Branch, IS_BRANCH|BRANCHTYPE_BC1|CONDTYPE_EQ );
		MakeOpcode1( BC1T,  Branch, IS_BRANCH|BRANCHTYPE_BC1|CONDTYPE_NE );
		MakeOpcode1( BC1FL, Branch, IS_BRANCH|BRANCHTYPE_BC1|CONDTYPE_EQ|IS_LIKELY );
		MakeOpcode1( BC1TL, Branch, IS_BRANCH|BRANCHTYPE_BC1|CONDTYPE_NE|IS_LIKELY );

		MakeOpcode1( ADD_S, CopDefault, 0 );
		MakeOpcode1( ADDA_S, CopDefault, 0 );
		MakeOpcode1( SUB_S, CopDefault, 0 );
		MakeOpcode1( SUBA_S, CopDefault, 0 );

		MakeOpcode1( ABS_S, CopDefault, 0 );
		MakeOpcode1( MOV_S, CopDefault, 0 );
		MakeOpcode1( NEG_S, CopDefault, 0 );
		MakeOpcode1( MAX_S, CopDefault, 0 );
		MakeOpcode1( MIN_S, CopDefault, 0 );

		MakeOpcode1( MUL_S, FPU_Mult, 0 );
		MakeOpcode1( DIV_S, 6*8, 0 );
		MakeOpcode1( SQRT_S, 6*8, 0 );
		MakeOpcode1( RSQRT_S, 8*8, 0 );
		MakeOpcode1( MULA_S, FPU_Mult, 0 );
		MakeOpcode1( MADD_S, FPU_Mult, 0 );
		MakeOpcode1( MSUB_S, FPU_Mult, 0 );
		MakeOpcode1( MADDA_S, FPU_Mult, 0 );
		MakeOpcode1( MSUBA_S, FPU_Mult, 0 );

		MakeOpcode1( C_F, CopDefault, 0 );
		MakeOpcode1( C_EQ, CopDefault, 0 );
		MakeOpcode1( C_LT, CopDefault, 0 );
		MakeOpcode1( C_LE, CopDefault, 0 );

		MakeOpcode1( CVT_S, CopDefault, 0 );
		MakeOpcode1( CVT_W, CopDefault, 0 );
	}

	namespace OpcodeTables
	{
		using namespace Opcodes;

		const OPCODE tbl_Standard[64] =
		{
			SPECIAL,       REGIMM,        J,             JAL,     BEQ,           BNE,     BLEZ,  BGTZ,
			ADDI,          ADDIU,         SLTI,          SLTIU,   ANDI,          ORI,     XORI,  LUI,
			COP0,          COP1,          COP2,          Unknown, BEQL,          BNEL,    BLEZL, BGTZL,
			DADDI,         DADDIU,        LDL,           LDR,     MMI,           Unknown, LQ,    SQ,
			LB,            LH,            LWL,           LW,      LBU,           LHU,     LWR,   LWU,
			SB,            SH,            SWL,           SW,      SDL,           SDR,     SWR,   CACHE,
			Unknown,       LWC1,          Unknown,       PREF,    Unknown,       Unknown, LQC2,  LD,
			Unknown,       SWC1,          Unknown,       Unknown, Unknown,       Unknown, SQC2,  SD
		};

		static const OPCODE tbl_Special[64] =
		{
			SLL,      Unknown,  SRL,      SRA,      SLLV,    Unknown, SRLV,    SRAV,
			JR,       JALR,     MOVZ,     MOVN,     SYSCALL, BREAK,   Unknown, SYNC,
			MFHI,     MTHI,     MFLO,     MTLO,     DSLLV,   Unknown, DSRLV,   DSRAV,
			MULT,     MULTU,    DIV,      DIVU,     Unknown, Unknown, Unknown, Unknown,
			ADD,      ADDU,     SUB,      SUBU,     AND,     OR,      XOR,     NOR,
			MFSA,     MTSA,     SLT,      SLTU,     DADD,    DADDU,   DSUB,    DSUBU,
			TGE,      TGEU,     TLT,      TLTU,     TEQ,     Unknown, TNE,     Unknown,
			DSLL,     Unknown,  DSRL,     DSRA,     DSLL32,  Unknown, DSRL32,  DSRA32
		};

		static const OPCODE tbl_RegImm[32] = {
			BLTZ,   BGEZ,   BLTZL,      BGEZL,   Unknown, Unknown, Unknown, Unknown,
			TGEI,   TGEIU,  TLTI,       TLTIU,   TEQI,    Unknown, TNEI,    Unknown,
			BLTZAL, BGEZAL, BLTZALL,    BGEZALL, Unknown, Unknown, Unknown, Unknown,
			MTSAB,  MTSAH , Unknown,    Unknown, Unknown, Unknown, Unknown, Unknown,
		};

		static const OPCODE tbl_MMI[64] =
		{
			MADD,               MADDU,           MMI_Unknown,          MMI_Unknown,          PLZCW,            MMI_Unknown,       MMI_Unknown,          MMI_Unknown,
			MMI0,      MMI2,   MMI_Unknown,          MMI_Unknown,          MMI_Unknown,      MMI_Unknown,       MMI_Unknown,          MMI_Unknown,
			MFHI1,              MTHI1,           MFLO1,                MTLO1,                MMI_Unknown,      MMI_Unknown,       MMI_Unknown,          MMI_Unknown,
			MULT1,              MULTU1,          DIV1,                 DIVU1,                MMI_Unknown,      MMI_Unknown,       MMI_Unknown,          MMI_Unknown,
			MADD1,              MADDU1,          MMI_Unknown,          MMI_Unknown,          MMI_Unknown,      MMI_Unknown,       MMI_Unknown,          MMI_Unknown,
			MMI1,      MMI3,   MMI_Unknown,          MMI_Unknown,          MMI_Unknown,      MMI_Unknown,       MMI_Unknown,          MMI_Unknown,
			PMFHL,              PMTHL,           MMI_Unknown,          MMI_Unknown,          PSLLH,            MMI_Unknown,       PSRLH,                PSRAH,
			MMI_Unknown,        MMI_Unknown,     MMI_Unknown,          MMI_Unknown,          PSLLW,            MMI_Unknown,       PSRLW,                PSRAW,
		};

		static const OPCODE tbl_MMI0[32] =
		{
			PADDW,         PSUBW,         PCGTW,          PMAXW,
			PADDH,         PSUBH,         PCGTH,          PMAXH,
			PADDB,         PSUBB,         PCGTB,          MMI_Unknown,
			MMI_Unknown,   MMI_Unknown,   MMI_Unknown,    MMI_Unknown,
			PADDSW,        PSUBSW,        PEXTLW,         PPACW,
			PADDSH,        PSUBSH,        PEXTLH,         PPACH,
			PADDSB,        PSUBSB,        PEXTLB,         PPACB,
			MMI_Unknown,   MMI_Unknown,   PEXT5,          PPAC5,
		};

		static const OPCODE tbl_MMI1[32] =
		{
			MMI_Unknown,   PABSW,         PCEQW,         PMINW,
			PADSBH,        PABSH,         PCEQH,         PMINH,
			MMI_Unknown,   MMI_Unknown,   PCEQB,         MMI_Unknown,
			MMI_Unknown,   MMI_Unknown,   MMI_Unknown,   MMI_Unknown,
			PADDUW,        PSUBUW,        PEXTUW,        MMI_Unknown,
			PADDUH,        PSUBUH,        PEXTUH,        MMI_Unknown,
			PADDUB,        PSUBUB,        PEXTUB,        QFSRV,
			MMI_Unknown,   MMI_Unknown,   MMI_Unknown,   MMI_Unknown,
		};


		static const OPCODE tbl_MMI2[32] =
		{
			PMADDW,        MMI_Unknown,   PSLLVW,        PSRLVW,
			PMSUBW,        MMI_Unknown,   MMI_Unknown,   MMI_Unknown,
			PMFHI,         PMFLO,         PINTH,         MMI_Unknown,
			PMULTW,        PDIVW,         PCPYLD,        MMI_Unknown,
			PMADDH,        PHMADH,        PAND,          PXOR,
			PMSUBH,        PHMSBH,        MMI_Unknown,   MMI_Unknown,
			MMI_Unknown,   MMI_Unknown,   PEXEH,         PREVH,
			PMULTH,        PDIVBW,        PEXEW,         PROT3W,
		};

		static const OPCODE tbl_MMI3[32] =
		{
			PMADDUW,       MMI_Unknown,   MMI_Unknown,   PSRAVW,
			MMI_Unknown,   MMI_Unknown,   MMI_Unknown,   MMI_Unknown,
			PMTHI,         PMTLO,         PINTEH,        MMI_Unknown,
			PMULTUW,       PDIVUW,        PCPYUD,        MMI_Unknown,
			MMI_Unknown,   MMI_Unknown,   POR,           PNOR,
			MMI_Unknown,   MMI_Unknown,   MMI_Unknown,   MMI_Unknown,
			MMI_Unknown,   MMI_Unknown,   PEXCH,         PCPYH,
			MMI_Unknown,   MMI_Unknown,   PEXCW,         MMI_Unknown,
		};

		static const OPCODE tbl_COP0[32] =
		{
			MFC0,         COP0_Unknown, COP0_Unknown, COP0_Unknown, MTC0,         COP0_Unknown, COP0_Unknown, COP0_Unknown,
			COP0_BC0, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown,
			COP0_C0,  COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown,
			COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown,
		};

		static const OPCODE tbl_COP0_BC0[32] =
		{
			BC0F,         BC0T,         BC0FL,        BC0TL,        COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown,
			COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown,
			COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown,
			COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown,
		};

		static const OPCODE tbl_COP0_C0[64] =
		{
			COP0_Unknown, TLBR,         TLBWI,        COP0_Unknown, COP0_Unknown, COP0_Unknown, TLBWR,        COP0_Unknown,
			TLBP,         COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown,
			COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown,
			ERET,         COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown,
			COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown,
			COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown,
			COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown,
			EI,           DI,           COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown, COP0_Unknown
		};

		static const OPCODE tbl_COP1[32] =
		{
			MFC1,         COP1_Unknown, CFC1,         COP1_Unknown, MTC1,         COP1_Unknown, CTC1,         COP1_Unknown,
			COP1_BC1, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown,
			COP1_S,   COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_W, COP1_Unknown, COP1_Unknown, COP1_Unknown,
			COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown,
		};

		static const OPCODE tbl_COP1_BC1[32] =
		{
			BC1F,         BC1T,         BC1FL,        BC1TL,        COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown,
			COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown,
			COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown,
			COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown, COP1_Unknown,
		};

		static const OPCODE tbl_COP1_S[64] =
		{
			ADD_S,       SUB_S,       MUL_S,       DIV_S,       SQRT_S,      ABS_S,       MOV_S,       NEG_S,
			COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,
			COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,RSQRT_S,     COP1_Unknown,
			ADDA_S,      SUBA_S,      MULA_S,      COP1_Unknown,MADD_S,      MSUB_S,      MADDA_S,     MSUBA_S,
			COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,CVT_W,       COP1_Unknown,COP1_Unknown,COP1_Unknown,
			MAX_S,       MIN_S,       COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,
			C_F,         COP1_Unknown,C_EQ,        COP1_Unknown,C_LT,        COP1_Unknown,C_LE,        COP1_Unknown,
			COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,
		};

		static const OPCODE tbl_COP1_W[64] =
		{
			COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,
			COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,
			COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,
			COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,
			CVT_S,       COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,
			COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,
			COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,
			COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,COP1_Unknown,
		};

	}	// end namespace R5900::OpcodeTables

	namespace Opcodes
	{
		using namespace OpcodeTables;

		const OPCODE& Class_SPECIAL(u32 op) { return tbl_Special[op & 0x3F]; }
		const OPCODE& Class_REGIMM(u32 op)  { return tbl_RegImm[(op >> 16) & 0x1F]; }

		const OPCODE& Class_MMI(u32 op)  { return tbl_MMI[op & 0x3F]; }
		const OPCODE& Class_MMI0(u32 op) { return tbl_MMI0[(op >> 6) & 0x1F]; }
		const OPCODE& Class_MMI1(u32 op) { return tbl_MMI1[(op >> 6) & 0x1F]; }
		const OPCODE& Class_MMI2(u32 op) { return tbl_MMI2[(op >> 6) & 0x1F]; }
		const OPCODE& Class_MMI3(u32 op) { return tbl_MMI3[(op >> 6) & 0x1F]; }

		const OPCODE& Class_COP0(u32 op) { return tbl_COP0[(op >> 21) & 0x1F]; }
		const OPCODE& Class_COP0_BC0(u32 op) { return tbl_COP0_BC0[(op >> 16) & 0x03]; }
		const OPCODE& Class_COP0_C0(u32 op) { return tbl_COP0_C0[op & 0x3F]; }

		const OPCODE& Class_COP1(u32 op) { return tbl_COP1[(op >> 21) & 0x1F]; }
		const OPCODE& Class_COP1_BC1(u32 op) { return tbl_COP1_BC1[(op >> 16) & 0x1F]; }
		const OPCODE& Class_COP1_S(u32 op) { return tbl_COP1_S[op & 0x3F]; }
		const OPCODE& Class_COP1_W(u32 op) { return tbl_COP1_W[op & 0x3F]; }

		// These are for future use when the COP2 tables are completed.
		//const OPCODE& Class_COP2() { return tbl_COP2[_Rs_]; }
		//const OPCODE& Class_COP2_BC2() { return tbl_COP2_BC2[_Rt_]; }
		//const OPCODE& Class_COP2_SPECIAL() { return tbl_COP2_SPECIAL[_Funct_]; }
		//const OPCODE& Class_COP2_SPECIAL2() { return tbl_COP2_SPECIAL2[(cpuRegs.code & 0x3) | ((cpuRegs.code >> 4) & 0x7c)]; }
	}
}	// end namespace R5900

void (*Int_COP2PrintTable[32])() = {
    COP2_Unknown, QMFC2,        CFC2,         COP2_Unknown, COP2_Unknown, QMTC2,        CTC2,         COP2_Unknown,
    COP2_BC2,     COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown,
    COP2_SPECIAL, COP2_SPECIAL, COP2_SPECIAL, COP2_SPECIAL, COP2_SPECIAL, COP2_SPECIAL, COP2_SPECIAL, COP2_SPECIAL,
	COP2_SPECIAL, COP2_SPECIAL, COP2_SPECIAL, COP2_SPECIAL, COP2_SPECIAL, COP2_SPECIAL, COP2_SPECIAL, COP2_SPECIAL,
};

void (*Int_COP2BC2PrintTable[32])() = {
    BC2F,         BC2T,         BC2FL,        BC2TL,        COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown,
    COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown,
    COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown,
    COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown, COP2_Unknown,
};

void (*Int_COP2SPECIAL1PrintTable[64])() =
{
 VADDx,       VADDy,       VADDz,       VADDw,       VSUBx,        VSUBy,        VSUBz,        VSUBw,
 VMADDx,      VMADDy,      VMADDz,      VMADDw,      VMSUBx,       VMSUBy,       VMSUBz,       VMSUBw,
 VMAXx,       VMAXy,       VMAXz,       VMAXw,       VMINIx,       VMINIy,       VMINIz,       VMINIw,
 VMULx,       VMULy,       VMULz,       VMULw,       VMULq,        VMAXi,        VMULi,        VMINIi,
 VADDq,       VMADDq,      VADDi,       VMADDi,      VSUBq,        VMSUBq,       VSUBi,        VMSUBi,
 VADD,        VMADD,       VMUL,        VMAX,        VSUB,         VMSUB,        VOPMSUB,      VMINI,
 VIADD,       VISUB,       VIADDI,      COP2_Unknown,VIAND,        VIOR,         COP2_Unknown, COP2_Unknown,
 VCALLMS,     VCALLMSR,    COP2_Unknown,COP2_Unknown,COP2_SPECIAL2,COP2_SPECIAL2,COP2_SPECIAL2,COP2_SPECIAL2,
};

void (*Int_COP2SPECIAL2PrintTable[128])() =
{
 VADDAx      ,VADDAy      ,VADDAz      ,VADDAw      ,VSUBAx      ,VSUBAy      ,VSUBAz      ,VSUBAw,
 VMADDAx     ,VMADDAy     ,VMADDAz     ,VMADDAw     ,VMSUBAx     ,VMSUBAy     ,VMSUBAz     ,VMSUBAw,
 VITOF0      ,VITOF4      ,VITOF12     ,VITOF15     ,VFTOI0      ,VFTOI4      ,VFTOI12     ,VFTOI15,
 VMULAx      ,VMULAy      ,VMULAz      ,VMULAw      ,VMULAq      ,VABS        ,VMULAi      ,VCLIPw,
 VADDAq      ,VMADDAq     ,VADDAi      ,VMADDAi     ,VSUBAq      ,VMSUBAq     ,VSUBAi      ,VMSUBAi,
 VADDA       ,VMADDA      ,VMULA       ,COP2_Unknown,VSUBA       ,VMSUBA      ,VOPMULA     ,VNOP,
 VMOVE       ,VMR32       ,COP2_Unknown,COP2_Unknown,VLQI        ,VSQI        ,VLQD        ,VSQD,
 VDIV        ,VSQRT       ,VRSQRT      ,VWAITQ      ,VMTIR       ,VMFIR       ,VILWR       ,VISWR,
 VRNEXT      ,VRGET       ,VRINIT      ,VRXOR       ,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,
 COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,
 COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,
 COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,
 COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,
 COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,
 COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,
 COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,COP2_Unknown,
};
