// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// EE JIT-vs-interp differential fuzzer.
//
// Purpose-built to hunt residency-coherence bugs in the arm64 EE recompiler:
// guest GPR values live in up to FOUR places at once (static pin register,
// allocator GPR slot, 128-bit NEON quad, cpuRegs.GPR memory), and a stale copy
// in any of them is a silent wrong-value bug that per-op unit tests miss.
//
// Generator strategy (differs from iop_jit_fuzz_tests deliberately):
//  - Each seed focuses its register traffic on a SMALL set (8 regs drawn from
//    the full file, always including pinned and unpinned members) so that
//    pin/slot/quad alias collisions are dense rather than diluted over 28 regs.
//  - All GPRs are seeded with random FULL 128-bit values so upper-half
//    staleness (NEON-quad channel) is visible in the post-state diff.
//  - The op mix concentrates on the cross-domain traffic: MMI quad writers and
//    readers, LQ/SQ, unaligned LDL/LDR/SDL/SDR and LWL/LWR/SWL/SWR
//    (read-modify-write dests), scalar 32/64-bit ALU, const producers
//    (LUI/ORI/ADDIU-from-zero), MOVZ/MOVN, MULT/DIV families, and stores of
//    regs in every residency state.
//
// Straight-line programs only (same soundness argument as the IOP fuzzer: both
// runners execute the whole program to the appended `jr ra` terminator, so the
// terminal-state comparison is exact). A second test adds intra-program
// forward branches — programs still run to full completion, so the comparison
// stays sound while exercising block splits and the branch-state fork/join
// paths in the recompiler.

#include "harness/EeRecTestHarness.h"

#include <gtest/gtest.h>

#include <vector>

#include <cstdio>
#include <cstdlib>
using namespace recompiler_tests;
using namespace mips;
namespace e = mips::ee;

