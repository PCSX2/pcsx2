// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

// Constexpr MIPS I / II / IOP-specific instruction encoders.
// Used by the recompiler test harness to build tiny programs in C++ without
// a cross-toolchain. Matches MIPS-I plus a few MIPS-II additions used by
// the IOP (R3000A variant) and EE (R5900 variant).
namespace mips {

namespace reg {
constexpr u32 zero = 0,  at = 1,  v0 = 2,  v1 = 3;
constexpr u32 a0   = 4,  a1 = 5,  a2 = 6,  a3 = 7;
constexpr u32 t0   = 8,  t1 = 9,  t2 = 10, t3 = 11;
constexpr u32 t4   = 12, t5 = 13, t6 = 14, t7 = 15;
constexpr u32 s0   = 16, s1 = 17, s2 = 18, s3 = 19;
constexpr u32 s4   = 20, s5 = 21, s6 = 22, s7 = 23;
constexpr u32 t8   = 24, t9 = 25, k0 = 26, k1 = 27;
constexpr u32 gp   = 28, sp = 29, s8 = 30, ra = 31;
}

// Low-level formats.
constexpr u32 RType(u32 op, u32 rs, u32 rt, u32 rd, u32 sa, u32 funct)
{
	return (op << 26) | ((rs & 0x1F) << 21) | ((rt & 0x1F) << 16) |
		   ((rd & 0x1F) << 11) | ((sa & 0x1F) << 6) | (funct & 0x3F);
}
constexpr u32 IType(u32 op, u32 rs, u32 rt, u32 imm16)
{
	return (op << 26) | ((rs & 0x1F) << 21) | ((rt & 0x1F) << 16) | (imm16 & 0xFFFF);
}
constexpr u32 JType(u32 op, u32 target_word)
{
	return (op << 26) | (target_word & 0x03FFFFFF);
}

// SPECIAL (op=0) — funct selects.
constexpr u32 SLL (u32 rd, u32 rt, u32 sa)    { return RType(0, 0,  rt, rd, sa, 0x00); }
constexpr u32 SRL (u32 rd, u32 rt, u32 sa)    { return RType(0, 0,  rt, rd, sa, 0x02); }
constexpr u32 SRA (u32 rd, u32 rt, u32 sa)    { return RType(0, 0,  rt, rd, sa, 0x03); }
constexpr u32 SLLV(u32 rd, u32 rt, u32 rs)    { return RType(0, rs, rt, rd, 0,  0x04); }
constexpr u32 SRLV(u32 rd, u32 rt, u32 rs)    { return RType(0, rs, rt, rd, 0,  0x06); }
constexpr u32 SRAV(u32 rd, u32 rt, u32 rs)    { return RType(0, rs, rt, rd, 0,  0x07); }
constexpr u32 JR  (u32 rs)                    { return RType(0, rs, 0,  0,  0,  0x08); }
constexpr u32 JALR(u32 rd, u32 rs)            { return RType(0, rs, 0,  rd, 0,  0x09); }
constexpr u32 SYSCALL_(u32 code = 0)          { return RType(0, 0, 0, 0, 0, 0x0C) | ((code & 0xFFFFF) << 6); }
constexpr u32 BREAK                           = RType(0, 0, 0, 0, 0, 0x0D);
constexpr u32 MFHI(u32 rd)                    { return RType(0, 0,  0,  rd, 0,  0x10); }
constexpr u32 MTHI(u32 rs)                    { return RType(0, rs, 0,  0,  0,  0x11); }
constexpr u32 MFLO(u32 rd)                    { return RType(0, 0,  0,  rd, 0,  0x12); }
constexpr u32 MTLO(u32 rs)                    { return RType(0, rs, 0,  0,  0,  0x13); }
constexpr u32 MULT (u32 rs, u32 rt)           { return RType(0, rs, rt, 0,  0,  0x18); }
constexpr u32 MULTU(u32 rs, u32 rt)           { return RType(0, rs, rt, 0,  0,  0x19); }
constexpr u32 DIV  (u32 rs, u32 rt)           { return RType(0, rs, rt, 0,  0,  0x1A); }
constexpr u32 DIVU (u32 rs, u32 rt)           { return RType(0, rs, rt, 0,  0,  0x1B); }
constexpr u32 ADD (u32 rd, u32 rs, u32 rt)    { return RType(0, rs, rt, rd, 0,  0x20); }
constexpr u32 ADDU(u32 rd, u32 rs, u32 rt)    { return RType(0, rs, rt, rd, 0,  0x21); }
constexpr u32 SUB (u32 rd, u32 rs, u32 rt)    { return RType(0, rs, rt, rd, 0,  0x22); }
constexpr u32 SUBU(u32 rd, u32 rs, u32 rt)    { return RType(0, rs, rt, rd, 0,  0x23); }
constexpr u32 AND (u32 rd, u32 rs, u32 rt)    { return RType(0, rs, rt, rd, 0,  0x24); }
constexpr u32 OR  (u32 rd, u32 rs, u32 rt)    { return RType(0, rs, rt, rd, 0,  0x25); }
constexpr u32 XOR (u32 rd, u32 rs, u32 rt)    { return RType(0, rs, rt, rd, 0,  0x26); }
constexpr u32 NOR (u32 rd, u32 rs, u32 rt)    { return RType(0, rs, rt, rd, 0,  0x27); }
constexpr u32 SLT (u32 rd, u32 rs, u32 rt)    { return RType(0, rs, rt, rd, 0,  0x2A); }
constexpr u32 SLTU(u32 rd, u32 rs, u32 rt)    { return RType(0, rs, rt, rd, 0,  0x2B); }

// REGIMM (op=1) — rt selects sub-op.
constexpr u32 BLTZ  (u32 rs, s16 off) { return IType(0x01, rs, 0x00, (u16)off); }
constexpr u32 BGEZ  (u32 rs, s16 off) { return IType(0x01, rs, 0x01, (u16)off); }
constexpr u32 BLTZAL(u32 rs, s16 off) { return IType(0x01, rs, 0x10, (u16)off); }
constexpr u32 BGEZAL(u32 rs, s16 off) { return IType(0x01, rs, 0x11, (u16)off); }

// I-type arith/logic.
constexpr u32 ADDI (u32 rt, u32 rs, s16 imm)  { return IType(0x08, rs, rt, (u16)imm); }
constexpr u32 ADDIU(u32 rt, u32 rs, s16 imm)  { return IType(0x09, rs, rt, (u16)imm); }
constexpr u32 SLTI (u32 rt, u32 rs, s16 imm)  { return IType(0x0A, rs, rt, (u16)imm); }
constexpr u32 SLTIU(u32 rt, u32 rs, s16 imm)  { return IType(0x0B, rs, rt, (u16)imm); }
constexpr u32 ANDI (u32 rt, u32 rs, u16 imm)  { return IType(0x0C, rs, rt, imm); }
constexpr u32 ORI  (u32 rt, u32 rs, u16 imm)  { return IType(0x0D, rs, rt, imm); }
constexpr u32 XORI (u32 rt, u32 rs, u16 imm)  { return IType(0x0E, rs, rt, imm); }
constexpr u32 LUI  (u32 rt, u16 imm)          { return IType(0x0F, 0,  rt, imm); }

// I-type branches.
constexpr u32 BEQ (u32 rs, u32 rt, s16 off) { return IType(0x04, rs, rt, (u16)off); }
constexpr u32 BNE (u32 rs, u32 rt, s16 off) { return IType(0x05, rs, rt, (u16)off); }
constexpr u32 BLEZ(u32 rs, s16 off)         { return IType(0x06, rs, 0,  (u16)off); }
constexpr u32 BGTZ(u32 rs, s16 off)         { return IType(0x07, rs, 0,  (u16)off); }

// Jumps (absolute, target is byte address).
constexpr u32 J  (u32 target) { return JType(0x02, target >> 2); }
constexpr u32 JAL(u32 target) { return JType(0x03, target >> 2); }

// Loads / stores. `offset` is sign-extended, `base` is the base GPR index.
constexpr u32 LB (u32 rt, s16 off, u32 base) { return IType(0x20, base, rt, (u16)off); }
constexpr u32 LH (u32 rt, s16 off, u32 base) { return IType(0x21, base, rt, (u16)off); }
constexpr u32 LWL(u32 rt, s16 off, u32 base) { return IType(0x22, base, rt, (u16)off); }
constexpr u32 LW (u32 rt, s16 off, u32 base) { return IType(0x23, base, rt, (u16)off); }
constexpr u32 LBU(u32 rt, s16 off, u32 base) { return IType(0x24, base, rt, (u16)off); }
constexpr u32 LHU(u32 rt, s16 off, u32 base) { return IType(0x25, base, rt, (u16)off); }
constexpr u32 LWR(u32 rt, s16 off, u32 base) { return IType(0x26, base, rt, (u16)off); }
constexpr u32 SB (u32 rt, s16 off, u32 base) { return IType(0x28, base, rt, (u16)off); }
constexpr u32 SH (u32 rt, s16 off, u32 base) { return IType(0x29, base, rt, (u16)off); }
constexpr u32 SWL(u32 rt, s16 off, u32 base) { return IType(0x2A, base, rt, (u16)off); }
constexpr u32 SW (u32 rt, s16 off, u32 base) { return IType(0x2B, base, rt, (u16)off); }
constexpr u32 SWR(u32 rt, s16 off, u32 base) { return IType(0x2E, base, rt, (u16)off); }

// COP0 (op=0x10).
constexpr u32 MFC0(u32 rt, u32 rd) { return (0x10u << 26) | (0x00u << 21) | ((rt & 0x1F) << 16) | ((rd & 0x1F) << 11); }
constexpr u32 MTC0(u32 rt, u32 rd) { return (0x10u << 26) | (0x04u << 21) | ((rt & 0x1F) << 16) | ((rd & 0x1F) << 11); }
constexpr u32 RFE  = (0x10u << 26) | (0x10u << 21) | 0x10u;

// COP0 branch on DMAC condition (op=0x10, rs=BC=0x08; rt selects F/T/FL/TL).
constexpr u32 BC0_(u32 brt, s16 ofs) { return (0x10u << 26) | (0x08u << 21) | ((brt & 0x1F) << 16) | (static_cast<u16>(ofs)); }
constexpr u32 BC0F (s16 ofs) { return BC0_(0, ofs); }
constexpr u32 BC0T (s16 ofs) { return BC0_(1, ofs); }
constexpr u32 BC0FL(s16 ofs) { return BC0_(2, ofs); }
constexpr u32 BC0TL(s16 ofs) { return BC0_(3, ofs); }

constexpr u32 NOP = 0u;

// ---------------------------------------------------------------------------
//  EE / R5900 extensions
// ---------------------------------------------------------------------------
// The EE reuses all of the MIPS-I encoders above (ADDU, AND, BEQ, ...) and
// adds the following families. 64-bit variants share MIPS-III encodings;
// MMI is PS2-specific (op=0x1C) with sub-op selection via `funct`/sub-field.
namespace ee {

// ---- 64-bit arith/logic (MIPS-III; SPECIAL, op=0) ----
constexpr u32 DADD  (u32 rd, u32 rs, u32 rt)    { return RType(0, rs, rt, rd, 0, 0x2C); }
constexpr u32 DADDU (u32 rd, u32 rs, u32 rt)    { return RType(0, rs, rt, rd, 0, 0x2D); }
constexpr u32 DSUB  (u32 rd, u32 rs, u32 rt)    { return RType(0, rs, rt, rd, 0, 0x2E); }
constexpr u32 DSUBU (u32 rd, u32 rs, u32 rt)    { return RType(0, rs, rt, rd, 0, 0x2F); }

// 64-bit shifts by immediate (SPECIAL, op=0; sa field)
constexpr u32 DSLL  (u32 rd, u32 rt, u32 sa)    { return RType(0, 0, rt, rd, sa, 0x38); }
constexpr u32 DSRL  (u32 rd, u32 rt, u32 sa)    { return RType(0, 0, rt, rd, sa, 0x3A); }
constexpr u32 DSRA  (u32 rd, u32 rt, u32 sa)    { return RType(0, 0, rt, rd, sa, 0x3B); }
constexpr u32 DSLL32(u32 rd, u32 rt, u32 sa)    { return RType(0, 0, rt, rd, sa, 0x3C); }
constexpr u32 DSRL32(u32 rd, u32 rt, u32 sa)    { return RType(0, 0, rt, rd, sa, 0x3E); }
constexpr u32 DSRA32(u32 rd, u32 rt, u32 sa)    { return RType(0, 0, rt, rd, sa, 0x3F); }

// 64-bit variable shifts (SPECIAL, op=0)
constexpr u32 DSLLV(u32 rd, u32 rt, u32 rs)     { return RType(0, rs, rt, rd, 0, 0x14); }
constexpr u32 DSRLV(u32 rd, u32 rt, u32 rs)     { return RType(0, rs, rt, rd, 0, 0x16); }
constexpr u32 DSRAV(u32 rd, u32 rt, u32 rs)     { return RType(0, rs, rt, rd, 0, 0x17); }

// 64-bit mul/div (SPECIAL)
constexpr u32 DMULT (u32 rs, u32 rt)            { return RType(0, rs, rt, 0, 0, 0x1C); }
constexpr u32 DMULTU(u32 rs, u32 rt)            { return RType(0, rs, rt, 0, 0, 0x1D); }
constexpr u32 DDIV  (u32 rs, u32 rt)            { return RType(0, rs, rt, 0, 0, 0x1E); }
constexpr u32 DDIVU (u32 rs, u32 rt)            { return RType(0, rs, rt, 0, 0, 0x1F); }

// 64-bit I-type (MIPS-III; top-level opcodes)
constexpr u32 DADDI (u32 rt, u32 rs, s16 imm)   { return IType(0x18, rs, rt, (u16)imm); }
constexpr u32 DADDIU(u32 rt, u32 rs, s16 imm)   { return IType(0x19, rs, rt, (u16)imm); }
constexpr u32 LDL   (u32 rt, s16 off, u32 base) { return IType(0x1A, base, rt, (u16)off); }
constexpr u32 LDR   (u32 rt, s16 off, u32 base) { return IType(0x1B, base, rt, (u16)off); }
constexpr u32 LD    (u32 rt, s16 off, u32 base) { return IType(0x37, base, rt, (u16)off); }
constexpr u32 SD    (u32 rt, s16 off, u32 base) { return IType(0x3F, base, rt, (u16)off); }
constexpr u32 SDL   (u32 rt, s16 off, u32 base) { return IType(0x2C, base, rt, (u16)off); }
constexpr u32 SDR   (u32 rt, s16 off, u32 base) { return IType(0x2D, base, rt, (u16)off); }
constexpr u32 LWU   (u32 rt, s16 off, u32 base) { return IType(0x27, base, rt, (u16)off); }

// 128-bit quad load/store (PS2-specific)
constexpr u32 LQ    (u32 rt, s16 off, u32 base) { return IType(0x1E, base, rt, (u16)off); }
constexpr u32 SQ    (u32 rt, s16 off, u32 base) { return IType(0x1F, base, rt, (u16)off); }

// Likely branches (MIPS-II/III additions; EE has these)
constexpr u32 BEQL  (u32 rs, u32 rt, s16 off) { return IType(0x14, rs, rt, (u16)off); }
constexpr u32 BNEL  (u32 rs, u32 rt, s16 off) { return IType(0x15, rs, rt, (u16)off); }
constexpr u32 BLEZL (u32 rs, s16 off)         { return IType(0x16, rs, 0,  (u16)off); }
constexpr u32 BGTZL (u32 rs, s16 off)         { return IType(0x17, rs, 0,  (u16)off); }
constexpr u32 BLTZL (u32 rs, s16 off) { return IType(0x01, rs, 0x02, (u16)off); }
constexpr u32 BGEZL (u32 rs, s16 off) { return IType(0x01, rs, 0x03, (u16)off); }

// MOVZ / MOVN (MIPS-IV; EE has them)
constexpr u32 MOVZ(u32 rd, u32 rs, u32 rt) { return RType(0, rs, rt, rd, 0, 0x0A); }
constexpr u32 MOVN(u32 rd, u32 rs, u32 rt) { return RType(0, rs, rt, rd, 0, 0x0B); }

// MFHI1 / MTHI1 / MFLO1 / MTLO1 use MMI1 encodings
// MULT1 / DIV1 etc. — MMI table (op=0x1C)
constexpr u32 MMI(u32 rs, u32 rt, u32 rd, u32 sa, u32 funct)
{
    return (0x1Cu << 26) | ((rs & 0x1F) << 21) | ((rt & 0x1F) << 16) |
           ((rd & 0x1F) << 11) | ((sa & 0x1F) << 6) | (funct & 0x3F);
}
// Common MMI-direct funct codes (PS2 programming manual §3 table).
constexpr u32 MADD  (u32 rd, u32 rs, u32 rt) { return MMI(rs, rt, rd, 0, 0x00); }
constexpr u32 MADDU (u32 rd, u32 rs, u32 rt) { return MMI(rs, rt, rd, 0, 0x01); }
constexpr u32 PLZCW (u32 rd, u32 rs)         { return MMI(rs, 0,  rd, 0, 0x04); }
constexpr u32 MFHI1 (u32 rd)                 { return MMI(0, 0, rd, 0, 0x10); }
constexpr u32 MTHI1 (u32 rs)                 { return MMI(rs, 0, 0,  0, 0x11); }
constexpr u32 MFLO1 (u32 rd)                 { return MMI(0, 0, rd, 0, 0x12); }
constexpr u32 MTLO1 (u32 rs)                 { return MMI(rs, 0, 0,  0, 0x13); }
constexpr u32 MULT1 (u32 rd, u32 rs, u32 rt) { return MMI(rs, rt, rd, 0, 0x18); }
constexpr u32 MULTU1(u32 rd, u32 rs, u32 rt) { return MMI(rs, rt, rd, 0, 0x19); }
constexpr u32 DIV1  (u32 rs, u32 rt)         { return MMI(rs, rt, 0,  0, 0x1A); }
constexpr u32 DIVU1 (u32 rs, u32 rt)         { return MMI(rs, rt, 0,  0, 0x1B); }

// ---- MMI SIMD sub-tables (MMI0/1/2/3 via funct-field selector) ----
// The sub-table is chosen by the funct field (bits 5:0):
//   MMI0 = funct 0x08, MMI2 = funct 0x09, MMI1 = funct 0x28, MMI3 = funct 0x29.
// The second-level op (the index into the chosen sub-table) goes in the sa
// field (bits 10:6). Encoded here as one helper per sub-table.
constexpr u32 MMI0(u32 rs, u32 rt, u32 rd, u32 sub) { return MMI(rs, rt, rd, sub, 0x08); }
constexpr u32 MMI2(u32 rs, u32 rt, u32 rd, u32 sub) { return MMI(rs, rt, rd, sub, 0x09); }
constexpr u32 MMI1(u32 rs, u32 rt, u32 rd, u32 sub) { return MMI(rs, rt, rd, sub, 0x28); }
constexpr u32 MMI3(u32 rs, u32 rt, u32 rd, u32 sub) { return MMI(rs, rt, rd, sub, 0x29); }

// Sub-op indexes verified against pcsx2/R5900OpcodeTables.cpp tbl_MMI*.
// MMI0 — parallel add/sub/compare-greater-than (selected sub-ops)
constexpr u32 PADDW (u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x00); }
constexpr u32 PADDH (u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x04); }
constexpr u32 PADDB (u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x08); }
constexpr u32 PSUBW (u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x01); }
constexpr u32 PSUBH (u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x05); }
constexpr u32 PSUBB (u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x09); }
constexpr u32 PCGTW (u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x02); }
constexpr u32 PCGTH (u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x06); }
constexpr u32 PCGTB (u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x0A); }
constexpr u32 PADDSW(u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x10); }
constexpr u32 PSUBSW(u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x11); }
constexpr u32 PADDSH(u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x14); }
constexpr u32 PSUBSH(u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x15); }
constexpr u32 PADDSB(u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x18); }
constexpr u32 PSUBSB(u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x19); }

