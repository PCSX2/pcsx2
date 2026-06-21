// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "VU.h"
#include "common/Pcsx2Defs.h"

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <vector>

namespace recompiler_tests {

struct VuMemWindow
{
	u32 addr = 0;
	std::vector<u8> bytes;
};

// Captures everything a diff between JIT and interp needs to compare for one
// VU instance. The architectural surface is `VF[32]`, `VI[32]` (low 16 bits),
// `ACC`, the spilled `q`/`p` scalars, the flag pipeline arrays, XGKICK
// state (VU1 only), and tracked windows of VU data memory.
//
// Fields explicitly excluded from `Diff*` (captured but never compared):
//   * `Mem`, `Micro` pointers — preserved on Restore(), never compared.
//   * `cycle`, `nextBlockCycles`, `start_pc`, `code` — dispatcher bookkeeping.
//   * `branch`, `branchpc`, `delaybranchpc`, `takedelaybranch`, `ebit` —
//     interpreter mid-execution sentinels; meaningless after E-bit termination.
//   * `fmac[4]`, `fdiv`, `efu`, `ialu[4]`, fmac/ialu r/w positions, counts —
//     interpreter pipeline modeling that microVU represents differently.
//   * `macflag`, `statusflag`, `clipflag` — `VURegs.h` calls these "kind of
//     hacky"; the architecturally-meaningful values live in `VI[REG_*_FLAG]`.
//   * `idx`, `flags`, `VIBackupCycles`, `VIOldValue`, `VIRegNumber` —
//     internal bookkeeping.
struct VuSnapshot
{
	int index = 0;
	VURegs regs{};
	std::vector<VuMemWindow> mem_windows;

	// Snapshots `vuRegs[index]` plus the requested data-memory windows from
	// `VU.Mem`. Captures the full struct verbatim; the diff function decides
	// which fields are architecturally significant.
	static VuSnapshot Capture(int index, const std::vector<VuMemWindow>& windows_to_capture);

	// Writes architectural state back to `vuRegs[index]` and restores any
	// captured memory windows. Preserves the live `Mem` / `Micro` pointers
	// so the restored struct keeps pointing at SysMemory-allocated VU memory.
	void Restore() const;

	// Zeroes the architectural state of one VU instance while preserving the
	// `Mem` / `Micro` pointers. Pipeline / dispatcher bookkeeping is reset.
	static void ZeroGlobals(int index);
};

// Diff mode controls strictness on the microVU-specific pipeline-state arrays
// (`micro_macflags[4]`, `micro_clipflags[4]`, `micro_statusflags[4]`) and on
// the interpreter's pending_q / pending_p shadows.
//   * Strict — bit-exact compare on every captured field. Use for
//     interp-vs-interp round-trip tests where divergence is impossible.
//   * PipelinePermissive — architectural fields strict (VF/VI/ACC/q/p/MAC/
//     STATUS/CLIP/XGKICK/memory), pipeline state ignored. Use for
//     JIT-vs-interp where the two disagree on internal pipeline modeling but
//     converge on architectural state at E-bit termination.
//   * XgkickPacketEquivalent — PipelinePermissive minus the xgkick scratch
//     fields (xgkickaddr/diff/sizeremaining/cyclecount/enable/endpacket).
//     The non-XGKICKHACK microVU emit path computes addr/size in locals and
//     fires the GIF transfer directly, without writing back to the
//     VU1.xgkick* fields — so the JIT and interp legitimately disagree on
//     those scratch fields. Tests using
//     this mode are expected to assert architectural equivalence by
//     comparing the GIF Path 1 packet bytes (VuTestHarness::
//     Path1PacketBytesJit/Interp).
enum class VuDiffMode
{
	Strict,
	PipelinePermissive,
	XgkickPacketEquivalent,
};

// Returns a list of human-readable field-level differences, empty when the
// snapshots match across every field considered architecturally significant
// for the chosen mode. `ignored_vi` opts specific VI indices out of the diff
// — used by tests that probe a termination path where the JIT and interp
// legitimately disagree on bookkeeping registers (e.g. REG_TPC after an
// M-bit break) without touching the architectural state under test.
std::vector<std::string> DiffVu(const VuSnapshot& a, const VuSnapshot& b,
	VuDiffMode mode = VuDiffMode::PipelinePermissive,
	const std::vector<int>& ignored_vi = {});

void PrintVu(std::ostream& os, const VuSnapshot& s);

} // namespace recompiler_tests
