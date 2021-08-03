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

#ifdef __linux__
#include <cstdarg>
#endif

#include "Debug.h"
#include "R5900.h"
#include "DisASM.h"
#include "R5900OpcodeTables.h"

// Allow to print register content when you print dissassembler info
// Note only a subset of the opcodes are supported. It is intended as a cheap debugger
//#define PRINT_REG_CONTENT

// Note: Perf is not important
static void vssappendf(std::string &dest, const char *format, va_list args)
{
    char first_try[128]; // this function is called 99% (100%?) of the times for small string
    va_list args_copy;
    va_copy(args_copy, args);

    s32 size = std::vsnprintf(first_try, 128, format, args_copy) + 1;

    va_end(args_copy);

    if (size < 0)
        return;
    if (size < 128) {
        dest += first_try;
        return;
    }

    std::vector<char> output(size + 1);
    std::vsnprintf(output.data(), size, format, args);

    dest += output.data();
}

void ssappendf(std::string &dest, const char *format, ...)
{
    va_list args;
    va_start(args, format);
    vssappendf(dest, format, args);
    va_end(args);
}


unsigned long opcode_addr;
u32 disasmOpcode;
bool disSimplify;

namespace R5900
{

/*
//DECODE PROCUDURES

//cop0
#define DECODE_FS           (DECODE_RD)
#define DECODE_FT           (DECODE_RT)
#define DECODE_FD           (DECODE_SA)
/// ********

#define DECODE_FUNCTION     ((disasmOpcode) & 0x3F)
#define DECODE_RD     ((disasmOpcode >> 11) & 0x1F) // The rd part of the instruction register
#define DECODE_RT     ((disasmOpcode >> 16) & 0x1F) // The rt part of the instruction register
#define DECODE_RS     ((disasmOpcode >> 21) & 0x1F) // The rs part of the instruction register
#define DECODE_SA     ((disasmOpcode >>  6) & 0x1F) // The sa part of the instruction register
#define DECODE_IMMED     ( disasmOpcode & 0xFFFF)      // The immediate part of the instruction register
#define DECODE_OFFSET  ((((short)DECODE_IMMED * 4) + opcode_addr + 4))
#define DECODE_JUMP     (opcode_addr & 0xf0000000)|((disasmOpcode&0x3ffffff)<<2)
#define DECODE_SYSCALL      ((opcode_addr & 0x03FFFFFF) >> 6)
#define DECODE_BREAK        (DECODE_SYSCALL)
#define DECODE_C0BC         ((disasmOpcode >> 16) & 0x03)
#define DECODE_C1BC         ((disasmOpcode >> 16) & 0x03)
#define DECODE_C2BC         ((disasmOpcode >> 16) & 0x03)
*/
/*************************CPUS REGISTERS**************************/
const char * const GPR_REG[32] = {
    "zero", "at", "v0", "v1", "a0", "a1", "a2", "a3",
    "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
    "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
    "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

const char * const COP0_REG[32] ={
	"Index","Random","EntryLo0","EntryLo1","Context","PageMask",
	"Wired","C0r7","BadVaddr","Count","EntryHi","Compare","Status",
	"Cause","EPC","PRId","Config","C0r17","C0r18","C0r19","C0r20",
	"C0r21","C0r22","C0r23","Debug","Perf","C0r26","C0r27","TagLo",
	"TagHi","ErrorPC","C0r31"
};

//floating point cop1 Floating point reg
const char * const COP1_REG_FP[32] ={
 	"f00","f01","f02","f03","f04","f05","f06","f07",
	"f08","f09","f10","f11","f12","f13","f14","f15",
	"f16","f17","f18","f19","f20","f21","f22","f23",
	"f24","f25","f26","f27","f28","f29","f30","f31"
};

//floating point cop1 control registers
const char * const COP1_REG_FCR[32] ={
 	"fcr00","fcr01","fcr02","fcr03","fcr04","fcr05","fcr06","fcr07",
	"fcr08","fcr09","fcr10","fcr11","fcr12","fcr13","fcr14","fcr15",
	"fcr16","fcr17","fcr18","fcr19","fcr20","fcr21","fcr22","fcr23",
	"fcr24","fcr25","fcr26","fcr27","fcr28","fcr29","fcr30","fcr31"
};

//floating point cop2 reg
const char * const COP2_REG_FP[32] ={
	"vf00","vf01","vf02","vf03","vf04","vf05","vf06","vf07",
	"vf08","vf09","vf10","vf11","vf12","vf13","vf14","vf15",
	"vf16","vf17","vf18","vf19","vf20","vf21","vf22","vf23",
	"vf24","vf25","vf26","vf27","vf28","vf29","vf30","vf31"
};

//cop2 control registers
const char * const COP2_REG_CTL[32] ={
	"vi00","vi01","vi02","vi03","vi04","vi05","vi06","vi07",
	"vi08","vi09","vi10","vi11","vi12","vi13","vi14","vi15",
	"Status","MACflag","ClipFlag","c2c19","R","I","Q","c2c23",
	"c2c24","c2c25","TPC","CMSAR0","FBRST","VPU-STAT","c2c30","CMSAR1"
};

const char * const COP2_VFnames[4] = { "x", "y", "z", "w" };

//gs privileged registers
const char * const GS_REG_PRIV[19] = {
	"PMODE","SMODE1","SMODE2","SRFSH","SYNCH1","SYNCH2","SYNCV",
	"DISPFB1","DISPLAY1","DISPFB2","DISPLAY2","EXTBUF","EXTDATA",
	"EXTWRITE","BGCOLOR","CSR","IMR","BUSDIR","SIGLBLID",
};

//gs privileged register addresses relative to 12000000h
const u32 GS_REG_PRIV_ADDR[19] = {
	0x00,0x10,0x20,0x30,0x40,0x50,0x60,0x70,0x80,0x90,
	0xa0,0xb0,0xc0,0xd0,0xE0,0x1000,0x1010,0x1040,0x1080
};

void P_COP2_Unknown( std::string& output );
void P_COP2_SPECIAL2( std::string& output );
void P_COP2_SPECIAL( std::string& output );
void P_COP2_BC2( std::string& output );

//****************************************************************************
//** COP2 - (VU0)                                                           **
//****************************************************************************
void P_QMFC2( std::string& output );
void P_CFC2( std::string& output );
void P_QMTC2( std::string& output );
void P_CTC2( std::string& output );
void P_BC2F( std::string& output );
void P_BC2T( std::string& output );
void P_BC2FL( std::string& output );
void P_BC2TL( std::string& output );
//*****************SPECIAL 1 VUO TABLE*******************************
void P_VADDx( std::string& output );
void P_VADDy( std::string& output );
void P_VADDz( std::string& output );
void P_VADDw( std::string& output );
void P_VSUBx( std::string& output );
void P_VSUBy( std::string& output );
void P_VSUBz( std::string& output );
void P_VSUBw( std::string& output );
void P_VMADDx( std::string& output );
void P_VMADDy( std::string& output );
void P_VMADDz( std::string& output );
void P_VMADDw( std::string& output );
void P_VMSUBx( std::string& output );
void P_VMSUBy( std::string& output );
void P_VMSUBz( std::string& output );
void P_VMSUBw( std::string& output );
void P_VMAXx( std::string& output );
void P_VMAXy( std::string& output );
void P_VMAXz( std::string& output );
void P_VMAXw( std::string& output );
void P_VMINIx( std::string& output );
void P_VMINIy( std::string& output );
void P_VMINIz( std::string& output );
void P_VMINIw( std::string& output );
void P_VMULx( std::string& output );
void P_VMULy( std::string& output );
void P_VMULz( std::string& output );
void P_VMULw( std::string& output );
void P_VMULq( std::string& output );
void P_VMAXi( std::string& output );
void P_VMULi( std::string& output );
void P_VMINIi( std::string& output );
void P_VADDq( std::string& output );
void P_VMADDq( std::string& output );
void P_VADDi( std::string& output );
void P_VMADDi( std::string& output );
void P_VSUBq( std::string& output );
void P_VMSUBq( std::string& output );
void P_VSUbi( std::string& output );
void P_VMSUBi( std::string& output );
void P_VADD( std::string& output );
void P_VMADD( std::string& output );
void P_VMUL( std::string& output );
void P_VMAX( std::string& output );
void P_VSUB( std::string& output );
void P_VMSUB( std::string& output );
void P_VOPMSUB( std::string& output );
void P_VMINI( std::string& output );
void P_VIADD( std::string& output );
void P_VISUB( std::string& output );
void P_VIADDI( std::string& output );
void P_VIAND( std::string& output );
void P_VIOR( std::string& output );
void P_VCALLMS( std::string& output );
void P_CALLMSR( std::string& output );
//***********************************END OF SPECIAL1 VU0 TABLE*****************************
//******************************SPECIAL2 VUO TABLE*****************************************
void P_VADDAx( std::string& output );
void P_VADDAy( std::string& output );
void P_VADDAz( std::string& output );
void P_VADDAw( std::string& output );
void P_VSUBAx( std::string& output );
void P_VSUBAy( std::string& output );
void P_VSUBAz( std::string& output );
void P_VSUBAw( std::string& output );
void P_VMADDAx( std::string& output );
void P_VMADDAy( std::string& output );
void P_VMADDAz( std::string& output );
void P_VMADDAw( std::string& output );
void P_VMSUBAx( std::string& output );
void P_VMSUBAy( std::string& output );
void P_VMSUBAz( std::string& output );
void P_VMSUBAw( std::string& output );
void P_VITOF0( std::string& output );
void P_VITOF4( std::string& output );
void P_VITOF12( std::string& output );
void P_VITOF15( std::string& output );
void P_VFTOI0( std::string& output );
void P_VFTOI4( std::string& output );
void P_VFTOI12( std::string& output );
void P_VFTOI15( std::string& output );
void P_VMULAx( std::string& output );
void P_VMULAy( std::string& output );
void P_VMULAz( std::string& output );
void P_VMULAw( std::string& output );
void P_VMULAq( std::string& output );
void P_VABS( std::string& output );
void P_VMULAi( std::string& output );
void P_VCLIPw( std::string& output );
void P_VADDAq( std::string& output );
void P_VMADDAq( std::string& output );
void P_VADDAi( std::string& output );
void P_VMADDAi( std::string& output );
void P_VSUBAq( std::string& output );
void P_VMSUBAq( std::string& output );
void P_VSUBAi( std::string& output );
void P_VMSUBAi( std::string& output );
void P_VADDA( std::string& output );
void P_VMADDA( std::string& output );
void P_VMULA( std::string& output );
void P_VSUBA( std::string& output );
void P_VMSUBA( std::string& output );
void P_VOPMULA( std::string& output );
void P_VNOP( std::string& output );
void P_VMONE( std::string& output );
void P_VMR32( std::string& output );
void P_VLQI( std::string& output );
void P_VSQI( std::string& output );
void P_VLQD( std::string& output );
void P_VSQD( std::string& output );
void P_VDIV( std::string& output );
void P_VSQRT( std::string& output );
void P_VRSQRT( std::string& output );
void P_VWAITQ( std::string& output );
void P_VMTIR( std::string& output );
void P_VMFIR( std::string& output );
void P_VILWR( std::string& output );
void P_VISWR( std::string& output );
void P_VRNEXT( std::string& output );
void P_VRGET( std::string& output );
void P_VRINIT( std::string& output );
void P_VRXOR( std::string& output );
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

//*************************************************************
// COP2 TABLES :)   [VU0 as a Co-Processor to the EE]
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
void (*COP2PrintTable[32])( std::string& output ) = {
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
void (*COP2BC2PrintTable[32])( std::string& output ) = {
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
void (*COP2SPECIAL1PrintTable[64])( std::string& output ) =
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
void (*COP2SPECIAL2PrintTable[128])( std::string& output ) =
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


void disR5900Fasm( std::string& output, u32 code, u32 pc, bool simplify )
{
	opcode_addr = pc;
	disasmOpcode = code;
	disSimplify = simplify;

	GetInstruction(code).disasm( output );
}

//*************************************************************
//************************COP2**********************************
void P_COP2_BC2( std::string& output )
{
	COP2BC2PrintTable[DECODE_C2BC]( output );
}
void P_COP2_SPECIAL( std::string& output )
{
	COP2SPECIAL1PrintTable[DECODE_FUNCTION ]( output );
}
void P_COP2_SPECIAL2( std::string& output )
{
	COP2SPECIAL2PrintTable[(disasmOpcode & 0x3) | ((disasmOpcode >> 4) & 0x7c)]( output );
}

//**************************UNKNOWN****************************
void P_COP2_Unknown( std::string& output )
{
	output += "COP2 ??";
}


//*************************************************************

//*****************SOME DECODE STUFF***************************

void label_decode( std::string& output, u32 addr )
{
	char buffer[32];
	sprintf(buffer, "->$0x%08X", addr);
	output += std::string(buffer);
}

void jump_decode( std::string& output )
{
    label_decode( output, DECODE_JUMP );
}

void offset_decode( std::string& output )
{
	label_decode( output, DECODE_OFFSET );
}

//*********************END OF DECODE ROUTINES******************

namespace OpcodeDisasm
{

void COP2( std::string& output )
{
	COP2PrintTable[DECODE_RS]( output );
}

// Unkown Opcode!
void Unknown( std::string& output )
{
	output += "?????";
}

void MMI_Unknown( std::string& output )
{
	output += "MMI ??";
}

void COP0_Unknown( std::string& output )
{
	output += "COP0 ??";
}

void COP1_Unknown( std::string& output )
{
	output += "FPU ??";
}

// sap!  it stands for string append.  It's not a friendly name but for now it makes
// the copy-paste marathon of code below more readable!
#define _sap( str ) ssappendf( output, str,

const char* signedImmediate(s32 imm, int len = 0)
{
	static char buffer[32];

	if (imm >= 0)
		sprintf(buffer,"0x%*X",len,imm);
	else
		sprintf(buffer,"-0x%*X",len,-imm);

	return buffer;
}

const char* disDestSource(int dest, int source)
{
	static char buffer[64];
#ifdef PRINT_REG_CONTENT
	sprintf(buffer,"%s,%s(0x%8.8x)",GPR_REG[dest],GPR_REG[source], cpuRegs.GPR.r[source].UL[0]);
#else
	if (disSimplify && dest == source)
		sprintf(buffer,"%s",GPR_REG[dest]);
	else
		sprintf(buffer,"%s,%s",GPR_REG[dest],GPR_REG[source]);

#endif

	return buffer;
}

void disBranch(std::string& output, const char* op)
{
	ssappendf(output, "%s\t", op);
	offset_decode(output);
}

void disBranch(std::string& output, const char* op, int rs)
{
#ifdef PRINT_REG_CONTENT
	ssappendf(output, "%s\t%s(0x%8.8x), ", op, GPR_REG[rs], cpuRegs.GPR.r[rs].UL[0]);
#else
	ssappendf(output, "%s\t%s, ", op, GPR_REG[rs]);
#endif
	offset_decode(output);
}

void disBranch(std::string& output, const char* op, int rs, int rt)
{
#ifdef PRINT_REG_CONTENT
	ssappendf(output, "%s\t%s(0x%8.8x), %s(0x%8.8x), ", op, GPR_REG[rs], cpuRegs.GPR.r[rs].UL[0], GPR_REG[rt], cpuRegs.GPR.r[rt].UL[0]);
#else
	ssappendf(output, "%s\t%s, %s, ", op, GPR_REG[rs], GPR_REG[rt]);
#endif
	offset_decode(output);
}

//********************* Standard Opcodes***********************
void J( std::string& output )      { output += "j\t";        jump_decode(output);}
void JAL( std::string& output )    { output += "jal\t";      jump_decode(output);}

void BEQ( std::string& output )
{
	int rs = DECODE_RS;
	int rt = DECODE_RT;

	if (disSimplify && rs == rt)
		disBranch(output, "b");
	else if (disSimplify && rs == 0 && rt != 0)
		disBranch(output, "beqz", rt);
	else if (disSimplify && rs != 0 && rt == 0)
		disBranch(output, "beqz", rs);
	else
		disBranch(output, "beq", rs, rt);
}

void BNE( std::string& output )
{
	int rs = DECODE_RS;
	int rt = DECODE_RT;

	if (disSimplify && rs == 0 && rt != 0)
		disBranch(output, "bnez", rt);
	else if (disSimplify && rs != 0 && rt == 0)
		disBranch(output, "bnez", rs);
	else
		disBranch(output, "bne", rs, rt);
}

void BLEZ( std::string& output )   { disBranch(output, "blez", DECODE_RS); }
void BGTZ( std::string& output )   { disBranch(output, "bgtz", DECODE_RS); }
void ADDI( std::string& output )   { _sap("addi\t%s, %s, 0x%04X")   GPR_REG[DECODE_RT], GPR_REG[DECODE_RS], DECODE_IMMED);}

void ADDIU( std::string& output )
{
	int rt = DECODE_RT;
	int rs = DECODE_RS;
	s16 imm = DECODE_IMMED;

	if (disSimplify && rs == 0)
		ssappendf(output, "li\t%s, %s",GPR_REG[rt],signedImmediate(imm));
	else
		ssappendf(output, "addiu\t%s, %s",disDestSource(rt,rs),signedImmediate(imm));
}

void SLTI( std::string& output )   { _sap("slti\t%s, 0x%04X")   disDestSource(DECODE_RT, DECODE_RS), DECODE_IMMED); }
void SLTIU( std::string& output )  { _sap("sltiu\t%s, 0x%04X")  disDestSource(DECODE_RT, DECODE_RS), DECODE_IMMED); }
void ANDI( std::string& output )   { _sap("andi\t%s, 0x%04X")   disDestSource(DECODE_RT, DECODE_RS), DECODE_IMMED);}

void ORI( std::string& output )
{
	int rt = DECODE_RT;
	int rs = DECODE_RS;

	u32 unsignedImm = (u16) DECODE_IMMED;

	if (disSimplify && rs == 0)
		ssappendf(output, "li\t%s, 0x%X",GPR_REG[rt],unsignedImm);
	else
		ssappendf(output, "ori\t%s, 0x%X",disDestSource(DECODE_RT, DECODE_RS),unsignedImm);
}

void XORI( std::string& output )   { _sap("xori\t%s, 0x%04X")       disDestSource(DECODE_RT, DECODE_RS), DECODE_IMMED); }
void LUI( std::string& output )    { _sap("lui\t%s, 0x%04X")        GPR_REG[DECODE_RT], DECODE_IMMED); }