// MMI1 — parallel compare-equal + extensions (selected sub-ops)
constexpr u32 PCEQW (u32 rd, u32 rs, u32 rt) { return MMI1(rs, rt, rd, 0x02); }
constexpr u32 PCEQH (u32 rd, u32 rs, u32 rt) { return MMI1(rs, rt, rd, 0x06); }
constexpr u32 PCEQB (u32 rd, u32 rs, u32 rt) { return MMI1(rs, rt, rd, 0x0A); }
constexpr u32 PADDUW(u32 rd, u32 rs, u32 rt) { return MMI1(rs, rt, rd, 0x10); }
constexpr u32 PSUBUW(u32 rd, u32 rs, u32 rt) { return MMI1(rs, rt, rd, 0x11); }
constexpr u32 PADDUH(u32 rd, u32 rs, u32 rt) { return MMI1(rs, rt, rd, 0x14); }
constexpr u32 PSUBUH(u32 rd, u32 rs, u32 rt) { return MMI1(rs, rt, rd, 0x15); }
constexpr u32 PADDUB(u32 rd, u32 rs, u32 rt) { return MMI1(rs, rt, rd, 0x18); }
constexpr u32 PSUBUB(u32 rd, u32 rs, u32 rt) { return MMI1(rs, rt, rd, 0x19); }