namespace {

constexpr u32 kData = RecompilerTestEnvironment::kScratchAddr; // 0x00020000
constexpr u32 kWindowBytes = 256;

// Focus-set candidates: everything but $zero, the memory bases $k0/$k1, and
// $ra (the harness terminator is `jr ra`). Includes the tier-1 pins
// ($sp/$v0/$v1/$a0/$a1) and tier-2 pins ($at/$s0) alongside unpinned regs.
constexpr u32 kDestPool[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
                             16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 28, 29, 30};
constexpr u32 kDestPoolN = sizeof(kDestPool) / sizeof(kDestPool[0]);
constexpr u32 kFocusN = 8;

struct Lcg
{
	u64 s;
	u32 next() { s = s * 6364136223846793005ull + 1442695040888963407ull; return static_cast<u32>(s >> 33); }
	u32 range(u32 n) { return next() % n; }
	u64 next64() { return (static_cast<u64>(next()) << 32) | next(); }
};

struct Focus
{
	u32 regs[kFocusN];
	u32 pick(Lcg& r) const { return regs[r.range(kFocusN)]; }
	// Source: usually from the focus set, sometimes anything (incl. $zero and
	// the bases) so const-$zero and base-aliasing paths get traffic too.
	u32 src(Lcg& r) const { return (r.range(8) == 0) ? r.range(31) : pick(r); }
};

Focus makeFocus(Lcg& r)
{
	Focus f{};
	for (u32 i = 0; i < kFocusN; ++i)
	{
	pick_again:
		const u32 cand = kDestPool[r.range(kDestPoolN)];
		for (u32 j = 0; j < i; ++j)
			if (f.regs[j] == cand)
				goto pick_again;
		f.regs[i] = cand;
	}
	return f;
}

u32 baseReg(Lcg& r) { return (r.next() & 1) ? reg::k0 : reg::k1; }

// EEFUZZ_COUNT scales every test's seed count and EEFUZZ_START offsets the
// seed range (soak runs shard [start, start+count) across fresh processes —
// per-seed cost grows superlinearly with in-process seed count, so many
// short-lived processes beat one long one). Defaults: checked-in count, 0.
u32 seedCount(u32 def)
{
	if (const char* c = std::getenv("EEFUZZ_COUNT"))
		return static_cast<u32>(std::strtoul(c, nullptr, 10));
	return def;
}

u32 seedStart()
{
	if (const char* c = std::getenv("EEFUZZ_START"))
		return static_cast<u32>(std::strtoul(c, nullptr, 10));
	return 0;
}

s16 fuzzImm(Lcg& r)
{
	const u32 v = r.next();
	switch ((v >> 4) & 3)
	{
		case 0: return static_cast<s16>(v & 0x7F);
		case 1: return static_cast<s16>(0x8000u | (v & 0x7FFF));
		default: return static_cast<s16>(v & 0xFFFF);
	}
}

// In-window offset; `access` is the access size in bytes, `align` the required
// alignment (1 for the unaligned-legal families).
s16 memOff(Lcg& r, u32 access, u32 align)
{
	const u32 span = kWindowBytes - access;
	u32 off = r.range(span + 1);
	off &= ~(align - 1);
	return static_cast<s16>(off);
}

// One random non-control-flow EE instruction over the focus set.
u32 genOp(Lcg& r, const Focus& f)
{
	const u32 d = f.pick(r), a = f.src(r), b = f.src(r);
	const s16 imm = fuzzImm(r);
	switch (r.range(73))
	{
		// ---- scalar 32-bit ALU ----
		case 0: return ADDU(d, a, b);
		case 1: return SUBU(d, a, b);
		case 2: return AND(d, a, b);
		case 3: return OR(d, a, b);
		case 4: return XOR(d, a, b);
		case 5: return NOR(d, a, b);
		case 6: return SLT(d, a, b);
		case 7: return SLTU(d, a, b);
		case 8: return SLL(d, a, r.range(32));
		case 9: return SRA(d, a, r.range(32));
		case 10: return SRLV(d, a, b);
		// ---- const producers / imm ALU ----
		case 11: return LUI(d, static_cast<u16>(imm));
		case 12: return ORI(d, a, static_cast<u16>(imm));
		case 13: return ADDIU(d, a, imm);
		case 14: return ADDIU(d, reg::zero, imm); // small-const producer
		case 15: return ANDI(d, a, static_cast<u16>(imm));
		case 16: return XORI(d, a, static_cast<u16>(imm));
		case 17: return SLTIU(d, a, imm);
		// ---- scalar 64-bit ALU / shifts ----
		case 18: return e::DADDU(d, a, b);
		case 19: return e::DSUBU(d, a, b);
		case 20: return e::DADDIU(d, a, imm);
		case 21: return e::DSLL(d, a, r.range(32));
		case 22: return e::DSRA32(d, a, r.range(32));
		case 23: return e::DSRLV(d, a, b);
		// ---- conditional moves (Phase 4b resident-dest family) ----
		case 24: return e::MOVZ(d, a, b);
		case 25: return e::MOVN(d, a, b);
		// ---- mult/div + HI/LO (allocator-bypass family) ----
		case 26: return MULT(a, b);
		case 27: return DIV(a, b);
		case 28: return DIVU(a, b);
		case 29: return e::MADD(d, a, b);
		case 30: return e::MULT1(d, a, b);
		case 31: return e::DIVU1(a, b);
		case 32: return MFHI(d);
		case 33: return MFLO(d);
		case 34: return MTHI(a);
		case 35: return MTLO(a);
		case 36: return e::MFLO1(d);
		case 37: return e::PMFHL(d, r.range(2) ? 0u : 3u); // LW / LH forms
		// ---- MMI quad writers/readers (NEON-quad channel) ----
		case 38: return e::PADDW(d, a, b);
		case 39: return e::PSUBH(d, a, b);
		case 40: return e::PCGTB(d, a, b);
		case 41: return e::PEXTLW(d, a, b);
		case 42: return e::PEXTUH(d, a, b);
		case 43: return e::PCPYLD(d, a, b);
		case 44: return e::PCPYUD(d, a, b);
		case 45: return e::PAND(d, a, b);
		case 46: return e::POR(d, a, b);
		case 47: return e::PPACW(d, a, b);
		case 48: return e::PSLLH(d, a, r.range(16));
		case 49: return e::PCPYH(d, a);
		// ---- memory: aligned loads/stores ----
		case 50: return LW(d, memOff(r, 4, 4), baseReg(r));
		case 51: return (r.next() & 1) ? LBU(d, memOff(r, 1, 1), baseReg(r))
		                               : LH(d, memOff(r, 2, 2), baseReg(r));
		case 52: return e::LD(d, memOff(r, 8, 8), baseReg(r));
		case 53: return e::LQ(d, memOff(r, 16, 16), baseReg(r));
		case 54: return (r.next() & 1) ? SW(a, memOff(r, 4, 4), baseReg(r))
		                               : SB(a, memOff(r, 1, 1), baseReg(r));
		case 55: return (r.next() & 1) ? e::SD(a, memOff(r, 8, 8), baseReg(r))
		                               : e::SQ(a, memOff(r, 16, 16), baseReg(r));
		// ---- unaligned families (read-modify-write dests; LDL/SDL fusion) ----
		case 56:
			switch (r.range(4))
			{
				case 0: return LWL(d, memOff(r, 4, 1), baseReg(r));
				case 1: return LWR(d, memOff(r, 4, 1), baseReg(r));
				case 2: return e::LDL(d, memOff(r, 8, 1), baseReg(r));
				default: return e::LDR(d, memOff(r, 8, 1), baseReg(r));
			}
		case 57:
			switch (r.range(4))
			{
				case 0: return SWL(a, memOff(r, 4, 1), baseReg(r));
				case 1: return SWR(a, memOff(r, 4, 1), baseReg(r));
				case 2: return e::SDL(a, memOff(r, 8, 1), baseReg(r));
				default: return e::SDR(a, memOff(r, 8, 1), baseReg(r));
			}
		// ---- sign/zero-extension loads (upper-half staleness class) ----
		case 58:
			switch (r.range(3))
			{
				case 0: return LB(d, memOff(r, 1, 1), baseReg(r));
				case 1: return LHU(d, memOff(r, 2, 2), baseReg(r));
				default: return e::LWU(d, memOff(r, 4, 4), baseReg(r));
			}
		// ---- remaining shift shapes ----
		case 59:
			switch (r.range(4))
			{
				case 0: return SLLV(d, a, b);
				case 1: return SRAV(d, a, b);
				case 2: return e::DSLLV(d, a, b);
				default: return e::DSRL32(d, a, r.range(32));
			}
		// ---- unsigned/wide mult + pipe-1 HI/LO ----
		case 60:
			switch (r.range(4))
			{
				case 0: return MULTU(a, b);
				case 1: return e::DMULTU(a, b);
				case 2: return e::MADDU(d, a, b);
				default: return e::MULTU1(d, a, b);
			}
		// ---- MMI parallel mult/mul-acc (native PMADDW/PMSUBW family) ----
		case 61:
			switch (r.range(6))
			{
				case 0: return e::PMADDW(d, a, b);
				case 1: return e::PMSUBW(d, a, b);
				case 2: return e::PMULTW(d, a, b);
				case 3: return e::PMADDH(d, a, b);
				case 4: return e::PMULTH(d, a, b);
				default: return e::PHMADH(d, a, b);
			}
		case 62: return (r.next() & 1) ? e::PMADDUW(d, a, b) : e::PMULTUW(d, a, b);
		// ---- MMI HI/LO 128-bit transfer ----
		case 63:
			switch (r.range(4))
			{
				case 0: return e::PMFHI(d);
				case 1: return e::PMFLO(d);
				case 2: return e::PMTHI(a);
				default: return e::PMTLO(a);
			}
		case 64: return e::PMFHL(d, r.range(5)); // all forms: LW/UW/SLW/LH/SH
		case 65: return e::PMTHL(a);
		// ---- MMI saturating / unsigned add-sub ----
		case 66:
			switch (r.range(6))
			{
				case 0: return e::PADDSW(d, a, b);
				case 1: return e::PSUBSW(d, a, b);
				case 2: return e::PADDSH(d, a, b);
				case 3: return e::PADDSB(d, a, b);
				case 4: return e::PADDUW(d, a, b);
				default: return e::PSUBUB(d, a, b);
			}
		// ---- MMI compare / min-max / abs ----
		case 67:
			switch (r.range(6))
			{
				case 0: return e::PCEQW(d, a, b);
				case 1: return e::PCEQB(d, a, b);
				case 2: return e::PMAXW(d, a, b);
				case 3: return e::PMINH(d, a, b);
				case 4: return e::PABSW(d, a);
				default: return e::PADSBH(d, a, b);
			}
		// ---- MMI pack/unpack/exchange remainder ----
		case 68:
			switch (r.range(6))
			{
				case 0: return e::PEXTLB(d, a, b);
				case 1: return e::PEXTUB(d, a, b);
				case 2: return e::PPACH(d, a, b);
				case 3: return e::PPACB(d, a, b);
				case 4: return e::PINTH(d, a, b);
				default: return e::PINTEH(d, a, b);
			}
		case 69:
			switch (r.range(6))
			{
				case 0: return e::PEXEH(d, a);
				case 1: return e::PREVH(d, a);
				case 2: return e::PEXEW(d, a);
				case 3: return e::PROT3W(d, a);
				case 4: return e::PEXCH(d, a);
				default: return e::PEXCW(d, a);
			}
		case 70: return (r.next() & 1) ? e::PEXT5(d, a) : e::PPAC5(d, a);
		case 71: return (r.next() & 1) ? e::PNOR(d, a, b) : e::PXOR(d, a, b);
		default:
			return e::PSRLH(d, a, r.range(16));
	}
}

void SeedState(EeRecTestHarness& h, Lcg& r)
{
	for (u32 i = 1; i < 32; ++i)
	{
		if (i == reg::k0 || i == reg::k1 || i == reg::ra)
			continue;
		h.SetGpr128(i, r.next64(), r.next64());
	}
	h.SetGpr64(reg::k0, kData);
	h.SetGpr64(reg::k1, kData); // same base, distinct residency entry
	h.SetLoPair(r.next64(), r.next64());
	h.SetHiPair(r.next64(), r.next64());
	for (u32 i = 0; i < kWindowBytes / 4; ++i)
		h.WriteU32(kData + i * 4, r.next());
	h.TrackMemWindow(kData, kWindowBytes);
}

} // namespace