void BEQL( std::string& output )
{
	int rs = DECODE_RS;
	int rt = DECODE_RT;

	if (disSimplify && rs == rt)
		disBranch(output, "bl");
	else if (disSimplify && rs == 0 && rt != 0)
		disBranch(output, "beqzl", rt);
	else if (disSimplify && rs != 0 && rt == 0)
		disBranch(output, "beqzl", rs);
	else
		disBranch(output, "beql", rs, rt);
}

void BNEL( std::string& output )
{
	int rs = DECODE_RS;
	int rt = DECODE_RT;

	if (disSimplify && rs == 0 && rt != 0)
		disBranch(output, "bnezl", rt);
	else if (disSimplify && rs != 0 && rt == 0)
		disBranch(output, "bnezl", rs);
	else
		disBranch(output, "bnel", rs, rt);
}

void BLEZL( std::string& output )  { disBranch(output, "blezl", DECODE_RS); }
void BGTZL( std::string& output )  { disBranch(output, "bgtzl", DECODE_RS); }
void DADDI( std::string& output )  { _sap("daddi\t%s, 0x%04X")      disDestSource(DECODE_RT, DECODE_RS), DECODE_IMMED); }
void DADDIU( std::string& output ) { _sap("daddiu\t%s, 0x%04X")     disDestSource(DECODE_RT, DECODE_RS), DECODE_IMMED); }