// MMI0 pack/unpack and min/max — sub-op indices from R5900OpcodeTables.cpp:536-546.
constexpr u32 PMAXW (u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x03); }
constexpr u32 PMAXH (u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x07); }
constexpr u32 PEXTLW(u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x12); }
constexpr u32 PPACW (u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x13); }
constexpr u32 PEXTLH(u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x16); }
constexpr u32 PPACH (u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x17); }
constexpr u32 PEXTLB(u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x1A); }
constexpr u32 PPACB (u32 rd, u32 rs, u32 rt) { return MMI0(rs, rt, rd, 0x1B); }

// MMI1 absolute-value / min — sub-op indices from R5900OpcodeTables.cpp:548-558.
constexpr u32 PABSW (u32 rd, u32 rt)         { return MMI1(0, rt, rd, 0x01); }
constexpr u32 PMINW (u32 rd, u32 rs, u32 rt) { return MMI1(rs, rt, rd, 0x03); }
constexpr u32 PADSBH(u32 rd, u32 rs, u32 rt) { return MMI1(rs, rt, rd, 0x04); }
constexpr u32 PABSH (u32 rd, u32 rt)         { return MMI1(0, rt, rd, 0x05); }
constexpr u32 PMINH (u32 rd, u32 rs, u32 rt) { return MMI1(rs, rt, rd, 0x07); }
constexpr u32 PEXTUW(u32 rd, u32 rs, u32 rt) { return MMI1(rs, rt, rd, 0x12); }
constexpr u32 PEXTUH(u32 rd, u32 rs, u32 rt) { return MMI1(rs, rt, rd, 0x16); }
constexpr u32 PEXTUB(u32 rd, u32 rs, u32 rt) { return MMI1(rs, rt, rd, 0x1A); }
// QFSRV — funnel-shift {Rs:Rt} right by cpuRegs.sa bytes (MMI1 sub 0x1B).
constexpr u32 QFSRV (u32 rd, u32 rs, u32 rt) { return MMI1(rs, rt, rd, 0x1B); }

