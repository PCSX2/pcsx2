// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// pcsx2-vurunner — headless VU microprogram replayer for codegen iteration.
//
// Loads .vucap files (produced by the live capture probe in mVUexecute,
// see pcsx2/vu_capture.h) and replays them through both the microVU JIT
// and the VU interpreter. Operating modes:
//
//   --diff (default) — run JIT and interp, gtest-equivalent divergence diff
//   --bench          — run JIT only, measure cycles/insns/branch-misses per
//                      iter via PMU counters; report median + median-abs-dev
//   --dump-asm       — template-JIT host-code disasm to <file>.codegen.s
//
// Modes can be combined; the binary runs each capture through each
// requested mode in turn.

#include "harness/RecompilerTestEnvironment.h"
#include "harness/VuReplay.h"
#include "harness/VuSnapshot.h"

#include "vu_capture.h"
#include "Config.h"
#include "Memory.h"
#include "VU.h"
#include "VUmicro.h"
#include "Gif_Unit.h"
#include "microVU_Divtrace.h"
#include "arm64/microVU_Persist-arm64.h"
#include "arm64/microVU_ProgCache-arm64.h"

#include "DebugTools/Debug.h"
#include "common/FPControl.h"
#include "common/PmuCounters.h"

#include <algorithm>
#include <cmath>
#include <csetjmp>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include <unistd.h>

