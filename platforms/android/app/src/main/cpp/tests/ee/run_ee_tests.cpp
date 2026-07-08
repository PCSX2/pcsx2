// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "run_ee_tests.h"
#include "ee_test_api.h"
#include "../test_bridge.h"

#include <android/log.h>
#include <cstring>
#include <initializer_list>

#include "pcsx2/R5900.h"
#include "pcsx2/VU.h"
#include "pcsx2/MemoryTypes.h"

#define TAG "EeJitTests"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ──────────────────────────────────────────────────────────────────────────────
// R5900 / MIPS32 instruction encoders
// ──────────────────────────────────────────────────────────────────────────────

// Halt: BEQ $0,$0,-1  (infinite self-branch, used as end-of-test marker)
static constexpr u32 HALT = 0x1000FFFFu;
// NOP: SLL $0,$0,0
static constexpr u32 NOP  = 0x00000000u;

// R-type (SPECIAL opcode = 0)
static constexpr u32 r_type(u32 funct, u32 rd, u32 rs, u32 rt, u32 sa = 0)
{ return (rs<<21)|(rt<<16)|(rd<<11)|(sa<<6)|funct; }

// I-type
static constexpr u32 i_type(u32 opcode, u32 rt, u32 rs, u32 imm16)
{ return (opcode<<26)|(rs<<21)|(rt<<16)|(imm16&0xFFFFu); }

// ── Arithmetic ────────────────────────────────────────────────────────────────
static constexpr u32 addiu(u32 rt, u32 rs, s16 imm) { return i_type(0x09,rt,rs,(u16)imm); }
static constexpr u32 addu (u32 rd, u32 rs, u32 rt)  { return r_type(0x21,rd,rs,rt); }
static constexpr u32 subu (u32 rd, u32 rs, u32 rt)  { return r_type(0x23,rd,rs,rt); }
static constexpr u32 lui  (u32 rt, u16 imm)         { return i_type(0x0F,rt,0,imm); }

// ── Logical ───────────────────────────────────────────────────────────────────
static constexpr u32 and_ (u32 rd, u32 rs, u32 rt)  { return r_type(0x24,rd,rs,rt); }
static constexpr u32 or_  (u32 rd, u32 rs, u32 rt)  { return r_type(0x25,rd,rs,rt); }
static constexpr u32 xor_ (u32 rd, u32 rs, u32 rt)  { return r_type(0x26,rd,rs,rt); }
static constexpr u32 nor_ (u32 rd, u32 rs, u32 rt)  { return r_type(0x27,rd,rs,rt); }
static constexpr u32 andi (u32 rt, u32 rs, u16 imm) { return i_type(0x0C,rt,rs,imm); }
static constexpr u32 ori  (u32 rt, u32 rs, u16 imm) { return i_type(0x0D,rt,rs,imm); }
static constexpr u32 xori (u32 rt, u32 rs, u16 imm) { return i_type(0x0E,rt,rs,imm); }

// ── Shifts ────────────────────────────────────────────────────────────────────
static constexpr u32 sll  (u32 rd, u32 rt, u32 sa)  { return r_type(0x00,rd,0,rt,sa); }
static constexpr u32 srl  (u32 rd, u32 rt, u32 sa)  { return r_type(0x02,rd,0,rt,sa); }
static constexpr u32 sra  (u32 rd, u32 rt, u32 sa)  { return r_type(0x03,rd,0,rt,sa); }
static constexpr u32 sllv (u32 rd, u32 rt, u32 rs)  { return r_type(0x04,rd,rs,rt); }
static constexpr u32 srlv (u32 rd, u32 rt, u32 rs)  { return r_type(0x06,rd,rs,rt); }
static constexpr u32 srav (u32 rd, u32 rt, u32 rs)  { return r_type(0x07,rd,rs,rt); }

// ── Compare ───────────────────────────────────────────────────────────────────
static constexpr u32 slt  (u32 rd, u32 rs, u32 rt)  { return r_type(0x2A,rd,rs,rt); }
static constexpr u32 sltu (u32 rd, u32 rs, u32 rt)  { return r_type(0x2B,rd,rs,rt); }
static constexpr u32 slti (u32 rt, u32 rs, s16 imm) { return i_type(0x0A,rt,rs,(u16)imm); }
static constexpr u32 sltiu(u32 rt, u32 rs, s16 imm) { return i_type(0x0B,rt,rs,(u16)imm); }

// ── Multiply / Divide (result in HI:LO) ──────────────────────────────────────
static constexpr u32 mult (u32 rs, u32 rt) { return r_type(0x18,0,rs,rt); }
static constexpr u32 multu(u32 rs, u32 rt) { return r_type(0x19,0,rs,rt); }
static constexpr u32 div_ (u32 rs, u32 rt) { return r_type(0x1A,0,rs,rt); }
static constexpr u32 divu (u32 rs, u32 rt) { return r_type(0x1B,0,rs,rt); }
static constexpr u32 mfhi (u32 rd)         { return r_type(0x10,rd,0,0); }
static constexpr u32 mflo (u32 rd)         { return r_type(0x12,rd,0,0); }
static constexpr u32 mthi (u32 rs)         { return r_type(0x11,0,rs,0); }
static constexpr u32 mtlo (u32 rs)         { return r_type(0x13,0,rs,0); }

// ── Load / Store ─────────────────────────────────────────────────────────────
static constexpr u32 lw  (u32 rt, u32 rs, s16 off) { return i_type(0x23,rt,rs,(u16)off); }
static constexpr u32 sw  (u32 rt, u32 rs, s16 off) { return i_type(0x2B,rt,rs,(u16)off); }
static constexpr u32 lb  (u32 rt, u32 rs, s16 off) { return i_type(0x20,rt,rs,(u16)off); }
static constexpr u32 lbu (u32 rt, u32 rs, s16 off) { return i_type(0x24,rt,rs,(u16)off); }
static constexpr u32 lh  (u32 rt, u32 rs, s16 off) { return i_type(0x21,rt,rs,(u16)off); }
static constexpr u32 lhu (u32 rt, u32 rs, s16 off) { return i_type(0x25,rt,rs,(u16)off); }
static constexpr u32 sb  (u32 rt, u32 rs, s16 off) { return i_type(0x28,rt,rs,(u16)off); }
static constexpr u32 sh  (u32 rt, u32 rs, s16 off) { return i_type(0x29,rt,rs,(u16)off); }
static constexpr u32 ld  (u32 rt, u32 rs, s16 off) { return i_type(0x37,rt,rs,(u16)off); }
static constexpr u32 sd  (u32 rt, u32 rs, s16 off) { return i_type(0x3F,rt,rs,(u16)off); }
// Unaligned word load/store (left/right half)
static constexpr u32 lwl (u32 rt, u32 rs, s16 off) { return i_type(0x22,rt,rs,(u16)off); }
static constexpr u32 lwr (u32 rt, u32 rs, s16 off) { return i_type(0x26,rt,rs,(u16)off); }
static constexpr u32 swl (u32 rt, u32 rs, s16 off) { return i_type(0x2A,rt,rs,(u16)off); }
static constexpr u32 swr (u32 rt, u32 rs, s16 off) { return i_type(0x2E,rt,rs,(u16)off); }

// ── Branches ─────────────────────────────────────────────────────────────────
// offset is in words, relative to PC+4
static constexpr u32 beq (u32 rs, u32 rt, s16 off) { return i_type(0x04,rt,rs,(u16)off); }
static constexpr u32 bne (u32 rs, u32 rt, s16 off) { return i_type(0x05,rt,rs,(u16)off); }
static constexpr u32 bgtz(u32 rs, s16 off)         { return i_type(0x07,0,rs,(u16)off); }
static constexpr u32 blez(u32 rs, s16 off)         { return i_type(0x06,0,rs,(u16)off); }
static constexpr u32 bltz(u32 rs, s16 off)         { return i_type(0x01,0,rs,(u16)off); }
static constexpr u32 bgez(u32 rs, s16 off)         { return i_type(0x01,1,rs,(u16)off); }
// Branch-likely: delay slot executes only when branch is taken; skipped otherwise
static constexpr u32 beql (u32 rs, u32 rt, s16 off) { return i_type(0x14,rt,rs,(u16)off); }
static constexpr u32 bnel (u32 rs, u32 rt, s16 off) { return i_type(0x15,rt,rs,(u16)off); }
static constexpr u32 blezl(u32 rs, s16 off)          { return i_type(0x16,0,rs,(u16)off); }
static constexpr u32 bgtzl(u32 rs, s16 off)          { return i_type(0x17,0,rs,(u16)off); }
static constexpr u32 bltzl(u32 rs, s16 off)          { return i_type(0x01,2,rs,(u16)off); } // REGIMM rt=2
static constexpr u32 bgezl(u32 rs, s16 off)          { return i_type(0x01,3,rs,(u16)off); } // REGIMM rt=3

// ── Jumps ─────────────────────────────────────────────────────────────────────
static constexpr u32 jr  (u32 rs)           { return r_type(0x08,0,rs,0); }
static constexpr u32 jalr(u32 rd, u32 rs)   { return r_type(0x09,rd,rs,0); }
// J-type absolute jumps: target=(PC_delay&0xF0000000)|(instr_index<<2)
// EE_TEST_PC=0x00100000 → upper 4 bits = 0, so target = instr_index<<2
static constexpr u32 j  (u32 target){ return (0x02u<<26)|((target>>2)&0x03FFFFFFu); }
static constexpr u32 jal(u32 target){ return (0x03u<<26)|((target>>2)&0x03FFFFFFu); }

// ── R5900 64-bit ──────────────────────────────────────────────────────────────
static constexpr u32 daddu (u32 rd, u32 rs, u32 rt) { return r_type(0x2D,rd,rs,rt); }
static constexpr u32 daddiu(u32 rt, u32 rs, s16 imm){ return i_type(0x19,rt,rs,(u16)imm); }
static constexpr u32 dsll  (u32 rd, u32 rt, u32 sa) { return r_type(0x38,rd,0,rt,sa); }
static constexpr u32 dsrl  (u32 rd, u32 rt, u32 sa) { return r_type(0x3A,rd,0,rt,sa); }
static constexpr u32 dsra  (u32 rd, u32 rt, u32 sa) { return r_type(0x3B,rd,0,rt,sa); }
static constexpr u32 dsll32(u32 rd, u32 rt, u32 sa) { return r_type(0x3C,rd,0,rt,sa); } // sa+32
static constexpr u32 dsrl32(u32 rd, u32 rt, u32 sa) { return r_type(0x3E,rd,0,rt,sa); }
static constexpr u32 dsra32(u32 rd, u32 rt, u32 sa) { return r_type(0x3F,rd,0,rt,sa); } // sa+32
static constexpr u32 dsubu (u32 rd, u32 rs, u32 rt) { return r_type(0x2F,rd,rs,rt); }
static constexpr u32 dsllv (u32 rd, u32 rt, u32 rs) { return r_type(0x14,rd,rs,rt); }
static constexpr u32 dsrlv (u32 rd, u32 rt, u32 rs) { return r_type(0x16,rd,rs,rt); }
static constexpr u32 dsrav (u32 rd, u32 rt, u32 rs) { return r_type(0x17,rd,rs,rt); }

// ── COP1 (FPU) ───────────────────────────────────────────────────────────────
// S-format: opcode=0x11, fmt=S(0x10), (ft<<16)|(fs<<11)|(fd<<6)|func
static constexpr u32 cop1_s(u32 func, u32 fd, u32 fs, u32 ft)
{ return (0x11u<<26)|(0x10u<<21)|(ft<<16)|(fs<<11)|(fd<<6)|func; }
// W-format (integer source): opcode=0x11, fmt=W(0x14)
static constexpr u32 cop1_w(u32 func, u32 fd, u32 fs)
{ return (0x11u<<26)|(0x14u<<21)|(0<<16)|(fs<<11)|(fd<<6)|func; }

static constexpr u32 mtc1(u32 rt, u32 fs) { return (0x11u<<26)|(0x04u<<21)|(rt<<16)|(fs<<11); }
static constexpr u32 mfc1(u32 rt, u32 fs) { return (0x11u<<26)|(0x00u<<21)|(rt<<16)|(fs<<11); }
// BC1F: branch if C=0 (nd=0, tf=0)
static constexpr u32 bc1f(s16 off) { return (0x11u<<26)|(0x08u<<21)|(0u<<16)|((u16)off); }
// BC1T: branch if C=1 (nd=0, tf=1)
static constexpr u32 bc1t(s16 off) { return (0x11u<<26)|(0x08u<<21)|(1u<<16)|((u16)off); }

static constexpr u32 add_s (u32 fd, u32 fs, u32 ft) { return cop1_s(0x00,fd,fs,ft); }
static constexpr u32 sub_s (u32 fd, u32 fs, u32 ft) { return cop1_s(0x01,fd,fs,ft); }
static constexpr u32 mul_s (u32 fd, u32 fs, u32 ft) { return cop1_s(0x02,fd,fs,ft); }
static constexpr u32 div_s (u32 fd, u32 fs, u32 ft) { return cop1_s(0x03,fd,fs,ft); }
static constexpr u32 sqrt_s(u32 fd, u32 ft)         { return cop1_s(0x04,fd,0,ft); }
static constexpr u32 abs_s (u32 fd, u32 fs)         { return cop1_s(0x05,fd,fs,0); }
static constexpr u32 mov_s (u32 fd, u32 fs)         { return cop1_s(0x06,fd,fs,0); }
static constexpr u32 neg_s (u32 fd, u32 fs)         { return cop1_s(0x07,fd,fs,0); }
static constexpr u32 adda_s (u32 fs, u32 ft)         { return cop1_s(0x18,0,fs,ft); }
static constexpr u32 suba_s (u32 fs, u32 ft)         { return cop1_s(0x19,0,fs,ft); }
static constexpr u32 mula_s (u32 fs, u32 ft)         { return cop1_s(0x1A,0,fs,ft); }
static constexpr u32 madd_s (u32 fd, u32 fs, u32 ft) { return cop1_s(0x1C,fd,fs,ft); }
static constexpr u32 msub_s (u32 fd, u32 fs, u32 ft) { return cop1_s(0x1D,fd,fs,ft); }
static constexpr u32 madda_s(u32 fs, u32 ft)         { return cop1_s(0x1E,0,fs,ft); }
static constexpr u32 max_s (u32 fd, u32 fs, u32 ft) { return cop1_s(0x28,fd,fs,ft); }
static constexpr u32 min_s (u32 fd, u32 fs, u32 ft) { return cop1_s(0x29,fd,fs,ft); }
static constexpr u32 c_f   (u32 fs, u32 ft)         { return cop1_s(0x30,0,fs,ft); }
static constexpr u32 c_eq  (u32 fs, u32 ft)         { return cop1_s(0x32,0,fs,ft); }
static constexpr u32 c_lt  (u32 fs, u32 ft)         { return cop1_s(0x34,0,fs,ft); } // C.OLT.S
static constexpr u32 c_le  (u32 fs, u32 ft)         { return cop1_s(0x36,0,fs,ft); } // C.OLE.S
// CVT.W.S: float → integer (fmt=S, func=0x24)
static constexpr u32 cvt_w (u32 fd, u32 fs)         { return cop1_s(0x24,fd,fs,0); }
// CVT.S.W: integer → float (fmt=W, func=0x20)
static constexpr u32 cvt_s (u32 fd, u32 fs)         { return cop1_w(0x20,fd,fs); }

// ── COP2 (VU0 macro mode) ─────────────────────────────────────────────────────
// opcode=0x12, bit25=1 (SPECIAL), bits24:21=dest nibble, ft=bits20:16,
// fs=bits15:11, fd=bits10:6, funct=bits5:0.
static constexpr u32 cop2_s1(u32 dest4, u32 ft, u32 fs, u32 fd, u32 funct) {
    return (0x12u<<26)|((0x10u|(dest4&0xFu))<<21)|(ft<<16)|(fs<<11)|(fd<<6)|funct;
}
// SPECIAL2: table index idx = (fd<<2)|(funct&3); funct high bits = 0b111100
static constexpr u32 cop2_s2(u32 dest4, u32 ft, u32 fs, u32 idx) {
    return cop2_s1(dest4, ft, fs, idx>>2, 0x3Cu|(idx&0x3u));
}
// QMFC2: rs=1, GPR[rt] ← VF[fs] (128-bit)
static constexpr u32 qmfc2(u32 rt, u32 fs) { return (0x12u<<26)|(1u<<21)|(rt<<16)|(fs<<11); }
// QMTC2: rs=5, VF[fs] ← GPR[rt] (128-bit)
static constexpr u32 qmtc2(u32 rt, u32 fs) { return (0x12u<<26)|(5u<<21)|(rt<<16)|(fs<<11); }

// SPECIAL1 arithmetic (dest=0xF = all components, fd = dest VF register)
static constexpr u32 vadd_xyzw  (u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,40); }
static constexpr u32 vsub_xyzw  (u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,44); }
static constexpr u32 vmul_xyzw  (u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,42); }
// SPECIAL1 VADDbc: fd = fs + ft.bc  (funct 0-3)
static constexpr u32 vaddx_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd, 0); }
static constexpr u32 vaddy_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd, 1); }
static constexpr u32 vaddz_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd, 2); }
static constexpr u32 vaddw_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd, 3); }
// SPECIAL1 VSUBbc: fd = fs - ft.bc  (funct 4-7)
static constexpr u32 vsubx_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd, 4); }
static constexpr u32 vsuby_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd, 5); }
static constexpr u32 vsubz_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd, 6); }
static constexpr u32 vsubw_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd, 7); }
// SPECIAL1 VMADDbc: fd = ACC + fs*ft.bc  (funct 8-11)
static constexpr u32 vmaddw_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,11); }
// SPECIAL1 VMSUBbc: fd = ACC - fs*ft.bc  (funct 12-15)
static constexpr u32 vmsubx_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,12); }
static constexpr u32 vmsuby_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,13); }
static constexpr u32 vmsubz_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,14); }
static constexpr u32 vmsubw_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,15); }
// SPECIAL1 VMAXbc: fd = max(fs, ft.bc)  (funct 16-19)
static constexpr u32 vmaxx_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,16); }
static constexpr u32 vmaxy_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,17); }
static constexpr u32 vmaxz_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,18); }
static constexpr u32 vmaxw_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,19); }
// SPECIAL1 VMINIbc: fd = mini(fs, ft.bc)  (funct 20-23)
static constexpr u32 vminix_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,20); }
static constexpr u32 vminiy_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,21); }
static constexpr u32 vminiz_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,22); }
static constexpr u32 vminiw_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,23); }
// SPECIAL1 VMULq: fd = fs*Q  (funct 28)
static constexpr u32 vmulq_xyzw(u32 fd,u32 fs){ return cop2_s1(0xF,0,fs,fd,28); }
// SPECIAL1 full-vector VMAX/VMINI: fd = max/mini(fs, ft)  (funct 43, 47)
static constexpr u32 vmax_xyzw (u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,43); }
static constexpr u32 vmini_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,47); }
// VIADD/VISUB/VIADDI/VIAND/VIOR — integer VI registers; dest nibble = 0 (unused)
static constexpr u32 viadd_vi (u32 id,u32 is_,u32 it) { return cop2_s1(0,it,is_,id,48); }
static constexpr u32 visub_vi (u32 id,u32 is_,u32 it) { return cop2_s1(0,it,is_,id,49); }
// VIADDI: VI[it] = VI[is] + imm5; imm5 goes in the fd field (bits[10:6])
static constexpr u32 viaddi_vi(u32 it,u32 is_,s8 imm5){ return cop2_s1(0,it,is_,(u32)(imm5&0x1F),50); }
static constexpr u32 viand_vi (u32 id,u32 is_,u32 it) { return cop2_s1(0,it,is_,id,52); }
static constexpr u32 vior_vi  (u32 id,u32 is_,u32 it) { return cop2_s1(0,it,is_,id,53); }

// SPECIAL2 unary (dest VF = _Ft_ field, source = _Fs_ field)
static constexpr u32 vabs_xyzw  (u32 ft,u32 fs){ return cop2_s2(0xF,ft,fs,29); }
static constexpr u32 vmove_xyzw (u32 ft,u32 fs){ return cop2_s2(0xF,ft,fs,48); }
// SPECIAL2 fixed-point conversions: VITOF0/4/12/15 (int→float), VFTOI0/4/12/15 (float→int)
static constexpr u32 vitof0_xyzw (u32 ft,u32 fs){ return cop2_s2(0xF,ft,fs,16); } // int * 2^0  → float
static constexpr u32 vitof4_xyzw (u32 ft,u32 fs){ return cop2_s2(0xF,ft,fs,17); } // int * 2^-4 → float
static constexpr u32 vitof12_xyzw(u32 ft,u32 fs){ return cop2_s2(0xF,ft,fs,18); } // int * 2^-12 → float
static constexpr u32 vitof15_xyzw(u32 ft,u32 fs){ return cop2_s2(0xF,ft,fs,19); } // int * 2^-15 → float
static constexpr u32 vftoi0_xyzw (u32 ft,u32 fs){ return cop2_s2(0xF,ft,fs,20); } // float * 2^0  → int
static constexpr u32 vftoi4_xyzw (u32 ft,u32 fs){ return cop2_s2(0xF,ft,fs,21); } // float * 2^4  → int
static constexpr u32 vftoi12_xyzw(u32 ft,u32 fs){ return cop2_s2(0xF,ft,fs,22); } // float * 2^12 → int
static constexpr u32 vftoi15_xyzw(u32 ft,u32 fs){ return cop2_s2(0xF,ft,fs,23); } // float * 2^15 → int
// SPECIAL2 VADDAbc: ACC = fs + ft.bc  (idx 0-3)
static constexpr u32 vaddax_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs, 0); }
static constexpr u32 vadday_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs, 1); }
static constexpr u32 vaddaz_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs, 2); }
static constexpr u32 vaddaw_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs, 3); }
// SPECIAL2 VSUBAbc: ACC = fs - ft.bc  (idx 4-7)
static constexpr u32 vsubax_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs, 4); }
static constexpr u32 vsubay_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs, 5); }
static constexpr u32 vsubaz_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs, 6); }
static constexpr u32 vsubaw_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs, 7); }
// SPECIAL2 multiply-accumulate → ACC (source = _Fs_, broadcast from _Ft_)
static constexpr u32 vmulax_xyzw (u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs,24); } // ACC =  fs*ft.x
static constexpr u32 vmadday_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs, 9); } // ACC += fs*ft.y
static constexpr u32 vmaddaz_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs,10); } // ACC += fs*ft.z
// SPECIAL2 VMSUBAbc: ACC -= fs*ft.bc  (idx 12-15)
static constexpr u32 vmsubax_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs,12); }
static constexpr u32 vmsubay_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs,13); }
static constexpr u32 vmsubaz_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs,14); }
static constexpr u32 vmsubaw_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs,15); }
// SPECIAL2 VMULAq: ACC = fs*Q  (idx 28)
static constexpr u32 vmulaq_xyzw(u32 fs){ return cop2_s2(0xF,0,fs,28); }
// DIVI/VSQRT: dest4 = (_Ftf_<<2)|_Fsf_  (component selectors instead of xyzw mask)
static constexpr u32 vdiv_q(u32 fs,u32 fsf,u32 ft,u32 ftf){ return cop2_s2((ftf<<2)|fsf,ft,fs,56); }
static constexpr u32 vsqrt_q(u32 ft,u32 ftf)               { return cop2_s2( ftf<<2,    ft, 0,57); }
// SPECIAL1 broadcast-multiply: fd.xyzw = VF[fs].xyzw * VF[ft].bc
static constexpr u32 vmulx_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,24); }
static constexpr u32 vmuly_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,25); }
static constexpr u32 vmulz_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,26); }
static constexpr u32 vmulw_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,27); }
// SPECIAL1 broadcast-VMADD: fd = ACC + VF[fs]*VF[ft].bc
static constexpr u32 vmaddx_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd, 8); }
static constexpr u32 vmaddy_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd, 9); }
static constexpr u32 vmaddz_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,10); }
// vmaddw_xyzw(fd,fs,ft) already defined above (funct=11)
// SPECIAL1 full-vector: fd = ACC ± VF[fs]*VF[ft]
static constexpr u32 vmadd_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,41); }
static constexpr u32 vmsub_xyzw(u32 fd,u32 fs,u32 ft){ return cop2_s1(0xF,ft,fs,fd,45); }
// SPECIAL1 cross-product-sub: fd.xyz = ACC.xyz - {fs.y*ft.z, fs.z*ft.x, fs.x*ft.y}
static constexpr u32 vopmsub(u32 fd,u32 fs,u32 ft)   { return cop2_s1(0xE,ft,fs,fd,46); }
// SPECIAL2: additional broadcast-accumulate ops (write to ACC)
static constexpr u32 vmulay_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs,25); }
static constexpr u32 vmulaz_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs,26); }
static constexpr u32 vmulaw_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs,27); }
static constexpr u32 vmaddax_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs, 8); }
static constexpr u32 vmaddaw_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs,11); }
// SPECIAL2 full-vector ACC ops
static constexpr u32 vmadda_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs,41); } // ACC += fs*ft
static constexpr u32 vmula_xyzw (u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs,42); } // ACC  = fs*ft
static constexpr u32 vadda_xyzw (u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs,40); } // ACC  = fs+ft
static constexpr u32 vsuba_xyzw (u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs,44); } // ACC  = fs-ft
static constexpr u32 vmsuba_xyzw(u32 fs,u32 ft){ return cop2_s2(0xF,ft,fs,45); } // ACC -= fs*ft
// SPECIAL2 cross-product-accumulate: ACC.xyz = {fs.y*ft.z, fs.z*ft.x, fs.x*ft.y}
static constexpr u32 vopmula(u32 fs,u32 ft){ return cop2_s2(0xE,ft,fs,46); }
// SPECIAL2 NOP
static constexpr u32 vnop(){ return cop2_s2(0,0,0,47); }

