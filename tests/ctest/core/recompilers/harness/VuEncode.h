// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

// Constexpr VU instruction encoders for the recompiler test harness.
//
// VU programs are 64-bit instruction pairs at byte offsets `pc, pc+8, pc+16,
// ...`. Each pair consists of a 32-bit lower word (at offset +0) and a 32-bit
// upper word (at offset +4). The interpreter and recompilers both fetch
// `ptr[0]` (lower) and `ptr[1]` (upper) per pair (see VU0microInterp.cpp:75
// and VU1microInterp.cpp:79 for the reference fetch pattern).
//
// Upper-word layout (per VUops.cpp `_Ft_` / `_Fs_` / `_Fd_` / `_XYZW`):
//   bits[31:27] — special bits I, E, M, D, T (one each, msb-first)
//   bits[26:25] — unused
//   bits[24:21] — XYZW destination mask (one bit per lane)
//   bits[20:16] — FT (5-bit register index)
//   bits[15:11] — FS
//   bits[10: 6] — FD (or sub-op selector for the UPPER_FD_xx tables)
//   bits[ 5: 0] — primary opcode (selects from VU*_UPPER_OPCODE[64])
//
// Lower-word layout:
//   bits[31:25] — primary opcode (selects from VU*_LOWER_OPCODE[128])
//   bits[24: 0] — opcode-specific (typically FT/FS/FD/imm fields)
//
// Coverage in this file is intentionally minimal: enough to write
// harness-validation tests (NOP pair with E-bit, integer ALU, simple
// FMAC) and lay down the encoder shape so additional suites can
// extend per-suite without churn.
namespace vu {

// VU register indices. VF and VI both have 32 entries; VF[0] is hardwired
// to (0, 0, 0, 1.0) and VI[0] is hardwired to 0. Names match the PS2 VU
// programming manual.
namespace vf {
constexpr u32 vf0  = 0,  vf1  = 1,  vf2  = 2,  vf3  = 3;
constexpr u32 vf4  = 4,  vf5  = 5,  vf6  = 6,  vf7  = 7;
constexpr u32 vf8  = 8,  vf9  = 9,  vf10 = 10, vf11 = 11;
constexpr u32 vf12 = 12, vf13 = 13, vf14 = 14, vf15 = 15;
constexpr u32 vf16 = 16, vf17 = 17, vf18 = 18, vf19 = 19;
constexpr u32 vf20 = 20, vf21 = 21, vf22 = 22, vf23 = 23;
constexpr u32 vf24 = 24, vf25 = 25, vf26 = 26, vf27 = 27;
constexpr u32 vf28 = 28, vf29 = 29, vf30 = 30, vf31 = 31;
}

namespace vi {
constexpr u32 vi0  = 0,  vi1  = 1,  vi2  = 2,  vi3  = 3;
constexpr u32 vi4  = 4,  vi5  = 5,  vi6  = 6,  vi7  = 7;
constexpr u32 vi8  = 8,  vi9  = 9,  vi10 = 10, vi11 = 11;
constexpr u32 vi12 = 12, vi13 = 13, vi14 = 14, vi15 = 15;
}

// Destination-mask bits for the upper instruction word.
namespace mask {
constexpr u32 x = 1u << 24;
constexpr u32 y = 1u << 23;
constexpr u32 z = 1u << 22;
constexpr u32 w = 1u << 21;
constexpr u32 xyzw = x | y | z | w;
constexpr u32 xyz  = x | y | z;
constexpr u32 none = 0;
}

// Special bits in the upper word. The interpreter checks these via
// `ptr[1] & 0x40000000` (E), `& 0x10000000` (D), `& 0x08000000` (T),
// `& 0x80000000` (I), `& 0x20000000` (M, VU0 only). See VU0microInterp.cpp
// _vu0Exec / VU1microInterp.cpp _vu1Exec for the reference checks.
namespace bits {
constexpr u32 I = 1u << 31; // lower word becomes 32-bit float immediate (VI[REG_I])
constexpr u32 E = 1u << 30; // end of microprogram (one delay-slot pair after)
constexpr u32 M = 1u << 29; // VU0 only — sets VUFLAG_MFLAGSET, breaks Execute loop
constexpr u32 D = 1u << 28; // INTC interrupt if FBRST.D-stop bit set
constexpr u32 T = 1u << 27; // INTC interrupt if FBRST.T-stop bit set
}

// One instruction pair. `lower` ends up at `pc+0`, `upper` at `pc+4`.
struct VuOp
{
	u32 lower = 0;
	u32 upper = 0;
};

// Bit-flag composers — return a copy of the pair with the requested bit
// OR'd into the upper word. Composable: `EBit(IBit(NopPair()))`.
constexpr VuOp WithBits(VuOp op, u32 mask) { op.upper |= mask; return op; }
constexpr VuOp EBit(VuOp op) { return WithBits(op, bits::E); }
constexpr VuOp MBit(VuOp op) { return WithBits(op, bits::M); }
constexpr VuOp DBit(VuOp op) { return WithBits(op, bits::D); }
constexpr VuOp TBit(VuOp op) { return WithBits(op, bits::T); }
constexpr VuOp IBit(VuOp op) { return WithBits(op, bits::I); }

// ---------------------------------------------------------------------------
//  Upper instruction encoders
// ---------------------------------------------------------------------------

// Generic upper-pipe builder for primary-table opcodes (bits[5:0]).
constexpr u32 Upper(u32 mask_xyzw, u32 ft, u32 fs, u32 fd, u32 op)
{
	return (mask_xyzw & 0x1E00000u)
	     | ((ft & 0x1Fu) << 16)
	     | ((fs & 0x1Fu) << 11)
	     | ((fd & 0x1Fu) <<  6)
	     | (op & 0x3Fu);
}

// FMAC primary-table ops: ADD/SUB/MADD/MSUB/MAX/MINI/MUL with full xyzw
// operands (FD ← FS op FT). Primary opcodes 0x28..0x2F.
constexpr u32 VADD_U (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x28); }
constexpr u32 VMADD_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x29); }
constexpr u32 VMUL_U (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x2A); }
constexpr u32 VMAX_U (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x2B); }
constexpr u32 VSUB_U (u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x2C); }
constexpr u32 VMSUB_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x2D); }
constexpr u32 VMINI_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x2F); }

