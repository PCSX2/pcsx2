// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE (R5900) recompiler — skeleton (Phase 1).
//
// ARM64 counterpart to pcsx2/x86/ix86-32/iR5900.cpp. At this stage every entry
// point is a stub: the recompiler is *defined* and links, providing the recCpu
// provider so VMManager can be wired to call Reserve/Reset/Shutdown on ARM64,
// but no guest code is actually compiled yet (recExecute fails loudly if reached).
// Real codegen lands incrementally in later phases (vtlb fastmem -> EE int ->
// branches -> coprocessors). The interpreter remains ground truth and the active
// provider until this is functional.

#include "arm64/aR5900.h"
#include "arm64/aR5900Analysis.h"

#include "Config.h"
#include "Memory.h"
#include "R5900.h"
#include "R5900OpcodeTables.h"
#include "VMManager.h"
#include "VU.h"
#include "VUmicro.h"
#include "vtlb.h"
#include "AndroidEEOpHist.h"
#include "EEDiffVerify.h" // @@EEDIFF@@ recompiler-vs-interpreter differential verifier

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/FastJmp.h"
#include "common/Pcsx2Defs.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <setjmp.h>
#include <unordered_map>
#include <vector>

extern void _vu0WaitMicro();
extern void vu0Sync(); // VU0.cpp — catch VU0 up to the EE clock (no force-finish); vc111


namespace a64 = vixl::aarch64;

// --------------------------------------------------------------------------------------
//  EE code-cache layout (Phase 1.3)
// --------------------------------------------------------------------------------------
// The EE recompiler region is pre-reserved by SysMemory (HostMemoryMap::EErec*, 64 MB).
// We do NOT allocate it ourselves; we just carve it:
//
//   [ GetEERec() ............................ recPtrEnd )   emitted block code
//   [ recPtrEnd ............................. GetEERecEnd() )   ArmConstantPool
//
// The constant pool holds far-jump trampolines and 64/128-bit literals that VIXL
// loads PC-relative (see ArmConstantPool in AsmHelpers). x86 has no such pool (it
// inlines immediates), so this tail carve-out is ARM64-specific. recPtr is the
// rolling emit cursor; block compilation (Phase 1.4) advances it and resets the
// whole cache when it runs past recPtrEnd.

// Space reserved at the tail of the EE rec region for the constant pool.
static constexpr u32 EE_CONSTPOOL_SIZE = static_cast<u32>(_1mb);

static u8* recPtr = nullptr;    // rolling emit cursor (start of next block)
static u8* recPtrEnd = nullptr; // end of the code region / start of the constant pool

static ArmConstantPool s_const_pool;

// --------------------------------------------------------------------------------------
//  recLUT block-lookup table (Phase 4.4)
// --------------------------------------------------------------------------------------
// Two-level guest-PC -> host-block lookup, ported from the x86 rec (iR5900.cpp +
// BaseblockEx.h). The top-level page table recLUT[pc>>16] holds, per 64 KB guest
// page, a base pointer pre-biased so that
//
//     slot   = (uptr*)(recLUT[pc >> 16] + pc * 2)
//     fnptr  = *slot
//
// indexes one host code pointer per 4-byte guest word (sizeof(uptr)==8, so pc*2 ==
// (pc/4)*8). The emitted DispatcherReg performs exactly this arithmetic and branches
// to `fnptr`. Uncompiled words point at the JITCompile stub (compile-on-jump);
// unmapped pages point every word at UnmappedRecLUTPage. The per-word slots live in
// recLutReserve (one contiguous array covering RAM + the three BIOS ROM regions);
// recLUT_SetPage maps each guest page (including its address mirrors) onto that
// array, mirroring x86 so the same code can be reached through any of its mirror
// addresses via a single shared block. hwLUT is intentionally omitted: invalidation
// is whole-cache reset for bring-up (Phase 4.5), so no HWADDR folding is needed.
alignas(16) static uptr recLUT[0x10000];

// One host-pointer slot per guest word of RAM + ROM + ROM1 + ROM2, plus a single
// shared 64 KB page worth of slots that every unmapped guest page aliases onto.
static std::vector<uptr> recLutReserve;
static std::vector<uptr> recLutUnmapped;
static size_t recLutEntries = 0;
static uptr* recRAM = nullptr;
static uptr* recROM = nullptr;  // BIOS (0x1fc0..0x2000 in 64 KB pages)
static uptr* recROM1 = nullptr; // DVD player
static uptr* recROM2 = nullptr; // Chinese ROM extension

// C++-side equivalent of the emitted lookup: address of the block slot for `pc`.
static __fi uptr* recPtrToBlock(u32 pc)
{
	return reinterpret_cast<uptr*>(recLUT[pc >> 16] + pc * (sizeof(uptr) / 4));
}

static __fi u32 recHWAddr(u32 pc)
{
	// Match the x86 recompiler's HWADDR() comparisons for RAM/BIOS mirrors. Fast
	// Boot EELOAD and ELF-entry checks are stored as physical addresses.
	const u32 ram_offset = pc & (Ps2MemSize::ExposedRam - 1);
	const u32 ram_base = pc - ram_offset;
	switch (ram_base)
	{
		case 0x00000000u:
		case 0x20000000u:
		case 0x30000000u:
		case 0x80000000u:
		case 0xa0000000u:
		case 0xb0000000u:
		case 0xc0000000u:
		case 0xd0000000u:
			return ram_offset;
		default:
			return pc;
	}
}

// Hard cap on instructions per block, so straight-line code can't run the emit
// cursor away before we get a chance to reset. (x86 uses page/branch boundaries;
// this is a simpler bring-up bound.)
static constexpr u32 MAX_BLOCK_INSTS = 256;

// Safety headroom kept free at the end of the code region. The cache-full check fires
// when recPtr crosses (recPtrEnd - this), guaranteeing the block currently being
// emitted always fits without VIXL ever trying to grow (realloc) the MAP_JIT buffer —
// which it cannot, and which aborts the process. A single block is at most
// MAX_BLOCK_INSTS guest ops plus a delay slot and the dispatch tail; even the largest
// host expansions stay well under 256 KB, so 1 MB is comfortably safe.
static constexpr u32 RECOMPILE_HEADROOM = static_cast<u32>(_1mb);

// Byte offsets (from RESTATEPTR = &cpuRegs) of the 64-bit cycle counters the emitted
// block tail reads/writes for the inline event test.
static constexpr u32 EE_CYCLE_OFFSET = static_cast<u32>(offsetof(cpuRegisters, cycle));
static constexpr u32 EE_NEXTEVENTCYCLE_OFFSET = static_cast<u32>(offsetof(cpuRegisters, nextEventCycle));
static constexpr u32 EE_HI_SCALAR_OFFSET = 32u * 16u;
static constexpr u32 EE_LO_SCALAR_OFFSET = 33u * 16u;

// Byte offset of cpuRegs.CP0.n.Status (the COP0 interrupt/mode status word the DI
// generator clears Status.EIE in — see recEmitCop0DI).
static constexpr u32 EE_COP0_STATUS_OFFSET =
	static_cast<u32>(offsetof(cpuRegisters, CP0) + offsetof(CP0regs, n.Status));

// Dynamically-generated dispatcher stubs (emitted into the head of the code cache by
// recGenDispatchers on every reset; addresses are stable across a reset because the
// stubs regenerate byte-identically at the same location — see recRecompile).
static const void* DispatcherReg = nullptr;        // lookup cpuRegs.pc in recLUT, jump
static const void* DispatcherEvent = nullptr;      // run event test, then fall to DispatcherReg
static const void* JITCompile = nullptr;           // compile block at cpuRegs.pc, then dispatch
static const void* EnterRecompiledCode = nullptr;  // C entry: pin RESTATEPTR, then dispatch
static const void* UnmappedRecLUTPage = nullptr;   // jumped to on an unmapped guest PC
static const void* DispatchBlockDiscard = nullptr; // manual block failed its checksum -> clear + recompile
static const void* DispatchPageReset = nullptr;    // counted manual block -> retry write-protection

// ============================================================================
//  EE block chaining — direct-B tail links   @@MAC_EE_BLOCKLINK@@
// ----------------------------------------------------------------------------
// Ported from the stock arm64 EE recompiler (arm64/aR5900.cpp:145-1355). On a
// statically-known block exit, recEmitEventTestAndDispatch normally emits a
// LUT-indirect tail dispatch (recEmitDispatchToKnownPc: adrp+add+ldr+br). This
// replaces that — on the same path, AFTER the untouched "B.pl DispatcherEvent"
// cycle guard — with a single patchable direct B to the successor block's
// entry, removing an indirect branch + LUT memory load per block exit on the
// hot (no-event-due) path.
//
// Safety invariants:
//  * Event timing is byte-identical: the direct B is reached ONLY when no event
//    is due (it sits after the B.pl guard); a pending event still diverts to
//    DispatcherEvent first. cpuRegs.pc == dispatch_pc is already guaranteed at
//    this tail (recEmitWritePc / branch codegen), so the unlinked form
//    (B -> DispatcherReg, which dispatches from cpuRegs.pc) is exactly the old
//    behavior, and the linked form jumps straight to that same block.
//  * No stale jumps: eeInvalidateLinks (from recClear, which every SMC path
//    funnels through) rewrites any inbound B back to DispatcherReg BEFORE the
//    target's host code is recycled; recResetRaw drops every record when the
//    whole cache is thrown away.
//  * A not-yet-compiled target leaves the site at DispatcherReg (correct
//    fallback); eePatchWaitingPredecessors wires it when the target compiles.
//
// Flip s_eeBlockLinkEnabled to false to fall back to pure LUT dispatch.
static bool s_eeBlockLinkEnabled = true;

struct EEBlockLinkExit
{
	u32 target_pc;      // statically-known successor PC (== cpuRegs.pc at the tail)
	u8* patch_site;     // address of the unconditional B to rewrite
	u8* fallthrough;    // unlinked target (DispatcherReg) — used by unpatch
	u8* current_target; // where patch_site currently points
};

struct EEBlockLinks
{
	u8*             entry;      // block's compiled entry — linked callers jump here
	EEBlockLinkExit exits[2];   // mac emits at most one; keep the stock shape for safety
	u32             num_exits;  // 0 or 1
};

// hwaddr(startpc) -> link record. recHWAddr folds RAM/BIOS mirrors so mirrored
// PCs collapse to one entry, matching the recLUT.
static std::unordered_map<u32, EEBlockLinks> s_eeBlockLinks;
// target_hw -> predecessor hwaddrs waiting for that target to compile.
static std::unordered_map<u32, std::vector<u32>> s_eeWaitingForHw;

// Exit staged by recEmitEventTestAndDispatch, consumed by the registration in
// recRecompile's tail. Reset at the top of each recRecompile.
static bool s_eeLinkStaged = false;
static u32  s_eeLinkTargetPc = 0;
static u8*  s_eeLinkPatchSite = nullptr;

static void eePatchLinkSite(EEBlockLinkExit& exit, u8* target)
{
	if (!exit.patch_site || exit.current_target == target)
		return;
	armEmitJmpPtr(exit.patch_site, target, true);
	exit.current_target = target;
}

static void eeUnpatchLinkSite(EEBlockLinkExit& exit)
{
	eePatchLinkSite(exit, exit.fallthrough); // back to DispatcherReg
}

static u8* eeFindBlockEntry(u32 target_pc)
{
	auto it = s_eeBlockLinks.find(recHWAddr(target_pc));
	return (it == s_eeBlockLinks.end()) ? nullptr : it->second.entry;
}

// Wire this block's exits to any targets already compiled.
static void eeTryForwardLink(EEBlockLinks& block)
{
	for (u32 e = 0; e < block.num_exits; e++)
	{
		if (u8* target_entry = eeFindBlockEntry(block.exits[e].target_pc))
			eePatchLinkSite(block.exits[e], target_entry);
	}
}

// Add this block to the reverse index for each unique exit target.
static void eeIndexBlockExits(u32 my_pc, const EEBlockLinks& bl)
{
	const u32 my_hw = recHWAddr(my_pc);
	for (u32 e = 0; e < bl.num_exits; e++)
	{
		const u32 target_hw = recHWAddr(bl.exits[e].target_pc);
		bool dup = false;
		for (u32 j = 0; j < e; j++)
			dup |= (recHWAddr(bl.exits[j].target_pc) == target_hw);
		if (!dup)
			s_eeWaitingForHw[target_hw].push_back(my_hw);
	}
}

// After a block compiles at my_pc/my_entry, patch any predecessor exits that
// were waiting for this target to jump straight here.
static void eePatchWaitingPredecessors(u32 my_pc, u8* my_entry)
{
	if (!my_entry)
		return;
	const u32 my_hw = recHWAddr(my_pc);
	auto wit = s_eeWaitingForHw.find(my_hw);
	if (wit == s_eeWaitingForHw.end())
		return;
	for (u32 pred_hw : wit->second)
	{
		auto bit = s_eeBlockLinks.find(pred_hw);
		if (bit == s_eeBlockLinks.end())
			continue; // stale — pred was invalidated
		EEBlockLinks& pred = bit->second;
		for (u32 e = 0; e < pred.num_exits; e++)
		{
			EEBlockLinkExit& exit = pred.exits[e];
			if (recHWAddr(exit.target_pc) == my_hw && exit.current_target != my_entry)
				eePatchLinkSite(exit, my_entry);
		}
	}
}

// Unpatch every inbound link whose target is in [start_hw, end_hw), then drop
// records for blocks whose own start is in that range. Called from recClear on
// EVERY SMC invalidation, so its cost is on the hot path during code-streaming
// loads (BF2 online MP.BIN, OPL/DEV9hdd — #283 / #272).
//
// The straight two-pass scan below is O(total compiled blocks) per call. During a
// disc-stream that overwrites EE code in thousands of small spans it becomes an
// O(N*M) host-cycle storm — the sole reason a chained build loads such titles ~4x
// slower in WALL-CLOCK than an unchained one (identical emulated work either way).
// The fast path walks only the CLEARED span instead: inbound links are found via
// the s_eeWaitingForHw reverse index (target_hw -> predecessor hwaddrs, already
// maintained by eeIndexBlockExits), and records are erased by direct key lookup.
// That is O(span/4 + inbound links) — bounded by the write, not by the cache size.
// When the span is wider than the whole block table (rare page/TLB resets) the flat
// scan is cheaper, so keep it as the fallback. recHWAddr is linear over the span
// (asserted by the caller), so word-stepped keys hit every block-start / target hw.
static void eeInvalidateLinksScan(u32 start_hw, u32 end_hw)
{
	for (auto& kv : s_eeBlockLinks)
	{
		EEBlockLinks& pred = kv.second;
		for (u32 e = 0; e < pred.num_exits; e++)
		{
			const u32 t = recHWAddr(pred.exits[e].target_pc);
			if (t >= start_hw && t < end_hw)
				eeUnpatchLinkSite(pred.exits[e]);
		}
	}
	for (auto it = s_eeBlockLinks.begin(); it != s_eeBlockLinks.end();)
	{
		if (it->first >= start_hw && it->first < end_hw)
			it = s_eeBlockLinks.erase(it);
		else
			++it;
	}
}

static void eeInvalidateLinks(u32 start_hw, u32 end_hw)
{
	const u32 span_words = (end_hw - start_hw) >> 2;
	if (span_words > s_eeBlockLinks.size())
	{
		eeInvalidateLinksScan(start_hw, end_hw); // cleared span wider than the table
		return;
	}
	for (u32 hw = start_hw; hw < end_hw; hw += 4)
	{
		// Unpatch inbound direct-B links whose target == hw, via the reverse index.
		auto wit = s_eeWaitingForHw.find(hw);
		if (wit != s_eeWaitingForHw.end())
		{
			for (u32 pred_hw : wit->second)
			{
				auto bit = s_eeBlockLinks.find(pred_hw);
				if (bit == s_eeBlockLinks.end())
					continue; // stale index entry — predecessor already gone
				EEBlockLinks& pred = bit->second;
				for (u32 e = 0; e < pred.num_exits; e++)
				{
					if (recHWAddr(pred.exits[e].target_pc) == hw)
						eeUnpatchLinkSite(pred.exits[e]);
				}
			}
		}
		// Drop the record whose own start == hw.
		auto bit = s_eeBlockLinks.find(hw);
		if (bit != s_eeBlockLinks.end())
			s_eeBlockLinks.erase(bit);
	}
}

// Drop all link state (full cache reset — every block is thrown away).
static void eeResetBlockLinks()
{
	s_eeBlockLinks.clear();
	s_eeWaitingForHw.clear();
	s_eeLinkStaged = false;
}

// Emit a single patchable B for a statically-known block exit (initially ->
// DispatcherReg) and stage it for registration. Exactly one 4-byte B so the
// site is stably patchable. Reached only when no event is due, with
// cpuRegs.pc == pc — so the unlinked DispatcherReg dispatch hits the same block.
static void recEmitLinkableExitToKnownPc(u32 pc)
{
	u8* patch_site;
	{
		// Capture the site INSIDE the scope: its ctor flushes any pending pool first,
		// so patch_site points exactly at the single B (never at a flushed pool).
		a64::SingleEmissionCheckScope guard(armAsm);
		patch_site = armGetCurrentCodePointer();
		const s64 disp = static_cast<s64>(
			reinterpret_cast<intptr_t>(DispatcherReg) - reinterpret_cast<intptr_t>(patch_site));
		pxAssert((disp & 3) == 0 && vixl::IsInt26(disp >> 2));
		armAsm->b(static_cast<int>(disp >> 2));
	}
	s_eeLinkStaged = true;
	s_eeLinkTargetPc = pc;
	s_eeLinkPatchSite = patch_site;
}

// Register a freshly-compiled block (entry + any staged exit) and resolve
// forward/backward links. Called from recRecompile after the block installs.
static void recRegisterBlockLinks(u32 startpc, u8* block_entry)
{
	EEBlockLinks bl{};
	bl.entry = block_entry;
	bl.num_exits = 0;
	if (s_eeLinkStaged)
	{
		EEBlockLinkExit& e = bl.exits[0];
		e.target_pc = s_eeLinkTargetPc;
		e.patch_site = s_eeLinkPatchSite;
		e.fallthrough = const_cast<u8*>(static_cast<const u8*>(DispatcherReg));
		e.current_target = const_cast<u8*>(static_cast<const u8*>(DispatcherReg));
		bl.num_exits = 1;
	}
	// Insert first, then index / forward-link / back-patch (mirrors stock order
	// so a self-loop resolves against the just-inserted record).
	EEBlockLinks& slot = (s_eeBlockLinks[recHWAddr(startpc)] = bl);
	if (slot.num_exits)
		eeIndexBlockExits(startpc, slot);
	eeTryForwardLink(slot);
	eePatchWaitingPredecessors(startpc, block_entry);
}

// Self-modifying-code (SMC) manual protection, mirroring x86 iR5900.cpp. Both arrays are
// indexed by host RAM page (the protection granularity, __pageshift — 16 KB on Apple
// Silicon, 4 KB on x86), so they stay consistent with the vtlb's m_PageProtectInfo. See
// recEmitManualProtection for how these drive the three-tier Write/Manual/uncounted scheme
// that stops the recompile storm on pages that mix code and data (the FMV/IPU case).
alignas(16) static u16 manual_page[Ps2MemSize::TotalRam >> __pageshift];
alignas(16) static u8 manual_counter[Ps2MemSize::TotalRam >> __pageshift];

// Execution / reset / exit plumbing, mirroring the x86 rec (iR5900.cpp).
static bool eeRecExecuting = false;
static bool eeRecNeedsReset = false;
static std::atomic_bool eeRecExitRequested{false};
static volatile u8 eeRecExitSignal = 0;
static jmp_buf s_jmp_buf;
// Landing pad for Cpu->CancelInstruction() raised by an interpreter single-step
// (intExecuteOneInst) op — e.g. a MIPS trap (TGE/TNE/...) whose condition is met.
// Distinct from s_jmp_buf: s_jmp_buf EXITS recExecute, this one re-dispatches so EE
// execution continues. Mirrors the old arm64 backend's m_SetJmp_CancelInstruction
// and the interpreter's intCancelInstruction (Interpreter.cpp).
static jmp_buf s_cancel_jmp_buf;

static void recResetRaw();
static void recGenDispatchers();
static void recRecompile(u32 startpc);
static void recEventTest();
static void recCheckExitAfterInterp();
static void recClear(u32 addr, u32 size);
static void dyna_block_discard(u32 start, u32 sz);
static void dyna_page_reset(u32 start, u32 sz);

// Associate one 64 KB guest page `pagebase+pageidx` with the slot array `mapbase`,
// biased so recPtrToBlock(pc) lands at &mapbase[mappage<<14 + (pc&0xffff)/4]. Direct
// port of x86 recLUT_SetPage (BaseblockEx.h) minus the hwLUT side-table.
static void recLUT_SetPage(uptr* mapbase, uint pagebase, uint pageidx, uint mappage)
{
	const uint page = pagebase + pageidx;
	pxAssert(page < 0x10000);
	recLUT[page] = reinterpret_cast<uptr>(&mapbase[(static_cast<s32>(mappage) - static_cast<s32>(page)) << 14]);
}

// Allocate the per-word slot arrays and build the page table mapping every mapped
// guest page (and its mirrors) onto them. Mirrors x86 recReserveRAM.
static void recReserveLUT()
{
	recLutEntries = (Ps2MemSize::ExposedRam + Ps2MemSize::Rom + Ps2MemSize::Rom1 + Ps2MemSize::Rom2) / 4;
	recLutReserve.assign(recLutEntries, 0);
	recLutUnmapped.assign(_64kb / 4, 0);

	uptr* basepos = recLutReserve.data();
	recRAM = basepos;
	basepos += (Ps2MemSize::ExposedRam / 4);
	recROM = basepos;
	basepos += (Ps2MemSize::Rom / 4);
	recROM1 = basepos;
	basepos += (Ps2MemSize::Rom1 / 4);
	recROM2 = basepos;
	basepos += (Ps2MemSize::Rom2 / 4);

	uptr* const unmapped = recLutUnmapped.data();
	for (int i = 0; i < 0x10000; i++)
		recLUT_SetPage(unmapped, i, 0, 0);

	for (int i = 0x0000; i < static_cast<int>(Ps2MemSize::ExposedRam / 0x10000); i++)
	{
		recLUT_SetPage(recRAM, 0x0000, i, i);
		recLUT_SetPage(recRAM, 0x2000, i, i);
		recLUT_SetPage(recRAM, 0x3000, i, i);
		recLUT_SetPage(recRAM, 0x8000, i, i);
		recLUT_SetPage(recRAM, 0xa000, i, i);
		recLUT_SetPage(recRAM, 0xb000, i, i);
		recLUT_SetPage(recRAM, 0xc000, i, i);
		recLUT_SetPage(recRAM, 0xd000, i, i);
	}

	for (int i = 0x1fc0; i < 0x2000; i++)
	{
		recLUT_SetPage(recROM, 0x0000, i, i - 0x1fc0);
		recLUT_SetPage(recROM, 0x8000, i, i - 0x1fc0);
		recLUT_SetPage(recROM, 0xa000, i, i - 0x1fc0);
	}

	for (int i = 0x1e00; i < 0x1e40; i++)
	{
		recLUT_SetPage(recROM1, 0x0000, i, i - 0x1e00);
		recLUT_SetPage(recROM1, 0x8000, i, i - 0x1e00);
		recLUT_SetPage(recROM1, 0xa000, i, i - 0x1e00);
	}

	for (int i = 0x1e40; i < 0x1e80; i++)
	{
		recLUT_SetPage(recROM2, 0x0000, i, i - 0x1e40);
		recLUT_SetPage(recROM2, 0x8000, i, i - 0x1e40);
		recLUT_SetPage(recROM2, 0xa000, i, i - 0x1e40);
	}
}

// Point every block slot at JITCompile (mapped words) / UnmappedRecLUTPage (unmapped
// pages) so the next jump to any guest PC compiles-on-demand or faults cleanly.
static void recClearLUT()
{
	for (uptr& slot : recLutReserve)
		slot = reinterpret_cast<uptr>(JITCompile);
	for (uptr& slot : recLutUnmapped)
		slot = reinterpret_cast<uptr>(UnmappedRecLUTPage);
}

static void recReserve()
{
	recPtr = SysMemory::GetEERec();
	recPtrEnd = SysMemory::GetEERecEnd() - EE_CONSTPOOL_SIZE;

	s_const_pool.Init(recPtrEnd, EE_CONSTPOOL_SIZE);

	recReserveLUT();
}

static void recShutdown()
{
	s_const_pool.Destroy();

	recLutReserve.clear();
	recLutReserve.shrink_to_fit();
	recLutUnmapped.clear();
	recLutUnmapped.shrink_to_fit();
	recRAM = recROM = recROM1 = recROM2 = nullptr;

	recPtr = nullptr;
	recPtrEnd = nullptr;
}

// @@COP2MODE@@: COP2 interp-routing mode under FullVU0SyncHack (VU0<->EE handshake games,
// e.g. Ratchet: Deadlocked SCUS-97465 via GameDB). Default = mode 12, the on-device-validated
// maximum-native config: SPECIAL1 + ACC families (the hot VMULA/VMADDA matrix chains) +
// transfers + LQC2/SQC2 all NATIVE; only the rare SPECIAL2 residue (DIV/SQRT/RSQRT, VI
// load/stores, ABS/CLIP/MOVE/MR32, ITOF/FTOI, RNG) runs the inline interpreter. Any other
// combination of ≥2 of those groups native breaks the game's transforms (interaction bug,
// individual groups all test clean — see the vc117-vc124 bisect). <DataRoot>/cop2mode.txt
// overrides for debugging (re-read at every rec reset; emit-time only, never the hot path).
static int s_cop2InterpMode = 12;
static bool s_cop2ModeLoaded = false;
static void recLoadCop2Mode()
{
	int m = 12;
	const std::string path = EmuFolders::DataRoot + "/cop2mode.txt";
	if (FILE* f = fopen(path.c_str(), "r"))
	{
		if (fscanf(f, "%d", &m) != 1)
			m = 12;
		fclose(f);
		Console.WriteLn("@@COP2MODE@@ override mode=%d (%s)", m, path.c_str());
	}
	s_cop2InterpMode = m;
	s_cop2ModeLoaded = true;
}

static void recResetRaw()
{
	s_cop2ModeLoaded = false; // @@COP2MODE@@ re-read on every cache reset
	// Rewind the emit cursor, drop all cached trampolines/literals, regenerate the
	// dispatcher stubs at the head of the cache, then reset every block slot. Order
	// matters: recGenDispatchers fills the JITCompile / UnmappedRecLUTPage pointers
	// that recClearLUT writes into the slots.
	recPtr = SysMemory::GetEERec();
	s_const_pool.Reset();
	recGenDispatchers();
	recClearLUT();
	eeRecExitSignal = 0;

	// Drop every block-chaining link — all host code is being discarded, so all
	// patch sites vanish with it and every record must go.
	eeResetBlockLinks();

	// Same for the fastmem backpatch registry: it is keyed by HOST code address, and the
	// rewound buffer reuses those addresses, so a stale LoadstoreBackpatchInfo would
	// mis-backpatch a fresh access (wrong guest_pc/registers/size). @@MAC_FASTMEM_BACKPATCH@@
	vtlb_ClearLoadStoreInfo();

	// Drop all SMC manual-protection state — every block is being thrown away, so the
	// per-page counters/weights must start fresh (mirrors x86 lpReset in recResetRaw).
	std::memset(manual_page, 0, sizeof(manual_page));
	std::memset(manual_counter, 0, sizeof(manual_counter));

	eeRecNeedsReset = false;
}

static void recResetEE()
{
	if (eeRecExecuting)
	{
		// Can't safely rewind the code cache out from under a running block; defer
		// the reset and bail out to the dispatcher loop at the next safe point.
		eeRecNeedsReset = true;
		eeRecExitRequested.store(true, std::memory_order_release);
		eeRecExitSignal = 1;
		cpuRegs.nextEventCycle = 0; // force an event test promptly
		return;
	}

	recResetRaw();
}

static void recStep()
{
	// Debugger single-step. Recompilers fall back to the interpreter for this.
}

// --------------------------------------------------------------------------------------
//  Single-instruction decode + dispatch (Phase 2.3)
// --------------------------------------------------------------------------------------
// MIPS primary opcodes we can translate so far. Everything else falls back to a
// NOP placeholder for now (interpreter remains the active provider — see below).
// The unaligned variants (LWL/LWR/LDL/LDR, SWL/SWR/SDL/SDR) need byte-merge
// codegen and are deferred; scalar + quad aligned access is covered here.
enum : u32
{
	OP_LQ = 0x1e,
	OP_SQ = 0x1f,
	OP_LB = 0x20,
	OP_LH = 0x21,
	OP_LW = 0x23,
	OP_LBU = 0x24,
	OP_LHU = 0x25,
	OP_LWU = 0x27,
	OP_SB = 0x28,
	OP_SH = 0x29,
	OP_SW = 0x2b,
	OP_LD = 0x37,
	OP_SD = 0x3f,
	OP_LWC1 = 0x31,
	OP_SWC1 = 0x39,
	OP_LQC2 = 0x36,
	OP_SQC2 = 0x3e,
};

// Defined below (block-compile helpers) — used by recTranslateOp's COP2 inline path.
static void recEmitInterpInline(u32 op);
static void recEmitWritePc(u32 pc);
static bool recTranslateOp(u32 op, u32 pc);

// Macro-mode native COP2 transfer ops (defined after the M2 sync helpers) — used by
// recTranslateOp's COP2 dispatch.
static void recCFC2();
static void recCTC2();
static void recQMFC2();
static void recQMTC2();
static void recLQC2();
static void recSQC2();

// Macro-mode native COP2 SPECIAL ALU emission (Phase 7.9 / M5). Defined in the aVU
// translation unit (aVU_Macro.inl) so they can reach the static microVU0 single-op
// emitters. recVUMacroIsMode0 classifies; recVUMacroEmitMode0 emits (true if a Mode-0
// op was emitted). The EE rec owns the sync prologue + cycle accounting (the case 0x12
// default below gates the FINISH + native emit on recVUMacroIsMode0, mVUFinishVU0).
bool recVUMacroIsMode0(u32 op);
bool recVUMacroEmitMode0(u32 op);
static void mVUFinishVU0();
static bool recCop2ForceInterp(u32 op); // vc105: FullVU0SyncHack → COP2 on inline interp

struct RecGprConstState
{
	bool known[32] = {};
	u64 value[32] = {};

	RecGprConstState()
	{
		known[0] = true;
		value[0] = 0;
	}
};

static void recConstKillAll(RecGprConstState& state)
{
	state = RecGprConstState();
}

static void recConstSetUnknown(RecGprConstState& state, u32 reg)
{
	if (reg == 0)
		return;

	state.known[reg] = false;
	state.value[reg] = 0;
}

static void recConstSetKnown(RecGprConstState& state, u32 reg, u64 value)
{
	if (reg == 0)
		return;

	state.known[reg] = true;
	state.value[reg] = value;
}

static void recEmitStoreGprConst(u32 reg, u64 value)
{
	if (reg == 0)
		return;

	armAsm->Mov(RSCRATCHADDR, value);
	armAsm->Str(RSCRATCHADDR, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(reg)));
}

static void recConstEmitKnown(RecGprConstState& state, u32 reg, u64 value)
{
	recEmitStoreGprConst(reg, value);
	recConstSetKnown(state, reg, value);
}

static __fi u64 recSignExtend32(u32 value)
{
	return static_cast<u64>(static_cast<s64>(static_cast<s32>(value)));
}