// ── MMI (opcode=0x1C, R-type) ─────────────────────────────────────────────────
// funct selects the sub-table; sa picks the operation within it.
static constexpr u32 mmi_r(u32 rs, u32 rt, u32 rd, u32 sa, u32 funct) {
    return (0x1Cu<<26)|(rs<<21)|(rt<<16)|(rd<<11)|(sa<<6)|funct;
}
// MMI top-level (funct is direct index into tbl_MMI)
static constexpr u32 madd_ee(u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd, 0, 0); } // (HI:LO)=rs*rt+(HI:LO); rd=LO
static constexpr u32 plzcw  (u32 rd,u32 rs)       { return mmi_r(rs, 0,rd, 0, 4); } // rd[0]=clrsb(rs[0])-1, rd[1]=clrsb(rs[1])-1
// Parallel shifts (funct=shift op, sa=shift amount, rt=source, rd=dest)
static constexpr u32 psllh(u32 rd,u32 rt,u32 sa){ return mmi_r(0,rt,rd,sa,52); } // rd.US[i]=rt.US[i]<<(sa&0xF)
static constexpr u32 psrlh(u32 rd,u32 rt,u32 sa){ return mmi_r(0,rt,rd,sa,54); } // rd.US[i]=rt.US[i]>>(sa&0xF) logical
static constexpr u32 psrah(u32 rd,u32 rt,u32 sa){ return mmi_r(0,rt,rd,sa,55); } // rd.US[i]=rt.SS[i]>>(sa&0xF) arithmetic
static constexpr u32 psllw(u32 rd,u32 rt,u32 sa){ return mmi_r(0,rt,rd,sa,60); } // rd.UL[i]=rt.UL[i]<<sa
static constexpr u32 psrlw(u32 rd,u32 rt,u32 sa){ return mmi_r(0,rt,rd,sa,62); } // rd.UL[i]=rt.UL[i]>>sa logical
static constexpr u32 psraw(u32 rd,u32 rt,u32 sa){ return mmi_r(0,rt,rd,sa,63); } // rd.UL[i]=rt.SL[i]>>sa arithmetic
// MMI0 (funct=8): parallel integer ops (sa selects sub-op)
static constexpr u32 paddw (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd, 0, 8); } // rd.UL[i]=rs.UL[i]+rt.UL[i]
static constexpr u32 psubw (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd, 1, 8); } // rd.UL[i]=rs.UL[i]-rt.UL[i]
static constexpr u32 pcgtw (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd, 2, 8); } // rd.UL[i]=(rs.SL[i]>rt.SL[i])?-1:0
static constexpr u32 pmaxw (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd, 3, 8); } // rd.SL[i]=max(rs.SL[i],rt.SL[i])
static constexpr u32 paddh (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd, 4, 8); } // rd.US[i]=rs.US[i]+rt.US[i]
static constexpr u32 psubh (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd, 5, 8); } // rd.US[i]=rs.US[i]-rt.US[i]
static constexpr u32 psubb (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd, 9, 8); } // rd.UC[i]=rs.UC[i]-rt.UC[i]
static constexpr u32 pextlw(u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd,18, 8); } // interleave lower 32-bit halves
static constexpr u32 ppacw (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd,19, 8); } // pack even 32-bit words
static constexpr u32 pextlh(u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd,22, 8); } // interleave lower 16-bit halves
static constexpr u32 ppach (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd,23, 8); } // pack even 16-bit halfwords
// MMI1 (funct=40): parallel compare/min/max/abs
static constexpr u32 pabsw (u32 rd,u32 rt)       { return mmi_r( 0,rt,rd, 1,40); } // rd.UL[i]=|rt.SL[i]|
static constexpr u32 pceqw (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd, 2,40); } // rd.UL[i]=(rs==rt)?-1:0
static constexpr u32 pminw (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd, 3,40); } // rd.SL[i]=min(rs.SL[i],rt.SL[i])
// MMI2 (funct=9): doubleword pack/bitwise ops
static constexpr u32 pcpyld(u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd,14, 9); } // rd.hi=rs.lo, rd.lo=rt.lo
static constexpr u32 pand_  (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd,18, 9); } // rd=rs&rt (128-bit)
// MMI3 (funct=41): doubleword pack/bitwise ops + PCPYH
static constexpr u32 pcpyud(u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd,14,41); } // rd.lo=rs.hi, rd.hi=rt.hi
static constexpr u32 por_  (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd,18,41); } // rd=rs|rt (128-bit)
static constexpr u32 pcpyh (u32 rd,u32 rt)       { return mmi_r( 0,rt,rd,27,41); } // rd.US[0..3]=rt.US[0]; rd.US[4..7]=rt.US[4]
// MMI pipeline-2: HI1:LO1 multiply / divide (opcode=0x1C, funct=0x10-0x1B)
static constexpr u32 mfhi1 (u32 rd)         { return mmi_r( 0, 0,rd, 0,0x10); }
static constexpr u32 mthi1 (u32 rs)         { return mmi_r(rs, 0, 0, 0,0x11); }
static constexpr u32 mflo1 (u32 rd)         { return mmi_r( 0, 0,rd, 0,0x12); }
static constexpr u32 mtlo1 (u32 rs)         { return mmi_r(rs, 0, 0, 0,0x13); }
static constexpr u32 mult1 (u32 rs, u32 rt) { return mmi_r(rs,rt, 0, 0,0x18); }
static constexpr u32 multu1(u32 rs, u32 rt) { return mmi_r(rs,rt, 0, 0,0x19); }
static constexpr u32 div1  (u32 rs, u32 rt) { return mmi_r(rs,rt, 0, 0,0x1A); }
static constexpr u32 divu1 (u32 rs, u32 rt) { return mmi_r(rs,rt, 0, 0,0x1B); }
// MMI0 saturating parallel arithmetic (sa index within funct=8/MMI0)
static constexpr u32 paddsw(u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd,16, 8); }
static constexpr u32 psubsw(u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd,17, 8); }
static constexpr u32 paddsh(u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd,20, 8); }
static constexpr u32 psubsh(u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd,21, 8); }
// MMI0 (funct=8) — additional ops not yet encoded
static constexpr u32 pcgth (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd, 6, 8); } // rd.SS[i]=rs.SS[i]>rt.SS[i]?-1:0
static constexpr u32 pmaxh (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd, 7, 8); } // rd.SS[i]=max(rs,rt) halfwords
static constexpr u32 paddb (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd, 8, 8); } // rd.UC[i]=rs.UC[i]+rt.UC[i] (wrap)
static constexpr u32 pcgtb (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd,10, 8); } // rd.SC[i]=rs.SC[i]>rt.SC[i]?-1:0
static constexpr u32 paddsb(u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd,24, 8); } // sat signed byte add
static constexpr u32 psubsb(u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd,25, 8); } // sat signed byte sub
static constexpr u32 pextlb(u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd,26, 8); } // interleave lower bytes
static constexpr u32 ppacb (u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd,27, 8); } // pack even bytes
// MMI1 (funct=40) — additional ops
static constexpr u32 padsbh(u32 rd,u32 rt)        { return mmi_r( 0,rt,rd, 4,40); } // add/sub bytes→halfwords
static constexpr u32 pabsh (u32 rd,u32 rt)        { return mmi_r( 0,rt,rd, 5,40); } // |halfword|
static constexpr u32 pceqh (u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd, 6,40); } // halfword eq
static constexpr u32 pminh (u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd, 7,40); } // min signed halfword
static constexpr u32 pceqb (u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,10,40); } // byte eq
static constexpr u32 padduw(u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,16,40); } // sat unsigned word add
static constexpr u32 psubuw(u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,17,40); } // sat unsigned word sub, clamp 0
static constexpr u32 pextuw(u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,18,40); } // interleave upper words
static constexpr u32 padduh(u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,20,40); } // sat unsigned halfword add
static constexpr u32 psubuh(u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,21,40); } // sat unsigned halfword sub
static constexpr u32 pextuh(u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,22,40); } // interleave upper halfwords
static constexpr u32 paddub(u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,24,40); } // sat unsigned byte add
static constexpr u32 psubub(u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,25,40); } // sat unsigned byte sub, clamp 0
static constexpr u32 pextub(u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,26,40); } // interleave upper bytes
// MMI2 (funct=9)
static constexpr u32 pmaddw(u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd, 0, 9); } // HI:LO += rs*rt (word); rd=LO
static constexpr u32 psllvw(u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd, 2, 9); } // rd.SL[0]=rt.UL[0]<<(rs.UL[0]&0x1F), sext
static constexpr u32 psrlvw(u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd, 3, 9); } // logical right variable shift
static constexpr u32 pmsubw(u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd, 4, 9); } // HI:LO -= rs*rt (word); rd=LO
static constexpr u32 pmfhi_m(u32 rd)               { return mmi_r( 0, 0,rd, 8, 9); } // rd(128) = HI(128)
static constexpr u32 pmflo_m(u32 rd)               { return mmi_r( 0, 0,rd, 9, 9); } // rd(128) = LO(128)
static constexpr u32 pinth  (u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,10, 9); } // interleave lower-rt with upper-rs halfwords
static constexpr u32 pmultw (u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,12, 9); } // rd=LO=rs*rt (two 32×32→64)
static constexpr u32 pdivw  (u32 rs,u32 rt)        { return mmi_r(rs,rt, 0,13, 9); } // LO=rs/rt, HI=rs%rt (word pairs)
static constexpr u32 pmaddh (u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,16, 9); } // HI:LO += rs*rt (halfword pairs)
static constexpr u32 phmadh (u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,17, 9); } // horizontal madd halfword
static constexpr u32 pxor   (u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,19, 9); } // rd = rs ^ rt (128-bit)
static constexpr u32 pmsubh (u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,20, 9); } // HI:LO -= rs*rt (halfwords)
static constexpr u32 phmsbh (u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,21, 9); } // horizontal msubh halfword
static constexpr u32 pexeh  (u32 rd,u32 rt)        { return mmi_r( 0,rt,rd,26, 9); } // swap US[0]↔US[2], US[4]↔US[6]
static constexpr u32 prevh  (u32 rd,u32 rt)        { return mmi_r( 0,rt,rd,27, 9); } // reverse halfwords within each word
static constexpr u32 pmulth (u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,28, 9); } // 8×(16×16) parallel halfword multiply
static constexpr u32 pdivbw (u32 rs,u32 rt)        { return mmi_r(rs,rt, 0,29, 9); } // LO.SL[0..3]=rs.SL[i]/rt.SS[0], HI=rem
static constexpr u32 pexew  (u32 rd,u32 rt)        { return mmi_r( 0,rt,rd,30, 9); } // swap UL[0]↔UL[2]
static constexpr u32 prot3w (u32 rd,u32 rt)        { return mmi_r( 0,rt,rd,31, 9); } // rotate UL[0..2] left
// MMI3 (funct=41)
static constexpr u32 pmadduw(u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd, 0,41); } // unsigned HI:LO += rs*rt; rd=LO
static constexpr u32 psravw (u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd, 3,41); } // arithmetic right variable shift
static constexpr u32 pmthi_m(u32 rs)               { return mmi_r(rs, 0, 0, 8,41); } // HI(128) = rs(128)
static constexpr u32 pmtlo_m(u32 rs)               { return mmi_r(rs, 0, 0, 9,41); } // LO(128) = rs(128)
static constexpr u32 pinteh (u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,10,41); } // interleave even halfwords
static constexpr u32 pmultuw(u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,12,41); } // unsigned word multiply → rd/LO/HI
static constexpr u32 pdivuw (u32 rs,u32 rt)        { return mmi_r(rs,rt, 0,13,41); } // unsigned LO=rs/rt, HI=rs%rt
static constexpr u32 pnor   (u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd,19,41); } // rd = ~(rs|rt) (128-bit)
static constexpr u32 pexch  (u32 rd,u32 rt)        { return mmi_r( 0,rt,rd,26,41); } // swap US[1]↔US[2], US[5]↔US[6]
static constexpr u32 pexcw  (u32 rd,u32 rt)        { return mmi_r( 0,rt,rd,30,41); } // swap UL[1]↔UL[2]
// EE base: LWU, MADDU, BLTZAL/BGEZAL, BC1TL/BC1FL, CFC1/CTC1
static constexpr u32 lwu    (u32 rt,u32 rs,s16 off){ return i_type(0x27,rt,rs,(u16)off); } // zero-extend 32-bit load
static constexpr u32 maddu_ee(u32 rd,u32 rs,u32 rt){ return mmi_r(rs,rt,rd, 0, 1); } // unsigned HI:LO += rs*rt; rd=LO
static constexpr u32 bltzal (u32 rs,s16 off)       { return i_type(0x01,0x10,rs,(u16)off); } // branch<0 + link r31
static constexpr u32 bgezal (u32 rs,s16 off)       { return i_type(0x01,0x11,rs,(u16)off); } // branch>=0 + link r31
static constexpr u32 bc1tl  (s16 off)              { return (0x11u<<26)|(0x08u<<21)|(3u<<16)|((u16)off); } // FPU branch-likely if C=1
static constexpr u32 bc1fl  (s16 off)              { return (0x11u<<26)|(0x08u<<21)|(2u<<16)|((u16)off); } // FPU branch-likely if C=0
static constexpr u32 cfc1   (u32 rt,u32 fs)        { return (0x11u<<26)|(0x02u<<21)|(rt<<16)|(fs<<11); } // GPR[rt]=FCR[fs]
static constexpr u32 ctc1   (u32 rt,u32 fs)        { return (0x11u<<26)|(0x06u<<21)|(rt<<16)|(fs<<11); } // FCR[fs]=GPR[rt]
// LQ/SQ: 128-bit load/store; address is silently aligned to 16 bytes
static constexpr u32 lq(u32 rt, u32 rs, s16 off) { return i_type(0x1E,rt,rs,(u16)off); }
static constexpr u32 sq(u32 rt, u32 rs, s16 off) { return i_type(0x1F,rt,rs,(u16)off); }

// Common IEEE 754 single-precision bit patterns (F32_ prefix avoids <math.h> FP_* macros)
static constexpr u32 F32_1_0    = 0x3F800000u; // 1.0
static constexpr u32 F32_2_0    = 0x40000000u; // 2.0
static constexpr u32 F32_3_0    = 0x40400000u; // 3.0
static constexpr u32 F32_4_0    = 0x40800000u; // 4.0
static constexpr u32 F32_5_0    = 0x40A00000u; // 5.0
static constexpr u32 F32_6_0    = 0x40C00000u; // 6.0
static constexpr u32 F32_7_0    = 0x40E00000u; // 7.0
static constexpr u32 F32_8_0    = 0x41000000u; // 8.0
static constexpr u32 F32_10_0   = 0x41200000u; // 10.0
static constexpr u32 F32_12_0   = 0x41400000u; // 12.0
static constexpr u32 F32_100_0  = 0x42C80000u; // 100.0
static constexpr u32 F32_HALF   = 0x3F000000u; // 0.5
static constexpr u32 F32_NEG1   = 0xBF800000u; // -1.0
static constexpr u32 F32_NEG2   = 0xC0000000u; // -2.0
static constexpr u32 F32_NEG3   = 0xC0400000u; // -3.0
static constexpr u32 F32_FMAX   = 0x7F7FFFFFu; // +Fmax (posFmax)
static constexpr u32 F32_NFMAX  = 0xFF7FFFFFu; // -Fmax (negFmax)
static constexpr u32 F32_INF    = 0x7F800000u; // +Inf (clamped by fpuDouble)
static constexpr u32 F32_DENORM = 0x00400000u; // denormal (flushed to 0 by fpuDouble)
static constexpr u32 F32_PZERO  = 0x00000000u; // +0
static constexpr u32 F32_9_0    = 0x41100000u; // 9.0
static constexpr u32 F32_3_75   = 0x40700000u; // 3.75
static constexpr u32 F32_TINY   = 0x00800000u; // min normal (2^-126)
// FCR31 C flag
static constexpr u32 FCR31_C    = 0x00800000u;

// ──────────────────────────────────────────────────────────────────────────────
// Test state and helpers
// ──────────────────────────────────────────────────────────────────────────────

static int s_pass, s_fail;

// REG_HI / REG_LO sentinels for GPRPreset/GPRExpect
static constexpr int REG_HI = 32;
static constexpr int REG_LO = 33;

struct GPRPreset { int reg; u64 val; };
struct GPRExpect { int reg; u64 val; };

static bool runGPRTest(const char* name,
                       const u32* words, u32 nwords,
                       std::initializer_list<GPRPreset> presets,
                       std::initializer_list<GPRExpect> expects)
{
    EE_TestWriteProg(words, nwords);

    // Zero all GPRs, HI, LO
    std::memset(&cpuRegs.GPR, 0, sizeof(cpuRegs.GPR));
    cpuRegs.HI.UD[0] = 0; cpuRegs.HI.UD[1] = 0;
    cpuRegs.LO.UD[0] = 0; cpuRegs.LO.UD[1] = 0;

    for (auto& p : presets)
    {
        if      (p.reg == REG_HI) cpuRegs.HI.UD[0] = p.val;
        else if (p.reg == REG_LO) cpuRegs.LO.UD[0] = p.val;
        else                      cpuRegs.GPR.r[p.reg].UD[0] = p.val;
    }

    EE_TestExec();

    bool ok = true;
    for (auto& e : expects)
    {
        u64 got;
        if      (e.reg == REG_HI) got = cpuRegs.HI.UD[0];
        else if (e.reg == REG_LO) got = cpuRegs.LO.UD[0];
        else                      got = cpuRegs.GPR.r[e.reg].UD[0];

        if (got != e.val)
        {
            const char* rname = (e.reg == REG_HI) ? "HI" :
                                (e.reg == REG_LO) ? "LO" : "GPR";
            int ridx          = (e.reg == REG_HI || e.reg == REG_LO) ? 0 : e.reg;
            LOGE("  FAIL %s: %s[%d] exp=0x%016llX got=0x%016llX",
                 name, rname, ridx,
                 (unsigned long long)e.val, (unsigned long long)got);
            ok = false;
        }
    }
    if (ok) { ++s_pass; LOGI("  PASS %s", name); }
    else    { ++s_fail; }
    return ok;
}

// Memory test variant: preset and check words in EE_TEST_DATA region.
struct MemPreset { u32 offset; u32 val; };
struct MemExpect { u32 offset; u32 val; };

static bool runMemTest(const char* name,
                       const u32* words, u32 nwords,
                       std::initializer_list<GPRPreset> gpr_in,
                       std::initializer_list<MemPreset> mem_in,
                       std::initializer_list<GPRExpect> gpr_out,
                       std::initializer_list<MemExpect> mem_out)
{
    EE_TestWriteProg(words, nwords);

    std::memset(&cpuRegs.GPR, 0, sizeof(cpuRegs.GPR));
    cpuRegs.HI.UD[0] = 0; cpuRegs.LO.UD[0] = 0;

    // Zero test data region (256 bytes)
    std::memset(eeMem->Main + EE_TEST_DATA, 0, 256);

    for (auto& p : gpr_in)
        cpuRegs.GPR.r[p.reg].UD[0] = p.val;

    for (auto& m : mem_in)
        *(u32*)(eeMem->Main + EE_TEST_DATA + m.offset) = m.val;

    EE_TestExec();

    bool ok = true;
    for (auto& e : gpr_out)
    {
        u64 got = cpuRegs.GPR.r[e.reg].UD[0];
        if (got != e.val)
        {
            LOGE("  FAIL %s: GPR[%d] exp=0x%016llX got=0x%016llX",
                 name, e.reg,
                 (unsigned long long)e.val, (unsigned long long)got);
            ok = false;
        }
    }
    for (auto& e : mem_out)
    {
        u32 got = *(const u32*)(eeMem->Main + EE_TEST_DATA + e.offset);
        if (got != e.val)
        {
            LOGE("  FAIL %s: MEM[+%02X] exp=0x%08X got=0x%08X",
                 name, e.offset, e.val, got);
            ok = false;
        }
    }
    if (ok) { ++s_pass; LOGI("  PASS %s", name); }
    else    { ++s_fail; }
    return ok;
}

// ──────────────────────────────────────────────────────────────────────────────
// FPU test helpers
// ──────────────────────────────────────────────────────────────────────────────

// reg 0-31 → FPR[n], FPR_ACC → fpuRegs.ACC, FPR_FCR31 → fpuRegs.fprc[31] C-bit
static constexpr int FPR_ACC   = 32;
static constexpr int FPR_FCR31 = 33;

struct FPRPreset { int reg; u32 val; };
struct FPRExpect { int reg; u32 val; };

static bool runFPUTest(const char* name,
                       const u32* words, u32 nwords,
                       std::initializer_list<FPRPreset> fpr_in,
                       std::initializer_list<FPRExpect> fpr_out,
                       std::initializer_list<GPRExpect> gpr_out = {})
{
    EE_TestWriteProg(words, nwords);
    std::memset(&cpuRegs.GPR, 0, sizeof(cpuRegs.GPR));
    cpuRegs.HI.UD[0] = 0; cpuRegs.LO.UD[0] = 0;
    std::memset(&fpuRegs.fpr, 0, sizeof(fpuRegs.fpr));
    fpuRegs.ACC.UL    = 0;
    fpuRegs.fprc[31]  = 0;

    for (auto& p : fpr_in) {
        if      (p.reg == FPR_ACC)   fpuRegs.ACC.UL        = p.val;
        else if (p.reg == FPR_FCR31) fpuRegs.fprc[31]       = p.val;
        else                         fpuRegs.fpr[p.reg].UL  = p.val;
    }

    EE_TestExec();

    bool ok = true;
    for (auto& e : fpr_out) {
        u32 got;
        if      (e.reg == FPR_ACC)   got = fpuRegs.ACC.UL;
        else if (e.reg == FPR_FCR31) got = fpuRegs.fprc[31] & FCR31_C;
        else                         got = fpuRegs.fpr[e.reg].UL;

        if (got != e.val) {
            const char* rname = (e.reg == FPR_ACC) ? "ACC" :
                                (e.reg == FPR_FCR31) ? "FCR31.C" : "FPR";
            int ridx = (e.reg == FPR_ACC || e.reg == FPR_FCR31) ? 0 : e.reg;
            LOGE("  FAIL %s: %s[%d] exp=0x%08X got=0x%08X",
                 name, rname, ridx, e.val, got);
            ok = false;
        }
    }
    for (auto& e : gpr_out) {
        u64 got = cpuRegs.GPR.r[e.reg].UD[0];
        if (got != e.val) {
            LOGE("  FAIL %s: GPR[%d] exp=0x%016llX got=0x%016llX",
                 name, e.reg,
                 (unsigned long long)e.val, (unsigned long long)got);
            ok = false;
        }
    }
    if (ok) { ++s_pass; LOGI("  PASS %s", name); }
    else    { ++s_fail; }
    return ok;
}

// ──────────────────────────────────────────────────────────────────────────────
// VU0 macro-mode (COP2) test helpers
// ──────────────────────────────────────────────────────────────────────────────

// REG_Q = VI[22] — local alias avoids pulling in VU.h
static constexpr int VU0_VI_Q = 22;

// Sentinels for VF4Expect/VF4Preset: values > 31 that don't collide with
// FPR_ACC (32) / FPR_FCR31 (33)
static constexpr int VF_ACC = 34; // VU0 accumulator
static constexpr int VF_Q   = 35; // VU0 Q register (single u32, check x only)

struct VF4Preset { int vf; u32 x, y, z, w; };
struct VF4Expect { int vf; u32 x, y, z, w; };
// 16-bit integer VI registers
struct VIPreset  { u32 vi; u16 val; };
struct VIExpect  { u32 vi; u16 val; };

static bool runVU0Test(const char* name,
                       const u32* words, u32 nwords,
                       std::initializer_list<VF4Preset> vf_in,
                       std::initializer_list<VF4Expect> vf_out,
                       std::initializer_list<GPRPreset> gpr_in  = {},
                       std::initializer_list<GPRExpect> gpr_out = {},
                       std::initializer_list<VIPreset>  vi_in   = {},
                       std::initializer_list<VIExpect>  vi_out  = {})
{
    EE_TestWriteProg(words, nwords);

    // Zero GPR, HI, LO
    std::memset(&cpuRegs.GPR, 0, sizeof(cpuRegs.GPR));
    cpuRegs.HI.UD[0] = 0; cpuRegs.LO.UD[0] = 0;

    // Zero VU0: VF[0..31], VI, ACC, q; then restore VF[0] = {0,0,0,1}
    std::memset(VU0.VF, 0, sizeof(VU0.VF));
    VU0.VF[0].f.x = 0.0f; VU0.VF[0].f.y = 0.0f;
    VU0.VF[0].f.z = 0.0f; VU0.VF[0].f.w = 1.0f;
    std::memset(VU0.VI, 0, sizeof(VU0.VI));
    VU0.ACC.UL[0] = VU0.ACC.UL[1] = VU0.ACC.UL[2] = VU0.ACC.UL[3] = 0;
    VU0.q.UL = 0;

    // Apply GPR presets
    for (auto& p : gpr_in)
        cpuRegs.GPR.r[p.reg].UD[0] = p.val;

    // Apply VF presets
    for (auto& p : vf_in) {
        if (p.vf == VF_ACC) {
            VU0.ACC.UL[0]=p.x; VU0.ACC.UL[1]=p.y;
            VU0.ACC.UL[2]=p.z; VU0.ACC.UL[3]=p.w;
        } else {
            VU0.VF[p.vf].UL[0]=p.x; VU0.VF[p.vf].UL[1]=p.y;
            VU0.VF[p.vf].UL[2]=p.z; VU0.VF[p.vf].UL[3]=p.w;
        }
    }

    // Apply VI presets
    for (auto& p : vi_in)
        VU0.VI[p.vi].UL = p.val;

    EE_TestExec();

    bool ok = true;

    // Check VF / ACC / Q expects
    for (auto& e : vf_out) {
        if (e.vf == VF_Q) {
            u32 got = VU0.VI[VU0_VI_Q].UL;
            if (got != e.x) {
                LOGE("  FAIL %s: Q exp=0x%08X got=0x%08X", name, e.x, got);
                ok = false;
            }
            continue;
        }
        const u32* src = (e.vf == VF_ACC) ? VU0.ACC.UL : VU0.VF[e.vf].UL;
        const char* rn  = (e.vf == VF_ACC) ? "ACC" : "VF";
        int ri = (e.vf == VF_ACC) ? 0 : e.vf;
        const u32 exp[4] = { e.x, e.y, e.z, e.w };
        for (int c = 0; c < 4; ++c) {
            if (src[c] != exp[c]) {
                LOGE("  FAIL %s: %s[%d].%c exp=0x%08X got=0x%08X",
                     name, rn, ri, "xyzw"[c], exp[c], src[c]);
                ok = false;
            }
        }
    }

    // Check GPR expects (UD[0] only)
    for (auto& e : gpr_out) {
        u64 got = cpuRegs.GPR.r[e.reg].UD[0];
        if (got != e.val) {
            LOGE("  FAIL %s: GPR[%d] exp=0x%016llX got=0x%016llX",
                 name, e.reg,
                 (unsigned long long)e.val, (unsigned long long)got);
            ok = false;
        }
    }

    // Check VI expects (16-bit, low bits of UL)
    for (auto& e : vi_out) {
        u16 got = static_cast<u16>(VU0.VI[e.vi].UL & 0xFFFF);
        if (got != e.val) {
            LOGE("  FAIL %s: VI[%u] exp=0x%04X got=0x%04X",
                 name, e.vi, (unsigned)e.val, (unsigned)got);
            ok = false;
        }
    }

    if (ok) { ++s_pass; LOGI("  PASS %s", name); }
    else    { ++s_fail; }
    return ok;
}

// ──────────────────────────────────────────────────────────────────────────────
// Helper: build address of EE_TEST_DATA into register `rd` using LUI+ORI.
// EE_TEST_DATA = 0x00200000 → LUI $rd, 0x0020  (no ORI needed, lower 16 = 0)
// ──────────────────────────────────────────────────────────────────────────────
static constexpr u32 load_data_addr(u32 rd)
{
    return lui(rd, (u16)(EE_TEST_DATA >> 16));
}

// ──────────────────────────────────────────────────────────────────────────────
// Upper-128-bit GPR test helpers
// ──────────────────────────────────────────────────────────────────────────────

// Full 128-bit GPR preset/expect: lo=UD[0] (bytes 0-7), hi=UD[1] (bytes 8-15)
struct GPR128Preset { int reg; u64 lo, hi; };
struct GPR128Expect { int reg; u64 lo, hi; };

static bool runGPR128Test(const char* name,
                          const u32* words, u32 nwords,
                          std::initializer_list<GPR128Preset> presets,
                          std::initializer_list<GPR128Expect> expects,
                          std::initializer_list<MemPreset> mem_in  = {},
                          std::initializer_list<MemExpect> mem_out = {})
{
    EE_TestWriteProg(words, nwords);

    std::memset(&cpuRegs.GPR, 0, sizeof(cpuRegs.GPR));
    cpuRegs.HI.UD[0] = cpuRegs.HI.UD[1] = 0;
    cpuRegs.LO.UD[0] = cpuRegs.LO.UD[1] = 0;

    std::memset(eeMem->Main + EE_TEST_DATA, 0, 256);

    for (auto& p : presets) {
        cpuRegs.GPR.r[p.reg].UD[0] = p.lo;
        cpuRegs.GPR.r[p.reg].UD[1] = p.hi;
    }
    for (auto& m : mem_in)
        *(u32*)(eeMem->Main + EE_TEST_DATA + m.offset) = m.val;

    EE_TestExec();

    bool ok = true;
    for (auto& e : expects) {
        u64 got_lo = cpuRegs.GPR.r[e.reg].UD[0];
        u64 got_hi = cpuRegs.GPR.r[e.reg].UD[1];
        if (got_lo != e.lo) {
            LOGE("  FAIL %s: GPR[%d].UD[0] exp=0x%016llX got=0x%016llX",
                 name, e.reg,
                 (unsigned long long)e.lo, (unsigned long long)got_lo);
            ok = false;
        }
        if (got_hi != e.hi) {
            LOGE("  FAIL %s: GPR[%d].UD[1] exp=0x%016llX got=0x%016llX",
                 name, e.reg,
                 (unsigned long long)e.hi, (unsigned long long)got_hi);
            ok = false;
        }
    }
    for (auto& e : mem_out) {
        u32 got = *(const u32*)(eeMem->Main + EE_TEST_DATA + e.offset);
        if (got != e.val) {
            LOGE("  FAIL %s: MEM[+%02X] exp=0x%08X got=0x%08X",
                 name, e.offset, e.val, got);
            ok = false;
        }
    }
    if (ok) { ++s_pass; LOGI("  PASS %s", name); }
    else    { ++s_fail; }
    return ok;
}

// ──────────────────────────────────────────────────────────────────────────────
// ARITHMETIC TESTS
// ──────────────────────────────────────────────────────────────────────────────

static void test_addiu_pos()
{
    static const u32 prog[] = { addiu(1,0,42), HALT };
    runGPRTest("addiu_pos", prog, 2, {}, {{1, 42u}});
}

static void test_addiu_neg()
{
    static const u32 prog[] = { addiu(1,0,-1), HALT };
    // sign-extends to 64-bit: -1 = 0xFFFFFFFFFFFFFFFF
    runGPRTest("addiu_neg", prog, 2, {}, {{1, 0xFFFFFFFFFFFFFFFFull}});
}

static void test_addiu_zero()
{
    static const u32 prog[] = { addiu(1,0,0), HALT };
    runGPRTest("addiu_zero", prog, 2, {}, {{1, 0u}});
}

static void test_addu_basic()
{
    // $3 = $1 + $2 = 10 + 20 = 30
    static const u32 prog[] = {
        addiu(1,0,10),
        addiu(2,0,20),
        addu(3,1,2),
        HALT,
    };
    runGPRTest("addu_basic", prog, 4, {}, {{3, 30u}});
}

static void test_addu_zero_dest()
{
    // Write to $0 — must stay 0
    static const u32 prog[] = {
        addiu(1,0,99),
        addu(0,1,1),   // $0 = $1+$1 — hardwired zero, must be ignored
        HALT,
    };
    runGPRTest("addu_zero_dest", prog, 3, {}, {{0, 0u}});
}

static void test_subu_basic()
{
    // $3 = $1 - $2 = 50 - 20 = 30
    static const u32 prog[] = {
        addiu(1,0,50),
        addiu(2,0,20),
        subu(3,1,2),
        HALT,
    };
    runGPRTest("subu_basic", prog, 4, {}, {{3, 30u}});
}

static void test_subu_underflow()
{
    // $3 = 1 - 2 = -1, wraps to 0xFFFFFFFF (sign-extended in 64-bit GPR)
    static const u32 prog[] = {
        addiu(1,0,1),
        addiu(2,0,2),
        subu(3,1,2),
        HALT,
    };
    runGPRTest("subu_underflow", prog, 4, {}, {{3, 0xFFFFFFFFFFFFFFFFull}});
}

static void test_lui()
{
    // LUI $1, 0xBEEF → $1 = 0xBEEF0000 (sign-extended to 64 bit)
    static const u32 prog[] = { lui(1,0xBEEF), HALT };
    // 0xBEEF0000 has bit 31 set, so sign-extends: 0xFFFFFFFFBEEF0000
    runGPRTest("lui", prog, 2, {}, {{1, 0xFFFFFFFFBEEF0000ull}});
}

static void test_lui_ori()
{
    // Load 32-bit constant 0x12345678
    static const u32 prog[] = {
        lui(1, 0x1234),
        ori(1, 1, 0x5678),
        HALT,
    };
    // 0x12345678 has bit 31 clear, sign-extends to 0x0000000012345678
    runGPRTest("lui_ori", prog, 3, {}, {{1, 0x12345678ull}});
}

// ──────────────────────────────────────────────────────────────────────────────
// LOGICAL TESTS
// ──────────────────────────────────────────────────────────────────────────────

static void test_and_basic()
{
    static const u32 prog[] = {
        ori(1,0,0x0FF0),
        ori(2,0,0xFF00),
        and_(3,1,2),
        HALT,
    };
    runGPRTest("and_basic", prog, 4, {}, {{3, 0x0F00u}});
}

static void test_or_basic()
{
    static const u32 prog[] = {
        ori(1,0,0x00FF),
        ori(2,0,0xFF00),
        or_(3,1,2),
        HALT,
    };
    runGPRTest("or_basic", prog, 4, {}, {{3, 0xFFFFu}});
}

static void test_xor_basic()
{
    static const u32 prog[] = {
        ori(1,0,0x0F0F),
        ori(2,0,0xFFFF),
        xor_(3,1,2),
        HALT,
    };
    runGPRTest("xor_basic", prog, 4, {}, {{3, 0xF0F0u}});
}

static void test_nor_basic()
{
    // NOR $3, $0, $0 → $3 = ~(0|0) = 0xFFFFFFFFFFFFFFFF
    static const u32 prog[] = { nor_(3,0,0), HALT };
    runGPRTest("nor_basic", prog, 2, {}, {{3, 0xFFFFFFFFFFFFFFFFull}});
}

static void test_andi()
{
    static const u32 prog[] = {
        addiu(1,0,-1),         // $1 = 0xFFFFFFFFFFFFFFFF
        andi(2,1,0xABCD),
        HALT,
    };
    runGPRTest("andi", prog, 3, {}, {{2, 0xABCDu}});
}

static void test_ori()
{
    static const u32 prog[] = {
        addiu(1,0,0x1200),
        ori(2,1,0x0034),
        HALT,
    };
    runGPRTest("ori", prog, 3, {}, {{2, 0x1234u}});
}

static void test_xori()
{
    static const u32 prog[] = {
        addiu(1,0,0x1234),
        xori(2,1,0x00FF),
        HALT,
    };
    runGPRTest("xori", prog, 3, {}, {{2, 0x12CBu}});
}

// ──────────────────────────────────────────────────────────────────────────────
// SHIFT TESTS
// ──────────────────────────────────────────────────────────────────────────────

static void test_sll_basic()
{
    static const u32 prog[] = {
        addiu(1,0,1),
        sll(2,1,4),    // $2 = 1 << 4 = 16
        HALT,
    };
    runGPRTest("sll_basic", prog, 3, {}, {{2, 16u}});
}

static void test_srl_basic()
{
    static const u32 prog[] = {
        addiu(1,0,0x80),
        srl(2,1,3),    // $2 = 0x80 >> 3 = 0x10
        HALT,
    };
    runGPRTest("srl_basic", prog, 3, {}, {{2, 0x10u}});
}

static void test_sra_positive()
{
    // SRA on positive value: same as SRL
    static const u32 prog[] = {
        addiu(1,0,0x40),
        sra(2,1,2),    // 0x40 >> 2 = 0x10
        HALT,
    };
    runGPRTest("sra_positive", prog, 3, {}, {{2, 0x10u}});
}

static void test_sra_negative()
{
    // SRA on negative value should sign-extend: -4 >> 1 = -2 (arithmetic)
    static const u32 prog[] = {
        addiu(1,0,-4),         // $1 = 0xFFFFFFFFFFFFFFFC
        sra(2,1,1),            // SRA operates on lower 32 bits: 0xFFFFFFFC >> 1 = 0xFFFFFFFE, sign-extended
        HALT,
    };
    runGPRTest("sra_negative", prog, 3, {}, {{2, 0xFFFFFFFFFFFFFFFEull}});
}

static void test_sllv()
{
    static const u32 prog[] = {
        addiu(1,0,1),
        addiu(2,0,8),
        sllv(3,1,2),   // $3 = 1 << 8 = 256
        HALT,
    };
    runGPRTest("sllv", prog, 4, {}, {{3, 256u}});
}

static void test_srlv()
{
    static const u32 prog[] = {
        addiu(1,0,0x100),
        addiu(2,0,4),
        srlv(3,1,2),   // 0x100 >> 4 = 0x10
        HALT,
    };
    runGPRTest("srlv", prog, 4, {}, {{3, 0x10u}});
}

static void test_srav()
{
    static const u32 prog[] = {
        addiu(1,0,-8),          // 0xFFFFFFF8
        addiu(2,0,2),
        srav(3,1,2),            // -8 >> 2 = -2 = 0xFFFFFFFFFFFFFFFE
        HALT,
    };
    runGPRTest("srav", prog, 4, {}, {{3, 0xFFFFFFFFFFFFFFFEull}});
}

// ──────────────────────────────────────────────────────────────────────────────
// COMPARE TESTS
// ──────────────────────────────────────────────────────────────────────────────

static void test_slt_true()
{
    static const u32 prog[] = {
        addiu(1,0,3),
        addiu(2,0,5),
        slt(3,1,2),    // $3 = (3 < 5) = 1
        HALT,
    };
    runGPRTest("slt_true", prog, 4, {}, {{3, 1u}});
}

static void test_slt_false()
{
    static const u32 prog[] = {
        addiu(1,0,5),
        addiu(2,0,3),
        slt(3,1,2),    // $3 = (5 < 3) = 0
        HALT,
    };
    runGPRTest("slt_false", prog, 4, {}, {{3, 0u}});
}