void disMemAccess( std::string& output, const char* name, int cop = 0)
{
	const char* rt;
	switch (cop)
	{
	case 0:
		rt = GPR_REG[DECODE_RT];
		break;
	case 1:
		rt = COP1_REG_FP[DECODE_FT];
		break;
	case 2:
		rt = COP2_REG_FP[DECODE_FT];
		break;
	default:
		rt = "???";
		break;
	}

	const char* rs = GPR_REG[DECODE_RS];
	s16 imm = DECODE_IMMED;

	if (disSimplify && imm == 0)
		ssappendf(output, "%s\t%s,(%s)",name,rt,rs);
	else
		ssappendf(output, "%s\t%s, %s(%s)",name,rt,signedImmediate(imm,4),rs);
}

void LDL( std::string& output )    { disMemAccess(output,"ldl"); }
void LDR( std::string& output )    { disMemAccess(output,"ldr"); }
void LB( std::string& output )     { disMemAccess(output,"lb"); }
void LH( std::string& output )     { disMemAccess(output,"lh"); }
void LWL( std::string& output )    { disMemAccess(output,"lwl"); }
void LW( std::string& output )     { disMemAccess(output,"lw"); }
void LBU( std::string& output )    { disMemAccess(output,"lbu"); }
void LHU( std::string& output )    { disMemAccess(output,"lhu"); }
void LWR( std::string& output )    { disMemAccess(output,"lwr"); }
void LWU( std::string& output )    { disMemAccess(output,"lwu"); }
void SB( std::string& output )     { disMemAccess(output,"sb"); }
void SH( std::string& output )     { disMemAccess(output,"sh"); }
void SWL( std::string& output )    { disMemAccess(output,"swl"); }
void SW( std::string& output )     { disMemAccess(output,"sw"); }
void SDL( std::string& output )    { disMemAccess(output,"sdl"); }
void SDR( std::string& output )    { disMemAccess(output,"sdr"); }
void SWR( std::string& output )    { disMemAccess(output,"swr"); }
void LD( std::string& output )     { disMemAccess(output,"ld"); }
void SD( std::string& output )     { disMemAccess(output,"sd"); }
void LQ( std::string& output )     { disMemAccess(output,"lq"); }
void SQ( std::string& output )     { disMemAccess(output,"sq"); }
void SWC1( std::string& output )   { disMemAccess(output,"swc1",1); }
void SQC2( std::string& output )   { disMemAccess(output,"sqc2",2); }
void PREF( std::string& output )   { output += "pref ---"; /*_sap("PREF\t%s, 0x%04X(%s)")   GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[RS]); */}
void LWC1( std::string& output )   { disMemAccess(output,"lwc1",1); }
void LQC2( std::string& output )   { disMemAccess(output,"lqc2",2); }
//********************END OF STANDARD OPCODES*************************

void SLL( std::string& output )
{
   if (disasmOpcode == 0x00000000)
        output += "nop";
    else
        _sap("sll\t%s, %s, 0x%02X") GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA);
}

void SRL( std::string& output )    { _sap("srl\t%s, 0x%02X")     disDestSource(DECODE_RD, DECODE_RT), DECODE_SA); }
void SRA( std::string& output )    { _sap("sra\t%s, 0x%02X")     disDestSource(DECODE_RD, DECODE_RT), DECODE_SA); }
void SLLV( std::string& output )   { _sap("sllv\t%s, %s")        disDestSource(DECODE_RD, DECODE_RT), GPR_REG[DECODE_RS]); }
void SRLV( std::string& output )   { _sap("srlv\t%s, %s")        disDestSource(DECODE_RD, DECODE_RT), GPR_REG[DECODE_RS]);}
void SRAV( std::string& output )   { _sap("srav\t%s, %s")        disDestSource(DECODE_RD, DECODE_RT), GPR_REG[DECODE_RS]); }
void JR( std::string& output )     { _sap("jr\t->%s")            GPR_REG[DECODE_RS]); }

