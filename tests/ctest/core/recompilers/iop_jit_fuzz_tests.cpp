// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// Comprehensive IOP JIT-vs-interp fuzzer (straight-line, full opcode set).
//
// Covers the FULL IOP opcode set rather than the imm subset already covered by
// iop_alu_tests. Broad randomized coverage like this catches aliasing
// miscompiles such as the const-minuend SUB/SUBU case where the destination
// aliased the subtrahend (Rd == Rt): a naive materialization of the constant
// into Rd clobbers Rt before the subtract, yielding cv - cv = 0. The minimal
// repro lives as a targeted regression in iop_alu_tests.cpp
// (Sub*ConstMinuendDestAliasesSubtrahend). This fuzzer stays as permanent broad
// coverage.
//
// Why straight-line only: the harness diffs JIT vs interp at a fixed EE-cycle
// budget, and the two runners debit that budget at different granularities (the
// rec uses static per-block cycle estimates, the interp counts dynamically). A
// single straight-line block runs to its `jr ra` terminator in one block
// regardless of budget, so both runners reach the identical terminal state —
// the comparison is sound. Multi-block programs straddle the budget boundary and
// get cut off at different points between the two runners (a cycle-accounting
// artifact, not a miscompile); control-flow coverage lives in the hand-written
// iop_branch_tests / iop_jump_tests / iop_multiblock_tests instead.

#include "harness/JitTestHarness.h"

#include <gtest/gtest.h>

#include <vector>

using namespace recompiler_tests;
using namespace mips;

namespace {

constexpr u32 kPc = RecompilerTestEnvironment::kProgramPc;     // 0x00010000
constexpr u32 kData = RecompilerTestEnvironment::kScratchAddr; // 0x00020000
constexpr u32 kWindowWords = 60;                              // 240-byte scratch window

// Dest registers: exclude $zero(0), $k0(26)/$k1(27) (memory bases), $ra(31).
// 28 dests > host pool (~14) → sustained eviction.
constexpr u32 kDest[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
                         17, 18, 19, 20, 21, 22, 23, 24, 25, 28, 29, 30};
constexpr u32 kNumDest = sizeof(kDest) / sizeof(kDest[0]);

struct Lcg
{
	u32 s;
	u32 next() { s = s * 1103515245u + 12345u; return s; }
	u32 range(u32 n) { return (next() >> 8) % n; }
};

u32 baseReg(Lcg& r) { return (r.next() & 1) ? reg::k0 : reg::k1; }
u32 destReg(Lcg& r) { return kDest[r.range(kNumDest)]; }
u32 srcReg(Lcg& r) { return r.range(31); } // 0..30 (any, incl. bases/zero)

s16 fuzzImm(Lcg& r)
{
	const u32 v = r.next();
	switch ((v >> 4) & 3)
	{
		case 0: return static_cast<s16>(v & 0x7F);
		case 1: return static_cast<s16>(0x8000u | (v & 0x7FFF)); // sign bit set
		default: return static_cast<s16>(v & 0xFFFF);
	}
}

// In-window offset for memory ops. `align` allows an unaligned skew for ops
// that legally take unaligned addresses (LWL/LWR/SWL/SWR, byte/half ops).
s16 memOff(Lcg& r, u32 align)
{
	u32 off = r.range(kWindowWords - 1) * 4;
	if (align < 4)
		off += r.range(4) & ~(align - 1);
	return static_cast<s16>(off);
}

// Generate one non-control-flow IOP instruction.
u32 genOp(Lcg& r)
{
	const u32 d = destReg(r), a = srcReg(r), b = srcReg(r);
	const s16 imm = fuzzImm(r);
	switch (r.range(34))
	{
		case 0:  return ADD(d, a, b);
		case 1:  return ADDU(d, a, b);
		case 2:  return SUB(d, a, b);
		case 3:  return SUBU(d, a, b);
		case 4:  return AND(d, a, b);
		case 5:  return OR(d, a, b);
		case 6:  return XOR(d, a, b);
		case 7:  return NOR(d, a, b);
		case 8:  return SLT(d, a, b);
		case 9:  return SLTU(d, a, b);
		case 10: return SLL(d, a, r.range(32));
		case 11: return SRL(d, a, r.range(32));
		case 12: return SRA(d, a, r.range(32));
		case 13: return SLLV(d, a, b);
		case 14: return SRLV(d, a, b);
		case 15: return SRAV(d, a, b);
		case 16: return MULT(a, b);
		case 17: return MULTU(a, b);
		case 18: return DIV(a, b);  // includes div-by-zero (canonical result)
		case 19: return DIVU(a, b);
		case 20: return MFHI(d);
		case 21: return MFLO(d);
		case 22: return MTHI(a);
		case 23: return MTLO(a);
		case 24: return ADDIU(d, a, imm);
		case 25: return ANDI(d, a, static_cast<u16>(imm));
		case 26: return ORI(d, a, static_cast<u16>(imm));
		case 27: return LUI(d, static_cast<u16>(imm));
		case 28: return LW(d, memOff(r, 4), baseReg(r));
		case 29: return LB(d, memOff(r, 1), baseReg(r));
		case 30: return LHU(d, memOff(r, 2), baseReg(r));
		case 31: return LWL(d, memOff(r, 1), baseReg(r));
		case 32: return SW(a, memOff(r, 4), baseReg(r));
		default: return SWL(a, memOff(r, 1), baseReg(r));
	}
}

void SeedState(JitTestHarness& h, Lcg& r)
{
	for (u32 i = 1; i < 31; ++i)
		h.SetGpr(i, r.next());
	h.SetGpr(reg::k0, kData);
	h.SetGpr(reg::k1, kData); // same valid base, distinct allocator entry
	h.SetHi(r.next());
	h.SetLo(r.next());
	for (u32 i = 0; i < kWindowWords; ++i)
		h.WriteU32(kData + i * 4, r.next());
	h.TrackMemWindow(kData, kWindowWords * 4);
}

} // namespace

// ── Straight-line: full op set, heavy register pressure, no branches ─────────
TEST(IopFuzz, StraightLineAllOps)
{
	for (u32 seed = 0; seed < 1500; ++seed)
	{
		SCOPED_TRACE(::testing::Message() << "seed=" << seed);
		Lcg r{seed * 2654435761u + 0x1234567u};
		JitTestHarness h;
		SeedState(h, r);

		std::vector<u32> prog;
		for (u32 i = 0; i < 80; ++i)
			prog.push_back(genOp(r));

		h.LoadProgramAt(kPc, prog.data(), prog.size(), /*append_jr_ra_term=*/true);
		h.Run();
		if (::testing::Test::HasFailure())
			return; // stop at first failing seed for a clean repro
	}
}
