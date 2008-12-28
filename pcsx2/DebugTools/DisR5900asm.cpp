/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2008  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "PrecompiledHeader.h"

#ifdef __LINUX__
#include <cstdarg>
#endif

#include "Debug.h"
#include "R5900.h"
#include "DisASM.h"
#include "InterTables.h"

unsigned long opcode_addr;

using namespace std;

/*
//DECODE PROCUDURES

//cop0
#define DECODE_FS           (DECODE_RD)
#define DECODE_FT           (DECODE_RT)
#define DECODE_FD           (DECODE_SA)
/// ********

#define DECODE_FUNCTION     ((cpuRegs.code) & 0x3F)
#define DECODE_RD     ((cpuRegs.code >> 11) & 0x1F) // The rd part of the instruction register 
#define DECODE_RT     ((cpuRegs.code >> 16) & 0x1F) // The rt part of the instruction register 
#define DECODE_RS     ((cpuRegs.code >> 21) & 0x1F) // The rs part of the instruction register 
#define DECODE_SA     ((cpuRegs.code >>  6) & 0x1F) // The sa part of the instruction register
#define DECODE_IMMED     ( cpuRegs.code & 0xFFFF)      // The immediate part of the instruction register
#define DECODE_OFFSET  ((((short)DECODE_IMMED * 4) + opcode_addr + 4))
#define DECODE_JUMP     (opcode_addr & 0xf0000000)|((cpuRegs.code&0x3ffffff)<<2)
#define DECODE_SYSCALL      ((opcode_addr & 0x03FFFFFF) >> 6)
#define DECODE_BREAK        (DECODE_SYSCALL)
#define DECODE_C0BC         ((cpuRegs.code >> 16) & 0x03)
#define DECODE_C1BC         ((cpuRegs.code >> 16) & 0x03)
#define DECODE_C2BC         ((cpuRegs.code >> 16) & 0x03)   
*/
/*************************CPUS REGISTERS**************************/
const char *GPR_REG[32] = {
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};
const char *COP0_REG[32] ={
	"Index","Random","EntryLo0","EntryLo1","Context","PageMask",
	"Wired","C0r7","BadVaddr","Count","EntryHi","Compare","Status",
	"Cause","EPC","PRId","Config","C0r17","C0r18","C0r19","C0r20",
	"C0r21","C0r22","C0r23","Debug","Perf","C0r26","C0r27","TagLo",
	"TagHi","ErrorPC","C0r31"
};
//floating point cop1 Floating point reg
const char *COP1_REG_FP[32] ={
 	"f00","f01","f02","f03","f04","f05","f06","f07",
	"f08","f09","f10","f11","f12","f13","f14","f15",
	"f16","f17","f18","f19","f20","f21","f21","f23",
	"f24","f25","f26","f27","f28","f29","f30","f31"
};
//floating point cop1 control registers
const char *COP1_REG_FCR[32] ={
 	"fcr00","fcr01","fcr02","fcr03","fcr04","fcr05","fcr06","fcr07",
	"fcr08","fcr09","fcr10","fcr11","fcr12","fcr13","fcr14","fcr15",
	"fcr16","fcr17","fcr18","fcr19","fcr20","fcr21","fcr21","fcr23",
	"fcr24","fcr25","fcr26","fcr27","fcr28","fcr29","fcr30","fcr31"
};

//floating point cop2 reg
const char *COP2_REG_FP[32] ={
	"vf00","vf01","vf02","vf03","vf04","vf05","vf06","vf07",
	"vf08","vf09","vf10","vf11","vf12","vf13","vf14","vf15",
	"vf16","vf17","vf18","vf19","vf20","vf21","vf21","vf23",
	"vf24","vf25","vf26","vf27","vf28","vf29","vf30","vf31"
};
//cop2 control registers

const char *COP2_REG_CTL[32] ={
	"vi00","vi01","vi02","vi03","vi04","vi05","vi06","vi07",
	"vi08","vi09","vi10","vi11","vi12","vi13","vi14","vi15",
	"Status","MACflag","ClipFlag","c2c19","R","I","Q","c2c23",
	"c2c24","c2c25","TPC","CMSAR0","FBRST","VPU-STAT","c2c30","CMSAR1"
};

void P_COP0_Unknown( string& output );
void P_COP0_BC0( string& output );
void P_COP0_Func( string& output ); 
void P_COP1_BC1( string& output );
void P_COP1_S( string& output );
void P_COP1_W( string& output );
void P_COP1_Unknown( string& output );
void P_COP2_BC2( string& output );
void P_COP2_SPECIAL( string& output );
void P_COP2_Unknown( string& output );
void P_COP2_SPECIAL2( string& output );

//****************************************************************************
//** COP0                                                                   **
//****************************************************************************
void P_MFC0( string& output );
void P_MTC0( string& output );
void P_BC0F( string& output );
void P_BC0T( string& output );
void P_BC0FL( string& output );
void P_BC0TL( string& output );
void P_TLBR( string& output );
void P_TLBWI( string& output );
void P_TLBWR( string& output );
void P_TLBP( string& output );
void P_ERET( string& output );
void P_DI( string& output );
void P_EI( string& output );
//****************************************************************************
//** END OF COP0                                                            **
//****************************************************************************
//****************************************************************************
//** COP1 - Floating Point Unit (FPU)                                       **
//****************************************************************************
void P_MFC1( string& output );
void P_CFC1( string& output );
void P_MTC1( string& output );
void P_CTC1( string& output );
void P_BC1F( string& output );
void P_BC1T( string& output );
void P_BC1FL( string& output );
void P_BC1TL( string& output );
void P_ADD_S( string& output );  
void P_SUB_S( string& output );  
void P_MUL_S( string& output );  
void P_DIV_S( string& output );  
void P_SQRT_S( string& output ); 
void P_ABS_S( string& output );  
void P_MOV_S( string& output ); 
void P_NEG_S( string& output ); 
void P_RSQRT_S( string& output );  
void P_ADDA_S( string& output ); 
void P_SUBA_S( string& output ); 
void P_MULA_S( string& output );
void P_MADD_S( string& output ); 
void P_MSUB_S( string& output ); 
void P_MADDA_S( string& output ); 
void P_MSUBA_S( string& output );
void P_CVT_W( string& output );
void P_MAX_S( string& output );
void P_MIN_S( string& output );
void P_C_F( string& output );
void P_C_EQ( string& output );
void P_C_LT( string& output );
void P_C_LE( string& output );
 void P_CVT_S( string& output );
//****************************************************************************
//** END OF COP1                                                            **
//****************************************************************************
//****************************************************************************
//** COP2 - (VU0)                                                           **
//****************************************************************************
void P_QMFC2( string& output ); 
void P_CFC2( string& output ); 
void P_QMTC2( string& output );
void P_CTC2( string& output );  
void P_BC2F( string& output );
void P_BC2T( string& output );
void P_BC2FL( string& output );
void P_BC2TL( string& output );
//*****************SPECIAL 1 VUO TABLE*******************************
void P_VADDx( string& output );       
void P_VADDy( string& output );       
void P_VADDz( string& output );       
void P_VADDw( string& output );       
void P_VSUBx( string& output );        
void P_VSUBy( string& output );        
void P_VSUBz( string& output );
void P_VSUBw( string& output ); 
void P_VMADDx( string& output );
void P_VMADDy( string& output );
void P_VMADDz( string& output );
void P_VMADDw( string& output );
void P_VMSUBx( string& output );
void P_VMSUBy( string& output );
void P_VMSUBz( string& output );       
void P_VMSUBw( string& output ); 
void P_VMAXx( string& output );       
void P_VMAXy( string& output );       
void P_VMAXz( string& output );       
void P_VMAXw( string& output );       
void P_VMINIx( string& output );       
void P_VMINIy( string& output );       
void P_VMINIz( string& output );       
void P_VMINIw( string& output ); 
void P_VMULx( string& output );       
void P_VMULy( string& output );       
void P_VMULz( string& output );       
void P_VMULw( string& output );       
void P_VMULq( string& output );        
void P_VMAXi( string& output );        
void P_VMULi( string& output );        
void P_VMINIi( string& output );
void P_VADDq( string& output );
void P_VMADDq( string& output );      
void P_VADDi( string& output );       
void P_VMADDi( string& output );      
void P_VSUBq( string& output );        
void P_VMSUBq( string& output );       
void P_VSUbi( string& output );        
void P_VMSUBi( string& output ); 
void P_VADD( string& output );        
void P_VMADD( string& output );       
void P_VMUL( string& output );        
void P_VMAX( string& output );        
void P_VSUB( string& output );         
void P_VMSUB( string& output );       
void P_VOPMSUB( string& output );      
void P_VMINI( string& output );  
void P_VIADD( string& output );       
void P_VISUB( string& output );       
void P_VIADDI( string& output );        
void P_VIAND( string& output );        
void P_VIOR( string& output );        
void P_VCALLMS( string& output );     
void P_CALLMSR( string& output );   
//***********************************END OF SPECIAL1 VU0 TABLE*****************************
//******************************SPECIAL2 VUO TABLE*****************************************
void P_VADDAx( string& output );      
void P_VADDAy( string& output );      
void P_VADDAz( string& output );      
void P_VADDAw( string& output );      
void P_VSUBAx( string& output );      
void P_VSUBAy( string& output );      
void P_VSUBAz( string& output );      
void P_VSUBAw( string& output );
void P_VMADDAx( string& output );     
void P_VMADDAy( string& output );     
void P_VMADDAz( string& output );     
void P_VMADDAw( string& output );     
void P_VMSUBAx( string& output );     
void P_VMSUBAy( string& output );     
void P_VMSUBAz( string& output );     
void P_VMSUBAw( string& output );
void P_VITOF0( string& output );      
void P_VITOF4( string& output );      
void P_VITOF12( string& output );     
void P_VITOF15( string& output );     
void P_VFTOI0( string& output );      
void P_VFTOI4( string& output );      
void P_VFTOI12( string& output );     
void P_VFTOI15( string& output );
void P_VMULAx( string& output );      
void P_VMULAy( string& output );      
void P_VMULAz( string& output );      
void P_VMULAw( string& output );      
void P_VMULAq( string& output );      
void P_VABS( string& output );        
void P_VMULAi( string& output );      
void P_VCLIPw( string& output );
void P_VADDAq( string& output );      
void P_VMADDAq( string& output );     
void P_VADDAi( string& output );      
void P_VMADDAi( string& output );     
void P_VSUBAq( string& output );      
void P_VMSUBAq( string& output );     
void P_VSUBAi( string& output );      
void P_VMSUBAi( string& output );
void P_VADDA( string& output );       
void P_VMADDA( string& output );      
void P_VMULA( string& output );       
void P_VSUBA( string& output );       
void P_VMSUBA( string& output );      
void P_VOPMULA( string& output );     
void P_VNOP( string& output );   
void P_VMONE( string& output );       
void P_VMR32( string& output );       
void P_VLQI( string& output );        
void P_VSQI( string& output );        
void P_VLQD( string& output );        
void P_VSQD( string& output );   
void P_VDIV( string& output );        
void P_VSQRT( string& output );       
void P_VRSQRT( string& output );      
void P_VWAITQ( string& output );     
void P_VMTIR( string& output );       
void P_VMFIR( string& output );       
void P_VILWR( string& output );       
void P_VISWR( string& output );  
void P_VRNEXT( string& output );      
void P_VRGET( string& output );       
void P_VRINIT( string& output );      
void P_VRXOR( string& output );  
//************************************END OF SPECIAL2 VUO TABLE****************************     