namespace
{

struct Options
{
	u32 iters = 1;
	bool diff = false;
	bool bench = false;
	bool dump_asm = false;
	bool dump_microcode = false;
	bool divtrace = false;
	bool bench_no_reprime = false;
	bool print_bases = false;
	bool no_progcache = false;  // determinism gate: force program cache + recording off
	u32 dump_count = 64;
	u32 cycle_override = 0;  // 0 = use captured budget
	std::string cache_dir;   // empty = persisted-JIT program cache off
	std::vector<std::string> files;
};

void PrintUsage(const char* argv0)
{
	std::fprintf(stderr,
		"Usage: %s [options] file.vucap [file.vucap ...]\n"
		"\n"
		"Modes (combinable; default is --diff):\n"
		"  --diff        Run JIT + interp, report architectural divergences.\n"
		"  --bench       Run JIT only, measure PMU cycles/insns per iter.\n"
		"  --dump-asm    Dump the TEMPLATE JIT's emitted host ARM64 to\n"
		"                <file>.codegen.s. (ARM64 JIT only).\n"
		"  --dump-microcode  Disassemble VU microcode starting at start_pc to stdout.\n"
		"  --divtrace    Per-microvu-op state-snapshot diff between JIT and interp;\n"
		"                report the FIRST divergent op with full context. ARM64 only.\n"
		"  --print-bases Print the process-layout anchors (image base, data/code\n"
		"                arenas, VU rec slabs) and exit. Two consecutive runs of a\n"
		"                non-PIE build must print identical addresses — the\n"
		"                determinism gate for the persisted-JIT program cache.\n"
		"\n"
		"Options:\n"
		"  --cache-dir D Enable the persisted-JIT VU program cache rooted at D\n"
		"                (D/vu_jit/vu{0,1}/...). Turns on emit-time fixup\n"
		"                recording; programs compiled this run are saved as\n"
		"                .vuprog payloads at every block-cache reset, and\n"
		"                programs already on disk hydrate instead of\n"
		"                recompiling. Run twice with the same D for the\n"
		"                cross-process gate: the 2nd run must report\n"
		"                payloadHits>0 / blockCompiles=0 per capture.\n"
		"  --no-progcache  Force the persisted-JIT program cache and emit-time\n"
		"                recording off for the whole process (overrides\n"
		"                --cache-dir) so a JIT-vs-interp diff run is\n"
		"                byte-reproducible.\n"
		"  --iters N     Number of replay iterations per capture (default 1).\n"
		"                Bench: throw out the first iter (JIT compile cost).\n"
		"  --dump-count N    Number of microinstructions to dump (default 64).\n"
		"  --cycles N    Override captured cycle budget (0 = keep original, default).\n"
		"  --help        Show this message.\n"
		"  --            Treat all subsequent args as filenames.\n"
		"\n"
		"Captures are produced by running pcsx2-qt under PCSX2_VU_CAPTURE_DIR=<dir>.\n",
		argv0);
}

bool ParseArgs(int argc, char** argv, Options& opts)
{
	bool end_of_options = false;
	for (int i = 1; i < argc; ++i)
	{
		const std::string a = argv[i];
		if (end_of_options || a.empty() || a[0] != '-')
		{
			opts.files.push_back(a);
			continue;
		}
		if (a == "--")
		{
			end_of_options = true;
		}
		else if (a == "--help" || a == "-h")
		{
			PrintUsage(argv[0]);
			std::exit(0);
		}
		else if (a == "--diff")
		{
			opts.diff = true;
		}
		else if (a == "--bench")
		{
			opts.bench = true;
		}
		else if (a == "--dump-asm")
		{
			opts.dump_asm = true;
		}
		else if (a == "--dump-microcode")
		{
			opts.dump_microcode = true;
		}
		else if (a == "--divtrace")
		{
			opts.divtrace = true;
		}
		else if (a == "--bench-no-reprime")
		{
			opts.bench_no_reprime = true;
		}
		else if (a == "--print-bases")
		{
			opts.print_bases = true;
		}
		else if (a == "--no-progcache")
		{
			opts.no_progcache = true;
		}
		else if (a == "--cache-dir")
		{
			if (i + 1 >= argc)
			{
				std::fprintf(stderr, "vurunner: --cache-dir requires an argument\n");
				return false;
			}
			opts.cache_dir = argv[++i];
			if (opts.cache_dir.empty())
			{
				std::fprintf(stderr, "vurunner: --cache-dir argument is empty\n");
				return false;
			}
		}
		else if (a == "--dump-count")
		{
			if (i + 1 >= argc)
			{
				std::fprintf(stderr, "vurunner: --dump-count requires an argument\n");
				return false;
			}
			const long n = std::strtol(argv[++i], nullptr, 10);
			if (n <= 0 || n > (1 << 16))
			{
				std::fprintf(stderr, "vurunner: --dump-count must be in (0, 65536]\n");
				return false;
			}
			opts.dump_count = static_cast<u32>(n);
		}
		else if (a == "--cycles")
		{
			if (i + 1 >= argc)
			{
				std::fprintf(stderr, "vurunner: --cycles requires an argument\n");
				return false;
			}
			const long n = std::strtol(argv[++i], nullptr, 10);
			if (n < 0)
			{
				std::fprintf(stderr, "vurunner: --cycles must be >= 0\n");
				return false;
			}
			opts.cycle_override = static_cast<u32>(n);
		}
		else if (a == "--iters")
		{
			if (i + 1 >= argc)
			{
				std::fprintf(stderr, "vurunner: --iters requires an argument\n");
				return false;
			}
			const long n = std::strtol(argv[++i], nullptr, 10);
			if (n <= 0 || n > (1 << 24))
			{
				std::fprintf(stderr, "vurunner: --iters must be in (0, 16M]\n");
				return false;
			}
			opts.iters = static_cast<u32>(n);
		}
		else
		{
			std::fprintf(stderr, "vurunner: unknown option '%s'\n", a.c_str());
			return false;
		}
	}
	if (opts.files.empty() && !opts.print_bases)
	{
		std::fprintf(stderr, "vurunner: no input files\n");
		return false;
	}
	if (!opts.diff && !opts.bench && !opts.dump_asm
		&& !opts.dump_microcode && !opts.divtrace && !opts.print_bases)
		opts.diff = true;
	return true;
}

int RunDumpMicrocode(const std::vector<vu_capture::CaptureRecord>& records,
	const std::vector<std::string>& names,
	u32 dump_count)
{
	for (size_t fi = 0; fi < records.size(); ++fi)
	{
		const auto& rec = records[fi];
		const u32 prog_size = static_cast<u32>(rec.microcode.size());
		const u32 limit = (rec.vu_index == 0) ? 0xFFFu : 0x3FFFu;
		std::printf("// %s  vu%u start_pc=0x%08X dump=%u insns\n",
			names[fi].c_str(), rec.vu_index, rec.start_pc, dump_count);

		u32 pc = rec.start_pc & ~7u;
		for (u32 i = 0; i < dump_count; ++i)
		{
			const u32 wrapped = pc & limit;
			if (wrapped + 8 > prog_size)
				break;
			u32 lo = 0, up = 0;
			std::memcpy(&lo, rec.microcode.data() + wrapped + 0, 4);
			std::memcpy(&up, rec.microcode.data() + wrapped + 4, 4);
			const char* upper_s = (rec.vu_index == 0) ?
				disVU0MicroUF(up, pc + 4) : disVU1MicroUF(up, pc + 4);
			std::string upper_copy = upper_s ? upper_s : "?";
			const char* lower_s = (rec.vu_index == 0) ?
				disVU0MicroLF(lo, pc) : disVU1MicroLF(lo, pc);
			std::printf("  %04X  %08X %08X  %-32s | %s\n",
				wrapped, up, lo, upper_copy.c_str(), lower_s ? lower_s : "?");
			// Stop on E-bit (bit 30 of upper word) — end of program.
			if (up & (1u << 30))
			{
				std::printf("  // [E-bit] — program end\n");
				break;
			}
			pc += 8;
		}
		std::printf("\n");
	}
	return 0;
}

// Median + MAD of one PMU counter across samples (skipping the first sample
// when skip_warmup is set, since the JIT compile cost dominates iter 0).
struct Stat
{
	u64 median = 0;
	u64 mad = 0;
	u64 min = 0;
	u64 max = 0;
};

Stat Summarize(const std::vector<u64>& xs)
{
	Stat s;
	if (xs.empty())
		return s;
	std::vector<u64> sorted = xs;
	std::sort(sorted.begin(), sorted.end());
	s.min = sorted.front();
	s.max = sorted.back();
	s.median = sorted[sorted.size() / 2];

	std::vector<u64> dev;
	dev.reserve(sorted.size());
	for (u64 x : sorted)
		dev.push_back(x > s.median ? x - s.median : s.median - x);
	std::sort(dev.begin(), dev.end());
	s.mad = dev[dev.size() / 2];
	return s;
}

// Returns the byte offset of the first byte that differs between two
// vectors, or -1 if they are byte-identical (including matching length).
ssize_t FirstByteDiff(const std::vector<u8>& a, const std::vector<u8>& b)
{
	const size_t n = std::min(a.size(), b.size());
	for (size_t i = 0; i < n; ++i)
		if (a[i] != b[i])
			return static_cast<ssize_t>(i);
	if (a.size() != b.size())
		return static_cast<ssize_t>(n);
	return -1;
}

void PrintPathDiff(const std::vector<u8>& jit, const std::vector<u8>& interp)
{
	if (jit == interp)
		return;
	std::printf("    PATH1: jit=%zu B interp=%zu B (lengths %s)\n",
		jit.size(), interp.size(), jit.size() == interp.size() ? "match" : "DIFFER");
	const ssize_t at = FirstByteDiff(jit, interp);
	if (at < 0)
		return;
	const size_t qw_off = static_cast<size_t>(at) & ~size_t{15};
	std::printf("    PATH1: first byte diff at offset 0x%zx (qword 0x%zx)\n",
		static_cast<size_t>(at), qw_off);
	auto dumpqw = [](const char* label, const std::vector<u8>& v, size_t off) {
		if (off + 16 > v.size())
		{
			std::printf("      %-6s [out of range, len=%zu]\n", label, v.size());
			return;
		}
		std::printf("      %-6s ", label);
		for (int i = 15; i >= 0; --i)
			std::printf("%02x", v[off + i]);
		std::printf("\n");
	};
	for (int qi = -1; qi <= 1; ++qi)
	{
		const ssize_t off = static_cast<ssize_t>(qw_off) + qi * 16;
		if (off < 0)
			continue;
		std::printf("    [qw 0x%zx]\n", static_cast<size_t>(off));
		dumpqw("jit", jit, static_cast<size_t>(off));
		dumpqw("interp", interp, static_cast<size_t>(off));
	}
}

int RunDiff(const std::vector<vu_capture::CaptureRecord>& records,
	const std::vector<std::string>& names,
	u32 iters,
	u32 cycle_override)
{
	int diverged_files = 0;
	int path_diverged_files = 0;
	for (size_t fi = 0; fi < records.size(); ++fi)
	{
		const auto& rec = records[fi];
		std::printf("[diff] %s  (vu%u start_pc=0x%08X cycles=%u)\n",
			names[fi].c_str(), rec.vu_index, rec.start_pc, rec.cycle_budget);
		bool any_diverged = false;
		bool path_diverged = false;
		const char* term = "unknown";
		for (u32 i = 0; i < iters; ++i)
		{
			const auto r = recompiler_tests::ReplayCapture(rec,
				recompiler_tests::VuDiffMode::PipelinePermissive, cycle_override);
			if (!r.ok)
			{
				std::printf("  iter %u: replay setup failed\n", i);
				any_diverged = true;
				break;
			}
			// Termination signal (interp = oracle): ebit = program ran to its
			// E-bit; budget = truncated by cycle budget (loop noise, not a bug).
			if (i == 0)
				term = r.interp_ebit ? "ebit" : "budget";
			const bool path_diff = (r.path1_packets_jit != r.path1_packets_interp);
			if (r.diverged || path_diff)
			{
				any_diverged = true;
				path_diverged = path_diff;
				std::printf("  iter %u: %zu register divergence(s)%s\n",
					i, r.diff_lines.size(), path_diff ? " [PATH1 DIFFERS]" : "");
				for (const auto& d : r.diff_lines)
					std::printf("    %s\n", d.c_str());
				if (path_diff)
					PrintPathDiff(r.path1_packets_jit, r.path1_packets_interp);
				break;
			}
		}
		std::printf("  term=%s\n", term);
		if (!any_diverged)
			std::printf("  ok (%u iters)\n", iters);
		else
		{
			++diverged_files;
			if (path_diverged)
				++path_diverged_files;
		}
	}
	if (diverged_files)
		std::printf("[diff] %d of %zu captures diverged (%d with PATH1 byte diff)\n",
			diverged_files, records.size(), path_diverged_files);
	return diverged_files == 0 ? 0 : 2;
}

int RunBench(const std::vector<vu_capture::CaptureRecord>& records,
	const std::vector<std::string>& names,
	u32 iters,
	bool no_reprime)
{
	const u32 effective_iters = std::max(2u, iters);  // need at least 1 warmup + 1 sample
	std::printf("# bench: iters=%u (iter 0 dropped as warmup), counters: cycles instructions branch-misses, reprime=%s\n",
		effective_iters, no_reprime ? "off" : "on");
	std::printf("%-50s %12s %12s %12s\n", "capture", "cycles_med", "insns_med", "br_miss_med");
	for (size_t fi = 0; fi < records.size(); ++fi)
	{
		const auto& rec = records[fi];
		const auto samples = recompiler_tests::BenchJit(rec, effective_iters, 0, !no_reprime);
		if (samples.size() < 2)
		{
			std::printf("%-50s  bench unavailable (PMU open failed?)\n", names[fi].c_str());
			continue;
		}
		std::vector<u64> cycles, insns, brmiss;
		for (size_t i = 1; i < samples.size(); ++i)  // skip warmup
		{
			cycles.push_back(samples[i][PmuCounters::CpuCycles]);
			insns.push_back(samples[i][PmuCounters::InstructionsRetired]);
			brmiss.push_back(samples[i][PmuCounters::BranchMisses]);
		}
		const Stat sc = Summarize(cycles);
		const Stat si = Summarize(insns);
		const Stat sb = Summarize(brmiss);
		std::printf("%-50s %12llu %12llu %12llu  (mad %llu/%llu/%llu)\n",
			names[fi].c_str(),
			(unsigned long long)sc.median,
			(unsigned long long)si.median,
			(unsigned long long)sb.median,
			(unsigned long long)sc.mad,
			(unsigned long long)si.mad,
			(unsigned long long)sb.mad);
	}
	return 0;
}

// Clamp/NaN by-design classifier. mVU broadcast FMACs leave operands unclamped,
// so the JIT can produce a raw NaN/Inf (exponent all-ones) where the
// interpreter's vuDouble clamps the operand first and then computes a large
// FINITE result — typically max-exponent (0xFE), e.g. 0xff7ffffe, NOT exactly
// ±FLT_MAX. mVUclamp1 also sign-strips NaN, so two NaN lanes can differ only in
// sign/payload. This whole family is shared with the x86 path and is NOT a bug.
//
// The signature is that the diverging lane is an *extreme saturation value* on
// at least one side: NaN/Inf (exponent all-ones), OR exactly ±maxfloat
// (0x?f7fffff) — the literal mVUclamp1 saturation output. The latter case
// surfaces when a NaN *operand* (which both engines hold identically) is
// consumed by a broadcast FMAC: the JIT clamps the NaN result to ±maxfloat
// while the interp's vuDouble path lands elsewhere (often 0). The result lanes
// are then both finite (e.g. JIT=0xff7fffff vs INTERP=0x0), so a NaN/Inf-only
// test misses it — but JIT==±maxfloat is the tell. Both-finite-and-
// neither-saturated is a genuine arithmetic divergence and stays real.
//
// A divergence is "clamp-only" iff every value-diff line is a vf/acc float lane
// matching this pattern, there is at least one such lane, and there are NO
// integer (viN), memory, or pipeline diffs. A single non-clamp diff (the
// FCGET→vi buckets, an integer op) keeps the whole divergence real.
static bool IsClampNanLane(u32 jit, u32 interp)
{
	// Signed-zero: both sides are zero, differing only in the sign bit. mVU
	// flushes denormals via the hardware FPCR FZ bit while the interp's vuDouble
	// flushes in software, so a denormal-underflow MUL/ADD lands at +0 vs -0 —
	// arithmetically equal, a cosmetic sign-of-zero difference shared with x86
	// (same family as the documented NaN-sign divergence).
	if ((jit & 0x7fffffffu) == 0 && (interp & 0x7fffffffu) == 0)
		return true;
	auto isExtreme = [](u32 v) {
		if ((v & 0x7f800000u) == 0x7f800000u) return true; // NaN or Inf
		if ((v & 0x7fffffffu) == 0x7f7fffffu) return true; // ±maxfloat (mVUclamp1)
		return false;
	};
	// CONSERVATIVE: only the JIT side being the saturated extreme is the
	// mVU-clamp / unclamped-broadcast signature (mVU is the clamper). If the
	// JIT produced a clean finite non-maxfloat value while the interp is the
	// extreme, that is NOT the by-design pattern — it is the cleanest possible
	// real-bug signature (the JIT computed something wrong) and must stay in the
	// genuine queue. The one exception is both-sides-NaN/Inf, which is the
	// documented mVUclamp1 NaN sign/payload-strip divergence.
	if (isExtreme(jit))
		return true;
	const bool jit_naninf = (jit & 0x7f800000u) == 0x7f800000u;
	const bool int_naninf = (interp & 0x7f800000u) == 0x7f800000u;
	return jit_naninf && int_naninf; // both NaN/Inf → clamp1 sign-strip
}

// Returns true if all architectural diffs are the by-design clamp/NaN family.
// diff_lines come from DiffVu: value lines are "<name>: JIT=0x.. INTERP=0x..".
// Q-disagree/P-disagree informational lines (appended later, no "JIT=0x") are
// ignored. Returns false if there are no value diffs at all.
static bool ClassifyClampOnly(const std::vector<std::string>& diff_lines)
{
	bool saw_clamp_lane = false;
	for (const auto& line : diff_lines)
	{
		const size_t jpos = line.find("JIT=0x");
		const size_t ipos = line.find("INTERP=0x");
		if (jpos == std::string::npos || ipos == std::string::npos)
			continue; // informational line (Q-disagree etc.) — skip
		// Only vf/acc float lanes can be clamp-pattern. ACC renders as "vf-1.*".
		const bool is_vf = line.rfind("vf", 0) == 0;
		if (!is_vf)
			return false; // viN / vumem / pipeline diff → genuine bug
		const u32 jit    = static_cast<u32>(std::strtoul(line.c_str() + jpos + 6, nullptr, 16));
		const u32 interp = static_cast<u32>(std::strtoul(line.c_str() + ipos + 9, nullptr, 16));
		if (!IsClampNanLane(jit, interp))
			return false; // a vf lane that isn't the clamp pattern → genuine bug
		saw_clamp_lane = true;
	}
	return saw_clamp_lane;
}

// vudivtrace driver. For each capture: enable divtrace mode, replay (which
// runs JIT then interp under divtrace, populating per-op snapshot streams),
// then walk the streams to find the FIRST op where JIT and interp disagree
// on architectural state. Print microvu PC + decoded mnemonic + field diffs.
int RunDivTrace(const std::vector<vu_capture::CaptureRecord>& records,
	const std::vector<std::string>& names,
	u32 cycle_override)
{
	int diverged_files = 0;
	for (size_t fi = 0; fi < records.size(); ++fi)
	{
		const auto& rec = records[fi];
		const int idx = static_cast<int>(rec.vu_index);
		std::printf("[divtrace] %s  (vu%d start_pc=0x%08X cycles=%u)\n",
			names[fi].c_str(), idx, rec.start_pc, rec.cycle_budget);

		// VIs the JIT defers writeback on (flag pipeline + TPC): STATUS=16,
		// MAC=17, CLIP=18, TPC=26. flushAll doesn't push these. Q (vi22) and
		// P (vi23) round-trip through current/pending lanes. The fingerprint
		// (FingerprintRegs) and DiffVu both ignore exactly this set so a
		// flagged divergence is real architectural state, not flag/Q/P noise.
		const std::vector<int> ignored_vi = {16, 17, 18, 22, 23, 26};

		// Pass 1 — fingerprints only (zero-length full-snapshot window). This
		// scales to multi-million-op vumain loops where the 3 KB/op full
		// StateSnap stream overflows.
		mvu_divtrace::EnterMode(idx);
		mvu_divtrace::ConfigureFullWindow(0, 0);
		const auto r = recompiler_tests::ReplayCapture(rec,
			recompiler_tests::VuDiffMode::PipelinePermissive, cycle_override);
		const u32 jit_count    = mvu_divtrace::g_jit_snap_idx.load(std::memory_order_acquire);
		const u32 interp_count = mvu_divtrace::g_interp_op_idx;
		const u32 meta_count   = static_cast<u32>(mvu_divtrace::g_meta.size());

		if (!r.ok)
		{
			std::printf("  replay setup failed\n");
			mvu_divtrace::ExitMode();
			++diverged_files;
			continue;
		}

		std::printf("  ops compiled: %u    jit-executed: %u    interp-executed: %u\n",
			meta_count, jit_count, interp_count);
		const char* term = r.interp_ebit ? "ebit" : "budget";
		std::printf("  term=%s\n", term);

		// Scan fingerprints for the first divergent execution index (and up to
		// 40 divergent indices for the pattern summary).
		const u32 cmp_n = std::min(jit_count, interp_count);
		u32 first_diff = cmp_n;
		std::vector<u32> divergent_idx;
		for (u32 i = 0; i < cmp_n; ++i)
		{
			if (mvu_divtrace::g_jit_fps[i] != mvu_divtrace::g_interp_fps[i])
			{
				if (first_diff == cmp_n)
					first_diff = i;
				divergent_idx.push_back(i);
				if (divergent_idx.size() >= 40) break;
			}
		}

		if (first_diff == cmp_n)
		{
			// The entire common prefix matched architecturally. The two sides
			// only differ in HOW MANY ops they ran. Classify that delta:
			//
			//  - counts equal                  → fully clean.
			//  - counts differ, interp stopped on the CYCLE BUDGET (not E-bit),
			//    and the JIT ran at least as far → benign block-boundary
			//    overshoot. VU0 runs in EE-driven partial chunks, so every VU0
			//    capture is budget-truncated mid-stream; the interp stops at the
			//    exact cycle while the JIT can only stop at its next atomic block
			//    boundary, so it runs a few extra ops of the SAME control flow
			//    interp would have continued into. Not a bug — this is the
			//    dominant VU0 false positive.
			//
			// Anything else with a clean prefix IS a real divergence: interp hit
			// an E-bit terminator the JIT ran past (missed terminator), or the
			// JIT stopped earlier than interp (premature terminator). Fall
			// through to the COUNT_MISMATCH report.
			if (jit_count == interp_count)
			{
				std::printf("  ok (%u ops matched architecturally)\n", cmp_n);
				mvu_divtrace::ExitMode();
				continue;
			}
			const bool interp_budget_truncated = !r.interp_ebit;
			if (interp_budget_truncated && jit_count >= interp_count)
			{
				std::printf("  ok (%u common ops matched; +%u JIT op(s) past the "
					"interp budget cutoff — benign block-boundary overshoot)\n",
					cmp_n, jit_count - interp_count);
				mvu_divtrace::ExitMode();
				continue;
			}
		}

		++diverged_files;
		std::vector<std::string> diff_lines;
		if (first_diff < cmp_n)
		{
			// Pass 2 — re-run with a small full-snapshot window around the
			// first divergence so the detailed report has real register state.
			// Window length is CTX+TAIL+1 regardless of program size (it's
			// [first_diff-CTX, first_diff+TAIL]), so a large CTX is cheap and
			// lets the snapshot-scan below catch roots the fingerprint localizer
			// mis-reports.
			const u32 CTX = 1024, TAIL = 8;
			const u32 wlo  = first_diff > CTX ? first_diff - CTX : 0;
			const u32 wlen = (first_diff - wlo) + 1 + TAIL;
			mvu_divtrace::Reset();
			mvu_divtrace::ConfigureFullWindow(wlo, wlen);
			recompiler_tests::ReplayCapture(rec,
				recompiler_tests::VuDiffMode::PipelinePermissive, cycle_override);
			mvu_divtrace::ExitMode();

			auto JS = [&](u32 g) -> const mvu_divtrace::StateSnap& {
				return mvu_divtrace::g_jit_snaps[g - wlo]; };
			auto ISn = [&](u32 g) -> const mvu_divtrace::StateSnap& {
				return mvu_divtrace::g_interp_snaps[g - wlo]; };

			const auto& js = JS(first_diff);
			const auto& is = ISn(first_diff);

			// Recompute the actual field diffs at the first divergent op.
			recompiler_tests::VuSnapshot jsnap, isnap;
			jsnap.index = idx; jsnap.regs = js.regs;
			isnap.index = idx; isnap.regs = is.regs;
			diff_lines = recompiler_tests::DiffVu(jsnap, isnap,
				recompiler_tests::VuDiffMode::PipelinePermissive, ignored_vi);
			// Q/P multiset info (appended — doesn't drive grouping, the
			// fingerprint already excluded Q/P).
			{
				const u32 ji_q = js.regs.VI[REG_Q].UL, ji_pq = js.regs.pending_q;
				const u32 in_q = is.regs.VI[REG_Q].UL;
				if (in_q != ji_q && in_q != ji_pq)
				{
					char line[160];
					std::snprintf(line, sizeof(line),
						"Q-disagree: INT=%08x not in JIT{%08x,%08x}",
						in_q, ji_q, ji_pq);
					diff_lines.push_back(line);
				}
				if (idx == 1)
				{
					const u32 ji_p = js.regs.VI[REG_P].UL, ji_pp = js.regs.pending_p;
					const u32 in_p = is.regs.VI[REG_P].UL;
					if (in_p != ji_p && in_p != ji_pp)
					{
						char line[160];
						std::snprintf(line, sizeof(line),
							"P-disagree: INT=%08x not in JIT{%08x,%08x}",
							in_p, ji_p, ji_pp);
						diff_lines.push_back(line);
					}
				}
			}

			std::printf("\n  === FIRST DIVERGENCE at op #%u ===\n", first_diff);
			std::printf("  JIT  meta_idx=%u  pre_xPC=0x%04X\n",  js.meta_idx, js.pre_xPC);
			std::printf("  INT  meta_idx=%u  pre_xPC=0x%04X\n",  is.meta_idx, is.pre_xPC);
			if (js.pre_xPC != is.pre_xPC)
			{
				std::printf("  ⚠ pre_xPC mismatch — control-flow divergence;\n");
				std::printf("     JIT and interp executed different ops at this step.\n");
			}

			const u32 microvu_pc = is.pre_xPC; // interp's xPC is authoritative
			const u32 prog_size  = static_cast<u32>(rec.microcode.size());
			const u32 limit      = (idx == 0) ? 0xFFFu : 0x3FFFu;
			const u32 wrapped    = microvu_pc & limit;
			// Mnemonics for the machine-readable [group] line below. First
			// whitespace-delimited token only (no operands) so the same op
			// buckets identically across programs.
			std::string grp_upper = "?", grp_lower = "?";
			int up_fs = -1, up_ft = -1, up_bc = -1;
			if (wrapped + 8 <= prog_size)
			{
				u32 lo = 0, up = 0;
				std::memcpy(&lo, rec.microcode.data() + wrapped + 0, 4);
				std::memcpy(&up, rec.microcode.data() + wrapped + 4, 4);
				up_fs = (up >> 11) & 0x1F;
				up_ft = (up >> 16) & 0x1F;
				up_bc = up & 0x3;
				// disVU?Micro?F share a static output buffer, so copy upper's
				// result before calling the lower decoder.
				const char* upper_s = (idx == 0)
					? disVU0MicroUF(up, microvu_pc + 4) : disVU1MicroUF(up, microvu_pc + 4);
				const std::string upper_copy = upper_s ? upper_s : "?";
				const char* lower_s = (idx == 0)
					? disVU0MicroLF(lo, microvu_pc) : disVU1MicroLF(lo, microvu_pc);
				std::printf("  microcode: %08X %08X\n", up, lo);
				std::printf("  upper:     %s\n", upper_copy.c_str());
				std::printf("  lower:     %s\n", lower_s ? lower_s : "?");

				// Disasm format is "<pc-hex> <op-hex>: <MNEM> <operands>" — the
				// mnemonic is the first token after ": ". Stop at space/comma.
				auto mnemonic = [](const std::string& s) -> std::string {
					size_t c = s.find(": ");
					size_t start = (c == std::string::npos) ? 0 : c + 2;
					while (start < s.size() && (s[start] == ' ' || s[start] == '\t'))
						++start;
					size_t e = start;
					while (e < s.size() && s[e] != ' ' && s[e] != '\t' && s[e] != ',')
						++e;
					return e > start ? s.substr(start, e - start) : s;
				};
				grp_upper = mnemonic(upper_copy);
				if (lower_s)
					grp_lower = mnemonic(lower_s);
			}

			// field "class": leading prefix of the first diff line up to the
			// first '.', ':', '[', '-', or digit (vf12.x→vf, vi02→vi,
			// Q-disagree→Q, mem[..]→mem). ACC renders as "vf-1.x" — special-
			// cased to "acc" — so cross-program diffs on the same register
			// class collapse into one bucket.
			std::string grp_field = "?";
			if (!diff_lines.empty())
			{
				const std::string& fl = diff_lines.front();
				if (fl.rfind("vf-1", 0) == 0)
				{
					grp_field = "acc";
				}
				else
				{
					size_t e = 0;
					while (e < fl.size() && fl[e] != '.' && fl[e] != ':'
						&& fl[e] != '[' && fl[e] != '-'
						&& !(fl[e] >= '0' && fl[e] <= '9'))
						++e;
					grp_field = e ? fl.substr(0, e) : fl;
				}
			}

			// By-design clamp/NaN family detector — strips the documented
			// mVU-FMAC NaN↔±FLT_MAX divergence off the real-bug queue.
			const bool clamp_only = ClassifyClampOnly(diff_lines);

			// Machine-readable grouping key for triage.py --group. One line
			// per diverging capture; bucket by (vu, mnem, field, term, clamp).
			std::printf("  [group] vu=%d mnem=%s/%s field=%s term=%s clamp=%d\n",
				idx, grp_upper.c_str(), grp_lower.c_str(), grp_field.c_str(),
				term, clamp_only ? 1 : 0);

			std::printf("\n  state diff (PipelinePermissive):\n");
			for (const auto& d : diff_lines)
				std::printf("    %s\n", d.c_str());

			// Show 5 ops of pre-divergence context with vi01/vi04/vi06/vi07
			// (commonly tested by IBxxx branches, so useful for spotting the
			// branch input that decided the split).
			std::printf("\n  pre-divergence context (5 ops + key VIs):\n");
			const u32 ctx_start = first_diff >= 5 ? first_diff - 5 : 0;
			for (u32 i = ctx_start; i < first_diff; ++i)
			{
				const auto& jss = JS(i);
				const auto& iss = ISn(i);
				std::printf("    op #%-4u xPC=0x%04X JIT vi01=%04x vi04=%04x vi06=%04x vi07=%04x  INT vi01=%04x vi04=%04x vi06=%04x vi07=%04x\n",
					i, jss.pre_xPC,
					jss.regs.VI[1].UL & 0xFFFF, jss.regs.VI[4].UL & 0xFFFF,
					jss.regs.VI[6].UL & 0xFFFF, jss.regs.VI[7].UL & 0xFFFF,
					iss.regs.VI[1].UL & 0xFFFF, iss.regs.VI[4].UL & 0xFFFF,
					iss.regs.VI[6].UL & 0xFFFF, iss.regs.VI[7].UL & 0xFFFF);
			}

			// Print vf00 + vf01 (DIV inputs in the 0x408 case) and Q lanes.
			std::printf("\n  context (vf00, vf01, Q+pending_q):\n");
			std::printf("    JIT  vf00.w=%08x  vf01.w=%08x  Q=%08x  pendQ=%08x\n",
				js.regs.VF[0].UL[3], js.regs.VF[1].UL[3],
				js.regs.VI[REG_Q].UL, js.regs.pending_q);
			std::printf("    INT  vf00.w=%08x  vf01.w=%08x  Q=%08x  pendQ=%08x\n",
				is.regs.VF[0].UL[3], is.regs.VF[1].UL[3],
				is.regs.VI[REG_Q].UL, is.regs.pending_q);

			// If the first divergence is on a VF reg, dump that VF's full
			// pre-op and post-op state on both sides — the JIT/interp PRE
			// values reveal whether divergence is bad input vs bad emit.
			if (!diff_lines.empty() && diff_lines.front().rfind("vf", 0) == 0)
			{
				const std::string& fl = diff_lines.front();
				size_t dot = fl.find('.');
				const std::string num = fl.substr(2, (dot == std::string::npos ? fl.size() : dot) - 2);
				char* end = nullptr;
				const long n = std::strtol(num.c_str(), &end, 10);
				if (end != num.c_str() && n >= 0 && n < 32)
				{
					std::printf("\n  diverging VF%ld lanes (xyzw):\n", n);
					if (first_diff > 0)
					{
						const auto& jp = JS(first_diff - 1);
						const auto& ip = ISn(first_diff - 1);
						std::printf("    JIT  pre  : %08x %08x %08x %08x\n",
							jp.regs.VF[n].UL[0], jp.regs.VF[n].UL[1],
							jp.regs.VF[n].UL[2], jp.regs.VF[n].UL[3]);
						std::printf("    INT  pre  : %08x %08x %08x %08x\n",
							ip.regs.VF[n].UL[0], ip.regs.VF[n].UL[1],
							ip.regs.VF[n].UL[2], ip.regs.VF[n].UL[3]);
					}
					std::printf("    JIT  post : %08x %08x %08x %08x\n",
						js.regs.VF[n].UL[0], js.regs.VF[n].UL[1],
						js.regs.VF[n].UL[2], js.regs.VF[n].UL[3]);
					std::printf("    INT  post : %08x %08x %08x %08x\n",
						is.regs.VF[n].UL[0], is.regs.VF[n].UL[1],
						is.regs.VF[n].UL[2], is.regs.VF[n].UL[3]);
				}
			}

			// ACC divergence (rendered "vf-1.*"): the VF-lane dump above guards
			// n>=0 and skips ACC, so dump ACC pre/post here plus the op's runtime
			// Fs/Ft operands. ACC is fingerprint-tracked, so at the FIRST
			// divergence ACC pre MUST agree — a post-only ACC divergence with
			// JIT=±maxfloat (0xff7fffff / 0x7f7fffff) is the documented
			// broadcast-unclamped overflow→clamp family, not a new emit bug.
			if (!diff_lines.empty() && diff_lines.front().rfind("vf-1", 0) == 0)
			{
				std::printf("\n  diverging ACC lanes (xyzw):\n");
				if (first_diff > 0)
				{
					const auto& jp = JS(first_diff - 1);
					const auto& ip = ISn(first_diff - 1);
					std::printf("    JIT  pre  : %08x %08x %08x %08x\n",
						jp.regs.ACC.UL[0], jp.regs.ACC.UL[1], jp.regs.ACC.UL[2], jp.regs.ACC.UL[3]);
					std::printf("    INT  pre  : %08x %08x %08x %08x\n",
						ip.regs.ACC.UL[0], ip.regs.ACC.UL[1], ip.regs.ACC.UL[2], ip.regs.ACC.UL[3]);
					auto dumpVf = [&](const char* who, const VURegs& r, int v) {
						if (v >= 0 && v < 32)
							std::printf("    %s vf%02d : %08x %08x %08x %08x\n", who, v,
								r.VF[v].UL[0], r.VF[v].UL[1], r.VF[v].UL[2], r.VF[v].UL[3]);
					};
					std::printf("  op operands (pre): Fs=vf%d Ft=vf%d bc=%c\n",
						up_fs, up_ft, "xyzw"[up_bc & 3]);
					dumpVf("JIT Fs", jp.regs, up_fs);
					dumpVf("INT Fs", ip.regs, up_fs);
					dumpVf("JIT Ft", jp.regs, up_ft);
					dumpVf("INT Ft", ip.regs, up_ft);
				}
				std::printf("    JIT  post : %08x %08x %08x %08x\n",
					js.regs.ACC.UL[0], js.regs.ACC.UL[1], js.regs.ACC.UL[2], js.regs.ACC.UL[3]);
				std::printf("    INT  post : %08x %08x %08x %08x\n",
					is.regs.ACC.UL[0], is.regs.ACC.UL[1], is.regs.ACC.UL[2], is.regs.ACC.UL[3]);
			}

			// Integer (viN) divergence: dump VI[0..15] pre (op N-1) and post
			// (op N) on both sides — shows which integer reg split and whether it
			// split AT this op (emit bug) or was already diverged in the pre-snap
			// (integer-pipeline / snapshot-alignment artifact).
			if (!diff_lines.empty() && diff_lines.front().rfind("vi", 0) == 0
				&& diff_lines.front().rfind("vf", 0) != 0)
			{
				auto dumpVI = [](const char* who, const VURegs& r) {
					std::printf("    %s:", who);
					for (int v = 0; v < 16; ++v)
						std::printf(" %x=%04x", v, r.VI[v].UL & 0xFFFF);
					std::printf("\n");
				};
				std::printf("\n  VI[0..15] pre (op #%u) / post (op #%u):\n",
					first_diff > 0 ? first_diff - 1 : 0, first_diff);
				if (first_diff > 0)
				{
					dumpVI("JIT pre ", JS(first_diff - 1).regs);
					dumpVI("INT pre ", ISn(first_diff - 1).regs);
				}
				dumpVI("JIT post", js.regs);
				dumpVI("INT post", is.regs);

				// Trace the single diverging integer reg across the whole window
				// to see if it RECONVERGES (integer-pipeline-boundary artifact) or
				// PROPAGATES (genuine hazard/emit bug).
				// Snapshot-based first-divergence over ALL VI[0..15] within the
				// window. The fingerprint first_diff can mis-localize badly when
				// the JIT/interp op streams desync (count mismatch) — the
				// fingerprint may report a downstream op while the true integer
				// root is much earlier. This scan walks the captured window from
				// its start and reports the earliest VI split, with the writing
				// op's microcode so the root op is identifiable.
				std::printf("\n  snapshot-scan: earliest VI[0..15] split in window:\n");
				bool found = false;
				for (u32 i = wlo; i < wlo + wlen && !found; ++i)
					for (int v = 0; v < 16; ++v)
						if ((JS(i).regs.VI[v].UL & 0xFFFF) != (ISn(i).regs.VI[v].UL & 0xFFFF))
						{
							const u32 xpc = ISn(i).pre_xPC;
							const u32 w = xpc & ((idx == 0) ? 0xFFFu : 0x3FFFu);
							u32 lw = 0;
							if (w + 4 <= rec.microcode.size())
								std::memcpy(&lw, rec.microcode.data() + w, 4);
							std::printf("    op #%u xPC=0x%04X vi%d: JIT=%04x INT=%04x  (lower=0x%08x)\n",
								i, xpc, v, JS(i).regs.VI[v].UL & 0xFFFF,
								ISn(i).regs.VI[v].UL & 0xFFFF, lw);
							found = true;
							break;
						}
				if (!found)
					std::printf("    (no VI split inside the %u-op window — root is further back)\n", wlen);
			}

			if (divergent_idx.size() > 1)
			{
				std::printf("\n  divergent ops summary (first %zu of the "
					"fingerprint stream; xPC only — out of detail window):\n",
					divergent_idx.size());
				for (u32 g : divergent_idx)
					std::printf("    op #%-7u xPC=0x%04X%s\n", g,
						mvu_divtrace::g_interp_xpc[g],
						g == first_diff ? "  <= first" : "");
			}
		}
		else
		{
			mvu_divtrace::ExitMode();
			// Reached only for a GENUINE terminator divergence: the common
			// prefix is architecturally clean, but the count delta is NOT the
			// benign budget-overshoot filtered above. Either interp hit an E-bit
			// the JIT ran past, or the JIT terminated before interp did.
			const char* kind = (jit_count > interp_count)
				? "JIT RAN PAST interp's terminator (missed E-bit / over-run)"
				: "JIT STOPPED BEFORE interp (premature terminator)";
			std::printf("  [group] vu=%d mnem=TERMINATOR_DIVERGENCE/- field=count term=%s clamp=0\n",
				idx, term);
			std::printf("\n  ⚠ TERMINATOR DIVERGENCE — clean common prefix (%u ops), but\n", cmp_n);
			std::printf("    counts disagree (jit=%u interp=%u, interp_term=%s): %s.\n",
				jit_count, interp_count, term, kind);
			std::printf("    The divergence is the terminator/branch at op #%u; inspect it.\n",
				cmp_n);
		}
		std::printf("\n");
	}
	if (diverged_files)
		std::printf("[divtrace] %d of %zu captures diverged\n",
			diverged_files, records.size());
	return diverged_files == 0 ? 0 : 5;
}


// Print the process-layout anchors the persisted-JIT program cache depends
// on. In a non-PIE build with the fixed-base arena reservation, every line
// must be identical across runs (the determinism gate); the image anchor is
// a libpcsx2 .text address standing in for every baked C-helper/global
// reference, the arena lines cover the rec slabs the cached code lives in.
int RunPrintBases()
{
	std::printf("[print-bases] image_anchor   %p (SysMemory::GetDataPtr)\n",
		reinterpret_cast<void*>(&SysMemory::GetDataPtr));
	std::printf("[print-bases] data_arena     %p\n", SysMemory::GetDataPtr(0));
	std::printf("[print-bases] code_arena     %p\n", SysMemory::GetCodePtr(0));
	std::printf("[print-bases] vu0_rec        %p\n", SysMemory::GetVU0Rec());
	std::printf("[print-bases] vu1_rec        %p\n", SysMemory::GetVU1Rec());
	std::printf("[print-bases] vu_mem         %p\n", SysMemory::GetVUMem());
	return 0;
}

int RunDumpAsm(const std::vector<vu_capture::CaptureRecord>& records,
	const std::vector<std::string>& names)
{
	int failures = 0;
	for (size_t fi = 0; fi < records.size(); ++fi)
	{
		const std::string out_path = names[fi] + ".codegen.s";
		const bool ok = recompiler_tests::DumpJitAsm(records[fi], out_path);
		std::printf("[dump-asm] %s -> %s: %s\n",
			names[fi].c_str(), out_path.c_str(), ok ? "ok" : "FAILED");
		if (!ok)
			++failures;
	}
	return failures == 0 ? 0 : 4;
}

} // namespace