// MMI2 — PAND, PXOR, paired copy-low (selected sub-ops)
constexpr u32 PAND  (u32 rd, u32 rs, u32 rt) { return MMI2(rs, rt, rd, 0x12); }
constexpr u32 PXOR  (u32 rd, u32 rs, u32 rt) { return MMI2(rs, rt, rd, 0x13); }
constexpr u32 PCPYLD(u32 rd, u32 rs, u32 rt) { return MMI2(rs, rt, rd, 0x0E); }

// MMI3 — POR, PNOR, paired copy-high, PCPYH (selected sub-ops)
constexpr u32 POR   (u32 rd, u32 rs, u32 rt) { return MMI3(rs, rt, rd, 0x12); }
constexpr u32 PNOR  (u32 rd, u32 rs, u32 rt) { return MMI3(rs, rt, rd, 0x13); }
constexpr u32 PCPYUD(u32 rd, u32 rs, u32 rt) { return MMI3(rs, rt, rd, 0x0E); }
constexpr u32 PCPYH (u32 rd, u32 rt)         { return MMI3(0, rt, rd, 0x1B); }

// MMI2 / MMI3 — pack/unpack/exchange + HI/LO transfer.
// Sub-op indices verified against tbl_MMI2 / tbl_MMI3 in
// pcsx2/R5900OpcodeTables.cpp:561-583.
constexpr u32 PMFHI (u32 rd)                 { return MMI2(0,  0, rd, 0x08); }
constexpr u32 PMFLO (u32 rd)                 { return MMI2(0,  0, rd, 0x09); }
constexpr u32 PMTHI (u32 rs)                 { return MMI3(rs, 0, 0,  0x08); }
constexpr u32 PMTLO (u32 rs)                 { return MMI3(rs, 0, 0,  0x09); }
constexpr u32 PINTH (u32 rd, u32 rs, u32 rt) { return MMI2(rs, rt, rd, 0x0A); }

// MMI2 parallel-mul / mul-acc subops (h-word lanes) — sub-op indices from
// R5900OpcodeTables.cpp tbl_MMI2.
constexpr u32 PMADDW (u32 rd, u32 rs, u32 rt) { return MMI2(rs, rt, rd, 0x00); }
constexpr u32 PMSUBW (u32 rd, u32 rs, u32 rt) { return MMI2(rs, rt, rd, 0x04); }
constexpr u32 PMADDH (u32 rd, u32 rs, u32 rt) { return MMI2(rs, rt, rd, 0x10); }
constexpr u32 PMSUBH (u32 rd, u32 rs, u32 rt) { return MMI2(rs, rt, rd, 0x14); }
constexpr u32 PMULTH (u32 rd, u32 rs, u32 rt) { return MMI2(rs, rt, rd, 0x1C); }
constexpr u32 PMULTW (u32 rd, u32 rs, u32 rt) { return MMI2(rs, rt, rd, 0x0C); }
constexpr u32 PMULTUW(u32 rd, u32 rs, u32 rt) { return MMI3(rs, rt, rd, 0x0C); }
constexpr u32 PMADDUW(u32 rd, u32 rs, u32 rt) { return MMI3(rs, rt, rd, 0x00); }
constexpr u32 PHMADH (u32 rd, u32 rs, u32 rt) { return MMI2(rs, rt, rd, 0x11); }
constexpr u32 PHMSBH (u32 rd, u32 rs, u32 rt) { return MMI2(rs, rt, rd, 0x15); }
constexpr u32 PEXT5  (u32 rd, u32 rt)         { return MMI0(0,  rt, rd, 0x1E); }
constexpr u32 PPAC5  (u32 rd, u32 rt)         { return MMI0(0,  rt, rd, 0x1F); }
constexpr u32 PINTEH (u32 rd, u32 rs, u32 rt) { return MMI3(rs, rt, rd, 0x0A); }
constexpr u32 PEXEH (u32 rd, u32 rt)         { return MMI2(0,  rt, rd, 0x1A); }
constexpr u32 PREVH (u32 rd, u32 rt)         { return MMI2(0,  rt, rd, 0x1B); }
constexpr u32 PEXEW (u32 rd, u32 rt)         { return MMI2(0,  rt, rd, 0x1E); }
constexpr u32 PROT3W(u32 rd, u32 rt)         { return MMI2(0,  rt, rd, 0x1F); }
constexpr u32 PEXCH (u32 rd, u32 rt)         { return MMI3(0,  rt, rd, 0x1A); }
constexpr u32 PEXCW (u32 rd, u32 rt)         { return MMI3(0,  rt, rd, 0x1E); }

// PMFHL — top-level MMI (funct=0x30) with sa selecting one of 5 sub-formats:
//   0=LW, 1=UW, 2=SLW, 3=LH, 4=SH (see MMI.cpp PMFHL()).
constexpr u32 PMFHL (u32 rd, u32 sub) { return MMI(0, 0, rd, sub, 0x30); }

// PMTHL — top-level MMI (funct=0x31). Only sa=0 (PMTHL.LW) is defined;
// other sa values are reserved (interp early-exits in MMI.cpp:218).
constexpr u32 PMTHL (u32 rs)          { return MMI(rs, 0, 0, 0, 0x31); }