/*
    CPU: Instructions encoded by opcode field.
    31---------26---------------------------------------------------0
    |  opcode   |                                                   |
    ------6----------------------------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
000 | *1    | *2    | J     | JAL   | BEQ   | BNE   | BLEZ  | BGTZ  |
001 | ADDI  | ADDIU | SLTI  | SLTIU | ANDI  | ORI   | XORI  | LUI   |
010 | *3    | *4    |  *5   | ---   | BEQL  | BNEL  | BLEZL | BGTZL |
011 | DADDI |DADDIU | LDL   | LDR   |  *6   |  ---  |  LQ   | SQ    |
100 | LB    | LH    | LWL   | LW    | LBU   | LHU   | LWR   | LWU   |
101 | SB    | SH    | SWL   | SW    | SDL   | SDR   | SWR   | CACHE |
110 | ---   | LWC1  | ---   | PREF  | ---   | ---   | LQC2  | LD    |
111 | ---   | SWC1  | ---   | ---   | ---   | ---   | SQC2  | SD    |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
     *1 = SPECIAL, see SPECIAL list    *2 = REGIMM, see REGIMM list
     *3 = COP0                         *4 = COP1
     *5 = COP2                         *6 = MMI table
*/

/*
     SPECIAL: Instr. encoded by function field when opcode field = SPECIAL
    31---------26------------------------------------------5--------0
    | = SPECIAL |                                         | function|
    ------6----------------------------------------------------6-----
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
000 | SLL   | ---   | SRL   | SRA   | SLLV  |  ---  | SRLV  | SRAV  |
001 | JR    | JALR  | MOVZ  | MOVN  |SYSCALL| BREAK |  ---  | SYNC  |
010 | MFHI  | MTHI  | MFLO  | MTLO  | DSLLV |  ---  | DSRLV | DSRAV |
011 | MULT  | MULTU | DIV   | DIVU  | ----  |  ---  | ----  | ----- |
100 | ADD   | ADDU  | SUB   | SUBU  | AND   | OR    | XOR   | NOR   |
101 | MFSA  | MTSA  | SLT   | SLTU  | DADD  | DADDU | DSUB  | DSUBU |
110 | TGE   | TGEU  | TLT   | TLTU  | TEQ   |  ---  | TNE   |  ---  |
111 | DSLL  |  ---  | DSRL  | DSRA  |DSLL32 |  ---  |DSRL32 |DSRA32 |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
*/

/*
    REGIMM: Instructions encoded by the rt field when opcode field = REGIMM.
    31---------26----------20-------16------------------------------0
    | = REGIMM  |          |   rt    |                              |
    ------6---------------------5------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
 00 | BLTZ  | BGEZ  | BLTZL | BGEZL |  ---  |  ---  |  ---  |  ---  |
 01 | TGEI  | TGEIU | TLTI  | TLTIU | TEQI  |  ---  | TNEI  |  ---  |
 10 | BLTZAL| BGEZAL|BLTZALL|BGEZALL|  ---  |  ---  |  ---  |  ---  |
 11 | MTSAB | MTSAH |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
*/

/*
    MMI: Instr. encoded by function field when opcode field = MMI
    31---------26------------------------------------------5--------0
    | = MMI     |                                         | function|
    ------6----------------------------------------------------6-----
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
000 | MADD  | MADDU |  ---  |  ---  | PLZCW |  ---  |  ---  |  ---  |
001 |  *1   |  *2   |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
010 | MFHI1 | MTHI1 | MFLO1 | MTLO1 |  ---  |  ---  |  ---  |  ---  |
011 | MULT1 | MULTU1| DIV1  | DIVU1 |  ---  |  ---  |  ---  |  ---  |
100 | MADD1 | MADDU1|  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
101 |  *3   |  *4   |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
110 | PMFHL | PMTHL |  ---  |  ---  | PSLLH |  ---  | PSRLH | PSRAH |
111 |  ---  |  ---  |  ---  |  ---  | PSLLW |  ---  | PSRLW | PSRAW |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|

     *1 = see MMI0 table    *2 = see MMI2 Table
     *3 = see MMI1 table    *4 = see MMI3 Table
*/

/*
  MMI0: Instr. encoded by function field when opcode field = MMI & MMI0

    31---------26------------------------------10--------6-5--------0
    |          |                              |function  | MMI0    |
    ------6----------------------------------------------------6-----
    |--000--|--001--|--010--|--011--| lo
000 |PADDW  | PSUBW | PCGTW | PMAXW |
001 |PADDH  | PSUBH | PCGTH | PMAXH |
010 |PADDB  | PSUBB | PCGTB |  ---  |
011 | ---   | ---   |  ---  |  ---  |
100 |PADDSW |PSUBSW |PEXTLW | PPACW |
101 |PADDSH |PSUBSH |PEXTLH | PPACH |
110 |PADDSB |PSUBSB |PEXTLB | PPACB |
111 | ---   |  ---  | PEXT5 | PPAC5 |
 hi |-------|-------|-------|-------|
*/

/*
  MMI1: Instr. encoded by function field when opcode field = MMI & MMI1

    31---------26------------------------------------------5--------0
    |           |                               |function  | MMI1    |
    ------6----------------------------------------------------6-----
    |--000--|--001--|--010--|--011--| lo
000 |  ---  | PABSW | PCEQW | PMINW |
001 |PADSBH | PABSH | PCEQH | PMINH | 
010 |  ---  |  ---  | PCEQB |  ---  |
011 |  ---  |  ---  |  ---  |  ---  |
100 |PADDUW |PSUBUW |PEXTUW |  ---  |
101 |PADDUH |PSUBUH |PEXTUH |  ---  |
110 |PADDUB |PSUBUB |PEXTUB | QFSRV |
111 |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|
*/

/*
  MMI2: Instr. encoded by function field when opcode field = MMI & MMI2

    31---------26------------------------------------------5--------0
    |           |                              |function   | MMI2    |
    ------6----------------------------------------------------6-----
    |--000--|--001--|--010--|--011--| lo
000 |PMADDW |  ---  |PSLLVW |PSRLVW |
001 |PMSUBW |  ---  |  ---  |  ---  | 
010 |PMFHI  |PMFLO  |PINTH  |  ---  |
011 |PMULTW |PDIVW  |PCPYLD |  ---  |
100 |PMADDH |PHMADH | PAND  |  PXOR |
101 |PMSUBH |PHMSBH |  ---  |  ---  |
110 | ---   |  ---  | PEXEH | PREVH |
111 |PMULTH |PDIVBW | PEXEW |PROT3W |
 hi |-------|-------|-------|-------|
*/

/*
  MMI3: Instr. encoded by function field when opcode field = MMI & MMI3
    31---------26------------------------------------------5--------0
    |           |                               |function  | MMI3   |
    ------6----------------------------------------------------6-----
    |--000--|--001--|--010--|--011--| lo
000 |PMADDUW|  ---  |  ---  |PSRAVW |
001 |  ---  |  ---  |  ---  |  ---  | 
010 |PMTHI  | PMTLO |PINTEH |  ---  |
011 |PMULTUW| PDIVUW|PCPYUD |  ---  |
100 |  ---  |  ---  |  POR  | PNOR  |
101 |  ---  |  ---  |  ---  |  ---  |
110 |  ---  |  ---  | PEXCH | PCPYH |
111 |  ---  |  ---  | PEXCW |  ---  |
 hi |-------|-------|-------|-------|
 */

