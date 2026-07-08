// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "run_ee_seq_tests.h"
#include "ee_test_api.h"
#include "../test_bridge.h"

#include <android/log.h>
#include <cstring>
#include <initializer_list>

#include "pcsx2/R5900.h"
#include "pcsx2/MemoryTypes.h"

#define TAG "EeSeqTests"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,  TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// ──────────────────────────────────────────────────────────────────────────────
// Instruction encoders
// ──────────────────────────────────────────────────────────────────────────────
static constexpr u32 HALT = 0x1000FFFFu; // BEQ $0,$0,-1
static constexpr u32 NOP  = 0x00000000u; // SLL $0,$0,0

static constexpr u32 r_type(u32 funct, u32 rd, u32 rs, u32 rt, u32 sa = 0)
{ return (rs<<21)|(rt<<16)|(rd<<11)|(sa<<6)|funct; }
static constexpr u32 i_type(u32 opcode, u32 rt, u32 rs, u32 imm16)
{ return (opcode<<26)|(rs<<21)|(rt<<16)|(imm16&0xFFFFu); }

// Arithmetic
static constexpr u32 addiu (u32 rt, u32 rs, s16 imm) { return i_type(0x09,rt,rs,(u16)imm); }
static constexpr u32 addu  (u32 rd, u32 rs, u32 rt)  { return r_type(0x21,rd,rs,rt); }
static constexpr u32 daddiu(u32 rt, u32 rs, s16 imm) { return i_type(0x19,rt,rs,(u16)imm); }
// Logical
static constexpr u32 andi  (u32 rt, u32 rs, u16 imm) { return i_type(0x0C,rt,rs,imm); }
static constexpr u32 ori   (u32 rt, u32 rs, u16 imm) { return i_type(0x0D,rt,rs,imm); }
// LUI
static constexpr u32 lui   (u32 rt, u16 imm)         { return i_type(0x0F,rt,0,imm); }
// Shifts
static constexpr u32 sll   (u32 rd, u32 rt, u32 sa)  { return r_type(0x00,rd,0,rt,sa); }
static constexpr u32 srl   (u32 rd, u32 rt, u32 sa)  { return r_type(0x02,rd,0,rt,sa); }
static constexpr u32 dsll  (u32 rd, u32 rt, u32 sa)  { return r_type(0x38,rd,0,rt,sa); }
static constexpr u32 dsrl  (u32 rd, u32 rt, u32 sa)  { return r_type(0x3A,rd,0,rt,sa); }
static constexpr u32 dsll32(u32 rd, u32 rt, u32 sa)  { return r_type(0x3C,rd,0,rt,sa); } // shift by sa+32
static constexpr u32 dsrl32(u32 rd, u32 rt, u32 sa)  { return r_type(0x3E,rd,0,rt,sa); } // shift by sa+32
// Compare
static constexpr u32 slt   (u32 rd, u32 rs, u32 rt)  { return r_type(0x2A,rd,rs,rt); }
// Multiply / Divide
static constexpr u32 mult  (u32 rs, u32 rt)          { return r_type(0x18,0,rs,rt); }
static constexpr u32 div_  (u32 rs, u32 rt)          { return r_type(0x1A,0,rs,rt); }
static constexpr u32 mfhi  (u32 rd)                  { return r_type(0x10,rd,0,0); }
static constexpr u32 mflo  (u32 rd)                  { return r_type(0x12,rd,0,0); }
static constexpr u32 mtlo  (u32 rs)                  { return r_type(0x13,0,rs,0); }
// MMI
static constexpr u32 mmi_r(u32 rs,u32 rt,u32 rd,u32 sa,u32 func)
{ return (0x1Cu<<26)|(rs<<21)|(rt<<16)|(rd<<11)|(sa<<6)|func; }
static constexpr u32 madd_ee (u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd, 0, 0); }
static constexpr u32 pmaddw  (u32 rd,u32 rs,u32 rt) { return mmi_r(rs,rt,rd, 0, 9); }
static constexpr u32 pmflo_m (u32 rd)               { return mmi_r( 0, 0,rd, 9, 9); }
// Load / Store
static constexpr u32 lw  (u32 rt, u32 rs, s16 off)  { return i_type(0x23,rt,rs,(u16)off); }
static constexpr u32 sw  (u32 rt, u32 rs, s16 off)  { return i_type(0x2B,rt,rs,(u16)off); }
static constexpr u32 lb  (u32 rt, u32 rs, s16 off)  { return i_type(0x20,rt,rs,(u16)off); }
static constexpr u32 lbu (u32 rt, u32 rs, s16 off)  { return i_type(0x24,rt,rs,(u16)off); }
static constexpr u32 lh  (u32 rt, u32 rs, s16 off)  { return i_type(0x21,rt,rs,(u16)off); }
static constexpr u32 sh  (u32 rt, u32 rs, s16 off)  { return i_type(0x29,rt,rs,(u16)off); }
static constexpr u32 sb  (u32 rt, u32 rs, s16 off)  { return i_type(0x28,rt,rs,(u16)off); }
static constexpr u32 ld  (u32 rt, u32 rs, s16 off)  { return i_type(0x37,rt,rs,(u16)off); }
static constexpr u32 sd  (u32 rt, u32 rs, s16 off)  { return i_type(0x3F,rt,rs,(u16)off); }
// Branches (offset in words relative to delay-slot PC)
static constexpr u32 beq (u32 rs, u32 rt, s16 off)  { return i_type(0x04,rt,rs,(u16)off); }
static constexpr u32 bne (u32 rs, u32 rt, s16 off)  { return i_type(0x05,rt,rs,(u16)off); }
static constexpr u32 bgtz(u32 rs, s16 off)           { return i_type(0x07,0,rs,(u16)off); }
static constexpr u32 blez(u32 rs, s16 off)           { return i_type(0x06,0,rs,(u16)off); }
static constexpr u32 bltz(u32 rs, s16 off)           { return i_type(0x01,0,rs,(u16)off); }
// Jumps
static constexpr u32 jr  (u32 rs)                    { return r_type(0x08,0,rs,0); }
// COP1 (FPU)
static constexpr u32 cop1_s(u32 func,u32 fd,u32 fs,u32 ft)
{ return (0x11u<<26)|(0x10u<<21)|(ft<<16)|(fs<<11)|(fd<<6)|func; }
static constexpr u32 cop1_w(u32 func,u32 fd,u32 fs)
{ return (0x11u<<26)|(0x14u<<21)|(0<<16)|(fs<<11)|(fd<<6)|func; }
static constexpr u32 bc1t  (s16 off)                { return (0x11u<<26)|(0x08u<<21)|(1u<<16)|((u16)off); }
static constexpr u32 bc1f  (s16 off)                { return (0x11u<<26)|(0x08u<<21)|(0u<<16)|((u16)off); }
static constexpr u32 add_s (u32 fd,u32 fs,u32 ft)   { return cop1_s(0x00,fd,fs,ft); }
static constexpr u32 mul_s (u32 fd,u32 fs,u32 ft)   { return cop1_s(0x02,fd,fs,ft); }
static constexpr u32 mula_s(u32 fs,u32 ft)           { return cop1_s(0x1A,0,fs,ft); }
static constexpr u32 madd_s(u32 fd,u32 fs,u32 ft)   { return cop1_s(0x1C,fd,fs,ft); }
static constexpr u32 c_lt  (u32 fs,u32 ft)           { return cop1_s(0x34,0,fs,ft); }
static constexpr u32 c_eq  (u32 fs,u32 ft)           { return cop1_s(0x32,0,fs,ft); }