// "i" broadcasts: read VI[REG_I] (the I-bit immediate) as a scalar and
// broadcast across xyzw. Per the UPPER_OPCODE table: 0x1D=MAXi, 0x1E=MULi,
// 0x1F=MINIi, 0x22=ADDi, 0x23=MADDi, 0x26=SUBi, 0x27=MSUBi.
constexpr u32 VMAXi_U (u32 mask_xyzw, u32 fd, u32 fs) { return Upper(mask_xyzw, 0, fs, fd, 0x1D); }
constexpr u32 VMULi_U (u32 mask_xyzw, u32 fd, u32 fs) { return Upper(mask_xyzw, 0, fs, fd, 0x1E); }
constexpr u32 VMINIi_U(u32 mask_xyzw, u32 fd, u32 fs) { return Upper(mask_xyzw, 0, fs, fd, 0x1F); }
constexpr u32 VADDi_U (u32 mask_xyzw, u32 fd, u32 fs) { return Upper(mask_xyzw, 0, fs, fd, 0x22); }
constexpr u32 VMADDi_U(u32 mask_xyzw, u32 fd, u32 fs) { return Upper(mask_xyzw, 0, fs, fd, 0x23); }
constexpr u32 VSUBi_U (u32 mask_xyzw, u32 fd, u32 fs) { return Upper(mask_xyzw, 0, fs, fd, 0x26); }
constexpr u32 VMSUBi_U(u32 mask_xyzw, u32 fd, u32 fs) { return Upper(mask_xyzw, 0, fs, fd, 0x27); }

// "q" broadcasts: read Q-pipe scalar and broadcast across xyzw.
// 0x1C=MULq, 0x20=ADDq, 0x21=MADDq, 0x24=SUBq, 0x25=MSUBq.
constexpr u32 VMULq_U (u32 mask_xyzw, u32 fd, u32 fs) { return Upper(mask_xyzw, 0, fs, fd, 0x1C); }
constexpr u32 VADDq_U (u32 mask_xyzw, u32 fd, u32 fs) { return Upper(mask_xyzw, 0, fs, fd, 0x20); }
constexpr u32 VMADDq_U(u32 mask_xyzw, u32 fd, u32 fs) { return Upper(mask_xyzw, 0, fs, fd, 0x21); }
constexpr u32 VSUBq_U (u32 mask_xyzw, u32 fd, u32 fs) { return Upper(mask_xyzw, 0, fs, fd, 0x24); }
constexpr u32 VMSUBq_U(u32 mask_xyzw, u32 fd, u32 fs) { return Upper(mask_xyzw, 0, fs, fd, 0x25); }