static bool recTryTranslateConstOp(u32 op, RecGprConstState& state)
{
	const u32 opcode = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	const u32 rd = (op >> 11) & 0x1f;
	const u32 sa = (op >> 6) & 0x1f;
	const u32 funct = op & 0x3f;
	const s32 imm = static_cast<s16>(op);
	const u32 imm_u = static_cast<u16>(op);

	auto src_known = [&](u32 reg) -> bool {
		return state.known[reg];
	};
	auto src = [&](u32 reg) -> u64 {
		return state.value[reg];
	};

	switch (opcode)
	{
		case 0x08: // ADDI
		case 0x09: // ADDIU
			if (!src_known(rs))
				return false;
			recConstEmitKnown(state, rt, recSignExtend32(static_cast<u32>(src(rs)) + static_cast<u32>(imm)));
			return true;

		case 0x18: // DADDI
		case 0x19: // DADDIU
			if (!src_known(rs))
				return false;
			recConstEmitKnown(state, rt, src(rs) + static_cast<u64>(static_cast<s64>(imm)));
			return true;

		case 0x0A: // SLTI
			if (!src_known(rs))
				return false;
			recConstEmitKnown(state, rt, (static_cast<s64>(src(rs)) < static_cast<s64>(imm)) ? 1 : 0);
			return true;

		case 0x0B: // SLTIU
			if (!src_known(rs))
				return false;
			recConstEmitKnown(state, rt, (src(rs) < static_cast<u64>(static_cast<s64>(imm))) ? 1 : 0);
			return true;

		case 0x0C: // ANDI
			if (!src_known(rs))
				return false;
			recConstEmitKnown(state, rt, src(rs) & imm_u);
			return true;

		case 0x0D: // ORI
			if (!src_known(rs))
				return false;
			recConstEmitKnown(state, rt, src(rs) | imm_u);
			return true;

		case 0x0E: // XORI
			if (!src_known(rs))
				return false;
			recConstEmitKnown(state, rt, src(rs) ^ imm_u);
			return true;

		case 0x0F: // LUI
			recConstEmitKnown(state, rt, recSignExtend32(static_cast<u32>(imm_u) << 16));
			return true;

		case 0x00:
			switch (funct)
			{
				case 0x00: // SLL
					if (!src_known(rt)) return false;
					recConstEmitKnown(state, rd, recSignExtend32(static_cast<u32>(src(rt)) << sa));
					return true;
				case 0x02: // SRL
					if (!src_known(rt)) return false;
					recConstEmitKnown(state, rd, recSignExtend32(static_cast<u32>(src(rt)) >> sa));
					return true;
				case 0x03: // SRA
					if (!src_known(rt)) return false;
					recConstEmitKnown(state, rd, recSignExtend32(static_cast<u32>(static_cast<s32>(static_cast<u32>(src(rt))) >> sa)));
					return true;
				case 0x04: // SLLV
					if (!src_known(rt) || !src_known(rs)) return false;
					recConstEmitKnown(state, rd, recSignExtend32(static_cast<u32>(src(rt)) << (src(rs) & 0x1f)));
					return true;
				case 0x06: // SRLV
					if (!src_known(rt) || !src_known(rs)) return false;
					recConstEmitKnown(state, rd, recSignExtend32(static_cast<u32>(src(rt)) >> (src(rs) & 0x1f)));
					return true;
				case 0x07: // SRAV
					if (!src_known(rt) || !src_known(rs)) return false;
					recConstEmitKnown(state, rd, recSignExtend32(static_cast<u32>(static_cast<s32>(static_cast<u32>(src(rt))) >> (src(rs) & 0x1f))));
					return true;
				case 0x14: // DSLLV
					if (!src_known(rt) || !src_known(rs)) return false;
					recConstEmitKnown(state, rd, src(rt) << (src(rs) & 0x3f));
					return true;
				case 0x16: // DSRLV
					if (!src_known(rt) || !src_known(rs)) return false;
					recConstEmitKnown(state, rd, src(rt) >> (src(rs) & 0x3f));
					return true;
				case 0x17: // DSRAV
					if (!src_known(rt) || !src_known(rs)) return false;
					recConstEmitKnown(state, rd, static_cast<u64>(static_cast<s64>(src(rt)) >> (src(rs) & 0x3f)));
					return true;
				case 0x38: // DSLL
					if (!src_known(rt)) return false;
					recConstEmitKnown(state, rd, src(rt) << sa);
					return true;
				case 0x3A: // DSRL
					if (!src_known(rt)) return false;
					recConstEmitKnown(state, rd, src(rt) >> sa);
					return true;
				case 0x3B: // DSRA
					if (!src_known(rt)) return false;
					recConstEmitKnown(state, rd, static_cast<u64>(static_cast<s64>(src(rt)) >> sa));
					return true;
				case 0x3C: // DSLL32
					if (!src_known(rt)) return false;
					recConstEmitKnown(state, rd, src(rt) << (sa + 32));
					return true;
				case 0x3E: // DSRL32
					if (!src_known(rt)) return false;
					recConstEmitKnown(state, rd, src(rt) >> (sa + 32));
					return true;
				case 0x3F: // DSRA32
					if (!src_known(rt)) return false;
					recConstEmitKnown(state, rd, static_cast<u64>(static_cast<s64>(src(rt)) >> (sa + 32)));
					return true;

				case 0x20: // ADD
				case 0x21: // ADDU
					if (!src_known(rs) || !src_known(rt)) return false;
					recConstEmitKnown(state, rd, recSignExtend32(static_cast<u32>(src(rs)) + static_cast<u32>(src(rt))));
					return true;
				case 0x22: // SUB
				case 0x23: // SUBU
					if (!src_known(rs) || !src_known(rt)) return false;
					recConstEmitKnown(state, rd, recSignExtend32(static_cast<u32>(src(rs)) - static_cast<u32>(src(rt))));
					return true;
				case 0x24: // AND
					if (!src_known(rs) || !src_known(rt)) return false;
					recConstEmitKnown(state, rd, src(rs) & src(rt));
					return true;
				case 0x25: // OR
					if (!src_known(rs) || !src_known(rt)) return false;
					recConstEmitKnown(state, rd, src(rs) | src(rt));
					return true;
				case 0x26: // XOR
					if (!src_known(rs) || !src_known(rt)) return false;
					recConstEmitKnown(state, rd, src(rs) ^ src(rt));
					return true;
				case 0x27: // NOR
					if (!src_known(rs) || !src_known(rt)) return false;
					recConstEmitKnown(state, rd, ~(src(rs) | src(rt)));
					return true;
				case 0x2A: // SLT
					if (!src_known(rs) || !src_known(rt)) return false;
					recConstEmitKnown(state, rd, (static_cast<s64>(src(rs)) < static_cast<s64>(src(rt))) ? 1 : 0);
					return true;
				case 0x2B: // SLTU
					if (!src_known(rs) || !src_known(rt)) return false;
					recConstEmitKnown(state, rd, (src(rs) < src(rt)) ? 1 : 0);
					return true;
				case 0x2C: // DADD
				case 0x2D: // DADDU
					if (!src_known(rs) || !src_known(rt)) return false;
					recConstEmitKnown(state, rd, src(rs) + src(rt));
					return true;
				case 0x2E: // DSUB
				case 0x2F: // DSUBU
					if (!src_known(rs) || !src_known(rt)) return false;
					recConstEmitKnown(state, rd, src(rs) - src(rt));
					return true;
				case 0x0A: // MOVZ
					if (!src_known(rt)) return false;
					if (src(rt) != 0)
						return true;
					if (!src_known(rs)) return false;
					recConstEmitKnown(state, rd, src(rs));
					return true;
				case 0x0B: // MOVN
					if (!src_known(rt)) return false;
					if (src(rt) == 0)
						return true;
					if (!src_known(rs)) return false;
					recConstEmitKnown(state, rd, src(rs));
					return true;
				default:
					return false;
			}

		default:
			return false;
	}
}

static void recConstApplyNativeEffects(u32 op, RecGprConstState& state)
{
	const u32 opcode = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	const u32 rd = (op >> 11) & 0x1f;
	const u32 funct = op & 0x3f;

	switch (opcode)
	{
		case 0x00:
			switch (funct)
			{
				case 0x11: // MTHI
				case 0x13: // MTLO
				case 0x18: // MULT
				case 0x19: // MULTU
				case 0x1A: // DIV
				case 0x1B: // DIVU
					if (funct == 0x18 || funct == 0x19)
						recConstSetUnknown(state, rd);
					return;
				default:
					recConstSetUnknown(state, rd);
					return;
			}

		case 0x08: case 0x09: case 0x0A: case 0x0B:
		case 0x0C: case 0x0D: case 0x0E: case 0x0F:
		case 0x18: case 0x19:
			recConstSetUnknown(state, rt);
			return;

		case OP_LQ: case OP_LB: case OP_LH: case OP_LW:
		case OP_LBU: case OP_LHU: case OP_LWU: case OP_LD:
		case 0x22: case 0x26: case 0x1A: case 0x1B: // LWL/LWR/LDL/LDR merge into rt
			recConstSetUnknown(state, rt);
			return;

		case 0x11: // COP1: MFC1/CFC1 write rt, other native FPU ops do not touch GPRs.
			if (rs == 0x00 || rs == 0x02)
				recConstSetUnknown(state, rt);
			return;

		case 0x10: // COP0 inline interpreter may touch CPU state.
		case 0x12: // COP2 inline interpreter may move VU data through GPRs.
		case OP_LQC2:
		case OP_SQC2:
			recConstKillAll(state);
			return;

		case 0x1C:
			recConstSetUnknown(state, rd);
			return;

		default:
			return;
	}
}

static bool recTranslateOpWithConst(u32 op, RecGprConstState& state)
{
	if (recTryTranslateConstOp(op, state))
		return true;

	if (!recTranslateOp(op, /*pc*/ 0)) // dead path (recTranslateOpWithConst has no callers)
	{
		recConstKillAll(state);
		return false;
	}

	recConstApplyNativeEffects(op, state);
	return true;
}

static bool recConstApplyCachedEffects(u32 op, RecGprConstState& state)
{
	const u32 opcode = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	const u32 rd = (op >> 11) & 0x1f;
	const u32 sa = (op >> 6) & 0x1f;
	const u32 funct = op & 0x3f;
	const s32 imm = static_cast<s16>(op);
	const u32 imm_u = static_cast<u16>(op);

	auto known = [&](u32 reg) -> bool {
		return state.known[reg];
	};
	auto value = [&](u32 reg) -> u64 {
		return state.value[reg];
	};
	auto set_known_or_unknown = [&](u32 reg, bool is_known, u64 val = 0) {
		if (is_known)
			recConstSetKnown(state, reg, val);
		else
			recConstSetUnknown(state, reg);
	};

	switch (opcode)
	{
		case 0x08: // ADDI
		case 0x09: // ADDIU
			set_known_or_unknown(rt, known(rs), recSignExtend32(static_cast<u32>(value(rs)) + static_cast<u32>(imm)));
			return true;

		case 0x18: // DADDI
		case 0x19: // DADDIU
			set_known_or_unknown(rt, known(rs), value(rs) + static_cast<u64>(static_cast<s64>(imm)));
			return true;

		case 0x0A: // SLTI
			set_known_or_unknown(rt, known(rs), (static_cast<s64>(value(rs)) < static_cast<s64>(imm)) ? 1 : 0);
			return true;

		case 0x0B: // SLTIU
			set_known_or_unknown(rt, known(rs), (value(rs) < static_cast<u64>(static_cast<s64>(imm))) ? 1 : 0);
			return true;

		case 0x0C: // ANDI
			set_known_or_unknown(rt, known(rs), value(rs) & imm_u);
			return true;

		case 0x0D: // ORI
			set_known_or_unknown(rt, known(rs), value(rs) | imm_u);
			return true;

		case 0x0E: // XORI
			set_known_or_unknown(rt, known(rs), value(rs) ^ imm_u);
			return true;

		case 0x0F: // LUI
			recConstSetKnown(state, rt, recSignExtend32(static_cast<u32>(imm_u) << 16));
			return true;

		case 0x00:
			switch (funct)
			{
				case 0x00: // SLL
					set_known_or_unknown(rd, known(rt), recSignExtend32(static_cast<u32>(value(rt)) << sa));
					return true;
				case 0x02: // SRL
					set_known_or_unknown(rd, known(rt), recSignExtend32(static_cast<u32>(value(rt)) >> sa));
					return true;
				case 0x03: // SRA
					set_known_or_unknown(rd, known(rt), recSignExtend32(static_cast<u32>(static_cast<s32>(static_cast<u32>(value(rt))) >> sa)));
					return true;
				case 0x04: // SLLV
					set_known_or_unknown(rd, known(rt) && known(rs), recSignExtend32(static_cast<u32>(value(rt)) << (value(rs) & 0x1f)));
					return true;
				case 0x06: // SRLV
					set_known_or_unknown(rd, known(rt) && known(rs), recSignExtend32(static_cast<u32>(value(rt)) >> (value(rs) & 0x1f)));
					return true;
				case 0x07: // SRAV
					set_known_or_unknown(rd, known(rt) && known(rs), recSignExtend32(static_cast<u32>(static_cast<s32>(static_cast<u32>(value(rt))) >> (value(rs) & 0x1f))));
					return true;
				case 0x14: // DSLLV
					set_known_or_unknown(rd, known(rt) && known(rs), value(rt) << (value(rs) & 0x3f));
					return true;
				case 0x16: // DSRLV
					set_known_or_unknown(rd, known(rt) && known(rs), value(rt) >> (value(rs) & 0x3f));
					return true;
				case 0x17: // DSRAV
					set_known_or_unknown(rd, known(rt) && known(rs), static_cast<u64>(static_cast<s64>(value(rt)) >> (value(rs) & 0x3f)));
					return true;
				case 0x38: // DSLL
					set_known_or_unknown(rd, known(rt), value(rt) << sa);
					return true;
				case 0x3A: // DSRL
					set_known_or_unknown(rd, known(rt), value(rt) >> sa);
					return true;
				case 0x3B: // DSRA
					set_known_or_unknown(rd, known(rt), static_cast<u64>(static_cast<s64>(value(rt)) >> sa));
					return true;
				case 0x3C: // DSLL32
					set_known_or_unknown(rd, known(rt), value(rt) << (sa + 32));
					return true;
				case 0x3E: // DSRL32
					set_known_or_unknown(rd, known(rt), value(rt) >> (sa + 32));
					return true;
				case 0x3F: // DSRA32
					set_known_or_unknown(rd, known(rt), static_cast<u64>(static_cast<s64>(value(rt)) >> (sa + 32)));
					return true;
				case 0x20: // ADD
				case 0x21: // ADDU
					set_known_or_unknown(rd, known(rs) && known(rt), recSignExtend32(static_cast<u32>(value(rs)) + static_cast<u32>(value(rt))));
					return true;
				case 0x22: // SUB
				case 0x23: // SUBU
					set_known_or_unknown(rd, known(rs) && known(rt), recSignExtend32(static_cast<u32>(value(rs)) - static_cast<u32>(value(rt))));
					return true;
				case 0x2C: // DADD
				case 0x2D: // DADDU
					set_known_or_unknown(rd, known(rs) && known(rt), value(rs) + value(rt));
					return true;
				case 0x2E: // DSUB
				case 0x2F: // DSUBU
					set_known_or_unknown(rd, known(rs) && known(rt), value(rs) - value(rt));
					return true;
				case 0x24: // AND
					set_known_or_unknown(rd, known(rs) && known(rt), value(rs) & value(rt));
					return true;
				case 0x25: // OR
					set_known_or_unknown(rd, known(rs) && known(rt), value(rs) | value(rt));
					return true;
				case 0x26: // XOR
					set_known_or_unknown(rd, known(rs) && known(rt), value(rs) ^ value(rt));
					return true;
				case 0x27: // NOR
					set_known_or_unknown(rd, known(rs) && known(rt), ~(value(rs) | value(rt)));
					return true;
				case 0x2A: // SLT
					set_known_or_unknown(rd, known(rs) && known(rt), (static_cast<s64>(value(rs)) < static_cast<s64>(value(rt))) ? 1 : 0);
					return true;
				case 0x2B: // SLTU
					set_known_or_unknown(rd, known(rs) && known(rt), (value(rs) < value(rt)) ? 1 : 0);
					return true;
				case 0x0A: // MOVZ
					if (rd == 0 || rs == rd)
						return true;
					if (!known(rt))
						recConstSetUnknown(state, rd);
					else if (value(rt) == 0)
						set_known_or_unknown(rd, known(rs), value(rs));
					return true;
				case 0x0B: // MOVN
					if (rd == 0 || rs == rd)
						return true;
					if (!known(rt))
						recConstSetUnknown(state, rd);
					else if (value(rt) != 0)
						set_known_or_unknown(rd, known(rs), value(rs));
					return true;
				case 0x10: // MFHI
				case 0x12: // MFLO
					recConstSetUnknown(state, rd);
					return true;
				default:
					return false;
			}

		default:
			return false;
	}
}

struct RecGprCacheEntry
{
	bool valid = false;
	bool dirty = false;
	u32 guest = 0;
	u32 age = 0;
};

struct RecGprCacheState
{
	RecGprCacheEntry entries[7];
	u32 age = 1;
};

// AAPCS64 callee-saved registers dedicated to the guest-GPR cache. x19/x21 hold
// &cpuRegs / the vtlb vmap base; x28 is now pinned as RFASTMEMBASE (the host-MMU
// fastmem base — see recGenDispatchers, @@MAC_FASTMEM_BACKPATCH@@), so it was dropped
// from the cache, leaving 7 slots. NOTE: x23-x26 double as microVU flag regs mVU_F0-F3
// (safe because the cache is killed before any COP2/VU0-macro emit); x27/x28 are outside
// the VU allocator's tracked range, which is why x28 is safe to pin. All of these survive
// the C helper calls a block makes (vtlb slow path, inline interpreter ops): the VU rec
// saves x19-x28 in its prologue, the IOP rec only touches x19 (saved), and the EE rec
// itself exits via longjmp which restores the full caller context.
static constexpr int REC_GPR_CACHE_REGS[7] = {20, 22, 23, 24, 25, 26, 27};
static_assert(std::size(RecGprCacheState{}.entries) == std::size(REC_GPR_CACHE_REGS),
	"guest-GPR cache entry count must match the register list");

static const a64::Register& recCacheReg(size_t index)
{
	return armXRegister(REC_GPR_CACHE_REGS[index]);
}

static const a64::Register& recCacheWReg(size_t index)
{
	return armWRegister(REC_GPR_CACHE_REGS[index]);
}

static void recCacheEmitFlushEntry(const RecGprCacheEntry& entry, size_t index)
{
	if (!entry.valid || !entry.dirty)
		return;

	armAsm->Str(recCacheReg(index), a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(entry.guest)));
}

static int recCacheFind(const RecGprCacheState& cache, u32 guest)
{
	for (size_t i = 0; i < std::size(cache.entries); i++)
	{
		if (cache.entries[i].valid && cache.entries[i].guest == guest)
			return static_cast<int>(i);
	}

	return -1;
}

static void recCacheFlushEntry(RecGprCacheState& cache, size_t index)
{
	RecGprCacheEntry& entry = cache.entries[index];
	if (!entry.valid || !entry.dirty)
		return;

	recCacheEmitFlushEntry(entry, index);
	entry.dirty = false;
}

// Drop a guest register from the cache without writing it back. Only correct when
// the instruction fully redefines the guest register in memory (e.g. LQ).
static void recCacheDiscardGuest(RecGprCacheState& cache, u32 guest)
{
	if (guest == 0)
		return;

	const int found = recCacheFind(cache, guest);
	if (found >= 0)
		cache.entries[static_cast<size_t>(found)] = RecGprCacheEntry();
}

// Write a single guest register back to cpuRegs if it is cached dirty. The entry
// stays valid (clean), so later ops can keep using the cached copy.
static void recCacheFlushGuest(RecGprCacheState& cache, u32 guest)
{
	if (guest == 0)
		return;

	const int found = recCacheFind(cache, guest);
	if (found >= 0)
		recCacheFlushEntry(cache, static_cast<size_t>(found));
}

static void recCacheFlushAll(RecGprCacheState& cache)
{
	for (size_t i = 0; i < std::size(cache.entries); i++)
		recCacheFlushEntry(cache, i);
}

static void recCacheEmitFlushAll(const RecGprCacheState& cache)
{
	for (size_t i = 0; i < std::size(cache.entries); i++)
		recCacheEmitFlushEntry(cache.entries[i], i);
}

static void recCacheKillAll(RecGprCacheState& cache)
{
	cache = RecGprCacheState();
}

static size_t recCacheAllocate(RecGprCacheState& cache, u32 guest, u32 pin_a = 0xff, u32 pin_b = 0xff)
{
	int found = recCacheFind(cache, guest);
	if (found >= 0)
	{
		cache.entries[found].age = cache.age++;
		return static_cast<size_t>(found);
	}

	size_t victim = std::size(cache.entries);
	u32 oldest = UINT32_MAX;
	for (size_t i = 0; i < std::size(cache.entries); i++)
	{
		const RecGprCacheEntry& entry = cache.entries[i];
		if (!entry.valid)
		{
			victim = i;
			break;
		}
		if (entry.guest == pin_a || entry.guest == pin_b)
			continue;
		if (entry.age < oldest)
		{
			oldest = entry.age;
			victim = i;
		}
	}

	if (victim == std::size(cache.entries))
	{
		// All cache registers are pinned by this instruction. This should be rare, but
		// flushing keeps the fallback path simple and correct.
		recCacheFlushAll(cache);
		recCacheKillAll(cache);
		victim = 0;
	}
	else
	{
		recCacheFlushEntry(cache, victim);
	}

	RecGprCacheEntry& entry = cache.entries[victim];
	entry.valid = true;
	entry.dirty = false;
	entry.guest = guest;
	entry.age = cache.age++;
	return victim;
}

static const a64::Register& recCacheLoad(RecGprCacheState& cache, u32 guest)
{
	if (guest == 0)
		return a64::xzr;

	int found = recCacheFind(cache, guest);
	const bool already_cached = (found >= 0);
	const size_t index = already_cached ? static_cast<size_t>(found) : recCacheAllocate(cache, guest);
	RecGprCacheEntry& entry = cache.entries[index];
	entry.age = cache.age++;
	if (!already_cached)
		armAsm->Ldr(recCacheReg(index), a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(guest)));

	return recCacheReg(index);
}

static const a64::Register& recCacheDest(RecGprCacheState& cache, u32 guest, u32 pin_a = 0xff, u32 pin_b = 0xff)
{
	if (guest == 0)
		return a64::xzr;

	const size_t index = recCacheAllocate(cache, guest, pin_a, pin_b);
	cache.entries[index].dirty = true;
	return recCacheReg(index);
}

static void recEmitCachedEffectiveAddr(RecGprCacheState& cache, const RecGprConstState& const_state,
	u32 rs, s32 imm, const a64::Register& addr)
{
	if (rs == 0)
	{
		armAsm->Mov(addr.W(), imm);
		return;
	}

	// Const-propagated address: GPR[rs] is a tracked compile-time constant (LUI/ORI
	// pairs, hardware register bases, ...), so the whole effective address collapses
	// to one immediate move instead of a cache load + add.
	if (const_state.known[rs])
	{
		const u32 ea = static_cast<u32>(const_state.value[rs]) + static_cast<u32>(imm);
		armAsm->Mov(addr.W(), ea);
		return;
	}

	const a64::Register& src = recCacheLoad(cache, rs);
	if (!addr.W().Is(src.W()))
		armAsm->Mov(addr.W(), src.W());
	if (imm != 0)
		armAsm->Add(addr.W(), addr.W(), imm);
}

static void recEmitVmapHostPointer(const a64::Register& host, const a64::Register& addr, a64::Label* slow_path)
{
	static_assert(sizeof(vtlb_private::VTLBVirtual) == sizeof(uptr), "VTLBVirtual is expected to be a raw pointer-sized entry");

	armAsm->Lsr(a64::w11, addr.W(), vtlb_private::VTLB_PAGE_BITS);
	armAsm->Ldr(host, a64::MemOperand(REVTLBPTR, a64::x11, a64::LSL, 3));
	armAsm->Add(host, host, addr.X());
	armAsm->Tbnz(host, sizeof(uptr) * 8 - 1, slow_path);
}

static void recEmitCachedDirectLoad(u32 bits, bool sign, const a64::Register& dst, const a64::Register& host)
{
	switch (bits)
	{
		case 8:
			sign ? armAsm->Ldrsb(dst.X(), a64::MemOperand(host)) : armAsm->Ldrb(dst.W(), a64::MemOperand(host));
			break;
		case 16:
			sign ? armAsm->Ldrsh(dst.X(), a64::MemOperand(host)) : armAsm->Ldrh(dst.W(), a64::MemOperand(host));
			break;
		case 32:
			sign ? armAsm->Ldrsw(dst.X(), a64::MemOperand(host)) : armAsm->Ldr(dst.W(), a64::MemOperand(host));
			break;
		case 64:
			armAsm->Ldr(dst.X(), a64::MemOperand(host));
			break;
		jNO_DEFAULT
	}
}

static void recEmitCachedDirectStore(u32 bits, const a64::Register& src, const a64::Register& host)
{
	switch (bits)
	{
		case 8:
			armAsm->Strb(src.W(), a64::MemOperand(host));
			break;
		case 16:
			armAsm->Strh(src.W(), a64::MemOperand(host));
			break;
		case 32:
			armAsm->Str(src.W(), a64::MemOperand(host));
			break;
		case 64:
			armAsm->Str(src.X(), a64::MemOperand(host));
			break;
		jNO_DEFAULT
	}
}

// Host-MMU fastmem backpatch toggle (@@MAC_FASTMEM_BACKPATCH@@). When on, EE integer
// load/store emit a single Ldr/Str through RFASTMEMBASE (x28); a fault backpatches to the
// slow path via vtlb_DynBackpatchLoadStore (RecStubs.cpp). Flip off = inline-vmap fallback.
static bool s_eeFastmemBackpatch = true;

static bool recUseBackpatchFastmem(u32 pc)
{
	// Skip PCs that already faulted once (settled MMIO): re-emit the vmap path so we don't
	// re-backpatch the same instruction on every recompile.
	return s_eeFastmemBackpatch && CHECK_FASTMEM && !vtlb_IsFaultingPC(pc);
}

// Record a single fastmem Ldr/Str for SIGSEGV backpatch. code_start must point at exactly
// one 4-byte access instruction (the whole premise of host-MMU backpatch).
static void recRecordFastmem(const u8* code_start, u32 pc, u8 addr_reg, u8 data_reg,
	u32 bits, bool is_signed, bool is_load)
{
	const u32 code_size = static_cast<u32>(armGetCurrentCodePointer() - code_start);
	pxAssert(code_size == 4);
	vtlb_AddLoadStoreInfo(reinterpret_cast<uptr>(code_start), code_size, pc,
		/*gpr_bitmask*/ 0, /*fpr_bitmask*/ 0, addr_reg, data_reg,
		static_cast<u8>(bits), is_signed, is_load, /*is_fpr*/ false);
}

// Exposed for aR5900FPU.cpp (LWC1/SWC1 live in a separate translation unit): emit a single-
// instruction backpatch fastmem 32-bit access when eligible. The 32-bit vaddr must already be
// in RXARG1 (x0, zero-extended). `data` is the value register (load: destination; store:
// source). Returns true if fastmem was emitted, so the caller then skips the vmap path.
// @@MAC_FASTMEM_BACKPATCH@@
bool armTryEmitFastmemScalar32(u32 pc, bool is_load, const a64::Register& data)
{
	if (!recUseBackpatchFastmem(pc))
		return false;
	const u8* code_start = armGetCurrentCodePointer();
	if (is_load)
		armAsm->Ldr(data.W(), a64::MemOperand(RFASTMEMBASE, RXARG1));
	else
		armAsm->Str(data.W(), a64::MemOperand(RFASTMEMBASE, RXARG1));
	recRecordFastmem(code_start, pc, RXARG1.GetCode(), data.GetCode(), 32, /*sign*/ false, is_load);
	return true;
}

static bool recTryTranslateCachedLoad(u32 bits, bool sign, u32 rt, u32 rs, s32 imm,
	RecGprCacheState& cache, const RecGprConstState& const_state, u32 pc)
{
	static const a64::Register RADDR = a64::x9;
	static const a64::Register RHOST = a64::x10;
	static const a64::Register RTEMP = a64::x11;

	recEmitCachedEffectiveAddr(cache, const_state, rs, imm, RADDR);
	const RecGprCacheState pre_load_cache = cache;

	const a64::Register& dst = (rt == 0) ? RTEMP : recCacheDest(cache, rt, rs);

	if (recUseBackpatchFastmem(pc))
	{
		// Single register-offset load through the pinned fastmem base. A handler/MMIO/unmapped
		// page faults -> HandlePageFault -> vtlb_BackpatchLoadStore -> the thunk. No slow branch,
		// no cache flush: the fast path is the common one. dst/RADDR high bits are already clean
		// (RADDR = zero-extended 32-bit vaddr), so [x28 + vaddr] lands inside the 4 GB window.
		const u8* code_start = armGetCurrentCodePointer();
		switch (bits)
		{
			case 8:  sign ? armAsm->Ldrsb(dst.X(), a64::MemOperand(RFASTMEMBASE, RADDR))
			              : armAsm->Ldrb(dst.W(), a64::MemOperand(RFASTMEMBASE, RADDR)); break;
			case 16: sign ? armAsm->Ldrsh(dst.X(), a64::MemOperand(RFASTMEMBASE, RADDR))
			              : armAsm->Ldrh(dst.W(), a64::MemOperand(RFASTMEMBASE, RADDR)); break;
			case 32: sign ? armAsm->Ldrsw(dst.X(), a64::MemOperand(RFASTMEMBASE, RADDR))
			              : armAsm->Ldr(dst.W(), a64::MemOperand(RFASTMEMBASE, RADDR)); break;
			case 64: armAsm->Ldr(dst.X(), a64::MemOperand(RFASTMEMBASE, RADDR)); break;
		}
		recRecordFastmem(code_start, pc, RADDR.GetCode(), dst.GetCode(), bits, sign, /*is_load*/ true);
		return true;
	}

	a64::Label slow_path;
	a64::Label done;
	recEmitVmapHostPointer(RHOST, RADDR, &slow_path);
	recEmitCachedDirectLoad(bits, sign, dst, RHOST);
	armAsm->B(&done);

	armAsm->Bind(&slow_path);
	recCacheEmitFlushAll(pre_load_cache);
	armEmitVtlbRead(bits, sign, RXRET, RADDR);
	if (rt != 0 && !dst.Is(RXRET))
		armAsm->Mov(dst, RXRET);

	armAsm->Bind(&done);
	return true;
}

static bool recTryTranslateCachedStore(u32 bits, u32 rt, u32 rs, s32 imm,
	RecGprCacheState& cache, const RecGprConstState& const_state, u32 pc)
{
	static const a64::Register RADDR = a64::x9;
	static const a64::Register RHOST = a64::x10;

	recEmitCachedEffectiveAddr(cache, const_state, rs, imm, RADDR);
	const a64::Register& src = recCacheLoad(cache, rt);
	const RecGprCacheState pre_store_cache = cache;

	if (recUseBackpatchFastmem(pc))
	{
		// Single register-offset store through the pinned fastmem base. A store into a
		// write-protected code page faults through HandlePageFault's ProtMode_Write branch
		// (mmap_ClearCpuBlock + retry), NOT the backpatch path — SMC stays correct.
		const u8* code_start = armGetCurrentCodePointer();
		switch (bits)
		{
			case 8:  armAsm->Strb(src.W(), a64::MemOperand(RFASTMEMBASE, RADDR)); break;
			case 16: armAsm->Strh(src.W(), a64::MemOperand(RFASTMEMBASE, RADDR)); break;
			case 32: armAsm->Str(src.W(), a64::MemOperand(RFASTMEMBASE, RADDR)); break;
			case 64: armAsm->Str(src.X(), a64::MemOperand(RFASTMEMBASE, RADDR)); break;
		}
		recRecordFastmem(code_start, pc, RADDR.GetCode(), src.GetCode(), bits, /*is_signed*/ false, /*is_load*/ false);
		return true;
	}

	a64::Label slow_path;
	a64::Label done;
	recEmitVmapHostPointer(RHOST, RADDR, &slow_path);
	recEmitCachedDirectStore(bits, src, RHOST);
	armAsm->B(&done);

	armAsm->Bind(&slow_path);
	recCacheEmitFlushAll(pre_store_cache);
	armEmitVtlbWrite(bits, RADDR, src);

	armAsm->Bind(&done);
	return true;
}

