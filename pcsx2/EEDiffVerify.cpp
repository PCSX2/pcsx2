// SPDX-License-Identifier: GPL-3.0+
//
// @@EEDIFF@@ THROWAWAY DIAGNOSTIC — EE recompiler-vs-interpreter differential verifier.
// See EEDiffVerify.h for the design overview. This file is the core: snapshot, the
// interpreter re-run with store-capture, and the compare/report.

#include "EEDiffVerify.h"

#include "R5900.h"
#include "R5900OpcodeTables.h"
#include "Memory.h"

#include "common/Console.h"

#include <cstdio>
#include <cstring>

// --------------------------------------------------------------------------------------
//  Global switches
// --------------------------------------------------------------------------------------
volatile bool g_ee_diff_verify = false;
bool g_ee_diff_capture_stores = false;

void eeDiffSetEnabled(bool enabled) { g_ee_diff_verify = enabled; }
bool eeDiffGetEnabled() { return g_ee_diff_verify; }

namespace
{
// Snapshot of the guest register state taken BEFORE the recompiled op runs (via
// eeDiffSnapshotPre). This is the interpreter's input for the re-run.
struct EeDiffRegs
{
	GPR_reg gpr[32];
	GPR_reg hi;
	GPR_reg lo;
	u32 sa;
	u32 pc;
};

EeDiffRegs g_diff_pre;   // regs at op entry (rec == interp input here)
EeDiffRegs g_diff_recpost; // regs after the recompiled op ran (rec ground-truth to match)

// Captured interpreter stores for the op currently being verified.
struct EeDiffStore
{
	u32 addr;
	u32 bits; // 8/16/32/64/128
	u64 lo;
	u64 hi;   // only for 128-bit
};
constexpr u32 EEDIFF_MAX_STORES = 8; // a single EE op stores at most a 128-bit quad
EeDiffStore g_diff_stores[EEDIFF_MAX_STORES];
u32 g_diff_store_count = 0;

// Log a bounded number of divergences (enough to characterize a systematic decompressor
// miscompile across several ops) without flooding logcat. False positives are now filtered at
// the source (RAM-only), so this budget is spent on real divergences.
u32 g_diff_reported = 0;
constexpr u32 EEDIFF_MAX_REPORTS = 64;

void snapshotInto(EeDiffRegs& dst)
{
	std::memcpy(dst.gpr, cpuRegs.GPR.r, sizeof(dst.gpr));
	dst.hi = cpuRegs.HI;
	dst.lo = cpuRegs.LO;
	dst.sa = cpuRegs.sa;
	dst.pc = cpuRegs.pc;
}

void restoreFrom(const EeDiffRegs& src)
{
	std::memcpy(cpuRegs.GPR.r, src.gpr, sizeof(src.gpr));
	cpuRegs.HI = src.hi;
	cpuRegs.LO = src.lo;
	cpuRegs.sa = src.sa;
	cpuRegs.pc = src.pc;
}

// Read the real (rec-written) memory at a captured store's address so we can compare it
// against what the interpreter WOULD have written. Uses the same vtlb read path the
// interpreter loads use, so it sees exactly what the rec committed. Reads happen with
// capture mode OFF, so they are real reads.
bool memMatchesCapture(const EeDiffStore& s, u64& real_out)
{
	switch (s.bits)
	{
		case 8:  real_out = memRead8(s.addr);  return real_out == (s.lo & 0xffu);
		case 16: real_out = memRead16(s.addr); return real_out == (s.lo & 0xffffu);
		case 32: real_out = memRead32(s.addr); return real_out == (s.lo & 0xffffffffu);
		case 64: real_out = memRead64(s.addr); return real_out == s.lo;
		case 128:
		{
			u128 v;
			memRead128(s.addr, &v);
			real_out = v.lo; // report low half; compare both
			return v.lo == s.lo && v.hi == s.hi;
		}
		default: real_out = 0; return true;
	}
}

// True iff the effective address lands in EE main RAM (32 MB, incl. KSEG0/KSEG1 mirrors) or
// the 16 KB scratchpad — the ONLY regions where "read the value back and compare it to what
// the interpreter would have written" is a valid store check. Everything else is memory-mapped
// I/O: EE registers 0x10000000-0x1000FFFF (DMAC/GIF/VIF/IPU/timers/INTC), GS privileged
// registers 0x12000000, VU/GS mem, BIOS ROM. A read of those does NOT return the last written
// value, so comparing a store there against a read-back is a GUARANTEED FALSE POSITIVE — and
// re-running such an access on the interpreter is unsafe (DMA kicks, counter/event side effects,
// Cpu->CancelInstruction longjmps). The texture-decompression bug we hunt writes to RAM, so
// RAM + scratchpad is exactly — and only — the region we verify.
inline bool eeDiffAddrIsRam(u32 addr)
{
	if ((addr & 0xFFFFC000u) == 0x70000000u) // scratchpad (0x70000000, 16 KB)
		return true;
	return (addr & 0x1FFFFFFFu) < 0x02000000u; // main RAM (0x00000000, 32 MB) + KSEG0/KSEG1 mirrors
}

// A memory op whose effective address is NOT RAM/scratchpad must be SKIPPED entirely: the
// interpreter re-run risks event-test/longjmp/DMA-kick side effects, and the store-compare
// would be a false positive (I/O doesn't read back what you wrote). Only RAM/scratchpad
// accesses are verified. Effective address = GPR[rs].UL[0] + s16(imm) (standard EE base+offset;
// LWL/SWL/LQ/SQ mask low bits but stay in the same region, so the RAM test still holds).
// Returns true = "skip".
bool opTouchesNonRam(u32 op, const EeDiffRegs& pre)
{
	const u32 primary = op >> 26;
	bool is_mem;
	switch (primary)
	{
		case 0x1e: // LQ
		case 0x1f: // SQ
		case 0x20: case 0x21: case 0x22: case 0x23: // LB LH LWL LW
		case 0x24: case 0x25: case 0x26: case 0x27: // LBU LHU LWR LWU
		case 0x28: case 0x29: case 0x2a: case 0x2b: // SB SH SWL SW
		case 0x2c: case 0x2d: case 0x2e: case 0x2f: // SDL SDR SWR CACHE
		case 0x1a: case 0x1b:                        // LDL LDR
		case 0x37: case 0x3f:                        // LD SD
			is_mem = true;
			break;
		default:
			is_mem = false;
			break;
	}
	if (!is_mem)
		return false;

	const u32 rs = (op >> 21) & 0x1f;
	const u32 addr = pre.gpr[rs].UL[0] + static_cast<u32>(static_cast<s32>(static_cast<s16>(op)));
	return !eeDiffAddrIsRam(addr);
}

// Ops whose interpreter re-run would raise a CPU exception: SYSCALL/BREAK and the conditional
// TRAPs. The interpreter services these via cpuException -> CP0 mutation + a longjmp we neither
// snapshot (no CP0 in EeDiffRegs) nor can survive mid-block. The rec ends the block on these
// anyway, so they carry no straight-line divergence to check. Skip the re-run. (Trapping-arith
// signed-overflow is handled separately by opWouldTrapOverflow.)
bool opRaisesException(u32 op)
{
	const u32 primary = op >> 26;
	if (primary == 0x00) // SPECIAL
	{
		const u32 funct = op & 0x3f;
		if (funct == 0x0c || funct == 0x0d) return true; // SYSCALL, BREAK
		if (funct >= 0x30 && funct <= 0x36) return true; // TGE TGEU TLT TLTU TEQ (.34) TNE (.36)
		return false;
	}
	if (primary == 0x01) // REGIMM
	{
		const u32 rt = (op >> 16) & 0x1f;
		return (rt >= 0x08 && rt <= 0x0e); // TGEI TGEIU TLTI TLTIU TEQI TNEI
	}
	return false;
}

// The EE's trapping arithmetic (ADD/ADDI/SUB/DADD/DADDI/DSUB) raises a signed-overflow
// exception in the INTERPRETER (cpuException -> mutates CP0 + pc, possibly a longjmp), but
// the recompiler intentionally SKIPS that trap (treats them as ADDU/…, matching the x86
// JIT — see aR5900Arith.cpp). So on a real overflow they legitimately diverge and re-running
// the interpreter would corrupt CP0 irreversibly (we don't snapshot CP0). Detect overflow
// from the pre-op operands (mirroring _add32_Overflow/_add64_Overflow exactly) and SKIP the
// re-run when it would trap. The far-more-common non-overflow path is still fully verified.
// Returns true = "skip this op (would trap)".
bool add32WouldOverflow(s32 x, s32 y)
{
	GPR_reg64 r;
	r.SD[0] = static_cast<s64>(x) + y;
	return (r.UL[0] >> 31) != (r.UL[1] & 1);
}
bool add64WouldOverflow(s64 x, s64 y)
{
	const s64 result = x + y;
	return ((~(x ^ y)) & (x ^ result)) < 0;
}
bool opWouldTrapOverflow(u32 op, const EeDiffRegs& pre)
{
	const u32 primary = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	const s32 imm = static_cast<s16>(op);
	const s64 srs = pre.gpr[rs].SD[0];
	const s64 srt = pre.gpr[rt].SD[0];

	switch (primary)
	{
		case 0x08: return add32WouldOverflow(static_cast<s32>(srs), imm);          // ADDI
		case 0x18: return add64WouldOverflow(srs, imm);                            // DADDI
		case 0x00: // SPECIAL — funct disambiguates the trapping R-type adds/subs
			switch (op & 0x3f)
			{
				case 0x20: return add32WouldOverflow(static_cast<s32>(srs), static_cast<s32>(srt)); // ADD
				case 0x22: return add32WouldOverflow(static_cast<s32>(srs), static_cast<s32>(-srt)); // SUB
				case 0x2c: return add64WouldOverflow(srs, srt);                     // DADD
				case 0x2e: return add64WouldOverflow(srs, -srt);                    // DSUB
				default:   return false;
			}
		default: return false;
	}
}

void reportDivergence(u32 pc, u32 op, const char* what,
	const char* detail, u64 rec_val, u64 interp_val)
{
	const u32 primary = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	const u32 rd = (op >> 11) & 0x1f;
	const u32 sa = (op >> 6) & 0x1f;
	const u32 funct = op & 0x3f;
	const char* name = ::R5900::GetInstruction(op).Name;

	Console.WriteLn(
		"@@EEDIFF@@ pc=%08x op=%08x %-8s DIVERGE %s(%s) rec=%016llx interp=%016llx "
		"| primary=%02x funct=%02x rs=%u rt=%u rd=%u sa=%u",
		pc, op, name, what, detail,
		static_cast<unsigned long long>(rec_val),
		static_cast<unsigned long long>(interp_val),
		primary, funct, rs, rt, rd, sa);
}
} // namespace