// Broadcast variants: ADDx/y/z/w (FD ← FS + FT.bc). Primary opcodes 0x00..0x03.
constexpr u32 VADDx_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x00); }
constexpr u32 VADDy_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x01); }
constexpr u32 VADDz_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x02); }
constexpr u32 VADDw_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x03); }
constexpr u32 VSUBx_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x04); }
constexpr u32 VSUBy_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x05); }
constexpr u32 VSUBz_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x06); }
constexpr u32 VSUBw_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x07); }
constexpr u32 VMULx_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x18); }
constexpr u32 VMULy_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x19); }
constexpr u32 VMULz_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x1A); }
constexpr u32 VMULw_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x1B); }
// MADD / MSUB broadcast variants (FD ← ACC + FS * FT.bc / FD ← ACC - FS * FT.bc).
// Primary opcodes 0x08..0x0F per PREFIX##_UPPER_OPCODE (VUops.cpp:3749).
constexpr u32 VMADDx_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x08); }
constexpr u32 VMADDy_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x09); }
constexpr u32 VMADDz_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x0A); }
constexpr u32 VMADDw_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x0B); }
constexpr u32 VMSUBx_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x0C); }
constexpr u32 VMSUBy_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x0D); }
constexpr u32 VMSUBz_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x0E); }
constexpr u32 VMSUBw_U(u32 mask_xyzw, u32 fd, u32 fs, u32 ft) { return Upper(mask_xyzw, ft, fs, fd, 0x0F); }

// Upper NOP. Lives in UPPER_FD_11_TABLE[0x0B], reached by primary opcode
// 0x3F + sub-op 0x0B in the FD field. Result: 0x000002FF.
constexpr u32 VNOP_U()
{
	return (0x3Fu) | (0x0Bu << 6);
}

// ---------------------------------------------------------------------------
//  Lower instruction encoders
// ---------------------------------------------------------------------------

// Generic lower-pipe builder for primary-table opcodes (bits[31:25]).
constexpr u32 Lower(u32 op7, u32 ft, u32 fs, u32 fd_or_imm, u32 fields_24_to_0_extra = 0)
{
	return ((op7 & 0x7Fu) << 25)
	     | ((ft & 0x1Fu) << 16)
	     | ((fs & 0x1Fu) << 11)
	     | ((fd_or_imm & 0x1Fu) <<  6)
	     | (fields_24_to_0_extra & 0x3Fu);
}

// IADDIU — `it = is + uimm15`. Primary 0x08. Operand layout (per VUops.cpp
// _vuIADDIU at line 1033): `imm15 = ((code >> 10) & 0x7800) | (code & 0x7FF)`
// — split between bits[14:11] (high 4 bits) and bits[10:0] (low 11 bits).
constexpr u32 VIADDIU_L(u32 it, u32 is, u32 uimm15)
{
	const u32 hi4  = (uimm15 >> 11) & 0xFu;
	const u32 lo11 = uimm15 & 0x7FFu;
	return (0x08u << 25) | ((it & 0x1Fu) << 16) | ((is & 0x1Fu) << 11) | (hi4 << 21) | lo11;
}

// ILW — load from VU mem to VI register. Primary 0x04, imm11 in bits[10:0]
// with sign extension via bit[10]. Operand layout per _vuILW VUops.cpp:1150.
constexpr u32 VILW_L(u32 mask_xyzw, u32 it, u32 is, s16 imm11)
{
	const u32 imm = static_cast<u32>(imm11) & 0x7FFu;
	return (0x04u << 25) | (mask_xyzw & 0x1E00000u)
	     | ((it & 0x1Fu) << 16) | ((is & 0x1Fu) << 11) | imm;
}

// ISW — store VI register to VU mem. Primary 0x05.
constexpr u32 VISW_L(u32 mask_xyzw, u32 it, u32 is, s16 imm11)
{
	const u32 imm = static_cast<u32>(imm11) & 0x7FFu;
	return (0x05u << 25) | (mask_xyzw & 0x1E00000u)
	     | ((it & 0x1Fu) << 16) | ((is & 0x1Fu) << 11) | imm;
}

// LQ / SQ — 128-bit load/store between VU mem and VF register. Primary 0x00 / 0x01.
constexpr u32 VLQ_L(u32 mask_xyzw, u32 ft, u32 is, s16 imm11)
{
	const u32 imm = static_cast<u32>(imm11) & 0x7FFu;
	return (0x00u << 25) | (mask_xyzw & 0x1E00000u)
	     | ((ft & 0x1Fu) << 16) | ((is & 0x1Fu) << 11) | imm;
}
constexpr u32 VSQ_L(u32 mask_xyzw, u32 fs, u32 it, s16 imm11)
{
	const u32 imm = static_cast<u32>(imm11) & 0x7FFu;
	return (0x01u << 25) | (mask_xyzw & 0x1E00000u)
	     | ((it & 0x1Fu) << 16) | ((fs & 0x1Fu) << 11) | imm;
}