// ── Straight-line: dense pin/slot/quad alias traffic, no branches ────────────
TEST(EeFuzz, StraightLineResidencyMix)
{
	// Repro/minimization knobs (local debugging only): EEFUZZ_SEED pins a single
	// seed; EEFUZZ_KEEP="a-b" NOPs every generated op outside [a,b].
	const char* only_env = std::getenv("EEFUZZ_SEED");
	const u32 only = only_env ? static_cast<u32>(std::atoi(only_env)) : ~0u;
	u32 keep_lo = 0, keep_hi = ~0u;
	if (const char* keep = std::getenv("EEFUZZ_KEEP"))
		std::sscanf(keep, "%u-%u", &keep_lo, &keep_hi);

	const u32 start = seedStart(), count = seedCount(2000);
	for (u32 seed = start; seed < start + count; ++seed)
	{
		if (only != ~0u && seed != only)
			continue;
		SCOPED_TRACE(::testing::Message() << "seed=" << seed);
		if (std::getenv("EEFUZZ_TRACE"))
			std::fprintf(stderr, "EEFUZZ seed=%u\n", seed);
		Lcg r{seed * 0x9E3779B97F4A7C15ull + 0xDEADBEEFull};
		const Focus f = makeFocus(r);
		EeRecTestHarness h;
		SeedState(h, r);

		std::vector<u32> prog;
		for (u32 i = 0; i < 120; ++i)
			prog.push_back(genOp(r, f));
		for (u32 i = 0; i < prog.size(); ++i)
			if (i < keep_lo || i > keep_hi)
				prog[i] = NOP;

		h.LoadProgram(prog);
		h.Run();
		if (::testing::Test::HasFailure())
		{
			// Dump the program for offline disassembly/minimization.
			std::fprintf(stderr, "EEFUZZ FAILING seed=%u prog:\n", seed);
			for (size_t i = 0; i < prog.size(); ++i)
				std::fprintf(stderr, "  %3zu: %08x\n", i, prog[i]);
			return; // stop at first failing seed for a clean repro
		}
	}
}