// Parallel shift immediates — top-level MMI table (funct = 0x34..0x3F).
// Uses sa for shift amount; rs is unused. See tbl_MMI[64] in
// pcsx2/R5900OpcodeTables.cpp:524-534.
constexpr u32 PSLLH (u32 rd, u32 rt, u32 sa) { return MMI(0, rt, rd, sa, 0x34); }
constexpr u32 PSRLH (u32 rd, u32 rt, u32 sa) { return MMI(0, rt, rd, sa, 0x36); }
constexpr u32 PSRAH (u32 rd, u32 rt, u32 sa) { return MMI(0, rt, rd, sa, 0x37); }
constexpr u32 PSLLW (u32 rd, u32 rt, u32 sa) { return MMI(0, rt, rd, sa, 0x3C); }
constexpr u32 PSRLW (u32 rd, u32 rt, u32 sa) { return MMI(0, rt, rd, sa, 0x3E); }
constexpr u32 PSRAW (u32 rd, u32 rt, u32 sa) { return MMI(0, rt, rd, sa, 0x3F); }

// ---- Trap instructions (SPECIAL, op=0) ----
constexpr u32 TGE  (u32 rs, u32 rt) { return RType(0, rs, rt, 0, 0, 0x30); }
constexpr u32 TGEU (u32 rs, u32 rt) { return RType(0, rs, rt, 0, 0, 0x31); }
constexpr u32 TLT  (u32 rs, u32 rt) { return RType(0, rs, rt, 0, 0, 0x32); }
constexpr u32 TLTU (u32 rs, u32 rt) { return RType(0, rs, rt, 0, 0, 0x33); }
constexpr u32 TEQ  (u32 rs, u32 rt) { return RType(0, rs, rt, 0, 0, 0x34); }
constexpr u32 TNE  (u32 rs, u32 rt) { return RType(0, rs, rt, 0, 0, 0x36); }

// Trap-immediate (REGIMM, op=1)
constexpr u32 TGEI  (u32 rs, s16 imm) { return IType(0x01, rs, 0x08, (u16)imm); }
constexpr u32 TGEIU (u32 rs, s16 imm) { return IType(0x01, rs, 0x09, (u16)imm); }
constexpr u32 TLTI  (u32 rs, s16 imm) { return IType(0x01, rs, 0x0A, (u16)imm); }
constexpr u32 TLTIU (u32 rs, s16 imm) { return IType(0x01, rs, 0x0B, (u16)imm); }
constexpr u32 TEQI  (u32 rs, s16 imm) { return IType(0x01, rs, 0x0C, (u16)imm); }
constexpr u32 TNEI  (u32 rs, s16 imm) { return IType(0x01, rs, 0x0E, (u16)imm); }

// ---- Shift-amount register (PS2-specific) ----
// MFSA / MTSA live in the SPECIAL table at funct 0x28 / 0x29.
// MTSAB / MTSAH live in the REGIMM table at rt = 0x18 / 0x19.
constexpr u32 MFSA  (u32 rd)              { return RType(0, 0, 0, rd, 0, 0x28); }
constexpr u32 MTSA  (u32 rs)              { return RType(0, rs, 0, 0, 0, 0x29); }
constexpr u32 MTSAB (u32 rs, s16 imm)     { return IType(0x01, rs, 0x18, (u16)imm); }
constexpr u32 MTSAH (u32 rs, s16 imm)     { return IType(0x01, rs, 0x19, (u16)imm); }

// ---- SYNC (SPECIAL, funct=0x0F) ----
constexpr u32 SYNC = (0u << 26) | 0x0Fu;

// ---- COP0 privileged helpers ----
constexpr u32 ERET = (0x10u << 26) | (0x10u << 21) | 0x18u;
constexpr u32 EI   = (0x10u << 26) | (0x10u << 21) | 0x38u;
constexpr u32 DI   = (0x10u << 26) | (0x10u << 21) | 0x39u;

// ---- COP1 (FPU, op=0x11). Single-precision variants (fmt=0x10). ----
constexpr u32 COP1(u32 rs, u32 ft, u32 fs, u32 fd, u32 funct)
{
    return (0x11u << 26) | ((rs & 0x1F) << 21) | ((ft & 0x1F) << 16) |
           ((fs & 0x1F) << 11) | ((fd & 0x1F) << 6) | (funct & 0x3F);
}
constexpr u32 MFC1 (u32 rt, u32 fs)                    { return COP1(0x00, rt, fs, 0, 0); }
constexpr u32 MTC1 (u32 rt, u32 fs)                    { return COP1(0x04, rt, fs, 0, 0); }
constexpr u32 CFC1 (u32 rt, u32 fs)                    { return COP1(0x02, rt, fs, 0, 0); }
constexpr u32 CTC1 (u32 rt, u32 fs)                    { return COP1(0x06, rt, fs, 0, 0); }
constexpr u32 ADD_S(u32 fd, u32 fs, u32 ft)            { return COP1(0x10, ft, fs, fd, 0x00); }
constexpr u32 SUB_S(u32 fd, u32 fs, u32 ft)            { return COP1(0x10, ft, fs, fd, 0x01); }
constexpr u32 MUL_S(u32 fd, u32 fs, u32 ft)            { return COP1(0x10, ft, fs, fd, 0x02); }
constexpr u32 DIV_S(u32 fd, u32 fs, u32 ft)            { return COP1(0x10, ft, fs, fd, 0x03); }
// PS2 SQRT.S reads ft (rt slot, bits 20:16), NOT fs. Both PCSX2 interp
// (FPU.cpp _FtValUl_) and the JIT (XMMINFO_READT) follow that quirk; the
// encoder must place the source in the rt slot to match.
constexpr u32 SQRT_S(u32 fd, u32 ft)                   { return COP1(0x10, ft, 0,  fd, 0x04); }
// RSQRT.S: fd = fs / sqrt(ft). funct 0x16; reads fs (dividend) and ft (divisor).
constexpr u32 RSQRT_S(u32 fd, u32 fs, u32 ft)          { return COP1(0x10, ft, fs, fd, 0x16); }
constexpr u32 ABS_S(u32 fd, u32 fs)                    { return COP1(0x10, 0,  fs, fd, 0x05); }
constexpr u32 MOV_S(u32 fd, u32 fs)                    { return COP1(0x10, 0,  fs, fd, 0x06); }
constexpr u32 NEG_S(u32 fd, u32 fs)                    { return COP1(0x10, 0,  fs, fd, 0x07); }
constexpr u32 MAX_S(u32 fd, u32 fs, u32 ft)            { return COP1(0x10, ft, fs, fd, 0x28); }
constexpr u32 MIN_S(u32 fd, u32 fs, u32 ft)            { return COP1(0x10, ft, fs, fd, 0x29); }
constexpr u32 CVT_W_S(u32 fd, u32 fs)                  { return COP1(0x10, 0,  fs, fd, 0x24); }
constexpr u32 CVT_S_W(u32 fd, u32 fs)                  { return COP1(0x14, 0,  fs, fd, 0x20); }
// PS2-specific FPU accumulator family. ADDA/SUBA/MULA write to ACC (no Fd
// field in the op); MADD/MSUB/MADDA/MSUBA are multiply-then-add/sub forms.
constexpr u32 ADDA_S (u32 fs, u32 ft)                  { return COP1(0x10, ft, fs, 0,  0x18); }
constexpr u32 SUBA_S (u32 fs, u32 ft)                  { return COP1(0x10, ft, fs, 0,  0x19); }
constexpr u32 MULA_S (u32 fs, u32 ft)                  { return COP1(0x10, ft, fs, 0,  0x1A); }
constexpr u32 MADD_S (u32 fd, u32 fs, u32 ft)          { return COP1(0x10, ft, fs, fd, 0x1C); }
constexpr u32 MSUB_S (u32 fd, u32 fs, u32 ft)          { return COP1(0x10, ft, fs, fd, 0x1D); }
constexpr u32 MADDA_S(u32 fs, u32 ft)                  { return COP1(0x10, ft, fs, 0,  0x1E); }
constexpr u32 MSUBA_S(u32 fs, u32 ft)                  { return COP1(0x10, ft, fs, 0,  0x1F); }
// Compare — PS2 FPU implements four conditions: F (0x30), EQ (0x32),
// LT (0x34), LE (0x36). NOT the standard MIPS layout that puts LT/LE at
// 0x3C/0x3E — the PS2 table at R5900OpcodeTables.cpp tbl_COP1_S[0x34/0x36]
// is what dispatches recC_LT/recC_LE.
constexpr u32 C_F_S  (u32 fs, u32 ft)                  { return COP1(0x10, ft, fs, 0, 0x30); }
constexpr u32 C_EQ_S (u32 fs, u32 ft)                  { return COP1(0x10, ft, fs, 0, 0x32); }
constexpr u32 C_LT_S (u32 fs, u32 ft)                  { return COP1(0x10, ft, fs, 0, 0x34); }
constexpr u32 C_LE_S (u32 fs, u32 ft)                  { return COP1(0x10, ft, fs, 0, 0x36); }
// Branch on COP1 condition — BC1F (cond=0) / BC1T (cond=1), rt field 0/1
constexpr u32 BC1F(s16 off) { return (0x11u << 26) | (0x08u << 21) | (0x00u << 16) | (u16)off; }
constexpr u32 BC1T(s16 off) { return (0x11u << 26) | (0x08u << 21) | (0x01u << 16) | (u16)off; }
// FPU 32-bit load/store — top-level opcodes LWC1 (0x31) / SWC1 (0x39).
// ft = FPU register, off(base) = EA. ft occupies the rt slot.
constexpr u32 LWC1(u32 ft, s16 off, u32 base) { return IType(0x31, base, ft, (u16)off); }
constexpr u32 SWC1(u32 ft, s16 off, u32 base) { return IType(0x39, base, ft, (u16)off); }