// LowerOP family — primary opcode 0x40, sub-op in bits[5:0] (LowerOP_OPCODE),
// and for sub-ops 0x3C..0x3F a secondary in bits[10:6] (LowerOP_T3_xx_OPCODE).
//
// LowerOP_OPCODE (bits[5:0]):
//   0x30 IADD  0x31 ISUB  0x32 IADDI  0x34 IAND  0x35 IOR
//   0x3C LowerOP_T3_00  0x3D _T3_01  0x3E _T3_10  0x3F _T3_11
//
// LowerOP_T3_xx (sub-op in bits[10:6]) — VMOVE/VMR32, VLQI/VSQI, VDIV/VSQRT/VRSQRT/VWAITQ
// VMTIR/VMFIR, VMFP, VITOFx/VFTOIx, RAND/REGS, EFU ops, XGKICK/XITOP/XTOP. See
// VUops.cpp:3607-3650 for the full table.

// Direct sub-op assembler — pass the secondary in `sec5` (bits[10:6]) and the
// T3 selector (0x3C..0x3F) explicitly.
constexpr u32 BuildLowerT3(u32 t3_sel, u32 sec5, u32 ft, u32 fs, u32 mask_xyzw_or_zero,
                            u32 fsf = 0, u32 ftf = 0)
{
	return (0x40u << 25)
	     | (mask_xyzw_or_zero & 0x1E00000u)
	     | ((ftf & 0x3u) << 23)
	     | ((fsf & 0x3u) << 21)
	     | ((ft & 0x1Fu) << 16)
	     | ((fs & 0x1Fu) << 11)
	     | ((sec5 & 0x1Fu) << 6)
	     | (t3_sel & 0x3Fu);
}

// VMOVE: ft = fs (per-lane, masked). T3_00 sub 0x0C.
constexpr u32 VMOVE_L(u32 mask_xyzw, u32 ft, u32 fs)
{
	return BuildLowerT3(0x3C, 0x0C, ft, fs, mask_xyzw);
}

// VMR32: rotates fs xyzw -> ft yzwx (lanes shifted by 1). T3_01 sub 0x0C.
constexpr u32 VMR32_L(u32 mask_xyzw, u32 ft, u32 fs)
{
	return BuildLowerT3(0x3D, 0x0C, ft, fs, mask_xyzw);
}

// VMFIR: ft.{xyzw masked} = sign_extend_s32(VI[is]). T3_01 sub 0x0F.
constexpr u32 VMFIR_L(u32 mask_xyzw, u32 ft, u32 is)
{
	return BuildLowerT3(0x3D, 0x0F, ft, is, mask_xyzw);
}

// VMTIR: VI[it].US[0] = u16(fs.f[fsf]). T3_00 sub 0x0F. Uses Fsf.
constexpr u32 VMTIR_L(u32 it, u32 fs, u32 fsf)
{
	return BuildLowerT3(0x3C, 0x0F, it, fs, /*mask*/0, fsf);
}

// Q-pipe primary scalar ops:
//   VDIV   T3_00 sub 0x0E — q = fs[fsf] / ft[ftf]
//   VSQRT  T3_01 sub 0x0E — q = sqrt(|ft[ftf]|)
//   VRSQRT T3_10 sub 0x0E — q = fs[fsf] / sqrt(|ft[ftf]|)
//   VWAITQ T3_11 sub 0x0E — stall until Q-pipe completes
constexpr u32 VDIV_L  (u32 fs, u32 fsf, u32 ft, u32 ftf) { return BuildLowerT3(0x3C, 0x0E, ft, fs, /*mask*/0, fsf, ftf); }
constexpr u32 VSQRT_L (u32 ft, u32 ftf)                 { return BuildLowerT3(0x3D, 0x0E, ft, /*fs*/0, /*mask*/0, /*fsf*/0, ftf); }
constexpr u32 VRSQRT_L(u32 fs, u32 fsf, u32 ft, u32 ftf) { return BuildLowerT3(0x3E, 0x0E, ft, fs, /*mask*/0, fsf, ftf); }
constexpr u32 VWAITQ_L() { return BuildLowerT3(0x3F, 0x0E, 0, 0, 0); }

// VLQI: ft = MEM[VI[is]*16]; VI[is]++. T3_00 sub 0x0D. Mask diffs the loaded lanes.
constexpr u32 VLQI_L(u32 mask_xyzw, u32 ft, u32 is) { return BuildLowerT3(0x3C, 0x0D, ft, is, mask_xyzw); }
constexpr u32 VSQI_L(u32 mask_xyzw, u32 fs, u32 it) { return BuildLowerT3(0x3D, 0x0D, it, fs, mask_xyzw); }
constexpr u32 VLQD_L(u32 mask_xyzw, u32 ft, u32 is) { return BuildLowerT3(0x3E, 0x0D, ft, is, mask_xyzw); }
constexpr u32 VSQD_L(u32 mask_xyzw, u32 fs, u32 it) { return BuildLowerT3(0x3F, 0x0D, it, fs, mask_xyzw); }