// ── Forward branches: block splits + branch-state fork/join under the same
//    residency traffic. Branches only skip forward over freshly generated ops,
//    so every program still runs to the terminator on both runners. ──────────
TEST(EeFuzz, ForwardBranchResidencyMix)
{
	const u32 start = seedStart(), count = seedCount(1000);
	for (u32 seed = start; seed < start + count; ++seed)
	{
		SCOPED_TRACE(::testing::Message() << "seed=" << seed);
		if (std::getenv("EEFUZZ_TRACE"))
			std::fprintf(stderr, "EEFUZZ seed=%u\n", seed);
		Lcg r{seed * 0xC2B2AE3D27D4EB4Full + 0x12345u};
		const Focus f = makeFocus(r);
		EeRecTestHarness h;
		SeedState(h, r);

		std::vector<u32> prog;
		while (prog.size() < 110)
		{
			if (r.range(5) == 0)
			{
				// Branch skipping `skip` generated ops; delay slot always
				// executes (no likely variants here — those squash, which is
				// fine for interp/JIT parity but halves delay-slot coverage).
				const u32 skip = 1 + r.range(5);
				const u32 s1 = f.src(r), s2 = f.src(r);
				const s16 off = static_cast<s16>(1 + skip); // dslot + skipped ops
				switch (r.range(6))
				{
					case 0: prog.push_back(BEQ(s1, s2, off)); break;
					case 1: prog.push_back(BNE(s1, s2, off)); break;
					case 2: prog.push_back(BLTZ(s1, off)); break;
					case 3: prog.push_back(BGEZ(s1, off)); break;
					case 4: prog.push_back(BLEZ(s1, off)); break;
					default: prog.push_back(BGTZ(s1, off)); break;
				}
				prog.push_back(genOp(r, f)); // delay slot
				for (u32 k = 0; k < skip; ++k)
					prog.push_back(genOp(r, f)); // skipped-if-taken body
			}
			else
			{
				prog.push_back(genOp(r, f));
			}
		}

		h.LoadProgram(prog);
		h.Run();
		if (::testing::Test::HasFailure())
			return;
	}
}