static void test_slt_signed()
{
    // -1 signed < 1 (unsigned -1 > 1, but signed: yes)
    static const u32 prog[] = {
        addiu(1,0,-1),
        addiu(2,0,1),
        slt(3,1,2),    // signed: -1 < 1 = 1
        HALT,
    };
    runGPRTest("slt_signed", prog, 4, {}, {{3, 1u}});
}

static void test_sltu_unsigned()
{
    // -1 as unsigned (0xFFFFFFFF) is > 1
    static const u32 prog[] = {
        addiu(1,0,-1),
        addiu(2,0,1),
        sltu(3,1,2),   // unsigned: 0xFFFF... > 1 → 0
        HALT,
    };
    runGPRTest("sltu_unsigned", prog, 4, {}, {{3, 0u}});
}

static void test_slti()
{
    static const u32 prog[] = {
        addiu(1,0,3),
        slti(2,1,10),  // (3 < 10) = 1
        HALT,
    };
    runGPRTest("slti", prog, 3, {}, {{2, 1u}});
}

static void test_sltiu()
{
    static const u32 prog[] = {
        addiu(1,0,3),
        sltiu(2,1,10), // unsigned: (3 < 10) = 1
        HALT,
    };
    runGPRTest("sltiu", prog, 3, {}, {{2, 1u}});
}

// ──────────────────────────────────────────────────────────────────────────────
// HI/LO / MULTIPLY / DIVIDE
// ──────────────────────────────────────────────────────────────────────────────

static void test_mult_basic()
{
    // 7 * 6 = 42; result in LO (and HI = 0 for small values)
    static const u32 prog[] = {
        addiu(1,0,7),
        addiu(2,0,6),
        mult(1,2),
        mflo(3),
        HALT,
    };
    runGPRTest("mult_basic", prog, 5, {}, {{3, 42u}});
}

static void test_mult_negative()
{
    // -3 * 4 = -12; LO = 0xFFFFFFFFFFFFFFFF + 1 - 12 = 0xFFFFFFFFFFFFFFF4
    static const u32 prog[] = {
        addiu(1,0,-3),
        addiu(2,0,4),
        mult(1,2),
        mflo(3),
        HALT,
    };
    runGPRTest("mult_negative", prog, 5, {}, {{3, 0xFFFFFFFFFFFFFFF4ull}});
}

static void test_multu_basic()
{
    static const u32 prog[] = {
        addiu(1,0,100),
        addiu(2,0,200),
        multu(1,2),
        mflo(3),
        HALT,
    };
    runGPRTest("multu_basic", prog, 5, {}, {{3, 20000u}});
}

static void test_div_basic()
{
    // 10 / 3 = quotient 3 (LO), remainder 1 (HI)
    static const u32 prog[] = {
        addiu(1,0,10),
        addiu(2,0,3),
        div_(1,2),
        mflo(3),
        mfhi(4),
        HALT,
    };
    runGPRTest("div_basic", prog, 6, {}, {{3, 3u}, {4, 1u}});
}

static void test_divu_basic()
{
    // 20 / 4 = 5 r 0
    static const u32 prog[] = {
        addiu(1,0,20),
        addiu(2,0,4),
        divu(1,2),
        mflo(3),
        mfhi(4),
        HALT,
    };
    runGPRTest("divu_basic", prog, 6, {}, {{3, 5u}, {4, 0u}});
}

static void test_mthi_mfhi()
{
    static const u32 prog[] = {
        addiu(1,0,0x7777),
        mthi(1),
        mfhi(2),
        HALT,
    };
    runGPRTest("mthi_mfhi", prog, 4, {}, {{2, 0x7777u}});
}

static void test_mtlo_mflo()
{
    static const u32 prog[] = {
        addiu(1,0,0x5678),
        mtlo(1),
        mflo(2),
        HALT,
    };
    runGPRTest("mtlo_mflo", prog, 4, {}, {{2, 0x5678u}});
}

// ──────────────────────────────────────────────────────────────────────────────
// LOAD / STORE TESTS
// ──────────────────────────────────────────────────────────────────────────────

static void test_lw_sw()
{
    // Store 0xDEADBEEF to memory then load it back.
    static const u32 prog[] = {
        load_data_addr(1),       // $1 = EE_TEST_DATA
        addiu(2,0,-1),           // $2 = 0xFFFFFFFFFFFFFFFF (store 0xFFFFFFFF)
        sw(2,1,0),               // mem[EE_TEST_DATA] = 0xFFFFFFFF
        lw(3,1,0),               // $3 = mem[EE_TEST_DATA] (sign-extended)
        HALT,
    };
    runGPRTest("lw_sw", prog, 5, {}, {{3, 0xFFFFFFFFFFFFFFFFull}});
}

static void test_lw_positive()
{
    static const u32 prog[] = {
        load_data_addr(1),
        lw(2,1,0),
        HALT,
    };
    runMemTest("lw_positive", prog, 3,
        {},
        {{0, 0x12345678u}},
        {{2, 0x12345678ull}},
        {});
}

static void test_sw_check()
{
    // SW writes to memory, then verify via MemExpect
    static const u32 prog[] = {
        load_data_addr(1),
        addiu(2,0,0x5555),
        sw(2,1,4),               // mem[EE_TEST_DATA+4] = 0x5555
        HALT,
    };
    runMemTest("sw_check", prog, 4,
        {},
        {},
        {},
        {{4, 0x5555u}});
}

static void test_lb_lbu()
{
    // LB sign-extends; LBU zero-extends
    static const u32 prog[] = {
        load_data_addr(1),
        lb(2,1,0),               // $2 = sign-extend(0xFF) = -1
        lbu(3,1,0),              // $3 = zero-extend(0xFF) = 255
        HALT,
    };
    runMemTest("lb_lbu", prog, 4,
        {},
        {{0, 0x000000FFu}},      // byte at offset 0 = 0xFF
        {{2, 0xFFFFFFFFFFFFFFFFull}, {3, 0xFFu}},
        {});
}

static void test_lh_lhu()
{
    // LH sign-extends; LHU zero-extends
    static const u32 prog[] = {
        load_data_addr(1),
        lh(2,1,0),               // $2 = sign-extend(0x8000) = -32768
        lhu(3,1,0),              // $3 = 0x8000
        HALT,
    };
    runMemTest("lh_lhu", prog, 4,
        {},
        {{0, 0x00008000u}},      // halfword at offset 0 = 0x8000
        {{2, 0xFFFFFFFFFFFF8000ull}, {3, 0x8000u}},
        {});
}

static void test_sb_sh()
{
    // SB/SH write sub-word values; place them in non-overlapping u32 slots
    static const u32 prog[] = {
        load_data_addr(1),
        ori(2,0,0x12),
        sb(2,1,0),               // mem[EE_TEST_DATA+0] byte = 0x12
        ori(3,0,0x3456),
        sh(3,1,4),               // mem[EE_TEST_DATA+4] halfword = 0x3456
        HALT,
    };
    runMemTest("sb_sh", prog, 6,
        {},
        {},
        {},
        {{0, 0x00000012u}, {4, 0x00003456u}});
}

// ──────────────────────────────────────────────────────────────────────────────
// BRANCH TESTS
// Note: branch offset is in words relative to PC+4.
// Pattern: branch taken → set $3=1; branch not taken → $3=99 then halts.
// ──────────────────────────────────────────────────────────────────────────────

static void test_beq_taken()
{
    // $1 == $2 (both 5) → branch taken → skip ADDIU $3,0,99
    static const u32 prog[] = {
        addiu(1,0,5),            // 0
        addiu(2,0,5),            // 1
        addiu(3,0,1),            // 2  ($3 pre-set to 1)
        beq(1,2,2),              // 3  branch to word 6 if $1==$2
        NOP,                     // 4  delay slot
        addiu(3,0,99),           // 5  skipped if taken
        HALT,                    // 6  branch target
    };
    runGPRTest("beq_taken", prog, 7, {}, {{3, 1u}});
}

static void test_beq_not_taken()
{
    static const u32 prog[] = {
        addiu(1,0,5),
        addiu(2,0,7),            // $2 ≠ $1
        addiu(3,0,1),
        beq(1,2,2),              // not taken
        NOP,
        addiu(3,0,99),           // executes since not taken
        HALT,
    };
    runGPRTest("beq_not_taken", prog, 7, {}, {{3, 99u}});
}

static void test_bne_taken()
{
    static const u32 prog[] = {
        addiu(1,0,3),
        addiu(2,0,7),            // $1 ≠ $2
        addiu(3,0,1),
        bne(1,2,2),              // taken
        NOP,
        addiu(3,0,99),           // skipped
        HALT,
    };
    runGPRTest("bne_taken", prog, 7, {}, {{3, 1u}});
}

static void test_bne_not_taken()
{
    static const u32 prog[] = {
        addiu(1,0,5),
        addiu(2,0,5),
        addiu(3,0,1),
        bne(1,2,2),
        NOP,
        addiu(3,0,99),
        HALT,
    };
    runGPRTest("bne_not_taken", prog, 7, {}, {{3, 99u}});
}

static void test_bgtz_taken()
{
    static const u32 prog[] = {
        addiu(1,0,1),            // $1 > 0
        addiu(3,0,1),
        bgtz(1,2),               // taken
        NOP,
        addiu(3,0,99),
        HALT,
    };
    runGPRTest("bgtz_taken", prog, 6, {}, {{3, 1u}});
}

static void test_bgtz_not_taken()
{
    static const u32 prog[] = {
        addiu(1,0,0),            // $1 == 0, not > 0
        addiu(3,0,1),
        bgtz(1,2),
        NOP,
        addiu(3,0,99),
        HALT,
    };
    runGPRTest("bgtz_not_taken", prog, 6, {}, {{3, 99u}});
}

static void test_blez_taken()
{
    static const u32 prog[] = {
        addiu(1,0,0),            // $1 <= 0
        addiu(3,0,1),
        blez(1,2),               // taken
        NOP,
        addiu(3,0,99),
        HALT,
    };
    runGPRTest("blez_taken", prog, 6, {}, {{3, 1u}});
}

static void test_bltz_taken()
{
    static const u32 prog[] = {
        addiu(1,0,-1),           // $1 < 0
        addiu(3,0,1),
        bltz(1,2),               // taken
        NOP,
        addiu(3,0,99),
        HALT,
    };
    runGPRTest("bltz_taken", prog, 6, {}, {{3, 1u}});
}

static void test_bgez_taken()
{
    static const u32 prog[] = {
        addiu(1,0,0),            // $1 >= 0
        addiu(3,0,1),
        bgez(1,2),               // taken
        NOP,
        addiu(3,0,99),
        HALT,
    };
    runGPRTest("bgez_taken", prog, 6, {}, {{3, 1u}});
}

static void test_branch_delay_slot_executes()
{
    // Delay slot MUST execute even on taken branch.
    // $3 set in delay slot → must be 1 after taken BEQ.
    // word 0: addiu $1
    // word 1: addiu $2
    // word 2: beq offset=1 → target = (TEST_PC+8) + 4 + 4 = word 4
    // word 3: delay slot — $3=1
    // word 4: HALT
    static const u32 prog[] = {
        addiu(1,0,5),
        addiu(2,0,5),
        beq(1,2,1),              // taken, branch to word 4
        addiu(3,0,1),            // delay slot: must execute before branch
        HALT,                    // word 4: branch target
    };
    runGPRTest("branch_delay_slot_executes", prog, 5, {}, {{3, 1u}});
}

// ──────────────────────────────────────────────────────────────────────────────
// JUMP TESTS
// ──────────────────────────────────────────────────────────────────────────────

static void test_jr_basic()
{
    // Load address of word 3 into $1, JR $1 → skip word 2 (ADDIU $3,99)
    // word 0: addiu $1, load target
    // word 1: jr $1
    // word 2: NOP (delay slot)
    // word 3: (should be next, the JR target)
    // But we need to compute the target address.
    // TEST_PC + 3*4 = TEST_PC + 12 = 0x0010000C
    // Load with LUI+ORI: lui(1, TEST_PC>>16), ori(1,1, (TEST_PC+12)&0xFFFF)
    static const u32 prog[] = {
        lui(1, (u16)(EE_TEST_PC >> 16)),                           // 0
        ori(1, 1, (u16)((EE_TEST_PC + 5*4) & 0xFFFFu)),           // 1 — target = word 5
        addiu(3,0,1),                                              // 2 (pre-set $3=1)
        jr(1),                                                     // 3
        NOP,                                                       // 4 delay slot
        HALT,                                                      // 5 = jump target
    };
    runGPRTest("jr_basic", prog, 6, {}, {{3, 1u}});
}

// ──────────────────────────────────────────────────────────────────────────────
// R5900 64-BIT TESTS
// ──────────────────────────────────────────────────────────────────────────────

static void test_daddu_basic()
{
    // $1 = 0x0000000180000000 (> 32-bit), $2 = 1 → $3 = 0x0000000180000001
    // Build $1: LUI(1, 0x0001) + DSLL32(1,1,1) — or use preset
    static const u32 prog[] = {
        addiu(3,0,1),            // 3 = 1
        daddu(4,3,3),            // 4 = 1+1 = 2
        daddu(5,4,3),            // 5 = 2+1 = 3
        HALT,
    };
    runGPRTest("daddu_basic", prog, 4, {}, {{4, 2u}, {5, 3u}});
}

static void test_daddiu_basic()
{
    static const u32 prog[] = {
        daddiu(1,0,100),
        daddiu(2,1,-1),          // 100 - 1 = 99
        HALT,
    };
    runGPRTest("daddiu_basic", prog, 3, {}, {{1, 100u}, {2, 99u}});
}

static void test_dsll_basic()
{
    // $1 = 1, DSLL $2, $1, 32 → $2 = 0x0000000100000000
    static const u32 prog[] = {
        addiu(1,0,1),
        dsll32(2,1,0),           // $2 = $1 << 32
        HALT,
    };
    runGPRTest("dsll_basic", prog, 3, {}, {{2, 0x0000000100000000ull}});
}

static void test_dsrl_basic()
{
    // $1 = 0x0000000100000000, DSRL $2,$1,32 → $2 = 1
    static const u32 prog[] = {
        addiu(1,0,1),
        dsll32(2,1,0),           // $2 = 0x100000000
        dsrl32(3,2,0),           // $3 = 0x100000000 >> 32 = 1
        HALT,
    };
    runGPRTest("dsrl_basic", prog, 4, {}, {{3, 1u}});
}

static void test_dsra_negative()
{
    // Set $1 = 0x8000000000000000 (most negative 64-bit), DSRA by 1 → 0xC000000000000000
    static const u32 prog[] = {
        addiu(1,0,1),
        dsll32(2,1,0),           // $2 = 0x100000000
        dsll(2,2,31),            // $2 <<= 31 → 0x8000000000000000
        dsra(3,2,1),             // arithmetic: 0x8000000000000000 >> 1 = 0xC000000000000000
        HALT,
    };
    runGPRTest("dsra_negative", prog, 5, {}, {{3, 0xC000000000000000ull}});
}

// ──────────────────────────────────────────────────────────────────────────────
// ZERO-REGISTER WRITE-GUARD
// ──────────────────────────────────────────────────────────────────────────────

static void test_r0_hardwired_zero()
{
    // Any instruction that writes to $0 must leave it as 0.
    static const u32 prog[] = {
        addiu(1,0,99),
        addu(0,1,1),             // $0 = $1+$1 — should be ignored
        sll(0,1,4),              // $0 = $1<<4 — should be ignored
        HALT,
    };
    runGPRTest("r0_hardwired_zero", prog, 4, {}, {{0, 0u}});
}

// ──────────────────────────────────────────────────────────────────────────────
// COP1 / FPU TESTS
// ──────────────────────────────────────────────────────────────────────────────

// MTC1/MFC1 roundtrip — bit pattern preserved through integer↔FPR move
static void test_mtc1_mfc1()
{
    // lui $1, 0x3F80 → $1 = 0x3F800000 (1.0); mtc1 → FPR[1]; mfc1 → $2
    static const u32 prog[] = {
        lui(1, 0x3F80),
        mtc1(1, 1),   // FPR[1] = $1
        mfc1(2, 1),   // $2 = FPR[1] (sign-extended to 64-bit)
        HALT,
    };
    // MFC1 sign-extends: 0x3F800000 has bit 31 = 0, so 64-bit = 0x000000003F800000
    runGPRTest("mtc1_mfc1", prog, 4, {}, {{2, 0x000000003F800000ull}});
}

// ADD.S normal: 1.0 + 2.0 = 3.0
static void test_add_s_normal()
{
    static const u32 prog[] = { add_s(3,1,2), HALT };
    runFPUTest("add_s_normal", prog, 2, {{1,F32_1_0},{2,F32_2_0}}, {{3,F32_3_0}});
}

// ADD.S overflow → result clamped to +Fmax (not IEEE infinity)
static void test_add_s_overflow()
{
    static const u32 prog[] = { add_s(3,1,2), HALT };
    runFPUTest("add_s_overflow", prog, 2, {{1,F32_FMAX},{2,F32_FMAX}}, {{3,F32_FMAX}});
}

// ADD.S with denormal input: fpuDouble flushes denormal to ±0 before arithmetic
static void test_add_s_denorm_flush()
{
    static const u32 prog[] = { add_s(3,1,2), HALT };
    // denormal flushed to 0 → 1.0 + 0.0 = 1.0
    runFPUTest("add_s_denorm_flush", prog, 2, {{1,F32_1_0},{2,F32_DENORM}}, {{3,F32_1_0}});
}

// ADD.S with +Inf input: fpuDouble clamps Inf to Fmax before arithmetic
static void test_add_s_inf_clamp()
{
    static const u32 prog[] = { add_s(3,1,2), HALT };
    // fpuDouble(INF)=Fmax, Fmax+Fmax → overflow → Fmax
    runFPUTest("add_s_inf_clamp", prog, 2, {{1,F32_FMAX},{2,F32_INF}}, {{3,F32_FMAX}});
}

// SUB.S normal: 5.0 - 3.0 = 2.0
static void test_sub_s_normal()
{
    static const u32 prog[] = { sub_s(3,1,2), HALT };
    runFPUTest("sub_s_normal", prog, 2, {{1,F32_5_0},{2,F32_3_0}}, {{3,F32_2_0}});
}

// SUB.S yielding negative: 1.0 - 3.0 = -2.0
static void test_sub_s_negative()
{
    static const u32 prog[] = { sub_s(3,1,2), HALT };
    runFPUTest("sub_s_negative", prog, 2, {{1,F32_1_0},{2,F32_3_0}}, {{3,F32_NEG2}});
}

// MUL.S normal: 2.0 * 3.0 = 6.0
static void test_mul_s_normal()
{
    static const u32 prog[] = { mul_s(3,1,2), HALT };
    runFPUTest("mul_s_normal", prog, 2, {{1,F32_2_0},{2,F32_3_0}}, {{3,F32_6_0}});
}

// MUL.S neg*neg = positive: -1.0 * -3.0 = 3.0
static void test_mul_s_neg_neg()
{
    static const u32 prog[] = { mul_s(3,1,2), HALT };
    runFPUTest("mul_s_neg_neg", prog, 2, {{1,F32_NEG1},{2,F32_NEG3}}, {{3,F32_3_0}});
}

// MUL.S underflow: two tiny normals → product denormal → flushed to 0
static void test_mul_s_underflow()
{
    static const u32 prog[] = { mul_s(3,1,2), HALT };
    // F32_TINY=2^-126; 2^-126 * 2^-126 = 2^-252 → denormal → 0
    runFPUTest("mul_s_underflow", prog, 2, {{1,F32_TINY},{2,F32_TINY}}, {{3,F32_PZERO}});
}

// DIV.S normal: 6.0 / 2.0 = 3.0
static void test_div_s_normal()
{
    static const u32 prog[] = { div_s(3,1,2), HALT };
    runFPUTest("div_s_normal", prog, 2, {{1,F32_6_0},{2,F32_2_0}}, {{3,F32_3_0}});
}

// DIV.S by zero: non-zero / 0 → +Fmax (PS2 clamps, no IEEE infinity)
static void test_div_s_by_zero()
{
    static const u32 prog[] = { div_s(3,1,2), HALT };
    runFPUTest("div_s_by_zero", prog, 2, {{1,F32_1_0},{2,F32_PZERO}}, {{3,F32_FMAX}});
}

// DIV.S negative result: 1.0 / -2.0 = -0.5
static void test_div_s_negative()
{
    static const u32 prog[] = { div_s(3,1,2), HALT };
    runFPUTest("div_s_negative", prog, 2, {{1,F32_1_0},{2,F32_NEG2}}, {{3,F32_HALF|0x80000000u}});
}

// SQRT.S: sqrt(4.0) = 2.0
static void test_sqrt_s()
{
    static const u32 prog[] = { sqrt_s(2,1), HALT };
    runFPUTest("sqrt_s", prog, 2, {{1,F32_4_0}}, {{2,F32_2_0}});
}

// SQRT.S of zero: sqrt(+0) = +0
static void test_sqrt_s_zero()
{
    static const u32 prog[] = { sqrt_s(2,1), HALT };
    runFPUTest("sqrt_s_zero", prog, 2, {{1,F32_PZERO}}, {{2,F32_PZERO}});
}

// SQRT.S of negative: PS2 sets I flag, returns sqrt(|input|)
static void test_sqrt_s_negative()
{
    static const u32 prog[] = { sqrt_s(2,1), HALT };
    // sqrt(-4.0) → sqrt(4.0) = 2.0 (and sets FPUflagI, not tested here)
    runFPUTest("sqrt_s_negative", prog, 2, {{1,F32_4_0|0x80000000u}}, {{2,F32_2_0}});
}

// ABS.S: |-1.0| = 1.0 (clears sign bit)
static void test_abs_s()
{
    static const u32 prog[] = { abs_s(2,1), HALT };
    runFPUTest("abs_s", prog, 2, {{1,F32_NEG1}}, {{2,F32_1_0}});
}

// NEG.S: -(1.0) = -1.0 (flips sign bit)
static void test_neg_s()
{
    static const u32 prog[] = { neg_s(2,1), HALT };
    runFPUTest("neg_s", prog, 2, {{1,F32_1_0}}, {{2,F32_NEG1}});
}

// MOV.S: bit-copy (does NOT go through fpuDouble — Inf preserved)
static void test_mov_s()
{
    static const u32 prog[] = { mov_s(2,1), HALT };
    runFPUTest("mov_s_inf_preserved", prog, 2, {{1,F32_INF}}, {{2,F32_INF}});
}

// CVT.S.W: integer 3 → float 3.0
static void test_cvt_s_from_int()
{
    static const u32 prog[] = { cvt_s(2,1), HALT };
    runFPUTest("cvt_s_from_int", prog, 2, {{1,3u}}, {{2,F32_3_0}});
}

// CVT.S.W: integer -1 → float -1.0
static void test_cvt_s_neg_int()
{
    static const u32 prog[] = { cvt_s(2,1), HALT };
    runFPUTest("cvt_s_neg_int", prog, 2, {{1,0xFFFFFFFFu}}, {{2,F32_NEG1}});
}

// CVT.W.S: 3.75 → truncates to integer 3
static void test_cvt_w_trunc()
{
    static const u32 prog[] = { cvt_w(2,1), HALT };
    runFPUTest("cvt_w_trunc", prog, 2, {{1,F32_3_75}}, {{2,3u}});
}

// CVT.W.S large positive → clamped to 0x7FFFFFFF
static void test_cvt_w_clamp_pos()
{
    static const u32 prog[] = { cvt_w(2,1), HALT };
    runFPUTest("cvt_w_clamp_pos", prog, 2, {{1,F32_FMAX}}, {{2,0x7FFFFFFFu}});
}

// CVT.W.S large negative → clamped to 0x80000000
static void test_cvt_w_clamp_neg()
{
    static const u32 prog[] = { cvt_w(2,1), HALT };
    runFPUTest("cvt_w_clamp_neg", prog, 2, {{1,F32_NFMAX}}, {{2,0x80000000u}});
}

// C.EQ.S: equal floats → C flag set
static void test_c_eq_set()
{
    static const u32 prog[] = { c_eq(1,2), HALT };
    runFPUTest("c_eq_set", prog, 2, {{1,F32_1_0},{2,F32_1_0}}, {{FPR_FCR31,FCR31_C}});
}

// C.EQ.S: unequal floats → C flag clear
static void test_c_eq_clear()
{
    static const u32 prog[] = { c_eq(1,2), HALT };
    runFPUTest("c_eq_clear", prog, 2, {{1,F32_1_0},{2,F32_2_0}}, {{FPR_FCR31,0u}});
}

// C.LT.S: 1.0 < 2.0 → C flag set
static void test_c_lt_set()
{
    static const u32 prog[] = { c_lt(1,2), HALT };
    runFPUTest("c_lt_set", prog, 2, {{1,F32_1_0},{2,F32_2_0}}, {{FPR_FCR31,FCR31_C}});
}

// C.LT.S: 2.0 < 1.0 → C flag clear
static void test_c_lt_clear()
{
    static const u32 prog[] = { c_lt(1,2), HALT };
    runFPUTest("c_lt_clear", prog, 2, {{1,F32_2_0},{2,F32_1_0}}, {{FPR_FCR31,0u}});
}

// C.LE.S equal case: 1.0 <= 1.0 → C flag set
static void test_c_le_equal()
{
    static const u32 prog[] = { c_le(1,2), HALT };
    runFPUTest("c_le_equal", prog, 2, {{1,F32_1_0},{2,F32_1_0}}, {{FPR_FCR31,FCR31_C}});
}

// C.LE.S less-than case: 1.0 <= 2.0 → C flag set
static void test_c_le_less()
{
    static const u32 prog[] = { c_le(1,2), HALT };
    runFPUTest("c_le_less", prog, 2, {{1,F32_1_0},{2,F32_2_0}}, {{FPR_FCR31,FCR31_C}});
}

// C.F: always clears C regardless of operands
static void test_c_f_clears()
{
    // Pre-set C flag in FCR31, then C.F should clear it
    static const u32 prog[] = { c_f(1,2), HALT };
    runFPUTest("c_f_clears", prog, 2,
        {{1,F32_1_0},{2,F32_1_0},{FPR_FCR31,FCR31_C}},
        {{FPR_FCR31,0u}});
}

// BC1T: C=1 → branch taken
// word 0: c_eq $f1,$f1  (1.0==1.0 → C=1)
// word 1: addiu $3,0,1  (pre-set $3=1 as "taken" sentinel)
// word 2: bc1t offset=2 → target = word 5
// word 3: NOP           (delay slot, always executes)
// word 4: addiu $3,0,99 (not-taken path, skipped)
// word 5: HALT
static void test_bc1t_taken()
{
    static const u32 prog[] = {
        c_eq(1,1),
        addiu(3,0,1),
        bc1t(2),
        NOP,
        addiu(3,0,99),
        HALT,
    };
    runFPUTest("bc1t_taken", prog, 6, {{1,F32_1_0}}, {}, {{3,1u}});
}

// BC1T: C=0 → branch NOT taken, falls through to $3=99
static void test_bc1t_not_taken()
{
    static const u32 prog[] = {
        c_eq(1,2),           // 1.0 != 2.0 → C=0
        addiu(3,0,1),
        bc1t(2),             // not taken
        NOP,                 // delay slot
        addiu(3,0,99),       // falls through here
        HALT,
    };
    runFPUTest("bc1t_not_taken", prog, 6, {{1,F32_1_0},{2,F32_2_0}}, {}, {{3,99u}});
}

// BC1F: C=0 → branch taken
static void test_bc1f_taken()
{
    static const u32 prog[] = {
        c_f(1,2),            // C.F always clears C
        addiu(3,0,1),
        bc1f(2),             // C=0 → taken → word 5
        NOP,
        addiu(3,0,99),
        HALT,
    };
    runFPUTest("bc1f_taken", prog, 6, {{1,F32_1_0},{2,F32_2_0}}, {}, {{3,1u}});
}

// BC1F: C=1 → branch NOT taken, falls through to $3=99
static void test_bc1f_not_taken()
{
    static const u32 prog[] = {
        c_eq(1,1),           // 1.0 == 1.0 → C=1
        addiu(3,0,1),
        bc1f(2),             // C=1 → NOT taken
        NOP,                 // delay slot
        addiu(3,0,99),       // falls through here
        HALT,
    };
    runFPUTest("bc1f_not_taken", prog, 6, {{1,F32_1_0}}, {}, {{3,99u}});
}

// C.LT.S + BC1T taken: 3.0 < 5.0 → C=1 → bc1t branches (vertex inside frustum)
static void test_bc1t_c_lt_taken()
{
    static const u32 prog[] = {
        c_lt(1,2),           // 3.0 < 5.0 → C=1
        addiu(3,0,1),
        bc1t(2),             // C=1 → taken → word 5 (HALT)
        NOP,
        addiu(3,0,99),       // wrong-path sentinel
        HALT,
    };
    runFPUTest("bc1t_c_lt_taken", prog, 6,
        {{1,F32_3_0},{2,F32_5_0}}, {}, {{3,1u}});
}

// C.LT.S + BC1T not taken: 5.0 < 3.0 → C=0 → bc1t falls through (vertex outside frustum)
static void test_bc1t_c_lt_not_taken()
{
    static const u32 prog[] = {
        c_lt(1,2),           // 5.0 < 3.0 → C=0
        addiu(3,0,1),
        bc1t(2),             // C=0 → NOT taken
        NOP,
        addiu(3,0,99),       // falls through here
        HALT,
    };
    runFPUTest("bc1t_c_lt_not_taken", prog, 6,
        {{1,F32_5_0},{2,F32_3_0}}, {}, {{3,99u}});
}

// C.LT.S + BC1F taken: 5.0 < 3.0 → C=0 → bc1f branches (clip path)
static void test_bc1f_c_lt_taken()
{
    static const u32 prog[] = {
        c_lt(1,2),           // 5.0 < 3.0 → C=0
        addiu(3,0,1),
        bc1f(2),             // C=0 → taken → word 5 (HALT)
        NOP,
        addiu(3,0,99),       // wrong-path sentinel
        HALT,
    };
    runFPUTest("bc1f_c_lt_taken", prog, 6,
        {{1,F32_5_0},{2,F32_3_0}}, {}, {{3,1u}});
}

// C.LE.S + BC1T taken: 3.0 <= 3.0 → C=1 → bc1t branches (on boundary = inside)
static void test_bc1t_c_le_boundary()
{
    static const u32 prog[] = {
        c_le(1,2),           // 3.0 <= 3.0 → C=1
        addiu(3,0,1),
        bc1t(2),             // C=1 → taken → word 5 (HALT)
        NOP,
        addiu(3,0,99),       // wrong-path sentinel
        HALT,
    };
    runFPUTest("bc1t_c_le_boundary", prog, 6,
        {{1,F32_3_0},{2,F32_3_0}}, {}, {{3,1u}});
}

// C.LT.S + BC1T taken with negative coords: -3.0 < -1.0 → C=1 → bc1t branches
static void test_bc1t_negative_coords()
{
    static const u32 prog[] = {
        c_lt(1,2),           // -3.0 < -1.0 → C=1
        addiu(3,0,1),
        bc1t(2),             // C=1 → taken → word 5 (HALT)
        NOP,
        addiu(3,0,99),       // wrong-path sentinel
        HALT,
    };
    runFPUTest("bc1t_negative_coords", prog, 6,
        {{1,F32_NEG3},{2,F32_NEG1}}, {}, {{3,1u}});
}

// MADD.S: ACC + FPR[1]*FPR[2] → FPR[3]; ACC=1.0, 1=2.0, 2=3.0 → 7.0
static void test_madd_s()
{
    static const u32 prog[] = { madd_s(3,1,2), HALT };
    runFPUTest("madd_s", prog, 2,
        {{1,F32_2_0},{2,F32_3_0},{FPR_ACC,F32_1_0}},
        {{3,F32_7_0}});
}

// ADDA.S: ACC = FPR[1]+FPR[2]; 2.0+3.0=5.0
static void test_adda_s()
{
    static const u32 prog[] = { adda_s(1,2), HALT };
    runFPUTest("adda_s", prog, 2,
        {{1,F32_2_0},{2,F32_3_0}},
        {{FPR_ACC,F32_5_0}});
}