// EFU P-pipeline ops — VU1-only (EFU not present on VU0). All write to the
// architectural P scalar, which `mVUendProgram` commits to VI[REG_P] at
// E-bit. VWAITP stalls until the P-pipeline drains. Per VUops.cpp:1652-1805,
// EVERY EFU op reads from VF[_Fs_] (lane indexed by _Fsf_ where applicable).
// Dispatch table encodings:
//   T3_00 sec 0x1C VESADD   (Fs xyz)
//   T3_00 sec 0x1D VEATANxy (Fs xy)
//   T3_00 sec 0x1E VESQRT   (Fs + Fsf)
//   T3_00 sec 0x1F VESIN    (Fs + Fsf)
//   T3_01 sec 0x1C VERSADD  (Fs xyz)
//   T3_01 sec 0x1D VEATANxz (Fs xz)
//   T3_01 sec 0x1E VERSQRT  (Fs + Fsf)  ← only Fsf, despite the function reading Ft fields too
//   T3_01 sec 0x1F VEATAN   (Fs + Fsf)
//   T3_10 sec 0x1C VELENG   (Fs xyz)
//   T3_10 sec 0x1D VESUM    (Fs xyzw)
//   T3_10 sec 0x1E VERCPR   (Fs + Fsf)
//   T3_10 sec 0x1F VEEXP    (Fs + Fsf)
//   T3_11 sec 0x1C VERLENG  (Fs xyz)
//   T3_11 sec 0x1E VWAITP   (no operands)
constexpr u32 VESADD_L  (u32 fs)                  { return BuildLowerT3(0x3C, 0x1C, 0, fs, 0); }
constexpr u32 VEATANXY_L(u32 fs)                  { return BuildLowerT3(0x3C, 0x1D, 0, fs, 0); }
constexpr u32 VESQRT_L  (u32 fs, u32 fsf)         { return BuildLowerT3(0x3C, 0x1E, 0, fs, 0, fsf, 0); }
constexpr u32 VESIN_L   (u32 fs, u32 fsf)         { return BuildLowerT3(0x3C, 0x1F, 0, fs, 0, fsf, 0); }
constexpr u32 VERSADD_L (u32 fs)                  { return BuildLowerT3(0x3D, 0x1C, 0, fs, 0); }
constexpr u32 VEATANXZ_L(u32 fs)                  { return BuildLowerT3(0x3D, 0x1D, 0, fs, 0); }
constexpr u32 VERSQRT_L (u32 fs, u32 fsf)         { return BuildLowerT3(0x3D, 0x1E, 0, fs, 0, fsf, 0); }
constexpr u32 VEATAN_L  (u32 fs, u32 fsf)         { return BuildLowerT3(0x3D, 0x1F, 0, fs, 0, fsf, 0); }
constexpr u32 VELENG_L  (u32 fs)                  { return BuildLowerT3(0x3E, 0x1C, 0, fs, 0); }
constexpr u32 VESUM_L   (u32 fs)                  { return BuildLowerT3(0x3E, 0x1D, 0, fs, 0); }
constexpr u32 VERCPR_L  (u32 fs, u32 fsf)         { return BuildLowerT3(0x3E, 0x1E, 0, fs, 0, fsf, 0); }
constexpr u32 VEEXP_L   (u32 fs, u32 fsf)         { return BuildLowerT3(0x3E, 0x1F, 0, fs, 0, fsf, 0); }
constexpr u32 VERLENG_L (u32 fs)                  { return BuildLowerT3(0x3F, 0x1C, 0, fs, 0); }
constexpr u32 VWAITP_L  ()                        { return BuildLowerT3(0x3F, 0x1E, 0, 0, 0); }

// VU1-only XGKICK — T3_00 sub 0x1B (per VUops.cpp:3614, table index 27).
// Reads the current GIF-target address from VI[is], drains it into the GIF
// Path 1 stream.
constexpr u32 VXGKICK_L(u32 is) { return BuildLowerT3(0x3C, 0x1B, 0, is, 0); }

// LowerOP integer ALU — primary 0x40, sub-op in bits[5:0]. Uses Id/Is/It (low
// 4 bits of the FD/FS/FT fields, picking VI[0..15]).
constexpr u32 BuildLowerInt(u32 sub6, u32 id, u32 is, u32 it)
{
	return (0x40u << 25)
	     | ((it & 0x1Fu) << 16)
	     | ((is & 0x1Fu) << 11)
	     | ((id & 0x1Fu) << 6)
	     | (sub6 & 0x3Fu);
}
constexpr u32 VIADD_L(u32 id, u32 is, u32 it) { return BuildLowerInt(0x30, id, is, it); }
constexpr u32 VISUB_L(u32 id, u32 is, u32 it) { return BuildLowerInt(0x31, id, is, it); }
constexpr u32 VIAND_L(u32 id, u32 is, u32 it) { return BuildLowerInt(0x34, id, is, it); }
constexpr u32 VIOR_L (u32 id, u32 is, u32 it) { return BuildLowerInt(0x35, id, is, it); }