// ── Backward loops: real block re-entry. A counted loop makes the JIT
//    re-execute a compiled block body with whatever residency state the
//    previous iteration left behind — coverage the forward-only tests can
//    never reach (pins/slots live across the dispatcher round-trip).
//    Soundness: the counter reg is excluded from the focus set, so no
//    generated op can write it (genOp dests come only from the focus set);
//    the loop decrements it to exactly zero, so both runners execute the
//    identical full trace to the terminator. ───────────────────────────────
TEST(EeFuzz, BackwardLoopResidencyMix)
{
	const u32 start = seedStart(), count = seedCount(1000);
	for (u32 seed = start; seed < start + count; ++seed)
	{
		SCOPED_TRACE(::testing::Message() << "seed=" << seed);
		if (std::getenv("EEFUZZ_TRACE"))
			std::fprintf(stderr, "EEFUZZ seed=%u\n", seed);
		Lcg r{seed * 0xA24BAED4963EE407ull + 0xB5EFull};
		const Focus f = makeFocus(r);
		// Counter: any pool reg not in the focus set (so genOp never writes it).
		u32 ctr;
		do
			ctr = kDestPool[r.range(kDestPoolN)];
		while ([&] { for (u32 x : f.regs) if (x == ctr) return true; return false; }());

		EeRecTestHarness h;
		SeedState(h, r);

		std::vector<u32> prog;
		const u32 nLoops = 1 + r.range(3);
		for (u32 l = 0; l < nLoops; ++l)
		{
			const u32 iters = 2 + r.range(6);
			prog.push_back(ADDIU(ctr, reg::zero, static_cast<s16>(iters)));
			const u32 bodyLen = 4 + r.range(16);
			const size_t loopStart = prog.size();
			for (u32 i = 0; i < bodyLen; ++i)
				prog.push_back(genOp(r, f));
			prog.push_back(ADDIU(ctr, ctr, -1));
			// Branch back to loopStart; offset is relative to the delay slot.
			const s16 off = static_cast<s16>(
				static_cast<s32>(loopStart) - static_cast<s32>(prog.size() + 1));
			prog.push_back(BNE(ctr, reg::zero, off));
			prog.push_back(genOp(r, f)); // delay slot (runs every iteration)
		}
		for (u32 i = 0; i < 8; ++i)
			prog.push_back(genOp(r, f)); // post-loop consume traffic

		h.LoadProgram(prog);
		h.Run();
		if (::testing::Test::HasFailure())
		{
			std::fprintf(stderr, "EEFUZZ FAILING seed=%u prog:\n", seed);
			for (size_t i = 0; i < prog.size(); ++i)
				std::fprintf(stderr, "  %3zu: %08x\n", i, prog[i]);
			return;
		}
	}
}