void JALR( std::string& output )
{
    int rd = DECODE_RD;

    if (rd == 31)
        _sap("jalr\t->%s") GPR_REG[DECODE_RS]);
    else
        _sap("jalr\t%s, ->%s") GPR_REG[rd], GPR_REG[DECODE_RS]);
}

void SYNC( std::string& output )    { output += "SYNC"; }
void MFHI( std::string& output )    { _sap("mfhi\t%s")          GPR_REG[DECODE_RD]); }
void MTHI( std::string& output )    { _sap("mthi\t%s")          GPR_REG[DECODE_RS]); }
void MFLO( std::string& output )    { _sap("mflo\t%s")          GPR_REG[DECODE_RD]); }
void MTLO( std::string& output )    { _sap("mtlo\t%s")          GPR_REG[DECODE_RS]); }
void DSLLV( std::string& output )   { _sap("dsllv\t%s, %s")     disDestSource(DECODE_RD, DECODE_RT), GPR_REG[DECODE_RS]); }
void DSRLV( std::string& output )   { _sap("dsrlv\t%s, %s")     disDestSource(DECODE_RD, DECODE_RT), GPR_REG[DECODE_RS]); }
void DSRAV( std::string& output )   { _sap("dsrav\t%s, %s")     disDestSource(DECODE_RD, DECODE_RT), GPR_REG[DECODE_RS]); }
void MULT( std::string& output )    { _sap("mult\t%s, %s, %s")  GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]);}
void MULTU( std::string& output )   { _sap("multu\t%s, %s, %s") GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]);}
void DIV( std::string& output )     { _sap("div\t%s, %s")       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void DIVU( std::string& output )    { _sap("divu\t%s, %s")      GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }

void disAddAddu( std::string& output, const char* name )
{
	int rd = DECODE_RD;
	int rs = DECODE_RS;
	int rt = DECODE_RT;

	if (disSimplify && rs == 0)
		ssappendf(output,"move\t%s, %s",GPR_REG[rd],GPR_REG[rt]);
	else if (disSimplify && rt == 0)
		ssappendf(output,"move\t%s, %s",GPR_REG[rd],GPR_REG[rs]);
	else if (disSimplify && rd == rs)
		ssappendf(output, "%s\t%s, %s",name,GPR_REG[rd],GPR_REG[rt]);
	else if (disSimplify && rd == rt)
		ssappendf(output, "%s\t%s, %s",name,GPR_REG[rd],GPR_REG[rs]);
	else
		ssappendf(output, "%s\t%s, %s, %s",name,GPR_REG[rd], GPR_REG[rs], GPR_REG[rt]);
}

void ADD( std::string& output )     { disAddAddu(output,"add"); }
void ADDU( std::string& output )     { disAddAddu(output,"addu"); }

void SUB( std::string& output )     { _sap("sub\t%s, %s")   disDestSource(DECODE_RD,DECODE_RS), GPR_REG[DECODE_RT]); }
void SUBU( std::string& output )    { _sap("subu\t%s, %s")  disDestSource(DECODE_RD,DECODE_RS), GPR_REG[DECODE_RT]); }
void AND( std::string& output )     { _sap("and\t%s, %s")   disDestSource(DECODE_RD,DECODE_RS), GPR_REG[DECODE_RT]); }
void OR( std::string& output )      { _sap("or\t%s, %s")    disDestSource(DECODE_RD,DECODE_RS), GPR_REG[DECODE_RT]); }
void XOR( std::string& output )     { _sap("xor\t%s, %s")   disDestSource(DECODE_RD,DECODE_RS), GPR_REG[DECODE_RT]); }
void NOR( std::string& output )     { _sap("nor\t%s, %s")   disDestSource(DECODE_RD,DECODE_RS), GPR_REG[DECODE_RT]); }
void SLT( std::string& output )     { _sap("slt\t%s, %s")   disDestSource(DECODE_RD,DECODE_RS), GPR_REG[DECODE_RT]); }
void SLTU( std::string& output )    { _sap("sltu\t%s, %s")  disDestSource(DECODE_RD,DECODE_RS), GPR_REG[DECODE_RT]); }

void disDaddDaddu( std::string& output, const char* name )
{
	int rd = DECODE_RD;
	int rs = DECODE_RS;
	int rt = DECODE_RT;

	if (disSimplify && rs == 0)
		ssappendf(output,"dmove\t%s, %s",GPR_REG[DECODE_RD],GPR_REG[DECODE_RT]);
	else if (disSimplify && rt == 0)
		ssappendf(output,"dmove\t%s, %s",GPR_REG[DECODE_RD],GPR_REG[DECODE_RS]);
	else if (disSimplify && rd == rs)
		ssappendf(output, "%s\t%s, %s",name,GPR_REG[rd],GPR_REG[rt]);
	else if (disSimplify && rd == rt)
		ssappendf(output, "%s\t%s, %s",name,GPR_REG[rd],GPR_REG[rs]);
	else
		ssappendf(output, "%s\t%s, %s, %s",name,GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]);
}

void DADD( std::string& output )    { disDaddDaddu(output,"dadd"); }
void DADDU( std::string& output )    { disDaddDaddu(output,"daddu"); }

