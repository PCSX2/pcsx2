// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 (vixl MacroAssembler) codegen tests.
//
// Each test verifies the exact byte sequence emitted for a given
// armAsm.Xxx() call.  Expected bytes are little-endian A64 encodings.
//
// Call-site convention: production code writes "armAsm->Xxx(...)".
//                       tests write       "armAsm.Xxx(...)" — same operands.

#include "codegen_tests.h"
#include <gtest/gtest.h>

using namespace vixl::aarch64;

// ---------------------------------------------------------------------------
// Integer register moves
// ---------------------------------------------------------------------------
TEST(ArmCodegenTests, MOVTest)
{
	// MOV Xd, Xn  =>  ORR Xd, XZR, Xn  (sf=1, opc=01, 01010, shift=0, N=0)
	ARM_CODEGEN_TEST(armAsm.Mov(x0, x1),   "e0 03 01 aa");
	ARM_CODEGEN_TEST(armAsm.Mov(x1, x0),   "e1 03 00 aa");
	ARM_CODEGEN_TEST(armAsm.Mov(x2, x3),   "e2 03 03 aa");
	ARM_CODEGEN_TEST(armAsm.Mov(x19, x20), "f3 03 14 aa");
	// MOV Wd, Wn  =>  ORR Wd, WZR, Wn  (sf=0)
	ARM_CODEGEN_TEST(armAsm.Mov(w0, w1),   "e0 03 01 2a");
	ARM_CODEGEN_TEST(armAsm.Mov(w2, w3),   "e2 03 03 2a");
}

// ---------------------------------------------------------------------------
// Integer arithmetic — register form
// ---------------------------------------------------------------------------
TEST(ArmCodegenTests, ArithRegTest)
{
	// ADD x0, x1, x2  (shifted register, shift=LSL, amount=0)
	ARM_CODEGEN_TEST(armAsm.Add(x0, x1, x2), "20 00 02 8b");
	// ADD w0, w1, w2
	ARM_CODEGEN_TEST(armAsm.Add(w0, w1, w2), "20 00 02 0b");
	// SUB x0, x1, x2
	ARM_CODEGEN_TEST(armAsm.Sub(x0, x1, x2), "20 00 02 cb");
	// SUB w0, w1, w2
	ARM_CODEGEN_TEST(armAsm.Sub(w0, w1, w2), "20 00 02 4b");
	// AND x0, x1, x2
	ARM_CODEGEN_TEST(armAsm.And(x0, x1, x2), "20 00 02 8a");
	// ORR x0, x1, x2
	ARM_CODEGEN_TEST(armAsm.Orr(x0, x1, x2), "20 00 02 aa");
	// EOR x0, x1, x2
	ARM_CODEGEN_TEST(armAsm.Eor(x0, x1, x2), "20 00 02 ca");
	// SUBS (sets flags) — SUBS x0, x1, x2
	ARM_CODEGEN_TEST(armAsm.Subs(x0, x1, x2), "20 00 02 eb");
	// CMP x1, x2  =>  SUBS xzr, x1, x2
	ARM_CODEGEN_TEST(armAsm.Cmp(x1, x2), "3f 00 02 eb");
}

// ---------------------------------------------------------------------------
// Integer arithmetic — immediate form
// ---------------------------------------------------------------------------
TEST(ArmCodegenTests, ArithImmTest)
{
	// ADD x0, x1, #4   (imm12=4, shift=0)
	ARM_CODEGEN_TEST(armAsm.Add(x0, x1, 4),   "20 10 00 91");
	// ADD x0, x1, #1
	ARM_CODEGEN_TEST(armAsm.Add(x0, x1, 1),   "20 04 00 91");
	// SUB x0, x1, #4
	ARM_CODEGEN_TEST(armAsm.Sub(x0, x1, 4),   "20 10 00 d1");
	// SUB x0, x1, #1
	ARM_CODEGEN_TEST(armAsm.Sub(x0, x1, 1),   "20 04 00 d1");
	// ADD w0, w1, #4
	ARM_CODEGEN_TEST(armAsm.Add(w0, w1, 4),   "20 10 00 11");
}

