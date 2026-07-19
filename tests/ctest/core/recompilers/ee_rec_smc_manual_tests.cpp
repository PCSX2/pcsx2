// SPDX-FileCopyrightText: 2026 yaps2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ProtMode_Manual inline-check coverage (FX-03a gate).
//
// Blocks in guest pages 0x81/0x80001 are FORCED into manual protection
// (contains_thread_stack in memory_protect_recompiled_code): every block
// entry re-compares the block's source words against live RAM and jumps to
// DispatchBlockDiscard on mismatch. No other test exercises this emission.
// These tests pin both halves of the check's contract across every
// chunk/tail geometry of the compare emission, so the FX-03a fast-compare
// rewrite (hoisted bases + Ldp/Ccmp chain) cannot silently drop coverage of
// a word:
//   - pass: unmodified source re-enters the cached block and produces the
//     same result (no spurious discard);
//   - discard: a RAW source modification — no recClear, no protection
//     fault, which is exactly the write class manual mode exists for — is
//     detected at the next entry and the block recompiles.

#include "harness/EeRecTestHarness.h"

#include "Memory.h"
#include "R5900.h"

#include <gtest/gtest.h>

using namespace recompiler_tests;
using namespace mips;

namespace
{
// Page 0x81 -> contains_thread_stack forces ProtMode_Manual on every block.
constexpr u32 kManualPc = 0x00081000;

// Seed a routine at kManualPc:
//   word 0        : ADDIU v0, zero, 0
//   words 1..adds : ADDIU v0, v0, 1
//   words +1, +2  : JR ra / NOP
// Block size = adds + 3 words, so adds 0..7 covers check sizes 12..40
// bytes: pure 8B+4B tails, exact 16B chunks, and every chunk+tail mix.
void SeedManualRoutine(EeRecTestHarness& h, u32 adds)
{
	u32 addr = kManualPc;
	h.WriteU32(addr, ADDIU(reg::v0, reg::zero, 0));
	addr += 4;
	for (u32 i = 0; i < adds; i++, addr += 4)
		h.WriteU32(addr, ADDIU(reg::v0, reg::v0, 1));
	h.WriteU32(addr, JR(reg::ra));
	h.WriteU32(addr + 4, NOP);
}

// Caller at kProgramPc: JAL into the manual page, preserving the harness
// return address around the call so the appended JR ra still parks.
void LoadCaller(EeRecTestHarness& h)
{
	h.LoadProgram({
		OR(reg::t0, reg::ra, reg::zero),
		JAL(kManualPc),
		NOP,
		OR(reg::ra, reg::t0, reg::zero),
	});
}
} // namespace

TEST(EeRecSmcManual, CheckPassesUnmodifiedAcrossGeometries)
{
	for (u32 adds = 0; adds <= 7; adds++)
	{
		EeRecTestHarness h;
		SeedManualRoutine(h, adds);
		LoadCaller(h);
		h.Run(EeRecTestHarness::RunMode::FreshCache);
		h.ExpectGpr64(reg::v0, adds);
		// Re-entry of the cached manual block: the inline check must pass
		// and must NOT discard (same result from the same compiled block).
		h.Run(EeRecTestHarness::RunMode::PreserveCache);
		h.ExpectGpr64(reg::v0, adds);
	}
}

TEST(EeRecSmcManual, RawModifyEachWordTriggersDiscard)
{
	for (u32 adds = 0; adds <= 7; adds++)
	{
		for (u32 mod = 0; mod <= adds; mod++) // every ADDIU word position
		{
			EeRecTestHarness h;
			SeedManualRoutine(h, adds);
			LoadCaller(h);
			h.Run(EeRecTestHarness::RunMode::FreshCache);
			h.ExpectGpr64(reg::v0, adds);

			// RAW write: straight into the physical mirror, bypassing
			// memWrite32/Cpu->Clear. Only the manual-mode inline check can
			// catch this.
			const u32 addr = kManualPc + mod * 4;
			const u32 new_insn = (mod == 0) ? ADDIU(reg::v0, reg::zero, 50) :
			                                  ADDIU(reg::v0, reg::v0, 101);
			*(u32*)PSM(addr) = new_insn;

			h.Run(EeRecTestHarness::RunMode::PreserveCache);
			h.ExpectGpr64(reg::v0, (mod == 0) ? adds + 50ull : adds + 100ull);
		}
	}
}