// MAX.S positive inputs: max(3.0, 5.0) = 5.0
static void test_max_s_pos()
{
    static const u32 prog[] = { max_s(3,1,2), HALT };
    runFPUTest("max_s_pos", prog, 2, {{1,F32_3_0},{2,F32_5_0}}, {{3,F32_5_0}});
}

// MIN.S negative inputs: min(-1.0, -2.0) = -2.0
// PS2 fp_min: both negative → std::max<s32> of bit patterns
// -1.0 (0xBF800000) as s32 < -2.0 (0xC0000000) as s32 → max picks 0xC0000000 = -2.0 ✓
static void test_min_s_neg()
{
    static const u32 prog[] = { min_s(3,1,2), HALT };
    runFPUTest("min_s_neg", prog, 2, {{1,F32_NEG1},{2,F32_NEG2}}, {{3,F32_NEG2}});
}

// ──────────────────────────────────────────────────────────────────────────────
// COP2 / VU0 MACRO-MODE TESTS
// ──────────────────────────────────────────────────────────────────────────────

// Round-trip: QMTC2 loads GPR→VF, QMFC2 reads VF→GPR.
static void test_cop2_qmtc2_qmfc2()
{
    // Pack x=1.0 and y=2.0 into lower 64 bits of GPR[1]; z=w=0 via UD[1]=0.
    static const u32 prog[] = {
        qmtc2(1, 1),   // VF[1] ← GPR[1] (128-bit)
        qmfc2(2, 1),   // GPR[2] ← VF[1]
        HALT,
    };
    // UD[0] in little-endian: bytes 0-3 = x (F32_1_0), bytes 4-7 = y (F32_2_0)
    const u64 xy = ((u64)F32_2_0 << 32) | (u64)F32_1_0;
    runVU0Test("cop2_qmtc2_qmfc2", prog, 3,
        {},   // VF presets: none (loaded via QMTC2)
        {},   // VF expects: none (checked via GPR)
        {{1, xy}},      // GPR[1] = 1.0|2.0 packed
        {{2, xy}});     // GPR[2].UD[0] == GPR[1].UD[0] after round-trip
}

// VF[3] = VF[1] + VF[2], all components.
static void test_cop2_vadd_xyzw()
{
    static const u32 prog[] = { vadd_xyzw(3, 1, 2), HALT };
    runVU0Test("cop2_vadd_xyzw", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_5_0, F32_6_0, F32_7_0, F32_1_0} },
        { {3, F32_6_0, F32_8_0, F32_10_0, F32_5_0} });
        // 1+5=6, 2+6=8, 3+7=10, 4+1=5
}

// VF[3] = VF[1] - VF[2], all components.
static void test_cop2_vsub_xyzw()
{
    static const u32 prog[] = { vsub_xyzw(3, 1, 2), HALT };
    runVU0Test("cop2_vsub_xyzw", prog, 2,
        { {1, F32_6_0,  F32_8_0,  F32_10_0, F32_12_0},
          {2, F32_1_0,  F32_2_0,  F32_3_0,  F32_4_0 } },
        { {3, F32_5_0,  F32_6_0,  F32_7_0,  F32_8_0 } });
}

// VF[3] = VF[1] * VF[2], all components.
static void test_cop2_vmul_xyzw()
{
    static const u32 prog[] = { vmul_xyzw(3, 1, 2), HALT };
    runVU0Test("cop2_vmul_xyzw", prog, 2,
        { {1, F32_2_0, F32_3_0, F32_4_0, F32_5_0},
          {2, F32_2_0, F32_2_0, F32_2_0, F32_2_0} },
        { {3, F32_4_0, F32_6_0, F32_8_0, F32_10_0} });
}

// VABS: absolute value, all components.  Dest = _Ft_ field.
static void test_cop2_vabs()
{
    static const u32 prog[] = { vabs_xyzw(2, 1), HALT };
    runVU0Test("cop2_vabs", prog, 2,
        { {1, F32_NEG1, F32_NEG2, F32_3_0, F32_NFMAX} },
        { {2, F32_1_0,  F32_2_0,  F32_3_0, F32_FMAX } });
}

// VMOVE: copy VF[1]→VF[2].  Dest = _Ft_ field.
static void test_cop2_vmove()
{
    static const u32 prog[] = { vmove_xyzw(2, 1), HALT };
    runVU0Test("cop2_vmove", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0} },
        { {2, F32_1_0, F32_2_0, F32_3_0, F32_4_0} });
}

// VF[0] is hardwired {0,0,0,1}; VMOVE to VF[0] must be a no-op.
static void test_cop2_vf0_immutable()
{
    static const u32 prog[] = { vmove_xyzw(0, 1), HALT };
    runVU0Test("cop2_vf0_immutable", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0} },
        { {0, F32_PZERO, F32_PZERO, F32_PZERO, F32_1_0} });
}

// VFTOI0: float → signed integer, truncate.  Dest = _Ft_ field.
static void test_cop2_vftoi0()
{
    // 2.0→2, -3.0→0xFFFFFFFD, 0.5→0 (truncate), 100.0→100
    static const u32 prog[] = { vftoi0_xyzw(2, 1), HALT };
    runVU0Test("cop2_vftoi0", prog, 2,
        { {1, F32_2_0, F32_NEG3, F32_HALF, F32_100_0} },
        { {2, 2u, 0xFFFFFFFDu, 0u, 100u} });
}

// VITOF0: signed integer → float.  Dest = _Ft_ field.
static void test_cop2_vitof0()
{
    // 2→2.0, 0xFFFFFFFF(-1)→-1.0, 100→100.0, 0→0.0
    static const u32 prog[] = { vitof0_xyzw(2, 1), HALT };
    runVU0Test("cop2_vitof0", prog, 2,
        { {1, 2u, 0xFFFFFFFFu, 100u, 0u} },
        { {2, F32_2_0, F32_NEG1, F32_100_0, F32_PZERO} });
}

// VDIV: Q = VF[1].x / VF[2].x  (component selectors fsf=0, ftf=0)
static void test_cop2_vdiv()
{
    static const u32 prog[] = { vdiv_q(1,0, 2,0), HALT };
    runVU0Test("cop2_vdiv", prog, 2,
        { {1, F32_4_0,0,0,0}, {2, F32_2_0,0,0,0} },
        { {VF_Q, F32_2_0, 0,0,0} });  // Q = 4.0/2.0 = 2.0
}

// VSQRT: Q = sqrt(VF[1].x)
static void test_cop2_vsqrt()
{
    static const u32 prog[] = { vsqrt_q(1, 0), HALT };  // ft=VF[1], ftf=0 (x)
    runVU0Test("cop2_vsqrt", prog, 2,
        { {1, F32_4_0,0,0,0} },
        { {VF_Q, F32_2_0, 0,0,0} });  // Q = sqrt(4.0) = 2.0
}

// VADDx (broadcast): VF[3] = VF[1] + VF[2].x (broadcast x across xyzw)
static void test_cop2_vaddx_broadcast()
{
    static const u32 prog[] = { vaddx_xyzw(3, 1, 2), HALT };
    // VF[1]={3,3,3,3}, VF[2]={2,4,6,8}: broadcast VF[2].x=2.0 to all lanes
    runVU0Test("cop2_vaddx_broadcast", prog, 2,
        { {1, F32_3_0, F32_3_0, F32_3_0, F32_3_0},
          {2, F32_2_0, F32_4_0, F32_6_0, F32_8_0} },
        { {3, F32_5_0, F32_5_0, F32_5_0, F32_5_0} });
}

// VIADD: VI[3] = VI[1] + VI[2]  (16-bit integer add)
static void test_cop2_viadd()
{
    static const u32 prog[] = { viadd_vi(3, 1, 2), HALT };
    runVU0Test("cop2_viadd", prog, 2,
        {}, {},   // no VF presets/expects
        {}, {},   // no GPR presets/expects
        {{1, 5}, {2, 3}},   // VI[1]=5, VI[2]=3
        {{3, 8}});           // VI[3] = 8
}

// Matrix-vector multiply using the canonical PS2 sequence:
//   VMULAx  ACC,  mat0, vec   → ACC  = mat0 * vec.x
//   VMADDAy ACC,  mat1, vec   → ACC += mat1 * vec.y
//   VMADDAz ACC,  mat2, vec   → ACC += mat2 * vec.z
//   VMADDw  dst,  mat3, vec   → dst  = ACC + mat3 * vec.w
// Diagonal matrix: mat0={1,0,0,0}, mat1={0,2,0,0}, mat2={0,0,3,0}, mat3={0,0,0,4}
// Input vec={2,3,4,1}  → expected output={2,6,12,4}
static void test_cop2_matrix_vec_mul()
{
    // VF[1..4] = matrix columns (rows in row-major), VF[5] = input vector
    static const u32 prog[] = {
        vmulax_xyzw (1, 5),   // ACC   = VF[1] * VF[5].x
        vmadday_xyzw(2, 5),   // ACC  += VF[2] * VF[5].y
        vmaddaz_xyzw(3, 5),   // ACC  += VF[3] * VF[5].z
        vmaddw_xyzw (6, 4, 5),// VF[6] = ACC  + VF[4] * VF[5].w
        HALT,
    };
    runVU0Test("cop2_matrix_vec_mul", prog, 5,
        { {1, F32_1_0, F32_PZERO, F32_PZERO, F32_PZERO},  // col0: x-axis
          {2, F32_PZERO, F32_2_0, F32_PZERO, F32_PZERO},  // col1: 2*y-axis
          {3, F32_PZERO, F32_PZERO, F32_3_0, F32_PZERO},  // col2: 3*z-axis
          {4, F32_PZERO, F32_PZERO, F32_PZERO, F32_4_0},  // col3: 4*w-axis
          {5, F32_2_0, F32_3_0, F32_4_0, F32_1_0} },      // input vec {2,3,4,1}
        { {6, F32_2_0, F32_6_0, F32_12_0, F32_4_0} });    // {2,6,12,4}
}

// ──────────────────────────────────────────────────────────────────────────────
// COP2 extended: VMUL broadcasts, VMADD/VMSUB full, VOPMULA/VOPMSUB
// ──────────────────────────────────────────────────────────────────────────────

// VMULx: fd.xyzw = VF[fs].xyzw * VF[ft].x  (broadcasts x across all components)
// VF[1]={1,2,3,4}, VF[2]={3,...} → fd = {1*3, 2*3, 3*3, 4*3} = {3,6,9,12}
static void test_cop2_vmulx_broadcast()
{
    static const u32 prog[] = { vmulx_xyzw(3, 1, 2), HALT };
    runVU0Test("cop2_vmulx_broadcast", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_3_0, F32_PZERO, F32_PZERO, F32_PZERO} },
        { {3, F32_3_0, F32_6_0, F32_9_0, F32_12_0} });
}

// VMADDy: fd = ACC + VF[fs]*VF[ft].y  (broadcast y component of ft)
// ACC={1,2,3,4}, VF[1]={1,1,1,1}, VF[2].y=2.0 → fd = {1+2,2+2,3+2,4+2} = {3,4,5,6}
static void test_cop2_vmaddy_broadcast()
{
    static const u32 prog[] = { vmaddy_xyzw(3, 1, 2), HALT };
    runVU0Test("cop2_vmaddy_broadcast", prog, 2,
        { {VF_ACC, F32_1_0,  F32_2_0, F32_3_0, F32_4_0},
          {1,      F32_1_0,  F32_1_0, F32_1_0, F32_1_0},
          {2,      F32_PZERO,F32_2_0, F32_PZERO, F32_PZERO} },
        { {3, F32_3_0, F32_4_0, F32_5_0, F32_6_0} });
}

// VMADD (all): fd = ACC + VF[fs]*VF[ft], all four components independently
// ACC={2,4,6,8}, VF[1]={1,1,1,1}, VF[2]={2,2,2,2} → fd = {4,6,8,10}
static void test_cop2_vmadd_full()
{
    static const u32 prog[] = { vmadd_xyzw(3, 1, 2), HALT };
    runVU0Test("cop2_vmadd_full", prog, 2,
        { {VF_ACC, F32_2_0, F32_4_0, F32_6_0, F32_8_0},
          {1,      F32_1_0, F32_1_0, F32_1_0, F32_1_0},
          {2,      F32_2_0, F32_2_0, F32_2_0, F32_2_0} },
        { {3, F32_4_0, F32_6_0, F32_8_0, F32_10_0} });
}

// VMSUB (all): fd = ACC - VF[fs]*VF[ft]
// ACC={10,8,6,4}, VF[1]={1,1,1,1}, VF[2]={2,2,2,2} → fd = {8,6,4,2}
static void test_cop2_vmsub_full()
{
    static const u32 prog[] = { vmsub_xyzw(3, 1, 2), HALT };
    runVU0Test("cop2_vmsub_full", prog, 2,
        { {VF_ACC, F32_10_0, F32_8_0, F32_6_0, F32_4_0},
          {1,      F32_1_0,  F32_1_0, F32_1_0, F32_1_0},
          {2,      F32_2_0,  F32_2_0, F32_2_0, F32_2_0} },
        { {3, F32_8_0, F32_6_0, F32_4_0, F32_2_0} });
}

// VMADDA (all): ACC += VF[fs]*VF[ft]
// ACC={1,2,3,4}, VF[1]={1,2,3,4}, VF[2]={1,1,1,1} → ACC = {2,4,6,8}
static void test_cop2_vmadda_full()
{
    static const u32 prog[] = { vmadda_xyzw(1, 2), HALT };
    runVU0Test("cop2_vmadda_full", prog, 2,
        { {VF_ACC, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {1,      F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2,      F32_1_0, F32_1_0, F32_1_0, F32_1_0} },
        { {VF_ACC, F32_2_0, F32_4_0, F32_6_0, F32_8_0} });
}

// VMULA (all): ACC = VF[fs]*VF[ft]  (overwrites ACC, no prior value used)
// VF[1]={1,2,3,4}, VF[2]={2,2,2,2} → ACC = {2,4,6,8}
static void test_cop2_vmula_full()
{
    static const u32 prog[] = { vmula_xyzw(1, 2), HALT };
    runVU0Test("cop2_vmula_full", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_2_0, F32_2_0, F32_2_0, F32_2_0} },
        { {VF_ACC, F32_2_0, F32_4_0, F32_6_0, F32_8_0} });
}

// VOPMULA: ACC.xyz = { VF[Fs].y*VF[Ft].z,  VF[Fs].z*VF[Ft].x,  VF[Fs].x*VF[Ft].y }
// VF[1]={1,2,3,0}, VF[2]={4,5,6,0}:
//   ACC.x = 2*6 = 12,  ACC.y = 3*4 = 12,  ACC.z = 1*5 = 5
static void test_cop2_vopmula()
{
    static const u32 prog[] = { vopmula(1, 2), HALT };
    runVU0Test("cop2_vopmula", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_PZERO},
          {2, F32_4_0, F32_5_0, F32_6_0, F32_PZERO} },
        { {VF_ACC, F32_12_0, F32_12_0, F32_5_0, F32_PZERO} });
}

// VOPMSUB: fd.xyz = ACC.xyz - { VF[Fs].y*VF[Ft].z, VF[Fs].z*VF[Ft].x, VF[Fs].x*VF[Ft].y }
// ACC={10,10,7,0}, VF[1]={1,2,3,0}, VF[2]={4,5,6,0}:
//   fd.x = 10 - 2*6 = -2,  fd.y = 10 - 3*4 = -2,  fd.z = 7 - 1*5 = 2
static void test_cop2_vopmsub()
{
    static const u32 prog[] = { vopmsub(3, 1, 2), HALT };
    runVU0Test("cop2_vopmsub", prog, 2,
        { {VF_ACC, F32_10_0, F32_10_0, F32_7_0, F32_PZERO},
          {1,      F32_1_0,  F32_2_0,  F32_3_0, F32_PZERO},
          {2,      F32_4_0,  F32_5_0,  F32_6_0, F32_PZERO} },
        { {3, F32_NEG2, F32_NEG2, F32_2_0, F32_PZERO} });
}

// VOPMULA + VOPMSUB — canonical PS2 cross-product pair.
// With VF[1]={1,2,3} and VF[2]={4,5,6}:
//   VOPMULA(VF[1],VF[2]):        ACC.xyz = {2*6=12, 3*4=12, 1*5=5}
//   VOPMSUB(VF[3], VF[2], VF[1]):fd.xyz  = {12-5*3, 12-6*1, 5-4*2} = {-3, 6, -3}
// Expected == VF[1]×VF[2] = {1,2,3}×{4,5,6} = (-3, 6, -3).
static void test_cop2_vopmula_vopmsub_chain()
{
    static const u32 prog[] = {
        vopmula(1, 2),       // ACC.xyz = cross-product first half
        vopmsub(3, 2, 1),    // VF[3].xyz = complete cross product (fs/ft swapped)
        HALT,
    };
    runVU0Test("cop2_vopmula_vopmsub_chain", prog, 3,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_PZERO},
          {2, F32_4_0, F32_5_0, F32_6_0, F32_PZERO} },
        { {3, F32_NEG3, F32_6_0, F32_NEG3, F32_PZERO} });
}

// VMULA → VMADDA chain: verifies ACC write then accumulate uses correct intermediate value.
// VMULA (VF[1]*VF[2]): ACC = {1*2, 1*2, 1*2, 1*2} = {2,2,2,2}
// VMADDA(VF[1]*VF[3]): ACC += {1*4,...} → ACC = {6,6,6,6}
static void test_cop2_vmula_vmadda_chain()
{
    static const u32 prog[] = {
        vmula_xyzw(1, 2),    // ACC  = VF[1]*VF[2]
        vmadda_xyzw(1, 3),   // ACC += VF[1]*VF[3]
        HALT,
    };
    runVU0Test("cop2_vmula_vmadda_chain", prog, 3,
        { {1, F32_1_0, F32_1_0, F32_1_0, F32_1_0},
          {2, F32_2_0, F32_2_0, F32_2_0, F32_2_0},
          {3, F32_4_0, F32_4_0, F32_4_0, F32_4_0} },
        { {VF_ACC, F32_6_0, F32_6_0, F32_6_0, F32_6_0} });
}

// ──────────────────────────────────────────────────────────────────────────────
// UPPER 128-BIT GPR TESTS (MMI parallel ops + LQ/SQ)
// ──────────────────────────────────────────────────────────────────────────────

// PADDW: 4×32-bit parallel add; verifies UD[1] is written by the upper pair.
// GPR[1]={1,2,3,4}, GPR[2]={10,20,30,40} → GPR[3]={11,22,33,44}
// UD[0]=(22<<32)|11=0x000000160000000B, UD[1]=(44<<32)|33=0x0000002C00000021
static void test_paddw_upper_half()
{
    static const u32 prog[] = { paddw(3, 1, 2), HALT };
    runGPR128Test("paddw_upper_half", prog, 2,
        { {1, (2ull<<32)|1,   (4ull<<32)|3  },
          {2, (20ull<<32)|10, (40ull<<32)|30} },
        { {3, (22ull<<32)|11, (44ull<<32)|33} });
}

// PSUBW: 4×32-bit parallel subtract; same upper-half coverage.
// GPR[1]={100,200,300,400}, GPR[2]={10,20,30,40} → GPR[3]={90,180,270,360}
// UD[0]=(180<<32)|90=0x000000B40000005A, UD[1]=(360<<32)|270=0x000001680000010E
static void test_psubw_upper_half()
{
    static const u32 prog[] = { psubw(3, 1, 2), HALT };
    runGPR128Test("psubw_upper_half", prog, 2,
        { {1, (200ull<<32)|100, (400ull<<32)|300},
          {2, (20ull<<32)|10,   (40ull<<32)|30  } },
        { {3, (180ull<<32)|90,  (360ull<<32)|270} });
}

// PCPYLD: rd.UD[1]=rs.UD[0], rd.UD[0]=rt.UD[0] — packs lower halves of rs/rt.
static void test_pcpyld_pack_halves()
{
    static const u32 prog[] = { pcpyld(3, 1, 2), HALT };
    runGPR128Test("pcpyld_pack_halves", prog, 2,
        { {1, 0xAAAAAAAAAAAAAAAAull, 0xDEADDEADDEADDEADull},
          {2, 0xCCCCCCCCCCCCCCCCull, 0xBEEFBEEFBEEFBEEFull} },
        // rd.UD[0]=rt.UD[0]=0xCCCC..., rd.UD[1]=rs.UD[0]=0xAAAA...
        { {3, 0xCCCCCCCCCCCCCCCCull, 0xAAAAAAAAAAAAAAAAull} });
}

// PCPYUD: rd.UD[0]=rs.UD[1], rd.UD[1]=rt.UD[1] — extracts upper halves of rs/rt.
static void test_pcpyud_extract_upper()
{
    static const u32 prog[] = { pcpyud(3, 1, 2), HALT };
    runGPR128Test("pcpyud_extract_upper", prog, 2,
        { {1, 0x1111111111111111ull, 0x2222222222222222ull},
          {2, 0x3333333333333333ull, 0x4444444444444444ull} },
        // rd.UD[0]=rs.UD[1]=0x2222..., rd.UD[1]=rt.UD[1]=0x4444...
        { {3, 0x2222222222222222ull, 0x4444444444444444ull} });
}

// POR: rd = rs | rt (full 128-bit bitwise OR).
// 0x0F0F... | 0xF0F0... = 0xFFFF... for both halves.
static void test_por_full128()
{
    static const u32 prog[] = { por_(3, 1, 2), HALT };
    runGPR128Test("por_full128", prog, 2,
        { {1, 0x0F0F0F0F0F0F0F0Full, 0x0F0F0F0F0F0F0F0Full},
          {2, 0xF0F0F0F0F0F0F0F0ull, 0xF0F0F0F0F0F0F0F0ull} },
        { {3, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull} });
}

// PAND: rd = rs & rt (full 128-bit bitwise AND).
// 0xFF00FF00FF00FF00 & 0xF0F0F0F0F0F0F0F0 = 0xF000F000F000F000
static void test_pand_full128()
{
    static const u32 prog[] = { pand_(3, 1, 2), HALT };
    runGPR128Test("pand_full128", prog, 2,
        { {1, 0xFF00FF00FF00FF00ull, 0xFF00FF00FF00FF00ull},
          {2, 0xF0F0F0F0F0F0F0F0ull, 0xF0F0F0F0F0F0F0F0ull} },
        { {3, 0xF000F000F000F000ull, 0xF000F000F000F000ull} });
}

// LQ: 128-bit load fills both UD[0] and UD[1].
// mem = {0x11223344, 0xAABBCCDD, 0x55667788, 0x99AABBCC}
// → UD[0]=(0xAABBCCDD<<32)|0x11223344, UD[1]=(0x99AABBCC<<32)|0x55667788
static void test_lq_loads_upper_half()
{
    static const u32 prog[] = { load_data_addr(1), lq(2, 1, 0), HALT };
    runGPR128Test("lq_loads_upper_half", prog, 3,
        {},
        { {2, (0xAABBCCDDull<<32)|0x11223344ull,
              (0x99AABBCCull<<32)|0x55667788ull} },
        { {0, 0x11223344u}, {4, 0xAABBCCDDu},
          {8, 0x55667788u}, {12, 0x99AABBCCu} },
        {});
}

// SQ: 128-bit store writes both UD[0] and UD[1] to memory.
// GPR[2].UD[0]=0xDEADBEEF12345678 → UL[0]=0x12345678, UL[1]=0xDEADBEEF
// GPR[2].UD[1]=0xCAFEBABE87654321 → UL[2]=0x87654321, UL[3]=0xCAFEBABE
static void test_sq_stores_upper_half()
{
    static const u32 prog[] = { load_data_addr(1), sq(2, 1, 0), HALT };
    runGPR128Test("sq_stores_upper_half", prog, 3,
        { {2, 0xDEADBEEF12345678ull, 0xCAFEBABE87654321ull} },
        {},
        {},
        { {0,  0x12345678u}, {4,  0xDEADBEEFu},
          {8,  0x87654321u}, {12, 0xCAFEBABEu} });
}

// ──────────────────────────────────────────────────────────────────────────────
// MMI / EE-specific tests
// ──────────────────────────────────────────────────────────────────────────────

// MADD: (HI:LO) = rs*rt + (HI:LO); rd = LO. LO=0,HI=0; 5*6=30 → rd=30
static void test_madd_ee()
{
    static const u32 prog[] = { madd_ee(3,1,2), HALT };
    runGPR128Test("madd_ee", prog, 2,
        {{1, 5ull, 0ull}, {2, 6ull, 0ull}},
        {{3, 30ull, 0ull}});
}

// PLZCW positive: rs.UL[0]=1 → 30 leading-sign bits; rs.UL[1]=0x00010000 → 14
// CountLeadingSignBits(n)-1: for n=1, clz(1)=31 → 30; for 0x10000, clz=15 → 14
static void test_plzcw_positive()
{
    // rs.UD[0] = (UL[1]<<32)|UL[0] = (0x00010000<<32)|1
    static const u32 prog[] = { plzcw(3,1), HALT };
    runGPR128Test("plzcw_positive", prog, 2,
        {{1, (0x00010000ull<<32)|1ull, 0ull}},
        {{3, (14ull<<32)|30ull, 0ull}});
}

// PLZCW negative: rs.UL[0]=0x80000000 → 0; rs.UL[1]=0xFFFFFFFF → 31
static void test_plzcw_negative()
{
    static const u32 prog[] = { plzcw(3,1), HALT };
    runGPR128Test("plzcw_negative", prog, 2,
        {{1, (0xFFFFFFFFull<<32)|0x80000000ull, 0ull}},
        {{3, (31ull<<32)|0ull, 0ull}});
}

// PSLLH sa=4: all 8 lanes of 0x1234 << 4 = 0x2340
static void test_psllh()
{
    static const u32 prog[] = { psllh(3,1,4), HALT };
    runGPR128Test("psllh", prog, 2,
        {{1, 0x1234123412341234ull, 0x1234123412341234ull}},
        {{3, 0x2340234023402340ull, 0x2340234023402340ull}});
}

// PSRLH sa=4: all 8 lanes of 0x1234 >> 4 = 0x0123 (logical)
static void test_psrlh()
{
    static const u32 prog[] = { psrlh(3,1,4), HALT };
    runGPR128Test("psrlh", prog, 2,
        {{1, 0x1234123412341234ull, 0x1234123412341234ull}},
        {{3, 0x0123012301230123ull, 0x0123012301230123ull}});
}

// PSRAH sa=4: all 8 lanes of 0xFF00 (=-256 as s16) >> 4 = -16 = 0xFFF0
static void test_psrah()
{
    static const u32 prog[] = { psrah(3,1,4), HALT };
    runGPR128Test("psrah", prog, 2,
        {{1, 0xFF00FF00FF00FF00ull, 0xFF00FF00FF00FF00ull}},
        {{3, 0xFFF0FFF0FFF0FFF0ull, 0xFFF0FFF0FFF0FFF0ull}});
}

// PSLLW sa=8: all 4 lanes of 0x00001234 << 8 = 0x00123400
static void test_psllw()
{
    static const u32 prog[] = { psllw(3,1,8), HALT };
    runGPR128Test("psllw", prog, 2,
        {{1, 0x0000123400001234ull, 0x0000123400001234ull}},
        {{3, 0x0012340000123400ull, 0x0012340000123400ull}});
}

// PSRLW sa=8: all 4 lanes of 0x12340000 >> 8 = 0x00123400 (logical)
static void test_psrlw()
{
    static const u32 prog[] = { psrlw(3,1,8), HALT };
    runGPR128Test("psrlw", prog, 2,
        {{1, 0x1234000012340000ull, 0x1234000012340000ull}},
        {{3, 0x0012340000123400ull, 0x0012340000123400ull}});
}

// PSRAW sa=8: all 4 lanes of 0xFFFF0000 (=-65536 as s32) >> 8 = -256 = 0xFFFFFF00
static void test_psraw()
{
    static const u32 prog[] = { psraw(3,1,8), HALT };
    runGPR128Test("psraw", prog, 2,
        {{1, 0xFFFF0000FFFF0000ull, 0xFFFF0000FFFF0000ull}},
        {{3, 0xFFFFFF00FFFFFF00ull, 0xFFFFFF00FFFFFF00ull}});
}

// PADDH: 8×16-bit add; all lanes: 0x0100+0x0100=0x0200
static void test_paddh()
{
    static const u32 prog[] = { paddh(3,1,2), HALT };
    runGPR128Test("paddh", prog, 2,
        {{1, 0x0100010001000100ull, 0x0100010001000100ull},
         {2, 0x0100010001000100ull, 0x0100010001000100ull}},
        {{3, 0x0200020002000200ull, 0x0200020002000200ull}});
}

// PSUBH: 8×16-bit sub; all lanes: 0x0500-0x0200=0x0300
static void test_psubh()
{
    static const u32 prog[] = { psubh(3,1,2), HALT };
    runGPR128Test("psubh", prog, 2,
        {{1, 0x0500050005000500ull, 0x0500050005000500ull},
         {2, 0x0200020002000200ull, 0x0200020002000200ull}},
        {{3, 0x0300030003000300ull, 0x0300030003000300ull}});
}

// PSUBB: 16×8-bit sub; all bytes: 0x10-0x03=0x0D
static void test_psubb()
{
    static const u32 prog[] = { psubb(3,1,2), HALT };
    runGPR128Test("psubb", prog, 2,
        {{1, 0x1010101010101010ull, 0x1010101010101010ull},
         {2, 0x0303030303030303ull, 0x0303030303030303ull}},
        {{3, 0x0D0D0D0D0D0D0D0Dull, 0x0D0D0D0D0D0D0D0Dull}});
}

// PCGTW: 4×32-bit signed compare; all rs.SL=10 > rt.SL=5 → all 0xFFFFFFFF
static void test_pcgtw()
{
    static const u32 prog[] = { pcgtw(3,1,2), HALT };
    runGPR128Test("pcgtw", prog, 2,
        {{1, 0x0000000A0000000Aull, 0x0000000A0000000Aull},
         {2, 0x0000000500000005ull, 0x0000000500000005ull}},
        {{3, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull}});
}

// PMAXW: signed max per word; rs={10,-5,10,-5}, rt={5,2,5,2} → {10,2,10,2}
// rs.UD[0] = (-5<<32)|10 = 0xFFFFFFFB0000000A; rt.UD[0] = (2<<32)|5
static void test_pmaxw()
{
    static const u32 prog[] = { pmaxw(3,1,2), HALT };
    runGPR128Test("pmaxw", prog, 2,
        {{1, 0xFFFFFFFB0000000Aull, 0xFFFFFFFB0000000Aull},
         {2, 0x0000000200000005ull, 0x0000000200000005ull}},
        {{3, 0x000000020000000Aull, 0x000000020000000Aull}});
}

// PMINW: signed min per word; same inputs → {5,-5,5,-5}
static void test_pminw()
{
    static const u32 prog[] = { pminw(3,1,2), HALT };
    runGPR128Test("pminw", prog, 2,
        {{1, 0xFFFFFFFB0000000Aull, 0xFFFFFFFB0000000Aull},
         {2, 0x0000000200000005ull, 0x0000000200000005ull}},
        {{3, 0xFFFFFFFB00000005ull, 0xFFFFFFFB00000005ull}});
}

// PABSW: |rt.SL[i]|; rt={-10,5,-10,5} → {10,5,10,5}
static void test_pabsw()
{
    static const u32 prog[] = { pabsw(3,1), HALT };
    runGPR128Test("pabsw", prog, 2,
        {{1, 0x00000005FFFFFFF6ull, 0x00000005FFFFFFF6ull}},
        {{3, 0x000000050000000Aull, 0x000000050000000Aull}});
}

// PCEQW: equal mask per word; rs={10,5,10,5}, rt={10,10,10,10} → {-1,0,-1,0}
static void test_pceqw()
{
    static const u32 prog[] = { pceqw(3,1,2), HALT };
    runGPR128Test("pceqw", prog, 2,
        {{1, 0x000000050000000Aull, 0x000000050000000Aull},
         {2, 0x0000000A0000000Aull, 0x0000000A0000000Aull}},
        {{3, 0x00000000FFFFFFFFull, 0x00000000FFFFFFFFull}});
}