// VIADDI: it = is + sext5(imm5). Imm in bits[10:6] (FD field), sign-extended via bit[10].
// Per _vuIADDI VUops.cpp:1014.
constexpr u32 VIADDI_L(u32 it, u32 is, s32 simm5)
{
	const u32 imm5 = static_cast<u32>(simm5) & 0x1Fu;
	return (0x40u << 25) | ((it & 0x1Fu) << 16) | ((is & 0x1Fu) << 11) | (imm5 << 6) | 0x32u;
}

// VISUBIU: it = is - imm15. Primary 0x09. Same imm15 split as VIADDIU.
constexpr u32 VISUBIU_L(u32 it, u32 is, u32 uimm15)
{
	const u32 hi4  = (uimm15 >> 11) & 0xFu;
	const u32 lo11 = uimm15 & 0x7FFu;
	return (0x09u << 25) | ((it & 0x1Fu) << 16) | ((is & 0x1Fu) << 11) | (hi4 << 21) | lo11;
}

// Branches — primary opcode in bits[31:25]. Imm11 in bits[10:0] is signed,
// units of pairs (8 bytes), relative to VI[REG_TPC]+8 per `_branchAddr`.
constexpr u32 BuildBranch11(u32 op7, u32 it, u32 is, s16 imm11_pairs)
{
	const u32 imm = static_cast<u32>(imm11_pairs) & 0x7FFu;
	return ((op7 & 0x7Fu) << 25)
	     | ((it & 0x1Fu) << 16) | ((is & 0x1Fu) << 11) | imm;
}
constexpr u32 VB_L     (s16 imm11) { return BuildBranch11(0x20, 0,  0,  imm11); }
constexpr u32 VBAL_L   (u32 it, s16 imm11) { return BuildBranch11(0x21, it, 0,  imm11); }
constexpr u32 VJR_L    (u32 is)                   { return BuildBranch11(0x24, 0,  is, 0); }
constexpr u32 VJALR_L  (u32 it, u32 is)           { return BuildBranch11(0x25, it, is, 0); }
constexpr u32 VIBEQ_L  (u32 it, u32 is, s16 imm11){ return BuildBranch11(0x28, it, is, imm11); }
constexpr u32 VIBNE_L  (u32 it, u32 is, s16 imm11){ return BuildBranch11(0x29, it, is, imm11); }
constexpr u32 VIBLTZ_L (u32 is, s16 imm11)        { return BuildBranch11(0x2C, 0,  is, imm11); }
constexpr u32 VIBGTZ_L (u32 is, s16 imm11)        { return BuildBranch11(0x2D, 0,  is, imm11); }
constexpr u32 VIBLEZ_L (u32 is, s16 imm11)        { return BuildBranch11(0x2E, 0,  is, imm11); }
constexpr u32 VIBGEZ_L (u32 is, s16 imm11)        { return BuildBranch11(0x2F, 0,  is, imm11); }

// Flag ops — primary opcode in bits[31:25].
//   FCEQ/FCSET/FCAND/FCOR — 24-bit imm in bits[23:0].
//   FSEQ/FSAND/FSOR/FSSET — 12-bit imm: bits[10:0] | (bit[21] << 11). VUops.cpp:1352.
//   FMEQ/FMAND/FMOR — register-form (it, is). FCGET — register-form (it).
constexpr u32 BuildFlagImm24(u32 op7, u32 imm24)
{
	return ((op7 & 0x7Fu) << 25) | (imm24 & 0xFFFFFFu);
}
constexpr u32 BuildFlagImm12(u32 op7, u32 it, u32 imm12)
{
	const u32 lo11  = imm12 & 0x7FFu;
	const u32 bit11 = (imm12 >> 11) & 0x1u;
	return ((op7 & 0x7Fu) << 25) | ((it & 0x1Fu) << 16) | (bit11 << 21) | lo11;
}
constexpr u32 BuildFlagReg(u32 op7, u32 it, u32 is)
{
	return ((op7 & 0x7Fu) << 25) | ((it & 0x1Fu) << 16) | ((is & 0x1Fu) << 11);
}
constexpr u32 VFCEQ_L (u32 imm24) { return BuildFlagImm24(0x10, imm24); }
constexpr u32 VFCSET_L(u32 imm24) { return BuildFlagImm24(0x11, imm24); }
constexpr u32 VFCAND_L(u32 imm24) { return BuildFlagImm24(0x12, imm24); }
constexpr u32 VFCOR_L (u32 imm24) { return BuildFlagImm24(0x13, imm24); }
constexpr u32 VFSEQ_L (u32 it, u32 imm12) { return BuildFlagImm12(0x14, it, imm12); }
constexpr u32 VFSSET_L(u32 imm12) { return BuildFlagImm12(0x15, 0, imm12); }
constexpr u32 VFSAND_L(u32 it, u32 imm12) { return BuildFlagImm12(0x16, it, imm12); }
constexpr u32 VFSOR_L (u32 it, u32 imm12) { return BuildFlagImm12(0x17, it, imm12); }
constexpr u32 VFMEQ_L (u32 it, u32 is)    { return BuildFlagReg (0x18, it, is); }
constexpr u32 VFMAND_L(u32 it, u32 is)    { return BuildFlagReg (0x1A, it, is); }
constexpr u32 VFMOR_L (u32 it, u32 is)    { return BuildFlagReg (0x1B, it, is); }
constexpr u32 VFCGET_L(u32 it)            { return BuildFlagReg (0x1C, it, 0); }