// EE_TEST_DATA = 0x00200000 → LUI $rd, 0x0020
static constexpr u32 load_data_addr(u32 rd) { return lui(rd, (u16)(EE_TEST_DATA >> 16)); }

// FP constants
static constexpr u32 F32_1_0 = 0x3F800000u; // 1.0
static constexpr u32 F32_2_0 = 0x40000000u; // 2.0
static constexpr u32 F32_3_0 = 0x40400000u; // 3.0
static constexpr u32 F32_4_0 = 0x40800000u; // 4.0
static constexpr u32 F32_5_0 = 0x40A00000u; // 5.0
static constexpr u32 F32_7_0 = 0x40E00000u; // 7.0
static constexpr u32 F32_26_0= 0x41D00000u; // 26.0
static constexpr u32 F32_28_0= 0x41E00000u; // 28.0
static constexpr u32 FCR31_C = 0x00800000u;

// ──────────────────────────────────────────────────────────────────────────────
// Test state and helpers
// ──────────────────────────────────────────────────────────────────────────────
static int s_pass, s_fail;
static constexpr int REG_HI = 32;
static constexpr int REG_LO = 33;

struct GPRPreset { int reg; u64 val; };
struct GPRExpect { int reg; u64 val; };
struct MemPreset { u32 offset; u32 val; };
struct MemExpect { u32 offset; u32 val; };
struct FPRPreset { int reg; u32 val; };
struct FPRExpect { int reg; u32 val; };
static constexpr int FPR_ACC   = 32;
static constexpr int FPR_FCR31 = 33;

