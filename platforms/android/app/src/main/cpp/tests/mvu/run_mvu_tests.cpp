// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "run_mvu_tests.h"
#include "mvu_test_api.h"
#include "../test_bridge.h"

#include <android/log.h>
#include <cstring>
#include <initializer_list>
#include <utility>

#include "pcsx2/Config.h"
#include "pcsx2/Memory.h"
#include "pcsx2/VU.h"
#include "pcsx2/Vif.h"
#include "common/FPControl.h"

#define TAG "VuJitTests"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ──────────────────────────────────────────────────────────────
// VU0 instruction encoding helpers
// ──────────────────────────────────────────────────────────────
//
// Each VU0 instruction pair is 8 bytes:
//   words[n*2 + 0] = lower word  (integer / branch / misc ops)
//   words[n*2 + 1] = upper word  (floating-point ops)
//
// Dispatch:
//   lower: code >> 25 → mVULOWER_OPCODE[128]
//     if index == 0x40: code & 0x3f → mVULowerOP_OPCODE[64]
//   upper: code & 0x3f → mVU_UPPER_OPCODE[64]
//
// E-bit (bit 30 of upper word): terminates block after one delay slot pair.

static constexpr u32 LOWER_NOP      = 0x80000032u; // IADDI VI[0],VI[0],0 → no-op
static constexpr u32 UPPER_NOP      = 0x000002FFu; // UPPER_FD_11[0xB] = NOP
static constexpr u32 UPPER_EBIT_NOP = 0x400002FFu; // E-bit | NOP

// ── LowerOP (bits[31:25] = 0x40) ────────────────────────────
// IADDI  VI[dest] = VI[src] + sign_ext(imm5)
static constexpr u32 lower_iaddi(u32 dest, u32 src, u32 imm5)
{
    return (0x40u << 25) | ((dest & 0xFu) << 16) | ((src & 0xFu) << 11)
         | ((imm5 & 0x1Fu) << 6) | 0x32u;
}

// IADD   VI[id] = VI[is] + VI[it]
static constexpr u32 lower_iadd(u32 id, u32 is, u32 it)
{
    return (0x40u << 25) | ((it & 0xFu) << 16) | ((is & 0xFu) << 11)
         | ((id & 0xFu) << 6) | 0x30u;
}

// ISUB   VI[id] = VI[is] - VI[it]
static constexpr u32 lower_isub(u32 id, u32 is, u32 it)
{
    return (0x40u << 25) | ((it & 0xFu) << 16) | ((is & 0xFu) << 11)
         | ((id & 0xFu) << 6) | 0x31u;
}

// IAND   VI[id] = VI[is] & VI[it]
static constexpr u32 lower_iand(u32 id, u32 is, u32 it)
{
    return (0x40u << 25) | ((it & 0xFu) << 16) | ((is & 0xFu) << 11)
         | ((id & 0xFu) << 6) | 0x34u;
}

// IOR    VI[id] = VI[is] | VI[it]
static constexpr u32 lower_ior(u32 id, u32 is, u32 it)
{
    return (0x40u << 25) | ((it & 0xFu) << 16) | ((is & 0xFu) << 11)
         | ((id & 0xFu) << 6) | 0x35u;
}

// ── Direct opcode instructions ────────────────────────────────
// IADDIU VI[dest] = VI[src] + imm15  (bits[31:25] = 0x08)
//   imm15 scattered: bits[24:21] = imm15[14:11], bits[10:0] = imm15[10:0]
static constexpr u32 lower_iaddiu(u32 dest, u32 src, u32 imm15)
{
    return (0x08u << 25) | (((imm15 >> 11) & 0xFu) << 21)
         | ((dest & 0xFu) << 16) | ((src & 0xFu) << 11) | (imm15 & 0x7FFu);
}

// ISUBIU VI[dest] = VI[src] - imm15  (bits[31:25] = 0x09)
static constexpr u32 lower_isubiu(u32 dest, u32 src, u32 imm15)
{
    return (0x09u << 25) | (((imm15 >> 11) & 0xFu) << 21)
         | ((dest & 0xFu) << 16) | ((src & 0xFu) << 11) | (imm15 & 0x7FFu);
}

// IBNE   if (VI[is] != VI[it]) branch  (bits[31:25] = 0x29)
//   branchAddr = ((iPC + 2 + imm11*2) & mask) * 4
static constexpr u32 lower_ibne(u32 is, u32 it, s32 imm11)
{
    return (0x29u << 25) | ((it & 0xFu) << 16) | ((is & 0xFu) << 11)
         | (static_cast<u32>(imm11) & 0x7FFu);
}

// ──────────────────────────────────────────────────────────────
// Upper-word instruction encoders
// ──────────────────────────────────────────────────────────────
//
// General upper-word format:
//   bits[30]    = E-bit (end-of-block; delay slot follows)
//   bits[24:21] = DEST (x=bit3, y=bit2, z=bit1, w=bit0)
//   bits[20:16] = FT   (source/dest register)
//   bits[15:11] = FS   (source register)
//   bits[10:6]  = FD   (dest register, or sub-opcode for FD_xx)
//   bits[5:0]   = opcode

// DEST field bitmasks (bit3=X, bit2=Y, bit1=Z, bit0=W)
static constexpr u32 DEST_XYZW = 0xFu;
static constexpr u32 DEST_X    = 0x8u;
static constexpr u32 DEST_Y    = 0x4u;
static constexpr u32 DEST_Z    = 0x2u;
static constexpr u32 DEST_W    = 0x1u;

static constexpr u32 upper_op(u32 opcode, u32 fd, u32 fs, u32 ft, u32 dest = DEST_XYZW)
{
    return ((dest & 0xFu) << 21) | ((ft & 0xFu) << 16) | ((fs & 0xFu) << 11)
         | ((fd & 0xFu) << 6)   | (opcode & 0x3Fu);
}

// Set E-bit (end-of-block) on any upper word
static constexpr u32 ebit(u32 w) { return w | (1u << 30); }

// opcode 0x28: ADD.dest FD, FS, FT
static constexpr u32 upper_add(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x28u, fd, fs, ft, d); }
// opcode 0x2C: SUB.dest FD, FS, FT
static constexpr u32 upper_sub(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x2Cu, fd, fs, ft, d); }
// opcode 0x2A: MUL.dest FD, FS, FT
static constexpr u32 upper_mul(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x2Au, fd, fs, ft, d); }
// opcode 0x2B: MAX.dest FD, FS, FT
static constexpr u32 upper_max(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x2Bu, fd, fs, ft, d); }
// opcode 0x2F: MINI.dest FD, FS, FT
static constexpr u32 upper_mini(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x2Fu, fd, fs, ft, d); }
// Broadcast ADD: opcodes 0x00–0x03 (FT.xyzw broadcast to all lanes)
static constexpr u32 upper_addx(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x00u, fd, fs, ft, d); }
static constexpr u32 upper_addy(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x01u, fd, fs, ft, d); }
static constexpr u32 upper_addz(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x02u, fd, fs, ft, d); }
static constexpr u32 upper_addw(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x03u, fd, fs, ft, d); }
// Broadcast SUB: opcodes 0x04–0x07
static constexpr u32 upper_subx(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x04u, fd, fs, ft, d); }
static constexpr u32 upper_suby(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x05u, fd, fs, ft, d); }
static constexpr u32 upper_subz(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x06u, fd, fs, ft, d); }
static constexpr u32 upper_subw(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x07u, fd, fs, ft, d); }
// Broadcast MADD: opcodes 0x08–0x0B
// FD.dest = ACC.dest + FS.dest * FT.{x,y,z,w}
static constexpr u32 upper_maddx(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x08u, fd, fs, ft, d); }
static constexpr u32 upper_maddy(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x09u, fd, fs, ft, d); }
static constexpr u32 upper_maddz(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x0Au, fd, fs, ft, d); }
static constexpr u32 upper_maddw(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x0Bu, fd, fs, ft, d); }
// Broadcast MSUB: opcodes 0x0C–0x0F
// FD.dest = ACC.dest - FS.dest * FT.{x,y,z,w}
static constexpr u32 upper_msubx(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x0Cu, fd, fs, ft, d); }
static constexpr u32 upper_msuby(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x0Du, fd, fs, ft, d); }
static constexpr u32 upper_msubz(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x0Eu, fd, fs, ft, d); }
static constexpr u32 upper_msubw(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x0Fu, fd, fs, ft, d); }
// Broadcast MUL: opcodes 0x18–0x1B
static constexpr u32 upper_mulx(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x18u, fd, fs, ft, d); }
static constexpr u32 upper_muly(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x19u, fd, fs, ft, d); }
static constexpr u32 upper_mulz(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x1Au, fd, fs, ft, d); }
static constexpr u32 upper_mulw(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x1Bu, fd, fs, ft, d); }
// Broadcast MAX: opcodes 0x10–0x13; MAXi 0x1D
static constexpr u32 upper_maxx(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x10u, fd, fs, ft, d); }
static constexpr u32 upper_maxy(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x11u, fd, fs, ft, d); }
static constexpr u32 upper_maxz(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x12u, fd, fs, ft, d); }
static constexpr u32 upper_maxw(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x13u, fd, fs, ft, d); }
static constexpr u32 upper_maxi(u32 fd, u32 fs, u32 d = DEST_XYZW)
    { return upper_op(0x1Du, fd, fs, 0, d); }
// Broadcast MINI: opcodes 0x14–0x17; MINIi 0x1F
static constexpr u32 upper_minix(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x14u, fd, fs, ft, d); }
static constexpr u32 upper_miniy(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x15u, fd, fs, ft, d); }
static constexpr u32 upper_miniz(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x16u, fd, fs, ft, d); }
static constexpr u32 upper_miniw(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x17u, fd, fs, ft, d); }
static constexpr u32 upper_minii(u32 fd, u32 fs, u32 d = DEST_XYZW)
    { return upper_op(0x1Fu, fd, fs, 0, d); }

// ABS: FT.dest = |FS|  (UPPER_FD_01 opcode=0x3D, sub-index 7)
// FT = destination register, FS = source register
static constexpr u32 upper_abs(u32 ft, u32 fs, u32 d = DEST_XYZW)
{
    return ((d & 0xFu) << 21) | ((ft & 0xFu) << 16) | ((fs & 0xFu) << 11)
         | (7u << 6) | 0x3Du;
}

// FTOI0: FT.dest = (s32)FS  (UPPER_FD_00 opcode=0x3C, sub-index 5)
// Converts float to signed integer (truncate toward zero, no fractional bits)
static constexpr u32 upper_ftoi0(u32 ft, u32 fs, u32 d = DEST_XYZW)
{
    return ((d & 0xFu) << 21) | ((ft & 0xFu) << 16) | ((fs & 0xFu) << 11)
         | (5u << 6) | 0x3Cu;
}

// ITOF0: FT.dest = (float)(s32)FS  (UPPER_FD_00 opcode=0x3C, sub-index 4)
// Converts signed integer to float (no fractional bits)
static constexpr u32 upper_itof0(u32 ft, u32 fs, u32 d = DEST_XYZW)
{
    return ((d & 0xFu) << 21) | ((ft & 0xFu) << 16) | ((fs & 0xFu) << 11)
         | (4u << 6) | 0x3Cu;
}

// ITOF4:  FT.dest = (float)(s32)FS / 16.0     (UPPER_FD_01 opcode=0x3D, sub-index 4)
// FTOI4:  FT.dest = (s32)(FS * 16.0)          (UPPER_FD_01 opcode=0x3D, sub-index 5)
// ITOF12: FT.dest = (float)(s32)FS / 4096.0   (UPPER_FD_10 opcode=0x3E, sub-index 4)
// FTOI12: FT.dest = (s32)(FS * 4096.0)        (UPPER_FD_10 opcode=0x3E, sub-index 5)
// ITOF15: FT.dest = (float)(s32)FS / 32768.0  (UPPER_FD_11 opcode=0x3F, sub-index 4)
// FTOI15: FT.dest = (s32)(FS * 32768.0)       (UPPER_FD_11 opcode=0x3F, sub-index 5)
static constexpr u32 upper_itof4(u32 ft, u32 fs, u32 d = DEST_XYZW)
    { return ((d&0xFu)<<21)|((ft&0xFu)<<16)|((fs&0xFu)<<11)|(4u<<6)|0x3Du; }
static constexpr u32 upper_ftoi4(u32 ft, u32 fs, u32 d = DEST_XYZW)
    { return ((d&0xFu)<<21)|((ft&0xFu)<<16)|((fs&0xFu)<<11)|(5u<<6)|0x3Du; }
static constexpr u32 upper_itof12(u32 ft, u32 fs, u32 d = DEST_XYZW)
    { return ((d&0xFu)<<21)|((ft&0xFu)<<16)|((fs&0xFu)<<11)|(4u<<6)|0x3Eu; }
static constexpr u32 upper_ftoi12(u32 ft, u32 fs, u32 d = DEST_XYZW)
    { return ((d&0xFu)<<21)|((ft&0xFu)<<16)|((fs&0xFu)<<11)|(5u<<6)|0x3Eu; }
static constexpr u32 upper_itof15(u32 ft, u32 fs, u32 d = DEST_XYZW)
    { return ((d&0xFu)<<21)|((ft&0xFu)<<16)|((fs&0xFu)<<11)|(4u<<6)|0x3Fu; }
static constexpr u32 upper_ftoi15(u32 ft, u32 fs, u32 d = DEST_XYZW)
    { return ((d&0xFu)<<21)|((ft&0xFu)<<16)|((fs&0xFu)<<11)|(5u<<6)|0x3Fu; }

// ── Lower-word VF encoders ────────────────────────────────────
// MOVE FT.dest = FS  (LowerOP → mVULowerOP_T3_00, sub-index 12)
//   bits[31:25]=0x40, bits[5:0]=0x3C → T3_00, bits[10:6]=12 → MOVE
static constexpr u32 lower_move(u32 ft, u32 fs, u32 dest = DEST_XYZW)
{
    return (0x40u << 25) | ((dest & 0xFu) << 21) | ((ft & 0xFu) << 16)
         | ((fs & 0xFu) << 11) | (12u << 6) | 0x3Cu;
}

// MR32 FT.dest = rotate(FS)  (LowerOP → T3_01, sub-index 12)
//   FT.xyzw = {FS.y, FS.z, FS.w, FS.x}  (PSHUFD 0x39 — rotate left by one lane)
static constexpr u32 lower_mr32(u32 ft, u32 fs, u32 dest = DEST_XYZW)
{
    return (0x40u << 25) | ((dest & 0xFu) << 21) | ((ft & 0xFu) << 16)
         | ((fs & 0xFu) << 11) | (12u << 6) | 0x3Du;
}

// MTIR VI[IT], VF[FS].fsf  (LowerOP → T3_00, sub-index 15)
//   VI[IT] = VF[FS].fsf bits[15:0]
//   fsf: 0=x, 1=y, 2=z, 3=w  (bits[22:21])
//   IT = dest VI reg (bits[20:16]), FS = source VF reg (bits[15:11])
static constexpr u32 lower_mtir(u32 it, u32 fs, u32 fsf)
{
    return (0x40u << 25) | ((fsf & 0x3u) << 21) | ((it & 0xFu) << 16)
         | ((fs & 0xFu) << 11) | (15u << 6) | 0x3Cu;
}

// MFIR VF[FT].dest, VI[IS]  (LowerOP → T3_01, sub-index 15)
//   VF[FT].dest = (s32)(s16)VI[IS]   — sign-extends 16-bit VI to 32-bit
//   IS = source VI reg (bits[15:11]), FT = dest VF reg (bits[20:16])
static constexpr u32 lower_mfir(u32 ft, u32 is, u32 dest = DEST_XYZW)
{
    return (0x40u << 25) | ((dest & 0xFu) << 21) | ((ft & 0xFu) << 16)
         | ((is & 0xFu) << 11) | (15u << 6) | 0x3Du;
}

// ── RNG register ops (lower word) ─────────────────────────────
// All four RNG ops dispatch via: bits[31:25]=0x40, bits[10:6]=16 (sub-index 16).
// Note: EFU ops (ESADD, ERSADD, ESQRT, ERSQRT, ESIN, EATAN, EEXP, ELENG,
// ERLENG, EATANXY, EATANXZ, ESUM, MFP, WAITP) are VU0 NOPs — untestable here.
//
// RNEXT FT.dest = R; R advances  (T3_00 sub-index 16, opcode 0x3C)
//   R = ((R<<1 ^ ((R>>4)^(R>>22))) & 0x7fffff) | 0x3f800000
static constexpr u32 lower_rnext(u32 ft, u32 dest = DEST_XYZW)
{
    return (0x40u << 25) | ((dest & 0xFu) << 21) | ((ft & 0xFu) << 16)
         | (16u << 6) | 0x3Cu;
}
// RGET FT.dest = R  (T3_01 sub-index 16, opcode 0x3D)  — R unchanged
static constexpr u32 lower_rget(u32 ft, u32 dest = DEST_XYZW)
{
    return (0x40u << 25) | ((dest & 0xFu) << 21) | ((ft & 0xFu) << 16)
         | (16u << 6) | 0x3Du;
}
// RINIT R = (VF[FS].fsf & 0x7fffff) | 0x3f800000  (T3_10 sub-index 16, opcode 0x3E)
//   fsf: 0=x, 1=y, 2=z, 3=w  (bits[23:22])
static constexpr u32 lower_rinit(u32 fs, u32 fsf)
{
    return (0x40u << 25) | ((fsf & 0x3u) << 22) | ((fs & 0xFu) << 11)
         | (16u << 6) | 0x3Eu;
}
// RXOR R ^= (VF[FS].fsf & 0x7fffff)  (T3_11 sub-index 16, opcode 0x3F)
static constexpr u32 lower_rxor(u32 fs, u32 fsf)
{
    return (0x40u << 25) | ((fsf & 0x3u) << 22) | ((fs & 0xFu) << 11)
         | (16u << 6) | 0x3Fu;
}

// ── Q pipeline ops (lower word) ───────────────────────────────
// All four dispatch via: bits[31:25]=0x40, bits[10:6]=14 (sub-index 14).
//
// DIV Q, VF[FS][fsf], VF[FT][ftf]: Q = FS[fsf] / FT[ftf]   [T3_00 sub-14]
//   FS=numerator, FT=denominator; fsf/ftf: 0=x,1=y,2=z,3=w; latency=7 cycles
static constexpr u32 lower_div(u32 fs, u32 fsf, u32 ft, u32 ftf)
{
    return (0x40u << 25) | ((ftf & 0x3u) << 23) | ((fsf & 0x3u) << 21)
         | ((ft & 0x1Fu) << 16) | ((fs & 0x1Fu) << 11) | (0x0Eu << 6) | 0x3Cu;
}
// SQRT Q, VF[FT][ftf]: Q = sqrt(FT[ftf])                    [T3_01 sub-14]
//   latency=7 cycles
static constexpr u32 lower_sqrt(u32 ft, u32 ftf)
{
    return (0x40u << 25) | ((ftf & 0x3u) << 23) | ((ft & 0x1Fu) << 16)
         | (0x0Eu << 6) | 0x3Du;
}
// RSQRT Q, VF[FS][fsf], VF[FT][ftf]: Q = FS[fsf]/sqrt(FT[ftf])  [T3_10 sub-14]
//   latency=13 cycles
static constexpr u32 lower_rsqrt(u32 fs, u32 fsf, u32 ft, u32 ftf)
{
    return (0x40u << 25) | ((ftf & 0x3u) << 23) | ((fsf & 0x3u) << 21)
         | ((ft & 0x1Fu) << 16) | ((fs & 0x1Fu) << 11) | (0x0Eu << 6) | 0x3Eu;
}
// WAITQ: stall until Q is ready                              [T3_11 sub-14]
static constexpr u32 lower_waitq()
    { return (0x40u << 25) | (0x0Eu << 6) | 0x3Fu; }

// ── Accumulator ops (upper word) ──────────────────────────────
//
// Non-broadcast (FD_00/01/10 sub-indices):
//   ADDA  FD_00[10]  ACC.dest  = FS.dest + FT.dest
//   SUBA  FD_00[11]  ACC.dest  = FS.dest - FT.dest
//   MULA  FD_10[10]  ACC.dest  = FS.dest * FT.dest
//   MADDA FD_01[10]  ACC.dest += FS.dest * FT.dest
//   MSUBA FD_01[11]  ACC.dest -= FS.dest * FT.dest
//
// Broadcast (component of FT broadcast to all lanes):
//   ADDAx/y/z/w   FD_00/01/10/11[0]   ACC.dest  = FS.dest + FT.{x,y,z,w}
//   SUBAx/y/z/w   FD_00/01/10/11[1]   ACC.dest  = FS.dest - FT.{x,y,z,w}
//   MULAx/y/z/w   FD_00/01/10/11[6]   ACC.dest  = FS.dest * FT.{x,y,z,w}
//   MADDAx/y/z/w  FD_00/01/10/11[2]   ACC.dest += FS.dest * FT.{x,y,z,w}
//   MSUBAx/y/z/w  FD_00/01/10/11[3]   ACC.dest -= FS.dest * FT.{x,y,z,w}

#define ACC_OP(sub, opc, fs, ft, d) \
    (((d)&0xFu)<<21)|(((ft)&0xFu)<<16)|(((fs)&0xFu)<<11)|((sub)<<6)|(opc)

static constexpr u32 upper_adda(u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return ACC_OP(10u, 0x3Cu, fs, ft, d); }
static constexpr u32 upper_suba(u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return ACC_OP(11u, 0x3Cu, fs, ft, d); }
static constexpr u32 upper_mula(u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return ACC_OP(10u, 0x3Eu, fs, ft, d); }
static constexpr u32 upper_madda(u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return ACC_OP(10u, 0x3Du, fs, ft, d); }
static constexpr u32 upper_msuba(u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return ACC_OP(11u, 0x3Du, fs, ft, d); }

static constexpr u32 upper_addax(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(0u, 0x3Cu, fs, ft, d); }
static constexpr u32 upper_adday(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(0u, 0x3Du, fs, ft, d); }
static constexpr u32 upper_addaz(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(0u, 0x3Eu, fs, ft, d); }
static constexpr u32 upper_addaw(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(0u, 0x3Fu, fs, ft, d); }

static constexpr u32 upper_subax(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(1u, 0x3Cu, fs, ft, d); }
static constexpr u32 upper_subay(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(1u, 0x3Du, fs, ft, d); }
static constexpr u32 upper_subaz(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(1u, 0x3Eu, fs, ft, d); }
static constexpr u32 upper_subaw(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(1u, 0x3Fu, fs, ft, d); }

static constexpr u32 upper_mulax(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(6u, 0x3Cu, fs, ft, d); }
static constexpr u32 upper_mulay(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(6u, 0x3Du, fs, ft, d); }
static constexpr u32 upper_mulaz(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(6u, 0x3Eu, fs, ft, d); }
static constexpr u32 upper_mulaw(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(6u, 0x3Fu, fs, ft, d); }

static constexpr u32 upper_maddax(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(2u, 0x3Cu, fs, ft, d); }
static constexpr u32 upper_madday(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(2u, 0x3Du, fs, ft, d); }
static constexpr u32 upper_maddaz(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(2u, 0x3Eu, fs, ft, d); }
static constexpr u32 upper_maddaw(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(2u, 0x3Fu, fs, ft, d); }

static constexpr u32 upper_msubax(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(3u, 0x3Cu, fs, ft, d); }
static constexpr u32 upper_msubay(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(3u, 0x3Du, fs, ft, d); }
static constexpr u32 upper_msubaz(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(3u, 0x3Eu, fs, ft, d); }
static constexpr u32 upper_msubaw(u32 fs, u32 ft, u32 d = DEST_XYZW) { return ACC_OP(3u, 0x3Fu, fs, ft, d); }

#undef ACC_OP
// MADD.dest FD, FS, FT  (opcode 0x29)
//   FD.dest = ACC.dest + FS.dest * FT.dest
static constexpr u32 upper_madd(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x29u, fd, fs, ft, d); }
// MSUB.dest FD, FS, FT  (opcode 0x2D)
//   FD.dest = ACC.dest - FS.dest * FT.dest
static constexpr u32 upper_msub(u32 fd, u32 fs, u32 ft, u32 d = DEST_XYZW)
    { return upper_op(0x2Du, fd, fs, ft, d); }

// OPMULA.xyz ACC, FS, FT  — cross-product positive terms  (FD_10 sub-index 11)
//   ACC.x = FS.y*FT.z,  ACC.y = FS.z*FT.x,  ACC.z = FS.x*FT.y
//   dest is hardwired to .xyz (0xE) by the hardware.
static constexpr u32 upper_opmula(u32 fs, u32 ft)
    { return (0xEu<<21)|((ft&0xFu)<<16)|((fs&0xFu)<<11)|(11u<<6)|0x3Eu; }

// OPMSUB.xyz FD, FS, FT  — cross-product negative terms  (opcode 0x2E)
//   FD.x = ACC.x - FS.y*FT.z,  FD.y = ACC.y - FS.z*FT.x,  FD.z = ACC.z - FS.x*FT.y
//   dest is hardwired to .xyz (0xE) by the hardware.
static constexpr u32 upper_opmsub(u32 fd, u32 fs, u32 ft)
    { return (0xEu<<21)|((ft&0xFu)<<16)|((fs&0xFu)<<11)|((fd&0xFu)<<6)|0x2Eu; }

// ── Q/I register ops (upper word) ─────────────────────────────
// ADDq.dest FD, FS  (opcode 0x20): FD.dest = FS.dest + Q
static constexpr u32 upper_addq(u32 fd, u32 fs, u32 d = DEST_XYZW)
    { return upper_op(0x20u, fd, fs, 0, d); }
// SUBq.dest FD, FS  (opcode 0x24): FD.dest = FS.dest - Q
static constexpr u32 upper_subq(u32 fd, u32 fs, u32 d = DEST_XYZW)
    { return upper_op(0x24u, fd, fs, 0, d); }
// MULq.dest FD, FS  (opcode 0x1C): FD.dest = FS.dest * Q
static constexpr u32 upper_mulq(u32 fd, u32 fs, u32 d = DEST_XYZW)
    { return upper_op(0x1Cu, fd, fs, 0, d); }