// ---------------------------------------------------------------------------
// Move immediate
// ---------------------------------------------------------------------------
TEST(ArmCodegenTests, MovImmTest)
{
	// MOVZ x0, #0   =>  00 00 80 d2
	ARM_CODEGEN_TEST(armAsm.Mov(x0, uint64_t{0}),      "00 00 80 d2");
	// MOVZ x0, #1   =>  20 00 80 d2
	ARM_CODEGEN_TEST(armAsm.Mov(x0, uint64_t{1}),      "20 00 80 d2");
	// MOVZ x0, #0x1234  =>  80 46 82 d2
	ARM_CODEGEN_TEST(armAsm.Mov(x0, uint64_t{0x1234}), "80 46 82 d2");
	// MOVZ x0, #0xffff  =>  e0 ff 9f d2
	ARM_CODEGEN_TEST(armAsm.Mov(x0, uint64_t{0xffff}), "e0 ff 9f d2");
}

// ---------------------------------------------------------------------------
// Shift — immediate
// ---------------------------------------------------------------------------
TEST(ArmCodegenTests, ShiftImmTest)
{
	// LSL x0, x1, #4  =>  UBFM x0, x1, #60, #59
	ARM_CODEGEN_TEST(armAsm.Lsl(x0, x1, 4), "20 ec 7c d3");
	// LSR x0, x1, #4  =>  UBFM x0, x1, #4, #63
	ARM_CODEGEN_TEST(armAsm.Lsr(x0, x1, 4), "20 fc 44 d3");
	// ASR x0, x1, #4  =>  SBFM x0, x1, #4, #63
	ARM_CODEGEN_TEST(armAsm.Asr(x0, x1, 4), "20 fc 44 93");
	// LSL w0, w1, #4  =>  UBFM w0, w1, #28, #27  (32-bit form)
	ARM_CODEGEN_TEST(armAsm.Lsl(w0, w1, 4), "20 6c 1c 53");
}

// ---------------------------------------------------------------------------
// Load / Store (unsigned offset)
// ---------------------------------------------------------------------------
TEST(ArmCodegenTests, LoadStoreTest)
{
	// LDR x0, [x1]        — 64-bit, offset=0
	ARM_CODEGEN_TEST(armAsm.Ldr(x0, MemOperand(x1)),    "20 00 40 f9");
	// STR x0, [x1]
	ARM_CODEGEN_TEST(armAsm.Str(x0, MemOperand(x1)),    "20 00 00 f9");
	// LDR w0, [x1]        — 32-bit
	ARM_CODEGEN_TEST(armAsm.Ldr(w0, MemOperand(x1)),    "20 00 40 b9");
	// STR w0, [x1]
	ARM_CODEGEN_TEST(armAsm.Str(w0, MemOperand(x1)),    "20 00 00 b9");
	// LDR x0, [x1, #8]   — imm12 = 8/8 = 1
	ARM_CODEGEN_TEST(armAsm.Ldr(x0, MemOperand(x1, 8)), "20 04 40 f9");
	// LDR w0, [x1, #4]   — imm12 = 4/4 = 1
	ARM_CODEGEN_TEST(armAsm.Ldr(w0, MemOperand(x1, 4)), "20 04 40 b9");
	// LDR x0, [x1, x2]   — register offset
	ARM_CODEGEN_TEST(armAsm.Ldr(x0, MemOperand(x1, x2)), "20 68 62 f8");
}

// ---------------------------------------------------------------------------
// Load / Store pair
// ---------------------------------------------------------------------------
TEST(ArmCodegenTests, LoadStorePairTest)
{
	// STP x0, x1, [x2, #-16]!  (pre-index, typical prologue)
	ARM_CODEGEN_TEST(armAsm.Stp(x0, x1, MemOperand(x2, -16, PreIndex)), "40 04 bf a9");
	// LDP x0, x1, [x2], #16    (post-index, typical epilogue)
	ARM_CODEGEN_TEST(armAsm.Ldp(x0, x1, MemOperand(x2, 16, PostIndex)), "40 04 c1 a8");
}

// ---------------------------------------------------------------------------
// Misc / control flow
// ---------------------------------------------------------------------------
TEST(ArmCodegenTests, MiscTest)
{
	// RET  (branch to x30)
	ARM_CODEGEN_TEST(armAsm.Ret(), "c0 03 5f d6");
	// NOP
	ARM_CODEGEN_TEST(armAsm.Nop(), "1f 20 03 d5");
	// BLR x0
	ARM_CODEGEN_TEST(armAsm.Blr(x0), "00 00 3f d6");
	// BR x0
	ARM_CODEGEN_TEST(armAsm.Br(x0), "00 00 1f d6");
}