/*
    COP0: Instructions encoded by the rs field when opcode = COP0.
    31--------26-25------21 ----------------------------------------0
    |  = COP0   |   fmt   |                                         |
    ------6----------5-----------------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
 00 | MFC0  |  ---  |  ---  |  ---  | MTC0  |  ---  |  ---  |  ---  |
 01 |  *1   |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 10 |  *2   |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 11 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
     *1=BC See BC0 list       *2 = TLB instr, see TLB list
*/
void (*COP0PrintTable[32])( string& output ) = {
    P_MFC0,         P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_MTC0,         P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown,
    P_COP0_BC0,     P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown,
    P_COP0_Func,    P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown,
    P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown,
};
/*
    BC0: Instructions encoded by the rt field when opcode = COP0 & rs field=BC0
    31--------26-25------21 ----------------------------------------0
    |  = COP0   |   fmt   |                                         |
    ------6----------5-----------------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
 00 | BC0F  | BC0T  | BC0FL | BC0TL |  ---  |  ---  |  ---  |  ---  |
 01 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 10 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 11 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
*/
void (*COP0BC0PrintTable[32])( string& output ) = {
    P_BC0F,         P_BC0T,         P_BC0FL,        P_BC0TL,   P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown,
    P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown,
    P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown,
    P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown,
};
/*
    C0=Instructions encode by function field when Opcode field=COP0 & rs field=C0
    31---------26------------------------------------------5--------0
    |           |                                         |         |
    ------6----------------------------------------------------6-----
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
000 | ---   |  TLBR | TLBWI |  ---  |  ---  |  ---  | TLBWR |  ---  |
001 | TLBP  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
010 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
011 | ERET  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
100 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
101 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
110 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
111 |  EI   |  DI   |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
*/
void (*COP0C0PrintTable[64])( string& output ) = {
    P_COP0_Unknown, P_TLBR,         P_TLBWI,        P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_TLBWR,        P_COP0_Unknown,
    P_TLBP,         P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown,
    P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown,
    P_ERET,         P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown,
    P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown,
    P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown,
    P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown,
    P_EI,           P_DI,           P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown, P_COP0_Unknown
};
/*
    COP1: Instructions encoded by the fmt field when opcode = COP1.
    31--------26-25------21 ----------------------------------------0
    |  = COP1   |   fmt   |                                         |
    ------6----------5-----------------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
 00 | MFC1  |  ---  | CFC1  |  ---  | MTC1  |  ---  | CTC1  |  ---  |
 01 | *1    |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 10 | *2    |  ---  |  ---  |  ---  | *3    |  ---  |  ---  |  ---  |
 11 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
     *1 = BC instructions, see BC1 list   *2 = S instr, see FPU list            
     *3 = W instr, see FPU list 
*/
void (*COP1PrintTable[32])( string& output ) = {
    P_MFC1,         P_COP1_Unknown, P_CFC1,         P_COP1_Unknown, P_MTC1,         P_COP1_Unknown, P_CTC1,         P_COP1_Unknown,
    P_COP1_BC1,     P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown,
    P_COP1_S,       P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_W,       P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown,
    P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown,
};
/*
    BC1: Instructions encoded by the rt field when opcode = COP1 & rs field=BC1
    31--------26-25------21 ----------------------------------------0
    |  = COP1   |   fmt   |                                         |
    ------6----------5-----------------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
 00 | BC1F  | BC1T  | BC1FL | BC1TL |  ---  |  ---  |  ---  |  ---  |
 01 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 10 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 11 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|   
*/
void (*COP1BC1PrintTable[32])( string& output ) = {
    P_BC1F,         P_BC1T,         P_BC1FL,        P_BC1TL,        P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown,
    P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown,
    P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown,
    P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown, P_COP1_Unknown,
};
/*
    FPU: Instructions encoded by the function field when opcode = COP1
         and rs = S
    31--------26-25------21 -------------------------------5--------0
    |  = COP1   |  = S    |                               | function|
    ------6----------5-----------------------------------------6-----
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
000 | ADD.S | SUB.S | MUL.S | DIV.S | SQRT.S| ABS.S | MOV.S | NEG.S |
001 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | ---   |
010 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |RSQRT.S|  ---  |
011 | ADDA.S| SUBA.S| MULA.S|  ---  | MADD.S| MSUB.S|MADDA.S|MSUBA.S|
100 |  ---  | ---   |  ---  |  ---  | CVT.W |  ---  |  ---  |  ---  |
101 | MAX.S | MIN.S |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
110 | C.F   | ---   | C.EQ  |  ---  | C.LT  |  ---  |  C.LE |  ---  |
111 | ---   | ---   |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------| 
*/
void (*COP1SPrintTable[64])( string& output ) = {
P_ADD_S,       P_SUB_S,       P_MUL_S,       P_DIV_S,       P_SQRT_S,      P_ABS_S,       P_MOV_S,       P_NEG_S, 
P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,   
P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_RSQRT_S,     P_COP1_Unknown,  
P_ADDA_S,      P_SUBA_S,      P_MULA_S,      P_COP1_Unknown,P_MADD_S,      P_MSUB_S,      P_MADDA_S,     P_MSUBA_S,
P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_CVT_W,       P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown, 
P_MAX_S,       P_MIN_S,       P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown, 
P_C_F,         P_COP1_Unknown,P_C_EQ,        P_COP1_Unknown,P_C_LT,        P_COP1_Unknown,P_C_LE,        P_COP1_Unknown, 
P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown, 
};
/*
    FPU: Instructions encoded by the function field when opcode = COP1
         and rs = W
    31--------26-25------21 -------------------------------5--------0
    |  = COP1   |  = W    |                               | function|
    ------6----------5-----------------------------------------6-----
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
000 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
001 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
010 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
011 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
100 | CVT.S |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
101 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
110 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
111 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|       
*/  
void (*COP1WPrintTable[64])( string& output ) = { 
P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,   	
P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,   
P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,   
P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,   
P_CVT_S,       P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,   
P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,   
P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,   
P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,P_COP1_Unknown,   
};

//*************************************************************
//COP2 TABLES :) 
//*************************************************************
/* 
   COP2: Instructions encoded by the fmt field when opcode = COP2.
    31--------26-25------21 ----------------------------------------0
    |  = COP2   |   fmt   |                                         |
    ------6----------5-----------------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
 00 |  ---  | QMFC2 | CFC2  |  ---  |  ---  | QMTC2 | CTC2  |  ---  |
 01 | *1    |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 10 | *2    | *2    | *2    | *2    | *2    | *2    | *2    | *2    |
 11 | *2    | *2    | *2    | *2    | *2    | *2    | *2    | *2    |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|
     *1 = BC instructions, see BC2 list   *2 =see special1 table
*/
void (*COP2PrintTable[32])( string& output ) = {
    P_COP2_Unknown, P_QMFC2,        P_CFC2,         P_COP2_Unknown, P_COP2_Unknown, P_QMTC2,        P_CTC2,         P_COP2_Unknown,
    P_COP2_BC2,     P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown,
    P_COP2_SPECIAL, P_COP2_SPECIAL, P_COP2_SPECIAL, P_COP2_SPECIAL, P_COP2_SPECIAL, P_COP2_SPECIAL, P_COP2_SPECIAL, P_COP2_SPECIAL,
	P_COP2_SPECIAL, P_COP2_SPECIAL, P_COP2_SPECIAL, P_COP2_SPECIAL, P_COP2_SPECIAL, P_COP2_SPECIAL, P_COP2_SPECIAL, P_COP2_SPECIAL,

    
};
/*
    BC2: Instructions encoded by the rt field when opcode = COP2 & rs field=BC1
    31--------26-25------21 ----------------------------------------0
    |  = COP2   |   rs=BC2|                                         |
    ------6----------5-----------------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
 00 | BC2F  | BC2T  | BC2FL | BC2TL |  ---  |  ---  |  ---  |  ---  |
 01 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 10 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 11 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi |-------|-------|-------|-------|-------|-------|-------|-------|   
 */
void (*COP2BC2PrintTable[32])( string& output ) = {
    P_BC2F,         P_BC2T,         P_BC2FL,        P_BC2TL,        P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown,
    P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown,
    P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown,
    P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown, P_COP2_Unknown,
}; 
/*
    Special1 table : instructions encode by function field when opcode=COP2 & rs field=Special1
    31---------26---------------------------------------------------0
    |  =COP2   | rs=Special                                         |
    ------6----------------------------------------------------------
    |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
000 |VADDx  |VADDy  |VADDz  |VADDw  |VSUBx  |VSUBy  |VSUBz  |VSUBw  |
001 |VMADDx |VMADDy |VMADDz |VMADDw |VMSUBx |VMSUBy |VMSUBz |VMSUBw |
010 |VMAXx  |VMAXy  |VMAXz  |VMAXw  |VMINIx |VMINIy |VMINIz |VMINIw |
011 |VMULx  |VMULy  |VMULz  |VMULw  |VMULq  |VMAXi  |VMULi  |VMINIi |
100 |VADDq  |VMADDq |VADDi  |VMADDi |VSUBq  |VMSUBq |VSUbi  |VMSUBi |
101 |VADD   |VMADD  |VMUL   |VMAX   |VSUB   |VMSUB  |VOPMSUB|VMINI  |
110 |VIADD  |VISUB  |VIADDI |  ---  |VIAND  |VIOR   |  ---  |  ---  |
111 |VCALLMS|CALLMSR|  ---  |  ---  |  *1   |  *1   |  *1   |  *1   |
 hi |-------|-------|-------|-------|-------|-------|-------|-------| 
    *1=see special2 table  
*/
void (*COP2SPECIAL1PrintTable[64])( string& output ) = 
{ 
 P_VADDx,       P_VADDy,       P_VADDz,       P_VADDw,       P_VSUBx,        P_VSUBy,        P_VSUBz,        P_VSUBw,  
 P_VMADDx,      P_VMADDy,      P_VMADDz,      P_VMADDw,      P_VMSUBx,       P_VMSUBy,       P_VMSUBz,       P_VMSUBw, 
 P_VMAXx,       P_VMAXy,       P_VMAXz,       P_VMAXw,       P_VMINIx,       P_VMINIy,       P_VMINIz,       P_VMINIw, 
 P_VMULx,       P_VMULy,       P_VMULz,       P_VMULw,       P_VMULq,        P_VMAXi,        P_VMULi,        P_VMINIi,
 P_VADDq,       P_VMADDq,      P_VADDi,       P_VMADDi,      P_VSUBq,        P_VMSUBq,       P_VSUbi,        P_VMSUBi, 
 P_VADD,        P_VMADD,       P_VMUL,        P_VMAX,        P_VSUB,         P_VMSUB,        P_VOPMSUB,      P_VMINI,  
 P_VIADD,       P_VISUB,       P_VIADDI,      P_COP2_Unknown,P_VIAND,        P_VIOR,         P_COP2_Unknown, P_COP2_Unknown,
 P_VCALLMS,     P_CALLMSR,     P_COP2_Unknown,P_COP2_Unknown,P_COP2_SPECIAL2,P_COP2_SPECIAL2,P_COP2_SPECIAL2,P_COP2_SPECIAL2,  

};
/*
  Special2 table : instructions encode by function field when opcode=COp2 & rs field=Special2

     31---------26---------------------------------------------------0
     |  =COP2   | rs=Special2                                        |
     ------6----------------------------------------------------------
     |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
0000 |VADDAx |VADDAy |VADDAz |VADDAw |VSUBAx |VSUBAy |VSUBAz |VSUBAw |
0001 |VMADDAx|VMADDAy|VMADDAz|VMADDAw|VMSUBAx|VMSUBAy|VMSUBAz|VMSUBAw|
0010 |VITOF0 |VITOF4 |VITOF12|VITOF15|VFTOI0 |VFTOI4 |VFTOI12|VFTOI15|
0011 |VMULAx |VMULAy |VMULAz |VMULAw |VMULAq |VABS   |VMULAi |VCLIPw |
0100 |VADDAq |VMADDAq|VADDAi |VMADDAi|VSUBAq |VMSUBAq|VSUBAi |VMSUBAi|
0101 |VADDA  |VMADDA |VMULA  |  ---  |VSUBA  |VMSUBA |VOPMULA|VNOP   |
0110 |VMONE  |VMR32  |  ---  |  ---  |VLQI   |VSQI   |VLQD   |VSQD   |
0111 |VDIV   |VSQRT  |VRSQRT |VWAITQ |VMTIR  |VMFIR  |VILWR  |VISWR  |
1000 |VRNEXT |VRGET  |VRINIT |VRXOR  |  ---  |  ---  |  ---  |  ---  |
1001 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
1010 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
1011 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
1100 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
1101 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | 
1110 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
1111 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |
 hi  |-------|-------|-------|-------|-------|-------|-------|-------| 
*/
void (*COP2SPECIAL2PrintTable[128])( string& output ) = 
{
 P_VADDAx      ,P_VADDAy      ,P_VADDAz      ,P_VADDAw      ,P_VSUBAx      ,P_VSUBAy      ,P_VSUBAz      ,P_VSUBAw,
 P_VMADDAx     ,P_VMADDAy     ,P_VMADDAz     ,P_VMADDAw     ,P_VMSUBAx     ,P_VMSUBAy     ,P_VMSUBAz     ,P_VMSUBAw,
 P_VITOF0      ,P_VITOF4      ,P_VITOF12     ,P_VITOF15     ,P_VFTOI0      ,P_VFTOI4      ,P_VFTOI12     ,P_VFTOI15,
 P_VMULAx      ,P_VMULAy      ,P_VMULAz      ,P_VMULAw      ,P_VMULAq      ,P_VABS        ,P_VMULAi      ,P_VCLIPw,
 P_VADDAq      ,P_VMADDAq     ,P_VADDAi      ,P_VMADDAi     ,P_VSUBAq      ,P_VMSUBAq     ,P_VSUBAi      ,P_VMSUBAi,
 P_VADDA       ,P_VMADDA      ,P_VMULA       ,P_COP2_Unknown,P_VSUBA       ,P_VMSUBA      ,P_VOPMULA     ,P_VNOP,   
 P_VMONE       ,P_VMR32       ,P_COP2_Unknown,P_COP2_Unknown,P_VLQI        ,P_VSQI        ,P_VLQD        ,P_VSQD,   
 P_VDIV        ,P_VSQRT       ,P_VRSQRT      ,P_VWAITQ      ,P_VMTIR       ,P_VMFIR       ,P_VILWR       ,P_VISWR,  
 P_VRNEXT      ,P_VRGET       ,P_VRINIT      ,P_VRXOR       ,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown, 
 P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,
 P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,
 P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,
 P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,
 P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,
 P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,
 P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,P_COP2_Unknown,
};