// PEXTLW: interleave lower 32-bit halves of rs and rt
// rs.UL[0]=0xAAAAAAAA, rs.UL[1]=0xBBBBBBBB; rt.UL[0]=0x11111111, rt.UL[1]=0x22222222
// → rd.UL = {rt[0],rs[0],rt[1],rs[1]} = {0x1111,0xAAAA,0x2222,0xBBBB}
static void test_pextlw()
{
    static const u32 prog[] = { pextlw(3,1,2), HALT };
    runGPR128Test("pextlw", prog, 2,
        {{1, 0xBBBBBBBBAAAAAAAAull, 0ull},
         {2, 0x2222222211111111ull, 0ull}},
        {{3, 0xAAAAAAAA11111111ull, 0xBBBBBBBB22222222ull}});
}

// PPACW: pack even 32-bit words; takes UL[0] and UL[2] from each source
// rs.UL[0]=0xAAAAAAAA, rs.UL[2]=0xBBBBBBBB; rt.UL[0]=0x11111111, rt.UL[2]=0x22222222
// → rd.UL = {rt[0],rt[2],rs[0],rs[2]}
static void test_ppacw()
{
    static const u32 prog[] = { ppacw(3,1,2), HALT };
    runGPR128Test("ppacw", prog, 2,
        {{1, 0x00000000AAAAAAAAull, 0x00000000BBBBBBBBull},
         {2, 0x0000000011111111ull, 0x0000000022222222ull}},
        {{3, 0x2222222211111111ull, 0xBBBBBBBBAAAAAAAAull}});
}

// PEXTLH: interleave lower 4 halfwords of rs and rt
// rs.US[0..3]={0xAAAA,0xBBBB,0xCCCC,0xDDDD}; rt.US[0..3]={0x1111,0x2222,0x3333,0x4444}
// → rd.US = {rt[0],rs[0],rt[1],rs[1],rt[2],rs[2],rt[3],rs[3]}
static void test_pextlh()
{
    static const u32 prog[] = { pextlh(3,1,2), HALT };
    // rs.UD[0] = UL[1]<<32|UL[0] = (US[3]<<16|US[2])<<32|(US[1]<<16|US[0])
    //          = (0xDDDD<<16|0xCCCC)<<32|(0xBBBB<<16|0xAAAA)
    //          = 0xDDDDCCCCBBBBAAAA
    runGPR128Test("pextlh", prog, 2,
        {{1, 0xDDDDCCCCBBBBAAAAull, 0ull},
         {2, 0x4444333322221111ull, 0ull}},
        {{3, 0xBBBB2222AAAA1111ull, 0xDDDD4444CCCC3333ull}});
}

// PPACH: pack even halfwords; takes US[0,2,4,6] from each source
// rt.US[0,2,4,6]={0x1111,0x2222,0x3333,0x4444}; rs.US[0,2,4,6]={0xAAAA,0xBBBB,0xCCCC,0xDDDD}
// → rd.US = {rt[0],rt[2],rt[4],rt[6],rs[0],rs[2],rs[4],rs[6]}
static void test_ppach()
{
    static const u32 prog[] = { ppach(3,1,2), HALT };
    // rs: even halves only (odd=0): UL[0]=(0<<16|0xAAAA)=0x0000AAAA, UL[1]=0x0000BBBB
    runGPR128Test("ppach", prog, 2,
        {{1, 0x0000BBBB0000AAAAull, 0x0000DDDD0000CCCCull},
         {2, 0x0000222200001111ull, 0x0000444400003333ull}},
        {{3, 0x4444333322221111ull, 0xDDDDCCCCBBBBAAAAull}});
}

// PCPYH: broadcast rt.US[0] to lower 4 lanes, rt.US[4] to upper 4 lanes
// rt.US[0]=0xABCD, rt.US[4]=0x1234 → all lower lanes 0xABCD, all upper 0x1234
static void test_pcpyh()
{
    static const u32 prog[] = { pcpyh(3,1), HALT };
    // rt.UD[0] = US[0] only (others 0) = 0x000000000000ABCD
    // rt.UD[1] = US[4] only = 0x0000000000001234
    runGPR128Test("pcpyh", prog, 2,
        {{1, 0x000000000000ABCDull, 0x0000000000001234ull}},
        {{3, 0xABCDABCDABCDABCDull, 0x1234123412341234ull}});
}

// ──────────────────────────────────────────────────────────────────────────────
// COP2 PARTIAL DEST MASK TESTS
// dest nibble bits: 3=X, 2=Y, 1=Z, 0=W  (0xF=all, 0xE=XYZ, 0xC=XY, 0x8=X, 0x1=W)
// ──────────────────────────────────────────────────────────────────────────────

// dest=XYZ (0xE): VADD writes X,Y,Z; W component of fd is unchanged.
// VF[3] preloaded with {10,10,10,10}; after vadd_xyz VF[1]+VF[2]: X,Y,Z updated, W=10
static void test_cop2_partial_dest_xyz()
{
    static const u32 prog[] = { cop2_s1(0xE,2,1,3,40), HALT }; // vadd xyz dest
    runVU0Test("cop2_partial_dest_xyz", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_1_0, F32_1_0, F32_1_0, F32_1_0},
          {3, F32_10_0,F32_10_0,F32_10_0,F32_10_0} },
        { {3, F32_2_0, F32_3_0, F32_4_0, F32_10_0} });
        // x=1+1=2, y=2+1=3, z=3+1=4, w unchanged=10
}

// dest=XY (0xC): VMUL writes X,Y only; Z,W of fd unchanged.
static void test_cop2_partial_dest_xy()
{
    static const u32 prog[] = { cop2_s1(0xC,2,1,3,42), HALT }; // vmul xy dest
    runVU0Test("cop2_partial_dest_xy", prog, 2,
        { {1, F32_2_0, F32_3_0, F32_4_0, F32_5_0},
          {2, F32_2_0, F32_2_0, F32_2_0, F32_2_0},
          {3, F32_10_0,F32_10_0,F32_10_0,F32_10_0} },
        { {3, F32_4_0, F32_6_0, F32_10_0, F32_10_0} });
        // x=2*2=4, y=3*2=6, z unchanged=10, w unchanged=10
}

// dest=X only (0x8): VSUB writes X only; Y,Z,W unchanged.
static void test_cop2_partial_dest_x()
{
    static const u32 prog[] = { cop2_s1(0x8,2,1,3,44), HALT }; // vsub x dest
    runVU0Test("cop2_partial_dest_x", prog, 2,
        { {1, F32_5_0, F32_5_0, F32_5_0, F32_5_0},
          {2, F32_3_0, F32_3_0, F32_3_0, F32_3_0},
          {3, F32_10_0,F32_10_0,F32_10_0,F32_10_0} },
        { {3, F32_2_0, F32_10_0, F32_10_0, F32_10_0} });
        // x=5-3=2, yzw unchanged=10
}

// dest=W only (0x1): VADD writes W only; X,Y,Z unchanged.
static void test_cop2_partial_dest_w()
{
    static const u32 prog[] = { cop2_s1(0x1,2,1,3,40), HALT }; // vadd w dest
    runVU0Test("cop2_partial_dest_w", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_1_0, F32_1_0, F32_1_0, F32_1_0},
          {3, F32_10_0,F32_10_0,F32_10_0,F32_10_0} },
        { {3, F32_10_0, F32_10_0, F32_10_0, F32_5_0} });
        // w=4+1=5, xyz unchanged=10
}

// ──────────────────────────────────────────────────────────────────────────────
// COP2 VADDbc / VSUBbc BROADCAST TESTS
// ──────────────────────────────────────────────────────────────────────────────

// VADDy: fd.xyzw = fs.xyzw + ft.y  (broadcast y across all components)
// VF[1]={1,2,3,4}, VF[2].y=5.0 → fd={6,7,8,9}
static void test_cop2_vaddy_broadcast()
{
    static const u32 prog[] = { vaddy_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vaddy_broadcast", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_PZERO, F32_5_0, F32_PZERO, F32_PZERO} },
        { {3, F32_6_0, F32_7_0, F32_8_0, F32_9_0} });
}

// VADDz: fd.xyzw = fs.xyzw + ft.z
// VF[1]={1,1,1,1}, VF[2].z=3.0 → fd={4,4,4,4}
static void test_cop2_vaddz_broadcast()
{
    static const u32 prog[] = { vaddz_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vaddz_broadcast", prog, 2,
        { {1, F32_1_0, F32_1_0, F32_1_0, F32_1_0},
          {2, F32_PZERO, F32_PZERO, F32_3_0, F32_PZERO} },
        { {3, F32_4_0, F32_4_0, F32_4_0, F32_4_0} });
}

// VADDw: fd.xyzw = fs.xyzw + ft.w  (common for translation add)
// VF[1]={1,2,3,4}, VF[2].w=10.0 → fd={11,12,13,14}
static void test_cop2_vaddw_broadcast()
{
    static constexpr u32 F32_11_0 = 0x41300000u;
    static constexpr u32 F32_13_0 = 0x41500000u;
    static constexpr u32 F32_14_0 = 0x41600000u;
    static const u32 prog[] = { vaddw_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vaddw_broadcast", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_PZERO, F32_PZERO, F32_PZERO, F32_10_0} },
        { {3, F32_11_0, F32_12_0, F32_13_0, F32_14_0} });
}

// VSUBx: fd.xyzw = fs.xyzw - ft.x  (broadcast x subtract)
// VF[1]={5,6,7,8}, VF[2].x=2.0 → fd={3,4,5,6}
static void test_cop2_vsubx_broadcast()
{
    static const u32 prog[] = { vsubx_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vsubx_broadcast", prog, 2,
        { {1, F32_5_0, F32_6_0, F32_7_0, F32_8_0},
          {2, F32_2_0, F32_PZERO, F32_PZERO, F32_PZERO} },
        { {3, F32_3_0, F32_4_0, F32_5_0, F32_6_0} });
}

// VSUBw: fd.xyzw = fs.xyzw - ft.w  (common for homogeneous subtraction)
// VF[1]={10,8,6,4}, VF[2].w=3.0 → fd={7,5,3,1}
static void test_cop2_vsubw_broadcast()
{
    static const u32 prog[] = { vsubw_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vsubw_broadcast", prog, 2,
        { {1, F32_10_0, F32_8_0, F32_6_0, F32_4_0},
          {2, F32_PZERO, F32_PZERO, F32_PZERO, F32_3_0} },
        { {3, F32_7_0, F32_5_0, F32_3_0, F32_1_0} });
}

// ──────────────────────────────────────────────────────────────────────────────
// COP2 VMSUBbc BROADCAST TESTS (fd = ACC - fs*ft.bc)
// ──────────────────────────────────────────────────────────────────────────────

// VMSUBx: fd = ACC - fs*ft.x
// ACC={10,10,10,10}, VF[1]={1,1,1,1}, VF[2].x=2.0 → fd={10-2,10-2,...}={8,8,8,8}
static void test_cop2_vmsubx_broadcast()
{
    static const u32 prog[] = { vmsubx_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vmsubx_broadcast", prog, 2,
        { {VF_ACC, F32_10_0, F32_10_0, F32_10_0, F32_10_0},
          {1,      F32_1_0,  F32_1_0,  F32_1_0,  F32_1_0},
          {2,      F32_2_0,  F32_PZERO,F32_PZERO,F32_PZERO} },
        { {3, F32_8_0, F32_8_0, F32_8_0, F32_8_0} });
}

// VMSUBy: fd = ACC - fs*ft.y
// ACC={6,6,6,6}, VF[1]={1,1,1,1}, VF[2].y=3.0 → fd={6-3,...}={3,3,3,3}
static void test_cop2_vmsuby_broadcast()
{
    static const u32 prog[] = { vmsuby_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vmsuby_broadcast", prog, 2,
        { {VF_ACC, F32_6_0, F32_6_0, F32_6_0, F32_6_0},
          {1,      F32_1_0, F32_1_0, F32_1_0, F32_1_0},
          {2,      F32_PZERO, F32_3_0, F32_PZERO, F32_PZERO} },
        { {3, F32_3_0, F32_3_0, F32_3_0, F32_3_0} });
}

// VMSUBw: fd = ACC - fs*ft.w  (used in clip calculations)
// ACC={12,12,12,12}, VF[1]={2,2,2,2}, VF[2].w=4.0 → fd={12-8,...}={4,4,4,4}
static void test_cop2_vmsubw_broadcast()
{
    static const u32 prog[] = { vmsubw_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vmsubw_broadcast", prog, 2,
        { {VF_ACC, F32_12_0, F32_12_0, F32_12_0, F32_12_0},
          {1,      F32_2_0,  F32_2_0,  F32_2_0,  F32_2_0},
          {2,      F32_PZERO,F32_PZERO,F32_PZERO,F32_4_0} },
        { {3, F32_4_0, F32_4_0, F32_4_0, F32_4_0} });
}

// ──────────────────────────────────────────────────────────────────────────────
// COP2 VMAXbc / VMINIbc BROADCAST TESTS + VMAX/VMINI FULL-VECTOR
// ──────────────────────────────────────────────────────────────────────────────

// VMAXx: fd.xyzw = max(fs.xyzw, ft.x)  (clamp-from-below with broadcast)
// VF[1]={-1,3,-1,3}, VF[2].x=2.0 → fd={2,3,2,3}
static void test_cop2_vmaxx_broadcast()
{
    static const u32 prog[] = { vmaxx_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vmaxx_broadcast", prog, 2,
        { {1, F32_NEG1, F32_3_0, F32_NEG1, F32_3_0},
          {2, F32_2_0,  F32_PZERO, F32_PZERO, F32_PZERO} },
        { {3, F32_2_0, F32_3_0, F32_2_0, F32_3_0} });
}

// VMINIx: fd.xyzw = min(fs.xyzw, ft.x)  (clamp-from-above with broadcast)
// VF[1]={1,5,1,5}, VF[2].x=3.0 → fd={1,3,1,3}
static void test_cop2_vminix_broadcast()
{
    static const u32 prog[] = { vminix_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vminix_broadcast", prog, 2,
        { {1, F32_1_0, F32_5_0, F32_1_0, F32_5_0},
          {2, F32_3_0, F32_PZERO, F32_PZERO, F32_PZERO} },
        { {3, F32_1_0, F32_3_0, F32_1_0, F32_3_0} });
}

// VMAX full-vector: fd.xyzw = max(fs.xyzw, ft.xyzw), component-wise
// VF[1]={-1,5,2,-3}, VF[2]={1,3,4,-1} → fd={1,5,4,-1}
static void test_cop2_vmax_full()
{
    static const u32 prog[] = { vmax_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vmax_full", prog, 2,
        { {1, F32_NEG1, F32_5_0,  F32_2_0,  F32_NEG3},
          {2, F32_1_0,  F32_3_0,  F32_4_0,  F32_NEG1} },
        { {3, F32_1_0,  F32_5_0,  F32_4_0,  F32_NEG1} });
}

// VMINI full-vector: fd.xyzw = min(fs.xyzw, ft.xyzw), component-wise
// VF[1]={-1,5,2,-3}, VF[2]={1,3,4,-1} → fd={-1,3,2,-3}
static void test_cop2_vmini_full()
{
    static const u32 prog[] = { vmini_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vmini_full", prog, 2,
        { {1, F32_NEG1, F32_5_0,  F32_2_0,  F32_NEG3},
          {2, F32_1_0,  F32_3_0,  F32_4_0,  F32_NEG1} },
        { {3, F32_NEG1, F32_3_0,  F32_2_0,  F32_NEG3} });
}

// ──────────────────────────────────────────────────────────────────────────────
// COP2 VMULq / VMULAq (multiply by Q register)
// ──────────────────────────────────────────────────────────────────────────────

// VMULq: fd.xyzw = fs.xyzw * Q; Q first set by VDIV (6/2=3).
// VF[4]={1,2,3,4}, Q=3.0 → fd={3,6,9,12}
static void test_cop2_vmulq()
{
    static const u32 prog[] = {
        vdiv_q(5,0, 6,0),            // Q = VF[5].x / VF[6].x = 6.0/2.0 = 3.0
        vmulq_xyzw(3, 4),            // VF[3] = VF[4] * Q
        HALT,
    };
    runVU0Test("cop2_vmulq", prog, 3,
        { {4, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {5, F32_6_0, F32_PZERO, F32_PZERO, F32_PZERO},
          {6, F32_2_0, F32_PZERO, F32_PZERO, F32_PZERO} },
        { {3, F32_3_0, F32_6_0, F32_9_0, F32_12_0} });
}

// VMULAq: ACC = fs.xyzw * Q
// VF[4]={1,1,1,1}, Q=4.0 (set via 8/2) → ACC={4,4,4,4}
static void test_cop2_vmulaq()
{
    static const u32 prog[] = {
        vdiv_q(5,0, 6,0),            // Q = VF[5].x / VF[6].x = 8.0/2.0 = 4.0
        vmulaq_xyzw(4),              // ACC = VF[4] * Q
        HALT,
    };
    runVU0Test("cop2_vmulaq", prog, 3,
        { {4, F32_1_0, F32_1_0, F32_1_0, F32_1_0},
          {5, F32_8_0, F32_PZERO, F32_PZERO, F32_PZERO},
          {6, F32_2_0, F32_PZERO, F32_PZERO, F32_PZERO} },
        { {VF_ACC, F32_4_0, F32_4_0, F32_4_0, F32_4_0} });
}

// ──────────────────────────────────────────────────────────────────────────────
// COP2 FIXED-POINT CONVERSIONS: VITOF4/12/15, VFTOI4/12/15
// ──────────────────────────────────────────────────────────────────────────────

// VITOF4: float = int * 2^-4 = int / 16
// input = {16, 32, 48, 0xFFFF_FFF0(-16)} → {1.0, 2.0, 3.0, -1.0}
static void test_cop2_vitof4()
{
    static const u32 prog[] = { vitof4_xyzw(2,1), HALT };
    runVU0Test("cop2_vitof4", prog, 2,
        { {1, 16u, 32u, 48u, 0xFFFFFFF0u} },
        { {2, F32_1_0, F32_2_0, F32_3_0, F32_NEG1} });
}

// VITOF12: float = int * 2^-12 = int / 4096
// input = {4096, 8192, 0} → {1.0, 2.0, 0.0}
static void test_cop2_vitof12()
{
    static const u32 prog[] = { vitof12_xyzw(2,1), HALT };
    runVU0Test("cop2_vitof12", prog, 2,
        { {1, 4096u, 8192u, 0u, 0u} },
        { {2, F32_1_0, F32_2_0, F32_PZERO, F32_PZERO} });
}

// VITOF15: float = int * 2^-15 = int / 32768
// input = {32768, 65536, 0} → {1.0, 2.0, 0.0}
static void test_cop2_vitof15()
{
    static const u32 prog[] = { vitof15_xyzw(2,1), HALT };
    runVU0Test("cop2_vitof15", prog, 2,
        { {1, 32768u, 65536u, 0u, 0u} },
        { {2, F32_1_0, F32_2_0, F32_PZERO, F32_PZERO} });
}

// VFTOI4: int = float * 2^4 = float * 16 (truncate toward zero)
// input = {1.0, 2.0, 3.0, -1.0} → {16, 32, 48, 0xFFFFFFF0}
static void test_cop2_vftoi4()
{
    static const u32 prog[] = { vftoi4_xyzw(2,1), HALT };
    runVU0Test("cop2_vftoi4", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_NEG1} },
        { {2, 16u, 32u, 48u, 0xFFFFFFF0u} });
}

// VFTOI12: int = float * 2^12 = float * 4096
// input = {1.0, 0.5, 0.0} → {4096, 2048, 0}
static void test_cop2_vftoi12()
{
    static const u32 prog[] = { vftoi12_xyzw(2,1), HALT };
    runVU0Test("cop2_vftoi12", prog, 2,
        { {1, F32_1_0, F32_HALF, F32_PZERO, F32_PZERO} },
        { {2, 4096u, 2048u, 0u, 0u} });
}

// VFTOI15: int = float * 2^15 = float * 32768
// input = {1.0, 0.5, 0.0} → {32768, 16384, 0}
static void test_cop2_vftoi15()
{
    static const u32 prog[] = { vftoi15_xyzw(2,1), HALT };
    runVU0Test("cop2_vftoi15", prog, 2,
        { {1, F32_1_0, F32_HALF, F32_PZERO, F32_PZERO} },
        { {2, 32768u, 16384u, 0u, 0u} });
}

// VITOF4 → VFTOI4 round-trip: convert int→float→int, should recover original value
// input = {16, 32, 48, 64} → VITOF4 → {1,2,3,4} → VFTOI4 → {16,32,48,64}
static void test_cop2_vitof4_vftoi4_roundtrip()
{
    static const u32 prog[] = {
        vitof4_xyzw(2,1),            // VF[2] = VF[1] as float/16
        vftoi4_xyzw(3,2),            // VF[3] = VF[2]*16 as int
        HALT,
    };
    runVU0Test("cop2_vitof4_vftoi4_roundtrip", prog, 3,
        { {1, 16u, 32u, 48u, 64u} },
        { {3, 16u, 32u, 48u, 64u} });
}

// ──────────────────────────────────────────────────────────────────────────────
// COP2 VI INTEGER REGISTER OPS: VISUB, VIADDI, VIAND, VIOR
// ──────────────────────────────────────────────────────────────────────────────

// VISUB: VI[3] = VI[1] - VI[2]
static void test_cop2_visub()
{
    static const u32 prog[] = { visub_vi(3,1,2), HALT };
    runVU0Test("cop2_visub", prog, 2,
        {}, {},
        {}, {},
        {{1, 10}, {2, 3}},
        {{3, 7}});
}

// VISUB underflow wraps at 16-bit: 1 - 2 = 0xFFFF (unsigned wrap)
static void test_cop2_visub_wrap()
{
    static const u32 prog[] = { visub_vi(3,1,2), HALT };
    runVU0Test("cop2_visub_wrap", prog, 2,
        {}, {},
        {}, {},
        {{1, 1}, {2, 2}},
        {{3, 0xFFFF}});
}

// VIADDI: VI[2] = VI[1] + imm5 (signed 5-bit immediate)
static void test_cop2_viaddi_pos()
{
    static const u32 prog[] = { viaddi_vi(2,1,5), HALT };
    runVU0Test("cop2_viaddi_pos", prog, 2,
        {}, {},
        {}, {},
        {{1, 10}},
        {{2, 15}});
}

// VIADDI with negative immediate (-1 = 0x1F in 5-bit field): VI[2] = VI[1] - 1
static void test_cop2_viaddi_neg()
{
    static const u32 prog[] = { viaddi_vi(2,1,-1), HALT };
    runVU0Test("cop2_viaddi_neg", prog, 2,
        {}, {},
        {}, {},
        {{1, 8}},
        {{2, 7}});
}

// VIAND: VI[3] = VI[1] & VI[2]  (bitwise AND of 16-bit VI registers)
static void test_cop2_viand()
{
    static const u32 prog[] = { viand_vi(3,1,2), HALT };
    runVU0Test("cop2_viand", prog, 2,
        {}, {},
        {}, {},
        {{1, 0xFF0F}, {2, 0x0FFF}},
        {{3, 0x0F0F}});
}

// VIOR: VI[3] = VI[1] | VI[2]  (bitwise OR of 16-bit VI registers)
static void test_cop2_vior()
{
    static const u32 prog[] = { vior_vi(3,1,2), HALT };
    runVU0Test("cop2_vior", prog, 2,
        {}, {},
        {}, {},
        {{1, 0x00FF}, {2, 0xFF00}},
        {{3, 0xFFFF}});
}

// ──────────────────────────────────────────────────────────────────────────────
// COP2 VADDAbc / VSUBAbc / VMSUBAbc (broadcast → ACC)
// ──────────────────────────────────────────────────────────────────────────────

// VADDAx: ACC = fs + ft.x (broadcast x to all lanes)
// VF[1]={1,2,3,4}, VF[2].x=5.0 → ACC={6,7,8,9}
static void test_cop2_vaddax_acc()
{
    static const u32 prog[] = { vaddax_xyzw(1,2), HALT };
    runVU0Test("cop2_vaddax_acc", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_5_0, F32_PZERO, F32_PZERO, F32_PZERO} },
        { {VF_ACC, F32_6_0, F32_7_0, F32_8_0, F32_9_0} });
}

// VADDАw: ACC = fs + ft.w (broadcast w — common for translation accumulate)
// VF[1]={1,2,3,0}, VF[2].w=4.0 → ACC={5,6,7,4}
static void test_cop2_vaddaw_acc()
{
    static const u32 prog[] = { vaddaw_xyzw(1,2), HALT };
    runVU0Test("cop2_vaddaw_acc", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_PZERO},
          {2, F32_PZERO, F32_PZERO, F32_PZERO, F32_4_0} },
        { {VF_ACC, F32_5_0, F32_6_0, F32_7_0, F32_4_0} });
}

// VSUBAx: ACC = fs - ft.x (broadcast x subtract)
// VF[1]={5,6,7,8}, VF[2].x=2.0 → ACC={3,4,5,6}
static void test_cop2_vsubax_acc()
{
    static const u32 prog[] = { vsubax_xyzw(1,2), HALT };
    runVU0Test("cop2_vsubax_acc", prog, 2,
        { {1, F32_5_0, F32_6_0, F32_7_0, F32_8_0},
          {2, F32_2_0, F32_PZERO, F32_PZERO, F32_PZERO} },
        { {VF_ACC, F32_3_0, F32_4_0, F32_5_0, F32_6_0} });
}

// VSUBAw: ACC = fs - ft.w (broadcast w subtract — used for perspective norm)
// VF[1]={10,8,6,4}, VF[2].w=3.0 → ACC={7,5,3,1}
static void test_cop2_vsubaw_acc()
{
    static const u32 prog[] = { vsubaw_xyzw(1,2), HALT };
    runVU0Test("cop2_vsubaw_acc", prog, 2,
        { {1, F32_10_0, F32_8_0, F32_6_0, F32_4_0},
          {2, F32_PZERO, F32_PZERO, F32_PZERO, F32_3_0} },
        { {VF_ACC, F32_7_0, F32_5_0, F32_3_0, F32_1_0} });
}

// VMSUBAx: ACC -= fs*ft.x
// ACC={10,10,10,10}, VF[1]={2,2,2,2}, VF[2].x=3.0 → ACC={10-6,...}={4,4,4,4}
static void test_cop2_vmsubax_acc()
{
    static const u32 prog[] = { vmsubax_xyzw(1,2), HALT };
    runVU0Test("cop2_vmsubax_acc", prog, 2,
        { {VF_ACC, F32_10_0, F32_10_0, F32_10_0, F32_10_0},
          {1,      F32_2_0,  F32_2_0,  F32_2_0,  F32_2_0},
          {2,      F32_3_0,  F32_PZERO,F32_PZERO,F32_PZERO} },
        { {VF_ACC, F32_4_0, F32_4_0, F32_4_0, F32_4_0} });
}

// VMSUBAw: ACC -= fs*ft.w  (used in clip/cull calculations)
// ACC={8,8,8,8}, VF[1]={1,1,1,1}, VF[2].w=2.0 → ACC={6,6,6,6}
static void test_cop2_vmsubaw_acc()
{
    static const u32 prog[] = { vmsubaw_xyzw(1,2), HALT };
    runVU0Test("cop2_vmsubaw_acc", prog, 2,
        { {VF_ACC, F32_8_0, F32_8_0, F32_8_0, F32_8_0},
          {1,      F32_1_0, F32_1_0, F32_1_0, F32_1_0},
          {2,      F32_PZERO,F32_PZERO,F32_PZERO,F32_2_0} },
        { {VF_ACC, F32_6_0, F32_6_0, F32_6_0, F32_6_0} });
}

// ──────────────────────────────────────────────────────────────────────────────
// COP2 VADDA / VSUBA / VMSUBA FULL-VECTOR (ACC ops, no VF dest)
// ──────────────────────────────────────────────────────────────────────────────

// VADDA: ACC = fs + ft  (full vector, overwrites ACC)
// VF[1]={1,2,3,4}, VF[2]={4,3,2,1} → ACC={5,5,5,5}
static void test_cop2_vadda_full()
{
    static const u32 prog[] = { vadda_xyzw(1,2), HALT };
    runVU0Test("cop2_vadda_full", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_4_0, F32_3_0, F32_2_0, F32_1_0} },
        { {VF_ACC, F32_5_0, F32_5_0, F32_5_0, F32_5_0} });
}

// VSUBA: ACC = fs - ft  (full vector, overwrites ACC)
// VF[1]={10,8,6,4}, VF[2]={1,2,3,4} → ACC={9,6,3,0}
static void test_cop2_vsuba_full()
{
    static const u32 prog[] = { vsuba_xyzw(1,2), HALT };
    runVU0Test("cop2_vsuba_full", prog, 2,
        { {1, F32_10_0, F32_8_0, F32_6_0, F32_4_0},
          {2, F32_1_0,  F32_2_0, F32_3_0, F32_4_0} },
        { {VF_ACC, F32_9_0, F32_6_0, F32_3_0, F32_PZERO} });
}

// VMSUBA: ACC -= fs*ft  (full vector)
// ACC={10,10,10,10}, VF[1]={2,2,2,2}, VF[2]={3,3,3,3} → ACC={4,4,4,4}
static void test_cop2_vmsuba_full()
{
    static const u32 prog[] = { vmsuba_xyzw(1,2), HALT };
    runVU0Test("cop2_vmsuba_full", prog, 2,
        { {VF_ACC, F32_10_0, F32_10_0, F32_10_0, F32_10_0},
          {1,      F32_2_0,  F32_2_0,  F32_2_0,  F32_2_0},
          {2,      F32_3_0,  F32_3_0,  F32_3_0,  F32_3_0} },
        { {VF_ACC, F32_4_0, F32_4_0, F32_4_0, F32_4_0} });
}

// ──────────────────────────────────────────────────────────────────────────────
// COP2 CHAIN TESTS (multi-instruction sequences)
// ──────────────────────────────────────────────────────────────────────────────