// --------------------------------------------------------------------------------------
//  Store capture (called from vtlb_memWrite hook)
// --------------------------------------------------------------------------------------
void eeDiffCaptureStore(u32 addr, u32 bits, u64 lo, u64 hi)
{
	if (g_diff_store_count >= EEDIFF_MAX_STORES)
		return; // shouldn't happen for one op; drop extras defensively
	EeDiffStore& s = g_diff_stores[g_diff_store_count++];
	s.addr = addr;
	s.bits = bits;
	s.lo = lo;
	s.hi = hi;
}

// --------------------------------------------------------------------------------------
//  Emitted-code entry points
// --------------------------------------------------------------------------------------
extern "C" void eeDiffSnapshotPre()
{
	snapshotInto(g_diff_pre);
}

extern "C" void eeDiffVerify(u32 pc, u32 op)
{
	// The recompiled op has just run: cpuRegs == REC-post. Save it first so we can always
	// restore it (the rec owns guest state from here on).
	snapshotInto(g_diff_recpost);

	// Skip memory ops whose effective address is not RAM/scratchpad: the interpreter re-run
	// would touch memory-mapped I/O (event test / DMA kick / longjmp risk) and the store-compare
	// there is a false positive. cpuRegs is already REC-post; nothing to do. See opTouchesNonRam.
	if (opTouchesNonRam(op, g_diff_pre))
		return;

	// Skip ops whose interpreter re-run would raise a CPU exception (SYSCALL/BREAK/TRAP) — the
	// longjmp would escape mid-block and we don't snapshot CP0. See opRaisesException.
	if (opRaisesException(op))
		return;

	// Skip trapping-arithmetic ops that would raise a signed-overflow exception in the
	// interpreter (the rec deliberately doesn't trap — a legitimate, known divergence that
	// would also corrupt CP0 in the re-run). See opWouldTrapOverflow.
	if (opWouldTrapOverflow(op, g_diff_pre))
		return;

	restoreFrom(g_diff_pre); // cpuRegs = interpreter input (op-entry state)

	g_diff_store_count = 0;
	const u32 saved_code = cpuRegs.code;
	cpuRegs.code = op; // the interpreter's operand macros (_Rs_/_Rt_/_Imm_/...) read this

	g_ee_diff_capture_stores = true;
	::R5900::GetInstruction(op).interpret(); // one op; stores captured, not applied
	g_ee_diff_capture_stores = false;

	cpuRegs.code = saved_code;
	// cpuRegs is now INTERP-post (regs updated in place; stores diverted to the log).

	bool diverged = false;

	// (i) compare GPRs (full 128-bit), then HI, LO.
	if (g_diff_reported < EEDIFF_MAX_REPORTS)
	{
		for (u32 r = 0; r < 32 && !diverged; r++)
		{
			const GPR_reg& ri = cpuRegs.GPR.r[r];       // interp-post
			const GPR_reg& rr = g_diff_recpost.gpr[r];  // rec-post
			if (ri.UD[0] != rr.UD[0] || ri.UD[1] != rr.UD[1])
			{
				char detail[24];
				std::snprintf(detail, sizeof(detail), "GPR%u.lo", r);
				reportDivergence(pc, op, "reg", detail, rr.UD[0], ri.UD[0]);
				if (ri.UD[1] != rr.UD[1])
					reportDivergence(pc, op, "reg", "GPRhi", rr.UD[1], ri.UD[1]);
				diverged = true;
			}
		}
		if (!diverged && (cpuRegs.HI.UD[0] != g_diff_recpost.hi.UD[0] ||
						  cpuRegs.HI.UD[1] != g_diff_recpost.hi.UD[1]))
		{
			reportDivergence(pc, op, "reg", "HI", g_diff_recpost.hi.UD[0], cpuRegs.HI.UD[0]);
			diverged = true;
		}
		if (!diverged && (cpuRegs.LO.UD[0] != g_diff_recpost.lo.UD[0] ||
						  cpuRegs.LO.UD[1] != g_diff_recpost.lo.UD[1]))
		{
			reportDivergence(pc, op, "reg", "LO", g_diff_recpost.lo.UD[0], cpuRegs.LO.UD[0]);
			diverged = true;
		}

		// (ii) compare each captured interpreter store against the real memory the rec
		// already wrote. Read memory with capture mode OFF (real reads).
		for (u32 i = 0; i < g_diff_store_count && !diverged; i++)
		{
			const EeDiffStore& s = g_diff_stores[i];
			if (!eeDiffAddrIsRam(s.addr))
				continue; // 2nd-layer guard: never compare an I/O store against a read-back
			u64 real_val = 0;
			if (!memMatchesCapture(s, real_val))
			{
				char detail[40];
				std::snprintf(detail, sizeof(detail), "mem[%08x]/%ub", s.addr, s.bits);
				reportDivergence(pc, op, "mem", detail, real_val, s.lo);
				diverged = true;
			}
		}

		if (diverged)
			g_diff_reported++;
	}

	// CRITICAL: restore cpuRegs to REC-post so the real recompiled run is unaffected —
	// the recompiler owns guest state and continues from here.
	restoreFrom(g_diff_recpost);
	cpuRegs.code = saved_code;
}