// ---------------------------------------------------------------------------
//  Upper accumulator-target ops — UPPER_FD_xx tables. Used for flag-pipeline
//  and ACC chaining tests. Encoded as primary 0x3F+T3_xx via the FD field
//  acting as a sub-op selector.
// ---------------------------------------------------------------------------
constexpr u32 UpperFD(u32 mask_xyzw, u32 ft, u32 fs, u32 fd_subop, u32 fd_table_sel)
{
	// Primary opcodes 0x3C..0x3F = UPPER_FD_00..11. fd_subop occupies bits[10:6].
	return (mask_xyzw & 0x1E00000u)
	     | ((ft & 0x1Fu) << 16)
	     | ((fs & 0x1Fu) << 11)
	     | ((fd_subop & 0x1Fu) << 6)
	     | (0x3Cu | (fd_table_sel & 0x3u));
}

// ACC-target FMACs (write to ACC, not FD). Per VUops.cpp:3705-3747 the four
// FD_xx tables hold:
//   FD_00 0x0A=ADDA   0x0B=SUBA   0x04=ITOF0  0x05=FTOI0   0x06=MULAx   …
//   FD_01 0x0A=MADDA  0x0B=MSUBA  0x04=ITOF4  0x05=FTOI4   0x07=ABS     …
//   FD_10 0x0A=MULA   0x0B=OPMULA 0x04=ITOF12 0x05=FTOI12  …
//   FD_11 0x0A=unkn   0x0B=NOP    0x04=ITOF15 0x05=FTOI15  0x07=CLIP
constexpr u32 VADDA_U  (u32 mask_xyzw, u32 fs, u32 ft) { return UpperFD(mask_xyzw, ft, fs, 0x0A, 0); }
constexpr u32 VSUBA_U  (u32 mask_xyzw, u32 fs, u32 ft) { return UpperFD(mask_xyzw, ft, fs, 0x0B, 0); }
constexpr u32 VMADDA_U (u32 mask_xyzw, u32 fs, u32 ft) { return UpperFD(mask_xyzw, ft, fs, 0x0A, 1); }
constexpr u32 VMSUBA_U (u32 mask_xyzw, u32 fs, u32 ft) { return UpperFD(mask_xyzw, ft, fs, 0x0B, 1); }
constexpr u32 VMULA_U  (u32 mask_xyzw, u32 fs, u32 ft) { return UpperFD(mask_xyzw, ft, fs, 0x0A, 2); }
constexpr u32 VOPMULA_U(u32 fs, u32 ft)                { return UpperFD(mask::xyz, ft, fs, 0x0B, 2); }