static bool recTryTranslateCachedLoadQuad(u32 rt, u32 rs, s32 imm,
	RecGprCacheState& cache, const RecGprConstState& const_state, u32 pc)
{
	static const a64::Register RADDR = a64::x9;
	static const a64::Register RHOST = a64::x10;

	// Effective address, forced 16-byte aligned (the EE silently aligns 128-bit
	// accesses; matches the x86 recLQ `xAND(arg1regd, ~0x0F)` and armEmitLoadQuad).
	recEmitCachedEffectiveAddr(cache, const_state, rs, imm, RADDR);
	armAsm->And(RADDR.W(), RADDR.W(), ~0x0F);

	// Snapshot taken before the rt discard below on purpose: if the slow-path read
	// hits a TLB miss the handler longjmps out of the block, so at the call site
	// every guest register — including rt's old dirty low half — must already be
	// flushed to cpuRegs.
	const RecGprCacheState pre_load_cache = cache;

	// LQ overwrites the full 128-bit destination, but the scalar GPR cache only
	// tracks the low 64 bits. Discard any cached low half so a stale dirty entry
	// can't be flushed over the freshly loaded quad later. Done after the address
	// computation so rt==rs still uses the pre-load value above.
	recCacheDiscardGuest(cache, rt);

	if (recUseBackpatchFastmem(pc))
	{
		// Single 128-bit register-offset load through the fastmem base; a fault backpatches to
		// the thunk (size 128 -> vtlb_memRead128). Perform the read even when rt==0 (MMIO side
		// effects). RADDR is the 16-byte-aligned zero-extended vaddr -> stays in the 4 GB window.
		const u8* code_start = armGetCurrentCodePointer();
		armAsm->Ldr(RQSCRATCH, a64::MemOperand(RFASTMEMBASE, RADDR));
		vtlb_AddLoadStoreInfo(reinterpret_cast<uptr>(code_start),
			static_cast<u32>(armGetCurrentCodePointer() - code_start), pc,
			/*gpr*/ 0, /*fpr*/ 0, RADDR.GetCode(), RQSCRATCH.GetCode(),
			/*size*/ 128, /*sign*/ false, /*is_load*/ true, /*is_fpr*/ false);
		if (rt != 0)
			armAsm->Str(RQSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
		return true;
	}

	a64::Label slow_path;
	a64::Label done;
	recEmitVmapHostPointer(RHOST, RADDR, &slow_path);
	armAsm->Ldr(RQSCRATCH, a64::MemOperand(RHOST));
	if (rt != 0)
		armAsm->Str(RQSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	armAsm->B(&done);

	armAsm->Bind(&slow_path);
	recCacheEmitFlushAll(pre_load_cache);
	// Perform the read even when rt==0 (the access can have I/O side effects).
	armEmitVtlbReadQuad(RQSCRATCH, RADDR);
	if (rt != 0)
		armAsm->Str(RQSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));

	armAsm->Bind(&done);
	return true;
}

static bool recTryTranslateCachedStoreQuad(u32 rt, u32 rs, s32 imm,
	RecGprCacheState& cache, const RecGprConstState& const_state, u32 pc)
{
	static const a64::Register RADDR = a64::x9;
	static const a64::Register RHOST = a64::x10;

	recEmitCachedEffectiveAddr(cache, const_state, rs, imm, RADDR);
	armAsm->And(RADDR.W(), RADDR.W(), ~0x0F);

	// SQ reads the whole 128-bit GPR from cpuRegs. If prior cached scalar ops
	// dirtied the low half of rt, write it back first so the vector load sees a
	// coherent register (rt==0 reads the always-zero GPR[0] slot, no special case).
	recCacheFlushGuest(cache, rt);
	armAsm->Ldr(RQSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
	const RecGprCacheState pre_store_cache = cache;

	if (recUseBackpatchFastmem(pc))
	{
		// Single 128-bit store through the fastmem base. A store into a write-protected code
		// page faults through HandlePageFault's ProtMode_Write branch (clear + retry), NOT the
		// backpatch decoder — SMC stays correct (same as the scalar/vmap quad store).
		const u8* code_start = armGetCurrentCodePointer();
		armAsm->Str(RQSCRATCH, a64::MemOperand(RFASTMEMBASE, RADDR));
		vtlb_AddLoadStoreInfo(reinterpret_cast<uptr>(code_start),
			static_cast<u32>(armGetCurrentCodePointer() - code_start), pc,
			/*gpr*/ 0, /*fpr*/ 0, RADDR.GetCode(), RQSCRATCH.GetCode(),
			/*size*/ 128, /*sign*/ false, /*is_load*/ false, /*is_fpr*/ false);
		return true;
	}

	a64::Label slow_path;
	a64::Label done;
	recEmitVmapHostPointer(RHOST, RADDR, &slow_path);
	armAsm->Str(RQSCRATCH, a64::MemOperand(RHOST));
	armAsm->B(&done);

	armAsm->Bind(&slow_path);
	recCacheEmitFlushAll(pre_store_cache);
	armEmitVtlbWriteQuad(RADDR, RQSCRATCH);

	armAsm->Bind(&done);
	return true;
}

// Constant folding into the register cache: when every source operand of an ALU op
// is const-known, compute the result at compile time and emit a single immediate Mov
// into the destination's cache register (dirty — flushed on demand like any cached
// write). The folding formulas below are kept textually identical to the tracking
// formulas in recConstApplyCachedEffects so the emitted value and the const state can
// never diverge. Runs before recTryTranslateCachedOp in recTranslateOpOptimized;
// returns false to fall through when any needed source is unknown.
static bool recTryTranslateCachedConstOp(u32 op, RecGprConstState& const_state, RecGprCacheState& cache)
{
	const u32 opcode = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	const u32 rd = (op >> 11) & 0x1f;
	const u32 sa = (op >> 6) & 0x1f;
	const u32 funct = op & 0x3f;
	const s32 imm = static_cast<s16>(op);
	const u32 imm_u = static_cast<u16>(op);

	auto known = [&](u32 reg) -> bool {
		return const_state.known[reg];
	};
	auto value = [&](u32 reg) -> u64 {
		return const_state.value[reg];
	};
	auto emit_known = [&](u32 reg, u64 val) -> bool {
		if (reg != 0)
		{
			const a64::Register& dst = recCacheDest(cache, reg);
			armAsm->Mov(dst, val);
		}
		recConstSetKnown(const_state, reg, val);
		return true;
	};

	switch (opcode)
	{
		case 0x08: // ADDI
		case 0x09: // ADDIU
			if (!known(rs))
				return false;
			return emit_known(rt, recSignExtend32(static_cast<u32>(value(rs)) + static_cast<u32>(imm)));

		case 0x18: // DADDI
		case 0x19: // DADDIU
			if (!known(rs))
				return false;
			return emit_known(rt, value(rs) + static_cast<u64>(static_cast<s64>(imm)));

		case 0x0A: // SLTI
			if (!known(rs))
				return false;
			return emit_known(rt, (static_cast<s64>(value(rs)) < static_cast<s64>(imm)) ? 1 : 0);

		case 0x0B: // SLTIU
			if (!known(rs))
				return false;
			return emit_known(rt, (value(rs) < static_cast<u64>(static_cast<s64>(imm))) ? 1 : 0);

		case 0x0C: // ANDI
			if (!known(rs))
				return false;
			return emit_known(rt, value(rs) & imm_u);

		case 0x0D: // ORI
			if (!known(rs))
				return false;
			return emit_known(rt, value(rs) | imm_u);

		case 0x0E: // XORI
			if (!known(rs))
				return false;
			return emit_known(rt, value(rs) ^ imm_u);

		case 0x0F: // LUI
			return emit_known(rt, recSignExtend32(static_cast<u32>(imm_u) << 16));

		case 0x00:
			break;

		default:
			return false;
	}

	switch (funct)
	{
		case 0x00: // SLL
			if (!known(rt)) return false;
			return emit_known(rd, recSignExtend32(static_cast<u32>(value(rt)) << sa));
		case 0x02: // SRL
			if (!known(rt)) return false;
			return emit_known(rd, recSignExtend32(static_cast<u32>(value(rt)) >> sa));
		case 0x03: // SRA
			if (!known(rt)) return false;
			return emit_known(rd, recSignExtend32(static_cast<u32>(static_cast<s32>(static_cast<u32>(value(rt))) >> sa)));
		case 0x04: // SLLV
			if (!known(rt) || !known(rs)) return false;
			return emit_known(rd, recSignExtend32(static_cast<u32>(value(rt)) << (value(rs) & 0x1f)));
		case 0x06: // SRLV
			if (!known(rt) || !known(rs)) return false;
			return emit_known(rd, recSignExtend32(static_cast<u32>(value(rt)) >> (value(rs) & 0x1f)));
		case 0x07: // SRAV
			if (!known(rt) || !known(rs)) return false;
			return emit_known(rd, recSignExtend32(static_cast<u32>(static_cast<s32>(static_cast<u32>(value(rt))) >> (value(rs) & 0x1f))));
		case 0x14: // DSLLV
			if (!known(rt) || !known(rs)) return false;
			return emit_known(rd, value(rt) << (value(rs) & 0x3f));
		case 0x16: // DSRLV
			if (!known(rt) || !known(rs)) return false;
			return emit_known(rd, value(rt) >> (value(rs) & 0x3f));
		case 0x17: // DSRAV
			if (!known(rt) || !known(rs)) return false;
			return emit_known(rd, static_cast<u64>(static_cast<s64>(value(rt)) >> (value(rs) & 0x3f)));
		case 0x38: // DSLL
			if (!known(rt)) return false;
			return emit_known(rd, value(rt) << sa);
		case 0x3A: // DSRL
			if (!known(rt)) return false;
			return emit_known(rd, value(rt) >> sa);
		case 0x3B: // DSRA
			if (!known(rt)) return false;
			return emit_known(rd, static_cast<u64>(static_cast<s64>(value(rt)) >> sa));
		case 0x3C: // DSLL32
			if (!known(rt)) return false;
			return emit_known(rd, value(rt) << (sa + 32));
		case 0x3E: // DSRL32
			if (!known(rt)) return false;
			return emit_known(rd, value(rt) >> (sa + 32));
		case 0x3F: // DSRA32
			if (!known(rt)) return false;
			return emit_known(rd, static_cast<u64>(static_cast<s64>(value(rt)) >> (sa + 32)));

		case 0x20: // ADD
		case 0x21: // ADDU
			if (!known(rs) || !known(rt)) return false;
			return emit_known(rd, recSignExtend32(static_cast<u32>(value(rs)) + static_cast<u32>(value(rt))));
		case 0x22: // SUB
		case 0x23: // SUBU
			if (!known(rs) || !known(rt)) return false;
			return emit_known(rd, recSignExtend32(static_cast<u32>(value(rs)) - static_cast<u32>(value(rt))));
		case 0x2C: // DADD
		case 0x2D: // DADDU
			if (!known(rs) || !known(rt)) return false;
			return emit_known(rd, value(rs) + value(rt));
		case 0x2E: // DSUB
		case 0x2F: // DSUBU
			if (!known(rs) || !known(rt)) return false;
			return emit_known(rd, value(rs) - value(rt));
		case 0x24: // AND
			if (!known(rs) || !known(rt)) return false;
			return emit_known(rd, value(rs) & value(rt));
		case 0x25: // OR
			if (!known(rs) || !known(rt)) return false;
			return emit_known(rd, value(rs) | value(rt));
		case 0x26: // XOR
			if (!known(rs) || !known(rt)) return false;
			return emit_known(rd, value(rs) ^ value(rt));
		case 0x27: // NOR
			if (!known(rs) || !known(rt)) return false;
			return emit_known(rd, ~(value(rs) | value(rt)));
		case 0x2A: // SLT
			if (!known(rs) || !known(rt)) return false;
			return emit_known(rd, (static_cast<s64>(value(rs)) < static_cast<s64>(value(rt))) ? 1 : 0);
		case 0x2B: // SLTU
			if (!known(rs) || !known(rt)) return false;
			return emit_known(rd, (value(rs) < value(rt)) ? 1 : 0);

		case 0x0A: // MOVZ
			if (!known(rt))
				return false;
			if (value(rt) != 0) // condition false at compile time -> architectural no-op
				return true;
			if (!known(rs))
				return false;
			return emit_known(rd, value(rs));
		case 0x0B: // MOVN
			if (!known(rt))
				return false;
			if (value(rt) == 0) // condition false at compile time -> architectural no-op
				return true;
			if (!known(rs))
				return false;
			return emit_known(rd, value(rs));

		default:
			return false;
	}
}

// --- @@MAC_EE_CONSTFOLD@@ Mixed-operand constant folding (task #121) -------------------
// When exactly one source of a reg-reg ALU op is a compile-time-known constant (the other
// runtime), fold that constant into an ARM immediate instead of loading it from guest
// memory into a cache slot. This reuses the SAME const value that recConstEmitKnown already
// stored to memory and that recTryTranslateCachedConstOp already trusts (it Movs folded
// constants straight into dest cache regs) — so it adds no new correctness trust, only
// better instruction selection. Both the fully-const case (handled earlier by
// recTryTranslateCachedConstOp) and this mixed case leave the const tracker to
// recConstApplyCachedEffects, which marks the runtime destination unknown exactly as the
// plain reg-reg path would. The fold is taken ONLY when the immediate encodes as a single
// add/sub or logical instruction (IsImmAddSub / IsImmLogical), so it is never worse than
// the memory load it replaces. Flip to false to fall back to the plain reg-reg emitters.
static bool s_eeGprMixedConstFold = true;

// True iff `addend` (mod 2^width) can be added to a register with a single add/sub
// immediate — either directly (ADD) or via its two's complement (SUB). Pure test, emits
// nothing, so encodability can be decided before any cache load/dest allocation (avoids
// the load-after-dest aliasing hazard).
static __fi bool recAddImmEncodableW(u32 addend)
{
	return addend == 0 || a64::Assembler::IsImmAddSub(static_cast<int64_t>(addend)) ||
		   a64::Assembler::IsImmAddSub(static_cast<int64_t>(static_cast<u32>(0u - addend)));
}
static __fi bool recAddImmEncodableX(u64 addend)
{
	return addend == 0 || a64::Assembler::IsImmAddSub(static_cast<int64_t>(addend)) ||
		   a64::Assembler::IsImmAddSub(static_cast<int64_t>(0ull - addend));
}

// Emit dst.W = src.W + addend (mod 2^32) as a single add/sub immediate. Precondition:
// recAddImmEncodableW(addend) is true, so exactly one instruction is emitted.
static __fi void recEmitAddImmW(const a64::Register& dst, const a64::Register& src, u32 addend)
{
	if (addend == 0)
	{
		if (!dst.W().Is(src.W()))
			armAsm->Mov(dst.W(), src.W());
		return;
	}
	if (a64::Assembler::IsImmAddSub(static_cast<int64_t>(addend)))
		armAsm->Add(dst.W(), src.W(), addend);
	else
		armAsm->Sub(dst.W(), src.W(), static_cast<u32>(0u - addend));
}
// Emit dst.X = src.X + addend (mod 2^64) as a single add/sub immediate. Precondition:
// recAddImmEncodableX(addend) is true.
static __fi void recEmitAddImmX(const a64::Register& dst, const a64::Register& src, u64 addend)
{
	if (addend == 0)
	{
		if (!dst.X().Is(src.X()))
			armAsm->Mov(dst.X(), src.X());
		return;
	}
	if (a64::Assembler::IsImmAddSub(static_cast<int64_t>(addend)))
		armAsm->Add(dst.X(), src.X(), addend);
	else
		armAsm->Sub(dst.X(), src.X(), 0ull - addend);
}

static bool recTryTranslateCachedOp(u32 op, RecGprCacheState& cache, const RecGprConstState& const_state, u32 pc)
{
	const u32 opcode = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	const u32 rd = (op >> 11) & 0x1f;
	const u32 sa = (op >> 6) & 0x1f;
	const u32 funct = op & 0x3f;
	const s32 imm = static_cast<s16>(op);
	const u32 imm_u = static_cast<u16>(op);

	auto move_x = [](const a64::Register& dst, const a64::Register& src) {
		if (!dst.Is(src))
			armAsm->Mov(dst, src);
	};
	auto move_w = [](const a64::Register& dst, const a64::Register& src) {
		if (!dst.Is(src))
			armAsm->Mov(dst, src);
	};

	switch (opcode)
	{
		case 0x08: // ADDI
		case 0x09: // ADDIU
		{
			if (rt == 0)
				return true;
			const a64::Register& src = recCacheLoad(cache, rs);
			const a64::Register& dst = recCacheDest(cache, rt, rs);
			move_w(dst.W(), src.W());
			if (imm != 0)
				armAsm->Add(dst.W(), dst.W(), imm);
			armAsm->Sxtw(dst, dst.W());
			return true;
		}

		case 0x18: // DADDI
		case 0x19: // DADDIU
		{
			if (rt == 0)
				return true;
			const a64::Register& src = recCacheLoad(cache, rs);
			const a64::Register& dst = recCacheDest(cache, rt, rs);
			move_x(dst, src);
			if (imm != 0)
				armAsm->Add(dst, dst, imm);
			return true;
		}

		case 0x0A: // SLTI
		case 0x0B: // SLTIU
		{
			if (rt == 0)
				return true;
			const a64::Register& src = recCacheLoad(cache, rs);
			const a64::Register& dst = recCacheDest(cache, rt, rs);
			armAsm->Cmp(src, imm);
			armAsm->Cset(dst, opcode == 0x0A ? a64::lt : a64::lo);
			return true;
		}

		case 0x0C: // ANDI
		case 0x0D: // ORI
		case 0x0E: // XORI
		{
			if (rt == 0)
				return true;
			const a64::Register& src = recCacheLoad(cache, rs);
			const a64::Register& dst = recCacheDest(cache, rt, rs);
			// The vixl MacroAssembler encodes these as single logical-immediate
			// instructions when the mask is encodable (0xff, 0xffff, ... — the common
			// cases) and only falls back to materializing into a scratch register
			// otherwise, so this is never worse than the manual Mov+op pair.
			if (opcode == 0x0C)
			{
				if (imm_u == 0)
					armAsm->Mov(dst, 0);
				else
					armAsm->And(dst, src, imm_u);
			}
			else if (opcode == 0x0D)
			{
				if (imm_u == 0)
					move_x(dst, src);
				else
					armAsm->Orr(dst, src, imm_u);
			}
			else
			{
				if (imm_u == 0)
					move_x(dst, src);
				else
					armAsm->Eor(dst, src, imm_u);
			}
			return true;
		}

		case 0x0F: // LUI
		{
			if (rt == 0)
				return true;
			const s32 val = static_cast<s32>(static_cast<u32>(imm_u) << 16);
			const a64::Register& dst = recCacheDest(cache, rt);
			if (val == 0)
				armAsm->Mov(dst, 0);
			else
			{
				armAsm->Mov(dst.W(), val);
				armAsm->Sxtw(dst, dst.W());
			}
				return true;
			}

		case OP_LB:  return recTryTranslateCachedLoad(8,  true,  rt, rs, imm, cache, const_state, pc);
		case OP_LBU: return recTryTranslateCachedLoad(8,  false, rt, rs, imm, cache, const_state, pc);
		case OP_LH:  return recTryTranslateCachedLoad(16, true,  rt, rs, imm, cache, const_state, pc);
		case OP_LHU: return recTryTranslateCachedLoad(16, false, rt, rs, imm, cache, const_state, pc);
		case OP_LW:  return recTryTranslateCachedLoad(32, true,  rt, rs, imm, cache, const_state, pc);
		case OP_LWU: return recTryTranslateCachedLoad(32, false, rt, rs, imm, cache, const_state, pc);
		case OP_LD:  return recTryTranslateCachedLoad(64, false, rt, rs, imm, cache, const_state, pc);
		case OP_LQ:  return recTryTranslateCachedLoadQuad(rt, rs, imm, cache, const_state, pc);

		case OP_SB: return recTryTranslateCachedStore(8,  rt, rs, imm, cache, const_state, pc);
		case OP_SH: return recTryTranslateCachedStore(16, rt, rs, imm, cache, const_state, pc);
		case OP_SW: return recTryTranslateCachedStore(32, rt, rs, imm, cache, const_state, pc);
		case OP_SD: return recTryTranslateCachedStore(64, rt, rs, imm, cache, const_state, pc);
		case OP_SQ: return recTryTranslateCachedStoreQuad(rt, rs, imm, cache, const_state, pc);

		case 0x00:
			break;

		default:
			return false;
	}

	switch (funct)
	{
		case 0x0A: // MOVZ
		case 0x0B: // MOVN
		{
			if (rd == 0 || rs == rd)
				return true;

			const a64::Register& cond = recCacheLoad(cache, rt);
			armAsm->Cmp(cond, 0);
			const a64::Register& old_dst = recCacheLoad(cache, rd);
			const a64::Register& src = recCacheLoad(cache, rs);
			const a64::Register& dst = recCacheDest(cache, rd, rs, rt);
			armAsm->Csel(dst, src, old_dst, funct == 0x0A ? a64::eq : a64::ne);
			return true;
		}

		case 0x10: // MFHI
		case 0x12: // MFLO
		{
			if (rd == 0)
				return true;

			const a64::Register& dst = recCacheDest(cache, rd);
			armAsm->Ldr(dst, a64::MemOperand(RESTATEPTR, funct == 0x10 ? EE_HI_SCALAR_OFFSET : EE_LO_SCALAR_OFFSET));
			return true;
		}

		// --- Fast-tier cache coverage (@@MAC_FASTTIER@@): keep the guest-GPR cache LIVE across
		// the hot integer-math slow-tier ops instead of recCacheFlushAll'ing all 7 regs. HI/LO
		// live in memory (not cache regs), so the already-cached MFHI/MFLO read these stores.
		// Codegen is bit-identical to emitMult/emitDivS/emitDivU (aR5900MultDiv.cpp), minus the
		// reference's redundant memory reloads of rs/rt (we source them from cache regs). ---
		case 0x11: // MTHI
		case 0x13: // MTLO
		{
			const a64::Register& src = recCacheLoad(cache, rs);
			armAsm->Str(src, a64::MemOperand(RESTATEPTR, funct == 0x11 ? EE_HI_SCALAR_OFFSET : EE_LO_SCALAR_OFFSET));
			return true;
		}

		case 0x18: // MULT
		case 0x19: // MULTU
		{
			// 32x32->64. LO = sxt32(prod low), HI = sxt32(prod high) (sign-extended even for
			// MULTU); if rd != 0, GPR[rd] = LO (R5900 3-operand form). Keep the product in x17
			// (RSCRATCHADDR) — it survives the recCacheDest below (which may run cache macros).
			const bool mult_sign = (funct == 0x18);
			const a64::Register& lhs = recCacheLoad(cache, rs);
			const a64::Register& rhs = recCacheLoad(cache, rt);
			if (mult_sign)
				armAsm->Smull(RSCRATCHADDR, lhs.W(), rhs.W());
			else
				armAsm->Umull(RSCRATCHADDR, lhs.W(), rhs.W());
			armAsm->Sxtw(RXVIXLSCRATCH, RSCRATCHADDR.W());                 // LO = sxt32(low)
			armAsm->Str(RXVIXLSCRATCH, a64::MemOperand(RESTATEPTR, EE_LO_SCALAR_OFFSET));
			if (rd != 0)
			{
				const a64::Register& dst = recCacheDest(cache, rd, rs, rt); // pin rs/rt (sources)
				armAsm->Sxtw(dst, RSCRATCHADDR.W());                       // recompute LO from x17
			}
			armAsm->Asr(RSCRATCHADDR, RSCRATCHADDR, 32);                   // HI = sxt32(high)
			armAsm->Str(RSCRATCHADDR, a64::MemOperand(RESTATEPTR, EE_HI_SCALAR_OFFSET));
			return true;
		}

		case 0x1A: // DIV
		case 0x1B: // DIVU
		{
			// LO = rs/rt, HI = rs%rt (both sxt32). ARM S/UDIV reproduce the EE INT_MIN/-1 and
			// div-by-zero quotient for free; only the div-by-zero LO needs a fixup. Writes no
			// GPR cache reg (HI/LO memory only), so no recCacheDest / cache-effect needed.
			const bool div_sign = (funct == 0x1A);
			a64::Label div_done;
			const a64::Register& num = recCacheLoad(cache, rs); // dividend
			const a64::Register& den = recCacheLoad(cache, rt); // divisor
			if (div_sign)
				armAsm->Sdiv(RSCRATCHADDR.W(), num.W(), den.W());
			else
				armAsm->Udiv(RSCRATCHADDR.W(), num.W(), den.W());
			armAsm->Mul(RXVIXLSCRATCH.W(), RSCRATCHADDR.W(), den.W());     // x16 = quotient*divisor
			armAsm->Sxtw(RSCRATCHADDR, RSCRATCHADDR.W());                  // LO = sxt(quotient)
			armAsm->Str(RSCRATCHADDR, a64::MemOperand(RESTATEPTR, EE_LO_SCALAR_OFFSET));
			armAsm->Sub(RXVIXLSCRATCH.W(), num.W(), RXVIXLSCRATCH.W());    // remainder = num - q*den
			armAsm->Sxtw(RXVIXLSCRATCH, RXVIXLSCRATCH.W());                // HI = sxt(remainder)
			armAsm->Str(RXVIXLSCRATCH, a64::MemOperand(RESTATEPTR, EE_HI_SCALAR_OFFSET));
			armAsm->Cmp(den.W(), 0);                                       // div-by-zero LO fixup
			armAsm->B(a64::ne, &div_done);
			if (div_sign)
			{
				armAsm->Cmp(num.W(), 0);
				armAsm->Mov(RXVIXLSCRATCH.W(), 1);
				armAsm->Csneg(RXVIXLSCRATCH.W(), RXVIXLSCRATCH.W(), RXVIXLSCRATCH.W(), a64::lt); // (num<0)?1:-1
				armAsm->Sxtw(RXVIXLSCRATCH, RXVIXLSCRATCH.W());
			}
			else
			{
				armAsm->Mov(RXVIXLSCRATCH, 0xFFFFFFFFFFFFFFFFull);         // LO = -1
			}
			armAsm->Str(RXVIXLSCRATCH, a64::MemOperand(RESTATEPTR, EE_LO_SCALAR_OFFSET));
			armAsm->Bind(&div_done);
			return true;
		}

			case 0x00: // SLL
			case 0x02: // SRL
			case 0x03: // SRA
			{
				if (rd == 0)
					return true;
				if (rt == 0)
				{
					const a64::Register& dst = recCacheDest(cache, rd);
					armAsm->Mov(dst, 0);
					return true;
				}
				const a64::Register& src = recCacheLoad(cache, rt);
				const a64::Register& dst = recCacheDest(cache, rd, rt);
				if (funct == 0x00)
					armAsm->Lsl(dst.W(), src.W(), sa);
				else if (funct == 0x02)
				armAsm->Lsr(dst.W(), src.W(), sa);
			else
				armAsm->Asr(dst.W(), src.W(), sa);
			armAsm->Sxtw(dst, dst.W());
			return true;
		}

			case 0x04: // SLLV
			case 0x06: // SRLV
			case 0x07: // SRAV
			{
				if (rd == 0)
					return true;
				if (rt == 0)
				{
					const a64::Register& dst = recCacheDest(cache, rd);
					armAsm->Mov(dst, 0);
					return true;
				}
				const a64::Register& src = recCacheLoad(cache, rt);
				if (rs == 0)
				{
					const a64::Register& dst = recCacheDest(cache, rd, rt);
					move_w(dst.W(), src.W());
					armAsm->Sxtw(dst, dst.W());
					return true;
				}
				const a64::Register& sh = recCacheLoad(cache, rs);
				const a64::Register& dst = recCacheDest(cache, rd, rt, rs);
				if (funct == 0x04)
					armAsm->Lsl(dst.W(), src.W(), sh.W());
				else if (funct == 0x06)
				armAsm->Lsr(dst.W(), src.W(), sh.W());
			else
				armAsm->Asr(dst.W(), src.W(), sh.W());
			armAsm->Sxtw(dst, dst.W());
			return true;
		}

			case 0x14: // DSLLV
			case 0x16: // DSRLV
			case 0x17: // DSRAV
			{
				if (rd == 0)
					return true;
				if (rt == 0)
				{
					const a64::Register& dst = recCacheDest(cache, rd);
					armAsm->Mov(dst, 0);
					return true;
				}
				const a64::Register& src = recCacheLoad(cache, rt);
				if (rs == 0)
				{
					const a64::Register& dst = recCacheDest(cache, rd, rt);
					move_x(dst, src);
					return true;
				}
				const a64::Register& sh = recCacheLoad(cache, rs);
				const a64::Register& dst = recCacheDest(cache, rd, rt, rs);
				if (funct == 0x14)
					armAsm->Lsl(dst, src, sh);
				else if (funct == 0x16)
				armAsm->Lsr(dst, src, sh);
			else
				armAsm->Asr(dst, src, sh);
			return true;
		}

		case 0x38: // DSLL
		case 0x3A: // DSRL
		case 0x3B: // DSRA
		case 0x3C: // DSLL32
		case 0x3E: // DSRL32
		case 0x3F: // DSRA32
			{
				if (rd == 0)
					return true;
				const u32 shift = sa + ((funct == 0x3C || funct == 0x3E || funct == 0x3F) ? 32 : 0);
				if (rt == 0)
				{
					const a64::Register& dst = recCacheDest(cache, rd);
					armAsm->Mov(dst, 0);
					return true;
				}
				const a64::Register& src = recCacheLoad(cache, rt);
				const a64::Register& dst = recCacheDest(cache, rd, rt);
				if (shift == 0)
					move_x(dst, src);
				else if (funct == 0x38 || funct == 0x3C)
					armAsm->Lsl(dst, src, shift);
				else if (funct == 0x3A || funct == 0x3E)
					armAsm->Lsr(dst, src, shift);
				else
				armAsm->Asr(dst, src, shift);
			return true;
		}

		case 0x20: // ADD
		case 0x21: // ADDU
		case 0x22: // SUB
		case 0x23: // SUBU
		{
			if (rd == 0)
				return true;
			const bool is_add = (funct == 0x20 || funct == 0x21);
			// @@MAC_EE_CONSTFOLD@@ Exactly one const source -> fold into an add/sub imm.
			if (s_eeGprMixedConstFold)
			{
				// rt const (both ADD and SUB: dst = rs +/- c). Encode as rs + (add ? c : -c).
				if (const_state.known[rt] && !const_state.known[rs])
				{
					const u32 addend = is_add ? static_cast<u32>(const_state.value[rt])
											  : static_cast<u32>(0u - static_cast<u32>(const_state.value[rt]));
					if (recAddImmEncodableW(addend))
					{
						const a64::Register& src = recCacheLoad(cache, rs);
						const a64::Register& dst = recCacheDest(cache, rd, rs);
						recEmitAddImmW(dst, src, addend);
						armAsm->Sxtw(dst, dst.W());
						return true;
					}
				}
				// rs const, ADD only (commutative): dst = rt + c.
				else if (is_add && const_state.known[rs] && !const_state.known[rt])
				{
					const u32 addend = static_cast<u32>(const_state.value[rs]);
					if (recAddImmEncodableW(addend))
					{
						const a64::Register& src = recCacheLoad(cache, rt);
						const a64::Register& dst = recCacheDest(cache, rd, rt);
						recEmitAddImmW(dst, src, addend);
						armAsm->Sxtw(dst, dst.W());
						return true;
					}
				}
			}
			const a64::Register& lhs = recCacheLoad(cache, rs);
			const a64::Register& rhs = recCacheLoad(cache, rt);
			const a64::Register& dst = recCacheDest(cache, rd, rs, rt);
			if (is_add)
				armAsm->Add(dst.W(), lhs.W(), rhs.W());
			else
				armAsm->Sub(dst.W(), lhs.W(), rhs.W());
			armAsm->Sxtw(dst, dst.W());
			return true;
		}

		case 0x2C: // DADD
		case 0x2D: // DADDU
		case 0x2E: // DSUB
		case 0x2F: // DSUBU
		{
			if (rd == 0)
				return true;
			const bool is_add = (funct == 0x2C || funct == 0x2D);
			// @@MAC_EE_CONSTFOLD@@ Exactly one const source -> fold into a 64-bit add/sub imm.
			if (s_eeGprMixedConstFold)
			{
				if (const_state.known[rt] && !const_state.known[rs])
				{
					const u64 addend = is_add ? const_state.value[rt] : (0ull - const_state.value[rt]);
					if (recAddImmEncodableX(addend))
					{
						const a64::Register& src = recCacheLoad(cache, rs);
						const a64::Register& dst = recCacheDest(cache, rd, rs);
						recEmitAddImmX(dst, src, addend);
						return true;
					}
				}
				else if (is_add && const_state.known[rs] && !const_state.known[rt])
				{
					const u64 addend = const_state.value[rs];
					if (recAddImmEncodableX(addend))
					{
						const a64::Register& src = recCacheLoad(cache, rt);
						const a64::Register& dst = recCacheDest(cache, rd, rt);
						recEmitAddImmX(dst, src, addend);
						return true;
					}
				}
			}
			const a64::Register& lhs = recCacheLoad(cache, rs);
			const a64::Register& rhs = recCacheLoad(cache, rt);
			const a64::Register& dst = recCacheDest(cache, rd, rs, rt);
			if (is_add)
				armAsm->Add(dst, lhs, rhs);
			else
				armAsm->Sub(dst, lhs, rhs);
			return true;
		}

		case 0x24: // AND
		case 0x25: // OR
		case 0x26: // XOR
		case 0x27: // NOR
		{
			if (rd == 0)
				return true;
			// @@MAC_EE_CONSTFOLD@@ Exactly one const source -> fold into a 64-bit logical imm
			// (all four ops are commutative in their two register sources).
			if (s_eeGprMixedConstFold)
			{
				u32 vreg = 0xff; // the runtime (non-const) source register
				u64 c = 0;
				if (const_state.known[rt] && !const_state.known[rs]) { vreg = rs; c = const_state.value[rt]; }
				else if (const_state.known[rs] && !const_state.known[rt]) { vreg = rt; c = const_state.value[rs]; }
				if (vreg != 0xff)
				{
					if (c == 0)
					{
						// c==0 identities (not encodable as logical immediates): AND->0,
						// OR/XOR->src, NOR->~src.
						if (funct == 0x24)
						{
							const a64::Register& dst = recCacheDest(cache, rd);
							armAsm->Mov(dst, 0);
							return true;
						}
						const a64::Register& src = recCacheLoad(cache, vreg);
						const a64::Register& dst = recCacheDest(cache, rd, vreg);
						if (funct == 0x27)
							armAsm->Mvn(dst, src);
						else if (!dst.Is(src))
							armAsm->Mov(dst, src);
						return true;
					}
					if (a64::Assembler::IsImmLogical(c, 64))
					{
						const a64::Register& src = recCacheLoad(cache, vreg);
						const a64::Register& dst = recCacheDest(cache, rd, vreg);
						if (funct == 0x24)
							armAsm->And(dst, src, c);
						else if (funct == 0x25)
							armAsm->Orr(dst, src, c);
						else if (funct == 0x26)
							armAsm->Eor(dst, src, c);
						else
						{
							armAsm->Orr(dst, src, c);
							armAsm->Mvn(dst, dst);
						}
						return true;
					}
				}
			}
			const a64::Register& lhs = recCacheLoad(cache, rs);
			const a64::Register& rhs = recCacheLoad(cache, rt);
			const a64::Register& dst = recCacheDest(cache, rd, rs, rt);
			if (funct == 0x24)
				armAsm->And(dst, lhs, rhs);
			else if (funct == 0x25)
				armAsm->Orr(dst, lhs, rhs);
			else if (funct == 0x26)
				armAsm->Eor(dst, lhs, rhs);
			else
			{
				armAsm->Orr(dst, lhs, rhs);
				armAsm->Mvn(dst, dst);
			}
			return true;
		}

		case 0x2A: // SLT
		case 0x2B: // SLTU
		{
			if (rd == 0)
				return true;
			const a64::Register& lhs = recCacheLoad(cache, rs);
			const a64::Register& rhs = recCacheLoad(cache, rt);
			const a64::Register& dst = recCacheDest(cache, rd, rs, rt);
			armAsm->Cmp(lhs, rhs);
			armAsm->Cset(dst, funct == 0x2A ? a64::lt : a64::lo);
			return true;
		}

		default:
			return false;
	}
}

// Cache-side mirror of recConstApplyNativeEffects: after a native (non-cached)
// generator ran, discard the cached copy of every GPR it wrote to memory, so the
// cache never holds a stale value. Ops whose inline-interpreter handler can touch
// arbitrary CPU state (COP0/COP2/LQC2/SQC2) kill the whole cache, exactly like the
// const tracker. Keeping this switch in lockstep with recConstApplyNativeEffects is
// the correctness contract for the precise-invalidation path below.
static void recCacheApplyNativeEffects(u32 op, RecGprCacheState& cache)
{
	const u32 opcode = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	const u32 rd = (op >> 11) & 0x1f;
	const u32 funct = op & 0x3f;

	switch (opcode)
	{
		case 0x00:
			switch (funct)
			{
				case 0x11: // MTHI
				case 0x13: // MTLO
				case 0x18: // MULT
				case 0x19: // MULTU
				case 0x1A: // DIV
				case 0x1B: // DIVU
					if (funct == 0x18 || funct == 0x19)
						recCacheDiscardGuest(cache, rd);
					return;
				default:
					recCacheDiscardGuest(cache, rd);
					return;
			}

		case 0x08: case 0x09: case 0x0A: case 0x0B:
		case 0x0C: case 0x0D: case 0x0E: case 0x0F:
		case 0x18: case 0x19:
			recCacheDiscardGuest(cache, rt);
			return;

		case OP_LQ: case OP_LB: case OP_LH: case OP_LW:
		case OP_LBU: case OP_LHU: case OP_LWU: case OP_LD:
		case 0x22: case 0x26: case 0x1A: case 0x1B: // LWL/LWR/LDL/LDR merge into rt
			recCacheDiscardGuest(cache, rt);
			return;

		case 0x11: // COP1: MFC1/CFC1 write rt, other native FPU ops do not touch GPRs.
			if (rs == 0x00 || rs == 0x02)
				recCacheDiscardGuest(cache, rt);
			return;

		case 0x10: // COP0 inline interpreter may touch CPU state.
		case 0x12: // COP2 inline interpreter may move VU data through GPRs.
		case OP_LQC2:
		case OP_SQC2:
			recCacheKillAll(cache);
			return;

		case 0x1C:
			recCacheDiscardGuest(cache, rd);
			return;

		default:
			return;
	}
}

static bool recTranslateOpOptimized(u32 op, RecGprConstState& const_state, RecGprCacheState& cache, u32 pc)
{
	// Fold ops with fully const-known sources first: emits one immediate Mov into the
	// destination's cache register and updates the const state itself, so neither the
	// generic cached emitter nor the apply-effects pass runs for them.
	if (recTryTranslateCachedConstOp(op, const_state, cache))
		return true;

	if (recTryTranslateCachedOp(op, cache, const_state, pc))
	{
		if (!recConstApplyCachedEffects(op, const_state))
			recConstApplyNativeEffects(op, const_state);
		return true;
	}

	// Native generators (and the interpreter fallback) read and write guest GPRs
	// directly through cpuRegs in memory: write every dirty cached value back first
	// so they observe current state. Entries stay valid (clean), so subsequent
	// cached ops keep their registers — the previous flush-AND-kill here threw the
	// whole cache away around every MULT/DIV/MMI/COP1 op in mixed blocks.
	recCacheFlushAll(cache);

	if (recTryTranslateConstOp(op, const_state))
	{
		// The const store wrote the destination GPR to memory behind the cache's back.
		recCacheApplyNativeEffects(op, cache);
		return true;
	}

	if (!recTranslateOp(op, pc))
	{
		// Caller falls back to the inline interpreter, which can write any GPR.
		recCacheKillAll(cache);
		recConstKillAll(const_state);
		return false;
	}

	recCacheApplyNativeEffects(op, cache);
	recConstApplyNativeEffects(op, const_state);
	return true;
}

// Translate a single guest instruction (cpuRegs.code) into the open block. Returns
// true if a real generator handled it, false if it fell through to a placeholder.
// Decodes the MIPS fields explicitly and hands them to the Phase 2.3 load/store
// generators (which read/write guest GPRs through RESTATEPTR and route memory
// access via the slow-path vtlb helpers).
// MMI sub-group decoders (Phase 5.4). The MMI0/1/2/3 classes carry their real
// opcode in the `sa` field (bits 10:6); each indexes a 32-entry table (see
// R5900OpcodeTables.cpp tbl_MMI0..3) — the case labels below mirror those tables
// exactly. Any sub-op without a native generator returns false and falls back to
// the interpreter (e.g. QFSRV, whose shift amount is the runtime SA register).
static bool recTranslateMMI0(u32 sa, u32 rd, u32 rs, u32 rt)
{
	switch (sa)
	{
		case 0x00: armEmitPADDW(rd, rs, rt); return true;
		case 0x01: armEmitPSUBW(rd, rs, rt); return true;
		case 0x02: armEmitPCGTW(rd, rs, rt); return true;
		case 0x03: armEmitPMAXW(rd, rs, rt); return true;
		case 0x04: armEmitPADDH(rd, rs, rt); return true;
		case 0x05: armEmitPSUBH(rd, rs, rt); return true;
		case 0x06: armEmitPCGTH(rd, rs, rt); return true;
		case 0x07: armEmitPMAXH(rd, rs, rt); return true;
		case 0x08: armEmitPADDB(rd, rs, rt); return true;
		case 0x09: armEmitPSUBB(rd, rs, rt); return true;
		case 0x0A: armEmitPCGTB(rd, rs, rt); return true;
		case 0x10: armEmitPADDSW(rd, rs, rt); return true;
		case 0x11: armEmitPSUBSW(rd, rs, rt); return true;
		case 0x12: armEmitPEXTLW(rd, rs, rt); return true;
		case 0x13: armEmitPPACW(rd, rs, rt); return true;
		case 0x14: armEmitPADDSH(rd, rs, rt); return true;
		case 0x15: armEmitPSUBSH(rd, rs, rt); return true;
		case 0x16: armEmitPEXTLH(rd, rs, rt); return true;
		case 0x17: armEmitPPACH(rd, rs, rt); return true;
		case 0x18: armEmitPADDSB(rd, rs, rt); return true;
		case 0x19: armEmitPSUBSB(rd, rs, rt); return true;
		case 0x1A: armEmitPEXTLB(rd, rs, rt); return true;
		case 0x1B: armEmitPPACB(rd, rs, rt); return true;
		case 0x1E: armEmitPEXT5(rd, rt); return true;
		case 0x1F: armEmitPPAC5(rd, rt); return true;
		default:   return false;
	}
}

static bool recTranslateMMI1(u32 sa, u32 rd, u32 rs, u32 rt)
{
	switch (sa)
	{
		case 0x01: armEmitPABSW(rd, rt); return true;
		case 0x02: armEmitPCEQW(rd, rs, rt); return true;
		case 0x03: armEmitPMINW(rd, rs, rt); return true;
		case 0x04: armEmitPADSBH(rd, rs, rt); return true;
		case 0x05: armEmitPABSH(rd, rt); return true;
		case 0x06: armEmitPCEQH(rd, rs, rt); return true;
		case 0x07: armEmitPMINH(rd, rs, rt); return true;
		case 0x0A: armEmitPCEQB(rd, rs, rt); return true;
		case 0x10: armEmitPADDUW(rd, rs, rt); return true;
		case 0x11: armEmitPSUBUW(rd, rs, rt); return true;
		case 0x12: armEmitPEXTUW(rd, rs, rt); return true;
		case 0x14: armEmitPADDUH(rd, rs, rt); return true;
		case 0x15: armEmitPSUBUH(rd, rs, rt); return true;
		case 0x16: armEmitPEXTUH(rd, rs, rt); return true;
		case 0x18: armEmitPADDUB(rd, rs, rt); return true;
		case 0x19: armEmitPSUBUB(rd, rs, rt); return true;
		case 0x1A: armEmitPEXTUB(rd, rs, rt); return true;
		// 0x1B QFSRV: shift amount comes from the runtime SA register (cpuRegs.sa),
		// not the instruction — left to the interpreter.
		default:   return false;
	}
}

static bool recTranslateMMI2(u32 sa, u32 rd, u32 rs, u32 rt)
{
	// Indices mirror R5900OpcodeTables.cpp tbl_MMI2[(op>>6)&0x1F].
	switch (sa)
	{
		case 0x00: armEmitPMADDW(rd, rs, rt); return true;
		case 0x02: armEmitPSLLVW(rd, rs, rt); return true;
		case 0x03: armEmitPSRLVW(rd, rs, rt); return true;
		case 0x04: armEmitPMSUBW(rd, rs, rt); return true;
		case 0x08: armEmitPMFHI(rd); return true;
		case 0x09: armEmitPMFLO(rd); return true;
		case 0x0A: armEmitPINTH(rd, rs, rt); return true;
		case 0x0C: armEmitPMULTW(rd, rs, rt); return true;
		case 0x0E: armEmitPCPYLD(rd, rs, rt); return true;
		case 0x10: armEmitPMADDH(rd, rs, rt); return true;
		case 0x11: armEmitPHMADH(rd, rs, rt); return true;
		case 0x12: armEmitPAND(rd, rs, rt); return true;
		case 0x13: armEmitPXOR(rd, rs, rt); return true;
		case 0x14: armEmitPMSUBH(rd, rs, rt); return true;
		case 0x15: armEmitPHMSBH(rd, rs, rt); return true;
		case 0x1A: armEmitPEXEH(rd, rt); return true;
		case 0x1B: armEmitPREVH(rd, rt); return true;
		case 0x1C: armEmitPMULTH(rd, rs, rt); return true;
		case 0x1E: armEmitPEXEW(rd, rt); return true;
		case 0x1F: armEmitPROT3W(rd, rt); return true;
		default:   return false;
	}
}

static bool recTranslateMMI3(u32 sa, u32 rd, u32 rs, u32 rt)
{
	// Indices mirror R5900OpcodeTables.cpp tbl_MMI3[(op>>6)&0x1F].
	switch (sa)
	{
		case 0x00: armEmitPMADDUW(rd, rs, rt); return true;
		case 0x03: armEmitPSRAVW(rd, rs, rt); return true;
		case 0x08: armEmitPMTHI(rs); return true;
		case 0x09: armEmitPMTLO(rs); return true;
		case 0x0A: armEmitPINTEH(rd, rs, rt); return true;
		case 0x0C: armEmitPMULTUW(rd, rs, rt); return true;
		case 0x0E: armEmitPCPYUD(rd, rs, rt); return true;
		case 0x12: armEmitPOR(rd, rs, rt); return true;
		case 0x13: armEmitPNOR(rd, rs, rt); return true;
		case 0x1A: armEmitPEXCH(rd, rt); return true;
		case 0x1B: armEmitPCPYH(rd, rt); return true;
		case 0x1E: armEmitPEXCW(rd, rt); return true;
		default:   return false;
	}
}

static bool recTranslateOp(u32 op, u32 pc)
{
	const u32 opcode = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	const u32 rd = (op >> 11) & 0x1f;
	const u32 funct = op & 0x3f;
	const s32 imm = static_cast<s16>(op);

	const u32 sa = (op >> 6) & 0x1f;

	switch (opcode)
	{
		// SPECIAL — R-type register-register ops (Phase 3.2 + 3.3)
		case 0x00:
			switch (funct)
			{
				// Shifts (Phase 3.3) — immediate
				case 0x00: armEmitSLL(rd, rt, sa); return true;
				case 0x02: armEmitSRL(rd, rt, sa); return true;
				case 0x03: armEmitSRA(rd, rt, sa); return true;
				// Shifts (Phase 3.3) — variable
				case 0x04: armEmitSLLV(rd, rt, rs); return true;
				case 0x06: armEmitSRLV(rd, rt, rs); return true;
				case 0x07: armEmitSRAV(rd, rt, rs); return true;
				// Arithmetic (Phase 3.2)
				case 0x20: armEmitADD(rd, rs, rt); return true;
				case 0x21: armEmitADDU(rd, rs, rt); return true;
				case 0x22: armEmitSUB(rd, rs, rt); return true;
				case 0x23: armEmitSUBU(rd, rs, rt); return true;
				case 0x24: armEmitAND(rd, rs, rt); return true;
				case 0x25: armEmitOR(rd, rs, rt); return true;
				case 0x26: armEmitXOR(rd, rs, rt); return true;
				case 0x27: armEmitNOR(rd, rs, rt); return true;
				case 0x2A: armEmitSLT(rd, rs, rt); return true;
				case 0x2B: armEmitSLTU(rd, rs, rt); return true;
				case 0x2C: armEmitDADD(rd, rs, rt); return true;
				case 0x2D: armEmitDADDU(rd, rs, rt); return true;
				case 0x2E: armEmitDSUB(rd, rs, rt); return true;
				case 0x2F: armEmitDSUBU(rd, rs, rt); return true;
				// Shifts (Phase 3.3) — variable 64-bit
				case 0x14: armEmitDSLLV(rd, rt, rs); return true;
				case 0x16: armEmitDSRLV(rd, rt, rs); return true;
				case 0x17: armEmitDSRAV(rd, rt, rs); return true;
				// Shifts (Phase 3.3) — immediate 64-bit + DS*32
				case 0x38: armEmitDSLL(rd, rt, sa); return true;
				case 0x3A: armEmitDSRL(rd, rt, sa); return true;
				case 0x3B: armEmitDSRA(rd, rt, sa); return true;
				case 0x3C: armEmitDSLL32(rd, rt, sa); return true;
				case 0x3E: armEmitDSRL32(rd, rt, sa); return true;
				case 0x3F: armEmitDSRA32(rd, rt, sa); return true;
				// Moves (Phase 3.4)
				case 0x0A: armEmitMOVZ(rd, rs, rt); return true;
				case 0x0B: armEmitMOVN(rd, rs, rt); return true;
				case 0x10: armEmitMFHI(rd); return true;
				case 0x11: armEmitMTHI(rs); return true;
				case 0x12: armEmitMFLO(rd); return true;
				case 0x13: armEmitMTLO(rs); return true;
				// Multiply/Divide (Phase 3.5)
				case 0x18: armEmitMULT(rd, rs, rt); return true;
				case 0x19: armEmitMULTU(rd, rs, rt); return true;
				case 0x1A: armEmitDIV(rs, rt); return true;
				case 0x1B: armEmitDIVU(rs, rt); return true;
				// SYNC (funct 0x0f): a pipeline/memory barrier whose interpreter body
				// is EMPTY in this emulator (no EE pipeline/cache modelled — see
				// R5900OpcodeImpl SYNC()). Emit nothing instead of block-terminating
				// and single-stepping it. By far the dominant EE fallback op in real
				// games (The Getaway: ~59% of all single-stepped ops were SYNC).
				case 0x0f: return true;
				default:   return false;
			}

		// MMI — second-pipeline multiply/divide (Phase 3.5) + multiply-accumulate
		// and the pipeline-1 HI/LO moves. Remaining MMI ops fall through to false.
		case 0x1C:
			switch (funct)
			{
				case 0x00: armEmitMADD(rd, rs, rt); return true;   // MADD
				case 0x01: armEmitMADDU(rd, rs, rt); return true;  // MADDU
				case 0x10: armEmitMFHI1(rd); return true;          // MFHI1
				case 0x11: armEmitMTHI1(rs); return true;          // MTHI1
				case 0x12: armEmitMFLO1(rd); return true;          // MFLO1
				case 0x13: armEmitMTLO1(rs); return true;          // MTLO1
				case 0x18: armEmitMULT1(rd, rs, rt); return true;
				case 0x19: armEmitMULTU1(rd, rs, rt); return true;
				case 0x1A: armEmitDIV1(rs, rt); return true;
				case 0x1B: armEmitDIVU1(rs, rt); return true;
				case 0x20: armEmitMADD1(rd, rs, rt); return true;  // MADD1
				case 0x21: armEmitMADDU1(rd, rs, rt); return true; // MADDU1
				// Direct tbl_MMI entries (indexed by funct = op & 0x3F).
				case 0x04: armEmitPLZCW(rd, rs); return true;
				// MMI0/1/2/3 SIMD sub-groups (Phase 5.4); sub-op in `sa`.
				case 0x08: return recTranslateMMI0(sa, rd, rs, rt);
				case 0x28: return recTranslateMMI1(sa, rd, rs, rt);
				case 0x09: return recTranslateMMI2(sa, rd, rs, rt);
				case 0x29: return recTranslateMMI3(sa, rd, rs, rt);
				// PMFHL variant is in `sa`; PMTHL is only defined for sa==0.
				case 0x30: return armEmitPMFHL(rd, sa);
				case 0x31: armEmitPMTHL(rs, sa); return true;
				// Parallel shifts by immediate (Phase 5.4 continuation).
				case 0x34: armEmitPSLLH(rd, rt, sa); return true;
				case 0x36: armEmitPSRLH(rd, rt, sa); return true;
				case 0x37: armEmitPSRAH(rd, rt, sa); return true;
				case 0x3C: armEmitPSLLW(rd, rt, sa); return true;
				case 0x3E: armEmitPSRLW(rd, rt, sa); return true;
				case 0x3F: armEmitPSRAW(rd, rt, sa); return true;
				default:   return false;
			}

		// COP1 (FPU). The sub-opcode is the rs field, S-format ops sub-decode on
		// funct. Remaining float arithmetic / compares / BC1 branches return false
		// and fall to the interpreter until they get native EE FPU semantics.
		// Operand mapping per R5900OpcodeTables: ft=rt, fs=rd, fd=sa.
		case 0x11:
			switch (rs)
			{
				case 0x00: armEmitMFC1(rt, rd); return true; // MFC1
				case 0x02: armEmitCFC1(rt, rd); return true; // CFC1
				case 0x04: armEmitMTC1(rd, rt); return true; // MTC1 (fs=rd)
				case 0x06: armEmitCTC1(rd, rt); return true; // CTC1 (fs=rd)
				case 0x10:                                   // COP1_S (single-precision)
					switch (funct)
					{
						// Float arithmetic (Phase 5.2b): ft=rt, fs=rd, fd=sa.
						case 0x00: armEmitADD_S(sa, rd, rt); return true; // ADD_S
						case 0x01: armEmitSUB_S(sa, rd, rt); return true; // SUB_S
						case 0x02: armEmitMUL_S(sa, rd, rt); return true; // MUL_S
						case 0x03: armEmitDIV_S(sa, rd, rt); return true; // DIV_S
						case 0x04: armEmitSQRT_S(sa, rt); return true;    // SQRT_S (ft=rt)
						case 0x16: armEmitRSQRT_S(sa, rd, rt); return true; // RSQRT_S
						case 0x18: armEmitADDA_S(rd, rt); return true;    // ADDA_S (-> ACC)
						case 0x19: armEmitSUBA_S(rd, rt); return true;    // SUBA_S (-> ACC)
						case 0x1A: armEmitMULA_S(rd, rt); return true;    // MULA_S (-> ACC)
						case 0x1C: armEmitMADD_S(sa, rd, rt); return true;  // MADD_S
						case 0x1D: armEmitMSUB_S(sa, rd, rt); return true;  // MSUB_S
						case 0x1E: armEmitMADDA_S(rd, rt); return true;     // MADDA_S (-> ACC)
						case 0x1F: armEmitMSUBA_S(rd, rt); return true;     // MSUBA_S (-> ACC)
						case 0x28: armEmitMAX_S(sa, rd, rt); return true;   // MAX_S
						case 0x29: armEmitMIN_S(sa, rd, rt); return true;   // MIN_S
						case 0x24: armEmitCVT_W(sa, rd); return true;       // CVT_W (fd=sa, fs=rd)
						case 0x30: armEmitC_F(rd, rt); return true;  // C.F  (set FCR31 C-bit; fs=rd, ft=rt)
						case 0x32: armEmitC_EQ(rd, rt); return true; // C.EQ
						case 0x34: armEmitC_LT(rd, rt); return true; // C.LT
						case 0x36: armEmitC_LE(rd, rt); return true; // C.LE
						// Bit-exact ops (Phase 5.2a).
						case 0x05: armEmitABS_S(sa, rd); return true; // ABS_S (fd=sa, fs=rd)
						case 0x06: armEmitMOV_S(sa, rd); return true; // MOV_S
						case 0x07: armEmitNEG_S(sa, rd); return true; // NEG_S
						default:   return false;
					}
				case 0x14: // COP1_W: only CVT_S (funct 0x20); fd=sa, fs=rd.
					if (funct == 0x20) { armEmitCVT_S(sa, rd); return true; }
					return false;
				default: return false;
			}

		// REGIMM — rt-field selector. Only the trap-immediates are native here; the
		// BLTZ/BGEZ/... branches are emitted by the branch path (block-terminating) and
		// never reach recTranslateOp. Unhandled rt values fall through to false.
		case 0x01:
			return false;

		// Immediate arithmetic (Phase 3.1)
		case 0x08: armEmitADDI(rt, rs, imm); return true;
		case 0x09: armEmitADDIU(rt, rs, imm); return true;
		case 0x0A: armEmitSLTI(rt, rs, imm); return true;
		case 0x0B: armEmitSLTIU(rt, rs, imm); return true;
		case 0x0C: armEmitANDI(rt, rs, static_cast<u16>(op)); return true;
		case 0x0D: armEmitORI(rt, rs, static_cast<u16>(op)); return true;
		case 0x0E: armEmitXORI(rt, rs, static_cast<u16>(op)); return true;
		case 0x0F: armEmitLUI(rt, static_cast<u16>(op)); return true;
		case 0x18: armEmitDADDI(rt, rs, imm); return true;
		case 0x19: armEmitDADDIU(rt, rs, imm); return true;

		// Scalar loads. The (bits, sign) pair drives the extend inside the helper:
		// LWU zero-extends a word, LD is a full 64-bit load (sign is irrelevant).
		case OP_LB:  armEmitLoadGpr(8,  true,  rt, rs, imm); return true;
		case OP_LBU: armEmitLoadGpr(8,  false, rt, rs, imm); return true;
		case OP_LH:  armEmitLoadGpr(16, true,  rt, rs, imm); return true;
		case OP_LHU: armEmitLoadGpr(16, false, rt, rs, imm); return true;
		case OP_LW:  armEmitLoadGpr(32, true,  rt, rs, imm); return true;
		case OP_LWU: armEmitLoadGpr(32, false, rt, rs, imm); return true;
		case OP_LD:  armEmitLoadGpr(64, false, rt, rs, imm); return true;

		// Scalar stores (the low `bits` bits of GPR[rt]).
		case OP_SB: armEmitStoreGpr(8,  rt, rs, imm); return true;
		case OP_SH: armEmitStoreGpr(16, rt, rs, imm); return true;
		case OP_SW: armEmitStoreGpr(32, rt, rs, imm); return true;
		case OP_SD: armEmitStoreGpr(64, rt, rs, imm); return true;

		// Unaligned load/store byte-merge forms (interpreter-exact; heavily used in
		// memcpy-style loops — previously interpreter single-steps).
		case 0x22: armEmitLWL(rt, rs, imm); return true;
		case 0x26: armEmitLWR(rt, rs, imm); return true;
		case 0x2A: armEmitSWL(rt, rs, imm); return true;
		case 0x2E: armEmitSWR(rt, rs, imm); return true;
		case 0x1A: armEmitLDL(rt, rs, imm); return true;
		case 0x1B: armEmitLDR(rt, rs, imm); return true;
		case 0x2C: armEmitSDL(rt, rs, imm); return true;
		case 0x2D: armEmitSDR(rt, rs, imm); return true;

		// CACHE (0x2f): EE data-cache hint/maintenance. It DOES do real work in the
		// interpreter (Cache.cpp CACHE(): line invalidate/writeback, writes CP0.TagLo)
		// so it can't be a no-op — but it reads rs, writes no GPR, never touches
		// cpuRegs.pc, and raises no exception, so inline-interpret it in-block exactly
		// like the COP0 ops below instead of block-terminating + single-stepping. recTranslateOp
		// runs after recCacheFlushAll, so cpuRegs holds the current rs for the handler.
		// 2nd-most-dominant EE fallback op (The Getaway: ~39% of single-stepped ops).
		case 0x2f: recEmitInterpInline(op); return true;

		// 128-bit quadword load/store (16-byte aligned).
		case OP_LQ: armEmitLoadQuad(rt, rs, imm); return true;
		case OP_SQ: armEmitStoreQuad(rt, rs, imm); return true;

		// FPU load/store (Phase 5.2a) — 32-bit transfer between memory and FPR[rt].
		case OP_LWC1: armEmitLWC1(rt, rs, imm, pc); return true;
		case OP_SWC1: armEmitSWC1(rt, rs, imm, pc); return true;

		// COP0 (Phase 5.1) — same inline-interpreter strategy as COP2: keep straight-line
		// COP0 ops in the block instead of breaking it + single-stepping. COP0 is not a
		// per-op perf item (see x86/iCOP0.cpp's note), so the win is purely avoiding block
		// fragmentation. We must NOT inline anything that:
		//   - writes cpuRegs.pc:       BC0 branches (rs==0x08), ERET (C0 funct 0x18);
		//   - needs a live cpuRegs.cycle: MFC0/MTC0 of Count (Rd==9) or the PERF counters
		//     (Rd==25). This rec only flushes cpuRegs.cycle at the block tail, so a
		//     mid-block read would be stale — COP0.cpp warns that two MFC0 Count in one
		//     block before the cycle update return increment 0 and games lock up;
		//   - gates interrupts with timing the x86 rec specifically branches after: EI/DI,
		//     WAIT.
		// Those stay on the interpreter single-step path (return false). MTC0 Status/Config
		// are fine to inline: the x86 rec doesn't force a branch after them either, so a
		// resulting interrupt is recognised at the block-tail event test just the same;
		// TLB writes call MapTLB→recClear, which is safe mid-block (targeted recLUT reset,
		// the running block keeps its valid host code and recompiles cleared slots on the
		// next dispatch).
		case 0x10:
			switch (rs)
			{
				case 0x00: // MFC0
				case 0x04: // MTC0
					if (rd == 9 || rd == 25)
						return false; // Count / PERF need a live cpuRegs.cycle
					recEmitInterpInline(op);
					return true;
				case 0x10: // C0 — inline the TLB ops only
					switch (funct)
					{
						case 0x01: // TLBR
						case 0x02: // TLBWI
						case 0x06: // TLBWR
						case 0x08: // TLBP
							recEmitInterpInline(op);
							return true;
						default:
							return false; // ERET (0x18) writes PC; EI/DI/WAIT gate interrupts
					}
				default:
					return false; // BC0 branches (rs==0x08) + COP0_Unknown
			}

		// COP2 — VU0 macro mode. CpuVU0 is microVU0 (a recompiler), so a COP2 op may need
		// to finish/sync a deferred VU0 micro program before touching VU0 state. Macro mode
		// (Phase 7.9) drives that precise, analysis-driven sync via the M2 helpers + M1 flags.
		// Transfer ops ported natively as M3 lands them; the rest still inline the interpreter
		// (which self-syncs via _vu0FinishMicro) until M5 ports the ALU. The host-side
		// cpuRegs.code is set before the native handlers because their _Rt_/_Rd_ macros and
		// COP2_Interlock read it at emit time. The BC2 branches (rs==0x08) write cpuRegs.pc
		// and are emitted natively by recRecompile (recIsHandledBranch/recIsLikelyBranch +
		// recEmitBranch/armEmitBranchLikelyTest, Phase M4), which ends the block at them — so
		// they never reach here as a straight-line op (the case below is a defensive fallback).
		case 0x12:
			// vc105: FullVU0SyncHack routes every straight-line COP2 op to the inline
			// interpreter (the interpreter fully syncs VU0 on each op, so the EE/VU0 clocks
			// never drift and the reconciliation can't wedge the scheduler — Ratchet freeze).
			// BC2 branches (rs==0x08) are block-terminating control flow, handled by
			// recRecompile, so they never reach here; the predicate excludes them anyway.
			if (recCop2ForceInterp(op))
			{
				cpuRegs.code = op;
				recEmitInterpInline(op);
				return true;
			}
			switch (rs)
			{
				case 0x01: // QMFC2 (M3.3) — native, memory-backed
					cpuRegs.code = op;
					recQMFC2();
					return true;
				case 0x02: // CFC2 (M3.1) — native, memory-backed
					cpuRegs.code = op;
					recCFC2();
					return true;
				case 0x05: // QMTC2 (M3.3) — native, memory-backed
					cpuRegs.code = op;
					recQMTC2();
					return true;
				case 0x06: // CTC2 (M3.2) — native, memory-backed
					cpuRegs.code = op;
					recCTC2();
					return true;
				case 0x08:
					return false; // BC2F/BC2T/BC2FL/BC2TL — handled natively as a block-terminating
					              // branch in recRecompile (M4); never reached here in practice.
				default:
					// SPECIAL1/SPECIAL2 macro ops. All the VU ALU/transfer families emit natively
					// via the microVU0 single-op emitters (M5.1-M5.4). Faithful to x86 recCOP2_SPEC1:
					// emit the FINISH prologue — mVUFinishVU0 on EEINST_COP2_{SYNC,FINISH}_VU0, a
					// full finish (ALU ops never lazy-SYNC and never interlock) — then the native
					// op. mVUFinishVU0 commits no cycles (so the macro ops are excluded from
					// recOpNeedsCycleFlush and their cycles ride forward).
					//
					// The else branch is reached only by CALLMS/CALLMSR (M5.5), which stay on the
					// interpreter by design — x86 emits them via INTERPRETATE_COP2_FUNC, not a
					// native macro. The inline-interp path is faithful: the interpreter
					// (vu0ExecMicro) self-finishes any running VU0 and launches the microprogram,
					// reading VU state from the memory the macro emitters keep committed — at least
					// as strong as x86's iFlushCall(FLUSH_FREE_XMM | FLUSH_FREE_VU0). The matching
					// cycle commit (x86's scaleblockcycles_clear before recCall) is emitted in
					// recRecompile via recCop2IsCallms/recEmitCommitBlockCycles. (An unknown/illegal
					// COP2 SPECIAL op would also land here and harmlessly run the interpreter.)
					cpuRegs.code = op; // _Fs_/_Ft_/_X_Y_Z_W read microVU0.code = cpuRegs.code
					if (recVUMacroIsMode0(op))
					{
						if (g_pCurInstInfo->info & (EEINST_COP2_SYNC_VU0 | EEINST_COP2_FINISH_VU0))
							mVUFinishVU0();
						recVUMacroEmitMode0(op);
					}
					else
					{
						recEmitInterpInline(op); // CALLMS/CALLMSR (interp by design, M5.5)
					}
					return true;
			}

		// COP2 quadword load/store (VF[rt] ↔ memory). Native (M3.4): the analysis-driven
		// SYNC/FINISH dispatch + the vtlb quad path, targeting VU0.VF[rt]. No COP2_Interlock
		// (faithful to microVU_Macro.inl). cpuRegs.code set for the _Rt_/_Rs_/_Imm_ macros.
		case OP_LQC2:
			cpuRegs.code = op;
			if (recCop2ForceInterp(op)) { recEmitInterpInline(op); return true; } // vc105
			recLQC2();
			return true;
		case OP_SQC2:
			cpuRegs.code = op;
			if (recCop2ForceInterp(op)) { recEmitInterpInline(op); return true; } // vc105
			recSQC2();
			return true;

		default: return false;
	}
}

// --------------------------------------------------------------------------------------
//  Branch / jump compilation (Phase 4.3)
// --------------------------------------------------------------------------------------
// Decode a control-flow opcode at branchpc and emit the matching Phase 4.1/4.2
// generator (which writes cpuRegs.pc and any link register). Returns true if a
// generator handled it. The compile-time target/fallthrough/link constants follow
// the interpreter's macros with _PC_ == branchpc + 4 (the delay-slot address):
//   J/JAL  target = (instr_index << 2) | ((branchpc + 4) & 0xF0000000)
//   branch target = (branchpc + 4) + (s16(imm) << 2)
//   fallthrough / link = branchpc + 8
// Likely branches, coprocessor branches, and traps return false (interpreter
// fallback handles them, including their delay-slot semantics).
static bool recEmitBranch(u32 op, u32 branchpc)
{
	const u32 opcode = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	const u32 rd = (op >> 11) & 0x1f;
	const u32 funct = op & 0x3f;

	const u32 delaypc = branchpc + 4;
	const u32 jtarget = ((op & 0x03ffffff) << 2) | (delaypc & 0xf0000000u);
	const u32 btarget = delaypc + (static_cast<u32>(static_cast<s32>(static_cast<s16>(op))) << 2);
	const u32 fallthrough = branchpc + 8;
	const u32 linkpc = branchpc + 8;

	switch (opcode)
	{
		case 0x02: armEmitJ(jtarget); return true;
		case 0x03: armEmitJAL(jtarget, linkpc); return true;
		case 0x04: armEmitBEQ(rs, rt, btarget, fallthrough); return true;
		case 0x05: armEmitBNE(rs, rt, btarget, fallthrough); return true;
		case 0x06: armEmitBLEZ(rs, btarget, fallthrough); return true;
		case 0x07: armEmitBGTZ(rs, btarget, fallthrough); return true;

		case 0x00: // SPECIAL: JR / JALR
			if (funct == 0x08) { armEmitJR(rs); return true; }
			if (funct == 0x09) { armEmitJALR(rd, rs, linkpc); return true; }
			return false;

		case 0x01: // REGIMM: BLTZ / BGEZ / BLTZAL / BGEZAL (rt selector)
			switch (rt)
			{
				case 0x00: armEmitBLTZ(rs, btarget, fallthrough); return true;
				case 0x01: armEmitBGEZ(rs, btarget, fallthrough); return true;
				case 0x10: armEmitBLTZAL(rs, btarget, fallthrough, linkpc); return true;
				case 0x11: armEmitBGEZAL(rs, btarget, fallthrough, linkpc); return true;
				default: return false; // likely (BLTZL/...) + traps
			}

		case 0x11: // COP1: BC1 branches live under rs==0x08 (BC); rt selects tf/likely.
			if (rs == 0x08)
			{
				if (rt == 0x00) { armEmitBC1F(btarget, fallthrough); return true; }  // BC1F
				if (rt == 0x01) { armEmitBC1T(btarget, fallthrough); return true; }  // BC1T
			}
			return false; // BC1FL/BC1TL (likely) + non-branch COP1 ops

		case 0x12: // COP2: BC2 branches live under rs==0x08 (BC); rt selects tf/likely.
			if (rs == 0x08)
			{
				if (rt == 0x00) { armEmitBC2F(btarget, fallthrough); return true; }  // BC2F
				if (rt == 0x01) { armEmitBC2T(btarget, fallthrough); return true; }  // BC2T
			}
			return false; // BC2FL/BC2TL (likely) + COP2 transfer/macro ops (straight-line)

		case 0x10: // COP0: BC0 branches live under rs==0x08 (BC); rt selects tf/likely.
			if (rs == 0x08)
			{
				if (rt == 0x00) { armEmitBC0F(btarget, fallthrough); return true; }  // BC0F
				if (rt == 0x01) { armEmitBC0T(btarget, fallthrough); return true; }  // BC0T
			}
			return false; // BC0FL/BC0TL (likely) + COP0 transfer/TLB/DI ops (handled elsewhere)

		default: return false;
	}
}

static void recConstApplyBranchLink(u32 op, u32 branchpc, RecGprConstState& state)
{
	const u32 opcode = op >> 26;
	const u32 rd = (op >> 11) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	const u32 funct = op & 0x3f;
	const u32 linkpc = branchpc + 8;

	if (opcode == 0x03) // JAL
		recConstSetKnown(state, 31, linkpc);
	else if (opcode == 0x00 && funct == 0x09) // JALR
		recConstSetKnown(state, rd, linkpc);
	else if (opcode == 0x01 && (rt == 0x10 || rt == 0x11)) // BLTZAL / BGEZAL
		recConstSetKnown(state, 31, linkpc);
}

static bool recConstGetBranchSource(const RecGprConstState& state, u32 reg, bool link_before_read, u32 linkpc, u64* value)
{
	if (link_before_read && reg == 31)
	{
		*value = linkpc;
		return true;
	}

	if (!state.known[reg])
		return false;

	*value = state.value[reg];
	return true;
}

// Return a compile-time known next PC for branches whose condition is unconditional or
// collapses through tracked constants. The branch generator still emits the normal PC
// write; this is only used by the block tail to skip the generic dispatcher lookup.
static bool recGetKnownBranchTarget(u32 op, u32 branchpc, const RecGprConstState& state, u32* target)
{
	const u32 opcode = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;

	const u32 delaypc = branchpc + 4;
	const u32 jtarget = ((op & 0x03ffffff) << 2) | (delaypc & 0xf0000000u);
	const u32 btarget = delaypc + (static_cast<u32>(static_cast<s32>(static_cast<s16>(op))) << 2);
	const u32 fallthrough = branchpc + 8;
	const u32 linkpc = branchpc + 8;
	u64 lhs = 0;
	u64 rhs = 0;

	switch (opcode)
	{
		case 0x02: // J
		case 0x03: // JAL
			*target = jtarget;
			return true;

		case 0x04: // BEQ
			if (recConstGetBranchSource(state, rs, false, linkpc, &lhs) &&
				recConstGetBranchSource(state, rt, false, linkpc, &rhs))
			{
				*target = (lhs == rhs) ? btarget : fallthrough;
				return true;
			}
			return false;

		case 0x05: // BNE
			if (recConstGetBranchSource(state, rs, false, linkpc, &lhs) &&
				recConstGetBranchSource(state, rt, false, linkpc, &rhs))
			{
				*target = (lhs != rhs) ? btarget : fallthrough;
				return true;
			}
			return false;

		case 0x06: // BLEZ
			if (recConstGetBranchSource(state, rs, false, linkpc, &lhs))
			{
				*target = (static_cast<s64>(lhs) <= 0) ? btarget : fallthrough;
				return true;
			}
			return false;

		case 0x07: // BGTZ
			if (recConstGetBranchSource(state, rs, false, linkpc, &lhs))
			{
				*target = (static_cast<s64>(lhs) > 0) ? btarget : fallthrough;
				return true;
			}
			return false;

		case 0x01: // REGIMM
			switch (rt)
			{
				case 0x00: // BLTZ
				case 0x10: // BLTZAL
					if (!recConstGetBranchSource(state, rs, rt == 0x10, linkpc, &lhs))
						return false;
					*target = (static_cast<s64>(lhs) < 0) ? btarget : fallthrough;
					return true;
				case 0x01: // BGEZ
				case 0x11: // BGEZAL
					if (!recConstGetBranchSource(state, rs, rt == 0x11, linkpc, &lhs))
						return false;
					*target = (static_cast<s64>(lhs) >= 0) ? btarget : fallthrough;
					return true;
				default:
					return false;
			}

		default:
			return false;
	}
}

// Is this opcode a control-flow op we have a generator for? (Used to detect the
// block-terminating branch; everything else is either straight-line codegen or an
// interpreter fallback.)
static bool recIsHandledBranch(u32 op)
{
	const u32 opcode = op >> 26;
	const u32 funct = op & 0x3f;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	switch (opcode)
	{
		case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
			return true;
		case 0x00:
			return funct == 0x08 || funct == 0x09;
		case 0x01:
			return rt == 0x00 || rt == 0x01 || rt == 0x10 || rt == 0x11;
		case 0x11: // COP1: only BC1F/BC1T (rs==BC, rt 0/1); all other COP1 ops are straight-line.
			return rs == 0x08 && (rt == 0x00 || rt == 0x01);
		case 0x12: // COP2: only BC2F/BC2T (rs==BC, rt 0/1); all other COP2 ops are straight-line/macro.
			return rs == 0x08 && (rt == 0x00 || rt == 0x01);
		case 0x10: // COP0: only BC0F/BC0T (rs==BC, rt 0/1); MFC0/MTC0/TLB/DI handled elsewhere.
			return rs == 0x08 && (rt == 0x00 || rt == 0x01);
		default:
			return false;
	}
}

// Branch-likely forms (delay slot nullified when not taken). These get native
// codegen via armEmitBranchLikelyTest + a conditional skip over the delay-slot
// code in recRecompile; previously every one forced an interpreter single-step
// block (a C call + full dispatcher round-trip per execution).
static bool recIsLikelyBranch(u32 op)
{
	const u32 opcode = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	switch (opcode)
	{
		case 0x14: // BEQL
		case 0x15: // BNEL
		case 0x16: // BLEZL
		case 0x17: // BGTZL
			return true;
		case 0x01: // REGIMM: BLTZL / BGEZL
			return rt == 0x02 || rt == 0x03;
		case 0x11: // COP1: BC1FL / BC1TL
			return rs == 0x08 && (rt == 0x02 || rt == 0x03);
		case 0x12: // COP2: BC2FL / BC2TL
			return rs == 0x08 && (rt == 0x02 || rt == 0x03);
		case 0x10: // COP0: BC0FL / BC0TL
			return rs == 0x08 && (rt == 0x02 || rt == 0x03);
		default:
			return false;
	}
}

// COP0 DI (disable interrupts): COP0, CO (rs==0x10), funct 0x39.
static bool recIsCop0DI(u32 op)
{
	return (op >> 26) == 0x10 && ((op >> 21) & 0x1f) == 0x10 && (op & 0x3f) == 0x39;
}

// COP0 EI (enable interrupts, funct 0x38) / ERET (exception return, funct 0x18) —
// CO ops (rs==0x10). x86 compiles both via recBranchCall: end the block, run the
// interpreter handler, then event-test + dispatch (EI so a now-unmasked pending IRQ
// is serviced; ERET because it writes cpuRegs.pc). recRecompile handles them that way
// instead of single-stepping (the top EE interpreter fallback after the BC0/MADD work).
static bool recIsCop0EI(u32 op)
{
	return (op >> 26) == 0x10 && ((op >> 21) & 0x1f) == 0x10 && (op & 0x3f) == 0x38;
}
static bool recIsCop0ERET(u32 op)
{
	return (op >> 26) == 0x10 && ((op >> 21) & 0x1f) == 0x10 && (op & 0x3f) == 0x18;
}
static bool recIsCop0EIorERET(u32 op)
{
	return recIsCop0EI(op) || recIsCop0ERET(op);
}

// MIPS trap ops: SPECIAL T{GE,GEU,LT,LTU,EQ,NE} (funct 0x30-0x34,0x36) and REGIMM
// T{GE,GEU,LT,LTU,EQ,NE}I (rt 0x08-0x0C,0x0E). Emitted natively (block-conditional)
// in recRecompile — see recEmitTrapCompareIfTrap.
static bool recIsTrap(u32 op)
{
	const u32 opcode = op >> 26;
	if (opcode == 0x00)
	{
		const u32 funct = op & 0x3f;
		return funct == 0x30 || funct == 0x31 || funct == 0x32 ||
		       funct == 0x33 || funct == 0x34 || funct == 0x36;
	}
	if (opcode == 0x01)
	{
		const u32 rt = (op >> 16) & 0x1f;
		return rt == 0x08 || rt == 0x09 || rt == 0x0A ||
		       rt == 0x0B || rt == 0x0C || rt == 0x0E;
	}
	return false;
}

// Can `op` be safely emitted inline as DI's one-instruction-delayed slot? The x86
// recDI compiles whatever follows DI before applying the interrupt-disable; on this
// rec the delayed op is emitted straight-line via recEmitOp (native, else inline
// interpreter), so it must be an op that is correct to splice mid-block in program
// order. That excludes control-flow / PC-writing / interrupt-gating / exception-
// raising / cycle-sensitive ops, for which we instead end the block at DI and let it
// single-step (DI then applies immediately — an accepted corner; a benign straight-
// line op is what virtually always follows a DI). Branches are caught by the
// recIsHandledBranch / recIsLikelyBranch checks the caller already does.
static bool recCop0DelayOpUnsafe(u32 op)
{
	const u32 opcode = op >> 26;
	const u32 funct = op & 0x3f;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	const u32 rd = (op >> 11) & 0x1f;
	switch (opcode)
	{
		case 0x00: // SPECIAL: SYSCALL / BREAK / traps (JR/JALR already caught as branches)
			return funct == 0x0C || funct == 0x0D || (funct >= 0x30 && funct <= 0x37);
		case 0x01: // REGIMM traps: TGEI/TGEIU/TLTI/TLTIU/TEQI/TNEI (rt 0x08-0x0F)
			return rt >= 0x08 && rt <= 0x0F;
		case 0x10: // COP0: BC0 (rs 0x08); CO ERET/EI/DI/WAIT; cycle-sensitive Count/PERF
			if (rs == 0x08)
				return true;
			if (rs == 0x10) // CO
				return funct == 0x18 || funct == 0x38 || funct == 0x39 || funct == 0x20;
			if ((rs == 0x00 || rs == 0x04) && (rd == 9 || rd == 25))
				return true;
			return false;
		case 0x12: // COP2 / VU0 macro — keep off the inline delay path
		case 0x36: // LQC2 — VU0-syncing
		case 0x3E: // SQC2 — VU0-syncing
			return true;
		default:
			return false;
	}
}

// --------------------------------------------------------------------------------------
//  Wait-loop (idle-loop) detection
// --------------------------------------------------------------------------------------
// A block that ends with a branch back to its own start and whose body carries NO
// register state between iterations (every written GPR derives only from memory
// loads / constants / regs not written in the loop) is a poll loop: its condition
// can only change through an external event (interrupt, DMA, MTVU). Spinning it
// one tiny block at a time until cpuRegs.nextEventCycle burns a full host core —
// the classic EE-at-99% heat case. For such blocks the dispatch tail bumps
// cpuRegs.cycle up to nextEventCycle when the branch was taken, so the next event
// fires after one iteration instead of millions. This mirrors the x86 rec's
// WaitLoop speedhack semantics; conditional loops are gated behind
// EmuConfig.Speedhacks.WaitLoop (default on), unconditional self-loops (which can
// ONLY exit via an event, making the skip exact) are always optimized.
//
// The dataflow check walks the body+delay ops in program order: an op may only
// read a register that is (a) never written in the loop, (b) $zero, or (c) already
// (re)defined earlier in this iteration from allowed sources. A loop-carried
// counter (`addiu t0,t0,-1`) reads its own previous-iteration value and is
// rejected, so calibration/delay loops keep their exact iteration counts.
static constexpr u32 REC_WAITLOOP_MAX_OPS = 8;

// Decode the GPRs an allowed op reads/writes. Returns false if the op is not in
// the allowed (side-effect-free, natively compiled) set.
static bool recWaitLoopClassifyOp(u32 op, u32* reads, u32* writes)
{
	const u32 opcode = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	const u32 rd = (op >> 11) & 0x1f;
	const u32 funct = op & 0x3f;

	*reads = 0;
	*writes = 0;

	if (op == 0) // NOP
		return true;

	switch (opcode)
	{
		case 0x00: // SPECIAL: pure ALU/shift/select subset only
			switch (funct)
			{
				case 0x00: case 0x02: case 0x03: // SLL/SRL/SRA
				case 0x38: case 0x3A: case 0x3B: // DSLL/DSRL/DSRA
				case 0x3C: case 0x3E: case 0x3F: // DSLL32/DSRL32/DSRA32
					*reads = (1u << rt);
					*writes = (1u << rd);
					return true;
				case 0x04: case 0x06: case 0x07: // SLLV/SRLV/SRAV
				case 0x14: case 0x16: case 0x17: // DSLLV/DSRLV/DSRAV
					*reads = (1u << rt) | (1u << rs);
					*writes = (1u << rd);
					return true;
				case 0x20: case 0x21: case 0x22: case 0x23: // ADD/ADDU/SUB/SUBU
				case 0x24: case 0x25: case 0x26: case 0x27: // AND/OR/XOR/NOR
				case 0x2A: case 0x2B:                       // SLT/SLTU
				case 0x2C: case 0x2D: case 0x2E: case 0x2F: // DADD/DADDU/DSUB/DSUBU
					*reads = (1u << rs) | (1u << rt);
					*writes = (1u << rd);
					return true;
				case 0x0A: case 0x0B: // MOVZ/MOVN (rd is read AND written)
					*reads = (1u << rs) | (1u << rt) | (1u << rd);
					*writes = (1u << rd);
					return true;
				default:
					return false;
			}

		case 0x08: case 0x09: case 0x0A: case 0x0B: // ADDI/ADDIU/SLTI/SLTIU
		case 0x0C: case 0x0D: case 0x0E:            // ANDI/ORI/XORI
		case 0x18: case 0x19:                       // DADDI/DADDIU
			*reads = (1u << rs);
			*writes = (1u << rt);
			return true;

		case 0x0F: // LUI (pure constant)
			*writes = (1u << rt);
			return true;

		case OP_LB: case OP_LBU: case OP_LH: case OP_LHU:
		case OP_LW: case OP_LWU: case OP_LD: // scalar loads: rt = mem[rs+imm]
			*reads = (1u << rs);
			*writes = (1u << rt);
			return true;

		default:
			return false;
	}
}

// Run the dataflow check over the loop body (+ branch sources + delay slot).
// `ops` are the straight-line body ops in order; `branch_reads` the GPRs the
// branch condition reads; `delay_op` the delay-slot instruction. Program order
// per iteration is: body ops, branch condition read, delay slot.
static bool recWaitLoopBodyIsPure(const u32* ops, u32 num_ops, u32 branch_reads, u32 delay_op)
{
	// +1 slot for the delay op.
	u32 op_reads[REC_WAITLOOP_MAX_OPS + 1];
	u32 op_writes[REC_WAITLOOP_MAX_OPS + 1];

	for (u32 i = 0; i < num_ops; i++)
	{
		if (!recWaitLoopClassifyOp(ops[i], &op_reads[i], &op_writes[i]))
			return false;
	}
	if (!recWaitLoopClassifyOp(delay_op, &op_reads[num_ops], &op_writes[num_ops]))
		return false;

	// All registers written anywhere in the loop (delay slot included — it runs
	// before the next iteration's body). $zero writes are discarded by codegen.
	u32 written = 0;
	for (u32 i = 0; i <= num_ops; i++)
		written |= op_writes[i] & ~1u;

	// Program-order scan: reading a written-in-loop register before it has been
	// redefined this iteration means loop-carried state (e.g. a decrementing
	// counter) -> reject.
	u32 defined = 0;
	for (u32 i = 0; i < num_ops; i++)
	{
		if (((op_reads[i] & ~1u) & written & ~defined) != 0)
			return false;
		defined |= op_writes[i] & ~1u;
	}
	// Branch condition reads happen after the body...
	if (((branch_reads & ~1u) & written & ~defined) != 0)
		return false;
	// ...and the delay slot runs last.
	if (((op_reads[num_ops] & ~1u) & written & ~defined) != 0)
		return false;

	return true;
}

// GPRs a handled branch op's condition reads.
static u32 recBranchConditionReads(u32 op)
{
	const u32 opcode = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	switch (opcode)
	{
		case 0x02: return 0;                          // J
		case 0x04: case 0x05: return (1u << rs) | (1u << rt); // BEQ/BNE
		case 0x06: case 0x07: return (1u << rs);      // BLEZ/BGTZ
		case 0x01: // REGIMM: BLTZ/BGEZ only — the AL forms write a link register.
			return (rt == 0x00 || rt == 0x01) ? (1u << rs) : 0xffffffffu;
		case 0x10: // COP0: BC0F/BC0T read CPCOND0 (DMAC STAT/PCR), not GPRs -> 0 GPR reads.
			// Lets the DMA-wait spin qualify as a wait-loop so the existing fast-forward
			// idle-skips it (the big win). CPCOND0 flips only at event-scheduled DMA
			// completion, so the skip lands exactly at the next event. Gated by the
			// WaitLoop speedhack (BC0 is left conditional in recBranchIsUnconditional).
			return (rs == 0x08 && (rt == 0x00 || rt == 0x01)) ? 0u : 0xffffffffu;
		default: return 0xffffffffu;                  // anything else: not a candidate
	}
}

// Is this branch unconditionally taken (compile-time)? Such a self-loop can only
// exit via an event, so skipping its cycles is exact, not a speedhack.
static bool recBranchIsUnconditional(u32 op)
{
	const u32 opcode = op >> 26;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	switch (opcode)
	{
		case 0x02: return true;                       // J
		case 0x04: return rs == rt;                   // BEQ r,r
		case 0x06: return rs == 0;                    // BLEZ $zero
		case 0x01: return rt == 0x01 && rs == 0;      // BGEZ $zero
		default: return false;
	}
}

// Emit cpuRegs.code = op, then call the interpreter's handler for `op`. Used for a
// delay-slot instruction the straight-line generators can't handle. Does NOT touch
// cpuRegs.pc (the branch generator already committed the next PC, and a normal
// delay-slot op never writes PC). RESTATEPTR(x19) is callee-saved across the call.
static void recEmitInterpInline(u32 op)
{
	armAsm->Mov(RSCRATCHADDR.W(), op);
	armAsm->Str(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_CODE_OFFSET));
#if ARMSX2_ANDROID_EE_OPHIST
	// Route every in-block interp call through the counting thunk so the INLINE
	// fallback profile (COP2/VU0 etc.) is captured. It reads cpuRegs.code (set above).
	armEmitCall(reinterpret_cast<const void*>(&recOphistInlineThunk));
#else
	armEmitCall(reinterpret_cast<const void*>(::R5900::GetInstruction(op).interpret));
#endif
}

// @@EEDIFF@@ ---------------------------------------------------------------------------
// Differential verifier: wrap ONE straight-line op with a pre-op register snapshot and a
// post-op interpreter re-run + compare. Only emitted when g_ee_diff_verify was set at
// block-compile time (the toggle clears the EE block cache, so blocks recompile with the
// hooks). The caller must have flushed+killed the GPR/const cache first so cpuRegs in
// memory is authoritative at op entry (the recTranslateOp generators read/write cpuRegs
// directly through RESTATEPTR, so after the op runs cpuRegs == REC-post). RESTATEPTR(x19)
// is callee-saved across both C calls; the op itself sits between them unchanged.
//
// Returns true if a native generator handled the op (verify emitted), false if the op
// has no native generator — the caller then falls back to recEmitInterpInline WITHOUT a
// verify (an interp-fallback op can't diverge from itself; the bug is in *rec* codegen).
static bool recEmitDiffVerifyOp(u32 op, u32 pc)
{
	const u32 primary = op >> 26;

	// EXCLUDE coprocessor ops from the verify wrapper. recTranslateOp handles some COP0
	// (MFC0/MTC0/TLB) and COP2 (QMFC2/CFC2/QMTC2/CTC2/LQC2/SQC2/VU0 macro) ops by
	// emitting an inline interpreter call and/or touching VU0/COP0 state — NOT plain
	// GPRs. Re-running those on the interpreter would (a) compare interp-against-interp
	// (a tautology) and (b) DANGEROUSLY re-execute VU0 finish/launch side effects (only
	// memory writes are captured; VU state changes are not). The True Crime texture bug
	// is pure EE data-gen (ALU/shift/MMI/load-store), so restrict the verifier to those.
	// Returning false makes the caller single-step these on the interpreter, un-verified.
	//   0x10 COP0, 0x11 COP1(FPU), 0x12 COP2, 0x31 LWC1, 0x36 LQC2, 0x39 SWC1, 0x3e SQC2.
	// The COP1 family (COP1 + LWC1/SWC1) is excluded too: the snapshot/compare covers
	// GPR/HI/LO only, not fpr[]/ACC/FCR31, so an FPU re-run would (a) never be checked and
	// (b) leave the interpreter's fpr value in memory (restoreFrom only rewinds
	// GPR/HI/LO/PC/sa). The texture bug is integer, so this is a safe, deliberate scope
	// limit — extend the snapshot to the FPU file if an FP miscompile is ever suspected.
	if (primary == 0x10 || primary == 0x11 || primary == 0x12 || primary == 0x31 ||
		primary == 0x36 || primary == 0x39 || primary == 0x3e)
		return false;

	// Pre-op snapshot of cpuRegs -> g_diff_pre (no args).
	armEmitCall(reinterpret_cast<const void*>(&eeDiffSnapshotPre));

	// The real recompiled op — the UN-cached, memory-committed generator path (same one
	// recTranslateOpOptimized falls through to). Reads/writes guest state via RESTATEPTR.
	if (!recTranslateOp(op, pc))
		return false;

	// Post-op verify: eeDiffVerify(pc, op). Args in x0/x1 (RXARG1/RXARG2).
	armAsm->Mov(RXARG1.W(), pc);
	armAsm->Mov(RXARG2.W(), op);
	armEmitCall(reinterpret_cast<const void*>(&eeDiffVerify));
	return true;
}
// @@EEDIFF@@ ---------------------------------------------------------------------------

// COP0 DI — clear Status.EIE (disable interrupts) under the same condition as
// Interpreter::COP0::DI and the x86 recDI (iCOP0.cpp): only when the CPU is in a
// privileged context, i.e. (Status & (EXL|ERL|EDI)) != 0  ||  Status.KSU == 0.
// This emits just the "DI takes effect" status update; the one-instruction delay
// the x86 rec applies (recompileNextInstruction before this) is reproduced by the
// caller in recRecompile, which emits the following guest instruction first.
//
// Emitted with only encodable logical immediates so VIXL never needs a scratch
// register, and the status word is held in RSCRATCHADDR.W() (x17, removed from the
// VIXL scratch pool in armStartBlock) — so it cannot be clobbered by an implicit
// VIXL temp. Touches only cpuRegs.CP0.n.Status (no guest GPRs), so it is safe to
// splice into the middle of a block after the delayed instruction.
static void recEmitCop0DI()
{
	const a64::Register status = RSCRATCHADDR.W();
	armAsm->Ldr(status, a64::MemOperand(RESTATEPTR, EE_COP0_STATUS_OFFSET));

	a64::Label do_clear, done;
	armAsm->Tst(status, 0x6);      // EXL | ERL set -> privileged, clear EIE
	armAsm->B(&do_clear, a64::ne);
	armAsm->Tst(status, 0x20000);  // EDI set -> clear EIE
	armAsm->B(&do_clear, a64::ne);
	armAsm->Tst(status, 0x18);     // KSU: non-zero == user/supervisor -> leave EIE
	armAsm->B(&done, a64::ne);
	armAsm->Bind(&do_clear);
	armAsm->Bic(status, status, 0x10000); // EIE
	armAsm->Str(status, a64::MemOperand(RESTATEPTR, EE_COP0_STATUS_OFFSET));
	armAsm->Bind(&done);
}

// Compile one straight-line or delay-slot instruction: const-folded/native generator
// if we have one, otherwise an inline interpreter call.
// Block cycles accumulated up to and including the current COP2/LQC2/SQC2 op, stashed by the
// emit loop (recRecompile) for the op's handler to hand to the M2 sync helpers. The faithful
// analog of x86's s_nBlockCycles fed to scaleblockcycles_clear(): the helpers commit it to
// cpuRegs.cycle only on a real SYNC (mVUSyncVU0 / the COP2_Interlock SYNC branch), and the emit
// loop clears the accumulator only then. FINISH-only / no-sync ops leave the cycles in the
// accumulator so they ride forward and survive _vu0FinishMicro's cpuRegs.cycle = VU0.cycle
// collapse (a pre-commit, as the old unconditional pre-flush did, would be lost there).
static u32 s_cop2RawCycles = 0;

static void recEmitOp(u32 op, RecGprConstState& const_state, RecGprCacheState& cache_state, u32 pc)
{
	// Used only for branch delay slots, which the main emit loop's COP2 cycle stash does not
	// reach. A COP2/LQC2/SQC2 op here would otherwise read a stale s_cop2RawCycles; zero it so
	// its sync helper commits nothing (the block ends right after the delay slot, so the block
	// tail commits the accumulated cycles for accounting). The VU catch-up still reads the
	// current cpuRegs.cycle. Harmless for non-COP2 ops (they ignore it).
	s_cop2RawCycles = 0;
	if (!recTranslateOpOptimized(op, const_state, cache_state, pc))
		recEmitInterpInline(op);
}

// cpuRegs.pc = imm (block fallthrough / early-exit target).
static void recEmitWritePc(u32 pc)
{
	armAsm->Mov(RSCRATCHADDR.W(), pc);
	armAsm->Str(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_PC_OFFSET));
}