int main(int argc, char** argv)
{
	Options opts;
	if (!ParseArgs(argc, argv, opts))
	{
		PrintUsage(argv[0]);
		return 1;
	}

	if (opts.no_progcache)
		mVUPersist::SetProcessDisable(true);

	if (!opts.cache_dir.empty())
	{
		// Must be wired before Initialize: mVUinit runs the ProgCache VERSION
		// handshake against EmuFolders::Cache, and recording changes emitted
		// code forms from the very first compile. The config bool is what
		// production gates the whole feature on (vurunner doesn't go through
		// VMManager, so set it directly); SetRecordingEnabled mixes the
		// recording state into the options sentinel, so a --cache-dir run
		// can never collide with a recording-off cache of the same programs.
		if (mVUPersist::IsProcessDisabled())
		{
			std::fprintf(stderr, "vurunner: --cache-dir ignored — "
				"--no-progcache is set\n");
		}
		else
		{
			EmuFolders::Cache = opts.cache_dir;
			EmuConfig.Cpu.Recompiler.EnableVUProgramCache = true;
			mVUPersist::SetRecordingEnabled(true);
		}
	}

	if (!recompiler_tests::RecompilerTestEnvironment::Initialize())
	{
		std::fprintf(stderr, "vurunner: RecompilerTestEnvironment::Initialize failed\n");
		return 3;
	}

	if (opts.print_bases)
	{
		const int rc = RunPrintBases();
		if (opts.files.empty())
		{
			recompiler_tests::RecompilerTestEnvironment::Shutdown();
			return rc;
		}
	}

	std::vector<vu_capture::CaptureRecord> records;
	std::vector<std::string> names;
	records.reserve(opts.files.size());
	names.reserve(opts.files.size());
	for (const auto& path : opts.files)
	{
		vu_capture::CaptureRecord rec;
		if (!vu_capture::ReadFromFile(path, rec))
		{
			std::fprintf(stderr, "vurunner: failed to read %s (bad magic/version/sizes?)\n",
				path.c_str());
			recompiler_tests::RecompilerTestEnvironment::Shutdown();
			return 1;
		}
		records.push_back(std::move(rec));
		names.push_back(path);
	}

	int exit_code = 0;
	if (opts.dump_microcode)
		exit_code |= RunDumpMicrocode(records, names, opts.dump_count);
	if (opts.diff)
		exit_code |= RunDiff(records, names, opts.iters, opts.cycle_override);
	if (opts.bench)
		exit_code |= RunBench(records, names, opts.iters, opts.bench_no_reprime);
	if (opts.dump_asm)
		exit_code |= RunDumpAsm(records, names);
	if (opts.divtrace)
		exit_code |= RunDivTrace(records, names, opts.cycle_override);

	if (!opts.cache_dir.empty())
	{
		// Flush still-live programs to disk so their saves show in the
		// summary (mirrors the mVUclose-side save Shutdown would do).
		recompiler_tests::RecompilerTestEnvironment::ResetVuBlockCache(0);
		recompiler_tests::RecompilerTestEnvironment::ResetVuBlockCache(1);
		for (u32 vu = 0; vu < 2; vu++)
		{
			const auto c = mVUProgCache::GetStats(vu);
			const auto p = mVUPersist::GetStats(vu);
			std::printf("[cache] vu%u: entries=%llu payloadWrites=%llu payloadHits=%llu "
				"payloadMissing=%llu payloadRejects=%llu preloaded=%llu preloadHits=%llu "
				"programsHydrated=%llu blocksHydrated=%llu chunksRecorded=%llu "
				"chunksDropped=%llu blockCompiles=%llu\n",
				vu,
				(unsigned long long)c.entries,
				(unsigned long long)c.payloadWrites,
				(unsigned long long)c.payloadHits,
				(unsigned long long)c.payloadMissing,
				(unsigned long long)c.payloadRejects,
				(unsigned long long)c.preloadedPayloads,
				(unsigned long long)c.preloadHits,
				(unsigned long long)p.programsHydrated,
				(unsigned long long)p.blocksHydrated,
				(unsigned long long)p.chunksRecorded,
				(unsigned long long)p.chunksDropped,
				(unsigned long long)mVUPersist::GetBlockCompileCount(vu));
		}
	}

	recompiler_tests::RecompilerTestEnvironment::Shutdown();
	return exit_code;
}