// VADDAx → VMADDAy → VMADDAz → VMADDAw: mixed add-then-accumulate sequence.
// VF[1..4] = {1,2,3,4}; VF[5].xyzw = {1,2,3,4} (broadcast scalars).
// Step 1 VADDAx: ACC  = VF[1]+VF[5].x  = {1,2,3,4}+1   = {2,3,4,5}
// Step 2 VMADDAy: ACC += VF[2]*VF[5].y = {2,3,4,5}+{2,4,6,8}  = {4,7,10,13}
// Step 3 VMADDAz: ACC += VF[3]*VF[5].z = {4,7,10,13}+{3,6,9,12}= {7,13,19,25}
// Step 4 VMADDAw: ACC += VF[4]*VF[5].w = {7,13,19,25}+{4,8,12,16}={11,21,31,41}
static void test_cop2_vadda_vmadda_chain()
{
    static constexpr u32 F32_11_0b = 0x41300000u; // 11.0
    static constexpr u32 F32_21_0  = 0x41A80000u; // 21.0
    static constexpr u32 F32_31_0  = 0x41F80000u; // 31.0
    static constexpr u32 F32_41_0  = 0x42240000u; // 41.0
    static const u32 prog[] = {
        vaddax_xyzw (1, 5),          // ACC  = VF[1] + VF[5].x
        vmadday_xyzw(2, 5),          // ACC += VF[2] * VF[5].y
        vmaddaz_xyzw(3, 5),          // ACC += VF[3] * VF[5].z
        vmaddaw_xyzw(4, 5),          // ACC += VF[4] * VF[5].w
        HALT,
    };
    runVU0Test("cop2_vadda_vmadda_chain", prog, 5,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {3, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {4, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {5, F32_1_0, F32_2_0, F32_3_0, F32_4_0} },
        { {VF_ACC, F32_11_0b, F32_21_0, F32_31_0, F32_41_0} });
}

// VMULAx → VMADDAy → VMADDAz then VMADDw: classic 4x4 matrix multiply
// Same as existing matrix test but using VMULA*/VMADDA* → final VMADD instead of
// VMULAX*/VMADDAY* → VMADDW.
// Diagonal matrix: {2,0,0,0},{0,3,0,0},{0,0,4,0},{0,0,0,5}; vec={1,1,1,1}
// Expected: VF[6]={2,3,4,5}
static void test_cop2_vmula_vmadda_vmadd_chain()
{
    static const u32 prog[] = {
        vmulax_xyzw (1, 5),          // ACC   = VF[1]*VF[5].x
        vmadday_xyzw(2, 5),          // ACC  += VF[2]*VF[5].y
        vmaddaz_xyzw(3, 5),          // ACC  += VF[3]*VF[5].z
        vmaddw_xyzw (6,4, 5),        // VF[6] = ACC + VF[4]*VF[5].w
        HALT,
    };
    runVU0Test("cop2_vmula_vmadda_vmadd_chain", prog, 5,
        { {1, F32_2_0,    F32_PZERO, F32_PZERO, F32_PZERO},
          {2, F32_PZERO,  F32_3_0,   F32_PZERO, F32_PZERO},
          {3, F32_PZERO,  F32_PZERO, F32_4_0,   F32_PZERO},
          {4, F32_PZERO,  F32_PZERO, F32_PZERO, F32_5_0},
          {5, F32_1_0,    F32_1_0,   F32_1_0,   F32_1_0} },
        { {6, F32_2_0, F32_3_0, F32_4_0, F32_5_0} });
}

// VI chain: VIADD → VISUB → VIADDI → VIAND → VIOR sequence
// VI[1]=10, VI[2]=3; after VIADD: VI[3]=13; after VISUB(VI[3]-VI[2]): VI[4]=10;
// after VIADDI(VI[4]+5): VI[5]=15; after VIAND(15&0xF=15, since 0x000F&0x000F=15):
// VI[6] = VI[5] & VI[1] = 15 & 10 = 10; VI[7] = VI[6] | VI[2] = 10 | 3 = 11
static void test_cop2_vi_chain()
{
    static const u32 prog[] = {
        viadd_vi (3, 1, 2),          // VI[3] = VI[1]+VI[2] = 10+3 = 13
        visub_vi (4, 3, 2),          // VI[4] = VI[3]-VI[2] = 13-3 = 10
        viaddi_vi(5, 4, 5),          // VI[5] = VI[4]+5     = 10+5 = 15
        viand_vi (6, 5, 1),          // VI[6] = VI[5]&VI[1] = 15&10 = 10
        vior_vi  (7, 6, 2),          // VI[7] = VI[6]|VI[2] = 10|3  = 11
        HALT,
    };
    runVU0Test("cop2_vi_chain", prog, 6,
        {}, {},
        {}, {},
        {{1, 10}, {2, 3}},
        {{3, 13}, {4, 10}, {5, 15}, {6, 10}, {7, 11}});
}

// ──────────────────────────────────────────────────────────────────────────────
// ADDITIONAL BRANCH NOT-TAKEN TESTS
// ──────────────────────────────────────────────────────────────────────────────

static void test_blez_not_taken()
{
    // rs=1 > 0, so BLEZ not taken; falls through to $3=99
    static const u32 prog[] = {
        addiu(1,0,1),
        addiu(3,0,1),
        blez(1,2),
        NOP,
        addiu(3,0,99),
        HALT,
    };
    runGPRTest("blez_not_taken", prog, 6, {}, {{3, 99u}});
}

static void test_bltz_not_taken()
{
    // rs=0 is not < 0, so BLTZ not taken; falls through to $3=99
    static const u32 prog[] = {
        addiu(1,0,0),
        addiu(3,0,1),
        bltz(1,2),
        NOP,
        addiu(3,0,99),
        HALT,
    };
    runGPRTest("bltz_not_taken", prog, 6, {}, {{3, 99u}});
}

static void test_bgez_not_taken()
{
    // rs=-1 < 0, so BGEZ not taken; falls through to $3=99
    static const u32 prog[] = {
        addiu(1,0,-1),
        addiu(3,0,1),
        bgez(1,2),
        NOP,
        addiu(3,0,99),
        HALT,
    };
    runGPRTest("bgez_not_taken", prog, 6, {}, {{3, 99u}});
}

// ──────────────────────────────────────────────────────────────────────────────
// JALR TEST
// ──────────────────────────────────────────────────────────────────────────────

// JALR: jump to $1, link to $31.  Verifies link register = PC+8 of JALR insn.
// word 0: lui $1, hi(target)
// word 1: ori $1, $1, lo(target)  — target = word 5
// word 2: jalr $31, $1            — $31 = EE_TEST_PC+16 (PC+8), jump to word 5
// word 3: NOP  (delay slot)
// word 4: addiu $2, 0, 99         — skipped
// word 5: HALT
static void test_jalr_link()
{
    const u32 target = EE_TEST_PC + 5*4;
    static const u32 prog[] = {
        lui(1, (u16)(EE_TEST_PC >> 16)),
        ori(1, 1, (u16)(target & 0xFFFFu)),
        jalr(31, 1),
        NOP,
        addiu(2, 0, 99),
        HALT,
    };
    runGPRTest("jalr_link", prog, 6, {},
        {{31, (u64)(EE_TEST_PC + 4*4)}, {2, 0u}});
}

// ──────────────────────────────────────────────────────────────────────────────
// 64-BIT LOAD / STORE (LD / SD)
// ──────────────────────────────────────────────────────────────────────────────

// SD stores all 64 bits; LD loads all 64 bits (no sign/zero truncation).
static void test_ld_sd()
{
    static const u32 prog[] = {
        load_data_addr(1),
        addiu(2,0,-1),               // $2 = 0xFFFFFFFFFFFFFFFF
        sd(2,1,0),                   // mem[EE_TEST_DATA..+7] = all ones
        ld(3,1,0),                   // $3 = full 64-bit reload
        HALT,
    };
    runGPRTest("ld_sd", prog, 5, {}, {{3, 0xFFFFFFFFFFFFFFFFull}});
}

// Verify the upper 32 bits survive the round-trip (can't be done with lw/sw).
static void test_ld_sd_64bit()
{
    static const u32 prog[] = {
        load_data_addr(1),
        addiu(2,0,1),
        dsll32(2,2,0),               // $2 = 0x0000000100000000
        sd(2,1,0),
        ld(3,1,0),
        HALT,
    };
    runGPRTest("ld_sd_64bit", prog, 6, {}, {{3, 0x0000000100000000ull}});
}

// ──────────────────────────────────────────────────────────────────────────────
// R5900 64-BIT: DSUBU / DSRA32 / DSLLV / DSRLV / DSRAV
// ──────────────────────────────────────────────────────────────────────────────

// DSUBU: 64-bit unsigned subtract without overflow trap.
// $1 = 0x0000000200000000, $2 = 1 → $3 = 0x00000001FFFFFFFF
static void test_dsubu_basic()
{
    static const u32 prog[] = {
        addiu(1,0,1),
        dsll32(1,1,1),               // $1 = 2<<32 = 0x0000000200000000
        addiu(2,0,1),
        dsubu(3,1,2),
        HALT,
    };
    runGPRTest("dsubu_basic", prog, 5, {}, {{3, 0x00000001FFFFFFFFull}});
}

// DSUBU zero result: $1 == $2 → $3 = 0
static void test_dsubu_zero()
{
    static const u32 prog[] = {
        daddiu(1,0,0x7FFF),
        dsubu(2,1,1),
        HALT,
    };
    runGPRTest("dsubu_zero", prog, 3, {}, {{2, 0u}});
}

// DSRA32: arithmetic right shift by sa+32 bits.
// $2 = 0x8000000000000000, DSRA32 by 0 → 0xFFFFFFFF80000000
// (high word 0x80000000 sign-extends to fill the upper 32 bits)
static void test_dsra32_negative()
{
    static const u32 prog[] = {
        addiu(1,0,1),
        dsll32(2,1,0),               // $2 = 0x0000000100000000
        dsll(2,2,31),                // $2 = 0x8000000000000000
        dsra32(3,2,0),               // shift by 32: arithmetic → 0xFFFFFFFF80000000
        HALT,
    };
    runGPRTest("dsra32_negative", prog, 5, {}, {{3, 0xFFFFFFFF80000000ull}});
}

// DSRA32: positive value → zero-filled upper half.
// $1 = 0x0000000100000000, DSRA32 by 0 → 0x0000000000000001
static void test_dsra32_positive()
{
    static const u32 prog[] = {
        addiu(1,0,1),
        dsll32(2,1,0),               // $2 = 0x0000000100000000
        dsra32(3,2,0),               // >> 32 arithmetic = 1 (positive, no fill)
        HALT,
    };
    runGPRTest("dsra32_positive", prog, 4, {}, {{3, 1u}});
}

// DSLLV: shift $1 left by the amount in $2 (variable, 64-bit).
// $1=1, $2=40 → $3 = 1<<40 = 0x0000010000000000
static void test_dsllv()
{
    static const u32 prog[] = {
        addiu(1,0,1),
        addiu(2,0,40),
        dsllv(3,1,2),
        HALT,
    };
    runGPRTest("dsllv", prog, 4, {}, {{3, 0x0000010000000000ull}});
}

// DSRLV: logical right shift by variable amount.
// $1 = 1<<40, $2 = 40 → $3 = 1
static void test_dsrlv()
{
    static const u32 prog[] = {
        addiu(1,0,1),
        dsll32(1,1,8),               // $1 = 1<<(32+8) = 1<<40
        addiu(2,0,40),
        dsrlv(3,1,2),
        HALT,
    };
    runGPRTest("dsrlv", prog, 5, {}, {{3, 1u}});
}

// DSRAV: arithmetic right shift by variable amount.
// $2 = 0x8000000000000000, shift right 1 → 0xC000000000000000
static void test_dsrav()
{
    static const u32 prog[] = {
        addiu(1,0,1),
        dsll32(2,1,0),
        dsll(2,2,31),                // $2 = 0x8000000000000000
        addiu(3,0,1),
        dsrav(4,2,3),
        HALT,
    };
    runGPRTest("dsrav", prog, 6, {}, {{4, 0xC000000000000000ull}});
}

// ──────────────────────────────────────────────────────────────────────────────
// COP1 / FPU: SUBA.S / MULA.S / MSUB.S / MADDA.S
// ──────────────────────────────────────────────────────────────────────────────

// SUBA.S: ACC = FPR[1] - FPR[2]; 5.0 - 3.0 = 2.0
static void test_suba_s()
{
    static const u32 prog[] = { suba_s(1,2), HALT };
    runFPUTest("suba_s", prog, 2,
        {{1,F32_5_0},{2,F32_3_0}},
        {{FPR_ACC,F32_2_0}});
}

// MULA.S: ACC = FPR[1] * FPR[2]; 2.0 * 3.0 = 6.0
static void test_mula_s()
{
    static const u32 prog[] = { mula_s(1,2), HALT };
    runFPUTest("mula_s", prog, 2,
        {{1,F32_2_0},{2,F32_3_0}},
        {{FPR_ACC,F32_6_0}});
}

// MSUB.S: FPR[3] = ACC - FPR[1]*FPR[2]; ACC=10.0, 1=2.0, 2=3.0 → 10-6=4.0
static void test_msub_s()
{
    static const u32 prog[] = { msub_s(3,1,2), HALT };
    runFPUTest("msub_s", prog, 2,
        {{1,F32_2_0},{2,F32_3_0},{FPR_ACC,F32_10_0}},
        {{3,F32_4_0}});
}

// MADDA.S: ACC += FPR[1]*FPR[2]; ACC=1.0, 1=2.0, 2=3.0 → ACC=7.0
static void test_madda_s()
{
    static const u32 prog[] = { madda_s(1,2), HALT };
    runFPUTest("madda_s", prog, 2,
        {{1,F32_2_0},{2,F32_3_0},{FPR_ACC,F32_1_0}},
        {{FPR_ACC,F32_7_0}});
}

// ADDA.S → MADDA.S chain: verifies intermediate ACC is used correctly.
// ADDA(2+3=5): ACC=5.0; MADDA(1*2=2): ACC=7.0
static void test_adda_madda_chain()
{
    static const u32 prog[] = { adda_s(1,2), madda_s(3,4), HALT };
    runFPUTest("adda_madda_chain", prog, 3,
        {{1,F32_2_0},{2,F32_3_0},{3,F32_1_0},{4,F32_2_0}},
        {{FPR_ACC,F32_7_0}});
}

// MULA.S → MSUB.S chain: ACC = fs*ft; fd = ACC - fs2*ft2
// MULA(1*2=2): ACC=2.0; MSUB(fd=2-1*1=1.0)
static void test_mula_msub_chain()
{
    static const u32 prog[] = { mula_s(1,2), msub_s(5,3,4), HALT };
    runFPUTest("mula_msub_chain", prog, 3,
        {{1,F32_1_0},{2,F32_2_0},{3,F32_1_0},{4,F32_1_0}},
        {{5,F32_1_0},{FPR_ACC,F32_2_0}});
}

// ──────────────────────────────────────────────────────────────────────────────
// COP2 / VU0 MACRO-MODE: remaining broadcast multiplies and ACC ops
// ──────────────────────────────────────────────────────────────────────────────

// VMULy: fd.xyzw = VF[fs].xyzw * VF[ft].y
// VF[1]={1,2,3,4}, VF[2].y=3.0 → fd={3,6,9,12}
static void test_cop2_vmuly_broadcast()
{
    static const u32 prog[] = { vmuly_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vmuly_broadcast", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_PZERO, F32_3_0, F32_PZERO, F32_PZERO} },
        { {3, F32_3_0, F32_6_0, F32_9_0, F32_12_0} });
}

// VMULz: fd.xyzw = VF[fs].xyzw * VF[ft].z
// VF[1]={1,2,3,4}, VF[2].z=2.0 → fd={2,4,6,8}
static void test_cop2_vmulz_broadcast()
{
    static const u32 prog[] = { vmulz_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vmulz_broadcast", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_PZERO, F32_PZERO, F32_2_0, F32_PZERO} },
        { {3, F32_2_0, F32_4_0, F32_6_0, F32_8_0} });
}

// VMULw: fd.xyzw = VF[fs].xyzw * VF[ft].w  (common for perspective scaling)
// VF[1]={1,2,3,4}, VF[2].w=4.0 → fd={4,8,12,16}
// (F32_16_0 = 0x41800000)
static void test_cop2_vmulw_broadcast()
{
    static constexpr u32 F32_16_0 = 0x41800000u;
    static const u32 prog[] = { vmulw_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vmulw_broadcast", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_PZERO, F32_PZERO, F32_PZERO, F32_4_0} },
        { {3, F32_4_0, F32_8_0, F32_12_0, F32_16_0} });
}

// VMADDx (standalone): fd = ACC + VF[fs]*VF[ft].x
// ACC={1,2,3,4}, VF[1]={1,1,1,1}, VF[2].x=2.0 → fd={3,4,5,6}
static void test_cop2_vmaddx_broadcast()
{
    static const u32 prog[] = { vmaddx_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vmaddx_broadcast", prog, 2,
        { {VF_ACC, F32_1_0,  F32_2_0, F32_3_0, F32_4_0},
          {1,      F32_1_0,  F32_1_0, F32_1_0, F32_1_0},
          {2,      F32_2_0,  F32_PZERO, F32_PZERO, F32_PZERO} },
        { {3, F32_3_0, F32_4_0, F32_5_0, F32_6_0} });
}

// VMADDz: fd = ACC + VF[fs]*VF[ft].z
// ACC={2,2,2,2}, VF[1]={1,1,1,1}, VF[2].z=3.0 → fd={5,5,5,5}
static void test_cop2_vmaddz_broadcast()
{
    static const u32 prog[] = { vmaddz_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vmaddz_broadcast", prog, 2,
        { {VF_ACC, F32_2_0,  F32_2_0, F32_2_0, F32_2_0},
          {1,      F32_1_0,  F32_1_0, F32_1_0, F32_1_0},
          {2,      F32_PZERO, F32_PZERO, F32_3_0, F32_PZERO} },
        { {3, F32_5_0, F32_5_0, F32_5_0, F32_5_0} });
}

// VMADDw: fd = ACC + VF[fs]*VF[ft].w  (used in homogeneous transform final step)
// ACC={1,1,1,1}, VF[1]={2,2,2,2}, VF[2].w=3.0 → fd={7,7,7,7}
static void test_cop2_vmaddw_broadcast()
{
    static const u32 prog[] = { vmaddw_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vmaddw_broadcast", prog, 2,
        { {VF_ACC, F32_1_0, F32_1_0, F32_1_0, F32_1_0},
          {1,      F32_2_0, F32_2_0, F32_2_0, F32_2_0},
          {2,      F32_PZERO, F32_PZERO, F32_PZERO, F32_3_0} },
        { {3, F32_7_0, F32_7_0, F32_7_0, F32_7_0} });
}

// VMULAy: ACC = VF[fs].xyzw * VF[ft].y
// VF[1]={1,2,3,4}, VF[2].y=2.0 → ACC={2,4,6,8}
static void test_cop2_vmulay_acc()
{
    static const u32 prog[] = { vmulay_xyzw(1,2), HALT };
    runVU0Test("cop2_vmulay_acc", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_PZERO, F32_2_0, F32_PZERO, F32_PZERO} },
        { {VF_ACC, F32_2_0, F32_4_0, F32_6_0, F32_8_0} });
}

// VMULAz: ACC = VF[fs].xyzw * VF[ft].z
// VF[1]={1,2,3,4}, VF[2].z=3.0 → ACC={3,6,9,12}
static void test_cop2_vmulaz_acc()
{
    static const u32 prog[] = { vmulaz_xyzw(1,2), HALT };
    runVU0Test("cop2_vmulaz_acc", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_PZERO, F32_PZERO, F32_3_0, F32_PZERO} },
        { {VF_ACC, F32_3_0, F32_6_0, F32_9_0, F32_12_0} });
}

// VMULAw: ACC = VF[fs].xyzw * VF[ft].w  (used to init ACC from w-column of matrix)
// VF[1]={1,2,3,4}, VF[2].w=5.0 → ACC={5,10,15,20}
// (F32_15_0 = 0x41700000, F32_20_0 = 0x41A00000)
static void test_cop2_vmulaw_acc()
{
    static constexpr u32 F32_15_0 = 0x41700000u;
    static constexpr u32 F32_20_0 = 0x41A00000u;
    static const u32 prog[] = { vmulaw_xyzw(1,2), HALT };
    runVU0Test("cop2_vmulaw_acc", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_PZERO, F32_PZERO, F32_PZERO, F32_5_0} },
        { {VF_ACC, F32_5_0, F32_10_0, F32_15_0, F32_20_0} });
}

// VMADDAx: ACC += VF[fs].xyzw * VF[ft].x
// ACC={1,2,3,4}, VF[1]={1,1,1,1}, VF[2].x=3.0 → ACC={4,5,6,7}
static void test_cop2_vmaddax_acc()
{
    static const u32 prog[] = { vmaddax_xyzw(1,2), HALT };
    runVU0Test("cop2_vmaddax_acc", prog, 2,
        { {VF_ACC, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {1,      F32_1_0, F32_1_0, F32_1_0, F32_1_0},
          {2,      F32_3_0, F32_PZERO, F32_PZERO, F32_PZERO} },
        { {VF_ACC, F32_4_0, F32_5_0, F32_6_0, F32_7_0} });
}

// VMADDAw: ACC += VF[fs].xyzw * VF[ft].w
// ACC={2,2,2,2}, VF[1]={1,1,1,1}, VF[2].w=4.0 → ACC={6,6,6,6}
static void test_cop2_vmaddaw_acc()
{
    static const u32 prog[] = { vmaddaw_xyzw(1,2), HALT };
    runVU0Test("cop2_vmaddaw_acc", prog, 2,
        { {VF_ACC, F32_2_0, F32_2_0, F32_2_0, F32_2_0},
          {1,      F32_1_0, F32_1_0, F32_1_0, F32_1_0},
          {2,      F32_PZERO, F32_PZERO, F32_PZERO, F32_4_0} },
        { {VF_ACC, F32_6_0, F32_6_0, F32_6_0, F32_6_0} });
}

// VNOP: no-op — all registers unchanged.
static void test_cop2_vnop()
{
    static const u32 prog[] = { vnop(), HALT };
    runVU0Test("cop2_vnop", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0} },
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0} });
}

// ──────────────────────────────────────────────────────────────────────────────
// J / JAL (J-type absolute jumps)
// ──────────────────────────────────────────────────────────────────────────────

// J: jumps over prog[2] (skipped), lands at prog[3]; delay slot at prog[1] executes
static void test_j_skip()
{
    static const u32 prog[] = {
        j(EE_TEST_PC + 12), // jump to prog[3]
        NOP,                 // delay slot — executes
        addiu(1,0,99),       // skipped
        addiu(2,0,42),       // lands
        HALT,
    };
    runGPRTest("j_skip", prog, 5, {}, {{1,0u},{2,42u}});
}

// JAL: $31 = EE_TEST_PC+8 (address after delay slot); jumps to prog[3]
static void test_jal_abs()
{
    static const u32 prog[] = {
        jal(EE_TEST_PC + 12), // call; $31 = EE_TEST_PC+8
        NOP,                   // delay slot
        addiu(1,0,99),         // skipped
        addiu(2,0,42),         // lands
        HALT,
    };
    runGPRTest("jal_abs", prog, 5, {}, {{2,42u},{31,(u64)(EE_TEST_PC+8)}});
}

// ──────────────────────────────────────────────────────────────────────────────
// Branch-likely (delay slot executes only when branch is taken)
// ──────────────────────────────────────────────────────────────────────────────

// BEQL taken: 5==5 → delay slot runs, lands at prog[5]
static void test_beql_taken()
{
    static const u32 prog[] = {
        addiu(1,0,5),
        addiu(2,0,5),
        beql(1,2,2),    // taken (1==2), offset=2 → prog[5]
        addiu(3,0,1),   // delay slot — executes
        addiu(4,0,99),  // skipped
        addiu(5,0,2),   // lands
        HALT,
    };
    runGPRTest("beql_taken", prog, 7, {}, {{3,1u},{4,0u},{5,2u}});
}

// BEQL not taken: delay slot is skipped, next instruction executes
static void test_beql_not_taken()
{
    static const u32 prog[] = {
        addiu(1,0,5),
        addiu(2,0,6),
        beql(1,2,2),    // NOT taken (5!=6)
        addiu(3,0,1),   // delay slot — SKIPPED
        addiu(4,0,99),  // next instruction after failed branch-likely
        HALT,
    };
    runGPRTest("beql_not_taken", prog, 6, {}, {{3,0u},{4,99u}});
}

// BNEL taken: 3!=5 → delay slot runs, lands at prog[5]
static void test_bnel_taken()
{
    static const u32 prog[] = {
        addiu(1,0,3),
        addiu(2,0,5),
        bnel(1,2,2),    // taken (3!=5)
        addiu(3,0,1),   // delay slot — executes
        addiu(4,0,99),  // skipped
        addiu(5,0,2),   // lands
        HALT,
    };
    runGPRTest("bnel_taken", prog, 7, {}, {{3,1u},{4,0u},{5,2u}});
}

// BGTZL taken: 1 > 0 → delay slot runs, lands at prog[4]
static void test_bgtzl_taken()
{
    static const u32 prog[] = {
        addiu(1,0,1),
        bgtzl(1,2),     // taken (1>0)
        addiu(2,0,1),   // delay slot — executes
        addiu(3,0,99),  // skipped
        addiu(4,0,2),   // lands
        HALT,
    };
    runGPRTest("bgtzl_taken", prog, 6, {}, {{2,1u},{3,0u},{4,2u}});
}

// BLEZL taken: -1 ≤ 0 → delay slot runs, lands at prog[4]
static void test_blezl_taken()
{
    static const u32 prog[] = {
        addiu(1,0,-1),
        blezl(1,2),     // taken (-1≤0)
        addiu(2,0,1),   // delay slot — executes
        addiu(3,0,99),  // skipped
        addiu(4,0,2),   // lands
        HALT,
    };
    runGPRTest("blezl_taken", prog, 6, {}, {{2,1u},{3,0u},{4,2u}});
}

// BLTZL not taken: 1 ≥ 0 → delay slot skipped, execution falls through
static void test_bltzl_not_taken()
{
    static const u32 prog[] = {
        addiu(1,0,1),
        bltzl(1,2),     // NOT taken (1 ≥ 0)
        addiu(2,0,1),   // delay slot — SKIPPED
        addiu(3,0,99),  // next instruction after failed branch-likely
        HALT,
    };
    runGPRTest("bltzl_not_taken", prog, 5, {}, {{2,0u},{3,99u}});
}

// BGEZL taken: 0 ≥ 0 → delay slot runs, lands at prog[4]
static void test_bgezl_taken()
{
    static const u32 prog[] = {
        NOP,            // $1 stays 0
        bgezl(1,2),     // taken (0≥0)
        addiu(2,0,1),   // delay slot — executes
        addiu(3,0,99),  // skipped
        addiu(4,0,2),   // lands
        HALT,
    };
    runGPRTest("bgezl_taken", prog, 6, {}, {{2,1u},{3,0u},{4,2u}});
}

// ──────────────────────────────────────────────────────────────────────────────
// LWL / LWR / SWL / SWR — unaligned word load/store
// ──────────────────────────────────────────────────────────────────────────────

// LWL shift=3: fully loads aligned word; addr=EE_TEST_DATA+3 → shift=3
// result=(s32)mem=0x12345678 (positive → no sign extension in upper)
static void test_lwl_full()
{
    static const u32 prog[] = { lwl(1,2,3), HALT };
    runMemTest("lwl_full", prog, 2,
        {{2, EE_TEST_DATA}},
        {{0, 0x12345678u}},
        {{1, 0x0000000012345678ull}},
        {});
}

// LWL shift=1: two MSBs of word shifted left 16 into register upper half
// addr=EE_TEST_DATA+1, shift=1 → $1=(s32)((0&0xFFFF)|(0x12345678<<16))=(s32)0x56780000
static void test_lwl_partial()
{
    static const u32 prog[] = { lwl(1,2,1), HALT };
    runMemTest("lwl_partial", prog, 2,
        {{2, EE_TEST_DATA}},
        {{0, 0x12345678u}},
        {{1, 0x0000000056780000ull}},
        {});
}

// LWR shift=0: fully loads aligned word, sign-extended
static void test_lwr_full()
{
    static const u32 prog[] = { lwr(1,2,0), HALT };
    runMemTest("lwr_full", prog, 2,
        {{2, EE_TEST_DATA}},
        {{0, 0x12345678u}},
        {{1, 0x0000000012345678ull}},
        {});
}

// LWR shift=2: loads lower 2 bytes into $1[15:0]; upper 32 bits preserved (=0)
// addr=EE_TEST_DATA+2, shift=2 → $1.UL[0]=(0x12345678>>16)=0x00001234
static void test_lwr_partial()
{
    static const u32 prog[] = { lwr(1,2,2), HALT };
    runMemTest("lwr_partial", prog, 2,
        {{2, EE_TEST_DATA}},
        {{0, 0x12345678u}},
        {{1, 0x0000000000001234ull}},
        {});
}

// LWR+LWL combined: read 4 unaligned bytes starting at byte offset+1
// mem[+0]=0x12345678, mem[+4]=0x9ABCDEF0
// bytes[1..4]=0x56,0x34,0x12,0xF0 → LE u32=0xF0123456 → sign-ext=0xFFFFFFFFF0123456
static void test_lwr_lwl_unaligned()
{
    static const u32 prog[] = {
        lwr(1,2,1),
        lwl(1,2,4),
        HALT,
    };
    runMemTest("lwr_lwl_unaligned", prog, 3,
        {{2, EE_TEST_DATA}},
        {{0, 0x12345678u},{4, 0x9ABCDEF0u}},
        {{1, 0xFFFFFFFFF0123456ull}},
        {});
}

// SWR shift=0: stores full register word to memory
static void test_swr_full()
{
    static const u32 prog[] = { swr(1,2,0), HALT };
    runMemTest("swr_full", prog, 2,
        {{1, 0xDEADBEEFull},{2, EE_TEST_DATA}},
        {},
        {},
        {{0, 0xDEADBEEFu}});
}

// SWL+SWR combined: write 4 unaligned bytes starting at byte offset+1
// $1=0x44332211; SWR $1,1($2) → mem[+0]=(0x44332211<<8)|0=0x33221100
// SWL $1,4($2) → mem[+4]=(0x44332211>>24)=0x00000044
static void test_swl_swr_unaligned()
{
    static const u32 prog[] = {
        swr(1,2,1),
        swl(1,2,4),
        HALT,
    };
    runMemTest("swl_swr_unaligned", prog, 3,
        {{1, 0x44332211ull},{2, EE_TEST_DATA}},
        {},
        {},
        {{0, 0x33221100u},{4, 0x00000044u}});
}

// ──────────────────────────────────────────────────────────────────────────────
// MMI pipeline-2: MULT1 / MULTU1 / DIV1 / DIVU1 / MFHI1 / MFLO1 / MTHI1 / MTLO1
// ──────────────────────────────────────────────────────────────────────────────

// MULT1 10*5=50: LO1=50, HI1=0
static void test_mult1_basic()
{
    static const u32 prog[] = {
        addiu(1,0,10),
        addiu(2,0,5),
        mult1(1,2),
        mflo1(3),
        mfhi1(4),
        HALT,
    };
    runGPRTest("mult1_basic", prog, 6, {}, {{3,50u},{4,0u}});
}

// MULT1 signed: (-3)*4=-12; LO1=sign_ext(-12), HI1=sign_ext(-1)
static void test_mult1_negative()
{
    static const u32 prog[] = {
        addiu(1,0,-3),
        addiu(2,0,4),
        mult1(1,2),
        mflo1(3),
        mfhi1(4),
        HALT,
    };
    runGPRTest("mult1_negative", prog, 6, {},
        {{3, (u64)(s64)-12}, {4, (u64)(s64)-1}});
}

// MULTU1 unsigned: 100*200=20000 in LO1
static void test_multu1_basic()
{
    static const u32 prog[] = {
        addiu(1,0,100),
        addiu(2,0,200),
        multu1(1,2),
        mflo1(3),
        HALT,
    };
    runGPRTest("multu1_basic", prog, 5, {}, {{3,20000u}});
}

// DIV1 11/3=3 remainder 2
static void test_div1_basic()
{
    static const u32 prog[] = {
        addiu(1,0,11),
        addiu(2,0,3),
        div1(1,2),
        mflo1(3),
        mfhi1(4),
        HALT,
    };
    runGPRTest("div1_basic", prog, 6, {}, {{3,3u},{4,2u}});
}

// DIVU1 unsigned: 0xFFFFFFFF/2=0x7FFFFFFF remainder 1
static void test_divu1_basic()
{
    static const u32 prog[] = {
        addiu(1,0,-1),   // $1 = 0xFFFFFFFF unsigned
        addiu(2,0,2),
        divu1(1,2),
        mflo1(3),
        mfhi1(4),
        HALT,
    };
    runGPRTest("divu1_basic", prog, 6, {},
        {{3,0x000000007FFFFFFFull},{4,1u}});
}

// MTHI1 / MTLO1: write 0x1234 to HI1 and LO1, read back with MFHI1/MFLO1
static void test_mthi1_mtlo1()
{
    static const u32 prog[] = {
        addiu(1,0,0x1234),
        mthi1(1),
        mtlo1(1),
        mfhi1(2),
        mflo1(3),
        HALT,
    };
    runGPRTest("mthi1_mtlo1", prog, 6, {}, {{2,0x1234u},{3,0x1234u}});
}

// ──────────────────────────────────────────────────────────────────────────────
// MMI saturating parallel arithmetic: PADDSW / PSUBSW / PADDSH / PSUBSH
// ──────────────────────────────────────────────────────────────────────────────

// PADDSW: 0x7FFFFFFF + 1 → 0x7FFFFFFF (saturates at INT32_MAX, all 4 lanes)
static void test_paddsw_sat_pos()
{
    static const u32 prog[] = { paddsw(3,1,2), HALT };
    runGPR128Test("paddsw_sat_pos", prog, 2,
        { {1, 0x7FFFFFFF7FFFFFFFull, 0x7FFFFFFF7FFFFFFFull},
          {2, 0x0000000100000001ull, 0x0000000100000001ull} },
        { {3, 0x7FFFFFFF7FFFFFFFull, 0x7FFFFFFF7FFFFFFFull} });
}

// PADDSW: 0x80000000 + (-1) → 0x80000000 (saturates at INT32_MIN, all 4 lanes)
static void test_paddsw_sat_neg()
{
    static const u32 prog[] = { paddsw(3,1,2), HALT };
    runGPR128Test("paddsw_sat_neg", prog, 2,
        { {1, 0x8000000080000000ull, 0x8000000080000000ull},
          {2, 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFFFFFFFFFFull} },
        { {3, 0x8000000080000000ull, 0x8000000080000000ull} });
}