// Tail-dispatch to a compile-time-known next PC via the block's recLUT slot
// (adrp+add+ldr+br). This is now the FALLBACK path: when s_eeBlockLinkEnabled is
// set (default), recEmitEventTestAndDispatch instead emits a patchable direct B
// (recEmitLinkableExitToKnownPc) and the inbound-link backpatching this comment
// once warned was missing is implemented in eeInvalidateLinks (@@MAC_EE_BLOCKLINK@@).
// The LUT slot remains the single SMC-invalidation rewrite point, so this fallback
// can never enter a stale block; the slot load is a same-cacheline hit in steady state.
static void recEmitDispatchToKnownPc(u32 pc)
{
	armMoveAddressToReg(RXARG3, recPtrToBlock(pc));
	armAsm->Ldr(RXARG3, a64::MemOperand(RXARG3));
	armAsm->Br(RXARG3);
}

// EE cycle scaling — mirrors iR5900.cpp scaleblockcycles_calculation() so block
// timing matches the x86 rec / interpreter for a given EECycleRate.
static u32 recScaleBlockCycles(u32 raw)
{
	const bool lowcycles = (raw <= 40);
	const s8 cyclerate = EmuConfig.Speedhacks.EECycleRate;
	u32 scale_cycles;

	if (cyclerate == 0 || lowcycles || cyclerate < -99 || cyclerate > 3)
		scale_cycles = raw >> 3;
	else if (cyclerate > 1)
		scale_cycles = raw >> (2 + cyclerate);
	else if (cyclerate == 1)
		scale_cycles = static_cast<u32>((raw >> 3) / 1.3f);
	else if (cyclerate == -1)
		scale_cycles = (raw <= 80 || raw > 168 ? 5 : 7) * raw / 32;
	else
		scale_cycles = ((5 + (-2 * (cyclerate + 1))) * raw) >> 5;

	return (scale_cycles < 1) ? 1 : scale_cycles;
}