void DSUB( std::string& output )    { _sap("dsub\t%s, %s")      disDestSource(DECODE_RD,DECODE_RS), GPR_REG[DECODE_RT]); }
void DSUBU( std::string& output )   { _sap("dsubu\t%s, %s")     disDestSource(DECODE_RD,DECODE_RS), GPR_REG[DECODE_RT]); }
void TGE( std::string& output )     { _sap("tge\t%s, %s")       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void TGEU( std::string& output )    { _sap("tgeu\t%s, %s")      GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void TLT( std::string& output )     { _sap("tlt\t%s, %s")       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void TLTU( std::string& output )    { _sap("tltu\t%s, %s")      GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void TEQ( std::string& output )     { _sap("teq\t%s, %s")       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void TNE( std::string& output )     { _sap("tne\t%s, %s")       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void DSLL( std::string& output )    { _sap("dsll\t%s, 0x%02X")   disDestSource(DECODE_RD,DECODE_RT), DECODE_SA); }
void DSRL( std::string& output )    { _sap("dsrl\t%s, 0x%02X")   disDestSource(DECODE_RD,DECODE_RT), DECODE_SA); }
void DSRA( std::string& output )    { _sap("dsra\t%s, 0x%02X")   disDestSource(DECODE_RD,DECODE_RT), DECODE_SA); }
void DSLL32( std::string& output )  { _sap("dsll32\t%s, 0x%02X") disDestSource(DECODE_RD,DECODE_RT), DECODE_SA); }
void DSRL32( std::string& output )  { _sap("dsrl32\t%s, 0x%02X") disDestSource(DECODE_RD,DECODE_RT), DECODE_SA); }
void DSRA32( std::string& output )  { _sap("dsra32\t%s, 0x%02X") disDestSource(DECODE_RD,DECODE_RT), DECODE_SA); }
void MOVZ( std::string& output )    { _sap("movz\t%s, %s, %s") GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void MOVN( std::string& output )    { _sap("movn\t%s, %s, %s") GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void MFSA( std::string& output )    { _sap("mfsa\t%s")          GPR_REG[DECODE_RD]);}
void MTSA( std::string& output )    { _sap("mtsa\t%s")          GPR_REG[DECODE_RS]);}
//*** unsupport (yet) cpu opcodes
void SYSCALL( std::string& output ) { output +="syscall ---";/*_sap("syscall\t0x%05X")   DECODE_SYSCALL);*/}
void BREAK( std::string& output )   { output += "break   ---";/*_sap("break\t0x%05X")     DECODE_BREAK); */}
void CACHE( std::string& output )   { output += "cache   ---";/*_sap("cache\t%s, 0x%04X(%s)")  GPR_REG[DECODE_RT], DECODE_IMMED, GPR_REG[DECODE_RS]); */}
//************************REGIMM OPCODES***************************
void BLTZ( std::string& output )    { disBranch(output, "bltz", DECODE_RS); }
void BGEZ( std::string& output )    { disBranch(output, "bgez", DECODE_RS); }
void BLTZL( std::string& output )   { disBranch(output, "bltzl", DECODE_RS); }
void BGEZL( std::string& output )   { disBranch(output, "bgezl", DECODE_RS); }
void TGEI( std::string& output )    { _sap("tgei\t%s, 0x%04X") GPR_REG[DECODE_RS], DECODE_IMMED); }
void TGEIU( std::string& output )   { _sap("tgeiu\t%s,0x%04X") GPR_REG[DECODE_RS], DECODE_IMMED); }
void TLTI( std::string& output )    { _sap("tlti\t%s, 0x%04X") GPR_REG[DECODE_RS], DECODE_IMMED); }
void TLTIU( std::string& output )   { _sap("tltiu\t%s,0x%04X") GPR_REG[DECODE_RS], DECODE_IMMED); }
void TEQI( std::string& output )    { _sap("teqi\t%s, 0x%04X") GPR_REG[DECODE_RS], DECODE_IMMED); }
void TNEI( std::string& output )    { _sap("tnei\t%s, 0x%04X") GPR_REG[DECODE_RS], DECODE_IMMED); }
void BLTZAL( std::string& output )  { disBranch(output, "bltzal", DECODE_RS); }
void BGEZAL( std::string& output )  { disBranch(output, "bgezal", DECODE_RS); }
void BLTZALL( std::string& output ) { disBranch(output, "bltzall", DECODE_RS); }
void BGEZALL( std::string& output ) { disBranch(output, "bgezall", DECODE_RS); }
void MTSAB( std::string& output )   { _sap("mtsab\t%s, 0x%04X") GPR_REG[DECODE_RS], DECODE_IMMED);}
void MTSAH( std::string& output )   { _sap("mtsah\t%s, 0x%04X") GPR_REG[DECODE_RS], DECODE_IMMED);}


//***************************SPECIAL 2 CPU OPCODES*******************
const char* pmfhl_sub[] = { "lw", "uw", "slw", "lh", "sh" };

void MADD( std::string& output )    { _sap("madd\t%s, %s %s")        GPR_REG[DECODE_RD],GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void MADDU( std::string& output )   { _sap("maddu\t%s, %s %s")       GPR_REG[DECODE_RD],GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]);}
void PLZCW( std::string& output )   { _sap("plzcw\t%s, %s")          GPR_REG[DECODE_RD], GPR_REG[DECODE_RS]); }
void MADD1( std::string& output )   { _sap("madd1\t%s, %s %s")       GPR_REG[DECODE_RD],GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void MADDU1( std::string& output )  { _sap("maddu1\t%s, %s %s")      GPR_REG[DECODE_RD],GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void MFHI1( std::string& output )   { _sap("mfhi1\t%s")          GPR_REG[DECODE_RD]); }
void MTHI1( std::string& output )   { _sap("mthi1\t%s")          GPR_REG[DECODE_RS]); }
void MFLO1( std::string& output )   { _sap("mflo1\t%s")          GPR_REG[DECODE_RD]); }
void MTLO1( std::string& output )   { _sap("mtlo1\t%s")          GPR_REG[DECODE_RS]); }
void MULT1( std::string& output )   { _sap("mult1\t%s, %s, %s")        GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void MULTU1( std::string& output )  { _sap("multu1\t%s, %s, %s")        GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]);}
void DIV1( std::string& output )    { _sap("div1\t%s, %s")       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void DIVU1( std::string& output )   { _sap("divu1\t%s, %s")       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
//that have parametres that i haven't figure out how to display...
void PMFHL( std::string& output )   { _sap("pmfhl.%s \t%s")          pmfhl_sub[DECODE_SA], GPR_REG[DECODE_RD]); }
void PMTHL( std::string& output )   { _sap("pmthl.%s \t%s")          pmfhl_sub[DECODE_SA], GPR_REG[DECODE_RS]); }
void PSLLH( std::string& output )   { _sap("psllh   \t%s, %s, 0x%02X")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA); }
void PSRLH( std::string& output )   { _sap("psrlh   \t%s, %s, 0x%02X")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA);}
void PSRAH( std::string& output )   { _sap("psrah   \t%s, %s, 0x%02X")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA);}
void PSLLW( std::string& output )   { _sap( "psllw   \t%s, %s, 0x%02X")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA);}
void PSRLW( std::string& output )   { _sap( "psrlw   \t%s, %s, 0x%02X")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA);}
void PSRAW( std::string& output )   { _sap( "psraw   \t%s, %s, 0x%02X")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], DECODE_SA);}
//***************************END OF SPECIAL OPCODES******************
//*************************MMI0 OPCODES************************