// PSUBSW: 0x80000000 - 1 → 0x80000000 (saturates at INT32_MIN, all 4 lanes)
static void test_psubsw_sat_neg()
{
    static const u32 prog[] = { psubsw(3,1,2), HALT };
    runGPR128Test("psubsw_sat_neg", prog, 2,
        { {1, 0x8000000080000000ull, 0x8000000080000000ull},
          {2, 0x0000000100000001ull, 0x0000000100000001ull} },
        { {3, 0x8000000080000000ull, 0x8000000080000000ull} });
}

// PADDSH: 0x7FFF + 1 → 0x7FFF (saturates at INT16_MAX, all 8 lanes)
static void test_paddsh_sat_pos()
{
    static const u32 prog[] = { paddsh(3,1,2), HALT };
    runGPR128Test("paddsh_sat_pos", prog, 2,
        { {1, 0x7FFF7FFF7FFF7FFFull, 0x7FFF7FFF7FFF7FFFull},
          {2, 0x0001000100010001ull, 0x0001000100010001ull} },
        { {3, 0x7FFF7FFF7FFF7FFFull, 0x7FFF7FFF7FFF7FFFull} });
}

// PSUBSH: 0x8000 - 1 → 0x8000 (saturates at INT16_MIN, all 8 lanes)
static void test_psubsh_sat_neg()
{
    static const u32 prog[] = { psubsh(3,1,2), HALT };
    runGPR128Test("psubsh_sat_neg", prog, 2,
        { {1, 0x8000800080008000ull, 0x8000800080008000ull},
          {2, 0x0001000100010001ull, 0x0001000100010001ull} },
        { {3, 0x8000800080008000ull, 0x8000800080008000ull} });
}

// ──────────────────────────────────────────────────────────────────────────────
// COP2 missing y/z broadcast and accumulator variants
// ──────────────────────────────────────────────────────────────────────────────

// VSUBy broadcast: fd.xyzw = fs.xyzw - ft.y
// fs={5,4,3,2}, ft.y=1 → fd={4,3,2,1}
static void test_cop2_vsuby_broadcast()
{
    static const u32 prog[] = { vsuby_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vsuby_broadcast", prog, 2,
        { {1, F32_5_0, F32_4_0, F32_3_0, F32_2_0},
          {2, F32_PZERO, F32_1_0, F32_PZERO, F32_PZERO} },
        { {3, F32_4_0, F32_3_0, F32_2_0, F32_1_0} });
}

// VSUBz broadcast: fd.xyzw = fs.xyzw - ft.z
// fs={5,4,3,2}, ft.z=2 → fd={3,2,1,0}
static void test_cop2_vsubz_broadcast()
{
    static const u32 prog[] = { vsubz_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vsubz_broadcast", prog, 2,
        { {1, F32_5_0, F32_4_0, F32_3_0, F32_2_0},
          {2, F32_PZERO, F32_PZERO, F32_2_0, F32_PZERO} },
        { {3, F32_3_0, F32_2_0, F32_1_0, F32_PZERO} });
}

// VMSUBz broadcast: fd.xyzw = ACC.xyzw - fs.xyzw * ft.z
// ACC={10,10,10,10}, fs={2,2,2,2}, ft.z=3 → fd={4,4,4,4}
static void test_cop2_vmsubz_broadcast()
{
    static const u32 prog[] = { vmsubz_xyzw(3,1,2), HALT };
    runVU0Test("cop2_vmsubz_broadcast", prog, 2,
        { {VF_ACC, F32_10_0, F32_10_0, F32_10_0, F32_10_0},
          {1, F32_2_0, F32_2_0, F32_2_0, F32_2_0},
          {2, F32_PZERO, F32_PZERO, F32_3_0, F32_PZERO} },
        { {3, F32_4_0, F32_4_0, F32_4_0, F32_4_0} });
}

// VADDAy: ACC.xyzw = fs.xyzw + ft.y
// fs={1,2,3,4}, ft.y=2 → ACC={3,4,5,6}
static void test_cop2_vadday_acc()
{
    static const u32 prog[] = { vadday_xyzw(1,2), HALT };
    runVU0Test("cop2_vadday_acc", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_PZERO, F32_2_0, F32_PZERO, F32_PZERO} },
        { {VF_ACC, F32_3_0, F32_4_0, F32_5_0, F32_6_0} });
}

// VADDAz: ACC.xyzw = fs.xyzw + ft.z
// fs={1,2,3,4}, ft.z=3 → ACC={4,5,6,7}
static void test_cop2_vaddaz_acc()
{
    static const u32 prog[] = { vaddaz_xyzw(1,2), HALT };
    runVU0Test("cop2_vaddaz_acc", prog, 2,
        { {1, F32_1_0, F32_2_0, F32_3_0, F32_4_0},
          {2, F32_PZERO, F32_PZERO, F32_3_0, F32_PZERO} },
        { {VF_ACC, F32_4_0, F32_5_0, F32_6_0, F32_7_0} });
}

// VSUBAy: ACC.xyzw = fs.xyzw - ft.y
// fs={5,6,7,8}, ft.y=2 → ACC={3,4,5,6}
static void test_cop2_vsubay_acc()
{
    static const u32 prog[] = { vsubay_xyzw(1,2), HALT };
    runVU0Test("cop2_vsubay_acc", prog, 2,
        { {1, F32_5_0, F32_6_0, F32_7_0, F32_8_0},
          {2, F32_PZERO, F32_2_0, F32_PZERO, F32_PZERO} },
        { {VF_ACC, F32_3_0, F32_4_0, F32_5_0, F32_6_0} });
}

// VSUBAz: ACC.xyzw = fs.xyzw - ft.z
// fs={5,6,7,8}, ft.z=3 → ACC={2,3,4,5}
static void test_cop2_vsubaz_acc()
{
    static const u32 prog[] = { vsubaz_xyzw(1,2), HALT };
    runVU0Test("cop2_vsubaz_acc", prog, 2,
        { {1, F32_5_0, F32_6_0, F32_7_0, F32_8_0},
          {2, F32_PZERO, F32_PZERO, F32_3_0, F32_PZERO} },
        { {VF_ACC, F32_2_0, F32_3_0, F32_4_0, F32_5_0} });
}

// VMSUBAy: ACC.xyzw -= fs.xyzw * ft.y
// ACC={10,10,10,10}, fs={1,1,1,1}, ft.y=2 → ACC={8,8,8,8}
static void test_cop2_vmsubay_acc()
{
    static const u32 prog[] = { vmsubay_xyzw(1,2), HALT };
    runVU0Test("cop2_vmsubay_acc", prog, 2,
        { {VF_ACC, F32_10_0, F32_10_0, F32_10_0, F32_10_0},
          {1, F32_1_0, F32_1_0, F32_1_0, F32_1_0},
          {2, F32_PZERO, F32_2_0, F32_PZERO, F32_PZERO} },
        { {VF_ACC, F32_8_0, F32_8_0, F32_8_0, F32_8_0} });
}

// VMSUBAz: ACC.xyzw -= fs.xyzw * ft.z
// ACC={10,10,10,10}, fs={1,1,1,1}, ft.z=3 → ACC={7,7,7,7}
static void test_cop2_vmsubaz_acc()
{
    static const u32 prog[] = { vmsubaz_xyzw(1,2), HALT };
    runVU0Test("cop2_vmsubaz_acc", prog, 2,
        { {VF_ACC, F32_10_0, F32_10_0, F32_10_0, F32_10_0},
          {1, F32_1_0, F32_1_0, F32_1_0, F32_1_0},
          {2, F32_PZERO, F32_PZERO, F32_3_0, F32_PZERO} },
        { {VF_ACC, F32_7_0, F32_7_0, F32_7_0, F32_7_0} });
}

// ──────────────────────────────────────────────────────────────────────────────
// Base ISA gaps
// ──────────────────────────────────────────────────────────────────────────────

// DSLL32: rd = rt << (sa+32) — sa=0 means shift by 32
static void test_dsll32_basic()
{
    static const u32 prog[] = { dsll32(3,2,0), HALT };
    // 1 << 32 = 0x100000000
    runGPRTest("dsll32: 1<<32=0x100000000", prog, 2, {{2,1}}, {{3,0x100000000ULL}});
}

// DSLL32 sa=1: rt<<33
static void test_dsll32_sa1()
{
    static const u32 prog[] = { dsll32(3,2,1), HALT };
    // 1 << 33 = 0x200000000
    runGPRTest("dsll32: 1<<33=0x200000000", prog, 2, {{2,1}}, {{3,0x200000000ULL}});
}

// DSRL32: rd = rt >> (sa+32) logical
static void test_dsrl32_basic()
{
    static const u32 prog[] = { dsrl32(3,2,0), HALT };
    // 0x100000000 >> 32 = 1
    runGPRTest("dsrl32: 0x100000000>>32=1", prog, 2, {{2,0x100000000ULL}}, {{3,1ULL}});
}

// LWU: zero-extends 32-bit word (bit31 should NOT sign-extend)
static void test_lwu_zero_extend()
{
    // addiu $1, $0, DATA_BASE_LO; lui $1→ set $1 = EE_TEST_DATA
    // lui $2, DATA_HI; ori $2, DATA_LO to load base addr
    static const u32 prog[] = {
        lui(1, (u16)(EE_TEST_DATA >> 16)),
        ori(1, 1, (u16)(EE_TEST_DATA & 0xFFFF)),
        lwu(2, 1, 0),
        HALT
    };
    // Store 0x80001234 in memory — if sign-extended it would become 0xFFFFFFFF80001234
    runMemTest("lwu: 0x80001234 zero-extended", prog, 4,
        {}, {{0, 0x80001234u}},
        {{2, 0x80001234ULL}}, {});
}

// BLTZAL: branch-and-link if rs < 0; $31 = PC+8
static void test_bltzal_taken()
{
    // $1 = -1 (negative); bltzal $1,+1 → branch taken, $31 = PC+8
    // Pair: bltzal at offset 8 (byte), so $31 = EE_TEST_PC+8+8 = EE_TEST_PC+16
    // After branch: target is pair+2, which has addiu $2,$0,99; halt
    static const u32 prog[] = {
        addiu(1, 0, (u16)-1),          // $1 = -1
        bltzal(1, 1),                   // branch to +2 (after delay slot)
        addiu(3, 0, 42),                // delay slot: $3 = 42 (always executes)
        addiu(2, 0, 99),                // branch target: $2 = 99
        HALT
    };
    runGPRTest("bltzal taken: $2=99,$3=42,$31=link", prog, 5,
        {},
        {{2, 99}, {3, 42}, {31, (u64)(EE_TEST_PC + 12)}});
}

// BLTZAL not taken: $1 >= 0
static void test_bltzal_not_taken()
{
    // $1 = 5 >= 0 → branch not taken; but $31 still gets link = branch_PC+8
    // Fall-through: delay slot executes, then prog[3]=HALT (not prog[4]=addiu $2,99)
    // Branch target at prog[4] (offset=2 → target=delay_slot_addr+8=0x100010) is never reached
    static const u32 prog[] = {
        addiu(1, 0, 5),     // [0] $1 = 5 (>= 0, not taken)
        bltzal(1, 2),        // [1] link=$31=branch_PC+8; not taken; target=prog[4]
        addiu(2, 0, 10),    // [2] delay slot: $2 = 10
        HALT,               // [3] fall-through stops here; $2=10
        addiu(2, 0, 99),    // [4] branch target (unreachable)
        HALT
    };
    runGPRTest("bltzal not taken: $2=10,$31=link", prog, 6,
        {},
        {{2, 10}, {31, (u64)(EE_TEST_PC + 12)}});
}

// BGEZAL: branch-and-link if rs >= 0
static void test_bgezal_taken()
{
    static const u32 prog[] = {
        addiu(1, 0, 0),                 // $1 = 0 (>= 0, taken)
        bgezal(1, 1),                   // branch +1
        addiu(3, 0, 7),                 // delay slot
        addiu(2, 0, 55),                // branch target
        HALT
    };
    runGPRTest("bgezal taken: $2=55,$3=7", prog, 5,
        {},
        {{2, 55}, {3, 7}, {31, (u64)(EE_TEST_PC + 12)}});
}

// MADDU (EE): unsigned HI:LO += rs*rt; rd = LO
static void test_maddu_basic()
{
    // HI=0, LO=100, rs=7, rt=8 → LO = 100 + 7*8 = 156; HI = 0
    static const u32 prog[] = {
        addiu(1, 0, 7),
        addiu(2, 0, 8),
        mtlo(3),                        // LO = $3 (preset 100 below)
        mthi(0),                        // HI = 0
        maddu_ee(4, 1, 2),              // $4 = LO = 100+56 = 156
        mflo(4),
        HALT
    };
    // Note: mtlo($3) uses $3 as source → preset $3=100
    runGPRTest("maddu: 100+7*8=156", prog, 7,
        {{3, 100}}, {{4, 156}});
}

// BC1TL: FPU branch-likely if C=1; delay slot only executes when taken
static void test_bc1tl_taken()
{
    // C.EQ.S $f1,$f1 sets C=1; bc1tl +1; delay $2=42; target $2=99
    static const u32 prog[] = {
        mtc1(1, 1),                     // $f1 = GPR[1] (= 0)
        c_eq(1, 1),                     // C = (f1==f1) = 1
        bc1tl(1),                       // taken; delay executes
        addiu(2, 0, 42),                // delay slot (executes when taken)
        addiu(2, 0, 99),                // branch target
        HALT
    };
    runGPRTest("bc1tl taken: $2=99", prog, 6, {}, {{2, 99}});
}

// BC1TL not taken: C=0; delay slot must be SKIPPED
static void test_bc1tl_not_taken()
{
    // C.F always clears C; bc1tl not taken; delay slot skipped
    static const u32 prog[] = {
        mtc1(1, 1),
        c_f(1, 1),                      // C = 0
        bc1tl(1),                       // not taken; delay slot skipped
        addiu(2, 0, 77),                // delay slot — should NOT run
        HALT
    };
    runGPRTest("bc1tl not taken: delay skipped $2=0", prog, 5, {}, {{2, 0}});
}

// BC1FL: FPU branch-likely if C=0
static void test_bc1fl_taken()
{
    static const u32 prog[] = {
        mtc1(1, 1),
        c_f(1, 1),                      // C = 0
        bc1fl(1),                       // taken
        addiu(2, 0, 33),                // delay slot executes
        addiu(2, 0, 88),                // branch target
        HALT
    };
    runGPRTest("bc1fl taken: $2=88", prog, 6, {}, {{2, 88}});
}

// CFC1: read FCR31 (condition and rounding bits) into GPR
static void test_cfc1_read()
{
    // After c_eq sets C=1: FCR31 C bit = 0x00800000; CFC1 $1, $31
    static const u32 prog[] = {
        mtc1(2, 1),                     // $f1 = GPR[2] = 0
        c_eq(1, 1),                     // FCR31.C = 1 → bit23
        cfc1(1, 31),                    // GPR[1] = FCR31 (should have bit23 set)
        HALT
    };
    runGPRTest("cfc1: FCR31 C-bit set=0x800000", prog, 4,
        {}, {{1, FCR31_C}});
}

// CTC1: write GPR into FCR31
static void test_ctc1_write()
{
    // Write 0 to FCR31 to clear C; then c_eq would set it, but we test write-then-read
    static const u32 prog[] = {
        addiu(2, 0, 0),
        ctc1(2, 31),                    // FCR31 = 0
        cfc1(1, 31),                    // GPR[1] = FCR31 = 0
        HALT
    };
    runGPRTest("ctc1+cfc1: clear FCR31 roundtrip", prog, 4, {}, {{1, 0}});
}

// ──────────────────────────────────────────────────────────────────────────────
// MMI0 gaps: PCGTH, PMAXH, PADDB, PCGTB, PADDSB, PSUBSB, PEXTLB, PPACB
// ──────────────────────────────────────────────────────────────────────────────

// PCGTH: 8 signed halfword comparisons: rd.US[i] = rs.SS[i]>rt.SS[i] ? 0xFFFF : 0
// rs.US = {5,5,5,5,5,5,5,5}, rt.US = alternating {3,7,3,7,3,7,3,7}
// Results: {0xFFFF,0,0xFFFF,0, 0xFFFF,0,0xFFFF,0}
static void test_pcgth()
{
    static const u32 prog[] = { pcgth(3,1,2), HALT };
    runGPR128Test("pcgth: 5>3=FFFF,5>7=0 per HW", prog, 2,
        { {1, 0x0005000500050005ULL, 0x0005000500050005ULL},
          {2, 0x0007000300070003ULL, 0x0007000300070003ULL} },
        { {3, 0x0000FFFF0000FFFFULL, 0x0000FFFF0000FFFFULL} });
}

// PMAXH: max of 8 signed halfwords
// rs.US = {3,5,7,9,...}, rt.US = {6,4,6,4,...}
// max: {6,5,7,9, 6,5,7,9} → UD[0]=0x0009000700050006, UD[1]=same
static void test_pmaxh()
{
    static const u32 prog[] = { pmaxh(3,1,2), HALT };
    runGPR128Test("pmaxh: max per halfword", prog, 2,
        { {1, 0x0009000700050003ULL, 0x0009000700050003ULL},
          {2, 0x0004000600040006ULL, 0x0004000600040006ULL} },
        { {3, 0x0009000700050006ULL, 0x0009000700050006ULL} });
}

// PADDB: 16 unsigned byte adds (wrapping)
// rs.UC all = 1, rt.UC = {1,2,3,4,5,6,7,8,...}
// rd.UC = {2,3,4,5,6,7,8,9,...}
static void test_paddb()
{
    static const u32 prog[] = { paddb(3,1,2), HALT };
    runGPR128Test("paddb: byte wrapping add", prog, 2,
        { {1, 0x0101010101010101ULL, 0x0101010101010101ULL},
          {2, 0x0807060504030201ULL, 0x100F0E0D0C0B0A09ULL} },
        { {3, 0x0908070605040302ULL, 0x11100F0E0D0C0B0AULL} });
}

// PCGTB: 16 signed byte comparisons
// rs.UC all = 5, rt.UC alternating {3,7,...}
// rd.UC = {0xFF,0,0xFF,0,...}
static void test_pcgtb()
{
    static const u32 prog[] = { pcgtb(3,1,2), HALT };
    runGPR128Test("pcgtb: 5>3=FF,5>7=00", prog, 2,
        { {1, 0x0505050505050505ULL, 0x0505050505050505ULL},
          {2, 0x0703070307030703ULL, 0x0703070307030703ULL} },
        { {3, 0x00FF00FF00FF00FFULL, 0x00FF00FF00FF00FFULL} });
}

// PADDSB: saturating signed byte add
// rs.UC = 100 (0x64), rt.UC = 100 → sum 200 > 127 → saturate to 0x7F
static void test_paddsb()
{
    static const u32 prog[] = { paddsb(3,1,2), HALT };
    runGPR128Test("paddsb: 100+100 sat to 127", prog, 2,
        { {1, 0x6464646464646464ULL, 0x6464646464646464ULL},
          {2, 0x6464646464646464ULL, 0x6464646464646464ULL} },
        { {3, 0x7F7F7F7F7F7F7F7FULL, 0x7F7F7F7F7F7F7F7FULL} });
}

// PSUBSB: saturating signed byte sub
// rs.UC = 0x80 (-128), rt.UC = 1 → -128-1 = -129 → saturate to -128 = 0x80
static void test_psubsb()
{
    static const u32 prog[] = { psubsb(3,1,2), HALT };
    runGPR128Test("psubsb: -128-1 sat to -128=0x80", prog, 2,
        { {1, 0x8080808080808080ULL, 0x8080808080808080ULL},
          {2, 0x0101010101010101ULL, 0x0101010101010101ULL} },
        { {3, 0x8080808080808080ULL, 0x8080808080808080ULL} });
}

// PEXTLB: interleave lower 8 bytes of rs and rt
// rs.UC[0..7] = 0xAA, rt.UC[0..7] = 0xBB
// rd.SC = {BB,AA,BB,AA,...BB,AA} × 8
static void test_pextlb()
{
    static const u32 prog[] = { pextlb(3,1,2), HALT };
    runGPR128Test("pextlb: interleave lower bytes", prog, 2,
        { {1, 0xAAAAAAAAAAAAAAAAULL, 0xAAAAAAAAAAAAAAAAULL},
          {2, 0xBBBBBBBBBBBBBBBBULL, 0xBBBBBBBBBBBBBBBBULL} },
        { {3, 0xAABBAABBAABBAABBULL, 0xAABBAABBAABBAABBULL} });
}

// PPACB: pack even bytes from rt (low 8) and rs (high 8)
// rt.SC even bytes = {1,2,3,4,5,6,7,8}, rs.SC even bytes = {0xA,0xB,0xC,0xD,0xE,0xF,0x10,0x11}
static void test_ppacb()
{
    static const u32 prog[] = { ppacb(3,1,2), HALT };
    // rt.UD = 0x0804000300020001, 0x... (even bytes: 1,2,3,4,5,6,7,8 at positions 0,2,4,6,8,10,12,14)
    // rt: SC[0]=1,SC[1]=0,SC[2]=2,SC[3]=0,...  UD[0]=0x0004000300020001, UD[1]=0x0008000700060005
    // rs: SC[0..7]=10,0,11,0,12,0,13,0  UD[0]=0x000D000C000B000A, UD[1]=0x0011001000000000...
    // After ppacb: rd.SC[0..7]={1,2,3,4,5,6,7,8} rd.SC[8..15]={10,11,12,13,14,15,16,17}
    runGPR128Test("ppacb: pack even bytes", prog, 2,
        { {1, 0x000D000C000B000AULL, 0x00110010000F000EULL /* rs: UC[0,2,4,6]=10-13, UC[8,10,12,14]=14-17 */},
          {2, 0x0004000300020001ULL, 0x0008000700060005ULL} /* rt */ },
        { {3, 0x0807060504030201ULL, 0x11100F0E0D0C0B0AULL} });
    // Wait — rs.SC[8..15] in UD[1]: SC[8]=14=0x0E, SC[9]=0, SC[10]=15=0x0F, SC[11]=0
    // SC[12]=16=0x10, SC[13]=0, SC[14]=17=0x11, SC[15]=0
    // rs.UD[1] = 0x0011001000000000... no:
    // UD[1] = SC[15]<<56|...|SC[8] = 0<<56|0x11<<48|0<<40|0x10<<32|0<<24|0x0F<<16|0<<8|0x0E
    //       = 0x0011001000000000|0x00000F000E = 0x00110010000F000EULL
}

// ──────────────────────────────────────────────────────────────────────────────
// MMI1 gaps
// ──────────────────────────────────────────────────────────────────────────────

// PABSH: |halfword| for 8 signed halfwords
// rt.SS = {4, -4, 5, -5, 6, -6, 7, -7}
// |rd|   = {4,  4, 5,  5, 6,  6, 7,  7}
static void test_pabsh()
{
    static const u32 prog[] = { pabsh(3,2), HALT };
    // rt.UD[0]: US[0]=4, US[1]=0xFFFC(-4), US[2]=5, US[3]=0xFFFB(-5)
    //   = 0xFFFB0005FFFC0004ULL
    // rt.UD[1]: US[4]=6, US[5]=0xFFFA(-6), US[6]=7, US[7]=0xFFF9(-7)
    //   = 0xFFF90007FFFA0006ULL
    runGPR128Test("pabsh: absolute halfwords", prog, 2,
        { {2, 0xFFFB0005FFFC0004ULL, 0xFFF90007FFFA0006ULL} },
        { {3, 0x0005000500040004ULL, 0x0007000700060006ULL} });
}

// PCEQH: halfword equality
// rs.US = {5,5,5,5,...}, rt.US = {5,3,5,3,...}
// rd.US = {0xFFFF,0,0xFFFF,0,...}
static void test_pceqh()
{
    static const u32 prog[] = { pceqh(3,1,2), HALT };
    runGPR128Test("pceqh: halfword eq", prog, 2,
        { {1, 0x0005000500050005ULL, 0x0005000500050005ULL},
          {2, 0x0003000500030005ULL, 0x0003000500030005ULL} },
        { {3, 0x0000FFFF0000FFFFULL, 0x0000FFFF0000FFFFULL} });
}

// PMINH: min of 8 signed halfwords
// rs.US = {3,5,7,9,...}, rt.US = {6,4,6,4,...}
// min: {3,4,6,4,...}
static void test_pminh()
{
    static const u32 prog[] = { pminh(3,1,2), HALT };
    runGPR128Test("pminh: min per halfword", prog, 2,
        { {1, 0x0009000700050003ULL, 0x0009000700050003ULL},
          {2, 0x0004000600040006ULL, 0x0004000600040006ULL} },
        { {3, 0x0004000600040003ULL, 0x0004000600040003ULL} });
}

// PCEQB: 16 byte equality
// rs.UC all=5, rt.UC alternating {5,3,...}
// rd.UC = {0xFF,0,0xFF,0,...}
static void test_pceqb()
{
    static const u32 prog[] = { pceqb(3,1,2), HALT };
    runGPR128Test("pceqb: byte eq", prog, 2,
        { {1, 0x0505050505050505ULL, 0x0505050505050505ULL},
          {2, 0x0305030503050305ULL, 0x0305030503050305ULL} },
        { {3, 0x00FF00FF00FF00FFULL, 0x00FF00FF00FF00FFULL} });
}

// PADDUW: saturating unsigned word add (4 words)
// rs.UL = {0xFFFFFFF0,...}, rt.UL = {0x20,...} → 0xFFFFFFF0+0x20=0x100000010 → sat to 0xFFFFFFFF
static void test_padduw()
{
    static const u32 prog[] = { padduw(3,1,2), HALT };
    runGPR128Test("padduw: sat at 0xFFFFFFFF", prog, 2,
        { {1, 0xFFFFFFF0FFFFFFF0ULL, 0xFFFFFFF0FFFFFFF0ULL},
          {2, 0x0000002000000020ULL, 0x0000002000000020ULL} },
        { {3, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL} });
}

// PSUBUW: saturating unsigned word sub, floor at 0
// rs.UL = {5,...}, rt.UL = {10,...} → 5-10 < 0 → 0
static void test_psubuw()
{
    static const u32 prog[] = { psubuw(3,1,2), HALT };
    runGPR128Test("psubuw: 5-10=clamp0", prog, 2,
        { {1, 0x0000000500000005ULL, 0x0000000500000005ULL},
          {2, 0x000000100000000AULL, 0x000000100000000AULL} },
        { {3, 0x0000000000000000ULL, 0x0000000000000000ULL} });
}

// PEXTUW: interleave upper words of rs and rt
// rs.UL[2..3] = {0xAA,0xBB}, rt.UL[2..3] = {0xCC,0xDD}
// rd.UL = {CC,AA,DD,BB}
static void test_pextuw()
{
    static const u32 prog[] = { pextuw(3,1,2), HALT };
    runGPR128Test("pextuw: interleave upper words", prog, 2,
        { {1, 0x0000000000000000ULL, 0x00000000BBBBBBBBULL}, // rs.UL[2]=0xBBBBBBBB,UL[3]=0
          {2, 0x0000000000000000ULL, 0x00000000CCCCCCCCULL} }, // rt.UL[2]=0xCCCCCCCC,UL[3]=0
        { {3, 0xBBBBBBBBCCCCCCCCULL, 0x0000000000000000ULL} });
}

// PADDUH: saturating unsigned halfword add
// rs.US = 0xFFF0 each, rt.US = 0x0020 each → 0xFFF0+0x20=0x10010 → sat 0xFFFF
static void test_padduh()
{
    static const u32 prog[] = { padduh(3,1,2), HALT };
    runGPR128Test("padduh: sat at 0xFFFF", prog, 2,
        { {1, 0xFFF0FFF0FFF0FFF0ULL, 0xFFF0FFF0FFF0FFF0ULL},
          {2, 0x0020002000200020ULL, 0x0020002000200020ULL} },
        { {3, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL} });
}

// PSUBUH: saturating unsigned halfword sub, floor 0
// rs.US = 3, rt.US = 5 → 3-5 < 0 → 0
static void test_psubuh()
{
    static const u32 prog[] = { psubuh(3,1,2), HALT };
    runGPR128Test("psubuh: 3-5=clamp0", prog, 2,
        { {1, 0x0003000300030003ULL, 0x0003000300030003ULL},
          {2, 0x0005000500050005ULL, 0x0005000500050005ULL} },
        { {3, 0x0000000000000000ULL, 0x0000000000000000ULL} });
}

// PEXTUH: interleave upper halfwords of rs and rt
// rs.US[4..7]={0xAA,0xAA,0xAA,0xAA}, rt.US[4..7]={0xBB,...}
// rd.US = {BB,AA,BB,AA,BB,AA,BB,AA}
static void test_pextuh()
{
    static const u32 prog[] = { pextuh(3,1,2), HALT };
    runGPR128Test("pextuh: interleave upper halfwords", prog, 2,
        { {1, 0x0000000000000000ULL, 0xAAAAAAAAAAAAAAAAULL},
          {2, 0x0000000000000000ULL, 0xBBBBBBBBBBBBBBBBULL} },
        { {3, 0xAAAABBBBAAAABBBBULL, 0xAAAABBBBAAAABBBBULL} });
}

// PADDUB: saturating unsigned byte add
// rs.UC = 0xF0, rt.UC = 0x20 → 0x110 → sat 0xFF
static void test_paddub()
{
    static const u32 prog[] = { paddub(3,1,2), HALT };
    runGPR128Test("paddub: sat at 0xFF", prog, 2,
        { {1, 0xF0F0F0F0F0F0F0F0ULL, 0xF0F0F0F0F0F0F0F0ULL},
          {2, 0x2020202020202020ULL, 0x2020202020202020ULL} },
        { {3, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL} });
}

// PSUBUB: saturating unsigned byte sub, floor 0
// rs.UC = 3, rt.UC = 5 → 3-5 < 0 → 0
static void test_psubub()
{
    static const u32 prog[] = { psubub(3,1,2), HALT };
    runGPR128Test("psubub: 3-5=clamp0", prog, 2,
        { {1, 0x0303030303030303ULL, 0x0303030303030303ULL},
          {2, 0x0505050505050505ULL, 0x0505050505050505ULL} },
        { {3, 0x0000000000000000ULL, 0x0000000000000000ULL} });
}

// PEXTUB: interleave upper bytes of rs and rt
// rs.UC[8..15] = 0xAA each, rt.UC[8..15] = 0xBB each
// rd.SC = {BB,AA,...} × 8
static void test_pextub()
{
    static const u32 prog[] = { pextub(3,1,2), HALT };
    runGPR128Test("pextub: interleave upper bytes", prog, 2,
        { {1, 0x0000000000000000ULL, 0xAAAAAAAAAAAAAAAAULL},
          {2, 0x0000000000000000ULL, 0xBBBBBBBBBBBBBBBBULL} },
        { {3, 0xAABBAABBAABBAABBULL, 0xAABBAABBAABBAABBULL} });
}

// PADSBH: rs=$0 hardcoded; lower 4 halfwords = 0 - rt (negate), upper 4 = 0 + rt (copy)
// rt.US[n] = 0x0202 → lower: 0-0x0202=0xFDFE, upper: 0+0x0202=0x0202
static void test_padsbh()
{
    static const u32 prog[] = { padsbh(3,1), HALT };
    // padsbh encoder: mmi_r(0,rt,rd,...) → rs=$0 always
    // lower halfwords: rd.US[n] = $0.US[n] - rt.US[n] = 0 - 0x0202 = 0xFDFE
    // upper halfwords: rd.US[n] = $0.US[n] + rt.US[n] = 0 + 0x0202 = 0x0202
    runGPR128Test("padsbh: negate lower HWs, copy upper HWs", prog, 2,
        { {1, 0x0202020202020202ULL, 0x0202020202020202ULL} },
        { {3, 0xFDFEFDFEFDFEFDFEULL, 0x0202020202020202ULL} });
}

// ──────────────────────────────────────────────────────────────────────────────
// MMI2 gaps: PXOR, PMFHI/PMFLO, PINTH, PSLLVW, PSRLVW, PMULTH,
//            PEXEH, PREVH, PEXEW, PROT3W, PMADDH, PMSUBH, PMADDW, PMSUBW,
//            PMULTW, PDIVW, PHMADH, PHMSBH
// ──────────────────────────────────────────────────────────────────────────────

// PXOR: 128-bit XOR
static void test_pxor()
{
    static const u32 prog[] = { pxor(3,1,2), HALT };
    runGPR128Test("pxor: 128-bit XOR", prog, 2,
        { {1, 0xAAAAAAAAAAAAAAAAULL, 0xAAAAAAAAAAAAAAAAULL},
          {2, 0x5555555555555555ULL, 0x5555555555555555ULL} },
        { {3, 0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL} });
}