// Commit the block's accumulated (scaled) cycles to cpuRegs.cycle, mirroring x86's
// scaleblockcycles_clear() add. Used by the CALLMS/CALLMSR path: x86's INTERPRETATE_COP2_FUNC
// does `cpuRegs.cycle += scaleblockcycles_clear()` immediately before calling the interpreter,
// so the VU0 microprogram it launches (vu0ExecMicro sets VU0.cycle = cpuRegs.cycle) starts at
// the correct EE time. This is the same commit emitted inside mVUSyncVU0, minus the VU0
// catch-up — a LAUNCH (unlike a FINISH) does not collapse cpuRegs.cycle, so the cycles must be
// committed here rather than ridden forward. RXVIXLSCRATCH (x16) is dead between ops.
static void recEmitCommitBlockCycles(u32 raw)
{
	if (raw == 0)
		return;
	armAsm->Ldr(RXVIXLSCRATCH, a64::MemOperand(RESTATEPTR, EE_CYCLE_OFFSET));
	armAsm->Add(RXVIXLSCRATCH, RXVIXLSCRATCH, recScaleBlockCycles(raw));
	armAsm->Str(RXVIXLSCRATCH, a64::MemOperand(RESTATEPTR, EE_CYCLE_OFFSET));
}

// --------------------------------------------------------------------------------------
//  MIPS trap opcodes — native codegen (block-conditional). The interpreter trap funcs
//  (R5900OpcodeImpl.cpp) compute "if (cond) trap()", and trap() does cpuRegs.pc -= 4 then
//  cpuException(0x34) which redirects pc to the exception vector. The recompiler can't
//  continue straight-line through a taken trap, so this mirrors x86's recBranchCall
//  treatment (block-terminating) — but only on the rare TAKEN path: we emit a native
//  64-bit compare and branch OVER the raise block when the trap is NOT taken (the common
//  case stays in-block, no dispatch). On the taken path we run the interpreter op (which
//  raises), commit the block's cycles, and tail into DispatcherEvent to service events and
//  re-dispatch from the new pc. The caller has already flushed+killed the GPR cache (so
//  memory is authoritative for both the compare and the interpreter), exactly like a
//  branch. RSCRATCHADDR(x17)=lhs, RXVIXLSCRATCH(x16)=rhs — dead scratch between ops; both
//  are consumed by the Cmp before the raise block (which reuses x17) runs.
//
//  The skip condition passed in is the INVERSE of the interpreter's trap-if test:
//   TGE/TGEI rs>=rt  -> skip lt   TGEU/TGEIU rs>=rt(u) -> skip lo
//   TLT/TLTI rs<rt   -> skip ge   TLTU/TLTIU rs<rt(u)  -> skip hs
//   TEQ/TEQI rs==rt  -> skip ne   TNE/TNEI   rs!=rt    -> skip eq
static void recEmitTrapRegCompare(u32 rs, u32 rt, a64::Condition skip_cond, a64::Label* skip)
{
	armAsm->Ldr(RSCRATCHADDR, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs))); // GPR[rs].UD[0]
	if (rt == 0)
		armAsm->Cmp(RSCRATCHADDR, 0);
	else
	{
		armAsm->Ldr(RXVIXLSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt))); // GPR[rt].UD[0]
		armAsm->Cmp(RSCRATCHADDR, RXVIXLSCRATCH);
	}
	armAsm->B(skip, skip_cond);
}