// ── Branch-into-delay-slot loops: a forward exit branch at the loop head
//    whose DELAY SLOT is the backward branch's re-entry target (the guest
//    dcache-flush idiom: `beqz exit; addiu ctr,-1; body...; bgtz ctr, <the
//    addiu>`). This is the shape the other mixes never emit — no generated
//    branch targets another branch's delay slot — and exactly the blind spot
//    that let the SL-05 zero-length-block wedge reach live UYA: the head
//    branch is a superblock continuation site whose delay slot is a
//    backward-split target, exercising the split clamp's degenerate
//    (site-at-startpc, loop 0 — the head IS the program entry) and
//    non-degenerate (later loops) cases, plus block re-entry AT a delay-slot
//    address. Termination: ctr is outside the focus set and decrements
//    exactly once per iteration (the delay-slot ADDIU runs on entry and on
//    every backward re-entry); BGTZ falls through at zero, so both runners
//    execute the identical full trace. ─────────────────────────────────────
TEST(EeFuzz, DelaySlotTargetLoopMix)
{
	const u32 start = seedStart(), count = seedCount(1000);
	for (u32 seed = start; seed < start + count; ++seed)
	{
		SCOPED_TRACE(::testing::Message() << "seed=" << seed);
		if (std::getenv("EEFUZZ_TRACE"))
			std::fprintf(stderr, "EEFUZZ seed=%u\n", seed);
		Lcg r{seed * 0xD6E8FEB86659FD93ull + 0x5EEDull};
		const Focus f = makeFocus(r);
		// Counter: any pool reg not in the focus set (so genOp never writes it).
		u32 ctr;
		do
			ctr = kDestPool[r.range(kDestPoolN)];
		while ([&] { for (u32 x : f.regs) if (x == ctr) return true; return false; }());

		EeRecTestHarness h;
		SeedState(h, r);

		std::vector<u32> prog;
		const u32 nLoops = 1 + r.range(3);
		for (u32 l = 0; l < nLoops; ++l)
		{
			const u32 iters = 2 + r.range(6);
			if (l == 0)
			{
				// Loop 0's head branch IS the program entry — the site sits at
				// the block's first instruction (the SL-05 degenerate shape).
				h.SetGpr64(ctr, iters);
			}
			else
			{
				prog.push_back(ADDIU(ctr, reg::zero, static_cast<s16>(iters)));
			}
			const u32 bodyLen = 2 + r.range(12);
			// Head: exit → past the backward branch's delay slot
			// (target index = head + 1 + off = head + bodyLen + 4).
			const s16 exit_off = static_cast<s16>(bodyLen + 3);
			if (r.range(2))
				prog.push_back(BEQ(ctr, reg::zero, exit_off));
			else
				prog.push_back(BLEZ(ctr, exit_off));
			prog.push_back(ADDIU(ctr, ctr, -1)); // ds — the backward target
			for (u32 i = 0; i < bodyLen; ++i)
				prog.push_back(genOp(r, f));
			prog.push_back(BGTZ(ctr, static_cast<s16>(-(static_cast<s32>(bodyLen) + 2))));
			prog.push_back(genOp(r, f)); // bgtz ds (runs every iteration)
		}
		for (u32 i = 0; i < 8; ++i)
			prog.push_back(genOp(r, f)); // post-loop consume traffic

		h.LoadProgram(prog);
		h.Run();
		if (::testing::Test::HasFailure())
		{
			std::fprintf(stderr, "EEFUZZ FAILING seed=%u prog:\n", seed);
			for (size_t i = 0; i < prog.size(); ++i)
				std::fprintf(stderr, "  %3zu: %08x\n", i, prog[i]);
			return;
		}
	}
}
