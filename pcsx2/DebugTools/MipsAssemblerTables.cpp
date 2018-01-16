/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2014-2014  PCSX2 Dev Team
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
#include "MipsAssemblerTables.h"

/* Placeholders for encoding

	s	source register
	d	destination register
	t	target register
	S	float source reg
	D	float dest reg
	T	float traget reg
	i	16 bit immediate value
	I	32 bit immediate value
	u	Shifted 16 bit immediate (upper)
	n	negative 16 bit immediate (for subi/u aliases)
	b	26 bit immediate
	a	5 bit immediate
*/

// NOTE: This tables also contains opcodes that aren't available on PS2. This was done
// because it's shared between multiple projects, and manually removing the opcodes every
// time is error prone and makes it harder to maintain. They aren't accessible, so they
// cause no harm besides appearing here.
const tMipsOpcode MipsOpcodes[] = {
//     31---------26---------------------------------------------------0
//     |  opcode   |                                                   |
//     ------6----------------------------------------------------------
//     |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
// 000 | *1    | *2    | J     | JAL   | BEQ   | BNE   | BLEZ  | BGTZ  | 00..07
// 001 | ADDI  | ADDIU | SLTI  | SLTIU | ANDI  | ORI   | XORI  | LUI   | 08..0F
// 010 | *3    | *4    | ---   | ---   | BEQL  | BNEL  | BLEZL | BGTZL | 10..17
// 011 | DADDI | DADDIU| LDL   | LDR   | ---   | ---   | LQ    | SQ   | 18..1F
// 100 | LB    | LH    | LWL   | LW    | LBU   | LHU   | LWR   | LWU   | 20..27
// 101 | SB    | SH    | SWL   | SW    | SDL   | SDR   | SWR   | CACHE | 28..2F
// 110 | LL    | LWC1  | LV.S  | ---   | LLD   | ULV.Q | LV.Q  | LD    | 30..37
// 111 | SC    | SWC1  | SV.S  | ---   | SCD   | USV.Q | SV.Q  | SD    | 38..3F
//  hi |-------|-------|-------|-------|-------|-------|-------|-------|
//		*1 = SPECIAL	*2 = REGIMM		*3 = COP0		*4 = COP1
	{ "j",		"I",			MIPS_OP(0x02), 			MA_MIPS1,	MO_IPCA|MO_DELAY|MO_NODELAYSLOT },
	{ "jal",	"I",			MIPS_OP(0x03),			MA_MIPS1,	MO_IPCA|MO_DELAY|MO_NODELAYSLOT },
	{ "beq",	"s,t,i",		MIPS_OP(0x04),			MA_MIPS1,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "beqz",	"s,i",			MIPS_OP(0x04),			MA_MIPS1,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "b",		"i",			MIPS_OP(0x04), 			MA_MIPS1,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "bne",	"s,t,i",		MIPS_OP(0x05),			MA_MIPS1,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "bnez",	"s,i",			MIPS_OP(0x05),			MA_MIPS1,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "blez",	"s,i",			MIPS_OP(0x06),			MA_MIPS1,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "bgtz",	"s,i",			MIPS_OP(0x07),			MA_MIPS1,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "addi",	"t,s,i",		MIPS_OP(0x08),			MA_MIPS1,	0 },
	{ "addi",	"s,i",			MIPS_OP(0x08),			MA_MIPS1,	MO_RST },
	{ "addiu",	"t,s,i",		MIPS_OP(0x09),			MA_MIPS1,	0 },
	{ "addiu",	"s,i",			MIPS_OP(0x09),			MA_MIPS1,	MO_RST },
	{ "slti",	"t,s,i",		MIPS_OP(0x0A),			MA_MIPS1,	0 },
	{ "slti",	"s,i",			MIPS_OP(0x0A),			MA_MIPS1,	MO_RST },
	{ "sltiu",	"t,s,i",		MIPS_OP(0x0B),			MA_MIPS1,	0 },
	{ "sltiu",	"s,i",			MIPS_OP(0x0B),			MA_MIPS1,	MO_RST },
	{ "andi",	"t,s,i",		MIPS_OP(0x0C),			MA_MIPS1,	0 },
	{ "andi",	"s,i",			MIPS_OP(0x0C),			MA_MIPS1,	MO_RST },
	{ "ori",	"t,s,i",		MIPS_OP(0x0D),			MA_MIPS1,	0 },
	{ "ori",	"s,i",			MIPS_OP(0x0D),			MA_MIPS1,	MO_RST },
	{ "xori",	"t,s,i",		MIPS_OP(0x0E),			MA_MIPS1,	0 },
	{ "xori",	"s,i",			MIPS_OP(0x0E),			MA_MIPS1,	MO_RST },
	{ "lui",	"t,i",			MIPS_OP(0x0F),			MA_MIPS1,	0 },
	{ "beql",	"s,t,i",		MIPS_OP(0x14),			MA_MIPS2,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "beqzl",	"s,i",			MIPS_OP(0x14),			MA_MIPS2,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "bnel",	"s,t,i",		MIPS_OP(0x15),			MA_MIPS2,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "bnezl",	"s,i",			MIPS_OP(0x15),			MA_MIPS2,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "blezl",	"s,i",			MIPS_OP(0x16),			MA_MIPS2,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "bgtzl",	"s,i",			MIPS_OP(0x17),			MA_MIPS2,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "daddi",	"t,s,i",		MIPS_OP(0x18),			MA_MIPS3,	MO_64BIT },
	{ "daddi",	"s,i",			MIPS_OP(0x18),			MA_MIPS3,	MO_64BIT|MO_RST },
	{ "daddiu",	"t,s,i",		MIPS_OP(0x19),			MA_MIPS3,	MO_64BIT },
	{ "daddiu",	"s,i",			MIPS_OP(0x19),			MA_MIPS3,	MO_64BIT|MO_RST },
	{ "ldl",	"t,i(s)",		MIPS_OP(0x1A),			MA_MIPS3,	MO_64BIT|MO_DELAYRT },
	{ "ldl",	"t,(s)",		MIPS_OP(0x1A),			MA_MIPS3,	MO_64BIT|MO_DELAYRT },
	{ "ldr",	"t,i(s)",		MIPS_OP(0x1B),			MA_MIPS3,	MO_64BIT|MO_DELAYRT|MO_IGNORERTD },
	{ "ldr",	"t,(s)",		MIPS_OP(0x1B),			MA_MIPS3,	MO_64BIT|MO_DELAYRT|MO_IGNORERTD },
	{ "lq",		"t,i(s)",		MIPS_OP(0x1E),			MA_MIPS1,	MO_DELAYRT },
	{ "sq",		"t,i(s)",		MIPS_OP(0x1F),			MA_MIPS1,	MO_DELAYRT },
	{ "lb",		"t,i(s)",		MIPS_OP(0x20),			MA_MIPS1,	MO_DELAYRT },
	{ "lb",		"t,(s)",		MIPS_OP(0x20),			MA_MIPS1,	MO_DELAYRT },
	{ "lh",		"t,i(s)",		MIPS_OP(0x21),			MA_MIPS1,	MO_DELAYRT },
	{ "lh",		"t,(s)",		MIPS_OP(0x21),			MA_MIPS1,	MO_DELAYRT },
	{ "lwl",	"t,i(s)",		MIPS_OP(0x22),			MA_MIPS1,	MO_DELAYRT },
	{ "lwl",	"t,(s)",		MIPS_OP(0x22),			MA_MIPS1,	MO_DELAYRT },
	{ "lw",		"t,i(s)",		MIPS_OP(0x23),			MA_MIPS1,	MO_DELAYRT },
	{ "lw",		"t,(s)",		MIPS_OP(0x23),			MA_MIPS1,	MO_DELAYRT },
	{ "lbu",	"t,i(s)",		MIPS_OP(0x24),			MA_MIPS1,	MO_DELAYRT },
	{ "lbu",	"t,(s)",		MIPS_OP(0x24),			MA_MIPS1,	MO_DELAYRT },
	{ "lhu",	"t,i(s)",		MIPS_OP(0x25),			MA_MIPS1,	MO_DELAYRT },
	{ "lhu",	"t,(s)",		MIPS_OP(0x25),			MA_MIPS1,	MO_DELAYRT },
	{ "lwr",	"t,i(s)",		MIPS_OP(0x26),			MA_MIPS1,	MO_DELAYRT|MO_IGNORERTD },
	{ "lwr",	"t,(s)",		MIPS_OP(0x26),			MA_MIPS1,	MO_DELAYRT|MO_IGNORERTD },
	{ "lwu",	"t,i(s)",		MIPS_OP(0x27),			MA_MIPS3,	MO_64BIT|MO_DELAYRT },
	{ "lwu",	"t,(s)",		MIPS_OP(0x27),			MA_MIPS3,	MO_64BIT|MO_DELAYRT },
	{ "sb",		"t,i(s)",		MIPS_OP(0x28),			MA_MIPS1,	0 },
	{ "sb",		"t,(s)",		MIPS_OP(0x28),			MA_MIPS1,	0 },
	{ "sh",		"t,i(s)",		MIPS_OP(0x29),			MA_MIPS1,	0 },
	{ "sh",		"t,(s)",		MIPS_OP(0x29),			MA_MIPS1,	0 },
	{ "swl",	"t,i(s)",		MIPS_OP(0x2A),			MA_MIPS1,	0 },
	{ "swl",	"t,(s)",		MIPS_OP(0x2A),			MA_MIPS1,	0 },
	{ "sw",		"t,i(s)",		MIPS_OP(0x2B),			MA_MIPS1,	0 },
	{ "sw",		"t,(s)",		MIPS_OP(0x2B),			MA_MIPS1,	0 },
	{ "sdl",	"t,i(s)",		MIPS_OP(0x2C),			MA_MIPS3,	MO_64BIT|MO_DELAYRT },
	{ "sdl",	"t,(s)",		MIPS_OP(0x2C),			MA_MIPS3,	MO_64BIT|MO_DELAYRT },
	{ "sdr",	"t,i(s)",		MIPS_OP(0x2D),			MA_MIPS3,	MO_64BIT|MO_DELAYRT|MO_IGNORERTD },
	{ "sdr",	"t,(s)",		MIPS_OP(0x2D),			MA_MIPS3,	MO_64BIT|MO_DELAYRT|MO_IGNORERTD },
	{ "swr",	"t,i(s)",		MIPS_OP(0x2E),			MA_MIPS1,	0 },
	{ "swr",	"t,(s)",		MIPS_OP(0x2E),			MA_MIPS1,	0 },
	{ "cache",	"t,i(s)",		MIPS_OP(0x2F),			MA_PS2,		0 },
	{ "ll",		"t,i(s)",		MIPS_OP(0x30),			MA_MIPS2,	MO_DELAYRT },
	{ "ll",		"t,(s)",		MIPS_OP(0x30),			MA_MIPS2,	MO_DELAYRT },
	{ "lwc1",	"T,i(s)",		MIPS_OP(0x31),			MA_MIPS1,	0 },
	{ "lwc1",	"T,(s)",		MIPS_OP(0x31),			MA_MIPS1,	0 },
	{ "lv.s",	"vt,i(s)",		MIPS_OP(0x32),			MA_PSP,		MO_VFPU_SINGLE|MO_VFPU_MIXED|MO_IMMALIGNED },
	{ "lv.s",	"vt,(s)",		MIPS_OP(0x32),			MA_PSP,		MO_VFPU_SINGLE|MO_VFPU_MIXED },
	{ "lld",	"t,i(s)",		MIPS_OP(0x34),			MA_MIPS3,	MO_64BIT|MO_DELAYRT },
	{ "lld",	"t,(s)",		MIPS_OP(0x34),			MA_MIPS3,	MO_64BIT|MO_DELAYRT },
	{ "ulv.q",	"vt,i(s)",		MIPS_OP(0x35),			MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED|MO_IMMALIGNED },
	{ "ulv.q",	"vt,(s)",		MIPS_OP(0x35),			MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED },
	{ "lvl.q",	"vt,i(s)",		MIPS_OP(0x35),			MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED|MO_VFPU_6BIT|MO_IMMALIGNED },
	{ "lvl.q",	"vt,(s)",		MIPS_OP(0x35),			MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED|MO_VFPU_6BIT },
	{ "lvr.q",	"vt,i(s)",		MIPS_OP(0x35)|0x02,		MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED|MO_VFPU_6BIT|MO_IMMALIGNED },
	{ "lvr.q",	"vt,(s)",		MIPS_OP(0x35)|0x02,		MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED|MO_VFPU_6BIT },
	{ "lv.q",	"vt,i(s)",		MIPS_OP(0x36),			MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED|MO_VFPU_6BIT|MO_IMMALIGNED },
	{ "lv.q",	"vt,(s)",		MIPS_OP(0x36),			MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED|MO_VFPU_6BIT },
	{ "lqc2",	"Vt,i(s)",		MIPS_OP(0x36),			MA_PS2,		MO_DELAYRT },
	{ "ld",		"t,i(s)",		MIPS_OP(0x37),			MA_MIPS3,	MO_64BIT|MO_DELAYRT },
	{ "ld",		"t,(s)",		MIPS_OP(0x37),			MA_MIPS3,	MO_64BIT|MO_DELAYRT },
	{ "sc",		"t,i(s)",		MIPS_OP(0x38),			MA_MIPS2,	0 },
	{ "sc",		"t,(s)",		MIPS_OP(0x38),			MA_MIPS2,	0 },
	{ "swc1",	"T,i(s)",		MIPS_OP(0x39),			MA_MIPS1,	0 },
	{ "swc1",	"T,(s)",		MIPS_OP(0x39),			MA_MIPS1,	0 },
	{ "sv.s",	"vt,i(s)",		MIPS_OP(0x3A),			MA_PSP,		MO_VFPU_SINGLE|MO_VFPU_MIXED|MO_IMMALIGNED },
	{ "sv.s",	"vt,(s)",		MIPS_OP(0x3A),			MA_PSP,		MO_VFPU_SINGLE|MO_VFPU_MIXED },
	{ "scd",	"t,i(s)",		MIPS_OP(0x3C),			MA_MIPS3,	MO_64BIT|MO_DELAYRT },
	{ "scd",	"t,(s)",		MIPS_OP(0x3C),			MA_MIPS3,	MO_64BIT|MO_DELAYRT },
	{ "usv.q",	"vt,i(s)",		MIPS_OP(0x3D),			MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED|MO_IMMALIGNED },
	{ "usv.q",	"vt,(s)",		MIPS_OP(0x3D),			MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED },
	{ "svl.q",	"vt,i(s)",		MIPS_OP(0x3D),			MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED|MO_VFPU_6BIT|MO_IMMALIGNED },
	{ "svl.q",	"vt,(s)",		MIPS_OP(0x3D),			MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED|MO_VFPU_6BIT },
	{ "svr.q",	"vt,i(s)",		MIPS_OP(0x3D)|0x02,		MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED|MO_VFPU_6BIT|MO_IMMALIGNED },
	{ "svr.q",	"vt,(s)",		MIPS_OP(0x3D)|0x02,		MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED|MO_VFPU_6BIT },
	{ "sv.q",	"vt,i(s)",		MIPS_OP(0x3E),			MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED|MO_VFPU_6BIT|MO_IMMALIGNED },
	{ "sv.q",	"vt,(s)",		MIPS_OP(0x3E),			MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED|MO_VFPU_6BIT },
	{ "sv.q",	"vt,i(s),/w/b",	MIPS_OP(0x3E)|0x02,		MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED|MO_VFPU_6BIT|MO_IMMALIGNED },
	{ "sv.q",	"vt,(s),/w/b",	MIPS_OP(0x3E)|0x02,		MA_PSP,		MO_VFPU_QUAD|MO_VFPU_MIXED|MO_VFPU_6BIT },
	{ "sqc2",	"Vt,i(s)",		MIPS_OP(0x3E),			MA_PS2,		MO_DELAYRT },
	{ "sd",		"t,i(s)",		MIPS_OP(0x3F),			MA_MIPS3,	MO_64BIT|MO_DELAYRT },
	{ "sd",		"t,(s)",		MIPS_OP(0x3F),			MA_MIPS3,	MO_64BIT|MO_DELAYRT },

//     31---------26------------------------------------------5--------0
//     |=   SPECIAL|                                         | function|
//     ------6----------------------------------------------------6-----
//     |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
// 000 | SLL   | ---   | SRL*1 | SRA   | SLLV  |  ---  | SRLV*2| SRAV  | 00..07
// 001 | JR    | JALR  | MOVZ  | MOVN  |SYSCALL| BREAK |  ---  | SYNC  | 08..0F
// 010 | MFHI  | MTHI  | MFLO  | MTLO  | DSLLV |  ---  |   *3  |  *4   | 10..17
// 011 | MULT  | MULTU | DIV   | DIVU  | MADD  | MADDU | ----  | ----- | 18..1F
// 100 | ADD   | ADDU  | SUB   | SUBU  | AND   | OR    | XOR   | NOR   | 20..27
// 101 | mfsa  | mtsa  | SLT   | SLTU  |  *5   |  *6   |  *7   |  *8   | 28..2F
// 110 | TGE   | TGEU  | TLT   | TLTU  | TEQ   |  ---  | TNE   |  ---  | 30..37
// 111 | dsll  |  ---  | dsrl  | dsra  |dsll32 |  ---  |dsrl32 |dsra32 | 38..3F
//  hi |-------|-------|-------|-------|-------|-------|-------|-------|
// *1:	rotr when rs = 1 (PSP only)		*2:	rotrv when sa = 1 (PSP only)
// *3:	dsrlv on PS2, clz on PSP		*4:	dsrav on PS2, clo on PSP
// *5:	dadd on PS2, max on PSP			*6:	daddu on PS2, min on PSP
// *7:	dsub on PS2, msub on PSP		*8:	dsubu on PS2, msubu on PSP
	{ "sll",	"d,t,a",	MIPS_SPECIAL(0x00),				MA_MIPS1,	0 },
	{ "sll",	"d,a",		MIPS_SPECIAL(0x00),				MA_MIPS1,	MO_RDT },
	{ "nop",	"",			MIPS_SPECIAL(0x00),				MA_MIPS1,	0 },
	{ "srl",	"d,t,a",	MIPS_SPECIAL(0x02),				MA_MIPS1,	0 },
	{ "srl",	"d,a",		MIPS_SPECIAL(0x02),				MA_MIPS1,	MO_RDT },
	{ "rotr",	"d,t,a",	MIPS_SPECIAL(0x02)|MIPS_RS(1),	MA_PSP,		0 },
	{ "rotr",	"d,a",		MIPS_SPECIAL(0x02)|MIPS_RS(1),	MA_PSP,		MO_RDT },
	{ "sra",	"d,t,a",	MIPS_SPECIAL(0x03),				MA_MIPS1,	0 },
	{ "sra",	"d,a",		MIPS_SPECIAL(0x03),				MA_MIPS1,	MO_RDT },
	{ "sllv",	"d,t,s",	MIPS_SPECIAL(0x04),				MA_MIPS1,	0 },
	{ "sllv",	"d,s",		MIPS_SPECIAL(0x04),				MA_MIPS1,	MO_RDT },
	{ "srlv",	"d,t,s",	MIPS_SPECIAL(0x06),				MA_MIPS1,	0 },
	{ "srlv",	"d,s",		MIPS_SPECIAL(0x06),				MA_MIPS1,	MO_RDT },
	{ "rotrv",	"d,t,s",	MIPS_SPECIAL(0x06)|MIPS_SA(1),	MA_PSP,		0 },
	{ "rotrv",	"d,s",		MIPS_SPECIAL(0x06)|MIPS_SA(1),	MA_PSP,		MO_RDT },
	{ "srav",	"d,t,s",	MIPS_SPECIAL(0x07),				MA_MIPS1,	0 },
	{ "srav",	"d,s",		MIPS_SPECIAL(0x07),				MA_MIPS1,	MO_RDT },
	{ "jr",		"s",		MIPS_SPECIAL(0x08),				MA_MIPS1,	MO_DELAY|MO_NODELAYSLOT },
	{ "jalr",	"s,d",		MIPS_SPECIAL(0x09),				MA_MIPS1,	MO_DELAY|MO_NODELAYSLOT },
	{ "jalr",	"s",		MIPS_SPECIAL(0x09)|MIPS_RD(31),	MA_MIPS1,	MO_DELAY|MO_NODELAYSLOT },
	{ "movz",	"d,s,t",	MIPS_SPECIAL(0x0A),				MA_MIPS4|MA_PS2|MA_PSP,	0 },
	{ "movn",	"d,s,t",	MIPS_SPECIAL(0x0B),				MA_MIPS4|MA_PS2|MA_PSP,	0 },
	{ "syscall","b",		MIPS_SPECIAL(0x0C),				MA_MIPS1,	MO_NODELAYSLOT },
	{ "break",	"b",		MIPS_SPECIAL(0x0D),				MA_MIPS1,	MO_NODELAYSLOT },
	{ "sync",	"",			MIPS_SPECIAL(0x0F),				MA_MIPS2,	0 },
	{ "mfhi",	"d",		MIPS_SPECIAL(0x10),				MA_MIPS1,	0 },
	{ "mthi",	"s",		MIPS_SPECIAL(0x11),				MA_MIPS1,	0 },
	{ "mflo",	"d",		MIPS_SPECIAL(0x12),				MA_MIPS1,	0 },
	{ "mtlo",	"s",		MIPS_SPECIAL(0x13),				MA_MIPS1,	0 },
	{ "dsllv",	"d,t,s",	MIPS_SPECIAL(0x14),				MA_MIPS3,	MO_64BIT },
	{ "dsllv",	"d,s",		MIPS_SPECIAL(0x14),				MA_MIPS3,	MO_64BIT },
	{ "dsrlv",	"d,t,s",	MIPS_SPECIAL(0x16),				MA_MIPS3,	MO_64BIT },
	{ "dsrlv",	"d,s",		MIPS_SPECIAL(0x16),				MA_MIPS3,	MO_64BIT|MO_RDT },
	{ "clz",	"d,s",		MIPS_SPECIAL(0x16),				MA_PSP,		0 },
	{ "dsrav",	"d,t,s",	MIPS_SPECIAL(0x17),				MA_MIPS3,	MO_64BIT },
	{ "dsrav",	"d,s",		MIPS_SPECIAL(0x17),				MA_MIPS3,	MO_64BIT|MO_RDT },
	{ "clo",	"d,s",		MIPS_SPECIAL(0x17),				MA_PSP,		0 },
	{ "mult",	"s,t",		MIPS_SPECIAL(0x18),				MA_MIPS1,	0 },
	{ "mult",	"r\x0,s,t",	MIPS_SPECIAL(0x18),				MA_MIPS1,	0 },
	{ "multu",	"s,t",		MIPS_SPECIAL(0x19),				MA_MIPS1,	0 },
	{ "multu",	"r\x0,s,t",	MIPS_SPECIAL(0x19),				MA_MIPS1,	0 },
	{ "div",	"s,t",		MIPS_SPECIAL(0x1A),				MA_MIPS1,	0 },
	{ "div",	"r\x0,s,t",	MIPS_SPECIAL(0x1A),				MA_MIPS1,	0 },
	{ "divu",	"s,t",		MIPS_SPECIAL(0x1B),				MA_MIPS1,	0 },
	{ "divu",	"r\x0,s,t",	MIPS_SPECIAL(0x1B),				MA_MIPS1,	0 },
	{ "dmult",	"s,t",		MIPS_SPECIAL(0x1C),				MA_MIPS3|MA_EXPS2,	MO_64BIT },
	{ "dmult",	"r\x0,s,t",	MIPS_SPECIAL(0x1C),				MA_MIPS3|MA_EXPS2,	MO_64BIT },
	{ "madd",	"s,t",		MIPS_SPECIAL(0x1C),				MA_PSP,		0 },
	{ "dmultu",	"s,t",		MIPS_SPECIAL(0x1D),				MA_MIPS3|MA_EXPS2,	MO_64BIT },
	{ "dmultu",	"r\x0,s,t",	MIPS_SPECIAL(0x1D),				MA_MIPS3|MA_EXPS2,	MO_64BIT },
	{ "maddu",	"s,t",		MIPS_SPECIAL(0x1D),				MA_PSP,		0 },
	{ "ddiv",	"s,t",		MIPS_SPECIAL(0x1E),				MA_MIPS3|MA_EXPS2,	MO_64BIT },
	{ "ddiv",	"r\x0,s,t",	MIPS_SPECIAL(0x1E),				MA_MIPS3|MA_EXPS2,	MO_64BIT },
	{ "ddivu",	"s,t",		MIPS_SPECIAL(0x1F),				MA_MIPS3|MA_EXPS2,	MO_64BIT },
	{ "ddivu",	"r\x0,s,t",	MIPS_SPECIAL(0x1F),				MA_MIPS3|MA_EXPS2,	MO_64BIT },
	{ "add",	"d,s,t",	MIPS_SPECIAL(0x20),				MA_MIPS1,	0 },
	{ "add",	"s,t",		MIPS_SPECIAL(0x20),				MA_MIPS1,	MO_RSD },
	{ "addu",	"d,s,t",	MIPS_SPECIAL(0x21),				MA_MIPS1,	0 },
	{ "addu",	"s,t",		MIPS_SPECIAL(0x21),				MA_MIPS1,	MO_RSD },
	{ "move",	"d,s",		MIPS_SPECIAL(0x21),				MA_MIPS1,	0 },
	{ "sub",	"d,s,t",	MIPS_SPECIAL(0x22),				MA_MIPS1,	0 },
	{ "sub",	"s,t",		MIPS_SPECIAL(0x22),				MA_MIPS1,	MO_RSD },
	{ "neg",	"d,t",		MIPS_SPECIAL(0x22),				MA_MIPS1,	0 },
	{ "subu",	"d,s,t",	MIPS_SPECIAL(0x23),				MA_MIPS1,	0 },
	{ "subu",	"s,t",		MIPS_SPECIAL(0x23),				MA_MIPS1,	MO_RSD },
	{ "negu",	"d,t",		MIPS_SPECIAL(0x23),				MA_MIPS1,	0 },
	{ "and",	"d,s,t",	MIPS_SPECIAL(0x24),				MA_MIPS1,	0 },
	{ "and",	"s,t",		MIPS_SPECIAL(0x24),				MA_MIPS1,	MO_RSD },
	{ "or",		"d,s,t",	MIPS_SPECIAL(0x25),				MA_MIPS1,	0 },
	{ "or",		"s,t",		MIPS_SPECIAL(0x25),				MA_MIPS1,	MO_RSD },
	{ "xor",	"d,s,t",	MIPS_SPECIAL(0x26), 			MA_MIPS1,	0 },
	{ "eor",	"d,s,t",	MIPS_SPECIAL(0x26),				MA_MIPS1,	0 },
	{ "xor",	"s,t",		MIPS_SPECIAL(0x26), 			MA_MIPS1,	MO_RSD },
	{ "eor",	"s,t",		MIPS_SPECIAL(0x26), 			MA_MIPS1,	MO_RSD },
	{ "nor",	"d,s,t",	MIPS_SPECIAL(0x27),				MA_MIPS1,	0 },
	{ "nor",	"s,t",		MIPS_SPECIAL(0x27),				MA_MIPS1,	MO_RSD },
	{ "mfsa",	"d",		MIPS_SPECIAL(0x28),				MA_PS2,		0 },
	{ "mtsa",	"s",		MIPS_SPECIAL(0x29),				MA_PS2,		0 },
	{ "slt",	"d,s,t",	MIPS_SPECIAL(0x2A),				MA_MIPS1,	0 },
	{ "slt",	"s,t",		MIPS_SPECIAL(0x2A),				MA_MIPS1,	MO_RSD},
	{ "sltu",	"d,s,t",	MIPS_SPECIAL(0x2B),				MA_MIPS1,	0 },
	{ "sltu",	"s,t",		MIPS_SPECIAL(0x2B),				MA_MIPS1,	MO_RSD },
	{ "dadd",	"d,s,t",	MIPS_SPECIAL(0x2C),				MA_MIPS3,	MO_64BIT },
	{ "max",	"d,s,t",	MIPS_SPECIAL(0x2C),				MA_PSP,		0 },
	{ "daddu",	"d,s,t",	MIPS_SPECIAL(0x2D), 			MA_MIPS3,	MO_64BIT },
	{ "dmove",	"d,s",		MIPS_SPECIAL(0x2D), 			MA_MIPS3,	MO_64BIT },
	{ "min",	"d,s,t",	MIPS_SPECIAL(0x2D), 			MA_PSP,		0 },
	{ "dsub",	"d,s,t",	MIPS_SPECIAL(0x2E), 			MA_MIPS3,	MO_64BIT },
	{ "msub",	"s,t",		MIPS_SPECIAL(0x2E),				MA_PSP,		0 },
	{ "dsubu",	"d,s,t",	MIPS_SPECIAL(0x2F), 			MA_MIPS3,	MO_64BIT },
	{ "msubu",	"s,t",		MIPS_SPECIAL(0x2F),				MA_PSP,		0 },
	{ "tge",	"s,t",		MIPS_SPECIAL(0x30),				MA_MIPS2,	MO_RSD },
	{ "tgeu",	"s,t",		MIPS_SPECIAL(0x31),				MA_MIPS2,	MO_RSD },
	{ "tlt",	"s,t",		MIPS_SPECIAL(0x32),				MA_MIPS2,	MO_RSD },
	{ "tltu",	"s,t",		MIPS_SPECIAL(0x33),				MA_MIPS2,	MO_RSD },
	{ "teq",	"s,t",		MIPS_SPECIAL(0x34),				MA_MIPS2,	MO_RSD },
	{ "tne",	"s,t",		MIPS_SPECIAL(0x36),				MA_MIPS2,	MO_RSD },
	{ "dsll",	"d,t,a",	MIPS_SPECIAL(0x38),				MA_MIPS3,	MO_64BIT },
	{ "dsll",	"d,a",		MIPS_SPECIAL(0x38),				MA_MIPS3,	MO_64BIT|MO_RDT },
	{ "dsrl",	"d,t,a",	MIPS_SPECIAL(0x3A),				MA_MIPS3,	MO_64BIT },
	{ "dsrl",	"d,a",		MIPS_SPECIAL(0x3A),				MA_MIPS3,	MO_64BIT|MO_RDT },
	{ "dsra",	"d,t,a",	MIPS_SPECIAL(0x3B),				MA_MIPS3,	MO_64BIT },
	{ "dsra",	"d,a",		MIPS_SPECIAL(0x3B),				MA_MIPS3,	MO_64BIT|MO_RDT },
	{ "dsll32",	"d,t,a",	MIPS_SPECIAL(0x3C),				MA_MIPS3,	MO_64BIT },
	{ "dsll32",	"d,a",		MIPS_SPECIAL(0x3C),				MA_MIPS3,	MO_64BIT|MO_RDT },
	{ "dsrl32",	"d,t,a",	MIPS_SPECIAL(0x3E),				MA_MIPS3,	MO_64BIT },
	{ "dsrl32",	"d,a",		MIPS_SPECIAL(0x3E),				MA_MIPS3,	MO_64BIT|MO_RDT },
	{ "dsra32",	"d,t,a",	MIPS_SPECIAL(0x3F),				MA_MIPS3,	MO_64BIT },
	{ "dsra32",	"d,a",		MIPS_SPECIAL(0x3F),				MA_MIPS3,	MO_64BIT|MO_RDT },

//     REGIMM: encoded by the rt field when opcode field = REGIMM.
//     31---------26----------20-------16------------------------------0
//     |=    REGIMM|          |   rt    |                              |
//     ------6---------------------5------------------------------------
//     |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
//  00 | BLTZ  | BGEZ  | BLTZL | BGEZL |  ---  |  ---  |  ---  |  ---  | 00-07
//  01 | tgei  | tgeiu | tlti  | tltiu | teqi  |  ---  | tnei  |  ---  | 08-0F
//  10 | BLTZAL| BGEZAL|BLTZALL|BGEZALL|  ---  |  ---  |  ---  |  ---  | 10-17
//  11 | mtsab | mtsah |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | 18-1F
//  hi |-------|-------|-------|-------|-------|-------|-------|-------|
	{ "bltz",	"s,i",		MIPS_REGIMM(0x00),				MA_MIPS1,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "bgez",	"s,i",		MIPS_REGIMM(0x01),				MA_MIPS1,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "bltzl",	"s,i",		MIPS_REGIMM(0x02),				MA_MIPS2,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "bgezl",	"s,i",		MIPS_REGIMM(0x03),				MA_MIPS2,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "tgei",	"s,i",		MIPS_REGIMM(0x08),				MA_MIPS2,	0 },
	{ "tgeiu",	"s,i",		MIPS_REGIMM(0x09),				MA_MIPS2,	0 },
	{ "tlti",	"s,i",		MIPS_REGIMM(0x0A),				MA_MIPS2,	0 },
	{ "tltiu",	"s,i",		MIPS_REGIMM(0x0B),				MA_MIPS2,	0 },
	{ "teqi",	"s,i",		MIPS_REGIMM(0x0C),				MA_MIPS2,	0 },
	{ "tnei",	"s,i",		MIPS_REGIMM(0x0E),				MA_MIPS2,	0 },
	{ "bltzal",	"s,i",		MIPS_REGIMM(0x10),				MA_MIPS1,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "bgezal",	"s,i",		MIPS_REGIMM(0x11),				MA_MIPS1,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "bltzall","s,i",		MIPS_REGIMM(0x12),				MA_MIPS2,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "bgezall","s,i",		MIPS_REGIMM(0x13),				MA_MIPS2,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "mtsab",	"s,i",		MIPS_REGIMM(0x18),				MA_PS2,	0 },
	{ "mtsah",	"s,i",		MIPS_REGIMM(0x19),				MA_PS2,	0 },

//     31-------26------21---------------------------------------------0
//     |=    COP1|  rs  |                                              |
//     -----6-------5---------------------------------------------------
//     |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
//  00 |  MFC1 |  ---  |  CFC1 |  ---  |  MTC1 |  ---  |  CTC1 |  ---  | 00..07
//  01 |  BC*  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | 08..0F
//  10 |  S*   |  ---  |  ---  |  ---  |  W*   |  ---  |  ---  |  ---  | 10..17
//  11 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | 18..1F
//  hi |-------|-------|-------|-------|-------|-------|-------|-------|
	{ "mfc1",	"t,S",		MIPS_COP1(0x00),				MA_MIPS2,	0 },
	{ "cfc1",	"t,S",		MIPS_COP1(0x02),				MA_MIPS2,	0 },
	{ "mtc1",	"t,S",		MIPS_COP1(0x04),				MA_MIPS2,	0 },
	{ "ctc1",	"t,S",		MIPS_COP1(0x06),				MA_MIPS2,	0 },

//     31---------21-------16------------------------------------------0
//     |=    COP1BC|  rt   |                                           |
//     ------11---------5-----------------------------------------------
//     |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
//  00 |  BC1F | BC1T  | BC1FL | BC1TL |  ---  |  ---  |  ---  |  ---  | 00..07
//  01 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | 08..0F
//  10 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | 10..17
//  11 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | 18..1F
//  hi |-------|-------|-------|-------|-------|-------|-------|-------|
	{ "bc1f",	"I",		MIPS_COP1BC(0x00),				MA_MIPS2,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "bc1t",	"I",		MIPS_COP1BC(0x01),				MA_MIPS2,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "bc1fl",	"I",		MIPS_COP1BC(0x02),				MA_MIPS2,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },
	{ "bc1tl",	"I",		MIPS_COP1BC(0x03),				MA_MIPS2,	MO_IPCR|MO_DELAY|MO_NODELAYSLOT },

//     31---------21------------------------------------------5--------0
//     |=  COP1S  |                                          | function|
//     -----11----------------------------------------------------6-----
//     |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
// 000 |  add  |  sub  |  mul  |  div  | sqrt  |  abs  |  mov  |  neg  | 00..07
// 001 |  ---  |  ---  |  ---  |  ---  |round.w|trunc.w|ceil.w |floor.w| 08..0F
// 010 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | rsqrt |  ---  | 10..17
// 011 |  adda |  suba | mula  |  ---  | madd  |  msub | madda | msuba | 18..1F
// 100 |  ---  |  ---  |  ---  |  ---  | cvt.w |  ---  |  ---  |  ---  | 20..27
// 101 |  max  |  min  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | 28..2F
// 110 |  c.f  | c.un  | c.eq  | c.ueq |c.(o)lt| c.ult |c.(o)le| c.ule | 30..37
// 110 |  c.sf | c.ngle| c.seq | c.ngl | c.lt  | c.nge | c.le  | c.ngt | 38..3F
//  hi |-------|-------|-------|-------|-------|-------|-------|-------|
	{ "add.s",		"D,S,T",	MIPS_COP1S(0x00),			MA_MIPS2,	0 },
	{ "add.s",		"S,T",		MIPS_COP1S(0x00),			MA_MIPS2,	MO_FRSD },
	{ "sub.s",		"D,S,T",	MIPS_COP1S(0x01),			MA_MIPS2,	0 },
	{ "sub.s",		"S,T",		MIPS_COP1S(0x01),			MA_MIPS2,	MO_FRSD },
	{ "mul.s",		"D,S,T",	MIPS_COP1S(0x02),			MA_MIPS2,	0 },
	{ "mul.s",		"S,T",		MIPS_COP1S(0x02),			MA_MIPS2,	MO_FRSD },
	{ "div.s",		"D,S,T",	MIPS_COP1S(0x03),			MA_MIPS2,	0 },
	{ "div.s",		"S,T",		MIPS_COP1S(0x03),			MA_MIPS2,	MO_FRSD },
	{ "sqrt.s",		"D,S",		MIPS_COP1S(0x04),			MA_MIPS2,	0 },
	{ "abs.s",		"D,S",		MIPS_COP1S(0x05),			MA_MIPS2,	0 },
	{ "mov.s",		"D,S",		MIPS_COP1S(0x06),			MA_MIPS2,	0 },
	{ "neg.s",		"D,S",		MIPS_COP1S(0x07),			MA_MIPS2,	0 },
	{ "round.w.s",	"D,S",		MIPS_COP1S(0x0C),			MA_PSP,	0 },
	{ "trunc.w.s",	"D,S",		MIPS_COP1S(0x0D),			MA_PSP,	0 },
	{ "ceil.w.s",	"D,S",		MIPS_COP1S(0x0E),			MA_PSP,	0 },
	{ "floor.w.s",	"D,S",		MIPS_COP1S(0x0F),			MA_PSP,	0 },
	{ "rsqrt.w.s",	"D,S",		MIPS_COP1S(0x16),			MA_PS2,	0 },
	{ "adda.s",		"S,T",		MIPS_COP1S(0x18),			MA_PS2,	0 },
	{ "suba.s",		"S,T",		MIPS_COP1S(0x19),			MA_PS2,	0 },
	{ "mula.s",		"S,T",		MIPS_COP1S(0x1A),			MA_PS2,	0 },
	{ "madd.s",		"D,S,T",	MIPS_COP1S(0x1C),			MA_PS2,	0 },
	{ "madd.s",		"S,T",		MIPS_COP1S(0x1C),			MA_PS2,	MO_FRSD },
	{ "msub.s",		"D,S,T",	MIPS_COP1S(0x1D),			MA_PS2,	0 },
	{ "msub.s",		"S,T",		MIPS_COP1S(0x1D),			MA_PS2,	MO_FRSD },
	{ "madda.s",	"S,T",		MIPS_COP1S(0x1E),			MA_PS2,	0 },
	{ "msuba.s",	"S,T",		MIPS_COP1S(0x1F),			MA_PS2,	0 },
	{ "cvt.w.s",	"D,S",		MIPS_COP1S(0x24),			MA_MIPS2,	0 },
	{ "max.s",		"D,S,T",	MIPS_COP1S(0x28),			MA_PS2,	0 },
	{ "min.s",		"D,S,T",	MIPS_COP1S(0x29),			MA_PS2,	0 },
	{ "c.f.s",		"S,T",		MIPS_COP1S(0x30),			MA_MIPS2,	0 },
	{ "c.un.s",		"S,T",		MIPS_COP1S(0x31),			MA_PSP,	0 },
	{ "c.eq.s",		"S,T",		MIPS_COP1S(0x32),			MA_MIPS2,	0 },
	{ "c.ueq.s",	"S,T",		MIPS_COP1S(0x33),			MA_PSP,	0 },
	{ "c.olt.s",	"S,T",		MIPS_COP1S(0x34),			MA_PSP,	0 },
	{ "c.lt.s",		"S,T",		MIPS_COP1S(0x34),			MA_PS2,	0 },
	{ "c.ult.s",	"S,T",		MIPS_COP1S(0x35),			MA_PSP,	0 },
	{ "c.ole.s",	"S,T",		MIPS_COP1S(0x36),			MA_PSP,	0 },
	{ "c.le.s",		"S,T",		MIPS_COP1S(0x36),			MA_PS2,	0 },
	{ "c.ule.s",	"S,T",		MIPS_COP1S(0x37),			MA_PSP,	0 },
	{ "c.sf.s",		"S,T",		MIPS_COP1S(0x38),			MA_PSP,	0 },
	{ "c.ngle.s",	"S,T",		MIPS_COP1S(0x39),			MA_PSP,	0 },
	{ "c.seq.s",	"S,T",		MIPS_COP1S(0x3A),			MA_PSP,	0 },
	{ "c.ngl.s",	"S,T",		MIPS_COP1S(0x3B),			MA_PSP,	0 },
	{ "c.lt.s",		"S,T",		MIPS_COP1S(0x3C),			MA_PSP,	0 },
	{ "c.nge.s",	"S,T",		MIPS_COP1S(0x3D),			MA_PSP,	0 },
	{ "c.le.s",		"S,T",		MIPS_COP1S(0x3E),			MA_PSP,	0 },
	{ "c.ngt.s",	"S,T",		MIPS_COP1S(0x3F),			MA_PSP,	0 },

//     COP1W: encoded by function field
//     31---------21------------------------------------------5--------0
//     |=  COP1W  |                                          | function|
//     -----11----------------------------------------------------6-----
//     |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--| lo
// 000 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | 00..07
// 001 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | 08..0F
// 010 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | 10..17
// 011 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | 18..1F
// 100 |cvt.s.w|  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | 20..27
// 101 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | 28..2F
// 110 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | 30..37
// 110 |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  |  ---  | 38..3F
//  hi |-------|-------|-------|-------|-------|-------|-------|-------|
	{ "cvt.s.w",	"D,S",		MIPS_COP1W(0x20),			MA_MIPS2,	0 },

//     31---------26-----23--------------------------------------------0
//     |= VFPU0| VOP | |
//     ------6--------3-------------------------------------------------
//     |--000--|--001--|--010--|--011--|--100--|--101--|--110--|--111--|
// 000 | VADD  | VSUB  | VSBN  | ---   | ---   | ---   | ---   | VDIV  | 00..07
//  hi |-------|-------|-------|-------|-------|-------|-------|-------|
	{ "vadd.S",		"vd,vs,vt",	MIPS_VFPU0(0x00),			MA_PSP,	MO_VFPU },
	{ "vsub.S",		"vd,vs,vt",	MIPS_VFPU0(0x01),			MA_PSP,	MO_VFPU },
	{ "vsbn.S",		"vd,vs,vt",	MIPS_VFPU0(0x02),			MA_PSP,	MO_VFPU },
	{ "vdiv.S",		"vd,vs,vt",	MIPS_VFPU0(0x07),			MA_PSP,	MO_VFPU },

	// allegrex0
	{ "seh",		"d,t",		MIPS_ALLEGREX0(16),			MA_PSP },
	{ "seh",		"d,t",		MIPS_ALLEGREX0(24),			MA_PSP },

	// END
	{ NULL,		NULL,		0,			0 }
};


const MipsArchDefinition mipsArchs[] = {
	// MARCH_PSX
	{ "PSX",		MA_MIPS1,							MA_EXPSX,	0 },
	// MARCH_N64
	{ "N64",		MA_MIPS1|MA_MIPS2|MA_MIPS3,			MA_EXN64,	MO_FPU },
	// MARCH_PS2
	{ "PS2",		MA_MIPS1|MA_MIPS2|MA_MIPS3|MA_PS2,	MA_EXPS2,	MO_64BIT|MO_FPU },
	// MARCH_PSP
	{ "PSP",		MA_MIPS1|MA_MIPS2|MA_MIPS3|MA_PSP,	MA_EXPSP,	MO_FPU },
	// MARCH_INVALID
	{ "Invalid",	0,									0,			0 },
};