static void recEmitTrapImmCompare(u32 rs, s32 imm, a64::Condition skip_cond, a64::Label* skip)
{
	armAsm->Ldr(RSCRATCHADDR, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rs))); // GPR[rs].UD[0]
	// _Imm_ is the sign-extended 16-bit immediate. The signed forms compare against the
	// s64 value; the unsigned forms (TGEIU/TLTIU) compare (u64)_Imm_, which is the same
	// bit pattern — only the branch condition differs.
	armAsm->Mov(RXVIXLSCRATCH, static_cast<u64>(static_cast<s64>(imm)));
	armAsm->Cmp(RSCRATCHADDR, RXVIXLSCRATCH);
	armAsm->B(skip, skip_cond);
}

// Emits the trap-condition compare + "branch over the raise block when NOT taken" for a
// trap op; returns false (emitting nothing) for a non-trap op. Decode mirrors recIsTrap.
static bool recEmitTrapCompareIfTrap(u32 op, a64::Label* skip)
{
	const u32 opcode = op >> 26;
	const u32 funct = op & 0x3f;
	const u32 rs = (op >> 21) & 0x1f;
	const u32 rt = (op >> 16) & 0x1f;
	const s32 imm = static_cast<s16>(op);
	if (opcode == 0x00) // SPECIAL register-form traps
	{
		switch (funct)
		{
			case 0x30: recEmitTrapRegCompare(rs, rt, a64::lt, skip); return true; // TGE
			case 0x31: recEmitTrapRegCompare(rs, rt, a64::lo, skip); return true; // TGEU
			case 0x32: recEmitTrapRegCompare(rs, rt, a64::ge, skip); return true; // TLT
			case 0x33: recEmitTrapRegCompare(rs, rt, a64::hs, skip); return true; // TLTU
			case 0x34: recEmitTrapRegCompare(rs, rt, a64::ne, skip); return true; // TEQ
			case 0x36: recEmitTrapRegCompare(rs, rt, a64::eq, skip); return true; // TNE
			default:   return false;
		}
	}
	if (opcode == 0x01) // REGIMM immediate-form traps
	{
		switch (rt)
		{
			case 0x08: recEmitTrapImmCompare(rs, imm, a64::lt, skip); return true; // TGEI
			case 0x09: recEmitTrapImmCompare(rs, imm, a64::lo, skip); return true; // TGEIU
			case 0x0A: recEmitTrapImmCompare(rs, imm, a64::ge, skip); return true; // TLTI
			case 0x0B: recEmitTrapImmCompare(rs, imm, a64::hs, skip); return true; // TLTIU
			case 0x0C: recEmitTrapImmCompare(rs, imm, a64::ne, skip); return true; // TEQI
			case 0x0E: recEmitTrapImmCompare(rs, imm, a64::eq, skip); return true; // TNEI
			default:   return false;
		}
	}
	return false;
}

// vc105: FullVU0SyncHack correctness path. Native macro-mode COP2 uses an analysis-driven,
// one-block-at-a-time VU0 sync that lets cpuRegs.cycle and VU0.cycle drift billions apart when
// the EE defers a launched program; the eventual `cpuRegs.cycle += VU0.cycle - startcycle`
// reconciliation in _vu0run then slams the EE clock backward and the event scheduler wedges
// (Ratchet: Deadlocked SCUS-97465 — image freezes, audio keeps running). The interpreter never
// drifts because it fully syncs (vu0Sync/_vu0FinishMicro) on EVERY COP2 op (VU0.cpp). So under
// FullVU0SyncHack we route every straight-line COP2 op (transfers + macro ALU + LQC2/SQC2, but
// NOT the BC2 branches, which are block-terminating control flow) to the inline interpreter —
// exactly the mechanism CALLMS/CALLMSR already use (recEmitInterpInline + a live-cycle commit).
// The EE recompiler still handles all non-COP2 code, so this is far faster than EE-interpreter.
static bool recCop2ForceInterp(u32 op)
{
	// FullVU0SyncHack correctness path. The native macro-mode COP2 recompiler has two timing
	// defects on VU0-handshake games (Ratchet: Deadlocked SCUS-97465): (1) a backward EE-clock
	// jump on deferred VU0 finish — fixed by the monotonic guard in _vu0run (VU0.cpp); and (2) a
	// deeper EE event-scheduler wedge when the game idles that the guard does NOT fix (vc106
	// still froze at d=+3.9e9). Routing every straight-line COP2 op to the inline interpreter
	// (proven: vc105 runs at 100% / native res) sidesteps BOTH — the interpreter keeps VU0
	// tightly synced every op, so the scheduler never wedges. The EE rec still runs all non-COP2
	// code, so the cost is only the few VU0 ops per frame. kForceInterp stays true; the _vu0run
	// guard is kept anyway (a correct monotonic-clock invariant, harmless in the normal case).
	// vc110: the freeze is a VU0<->EE HANDSHAKE DEADLOCK — the game's main thread blocks in the
	// kernel idle loop waiting for a VU0-side signal the native macro-mode COP2 path drops (the
	// interpreter produces it; vc105 never froze). That signal rides the EE<->VU0 TRANSFER ops,
	// not the vertex math. kMode: 0 = all-native; 1 = transfers (QMFC2/CFC2/QMTC2/CTC2) + LQC2/
	// SQC2 → interp, keep hot native VU0 macro ALU on the rec (this test — perf-preserving);
	// 2 = all COP2 → interp (vc105 fallback, safe/slower).
	if (!EmuConfig.Gamefixes.FullVU0SyncHack)
		return false;
	// kMode: 0 = all native (fast, alignment/flicker glitch); 1 = transfers+LQC2/SQC2 → interp;
	// 2 = all COP2 → interp (correct, slower); 3 = READS (QMFC2/CFC2) → interp (vc115: still broken);
	// 4 = ALU/SPECIAL (rs>=0x10) → interp, transfers+LQC2/SQC2 native (vc116 — the never-tested
	// complement of kMode 1). Evidence: EVERY correct config (EE-interp, vc105) runs the macro ALU
	// on the interpreter; EVERY broken one (native, vc110, vc115) runs it native — transfers/reads
	// have been interp-routed both ways without fixing the graphics. This isolates the ALU. The
	// interp COP2_SPECIAL wrapper also computes FULL flags (no FLAGHACK elision) — a wrong
	// MAC/status-flag read is exactly the boolean-flip signature (stuck facing / model blinking).
	// vc117 bisect: kMode 5 = SPECIAL2 (funct>=0x3c: ADDA/MADDA/MULA/SUBA/MSUBA families, CLIP,
	// DIV/SQRT/RSQRT, MOVE/MR32, ITOF/FTOI, LQI/SQI/LQD/SQD, MFIR/MTIR, RNG) → interp; SPECIAL1
	// (funct<0x3c: VADD/VSUB/VMUL/VMADD/VMSUB/VMAX/VMINI + bc/i/q, IADD/ISUB/IAND/IOR) stays
	// NATIVE. kMode 6 = the complement (SPECIAL1 → interp, SPECIAL2 native). vc116 (kMode 4,
	// all-ALU→interp) proved the bug is in one of these; this halves the search.
	// vc124: runtime mode from <DataRoot>/cop2mode.txt (see recLoadCop2Mode). Modes:
	// 0=all native; 1=transfers+LQ/SQ→interp; 2=ALL COP2→interp (safe); 3=reads→interp;
	// 4=all ALU→interp (vc116-good); 5=SPECIAL2→interp (vc117-good); 6=SPECIAL1→interp;
	// 7-11 = suspect-group→interp bisects; 12-16 = ONLY-one-group-NATIVE probes
	// (12=ACC,13=Q,14=mem/VI,15=moves/clip,16=converts — broken verdict convicts that group).
	if (!s_cop2ModeLoaded)
		recLoadCop2Mode();
	const int kMode = s_cop2InterpMode;
	if (kMode == 0)
		return false;
	const u32 opc = op >> 26;
	if (opc == OP_LQC2 || opc == OP_SQC2)
		return (kMode == 1 || kMode == 2); // kMode 3/4/5/6 keep LQC2/SQC2 native
	if (opc != 0x12)
		return false;
	const u32 rs = (op >> 21) & 0x1f;
	if (rs == 0x08)
		return false; // BC2 branches — control flow, native
	if (kMode == 2)
		return true; // all straight-line COP2 → interp
	if (kMode == 3)
		return (rs == 0x01 || rs == 0x02); // READS only (QMFC2/CFC2) → interp
	if (kMode == 4)
		return rs >= 0x10; // ALL ALU/SPECIAL → interp (vc116 — known-good graphics)
	if (kMode == 5)
		return rs >= 0x10 && (op & 0x3f) >= 0x3c; // SPECIAL2 → interp, SPECIAL1 native
	if (kMode == 6)
		return rs >= 0x10 && (op & 0x3f) < 0x3c; // SPECIAL1 → interp, SPECIAL2 native
	if (kMode == 7)
	{
		// vc118: ONLY the SPECIAL2 ACC families → interp (ADDA/SUBA/MADDA/MSUBA/MULA + bc/i/q,
		// OPMULA — everything that writes/reads the accumulator); the rest of SPECIAL2 (ITOF/
		// FTOI/ABS/CLIP/MOVE/MR32/LQI/SQI/DIV/SQRT/RSQRT/MTIR/MFIR/ILWR/ISWR/RNG) NATIVE, all
		// of SPECIAL1 NATIVE. Suspect: macro-mode ACC doesn't survive the per-op regAlloc
		// reset/flush round-trip (MULA's ACC lost before MADDA reads it).
		if (rs < 0x10 || (op & 0x3f) < 0x3c)
			return false; // not SPECIAL2
		const u32 idx = (op & 3) | ((op >> 4) & 0x7c); // SPECIAL2 table index
		return idx <= 0x0f                                  // ADDAbc/SUBAbc/MADDAbc/MSUBAbc
		    || (idx >= 0x18 && idx <= 0x1c)                 // MULAbc + MULAq
		    || idx == 0x1e                                  // MULAi
		    || (idx >= 0x20 && idx <= 0x2e && idx != 0x2b); // *Aq/*Ai/plain A-forms + OPMULA
	}
	if (kMode == 8)
	{
		// vc119: vc118 (ACC→interp, rest native) BROKE → ACC families are INNOCENT; the bug is
		// in the rest of SPECIAL2. Prime suspect = the Q-register group (DIV/SQRT/RSQRT, mode
		// 0x112: bespoke Q lane load/store + D/I flag fold). Route ONLY those (+WAITQ) → interp;
		// EVERYTHING else native.
		if (rs < 0x10 || (op & 0x3f) < 0x3c)
			return false; // not SPECIAL2
		const u32 idx = (op & 3) | ((op >> 4) & 0x7c);
		return idx >= 0x38 && idx <= 0x3b; // DIV/SQRT/RSQRT/WAITQ → interp
	}
	if (kMode == 9)
	{
		// vc120: vc119 exonerated the Q group (interp'd, still broke). Guilty ∈ misc SPECIAL2.
		// This splits it: VI-pointer/memory ops (LQI/SQI/LQD/SQD 0x34-0x37 — matrix-row walking —
		// + MTIR/MFIR/ILWR/ISWR 0x3c-0x3f) → interp; converts/moves/CLIP/RNG + ACC + Q all NATIVE.
		if (rs < 0x10 || (op & 0x3f) < 0x3c)
			return false; // not SPECIAL2
		const u32 idx = (op & 3) | ((op >> 4) & 0x7c);
		return (idx >= 0x34 && idx <= 0x37) || (idx >= 0x3c && idx <= 0x3f);
	}
	if (kMode == 10)
	{
		// vc121: vc120 exonerated mem/VI ops. Remaining: ITOF/FTOI, ABS, CLIP, MOVE, MR32, RNG.
		// This puts the moves/clip cluster (ABS 0x1d, CLIP 0x1f, MOVE 0x30, MR32 0x31) → interp;
		// converts (ITOF/FTOI 0x10-0x17) + RNG (0x40-0x43) stay NATIVE with everything else.
		if (rs < 0x10 || (op & 0x3f) < 0x3c)
			return false; // not SPECIAL2
		const u32 idx = (op & 3) | ((op >> 4) & 0x7c);
		return idx == 0x1d || idx == 0x1f || idx == 0x30 || idx == 0x31;
	}
	if (kMode == 11)
	{
		// vc122: single-culprit ledger leaves {ITOF/FTOI, RNG}. Converts only → interp
		// (idx 0x10-0x17); RNG + everything else NATIVE. If broken too, the single-culprit
		// assumption is wrong (two guilty families in different bisect groups).
		if (rs < 0x10 || (op & 0x3f) < 0x3c)
			return false; // not SPECIAL2
		const u32 idx = (op & 3) | ((op >> 4) & 0x7c);
		return idx >= 0x10 && idx <= 0x17; // ITOF0/4/12/15 + FTOI0/4/12/15 → interp
	}
	// vc123+ (kMode 12..16): MULTI-CULPRIT hunt — flip ONE group native at a time from the
	// known-good all-SPECIAL2-interp baseline (vc117). Each build independently convicts or
	// clears its group: broken => that group is guilty; good => innocent. SPECIAL1 stays
	// native throughout (exonerated by vc117).
	if (kMode >= 12 && kMode <= 16)
	{
		if (rs < 0x10 || (op & 0x3f) < 0x3c)
			return false; // not SPECIAL2
		const u32 idx = (op & 3) | ((op >> 4) & 0x7c);
		const bool acc   = idx <= 0x0f || (idx >= 0x18 && idx <= 0x1c) || idx == 0x1e
		                || (idx >= 0x20 && idx <= 0x2e && idx != 0x2b);
		const bool qgrp  = idx >= 0x38 && idx <= 0x3b;
		const bool mem   = (idx >= 0x34 && idx <= 0x37) || (idx >= 0x3c && idx <= 0x3f);
		const bool moves = idx == 0x1d || idx == 0x1f || idx == 0x30 || idx == 0x31;
		const bool conv  = idx >= 0x10 && idx <= 0x17;
		switch (kMode)
		{
			case 12: return !acc;   // ONLY ACC native (vc123)
			case 13: return !qgrp;  // ONLY Q-group native
			case 14: return !mem;   // ONLY mem/VI native
			case 15: return !moves; // ONLY ABS/CLIP/MOVE/MR32 native
			case 16: return !conv;  // ONLY converts native
		}
	}
	return (rs == 0x01 || rs == 0x02 || rs == 0x05 || rs == 0x06); // transfers only (kMode 1)
}

// True for ops that run the interpreter inline AND need a live, current cpuRegs.cycle —
// COP2 / VU0-macro ops (opcode 0x12, excluding the BC2 branches which already single-step).
// The VU sync inside the COP2 handler reads cpuRegs.cycle, so the block's accumulated cycles
// must be committed first; x86 does this via `cpuRegs.cycle += scaleblockcycles_clear()` before
// every COP2 op (microVU_Macro.inl). Without it the VU kicks at a stale EE time and geometry
// is submitted a beat early/late (e.g. Crash Twinsanity object pop-in / overlap).
// True for COP2 / VU0-macro ops (opcode 0x12, excluding the BC2 branches) and the COP2 quad
// load/stores (LQC2/SQC2). Their macro-mode handlers may emit a VU0 catch-up sync that reads
// cpuRegs.cycle, so the emit loop stashes the block's accumulated cycles (s_cop2RawCycles) for
// the handler to pass into the M2 sync helpers. The helpers commit those cycles to cpuRegs.cycle
// exactly where x86 does — inside mVUSyncVU0 / the COP2_Interlock SYNC branch — and ONLY when the
// op actually syncs VU0. (The cycles must NOT be committed before a FINISH: _vu0FinishMicro
// overwrites cpuRegs.cycle with VU0.cycle (VU0.cpp), so a pre-commit would be lost; x86 keeps
// the uncommitted cycles in s_nBlockCycles so they ride past the finish.) See recRecompile.
static bool recOpNeedsCycleFlush(u32 op)
{
	// Force-interp COP2 ops run the interpreter inline (like CALLMS) and need a live clock.
	if (recCop2ForceInterp(op))
		return true;
	if ((op >> 26) == 0x12)
	{
		if (((op >> 21) & 0x1f) == 0x08)
			return false; // BC2 branch — no sync / cycle commit (M4)
		// Native Mode-0 ALU ops (M5.1) only ever FINISH (mVUFinishVU0 commits nothing),
		// so their cycles must accumulate and ride forward to the next real sync / block
		// tail — not be stashed-and-cleared on EEINST_COP2_SYNC_VU0. Treat them like a
		// normal op. Transfer ops + still-inline-interp ALU ops keep the stash+clear path.
		return !recVUMacroIsMode0(op);
	}
	return (op >> 26) == OP_LQC2 || (op >> 26) == OP_SQC2;
}

// True for MFC0/MTC0 of the Count (rd 9) or PERF (rd 25) registers — the COP0 ops the EE
// rec previously single-stepped (recTranslateOpOptimized returns false for them) because
// they read a live cpuRegs.cycle this rec only flushes at the block tail. Games busy-poll
// Count for timing, so the single-step path can dominate EE (Jackie Chan Adventures: ~80%
// of EE fallbacks). recRecompile handles these by committing the block's accumulated cycles
// before an INLINE interp call (so the read is live), instead of the expensive single-step.
// Per-op commit also fixes the historic "two MFC0 Count in one block read the same stale
// value -> games lock up" hazard: each read now advances cpuRegs.cycle. Excludes BC0 / ERET
// / EI / DI / WAIT, which still single-step (they write PC or gate interrupts).
static bool recCop0NeedsLiveCycle(u32 op)
{
	if ((op >> 26) != 0x10)
		return false; // COP0
	const u32 rs = (op >> 21) & 0x1f;
	if (rs != 0x00 && rs != 0x04)
		return false; // MFC0 / MTC0 only (BC0 rs==0x08 + C0 rs==0x10 keep their paths)
	const u32 rd = (op >> 11) & 0x1f;
	return (rd == 9 || rd == 25); // Count / PERF
}

// CALLMS (COP2 SPECIAL1 funct 0x38) / CALLMSR (0x39) — x86's only INTERPRETATE_COP2_FUNC ops
// (microVU_Macro.inl:295-296). M5.5 keeps them on the inline interpreter (faithful: the interp
// path self-finishes VU0 and launches the microprogram via vu0ExecMicro), but unlike the native
// FINISH macro ops they must commit the block cycles before the launch — see recRecompile. The
// rs>=0x10 guard restricts to CO/SPECIAL1 ops (excludes the transfer ops, whose low 6 bits are
// rd/sa, not a funct); funct 0x38/0x39 is always SPECIAL1 (SPECIAL2 is funct 0x3c-0x3f).
static bool recCop2IsCallms(u32 op)
{
	if ((op >> 26) != 0x12 || ((op >> 21) & 0x1f) < 0x10)
		return false;
	const u32 funct = op & 0x3f;
	return funct == 0x38 || funct == 0x39;
}

// --------------------------------------------------------------------------------------
//  Macro mode (Phase 7.9 / M2) — EE↔VU0 sync / interlock emit helpers
// --------------------------------------------------------------------------------------
// Faithful VIXL ports of microVU_Macro.inl's mVUFinishVU0 / mVUSyncVU0 / COP2_Interlock.
// These emit the *precise, analysis-driven* VU0 catch-up that x86 macro mode does, to
// replace the current blanket inline-interp self-sync (Phase 5.3). They are not wired
// into the COP2 path yet — M3 consumes the M1 EEINST_COP2_* flags through them — so they
// are [[maybe_unused]] for now (no behavior change this phase).
//
// Translation notes vs x86:
//   - No EE register allocator on ARM64, so the x86 iFlushCall(FLUSH_FOR_POSSIBLE_MICRO_EXEC)
//     / _freeX86reg(eax) calls have no equivalent — we are memory-backed and use the
//     caller-saved scratch GPRs directly (M3's transfer ops likewise spill to cpuRegs).
//   - x86's `rax` (block-cycle accumulator -> VU0 catch-up delta) maps to RXVIXLSCRATCH (x16),
//     which is dead before the ExecuteBlockJIT args are loaded into x0/x1.
//   - x86 scaleblockcycles_clear() is reproduced with recScaleBlockCycles(raw): the caller
//     passes the block's accumulated raw cycles (s_cop2RawCycles), the helper commits them to
//     cpuRegs.cycle here (its `if (raw != 0)` branch), and the emit loop clears its accumulator
//     iff this op syncs — see recOpNeedsCycleFlush / s_cop2RawCycles.
//   - xLoadFarAddr(arg1reg, CpuVU0) bakes the (stable, post-init) CpuVU0 object pointer as an
//     immediate; armMoveAddressToReg(RXARG1, CpuVU0) does the same. s_nBlockInterlocked is a
//     compile-time bool baked into arg2 just like x86.

// Per-block "this block contains an interlocked (cpuRegs.code & 1) COP2 op" flag — x86's
// s_nBlockInterlocked. Set by COP2_Interlock, baked into the ExecuteBlockJIT `interlocked`
// arg, reset per block in recRecompile.
static bool s_nBlockInterlocked = false;

// mVUFinishVU0: if VU0 is running a micro program (VPU_STAT&1), finish it (run to E-bit).
static void mVUFinishVU0()
{
	armMoveAddressToReg(RSCRATCHADDR, &VU0.VI[REG_VPU_STAT].UL);
	armAsm->Ldr(RWARG3, a64::MemOperand(RSCRATCHADDR));
	a64::Label skipvuidle;
	armAsm->Tbz(RWARG3, 0, &skipvuidle); // VPU_STAT&1 == 0 -> nothing running
	armEmitCall(reinterpret_cast<const void*>(_vu0FinishMicro));
	armAsm->Bind(&skipvuidle);
}

// mVUSyncVU0: commit the block's cycles, then if VU0 is running and has fallen >=4 cycles
// behind the EE, run one VU0 block to catch it up (lazy sync, not a full finish).
static void mVUSyncVU0(u32 raw)
{
	const a64::Register rax = RXVIXLSCRATCH; // x16 (dead before the call args are set up)

	// scaleblockcycles_clear(): cpuRegs.cycle += scaled raw; keep the new value in rax.
	armAsm->Ldr(rax, a64::MemOperand(RESTATEPTR, EE_CYCLE_OFFSET));
	if (raw != 0)
	{
		armAsm->Add(rax, rax, recScaleBlockCycles(raw));
		armAsm->Str(rax, a64::MemOperand(RESTATEPTR, EE_CYCLE_OFFSET));
	}

	armMoveAddressToReg(RSCRATCHADDR, &VU0.VI[REG_VPU_STAT].UL);
	armAsm->Ldr(RWARG3, a64::MemOperand(RSCRATCHADDR));
	a64::Label skipvuidle;
	armAsm->Tbz(RWARG3, 0, &skipvuidle);

	// rax -= VU0.cycle  (and, under the VU-sync gamefixes, -= VU0.nextBlockCycles)
	armMoveAddressToReg(RSCRATCHADDR, &VU0.cycle);
	armAsm->Ldr(RXARG3, a64::MemOperand(RSCRATCHADDR));
	armAsm->Sub(rax, rax, RXARG3);
	if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
	{
		armMoveAddressToReg(RSCRATCHADDR, &VU0.nextBlockCycles);
		armAsm->Ldr(RXARG3, a64::MemOperand(RSCRATCHADDR));
		armAsm->Sub(rax, rax, RXARG3);
	}

	a64::Label skip;
	armAsm->Cmp(rax, 4);
	armAsm->B(&skip, a64::lt); // < 4 cycles behind: don't bother running a block
	armMoveAddressToReg(RXARG1, CpuVU0);
	armAsm->Mov(RWARG2, s_nBlockInterlocked ? 1 : 0);
	armEmitCall(reinterpret_cast<const void*>(&BaseVUmicroCPU::ExecuteBlockJIT));
	armAsm->Bind(&skip);
	armAsm->Bind(&skipvuidle);
}

// COP2_Interlock: the cpuRegs.code & 1 interlocked path. For an interlocked op that the
// M1 MicroFinish pass flagged as needing sync (EEINST_COP2_SYNC_VU0), commit cycles and
// either run-to-catch-up + _vu0WaitMicro (M-bit sync) or _vu0FinishMicro.
static void COP2_Interlock(bool mBitSync, u32 raw)
{
	// vc112: the interpreter does vu0Sync() at the TOP of every COP2 transfer op (VU0.cpp
	// QMFC2/CFC2/QMTC2/CTC2) — with a LIVE cpuRegs.cycle — catching VU0 up to the EE clock
	// BEFORE any force-finish. The native path skipped this, so its force-finish ran VU0 from a
	// stale, far-behind position and broke the VU0 handshake (Ratchet freeze). vc111 added the
	// leading vu0Sync but with a STALE clock (block cycles not yet committed), so it only
	// PARTIALLY caught VU0 up → VF reads returned stale data (model flicker / stuck transform).
	// Fix: commit the block's cycles FIRST so vu0Sync sees the live clock and fully syncs — the
	// interpreter's exact order. This is the sole cycle commit for transfer ops now (the handlers
	// pass 0 to their downstream mVUSyncVU0 so there is no double-commit).
	recEmitCommitBlockCycles(raw);
	armEmitCall(reinterpret_cast<const void*>(::vu0Sync)); // both reads+writes (freeze lives on writes)

	if (!(cpuRegs.code & 1))
		return;

	s_nBlockInterlocked = true;

	// We can safely skip the sync when nothing between CFC2/CTC2/COP2 ops can kick VU0.
	if (!(g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0))
		return;

	const a64::Register rax = RXVIXLSCRATCH; // x16

	armAsm->Ldr(rax, a64::MemOperand(RESTATEPTR, EE_CYCLE_OFFSET)); // already committed above (vc112)

	armMoveAddressToReg(RSCRATCHADDR, &VU0.VI[REG_VPU_STAT].UL);
	armAsm->Ldr(RWARG3, a64::MemOperand(RSCRATCHADDR));
	a64::Label skipvuidle;
	armAsm->Tbz(RWARG3, 0, &skipvuidle);

	if (mBitSync)
	{
		armMoveAddressToReg(RSCRATCHADDR, &VU0.cycle);
		armAsm->Ldr(RXARG3, a64::MemOperand(RSCRATCHADDR));
		armAsm->Sub(rax, rax, RXARG3);

		// Ratchet (and maybe others) flicker polygons under lazy COP2 sync unless the
		// micro resumption isn't deferred an extra EE block — hence the extra subtract.
		if (EmuConfig.Gamefixes.VUSyncHack || EmuConfig.Gamefixes.FullVU0SyncHack)
		{
			armMoveAddressToReg(RSCRATCHADDR, &VU0.nextBlockCycles);
			armAsm->Ldr(RXARG3, a64::MemOperand(RSCRATCHADDR));
			armAsm->Sub(rax, rax, RXARG3);
		}

		a64::Label skip;
		armAsm->Cmp(rax, 4);
		armAsm->B(&skip, a64::lt);
		armMoveAddressToReg(RXARG1, CpuVU0);
		armAsm->Mov(RWARG2, s_nBlockInterlocked ? 1 : 0);
		armEmitCall(reinterpret_cast<const void*>(&BaseVUmicroCPU::ExecuteBlockJIT));
		armAsm->Bind(&skip);

		armEmitCall(reinterpret_cast<const void*>(::_vu0WaitMicro));
	}
	else
	{
		armEmitCall(reinterpret_cast<const void*>(_vu0FinishMicro));
	}
	armAsm->Bind(&skipvuidle);
}

// --------------------------------------------------------------------------------------
//  Macro mode (Phase 7.9 / M3) — native COP2 transfer ops (faithful, memory-backed)
// --------------------------------------------------------------------------------------
// Faithful ports of microVU_Macro.inl's recCFC2/recCTC2/recQMFC2/recQMTC2, with the x86
// register-allocator calls (_allocX86reg/_allocVFtoXMMreg/_checkXMMreg/_eeMoveGPRtoR…)
// replaced by direct, non-caching memory access: the emit loop has already flushed the EE
// GPR cache to memory before recTranslateOp runs (recTranslateOpOptimized: recCacheFlushAll),
// and recCacheApplyNativeEffects/recConstApplyNativeEffects kill the whole cache after a 0x12
// op, so reading/writing cpuRegs.GPR and VU0.VI straight from memory is correct. They read
// the *host-side* cpuRegs.code via the _Rt_/_Rd_ macros (and cpuRegs.code & 1 for interlock),
// so the recTranslateOp dispatch must `cpuRegs.code = op` before calling.
//
// Cycle accounting (faithful to x86): the emit loop does NOT pre-commit cpuRegs.cycle. Instead
// it stashes the block's accumulated raw cycles in s_cop2RawCycles and these handlers pass it to
// the M2 sync helpers, which commit it to cpuRegs.cycle (recScaleBlockCycles, x86's
// scaleblockcycles_clear) only on a real SYNC — inside mVUSyncVU0 / the COP2_Interlock SYNC
// branch — and the emit loop clears its accumulator only then. mVUFinishVU0 (and any op that
// doesn't SYNC) commits nothing, so the accumulated cycles ride forward to the next sync / block
// tail. This is essential: _vu0FinishMicro overwrites cpuRegs.cycle with VU0.cycle (VU0.cpp), so
// pre-committing before a finish (as an earlier unconditional pre-flush did) silently lost those
// cycles; x86 keeps them uncommitted in s_nBlockCycles for exactly this reason.