// Broadcast ACC-target FMACs (ACC ← op(FS, FT.bc)). bc selects the FD table
// (fd_table_sel: 0=x 1=y 2=z 3=w); sub-op index within every table per
// VUops.cpp:3705-3747: [0]=ADDAbc [1]=SUBAbc [2]=MADDAbc [3]=MSUBAbc
// [6]=MULAbc.
constexpr u32 VMULAx_U (u32 mask_xyzw, u32 fs, u32 ft) { return UpperFD(mask_xyzw, ft, fs, 0x06, 0); }
constexpr u32 VMULAy_U (u32 mask_xyzw, u32 fs, u32 ft) { return UpperFD(mask_xyzw, ft, fs, 0x06, 1); }
constexpr u32 VMULAz_U (u32 mask_xyzw, u32 fs, u32 ft) { return UpperFD(mask_xyzw, ft, fs, 0x06, 2); }
constexpr u32 VMULAw_U (u32 mask_xyzw, u32 fs, u32 ft) { return UpperFD(mask_xyzw, ft, fs, 0x06, 3); }
constexpr u32 VMADDAx_U(u32 mask_xyzw, u32 fs, u32 ft) { return UpperFD(mask_xyzw, ft, fs, 0x02, 0); }
constexpr u32 VMADDAy_U(u32 mask_xyzw, u32 fs, u32 ft) { return UpperFD(mask_xyzw, ft, fs, 0x02, 1); }
constexpr u32 VMADDAz_U(u32 mask_xyzw, u32 fs, u32 ft) { return UpperFD(mask_xyzw, ft, fs, 0x02, 2); }
constexpr u32 VMADDAw_U(u32 mask_xyzw, u32 fs, u32 ft) { return UpperFD(mask_xyzw, ft, fs, 0x02, 3); }
constexpr u32 VMSUBAx_U(u32 mask_xyzw, u32 fs, u32 ft) { return UpperFD(mask_xyzw, ft, fs, 0x03, 0); }
constexpr u32 VMSUBAy_U(u32 mask_xyzw, u32 fs, u32 ft) { return UpperFD(mask_xyzw, ft, fs, 0x03, 1); }
constexpr u32 VMSUBAz_U(u32 mask_xyzw, u32 fs, u32 ft) { return UpperFD(mask_xyzw, ft, fs, 0x03, 2); }
constexpr u32 VMSUBAw_U(u32 mask_xyzw, u32 fs, u32 ft) { return UpperFD(mask_xyzw, ft, fs, 0x03, 3); }

// Fixed-point conversion families — VFTOI{0,4,12,15} truncate fs * 2^N → s32;
// VITOF{0,4,12,15} convert s32 → fs / 2^N. All masked per dest lane.
constexpr u32 VITOF0_U  (u32 mask_xyzw, u32 ft, u32 fs) { return UpperFD(mask_xyzw, ft, fs, 0x04, 0); }
constexpr u32 VITOF4_U  (u32 mask_xyzw, u32 ft, u32 fs) { return UpperFD(mask_xyzw, ft, fs, 0x04, 1); }
constexpr u32 VITOF12_U (u32 mask_xyzw, u32 ft, u32 fs) { return UpperFD(mask_xyzw, ft, fs, 0x04, 2); }
constexpr u32 VITOF15_U (u32 mask_xyzw, u32 ft, u32 fs) { return UpperFD(mask_xyzw, ft, fs, 0x04, 3); }
constexpr u32 VFTOI0_U  (u32 mask_xyzw, u32 ft, u32 fs) { return UpperFD(mask_xyzw, ft, fs, 0x05, 0); }
constexpr u32 VFTOI4_U  (u32 mask_xyzw, u32 ft, u32 fs) { return UpperFD(mask_xyzw, ft, fs, 0x05, 1); }
constexpr u32 VFTOI12_U (u32 mask_xyzw, u32 ft, u32 fs) { return UpperFD(mask_xyzw, ft, fs, 0x05, 2); }
constexpr u32 VFTOI15_U (u32 mask_xyzw, u32 ft, u32 fs) { return UpperFD(mask_xyzw, ft, fs, 0x05, 3); }

// VABS — FD_01 sub 0x07.
constexpr u32 VABS_U(u32 mask_xyzw, u32 ft, u32 fs) { return UpperFD(mask_xyzw, ft, fs, 0x07, 1); }

// VCLIP — UPPER_FD_11 sub 0x07. Operands fs (xyz) tested against ft.w; result
// folded into VI[REG_CLIP_FLAG] as a 24-bit rolling history (no FD writeback).
constexpr u32 VCLIP_U(u32 fs, u32 ft) { return UpperFD(0, ft, fs, 0x07, 3); }

// Lower NOP isn't a discrete opcode. Instead, set the I-bit on the upper word
// and the lower word becomes a 32-bit float immediate that loads into
// VI[REG_I] (see VU0microInterp.cpp _vu0Exec lines 113-127). Use `VLitI(value)`
// when you want a known I value, or `VLitZero()` for a zero filler.
constexpr u32 VLitI(u32 imm32) { return imm32; }
constexpr u32 VLitZero()       { return 0u; }

// ---------------------------------------------------------------------------
//  Convenience pair builders
// ---------------------------------------------------------------------------

// Pure NOP pair. Sets I-bit so the lower is interpreted as a float immediate
// (VI[REG_I] ← 0); upper is the architectural NOP from UPPER_FD_11_TABLE.
constexpr VuOp NopPair()
{
	return IBit(VuOp{VLitZero(), VNOP_U()});
}

// E-bit-terminated NOP pair. The interpreter's E-bit cleanup runs one
// instruction-pair AFTER the E-bit pair, so any E-bit-terminated test
// program must include a follow-up pair as the architectural delay slot.
// The harness's `LoadProgram` appends `NopPair()` automatically when the
// last user-supplied pair carries the E bit.
constexpr VuOp EBitNopPair()
{
	return EBit(NopPair());
}

} // namespace vu