// ADDi.dest FD, FS  (opcode 0x22): FD.dest = FS.dest + I
static constexpr u32 upper_addi(u32 fd, u32 fs, u32 d = DEST_XYZW)
    { return upper_op(0x22u, fd, fs, 0, d); }
// SUBi.dest FD, FS  (opcode 0x26): FD.dest = FS.dest - I
static constexpr u32 upper_subi(u32 fd, u32 fs, u32 d = DEST_XYZW)
    { return upper_op(0x26u, fd, fs, 0, d); }
// MULi.dest FD, FS  (opcode 0x1E): FD.dest = FS.dest * I
static constexpr u32 upper_muli(u32 fd, u32 fs, u32 d = DEST_XYZW)
    { return upper_op(0x1Eu, fd, fs, 0, d); }
// MADDq.dest FD, FS  (opcode 0x21): FD.dest = ACC.dest + FS.dest * Q
static constexpr u32 upper_maddq(u32 fd, u32 fs, u32 d = DEST_XYZW)
    { return upper_op(0x21u, fd, fs, 0, d); }
// MADDi.dest FD, FS  (opcode 0x23): FD.dest = ACC.dest + FS.dest * I
static constexpr u32 upper_maddi(u32 fd, u32 fs, u32 d = DEST_XYZW)
    { return upper_op(0x23u, fd, fs, 0, d); }
// MSUBq.dest FD, FS  (opcode 0x25): FD.dest = ACC.dest - FS.dest * Q
static constexpr u32 upper_msubq(u32 fd, u32 fs, u32 d = DEST_XYZW)
    { return upper_op(0x25u, fd, fs, 0, d); }
// MSUBi.dest FD, FS  (opcode 0x27): FD.dest = ACC.dest - FS.dest * I
static constexpr u32 upper_msubi(u32 fd, u32 fs, u32 d = DEST_XYZW)
    { return upper_op(0x27u, fd, fs, 0, d); }
// MULAq.dest ACC, FS  (FD_00 sub-7): ACC.dest = FS.dest * Q
static constexpr u32 upper_mulaq(u32 fs, u32 d = DEST_XYZW)
    { return ((d&0xFu)<<21)|((fs&0xFu)<<11)|(7u<<6)|0x3Cu; }
// ADDAq.dest ACC, FS  (FD_00 sub-8): ACC.dest = FS.dest + Q
static constexpr u32 upper_addaq(u32 fs, u32 d = DEST_XYZW)
    { return ((d&0xFu)<<21)|((fs&0xFu)<<11)|(8u<<6)|0x3Cu; }
// SUBAq.dest ACC, FS  (FD_00 sub-9): ACC.dest = FS.dest - Q
static constexpr u32 upper_subaq(u32 fs, u32 d = DEST_XYZW)
    { return ((d&0xFu)<<21)|((fs&0xFu)<<11)|(9u<<6)|0x3Cu; }
// MADDAq.dest ACC, FS  (FD_01 sub-8): ACC.dest += FS.dest * Q
static constexpr u32 upper_maddaq(u32 fs, u32 d = DEST_XYZW)
    { return ((d&0xFu)<<21)|((fs&0xFu)<<11)|(8u<<6)|0x3Du; }
// MSUBAq.dest ACC, FS  (FD_01 sub-9): ACC.dest -= FS.dest * Q
static constexpr u32 upper_msubaq(u32 fs, u32 d = DEST_XYZW)
    { return ((d&0xFu)<<21)|((fs&0xFu)<<11)|(9u<<6)|0x3Du; }
// MULAi.dest ACC, FS  (FD_10 sub-7): ACC.dest = FS.dest * I
static constexpr u32 upper_mulai(u32 fs, u32 d = DEST_XYZW)
    { return ((d&0xFu)<<21)|((fs&0xFu)<<11)|(7u<<6)|0x3Eu; }
// ADDAi.dest ACC, FS  (FD_10 sub-8): ACC.dest = FS.dest + I
static constexpr u32 upper_addai(u32 fs, u32 d = DEST_XYZW)
    { return ((d&0xFu)<<21)|((fs&0xFu)<<11)|(8u<<6)|0x3Eu; }
// SUBAi.dest ACC, FS  (FD_10 sub-9): ACC.dest = FS.dest - I
static constexpr u32 upper_subai(u32 fs, u32 d = DEST_XYZW)
    { return ((d&0xFu)<<21)|((fs&0xFu)<<11)|(9u<<6)|0x3Eu; }
// MADDAi.dest ACC, FS  (FD_11 sub-8): ACC.dest += FS.dest * I
static constexpr u32 upper_maddai(u32 fs, u32 d = DEST_XYZW)
    { return ((d&0xFu)<<21)|((fs&0xFu)<<11)|(8u<<6)|0x3Fu; }
// MSUBAi.dest ACC, FS  (FD_11 sub-9): ACC.dest -= FS.dest * I
static constexpr u32 upper_msubai(u32 fs, u32 d = DEST_XYZW)
    { return ((d&0xFu)<<21)|((fs&0xFu)<<11)|(9u<<6)|0x3Fu; }

// ── Load/Store (lower word) ────────────────────────────────────
// LQ FT.dest, imm11(IS)  (opcode 0x00)
//   FT.dest = VU_MEM[(VI[IS] + imm11) & 0xFF]
static constexpr u32 lower_lq(u32 ft, u32 is, s32 imm11, u32 dest = DEST_XYZW)
{
    return (0x00u << 25) | ((dest & 0xFu) << 21) | ((ft & 0xFu) << 16)
         | ((is & 0xFu) << 11) | (static_cast<u32>(imm11) & 0x7FFu);
}
// SQ VF[FS].dest, imm11(IT)  (opcode 0x01)
//   VU_MEM[(VI[IT] + imm11) & 0xFF] = VF[FS].dest
static constexpr u32 lower_sq(u32 fs_vf, u32 it_vi, s32 imm11, u32 dest = DEST_XYZW)
{
    return (0x01u << 25) | ((dest & 0xFu) << 21) | ((it_vi & 0xFu) << 16)
         | ((fs_vf & 0xFu) << 11) | (static_cast<u32>(imm11) & 0x7FFu);
}
// LQI FT.dest, (FS)+  (T3_00 sub-index 13)
//   FT.dest = VU_MEM[VI[FS]]; VI[FS]++
static constexpr u32 lower_lqi(u32 ft, u32 fs_vi, u32 dest = DEST_XYZW)
{
    return (0x40u << 25) | ((dest & 0xFu) << 21) | ((ft & 0xFu) << 16)
         | ((fs_vi & 0xFu) << 11) | (13u << 6) | 0x3Cu;
}
// SQI VF[FS].dest, (FT)+  (T3_01 sub-index 13)
//   VU_MEM[VI[FT]] = VF[FS].dest; VI[FT]++
static constexpr u32 lower_sqi(u32 fs_vf, u32 ft_vi, u32 dest = DEST_XYZW)
{
    return (0x40u << 25) | ((dest & 0xFu) << 21) | ((ft_vi & 0xFu) << 16)
         | ((fs_vf & 0xFu) << 11) | (13u << 6) | 0x3Du;
}
// LQD FT.dest, -(FS)  (T3_10 sub-index 13)
//   VI[FS]--; FT.dest = VU_MEM[VI[FS]]
static constexpr u32 lower_lqd(u32 ft, u32 fs_vi, u32 dest = DEST_XYZW)
{
    return (0x40u << 25) | ((dest & 0xFu) << 21) | ((ft & 0xFu) << 16)
         | ((fs_vi & 0xFu) << 11) | (13u << 6) | 0x3Eu;
}
// SQD VF[FS].dest, -(FT)  (T3_11 sub-index 13)
//   VI[FT]--; VU_MEM[VI[FT]] = VF[FS].dest
static constexpr u32 lower_sqd(u32 fs_vf, u32 ft_vi, u32 dest = DEST_XYZW)
{
    return (0x40u << 25) | ((dest & 0xFu) << 21) | ((ft_vi & 0xFu) << 16)
         | ((fs_vf & 0xFu) << 11) | (13u << 6) | 0x3Fu;
}

// ILW IT.dest, Imm11(IS)  (opcode 4)
//   VI[IT] = VU_MEM[(VI[IS]+Imm11)*16 + offsetSS]  (16-bit load; offsetSS: X=0,Y=4,Z=8,W=12)
static constexpr u32 lower_ilw(u32 it, u32 is, s32 imm11, u32 dest = DEST_X)
{
    return (4u << 25) | ((dest & 0xFu) << 21) | ((it & 0xFu) << 16)
         | ((is & 0xFu) << 11) | (static_cast<u32>(imm11) & 0x7FFu);
}
// ISW IT.dest, Imm11(IS)  (opcode 5)
//   VU_MEM[(VI[IS]+Imm11)*16 + offsetSS] = u32(VI[IT])  (32-bit store per selected lane)
static constexpr u32 lower_isw(u32 it, u32 is, s32 imm11, u32 dest = DEST_XYZW)
{
    return (5u << 25) | ((dest & 0xFu) << 21) | ((it & 0xFu) << 16)
         | ((is & 0xFu) << 11) | (static_cast<u32>(imm11) & 0x7FFu);
}
// ILWR IT.dest, (IS)  (T3_10 sub-index 15)
//   VI[IT] = VU_MEM[VI[IS]*16 + offsetSS]  (no offset)
static constexpr u32 lower_ilwr(u32 it, u32 is, u32 dest = DEST_X)
{
    return (0x40u << 25) | ((dest & 0xFu) << 21) | ((it & 0xFu) << 16)
         | ((is & 0xFu) << 11) | (15u << 6) | 0x3Eu;
}
// ISWR IT.dest, (IS)  (T3_11 sub-index 15)
//   VU_MEM[VI[IS]*16 + offsetSS] = u32(VI[IT])  (no offset)
static constexpr u32 lower_iswr(u32 it, u32 is, u32 dest = DEST_XYZW)
{
    return (0x40u << 25) | ((dest & 0xFu) << 21) | ((it & 0xFu) << 16)
         | ((is & 0xFu) << 11) | (15u << 6) | 0x3Fu;
}
// MFP FT.dest, P  (T3_00 sub-index 25)  — VU0: NOP
static constexpr u32 lower_mfp(u32 ft, u32 dest = DEST_XYZW)
{
    return (0x40u << 25) | ((dest & 0xFu) << 21) | ((ft & 0xFu) << 16)
         | (25u << 6) | 0x3Cu;
}
// XTOP IT  (T3_00 sub-index 26)  — VU0: NOP
static constexpr u32 lower_xtop(u32 it)
{
    return (0x40u << 25) | ((it & 0xFu) << 16) | (26u << 6) | 0x3Cu;
}
// XITOP IT  (T3_01 sub-index 26)
//   VI[IT] = vifRegs[idx].itop & (isVU1 ? 0x3FF : 0xFF)
static constexpr u32 lower_xitop(u32 it)
{
    return (0x40u << 25) | ((it & 0xFu) << 16) | (26u << 6) | 0x3Du;
}
// XGKICK IS  (T3_00 sub-index 27)  — VU0: NOP; VU1: send GIF packet at VI[IS]
static constexpr u32 lower_xgkick(u32 is)
{
    return (0x40u << 25) | ((is & 0xFu) << 11) | (27u << 6) | 0x3Cu;
}
// WAITP  (T3_11 sub-index 30)  — VU0: NOP; VU1: stall until P (EFU) result ready
static constexpr u32 lower_waitp()
{
    return (0x40u << 25) | (30u << 6) | 0x3Fu;
}

// ── Branch/Jump (lower word) ───────────────────────────────────
// branchAddr(iPC, imm11) = ((iPC + 2 + imm11*2) & progMemMask) << 2
// iPC is in 32-bit word units (each pair = 2 words).
// Branches here use imm11 in pair units relative to the NEXT pair.
// At pair N (iPC = N*2): target pair = N + 1 + imm11
//
// IBEQ IT, IS, imm11: branch if VI[IS] == VI[IT]  (opcode 0x28)
static constexpr u32 lower_ibeq(u32 is, u32 it, s32 imm11)
{
    return (0x28u << 25) | ((it & 0xFu) << 16) | ((is & 0xFu) << 11)
         | (static_cast<u32>(imm11) & 0x7FFu);
}
// IBGTZ IS, imm11: branch if VI[IS] > 0 (signed)  (opcode 0x2D)
static constexpr u32 lower_ibgtz(u32 is, s32 imm11)
{
    return (0x2Du << 25) | ((is & 0xFu) << 11) | (static_cast<u32>(imm11) & 0x7FFu);
}
// IBLTZ IS, imm11: branch if VI[IS] < 0 (signed)  (opcode 0x2C)
static constexpr u32 lower_ibltz(u32 is, s32 imm11)
{
    return (0x2Cu << 25) | ((is & 0xFu) << 11) | (static_cast<u32>(imm11) & 0x7FFu);
}
// IBLEZ IS, imm11: branch if VI[IS] <= 0 (signed)  (opcode 0x2E)
static constexpr u32 lower_iblez(u32 is, s32 imm11)
{
    return (0x2Eu << 25) | ((is & 0xFu) << 11) | (static_cast<u32>(imm11) & 0x7FFu);
}
// IBGEZ IS, imm11: branch if VI[IS] >= 0 (signed)  (opcode 0x2F)
static constexpr u32 lower_ibgez(u32 is, s32 imm11)
{
    return (0x2Fu << 25) | ((is & 0xFu) << 11) | (static_cast<u32>(imm11) & 0x7FFu);
}
// B imm11: unconditional branch  (opcode 0x20)
static constexpr u32 lower_b(s32 imm11)
{
    return (0x20u << 25) | (static_cast<u32>(imm11) & 0x7FFu);
}
// BAL IT, imm11: branch and link  (opcode 0x21)
//   VI[IT] = (xPC + 16) / 8  where xPC = byte offset of BAL
static constexpr u32 lower_bal(u32 it, s32 imm11)
{
    return (0x21u << 25) | ((it & 0xFu) << 16) | (static_cast<u32>(imm11) & 0x7FFu);
}
// JR IS: jump register  (opcode 0x24)
//   target byte = VI[IS] << 3  →  to jump to pair N set VI[IS] = N
static constexpr u32 lower_jr(u32 is)
{
    return (0x24u << 25) | ((is & 0xFu) << 11);
}
// JALR IS, IT: jump and link register  (opcode 0x25)
//   VI[IT] = (xPC + 16) / 8;  target byte = VI[IS] << 3
static constexpr u32 lower_jalr(u32 is, u32 it)
{
    return (0x25u << 25) | ((it & 0xFu) << 16) | ((is & 0xFu) << 11);
}

// ── Flag register ops (lower word) ────────────────────────────
// CLIP register:
//   FCSET $imm24: CLIP = imm24                               [bits 31:25 = 0x11]
static constexpr u32 lower_fcset(u32 imm24)
    { return (0x11u << 25) | (imm24 & 0xFFFFFFu); }
//   FCGET FT: VI[FT] = CLIP & 0xFFF                         [bits 31:25 = 0x1C]
static constexpr u32 lower_fcget(u32 ft)
    { return (0x1Cu << 25) | ((ft & 0xFu) << 16); }
//   FCAND $imm24: VI[1] = (CLIP & imm24) != 0 ? 1 : 0       [bits 31:25 = 0x12]
static constexpr u32 lower_fcand(u32 imm24)
    { return (0x12u << 25) | (imm24 & 0xFFFFFFu); }
//   FCOR $imm24: VI[1] = ((CLIP | imm24) == 0xFFFFFF) ? 1:0 [bits 31:25 = 0x13]
static constexpr u32 lower_fcor(u32 imm24)
    { return (0x13u << 25) | (imm24 & 0xFFFFFFu); }
// STATUS flag:
//   FSAND FT, $imm12: VI[FT] = STATUS & imm12               [bits 31:25 = 0x16]
//   imm12 bits: 0=Z 1=S 2=U 3=O 4=I 5=D 6=ZS 7=SS 8=US 9=OS 10=IS 11=DS
//   Note: bits 2-5 and 8-11 trigger DevCon warnings; use 0x00C3 for Z/S/ZS/SS only.
static constexpr u32 lower_fsand(u32 ft, u32 imm12)
    { return (0x16u << 25) | ((ft & 0xFu) << 16) | (imm12 & 0xFFFu); }
//   FSOR  FT, $imm12: VI[FT] = STATUS | imm12               [bits 31:25 = 0x17]
static constexpr u32 lower_fsor(u32 ft, u32 imm12)
    { return (0x17u << 25) | ((ft & 0xFu) << 16) | (imm12 & 0xFFFu); }
// MAC flag:
//   FMAND FT, IS: VI[FT] = MAC & VI[IS]                     [bits 31:25 = 0x1A]
//   MAC bits: 0-3=Zw,Zz,Zy,Zx  4-7=Sw,Sz,Sy,Sx  8-15=overflow (with vuOverflowHack)
static constexpr u32 lower_fmand(u32 ft, u32 is)
    { return (0x1Au << 25) | ((ft & 0xFu) << 16) | ((is & 0xFu) << 11); }
//   FMOR  FT, IS: VI[FT] = MAC | VI[IS]                     [bits 31:25 = 0x1B]
static constexpr u32 lower_fmor(u32 ft, u32 is)
    { return (0x1Bu << 25) | ((ft & 0xFu) << 16) | ((is & 0xFu) << 11); }
// FSEQ  FT, $imm12: VI[FT] = (STATUS == imm12) ? 1 : 0    [bits 31:25 = 0x14]
//   imm12 encoding: {code[21], code[10:0]}
static constexpr u32 lower_fseq(u32 ft, u32 imm12)
    { return (0x14u << 25) | ((ft & 0xFu) << 16) | (((imm12 >> 11) & 1u) << 21) | (imm12 & 0x7FFu); }
// FSSET $imm12: STATUS sticky bits set from imm12[11:6]    [bits 31:25 = 0x15]
//   imm12 bits 6-11: ZS=6, SS=7, US=8, OS=9, IS=10, DS=11; bits 0-5 are ignored
//   imm12 encoding: {code[21], code[10:0]}
static constexpr u32 lower_fsset(u32 imm12)
    { return (0x15u << 25) | (((imm12 >> 11) & 1u) << 21) | (imm12 & 0x7FFu); }
// FMEQ  FT, IS: VI[FT] = (MAC == VI[IS]) ? 1 : 0          [bits 31:25 = 0x18]
static constexpr u32 lower_fmeq(u32 ft, u32 is)
    { return (0x18u << 25) | ((ft & 0xFu) << 16) | ((is & 0xFu) << 11); }

// ── CLIP instruction (upper word) ─────────────────────────────
// CLIP.xyz FS, FT: compares |FS.xyz| against |FT.w|, updates 6 CLIP bits.
//   New CLIP = (old_CLIP << 6) | comp6, where comp6 bits:
//     0=+x(FS.x>|FT.w|), 1=-x(FS.x<-|FT.w|), 2=+y, 3=-y, 4=+z, 5=-z
//   Upper FD_11 sub-index 7 (bits[10:6]=7, bits[5:0]=0x3F)
static constexpr u32 upper_clip(u32 fs, u32 ft)
    { return ((ft & 0xFu) << 16) | ((fs & 0xFu) << 11) | (7u << 6) | 0x3Fu; }

// ──────────────────────────────────────────────────────────────
// IEEE 754 single-precision bit patterns for test values
// ──────────────────────────────────────────────────────────────
static constexpr u32 FP_0  = 0x00000000u; // 0.0f
static constexpr u32 FP_1  = 0x3F800000u; // 1.0f
static constexpr u32 FP_2  = 0x40000000u; // 2.0f
static constexpr u32 FP_3  = 0x40400000u; // 3.0f
static constexpr u32 FP_4  = 0x40800000u; // 4.0f
static constexpr u32 FP_5  = 0x40A00000u; // 5.0f
static constexpr u32 FP_6  = 0x40C00000u; // 6.0f
static constexpr u32 FP_8  = 0x41000000u; // 8.0f
static constexpr u32 FP_7  = 0x40E00000u; // 7.0f
static constexpr u32 FP_N1 = 0xBF800000u; // -1.0f
static constexpr u32 FP_N2 = 0xC0000000u; // -2.0f
static constexpr u32 FP_N3 = 0xC0400000u; // -3.0f
static constexpr u32 FP_N4 = 0xC0800000u; // -4.0f

// IEEE / PS2 special values used by clamp tests
static constexpr u32 FP_PS2_MAX    = 0x7F7FFFFFu; // PS2 max positive finite (= IEEE max finite)
static constexpr u32 FP_PS2_MIN    = 0xFF7FFFFFu; // PS2 max negative finite
static constexpr u32 FP_IEEE_INF   = 0x7F800000u; // +infinity
static constexpr u32 FP_IEEE_NINF  = 0xFF800000u; // -infinity
static constexpr u32 FP_IEEE_NAN   = 0x7FC00000u; // +quiet NaN
static constexpr u32 FP_IEEE_NNAN  = 0xFFC00000u; // -quiet NaN

// FTZ / DaZ test values
// FP_TINY_NORM: 2^-64 — small but normal; squaring it (2^-128) produces a denormal
static constexpr u32 FP_TINY_NORM  = 0x1F800000u; // 2^-64 ≈ 5.42e-20, normal
// FP_DENORM: a denormal value (= FP_TINY_NORM²); with FPCR.FZ=1 it is treated as 0
static constexpr u32 FP_DENORM     = 0x00200000u; // 2^-128 ≈ 2.94e-39, subnormal

// ──────────────────────────────────────────────────────────────
// Special register sentinels for test helpers
// ──────────────────────────────────────────────────────────────

// Use as VFPreset/VFExpect reg to access the ACC register
static constexpr int REG_ACC      = 32;
// Use as VFPreset reg to preset Q pipeline register (scalar; x field used)
static constexpr int REG_Q_PRESET = 33;
// Use as VFPreset reg to preset I pipeline register (scalar; x field used)
static constexpr int REG_I_PRESET = 34;
// Use as VFPreset reg to preset the R (random) register (VI[REG_R].UL; x field used)
static constexpr int REG_R_PRESET = 35;

// Memory cell: quadword address + 4 component values
struct MemCell { u32 addr_qw; u32 x, y, z, w; };

// ──────────────────────────────────────────────────────────────
// Test state
// ──────────────────────────────────────────────────────────────

static int s_pass, s_fail;

// ──────────────────────────────────────────────────────────────
// VF register test runner
// ──────────────────────────────────────────────────────────────

struct VFPreset { int reg; u32 x, y, z, w; };
struct VFExpect { int reg; u32 x, y, z, w; };

static bool runVFTest(const char* name,
                      const u32* words, u32 nwords,
                      std::initializer_list<VFPreset> vf_in,
                      std::initializer_list<VFExpect> vf_out)
{
    mVU0_TestWriteProg(words, nwords);

    // Zero all VF regs, ACC, VI; restore VF0 hardwired w=1.0f
    std::memset(VU0.VF, 0, sizeof(VU0.VF));
    VU0.VF[0].UL[3] = FP_1;
    std::memset(VU0.VI, 0, sizeof(VU0.VI));
    std::memset(&VU0.ACC, 0, sizeof(VU0.ACC));
    std::memset(VU0.micro_clipflags,   0, sizeof(VU0.micro_clipflags));
    std::memset(VU0.micro_statusflags, 0, sizeof(VU0.micro_statusflags));
    std::memset(VU0.micro_macflags,    0, sizeof(VU0.micro_macflags));

    for (auto& p : vf_in) {
        if (p.reg == REG_ACC) {
            VU0.ACC.UL[0] = p.x; VU0.ACC.UL[1] = p.y;
            VU0.ACC.UL[2] = p.z; VU0.ACC.UL[3] = p.w;
        } else if (p.reg == REG_Q_PRESET) {
            VU0.VI[REG_Q].UL = p.x;
            VU0.pending_q    = p.x;
        } else if (p.reg == REG_I_PRESET) {
            VU0.VI[REG_I].UL = p.x;
        } else if (p.reg == REG_R_PRESET) {
            VU0.VI[REG_R].UL = p.x;
        } else {
            VU0.VF[p.reg].UL[0] = p.x;
            VU0.VF[p.reg].UL[1] = p.y;
            VU0.VF[p.reg].UL[2] = p.z;
            VU0.VF[p.reg].UL[3] = p.w;
        }
    }

    // Ensure FPCR matches VU0 settings (FZ=1, DaZ=1) for the duration of the
    // block — mirrors what the JIT dispatcher does at runtime.
    {
        FPControlRegisterBackup fpcr_guard(EmuConfig.Cpu.VU0FPCR);
        mVU0_TestExec(0u, 1000000u);
    }

    bool ok = true;
    for (auto& e : vf_out) {
        u32 gx, gy, gz, gw;
        if (e.reg == REG_ACC) {
            gx = VU0.ACC.UL[0]; gy = VU0.ACC.UL[1];
            gz = VU0.ACC.UL[2]; gw = VU0.ACC.UL[3];
        } else {
            gx = VU0.VF[e.reg].UL[0]; gy = VU0.VF[e.reg].UL[1];
            gz = VU0.VF[e.reg].UL[2]; gw = VU0.VF[e.reg].UL[3];
        }
        if (gx != e.x || gy != e.y || gz != e.z || gw != e.w) {
            const char* label = (e.reg == REG_ACC) ? "ACC" : "VF";
            int         idx   = (e.reg == REG_ACC) ? 0    : e.reg;
            LOGE("  FAIL %s: %s[%d] exp={%08X,%08X,%08X,%08X} got={%08X,%08X,%08X,%08X}",
                 name, label, idx, e.x, e.y, e.z, e.w, gx, gy, gz, gw);
            ok = false;
        }
    }
    if (ok) { ++s_pass; LOGI("  PASS %s", name); }
    else    { ++s_fail; }
    return ok;
}

// ──────────────────────────────────────────────────────────────
// Load/Store test runner
// Handles VF registers, VI integer registers, and VU data memory.
// ──────────────────────────────────────────────────────────────