// recCFC2: VU0 control reg (VI[rd]) -> GPR[rt], with the interlock / lazy-sync prologue and
// the per-register sign/zero-extend the interpreter uses (CFC2 in VU0.cpp).
static void recCFC2()
{
	COP2_Interlock(false, s_cop2RawCycles);

	if (!_Rt_)
		return;

	if (!(cpuRegs.code & 1))
	{
		if (g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0)
			mVUSyncVU0(0); // vc112: cycles already committed in COP2_Interlock — no double-commit
		else if (g_pCurInstInfo->info & EEINST_COP2_FINISH_VU0)
			mVUFinishVU0();
	}

	const u32 rt = _Rt_;
	const u32 rd = _Rd_;
	const a64::Register val = RXVIXLSCRATCH; // x16 — dead after the sync calls above

	if (rd == 0)
	{
		// why would you read vi00? -> 0
		armAsm->Mov(val, 0);
	}
	else if (rd == REG_I)
	{
		// sign-extend the 32-bit VI[REG_I] into the 64-bit GPR
		armMoveAddressToReg(RSCRATCHADDR, &VU0.VI[REG_I].UL);
		armAsm->Ldr(val.W(), a64::MemOperand(RSCRATCHADDR));
		armAsm->Sxtw(val, val.W());
	}
	else if (rd == REG_R)
	{
		armMoveAddressToReg(RSCRATCHADDR, &VU0.VI[REG_R].UL);
		armAsm->Ldr(val.W(), a64::MemOperand(RSCRATCHADDR));
		armAsm->Sxtw(val, val.W());
		armAsm->And(val, val, 0x7FFFFF);
	}
	else if (rd >= REG_STATUS_FLAG) // FixMe (x86): should R-Reg have upper 9 bits 0?
	{
		armMoveAddressToReg(RSCRATCHADDR, &VU0.VI[rd].UL);
		armAsm->Ldr(val.W(), a64::MemOperand(RSCRATCHADDR));
		armAsm->Sxtw(val, val.W());
	}
	else
	{
		// zero-extend the low 16 bits of VI[rd] (Ldrh zero-extends to W, W-write clears the
		// upper 32 of the X reg -> full 64-bit zero-extend)
		armMoveAddressToReg(RSCRATCHADDR, &VU0.VI[rd].UL);
		armAsm->Ldrh(val.W(), a64::MemOperand(RSCRATCHADDR));
	}

	armAsm->Str(val, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
}

// recCTC2: GPR[rt] -> VU0 control reg (VI[rd]), with the interlock(mBitSync=1)/lazy-sync
// prologue and the per-register write semantics from microVU_Macro.inl:recCTC2 (NOT the
// interpreter CTC2 — macro mode's REG_STATUS path also broadcasts the denormalized sticky
// status flag into VU0.micro_statusflags, which microVU0 reads). Memory-backed: the x86
// register-allocator (eax/_eeMoveGPRtoR/_allocVFtoXMMreg) becomes direct GPR<->VI loads/
// stores. _Rd_ is a compile-time constant, so only one switch arm is ever emitted.
static void recCTC2()
{
	COP2_Interlock(true, s_cop2RawCycles);

	if (!_Rd_)
		return;

	if (!(cpuRegs.code & 1))
	{
		if (g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0)
			mVUSyncVU0(0); // vc112: cycles already committed in COP2_Interlock — no double-commit
		else if (g_pCurInstInfo->info & EEINST_COP2_FINISH_VU0)
			mVUFinishVU0();
	}

	const u32 rt = _Rt_;
	const u32 rd = _Rd_;

	switch (rd)
	{
		case REG_MAC_FLAG:
		case REG_TPC:
		case REG_VPU_STAT:
			break; // read-only regs

		case REG_R:
			// VI[R] = (GPR[rt] & 0x7FFFFF) | 0x3F800000
			armAsm->Ldr(RWARG1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
			armAsm->And(RWARG1, RWARG1, 0x7FFFFF);
			armAsm->Orr(RWARG1, RWARG1, 0x3F800000);
			armMoveAddressToReg(RSCRATCHADDR, &VU0.VI[REG_R].UL);
			armAsm->Str(RWARG1, a64::MemOperand(RSCRATCHADDR));
			break;

		case REG_STATUS_FLAG:
		{
			// VI[STATUS] = (VI[STATUS] & 0x3F) | (rt ? (GPR[rt] & 0xFC0) : 0)
			armMoveAddressToReg(RSCRATCHADDR, &VU0.VI[REG_STATUS_FLAG].UL);
			armAsm->Ldr(RWARG2, a64::MemOperand(RSCRATCHADDR));
			armAsm->And(RWARG2, RWARG2, 0x3F);
			if (rt)
			{
				armAsm->Ldr(RWARG1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
				armAsm->And(RWARG1, RWARG1, 0xFC0);
				armAsm->Orr(RWARG2, RWARG2, RWARG1);
			}
			armAsm->Str(RWARG2, a64::MemOperand(RSCRATCHADDR));

			// Update microVU's sticky status flags: denormalize VI[STATUS] and broadcast it
			// across all 4 lanes of VU0.micro_statusflags. Inline port of mVUallocSFLAGd
			// (aVU_Alloc.inl) — pure bit-math, no microVU reg-alloc — into reg=w0,tmp1=w1,tmp2=w2.
			armMoveAddressToReg(RSCRATCHADDR, &VU0.VI[REG_STATUS_FLAG].UL);
			armAsm->Ldr(RWARG3, a64::MemOperand(RSCRATCHADDR)); // tmp2 = *memAddr
			armAsm->Mov(RWARG1, RWARG3);                        // reg
			armAsm->Lsr(RWARG1, RWARG1, 3);
			armAsm->And(RWARG1, RWARG1, 0x18);
			armAsm->Mov(RWARG2, RWARG3);                        // tmp1
			armAsm->Lsl(RWARG2, RWARG2, 11);
			armAsm->And(RWARG2, RWARG2, 0x1800);
			armAsm->Orr(RWARG1, RWARG1, RWARG2);
			armAsm->Lsl(RWARG3, RWARG3, 14);
			armAsm->And(RWARG3, RWARG3, 0x3cf0000);
			armAsm->Orr(RWARG1, RWARG1, RWARG3);

			armMoveAddressToReg(RSCRATCHADDR, &VU0.micro_statusflags[0]);
			armAsm->Dup(RQSCRATCH.V4S(), RWARG1);
			armAsm->Str(RQSCRATCH, a64::MemOperand(RSCRATCHADDR));
			break;
		}

		case REG_CMSAR1: // Execute VU1 Micro SubRoutine
			armAsm->Mov(RWARG1, 1);
			armEmitCall(reinterpret_cast<const void*>(vu1Finish));
			armAsm->Ldr(RWARG1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
			armEmitCall(reinterpret_cast<const void*>(vu1ExecMicro));
			break;

		case REG_FBRST:
		{
			if (!rt)
			{
				armMoveAddressToReg(RSCRATCHADDR, &VU0.VI[REG_FBRST].UL);
				armAsm->Str(a64::wzr, a64::MemOperand(RSCRATCHADDR));
				return;
			}

			// TEST_FBRST_RESET: GPR[rt] is stable in memory across the reset calls, so reload it
			// each time instead of pinning a callee-saved reg (x86 allocs MODE_CALLEESAVED).
			a64::Label skip0;
			armAsm->Ldr(RWVIXLSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
			armAsm->Tst(RWVIXLSCRATCH, 0x002); // VU0 Reset
			armAsm->B(&skip0, a64::eq);
			armEmitCall(reinterpret_cast<const void*>(vu0ResetRegs));
			armAsm->Bind(&skip0);

			a64::Label skip1;
			armAsm->Ldr(RWVIXLSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
			armAsm->Tst(RWVIXLSCRATCH, 0x200); // VU1 Reset
			armAsm->B(&skip1, a64::eq);
			armEmitCall(reinterpret_cast<const void*>(vu1ResetRegs));
			armAsm->Bind(&skip1);

			armAsm->Ldr(RWARG1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
			armAsm->And(RWARG1, RWARG1, 0x0C0C);
			armMoveAddressToReg(RSCRATCHADDR, &VU0.VI[REG_FBRST].UL);
			armAsm->Str(RWARG1, a64::MemOperand(RSCRATCHADDR));
			break;
		}

		case 0:
			break; // ignore writes to vi00

		default:
			// VI 1..15 are 16-bit (write US[0]); VI >= REG_STATUS_FLAG (incl. REG_I, whose
			// x86 FPR mirror at VF#33 == &VU0.VI[REG_I].F collapses to this memory store with
			// no VF cache) take the full 32-bit write.
			armMoveAddressToReg(RSCRATCHADDR, &VU0.VI[rd].UL);
			if (rd < REG_STATUS_FLAG)
			{
				armAsm->Ldrh(RWARG1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
				armAsm->Strh(RWARG1, a64::MemOperand(RSCRATCHADDR));
			}
			else
			{
				armAsm->Ldr(RWARG1, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(rt)));
				armAsm->Str(RWARG1, a64::MemOperand(RSCRATCHADDR));
			}
			break;
	}
}

// recQMFC2: VF[rd] (128-bit) -> GPR[rt] (128-bit). Interlock(false)/lazy-sync prologue, then a
// straight quad copy via RQSCRATCH. x86's vf00 cache special-case is moot memory-backed (no VF
// cache); reading VF[0] from memory is the real vf00.
static void recQMFC2()
{
	COP2_Interlock(false, s_cop2RawCycles);

	if (!_Rt_)
		return;

	if (!(cpuRegs.code & 1))
	{
		if (g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0)
			mVUSyncVU0(0); // vc112: cycles already committed in COP2_Interlock — no double-commit
		else if (g_pCurInstInfo->info & EEINST_COP2_FINISH_VU0)
			mVUFinishVU0();
	}

	armMoveAddressToReg(RSCRATCHADDR, &VU0.VF[_Rd_]);
	armAsm->Ldr(RQSCRATCH, a64::MemOperand(RSCRATCHADDR));
	armAsm->Str(RQSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(_Rt_)));
}

// recQMTC2: GPR[rt] (128-bit) -> VF[rd] (128-bit). Interlock(true)/lazy-sync prologue; vf00 is
// not writable (early-out), and rt==0 zeroes the destination.
static void recQMTC2()
{
	COP2_Interlock(true, s_cop2RawCycles);

	if (!_Rd_)
		return; // can't write vf00

	if (!(cpuRegs.code & 1))
	{
		if (g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0)
			mVUSyncVU0(0); // vc112: cycles already committed in COP2_Interlock — no double-commit
		else if (g_pCurInstInfo->info & EEINST_COP2_FINISH_VU0)
			mVUFinishVU0();
	}

	armMoveAddressToReg(RSCRATCHADDR, &VU0.VF[_Rd_]);
	if (_Rt_)
		armAsm->Ldr(RQSCRATCH, a64::MemOperand(RESTATEPTR, EE_GPR_OFFSET(_Rt_)));
	else
		armAsm->Movi(RQSCRATCH.V4S(), 0);
	armAsm->Str(RQSCRATCH, a64::MemOperand(RSCRATCHADDR));
}

// recLQC2: memory[GPR[rs] + imm] (128-bit, 16-byte aligned) -> VF[rt]. Unlike the COP2
// transfer ops above there is NO COP2_Interlock (faithful to microVU_Macro.inl:recLQC2,
// which only does the analysis-driven SYNC/FINISH dispatch); the quad load reuses the
// non-cached vtlb quad path (armEmitVtlbReadQuad), the same slow path armEmitLoadQuad uses.
// Memory-backed: the EE GPR cache is flushed before recTranslateOp and killed after, so the
// effective address reads GPR[rs] straight from cpuRegs. LQC2 to vf00 (!_Rt_) discards.
static void recLQC2()
{
	if (g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0)
		mVUSyncVU0(s_cop2RawCycles);
	else if (g_pCurInstInfo->info & EEINST_COP2_FINISH_VU0)
		mVUFinishVU0();

	// Effective address into the read helper's first argument register, 16-byte aligned
	// (the EE silently aligns 128-bit accesses; matches x86 recLQC2's xAND(arg1regd, ~0xF)).
	armEmitEffectiveAddr(RWARG1, _Rs_, _Imm_);
	armAsm->And(RWARG1, RWARG1, ~0x0F);

	// Perform the read even when discarding (vf00) — the access can have I/O side effects.
	// The call inside ReadQuad clobbers v0-v7/v16-v31, so the Mov to RQSCRATCH is after it.
	armEmitVtlbReadQuad(RQSCRATCH, RWARG1);

	if (!_Rt_)
		return; // loading to vf00 -> toss away

	armMoveAddressToReg(RSCRATCHADDR, &VU0.VF[_Rt_]);
	armAsm->Str(RQSCRATCH, a64::MemOperand(RSCRATCHADDR));
}

// recSQC2: VF[rt] (128-bit) -> memory[GPR[rs] + imm] (16-byte aligned). No COP2_Interlock
// (faithful to microVU_Macro.inl:recSQC2 — SYNC/FINISH dispatch only). vf00 stores VU0.VF[0]
// (memory-backed: no microVU VF cache to special-case). Reuses the non-cached vtlb quad path.
static void recSQC2()
{
	if (g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0)
		mVUSyncVU0(s_cop2RawCycles);
	else if (g_pCurInstInfo->info & EEINST_COP2_FINISH_VU0)
		mVUFinishVU0();

	// Load VF[rt] (vf00 reads VU0.VF[0]) into the quad scratch before computing the address;
	// WriteQuad moves it to q0 before its call, so it only needs to live until then.
	armMoveAddressToReg(RSCRATCHADDR, &VU0.VF[_Rt_]);
	armAsm->Ldr(RQSCRATCH, a64::MemOperand(RSCRATCHADDR));

	// Effective address into the write helper's first argument register, 16-byte aligned.
	armEmitEffectiveAddr(RWARG1, _Rs_, _Imm_);
	armAsm->And(RWARG1, RWARG1, ~0x0F);

	armEmitVtlbWriteQuad(RWARG1, RQSCRATCH);
}

// Install a freshly-compiled block's self-modifying-code protection and return the pointer
// to record in its recLUT slot. Direct port of x86 memory_protect_recompiled_code
// (iR5900.cpp), adapted to this port's body-first layout: the caller has already emitted
// the block body + dispatch tail (entry `body_entry`); for a manually-protected page we
// emit a checksum prologue AFTER the body and make THAT the block entry (it verifies the
// guest code and branches into the body).
//
// The whole point: on Apple Silicon the host page is 16 KB (4 KB on x86), so a single data
// write — e.g. an FMV frame the IPU/EE streams into RAM — can sit on the same page as
// compiled code and fault it. Tier 1 (Write) re-protects read-only and recompiles on every
// such write, which thrashes. Once a page has faulted it becomes Manual: we stop
// re-protecting it and instead self-check the code bytes on each block entry, so pure data
// writes no longer fault or invalidate. Blocks are kept within a single host page (see the
// page-boundary stop in recRecompile) so one page's mode governs the whole block.
//
// Assumes `body_entry` is the start of a real compiled block (not an interpreter
// single-step block — those re-read guest memory every run and need no protection).
static u8* recEmitManualProtection(u32 startpc, u32 endpc, u8* body_entry)
{
	const u32 size_bytes = endpc - startpc;
	const u32 size_words = size_bytes >> 2;

	// The kernel/EENULL thread-context pages alias one physical page across many virtual
	// mappings; always treat them as manual (matches x86).
	const bool contains_thread_stack = ((startpc >> 12) == 0x81) || ((startpc >> 12) == 0x80001);
	const vtlb_ProtectionMode mode = contains_thread_stack ? ProtMode_Manual : mmap_GetRamPageInfo(startpc);

	// Index into manual_page/counter by host RAM page, matching the vtlb's m_PageProtectInfo.
	const u32 rampage = static_cast<u32>(
		(reinterpret_cast<uptr>(PSM(startpc)) - reinterpret_cast<uptr>(eeMem->Main)) >> __pageshift);

	switch (mode)
	{
		case ProtMode_NotRequired:
			// ROM / unbacked — never written, nothing to protect.
			return body_entry;

		case ProtMode_None:
		case ProtMode_Write:
			// Cheap tier: write-protect the page so a future write faults and clears us.
			mmap_MarkCountedRamPage(startpc);
			manual_page[rampage] = 0;
			return body_entry;

		case ProtMode_Manual:
		default:
			break;
	}

	// Manual tier: emit the runtime self-check prologue. It becomes the block's entry.
	u8* const prologue = armGetCurrentCodePointer();

	// Args for the discard / page-reset helpers, kept live across the checks below
	// (the checks only touch x9/w10/w11).
	armAsm->Mov(RWARG1, startpc);     // x0 = startpc (guest vaddr)
	armAsm->Mov(RWARG2, size_bytes);  // x1 = block size in bytes

	// Compare every compiled guest word against the value captured at compile time. A
	// mismatch means the code itself changed (real SMC / module reload) -> discard.
	const u8* const base = static_cast<const u8*>(PSM(startpc));
	armMoveAddressToReg(a64::x9, base);
	for (u32 i = 0; i < size_words; i++)
	{
		const u32 captured = *reinterpret_cast<const u32*>(base + i * 4);
		armAsm->Ldr(a64::w10, a64::MemOperand(a64::x9, i * 4));
		armAsm->Mov(a64::w11, captured);
		armAsm->Cmp(a64::w10, a64::w11);
		armEmitCondBranch(a64::ne, DispatchBlockDiscard);
	}

	// Counted heuristic: a Manual block that runs a lot periodically retries cheap
	// write-protection (in case the write that demoted the page was a one-off). After the
	// page has been retried enough times (manual_counter > 3) it stays Manual permanently.
	if (!contains_thread_stack && manual_counter[rampage] <= 3)
	{
		armMoveAddressToReg(a64::x9, &manual_page[rampage]);
		armAsm->Ldrh(a64::w10, a64::MemOperand(a64::x9));
		armAsm->Add(a64::w10, a64::w10, size_words);
		armAsm->Strh(a64::w10, a64::MemOperand(a64::x9)); // truncates to 16 bits, like x86 xADD ptr16
		armAsm->Tst(a64::w10, 0x10000);                   // carry out of the 16-bit accumulator
		armEmitCondBranch(a64::ne, DispatchPageReset);
	}

	armEmitJmp(body_entry);
	return prologue;
}

// --------------------------------------------------------------------------------------
//  Dispatcher stubs (Phase 4.4)
// --------------------------------------------------------------------------------------
// Entered when DispatcherReg looks up a guest PC whose 64 KB page has no recompiler
// slot array (scratchpad / hardware registers / TLB-mapped code we don't cover yet).
// Logs once and bails out of the rec via the exit longjmp, mirroring x86 recError(0).
static void recExitUnmapped()
{
	Console.Error("ARM64 EE rec: jump to unmapped recLUT page (PC=0x%08x)", cpuRegs.pc);
	eeRecExitRequested.store(true, std::memory_order_release);
	longjmp(s_jmp_buf, 1);
}

// Emit the four dispatcher stubs into one contiguous block at the head of the code
// cache. They reference each other by label (DispatcherEvent / JITCompile / Enter /
// Unmapped all fall through to DispatcherReg) and are recorded as raw entry pointers.
// Regenerated on every reset; because recLUT, recEventTest and recRecompile live at
// fixed addresses, regeneration is byte-identical at the same location — which is why
// recRecompile can reset the cache mid-compile and safely return into JITCompile.
static void recGenDispatchers()
{
	armSetAsmPtr(recPtr, recPtrEnd - recPtr, &s_const_pool);
	armStartBlock();

	a64::Label dispatcher_reg;

	// DispatcherReg: fnptr = *(uptr*)(recLUT[pc>>16] + pc*2); br fnptr.
	//
	// Re-pin RESTATEPTR (x19) = &cpuRegs on every dispatch. Although EnterRecompiledCode
	// establishes it once, the C++ callees we re-enter through (recEventTest ->
	// _cpuEventTest_Shared in particular, which services DMA/VIF and runs other ARM64 JIT)
	// do NOT preserve x19 across the call — so by the time control returns to the
	// dispatcher it can hold garbage. Reloading it here (the single point every block,
	// event-test and compile path funnels back through) keeps it authoritative cheaply,
	// instead of relying on every external callee honouring the reservation.
	DispatcherReg = armGetCurrentCodePointer();
	armAsm->Bind(&dispatcher_reg);
	armMoveAddressToReg(RESTATEPTR, &cpuRegs);
	armLoadPtr(REVTLBPTR, &vtlb_private::vtlbdata.vmap);
	if (CHECK_FASTMEM)
		armLoadPtr(RFASTMEMBASE, &vtlb_private::vtlbdata.fastmem_base); // x28 = host-MMU fastmem base
	armAsm->Ldr(RWARG1, a64::MemOperand(RESTATEPTR, EE_PC_OFFSET));    // x0 = pc (zero-extended)
	armAsm->Lsr(RXARG2, RXARG1, 16);                                  // x1 = pc >> 16
	armMoveAddressToReg(RXARG3, recLUT);                              // x2 = &recLUT[0]
	armAsm->Ldr(RXARG3, a64::MemOperand(RXARG3, RXARG2, a64::LSL, 3)); // x2 = recLUT[page]
	armAsm->Add(RXARG3, RXARG3, a64::Operand(RXARG1, a64::LSL, 1));    // x2 = base + pc*2
	armAsm->Ldr(RXARG3, a64::MemOperand(RXARG3));                      // x2 = fnptr
	armAsm->Br(RXARG3);

	// DispatcherEvent: run the EE event test, then fall through to DispatcherReg (which
	// re-pins RESTATEPTR, since recEventTest clobbers it).
	DispatcherEvent = armGetCurrentCodePointer();
	armEmitCall(reinterpret_cast<const void*>(recEventTest));
	armAsm->B(&dispatcher_reg);

	// JITCompile: compile the block at cpuRegs.pc (which sets its recLUT slot), then
	// re-dispatch — the slot now points at the freshly compiled block.
	JITCompile = armGetCurrentCodePointer();
	armAsm->Ldr(RWARG1, a64::MemOperand(RESTATEPTR, EE_PC_OFFSET));
	armEmitCall(reinterpret_cast<const void*>(recRecompile));
	armAsm->B(&dispatcher_reg);

	// EnterRecompiledCode: the C entry point. Pin RESTATEPTR (x19) = &cpuRegs once,
	// then dispatch. We never return through here (exit is a longjmp out of
	// recEventTest), so callee-saved registers need no preserving — longjmp restores
	// recExecute's full context. Blocks therefore need no per-block prologue/epilogue.
	EnterRecompiledCode = armGetCurrentCodePointer();
	armMoveAddressToReg(RESTATEPTR, &cpuRegs);
	armLoadPtr(REVTLBPTR, &vtlb_private::vtlbdata.vmap);
	if (CHECK_FASTMEM)
		armLoadPtr(RFASTMEMBASE, &vtlb_private::vtlbdata.fastmem_base); // x28 = host-MMU fastmem base
	armAsm->B(&dispatcher_reg);

	// UnmappedRecLUTPage: target for every word of an unmapped guest page.
	UnmappedRecLUTPage = armGetCurrentCodePointer();
	armEmitCall(reinterpret_cast<const void*>(recExitUnmapped));
	armAsm->B(&dispatcher_reg);

	// DispatchBlockDiscard / DispatchPageReset: the tails of a manually-protected block's
	// entry checksum (see recEmitManualProtection). The checksum prologue has already loaded
	// x0 = startpc and x1 = block size (bytes) and branches here on failure; we run the C
	// helper, then re-dispatch (the slot now points back at JITCompile, so it recompiles).
	DispatchBlockDiscard = armGetCurrentCodePointer();
	armEmitCall(reinterpret_cast<const void*>(dyna_block_discard));
	armAsm->B(&dispatcher_reg);

	DispatchPageReset = armGetCurrentCodePointer();
	armEmitCall(reinterpret_cast<const void*>(dyna_page_reset));
	armAsm->B(&dispatcher_reg);

	recPtr = armEndBlock();
}

// Emit a block's tail: charge the block's scaled guest cycles, then the inline event
// test. Mirrors iR5900.cpp iBranchTest (dynamic-target form): if (s64)(cycle -
// nextEventCycle) < 0 there is no event due, so jump straight back into the dispatcher
// (DispatcherReg re-reads cpuRegs.pc and chains into the next block); otherwise fall
// to DispatcherEvent to service events first. `add_cycles` is false for interpreter
// single-step blocks, which charge their own cycles inside intExecuteOneInst.
// `waitloop_selfpc`: non-zero marks this block as a detected wait/idle loop with
// the given start PC. The tail then checks whether the branch was taken back to
// the loop start and, if so, bumps cpuRegs.cycle up to nextEventCycle so the next
// event fires after one iteration instead of the EE busy-spinning host-side until
// the event (the main EE-at-99%/heat case for polling loops).
static void recEmitEventTestAndDispatch(u32 scaled_cycles, bool add_cycles, bool known_dispatch_pc, u32 dispatch_pc,
	u32 waitloop_selfpc = 0)
{
	armAsm->Ldr(RXARG1, a64::MemOperand(RESTATEPTR, EE_CYCLE_OFFSET)); // x0 = cpuRegs.cycle (u64)
	if (add_cycles)
		armAsm->Add(RXARG1, RXARG1, scaled_cycles);
	armAsm->Ldr(RXARG2, a64::MemOperand(RESTATEPTR, EE_NEXTEVENTCYCLE_OFFSET));
	if (waitloop_selfpc != 0)
	{
		a64::Label no_bump;
		armAsm->Ldr(RWARG3, a64::MemOperand(RESTATEPTR, EE_PC_OFFSET));
		armAsm->Mov(RWARG4, waitloop_selfpc);
		armAsm->Cmp(RWARG3, RWARG4);
		armAsm->B(&no_bump, a64::ne); // branch not taken back to loop start: normal tail
		armAsm->Cmp(RXARG1, RXARG2);
		armAsm->Csel(RXARG1, RXARG2, RXARG1, a64::lt); // cycle = max(cycle, nextEventCycle)
		armAsm->Bind(&no_bump);
	}
	if (add_cycles || waitloop_selfpc != 0)
		armAsm->Str(RXARG1, a64::MemOperand(RESTATEPTR, EE_CYCLE_OFFSET));

	a64::Label no_forced_exit;
	armMoveAddressToReg(RXARG3, const_cast<u8*>(&eeRecExitSignal));
	armAsm->Ldrb(RWARG3, a64::MemOperand(RXARG3));
	armAsm->Cbz(RWARG3, &no_forced_exit);
	armEmitCall(reinterpret_cast<const void*>(recCheckExitAfterInterp));
	armAsm->Ldr(RXARG1, a64::MemOperand(RESTATEPTR, EE_CYCLE_OFFSET));
	armAsm->Ldr(RXARG2, a64::MemOperand(RESTATEPTR, EE_NEXTEVENTCYCLE_OFFSET));
	armAsm->Bind(&no_forced_exit);

	armAsm->Cmp(RXARG1, RXARG2);

	if (known_dispatch_pc)
	{
		armEmitCondBranch(a64::pl, DispatcherEvent); // event due => service before continuing
		if (s_eeBlockLinkEnabled)
			recEmitLinkableExitToKnownPc(dispatch_pc); // patchable direct B, staged for linking
		else
			recEmitDispatchToKnownPc(dispatch_pc); // LUT-indirect fallback
		return;
	}

	armEmitCondBranch(a64::mi, DispatcherReg); // N set => (cycle - nextEvent) < 0 => continue
	armEmitJmp(DispatcherEvent);
}

// --------------------------------------------------------------------------------------
//  EEINST inst-cache (Phase 7.9 M0.2 — macro-mode analysis substrate)
// --------------------------------------------------------------------------------------
// Per-block instruction-info array, mirroring the x86 rec's s_pInstCache. The M1 COP2
// analysis passes write the EEINST_COP2_* bits here per instruction, and the macro-mode
// emit (M2/M3) reads them off g_pCurInstInfo. Indexed by (pc - startpc) >> 2.
//
// Unlike x86 (whose blocks run unbounded until a branch, so it mallocs+grows the cache),
// ARM64 blocks are capped to MAX_BLOCK_INSTS guest ops and one host page, so a fixed
// array suffices: + a branch delay slot + the x86-style end sentinel.
static constexpr u32 EE_INST_CACHE_SIZE = MAX_BLOCK_INSTS + 4;
static EEINST s_instCache[EE_INST_CACHE_SIZE];
static u32 s_eeEndBlock = 0; // first pc past the current block (x86 s_nEndBlock equiv.)

// Forward pre-scan of the block range, mirroring the x86 rec's s_nEndBlock walk
// (ix86-32/iR5900.cpp:2292) but matching THIS rec's actual block boundaries so the
// EEINST indices line up with what the emit loop below compiles. It over-approximates
// safely: it ends only at a control-flow op (branch/jump + delay slot), a host-page
// boundary, or the instruction cap — exactly the emit loop's terminators *except* the
// "un-compilable op ends the block early" case, which only makes the real block shorter.
// So the scanned range is always >= the emitted range, keeping every g_pCurInstInfo
// index in bounds. (No compile is attempted here — it is pure opcode inspection.)
static u32 recScanBlockEnd(u32 startpc)
{
	u32 pc = startpc;
	u32 count = 0;
	for (;;)
	{
		// Host-page boundary — same single-page-per-block rule as the emit loop.
		if (pc != startpc && (pc & ~__pagemask) != (startpc & ~__pagemask))
			break;

		if (count >= MAX_BLOCK_INSTS)
			break;

		const u32 op = memRead32(pc);
		count++;

		// Branch / branch-likely: the block ends after the delay slot (pc += 8), exactly
		// as the emit loop terminates. (A J/JAL/JR/JALR/Bcc or a likely Bccl.)
		if (recIsHandledBranch(op) || recIsLikelyBranch(op))
		{
			pc += 8;
			break;
		}

		pc += 4;
	}
	return pc;
}

// Skip MPEG game-fix (CHECK_SKIPMPEGHACK) — ported from the x86 rec's
// skipMPEG_By_Pattern (ix86-32/iR5900.cpp). It was previously x86-only, so the
// "Skip MPEG" toggle did nothing on Android (this ARM64 EE backend). The PS2
// sceMpegIsEnd routine is a tiny 3-instruction leaf:
//     lw reg, 0x40(a0) ; jr ra ; lw v0, 0(reg)
// When the fix is on and a block starts exactly on that pattern, we don't
// recompile it — we stub it to force v0 = 1 ("movie finished") and return to ra,
// so games that spin waiting for an FMV to end skip it (Katamari et al.). The
// pattern detection is architecture-independent; the host state change is done by
// this C helper (called via armEmitCall) so no hand-emitted field writes are
// needed, and the normal block finalize then event-tests + dispatches from the
// cpuRegs.pc the helper set.
static void eeSkipMpegIsEnd()
{
	cpuRegs.GPR.n.v0.UD[0] = 1;          // v0 = 1 (UL[0] = 1, UL[1] = 0)
	cpuRegs.pc = cpuRegs.GPR.n.ra.UL[0]; // jr ra
}

// Returns true (and emits the stub call) when [startpc] is the sceMpegIsEnd leaf
// and the fix is enabled. s_eeEndBlock must already be set (recScanBlockEnd).
static bool recTrySkipMpeg(u32 startpc)
{
	if (!CHECK_SKIPMPEGHACK)
		return false;

	// Exactly three words, middle op == `jr ra` (0x03e00008).
	if (s_eeEndBlock != startpc + 12 || memRead32(startpc + 4) != 0x03e00008u)
		return false;

	const u32 code = memRead32(startpc);
	const u32 p1 = 0x8c800040u;                             // lw ?, 0x40(a0)
	const u32 p2 = 0x8c020000u | ((code & 0x1f0000u) << 5); // lw v0, 0(reg)
	if ((code & 0xffe0ffffu) != p1)
		return false;
	if (memRead32(startpc + 8) != p2)
		return false;

	armEmitCall(reinterpret_cast<const void*>(eeSkipMpegIsEnd));
	Console.WriteLn("sceMpegIsEnd pattern found! Recompiling skip video fix... [ARM64]");
	return true;
}

// --------------------------------------------------------------------------------------
//  Block compiler (Phase 4.3 / 4.4)
// --------------------------------------------------------------------------------------
// Compile a straight-line run starting at startpc into one host block and install its
// entry into the recLUT slot for startpc. Unlike the Phase 4.3 version this block has
// NO prologue/epilogue and never RETs — RESTATEPTR is pinned once by EnterRecompiledCode
// and the block ends by branching into the dispatcher (via recEmitEventTestAndDispatch).
// The run:
//   - emits straight-line ops we can codegen inline;
//   - stops at the first control-flow op we have a generator for (branch + delay slot),
//     the branch generator having written cpuRegs.pc;
//   - if the *first* op is one we can't codegen, emits a one-shot block that single-steps
//     it through the interpreter (intExecuteOneInst handles its own PC/delay/cycles);
//   - otherwise ends at the next un-compilable op (or the length cap), writing cpuRegs.pc
//     so the next dispatch resumes there.
// Thunk carving for fastmem backpatch (@@MAC_FASTMEM_BACKPATCH@@) — ported from the stock
// arm64 backend (arm64/aR5900.cpp). Carves a scratch code region from the EE code buffer
// with NO const pool, so armEmitJmp/armEmitCall inside the thunk inline the target through
// x16 rather than routing via a trampoline (x16 is scratch, clobbered by the call anyway).
u8* recBeginThunk()
{
	if (recPtr >= recPtrEnd)
		eeRecNeedsReset = true;
	armSetAsmPtr(recPtr, recPtrEnd - recPtr, nullptr);
	recPtr = armStartBlock();
	return recPtr;
}

u8* recEndThunk()
{
	u8* block_end = armEndBlock();
	pxAssert(block_end < recPtrEnd);
	recPtr = block_end;
	return block_end;
}

static void recRecompile(u32 startpc)
{
	const u32 hw_startpc = recHWAddr(startpc);

	// Reset the whole cache if the emit cursor has run within one block's worth of the
	// constant-pool tail. Doing it here (before emitting) is safe: the dispatcher stubs
	// regenerate byte-identically at the same addresses, so the JITCompile stub this
	// call returns into is unchanged. Mirrors x86 recRecompile.
	if (recPtr >= recPtrEnd - RECOMPILE_HEADROOM)
		eeRecNeedsReset = true;

	if (hw_startpc == VMManager::Internal::GetCurrentELFEntryPoint())
	{
		VMManager::Internal::EntryPointCompilingOnCPUThread();
	}

	if (eeRecNeedsReset)
		recResetRaw();

	// Each block starts with no staged link exit; only the known-target tail sets one.
	s_eeLinkStaged = false;

	armSetAsmPtr(recPtr, recPtrEnd - recPtr, &s_const_pool);
	u8* const entry = armStartBlock();

	if (hw_startpc == EELOAD_START)
	{
		const u32 mainjump = memRead32(EELOAD_START + 0x9c);
		if (mainjump >> 26 == 3) // JAL
			g_eeloadMain = ((EELOAD_START + 0xa0) & 0xf0000000U) | ((mainjump << 2) & 0x0fffffffu);
	}

	if (g_eeloadMain && hw_startpc == recHWAddr(g_eeloadMain))
	{
		armEmitCall(reinterpret_cast<const void*>(eeloadHook));
		if (VMManager::Internal::IsFastBootInProgress())
		{
			const u32 typeAexecjump = memRead32(EELOAD_START + 0x470);
			const u32 typeBexecjump = memRead32(EELOAD_START + 0x5b0);
			const u32 typeCexecjump = memRead32(EELOAD_START + 0x618);
			const u32 typeDexecjump = memRead32(EELOAD_START + 0x600);
			if ((typeBexecjump >> 26 == 3) || (typeCexecjump >> 26 == 3) || (typeDexecjump >> 26 == 3))
				g_eeloadExec = EELOAD_START + 0x2b8;
			else if (typeAexecjump >> 26 == 3)
				g_eeloadExec = EELOAD_START + 0x170;
			else
				Console.WriteLn("recRecompile: Could not enable launch arguments for fast boot mode; unidentified BIOS version! Please report this to the PCSX2 developers.");
		}
	}

	if (g_eeloadExec && hw_startpc == recHWAddr(g_eeloadExec))
	{
		armEmitCall(reinterpret_cast<const void*>(eeloadHook2));
	}

	u32 pc = startpc;
	u32 endpc = startpc;
	u32 raw_cycles = 0;
	// EE memory-speed multiplier: when the COP0 Config.DIE (i-cache enable) bit is clear, every
	// EE instruction costs double cycles. Read at compile time, matching the x86 rec's per-op
	// accounting in recompileNextInstruction (iR5900.cpp). Without it cpuRegs.cycle advances at
	// half rate whenever DIE is clear, so the EE runs ahead of the GS/VU/IOP/VBlank schedule.
	const u32 ee_cycle_mult = 2 - ((cpuRegs.CP0.n.Config >> 18) & 0x1);
	// Per-op cycle cost incl. the x86 rec's NOP special-case (a real NOP is treated as ~9 cycles).
	const auto eeOpCycles = [ee_cycle_mult](u32 opc) -> u32 {
		return (opc == 0 ? 9u : static_cast<u32>(::R5900::GetInstruction(opc).cycles)) * ee_cycle_mult;
	};
	u32 compiled = 0;
	bool interp_step = false;
	bool known_dispatch_pc = false;
	u32 dispatch_pc = 0;
	u32 waitloop_selfpc = 0;
	u32 waitloop_ops[REC_WAITLOOP_MAX_OPS];
	u32 waitloop_num_ops = 0;
	bool waitloop_possible = true;
	RecGprConstState const_state;
	RecGprCacheState cache_state;

	// @@EEDIFF@@ Snapshot the diff-verify enable ONCE per block. When set, every op is
	// wrapped with a pre/post interpreter compare and the GPR/const cache is forced off
	// (memory stays authoritative between ops so the snapshot/compare is exact). The
	// toggle clears the whole EE block cache, so all blocks compiled while it is on carry
	// the hooks and all compiled while off are hook-free (zero overhead).
	const bool ee_diff = g_ee_diff_verify;

	// Macro mode (M2): reset the per-block "contains an interlocked COP2 op" flag. Set by
	// COP2_Interlock during emit, baked into the VU0 ExecuteBlockJIT `interlocked` arg.
	s_nBlockInterlocked = false;

	// Build the per-block EEINST inst-cache (Phase 7.9 M0.2). Pre-scan the block range
	// and clear one EEINST slot per instruction so the M1 COP2 analysis passes have a
	// place to write and the emit loop can expose a g_pCurInstInfo per op. No emit/
	// behavior change yet — the flags computed here are not consumed until M3.
	s_eeEndBlock = recScanBlockEnd(startpc);
	{
		u32 ninst = (s_eeEndBlock - startpc) >> 2;
		if (ninst >= EE_INST_CACHE_SIZE)
			ninst = EE_INST_CACHE_SIZE - 1; // can't happen (range is capped) — defensive
		std::memset(s_instCache, 0, sizeof(EEINST) * (ninst + 1)); // +1: end sentinel
	}

	// Phase 7.9 M1 — COP2 macro-mode analysis passes. Only worth running when the block
	// actually contains COP2 / LQC2 / SQC2 ops (mirrors the x86 rec's has_cop2_instructions
	// gate). The passes write the EEINST_COP2_* bits into s_instCache using the no-offset
	// convention (base = s_instCache, instruction at pc -> s_instCache[(pc-startpc)>>2]),
	// matching the per-op g_pCurInstInfo the emit loop hands out below. The flags are
	// computed-ready but NOT consumed yet (consumption starts in M3) — no behavior change.
	// Call order matches x86 (MicroFinish then, under vuFlagHack, FlagHack).
	{
		bool has_cop2 = false;
		for (u32 i = startpc; i < s_eeEndBlock; i += 4)
		{
			const u32 op26 = memRead32(i) >> 26;
			if (op26 == 022 || op26 == 066 || op26 == 076) // COP2 / LQC2 / SQC2
			{
				has_cop2 = true;
				break;
			}
		}
		if (has_cop2)
		{
			R5900::COP2MicroFinishPass().Run(startpc, s_eeEndBlock, s_instCache);
			if (EmuConfig.Speedhacks.vuFlagHack)
				R5900::COP2FlagHackPass().Run(startpc, s_eeEndBlock, s_instCache);

			eeDumpCOP2AnnotatedBlock(startpc, s_eeEndBlock, s_instCache); // M1.3 (env-gated)
		}
	}

	// Skip MPEG game-fix: if enabled and this block IS the sceMpegIsEnd leaf, stub it
	// (force v0=1 + jr ra) instead of recompiling, then fall straight into the normal
	// block finalize/dispatch below — which resumes at cpuRegs.pc (= ra) the stub set.
	const bool skipped_mpeg = recTrySkipMpeg(startpc);
	if (skipped_mpeg)
	{
		endpc = s_eeEndBlock; // the 3-word leaf [startpc, startpc+12)
		raw_cycles = eeOpCycles(memRead32(startpc)) + eeOpCycles(0x03e00008u) +
			eeOpCycles(memRead32(startpc + 8));
		// known_dispatch_pc stays false -> dispatch dynamically from cpuRegs.pc.
	}

	for (; !skipped_mpeg;)
	{
		// Keep every block within a single host RAM page so its SMC protection mode (see
		// recEmitManualProtection) governs the whole block, and so a page-fault clear of
		// the block's page always hits the block's start slot. (A branch's delay slot may
		// still spill one word into the next page — an accepted corner, as on x86.)
		if (pc != startpc && (pc & ~__pagemask) != (startpc & ~__pagemask))
		{
			recEmitWritePc(pc);
			known_dispatch_pc = true;
			dispatch_pc = pc;
			break;
		}

		const u32 op = memRead32(pc);

		// Point g_pCurInstInfo at this instruction's EEINST slot (M0.2). The pre-scan
		// guarantees the index is in bounds (its range >= the emitted range); clamp
		// defensively all the same. Consumed by the macro-mode COP2 emit from M3 on.
		{
			u32 idx = (pc - startpc) >> 2;
			if (idx >= EE_INST_CACHE_SIZE)
				idx = EE_INST_CACHE_SIZE - 1;
			g_pCurInstInfo = &s_instCache[idx];
		}

		// @@EEDIFF@@ Force the guest state committed to memory before EVERY op so the
		// diff verifier's snapshot/compare (and the interpreter re-run, which reads
		// cpuRegs) sees authoritative state. Writes back dirty cached GPRs, drops the
		// cache, and clears const tracking so no op's inputs live only in a host reg or
		// the compiler's const map. Cheap: only when the diagnostic is on.
		if (ee_diff)
		{
			recCacheFlushAll(cache_state);
			recCacheKillAll(cache_state);
			recConstKillAll(const_state);
		}

		// COP0 DI — the interrupt-disable must take effect one instruction LATE, exactly as
		// the x86 recDI (iCOP0.cpp): emit the *following* guest instruction first, then the
		// Status.EIE clear. Without this delay several games disable IRQs one op too early
		// and hang at boot (Jak X, Namco 50th Anniversary, SpongeBob the Movie / Battle for
		// Bikini Bottom, The Incredibles (+ Rise of the Underminer), Soukou Kihei Armodyne,
		// Garfield: Saving Arlene, Tales of Fandom Vol. 2). The delayed op is emitted
		// straight-line in program order (recEmitOp), then recEmitCop0DI; the pair advances
		// pc by 8. A DI in a branch delay slot never reaches here (delay slots go through
		// recEmitOp, where DI inline-interprets immediately — matching x86's
		// g_recompilingDelaySlot path). If the following op can't be safely spliced inline
		// (control-flow / PC-writing / exception / cycle-sensitive), fall through to end the
		// block at DI and single-step it (rare; DI then applies immediately).
		if (recIsCop0DI(op))
		{
			const u32 next_op = memRead32(pc + 4);
			if (!recIsHandledBranch(next_op) && !recIsLikelyBranch(next_op) &&
				!recCop0DelayOpUnsafe(next_op))
			{
				raw_cycles += eeOpCycles(op);

				// Compile the delayed instruction (point g_pCurInstInfo at its slot for any
				// analysis-driven emit), then apply DI after it has executed.
				{
					u32 nidx = ((pc + 4) - startpc) >> 2;
					if (nidx >= EE_INST_CACHE_SIZE)
						nidx = EE_INST_CACHE_SIZE - 1;
					g_pCurInstInfo = &s_instCache[nidx];
				}
				recEmitOp(next_op, const_state, cache_state, pc + 4);
				recEmitCop0DI();
				raw_cycles += eeOpCycles(next_op);

				// A block containing a DI is not a poll loop.
				waitloop_possible = false;
				pc += 8;
				endpc = pc;
				compiled += 2;
				if (compiled >= MAX_BLOCK_INSTS)
				{
					recEmitWritePc(pc);
					known_dispatch_pc = true;
					dispatch_pc = pc;
					break;
				}
				continue;
			}
			// else: fall through — recTranslateOpOptimized(DI) returns false below, so the
			// block ends here / single-steps DI (no cycles charged for DI on this path).
		}

		// MIPS trap ops (TGE/TLT/TEQ/TNE + immediate forms). Native, block-conditional:
		// emit a compare and, when the trap is NOT taken (the overwhelmingly common case),
		// branch over the raise block and STAY in-block — no block-terminate, no dispatch
		// round-trip (the regression that made these single-step every execution). On the
		// rare taken path we run the interpreter op (which raises via cpuException, setting
		// cpuRegs.pc to the exception vector), commit the block's cycles, and tail into the
		// dispatcher — mirroring x86's recBranchCall, but only for the taken path. We must
		// make memory authoritative first (the compare reads guest GPRs; the taken path's
		// interpreter reads cpuRegs), so flush + kill the GPR/const cache exactly as a
		// block-terminating branch does.
		if (recIsTrap(op))
		{
			raw_cycles += eeOpCycles(op);
			recCacheFlushAll(cache_state);
			recCacheKillAll(cache_state);
			recConstKillAll(const_state);

			a64::Label skip;
			recEmitTrapCompareIfTrap(op, &skip);     // compare + B(skip) when NOT taken
			recEmitWritePc(pc + 4);                  // trap() does pc-=4 -> EPC = trap pc
			recEmitInterpInline(op);                 // trap taken: raise -> cpuRegs.pc = vector
			recEmitCommitBlockCycles(raw_cycles);    // commit cycles incl. the trap
			armEmitJmp(DispatcherEvent);             // event test + re-dispatch from cpuRegs.pc
			armAsm->Bind(&skip);                     // NOT taken: fall through, stay in-block

			waitloop_possible = false;               // a block with a trap is not a poll loop
			pc += 4;
			endpc = pc;
			if (++compiled >= MAX_BLOCK_INSTS)
			{
				recEmitWritePc(pc);
				known_dispatch_pc = true;
				dispatch_pc = pc;
				break;
			}
			continue;
		}

		if (recIsHandledBranch(op))
		{
			// Terminate the block: branch generator + delay slot + dispatch tail.
			raw_cycles += eeOpCycles(op);
			known_dispatch_pc = recGetKnownBranchTarget(op, pc, const_state, &dispatch_pc);
			recCacheFlushAll(cache_state);
			recCacheKillAll(cache_state);
			recEmitBranch(op, pc); // writes cpuRegs.pc (taken/fallthrough/link)
			recConstApplyBranchLink(op, pc, const_state);

			const u32 delay_op = memRead32(pc + 4);
			raw_cycles += eeOpCycles(delay_op);
			recEmitOp(delay_op, const_state, cache_state, pc + 4); // delay slot — must not write cpuRegs.pc
			endpc = pc + 8;

			// Wait-loop detection: does this branch loop back to the block start with a
			// body that carries no register state between iterations? Non-linking forms
			// only (J/BEQ/BNE/BLEZ/BGTZ/BLTZ/BGEZ). Unconditional self-loops can only
			// exit via an event, so the skip is exact and always enabled; conditional
			// (polling) loops follow the WaitLoop speedhack toggle like the x86 rec.
			{
				const u32 opc = op >> 26;
				const u32 looptarget = (opc == 0x02) ?
					(((op & 0x03ffffff) << 2) | ((pc + 4) & 0xf0000000u)) :
					((pc + 4) + (static_cast<u32>(static_cast<s32>(static_cast<s16>(op))) << 2));
				const u32 cond_reads = recBranchConditionReads(op);
				const bool unconditional = recBranchIsUnconditional(op);

				if (looptarget == startpc && waitloop_possible && waitloop_num_ops == compiled &&
					cond_reads != 0xffffffffu && (unconditional || EmuConfig.Speedhacks.WaitLoop) &&
					recWaitLoopBodyIsPure(waitloop_ops, waitloop_num_ops, cond_reads, delay_op))
				{
					waitloop_selfpc = startpc;
				}
			}
			break;
		}

		if (recIsLikelyBranch(op))
		{
			// Branch-likely: delay slot executes ONLY when taken. Emit the condition
			// test + PC select, then jump over the delay-slot code when not taken.
			// The cache/const state diverges across the two paths, so it is flushed
			// and discarded inside the taken path before the skip label.
			raw_cycles += eeOpCycles(op);

			const u32 btarget = (pc + 4) + (static_cast<u32>(static_cast<s32>(static_cast<s16>(op))) << 2);
			const u32 fallthrough = pc + 8;

			recCacheFlushAll(cache_state);
			recCacheKillAll(cache_state);

			const a64::Condition taken = armEmitBranchLikelyTest(op, btarget, fallthrough);
			a64::Label skip_delay;
			armAsm->B(&skip_delay, a64::InvertCondition(taken));

			const u32 delay_op = memRead32(pc + 4);
			raw_cycles += eeOpCycles(delay_op);
			recEmitOp(delay_op, const_state, cache_state, pc + 4);
			recCacheFlushAll(cache_state);
			recCacheKillAll(cache_state);
			recConstKillAll(const_state);

			armAsm->Bind(&skip_delay);
			endpc = pc + 8;
			break;
		}

		// COP2 / VU0-macro ops: the cycle commit happens INSIDE the macro-mode sync helpers,
		// exactly where x86 does it (mVUSyncVU0 / the COP2_Interlock SYNC branch) and only for
		// ops that actually SYNC VU0. Stash the block's accumulated cycles (incl. this op's,
		// matching x86 order) for the handler to pass to the helper; clear the accumulator only
		// when a commit is emitted — iff the op syncs (EEINST_COP2_SYNC_VU0), which is the union
		// of both helpers' compile-time commit gate. FINISH-only / no-sync ops leave the cycles
		// in the accumulator so they ride forward (x86 keeps them in s_nBlockCycles), surviving
		// the _vu0FinishMicro cpuRegs.cycle = VU0.cycle collapse a pre-commit would have lost.
		const bool needs_cycle_flush = recOpNeedsCycleFlush(op);
		if (needs_cycle_flush)
		{
			raw_cycles += eeOpCycles(op);
			s_cop2RawCycles = raw_cycles;
			if (recCop2IsCallms(op) || recCop2ForceInterp(op))
			{
				// CALLMS/CALLMSR are x86's only INTERPRETATE_COP2_FUNC ops: they commit the
				// scaled block cycles to cpuRegs.cycle and clear the accumulator
				// (scaleblockcycles_clear) BEFORE the inline interpreter runs vu0ExecMicro,
				// which sets VU0.cycle = cpuRegs.cycle — so the launched VU0 microprogram sees
				// the committed EE time. The native FINISH macro ops correctly ride cycles
				// forward (mVUFinishVU0 commits nothing; _vu0FinishMicro collapses cpuRegs.cycle),
				// but a LAUNCH does not collapse it, so for these two ops the cycles must be
				// committed here. Emitted before recTranslateOpOptimized's cache flush + interp
				// call below, mirroring x86's order (commit, then recCall(V##f)).
				// vc105: force-interp COP2 ops (FullVU0SyncHack) are ALSO run via the inline
				// interpreter, whose vu0Sync/_vu0FinishMicro read cpuRegs.cycle — same live-clock
				// requirement as CALLMS, so they take this commit-then-inline path too.
				recEmitCommitBlockCycles(s_cop2RawCycles);
				raw_cycles = 0;
			}
			else if (g_pCurInstInfo->info & EEINST_COP2_SYNC_VU0)
				raw_cycles = 0;
		}

		// MFC0/MTC0 of Count(rd9)/PERF(rd25): commit the block's cycles (incl. this op) so
		// the read is live, clear the accumulator, then INLINE-interp in-block — instead of
		// the expensive single-step path (these were ~80% of Jackie Chan's EE fallbacks, a
		// Count busy-poll). Same commit-then-inline shape as the CALLMS launch above; the
		// per-op commit makes consecutive Count reads see an advancing cpuRegs.cycle (no
		// stale-value lock-up). See recCop0NeedsLiveCycle + the COP0 note in recTranslateOpOptimized.
		if (recCop0NeedsLiveCycle(op))
		{
			raw_cycles += eeOpCycles(op);
			recEmitCommitBlockCycles(raw_cycles);
			raw_cycles = 0;
			// Flush + kill the GPR register cache / const tracking before the inline interp,
			// exactly like the trap path (recCacheFlushAll/KillAll/ConstKillAll above): MTC0
			// reads cpuRegs.GPR.r[rt] from memory (must be authoritative) and MFC0 WRITES it —
			// a stale cached copy in a callee-saved host reg would survive the C call and shadow
			// the Count value, silently breaking the very poll loop this targets.
			recCacheFlushAll(cache_state);
			recCacheKillAll(cache_state);
			recConstKillAll(const_state);
			recEmitInterpInline(op);
			waitloop_possible = false; // inline live-cycle op — not a wait-loop body
			pc += 4;
			endpc = pc;
			if (++compiled >= MAX_BLOCK_INSTS)
			{
				recEmitWritePc(pc);
				known_dispatch_pc = true;
				dispatch_pc = pc;
				break;
			}
			continue;
		}

		// COP0 EI / ERET (CO ops, rs==0x10; funct 0x38 / 0x18). Faithful to x86
		// recEI/recERET (recBranchCall): run the interpreter handler in-block, then END
		// the block so the tail event-tests + dispatches from cpuRegs.pc. EI: a now-
		// unmasked pending IRQ gets serviced by that event test (x86 "must branch after
		// enabling interrupts"); ERET: it writes cpuRegs.pc, so the dispatch must read it.
		// Like the handled-branch path, the block's cycles ride in raw_cycles and the
		// post-loop commits them (x86's iBranchTest also commits AFTER the Interp call) —
		// no early commit, so no spurious tail +1. Replaces the per-op single-step (the
		// top EE interpreter fallback after the BC0/MADD work).
		if (recIsCop0EIorERET(op))
		{
			raw_cycles += eeOpCycles(op);
			recCacheFlushAll(cache_state);
			recCacheKillAll(cache_state);
			recConstKillAll(const_state);
			if (!recIsCop0ERET(op))
				recEmitWritePc(pc + 4); // EI doesn't write PC; resume after it unless the event test diverts
			recEmitInterpInline(op);    // Interp::EI sets Status.EIE / Interp::ERET sets cpuRegs.pc
			waitloop_possible = false;
			known_dispatch_pc = false;  // dispatch from cpuRegs.pc: an IRQ raised by the tail event test, or ERET's new pc
			endpc = pc + 4;
			break;
		}

		// @@EEDIFF@@ Diagnostic path: wrap the op with snapshot + interpreter re-run +
		// compare. Uses the raw recTranslateOp (memory-committed generators) because the
		// cache/const were just killed for this op. Wait-loop detection is disabled here
		// (waitloop_possible is forced false below) so the extra hook calls never sit in a
		// "pure" body. Falls through to the normal un-compilable handling if there is no
		// native generator (an interp-fallback op can't diverge from itself).
		if (ee_diff)
		{
			if (recEmitDiffVerifyOp(op, pc))
			{
				waitloop_possible = false; // verified block is never a wait-loop
				if (!needs_cycle_flush)
					raw_cycles += eeOpCycles(op);
				pc += 4;
				endpc = pc;
				if (++compiled >= MAX_BLOCK_INSTS)
				{
					recEmitWritePc(pc);
					known_dispatch_pc = true;
					dispatch_pc = pc;
					break;
				}
				continue;
			}
			// else: no native generator — fall through to the shared un-compilable path,
			// which single-steps the op on the interpreter (no verify needed).
		}
		// Straight-line op we can codegen? (Generators decode from `op` directly;
		// they never read cpuRegs.code, so nothing to set here at compile time.)
		else if (recTranslateOpOptimized(op, const_state, cache_state, pc))
		{
			// Record the body for wait-loop analysis (only short blocks qualify).
			if (waitloop_num_ops < REC_WAITLOOP_MAX_OPS)
				waitloop_ops[waitloop_num_ops++] = op;
			else
				waitloop_possible = false;

			if (!needs_cycle_flush)
				raw_cycles += eeOpCycles(op);
			pc += 4;
			endpc = pc;
			if (++compiled >= MAX_BLOCK_INSTS)
			{
				recEmitWritePc(pc); // resume at the next instruction
				known_dispatch_pc = true;
				dispatch_pc = pc;
				break;
			}
			continue;
		}

		// Un-compilable op (likely branch / syscall / COP0 / MMI SIMD / ...).
		if (compiled == 0)
		{
			// Block starts on it — emit a one-shot interpreter single-step block. It
				// runs exactly one guest instruction (handling its own PC, delay slot and
				// cycle accounting), then re-dispatches via the tail. No compiled cycles to
				// charge (intExecuteOneInst does that itself).
				armEmitCall(reinterpret_cast<const void*>(intExecuteOneInst));
				armEmitCall(reinterpret_cast<const void*>(recCheckExitAfterInterp));
				endpc = pc + 4;
			interp_step = true;
			break;
		}

		// End the block here; the next dispatch will single-step this op.
		recEmitWritePc(pc);
		known_dispatch_pc = true;
		dispatch_pc = pc;
		break;
	}

	recCacheFlushAll(cache_state);
	recCacheKillAll(cache_state);

	recEmitEventTestAndDispatch(interp_step ? 0 : recScaleBlockCycles(raw_cycles), !interp_step,
		!interp_step && known_dispatch_pc, dispatch_pc, waitloop_selfpc);

	// Apply SMC protection (must emit any checksum prologue into this block's stream before
	// armEndBlock flushes it). `block_entry` is what subsequent dispatches jump to.
	u8* block_entry = entry;
	if (interp_step)
	{
		// Single-step interp blocks re-read guest memory each run -> no checksum needed.
		// Still keep the page's protection state consistent: mark a fresh page counted, but
		// never re-protect a page that's already Manual (that would revive the write-fault
		// thrash the Manual tier exists to avoid).
		const vtlb_ProtectionMode mode = mmap_GetRamPageInfo(startpc);
		if (mode == ProtMode_None || mode == ProtMode_Write)
			mmap_MarkCountedRamPage(startpc);
	}
	else
	{
		block_entry = recEmitManualProtection(startpc, endpc, entry);
	}

	recPtr = armEndBlock();

	// Install the block so subsequent dispatches to startpc (and its address mirrors)
	// branch straight into it instead of recompiling.
	*recPtrToBlock(startpc) = reinterpret_cast<uptr>(block_entry);

	// Register for direct-B block chaining: resolve forward links (target already
	// compiled) and back-patch any predecessors that were waiting on this block.
	if (s_eeBlockLinkEnabled)
		recRegisterBlockLinks(startpc, block_entry);
}

static void recEventTest()
{
	const auto exit_execution = []() {
		eeRecExitRequested.store(false, std::memory_order_release);
		eeRecExitSignal = 0;
		longjmp(s_jmp_buf, 1);
	};

	VMState st = VMManager::GetState();
	if (st == VMState::Stopping || st == VMState::Shutdown)
		exit_execution();

	_cpuEventTest_Shared();

	st = VMManager::GetState();
	if (eeRecExitRequested.load(std::memory_order_acquire) ||
		st == VMState::Stopping || st == VMState::Shutdown)
		exit_execution();
}

static void recCheckExitAfterInterp()
{
	const VMState st = VMManager::GetState();
	if (eeRecExitRequested.load(std::memory_order_acquire) ||
		st == VMState::Stopping || st == VMState::Shutdown)
	{
		eeRecExitRequested.store(false, std::memory_order_release);
		eeRecExitSignal = 0;
		longjmp(s_jmp_buf, 1);
	}
}

// C entry point. Pins the exit longjmp target, then jumps into the generated
// EnterRecompiledCode stub, which establishes RESTATEPTR and runs blocks chained
// entirely in host code (block -> DispatcherReg -> block ...). Control only returns
// here via the longjmp in recEventTest (state-check / exit request).
static void recExecute()
{
	if (eeRecNeedsReset || !EnterRecompiledCode)
		recResetRaw();

	if (setjmp(s_jmp_buf) != 0)
	{
		eeRecExecuting = false;
		return;
	}

	eeRecExecuting = true;

	// Cancel-instruction landing pad. An inline-interpreted op (recEmitInterpInline) that
	// aborts the in-flight guest instruction — a vtlb TLB miss (vtlb.cpp), an address error
	// (R5900OpcodeImpl RaiseAddressError), or a met MIPS trap raised by the native trap
	// block's interpreter call — reaches Cpu->CancelInstruction() -> recCancelInstruction()
	// -> longjmp(s_cancel_jmp_buf). We land here (NOT the s_jmp_buf exit path), then
	// re-dispatch into the recompiled code from cpuRegs.pc, which cpuException already set to
	// the exception vector. The faulting op never reached intUpdateCPUCycles, so charge a
	// small fixed cycle (matching the old backend's +8) to guarantee forward progress and let
	// any due event fire before re-entry. Then recEventTest() runs the shared event test
	// AND honors a pending exit (VMState Stopping/Shutdown or eeRecExitRequested) by
	// longjmp'ing to s_jmp_buf above, so Stop/Shutdown still unwinds. (Native traps that
	// DON'T longjmp commit cycles + tail to DispatcherEvent themselves; this pad only
	// catches the interp calls that abort via cpuException.) Mirrors trak's cancel pad.
	if (setjmp(s_cancel_jmp_buf) != 0)
	{
		cpuRegs.cycle += 8;
		recEventTest();
	}

	reinterpret_cast<void (*)()>(reinterpret_cast<uintptr_t>(EnterRecompiledCode))();
	// EnterRecompiledCode never returns; the only way out is one of the longjmps above.
}

static void recSafeExitExecution()
{
	// Ask the dispatcher loop to fastjmp out at the next event test. Forcing the
	// event cycle to 0 guarantees the test fires after the current block.
	eeRecExitRequested.store(true, std::memory_order_release);
	eeRecExitSignal = 1;
	cpuRegs.nextEventCycle = 0;
}

static void recCancelInstruction()
{
	// Raised when an inline-interpreted op aborts the in-flight guest instruction: a vtlb
	// TLB miss (vtlb.cpp), an address error (R5900OpcodeImpl RaiseAddressError), or a met
	// MIPS trap (trap() -> cpuException(0x34) -> Cpu->CancelInstruction()). cpuException has
	// already rewritten cpuRegs.pc to the exception vector; we must NOT exit recExecute (that
	// would stop the EE), only unwind the in-flight interp call and re-dispatch from the new
	// PC. This matches the old arm64 backend and the interpreter's intCancelInstruction, both
	// of which longjmp back into their loop.
	longjmp(s_cancel_jmp_buf, 1);
}

static void recClear(u32 addr, u32 size)
{
	// Targeted invalidation (Phase 4.5): reset only the recLUT slots covering
	// [addr, addr+size) back to JITCompile, so the next dispatch to any of those guest
	// words recompiles fresh. The orphaned host code for the discarded blocks is
	// reclaimed at the next full cache reset (when recPtr wraps past recPtrEnd in
	// recRecompile). This mirrors the x86 rec's per-range clear instead of the old
	// bring-up whole-cache reset: recResetRaw rebuilds the dispatchers AND rewrites the
	// entire multi-million-entry recLUT, and Cpu->Clear is called a page at a time
	// (MapTLB issues one 0x400 clear per mapped TLB page during BIOS setup), so a
	// whole-cache reset per call made boot effectively hang.
	//
	// Safe while executing: recClear is always invoked synchronously on the EE thread
	// (a store page-fault or an interpreted TLBWI), so there is no concurrent block. An
	// in-flight block whose slot we clear keeps running its still-valid host code to
	// completion, then re-dispatches through DispatcherReg, which recompiles the slot.
	if (!JITCompile)
		return; // rec not yet generated — nothing compiled to invalidate.

	const u32 end = addr + size;
	for (u32 pc = addr & ~3u; pc < end; pc += 4)
	{
		uptr* const slot = recPtrToBlock(pc);
		// Skip unmapped guest pages: their slots all alias one shared page pointing at
		// UnmappedRecLUTPage; don't turn an unmapped word into a compile-on-jump word.
		if (*slot != reinterpret_cast<uptr>(UnmappedRecLUTPage))
			*slot = reinterpret_cast<uptr>(JITCompile);
	}

	// Unpatch any direct-B links whose target is in the cleared range BEFORE that
	// host code is recycled, so no predecessor can branch into a stale block.
	// recHWAddr is linear over the (small, intra-mirror) cleared range.
	if (s_eeBlockLinkEnabled)
	{
		const u32 start_pc = addr & ~3u;
		const u32 span = (addr + size) - start_pc;
		const u32 start_hw = recHWAddr(start_pc);
		// Tripwire: the flat [start_hw, start_hw+span) range assumes recHWAddr is
		// linear across the cleared span (no RAM/BIOS mirror-fold crossing) — true for
		// every current caller (page-aligned RAM, 0x400 TLB spans). Catch a future one.
		pxAssert(span < 4 || recHWAddr(start_pc) + span == recHWAddr(start_pc + span - 4) + 4);
		eeInvalidateLinks(start_hw, start_hw + span);
	}
}

// Called (via the DispatchBlockDiscard stub) when a manually-protected block fails its
// entry checksum: the guest code really changed, so throw the block away and recompile.
// `start` is the guest startpc, `sz` the block size in bytes. Mirrors x86 dyna_block_discard.
static void dyna_block_discard(u32 start, u32 sz)
{
	recClear(start, sz);
}

// Called (via the DispatchPageReset stub) when a counted manual block has run enough times
// to be worth retrying cheap write-protection: clear the whole page's blocks, bump the
// per-page retry counter, and re-arm vtlb write protection. Mirrors x86 dyna_page_reset.
static void dyna_page_reset(u32 start, u32 sz)
{
	recClear(start & ~__pagemask, __pagesize);
	const u32 rampage = static_cast<u32>(
		(reinterpret_cast<uptr>(PSM(start)) - reinterpret_cast<uptr>(eeMem->Main)) >> __pageshift);
	manual_counter[rampage]++;
	mmap_MarkCountedRamPage(start);
}

R5900cpu recCpu = {
	recReserve,
	recShutdown,

	recResetEE,
	recStep,
	recExecute,

	recSafeExitExecution,
	recCancelInstruction,
	recClear};