// ---------------------------------------------------------------------------
// NEON — vector operations
// ---------------------------------------------------------------------------
TEST(ArmCodegenTests, VectorTest)
{
	// MOV v0.16B, v1.16B  =>  ORR v0.16B, v1.16B, v1.16B  (Q=1, size=10, opcode=00011)
	ARM_CODEGEN_TEST(armAsm.Mov(v0.V16B(), v1.V16B()), "20 1c a1 4e");

	// ADD v0.4S, v1.4S, v2.4S  (integer add, Q=1, size=10, opcode=10000)
	ARM_CODEGEN_TEST(armAsm.Add(v0.V4S(), v1.V4S(), v2.V4S()), "20 84 a2 4e");

	// AND v0.16B, v1.16B, v2.16B  (Q=1, size=00, opcode=00011, U=0)
	ARM_CODEGEN_TEST(armAsm.And(v0.V16B(), v1.V16B(), v2.V16B()), "20 1c 22 4e");

	// EOR v0.16B, v1.16B, v2.16B  (Q=1, size=00, opcode=00011, U=1)
	ARM_CODEGEN_TEST(armAsm.Eor(v0.V16B(), v1.V16B(), v2.V16B()), "20 1c 22 6e");

	// ORR v0.16B, v1.16B, v2.16B  (Q=1, size=10, opcode=00011, U=0)
	ARM_CODEGEN_TEST(armAsm.Orr(v0.V16B(), v1.V16B(), v2.V16B()), "20 1c a2 4e");

	// FADD v0.4S, v1.4S, v2.4S  (Q=1, sz=0, opcode=11010)
	ARM_CODEGEN_TEST(armAsm.Fadd(v0.V4S(), v1.V4S(), v2.V4S()), "20 d4 22 4e");

	// FMUL v0.4S, v1.4S, v2.4S  (Q=1, sz=0, opcode=11011)
	ARM_CODEGEN_TEST(armAsm.Fmul(v0.V4S(), v1.V4S(), v2.V4S()), "20 dc 22 6e");

	// FSUB v0.4S, v1.4S, v2.4S  (Q=1, sz=0, opcode=11010, U=1)
	ARM_CODEGEN_TEST(armAsm.Fsub(v0.V4S(), v1.V4S(), v2.V4S()), "20 d4 a2 4e");

	// FDIV v0.4S, v1.4S, v2.4S
	ARM_CODEGEN_TEST(armAsm.Fdiv(v0.V4S(), v1.V4S(), v2.V4S()), "20 fc 22 6e");

	// DUP v0.4S, w1   (scalar GPR to all lanes)
	ARM_CODEGEN_TEST(armAsm.Dup(v0.V4S(), w1), "20 0c 04 4e");
}

// ---------------------------------------------------------------------------
// NEON — shuffle / permute
// ---------------------------------------------------------------------------
TEST(ArmCodegenTests, VectorShuffleTest)
{
	// ZIP1 v0.4S, v1.4S, v2.4S
	ARM_CODEGEN_TEST(armAsm.Zip1(v0.V4S(), v1.V4S(), v2.V4S()), "20 38 82 4e");
	// ZIP2 v0.4S, v1.4S, v2.4S
	ARM_CODEGEN_TEST(armAsm.Zip2(v0.V4S(), v1.V4S(), v2.V4S()), "20 78 82 4e");
	// UZP1 v0.4S, v1.4S, v2.4S
	ARM_CODEGEN_TEST(armAsm.Uzp1(v0.V4S(), v1.V4S(), v2.V4S()), "20 18 82 4e");
	// UZP2 v0.4S, v1.4S, v2.4S
	ARM_CODEGEN_TEST(armAsm.Uzp2(v0.V4S(), v1.V4S(), v2.V4S()), "20 58 82 4e");
	// TRN1 v0.4S, v1.4S, v2.4S
	ARM_CODEGEN_TEST(armAsm.Trn1(v0.V4S(), v1.V4S(), v2.V4S()), "20 28 82 4e");
	// TRN2 v0.4S, v1.4S, v2.4S
	ARM_CODEGEN_TEST(armAsm.Trn2(v0.V4S(), v1.V4S(), v2.V4S()), "20 68 82 4e");
}

// ---------------------------------------------------------------------------
// NEON — conversion
// ---------------------------------------------------------------------------
TEST(ArmCodegenTests, VectorCvtTest)
{
	// SCVTF v0.4S, v1.4S   (signed int to float, 4 lanes)
	ARM_CODEGEN_TEST(armAsm.Scvtf(v0.V4S(), v1.V4S()), "20 d8 21 4e");
	// FCVTZS v0.4S, v1.4S  (float to signed int, round-toward-zero)
	ARM_CODEGEN_TEST(armAsm.Fcvtzs(v0.V4S(), v1.V4S()), "20 b8 a1 4e");
}