// 64-bit GPR test (also handles HI/LO via REG_HI/REG_LO sentinels)
static bool runGPRTest(const char* name,
                       const u32* words, u32 nwords,
                       std::initializer_list<GPRPreset> presets,
                       std::initializer_list<GPRExpect> expects)
{
    EE_TestWriteProg(words, nwords);
    std::memset(&cpuRegs.GPR, 0, sizeof(cpuRegs.GPR));
    cpuRegs.HI.UD[0] = 0; cpuRegs.HI.UD[1] = 0;
    cpuRegs.LO.UD[0] = 0; cpuRegs.LO.UD[1] = 0;

    for (auto& p : presets) {
        if      (p.reg == REG_HI) cpuRegs.HI.UD[0] = p.val;
        else if (p.reg == REG_LO) cpuRegs.LO.UD[0] = p.val;
        else                      cpuRegs.GPR.r[p.reg].UD[0] = p.val;
    }
    EE_TestExec();

    bool ok = true;
    for (auto& e : expects) {
        u64 got;
        if      (e.reg == REG_HI) got = cpuRegs.HI.UD[0];
        else if (e.reg == REG_LO) got = cpuRegs.LO.UD[0];
        else                      got = cpuRegs.GPR.r[e.reg].UD[0];

        if (got != e.val) {
            const char* rn = (e.reg == REG_HI) ? "HI" :
                             (e.reg == REG_LO) ? "LO" : "GPR";
            int ri = (e.reg == REG_HI || e.reg == REG_LO) ? 0 : e.reg;
            LOGE("  FAIL %s: %s[%d] exp=0x%016llX got=0x%016llX",
                 name, rn, ri,
                 (unsigned long long)e.val, (unsigned long long)got);
            ok = false;
        }
    }
    if (ok) { ++s_pass; LOGI("  PASS %s", name); }
    else    { ++s_fail; }
    return ok;
}