// PMFHI_M: copy 128-bit HI into GPR
// Set HI via PMTHI_M then read back with PMFHI_M
static void test_pmfhi_m()
{
    static const u32 prog[] = { pmthi_m(1), pmfhi_m(3), HALT };
    runGPR128Test("pmfhi_m: HI roundtrip", prog, 3,
        { {1, 0x1234567890ABCDEFULL, 0xFEDCBA9876543210ULL} },
        { {3, 0x1234567890ABCDEFULL, 0xFEDCBA9876543210ULL} });
}

// PMFLO_M: copy 128-bit LO into GPR
static void test_pmflo_m()
{
    static const u32 prog[] = { pmtlo_m(1), pmflo_m(3), HALT };
    runGPR128Test("pmflo_m: LO roundtrip", prog, 3,
        { {1, 0xABCDEF0123456789ULL, 0x0011223344556677ULL} },
        { {3, 0xABCDEF0123456789ULL, 0x0011223344556677ULL} });
}

// PINTH: interleave lower-rt halfwords with upper-rs halfwords
// rd.US = {rt.US[0],rs.US[4], rt.US[1],rs.US[5], rt.US[2],rs.US[6], rt.US[3],rs.US[7]}
static void test_pinth()
{
    static const u32 prog[] = { pinth(3,1,2), HALT };
    // rs.US[4..7] = {0xAA,0xAA,0xAA,0xAA} → rs.UD[1] = 0xAAAAAAAAAAAAAAAA
    // rt.US[0..3] = {0xBB,...} → rt.UD[0] = 0xBBBBBBBBBBBBBBBB
    // rd.US = {BB,AA,BB,AA,BB,AA,BB,AA}
    runGPR128Test("pinth: interleave lower-rt upper-rs HWs", prog, 3,
        { {1, 0x0000000000000000ULL, 0xAAAAAAAAAAAAAAAAULL}, // rs
          {2, 0xBBBBBBBBBBBBBBBBULL, 0x0000000000000000ULL} }, // rt
        { {3, 0xAAAABBBBAAAABBBBULL, 0xAAAABBBBAAAABBBBULL} });
}

// PSLLVW: rd.SL[0]=rt.UL[0]<<(rs.UL[0]&0x1F), sign-extended to SL[1]
static void test_psllvw()
{
    static const u32 prog[] = { psllvw(3,1,2), HALT };
    // rs.UL[0]=3 (shift by 3), rt.UL[0]=0x10000000 → result = 0x10000000<<3 = 0x80000000
    // Sign-extended: 0x80000000 as s32 = -0x80000000, UD[0] = (s64)(s32)0x80000000
    //              = 0xFFFFFFFF80000000ULL
    // rs.UL[2]=1, rt.UL[2]=0x40000000 → 0x40000000<<1=0x80000000, UD[1]=0xFFFFFFFF80000000
    runGPR128Test("psllvw: logical shl, sext result", prog, 3,
        { {1, 0x0000000000000003ULL, 0x0000000100000001ULL}, // rs: UL[0]=3,UL[2]=1
          {2, 0x0000000010000000ULL, 0x4000000040000000ULL} }, // rt: UL[0]=0x10000000,UL[2]=0x40000000
        { {3, 0xFFFFFFFF80000000ULL, 0xFFFFFFFF80000000ULL} });
}

// PSRLVW: logical right shift variable
// rs.UL[0]=2, rt.UL[0]=0x80000000 → 0x80000000>>2 = 0x20000000, SL[0]=0x20000000 (positive, sext=same)
static void test_psrlvw()
{
    static const u32 prog[] = { psrlvw(3,1,2), HALT };
    runGPR128Test("psrlvw: logical shr, sext result", prog, 3,
        { {1, 0x0000000000000002ULL, 0x0000000200000002ULL},
          {2, 0x8000000080000000ULL, 0x8000000080000000ULL} },
        { {3, 0x0000000020000000ULL, 0x0000000020000000ULL} });
}

// PEXEH: swap US[0]↔US[2] and US[4]↔US[6]
// rt.US = {1,2,3,4, 5,6,7,8} → rd.US = {3,2,1,4, 7,6,5,8}
static void test_pexeh()
{
    static const u32 prog[] = { pexeh(3,2), HALT };
    runGPR128Test("pexeh: swap center halfwords in each word", prog, 2,
        { {2, 0x0004000300020001ULL, 0x0008000700060005ULL} },
        { {3, 0x0004000100020003ULL, 0x0008000500060007ULL} });
}

// REVH: reverse 4 halfwords within each 64-bit half
// rt.US[0..3]={1,2,3,4} → rd.US[0..3]={4,3,2,1}
static void test_prevh()
{
    static const u32 prog[] = { prevh(3,2), HALT };
    runGPR128Test("prevh: reverse halfwords per 64-bit half", prog, 2,
        { {2, 0x0004000300020001ULL, 0x0008000700060005ULL} },
        { {3, 0x0001000200030004ULL, 0x0005000600070008ULL} });
}

// PEXEW: swap UL[0]↔UL[2]
// rt.UL = {1,2,3,4} → rd.UL = {3,2,1,4}
static void test_pexew()
{
    static const u32 prog[] = { pexew(3,2), HALT };
    runGPR128Test("pexew: swap words 0 and 2", prog, 2,
        { {2, 0x0000000200000001ULL, 0x0000000400000003ULL} },
        { {3, 0x0000000200000003ULL, 0x0000000400000001ULL} });
}

// PROT3W: rotate words 0,1,2 left by one
// rt.UL = {1,2,3,4} → rd.UL = {2,3,1,4}
static void test_prot3w()
{
    static const u32 prog[] = { prot3w(3,2), HALT };
    runGPR128Test("prot3w: rotate lower 3 words", prog, 2,
        { {2, 0x0000000200000001ULL, 0x0000000400000003ULL} },
        { {3, 0x0000000300000002ULL, 0x0000000400000001ULL} });
}

// PMULTH: 8×(16×16) parallel halfword multiply → results in LO/HI and rd
// rs.SS = {2,3,4,5, 6,7,8,9}, rt.SS = {10,10,10,10,10,10,10,10}
// Products: {20,30,40,50, 60,70,80,90}
// rd.SL[0]=20,rd.SL[1]=30,rd.SL[2]=60,rd.SL[3]=70
// Note: rd layout: SL[0..1] from lower pair, SL[2..3] from upper pair
static void test_pmulth()
{
    static const u32 prog[] = { pmulth(3,1,2), HALT };
    // rs.US = {2,3,4,5,6,7,8,9}: UD[0]=0x0005000400030002, UD[1]=0x0009000800070006
    // rt.US = {10,...}:           UD[0]=UD[1]=0x000A000A000A000AULL
    // Products a0=20,a1=30,a2=40,a3=50,a4=60,a5=70,a6=80,a7=90
    // rd.SL[0]=a0=20, rd.SL[1]=a1=30, rd.SL[2]=a2=40... hmm check layout
    // from source: rd.SL[0]=a0, rd.SL[1]=a1, rd.SL[2]=a2, rd.SL[3]=a3
    // rd.UD[0] = (a1<<32)|a0 = (30<<32)|20 = 0x0000001E00000014ULL
    // rd.UD[1] = (a3<<32)|a2 = (50<<32)|40 = 0x0000003200000028ULL
    runGPR128Test("pmulth: 8 halfword multiplies", prog, 3,
        { {1, 0x0005000400030002ULL, 0x0009000800070006ULL},
          {2, 0x000A000A000A000AULL, 0x000A000A000A000AULL} },
        { {3, 0x0000002800000014ULL, 0x000000500000003CULL} });
    // rd.UL[0]=a0=20, rd.UL[1]=a2=40, rd.UL[2]=a4=60, rd.UL[3]=a6=80
}

// PMADDW: HI:LO += rs.SL[pair]*rt.SL[pair]; rd = LO
// HI=0, LO=0, rs.SL[0]=3,rt.SL[0]=4 → 12; rs.SL[2]=5,rt.SL[2]=6 → 30
// rd.UD[0] = (12<<32)|12 = 0x0000000C0000000C (SL[0]=SL[1]=12)
// rd.UD[1] = (30<<32)|30 = 0x0000001E0000001E
static void test_pmaddw()
{
    static const u32 prog[] = { pmaddw(3,1,2), HALT };
    runGPR128Test("pmaddw: two 32x32 MADD into LO", prog, 3,
        { {1, 0x0000000000000003ULL, 0x0000000500000005ULL},
          {2, 0x0000000000000004ULL, 0x0000000600000006ULL} },
        { {3, 0x000000000000000CULL, 0x000000000000001EULL} }); // rd.UL[1]=HI=0, rd.UL[3]=HI=0
}

// PMSUBW: HI:LO -= rs*rt; rd=LO
// HI=0, LO preset to 100, subtract 3*4=12 → LO=88; 5*6=30 → LO=70
static void test_pmsubw()
{
    // Use PMTLO_M to preset LO, then PMSUBW
    static const u32 prog[] = { pmtlo_m(4), pmsubw(3,1,2), HALT };
    // $4 preset: UD[0]=100,UD[1]=100
    runGPR128Test("pmsubw: two 32x32 MSUB from LO", prog, 3,
        { {4, 0x0000006400000064ULL, 0x0000006400000064ULL}, // LO preset = 100
          {1, 0x0000000000000003ULL, 0x0000000500000005ULL},
          {2, 0x0000000000000004ULL, 0x0000000600000006ULL} },
        { {3, 0x0000000000000058ULL, 0x0000000000000046ULL} }); // 100-12=88, 100-30=70; HI=0
}

// PMULTW: rd=LO=rs.SL*rt.SL (two 32×32→64)
// rs.SL[0]=6,rt.SL[0]=7 → LO=42; rs.SL[2]=8,rt.SL[2]=9 → LO=72
static void test_pmultw()
{
    static const u32 prog[] = { pmultw(3,1,2), HALT };
    runGPR128Test("pmultw: two 32x32 → LO", prog, 3,
        { {1, 0x0000000000000006ULL, 0x0000000800000008ULL},
          {2, 0x0000000000000007ULL, 0x0000000900000009ULL} },
        { {3, 0x000000000000002AULL, 0x0000000000000048ULL} }); // rd.SD[dd]=64-bit product; high32=0
}

// PDIVW: LO=rs/rt, HI=rs%rt (two word pairs); read back with PMFLO_M
static void test_pdivw()
{
    // rs.SL[0]=21, rt.SL[0]=4 → LO.SL[0]=5, HI.SL[0]=1
    // rs.SL[2]=20, rt.SL[2]=3 → LO.SL[2]=6, HI.SL[2]=2
    static const u32 prog[] = { pdivw(1,2), pmflo_m(3), HALT };
    runGPR128Test("pdivw: quotient in LO", prog, 3,
        { {1, 0x0000000000000015ULL, 0x0000001400000014ULL}, // rs: SL[0]=21,SL[2]=20
          {2, 0x0000000000000004ULL, 0x0000000300000003ULL} }, // rt: SL[0]=4,SL[2]=3
        { {3, 0x0000000000000005ULL, 0x0000000000000006ULL} }); // LO.SD[dd]=(s32)quotient; UL[1]=UL[3]=0
}

// PMADDH: HI:LO += halfword pairs; rd = some columns of LO
// Simple: HI=LO=0, rs.SS[0]=2,rt.SS[0]=3 → a0=6; check rd
static void test_pmaddh()
{
    static const u32 prog[] = { pmaddh(3,1,2), HALT };
    // rs.SS = {2,2,2,2,2,2,2,2}, rt.SS = {3,3,...}
    // a[i] = rs.SS[i]*rt.SS[i] = 6 for all
    // rd.SL layout from pmaddh: check pcsx2 source...
    // From MMI.cpp PMADDH:
    //   LO.SL[0]  = LO.SL[0]  + a0; rd.SL[0] = LO.SL[0]; LO.SL[1] = a1; rd.SL[1] = a1
    //   HI.SL[0]  = HI.SL[0]  + a2; rd... hmm complex
    // Let me just verify the instruction fires (any output change = success)
    // With all 6's: rd changes from 0. Check at least UD[0] != 0.
    runGPR128Test("pmaddh: halfword multiply-accumulate", prog, 3,
        { {1, 0x0002000200020002ULL, 0x0002000200020002ULL},
          {2, 0x0003000300030003ULL, 0x0003000300030003ULL} },
        { {3, 0x0000000600000006ULL, 0x0000000600000006ULL} });
}

// PMSUBH: HI:LO -= halfword pairs
// preset LO=100 for each slot; sub 2*3=6 → 94
static void test_pmsubh()
{
    static const u32 prog[] = { pmtlo_m(4), pmthi_m(4), pmsubh(3,1,2), HALT }; // preset both LO and HI
    runGPR128Test("pmsubh: halfword multiply-subtract", prog, 4,
        { {4, 0x0000006400000064ULL, 0x0000006400000064ULL},
          {1, 0x0002000200020002ULL, 0x0002000200020002ULL},
          {2, 0x0003000300030003ULL, 0x0003000300030003ULL} },
        { {3, 0x0000005E0000005EULL, 0x0000005E0000005EULL} }); // 100-6=94
}

// PHMADH: horizontal multiply-add halfwords (consecutive pairs summed)
// rs.SS={1,2,3,4,...}, rt.SS={10,10,...}
// a0=1*10+2*10=30, a1=3*10+4*10=70, ...
static void test_phmadh()
{
    static const u32 prog[] = { phmadh(3,1,2), HALT };
    runGPR128Test("phmadh: horizontal halfword madd", prog, 3,
        { {1, 0x0004000300020001ULL, 0x0008000700060005ULL},
          {2, 0x000A000A000A000AULL, 0x000A000A000A000AULL} },
        { {3, 0x000000460000001EULL, 0x000000960000006EULL} });
    // a0=(1+2)*10=30=0x1E, a1=(3+4)*10=70=0x46, a2=(5+6)*10=110=0x6E, a3=(7+8)*10=150=0x96
    // rd.SL[0]=a0=30, rd.SL[1]=a1=70, rd.SL[2]=a2=110, rd.SL[3]=a3=150
    // rd.UD[0]=(70<<32)|30=0x000000460000001EULL
    // rd.UD[1]=(150<<32)|110=0x00000096_0000006EULL
}

// PHMSBH: horizontal multiply-subtract halfwords
// a0=1*10-2*10=-10, ...
static void test_phmsbh()
{
    static const u32 prog[] = { phmsbh(3,1,2), HALT };
    // a0=SS[1]*10-SS[0]*10=2*10-1*10=10, a1=SS[3]*10-SS[2]*10=4*10-3*10=10, etc.
    runGPR128Test("phmsbh: horizontal halfword msub", prog, 3,
        { {1, 0x0004000300020001ULL, 0x0008000700060005ULL},
          {2, 0x000A000A000A000AULL, 0x000A000A000A000AULL} },
        { {3, 0x0000000A0000000AULL, 0x0000000A0000000AULL} }); // SS[n+1]*SS[n+1] - SS[n]*SS[n] = 20-10=10
}

// ──────────────────────────────────────────────────────────────────────────────
// MMI3 gaps: PNOR, PSRAVW, PMTHI/PMTLO, PINTEH, PMULTUW, PDIVUW, PMADDUW,
//            PEXCH, PEXCW
// ──────────────────────────────────────────────────────────────────────────────

// PNOR: rd = ~(rs | rt) (128-bit)
static void test_pnor()
{
    static const u32 prog[] = { pnor(3,1,2), HALT };
    runGPR128Test("pnor: ~(rs|rt) 128-bit", prog, 2,
        { {1, 0xAAAAAAAAAAAAAAAAULL, 0xAAAAAAAAAAAAAAAAULL},
          {2, 0x5555555555555555ULL, 0x5555555555555555ULL} },
        { {3, 0x0000000000000000ULL, 0x0000000000000000ULL} }); // ~(AAAA|5555) = ~FFFF = 0
}

// PSRAVW: arithmetic right shift variable (from MMI3)
// rs.UL[0]=2 (shift), rt.SL[0]=0x80000000(-2147483648) → -2147483648>>2=-536870912=0xE0000000
// sext to SL[1]: 0xFFFFFFFFE0000000
static void test_psravw()
{
    static const u32 prog[] = { psravw(3,1,2), HALT };
    runGPR128Test("psravw: arithmetic right shift", prog, 3,
        { {1, 0x0000000000000002ULL, 0x0000000200000002ULL}, // rs: shift=2
          {2, 0x8000000080000000ULL, 0x8000000080000000ULL} }, // rt: -2147483648
        { {3, 0xFFFFFFFFE0000000ULL, 0xFFFFFFFFE0000000ULL} }); // -2147483648>>2 sext
}

// PINTEH: interleave even halfwords (US[0],US[2],US[4],US[6] from rs and rt)
// rd.US={rt[0],rs[0], rt[2],rs[2], rt[4],rs[4], rt[6],rs[6]}
static void test_pinteh()
{
    static const u32 prog[] = { pinteh(3,1,2), HALT };
    // rs.US even positions = 0xAAAA, rs.US odd = 0; rs.UD[0]=0x0000AAAA0000AAAA
    // rt.US even positions = 0xBBBB; rt.UD[0]=0x0000BBBB0000BBBB
    // rd.US = {BB,AA,BB,AA,BB,AA,BB,AA}
    runGPR128Test("pinteh: interleave even halfwords", prog, 3,
        { {1, 0x0000AAAA0000AAAAULL, 0x0000AAAA0000AAAAULL},
          {2, 0x0000BBBB0000BBBBULL, 0x0000BBBB0000BBBBULL} },
        { {3, 0xAAAABBBBAAAABBBBULL, 0xAAAABBBBAAAABBBBULL} });
}

// PMULTUW: unsigned 64-bit multiply (2 pairs); rd.UD[dd]=full 64-bit product
// rs.UL[0]=3, rt.UL[0]=7 → 21; rs.UL[2]=4, rt.UL[2]=5 → 20
static void test_pmultuw()
{
    static const u32 prog[] = { pmultuw(3,1,2), HALT };
    runGPR128Test("pmultuw: unsigned word multiply", prog, 2,
        { {1, 0x0000000000000003ULL, 0x0000000000000004ULL}, // rs: UL[0]=3,UL[2]=4
          {2, 0x0000000000000007ULL, 0x0000000000000005ULL} }, // rt: UL[0]=7,UL[2]=5
        { {3, 0x0000000000000015ULL, 0x0000000000000014ULL} }); // 3*7=21, 4*5=20
}

// PDIVUW: unsigned LO=rs/rt, HI=rs%rt; read back with PMFLO_M
static void test_pdivuw()
{
    // rs.UL[0]=0xFFFFFFFE(4294967294), rt.UL[0]=3 → 4294967294/3=1431655764, rem=2
    static const u32 prog[] = { pdivuw(1,2), pmflo_m(3), HALT };
    runGPR128Test("pdivuw: unsigned word divide quotient", prog, 3,
        { {1, 0x0000001200000015ULL, 0x0000001200000015ULL}, // rs: 21 (0x15) and 18 (0x12)
          {2, 0x0000000400000004ULL, 0x0000000400000004ULL} }, // rt: 4
        { {3, 0x0000000000000005ULL, 0x0000000000000005ULL} }); // LO.SD=(s32)quotient; UL[1]=UL[3]=0
}

// PMADDUW: unsigned HI1:LO1 += rs.UL*rt.UL; rd=LO
// HI=LO=0, rs.UL[0]=3,rt.UL[0]=7 → LO=21; rs.UL[2]=4,rt.UL[2]=5 → LO=20
static void test_pmadduw()
{
    static const u32 prog[] = { pmadduw(3,1,2), HALT };
    runGPR128Test("pmadduw: unsigned word MADD", prog, 3,
        { {1, 0x0000000000000003ULL, 0x0000000400000004ULL},
          {2, 0x0000000000000007ULL, 0x0000000500000005ULL} },
        { {3, 0x0000000000000015ULL, 0x0000000000000014ULL} }); // rd.UD[dd]=full 64-bit product; UL[1]=UL[3]=0
}

// PEXCH: swap US[1]↔US[2] and US[5]↔US[6]
// rt.US = {1,2,3,4, 5,6,7,8} → rd.US = {1,3,2,4, 5,7,6,8}
static void test_pexch()
{
    static const u32 prog[] = { pexch(3,2), HALT };
    runGPR128Test("pexch: swap center halfwords", prog, 2,
        { {2, 0x0004000300020001ULL, 0x0008000700060005ULL} },
        { {3, 0x0004000200030001ULL, 0x0008000600070005ULL} });
}

// PEXCW: swap UL[1]↔UL[2]
// rt.UL = {1,2,3,4} → rd.UL = {1,3,2,4}
static void test_pexcw()
{
    static const u32 prog[] = { pexcw(3,2), HALT };
    runGPR128Test("pexcw: swap words 1 and 2", prog, 2,
        { {2, 0x0000000200000001ULL, 0x0000000400000003ULL} },
        { {3, 0x0000000300000001ULL, 0x0000000400000002ULL} });
}

// ──────────────────────────────────────────────────────────────────────────────
// Entry point
// ──────────────────────────────────────────────────────────────────────────────

void RunEeJitTests()
{
    s_pass = s_fail = 0;
    LOGI("=== EE interpreter tests start ===");

    EE_TestInit();

    // Arithmetic
    test_addiu_pos();
    test_addiu_neg();
    test_addiu_zero();
    test_addu_basic();
    test_addu_zero_dest();
    test_subu_basic();
    test_subu_underflow();
    test_lui();
    test_lui_ori();

    // Logical
    test_and_basic();
    test_or_basic();
    test_xor_basic();
    test_nor_basic();
    test_andi();
    test_ori();
    test_xori();

    // Shifts
    test_sll_basic();
    test_srl_basic();
    test_sra_positive();
    test_sra_negative();
    test_sllv();
    test_srlv();
    test_srav();

    // Compare
    test_slt_true();
    test_slt_false();
    test_slt_signed();
    test_sltu_unsigned();
    test_slti();
    test_sltiu();

    // HI/LO / Multiply / Divide
    test_mult_basic();
    test_mult_negative();
    test_multu_basic();
    test_div_basic();
    test_divu_basic();
    test_mthi_mfhi();
    test_mtlo_mflo();
    // Pipeline-2 multiply/divide
    test_mult1_basic();
    test_mult1_negative();
    test_multu1_basic();
    test_div1_basic();
    test_divu1_basic();
    test_mthi1_mtlo1();

    // Load / Store
    test_lw_sw();
    test_lw_positive();
    test_sw_check();
    test_lb_lbu();
    test_lh_lhu();
    test_sb_sh();
    test_ld_sd();
    test_ld_sd_64bit();
    // Unaligned word load/store
    test_lwl_full();
    test_lwl_partial();
    test_lwr_full();
    test_lwr_partial();
    test_lwr_lwl_unaligned();
    test_swr_full();
    test_swl_swr_unaligned();

    // Branches
    test_beq_taken();
    test_beq_not_taken();
    test_bne_taken();
    test_bne_not_taken();
    test_bgtz_taken();
    test_bgtz_not_taken();
    test_blez_taken();
    test_blez_not_taken();
    test_bltz_taken();
    test_bltz_not_taken();
    test_bgez_taken();
    test_bgez_not_taken();
    test_branch_delay_slot_executes();
    // Branch-likely
    test_beql_taken();
    test_beql_not_taken();
    test_bnel_taken();
    test_bgtzl_taken();
    test_blezl_taken();
    test_bltzl_not_taken();
    test_bgezl_taken();

    // Jump
    test_jr_basic();
    test_jalr_link();
    test_j_skip();
    test_jal_abs();

    // R5900 64-bit
    test_daddu_basic();
    test_daddiu_basic();
    test_dsll_basic();
    test_dsrl_basic();
    test_dsra_negative();
    test_dsubu_basic();
    test_dsubu_zero();
    test_dsra32_negative();
    test_dsra32_positive();
    test_dsllv();
    test_dsrlv();
    test_dsrav();

    // Zero-register guard
    test_r0_hardwired_zero();

    // COP1 / FPU
    test_mtc1_mfc1();
    test_add_s_normal();
    test_add_s_overflow();
    test_add_s_denorm_flush();
    test_add_s_inf_clamp();
    test_sub_s_normal();
    test_sub_s_negative();
    test_mul_s_normal();
    test_mul_s_neg_neg();
    test_mul_s_underflow();
    test_div_s_normal();
    test_div_s_by_zero();
    test_div_s_negative();
    test_sqrt_s();
    test_sqrt_s_zero();
    test_sqrt_s_negative();
    test_abs_s();
    test_neg_s();
    test_mov_s();
    test_cvt_s_from_int();
    test_cvt_s_neg_int();
    test_cvt_w_trunc();
    test_cvt_w_clamp_pos();
    test_cvt_w_clamp_neg();
    test_c_eq_set();
    test_c_eq_clear();
    test_c_lt_set();
    test_c_lt_clear();
    test_c_le_equal();
    test_c_le_less();
    test_c_f_clears();
    test_bc1t_taken();
    test_bc1t_not_taken();
    test_bc1f_taken();
    test_bc1f_not_taken();
    test_bc1t_c_lt_taken();
    test_bc1t_c_lt_not_taken();
    test_bc1f_c_lt_taken();
    test_bc1t_c_le_boundary();
    test_bc1t_negative_coords();
    test_madd_s();
    test_adda_s();
    test_suba_s();
    test_mula_s();
    test_msub_s();
    test_madda_s();
    test_adda_madda_chain();
    test_mula_msub_chain();
    test_max_s_pos();
    test_min_s_neg();

    // COP2 / VU0 macro mode
    test_cop2_qmtc2_qmfc2();
    test_cop2_vadd_xyzw();
    test_cop2_vsub_xyzw();
    test_cop2_vmul_xyzw();
    test_cop2_vabs();
    test_cop2_vmove();
    test_cop2_vf0_immutable();
    test_cop2_vftoi0();
    test_cop2_vitof0();
    test_cop2_vdiv();
    test_cop2_vsqrt();
    test_cop2_vaddx_broadcast();
    test_cop2_viadd();
    test_cop2_matrix_vec_mul();
    test_cop2_vmulx_broadcast();
    test_cop2_vmaddy_broadcast();
    test_cop2_vmadd_full();
    test_cop2_vmsub_full();
    test_cop2_vmadda_full();
    test_cop2_vmula_full();
    test_cop2_vopmula();
    test_cop2_vopmsub();
    test_cop2_vopmula_vopmsub_chain();
    test_cop2_vmula_vmadda_chain();
    test_cop2_vmuly_broadcast();
    test_cop2_vmulz_broadcast();
    test_cop2_vmulw_broadcast();
    test_cop2_vmaddx_broadcast();
    test_cop2_vmaddz_broadcast();
    test_cop2_vmaddw_broadcast();
    test_cop2_vmulay_acc();
    test_cop2_vmulaz_acc();
    test_cop2_vmulaw_acc();
    test_cop2_vmaddax_acc();
    test_cop2_vmaddaw_acc();
    test_cop2_vnop();
    // Partial dest masks
    test_cop2_partial_dest_xyz();
    test_cop2_partial_dest_xy();
    test_cop2_partial_dest_x();
    test_cop2_partial_dest_w();
    // VADDbc / VSUBbc broadcasts
    test_cop2_vaddy_broadcast();
    test_cop2_vaddz_broadcast();
    test_cop2_vaddw_broadcast();
    test_cop2_vsubx_broadcast();
    test_cop2_vsuby_broadcast();
    test_cop2_vsubz_broadcast();
    test_cop2_vsubw_broadcast();
    // VMSUBbc broadcasts
    test_cop2_vmsubx_broadcast();
    test_cop2_vmsuby_broadcast();
    test_cop2_vmsubz_broadcast();
    test_cop2_vmsubw_broadcast();
    // VMAXbc / VMINIbc + full-vector
    test_cop2_vmaxx_broadcast();
    test_cop2_vminix_broadcast();
    test_cop2_vmax_full();
    test_cop2_vmini_full();
    // VMULq / VMULAq
    test_cop2_vmulq();
    test_cop2_vmulaq();
    // Fixed-point conversions
    test_cop2_vitof4();
    test_cop2_vitof12();
    test_cop2_vitof15();
    test_cop2_vftoi4();
    test_cop2_vftoi12();
    test_cop2_vftoi15();
    test_cop2_vitof4_vftoi4_roundtrip();
    // VI integer ops
    test_cop2_visub();
    test_cop2_visub_wrap();
    test_cop2_viaddi_pos();
    test_cop2_viaddi_neg();
    test_cop2_viand();
    test_cop2_vior();
    // VADDAbc / VSUBAbc / VMSUBAbc
    test_cop2_vaddax_acc();
    test_cop2_vadday_acc();
    test_cop2_vaddaz_acc();
    test_cop2_vaddaw_acc();
    test_cop2_vsubax_acc();
    test_cop2_vsubay_acc();
    test_cop2_vsubaz_acc();
    test_cop2_vsubaw_acc();
    test_cop2_vmsubax_acc();
    test_cop2_vmsubay_acc();
    test_cop2_vmsubaz_acc();
    test_cop2_vmsubaw_acc();
    // VADDA / VSUBA / VMSUBA full-vector
    test_cop2_vadda_full();
    test_cop2_vsuba_full();
    test_cop2_vmsuba_full();
    // COP2 chain tests
    test_cop2_vadda_vmadda_chain();
    test_cop2_vmula_vmadda_vmadd_chain();
    test_cop2_vi_chain();

    // Upper 128-bit GPR half (MMI parallel ops + LQ/SQ)
    test_paddw_upper_half();
    test_psubw_upper_half();
    test_pcpyld_pack_halves();
    test_pcpyud_extract_upper();
    test_por_full128();
    test_pand_full128();
    test_lq_loads_upper_half();
    test_sq_stores_upper_half();
    // MMI / EE-specific
    test_madd_ee();
    test_plzcw_positive();
    test_plzcw_negative();
    test_psllh();
    test_psrlh();
    test_psrah();
    test_psllw();
    test_psrlw();
    test_psraw();
    test_paddh();
    test_psubh();
    test_psubb();
    test_pcgtw();
    test_pmaxw();
    test_pminw();
    test_pabsw();
    test_pceqw();
    test_pextlw();
    test_ppacw();
    test_pextlh();
    test_ppach();
    test_pcpyh();
    // Saturating parallel arithmetic
    test_paddsw_sat_pos();
    test_paddsw_sat_neg();
    test_psubsw_sat_neg();
    test_paddsh_sat_pos();
    test_psubsh_sat_neg();

    // R5900 base ISA gaps
    test_dsll32_basic();
    test_dsll32_sa1();
    test_dsrl32_basic();
    test_lwu_zero_extend();
    test_bltzal_taken();
    test_bltzal_not_taken();
    test_bgezal_taken();
    test_maddu_basic();
    test_bc1tl_taken();
    test_bc1tl_not_taken();
    test_bc1fl_taken();
    test_cfc1_read();
    test_ctc1_write();

    // MMI0 gaps
    test_pcgth();
    test_pmaxh();
    test_paddb();
    test_pcgtb();
    test_paddsb();
    test_psubsb();
    test_pextlb();
    test_ppacb();

    // MMI1 gaps
    test_pabsh();
    test_pceqh();
    test_pminh();
    test_pceqb();
    test_padduw();
    test_psubuw();
    test_pextuw();
    test_padduh();
    test_psubuh();
    test_pextuh();
    test_paddub();
    test_psubub();
    test_pextub();
    test_padsbh();

    // MMI2 gaps
    test_pxor();
    test_pmfhi_m();
    test_pmflo_m();
    test_pinth();
    test_psllvw();
    test_psrlvw();
    test_pexeh();
    test_prevh();
    test_pexew();
    test_prot3w();
    test_pmulth();
    test_pmaddw();
    test_pmsubw();
    test_pmultw();
    test_pdivw();
    test_pmaddh();
    test_pmsubh();
    test_phmadh();
    test_phmsbh();

    // MMI3 gaps
    test_pnor();
    test_psravw();
    test_pinteh();
    test_pmultuw();
    test_pdivuw();
    test_pmadduw();
    test_pexch();
    test_pexcw();

    LOGI("=== EE interpreter: %d/%d passed ===", s_pass, s_pass + s_fail);
    ReportTestResults("EeJitTests", s_pass, s_pass + s_fail);

    EE_TestShutdown();
}
