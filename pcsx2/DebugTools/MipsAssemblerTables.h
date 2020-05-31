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

#pragma once

#define MA_MIPS1			0x0000001
#define MA_MIPS2			0x0000002
#define MA_MIPS3			0x0000004
#define MA_MIPS4			0x0000008
#define MA_PS2				0x0000010
#define MA_PSP				0x0000020

#define MA_EXPSX			0x0000100
#define MA_EXN64			0x0000200
#define MA_EXPS2			0x0000400
#define MA_EXPSP			0x0000800

#define MO_IPCA			0x00000001	// pc >> 2
#define MO_IPCR			0x00000002	// PC, -> difference >> 2
#define MO_RSD			0x00000004	// rs = rd
#define MO_RST			0x00000008	// rs = rt
#define MO_RDT			0x00000010	// rd = rt
#define MO_DELAY		0x00000020	// delay slot follows
#define MO_NODELAYSLOT	0x00000040	// can't be in a delay slot
#define MO_DELAYRT		0x00000080	// rt won't be available for one instruction
#define MO_IGNORERTD	0x00000100	// don't care for rt delay
#define MO_FRSD			0x00000200	// float rs + rd
#define MO_IMMALIGNED	0x00000400	// immediate 4 byte aligned
#define MO_VFPU_MIXED	0x00000800	// mixed mode vfpu register
#define MO_VFPU_6BIT	0x00001000	// vfpu register can have 6 bits max
#define MO_VFPU_SINGLE	0x00002000	// single vfpu reg
#define MO_VFPU_QUAD	0x00004000	// quad vfpu reg
#define MO_VFPU			0x00008000	// vfpu type opcode
#define MO_64BIT		0x00010000	// only available on 64 bit cpus
#define MO_FPU			0x00020000	// only available with an fpu

#define BITFIELD(START,LENGTH,VALUE)	(((VALUE) << (START)))
#define MIPS_FUNC(VALUE)				BITFIELD(0,6,(VALUE))
#define MIPS_SA(VALUE)					BITFIELD(6,5,(VALUE))
#define MIPS_SECFUNC(VALUE)				MIPS_SA((VALUE))
#define MIPS_OP(VALUE)					BITFIELD(26,6,(VALUE))

#define MIPS_RS(VALUE)					BITFIELD(21,5,(VALUE))
#define MIPS_RT(VALUE)					BITFIELD(16,5,(VALUE))
#define MIPS_RD(VALUE)					BITFIELD(11,5,(VALUE))
#define MIPS_FS(VALUE)					MIPS_RD((VALUE))
#define MIPS_FT(VALUE)					MIPS_RT((VALUE))
#define MIPS_FD(VALUE)					MIPS_SA((VALUE))

#define MIPS_SPECIAL(VALUE)				(MIPS_OP(0) | MIPS_FUNC(VALUE))
#define MIPS_REGIMM(VALUE)				(MIPS_OP(1) | MIPS_RT(VALUE))
#define MIPS_COP0(VALUE)				(MIPS_OP(16) | MIPS_RS(VALUE))
#define MIPS_COP1(VALUE)				(MIPS_OP(17) | MIPS_RS(VALUE))
#define MIPS_COP1BC(VALUE)				(MIPS_COP1(8) | MIPS_RT(VALUE))
#define MIPS_COP1S(VALUE)				(MIPS_COP1(16) | MIPS_FUNC(VALUE))
#define MIPS_COP1W(VALUE)				(MIPS_COP1(20) | MIPS_FUNC(VALUE))

#define MIPS_VFPUSIZE(VALUE)			( (((VALUE) & 1) << 7) | (((VALUE) & 2) << 14) )
#define MIPS_VFPU0(VALUE)				(MIPS_OP(0x18) | BITFIELD(23,3,(VALUE)))

#define MIPS_ALLEGREX0(VALUE)			(MIPS_SPECIAL(32) | MIPS_SECFUNC(VALUE))

struct MipsArchDefinition
{
	const char* name;
	int supportSets;
	int excludeMask;
	int flags;
};

extern const MipsArchDefinition mipsArchs[];

typedef struct {
	const char* name;
	const char* encoding;
	int destencoding;
	int archs;
	int flags;
} tMipsOpcode;

extern const tMipsOpcode MipsOpcodes[];