// ---- COP2 (VU0 macro mode + microprogram kick + register transfer) ----
//
// Layout: bits[31:26] = 0x12 (COP2 primary). bits[25:21] = rs sub-selector.
//   rs=0x00 MFC2          rs=0x01 QMFC2          rs=0x02 CFC2
//   rs=0x04 MTC2          rs=0x05 QMTC2          rs=0x06 CTC2
//   rs=0x08 BC2 (branches; not used by handoff tests)
//   rs>=0x10 (CO=1, bit[25] set): macro-mode VU op or VCALLMS
//
// For COP2-CO ops (CO=1), bits[5:0] = funct (mirror of VU upper-pipe primary
// table for VADDx..VMINIw and the SPECIAL2 trampolines 0x3C..0x3F). Macro
// VADDx fd, fs, ft is then identical in shape to a VU upper-word VADDx
// except the top byte is 0x4A rather than the bare VU upper bits.
//
// Field reuse: _Ft_ = bits[20:16] (rt), _Fs_ = bits[15:11] (rd), _Fd_ =
// bits[10:6] (sa). The XYZW destination mask occupies bits[24:21].
constexpr u32 COP2(u32 rs, u32 rt, u32 rd, u32 sa, u32 funct)
{
	return (0x12u << 26) | ((rs & 0x1Fu) << 21)
	     | ((rt & 0x1Fu) << 16) | ((rd & 0x1Fu) << 11)
	     | ((sa & 0x1Fu) <<  6) | (funct & 0x3Fu);
}

// Register-transfer ops — rt is an EE GPR index, fs is a VU0 register index.
// CFC2/CTC2 access VI[fs]; QMFC2/QMTC2/MFC2/MTC2 access VF[fs].
constexpr u32 CFC2 (u32 rt, u32 fs) { return COP2(0x02, rt, fs, 0, 0); }
constexpr u32 CTC2 (u32 rt, u32 fs) { return COP2(0x06, rt, fs, 0, 0); }
constexpr u32 MFC2 (u32 rt, u32 fs) { return COP2(0x00, rt, fs, 0, 0); }
constexpr u32 MTC2 (u32 rt, u32 fs) { return COP2(0x04, rt, fs, 0, 0); }
constexpr u32 QMFC2(u32 rt, u32 fs) { return COP2(0x01, rt, fs, 0, 0); }
constexpr u32 QMTC2(u32 rt, u32 fs) { return COP2(0x05, rt, fs, 0, 0); }

// Interlocked (.I) forms — funct bit 0 set. The recompiler routes these
// through the COP2_Interlock sync protocol (exact VU0 catch-up + wait/finish)
// instead of the non-interlock run-ahead gating.
constexpr u32 CFC2_I (u32 rt, u32 fs) { return COP2(0x02, rt, fs, 0, 1); }
constexpr u32 CTC2_I (u32 rt, u32 fs) { return COP2(0x06, rt, fs, 0, 1); }
constexpr u32 QMFC2_I(u32 rt, u32 fs) { return COP2(0x01, rt, fs, 0, 1); }
constexpr u32 QMTC2_I(u32 rt, u32 fs) { return COP2(0x05, rt, fs, 0, 1); }

// COP2 condition branches — rs=0x08 (BC2), rt selects the variant; imm16 is the
// signed branch offset (NOT the CO-op rd/sa/funct layout). CP2COND = bit 8 of
// VU0.VI[REG_VPU_STAT]: BC2F taken when clear, BC2T taken when set; FL/TL are
// the likely (delay-slot-squashing) variants.
constexpr u32 BC2F (s16 off) { return (0x12u << 26) | (0x08u << 21) | (0x00u << 16) | (u16)off; }
constexpr u32 BC2T (s16 off) { return (0x12u << 26) | (0x08u << 21) | (0x01u << 16) | (u16)off; }
constexpr u32 BC2FL(s16 off) { return (0x12u << 26) | (0x08u << 21) | (0x02u << 16) | (u16)off; }
constexpr u32 BC2TL(s16 off) { return (0x12u << 26) | (0x08u << 21) | (0x03u << 16) | (u16)off; }

// LQC2 / SQC2 — top-level opcode 0x36 / 0x3E. Base+offset addressing.
constexpr u32 LQC2(u32 ft, u32 base, s16 offset) { return IType(0x36, base, ft, (u16)offset); }
constexpr u32 SQC2(u32 ft, u32 base, s16 offset) { return IType(0x3E, base, ft, (u16)offset); }