void PADDW( std::string& output ){  _sap( "paddw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PSUBW( std::string& output ){  _sap( "psubw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PCGTW( std::string& output ){  _sap( "pcgtw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PMAXW( std::string& output ){  _sap( "pmaxw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PADDH( std::string& output ){  _sap( "paddh\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PSUBH( std::string& output ){  _sap( "psubh\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PCGTH( std::string& output ){  _sap( "pcgth\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PMAXH( std::string& output ){  _sap( "pmaxh\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PADDB( std::string& output ){  _sap( "paddb\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PSUBB( std::string& output ){  _sap( "psubb\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PCGTB( std::string& output ){  _sap( "pcgtb\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PADDSW( std::string& output ){ _sap( "paddsw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PSUBSW( std::string& output ){ _sap( "psubsw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PEXTLW( std::string& output ){ _sap( "pextlw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PPACW( std::string& output ) { _sap( "ppacw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PADDSH( std::string& output ){ _sap( "paddsh\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PSUBSH( std::string& output ){ _sap( "psubsh\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PEXTLH( std::string& output ){ _sap( "pextlh\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PPACH( std::string& output ) { _sap( "ppach\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PADDSB( std::string& output ){ _sap( "paddsb\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PSUBSB( std::string& output ){ _sap( "psubsb\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PEXTLB( std::string& output ){ _sap( "pextlb\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PPACB( std::string& output ) { _sap( "ppacb\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PEXT5( std::string& output ) { _sap( "pext5\t%s, %s")      GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]); }
void PPAC5( std::string& output ) { _sap( "ppac5\t%s, %s")      GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]); }
//**********END OF MMI0 OPCODES*********************************
//**********MMI1 OPCODES**************************************
void PABSW( std::string& output ){  _sap( "pabsw%s, %s")      GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]); }
void PCEQW( std::string& output ){  _sap( "pceqw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PMINW( std::string& output ){  _sap( "pminw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PADSBH( std::string& output ){ _sap( "padsbh\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PABSH( std::string& output ){  _sap( "pabsh%s, %s")      GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]); }
void PCEQH( std::string& output ){  _sap( "pceqh\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PMINH( std::string& output ){  _sap( "pminh\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PCEQB( std::string& output ){  _sap( "pceqb\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PADDUW( std::string& output ){ _sap( "padduw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PSUBUW( std::string& output ){ _sap( "psubuw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PEXTUW( std::string& output ){ _sap( "pextuw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PADDUH( std::string& output ){ _sap( "padduh\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PSUBUH( std::string& output ){ _sap( "psubuh\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PEXTUH( std::string& output ){ _sap( "pextuh\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PADDUB( std::string& output ){ _sap( "paddub\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PSUBUB( std::string& output ){ _sap( "psubub\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PEXTUB( std::string& output ){ _sap( "pextub\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void QFSRV( std::string& output ) { _sap( "qfsrv\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
//********END OF MMI1 OPCODES***********************************
//*********MMI2 OPCODES***************************************
void PMADDW( std::string& output ){ _sap( "pmaddw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PSLLVW( std::string& output ){ _sap( "psllvw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PSRLVW( std::string& output ){ _sap( "psrlvw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PMSUBW( std::string& output ){ _sap( "msubw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PMFHI( std::string& output ){  _sap( "pmfhi\t%s")          GPR_REG[DECODE_RD]); }
void PMFLO( std::string& output ){  _sap( "pmflo\t%s")          GPR_REG[DECODE_RD]); }
void PINTH( std::string& output ){  _sap( "pinth\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PMULTW( std::string& output ){ _sap( "pmultw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PDIVW( std::string& output ){  _sap( "pdivw\t%s, %s")      GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PCPYLD( std::string& output ){ _sap( "pcpyld\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PMADDH( std::string& output ){ _sap( "pmaddh\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PHMADH( std::string& output ){ _sap( "phmadh\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PAND( std::string& output ){   _sap( "pand\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PXOR( std::string& output ){   _sap( "pxor\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PMSUBH( std::string& output ){ _sap( "pmsubh\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PHMSBH( std::string& output ){ _sap( "phmsbh\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PEXEH( std::string& output ){  _sap( "pexeh\t%s, %s")      GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]); }
void PREVH( std::string& output ){  _sap( "prevh\t%s, %s")      GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]); }
void PMULTH( std::string& output ){ _sap( "pmulth\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PDIVBW( std::string& output ){ _sap( "pdivbw\t%s, %s")      GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PEXEW( std::string& output ){  _sap( "pexew\t%s, %s")      GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]); }
void PROT3W( std::string& output ){ _sap( "prot3w\t%s, %s")      GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]); }
//*****END OF MMI2 OPCODES***********************************
//*************************MMI3 OPCODES************************
void PMADDUW( std::string& output ){ _sap("pmadduw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], GPR_REG[DECODE_RS]); }
void PSRAVW( std::string& output ){  _sap("psravw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RT], GPR_REG[DECODE_RS]); }
void PMTHI( std::string& output ){   _sap("pmthi\t%s")          GPR_REG[DECODE_RS]); }
void PMTLO( std::string& output ){   _sap("pmtlo\t%s")          GPR_REG[DECODE_RS]); }
void PINTEH( std::string& output ){  _sap("pinteh\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PMULTUW( std::string& output ){ _sap("pmultuw\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PDIVUW( std::string& output ){  _sap("pdivuw\t%s, %s")       GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PCPYUD( std::string& output ){  _sap("pcpyud\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void POR( std::string& output ){     _sap("por\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PNOR( std::string& output ){    _sap("pnor\t%s, %s, %s")   GPR_REG[DECODE_RD], GPR_REG[DECODE_RS], GPR_REG[DECODE_RT]); }
void PEXCH( std::string& output ){   _sap("pexch\t%s, %s")       GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]);}
void PCPYH( std::string& output ){   _sap("pcpyh\t%s, %s")       GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]);}
void PEXCW( std::string& output ){   _sap("pexcw\t%s, %s")       GPR_REG[DECODE_RD], GPR_REG[DECODE_RT]);}
//**********************END OF MMI3 OPCODES********************

//****************************************************************************
//** COP0                                                                   **
//****************************************************************************
void MFC0( std::string& output ){  _sap("mfc0\t%s, %s")  GPR_REG[DECODE_RT], COP0_REG[DECODE_FS]); }
void MTC0( std::string& output ){  _sap("mtc0\t%s, %s")  GPR_REG[DECODE_RT], COP0_REG[DECODE_FS]); }
void BC0F( std::string& output ){  output += "bc0f\t";       offset_decode(output); }
void BC0T( std::string& output ){  output += "bc0t\t";       offset_decode(output); }
void BC0FL( std::string& output ){ output += "bc0fl\t";      offset_decode(output); }
void BC0TL( std::string& output ){ output += "bc0tl\t";      offset_decode(output); }
void TLBR( std::string& output ){  output += "tlbr";}
void TLBWI( std::string& output ){ output += "tlbwi";}
void TLBWR( std::string& output ){ output += "tlbwr";}
void TLBP( std::string& output ){  output += "tlbp";}
void ERET( std::string& output ){  output += "eret";}
void DI( std::string& output ){    output += "di";}
void EI( std::string& output ){    output += "ei";}
//****************************************************************************
//** END OF COP0                                                            **
//****************************************************************************
//****************************************************************************
//** COP1 - Floating Point Unit (FPU)                                       **
//****************************************************************************
void MFC1( std::string& output ){   _sap("mfc1\t%s, %s")      GPR_REG[DECODE_RT], COP1_REG_FP[DECODE_FS]);  }
void CFC1( std::string& output ){   _sap("cfc1\t%s, %s")      GPR_REG[DECODE_RT], COP1_REG_FCR[DECODE_FS]); }
void MTC1( std::string& output ){   _sap("mtc1\t%s, %s")      GPR_REG[DECODE_RT], COP1_REG_FP[DECODE_FS]);  }
void CTC1( std::string& output ){   _sap("ctc1\t%s, %s")      GPR_REG[DECODE_RT], COP1_REG_FCR[DECODE_FS]); }
void BC1F( std::string& output ){   output += "bc1f\t";      offset_decode(output); }
void BC1T( std::string& output ){   output += "bc1t\t";      offset_decode(output); }
void BC1FL( std::string& output ){  output += "bc1fl\t";     offset_decode(output); }
void BC1TL( std::string& output ){  output += "bc1tl\t";     offset_decode(output); }
void ADD_S( std::string& output ){  _sap("add.s\t%s, %s, %s") COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]);}
void SUB_S( std::string& output ){  _sap("sub.s\t%s, %s, %s") COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]);}
void MUL_S( std::string& output ){  _sap("mul.s\t%s, %s, %s") COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]);}
void DIV_S( std::string& output ){  _sap("div.s\t%s, %s, %s") COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }
void SQRT_S( std::string& output ){ _sap("sqrt.s\t%s, %s")   COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FT]); }
void ABS_S( std::string& output ){  _sap("abs.s\t%s, %s")     COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS]); }
void MOV_S( std::string& output ){  _sap("mov.s\t%s, %s")     COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS]); }
void NEG_S( std::string& output ){  _sap("neg.s\t%s, %s")     COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS]);}
void RSQRT_S( std::string& output ){_sap("rsqrt.s\t%s, %s, %s") COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]);}
void ADDA_S( std::string& output ){ _sap("adda.s\t%s, %s")     COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }
void SUBA_S( std::string& output ){ _sap("suba.s\t%s, %s")     COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }
void MULA_S( std::string& output ){ _sap("mula.s\t%s, %s")   COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }
void MADD_S( std::string& output ){ _sap("madd.s\t%s, %s, %s") COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }
void MSUB_S( std::string& output ){ _sap("msub.s\t%s, %s, %s") COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }
void MADDA_S( std::string& output ){_sap("madda.s\t%s, %s")   COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }
void MSUBA_S( std::string& output ){_sap("msuba.s\t%s, %s")   COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }
void CVT_W( std::string& output ){  _sap("cvt.w.s\t%s, %s")   COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS]); }
void MAX_S( std::string& output ){  _sap("max.s\t%s, %s, %s") COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]);}
void MIN_S( std::string& output ){  _sap("min.s\t%s, %s, %s") COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]);}
void C_F( std::string& output ){    _sap("c.f.s\t%s, %s")     COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }
void C_EQ( std::string& output ){   _sap("c.eq.s\t%s, %s")    COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }
void C_LT( std::string& output ){   _sap("c.lt.s\t%s, %s")    COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }
void C_LE( std::string& output ){   _sap("c.le.s\t%s, %s")    COP1_REG_FP[DECODE_FS], COP1_REG_FP[DECODE_FT]); }
void CVT_S( std::string& output ){  _sap("cvt.s.w\t%s, %s")   COP1_REG_FP[DECODE_FD], COP1_REG_FP[DECODE_FS]); }
//****************************************************************************
//** END OF COP1                                                            **
//****************************************************************************

}	// End namespace R5900::OpcodeDisasm

//****************************************************************************
//** COP2 - (VU0)                                                           **
//****************************************************************************
void P_QMFC2( std::string& output ){   _sap("qmfc2\t%s, %s")      GPR_REG[DECODE_RT], COP2_REG_FP[DECODE_FS]);  }
void P_CFC2( std::string& output ){    _sap("cfc2\t%s, %s")      GPR_REG[DECODE_RT], COP2_REG_CTL[DECODE_FS]); }
void P_QMTC2( std::string& output ){   _sap("qmtc2\t%s, %s")      GPR_REG[DECODE_RT], COP2_REG_FP[DECODE_FS]); }
void P_CTC2( std::string& output ){    _sap("ctc2\t%s, %s")      GPR_REG[DECODE_RT], COP2_REG_CTL[DECODE_FS]); }
void P_BC2F( std::string& output ){    output += "bc2f\t";      offset_decode(output); }
void P_BC2T( std::string& output ){    output += "bc2t\t";      offset_decode(output); }
void P_BC2FL( std::string& output ){   output += "bc2fl\t";     offset_decode(output); }
void P_BC2TL( std::string& output ){   output += "bc2tl\t";     offset_decode(output); }
//******************************SPECIAL 1 VUO TABLE****************************************
#define _X ((disasmOpcode>>24) & 1)
#define _Y ((disasmOpcode>>23) & 1)
#define _Z ((disasmOpcode>>22) & 1)
#define _W ((disasmOpcode>>21) & 1)

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
	return arr[(disasmOpcode>>21)&3];
}

char dest_ftf()
{
	const char arr[4] = { 'x', 'y', 'z', 'w' };
	return arr[(disasmOpcode>>23)&3];
}

void P_VADDx( std::string& output ){_sap("vaddx.%s %s, %s, %sx") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VADDy( std::string& output ){_sap("vaddy.%s %s, %s, %sy") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VADDz( std::string& output ){_sap("vaddz.%s %s, %s, %sz") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VADDw( std::string& output ){_sap("vaddw.%s %s, %s, %sw") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VSUBx( std::string& output ){_sap("vsubx.%s %s, %s, %sx") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VSUBy( std::string& output ){_sap("vsuby.%s %s, %s, %sy") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VSUBz( std::string& output ){_sap("vsubz.%s %s, %s, %sz") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VSUBw( std::string& output ){_sap("vsubw.%s %s, %s, %sw") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMADDx( std::string& output ){_sap("vmaddx.%s %s, %s, %sx") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMADDy( std::string& output ){_sap("vmaddy.%s %s, %s, %sy") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMADDz( std::string& output ){_sap("vmaddz.%s %s, %s, %sz") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMADDw( std::string& output ){_sap("vmaddw.%s %s, %s, %sw") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMSUBx( std::string& output ){_sap("vmsubx.%s %s, %s, %sx") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMSUBy( std::string& output ){_sap("vmsuby.%s %s, %s, %sy") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMSUBz( std::string& output ){_sap("vmsubz.%s %s, %s, %sz") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMSUBw( std::string& output ){_sap("vmsubw.%s %s, %s, %sw") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMAXx( std::string& output ){_sap("vmaxx.%s %s, %s, %sx") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMAXy( std::string& output ){_sap("vmaxy.%s %s, %s, %sy") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMAXz( std::string& output ){_sap("vmaxz.%s %s, %s, %sz") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMAXw( std::string& output ){_sap("vmaxw.%s %s, %s, %sw") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMINIx( std::string& output ){_sap("vminix.%s %s, %s, %sx") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMINIy( std::string& output ){_sap("vminiy.%s %s, %s, %sy") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); ;}
void P_VMINIz( std::string& output ){_sap("vminiz.%s %s, %s, %sz") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMINIw( std::string& output ){_sap("vminiw.%s %s, %s, %sw") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMULx( std::string& output ){_sap("vmulx.%s %s,%s,%sx") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMULy( std::string& output ){_sap("vmuly.%s %s,%s,%sy") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMULz( std::string& output ){_sap("vmulz.%s %s,%s,%sz") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMULw( std::string& output ){_sap("vmulw.%s %s,%s,%sw") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMULq( std::string& output ){_sap("vmulq.%s %s,%s,Q") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VMAXi( std::string& output ){_sap("vmaxi.%s %s,%s,I") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VMULi( std::string& output ){_sap("vmuli.%s %s,%s,I") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VMINIi( std::string& output ){_sap("vminii.%s %s,%s,I") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VADDq( std::string& output ){_sap("vaddq.%s %s,%s,Q") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VMADDq( std::string& output ){_sap("vmaddq.%s %s,%s,Q") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VADDi( std::string& output ){_sap("vaddi.%s %s,%s,I") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VMADDi( std::string& output ){_sap("vmaddi.%s %s,%s,I") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VSUBq( std::string& output ){_sap("vsubq.%s %s,%s,Q") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VMSUBq( std::string& output ){_sap("vmsubq.%s %s,%s,Q") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VSUbi( std::string& output ){_sap("vsubi.%s %s,%s,I") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VMSUBi( std::string& output ){_sap("vmsubi.%s %s,%s,I") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS]); }
void P_VADD( std::string& output ){_sap("vadd.%s %s, %s, %s") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMADD( std::string& output ){_sap("vmadd.%s %s, %s, %s") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMUL( std::string& output ){_sap("vmul.%s %s, %s, %s") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMAX( std::string& output ){_sap("vmax.%s %s, %s, %s") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VSUB( std::string& output ){_sap("vsub.%s %s, %s, %s") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMSUB( std::string& output ){_sap("vmsub.%s %s, %s, %s") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VOPMSUB( std::string& output ){_sap("vopmsub.xyz %s, %s, %s") COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMINI( std::string& output ){_sap("vmini.%s %s, %s, %s") dest_string(),COP2_REG_FP[DECODE_FD], COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VIADD( std::string& output ){_sap("viadd %s, %s, %s") COP2_REG_CTL[DECODE_SA], COP2_REG_CTL[DECODE_FS], COP2_REG_CTL[DECODE_FT]);}
void P_VISUB( std::string& output ){_sap("visub %s, %s, %s") COP2_REG_CTL[DECODE_SA], COP2_REG_CTL[DECODE_FS], COP2_REG_CTL[DECODE_FT]);}
void P_VIADDI( std::string& output ){_sap("viaddi %s, %s, 0x%x") COP2_REG_CTL[DECODE_FT], COP2_REG_CTL[DECODE_FS], DECODE_SA);}
void P_VIAND( std::string& output ){_sap("viand %s, %s, %s") COP2_REG_CTL[DECODE_SA], COP2_REG_CTL[DECODE_FS], COP2_REG_CTL[DECODE_FT]);}
void P_VIOR( std::string& output ){_sap("vior %s, %s, %s") COP2_REG_CTL[DECODE_SA], COP2_REG_CTL[DECODE_FS], COP2_REG_CTL[DECODE_FT]);}
void P_VCALLMS( std::string& output ){output += "vcallms";}
void P_CALLMSR( std::string& output ){output += "callmsr";}
//***********************************END OF SPECIAL1 VU0 TABLE*****************************
//******************************SPECIAL2 VUO TABLE*****************************************
void P_VADDAx( std::string& output ){_sap("vaddax.%s ACC,%s,%sx") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VADDAy( std::string& output ){_sap("vadday.%s ACC,%s,%sy") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VADDAz( std::string& output ){_sap("vaddaz.%s ACC,%s,%sz") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VADDAw( std::string& output ){_sap("vaddaw.%s ACC,%s,%sw") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VSUBAx( std::string& output ){_sap("vsubax.%s ACC,%s,%sx") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VSUBAy( std::string& output ){_sap("vsubay.%s ACC,%s,%sy") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VSUBAz( std::string& output ){_sap("vsubaz.%s ACC,%s,%sz") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VSUBAw( std::string& output ){_sap("vsubaw.%s ACC,%s,%sw") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMADDAx( std::string& output ){_sap("vmaddax.%s ACC,%s,%sx") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMADDAy( std::string& output ){_sap("vmadday.%s ACC,%s,%sy") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMADDAz( std::string& output ){_sap("vmaddaz.%s ACC,%s,%sz") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMADDAw( std::string& output ){_sap("vmaddaw.%s ACC,%s,%sw") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMSUBAx( std::string& output ){_sap("vmsubax.%s ACC,%s,%sx") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMSUBAy( std::string& output ){_sap("vmsubay.%s ACC,%s,%sy") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMSUBAz( std::string& output ){_sap("vmsubaz.%s ACC,%s,%sz") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMSUBAw( std::string& output ){_sap("vmsubaw.%s ACC,%s,%sw") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VITOF0( std::string& output ){_sap("vitof0.%s %s, %s") dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]); }
void P_VITOF4( std::string& output ){_sap("vitof4.%s %s, %s") dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]); }
void P_VITOF12( std::string& output ){_sap("vitof12.%s %s, %s") dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]); }
void P_VITOF15( std::string& output ){_sap("vitof15.%s %s, %s") dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]); }
void P_VFTOI0( std::string& output ) {_sap("vftoi0.%s %s, %s") dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]); }
void P_VFTOI4( std::string& output ) {_sap("vftoi4.%s %s, %s") dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]); }
void P_VFTOI12( std::string& output ){_sap("vftoi12.%s %s, %s") dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]); }
void P_VFTOI15( std::string& output ){_sap("vftoi15.%s %s, %s") dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]); }
void P_VMULAx( std::string& output ){_sap("vmulax.%s ACC,%s,%sx") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMULAy( std::string& output ){_sap("vmulay.%s ACC,%s,%sy") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMULAz( std::string& output ){_sap("vmulaz.%s ACC,%s,%sz") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMULAw( std::string& output ){_sap("vmulaw.%s ACC,%s,%sw") dest_string(),COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]);}
void P_VMULAq( std::string& output ){_sap("vmulaq.%s ACC %s, Q") dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VABS( std::string& output ){_sap("vabs.%s %s, %s") dest_string(),COP2_REG_FP[DECODE_FT], COP2_REG_FP[DECODE_FS]);}
void P_VMULAi( std::string& output ){_sap("vmulai.%s ACC %s, I") dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VCLIPw( std::string& output ){_sap("vclip %sxyz, %sw") COP2_REG_FP[DECODE_FS], COP2_REG_FP[DECODE_FT]);}
void P_VADDAq( std::string& output ){_sap("vaddaq.%s ACC %s, Q") dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VMADDAq( std::string& output ){_sap("vmaddaq.%s ACC %s, Q") dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VADDAi( std::string& output ){_sap("vaddai.%s ACC %s, I") dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VMADDAi( std::string& output ){_sap("vmaddai.%s ACC %s, I") dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VSUBAq( std::string& output ){_sap("vsubaq.%s ACC %s, Q") dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VMSUBAq( std::string& output ){_sap("vmsubaq.%s ACC %s, Q") dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VSUBAi( std::string& output ){_sap("vsubai.%s ACC %s, I") dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VMSUBAi( std::string& output ){_sap("vmsubai.%s ACC %s, I") dest_string(), COP2_REG_FP[DECODE_FS]); }
void P_VADDA( std::string& output ){_sap("vadda.%s ACC %s, %s") dest_string(), COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMADDA( std::string& output ){_sap("vmadda.%s ACC %s, %s") dest_string(), COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMULA( std::string& output ){_sap("vmula.%s ACC %s, %s") dest_string(), COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VSUBA( std::string& output ){_sap("vsuba.%s ACC %s, %s") dest_string(), COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VMSUBA( std::string& output ){_sap("vmsuba.%s ACC %s, %s") dest_string(), COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VOPMULA( std::string& output ){_sap("vopmula.xyz %sxyz, %sxyz") COP2_REG_FP[DECODE_FS],COP2_REG_FP[DECODE_FT]); }
void P_VNOP( std::string& output ){output += "vnop";}
void P_VMONE( std::string& output ){_sap("vmove.%s, %s, %s") dest_string(), COP2_REG_FP[DECODE_FT],COP2_REG_FP[DECODE_FS]); }
void P_VMR32( std::string& output ){_sap("vmr32.%s, %s, %s") dest_string(), COP2_REG_FP[DECODE_FT],COP2_REG_FP[DECODE_FS]); }
void P_VLQI( std::string& output ){_sap("vlqi %s%s, (%s++)") COP2_REG_FP[DECODE_FT], dest_string(), COP2_REG_CTL[DECODE_FS]);}
void P_VSQI( std::string& output ){_sap("vsqi %s%s, (%s++)") COP2_REG_FP[DECODE_FS], dest_string(), COP2_REG_CTL[DECODE_FT]);}
void P_VLQD( std::string& output ){_sap("vlqd %s%s, (--%s)") COP2_REG_FP[DECODE_FT], dest_string(), COP2_REG_CTL[DECODE_FS]);}
void P_VSQD( std::string& output ){_sap("vsqd %s%s, (--%s)") COP2_REG_FP[DECODE_FS], dest_string(), COP2_REG_CTL[DECODE_FT]);}
void P_VDIV( std::string& output ){_sap("vdiv Q, %s%c, %s%c") COP2_REG_FP[DECODE_FS], dest_fsf(), COP2_REG_FP[DECODE_FT], dest_ftf());}
void P_VSQRT( std::string& output ){_sap("vsqrt Q, %s%c") COP2_REG_FP[DECODE_FT], dest_ftf());}
void P_VRSQRT( std::string& output ){_sap("vrsqrt Q, %s%c, %s%c") COP2_REG_FP[DECODE_FS], dest_fsf(), COP2_REG_FP[DECODE_FT], dest_ftf());}
void P_VWAITQ( std::string& output ){output += "vwaitq";}
void P_VMTIR( std::string& output ){_sap("vmtir %s, %s%c") COP2_REG_CTL[DECODE_FT], COP2_REG_FP[DECODE_FS], dest_fsf());}
void P_VMFIR( std::string& output ){_sap("vmfir %s%c, %s") COP2_REG_FP[DECODE_FT], dest_string(), COP2_REG_CTL[DECODE_FS]);}
void P_VILWR( std::string& output ){_sap("vilwr %s, (%s)%s") COP2_REG_CTL[DECODE_FT], COP2_REG_CTL[DECODE_FS], dest_string());}
void P_VISWR( std::string& output ){_sap("viswr %s, (%s)%s") COP2_REG_CTL[DECODE_FT], COP2_REG_CTL[DECODE_FS], dest_string());}
void P_VRNEXT( std::string& output ){_sap("vrnext %s%s, R") COP2_REG_CTL[DECODE_FT], dest_string());}
void P_VRGET( std::string& output ){_sap("vrget %s%s, R") COP2_REG_CTL[DECODE_FT], dest_string());}
void P_VRINIT( std::string& output ){_sap("vrinit R, %s%s") COP2_REG_CTL[DECODE_FS], dest_string());}
void P_VRXOR( std::string& output ){_sap("vrxor R, %s%s") COP2_REG_CTL[DECODE_FS], dest_string());}
//************************************END OF SPECIAL2 VUO TABLE****************************

}