static bool runLSTest(const char* name,
                      const u32* words, u32 nwords,
                      std::initializer_list<std::pair<int,u16>> vi_in,
                      std::initializer_list<VFPreset>           vf_in,
                      std::initializer_list<MemCell>            mem_in,
                      std::initializer_list<VFExpect>           vf_out,
                      std::initializer_list<std::pair<int,u16>> vi_out,
                      std::initializer_list<MemCell>            mem_out,
                      u32 startPC = 0u)
{
    mVU0_TestWriteProg(words, nwords);

    std::memset(VU0.Mem, 0, 0x1000); // VU0 data memory = 4 KB
    std::memset(VU0.VF, 0, sizeof(VU0.VF));
    VU0.VF[0].UL[3] = FP_1;
    std::memset(VU0.VI, 0, sizeof(VU0.VI));
    std::memset(&VU0.ACC, 0, sizeof(VU0.ACC));
    std::memset(VU0.micro_clipflags,   0, sizeof(VU0.micro_clipflags));
    std::memset(VU0.micro_statusflags, 0, sizeof(VU0.micro_statusflags));
    std::memset(VU0.micro_macflags,    0, sizeof(VU0.micro_macflags));

    for (auto& kv : vi_in)
        VU0.VI[kv.first].US[0] = kv.second;
    for (auto& p : vf_in) {
        VU0.VF[p.reg].UL[0] = p.x; VU0.VF[p.reg].UL[1] = p.y;
        VU0.VF[p.reg].UL[2] = p.z; VU0.VF[p.reg].UL[3] = p.w;
    }
    for (auto& m : mem_in) {
        u32* p = reinterpret_cast<u32*>(VU0.Mem + m.addr_qw * 16u);
        p[0] = m.x; p[1] = m.y; p[2] = m.z; p[3] = m.w;
    }

    {
        FPControlRegisterBackup fpcr_guard(EmuConfig.Cpu.VU0FPCR);
        mVU0_TestExec(startPC, 1000000u);
    }

    bool ok = true;
    for (auto& e : vf_out) {
        u32 gx = VU0.VF[e.reg].UL[0], gy = VU0.VF[e.reg].UL[1];
        u32 gz = VU0.VF[e.reg].UL[2], gw = VU0.VF[e.reg].UL[3];
        if (gx != e.x || gy != e.y || gz != e.z || gw != e.w) {
            LOGE("  FAIL %s: VF[%d] exp={%08X,%08X,%08X,%08X} got={%08X,%08X,%08X,%08X}",
                 name, e.reg, e.x, e.y, e.z, e.w, gx, gy, gz, gw);
            ok = false;
        }
    }
    for (auto& kv : vi_out) {
        u16 got = VU0.VI[kv.first].US[0];
        if (got != kv.second) {
            LOGE("  FAIL %s: VI[%d] expected 0x%04X got 0x%04X",
                 name, kv.first, (unsigned)kv.second, (unsigned)got);
            ok = false;
        }
    }
    for (auto& m : mem_out) {
        const u32* p = reinterpret_cast<const u32*>(VU0.Mem + m.addr_qw * 16u);
        if (p[0] != m.x || p[1] != m.y || p[2] != m.z || p[3] != m.w) {
            LOGE("  FAIL %s: MEM[%u] exp={%08X,%08X,%08X,%08X} got={%08X,%08X,%08X,%08X}",
                 name, m.addr_qw, m.x, m.y, m.z, m.w, p[0], p[1], p[2], p[3]);
            ok = false;
        }
    }

    if (ok) { ++s_pass; LOGI("  PASS %s", name); }
    else    { ++s_fail; }
    return ok;
}

// ──────────────────────────────────────────────────────────────
// Clamp test runner
//
// Temporarily sets EmuConfig clamping flags, runs a VF test, then
// restores defaults.  runVFTest calls mVU0_TestWriteProg internally
// which invalidates the JIT cache, so the block is always recompiled
// with whatever EmuConfig says at the time of execution.
//
// Program note: to test the REGULAR clamp path (mVUclamp1/2, clampE=false)
// the tested instruction must NOT carry the E-bit.  The E-bit goes on the
// pair immediately after, and the delay slot follows:
//
//   Pair 0: LOWER_NOP | upper_xxx(...)          ← tested instruction, no E-bit
//   Pair 1: LOWER_NOP | UPPER_EBIT_NOP          ← terminates block
//   Pair 2: LOWER_NOP | UPPER_NOP               ← delay slot
// ──────────────────────────────────────────────────────────────

static bool runClampTest(const char* name,
                         const u32* words, u32 nwords,
                         bool overflow, bool signOverflow,
                         std::initializer_list<VFPreset> vf_in,
                         std::initializer_list<VFExpect> vf_out)
{
    EmuConfig.Cpu.Recompiler.vu0Overflow    = overflow    ? 1 : 0;
    EmuConfig.Cpu.Recompiler.vu0SignOverflow = signOverflow ? 1 : 0;
    bool ok = runVFTest(name, words, nwords, vf_in, vf_out);
    EmuConfig.Cpu.Recompiler.vu0Overflow    = 0;
    EmuConfig.Cpu.Recompiler.vu0SignOverflow = 0;
    return ok;
}

// ──────────────────────────────────────────────────────────────
// Test runner
// ──────────────────────────────────────────────────────────────

static bool runTest(const char* name,
                    const u32* words, u32 nwords,
                    std::initializer_list<std::pair<int, u16>> presets,
                    std::initializer_list<std::pair<int, u16>> expected,
                    u32 startPC = 0u)
{
    mVU0_TestWriteProg(words, nwords);

    std::memset(VU0.VI, 0, sizeof(VU0.VI));
    std::memset(VU0.micro_clipflags,   0, sizeof(VU0.micro_clipflags));
    std::memset(VU0.micro_statusflags, 0, sizeof(VU0.micro_statusflags));
    std::memset(VU0.micro_macflags,    0, sizeof(VU0.micro_macflags));
    for (auto& kv : presets)
        VU0.VI[kv.first].US[0] = kv.second;

    mVU0_TestExec(startPC, 1000000u);

    bool ok = true;
    for (auto& kv : expected) {
        u16 got = VU0.VI[kv.first].US[0];
        if (got != kv.second) {
            LOGE("  FAIL %s: VI[%d] expected 0x%04X got 0x%04X",
                 name, kv.first, (unsigned)kv.second, (unsigned)got);
            ok = false;
        }
    }

    if (ok) { ++s_pass; LOGI("  PASS %s", name); }
    else    { ++s_fail; }
    return ok;
}

// ──────────────────────────────────────────────────────────────
// IADDI tests
// ──────────────────────────────────────────────────────────────