// VCALLMS / VCALLMSR — kick a VU0 microprogram. COP2-CO funct 0x38 / 0x39.
// VCALLMS encodes the start-PC/8 in bits[20:6] (15-bit imm).
constexpr u32 VCALLMS (u32 startpc_div8)
{
	return (0x12u << 26) | (1u << 25) | ((startpc_div8 & 0x7FFFu) << 6) | 0x38u;
}
constexpr u32 VCALLMSR()
{
	return (0x12u << 26) | (1u << 25) | 0x39u;
}

// COP2-CO macro-mode VU ops. The funct field mirrors the VU upper-pipe primary
// opcode (0x28 = VADD, 0x29 = VMADD, 0x2A = VMUL, 0x2B = VMAX, 0x2C = VSUB,
// 0x2D = VMSUB, 0x2E = VOPMSUB, 0x2F = VMINI). All take an XYZW dest mask.
//
// Operand convention matches EE bit layout: rt = ft (bits 20-16), rd = fs
// (bits 15-11), sa = fd (bits 10-6).
constexpr u32 COP2_FMAC(u32 mask_xyzw, u32 fd, u32 fs, u32 ft, u32 funct)
{
	return (0x12u << 26) | (1u << 25)
	     | ((mask_xyzw & 0xFu) << 21)
	     | ((ft & 0x1Fu) << 16) | ((fs & 0x1Fu) << 11)
	     | ((fd & 0x1Fu) <<  6) | (funct & 0x3Fu);
}
constexpr u32 VADD_C2   (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, fd, fs, ft, 0x28); }
constexpr u32 VMADD_C2  (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, fd, fs, ft, 0x29); }

// VMADDx/y/z/w — broadcast MADD (fd = ACC + fs * ft.bc). COP2 SPECIAL1 funct
// 0x08-0x0B (row 1 of Int_COP2SPECIAL1PrintTable).
constexpr u32 VMADDx_C2 (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, fd, fs, ft, 0x08); }
constexpr u32 VMADDy_C2 (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, fd, fs, ft, 0x09); }
constexpr u32 VMADDz_C2 (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, fd, fs, ft, 0x0A); }
constexpr u32 VMADDw_C2 (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, fd, fs, ft, 0x0B); }
// VMULx/y/z/w — broadcast MUL (fd = fs * ft.bc). COP2 SPECIAL1 funct 0x18-0x1B
// (row 3 of Int_COP2SPECIAL1PrintTable).
constexpr u32 VMULx_C2  (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, fd, fs, ft, 0x18); }
constexpr u32 VMULy_C2  (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, fd, fs, ft, 0x19); }
constexpr u32 VMULz_C2  (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, fd, fs, ft, 0x1A); }
constexpr u32 VMULw_C2  (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, fd, fs, ft, 0x1B); }
constexpr u32 VMUL_C2   (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, fd, fs, ft, 0x2A); }
constexpr u32 VMAX_C2   (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, fd, fs, ft, 0x2B); }
constexpr u32 VSUB_C2   (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, fd, fs, ft, 0x2C); }
constexpr u32 VMSUB_C2  (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, fd, fs, ft, 0x2D); }
constexpr u32 VOPMSUB_C2(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, fd, fs, ft, 0x2E); }
constexpr u32 VMINI_C2  (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, fd, fs, ft, 0x2F); }

// Integer-ALU macro ops (funct 0x30/0x31/0x32/0x34/0x35). Same field layout
// as the VU LowerOP integer table but issued as EE COP2-CO. The SA field is
// the VU integer destination index (4-bit valid).
constexpr u32 VIADD_C2 (u32 id, u32 is, u32 it) { return COP2_FMAC(0, id, is, it, 0x30); }
constexpr u32 VISUB_C2 (u32 id, u32 is, u32 it) { return COP2_FMAC(0, id, is, it, 0x31); }
constexpr u32 VIAND_C2 (u32 id, u32 is, u32 it) { return COP2_FMAC(0, id, is, it, 0x34); }
constexpr u32 VIOR_C2  (u32 id, u32 is, u32 it) { return COP2_FMAC(0, id, is, it, 0x35); }

// Fixed-point conversion macro ops (COP2-CO SPECIAL2 indices 16-23). FTOI/ITOF
// share funct 0x3C..0x3F (the SPECIAL2 escape) with the sub-op selector in the
// _Sa_/fd field (bits 10-6): 0x04 = ITOFx, 0x05 = FTOIx. The funct low 2 bits
// pick the fixed-point fraction (0x3C/3D/3E/3F = 0/4/12/15). ft (bits 20-16) is
// the destination, fs (bits 15-11) the source.
constexpr u32 VFTOI0_C2 (u32 mask_xyzw, u32 ft, u32 fs) { return COP2_FMAC(mask_xyzw, 0x05, fs, ft, 0x3C); }
constexpr u32 VFTOI4_C2 (u32 mask_xyzw, u32 ft, u32 fs) { return COP2_FMAC(mask_xyzw, 0x05, fs, ft, 0x3D); }
constexpr u32 VFTOI12_C2(u32 mask_xyzw, u32 ft, u32 fs) { return COP2_FMAC(mask_xyzw, 0x05, fs, ft, 0x3E); }
constexpr u32 VFTOI15_C2(u32 mask_xyzw, u32 ft, u32 fs) { return COP2_FMAC(mask_xyzw, 0x05, fs, ft, 0x3F); }
constexpr u32 VITOF0_C2 (u32 mask_xyzw, u32 ft, u32 fs) { return COP2_FMAC(mask_xyzw, 0x04, fs, ft, 0x3C); }

// VCLIP — COP2-CO SPECIAL2 index 31 (sub-op 0x07, funct 0x3F). Tests fs.xyz
// against |ft.w|; folds a 6-bit result into VI[REG_CLIP_FLAG] (no FD write).
constexpr u32 VCLIP_C2  (u32 ft, u32 fs) { return COP2_FMAC(0, 0x07, fs, ft, 0x3F); }

// VMULAw — broadcast-accumulator MUL (ACC = fs * ft.w). COP2-CO SPECIAL2 index
// 27 (sub-op 0x06, funct 0x3F). VMULAx/y/z are indices 24-26 (funct 0x3C/3D/3E).
constexpr u32 VMULAw_C2 (u32 mask_xyzw, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, 0x06, fs, ft, 0x3F); }
constexpr u32 VMULAx_C2 (u32 mask_xyzw, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, 0x06, fs, ft, 0x3C); }
constexpr u32 VMULAy_C2 (u32 mask_xyzw, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, 0x06, fs, ft, 0x3D); }

// VMADDAx/y — broadcast multiply-accumulate into ACC (ACC += fs * ft.bc).
// SPEC2 sub-op 0x02, funct 0x3C/0x3D (SPECIAL2 indices 0x08/0x09).
constexpr u32 VMADDAx_C2(u32 mask_xyzw, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, 0x02, fs, ft, 0x3C); }
constexpr u32 VMADDAy_C2(u32 mask_xyzw, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, 0x02, fs, ft, 0x3D); }
constexpr u32 VMULAz_C2 (u32 mask_xyzw, u32 fs, u32 ft) { return COP2_FMAC(mask_xyzw, 0x06, fs, ft, 0x3E); }