//**************************TABLES CALLS***********************


void disR5900Fasm(string& output, u32 code, u32 pc)
{
	string dbuf;
	char obuf[48];

	const u32 scode = cpuRegs.code;
	opcode_addr = pc;
	cpuRegs.code = code;

	sprintf(obuf, "%08X:\t", pc );
	output.assign( obuf );
	EE::OpcodeTables::Standard[(code) >> 26].decode( output );

	cpuRegs.code = scode;
}

//***********COP0 TABLE CALLS********************************

void P_COP0_BC0( string& output )
{
	COP0BC0PrintTable[DECODE_C0BC]( output );
}
void P_COP0_Func( string& output )
{
	COP0C0PrintTable[DECODE_FUNCTION]( output );
}

//****************END OF MMI TABLES CALLS**********************
//COP1 TABLECALLS*******************************************
void P_COP1_BC1( string& output )
{
	COP1BC1PrintTable[DECODE_C1BC]( output );
}
void P_COP1_S( string& output )
{
	COP1SPrintTable[DECODE_FUNCTION]( output );
}
void P_COP1_W( string& output )
{
	COP1WPrintTable[DECODE_FUNCTION]( output );
}
//**********************END OF COP1 TABLE CALLS

//*************************************************************
//************************COP2**********************************
void P_COP2_BC2( string& output )
{
	COP2BC2PrintTable[DECODE_C2BC]( output );
}
void P_COP2_SPECIAL( string& output )
{
	COP2SPECIAL1PrintTable[DECODE_FUNCTION ]( output );
}
void P_COP2_SPECIAL2( string& output )
{
	COP2SPECIAL2PrintTable[(cpuRegs.code & 0x3) | ((cpuRegs.code >> 4) & 0x7c)]( output );
}

//**************************UNKNOWN****************************
void P_COP0_Unknown( string& output )
{
	output.append( "COP0 ??" );
}
void P_COP1_Unknown( string& output )
{
	output.append( "COP1 ??" );
}
void P_COP2_Unknown( string& output )
{
	output.append( "COP2 ??" );
}



//*************************************************************

//*****************SOME DECODE STUFF***************************
void jump_decode( string& output )
{
    char buf[256];
    u32 addr;
    addr = DECODE_JUMP;
    sprintf(buf, "0x%08X", addr);
	dFindSym( output, addr );
	output.append( buf );
}

void offset_decode( string& output )
{
    char buf[256];
    u32 addr;
    addr = DECODE_OFFSET;
    sprintf(buf, "0x%08X", addr);
	dFindSym( output, addr );
	output.append( buf );
}

//*********************END OF DECODE ROUTINES******************

void strAppend( string& output, const char *fmt, ... )
{
	va_list list;
	char tmp[512];

	va_start(list, fmt);
	vsprintf(tmp, fmt, list);
	output.append( tmp );
	va_end(list);
}