static void test_iaddi_pos()
{
    const u32 prog[] = { lower_iaddi(1,0,5), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runTest("IADDI: VI[1] = 0+5 = 5", prog, 4, {}, {{1, 5}});
}

static void test_iaddi_neg()
{
    const u32 prog[] = { lower_iaddi(1,0,0x1Du), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runTest("IADDI: VI[1] = 0+(-3) = 0xFFFD", prog, 4, {}, {{1, 0xFFFDu}});
}

static void test_iaddi_self()
{
    const u32 prog[] = { lower_iaddi(1,1,2), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runTest("IADDI: VI[1] = VI[1]+2, preset 10 → 12", prog, 4, {{1,10}}, {{1,12}});
}

static void test_iaddi_zero_imm()
{
    const u32 prog[] = { lower_iaddi(2,1,0), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runTest("IADDI: VI[2] = VI[1]+0, preset 7 → 7", prog, 4, {{1,7}}, {{2,7}});
}

// ──────────────────────────────────────────────────────────────
// IADD tests
// ──────────────────────────────────────────────────────────────

static void test_iadd()
{
    const u32 prog[] = { lower_iadd(3,1,2), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runTest("IADD: VI[3] = 3+4 = 7", prog, 4, {{1,3},{2,4}}, {{3,7}});
}

static void test_iadd_src0()
{
    const u32 prog[] = { lower_iadd(1,0,2), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runTest("IADD: VI[1] = VI[0](0)+VI[2] = 9", prog, 4, {{2,9}}, {{1,9}});
}

// ──────────────────────────────────────────────────────────────
// ISUB tests
// ──────────────────────────────────────────────────────────────

static void test_isub()
{
    const u32 prog[] = { lower_isub(3,1,2), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runTest("ISUB: VI[3] = 7-4 = 3", prog, 4, {{1,7},{2,4}}, {{3,3}});
}

static void test_isub_self()
{
    const u32 prog[] = { lower_isub(1,1,1), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runTest("ISUB: VI[1] = VI[1]-VI[1] = 0, preset 42", prog, 4, {{1,42}}, {{1,0}});
}

// ──────────────────────────────────────────────────────────────
// IAND tests
// ──────────────────────────────────────────────────────────────

static void test_iand()
{
    const u32 prog[] = { lower_iand(3,1,2), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runTest("IAND: 0xFF & 0x0F = 0x0F", prog, 4, {{1,0xFF},{2,0x0F}}, {{3,0x0Fu}});
}

static void test_iand_same()
{
    const u32 prog[] = { lower_iand(1,1,1), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runTest("IAND: VI[1]&VI[1] = identity", prog, 4, {{1,0x1234}}, {{1,0x1234u}});
}

// ──────────────────────────────────────────────────────────────
// IOR tests
// ──────────────────────────────────────────────────────────────

static void test_ior()
{
    const u32 prog[] = { lower_ior(3,1,2), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runTest("IOR: 0xF0|0x0F = 0xFF", prog, 4, {{1,0xF0},{2,0x0F}}, {{3,0xFFu}});
}

static void test_ior_no_overlap()
{
    const u32 prog[] = { lower_ior(3,1,2), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runTest("IOR: 0x1200|0x0034 = 0x1234", prog, 4, {{1,0x1200},{2,0x0034}}, {{3,0x1234u}});
}

// ──────────────────────────────────────────────────────────────
// IADDIU tests
// ──────────────────────────────────────────────────────────────

static void test_iaddiu()
{
    const u32 prog[] = { lower_iaddiu(1,0,100), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runTest("IADDIU: 0+100 = 100", prog, 4, {}, {{1,100}});
}

static void test_iaddiu_large()
{
    const u32 prog[] = { lower_iaddiu(1,0,0x4000u), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runTest("IADDIU: 0+0x4000 = 0x4000 (upper imm nibble)", prog, 4, {}, {{1,0x4000u}});
}

static void test_iaddiu_max()
{
    const u32 prog[] = { lower_iaddiu(2,1,0x7FFFu), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runTest("IADDIU: VI[1](1)+0x7FFF = 0x8000", prog, 4, {{1,1}}, {{2,0x8000u}});
}

// ──────────────────────────────────────────────────────────────
// ISUBIU tests
// ──────────────────────────────────────────────────────────────

static void test_isubiu()
{
    const u32 prog[] = { lower_isubiu(1,2,3), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runTest("ISUBIU: VI[2](10)-3 = 7", prog, 4, {{2,10}}, {{1,7}});
}

// ──────────────────────────────────────────────────────────────
// Multi-instruction block
// Tests that the JIT chains multiple pairs and that register values
// written in earlier pairs are readable in the delay slot.
//
// Pair 0: IADDI VI[1] = 0+5     | NOP
// Pair 1: IADDI VI[2] = 0+3     | E-bit NOP   ← terminates
// Pair 2: IADD  VI[3]=VI[1]+VI[2] | NOP        ← delay slot (VI[1]=5, VI[2]=3)
// ──────────────────────────────────────────────────────────────

static void test_multi_instruction_block()
{
    const u32 prog[] = {
        lower_iaddi(1,0,5), UPPER_NOP,
        lower_iaddi(2,0,3), UPPER_EBIT_NOP,
        lower_iadd(3,1,2),  UPPER_NOP,
        LOWER_NOP,          UPPER_NOP,
    };
    runTest("Multi-pair: VI[1]=5, VI[2]=3, delay-slot VI[3]=8",
            prog, 8, {}, {{1,5},{2,3},{3,8}});
}

// ──────────────────────────────────────────────────────────────
// VI-Delay documentation test
//
// The PS2 VU has a 1-instruction write latency on integer registers.
// When IBNE reads a VI register written by the immediately preceding
// instruction, it sees the OLD value.  The JIT logs "Branch VI-Delay".
//
// Pair 0 (iPC=0): IADDI VI[1]++       | NOP   ← writes VI[1] (latency)
// Pair 1 (iPC=2): IBNE VI[1],VI[2],-2 | NOP   ← reads PREVIOUS VI[1]
// Pair 2 (iPC=4): NOP                  | NOP   ← delay slot
// Pair 3 (iPC=6): NOP                  | E-bit ← terminate
// Pair 4 (iPC=8): NOP                  | NOP
//
// branchAddr(iPC=2, imm11=-2) = 0 = pair 0 ✓
// Preset VI[2]=3, VI[1]=0.  Branch exits when IBNE reads VI[1]==3
// (old value), which happens after VI[1] has been incremented to 4.
// Expected: VI[1]=4 (not 3 — VI-Delay caused one extra iteration).
// ──────────────────────────────────────────────────────────────

static void test_vi_delay_branch()
{
    const u32 prog[] = {
        lower_iaddi(1,1,1),  UPPER_NOP,       // Pair 0: VI[1]++
        lower_ibne(1,2,-2),  UPPER_NOP,       // Pair 1: reads DELAYED VI[1]
        LOWER_NOP,           UPPER_NOP,        // Pair 2: delay slot
        LOWER_NOP,           UPPER_EBIT_NOP,   // Pair 3: terminate
        LOWER_NOP,           UPPER_NOP,        // Pair 4: E-bit delay slot
    };
    // VI[1]=4: IBNE read-after-write latency causes one extra loop iteration
    runTest("VI-Delay: IBNE reads stale VI[1], loops once extra → VI[1]=4",
            prog, 10, {{2,3}}, {{1,4}});
}

// ──────────────────────────────────────────────────────────────
// IBNE loop (NOP gap avoids VI-Delay)
//
// Inserting a NOP pair between the IADDI and IBNE lets VI[1] settle
// so the branch reads the freshly written value.
//
// Pair 0 (iPC=0): IADDI VI[1]++        | NOP
// Pair 1 (iPC=2): NOP                   | NOP   ← VI[1] settles here
// Pair 2 (iPC=4): IBNE VI[1],VI[2],-3  | NOP   ← reads settled VI[1]
// Pair 3 (iPC=6): NOP                   | NOP   ← delay slot
// Pair 4 (iPC=8): NOP                   | E-bit ← loop-exit terminate
// Pair 5 (iPC=A): NOP                   | NOP   ← E-bit delay slot
//
// branchAddr(iPC=4, imm11=-3) = ((4+2-6)&mask)<<2 = 0 = pair 0 ✓
// Preset VI[2]=3, VI[1]=0.  Expected: VI[1]=3.
// ──────────────────────────────────────────────────────────────

static void test_ibne_loop()
{
    const u32 prog[] = {
        lower_iaddi(1,1,1),   UPPER_NOP,       // Pair 0: VI[1]++
        LOWER_NOP,            UPPER_NOP,        // Pair 1: NOP (VI-Delay sink)
        lower_ibne(1,2,-3),   UPPER_NOP,       // Pair 2: if VI[1]≠VI[2] → pair 0
        LOWER_NOP,            UPPER_NOP,        // Pair 3: delay slot
        LOWER_NOP,            UPPER_EBIT_NOP,   // Pair 4: loop-exit terminate
        LOWER_NOP,            UPPER_NOP,        // Pair 5: E-bit delay slot
    };
    runTest("IBNE loop (NOP gap): VI[1] counts 0→3", prog, 12, {{2,3}}, {{1,3}});
}

// ──────────────────────────────────────────────────────────────
// Branch delay slot always runs
//
// Pair 0: IBNE VI[1](1), VI[2](2), +1  | NOP  ← taken (1≠2)
// Pair 1: IADDI VI[3], VI[0], 1         | NOP  ← delay slot (always runs)
// Pair 2: NOP                            | E-bit ← terminate
// Pair 3: NOP                            | NOP
//
// branchAddr(iPC=0, imm11=+1) = ((0+2+2)&mask)<<2 = 16 = pair 2 ✓
// Both taken and not-taken paths arrive at pair 2, delay slot sets VI[3]=1.
// ──────────────────────────────────────────────────────────────

static void test_branch_delay_slot()
{
    const u32 prog[] = {
        lower_ibne(1,2,1),   UPPER_NOP,       // Pair 0: IBNE (taken)
        lower_iaddi(3,0,1),  UPPER_NOP,       // Pair 1: delay slot → VI[3]=1
        LOWER_NOP,           UPPER_EBIT_NOP,   // Pair 2: terminate
        LOWER_NOP,           UPPER_NOP,        // Pair 3: E-bit delay slot
    };
    runTest("Branch delay slot always runs: VI[3]=1",
            prog, 8, {{1,1},{2,2}}, {{3,1}});
}

// ──────────────────────────────────────────────────────────────
// ADD.xyzw tests
// ──────────────────────────────────────────────────────────────

// Basic component-wise add: VF3 = VF1 + VF2
// {1,2,3,4} + {4,3,2,1} = {5,5,5,5}
static void test_vf_add()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_add(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("ADD.xyzw: {1,2,3,4}+{4,3,2,1}={5,5,5,5}", prog, 4,
        {{1, FP_1,FP_2,FP_3,FP_4}, {2, FP_4,FP_3,FP_2,FP_1}},
        {{3, FP_5,FP_5,FP_5,FP_5}});
}

// ──────────────────────────────────────────────────────────────
// SUB.xyzw tests
// ──────────────────────────────────────────────────────────────

// {4,3,2,1} - {1,2,3,4} = {3,1,-1,-3}
static void test_vf_sub()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_sub(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("SUB.xyzw: {4,3,2,1}-{1,2,3,4}={3,1,-1,-3}", prog, 4,
        {{1, FP_4,FP_3,FP_2,FP_1}, {2, FP_1,FP_2,FP_3,FP_4}},
        {{3, FP_3,FP_1,FP_N1,FP_N3}});
}

// ──────────────────────────────────────────────────────────────
// MUL.xyzw tests
// ──────────────────────────────────────────────────────────────

// {2,3,1,4} * {4,2,3,2} = {8,6,3,8}
static void test_vf_mul()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_mul(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MUL.xyzw: {2,3,1,4}*{4,2,3,2}={8,6,3,8}", prog, 4,
        {{1, FP_2,FP_3,FP_1,FP_4}, {2, FP_4,FP_2,FP_3,FP_2}},
        {{3, FP_8,FP_6,FP_3,FP_8}});
}

// ──────────────────────────────────────────────────────────────
// MAX.xyzw tests
// ──────────────────────────────────────────────────────────────

// max({1,4,2,3}, {3,2,4,1}) = {3,4,4,3}
static void test_vf_max()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_max(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MAX.xyzw: max({1,4,2,3},{3,2,4,1})={3,4,4,3}", prog, 4,
        {{1, FP_1,FP_4,FP_2,FP_3}, {2, FP_3,FP_2,FP_4,FP_1}},
        {{3, FP_3,FP_4,FP_4,FP_3}});
}

// ──────────────────────────────────────────────────────────────
// MINI.xyzw tests
// ──────────────────────────────────────────────────────────────

// min({1,4,2,3}, {3,2,4,1}) = {1,2,2,1}
static void test_vf_mini()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_mini(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MINI.xyzw: min({1,4,2,3},{3,2,4,1})={1,2,2,1}", prog, 4,
        {{1, FP_1,FP_4,FP_2,FP_3}, {2, FP_3,FP_2,FP_4,FP_1}},
        {{3, FP_1,FP_2,FP_2,FP_1}});
}

// ──────────────────────────────────────────────────────────────
// ABS tests
// ──────────────────────────────────────────────────────────────

// |{-1,-2,3,-4}| = {1,2,3,4}
static void test_vf_abs()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_abs(2,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("ABS: |{-1,-2,3,-4}|={1,2,3,4}", prog, 4,
        {{1, FP_N1,FP_N2,FP_3,FP_N4}},
        {{2, FP_1,FP_2,FP_3,FP_4}});
}

// ──────────────────────────────────────────────────────────────
// ADDx broadcast tests
// ──────────────────────────────────────────────────────────────

// ADDx.xyzw VF3, VF1, VF2: VF3[i] = VF1[i] + VF2.x
// VF1={1,2,3,4}, VF2.x=2 → VF3={3,4,5,6}
static void test_vf_addx()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_addx(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("ADDx: {1,2,3,4}+VF2.x(2)={3,4,5,6}", prog, 4,
        {{1, FP_1,FP_2,FP_3,FP_4}, {2, FP_2,FP_0,FP_0,FP_0}},
        {{3, FP_3,FP_4,FP_5,FP_6}});
}

// ──────────────────────────────────────────────────────────────
// MULw broadcast tests
// ──────────────────────────────────────────────────────────────

// MULw.xyzw VF3, VF1, VF2: VF3[i] = VF1[i] * VF2.w
// VF1={1,2,3,4}, VF2.w=2 → VF3={2,4,6,8}
static void test_vf_mulw()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_mulw(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MULw: {1,2,3,4}*VF2.w(2)={2,4,6,8}", prog, 4,
        {{1, FP_1,FP_2,FP_3,FP_4}, {2, FP_0,FP_0,FP_0,FP_2}},
        {{3, FP_2,FP_4,FP_6,FP_8}});
}

// ──────────────────────────────────────────────────────────────
// Broadcast ADD/SUB/MUL variants
// VF2={2,3,4,5} — distinct per-component values so each test proves
// the correct lane was selected, not just that the op worked.
// ──────────────────────────────────────────────────────────────

static void test_addy() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_addy(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("ADDy: {1,2,3,4}+VF2.y(3)={4,5,6,7}", prog, 4,
        {{1,FP_1,FP_2,FP_3,FP_4},{2,FP_2,FP_3,FP_4,FP_5}},
        {{3,FP_4,FP_5,FP_6,FP_7}});
}
static void test_addz() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_addz(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("ADDz: {1,2,3,4}+VF2.z(4)={5,6,7,8}", prog, 4,
        {{1,FP_1,FP_2,FP_3,FP_4},{2,FP_2,FP_3,FP_4,FP_5}},
        {{3,FP_5,FP_6,FP_7,FP_8}});
}
static void test_addw() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_addw(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("ADDw: {1,2,3,4}+VF2.w(5)={6,7,8,9}", prog, 4,
        {{1,FP_1,FP_2,FP_3,FP_4},{2,FP_2,FP_3,FP_4,FP_5}},
        {{3,FP_6,FP_7,FP_8,0x41100000u}}); // 9.0f = 0x41100000
}
static void test_subx() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_subx(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("SUBx: {3,5,7,8}-VF2.x(2)={1,3,5,6}", prog, 4,
        {{1,FP_3,FP_5,FP_7,FP_8},{2,FP_2,FP_3,FP_4,FP_5}},
        {{3,FP_1,FP_3,FP_5,FP_6}});
}
static void test_suby() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_suby(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("SUBy: {3,5,7,8}-VF2.y(3)={0,2,4,5}", prog, 4,
        {{1,FP_3,FP_5,FP_7,FP_8},{2,FP_2,FP_3,FP_4,FP_5}},
        {{3,FP_0,FP_2,FP_4,FP_5}});
}
static void test_subz() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_subz(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("SUBz: {3,5,7,8}-VF2.z(4)={-1,1,3,4}", prog, 4,
        {{1,FP_3,FP_5,FP_7,FP_8},{2,FP_2,FP_3,FP_4,FP_5}},
        {{3,FP_N1,FP_1,FP_3,FP_4}});
}
static void test_subw() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_subw(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("SUBw: {3,5,7,8}-VF2.w(5)={-2,0,2,3}", prog, 4,
        {{1,FP_3,FP_5,FP_7,FP_8},{2,FP_2,FP_3,FP_4,FP_5}},
        {{3,FP_N2,FP_0,FP_2,FP_3}});
}
static void test_mulx() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_mulx(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MULx: {1,2,3,4}*VF2.x(2)={2,4,6,8}", prog, 4,
        {{1,FP_1,FP_2,FP_3,FP_4},{2,FP_2,FP_3,FP_4,FP_5}},
        {{3,FP_2,FP_4,FP_6,FP_8}});
}
static void test_muly() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_muly(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MULy: {1,2,3,4}*VF2.y(3)={3,6,9,12}", prog, 4,
        {{1,FP_1,FP_2,FP_3,FP_4},{2,FP_2,FP_3,FP_4,FP_5}},
        {{3,FP_3,FP_6,0x41100000u,0x41400000u}}); // 9.0=0x41100000, 12.0=0x41400000
}
static void test_mulz() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_mulz(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MULz: {1,2,3,4}*VF2.z(4)={4,8,12,16}", prog, 4,
        {{1,FP_1,FP_2,FP_3,FP_4},{2,FP_2,FP_3,FP_4,FP_5}},
        {{3,FP_4,FP_8,0x41400000u,0x41800000u}}); // 12.0, 16.0=0x41800000

}

// ──────────────────────────────────────────────────────────────
// Broadcast MAX / MAXi
// VF1={1,3,5,7}, broadcast value=4 → max per lane: {4,4,5,7}
// ──────────────────────────────────────────────────────────────

static void test_maxx() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_maxx(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MAXx: max({1,3,5,7},VF2.x=4)={4,4,5,7}", prog, 4,
        {{1,FP_1,FP_3,FP_5,FP_7},{2,FP_4,FP_2,FP_3,FP_1}},
        {{3,FP_4,FP_4,FP_5,FP_7}});
}
static void test_maxy() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_maxy(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MAXy: max({1,3,5,7},VF2.y=4)={4,4,5,7}", prog, 4,
        {{1,FP_1,FP_3,FP_5,FP_7},{2,FP_1,FP_4,FP_3,FP_2}},
        {{3,FP_4,FP_4,FP_5,FP_7}});
}
static void test_maxz() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_maxz(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MAXz: max({1,3,5,7},VF2.z=4)={4,4,5,7}", prog, 4,
        {{1,FP_1,FP_3,FP_5,FP_7},{2,FP_1,FP_2,FP_4,FP_3}},
        {{3,FP_4,FP_4,FP_5,FP_7}});
}
static void test_maxw() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_maxw(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MAXw: max({1,3,5,7},VF2.w=4)={4,4,5,7}", prog, 4,
        {{1,FP_1,FP_3,FP_5,FP_7},{2,FP_1,FP_2,FP_3,FP_4}},
        {{3,FP_4,FP_4,FP_5,FP_7}});
}
static void test_maxi() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_maxi(3,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("MAXi: max({1,3,5,7},I=4)={4,4,5,7}", prog, 4,
        {{REG_I_PRESET,FP_4,0,0,0},{1,FP_1,FP_3,FP_5,FP_7}},
        {{3,FP_4,FP_4,FP_5,FP_7}});
}

// ──────────────────────────────────────────────────────────────
// Broadcast MINI / MINIi
// VF1={1,3,5,7}, broadcast value=4 → min per lane: {1,3,4,4}
// ──────────────────────────────────────────────────────────────

static void test_minix() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_minix(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MINIx: min({1,3,5,7},VF2.x=4)={1,3,4,4}", prog, 4,
        {{1,FP_1,FP_3,FP_5,FP_7},{2,FP_4,FP_2,FP_3,FP_1}},
        {{3,FP_1,FP_3,FP_4,FP_4}});
}
static void test_miniy() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_miniy(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MINIy: min({1,3,5,7},VF2.y=4)={1,3,4,4}", prog, 4,
        {{1,FP_1,FP_3,FP_5,FP_7},{2,FP_1,FP_4,FP_3,FP_2}},
        {{3,FP_1,FP_3,FP_4,FP_4}});
}
static void test_miniz() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_miniz(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MINIz: min({1,3,5,7},VF2.z=4)={1,3,4,4}", prog, 4,
        {{1,FP_1,FP_3,FP_5,FP_7},{2,FP_1,FP_2,FP_4,FP_3}},
        {{3,FP_1,FP_3,FP_4,FP_4}});
}
static void test_miniw() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_miniw(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MINIw: min({1,3,5,7},VF2.w=4)={1,3,4,4}", prog, 4,
        {{1,FP_1,FP_3,FP_5,FP_7},{2,FP_1,FP_2,FP_3,FP_4}},
        {{3,FP_1,FP_3,FP_4,FP_4}});
}
static void test_minii() {
    const u32 prog[] = { LOWER_NOP, ebit(upper_minii(3,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("MINIi: min({1,3,5,7},I=4)={1,3,4,4}", prog, 4,
        {{REG_I_PRESET,FP_4,0,0,0},{1,FP_1,FP_3,FP_5,FP_7}},
        {{3,FP_1,FP_3,FP_4,FP_4}});
}

// ──────────────────────────────────────────────────────────────
// FTOI0 tests
// ──────────────────────────────────────────────────────────────

// FTOI0: VF2 = (s32)VF1 with 0 fractional bits (truncate toward zero)
// {3.0, -1.0, 2.0, 4.0} → {3, -1(=0xFFFFFFFF), 2, 4}
static void test_vf_ftoi0()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_ftoi0(2,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("FTOI0: {3,-1,2,4}→{0x3,0xFFFFFFFF,0x2,0x4}", prog, 4,
        {{1, FP_3,FP_N1,FP_2,FP_4}},
        {{2, 0x00000003u, 0xFFFFFFFFu, 0x00000002u, 0x00000004u}});
}

// ──────────────────────────────────────────────────────────────
// ITOF0 tests
// ──────────────────────────────────────────────────────────────

// ITOF0: VF2 = (float)(s32)VF1 with 0 fractional bits
// {3, -1(=0xFFFFFFFF), 2, 4} → {3.0, -1.0, 2.0, 4.0}
static void test_vf_itof0()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_itof0(2,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("ITOF0: {0x3,0xFFFFFFFF,0x2,0x4}→{3,-1,2,4}", prog, 4,
        {{1, 0x00000003u, 0xFFFFFFFFu, 0x00000002u, 0x00000004u}},
        {{2, FP_3,FP_N1,FP_2,FP_4}});
}

// ──────────────────────────────────────────────────────────────
// ITOF4 / FTOI4 tests  (4 fractional bits, scale = 16)
// ──────────────────────────────────────────────────────────────
// Input integers {32, -16, 16, 48} as raw bits → / 16.0 → {2, -1, 1, 3}
static void test_vf_itof4()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_itof4(2,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("ITOF4: {32,-16,16,48}→{2,-1,1,3}", prog, 4,
        {{1, 0x00000020u, 0xFFFFFFF0u, 0x00000010u, 0x00000030u}},
        {{2, FP_2, FP_N1, FP_1, FP_3}});
}

// Input floats {3,-1,2,4} * 16 → integers {48, -16, 32, 64}
static void test_vf_ftoi4()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_ftoi4(2,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("FTOI4: {3,-1,2,4}→{48,-16,32,64}", prog, 4,
        {{1, FP_3, FP_N1, FP_2, FP_4}},
        {{2, 0x00000030u, 0xFFFFFFF0u, 0x00000020u, 0x00000040u}});
}

// ──────────────────────────────────────────────────────────────
// ITOF12 / FTOI12 tests  (12 fractional bits, scale = 4096)
// ──────────────────────────────────────────────────────────────
// Input integers {8192, -4096, 4096, 12288} → / 4096.0 → {2, -1, 1, 3}
static void test_vf_itof12()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_itof12(2,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("ITOF12: {8192,-4096,4096,12288}→{2,-1,1,3}", prog, 4,
        {{1, 0x00002000u, 0xFFFFF000u, 0x00001000u, 0x00003000u}},
        {{2, FP_2, FP_N1, FP_1, FP_3}});
}

// Input floats {3,-1,2,4} * 4096 → {12288, -4096, 8192, 16384}
static void test_vf_ftoi12()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_ftoi12(2,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("FTOI12: {3,-1,2,4}→{12288,-4096,8192,16384}", prog, 4,
        {{1, FP_3, FP_N1, FP_2, FP_4}},
        {{2, 0x00003000u, 0xFFFFF000u, 0x00002000u, 0x00004000u}});
}

// ──────────────────────────────────────────────────────────────
// ITOF15 / FTOI15 tests  (15 fractional bits, scale = 32768)
// ──────────────────────────────────────────────────────────────
// Input integers {65536, -32768, 32768, 98304} → / 32768.0 → {2, -1, 1, 3}
static void test_vf_itof15()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_itof15(2,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("ITOF15: {65536,-32768,32768,98304}→{2,-1,1,3}", prog, 4,
        {{1, 0x00010000u, 0xFFFF8000u, 0x00008000u, 0x00018000u}},
        {{2, FP_2, FP_N1, FP_1, FP_3}});
}

// Input floats {3,-1,2,4} * 32768 → {98304, -32768, 65536, 131072}
static void test_vf_ftoi15()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_ftoi15(2,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("FTOI15: {3,-1,2,4}→{98304,-32768,65536,131072}", prog, 4,
        {{1, FP_3, FP_N1, FP_2, FP_4}},
        {{2, 0x00018000u, 0xFFFF8000u, 0x00010000u, 0x00020000u}});
}

// ──────────────────────────────────────────────────────────────
// MOVE (lower-word) tests
// ──────────────────────────────────────────────────────────────

// MOVE VF2, VF1: copies all components
// {1,2,3,4} → VF2={1,2,3,4}
static void test_vf_move()
{
    const u32 prog[] = { lower_move(2,1), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runVFTest("MOVE: VF2=VF1={1,2,3,4}", prog, 4,
        {{1, FP_1,FP_2,FP_3,FP_4}},
        {{2, FP_1,FP_2,FP_3,FP_4}});
}

// ──────────────────────────────────────────────────────────────
// MR32 test
// MR32.xyzw FT, FS: FT.xyzw = {FS.y, FS.z, FS.w, FS.x}  (rotate left)
// ──────────────────────────────────────────────────────────────

// Full DEST: {1,2,3,4} → {2,3,4,1}
static void test_mr32()
{
    const u32 prog[] = { lower_mr32(2,1), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runVFTest("MR32: {1,2,3,4}→{2,3,4,1}", prog, 4,
        {{1, FP_1,FP_2,FP_3,FP_4}},
        {{2, FP_2,FP_3,FP_4,FP_1}});
}

// ──────────────────────────────────────────────────────────────
// MTIR / MFIR — VI↔VF component transfer tests
// ──────────────────────────────────────────────────────────────

// MTIR VI[IT], VF[FS].fsf → VI[IT] = lower 16 bits of VF component bit pattern

static void test_mtir_x()
{
    // VF[1].x = 0x12345678 → VI[2] = 0x5678
    const u32 prog[] = { lower_mtir(2,1,0), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("MTIR: VF[1].x=0x12345678 → VI[2]=0x5678", prog, 4,
        {}, {{1, 0x12345678u, 0,0,0}}, {},
        {}, {{2, 0x5678u}}, {});
}

static void test_mtir_y()
{
    // VF[1].y = 0xABCD0042 → VI[2] = 0x0042
    const u32 prog[] = { lower_mtir(2,1,1), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("MTIR: VF[1].y=0xABCD0042 → VI[2]=0x0042", prog, 4,
        {}, {{1, 0,0xABCD0042u,0,0}}, {},
        {}, {{2, 0x0042u}}, {});
}

static void test_mtir_z()
{
    // VF[1].z = 0x00FF8000 → VI[2] = 0x8000
    const u32 prog[] = { lower_mtir(2,1,2), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("MTIR: VF[1].z=0x00FF8000 → VI[2]=0x8000", prog, 4,
        {}, {{1, 0,0,0x00FF8000u,0}}, {},
        {}, {{2, 0x8000u}}, {});
}

static void test_mtir_w()
{
    // VF[1].w = 0xDEADBEEF → VI[2] = 0xBEEF
    const u32 prog[] = { lower_mtir(2,1,3), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("MTIR: VF[1].w=0xDEADBEEF → VI[2]=0xBEEF", prog, 4,
        {}, {{1, 0,0,0,0xDEADBEEFu}}, {},
        {}, {{2, 0xBEEFu}}, {});
}

// MFIR VF[FT].dest, VI[IS] → VF components = (s32)(s16)VI[IS]

static void test_mfir_positive()
{
    // VI[1]=5 → MFIR VF[2].xyzw → all components = 0x00000005
    const u32 prog[] = { lower_mfir(2,1), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("MFIR: VI[1]=5 → VF[2].xyzw=0x00000005", prog, 4,
        {{1, 5}}, {}, {},
        {{2, 0x00000005u,0x00000005u,0x00000005u,0x00000005u}}, {}, {});
}

static void test_mfir_sign_extend()
{
    // VI[1]=0x8000 (s16=-32768) → sign-extends to 0xFFFF8000
    const u32 prog[] = { lower_mfir(2,1), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("MFIR: VI[1]=0x8000 → VF[2].xyzw=0xFFFF8000 (sign-ext)", prog, 4,
        {{1, 0x8000u}}, {}, {},
        {{2, 0xFFFF8000u,0xFFFF8000u,0xFFFF8000u,0xFFFF8000u}}, {}, {});
}

static void test_mfir_zero()
{
    // VI[1]=0 → VF[2] all = 0
    const u32 prog[] = { lower_mfir(2,1), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("MFIR: VI[1]=0 → VF[2].xyzw=0", prog, 4,
        {{1, 0}}, {}, {},
        {{2, 0u,0u,0u,0u}}, {}, {});
}

static void test_mfir_partial_dest_x()
{
    // MFIR VF[2].x, VI[1]=7 — only X written; Y,Z,W preset unchanged
    const u32 prog[] = { lower_mfir(2,1,DEST_X), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("MFIR: VI[1]=7, .x only → VF[2].x=7, yzw preserved", prog, 4,
        {{1, 7}},
        {{2, 0, FP_1, FP_2, FP_3}},   // preset y=1.0, z=2.0, w=3.0
        {},
        {{2, 0x00000007u, FP_1, FP_2, FP_3}}, {}, {});
}

static void test_mfir_partial_dest_w()
{
    // MFIR VF[2].w, VI[1]=0x7FFF — only W written
    const u32 prog[] = { lower_mfir(2,1,DEST_W), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("MFIR: VI[1]=0x7FFF, .w only → VF[2].w=0x7FFF, xyz preserved", prog, 4,
        {{1, 0x7FFFu}},
        {{2, FP_1, FP_2, FP_3, 0}},   // preset x=1.0, y=2.0, z=3.0
        {},
        {{2, FP_1, FP_2, FP_3, 0x00007FFFu}}, {}, {});
}

// ──────────────────────────────────────────────────────────────
// Partial DEST mask tests
// ──────────────────────────────────────────────────────────────

// ADD.x VF3, VF1, VF2: only X component written; Y,Z,W unchanged
// VF3 preset {0,2,3,4}; VF1={1,*,*,*}, VF2={4,*,*,*}
// After: VF3.x = 1+4 = 5; VF3.{y,z,w} unchanged from preset
static void test_vf_add_dest_x()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_add(3,1,2,DEST_X)), LOWER_NOP, UPPER_NOP };
    runVFTest("ADD.x: only X written, Y/Z/W preserved", prog, 4,
        {{1, FP_1,FP_0,FP_0,FP_0},
         {2, FP_4,FP_0,FP_0,FP_0},
         {3, FP_0,FP_2,FP_3,FP_4}},           // preset VF3 to known values
        {{3, FP_5,FP_2,FP_3,FP_4}});           // x=5, y/z/w unchanged
}

// MUL.yw VF3, VF1, VF2: Y and W written; X,Z unchanged
// VF1={*,2,*,4}, VF2={*,3,*,2}; VF3 preset {1,0,5,0}
// After: VF3.y=2*3=6, VF3.w=4*2=8; VF3.x=1, VF3.z=5 (unchanged)
static void test_vf_mul_dest_yw()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_mul(3,1,2, DEST_Y|DEST_W)), LOWER_NOP, UPPER_NOP };
    runVFTest("MUL.yw: only Y and W written, X/Z preserved", prog, 4,
        {{1, FP_0,FP_2,FP_0,FP_4},
         {2, FP_0,FP_3,FP_0,FP_2},
         {3, FP_1,FP_0,FP_5,FP_0}},
        {{3, FP_1,FP_6,FP_5,FP_8}});
}

// ──────────────────────────────────────────────────────────────
// Encoding / alignment tests
// ──────────────────────────────────────────────────────────────

// Verify that UPPER_NOP (FD_11 index 11) leaves VF registers unchanged.
// Run a block with nothing but NOP pairs; all VF registers should be
// untouched.  Preset VF1 to a known value and confirm it survives.
static void test_vf_upper_nop_preserves()
{
    const u32 prog[] = {
        LOWER_NOP, UPPER_NOP,
        LOWER_NOP, UPPER_NOP,
        LOWER_NOP, UPPER_EBIT_NOP,
        LOWER_NOP, UPPER_NOP,
    };
    runVFTest("UPPER_NOP: VF registers unchanged through NOP block", prog, 8,
        {{1, FP_1,FP_2,FP_3,FP_4}},
        {{1, FP_1,FP_2,FP_3,FP_4}});
}

// Chain: two sequential VF instructions in the same block.
// Pair 0: NOP lower, ADD VF2,VF1,VF0  (VF2 = VF1 + VF0{0,0,0,1.0})
// Pair 1: NOP lower, E-bit MUL VF3,VF2,VF2  (VF3 = VF2 * VF2)
// Pair 2: delay-slot NOP
// VF1={2,3,4,0}; after ADD: VF2={2,3,4,1}
// VF3 = {2²,3²,4²,1²} = {4,9,16,1} = {FP_4,0x41100000,0x41800000,FP_1}
static void test_vf_chained_add_mul()
{
    const u32 prog[] = {
        LOWER_NOP, upper_add(2,1,0),          // VF2 = VF1 + VF0
        LOWER_NOP, ebit(upper_mul(3,2,2)),    // VF3 = VF2 * VF2  (E-bit)
        LOWER_NOP, UPPER_NOP,                  // delay slot
    };
    runVFTest("Chain ADD+MUL: VF2=VF1+VF0, VF3=VF2*VF2={4,9,16,1}", prog, 6,
        {{1, FP_2,FP_3,FP_4,FP_0}},
        {{2, FP_2,FP_3,FP_4,FP_1},
         {3, FP_4,0x41100000u/*9.0f*/,0x41800000u/*16.0f*/,FP_1}});
}

// ──────────────────────────────────────────────────────────────
// Hardwired zero guard tests
// ──────────────────────────────────────────────────────────────

// VI[0] is hardwired to 0 — writes are silently ignored by the JIT
// (analyzeVIreg2 guards with `if (xReg)`, so xReg=0 records no write).
static void test_vi0_write_guard()
{
    // Try to write VI[0] via three different integer instructions.
    // All three should be no-ops; VI[0] must remain 0.
    const u32 prog[] = {
        lower_iaddi(0,0,7),  UPPER_NOP,        // IADDI VI[0]+=7  → ignored
        lower_iadd(0,1,2),   UPPER_NOP,        // IADD  VI[0]=VI[1]+VI[2] → ignored
        lower_iaddiu(0,1,15),UPPER_EBIT_NOP,   // IADDIU VI[0]=VI[1]+15   → ignored
        LOWER_NOP,           UPPER_NOP,
    };
    runTest("VI[0] write guard: IADDI/IADD/IADDIU → VI[0] stays 0", prog, 8,
        {{1,3},{2,4}}, {{0,0}});
}

// VF[0] is hardwired to {0.0, 0.0, 0.0, 1.0} — writes are silently
// ignored (pass2 guards with `if (!_Ft_) return`).
static void test_vf0_write_guard()
{
    // Upper-word: ADD VF[0], VF[1], VF[2]  — dest=0, should be no-op
    const u32 prog[] = {
        lower_move(0,1),              UPPER_NOP,       // MOVE VF[0]=VF[1] (lower) → ignored
        LOWER_NOP,   ebit(upper_add(0,1,2)),           // ADD  VF[0]=VF[1]+VF[2]   → ignored
        LOWER_NOP,                    UPPER_NOP,
    };
    runVFTest("VF[0] write guard: MOVE+ADD → VF[0] stays {0,0,0,1}", prog, 6,
        {{1, FP_3,FP_3,FP_3,FP_3}, {2, FP_4,FP_4,FP_4,FP_4}},
        {{0, FP_0,FP_0,FP_0,FP_1}});
}

// ──────────────────────────────────────────────────────────────
// Accumulator op tests
// ──────────────────────────────────────────────────────────────

// ADDA.xyzw ACC, VF1, VF2: ACC = VF1 + VF2
// {1,2,3,4} + {4,3,2,1} = {5,5,5,5}
static void test_adda()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_adda(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("ADDA: ACC={1+4,2+3,3+2,4+1}={5,5,5,5}", prog, 4,
        {{1, FP_1,FP_2,FP_3,FP_4}, {2, FP_4,FP_3,FP_2,FP_1}},
        {{REG_ACC, FP_5,FP_5,FP_5,FP_5}});
}

// MADD.xyzw VF3, VF1, VF2: VF3 = ACC + VF1*VF2
// Preset ACC={1,2,3,4}, VF1={2,2,2,2}, VF2={2,2,2,2}
// VF3 = {1+4, 2+4, 3+4, 4+4} = {5,6,7,8}
static void test_madd()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_madd(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MADD: ACC+VF1*VF2={1+4,2+4,3+4,4+4}={5,6,7,8}", prog, 4,
        {{REG_ACC, FP_1,FP_2,FP_3,FP_4},
         {1,       FP_2,FP_2,FP_2,FP_2},
         {2,       FP_2,FP_2,FP_2,FP_2}},
        {{3, FP_5,FP_6,FP_7,FP_8}});
}

// MADDA.xyzw ACC, VF1, VF2: ACC += VF1*VF2
// Preset ACC={1,2,3,4}, VF1={2,2,2,2}, VF2={2,2,2,2}
// ACC = {1+4, 2+4, 3+4, 4+4} = {5,6,7,8}
static void test_madda()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_madda(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MADDA: ACC+={2*2,...}; {1+4,2+4,3+4,4+4}={5,6,7,8}", prog, 4,
        {{REG_ACC, FP_1,FP_2,FP_3,FP_4},
         {1,       FP_2,FP_2,FP_2,FP_2},
         {2,       FP_2,FP_2,FP_2,FP_2}},
        {{REG_ACC, FP_5,FP_6,FP_7,FP_8}});
}

// MSUB.xyzw VF3, VF1, VF2: VF3 = ACC - VF1*VF2
// Preset ACC={5,6,7,8}, VF1={1,1,1,1}, VF2={2,2,2,2}
// VF3 = {5-2, 6-2, 7-2, 8-2} = {3,4,5,6}
static void test_msub()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_msub(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MSUB: ACC-VF1*VF2={5-2,6-2,7-2,8-2}={3,4,5,6}", prog, 4,
        {{REG_ACC, FP_5,FP_6,FP_7,FP_8},
         {1,       FP_1,FP_1,FP_1,FP_1},
         {2,       FP_2,FP_2,FP_2,FP_2}},
        {{3, FP_3,FP_4,FP_5,FP_6}});
}

// ──────────────────────────────────────────────────────────────
// SUBA / MULA / MSUBA (non-broadcast ACC write ops)
// ──────────────────────────────────────────────────────────────

// SUBA.xyzw ACC, VF1, VF2: ACC = VF1 - VF2
// VF1={5,6,7,8}, VF2={2,3,4,5} → ACC={3,3,3,3}
static void test_suba()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_suba(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("SUBA: ACC={5-2,6-3,7-4,8-5}={3,3,3,3}", prog, 4,
        {{1, FP_5,FP_6,FP_7,FP_8}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_3,FP_3,FP_3,FP_3}});
}

// MULA.xyzw ACC, VF1, VF2: ACC = VF1 * VF2
// VF1={1,2,3,4}, VF2={2,2,2,2} → ACC={2,4,6,8}
static void test_mula()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_mula(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MULA: ACC={1*2,2*2,3*2,4*2}={2,4,6,8}", prog, 4,
        {{1, FP_1,FP_2,FP_3,FP_4}, {2, FP_2,FP_2,FP_2,FP_2}},
        {{REG_ACC, FP_2,FP_4,FP_6,FP_8}});
}

// MSUBA.xyzw ACC, VF1, VF2: ACC -= VF1 * VF2
// Preset ACC={5,6,7,8}, VF1={1,1,1,1}, VF2={2,2,2,2} → ACC={3,4,5,6}
static void test_msuba()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_msuba(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MSUBA: ACC-={1*2,...}; {5-2,6-2,7-2,8-2}={3,4,5,6}", prog, 4,
        {{REG_ACC, FP_5,FP_6,FP_7,FP_8},
         {1,       FP_1,FP_1,FP_1,FP_1},
         {2,       FP_2,FP_2,FP_2,FP_2}},
        {{REG_ACC, FP_3,FP_4,FP_5,FP_6}});
}

// ──────────────────────────────────────────────────────────────
// Broadcast ADDAx/y/z/w: ACC.dest = FS.dest + FT.{x,y,z,w}
// Setup: VF1={1,1,1,1}, VF2={2,3,4,5}
//   ADDAx: {1+2,...}={3,3,3,3}  ADDAy: {1+3,...}={4,4,4,4}
//   ADDAz: {1+4,...}={5,5,5,5}  ADDAw: {1+5,...}={6,6,6,6}
// ──────────────────────────────────────────────────────────────
static void test_addax()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_addax(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("ADDAx: ACC=VF1+VF2.x={1+2,...}={3,3,3,3}", prog, 4,
        {{1, FP_1,FP_1,FP_1,FP_1}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_3,FP_3,FP_3,FP_3}});
}
static void test_adday()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_adday(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("ADDAy: ACC=VF1+VF2.y={1+3,...}={4,4,4,4}", prog, 4,
        {{1, FP_1,FP_1,FP_1,FP_1}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_4,FP_4,FP_4,FP_4}});
}
static void test_addaz()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_addaz(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("ADDAz: ACC=VF1+VF2.z={1+4,...}={5,5,5,5}", prog, 4,
        {{1, FP_1,FP_1,FP_1,FP_1}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_5,FP_5,FP_5,FP_5}});
}
static void test_addaw()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_addaw(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("ADDAw: ACC=VF1+VF2.w={1+5,...}={6,6,6,6}", prog, 4,
        {{1, FP_1,FP_1,FP_1,FP_1}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_6,FP_6,FP_6,FP_6}});
}

// ──────────────────────────────────────────────────────────────
// Broadcast SUBAx/y/z/w: ACC.dest = FS.dest - FT.{x,y,z,w}
// Setup: VF1={7,7,7,7}, VF2={2,3,4,5}
//   SUBAx: {7-2,...}={5,5,5,5}  SUBAy: {7-3,...}={4,4,4,4}
//   SUBAz: {7-4,...}={3,3,3,3}  SUBAw: {7-5,...}={2,2,2,2}
// ──────────────────────────────────────────────────────────────
static void test_subax()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_subax(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("SUBAx: ACC=VF1-VF2.x={7-2,...}={5,5,5,5}", prog, 4,
        {{1, FP_7,FP_7,FP_7,FP_7}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_5,FP_5,FP_5,FP_5}});
}
static void test_subay()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_subay(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("SUBAy: ACC=VF1-VF2.y={7-3,...}={4,4,4,4}", prog, 4,
        {{1, FP_7,FP_7,FP_7,FP_7}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_4,FP_4,FP_4,FP_4}});
}
static void test_subaz()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_subaz(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("SUBAz: ACC=VF1-VF2.z={7-4,...}={3,3,3,3}", prog, 4,
        {{1, FP_7,FP_7,FP_7,FP_7}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_3,FP_3,FP_3,FP_3}});
}
static void test_subaw()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_subaw(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("SUBAw: ACC=VF1-VF2.w={7-5,...}={2,2,2,2}", prog, 4,
        {{1, FP_7,FP_7,FP_7,FP_7}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_2,FP_2,FP_2,FP_2}});
}

// ──────────────────────────────────────────────────────────────
// Broadcast MULAx/y/z/w: ACC.dest = FS.dest * FT.{x,y,z,w}
// Setup: VF1={1,1,1,1}, VF2={2,3,4,5}
//   MULAx: {1*2,...}={2,2,2,2}  MULAy: {1*3,...}={3,3,3,3}
//   MULAz: {1*4,...}={4,4,4,4}  MULAw: {1*5,...}={5,5,5,5}
// ──────────────────────────────────────────────────────────────
static void test_mulax()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_mulax(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MULAx: ACC=VF1*VF2.x={1*2,...}={2,2,2,2}", prog, 4,
        {{1, FP_1,FP_1,FP_1,FP_1}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_2,FP_2,FP_2,FP_2}});
}
static void test_mulay()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_mulay(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MULAy: ACC=VF1*VF2.y={1*3,...}={3,3,3,3}", prog, 4,
        {{1, FP_1,FP_1,FP_1,FP_1}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_3,FP_3,FP_3,FP_3}});
}
static void test_mulaz()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_mulaz(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MULAz: ACC=VF1*VF2.z={1*4,...}={4,4,4,4}", prog, 4,
        {{1, FP_1,FP_1,FP_1,FP_1}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_4,FP_4,FP_4,FP_4}});
}
static void test_mulaw()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_mulaw(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MULAw: ACC=VF1*VF2.w={1*5,...}={5,5,5,5}", prog, 4,
        {{1, FP_1,FP_1,FP_1,FP_1}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_5,FP_5,FP_5,FP_5}});
}