// COP2-CO SPECIAL2 (LowerOP2 trampolines via SPEC1 funct 0x3C..0x3F).
// The SPEC2 dispatch index inside recCOP2SPECIAL2t is
//   (code & 0x3) | ((code >> 4) & 0x7c)
// = (sa << 2) | (funct & 3); i.e. sa selects the 4-op group and the low 2
// bits of funct pick within the group. funct's high nibble is always 0x3C.
constexpr u32 COP2_SPEC2(u32 mask_xyzw, u32 ft, u32 fs, u32 sa, u32 funct)
{
	return (0x12u << 26) | (1u << 25)
	     | ((mask_xyzw & 0xFu) << 21)
	     | ((ft & 0x1Fu) << 16) | ((fs & 0x1Fu) << 11)
	     | ((sa & 0x1Fu) <<  6) | (funct & 0x3Fu);
}

// VF load/store with VI post/pre-inc/dec (sa group = 0x0D).
// VLQI / VLQD: dest VF[ft] at bits[20:16], addr VI[is] at bits[15:11].
// VSQI / VSQD: addr VI[it] at bits[20:16], src VF[fs] at bits[15:11].
// (Load and store ops have the VF/VI positions swapped — load's dest VF
// shares the slot that store's addr VI uses.)
constexpr u32 VLQI_C2(u32 mask_xyzw, u32 ft, u32 is) { return COP2_SPEC2(mask_xyzw, ft, is, 0x0D, 0x3C); }
constexpr u32 VSQI_C2(u32 mask_xyzw, u32 fs, u32 it) { return COP2_SPEC2(mask_xyzw, it, fs, 0x0D, 0x3D); }
constexpr u32 VLQD_C2(u32 mask_xyzw, u32 ft, u32 is) { return COP2_SPEC2(mask_xyzw, ft, is, 0x0D, 0x3E); }
constexpr u32 VSQD_C2(u32 mask_xyzw, u32 fs, u32 it) { return COP2_SPEC2(mask_xyzw, it, fs, 0x0D, 0x3F); }

// VI ↔ VF transfer + VU memory ops on VI bank (sa group = 0x0F).
// VMTIR : VI[it]      = VF[fs].lane(fsf).  fsf occupies bits[22:21] = mask[1:0].
// VMFIR : VF[ft].mask = sign_extend(VI[is]).
// VILWR : VI[it]      = Mem[VI[is] * 16].lane (mask picks lane).
// VISWR : Mem[VI[is] * 16].lane = VI[it] (interp _Is_ = addr base, _It_ = value).
// VMOVE — SPEC2 group, sa=0x0C, funct=0x3C (SPECIAL2 index 0x30): VF[ft].mask = VF[fs].
constexpr u32 VMOVE_C2(u32 mask_xyzw, u32 ft, u32 fs) { return COP2_SPEC2(mask_xyzw, ft, fs, 0x0C, 0x3C); }

constexpr u32 VMTIR_C2(u32 fsf,        u32 it, u32 fs) { return COP2_SPEC2(fsf & 0x3,    it, fs, 0x0F, 0x3C); }
constexpr u32 VMFIR_C2(u32 mask_xyzw,  u32 ft, u32 is) { return COP2_SPEC2(mask_xyzw,    ft, is, 0x0F, 0x3D); }
constexpr u32 VILWR_C2(u32 mask_xyzw,  u32 it, u32 is) { return COP2_SPEC2(mask_xyzw,    it, is, 0x0F, 0x3E); }
constexpr u32 VISWR_C2(u32 mask_xyzw,  u32 it, u32 is) { return COP2_SPEC2(mask_xyzw,    it, is, 0x0F, 0x3F); }

// VOPMULA — SPEC2 group, sa=0xB, funct=0x3E. Result written to ACC, not VF[fd].
// PS2 hardware always writes XYZ lanes of ACC; W is preserved regardless of mask.
constexpr u32 VOPMULA_C2(u32 mask_xyzw, u32 fs, u32 ft) { return COP2_SPEC2(mask_xyzw, ft, fs, 0xB, 0x3E); }

// R-register / LFSR ops (sa group = 0x10).
// VRNEXT : advance LFSR; VF[ft].mask = R | 0x3F800000.
// VRGET  : VF[ft].mask = R | 0x3F800000.
// VRINIT : R = (VF[fs].lane(fsf) & 0x007FFFFF) | 0x3F800000.
// VRXOR  : R ^= VF[fs].lane(fsf).
constexpr u32 VRNEXT_C2(u32 mask_xyzw, u32 ft)         { return COP2_SPEC2(mask_xyzw,    ft,  0, 0x10, 0x3C); }
constexpr u32 VRGET_C2 (u32 mask_xyzw, u32 ft)         { return COP2_SPEC2(mask_xyzw,    ft,  0, 0x10, 0x3D); }
constexpr u32 VRINIT_C2(u32 fsf,                u32 fs){ return COP2_SPEC2(fsf & 0x3,     0, fs, 0x10, 0x3E); }
constexpr u32 VRXOR_C2 (u32 fsf,                u32 fs){ return COP2_SPEC2(fsf & 0x3,     0, fs, 0x10, 0x3F); }

// DIV-unit + pipeline ops (sa group = 0x0E). The broadcast-lane selectors
// occupy the dest-mask slot: fsf at bits[22:21], ftf at bits[24:23].
// VDIV   : Q = VF[fs].lane(fsf) / VF[ft].lane(ftf).
// VSQRT  : Q = sqrt(|VF[ft].lane(ftf)|).
// VRSQRT : Q = VF[fs].lane(fsf) / sqrt(|VF[ft].lane(ftf)|).
// VWAITQ : wait for the DIV unit (no-op in synchronous macro mode).
constexpr u32 VDIV_C2  (u32 fsf, u32 ftf, u32 fs, u32 ft) { return COP2_SPEC2(((ftf & 0x3) << 2) | (fsf & 0x3), ft, fs, 0x0E, 0x3C); }
constexpr u32 VSQRT_C2 (u32 ftf,          u32 ft)         { return COP2_SPEC2((ftf & 0x3) << 2,                 ft,  0, 0x0E, 0x3D); }
constexpr u32 VRSQRT_C2(u32 fsf, u32 ftf, u32 fs, u32 ft) { return COP2_SPEC2(((ftf & 0x3) << 2) | (fsf & 0x3), ft, fs, 0x0E, 0x3E); }
constexpr u32 VWAITQ_C2()                                 { return COP2_SPEC2(0, 0, 0, 0x0E, 0x3F); }

// VNOP — SPEC2 group, sa=0x0B, funct=0x3F. Architectural no-op, but still a
// COP2-CO op: the COP2MicroFinishPass can mark it EEINST_COP2_FINISH_VU0, in
// which case it must drain a pending VU0 micro (x86 syncs it via the
// recCOP2_SPEC1 dispatch wrapper).
constexpr u32 VNOP_C2()                                   { return COP2_SPEC2(0, 0, 0, 0x0B, 0x3F); }

// VADDq — SPECIAL1 funct 0x20: VF[fd].mask = VF[fs] + Q (broadcast). Handy as
// a Q-register witness: VADDq(mask_x, fd, /*fs*/0) captures Q into VF[fd].x.
constexpr u32 VADDq_C2(u32 mask_xyzw, u32 fd, u32 fs)     { return COP2_FMAC(mask_xyzw, fd, fs, 0, 0x20); }

} // namespace ee

} // namespace mips