namespace EE { namespace Debug { namespace OpcodePrint
{

// ********** VariousCOP TABLE CALLS **********
void COP0( string& output )
{
	COP0PrintTable[DECODE_RS]( output );
}
void COP1( string& output )
{
	COP1PrintTable[DECODE_RS]( output );
}
void COP2( string& output )
{
	COP2PrintTable[DECODE_RS]( output );
}

// Unkown Opcode!
void Unknown( string& output )
{
	output.append( "?????" );
}

void MMI_Unknown( string& output )
{
	output.append( "MMI ??" );
}

//********************* Standard Opcodes***********************
void J( string& output )      { output.append("j\t" );		jump_decode(output);}
void JAL( string& output )    { output.append("jal\t" );	jump_decode(output);} 
void BEQ( string& output )    { strAppend(output, "beq\t%s, %s, ",          GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); offset_decode(output); }
void BNE( string& output )    { strAppend(output, "bne\t%s, %s, ",          GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); offset_decode(output); }
void BLEZ( string& output )   { strAppend(output, "blez\t%s, ",             GPR_REG[DECODE_RS]); offset_decode(output); }
void BGTZ( string& output )   { strAppend(output, "bgtz\t%s, ",             GPR_REG[DECODE_RS]); offset_decode(output); }
void ADDI( string& output )   { strAppend(output, "addi\t%s, %s, 0x%04X",   GPR_REG[DECODE_RT], GPR_REG[DECODE_RS], DECODE_IMMED);}
void ADDIU( string& output )  { strAppend(output, "addiu\t%s, %s, 0x%04X",  GPR_REG[DECODE_RT], GPR_REG[DECODE_RS], DECODE_IMMED);}
void SLTI( string& output )   { strAppend(output, "slti\t%s, %s, 0x%04X",   GPR_REG[DECODE_RT], GPR_REG[DECODE_RS], DECODE_IMMED); }
void SLTIU( string& output )  { strAppend(output, "sltiu\t%s, %s, 0x%04X",  GPR_REG[DECODE_RT], GPR_REG[DECODE_RS], DECODE_IMMED); }
void ANDI( string& output )   { strAppend(output, "andi\t%s, %s, 0x%04X",   GPR_REG[DECODE_RT], GPR_REG[DECODE_RS], DECODE_IMMED);}
void ORI( string& output )    { strAppend(output, "ori\t%s, %s, 0x%04X",    GPR_REG[DECODE_RT], GPR_REG[DECODE_RS], DECODE_IMMED); }
void XORI( string& output )   { strAppend(output, "xori\t%s, %s, 0x%04X",   GPR_REG[DECODE_RT], GPR_REG[DECODE_RS], DECODE_IMMED); }
void LUI( string& output )    { strAppend(output, "lui\t%s, 0x%04X",        GPR_REG[DECODE_RT], DECODE_IMMED); }
void BEQL( string& output )   { strAppend(output, "beql\t%s, %s, %s",       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); offset_decode(output); }
void BNEL( string& output )   { strAppend(output, "bnel\t%s, %s, %s",       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); offset_decode(output); }
void BLEZL( string& output )  { strAppend(output, "blezl\t%s, ",            GPR_REG[DECODE_RS]); offset_decode(output); }
void BGTZL( string& output )  { strAppend(output, "bgtzl\t%s, ",            GPR_REG[DECODE_RS]); offset_decode(output); }
void DADDI( string& output )  { strAppend(output, "daddi\t%s, %s, 0x%04X",  GPR_REG[DECODE_RT], GPR_REG[DECODE_RS], DECODE_IMMED); }
void DADDIU( string& output ) { strAppend(output, "daddiu\t%s, %s, 0x%04X", GPR_REG[DECODE_RT], GPR_REG[DECODE_RS], DECODE_IMMED); }
void LDL( string& output )    { strAppend(output, "ldl\t%s, 0x%04X(%s)",    GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void LDR( string& output )    { strAppend(output, "ldr\t%s, 0x%04X(%s)",    GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void LB( string& output )     { strAppend(output, "lb\t%s, 0x%04X(%s)",     GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void LH( string& output )     { strAppend(output, "lh\t%s, 0x%04X(%s)",     GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void LWL( string& output )    { strAppend(output, "lwl\t%s, 0x%04X(%s)",    GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void LW( string& output )     { strAppend(output, "lw\t%s, 0x%04X(%s)",     GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void LBU( string& output )    { strAppend(output, "lbu\t%s, 0x%04X(%s)",    GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void LHU( string& output )    { strAppend(output, "lhu\t%s, 0x%04X(%s)",    GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void LWR( string& output )    { strAppend(output, "lwr\t%s, 0x%04X(%s)",    GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void LWU( string& output )    { strAppend(output, "lwu\t%s, 0x%04X(%s)",    GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void SB( string& output )     { strAppend(output, "sb\t%s, 0x%04X(%s)",     GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void SH( string& output )     { strAppend(output, "sh\t%s, 0x%04X(%s)",     GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void SWL( string& output )    { strAppend(output, "swl\t%s, 0x%04X(%s)",    GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void SW( string& output )     { strAppend(output, "sw\t%s, 0x%04X(%s)",     GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void SDL( string& output )    { strAppend(output, "sdl\t%s, 0x%04X(%s)",    GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void SDR( string& output )    { strAppend(output, "sdr\t%s, 0x%04X(%s)",    GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void SWR( string& output )    { strAppend(output, "swr\t%s, 0x%04X(%s)",    GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void LD( string& output )     { strAppend(output, "ld\t%s, 0x%04X(%s)",     GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void SD( string& output )     { strAppend(output, "sd\t%s, 0x%04X(%s)",     GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void LQ( string& output )     { strAppend(output, "lq\t%s, 0x%04X(%s)",     GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void SQ( string& output )     { strAppend(output, "sq\t%s, 0x%04X(%s)",     GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void SWC1( string& output )   { strAppend(output, "swc1\t%s, 0x%04X(%s)",   COP1_REG_FP[DECODE_FT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void SQC2( string& output )   { strAppend(output, "sqc2\t%s, 0x%04X(%s)",   COP2_REG_FP[DECODE_FT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void PREF( string& output )   { output.append( "pref ---");/*strAppend(output, "PREF\t%s, 0x%04X(%s)",   GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[RS]); */}
void LWC1( string& output )   { strAppend(output, "lwc1\t%s, 0x%04X(%s)",   COP1_REG_FP[DECODE_FT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
void LQC2( string& output )   { strAppend(output, "lqc2\t%s, 0x%04X(%s)",   COP2_REG_FP[DECODE_FT], DECODE_IMMED, GPR_REG[DECODE_RS]); }
//********************END OF STANDARD OPCODES*************************

void SLL( string& output )
{
   if (cpuRegs.code == 0x00000000)
        output.append( "nop");
    else
        strAppend(output, "sll\t%s, %s, 0x%02X", GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA);
}

void SRL( string& output )    { strAppend(output, "srl\t%s, %s, 0x%02X", GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA); }
void SRA( string& output )    { strAppend(output, "sra\t%s, %s, 0x%02X", GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA); }
void SLLV( string& output )   { strAppend(output, "sllv\t%s, %s, %s",    GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], GPR_REG[DECODE_RS]); }
void SRLV( string& output )   { strAppend(output, "srlv\t%s, %s, %s",    GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], GPR_REG[DECODE_RS]);}
void SRAV( string& output )   { strAppend(output, "srav\t%s, %s, %s",    GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], GPR_REG[DECODE_RS]); }
void JR( string& output )     { strAppend(output, "jr\t%s",              GPR_REG[DECODE_RS]); }

void JALR( string& output )
{
    int rd = DECODE_RD;

    if (rd == 31)
        strAppend(output, "jalr\t%s", GPR_REG[DECODE_RS]);
    else
        strAppend(output, "jalr\t%s, %s", GPR_REG[rd], GPR_REG[DECODE_RS]);
}


void SYNC( string& output )    { strAppend(output,  "SYNC");}
void MFHI( string& output )    { strAppend(output, "mfhi\t%s",          GPR_REG[DECODE_RD]); }
void MTHI( string& output )    { strAppend(output, "mthi\t%s",          GPR_REG[DECODE_RS]); }
void MFLO( string& output )    { strAppend(output, "mflo\t%s",          GPR_REG[DECODE_RD]); }
void MTLO( string& output )    { strAppend(output, "mtlo\t%s",          GPR_REG[DECODE_RS]); }
void DSLLV( string& output )   { strAppend(output, "dsllv\t%s, %s, %s", GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], GPR_REG[DECODE_RS]); }
void DSRLV( string& output )   { strAppend(output, "dsrlv\t%s, %s, %s", GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], GPR_REG[DECODE_RS]); }
void DSRAV( string& output )   { strAppend(output, "dsrav\t%s, %s, %s", GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], GPR_REG[DECODE_RS]); }
void MULT( string& output )    { strAppend(output, "mult\t%s, %s, %s", GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]);}	
void MULTU( string& output )   { strAppend(output, "multu\t%s, %s, %s", GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]);}
void DIV( string& output )     { strAppend(output, "div\t%s, %s",       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void DIVU( string& output )    { strAppend(output, "divu\t%s, %s",      GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void ADD( string& output )     { strAppend(output, "add\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void ADDU( string& output )    { strAppend(output, "addu\t%s, %s, %s",  GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void SUB( string& output )     { strAppend(output, "sub\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void SUBU( string& output )    { strAppend(output, "subu\t%s, %s, %s",  GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void AND( string& output )     { strAppend(output, "and\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void OR( string& output )      { strAppend(output, "or\t%s, %s, %s",    GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void XOR( string& output )     { strAppend(output, "xor\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void NOR( string& output )     { strAppend(output, "nor\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void SLT( string& output )     { strAppend(output, "slt\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void SLTU( string& output )    { strAppend(output, "sltu\t%s, %s, %s",  GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void DADD( string& output )    { strAppend(output, "dadd\t%s, %s, %s",  GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void DADDU( string& output )   { strAppend(output, "daddu\t%s, %s, %s", GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void DSUB( string& output )    { strAppend(output, "dsub\t%s, %s, %s",  GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void DSUBU( string& output )   { strAppend(output, "dsubu\t%s, %s, %s", GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void TGE( string& output )     { strAppend(output, "tge\t%s, %s",       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void TGEU( string& output )    { strAppend(output, "tgeu\t%s, %s",      GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void TLT( string& output )     { strAppend(output, "tlt\t%s, %s",       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void TLTU( string& output )    { strAppend(output, "tltu\t%s, %s",      GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void TEQ( string& output )     { strAppend(output, "teq\t%s, %s",       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void TNE( string& output )     { strAppend(output, "tne\t%s, %s",       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void DSLL( string& output )    { strAppend(output, "dsll\t%s, %s, 0x%02X",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA); }
void DSRL( string& output )    { strAppend(output, "dsrl\t%s, %s, 0x%02X",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA); }
void DSRA( string& output )    { strAppend(output, "dsra\t%s, %s, 0x%02X",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA); }
void DSLL32( string& output )  { strAppend(output, "dsll32\t%s, %s, 0x%02X", GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA); }
void DSRL32( string& output )  { strAppend(output, "dsrl32\t%s, %s, 0x%02X", GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA); }
void DSRA32( string& output )  { strAppend(output, "dsra32\t%s, %s, 0x%02X", GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA); }
void MOVZ( string& output )    { strAppend(output, "movz\t%s, %s, %s", GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void MOVN( string& output )    { strAppend(output, "movn\t%s, %s, %s", GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void MFSA( string& output )    { strAppend(output, "mfsa\t%s",          GPR_REG[DECODE_RD]);} 
void MTSA( string& output )    { strAppend(output, "mtsa\t%s",          GPR_REG[DECODE_RS]);}
//*** unsupport (yet) cpu opcodes
void SYSCALL( string& output ) { output.append( "syscall ---");/*strAppend(output, "syscall\t0x%05X",   DECODE_SYSCALL);*/}
void BREAK( string& output )   { output.append( "break   ---");/*strAppend(output, "break\t0x%05X",     DECODE_BREAK); */}
void CACHE( string& output )   { output.append( "cache   ---");/*strAppend(output, "cache\t%s, 0x%04X(%s)",  GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); */}
//************************REGIMM OPCODES***************************
void BLTZ( string& output )    { strAppend(output, "bltz\t%s, ",       GPR_REG[DECODE_RS]); offset_decode(output); }
void BGEZ( string& output )    { strAppend(output, "bgez\t%s, ",       GPR_REG[DECODE_RS]); offset_decode(output); }
void BLTZL( string& output )   { strAppend(output, "bltzl\t%s, ",      GPR_REG[DECODE_RS]); offset_decode(output); }
void BGEZL( string& output )   { strAppend(output, "bgezl\t%s, ",      GPR_REG[DECODE_RS]); offset_decode(output); }
void TGEI( string& output )    { strAppend(output, "tgei\t%s, 0x%04X", GPR_REG[DECODE_RS], DECODE_IMMED); }
void TGEIU( string& output )   { strAppend(output, "tgeiu\t%s,0x%04X", GPR_REG[DECODE_RS], DECODE_IMMED); }
void TLTI( string& output )    { strAppend(output, "tlti\t%s, 0x%04X", GPR_REG[DECODE_RS], DECODE_IMMED); }
void TLTIU( string& output )   { strAppend(output, "tltiu\t%s,0x%04X", GPR_REG[DECODE_RS], DECODE_IMMED); }
void TEQI( string& output )    { strAppend(output, "teqi\t%s, 0x%04X", GPR_REG[DECODE_RS], DECODE_IMMED); }
void TNEI( string& output )    { strAppend(output, "tnei\t%s, 0x%04X", GPR_REG[DECODE_RS], DECODE_IMMED); }
void BLTZAL( string& output )  { strAppend(output, "bltzal\t%s, ",     GPR_REG[DECODE_RS]); offset_decode(output); }
void BGEZAL( string& output )  { strAppend(output, "bgezal\t%s, ",     GPR_REG[DECODE_RS]); offset_decode(output); }
void BLTZALL( string& output ) { strAppend(output, "bltzall\t%s, ",    GPR_REG[DECODE_RS]); offset_decode(output); }
void BGEZALL( string& output ) { strAppend(output, "bgezall\t%s, ",    GPR_REG[DECODE_RS]); offset_decode(output); }
void MTSAB( string& output )   { strAppend(output, "mtsab\t%s, 0x%04X", GPR_REG[DECODE_RS], DECODE_IMMED);}
void MTSAH( string& output )   { strAppend(output, "mtsah\t%s, 0x%04X", GPR_REG[DECODE_RS], DECODE_IMMED);}


//***************************SPECIAL 2 CPU OPCODES*******************
const char* pmfhl_sub[] = { "lw", "uw", "slw", "lh", "sh" };

void MADD( string& output )    { strAppend(output, "madd\t%s, %s %s",        GPR_REG[DECODE_RD],GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void MADDU( string& output )   { strAppend(output, "maddu\t%s, %s %s",       GPR_REG[DECODE_RD],GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]);}
void PLZCW( string& output )   { strAppend(output, "plzcw\t%s, %s",          GPR_REG[DECODE_RD], GPR_REG[DECODE_RS]); }
void MADD1( string& output )   { strAppend(output, "madd1\t%s, %s %s",       GPR_REG[DECODE_RD],GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void MADDU1( string& output )  { strAppend(output, "maddu1\t%s, %s %s",      GPR_REG[DECODE_RD],GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void MFHI1( string& output )   { strAppend(output, "mfhi1\t%s",          GPR_REG[DECODE_RD]); }
void MTHI1( string& output )   { strAppend(output, "mthi1\t%s",          GPR_REG[DECODE_RS]); }
void MFLO1( string& output )   { strAppend(output, "mflo1\t%s",          GPR_REG[DECODE_RD]); }
void MTLO1( string& output )   { strAppend(output, "mtlo1\t%s",          GPR_REG[DECODE_RS]); }
void MULT1( string& output )   { strAppend(output, "mult1\t%s, %s, %s",        GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void MULTU1( string& output )  { strAppend(output, "multu1\t%s, %s, %s",        GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]);}
void DIV1( string& output )    { strAppend(output, "div1\t%s, %s",       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void DIVU1( string& output )   { strAppend(output, "divu1\t%s, %s",       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
//that have parametres that i haven't figure out how to display...
void PMFHL( string& output )   { strAppend(output, "pmfhl.%s \t%s",          pmfhl_sub[DECODE_SA], GPR_REG[DECODE_RD]); }
void PMTHL( string& output )   { strAppend(output, "pmthl.%s \t%s",          pmfhl_sub[DECODE_SA], GPR_REG[DECODE_RS]); }
void PSLLH( string& output )   { strAppend(output, "psllh   \t%s, %s, 0x%02X",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA); }
void PSRLH( string& output )   { strAppend(output, "psrlh   \t%s, %s, 0x%02X",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA);}
void PSRAH( string& output )   { strAppend(output, "psrah   \t%s, %s, 0x%02X",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA);}
void PSLLW( string& output )   { strAppend(output,  "psllw   \t%s, %s, 0x%02X",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA);}
void PSRLW( string& output )   { strAppend(output,  "psrlw   \t%s, %s, 0x%02X",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA);}
void PSRAW( string& output )   { strAppend(output,  "psraw   \t%s, %s, 0x%02X",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA);}
//***************************END OF SPECIAL OPCODES******************
//*************************MMI0 OPCODES************************

void PADDW( string& output ){  strAppend(output,  "paddw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }  
void PSUBW( string& output ){  strAppend(output,  "psubw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }  
void PCGTW( string& output ){  strAppend(output,  "pcgtw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }  
void PMAXW( string& output ){  strAppend(output,  "pmaxw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PADDH( string& output ){  strAppend(output,  "paddh\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }  
void PSUBH( string& output ){  strAppend(output,  "psubh\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }  
void PCGTH( string& output ){  strAppend(output,  "pcgth\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }  
void PMAXH( string& output ){  strAppend(output,  "pmaxh\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PADDB( string& output ){  strAppend(output,  "paddb\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }  
void PSUBB( string& output ){  strAppend(output,  "psubb\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }  
void PCGTB( string& output ){  strAppend(output,  "pcgtb\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PADDSW( string& output ){ strAppend(output,  "paddsw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PSUBSW( string& output ){ strAppend(output,  "psubsw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PEXTLW( string& output ){ strAppend(output,  "pextlw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }  
void PPACW( string& output ) { strAppend(output,  "ppacw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PADDSH( string& output ){ strAppend(output,  "paddsh\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PSUBSH( string& output ){ strAppend(output,  "psubsh\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PEXTLH( string& output ){ strAppend(output,  "pextlh\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }  
void PPACH( string& output ) { strAppend(output,  "ppach\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PADDSB( string& output ){ strAppend(output,  "paddsb\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PSUBSB( string& output ){ strAppend(output,  "psubsb\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PEXTLB( string& output ){ strAppend(output,  "pextlb\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }  
void PPACB( string& output ) { strAppend(output,  "ppacb\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PEXT5( string& output ) { strAppend(output,  "pext5\t%s, %s",      GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]); }  
void PPAC5( string& output ) { strAppend(output,  "ppac5\t%s, %s",      GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]); } 
//**********END OF MMI0 OPCODES*********************************
//**********MMI1 OPCODES**************************************
void PABSW( string& output ){  strAppend(output,  "pabsw%s, %s",      GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]); }
void PCEQW( string& output ){  strAppend(output,  "pceqw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PMINW( string& output ){  strAppend(output,  "pminw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PADSBH( string& output ){ strAppend(output,  "padsbh\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PABSH( string& output ){  strAppend(output,  "pabsh%s, %s",      GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]); }
void PCEQH( string& output ){  strAppend(output,  "pceqh\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PMINH( string& output ){  strAppend(output,  "pminh\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }  
void PCEQB( string& output ){  strAppend(output,  "pceqb\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PADDUW( string& output ){ strAppend(output,  "padduw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PSUBUW( string& output ){ strAppend(output,  "psubuw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PEXTUW( string& output ){ strAppend(output,  "pextuw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }  
void PADDUH( string& output ){ strAppend(output,  "padduh\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PSUBUH( string& output ){ strAppend(output,  "psubuh\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PEXTUH( string& output ){ strAppend(output,  "pextuh\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PADDUB( string& output ){ strAppend(output,  "paddub\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PSUBUB( string& output ){ strAppend(output,  "psubub\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PEXTUB( string& output ){ strAppend(output,  "pextub\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void QFSRV( string& output ) { strAppend(output,  "qfsrv\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
//********END OF MMI1 OPCODES***********************************
//*********MMI2 OPCODES***************************************
void PMADDW( string& output ){ strAppend(output,  "pmaddw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PSLLVW( string& output ){ strAppend(output,  "psllvw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PSRLVW( string& output ){ strAppend(output,  "psrlvw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PMSUBW( string& output ){ strAppend(output,  "msubw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PMFHI( string& output ){  strAppend(output,  "pmfhi\t%s",          GPR_REG[DECODE_RD]); }
void PMFLO( string& output ){  strAppend(output,  "pmflo\t%s",          GPR_REG[DECODE_RD]); } 
void PINTH( string& output ){  strAppend(output,  "pinth\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PMULTW( string& output ){ strAppend(output,  "pmultw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PDIVW( string& output ){  strAppend(output,  "pdivw\t%s, %s",      GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PCPYLD( string& output ){ strAppend(output,  "pcpyld\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PMADDH( string& output ){ strAppend(output,  "pmaddh\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PHMADH( string& output ){ strAppend(output,  "phmadh\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PAND( string& output ){   strAppend(output,  "pand\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PXOR( string& output ){   strAppend(output,  "pxor\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PMSUBH( string& output ){ strAppend(output,  "pmsubh\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PHMSBH( string& output ){ strAppend(output,  "phmsbh\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PEXEH( string& output ){  strAppend(output,  "pexeh\t%s, %s",      GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]); } 
void PREVH( string& output ){  strAppend(output,  "prevh\t%s, %s",      GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]); }  
void PMULTH( string& output ){ strAppend(output,  "pmulth\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PDIVBW( string& output ){ strAppend(output,  "pdivbw\t%s, %s",      GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PEXEW( string& output ){  strAppend(output,  "pexew\t%s, %s",      GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]); } 
void PROT3W( string& output ){ strAppend(output,  "prot3w\t%s, %s",      GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]); } 
//*****END OF MMI2 OPCODES***********************************
//*************************MMI3 OPCODES************************
void PMADDUW( string& output ){ strAppend(output, "pmadduw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], GPR_REG[DECODE_RS]); }
void PSRAVW( string& output ){  strAppend(output, "psravw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], GPR_REG[DECODE_RS]); }  
void PMTHI( string& output ){   strAppend(output, "pmthi\t%s",          GPR_REG[DECODE_RS]); }
void PMTLO( string& output ){   strAppend(output, "pmtlo\t%s",          GPR_REG[DECODE_RS]); }
void PINTEH( string& output ){  strAppend(output, "pinteh\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PMULTUW( string& output ){ strAppend(output, "pmultuw\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void PDIVUW( string& output ){  strAppend(output, "pdivuw\t%s, %s",       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PCPYUD( string& output ){  strAppend(output, "pcpyud\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); } 
void POR( string& output ){     strAppend(output, "por\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PNOR( string& output ){    strAppend(output, "pnor\t%s, %s, %s",   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }  
void PEXCH( string& output ){   strAppend(output, "pexch\t%s, %s",       GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]);}
void PCPYH( string& output ){   strAppend(output, "pcpyh\t%s, %s",       GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]);} 
void PEXCW( string& output ){   strAppend(output, "pexcw\t%s, %s",       GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]);}
//**********************END OF MMI3 OPCODES******************** 

} } }	// End namespace EE::Debug::OpcodePrint

//****************************************************************************
//** COP0                                                                   **
//****************************************************************************
void P_MFC0( string& output ){  strAppend(output, "mfc0\t%s, %s",  GPR_REG[DECODE_RT], COP0_REG[DECODE_FS]); }
void P_MTC0( string& output ){  strAppend(output, "mtc0\t%s, %s",  GPR_REG[DECODE_RT], COP0_REG[DECODE_FS]); }
void P_BC0F( string& output ){  output.append("bc0f\t");       offset_decode(output); }
void P_BC0T( string& output ){  output.append("bc0t\t");       offset_decode(output); }
void P_BC0FL( string& output ){ output.append("bc0fl\t");      offset_decode(output); }
void P_BC0TL( string& output ){ output.append("bc0tl\t");      offset_decode(output); }
void P_TLBR( string& output ){  output.append("tlbr");}
void P_TLBWI( string& output ){ output.append("tlbwi");}
void P_TLBWR( string& output ){ output.append("tlbwr");}
void P_TLBP( string& output ){  output.append("tlbp");}
void P_ERET( string& output ){  output.append("eret");}
void P_DI( string& output ){    output.append("di");}
void P_EI( string& output ){    output.append("ei");}
//****************************************************************************
//** END OF COP0                                                            **
//****************************************************************************
//****************************************************************************
//** COP1 - Floating Point Unit (FPU)                                       **
//****************************************************************************
void P_MFC1( string& output ){   strAppend(output, "mfc1\t%s, %s",      GPR_REG[DECODE_RT], COP1_REG_FP[DECODE_FS]);  }
void P_CFC1( string& output ){   strAppend(output, "cfc1\t%s, %s",      GPR_REG[DECODE_RT], COP1_REG_FCR[DECODE_FS]); }
void P_MTC1( string& output ){   strAppend(output, "mtc1\t%s, %s",      GPR_REG[DECODE_RT], COP1_REG_FP[DECODE_FS]);  }
void P_CTC1( string& output ){   strAppend(output, "ctc1\t%s, %s",      GPR_REG[DECODE_RT], COP1_REG_FCR[DECODE_FS]); }
void P_BC1F( string& output ){   output.append("bc1f\t");      offset_decode(output); }
void P_BC1T( string& output ){   output.append("bc1t\t");      offset_decode(output); }
void P_BC1FL( string& output ){  output.append("bc1fl\t");     offset_decode(output); }
void P_BC1TL( string& output ){  output.append("bc1tl\t");     offset_decode(output); }
void P_ADD_S( string& output ){  strAppend(output, "add.s\t%s, %s, %s", COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]);}  
void P_SUB_S( string& output ){  strAppend(output, "sub.s\t%s, %s, %s", COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]);}  
void P_MUL_S( string& output ){  strAppend(output, "mul.s\t%s, %s, %s", COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]);}  
void P_DIV_S( string& output ){  strAppend(output, "div.s\t%s, %s, %s", COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }  
void P_SQRT_S( string& output ){ strAppend(output, "sqrt.s\t%s, %s",   COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FT]); } 
void P_ABS_S( string& output ){  strAppend(output, "abs.s\t%s, %s",     COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS]); }  
void P_MOV_S( string& output ){  strAppend(output, "mov.s\t%s, %s",     COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS]); } 
void P_NEG_S( string& output ){  strAppend(output, "neg.s\t%s, %s",     COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS]);} 
void P_RSQRT_S( string& output ){strAppend(output, "rsqrt.s\t%s, %s, %s", COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]);}  
void P_ADDA_S( string& output ){ strAppend(output, "adda.s\t%s, %s",     COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); } 
void P_SUBA_S( string& output ){ strAppend(output, "suba.s\t%s, %s",     COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); } 
void P_MULA_S( string& output ){ strAppend(output, "mula.s\t%s, %s",   COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }
void P_MADD_S( string& output ){ strAppend(output, "madd.s\t%s, %s, %s", COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); } 
void P_MSUB_S( string& output ){ strAppend(output, "msub.s\t%s, %s, %s", COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); } 
void P_MADDA_S( string& output ){strAppend(output, "madda.s\t%s, %s",   COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); } 
void P_MSUBA_S( string& output ){strAppend(output, "msuba.s\t%s, %s",   COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }
void P_CVT_W( string& output ){  strAppend(output, "cvt.w.s\t%s, %s",   COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS]); }
void P_MAX_S( string& output ){  strAppend(output, "max.s\t%s, %s, %s", COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]);}
void P_MIN_S( string& output ){  strAppend(output, "min.s\t%s, %s, %s", COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]);}
void P_C_F( string& output ){    strAppend(output, "c.f.s\t%s, %s",     COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }
void P_C_EQ( string& output ){   strAppend(output, "c.eq.s\t%s, %s",    COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }
void P_C_LT( string& output ){   strAppend(output, "c.lt.s\t%s, %s",    COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }
void P_C_LE( string& output ){   strAppend(output, "c.le.s\t%s, %s",    COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); } 
void P_CVT_S( string& output ){  strAppend(output, "cvt.s.w\t%s, %s",   COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS]); }
//****************************************************************************
//** END OF COP1                                                            **
//****************************************************************************
//****************************************************************************
//** COP2 - (VU0)                                                           **
//****************************************************************************
void P_QMFC2( string& output ){   strAppend(output, "qmfc2\t%s, %s",      GPR_REG[DECODE_RT], COP2_REG_FP[DECODE_FS]);  } 
void P_CFC2( string& output ){    strAppend(output, "cfc2\t%s, %s",      GPR_REG[DECODE_RT], COP2_REG_CTL[DECODE_FS]); } 
void P_QMTC2( string& output ){   strAppend(output, "qmtc2\t%s, %s",      GPR_REG[DECODE_RT], COP2_REG_FP[DECODE_FS]); } 
void P_CTC2( string& output ){    strAppend(output, "ctc2\t%s, %s",      GPR_REG[DECODE_RT], COP2_REG_CTL[DECODE_FS]); }  
void P_BC2F( string& output ){    output.append("bc2f\t");      offset_decode(output); }
void P_BC2T( string& output ){    output.append("bc2t\t");      offset_decode(output); }
void P_BC2FL( string& output ){   output.append("bc2fl\t");     offset_decode(output); }
void P_BC2TL( string& output ){   output.append("bc2tl\t");     offset_decode(output); }
//******************************SPECIAL 1 VUO TABLE****************************************
#define _X ((cpuRegs.code>>24) & 1)
#define _Y ((cpuRegs.code>>23) & 1)
#define _Z ((cpuRegs.code>>22) & 1)
#define _W ((cpuRegs.code>>21) & 1)

const char *dest_string(void)
{
	static char str[5];
	int i = 0;
	
	if(_X) str[i++] = 'x';
	if(_Y) str[i++] = 'y';
	if(_Z) str[i++] = 'z';
	if(_W) str[i++] = 'w';
	str[i++] = 0;
	
	return (const char *)str;
}

char dest_fsf()
{
	const char arr[4] = { 'x', 'y', 'z', 'w' };
	return arr[(cpuRegs.code>>21)&3];
}

char dest_ftf()
{
	const char arr[4] = { 'x', 'y', 'z', 'w' };
	return arr[(cpuRegs.code>>23)&3];
}

void P_VADDx( string& output ){strAppend(output, "vaddx.%s %s, %s, %sx", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VADDy( string& output ){strAppend(output, "vaddy.%s %s, %s, %sy", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VADDz( string& output ){strAppend(output, "vaddz.%s %s, %s, %sz", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VADDw( string& output ){strAppend(output, "vaddw.%s %s, %s, %sw", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VSUBx( string& output ){strAppend(output, "vsubx.%s %s, %s, %sx", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VSUBy( string& output ){strAppend(output, "vsuby.%s %s, %s, %sy", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VSUBz( string& output ){strAppend(output, "vsubz.%s %s, %s, %sz", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VSUBw( string& output ){strAppend(output, "vsubw.%s %s, %s, %sw", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMADDx( string& output ){strAppend(output, "vmaddx.%s %s, %s, %sx", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMADDy( string& output ){strAppend(output, "vmaddy.%s %s, %s, %sy", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMADDz( string& output ){strAppend(output, "vmaddz.%s %s, %s, %sz", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMADDw( string& output ){strAppend(output, "vmaddw.%s %s, %s, %sw", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMSUBx( string& output ){strAppend(output, "vmsubx.%s %s, %s, %sx", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMSUBy( string& output ){strAppend(output, "vmsuby.%s %s, %s, %sy", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMSUBz( string& output ){strAppend(output, "vmsubz.%s %s, %s, %sz", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMSUBw( string& output ){strAppend(output, "vmsubw.%s %s, %s, %sw", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMAXx( string& output ){strAppend(output, "vmaxx.%s %s, %s, %sx", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMAXy( string& output ){strAppend(output, "vmaxy.%s %s, %s, %sy", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMAXz( string& output ){strAppend(output, "vmaxz.%s %s, %s, %sz", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMAXw( string& output ){strAppend(output, "vmaxw.%s %s, %s, %sw", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMINIx( string& output ){strAppend(output, "vminix.%s %s, %s, %sx", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMINIy( string& output ){strAppend(output, "vminiy.%s %s, %s, %sy", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); ;}
void P_VMINIz( string& output ){strAppend(output, "vminiz.%s %s, %s, %sz", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMINIw( string& output ){strAppend(output, "vminiw.%s %s, %s, %sw", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMULx( string& output ){strAppend(output,"vmulx.%s %s,%s,%sx", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMULy( string& output ){strAppend(output,"vmuly.%s %s,%s,%sy", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMULz( string& output ){strAppend(output,"vmulz.%s %s,%s,%sz", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMULw( string& output ){strAppend(output,"vmulw.%s %s,%s,%sw", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMULq( string& output ){strAppend(output,"vmulq.%s %s,%s,Q",dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VMAXi( string& output ){strAppend(output,"vmaxi.%s %s,%s,I",dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VMULi( string& output ){strAppend(output,"vmuli.%s %s,%s,I",dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VMINIi( string& output ){strAppend(output,"vminii.%s %s,%s,I",dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VADDq( string& output ){strAppend(output,"vaddq.%s %s,%s,Q",dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VMADDq( string& output ){strAppend(output,"vmaddq.%s %s,%s,Q",dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VADDi( string& output ){strAppend(output,"vaddi.%s %s,%s,I",dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VMADDi( string& output ){strAppend(output,"vmaddi.%s %s,%s,I",dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VSUBq( string& output ){strAppend(output,"vsubq.%s %s,%s,Q",dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VMSUBq( string& output ){strAppend(output,"vmsubq.%s %s,%s,Q",dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VSUbi( string& output ){strAppend(output,"vsubi.%s %s,%s,I",dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VMSUBi( string& output ){strAppend(output,"vmsubi.%s %s,%s,I",dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VADD( string& output ){strAppend(output, "vadd.%s %s, %s, %s", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMADD( string& output ){strAppend(output, "vmadd.%s %s, %s, %s", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMUL( string& output ){strAppend(output, "vmul.%s %s, %s, %s", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMAX( string& output ){strAppend(output, "vmax.%s %s, %s, %s", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VSUB( string& output ){strAppend(output, "vsub.%s %s, %s, %s", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMSUB( string& output ){strAppend(output, "vmsub.%s %s, %s, %s", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VOPMSUB( string& output ){strAppend(output, "vopmsub.xyz %s, %s, %s", COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMINI( string& output ){strAppend(output, "vmini.%s %s, %s, %s", dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VIADD( string& output ){strAppend(output,"viadd %s, %s, %s", COP2_REG_CTL[DECODE_SA], COP2_REG_CTL[DECODE_FS], COP2_REG_CTL[DECODE_FT]);}
void P_VISUB( string& output ){strAppend(output,"visub %s, %s, %s", COP2_REG_CTL[DECODE_SA], COP2_REG_CTL[DECODE_FS], COP2_REG_CTL[DECODE_FT]);}
void P_VIADDI( string& output ){strAppend(output,"viaddi %s, %s, 0x%x", COP2_REG_CTL[DECODE_FT], COP2_REG_CTL[DECODE_FS], DECODE_SA);}
void P_VIAND( string& output ){strAppend(output,"viand %s, %s, %s", COP2_REG_CTL[DECODE_SA], COP2_REG_CTL[DECODE_FS], COP2_REG_CTL[DECODE_FT]);}
void P_VIOR( string& output ){strAppend(output,"vior %s, %s, %s", COP2_REG_CTL[DECODE_SA], COP2_REG_CTL[DECODE_FS], COP2_REG_CTL[DECODE_FT]);}
void P_VCALLMS( string& output ){output.append("vcallms");}
void P_CALLMSR( string& output ){output.append("callmsr");}
//***********************************END OF SPECIAL1 VU0 TABLE*****************************
//******************************SPECIAL2 VUO TABLE*****************************************
void P_VADDAx( string& output ){strAppend(output,"vaddax.%s ACC,%s,%sx",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VADDAy( string& output ){strAppend(output,"vadday.%s ACC,%s,%sy",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VADDAz( string& output ){strAppend(output,"vaddaz.%s ACC,%s,%sz",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VADDAw( string& output ){strAppend(output,"vaddaw.%s ACC,%s,%sw",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VSUBAx( string& output ){strAppend(output,"vsubax.%s ACC,%s,%sx",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VSUBAy( string& output ){strAppend(output,"vsubay.%s ACC,%s,%sy",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VSUBAz( string& output ){strAppend(output,"vsubaz.%s ACC,%s,%sz",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VSUBAw( string& output ){strAppend(output,"vsubaw.%s ACC,%s,%sw",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMADDAx( string& output ){strAppend(output,"vmaddax.%s ACC,%s,%sx",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMADDAy( string& output ){strAppend(output,"vmadday.%s ACC,%s,%sy",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMADDAz( string& output ){strAppend(output,"vmaddaz.%s ACC,%s,%sz",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMADDAw( string& output ){strAppend(output,"vmaddaw.%s ACC,%s,%sw",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMSUBAx( string& output ){strAppend(output,"vmsubax.%s ACC,%s,%sx",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMSUBAy( string& output ){strAppend(output,"vmsubay.%s ACC,%s,%sy",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMSUBAz( string& output ){strAppend(output,"vmsubaz.%s ACC,%s,%sz",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMSUBAw( string& output ){strAppend(output,"vmsubaw.%s ACC,%s,%sw",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VITOF0( string& output ){strAppend(output, "vitof0.%s %s, %s", dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]); }
void P_VITOF4( string& output ){strAppend(output, "vitof4.%s %s, %s", dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]); }
void P_VITOF12( string& output ){strAppend(output, "vitof12.%s %s, %s", dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]); }
void P_VITOF15( string& output ){strAppend(output, "vitof15.%s %s, %s", dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]); }
void P_VFTOI0( string& output ) {strAppend(output, "vftoi0.%s %s, %s", dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]); }
void P_VFTOI4( string& output ) {strAppend(output, "vftoi4.%s %s, %s", dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]); }
void P_VFTOI12( string& output ){strAppend(output, "vftoi12.%s %s, %s", dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]); }
void P_VFTOI15( string& output ){strAppend(output, "vftoi15.%s %s, %s", dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]); }
void P_VMULAx( string& output ){strAppend(output,"vmulax.%s ACC,%s,%sx",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMULAy( string& output ){strAppend(output,"vmulay.%s ACC,%s,%sy",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMULAz( string& output ){strAppend(output,"vmulaz.%s ACC,%s,%sz",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMULAw( string& output ){strAppend(output,"vmulaw.%s ACC,%s,%sw",dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMULAq( string& output ){strAppend(output,"vmulaq.%s ACC %s, Q" ,dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VABS( string& output ){strAppend(output, "vabs.%s %s, %s", dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]);}
void P_VMULAi( string& output ){strAppend(output,"vmulaq.%s ACC %s, I" ,dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VCLIPw( string& output ){strAppend(output,"vclip %sxyz, %sw", COP2_REG_FP[DECODE_FS], COP2_REG_FP[DECODE_FT]);}
void P_VADDAq( string& output ){strAppend(output,"vaddaq.%s ACC %s, Q" ,dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VMADDAq( string& output ){strAppend(output,"vmaddaq.%s ACC %s, Q" ,dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VADDAi( string& output ){strAppend(output,"vaddai.%s ACC %s, I" ,dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VMADDAi( string& output ){strAppend(output,"vmaddai.%s ACC %s, Q" ,dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VSUBAq( string& output ){strAppend(output,"vsubaq.%s ACC %s, Q" ,dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VMSUBAq( string& output ){strAppend(output,"vmsubaq.%s ACC %s, Q" ,dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VSUBAi( string& output ){strAppend(output,"vsubai.%s ACC %s, I" ,dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VMSUBAi( string& output ){strAppend(output,"vmsubai.%s ACC %s, I" ,dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VADDA( string& output ){strAppend(output,"vadda.%s ACC %s, %s" ,dest_string(), COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMADDA( string& output ){strAppend(output,"vmadda.%s ACC %s, %s" ,dest_string(), COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMULA( string& output ){strAppend(output,"vmula.%s ACC %s, %s" ,dest_string(), COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VSUBA( string& output ){strAppend(output,"vsuba.%s ACC %s, %s" ,dest_string(), COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMSUBA( string& output ){strAppend(output,"vmsuba.%s ACC %s, %s" ,dest_string(), COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VOPMULA( string& output ){strAppend(output,"vopmula.xyz %sxyz, %sxyz" ,COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VNOP( string& output ){output.append("vnop");}
void P_VMONE( string& output ){strAppend(output,"vmove.%s, %s, %s" ,dest_string(), COP2_REG_FP[DECODE_FT],COP2_REG_FP[DECODE_FS]); }
void P_VMR32( string& output ){strAppend(output,"vmr32.%s, %s, %s" ,dest_string(), COP2_REG_FP[DECODE_FT],COP2_REG_FP[DECODE_FS]); }
void P_VLQI( string& output ){strAppend(output,"vlqi %s%s, (%s++)", COP2_REG_FP[DECODE_FT], dest_string(), COP2_REG_CTL[DECODE_FS]);}
void P_VSQI( string& output ){strAppend(output,"vsqi %s%s, (%s++)", COP2_REG_FP[DECODE_FS], dest_string(), COP2_REG_CTL[DECODE_FT]);}
void P_VLQD( string& output ){strAppend(output,"vlqd %s%s, (--%s)", COP2_REG_FP[DECODE_FT], dest_string(), COP2_REG_CTL[DECODE_FS]);}
void P_VSQD( string& output ){strAppend(output,"vsqd %s%s, (--%s)", COP2_REG_FP[DECODE_FS], dest_string(), COP2_REG_CTL[DECODE_FT]);}
void P_VDIV( string& output ){strAppend(output,"vdiv Q, %s%c, %s%c", COP2_REG_FP[DECODE_FS], dest_fsf(), COP2_REG_FP[DECODE_FT], dest_ftf());}
void P_VSQRT( string& output ){strAppend(output,"vsqrt Q, %s%c", COP2_REG_FP[DECODE_FT], dest_ftf());}
void P_VRSQRT( string& output ){strAppend(output,"vrsqrt Q, %s%c, %s%c", COP2_REG_FP[DECODE_FS], dest_fsf(), COP2_REG_FP[DECODE_FT], dest_ftf());}
void P_VWAITQ( string& output ){strAppend(output,"vwaitq");}
void P_VMTIR( string& output ){strAppend(output,"vmtir %s, %s%c", COP2_REG_CTL[DECODE_FT], COP2_REG_FP[DECODE_FS], dest_fsf());}
void P_VMFIR( string& output ){strAppend(output,"vmfir %s%c, %s", COP2_REG_FP[DECODE_FT], dest_string(), COP2_REG_CTL[DECODE_FS]);}
void P_VILWR( string& output ){strAppend(output,"vilwr %s, (%s)%s", COP2_REG_CTL[DECODE_FT], COP2_REG_CTL[DECODE_FS], dest_string());}
void P_VISWR( string& output ){strAppend(output,"viswr %s, (%s)%s", COP2_REG_CTL[DECODE_FT], COP2_REG_CTL[DECODE_FS], dest_string());}
void P_VRNEXT( string& output ){strAppend(output,"vrnext %s%s, R", COP2_REG_CTL[DECODE_FT], dest_string());}
void P_VRGET( string& output ){strAppend(output,"vrget %s%s, R", COP2_REG_CTL[DECODE_FT], dest_string());}
void P_VRINIT( string& output ){strAppend(output,"vrinit R, %s%s", COP2_REG_CTL[DECODE_FS], dest_string());}
void P_VRXOR( string& output ){strAppend(output,"vrxor R, %s%s", COP2_REG_CTL[DECODE_FS], dest_string());}
//************************************END OF SPECIAL2 VUO TABLE****************************     