// ──────────────────────────────────────────────────────────────
// Broadcast MADDAx/y/z/w: ACC.dest += FS.dest * FT.{x,y,z,w}
// Preset ACC={1,1,1,1}, VF1={1,1,1,1}, VF2={2,3,4,5}
//   MADDAx: {1+1*2,...}={3,3,3,3}  MADDAy: {1+1*3,...}={4,4,4,4}
//   MADDAz: {1+1*4,...}={5,5,5,5}  MADDAw: {1+1*5,...}={6,6,6,6}
// ──────────────────────────────────────────────────────────────
static void test_maddax()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_maddax(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MADDAx: ACC+=VF1*VF2.x={1+2,...}={3,3,3,3}", prog, 4,
        {{REG_ACC, FP_1,FP_1,FP_1,FP_1},
         {1,       FP_1,FP_1,FP_1,FP_1}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_3,FP_3,FP_3,FP_3}});
}
static void test_madday()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_madday(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MADDAy: ACC+=VF1*VF2.y={1+3,...}={4,4,4,4}", prog, 4,
        {{REG_ACC, FP_1,FP_1,FP_1,FP_1},
         {1,       FP_1,FP_1,FP_1,FP_1}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_4,FP_4,FP_4,FP_4}});
}
static void test_maddaz()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_maddaz(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MADDAz: ACC+=VF1*VF2.z={1+4,...}={5,5,5,5}", prog, 4,
        {{REG_ACC, FP_1,FP_1,FP_1,FP_1},
         {1,       FP_1,FP_1,FP_1,FP_1}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_5,FP_5,FP_5,FP_5}});
}
static void test_maddaw()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_maddaw(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MADDAw: ACC+=VF1*VF2.w={1+5,...}={6,6,6,6}", prog, 4,
        {{REG_ACC, FP_1,FP_1,FP_1,FP_1},
         {1,       FP_1,FP_1,FP_1,FP_1}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_6,FP_6,FP_6,FP_6}});
}

// ──────────────────────────────────────────────────────────────
// Broadcast MSUBAx/y/z/w: ACC.dest -= FS.dest * FT.{x,y,z,w}
// Preset ACC={7,7,7,7}, VF1={1,1,1,1}, VF2={2,3,4,5}
//   MSUBAx: {7-1*2,...}={5,5,5,5}  MSUBAy: {7-1*3,...}={4,4,4,4}
//   MSUBAz: {7-1*4,...}={3,3,3,3}  MSUBAw: {7-1*5,...}={2,2,2,2}
// ──────────────────────────────────────────────────────────────
static void test_msubax()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_msubax(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MSUBAx: ACC-=VF1*VF2.x={7-2,...}={5,5,5,5}", prog, 4,
        {{REG_ACC, FP_7,FP_7,FP_7,FP_7},
         {1,       FP_1,FP_1,FP_1,FP_1}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_5,FP_5,FP_5,FP_5}});
}
static void test_msubay()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_msubay(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MSUBAy: ACC-=VF1*VF2.y={7-3,...}={4,4,4,4}", prog, 4,
        {{REG_ACC, FP_7,FP_7,FP_7,FP_7},
         {1,       FP_1,FP_1,FP_1,FP_1}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_4,FP_4,FP_4,FP_4}});
}
static void test_msubaz()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_msubaz(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MSUBAz: ACC-=VF1*VF2.z={7-4,...}={3,3,3,3}", prog, 4,
        {{REG_ACC, FP_7,FP_7,FP_7,FP_7},
         {1,       FP_1,FP_1,FP_1,FP_1}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_3,FP_3,FP_3,FP_3}});
}
static void test_msubaw()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_msubaw(1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MSUBAw: ACC-=VF1*VF2.w={7-5,...}={2,2,2,2}", prog, 4,
        {{REG_ACC, FP_7,FP_7,FP_7,FP_7},
         {1,       FP_1,FP_1,FP_1,FP_1}, {2, FP_2,FP_3,FP_4,FP_5}},
        {{REG_ACC, FP_2,FP_2,FP_2,FP_2}});
}

// ──────────────────────────────────────────────────────────────
// Broadcast MADD tests
//
// MADDx/y/z/w FD, FS, FT: FD.dest = ACC.dest + FS.dest * FT.{x,y,z,w}
//
// Common setup: ACC={1,1,1,1}, VF1={1,1,1,1}, VF2={2,3,4,5}
//   MADDx: FD = {1+1*2, ...} = {3,3,3,3}
//   MADDy: FD = {1+1*3, ...} = {4,4,4,4}
//   MADDz: FD = {1+1*4, ...} = {5,5,5,5}
//   MADDw: FD = {1+1*5, ...} = {6,6,6,6}
// ──────────────────────────────────────────────────────────────

static void test_maddx()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_maddx(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MADDx: ACC+VF1*VF2.x={1+2,...}={3,3,3,3}", prog, 4,
        {{REG_ACC, FP_1,FP_1,FP_1,FP_1},
         {1,       FP_1,FP_1,FP_1,FP_1},
         {2,       FP_2,FP_3,FP_4,FP_5}},
        {{3, FP_3,FP_3,FP_3,FP_3}});
}

static void test_maddy()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_maddy(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MADDy: ACC+VF1*VF2.y={1+3,...}={4,4,4,4}", prog, 4,
        {{REG_ACC, FP_1,FP_1,FP_1,FP_1},
         {1,       FP_1,FP_1,FP_1,FP_1},
         {2,       FP_2,FP_3,FP_4,FP_5}},
        {{3, FP_4,FP_4,FP_4,FP_4}});
}

static void test_maddz()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_maddz(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MADDz: ACC+VF1*VF2.z={1+4,...}={5,5,5,5}", prog, 4,
        {{REG_ACC, FP_1,FP_1,FP_1,FP_1},
         {1,       FP_1,FP_1,FP_1,FP_1},
         {2,       FP_2,FP_3,FP_4,FP_5}},
        {{3, FP_5,FP_5,FP_5,FP_5}});
}

static void test_maddw()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_maddw(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MADDw: ACC+VF1*VF2.w={1+5,...}={6,6,6,6}", prog, 4,
        {{REG_ACC, FP_1,FP_1,FP_1,FP_1},
         {1,       FP_1,FP_1,FP_1,FP_1},
         {2,       FP_2,FP_3,FP_4,FP_5}},
        {{3, FP_6,FP_6,FP_6,FP_6}});
}

// ──────────────────────────────────────────────────────────────
// Broadcast MSUB tests
//
// MSUBx/y/z/w FD, FS, FT: FD.dest = ACC.dest - FS.dest * FT.{x,y,z,w}
//
// Common setup: ACC={7,7,7,7}, VF1={1,1,1,1}, VF2={2,3,4,5}
//   MSUBx: FD = {7-1*2, ...} = {5,5,5,5}
//   MSUBy: FD = {7-1*3, ...} = {4,4,4,4}
//   MSUBz: FD = {7-1*4, ...} = {3,3,3,3}
//   MSUBw: FD = {7-1*5, ...} = {2,2,2,2}
// ──────────────────────────────────────────────────────────────

static void test_msubx()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_msubx(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MSUBx: ACC-VF1*VF2.x={7-2,...}={5,5,5,5}", prog, 4,
        {{REG_ACC, FP_7,FP_7,FP_7,FP_7},
         {1,       FP_1,FP_1,FP_1,FP_1},
         {2,       FP_2,FP_3,FP_4,FP_5}},
        {{3, FP_5,FP_5,FP_5,FP_5}});
}

static void test_msuby()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_msuby(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MSUBy: ACC-VF1*VF2.y={7-3,...}={4,4,4,4}", prog, 4,
        {{REG_ACC, FP_7,FP_7,FP_7,FP_7},
         {1,       FP_1,FP_1,FP_1,FP_1},
         {2,       FP_2,FP_3,FP_4,FP_5}},
        {{3, FP_4,FP_4,FP_4,FP_4}});
}

static void test_msubz()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_msubz(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MSUBz: ACC-VF1*VF2.z={7-4,...}={3,3,3,3}", prog, 4,
        {{REG_ACC, FP_7,FP_7,FP_7,FP_7},
         {1,       FP_1,FP_1,FP_1,FP_1},
         {2,       FP_2,FP_3,FP_4,FP_5}},
        {{3, FP_3,FP_3,FP_3,FP_3}});
}

static void test_msubw()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_msubw(3,1,2)), LOWER_NOP, UPPER_NOP };
    runVFTest("MSUBw: ACC-VF1*VF2.w={7-5,...}={2,2,2,2}", prog, 4,
        {{REG_ACC, FP_7,FP_7,FP_7,FP_7},
         {1,       FP_1,FP_1,FP_1,FP_1},
         {2,       FP_2,FP_3,FP_4,FP_5}},
        {{3, FP_2,FP_2,FP_2,FP_2}});
}

// ──────────────────────────────────────────────────────────────
// Q pipeline register op tests
// Both VU0.VI[REG_Q].UL and VU0.pending_q are preset to the same
// value so that the JIT's xmmPQ (built from both at block entry) is
// consistent regardless of which half the opcase reads.
// ──────────────────────────────────────────────────────────────

// ADDq.xyzw VF2, VF1: VF2 = VF1 + Q
// VF1={1,2,3,4}, Q=2 → VF2={3,4,5,6}
static void test_addq()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_addq(2,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("ADDq: {1,2,3,4}+Q(2)={3,4,5,6}", prog, 4,
        {{1,          FP_1,FP_2,FP_3,FP_4},
         {REG_Q_PRESET, FP_2,0,0,0}},
        {{2, FP_3,FP_4,FP_5,FP_6}});
}

// MULq.xyzw VF2, VF1: VF2 = VF1 * Q
// VF1={1,2,3,4}, Q=2 → VF2={2,4,6,8}
static void test_mulq()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_mulq(2,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("MULq: {1,2,3,4}*Q(2)={2,4,6,8}", prog, 4,
        {{1,          FP_1,FP_2,FP_3,FP_4},
         {REG_Q_PRESET, FP_2,0,0,0}},
        {{2, FP_2,FP_4,FP_6,FP_8}});
}

// ──────────────────────────────────────────────────────────────
// I pipeline register op tests
// I register is stored in VU0.VI[REG_I].UL (REG_I = 21).
// ──────────────────────────────────────────────────────────────

// ADDi.xyzw VF2, VF1: VF2 = VF1 + I
// VF1={1,2,3,4}, I=3 → VF2={4,5,6,7}
static void test_addi()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_addi(2,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("ADDi: {1,2,3,4}+I(3)={4,5,6,7}", prog, 4,
        {{1,          FP_1,FP_2,FP_3,FP_4},
         {REG_I_PRESET, FP_3,0,0,0}},
        {{2, FP_4,FP_5,FP_6,FP_7}});
}

// MULi.xyzw VF2, VF1: VF2 = VF1 * I
// VF1={1,2,3,4}, I=2 → VF2={2,4,6,8}
static void test_muli()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_muli(2,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("MULi: {1,2,3,4}*I(2)={2,4,6,8}", prog, 4,
        {{1,          FP_1,FP_2,FP_3,FP_4},
         {REG_I_PRESET, FP_2,0,0,0}},
        {{2, FP_2,FP_4,FP_6,FP_8}});
}

// ──────────────────────────────────────────────────────────────
// SUBq / SUBi tests
// ──────────────────────────────────────────────────────────────

// SUBq.xyzw VF2, VF1: VF2 = VF1 - Q
// VF1={5,6,7,8}, Q=2 → VF2={3,4,5,6}
static void test_subq()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_subq(2,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("SUBq: {5,6,7,8}-Q(2)={3,4,5,6}", prog, 4,
        {{1,            FP_5,FP_6,FP_7,FP_8},
         {REG_Q_PRESET, FP_2,0,0,0}},
        {{2, FP_3,FP_4,FP_5,FP_6}});
}

// SUBi.xyzw VF2, VF1: VF2 = VF1 - I
// VF1={4,5,6,7}, I=3 → VF2={1,2,3,4}
static void test_subi()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_subi(2,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("SUBi: {4,5,6,7}-I(3)={1,2,3,4}", prog, 4,
        {{1,            FP_4,FP_5,FP_6,FP_7},
         {REG_I_PRESET, FP_3,0,0,0}},
        {{2, FP_1,FP_2,FP_3,FP_4}});
}

// ──────────────────────────────────────────────────────────────
// MADDq / MADDi / MSUBq / MSUBi  (FD = ACC ± FS*Q/I)
// ──────────────────────────────────────────────────────────────

// MADDq.xyzw VF3, VF1: VF3 = ACC + VF1*Q
// ACC={1,2,3,4}, VF1={2,2,2,2}, Q=3 → VF3={7,8,9,10}
static void test_maddq()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_maddq(3,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("MADDq: ACC+VF1*Q(3)={7,8,9,10}", prog, 4,
        {{REG_ACC,     FP_1,FP_2,FP_3,FP_4},
         {1,           FP_2,FP_2,FP_2,FP_2},
         {REG_Q_PRESET,FP_3,0,0,0}},
        {{3, FP_7,FP_8,0x41100000u,0x41200000u}});
}

// MADDi.xyzw VF3, VF1: VF3 = ACC + VF1*I
// ACC={1,2,3,4}, VF1={2,2,2,2}, I=3 → VF3={7,8,9,10}
static void test_maddi()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_maddi(3,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("MADDi: ACC+VF1*I(3)={7,8,9,10}", prog, 4,
        {{REG_ACC,     FP_1,FP_2,FP_3,FP_4},
         {1,           FP_2,FP_2,FP_2,FP_2},
         {REG_I_PRESET,FP_3,0,0,0}},
        {{3, FP_7,FP_8,0x41100000u,0x41200000u}});
}

// MSUBq.xyzw VF3, VF1: VF3 = ACC - VF1*Q
// ACC={10,9,8,7}, VF1={2,2,2,2}, Q=3 → VF3={4,3,2,1}
static void test_msubq()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_msubq(3,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("MSUBq: ACC-VF1*Q(3)={4,3,2,1}", prog, 4,
        {{REG_ACC,     0x41200000u,0x41100000u,FP_8,FP_7},
         {1,           FP_2,FP_2,FP_2,FP_2},
         {REG_Q_PRESET,FP_3,0,0,0}},
        {{3, FP_4,FP_3,FP_2,FP_1}});
}

// MSUBi.xyzw VF3, VF1: VF3 = ACC - VF1*I
// ACC={10,9,8,7}, VF1={2,2,2,2}, I=3 → VF3={4,3,2,1}
static void test_msubi()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_msubi(3,1)), LOWER_NOP, UPPER_NOP };
    runVFTest("MSUBi: ACC-VF1*I(3)={4,3,2,1}", prog, 4,
        {{REG_ACC,     0x41200000u,0x41100000u,FP_8,FP_7},
         {1,           FP_2,FP_2,FP_2,FP_2},
         {REG_I_PRESET,FP_3,0,0,0}},
        {{3, FP_4,FP_3,FP_2,FP_1}});
}

// ──────────────────────────────────────────────────────────────
// MULAq / ADDAq / MSUBAq / MULAi / ADDAi / MSUBAi / SUBAi / MADDAi
// (ACC ← operation with Q or I pipeline register)
// ──────────────────────────────────────────────────────────────

// MULAq.xyzw ACC, VF1: ACC = VF1*Q
// VF1={3,3,3,3}, Q=2 → ACC={6,6,6,6}
static void test_mulaq()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_mulaq(1)), LOWER_NOP, UPPER_NOP };
    runVFTest("MULAq: ACC=VF1*Q(2)={6,6,6,6}", prog, 4,
        {{1,           FP_3,FP_3,FP_3,FP_3},
         {REG_Q_PRESET,FP_2,0,0,0}},
        {{REG_ACC, FP_6,FP_6,FP_6,FP_6}});
}

// ADDAq.xyzw ACC, VF1: ACC = VF1+Q
// VF1={3,3,3,3}, Q=2 → ACC={5,5,5,5}
static void test_addaq()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_addaq(1)), LOWER_NOP, UPPER_NOP };
    runVFTest("ADDAq: ACC=VF1+Q(2)={5,5,5,5}", prog, 4,
        {{1,           FP_3,FP_3,FP_3,FP_3},
         {REG_Q_PRESET,FP_2,0,0,0}},
        {{REG_ACC, FP_5,FP_5,FP_5,FP_5}});
}

// SUBAq.xyzw ACC, VF1: ACC = VF1 - Q
// VF1={4,4,4,4}, Q=1 → ACC={3,3,3,3}
static void test_subaq()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_subaq(1)), LOWER_NOP, UPPER_NOP };
    runVFTest("SUBAq: ACC=VF1-Q(1)={3,3,3,3}", prog, 4,
        {{1,           FP_4,FP_4,FP_4,FP_4},
         {REG_Q_PRESET,FP_1,0,0,0}},
        {{REG_ACC, FP_3,FP_3,FP_3,FP_3}});
}

// MADDAq.xyzw ACC, VF1: ACC += VF1*Q
// preset ACC={1,2,3,4}, VF1={2,2,2,2}, Q=3 → ACC={7,8,9,10}
static void test_maddaq()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_maddaq(1)), LOWER_NOP, UPPER_NOP };
    runVFTest("MADDAq: ACC+=VF1*Q(3); {1,2,3,4}+6={7,8,9,10}", prog, 4,
        {{REG_ACC,     FP_1,FP_2,FP_3,FP_4},
         {1,           FP_2,FP_2,FP_2,FP_2},
         {REG_Q_PRESET,FP_3,0,0,0}},
        {{REG_ACC, FP_7,FP_8,0x41100000u,0x41200000u}});
}

// MSUBAq.xyzw ACC, VF1: ACC -= VF1*Q
// preset ACC={10,10,10,10}, VF1={2,2,2,2}, Q=3 → ACC={4,4,4,4}
static void test_msubaq()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_msubaq(1)), LOWER_NOP, UPPER_NOP };
    runVFTest("MSUBAq: ACC-=VF1*Q(3); {10,...}-6={4,...}", prog, 4,
        {{REG_ACC,     0x41200000u,0x41200000u,0x41200000u,0x41200000u},
         {1,           FP_2,FP_2,FP_2,FP_2},
         {REG_Q_PRESET,FP_3,0,0,0}},
        {{REG_ACC, FP_4,FP_4,FP_4,FP_4}});
}

// MULAi.xyzw ACC, VF1: ACC = VF1*I
// VF1={3,3,3,3}, I=2 → ACC={6,6,6,6}
static void test_mulai()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_mulai(1)), LOWER_NOP, UPPER_NOP };
    runVFTest("MULAi: ACC=VF1*I(2)={6,6,6,6}", prog, 4,
        {{1,           FP_3,FP_3,FP_3,FP_3},
         {REG_I_PRESET,FP_2,0,0,0}},
        {{REG_ACC, FP_6,FP_6,FP_6,FP_6}});
}

// ADDAi.xyzw ACC, VF1: ACC = VF1+I
// VF1={3,3,3,3}, I=2 → ACC={5,5,5,5}
static void test_addai()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_addai(1)), LOWER_NOP, UPPER_NOP };
    runVFTest("ADDAi: ACC=VF1+I(2)={5,5,5,5}", prog, 4,
        {{1,           FP_3,FP_3,FP_3,FP_3},
         {REG_I_PRESET,FP_2,0,0,0}},
        {{REG_ACC, FP_5,FP_5,FP_5,FP_5}});
}

// MSUBAi.xyzw ACC, VF1: ACC -= VF1*I
// preset ACC={10,10,10,10}, VF1={2,2,2,2}, I=3 → ACC={4,4,4,4}
static void test_msubai()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_msubai(1)), LOWER_NOP, UPPER_NOP };
    runVFTest("MSUBAi: ACC-=VF1*I(3); {10,...}-6={4,...}", prog, 4,
        {{REG_ACC,     0x41200000u,0x41200000u,0x41200000u,0x41200000u},
         {1,           FP_2,FP_2,FP_2,FP_2},
         {REG_I_PRESET,FP_3,0,0,0}},
        {{REG_ACC, FP_4,FP_4,FP_4,FP_4}});
}

// SUBAi.xyzw ACC, VF1: ACC = VF1-I
// VF1={5,5,5,5}, I=2 → ACC={3,3,3,3}
static void test_subai()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_subai(1)), LOWER_NOP, UPPER_NOP };
    runVFTest("SUBAi: ACC=VF1-I(2)={3,3,3,3}", prog, 4,
        {{1,           FP_5,FP_5,FP_5,FP_5},
         {REG_I_PRESET,FP_2,0,0,0}},
        {{REG_ACC, FP_3,FP_3,FP_3,FP_3}});
}

// MADDAi.xyzw ACC, VF1: ACC += VF1*I
// preset ACC={1,2,3,4}, VF1={2,2,2,2}, I=3 → ACC={7,8,9,10}
static void test_maddai()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_maddai(1)), LOWER_NOP, UPPER_NOP };
    runVFTest("MADDAi: ACC+=VF1*I(3); {1,2,3,4}+6={7,8,9,10}", prog, 4,
        {{REG_ACC,     FP_1,FP_2,FP_3,FP_4},
         {1,           FP_2,FP_2,FP_2,FP_2},
         {REG_I_PRESET,FP_3,0,0,0}},
        {{REG_ACC, FP_7,FP_8,0x41100000u,0x41200000u}});
}

// ──────────────────────────────────────────────────────────────
// Load/Store tests
//
// VU0 data memory = 4 KB (VU0.Mem), 256 quadwords (each 16 bytes).
// mVUaddrFix: byte = (VI[base] + imm11) & 0xFF << 4
// VF components stored as [x,y,z,w] = UL[0..3] in memory order.
// ──────────────────────────────────────────────────────────────