// Memory + GPR test
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
    std::memset(eeMem->Main + EE_TEST_DATA, 0, 256);

    for (auto& p : gpr_in)
        cpuRegs.GPR.r[p.reg].UD[0] = p.val;
    for (auto& m : mem_in)
        *(u32*)(eeMem->Main + EE_TEST_DATA + m.offset) = m.val;

    EE_TestExec();

    bool ok = true;
    for (auto& e : gpr_out) {
        u64 got = cpuRegs.GPR.r[e.reg].UD[0];
        if (got != e.val) {
            LOGE("  FAIL %s: GPR[%d] exp=0x%016llX got=0x%016llX",
                 name, e.reg,
                 (unsigned long long)e.val, (unsigned long long)got);
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

// FPU test
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
    fpuRegs.ACC.UL   = 0;
    fpuRegs.fprc[31] = 0;

    for (auto& p : fpr_in) {
        if      (p.reg == FPR_ACC)   fpuRegs.ACC.UL       = p.val;
        else if (p.reg == FPR_FCR31) fpuRegs.fprc[31]      = p.val;
        else                         fpuRegs.fpr[p.reg].UL = p.val;
    }
    EE_TestExec();

    bool ok = true;
    for (auto& e : fpr_out) {
        u32 got;
        if      (e.reg == FPR_ACC)   got = fpuRegs.ACC.UL;
        else if (e.reg == FPR_FCR31) got = fpuRegs.fprc[31] & FCR31_C;
        else                         got = fpuRegs.fpr[e.reg].UL;
        if (got != e.val) {
            const char* rn = (e.reg == FPR_ACC) ? "ACC" :
                             (e.reg == FPR_FCR31) ? "FCR31.C" : "FPR";
            LOGE("  FAIL %s: %s[%d] exp=0x%08X got=0x%08X",
                 name, rn, (e.reg >= FPR_ACC) ? 0 : e.reg, e.val, got);
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

// 128-bit GPR test
struct GPR128Preset { int reg; u64 lo, hi; };
struct GPR128Expect { int reg; u64 lo, hi; };

static bool runGPR128Test(const char* name,
                          const u32* words, u32 nwords,
                          std::initializer_list<GPR128Preset> presets,
                          std::initializer_list<GPR128Expect> expects)
{
    EE_TestWriteProg(words, nwords);
    std::memset(&cpuRegs.GPR, 0, sizeof(cpuRegs.GPR));
    cpuRegs.HI.UD[0] = cpuRegs.HI.UD[1] = 0;
    cpuRegs.LO.UD[0] = cpuRegs.LO.UD[1] = 0;

    for (auto& p : presets) {
        cpuRegs.GPR.r[p.reg].UD[0] = p.lo;
        cpuRegs.GPR.r[p.reg].UD[1] = p.hi;
    }
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
    if (ok) { ++s_pass; LOGI("  PASS %s", name); }
    else    { ++s_fail; }
    return ok;
}

// ──────────────────────────────────────────────────────────────────────────────
// GROUP 1 — Register forwarding / ALU chains
// ──────────────────────────────────────────────────────────────────────────────

// Three-deep addiu dependency chain: $1=3 → $2=$1+4=7 → $3=$2+5=12
static void seq_alu_chain3()
{
    static const u32 prog[] = {
        addiu(1,0,3), addiu(2,1,4), addiu(3,2,5), HALT
    };
    runGPRTest("seq_alu_chain3", prog, 4, {}, {{3, 12}});
}

// Shift chain: $1=8, SLL $2,$1,2=32, SRL $3,$2,1=16
static void seq_alu_shift_chain()
{
    static const u32 prog[] = {
        addiu(1,0,8), sll(2,1,2), srl(3,2,1), HALT
    };
    runGPRTest("seq_alu_shift_chain", prog, 4, {}, {{3, 16}});
}

// LUI → ORI → ANDI: isolate low byte
static void seq_lui_ori_andi()
{
    // $1 = 0x00015678 → ANDI $2,$1,0xFF → $2=0x78
    static const u32 prog[] = {
        lui(1,1), ori(1,1,0x5678), andi(2,1,0xFF), HALT
    };
    runGPRTest("seq_lui_ori_andi", prog, 4, {}, {{2, 0x78}});
}

// ──────────────────────────────────────────────────────────────────────────────
// GROUP 2 — Load / Store round-trips
// ──────────────────────────────────────────────────────────────────────────────

// SW → LW round-trip (positive word)
static void seq_sw_lw()
{
    // $2=0x12345678, SW $2,0($1), LW $3,0($1) → $3=0x12345678
    static const u32 prog[] = {
        load_data_addr(1), sw(2,1,0), lw(3,1,0), HALT
    };
    runGPRTest("seq_sw_lw", prog, 4,
        {{2, 0x12345678}},
        {{3, 0x12345678}});
}

// SH → LH: negative halfword sign-extends to 64 bits
static void seq_sh_lh_neg()
{
    // $2=0xFFFF (s16=-1), SH→LH → $3=0xFFFFFFFFFFFFFFFF
    static const u32 prog[] = {
        load_data_addr(1), sh(2,1,0), lh(3,1,0), HALT
    };
    runGPRTest("seq_sh_lh_neg", prog, 4,
        {{2, 0xFFFF}},
        {{3, 0xFFFFFFFFFFFFFFFFull}});
}

// SB → LBU: unsigned byte zero-extends
static void seq_sb_lbu()
{
    // $2=0xAB, SB→LBU → $3=0xAB (zero extended)
    static const u32 prog[] = {
        load_data_addr(1), sb(2,1,0), lbu(3,1,0), HALT
    };
    runGPRTest("seq_sb_lbu", prog, 4,
        {{2, 0xAB}},
        {{3, 0xAB}});
}

// SD → LD: 64-bit round-trip preserves full value
static void seq_sd_ld()
{
    // $2=0xDEADBEEFCAFEBABE, SD→LD → $3=same
    static const u32 prog[] = {
        load_data_addr(1), sd(2,1,0), ld(3,1,0), HALT
    };
    runGPRTest("seq_sd_ld", prog, 4,
        {{2, 0xDEADBEEFCAFEBABEull}},
        {{3, 0xDEADBEEFCAFEBABEull}});
}

// Three independent SW+LW pairs: verify no aliasing between offsets 0, 4, 8
static void seq_sw_multi_no_alias()
{
    static const u32 prog[] = {
        load_data_addr(10),
        sw(1,10,0), sw(2,10,4), sw(3,10,8),
        lw(4,10,0), lw(5,10,4), lw(6,10,8),
        HALT
    };
    runGPRTest("seq_sw_multi_no_alias", prog, 8,
        {{1, 0xAA}, {2, 0xBB}, {3, 0xCC}},
        {{4, 0xAA}, {5, 0xBB}, {6, 0xCC}});
}

// SW to offset +8 only; LW from offset 0 reads back 0 (no pollution).
// Uses runMemTest to guarantee a clean data region before execution.
static void seq_sw_offset_isolation()
{
    static const u32 prog[] = {
        load_data_addr(1), sw(2,1,8), lw(3,1,0), HALT
    };
    runMemTest("seq_sw_offset_isolation", prog, 4,
        {{2, 0xDEAD}}, {},          // GPR preset; memory pre-zeroed by runMemTest
        {{3, 0}},                   // LW from offset 0 → 0 (untouched)
        {{8, 0xDEAD}});             // SW to offset 8 landed correctly
}

// ──────────────────────────────────────────────────────────────────────────────
// GROUP 3 — HI:LO accumulator chains
// ──────────────────────────────────────────────────────────────────────────────

// MULT small product: 6×7=42; HI=0, LO=42
static void seq_mult_mfhi_mflo()
{
    static const u32 prog[] = {
        addiu(1,0,6), addiu(2,0,7),
        mult(1,2),
        mfhi(3), mflo(4),
        HALT
    };
    runGPRTest("seq_mult_mfhi_mflo", prog, 6,
        {},
        {{3, 0}, {4, 42}});
}

// MULT overflow into HI: LUI(0x8000) × 2 → LO=0, HI=-1
static void seq_mult_hi_overflow()
{
    // $1.SL[0]=-2147483648, $2=2; product=-4294967296
    // LO=(s32)(0)=0, HI=(s32)(0xFFFFFFFF)=-1 → HI.UD[0]=0xFFFFFFFFFFFFFFFF
    static const u32 prog[] = {
        lui(1,0x8000), addiu(2,0,2),
        mult(1,2),
        mflo(3), mfhi(4),
        HALT
    };
    runGPRTest("seq_mult_hi_overflow", prog, 6,
        {},
        {{3, 0}, {4, 0xFFFFFFFFFFFFFFFFull}});
}

// DIV: quotient in LO, remainder in HI
static void seq_div_quot_rem()
{
    // 17/5 = quotient 3, remainder 2
    static const u32 prog[] = {
        addiu(1,0,17), addiu(2,0,5),
        div_(1,2),
        mflo(3), mfhi(4),
        HALT
    };
    runGPRTest("seq_div_quot_rem", prog, 6,
        {},
        {{3, 3}, {4, 2}});
}

// MADD accumulation: LO preset to 10, MADD 3×4=12 → LO=22
static void seq_madd_accumulate()
{
    static const u32 prog[] = {
        addiu(1,0,10),
        mtlo(1),
        addiu(2,0,3), addiu(3,0,4),
        madd_ee(0,2,3),
        mflo(4),
        HALT
    };
    runGPRTest("seq_madd_accumulate", prog, 7,
        {},
        {{4, 22}});
}

// ──────────────────────────────────────────────────────────────────────────────
// GROUP 4 — Branch sequences
// ──────────────────────────────────────────────────────────────────────────────

// BEQ taken: $1=$2=5 → branches to $3=99, not fall-through $3=42
static void seq_beq_taken()
{
    // beq at prog[2]; delay_slot=prog[3]; target=prog[3]+3*4=prog[6]
    static const u32 prog[] = {
        addiu(1,0,5), addiu(2,0,5),
        beq(1,2,3),             // offset 3 → target=delay_slot+12=prog[6]
        NOP,                    // delay slot
        addiu(3,0,42), HALT,    // fall-through (not reached)
        addiu(3,0,99), HALT,    // branch target
    };
    runGPRTest("seq_beq_taken", prog, 8, {}, {{3, 99}});
}

// BEQ not taken: $1=5, $2=6 → delay slot $3=10 runs; branch target not reached
static void seq_beq_not_taken()
{
    // beq not taken; delay slot always runs
    static const u32 prog[] = {
        addiu(1,0,5), addiu(2,0,6),
        beq(1,2,3),             // not taken
        addiu(3,0,10),          // delay slot: always runs → $3=10
        HALT,                   // fall-through
        NOP, addiu(3,0,99), HALT, // branch target (not reached)
    };
    runGPRTest("seq_beq_not_taken", prog, 8, {}, {{3, 10}});
}

// Counted loop: $1 counts 0→10 via BNE
static void seq_loop_count_up()
{
    // prog[0]:addiu $1,0,0  [P]
    // prog[1]:addiu $2,0,10 [P+4]
    // prog[2]:addiu $1,$1,1 [P+8]  ← loop back here
    // prog[3]:bne $1,$2,-2  [P+12] → target=delay_slot+(-2*4)=P+16-8=P+8 ✓
    // prog[4]:NOP            [P+16] delay slot
    // prog[5]:HALT           [P+20]
    static const u32 prog[] = {
        addiu(1,0,0), addiu(2,0,10),
        addiu(1,1,1),
        bne(1,2,-2),
        NOP,
        HALT,
    };
    runGPRTest("seq_loop_count_up", prog, 6, {}, {{1, 10}});
}

// Countdown loop: $1 counts 5→0 via BGTZ
static void seq_bgtz_countdown()
{
    // prog[0]:addiu $1,0,5  [P]
    // prog[1]:addiu $1,$1,-1 [P+4]  ← loop back here
    // prog[2]:bgtz $1,-2    [P+8]  → target=delay_slot+(-2*4)=P+12-8=P+4 ✓
    // prog[3]:NOP            [P+12] delay slot
    // prog[4]:HALT           [P+16]
    static const u32 prog[] = {
        addiu(1,0,5),
        addiu(1,1,-1),
        bgtz(1,-2),
        NOP,
        HALT,
    };
    runGPRTest("seq_bgtz_countdown", prog, 5, {}, {{1, 0}});
}

// Delay slot always runs even when branch is taken; fall-through not executed
static void seq_delay_slot_always_runs()
{
    // beq($0,$0) always taken; delay slot sets $2=5; fall-through sets $3=7 (not reached)
    // beq at prog[0]: delay_slot=prog[1]; target=prog[1]+2*4=prog[3]
    static const u32 prog[] = {
        beq(0,0,2),             // always taken; target=delay_slot+8=prog[3]
        addiu(2,0,5),           // delay slot: always runs
        addiu(3,0,7),           // fall-through: never reached
        HALT,                   // branch target
    };
    runGPRTest("seq_delay_slot_always_runs", prog, 4, {}, {{2, 5}, {3, 0}});
}

// BLTZ taken; delay slot modifies branch register (decision already made)
static void seq_bltz_delay_modify_reg()
{
    // $1=-1 (negative) → BLTZ taken; delay slot: $1=100 (no effect on branch);
    // fall-through not reached; branch target: $3=99
    // bltz at prog[1]: delay_slot=prog[2]; target=delay_slot+3*4=prog[5]
    static const u32 prog[] = {
        addiu(1,0,-1),
        bltz(1,3),              // taken ($1=-1<0); target=delay_slot+12=prog[5]
        addiu(1,0,100),         // delay slot: $1=100 (branch already committed)
        addiu(3,0,0), HALT,     // fall-through (not reached)
        addiu(3,0,99), HALT,    // branch target
    };
    runGPRTest("seq_bltz_delay_modify_reg", prog, 7,
        {},
        {{1, 100}, {3, 99}});
}

// ──────────────────────────────────────────────────────────────────────────────
// GROUP 5 — 64-bit GPR sequences
// ──────────────────────────────────────────────────────────────────────────────

// DSLL → DSRL chain: 1 << 32 >> 16 = 0x10000
static void seq_dsll_dsrl_chain()
{
    static const u32 prog[] = {
        addiu(1,0,1),
        dsll32(2,1,0),          // $2 = 0x0000000100000000  (DSLL32 shifts by sa+32; sa=0 → shift 32)
        dsrl(3,2,16),           // $3 = 0x0000000000010000
        HALT,
    };
    runGPRTest("seq_dsll_dsrl_chain", prog, 4,
        {},
        {{3, 0x10000ULL}});
}

// DADDIU across 32-bit boundary: 0x7FFFFFFF + 1 = 0x80000000 (not sign-extended)
static void seq_daddiu_32bit_boundary()
{
    // LUI $1,0x7FFF → $1=0x7FFF0000; ORI $1,$1,0xFFFF → $1=0x7FFFFFFF
    // DADDIU $1,$1,1 → $1=0x80000000 (64-bit, no 32-bit sign extension)
    static const u32 prog[] = {
        lui(1,0x7FFF), ori(1,1,0xFFFF),
        daddiu(1,1,1),
        HALT,
    };
    runGPRTest("seq_daddiu_32bit_boundary", prog, 4,
        {},
        {{1, 0x0000000080000000ULL}});
}

// ──────────────────────────────────────────────────────────────────────────────
// GROUP 6 — FPU sequences
// ──────────────────────────────────────────────────────────────────────────────

// ADD.S → MUL.S chain: (3.0+4.0)*4.0 = 28.0
static void seq_fpu_add_mul_chain()
{
    static const u32 prog[] = {
        add_s(3,1,2),           // $f3 = 3.0+4.0 = 7.0
        mul_s(4,3,2),           // $f4 = 7.0*4.0 = 28.0
        HALT,
    };
    runFPUTest("seq_fpu_add_mul_chain", prog, 3,
        {{1, F32_3_0}, {2, F32_4_0}},
        {{4, F32_28_0}});
}

// MULA.S → MADD.S: ACC=2.0×3.0=6.0; $f5=ACC+4.0×5.0=26.0
static void seq_fpu_mula_madd()
{
    static const u32 prog[] = {
        mula_s(1,2),            // ACC = 2.0*3.0 = 6.0
        madd_s(5,3,4),          // $f5 = 6.0 + 4.0*5.0 = 26.0
        HALT,
    };
    runFPUTest("seq_fpu_mula_madd", prog, 3,
        {{1, F32_2_0}, {2, F32_3_0}, {3, F32_4_0}, {4, F32_5_0}},
        {{5, F32_26_0}});
}

// C.LT.S (1.0 < 2.0 → CC=1) → BC1T taken → GPR $1=1
static void seq_fpu_clt_bc1t_taken()
{
    // c_lt at prog[0]; bc1t at prog[1]; delay_slot=prog[2];
    // target=delay_slot+3*4=prog[5]
    static const u32 prog[] = {
        c_lt(1,2),              // CC = (1.0 < 2.0) = 1
        bc1t(3),                // taken; target=delay_slot+12=prog[5]
        NOP,                    // delay slot
        addiu(1,0,0), HALT,     // fall-through (not reached)
        addiu(1,0,1), HALT,     // branch target: $1=1
    };
    // Use GPR $1 to carry the result; FPR $f1=1.0, $f2=2.0
    // runFPUTest checks FPR but also accepts GPR expects
    runFPUTest("seq_fpu_clt_bc1t_taken", prog, 7,
        {{1, F32_1_0}, {2, F32_2_0}},
        {},
        {{1, 1}});
}

// C.EQ.S (3.0 == 3.0 → CC=1) → BC1F NOT taken (BC1F branches when CC=0)
static void seq_fpu_ceq_bc1f_not_taken()
{
    // cc=1 (equal), BC1F not taken → fall-through: GPR $1=5
    // bc1f at prog[1]; delay_slot=prog[2]; target=prog[5]
    static const u32 prog[] = {
        c_eq(1,2),              // CC = (3.0==3.0) = 1
        bc1f(3),                // NOT taken (CC=1); fall-through
        NOP,                    // delay slot
        addiu(1,0,5), HALT,     // fall-through: $1=5
        addiu(1,0,9), HALT,     // branch target (not reached)
    };
    runFPUTest("seq_fpu_ceq_bc1f_not_taken", prog, 7,
        {{1, F32_3_0}, {2, F32_3_0}},
        {},
        {{1, 5}});
}

// ──────────────────────────────────────────────────────────────────────────────
// GROUP 7 — MMI sequences
// ──────────────────────────────────────────────────────────────────────────────

// PMADDW × 2: each iteration adds 3×4=12 to LO; total LO=24 per lane
static void seq_pmaddw_chain()
{
    // rs=$1={UD[0]=3,UD[1]=3}, rt=$2={UD[0]=4,UD[1]=4}
    // After PMADDW×2: LO.UL[0]=24, LO.UL[2]=24; rd.UD[0]=24, rd.UD[1]=24
    static const u32 prog[] = {
        pmaddw(3,1,2),
        pmaddw(3,1,2),
        HALT,
    };
    runGPR128Test("seq_pmaddw_chain", prog, 3,
        {{1, 3ULL, 3ULL}, {2, 4ULL, 4ULL}},
        {{3, 24ULL, 24ULL}});
}

// ──────────────────────────────────────────────────────────────────────────────
// GROUP 8 — Control flow / compare chains
// ──────────────────────────────────────────────────────────────────────────────

// JR indirect: build target address, JR, verify landing
static void seq_jr_indirect()
{
    // prog[0]:lui $2,hi   → $2 = EE_TEST_PC + 24 (upper)
    // prog[1]:ori $2,$2,lo → $2 = EE_TEST_PC + 24 (target address = prog[6])
    // prog[2]:jr $2
    // prog[3]:NOP           delay slot
    // prog[4]:addiu $3,0,42 fall-through (not reached)
    // prog[5]:HALT
    // prog[6]:addiu $3,0,99 jump target
    // prog[7]:HALT
    static constexpr u16 tgt_hi = (u16)((EE_TEST_PC + 24) >> 16);
    static constexpr u16 tgt_lo = (u16)((EE_TEST_PC + 24) & 0xFFFF);
    static const u32 prog[] = {
        lui(2,tgt_hi), ori(2,2,tgt_lo),
        jr(2), NOP,
        addiu(3,0,42), HALT,
        addiu(3,0,99), HALT,
    };
    runGPRTest("seq_jr_indirect", prog, 8, {}, {{3, 99}});
}

// SLT chain → BNE: SLT(3<7)=1, SLT(7<7)=0, ADDU→1, BNE taken → $7=99
static void seq_slt_chain_branch()
{
    // slt $4,$1,$2 → $4=1; slt $5,$2,$2 → $5=0; addu $6,$4,$5 → $6=1
    // bne($6,$0,3) → taken; target=delay_slot+12=prog[9]
    static const u32 prog[] = {
        addiu(1,0,3), addiu(2,0,7),
        slt(4,1,2),             // $4=1 (3<7)
        slt(5,2,2),             // $5=0 (7 not < 7)
        addu(6,4,5),            // $6=1
        bne(6,0,3),             // taken; target=delay_slot+12=prog[9]
        NOP,                    // delay slot
        addiu(7,0,42), HALT,    // fall-through (not reached)
        addiu(7,0,99), HALT,    // branch target
    };
    runGPRTest("seq_slt_chain_branch", prog, 10, {}, {{6, 1}, {7, 99}});
}

// BLEZ not taken ($1=5>0) falls through; BLEZ taken ($1=-1≤0) branches
static void seq_blez_taken_not_taken()
{
    // BLEZ not taken: $1=5 > 0 → fall-through sets $3=1, then continues to second branch
    // BLEZ taken:     $2=-1 ≤ 0 → jumps past HALT to set $4=1
    // prog[2]: blez($1,2); delay_slot=prog[3]; target=prog[5]  (not taken → prog[4])
    // prog[5]: blez($2,2); delay_slot=prog[6]; target=prog[8]  (taken)
    static const u32 prog[] = {
        addiu(1,0,5), addiu(2,0,-1),    // prog[0,1]
        blez(1,2),                       // prog[2]: not taken ($1=5>0); target=prog[5]
        NOP,                             // prog[3]: delay slot
        addiu(3,0,1),                    // prog[4]: fall-through: $3=1
        blez(2,2),                       // prog[5]: taken ($2=-1≤0); target=prog[8]
        NOP,                             // prog[6]: delay slot
        HALT,                            // prog[7]: only reached if second blez not taken (wrong path)
        addiu(4,0,1), HALT,             // prog[8,9]: taken target: $4=1
    };
    runGPRTest("seq_blez_taken_not_taken", prog, 10, {}, {{3, 1}, {4, 1}});
}

// ──────────────────────────────────────────────────────────────────────────────
// Entry point
// ──────────────────────────────────────────────────────────────────────────────
void RunEeSeqTests()
{
    s_pass = s_fail = 0;
    LOGI("=== EE sequence tests start ===");

    EE_TestInit();

    // Group 1: Register forwarding / ALU chains
    seq_alu_chain3();
    seq_alu_shift_chain();
    seq_lui_ori_andi();

    // Group 2: Load / Store round-trips
    seq_sw_lw();
    seq_sh_lh_neg();
    seq_sb_lbu();
    seq_sd_ld();
    seq_sw_multi_no_alias();
    seq_sw_offset_isolation();

    // Group 3: HI:LO accumulator chains
    seq_mult_mfhi_mflo();
    seq_mult_hi_overflow();
    seq_div_quot_rem();
    seq_madd_accumulate();

    // Group 4: Branch sequences
    seq_beq_taken();
    seq_beq_not_taken();
    seq_loop_count_up();
    seq_bgtz_countdown();
    seq_delay_slot_always_runs();
    seq_bltz_delay_modify_reg();

    // Group 5: 64-bit GPR sequences
    seq_dsll_dsrl_chain();
    seq_daddiu_32bit_boundary();

    // Group 6: FPU sequences
    seq_fpu_add_mul_chain();
    seq_fpu_mula_madd();
    seq_fpu_clt_bc1t_taken();
    seq_fpu_ceq_bc1f_not_taken();

    // Group 7: MMI sequences
    seq_pmaddw_chain();

    // Group 8: Control flow / compare chains
    seq_jr_indirect();
    seq_slt_chain_branch();
    seq_blez_taken_not_taken();

    LOGI("=== EE sequence tests: %d/%d passed ===", s_pass, s_pass + s_fail);
    ReportTestResults("EeSeqTests", s_pass, s_pass + s_fail);

    EE_TestShutdown();
}