// LQ VF1, 0(VI[2]): VF1 = VU_MEM[VI[2]+0]
// VI[2]=4, MEM[4]={1,2,3,4} → VF1={1,2,3,4}
static void test_lq()
{
    const u32 prog[] = { lower_lq(1,2,0), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("LQ: VF1=MEM[4]={1,2,3,4}", prog, 4,
        {{2, 4}}, {},
        {{4u, FP_1,FP_2,FP_3,FP_4}},
        {{1, FP_1,FP_2,FP_3,FP_4}},
        {}, {});
}

// SQ VF[1], 0(VI[2]): VU_MEM[VI[2]+0] = VF1
// VI[2]=4, VF1={1,2,3,4} → MEM[4]={1,2,3,4}
static void test_sq()
{
    const u32 prog[] = { lower_sq(1,2,0), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("SQ: MEM[4]=VF1={1,2,3,4}", prog, 4,
        {{2, 4}}, {{1, FP_1,FP_2,FP_3,FP_4}},
        {},
        {}, {},
        {{4u, FP_1,FP_2,FP_3,FP_4}});
}

// LQI VF1, (VI[2])+: VF1 = VU_MEM[VI[2]]; VI[2]++
// VI[2]=4, MEM[4]={1,2,3,4} → VF1={1,2,3,4}, VI[2]=5
static void test_lqi()
{
    const u32 prog[] = { lower_lqi(1,2), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("LQI: VF1=MEM[4]; VI[2]++ → VF1={1,2,3,4} VI[2]=5", prog, 4,
        {{2, 4}}, {},
        {{4u, FP_1,FP_2,FP_3,FP_4}},
        {{1, FP_1,FP_2,FP_3,FP_4}},
        {{2, 5}}, {});
}

// SQI VF[1], (VI[2])+: VU_MEM[VI[2]] = VF1; VI[2]++
// VI[2]=4, VF1={1,2,3,4} → MEM[4]={1,2,3,4}, VI[2]=5
static void test_sqi()
{
    const u32 prog[] = { lower_sqi(1,2), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("SQI: MEM[4]=VF1; VI[2]++ → MEM[4]={1,2,3,4} VI[2]=5", prog, 4,
        {{2, 4}}, {{1, FP_1,FP_2,FP_3,FP_4}},
        {},
        {}, {{2, 5}},
        {{4u, FP_1,FP_2,FP_3,FP_4}});
}

// LQD VF1, -(VI[2]): VI[2]--; VF1 = VU_MEM[VI[2]]
// VI[2]=5, MEM[4]={1,2,3,4} → VI[2]=4, VF1={1,2,3,4}
static void test_lqd()
{
    const u32 prog[] = { lower_lqd(1,2), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("LQD: VI[2]--; VF1=MEM[4] → VF1={1,2,3,4} VI[2]=4", prog, 4,
        {{2, 5}}, {},
        {{4u, FP_1,FP_2,FP_3,FP_4}},
        {{1, FP_1,FP_2,FP_3,FP_4}},
        {{2, 4}}, {});
}

// SQD VF[1], -(VI[2]): VI[2]--; VU_MEM[VI[2]] = VF1
// VI[2]=5, VF1={1,2,3,4} → VI[2]=4, MEM[4]={1,2,3,4}
static void test_sqd()
{
    const u32 prog[] = { lower_sqd(1,2), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("SQD: VI[2]--; MEM[4]=VF1 → MEM[4]={1,2,3,4} VI[2]=4", prog, 4,
        {{2, 5}}, {{1, FP_1,FP_2,FP_3,FP_4}},
        {},
        {}, {{2, 4}},
        {{4u, FP_1,FP_2,FP_3,FP_4}});
}

// LQ VF1, 2(VI[2]): VF1 = VU_MEM[VI[2]+2]
// VI[2]=2, imm=2 → address=4, MEM[4]={1,2,3,4} → VF1={1,2,3,4}
static void test_lq_offset()
{
    const u32 prog[] = { lower_lq(1,2,2), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("LQ offset: VF1=MEM[VI[2]+2=4]={1,2,3,4}", prog, 4,
        {{2, 2}}, {},
        {{4u, FP_1,FP_2,FP_3,FP_4}},
        {{1, FP_1,FP_2,FP_3,FP_4}},
        {}, {});
}

// SQ VF[1], 3(VI[2]): VU_MEM[VI[2]+3] = VF1
// VI[2]=1, imm=3 → address=4, VF1={1,2,3,4} → MEM[4]={1,2,3,4}
static void test_sq_offset()
{
    const u32 prog[] = { lower_sq(1,2,3), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("SQ offset: MEM[VI[2]+3=4]=VF1={1,2,3,4}", prog, 4,
        {{2, 1}}, {{1, FP_1,FP_2,FP_3,FP_4}},
        {},
        {}, {},
        {{4u, FP_1,FP_2,FP_3,FP_4}});
}

// LQ VF1.xy, 0(VI[2]) DEST_XY: only X/Y written, Z/W preserved
// VI[2]=4, MEM[4]={1,2,3,4}, VF1 preset {0,0,5.0,6.0}
// → VF1={1,2,5.0,6.0}
static void test_lq_partial_dest()
{
    const u32 prog[] = { lower_lq(1,2,0,DEST_X|DEST_Y), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("LQ DEST_XY: X/Y from mem, Z/W preserved", prog, 4,
        {{2, 4}},
        {{1, 0u, 0u, FP_5, FP_6}},
        {{4u, FP_1,FP_2,FP_3,FP_4}},
        {{1, FP_1,FP_2,FP_5,FP_6}},
        {}, {});
}

// SQ VF[1].xz, 0(VI[2]) DEST_XZ: only X/Z written to mem, Y/W stay 0
// VI[2]=4, VF1={1,2,3,4}, MEM[4] starts as zeros
// → MEM[4]={1,0,3,0}
static void test_sq_partial_dest()
{
    const u32 prog[] = { lower_sq(1,2,0,DEST_X|DEST_Z), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("SQ DEST_XZ: X/Z to mem, Y/W unchanged (0)", prog, 4,
        {{2, 4}}, {{1, FP_1,FP_2,FP_3,FP_4}},
        {},
        {}, {},
        {{4u, FP_1, 0u, FP_3, 0u}});
}

// LQI VF1.yw, (VI[2])+ DEST_YW: only Y/W written, X/Z preserved; VI[2]++
// VI[2]=4, MEM[4]={1,2,3,4}, VF1 preset {5.0,0,6.0,0}
// → VF1={5.0,2,6.0,4}, VI[2]=5
static void test_lqi_partial_dest()
{
    const u32 prog[] = { lower_lqi(1,2,DEST_Y|DEST_W), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("LQI DEST_YW: Y/W from mem, X/Z preserved; VI[2]++", prog, 4,
        {{2, 4}},
        {{1, FP_5, 0u, FP_6, 0u}},
        {{4u, FP_1,FP_2,FP_3,FP_4}},
        {{1, FP_5,FP_2,FP_6,FP_4}},
        {{2, 5}}, {});
}

// ──────────────────────────────────────────────────────────────
// VU-CPU I/O tests: ILW, ISW, ILWR, ISWR, MFP, XTOP, XITOP
// ──────────────────────────────────────────────────────────────

// ILW.X  VI[2], 0(VI[3])  — VI[3]=5, load lower 16 bits of MEM[5].x into VI[2]
static void test_ilw()
{
    const u32 prog[] = { lower_ilw(2, 3, 0, DEST_X), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("ILW.X: VI[2]=MEM[VI[3]+0].x low16", prog, 4,
        {{3, 5}}, {}, {{5u, 0x00001234u, 0u, 0u, 0u}},
        {}, {{2, 0x1234}}, {});
}

// ILW.Y  VI[1], 2(VI[2])  — VI[2]=1, imm=2, addr=3; load lower 16 bits of MEM[3].y
static void test_ilw_offset_y()
{
    const u32 prog[] = { lower_ilw(1, 2, 2, DEST_Y), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("ILW.Y: VI[1]=MEM[VI[2]+2].y low16", prog, 4,
        {{2, 1}}, {}, {{3u, 0u, 0x00005678u, 0u, 0u}},
        {}, {{1, 0x5678}}, {});
}

// ISW.XYZW  VI[1], 0(VI[2])  — store VI[1] as u32 to all 4 lanes of MEM[VI[2]]
static void test_isw()
{
    const u32 prog[] = { lower_isw(1, 2, 0, DEST_XYZW), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("ISW.XYZW: MEM[VI[2]].xyzw = VI[1]", prog, 4,
        {{1, 0xABCD}, {2, 6}}, {}, {},
        {}, {}, {{6u, 0x0000ABCDu, 0x0000ABCDu, 0x0000ABCDu, 0x0000ABCDu}});
}

// ISW.XZ  VI[1], 0(VI[2])  — store VI[1] only to X and Z lanes
static void test_isw_partial()
{
    const u32 prog[] = { lower_isw(1, 2, 0, DEST_X|DEST_Z), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("ISW.XZ: MEM[VI[2]].xz = VI[1], y/w=0", prog, 4,
        {{1, 0x1234}, {2, 7}}, {}, {},
        {}, {}, {{7u, 0x00001234u, 0u, 0x00001234u, 0u}});
}

// ILWR.Z  VI[1], (VI[2])  — VI[2]=4, load lower 16 bits of MEM[4].z into VI[1]
static void test_ilwr()
{
    const u32 prog[] = { lower_ilwr(1, 2, DEST_Z), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("ILWR.Z: VI[1]=MEM[VI[2]].z low16", prog, 4,
        {{2, 4}}, {}, {{4u, 0u, 0u, 0x00009ABCu, 0u}},
        {}, {{1, 0x9ABC}}, {});
}

// ISWR.Y  VI[1], (VI[2])  — VI[1]=0x7777, VI[2]=5, store to MEM[5].y only
static void test_iswr()
{
    const u32 prog[] = { lower_iswr(1, 2, DEST_Y), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("ISWR.Y: MEM[VI[2]].y = VI[1]", prog, 4,
        {{1, 0x7777}, {2, 5}}, {}, {},
        {}, {}, {{5u, 0u, 0x00007777u, 0u, 0u}});
}

// MFP on VU0 is a NOP — VF[1] must remain unchanged
static void test_mfp_nop()
{
    const u32 prog[] = { lower_mfp(1, DEST_XYZW), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("MFP (VU0 NOP): VF[1] unchanged", prog, 4,
        {}, {{1, FP_1, FP_2, FP_3, FP_4}}, {},
        {{1, FP_1, FP_2, FP_3, FP_4}}, {}, {});
}

// XTOP on VU0 is a NOP — VI[1] must remain unchanged
static void test_xtop_nop()
{
    const u32 prog[] = { lower_xtop(1), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("XTOP (VU0 NOP): VI[1] unchanged", prog, 4,
        {{1, 42}}, {}, {},
        {}, {{1, 42}}, {});
}

// XITOP on VU0: VI[IT] = vif0Regs.itop & 0xFF
static void test_xitop()
{
    vif0Regs.itop = 0x1A5u; // masked to 0xA5 by JIT (&0xFF for VU0)
    const u32 prog[] = { lower_xitop(1), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("XITOP: VI[1] = vif0.itop(0x1A5) & 0xFF = 0xA5", prog, 4,
        {}, {}, {},
        {}, {{1, 0xA5}}, {});
    vif0Regs.itop = 0u;
}

// WAITP on VU0 is a NOP — VF[1] must remain unchanged
static void test_waitp_nop()
{
    const u32 prog[] = { lower_waitp(), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("WAITP (VU0 NOP): VF[1] unchanged", prog, 4,
        {}, {{1, FP_1, FP_2, FP_3, FP_4}}, {},
        {{1, FP_1, FP_2, FP_3, FP_4}}, {}, {});
}

// XGKICK on VU0 is a NOP — VI[IS] must remain unchanged
static void test_xgkick_nop()
{
    const u32 prog[] = { lower_xgkick(1), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("XGKICK (VU0 NOP): VI[1] unchanged", prog, 4,
        {{1, 0x1F}}, {}, {},
        {}, {{1, 0x1F}}, {});
}

// ──────────────────────────────────────────────────────────────
// Branch tests
//
// All branch programs use the same "skip over sentinel" pattern:
//
//   Pair 0 (iPC=0): branch instruction with imm11=+2 (to pair 3)
//   Pair 1 (iPC=2): delay slot: IADDI VI[3],VI[0],7 → VI[3]=7 (always)
//   Pair 2 (iPC=4): IADDI VI[3],VI[3],99  (only if NOT taken)
//   Pair 3 (iPC=6): NOP | E-bit (terminate)
//   Pair 4 (iPC=8): delay-slot NOP
//
// branchAddr(iPC=0, imm11=+2) = ((0+2+4)&mask)<<2 = 24 bytes = pair 3.
// Branch TAKEN:     VI[3] = 7   (delay slot, pair 2 skipped)
// Branch NOT taken: VI[3] = 106 (delay slot + pair 2)
// ──────────────────────────────────────────────────────────────

// IBEQ: equal  — VI[1]=5, VI[2]=5 → taken → VI[3]=7
static void test_ibeq_taken()
{
    const u32 prog[] = {
        lower_ibeq(1,2,2),   UPPER_NOP,
        lower_iaddi(3,0,7),  UPPER_NOP,
        lower_iaddi(3,3,99), UPPER_NOP,
        LOWER_NOP,           UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
    };
    runTest("IBEQ taken: VI[1]==VI[2]=5 → VI[3]=7", prog, 10,
        {{1,5},{2,5}}, {{3,7}});
}

static void test_ibgtz()
{
    const u32 prog[] = {
        lower_ibgtz(1,2),    UPPER_NOP,
        lower_iaddi(3,0,7),  UPPER_NOP,
        lower_iaddi(3,3,99), UPPER_NOP,
        LOWER_NOP,           UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
    };
    runTest("IBGTZ taken: VI[1]=1 > 0 → VI[3]=7", prog, 10,
        {{1,1}}, {{3,7}});
}

static void test_ibltz()
{
    // 0xFFFF = -1 as signed 16-bit
    const u32 prog[] = {
        lower_ibltz(1,2),    UPPER_NOP,
        lower_iaddi(3,0,7),  UPPER_NOP,
        lower_iaddi(3,3,99), UPPER_NOP,
        LOWER_NOP,           UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
    };
    runTest("IBLTZ taken: VI[1]=0xFFFF(-1) < 0 → VI[3]=7", prog, 10,
        {{1,0xFFFFu}}, {{3,7}});
}

static void test_iblez()
{
    const u32 prog[] = {
        lower_iblez(1,2),    UPPER_NOP,
        lower_iaddi(3,0,7),  UPPER_NOP,
        lower_iaddi(3,3,99), UPPER_NOP,
        LOWER_NOP,           UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
    };
    // VI[1]=0 <= 0 → taken
    runTest("IBLEZ taken: VI[1]=0 <= 0 → VI[3]=7", prog, 10,
        {}, {{3,7}});
}

static void test_ibgez()
{
    const u32 prog[] = {
        lower_ibgez(1,2),    UPPER_NOP,
        lower_iaddi(3,0,7),  UPPER_NOP,
        lower_iaddi(3,3,99), UPPER_NOP,
        LOWER_NOP,           UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
    };
    // VI[1]=0 >= 0 → taken
    runTest("IBGEZ taken: VI[1]=0 >= 0 → VI[3]=7", prog, 10,
        {}, {{3,7}});
}

// ──────────────────────────────────────────────────────────────
// Branch not-taken tests
//
// Common program structure (branch at pair 0 with imm11=+2):
//   Pair 0: <branch> (condition NOT met)       | NOP
//   Pair 1: delay slot: IADDI VI[3]+=1         | NOP   ← always runs
//   Pair 2: fall-through: IADDI VI[3]+=10      | NOP   ← only if not-taken
//   Pair 3: NOP                                | E-bit
//   Pair 4: delay slot                         | NOP
//
// Not-taken: delay slot (pair 1) + fall-through (pair 2) → VI[3] = 11
// If branch were wrongly taken: delay slot only → VI[3] = 1
// ──────────────────────────────────────────────────────────────

// IBNE not-taken: VI[1] == VI[2] → branch condition false → fall through
static void test_ibne_not_taken()
{
    const u32 prog[] = {
        lower_ibne(1,2,2),   UPPER_NOP,
        lower_iaddi(3,0,1),  UPPER_NOP,
        lower_iaddi(3,3,10), UPPER_NOP,
        LOWER_NOP,           UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
    };
    runTest("IBNE not-taken: VI[1]==VI[2]=5 → VI[3]=11", prog, 10,
        {{1,5},{2,5}}, {{3,11}});
}

// IBEQ not-taken: VI[1] != VI[2] → fall through
static void test_ibeq_not_taken()
{
    const u32 prog[] = {
        lower_ibeq(1,2,2),   UPPER_NOP,
        lower_iaddi(3,0,1),  UPPER_NOP,
        lower_iaddi(3,3,10), UPPER_NOP,
        LOWER_NOP,           UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
    };
    runTest("IBEQ not-taken: VI[1]=5 != VI[2]=3 → VI[3]=11", prog, 10,
        {{1,5},{2,3}}, {{3,11}});
}

// IBGTZ not-taken: VI[1] = 0 (not > 0) → fall through
static void test_ibgtz_not_taken()
{
    const u32 prog[] = {
        lower_ibgtz(1,2),    UPPER_NOP,
        lower_iaddi(3,0,1),  UPPER_NOP,
        lower_iaddi(3,3,10), UPPER_NOP,
        LOWER_NOP,           UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
    };
    runTest("IBGTZ not-taken: VI[1]=0 (not >0) → VI[3]=11", prog, 10,
        {}, {{3,11}});
}

// IBLTZ not-taken: VI[1] = 1 (not < 0) → fall through
static void test_ibltz_not_taken()
{
    const u32 prog[] = {
        lower_ibltz(1,2),    UPPER_NOP,
        lower_iaddi(3,0,1),  UPPER_NOP,
        lower_iaddi(3,3,10), UPPER_NOP,
        LOWER_NOP,           UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
    };
    runTest("IBLTZ not-taken: VI[1]=1 (not <0) → VI[3]=11", prog, 10,
        {{1,1}}, {{3,11}});
}

// IBLEZ not-taken: VI[1] = 1 (not <= 0) → fall through
static void test_iblez_not_taken()
{
    const u32 prog[] = {
        lower_iblez(1,2),    UPPER_NOP,
        lower_iaddi(3,0,1),  UPPER_NOP,
        lower_iaddi(3,3,10), UPPER_NOP,
        LOWER_NOP,           UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
    };
    runTest("IBLEZ not-taken: VI[1]=1 (not <=0) → VI[3]=11", prog, 10,
        {{1,1}}, {{3,11}});
}

// IBGEZ not-taken: VI[1] = 0xFFFF (-1 signed, not >= 0) → fall through
static void test_ibgez_not_taken()
{
    const u32 prog[] = {
        lower_ibgez(1,2),    UPPER_NOP,
        lower_iaddi(3,0,1),  UPPER_NOP,
        lower_iaddi(3,3,10), UPPER_NOP,
        LOWER_NOP,           UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
    };
    runTest("IBGEZ not-taken: VI[1]=0xFFFF(-1) (not >=0) → VI[3]=11", prog, 10,
        {{1,0xFFFFu}}, {{3,11}});
}

// B: unconditional branch — skips pair 2 sentinel
//
// Pair 0: B +2 (to pair 3)
// Pair 1: delay slot: IADDI VI[1],VI[0],7 → VI[1]=7
// Pair 2: IADDI VI[1],VI[1],99  (skipped)
// Pair 3: NOP | E-bit
// Pair 4: delay slot
static void test_b()
{
    const u32 prog[] = {
        lower_b(2),          UPPER_NOP,
        lower_iaddi(1,0,7),  UPPER_NOP,
        lower_iaddi(1,1,99), UPPER_NOP,
        LOWER_NOP,           UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
    };
    runTest("B: unconditional branch skips pair 2 → VI[1]=7", prog, 10,
        {}, {{1,7}});
}

// BAL IT, imm11: branch and link
// BAL at pair 0 (xPC=0): link = (0+16)/8 = 2
// VI[1] = 2 (link)
// Pair 0: BAL VI[1], +2  (link→VI[1]=2, branch to pair 3)
// Pair 1: delay slot NOP
// Pair 2: (skipped) IADDI VI[3],VI[0],99
// Pair 3: NOP | E-bit
// Pair 4: delay slot
static void test_bal()
{
    const u32 prog[] = {
        lower_bal(1,2),      UPPER_NOP,
        LOWER_NOP,           UPPER_NOP,
        lower_iaddi(3,0,99), UPPER_NOP,
        LOWER_NOP,           UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
    };
    runTest("BAL: link VI[1]=(0+16)/8=2, pair 2 skipped → VI[1]=2 VI[3]=0",
            prog, 10, {}, {{1,2},{3,0}});
}

// JR IS: jump register — target byte = VI[IS] << 3
// VI[1]=3 → jump to pair 3 (byte 24)
// Pair 0: JR VI[1]
// Pair 1: delay slot: IADDI VI[2],VI[0],5 → VI[2]=5
// Pair 2: IADDI VI[2],VI[2],99  (skipped)
// Pair 3: NOP | E-bit
// Pair 4: delay slot
static void test_jr()
{
    const u32 prog[] = {
        lower_jr(1),         UPPER_NOP,
        lower_iaddi(2,0,5),  UPPER_NOP,
        lower_iaddi(2,2,99), UPPER_NOP,
        LOWER_NOP,           UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
    };
    // VI[1]=3 → JR target = pair 3
    runTest("JR: VI[1]=3 → jump pair 3, delay VI[2]=5, pair 2 skipped",
            prog, 10, {{1,3}}, {{2,5}});
}

// JALR IS, IT: jump and link register
// VI[1]=3 → jump to pair 3; link VI[2] = (xPC+16)/8 = (0+16)/8 = 2
// Pair 0: JALR VI[1], VI[2]
// Pair 1: delay slot NOP
// Pair 2: (skipped) IADDI VI[3],VI[0],99
// Pair 3: NOP | E-bit
// Pair 4: delay slot
static void test_jalr()
{
    const u32 prog[] = {
        lower_jalr(1,2),     UPPER_NOP,
        LOWER_NOP,           UPPER_NOP,
        lower_iaddi(3,0,99), UPPER_NOP,
        LOWER_NOP,           UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
    };
    runTest("JALR: VI[1]=3→pair3, link VI[2]=2, VI[3]=0",
            prog, 10, {{1,3}}, {{2,2},{3,0}});
}

// ──────────────────────────────────────────────────────────────
// FTZ / DaZ tests
//
// ARM64 FPCR.FZ=1 flushes both subnormal inputs (DaZ) and subnormal
// outputs (FtZ) to ±0.  The PS2 VU always runs with FZ=1.
// runVFTest now sets FPCR = EmuConfig.Cpu.VU0FPCR (FZ=1) before each
// execution so these tests correctly observe the hardware behaviour.
// ──────────────────────────────────────────────────────────────

// MUL(2^-64, 2^-64): product 2^-128 is a subnormal → FTZ flushes to 0.
// Without FTZ the result would be 0x00200000 (= FP_DENORM).
static void test_ftz_mul_output()
{
    const u32 prog[] = {
        LOWER_NOP, upper_mul(3,1,2),
        LOWER_NOP, UPPER_EBIT_NOP,
        LOWER_NOP, UPPER_NOP,
    };
    runVFTest("FTZ: MUL(2^-64,2^-64)→subnormal→0", prog, 6,
        {{1, FP_TINY_NORM,FP_TINY_NORM,FP_TINY_NORM,FP_TINY_NORM},
         {2, FP_TINY_NORM,FP_TINY_NORM,FP_TINY_NORM,FP_TINY_NORM}},
        {{3, 0u,0u,0u,0u}});
}

// ADD(subnormal, 0): subnormal input flushed to 0 before add → 0+0=0.
// Without DaZ the result would be 0x00200000.
static void test_ftz_add_denorm_input()
{
    const u32 prog[] = {
        LOWER_NOP, upper_add(3,1,2),
        LOWER_NOP, UPPER_EBIT_NOP,
        LOWER_NOP, UPPER_NOP,
    };
    runVFTest("FTZ: ADD(subnorm,0)→0 (DaZ)", prog, 6,
        {{1, FP_DENORM,FP_DENORM,FP_DENORM,FP_DENORM},
         {2, FP_0,FP_0,FP_0,FP_0}},
        {{3, 0u,0u,0u,0u}});
}

// ADD(subnormal, 1.0): subnormal input treated as 0 → 0+1=1.
// Confirms the input flush is active, not a coincidental zero result.
static void test_ftz_add_denorm_with_normal()
{
    const u32 prog[] = {
        LOWER_NOP, upper_add(3,1,2),
        LOWER_NOP, UPPER_EBIT_NOP,
        LOWER_NOP, UPPER_NOP,
    };
    runVFTest("FTZ: ADD(subnorm,1.0)→1.0 (DaZ flushes subnorm input)", prog, 6,
        {{1, FP_DENORM,FP_DENORM,FP_DENORM,FP_DENORM},
         {2, FP_1,FP_1,FP_1,FP_1}},
        {{3, FP_1,FP_1,FP_1,FP_1}});
}

// ──────────────────────────────────────────────────────────────
// VF clamp tests
//
// All use a 3-pair program so the tested op sits at pair 0 with
// clampE=false, triggering the regular mVUclamp1/2 paths:
//
//   Pair 0: LOWER_NOP | <tested upper op>   (no E-bit → clampE=false)
//   Pair 1: LOWER_NOP | UPPER_EBIT_NOP      (E-bit → terminates)
//   Pair 2: LOWER_NOP | UPPER_NOP           (delay slot)
//
// ── mVUclamp1: result clamping (vu0Overflow) ─────────────────
//
// Mechanism: after arithmetic, Fminnm(result, PS2_MAX) then Fmaxnm(result, PS2_MIN).
//   • IEEE +∞ (0x7F800000) → Fminnm with 0x7F7FFFFF → 0x7F7FFFFF
//   • IEEE -∞ (0xFF800000) → Fmaxnm with 0xFF7FFFFF → 0xFF7FFFFF
//   • +NaN                 → Fminnm returns non-NaN operand  → 0x7F7FFFFF
//   • -NaN                 → Fminnm treats as +NaN (no sign trust) → 0x7F7FFFFF
//
// ── mVUclamp2: operand clamping (vu0SignOverflow) ────────────
//
// Mechanism: before arithmetic, integer Smin then Umin on each operand.
//   Smin(v, 0x7F7FFFFF): clamps positive-domain overflow (s32 > 0x7F7FFFFF → 0x7F7FFFFF)
//   Umin(v, 0xFF7FFFFF): clamps negative-domain overflow (u32 > 0xFF7FFFFF → 0xFF7FFFFF)
//   • +∞ (0x7F800000): Smin → 0x7F7FFFFF; Umin → no change → 0x7F7FFFFF
//   • -∞ (0xFF800000): Smin → no change (negative s32); Umin → 0xFF7FFFFF
//   • +NaN (0x7FC00000): Smin → 0x7F7FFFFF; Umin → no change → 0x7F7FFFFF
//   • -NaN (0xFFC00000): Smin → no change; Umin → 0xFF7FFFFF
// ──────────────────────────────────────────────────────────────

// ── Result overflow clamp (vu0Overflow=true) ─────────────────

// ADD(PS2_MAX, PS2_MAX) → IEEE +∞ without clamp, +PS2_MAX with.
static void test_clamp_overflow_add_pos()
{
    // 0x7F7FFFFF + 0x7F7FFFFF overflows IEEE → 0x7F800000 (+inf)
    // With vu0Overflow: Fminnm(+inf, 0x7F7FFFFF) = 0x7F7FFFFF
    const u32 prog[] = {
        LOWER_NOP, upper_add(3,1,2),
        LOWER_NOP, UPPER_EBIT_NOP,
        LOWER_NOP, UPPER_NOP,
    };
    runClampTest("clamp vu0Overflow: ADD(PS2_MAX+PS2_MAX)→+PS2_MAX", prog, 6,
        true, false,
        {{1, FP_PS2_MAX,FP_PS2_MAX,FP_PS2_MAX,FP_PS2_MAX},
         {2, FP_PS2_MAX,FP_PS2_MAX,FP_PS2_MAX,FP_PS2_MAX}},
        {{3, FP_PS2_MAX,FP_PS2_MAX,FP_PS2_MAX,FP_PS2_MAX}});
}

// ADD(-PS2_MAX, -PS2_MAX) → IEEE -∞ without clamp, -PS2_MAX with.
static void test_clamp_overflow_add_neg()
{
    // 0xFF7FFFFF + 0xFF7FFFFF overflows IEEE → 0xFF800000 (-inf)
    // With vu0Overflow: Fmaxnm(-inf, 0xFF7FFFFF) = 0xFF7FFFFF
    const u32 prog[] = {
        LOWER_NOP, upper_add(3,1,2),
        LOWER_NOP, UPPER_EBIT_NOP,
        LOWER_NOP, UPPER_NOP,
    };
    runClampTest("clamp vu0Overflow: ADD(-PS2_MAX+-PS2_MAX)→-PS2_MAX", prog, 6,
        true, false,
        {{1, FP_PS2_MIN,FP_PS2_MIN,FP_PS2_MIN,FP_PS2_MIN},
         {2, FP_PS2_MIN,FP_PS2_MIN,FP_PS2_MIN,FP_PS2_MIN}},
        {{3, FP_PS2_MIN,FP_PS2_MIN,FP_PS2_MIN,FP_PS2_MIN}});
}

// ADD(+∞, -∞) → NaN → with vu0Overflow clamped to +PS2_MAX.
// mVUclamp1 uses Fminnm: when one operand is NaN, returns the other.
// So Fminnm(NaN, PS2_MAX) = PS2_MAX, then Fmaxnm(PS2_MAX, PS2_MIN) = PS2_MAX.
static void test_clamp_overflow_nan()
{
    // +inf + (-inf) = NaN; clamp makes it +PS2_MAX
    const u32 prog[] = {
        LOWER_NOP, upper_add(3,1,2),
        LOWER_NOP, UPPER_EBIT_NOP,
        LOWER_NOP, UPPER_NOP,
    };
    runClampTest("clamp vu0Overflow: ADD(+inf,-inf)→NaN→+PS2_MAX", prog, 6,
        true, false,
        {{1, FP_IEEE_INF, FP_IEEE_INF, FP_IEEE_INF, FP_IEEE_INF},
         {2, FP_IEEE_NINF,FP_IEEE_NINF,FP_IEEE_NINF,FP_IEEE_NINF}},
        {{3, FP_PS2_MAX,FP_PS2_MAX,FP_PS2_MAX,FP_PS2_MAX}});
}

// ── Operand sign-overflow clamp (vu0SignOverflow=true) ───────

// ADD(+∞, 0) with vu0SignOverflow: +∞ operand → Smin → +PS2_MAX.
static void test_clamp_signoverflow_inf_pos()
{
    // Operand 0x7F800000: Smin(+inf, 0x7F7FFFFF) = 0x7F7FFFFF → add 0 → 0x7F7FFFFF
    const u32 prog[] = {
        LOWER_NOP, upper_add(3,1,2),
        LOWER_NOP, UPPER_EBIT_NOP,
        LOWER_NOP, UPPER_NOP,
    };
    runClampTest("clamp vu0SignOverflow: ADD(+inf,0)→+PS2_MAX", prog, 6,
        false, true,
        {{1, FP_IEEE_INF, FP_IEEE_INF, FP_IEEE_INF, FP_IEEE_INF}},
        {{3, FP_PS2_MAX,FP_PS2_MAX,FP_PS2_MAX,FP_PS2_MAX}});
}

// ADD(-∞, 0) with vu0SignOverflow: -∞ operand → Umin → -PS2_MAX.
static void test_clamp_signoverflow_inf_neg()
{
    // Operand 0xFF800000: Smin → unchanged (negative s32); Umin(0xFF800000, 0xFF7FFFFF) = 0xFF7FFFFF
    const u32 prog[] = {
        LOWER_NOP, upper_add(3,1,2),
        LOWER_NOP, UPPER_EBIT_NOP,
        LOWER_NOP, UPPER_NOP,
    };
    runClampTest("clamp vu0SignOverflow: ADD(-inf,0)→-PS2_MAX", prog, 6,
        false, true,
        {{1, FP_IEEE_NINF,FP_IEEE_NINF,FP_IEEE_NINF,FP_IEEE_NINF}},
        {{3, FP_PS2_MIN,FP_PS2_MIN,FP_PS2_MIN,FP_PS2_MIN}});
}

// ADD(+NaN, 0) with vu0SignOverflow: +NaN → Smin → +PS2_MAX.
static void test_clamp_signoverflow_nan_pos()
{
    // 0x7FC00000 as s32 = 2143289344 > 0x7F7FFFFF → Smin → 0x7F7FFFFF
    const u32 prog[] = {
        LOWER_NOP, upper_add(3,1,2),
        LOWER_NOP, UPPER_EBIT_NOP,
        LOWER_NOP, UPPER_NOP,
    };
    runClampTest("clamp vu0SignOverflow: ADD(+NaN,0)→+PS2_MAX", prog, 6,
        false, true,
        {{1, FP_IEEE_NAN, FP_IEEE_NAN, FP_IEEE_NAN, FP_IEEE_NAN}},
        {{3, FP_PS2_MAX,FP_PS2_MAX,FP_PS2_MAX,FP_PS2_MAX}});
}

// ADD(-NaN, 0) with vu0SignOverflow: -NaN → Umin → -PS2_MAX.
static void test_clamp_signoverflow_nan_neg()
{
    // 0xFFC00000 as u32 = 4291411968 > 0xFF7FFFFF = 4286578687 → Umin → 0xFF7FFFFF
    const u32 prog[] = {
        LOWER_NOP, upper_add(3,1,2),
        LOWER_NOP, UPPER_EBIT_NOP,
        LOWER_NOP, UPPER_NOP,
    };
    runClampTest("clamp vu0SignOverflow: ADD(-NaN,0)→-PS2_MAX", prog, 6,
        false, true,
        {{1, FP_IEEE_NNAN,FP_IEEE_NNAN,FP_IEEE_NNAN,FP_IEEE_NNAN}},
        {{3, FP_PS2_MIN,FP_PS2_MIN,FP_PS2_MIN,FP_PS2_MIN}});
}

// ──────────────────────────────────────────────────────────────
// RNG register tests (RINIT / RGET / RNEXT / RXOR)
//
// R register lives at VI[REG_R].UL (REG_R = 20).
// Preset via REG_R_PRESET sentinel in runVFTest.
// RGET is used to read R back into a VF register for verification.
//
// RNEXT algorithm:
//   gprT1 = (R>>4) & 1;  gprT2 = (R>>22) & 1;
//   R = ((R<<1) ^ gprT1 ^ gprT2) & 0x7fffff | 0x3f800000
// ──────────────────────────────────────────────────────────────

// RINIT then RGET: VF1.x = 0x40100000, fsf=x (0)
//   R = (0x40100000 & 0x7fffff) | 0x3f800000 = 0x00100000 | 0x3f800000 = 0x3f900000
//   RGET VF2: VF2.xyzw = {0x3f900000, ...}
static void test_rinit_from_vf()
{
    const u32 prog[] = {
        lower_rinit(1, 0), UPPER_NOP,           // RINIT: R = (VF1.x & 0x7fffff) | 0x3f800000
        lower_rget(2),     UPPER_EBIT_NOP,       // RGET: VF2 = R (terminates)
        LOWER_NOP,         UPPER_NOP,             // delay slot
    };
    runVFTest("RINIT(VF1.x=0x40100000)+RGET→VF2=0x3f900000", prog, 6,
        {{1, 0x40100000u, 0u, 0u, 0u}},
        {{2, 0x3f900000u, 0x3f900000u, 0x3f900000u, 0x3f900000u}});
}

// RGET does not advance R: preset R directly, verify VF gets the value unchanged.
static void test_rget_no_advance()
{
    const u32 prog[] = {
        lower_rget(2), UPPER_EBIT_NOP,
        LOWER_NOP,     UPPER_NOP,
    };
    runVFTest("RGET: copies R to VF2, does not advance", prog, 4,
        {{REG_R_PRESET, 0x3f900000u, 0u, 0u, 0u}},
        {{2, 0x3f900000u, 0x3f900000u, 0x3f900000u, 0x3f900000u}});
}

// RNEXT: R=0x3f800010 →
//   gprT1 = (0x3f800010>>4)&1 = 1;  gprT2 = (0x3f800010>>22)&1 = 0
//   R = ((0x3f800010<<1) ^ 1) & 0x7fffff | 0x3f800000
//     = (0x7f000020 ^ 1) & 0x7fffff | 0x3f800000
//     = 0x7f000021 & 0x7fffff | 0x3f800000
//     = 0x00000021 | 0x3f800000 = 0x3f800021
static void test_rnext_advance()
{
    const u32 prog[] = {
        lower_rnext(2), UPPER_EBIT_NOP,
        LOWER_NOP,      UPPER_NOP,
    };
    runVFTest("RNEXT: R=0x3f800010→0x3f800021, written to VF2.xyzw", prog, 4,
        {{REG_R_PRESET, 0x3f800010u, 0u, 0u, 0u}},
        {{2, 0x3f800021u, 0x3f800021u, 0x3f800021u, 0x3f800021u}});
}

// RXOR then RGET: R=0x3f900000, VF1.x=0x40100000 (fsf=x=0)
//   R ^= (0x40100000 & 0x7fffff) = 0x3f900000 ^ 0x00100000 = 0x3f800000
//   RGET VF2: VF2.xyzw = {0x3f800000, ...}
static void test_rxor()
{
    const u32 prog[] = {
        lower_rxor(1, 0), UPPER_NOP,             // RXOR: R ^= (VF1.x & 0x7fffff)
        lower_rget(2),    UPPER_EBIT_NOP,         // RGET: VF2 = R (terminates)
        LOWER_NOP,        UPPER_NOP,               // delay slot
    };
    runVFTest("RXOR(R=0x3f900000, VF1.x=0x40100000)→R=0x3f800000", prog, 6,
        {{REG_R_PRESET, 0x3f900000u, 0u, 0u, 0u},
         {1,            0x40100000u, 0u, 0u, 0u}},
        {{2, 0x3f800000u, 0x3f800000u, 0x3f800000u, 0x3f800000u}});
}

// RNEXT with partial dest=XY: only VF2.xy written; VF2.zw unchanged from preset.
// R=0x3f800010 → new R = 0x3f800021 (same as test_rnext_advance).
static void test_rnext_partial_dest()
{
    const u32 prog[] = {
        lower_rnext(2, DEST_X|DEST_Y), UPPER_EBIT_NOP,
        LOWER_NOP,                     UPPER_NOP,
    };
    runVFTest("RNEXT.xy: only X/Y written; Z/W from preset", prog, 4,
        {{REG_R_PRESET, 0x3f800010u, 0u, 0u, 0u},
         {2,            0u, 0u, FP_3, FP_4}},
        {{2, 0x3f800021u, 0x3f800021u, FP_3, FP_4}});
}

// ──────────────────────────────────────────────────────────────
// Flag register tests
//
// Expected values verified against pcsx2-master (x86 reference):
//
// STATUS register (12-bit PS2 format after mVUallocSFLAGc):
//   Bit 0 (Z):  any component was zero (from internal sticky bits[11:8])
//   Bit 1 (S):  any component was negative (internal bits[15:12])
//   Bit 6 (ZS): any component was zero, current (internal bits[3:0])
//   Bit 7 (SS): any component was negative, current (internal bits[7:4])
//
// MAC register (16-bit raw macFlag):
//   Bits 0-3: Zw,Zz,Zy,Zx — per-component zero flags
//   Bits 4-7: Sw,Sz,Sy,Sx — per-component sign flags
//
// CLIP register 6-bit new-bits layout (from mVU_CLIP):
//   Bit 0: +x (FS.x > |FT.w|)   Bit 1: -x (FS.x < -|FT.w|)
//   Bit 2: +y                     Bit 3: -y
//   Bit 4: +z                     Bit 5: -z
//   CLIP = (old_CLIP << 6) | comp6 (old_CLIP=0 in all tests below)
//
// For STATUS/MAC tests the arithmetic op and flag-read must be in the
// same JIT block so the analyzer enables flag updates:
//   Pair 0: LOWER_NOP | upper_mul(...)   (no E-bit → JIT sees FSAND ahead)
//   Pair 1: lower_fsand/fmand | UPPER_EBIT_NOP
//   Pair 2: delay slot
// ──────────────────────────────────────────────────────────────

// ── CLIP register: FCSET / FCGET / FCAND / FCOR ───────────────

// FCSET sets CLIP directly; FCGET reads lower 12 bits.
// 3 NOP pairs needed between FCSET (write ts=4) and FCGET (read at cycle≥4).
static void test_fcset_fcget()
{
    const u32 prog[] = {
        lower_fcset(0x123456u), UPPER_NOP,       // pair 0: write ts=4
        LOWER_NOP,              UPPER_NOP,       // pair 1
        LOWER_NOP,              UPPER_NOP,       // pair 2
        LOWER_NOP,              UPPER_NOP,       // pair 3
        lower_fcget(1),         UPPER_EBIT_NOP,  // pair 4: read cycle=4
        LOWER_NOP,              UPPER_NOP,       // delay slot
    };
    runTest("FCSET $123456 → FCGET VI[1] = 0x456", prog, 12,
            {}, {{1, 0x0456u}});
}

// FCAND: VI[1] = (CLIP & imm24) != 0 ? 1 : 0
static void test_fcand_match()
{
    const u32 prog[] = {
        lower_fcset(0x000015u), UPPER_NOP,
        LOWER_NOP,              UPPER_NOP,
        LOWER_NOP,              UPPER_NOP,
        LOWER_NOP,              UPPER_NOP,
        lower_fcand(0x000015u), UPPER_EBIT_NOP,
        LOWER_NOP,              UPPER_NOP,
    };
    runTest("FCAND: CLIP=0x15 & 0x15 != 0 → VI[1]=1", prog, 12,
            {}, {{1, 1u}});
}

static void test_fcand_nomatch()
{
    // 0x15 = 0b010101; 0x08 = 0b001000; AND = 0 → VI[1] = 0
    const u32 prog[] = {
        lower_fcset(0x000015u), UPPER_NOP,
        LOWER_NOP,              UPPER_NOP,
        LOWER_NOP,              UPPER_NOP,
        LOWER_NOP,              UPPER_NOP,
        lower_fcand(0x000008u), UPPER_EBIT_NOP,
        LOWER_NOP,              UPPER_NOP,
    };
    runTest("FCAND: CLIP=0x15 & 0x08 = 0 → VI[1]=0", prog, 12,
            {}, {{1, 0u}});
}

// FCOR: VI[1] = ((CLIP | imm24) == 0xFFFFFF) ? 1 : 0
static void test_fcor_full()
{
    const u32 prog[] = {
        lower_fcset(0xFFFFFFu), UPPER_NOP,
        LOWER_NOP,              UPPER_NOP,
        LOWER_NOP,              UPPER_NOP,
        LOWER_NOP,              UPPER_NOP,
        lower_fcor(0x000000u),  UPPER_EBIT_NOP,
        LOWER_NOP,              UPPER_NOP,
    };
    runTest("FCOR: CLIP=0xFFFFFF | 0 = 0xFFFFFF → VI[1]=1", prog, 12,
            {}, {{1, 1u}});
}

static void test_fcor_notfull()
{
    const u32 prog[] = {
        lower_fcset(0x000001u), UPPER_NOP,
        LOWER_NOP,              UPPER_NOP,
        LOWER_NOP,              UPPER_NOP,
        LOWER_NOP,              UPPER_NOP,
        lower_fcor(0x000000u),  UPPER_EBIT_NOP,
        LOWER_NOP,              UPPER_NOP,
    };
    runTest("FCOR: CLIP=0x1 | 0 != 0xFFFFFF → VI[1]=0", prog, 12,
            {}, {{1, 0u}});
}

// ── CLIP instruction ──────────────────────────────────────────

// CLIP.xyz VF1, VF2: VF1={2,3,4,0}, FT.w=1.0
// +x(2>1), +y(3>1), +z(4>1) → bits 0,2,4 = 0x15
// old_CLIP=0 (micro_clipflags zeroed) → new CLIP = (0<<6)|0x15 = 0x15
static void test_clip_all_positive()
{
    const u32 prog[] = {
        LOWER_NOP,      upper_clip(1, 2),   // pair 0: CLIP write ts=4
        LOWER_NOP,      UPPER_NOP,          // pair 1
        LOWER_NOP,      UPPER_NOP,          // pair 2
        LOWER_NOP,      UPPER_NOP,          // pair 3
        lower_fcget(3), UPPER_EBIT_NOP,     // pair 4: read cycle=4
        LOWER_NOP,      UPPER_NOP,          // delay slot
    };
    runLSTest("CLIP: {2,3,4}>1 → +x,+y,+z bits set → 0x15", prog, 12,
        {},
        {{1, FP_2, FP_3, FP_4, FP_0},
         {2, FP_0, FP_0, FP_0, FP_1}},
        {},
        {}, {{3, 0x0015u}}, {});
}

// CLIP with all-negative xyz: -x,-y,-z all < -|FT.w|
// bits 1,3,5 set = 0x2A
static void test_clip_all_negative()
{
    const u32 prog[] = {
        LOWER_NOP,      upper_clip(1, 2),
        LOWER_NOP,      UPPER_NOP,
        LOWER_NOP,      UPPER_NOP,
        LOWER_NOP,      UPPER_NOP,
        lower_fcget(3), UPPER_EBIT_NOP,
        LOWER_NOP,      UPPER_NOP,
    };
    runLSTest("CLIP: {-2,-3,-4}<-1 → -x,-y,-z bits set → 0x2A", prog, 12,
        {},
        {{1, FP_N2, FP_N3, FP_N4, FP_0},
         {2, FP_0,  FP_0,  FP_0,  FP_1}},
        {},
        {}, {{3, 0x002Au}}, {});
}

// ── STATUS flag: FSAND / FSOR ─────────────────────────────────
//
// imm12=0x00C3 masks bits 0,1,6,7 (Z,S,ZS,SS) without triggering
// DevCon warnings for U/O/I/D bits.

// MUL(1s, 0s)=0 → STATUS = 0x41 (Z bit0 + ZS bit6)
static void test_fsand_zero_result()
{
    const u32 prog[] = {
        LOWER_NOP,               upper_mul(3, 1, 2),  // pair 0: STATUS write ts=4
        LOWER_NOP,               UPPER_NOP,           // pair 1
        LOWER_NOP,               UPPER_NOP,           // pair 2
        LOWER_NOP,               UPPER_NOP,           // pair 3
        lower_fsand(1, 0x00C3u), UPPER_EBIT_NOP,      // pair 4: read cycle=4
        LOWER_NOP,               UPPER_NOP,           // delay slot
    };
    runLSTest("FSAND: MUL(1s,0s)→0 → STATUS Z+ZS = 0x41", prog, 12,
        {},
        {{1, FP_1,FP_1,FP_1,FP_1}, {2, FP_0,FP_0,FP_0,FP_0}},
        {},
        {}, {{1, 0x0041u}}, {});
}

// MUL(1s, -1s) → STATUS = 0x82 (S bit1 + SS bit7)
static void test_fsand_sign_result()
{
    const u32 prog[] = {
        LOWER_NOP,               upper_mul(3, 1, 2),
        LOWER_NOP,               UPPER_NOP,
        LOWER_NOP,               UPPER_NOP,
        LOWER_NOP,               UPPER_NOP,
        lower_fsand(1, 0x00C3u), UPPER_EBIT_NOP,
        LOWER_NOP,               UPPER_NOP,
    };
    runLSTest("FSAND: MUL(1s,-1s)→neg → STATUS S+SS = 0x82", prog, 12,
        {},
        {{1, FP_1, FP_1, FP_1, FP_1},
         {2, FP_N1,FP_N1,FP_N1,FP_N1}},
        {},
        {}, {{1, 0x0082u}}, {});
}

// FSOR: STATUS | imm12.  MUL→zero gives 0x41, OR 0x02 (S) → 0x43
static void test_fsor()
{
    const u32 prog[] = {
        LOWER_NOP,            upper_mul(3, 1, 2),
        LOWER_NOP,            UPPER_NOP,
        LOWER_NOP,            UPPER_NOP,
        LOWER_NOP,            UPPER_NOP,
        lower_fsor(1, 0x02u), UPPER_EBIT_NOP,
        LOWER_NOP,            UPPER_NOP,
    };
    runLSTest("FSOR: STATUS(0x41) | 0x02 → VI[1]=0x43", prog, 12,
        {},
        {{1, FP_1,FP_1,FP_1,FP_1}, {2, FP_0,FP_0,FP_0,FP_0}},
        {},
        {}, {{1, 0x0043u}}, {});
}

// ── MAC flag: FMAND / FMOR ────────────────────────────────────

// MUL(1s, 0s)=0 → MAC = 0x000F (Zw,Zz,Zy,Zx bits 0-3)
// VI[4]=0xFFFF masks all; FMAND VI[2],VI[4] → VI[2] = MAC & 0xFFFF = 0x000F
static void test_fmand_zero_result()
{
    const u32 prog[] = {
        LOWER_NOP,         upper_mul(3, 1, 2),  // pair 0: MAC write ts=4
        LOWER_NOP,         UPPER_NOP,           // pair 1
        LOWER_NOP,         UPPER_NOP,           // pair 2
        LOWER_NOP,         UPPER_NOP,           // pair 3
        lower_fmand(2, 4), UPPER_EBIT_NOP,      // pair 4: read cycle=4
        LOWER_NOP,         UPPER_NOP,           // delay slot
    };
    runLSTest("FMAND: MUL(1s,0s)→0 → MAC Zxyzw = 0x000F", prog, 12,
        {{4, 0xFFFFu}},
        {{1, FP_1,FP_1,FP_1,FP_1}, {2, FP_0,FP_0,FP_0,FP_0}},
        {},
        {}, {{2, 0x000Fu}}, {});
}

// MUL(1s, -1s) → MAC = 0x00F0 (Sw,Sz,Sy,Sx bits 4-7)
static void test_fmand_sign_result()
{
    const u32 prog[] = {
        LOWER_NOP,         upper_mul(3, 1, 2),
        LOWER_NOP,         UPPER_NOP,
        LOWER_NOP,         UPPER_NOP,
        LOWER_NOP,         UPPER_NOP,
        lower_fmand(2, 4), UPPER_EBIT_NOP,
        LOWER_NOP,         UPPER_NOP,
    };
    runLSTest("FMAND: MUL(1s,-1s)→neg → MAC Sxyzw = 0x00F0", prog, 12,
        {{4, 0xFFFFu}},
        {{1, FP_1, FP_1, FP_1, FP_1},
         {2, FP_N1,FP_N1,FP_N1,FP_N1}},
        {},
        {}, {{2, 0x00F0u}}, {});
}

// FMOR: VI[2] = MAC | VI[4].  MUL→zero gives MAC=0x000F; OR 0x00F0 → 0x00FF
static void test_fmor()
{
    const u32 prog[] = {
        LOWER_NOP,        upper_mul(3, 1, 2),
        LOWER_NOP,        UPPER_NOP,
        LOWER_NOP,        UPPER_NOP,
        LOWER_NOP,        UPPER_NOP,
        lower_fmor(2, 4), UPPER_EBIT_NOP,
        LOWER_NOP,        UPPER_NOP,
    };
    runLSTest("FMOR: MAC(0x000F) | VI[4](0x00F0) → VI[2]=0x00FF", prog, 12,
        {{4, 0x00F0u}},
        {{1, FP_1,FP_1,FP_1,FP_1}, {2, FP_0,FP_0,FP_0,FP_0}},
        {},
        {}, {{2, 0x00FFu}}, {});
}

// ── FSSET: STATUS sticky bits ─────────────────────────────────
//
// FSSET $imm12 sets the sticky bits (ZS/SS/US/OS/IS/DS) of the STATUS
// register directly from imm12[11:6], preserving the current (non-sticky)
// bits [5:0] from the most recent FMAC operation.
// Pipeline: VUPIPE_FMAC — result is visible 4 pairs after FSSET.

// FSSET 0x040 → sets only ZS (bit 6); prior STATUS=0; FSAND→0x040
static void test_fsset_zs()
{
    const u32 prog[] = {
        lower_fsset(0x040u),     UPPER_NOP,           // pair 0: FSSET ZS
        LOWER_NOP,               UPPER_NOP,           // pair 1
        LOWER_NOP,               UPPER_NOP,           // pair 2
        LOWER_NOP,               UPPER_NOP,           // pair 3
        lower_fsand(1, 0x00C3u), UPPER_EBIT_NOP,      // pair 4: read STATUS
        LOWER_NOP,               UPPER_NOP,           // pair 5: delay slot
    };
    runLSTest("FSSET 0x040: sets ZS sticky → FSAND→VI[1]=0x040", prog, 12,
        {}, {}, {},
        {}, {{1, 0x0040u}}, {});
}

// FSSET 0x0C0 → sets ZS+SS (bits 6,7); FSAND→0x0C0
static void test_fsset_zs_ss()
{
    const u32 prog[] = {
        lower_fsset(0x0C0u),     UPPER_NOP,
        LOWER_NOP,               UPPER_NOP,
        LOWER_NOP,               UPPER_NOP,
        LOWER_NOP,               UPPER_NOP,
        lower_fsand(1, 0x00C3u), UPPER_EBIT_NOP,
        LOWER_NOP,               UPPER_NOP,
    };
    runLSTest("FSSET 0x0C0: sets ZS+SS → FSAND→VI[1]=0x0C0", prog, 12,
        {}, {}, {},
        {}, {{1, 0x00C0u}}, {});
}

// ── FSEQ: STATUS equality ──────────────────────────────────────
//
// FSEQ $FT, $imm12: VI[FT] = (STATUS == imm12) ? 1 : 0
// The comparison is bitwise — all matching bits must agree exactly.

// STATUS=0 (no prior FMAC); FSEQ imm12=0x000 → match → VI[1]=1
static void test_fseq_zero_match()
{
    const u32 prog[] = {
        LOWER_NOP,              UPPER_NOP,           // pair 0
        LOWER_NOP,              UPPER_NOP,           // pair 1
        LOWER_NOP,              UPPER_NOP,           // pair 2
        LOWER_NOP,              UPPER_NOP,           // pair 3
        lower_fseq(1, 0x000u),  UPPER_EBIT_NOP,      // pair 4: FSEQ STATUS==0
        LOWER_NOP,              UPPER_NOP,           // pair 5: delay slot
    };
    runLSTest("FSEQ imm=0x000: STATUS==0 → VI[1]=1", prog, 12,
        {}, {}, {},
        {}, {{1, 1u}}, {});
}

// MUL(1s,0s)=0 → STATUS Z+ZS=0x041; FSEQ imm=0x041 → exact match → VI[1]=1
static void test_fseq_zs_match()
{
    const u32 prog[] = {
        LOWER_NOP,              upper_mul(3, 1, 2),  // pair 0: MUL→Z+ZS
        LOWER_NOP,              UPPER_NOP,           // pair 1
        LOWER_NOP,              UPPER_NOP,           // pair 2
        LOWER_NOP,              UPPER_NOP,           // pair 3
        lower_fseq(1, 0x041u),  UPPER_EBIT_NOP,      // pair 4: FSEQ STATUS==0x041
        LOWER_NOP,              UPPER_NOP,           // pair 5: delay slot
    };
    runLSTest("FSEQ imm=0x041: MUL(1s,0s)→Z+ZS match → VI[1]=1", prog, 12,
        {},
        {{1, FP_1,FP_1,FP_1,FP_1}, {2, FP_0,FP_0,FP_0,FP_0}},
        {},
        {}, {{1, 1u}}, {});
}

// MUL(1s,0s)=0 → STATUS Z+ZS=0x041; FSEQ imm=0x001 (Z only, misses ZS) → VI[1]=0
static void test_fseq_no_match()
{
    const u32 prog[] = {
        LOWER_NOP,              upper_mul(3, 1, 2),
        LOWER_NOP,              UPPER_NOP,
        LOWER_NOP,              UPPER_NOP,
        LOWER_NOP,              UPPER_NOP,
        lower_fseq(1, 0x001u),  UPPER_EBIT_NOP,
        LOWER_NOP,              UPPER_NOP,
    };
    runLSTest("FSEQ imm=0x001: STATUS=0x041 != 0x001 → VI[1]=0", prog, 12,
        {},
        {{1, FP_1,FP_1,FP_1,FP_1}, {2, FP_0,FP_0,FP_0,FP_0}},
        {},
        {}, {{1, 0u}}, {});
}

// ── FMEQ: MAC equality ─────────────────────────────────────────
//
// FMEQ $FT, $IS: VI[FT] = (MAC == VI[IS]) ? 1 : 0
// MAC is 16-bit; comparison is exact (all 16 bits must match).

// MUL(1s,0s)=0 → MAC=Zxyzw=0x000F; VI[4]=0x000F; FMEQ → match → VI[1]=1
static void test_fmeq_match()
{
    const u32 prog[] = {
        LOWER_NOP,           upper_mul(3, 1, 2),  // pair 0: MUL→MAC Zxyzw=0x000F
        LOWER_NOP,           UPPER_NOP,           // pair 1
        LOWER_NOP,           UPPER_NOP,           // pair 2
        LOWER_NOP,           UPPER_NOP,           // pair 3
        lower_fmeq(1, 4),    UPPER_EBIT_NOP,      // pair 4: FMEQ VI[1]=MAC==VI[4]
        LOWER_NOP,           UPPER_NOP,           // pair 5: delay slot
    };
    runLSTest("FMEQ: MUL(1s,0s)→MAC=0x000F == VI[4]=0x000F → VI[1]=1", prog, 12,
        {{4, 0x000Fu}},
        {{1, FP_1,FP_1,FP_1,FP_1}, {2, FP_0,FP_0,FP_0,FP_0}},
        {},
        {}, {{1, 1u}}, {});
}

// MAC=0x000F; VI[4]=0x00FF (mismatch); FMEQ → VI[1]=0
static void test_fmeq_no_match()
{
    const u32 prog[] = {
        LOWER_NOP,           upper_mul(3, 1, 2),
        LOWER_NOP,           UPPER_NOP,
        LOWER_NOP,           UPPER_NOP,
        LOWER_NOP,           UPPER_NOP,
        lower_fmeq(1, 4),    UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
    };
    runLSTest("FMEQ: MAC=0x000F != VI[4]=0x00FF → VI[1]=0", prog, 12,
        {{4, 0x00FFu}},
        {{1, FP_1,FP_1,FP_1,FP_1}, {2, FP_0,FP_0,FP_0,FP_0}},
        {},
        {}, {{1, 0u}}, {});
}

// ──────────────────────────────────────────────────────────────
// WAITQ tests
//
// WAITQ stalls execution until the Q pipeline register (result of
// DIV/SQRT/RSQRT) is fully computed.  Each test issues the operation
// at pair 0, WAITQ at pair 1, then immediately uses Q at pair 2
// (no additional stall NOPs needed — WAITQ absorbs the latency).
// ──────────────────────────────────────────────────────────────

// DIV Q = VF[1].x / VF[2].x = 4/2 = 2.0; WAITQ; ADDq VF[3]=VF[1]+Q → {6,6,6,6}
static void test_waitq_div()
{
    const u32 prog[] = {
        lower_div(1, 0, 2, 0),   UPPER_NOP,              // pair 0: DIV Q=VF[1].x/VF[2].x
        lower_waitq(),           UPPER_NOP,              // pair 1: WAITQ
        LOWER_NOP,               ebit(upper_addq(3, 1)), // pair 2: ADDq VF[3]=VF[1]+Q
        LOWER_NOP,               UPPER_NOP,              // pair 3: delay slot
    };
    runVFTest("WAITQ: DIV(4/2)→Q=2, ADDq VF[3]=VF[1]+Q={6,6,6,6}", prog, 8,
        {{1, FP_4,FP_4,FP_4,FP_4}, {2, FP_2,FP_2,FP_2,FP_2}},
        {{3, FP_6,FP_6,FP_6,FP_6}});
}

// SQRT Q = sqrt(VF[1].w) = sqrt(4.0) = 2.0; WAITQ; MULq VF[3]=VF[2]*Q → {6,6,6,6}
static void test_waitq_sqrt()
{
    const u32 prog[] = {
        lower_sqrt(1, 3),        UPPER_NOP,              // pair 0: SQRT Q=sqrt(VF[1].w)
        lower_waitq(),           UPPER_NOP,              // pair 1: WAITQ
        LOWER_NOP,               ebit(upper_mulq(3, 2)), // pair 2: MULq VF[3]=VF[2]*Q
        LOWER_NOP,               UPPER_NOP,              // pair 3: delay slot
    };
    runVFTest("WAITQ: SQRT(4.0)→Q=2, MULq VF[3]=VF[2]*Q={6,6,6,6}", prog, 8,
        {{1, FP_4,FP_4,FP_4,FP_4}, {2, FP_3,FP_3,FP_3,FP_3}},
        {{3, FP_6,FP_6,FP_6,FP_6}});
}

// RSQRT Q = VF[1].x / sqrt(VF[2].x) = 4/sqrt(4) = 2.0; WAITQ; ADDq VF[3]=VF[1]+Q → {6,6,6,6}
static void test_waitq_rsqrt()
{
    const u32 prog[] = {
        lower_rsqrt(1, 0, 2, 0), UPPER_NOP,              // pair 0: RSQRT Q=VF[1].x/sqrt(VF[2].x)
        lower_waitq(),           UPPER_NOP,              // pair 1: WAITQ
        LOWER_NOP,               ebit(upper_addq(3, 1)), // pair 2: ADDq VF[3]=VF[1]+Q
        LOWER_NOP,               UPPER_NOP,              // pair 3: delay slot
    };
    runVFTest("WAITQ: RSQRT(4/sqrt(4))→Q=2, ADDq VF[3]=VF[1]+Q={6,6,6,6}", prog, 8,
        {{1, FP_4,FP_4,FP_4,FP_4}, {2, FP_4,FP_4,FP_4,FP_4}},
        {{3, FP_6,FP_6,FP_6,FP_6}});
}

// ──────────────────────────────────────────────────────────────
// OPMULA / OPMSUB — cross-product accumulator ops
// ──────────────────────────────────────────────────────────────

// OPMULA.xyz ACC, VF1, VF2
// VF1=(1,2,3,0), VF2=(4,5,6,0)
// ACC.x = VF1.y*VF2.z = 2*6 = 12.0 = 0x41400000
// ACC.y = VF1.z*VF2.x = 3*4 = 12.0 = 0x41400000
// ACC.z = VF1.x*VF2.y = 1*5 =  5.0 = 0x40A00000
// ACC.w = 0 (OPMULA only writes .xyz)
static void test_opmula()
{
    const u32 prog[] = { LOWER_NOP, ebit(upper_opmula(1, 2)), LOWER_NOP, UPPER_NOP };
    runVFTest("OPMULA: ACC.xyz={12,12,5,0}", prog, 4,
        {{1, FP_1, FP_2, FP_3, FP_0},
         {2, FP_4, FP_5, FP_6, FP_0}},
        {{REG_ACC, 0x41400000u, 0x41400000u, FP_5, FP_0}});
}

// OPMSUB.xyz VF3, FS=VF2, FT=VF1
// preset ACC={21,20,12,0}, VF1=(2,3,4,0), VF2=(5,6,7,0)
// VF3.x = ACC.x - VF2.y*VF1.z = 21 - 6*4 = 21 - 24 = -3.0 = FP_N3
// VF3.y = ACC.y - VF2.z*VF1.x = 20 - 7*2 = 20 - 14 =  6.0 = FP_6
// VF3.z = ACC.z - VF2.x*VF1.y = 12 - 5*3 = 12 - 15 = -3.0 = FP_N3
// VF3.w = 0 (OPMSUB only writes .xyz)
static void test_opmsub()
{
    // 21.0f=0x41A80000, 20.0f=0x41A00000, 12.0f=0x41400000
    const u32 prog[] = { LOWER_NOP, ebit(upper_opmsub(3, 2, 1)), LOWER_NOP, UPPER_NOP };
    runVFTest("OPMSUB: VF3.xyz={-3,6,-3,0}", prog, 4,
        {{REG_ACC, 0x41A80000u, 0x41A00000u, 0x41400000u, FP_0},
         {1, FP_2, FP_3, FP_4, FP_0},
         {2, FP_5, FP_6, FP_7, FP_0}},
        {{3, FP_N3, FP_6, FP_N3, FP_0}});
}

// ──────────────────────────────────────────────────────────────
// ILW z and w component offsets
// ──────────────────────────────────────────────────────────────

// ILW.Z  VI[1], 0(VI[2])  — VI[2]=3, load MEM[3].z (u16) into VI[1]
static void test_ilw_offset_z()
{
    const u32 prog[] = { lower_ilw(1, 2, 0, DEST_Z), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("ILW.Z: VI[1]=MEM[VI[2]+0].z low16", prog, 4,
        {{2, 3}}, {}, {{3u, 0u, 0u, 0x0000DEF0u, 0u}},
        {}, {{1, 0xDEF0u}}, {});
}

// ILW.W  VI[1], 0(VI[2])  — VI[2]=4, load MEM[4].w (u16) into VI[1]
static void test_ilw_offset_w()
{
    const u32 prog[] = { lower_ilw(1, 2, 0, DEST_W), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("ILW.W: VI[1]=MEM[VI[2]+0].w low16", prog, 4,
        {{2, 4}}, {}, {{4u, 0u, 0u, 0u, 0x0000ABCDu}},
        {}, {{1, 0xABCDu}}, {});
}

// ──────────────────────────────────────────────────────────────
// MFIR partial dest y and z
// ──────────────────────────────────────────────────────────────

// MFIR VF[2].y, VI[1]=9 — only Y written; X,Z,W preset unchanged
static void test_mfir_partial_dest_y()
{
    const u32 prog[] = { lower_mfir(2, 1, DEST_Y), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("MFIR: VI[1]=9, .y only → VF[2].y=9, xzw preserved", prog, 4,
        {{1, 9}},
        {{2, FP_1, 0, FP_2, FP_3}},   // preset x=1.0, z=2.0, w=3.0
        {},
        {{2, FP_1, 0x00000009u, FP_2, FP_3}}, {}, {});
}

// MFIR VF[2].z, VI[1]=0x1234 — only Z written; X,Y,W preset unchanged
static void test_mfir_partial_dest_z()
{
    const u32 prog[] = { lower_mfir(2, 1, DEST_Z), UPPER_EBIT_NOP, LOWER_NOP, UPPER_NOP };
    runLSTest("MFIR: VI[1]=0x1234, .z only → VF[2].z=0x1234, xyw preserved", prog, 4,
        {{1, 0x1234u}},
        {{2, FP_1, FP_2, 0, FP_3}},   // preset x=1.0, y=2.0, w=3.0
        {},
        {{2, FP_1, FP_2, 0x00001234u, FP_3}}, {}, {});
}

// ──────────────────────────────────────────────────────────────
// DIV edge cases: divide by zero
// ──────────────────────────────────────────────────────────────

// DIV(+1.0 / 0.0): same-sign zero-divide → Q = FP_PS2_MAX (0x7F7FFFFF)
// WAITQ; ADDq VF[3] = VF[1]+Q, VF[1]=(0,0,0,0) → VF[3]={PS2_MAX, PS2_MAX, PS2_MAX, PS2_MAX}
static void test_div_by_zero_pos()
{
    const u32 prog[] = {
        lower_div(1, 0, 2, 0),   UPPER_NOP,              // pair 0: DIV Q=VF[1].x/VF[2].x
        lower_waitq(),           UPPER_NOP,              // pair 1: WAITQ
        LOWER_NOP,               ebit(upper_addq(3, 4)), // pair 2: ADDq VF[3]=VF[4]+Q
        LOWER_NOP,               UPPER_NOP,              // pair 3: delay slot
    };
    runVFTest("WAITQ: DIV(1/0)→Q=PS2_MAX, ADDq VF[3]={PS2_MAX,...}", prog, 8,
        {{1, FP_1, FP_0, FP_0, FP_0},  // VF[1].x = 1.0 (numerator)
         {2, FP_0, FP_0, FP_0, FP_0},  // VF[2].x = 0.0 (denominator)
         {4, FP_0, FP_0, FP_0, FP_0}}, // VF[4] = 0 (addend for ADDq)
        {{3, FP_PS2_MAX, FP_PS2_MAX, FP_PS2_MAX, FP_PS2_MAX}});
}

// DIV(-1.0 / 0.0): opposite-sign zero-divide → Q = FP_PS2_MIN (0xFF7FFFFF)
// WAITQ; ADDq VF[3] = VF[4]+Q, VF[4]=(0,0,0,0) → VF[3]={PS2_MIN,...}
static void test_div_by_zero_neg()
{
    const u32 prog[] = {
        lower_div(1, 0, 2, 0),   UPPER_NOP,
        lower_waitq(),           UPPER_NOP,
        LOWER_NOP,               ebit(upper_addq(3, 4)),
        LOWER_NOP,               UPPER_NOP,
    };
    runVFTest("WAITQ: DIV(-1/0)→Q=PS2_MIN, ADDq VF[3]={PS2_MIN,...}", prog, 8,
        {{1, FP_N1, FP_0, FP_0, FP_0}, // VF[1].x = -1.0 (numerator)
         {2, FP_0,  FP_0, FP_0, FP_0}, // VF[2].x = 0.0 (denominator)
         {4, FP_0,  FP_0, FP_0, FP_0}},
        {{3, FP_PS2_MIN, FP_PS2_MIN, FP_PS2_MIN, FP_PS2_MIN}});
}

// ──────────────────────────────────────────────────────────────
// Multi-start-PC tests
//
// startPC is a byte offset into VU microprogram memory.
// Each instruction pair occupies 8 bytes, so:
//   startPC=0  → pair 0,  startPC=8  → pair 1,
//   startPC=16 → pair 2,  startPC=24 → pair 3, ...
// ──────────────────────────────────────────────────────────────

// Pair 0 is skipped when startPC=8.
// VI[1] must remain 0 (pair 0 not executed); VI[2] must be set by pair 1.
static void test_startpc_skip_first_pair()
{
    // imm5 is 5-bit signed: max positive value = 15
    const u32 prog[] = {
        lower_iaddi(1,0,7), UPPER_NOP,       // pair 0: VI[1]=7  (skipped)
        lower_iaddi(2,0,9), UPPER_EBIT_NOP,  // pair 1: VI[2]=9, end
        LOWER_NOP,          UPPER_NOP,       // delay slot
    };
    runTest("startPC=8: pair 0 skipped, VI[1]=0 VI[2]=9", prog, 6,
            {}, {{1, 0u}, {2, 9u}}, /*startPC=*/8u);
}

// Three-pair chain with no E-bit until pair 2.
// VI[1] increments by 1 per pair executed.
// startPC=0  → pairs 0+1+2 → VI[1]=3
// startPC=8  → pairs   1+2 → VI[1]=2
// startPC=16 → pair      2 → VI[1]=1
static void test_startpc_chain_pc0()
{
    const u32 prog[] = {
        lower_iaddi(1,1,1), UPPER_NOP,       // pair 0: VI[1]++
        lower_iaddi(1,1,1), UPPER_NOP,       // pair 1: VI[1]++
        lower_iaddi(1,1,1), UPPER_EBIT_NOP,  // pair 2: VI[1]++, end
        LOWER_NOP,          UPPER_NOP,       // delay slot
    };
    runTest("startPC=0: chain 3 pairs → VI[1]=3", prog, 8,
            {}, {{1, 3u}}, /*startPC=*/0u);
}

static void test_startpc_chain_pc8()
{
    const u32 prog[] = {
        lower_iaddi(1,1,1), UPPER_NOP,
        lower_iaddi(1,1,1), UPPER_NOP,
        lower_iaddi(1,1,1), UPPER_EBIT_NOP,
        LOWER_NOP,          UPPER_NOP,
    };
    runTest("startPC=8: chain 2 pairs → VI[1]=2", prog, 8,
            {}, {{1, 2u}}, /*startPC=*/8u);
}

static void test_startpc_chain_pc16()
{
    const u32 prog[] = {
        lower_iaddi(1,1,1), UPPER_NOP,
        lower_iaddi(1,1,1), UPPER_NOP,
        lower_iaddi(1,1,1), UPPER_EBIT_NOP,
        LOWER_NOP,          UPPER_NOP,
    };
    runTest("startPC=16: chain 1 pair → VI[1]=1", prog, 8,
            {}, {{1, 1u}}, /*startPC=*/16u);
}

// Two independent E-bit blocks in one microprogram.
// Block A at startPC=0: sets VI[1]=10.
// Block B at startPC=16: sets VI[1]=20.
// Each run recompiles; verifies correct dispatch to each block.
static void test_startpc_two_blocks_A()
{
    // imm5 max = 15; use 10 and 13
    const u32 prog[] = {
        lower_iaddi(1,0,10), UPPER_EBIT_NOP,  // pair 0: VI[1]=10, end
        LOWER_NOP,           UPPER_NOP,        // delay slot (pair 1)
        lower_iaddi(1,0,13), UPPER_EBIT_NOP,  // pair 2: VI[1]=13, end
        LOWER_NOP,           UPPER_NOP,        // delay slot (pair 3)
    };
    runTest("startPC=0: block A → VI[1]=10", prog, 8,
            {}, {{1, 10u}}, /*startPC=*/0u);
}

static void test_startpc_two_blocks_B()
{
    const u32 prog[] = {
        lower_iaddi(1,0,10), UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
        lower_iaddi(1,0,13), UPPER_EBIT_NOP,
        LOWER_NOP,           UPPER_NOP,
    };
    runTest("startPC=16: block B → VI[1]=13", prog, 8,
            {}, {{1, 13u}}, /*startPC=*/16u);
}

// ──────────────────────────────────────────────────────────────
// Entry point
// ──────────────────────────────────────────────────────────────

void RunVuJitTests()
{
    s_pass = s_fail = 0;
    LOGI("=== microVU JIT tests start ===");

    // Track whether THIS function allocated SysMemory so the matching
    // Release at the bottom only fires on the path that owns the memory.
    // Without this, the leaked mappings from VU JIT tests collide with
    // the next CPUThreadInitialize → SysMemory::Allocate → unique_ptr
    // assignment of s_memory_mapping_area, which destructs an area that
    // still has m_num_mappings != 0 and trips
    // `LnxHostSys.cpp:166: No mappings left`. Game launch immediately
    // after app start would crash before the VM even booted.
    bool owns_memory = false;
    if (VU0.Micro == nullptr) {
        if (!SysMemory::Allocate()) {
            LOGE("SysMemory::Allocate() failed — aborting VU JIT tests");
            return;
        }
        owns_memory = true;
    }

    mVU0_TestInit();
    // ── Integer ops ──────────────────────────────────────────────
    test_iaddi_pos(); test_iaddi_neg(); test_iaddi_self(); test_iaddi_zero_imm();
    test_iadd(); test_iadd_src0();
    test_isub(); test_isub_self();
    test_iand(); test_iand_same();
    test_ior(); test_ior_no_overlap();
    test_iaddiu(); test_iaddiu_large(); test_iaddiu_max();
    test_isubiu();

    // ── Multi-pair blocks + VI-delay ─────────────────────────────
    test_multi_instruction_block();
    test_vi_delay_branch();
    test_ibne_loop();
    test_branch_delay_slot();

    // ── VF scalar arithmetic ─────────────────────────────────────
    test_vf_add(); test_vf_sub(); test_vf_mul();
    test_vf_abs();
    test_vf_max(); test_vf_mini();
    test_vf_add_dest_x(); test_vf_mul_dest_yw();
    test_vf_upper_nop_preserves(); test_vf_chained_add_mul();

    // ── Broadcast ADD ─────────────────────────────────────────────
    test_vf_addx(); test_addy(); test_addz(); test_addw();

    // ── Broadcast SUB ─────────────────────────────────────────────
    test_subx(); test_suby(); test_subz(); test_subw();

    // ── Broadcast MUL ─────────────────────────────────────────────
    test_mulx(); test_muly(); test_mulz(); test_vf_mulw();

    // ── Broadcast MAX / MINI ──────────────────────────────────────
    test_maxx(); test_maxy(); test_maxz(); test_maxw(); test_maxi();
    test_minix(); test_miniy(); test_miniz(); test_miniw(); test_minii();

    // ── Hardwired-zero guards ─────────────────────────────────────
    test_vi0_write_guard(); test_vf0_write_guard();

    // ── Accumulator ops ───────────────────────────────────────────
    test_adda(); test_suba(); test_mula();
    test_madd(); test_madda(); test_msuba(); test_msub();

    // ── Broadcast ADDA/SUBA ───────────────────────────────────────
    test_addax(); test_adday(); test_addaz(); test_addaw();
    test_subax(); test_subay(); test_subaz(); test_subaw();

    // ── Broadcast MULA ────────────────────────────────────────────
    test_mulax(); test_mulay(); test_mulaz(); test_mulaw();

    // ── Broadcast MADDA/MSUBA ─────────────────────────────────────
    test_maddax(); test_madday(); test_maddaz(); test_maddaw();
    test_msubax(); test_msubay(); test_msubaz(); test_msubaw();

    // ── Broadcast MADD/MSUB ───────────────────────────────────────
    test_maddx(); test_maddy(); test_maddz(); test_maddw();
    test_msubx(); test_msuby(); test_msubz(); test_msubw();

    // ── OPMULA / OPMSUB (cross product) ──────────────────────────
    test_opmula(); test_opmsub();

    // ── Q pipeline ────────────────────────────────────────────────
    test_addq(); test_subq(); test_mulq();
    test_maddq(); test_msubq();
    test_waitq_div(); test_waitq_sqrt(); test_waitq_rsqrt();
    test_div_by_zero_pos(); test_div_by_zero_neg();

    // ── Q-pipeline accumulator ops ────────────────────────────────
    test_mulaq(); test_addaq(); test_subaq();
    test_maddaq(); test_msubaq();

    // ── I pipeline ────────────────────────────────────────────────
    test_addi(); test_subi(); test_muli();
    test_maddi(); test_msubi();

    // ── I-pipeline accumulator ops ────────────────────────────────
    test_mulai(); test_addai(); test_subai();
    test_maddai(); test_msubai();

    // ── FTOI/ITOF conversions ─────────────────────────────────────
    test_vf_ftoi0(); test_vf_itof0();
    test_vf_ftoi4(); test_vf_itof4();
    test_vf_ftoi12(); test_vf_itof12();
    test_vf_ftoi15(); test_vf_itof15();

    // ── Data transfer (MOVE / MR32 / MTIR / MFIR) ────────────────
    test_vf_move(); test_mr32();
    test_mtir_x(); test_mtir_y(); test_mtir_z(); test_mtir_w();
    test_mfir_positive(); test_mfir_sign_extend(); test_mfir_zero();
    test_mfir_partial_dest_x(); test_mfir_partial_dest_y();
    test_mfir_partial_dest_z(); test_mfir_partial_dest_w();

    // ── Load/Store (LQ/SQ + variants) ────────────────────────────
    test_lq(); test_sq();
    test_lqi(); test_sqi();
    test_lqd(); test_sqd();
    test_lq_offset(); test_sq_offset();
    test_lq_partial_dest(); test_sq_partial_dest(); test_lqi_partial_dest();

    // ── Integer memory (ILW / ISW) ────────────────────────────────
    test_ilw(); test_ilw_offset_y(); test_ilw_offset_z(); test_ilw_offset_w();
    test_isw(); test_isw_partial();
    test_ilwr(); test_iswr();

    // ── VU1-only / NOP ops (VU0 behaviour) ───────────────────────
    test_mfp_nop(); test_xtop_nop(); test_xitop();
    test_waitp_nop(); test_xgkick_nop();

    // ── Branch instructions ───────────────────────────────────────
    test_ibeq_taken(); test_ibeq_not_taken();
    test_ibne_not_taken();
    test_ibgtz(); test_ibgtz_not_taken();
    test_ibltz(); test_ibltz_not_taken();
    test_iblez(); test_iblez_not_taken();
    test_ibgez(); test_ibgez_not_taken();
    test_b(); test_bal(); test_jr(); test_jalr();

    // ── startPC dispatch ─────────────────────────────────────────
    test_startpc_skip_first_pair();
    test_startpc_chain_pc0(); test_startpc_chain_pc8(); test_startpc_chain_pc16();
    test_startpc_two_blocks_A(); test_startpc_two_blocks_B();

    // ── FTZ / DaZ ─────────────────────────────────────────────────
    test_ftz_mul_output();
    test_ftz_add_denorm_input();
    test_ftz_add_denorm_with_normal();

    // ── VF clamp ─────────────────────────────────────────────────
    test_clamp_overflow_add_pos(); test_clamp_overflow_add_neg();
    test_clamp_overflow_nan();
    test_clamp_signoverflow_inf_pos(); test_clamp_signoverflow_inf_neg();
    test_clamp_signoverflow_nan_pos(); test_clamp_signoverflow_nan_neg();

    // ── RNG registers ─────────────────────────────────────────────
    test_rinit_from_vf(); test_rget_no_advance();
    test_rnext_advance(); test_rxor(); test_rnext_partial_dest();

    // ── CLIP register ─────────────────────────────────────────────
    test_fcset_fcget(); test_fcand_match(); test_fcand_nomatch();
    test_fcor_full(); test_fcor_notfull();
    test_clip_all_positive(); test_clip_all_negative();

    // ── STATUS flags ─────────────────────────────────────────────
    test_fsand_zero_result(); test_fsand_sign_result(); test_fsor();
    test_fsset_zs(); test_fsset_zs_ss();
    test_fseq_zero_match(); test_fseq_zs_match(); test_fseq_no_match();

    // ── MAC flags ─────────────────────────────────────────────────
    test_fmand_zero_result(); test_fmand_sign_result(); test_fmor();
    test_fmeq_match(); test_fmeq_no_match();

    LOGI("=== microVU JIT: %d/%d passed ===", s_pass, s_pass + s_fail);
    ReportTestResults("VuJitTests", s_pass, s_pass + s_fail);

    mVU0_TestShutdown();

    // Match the entry-time Allocate. Without this, s_memory_mapping_area
    // stays populated with VU0/EE/IOP mappings, and the next CPUThreadInit
    // (when the user launches a game) hits the "No mappings left" assert.
    if (owns_memory)
        SysMemory::Release();
}
