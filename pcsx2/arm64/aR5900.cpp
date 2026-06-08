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

#include "Config.h"
#include "Memory.h"
#include "R5900.h"
#include "R5900OpcodeTables.h"
#include "VMManager.h"
#include "vtlb.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/FastJmp.h"
#include "common/Pcsx2Defs.h"

#if defined(__APPLE__) && TARGET_OS_IPHONE
#include "common/Darwin/DarwinMisc.h"
#include <TargetConditionals.h>
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <vector>

#ifndef ARMSX2_ENABLE_EE_HOTPATH_DIAGNOSTICS
#define ARMSX2_ENABLE_EE_HOTPATH_DIAGNOSTICS 0
#endif

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
static bool eeRecExitRequested = false;
static fastjmp_buf s_jmp_buf;

static void recResetRaw();
static void recGenDispatchers();
static void recRecompile(u32 startpc);
static void recEventTest();
static void recClear(u32 addr, u32 size);
static void dyna_block_discard(u32 start, u32 sz);
static void dyna_page_reset(u32 start, u32 sz);

static void recDumpCodeBytes(const char* label, const void* rx_ptr, size_t len)
{
#if ARMSX2_ENABLE_EE_HOTPATH_DIAGNOSTICS
	const size_t dump_len = std::min<size_t>(len, 32);
	char rx_hex[65] = {};
	char rw_hex[65] = {};
	const u8* const rx = static_cast<const u8*>(rx_ptr);

	for (size_t i = 0; i < dump_len; i++)
		std::snprintf(&rx_hex[i * 2], sizeof(rx_hex) - (i * 2), "%02x", rx[i]);

#if defined(__APPLE__) && TARGET_OS_IPHONE
	const ptrdiff_t rw_offset = DarwinMisc::g_code_rw_offset;
	const u8* const rw = rw_offset != 0 ? (rx + rw_offset) : rx;
	for (size_t i = 0; i < dump_len; i++)
		std::snprintf(&rw_hex[i * 2], sizeof(rw_hex) - (i * 2), "%02x", rw[i]);

	std::fprintf(stderr,
		"@@EE_REC_BYTES@@ label=%s rx=%p rw=%p len=%zu cmp=%d rx=%s rw=%s\n",
		label, rx, rw, dump_len, std::memcmp(rx, rw, dump_len), rx_hex, rw_hex);
#else
	std::fprintf(stderr, "@@EE_REC_BYTES@@ label=%s rx=%p len=%zu rx=%s\n",
		label, rx, dump_len, rx_hex);
#endif
	std::fflush(stderr);
#else
	(void)label;
	(void)rx_ptr;
	(void)len;
#endif
}

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

static void recResetRaw()
{
	// Rewind the emit cursor, drop all cached trampolines/literals, regenerate the
	// dispatcher stubs at the head of the cache, then reset every block slot. Order
	// matters: recGenDispatchers fills the JITCompile / UnmappedRecLUTPage pointers
	// that recClearLUT writes into the slots.
	recPtr = SysMemory::GetEERec();
	s_const_pool.Reset();
	recGenDispatchers();
#if ARMSX2_ENABLE_EE_HOTPATH_DIAGNOSTICS
	static int s_reset_dump_count = 0;
	if (s_reset_dump_count++ < 2)
	{
		std::fprintf(stderr,
			"@@EE_REC_RESET@@ base=%p end=%p recPtr=%p dispatch=%p event=%p compile=%p enter=%p unmapped=%p rw_offset=%td\n",
			SysMemory::GetEERec(), SysMemory::GetEERecEnd(), recPtr, DispatcherReg, DispatcherEvent,
			JITCompile, EnterRecompiledCode, UnmappedRecLUTPage,
#if defined(__APPLE__) && TARGET_OS_IPHONE
			DarwinMisc::g_code_rw_offset
#else
			static_cast<ptrdiff_t>(0)
#endif
		);
		std::fflush(stderr);
		recDumpCodeBytes("dispatcher", DispatcherReg, 32);
		recDumpCodeBytes("jitcompile", JITCompile, 32);
		recDumpCodeBytes("enter", EnterRecompiledCode, 32);
	}
#endif
	recClearLUT();

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
		eeRecExitRequested = true;
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
static bool recTranslateOp(u32 op);

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

	if (!recTranslateOp(op))
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

static constexpr int REC_GPR_CACHE_REGS[7] = {22, 23, 24, 25, 26, 27, 28};

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

static void recEmitCachedEffectiveAddr(RecGprCacheState& cache, u32 rs, s32 imm, const a64::Register& addr)
{
	if (rs == 0)
	{
		armAsm->Mov(addr.W(), imm);
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

static bool recTryTranslateCachedLoad(u32 bits, bool sign, u32 rt, u32 rs, s32 imm, RecGprCacheState& cache)
{
	static const a64::Register RADDR = a64::x9;
	static const a64::Register RHOST = a64::x10;
	static const a64::Register RTEMP = a64::x11;

	recEmitCachedEffectiveAddr(cache, rs, imm, RADDR);
	const RecGprCacheState pre_load_cache = cache;

	const a64::Register& dst = (rt == 0) ? RTEMP : recCacheDest(cache, rt, rs);

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

static bool recTryTranslateCachedStore(u32 bits, u32 rt, u32 rs, s32 imm, RecGprCacheState& cache)
{
	static const a64::Register RADDR = a64::x9;
	static const a64::Register RHOST = a64::x10;

	recEmitCachedEffectiveAddr(cache, rs, imm, RADDR);
	const a64::Register& src = recCacheLoad(cache, rt);
	const RecGprCacheState pre_store_cache = cache;

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

static bool recTryTranslateCachedOp(u32 op, RecGprCacheState& cache)
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
			if (opcode == 0x0C)
			{
				if (imm_u == 0)
					armAsm->Mov(dst, 0);
				else
				{
					armAsm->Mov(RXVIXLSCRATCH, imm_u);
					armAsm->And(dst, src, RXVIXLSCRATCH);
				}
			}
			else if (opcode == 0x0D)
			{
				move_x(dst, src);
				if (imm_u != 0)
				{
					armAsm->Mov(RXVIXLSCRATCH, imm_u);
					armAsm->Orr(dst, dst, RXVIXLSCRATCH);
				}
			}
			else
			{
				move_x(dst, src);
				if (imm_u != 0)
				{
					armAsm->Mov(RXVIXLSCRATCH, imm_u);
					armAsm->Eor(dst, dst, RXVIXLSCRATCH);
				}
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

		case OP_LB:  return recTryTranslateCachedLoad(8,  true,  rt, rs, imm, cache);
		case OP_LBU: return recTryTranslateCachedLoad(8,  false, rt, rs, imm, cache);
		case OP_LH:  return recTryTranslateCachedLoad(16, true,  rt, rs, imm, cache);
		case OP_LHU: return recTryTranslateCachedLoad(16, false, rt, rs, imm, cache);
		case OP_LW:  return recTryTranslateCachedLoad(32, true,  rt, rs, imm, cache);
		case OP_LWU: return recTryTranslateCachedLoad(32, false, rt, rs, imm, cache);
		case OP_LD:  return recTryTranslateCachedLoad(64, false, rt, rs, imm, cache);

		case OP_SB: return recTryTranslateCachedStore(8,  rt, rs, imm, cache);
		case OP_SH: return recTryTranslateCachedStore(16, rt, rs, imm, cache);
		case OP_SW: return recTryTranslateCachedStore(32, rt, rs, imm, cache);
		case OP_SD: return recTryTranslateCachedStore(64, rt, rs, imm, cache);

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

		case 0x00: // SLL
		case 0x02: // SRL
		case 0x03: // SRA
		{
			if (rd == 0)
				return true;
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
			const a64::Register& src = recCacheLoad(cache, rt);
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
			const a64::Register& src = recCacheLoad(cache, rt);
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
			const a64::Register& src = recCacheLoad(cache, rt);
			const a64::Register& dst = recCacheDest(cache, rd, rt);
			if (funct == 0x38 || funct == 0x3C)
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
			const a64::Register& lhs = recCacheLoad(cache, rs);
			const a64::Register& rhs = recCacheLoad(cache, rt);
			const a64::Register& dst = recCacheDest(cache, rd, rs, rt);
			if (funct == 0x20 || funct == 0x21)
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
			const a64::Register& lhs = recCacheLoad(cache, rs);
			const a64::Register& rhs = recCacheLoad(cache, rt);
			const a64::Register& dst = recCacheDest(cache, rd, rs, rt);
			if (funct == 0x2C || funct == 0x2D)
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

static bool recTranslateOpOptimized(u32 op, RecGprConstState& const_state, RecGprCacheState& cache)
{
	if (recTryTranslateCachedOp(op, cache))
	{
		if (!recConstApplyCachedEffects(op, const_state))
			recConstApplyNativeEffects(op, const_state);
		return true;
	}

	recCacheFlushAll(cache);
	recCacheKillAll(cache);

	if (recTryTranslateConstOp(op, const_state))
	{
		return true;
	}

	if (!recTranslateOp(op))
	{
		recConstKillAll(const_state);
		return false;
	}

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

static bool recTranslateOp(u32 op)
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
				default:   return false;
			}

		// MMI — second-pipeline multiply/divide (Phase 3.5). Other MMI ops (SIMD,
		// MFHI1/MFLO1, ...) are not yet implemented and fall through to false.
		case 0x1C:
			switch (funct)
			{
				case 0x18: armEmitMULT1(rd, rs, rt); return true;
				case 0x19: armEmitMULTU1(rd, rs, rt); return true;
				case 0x1A: armEmitDIV1(rs, rt); return true;
				case 0x1B: armEmitDIVU1(rs, rt); return true;
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

		// 128-bit quadword load/store (16-byte aligned).
		case OP_LQ: armEmitLoadQuad(rt, rs, imm); return true;
		case OP_SQ: armEmitStoreQuad(rt, rs, imm); return true;

		// FPU load/store (Phase 5.2a) — 32-bit transfer between memory and FPR[rt].
		case OP_LWC1: armEmitLWC1(rt, rs, imm); return true;
		case OP_SWC1: armEmitSWC1(rt, rs, imm); return true;

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

		// COP2 — VU0 macro mode (Phase 5.3). On ARM64 CpuVU0 is the synchronous VU0
		// interpreter, so unlike the x86 rec there is no deferred microVU program to
		// finish/sync (mVUFinishVU0) before touching VU0 state. That makes running the
		// interpreter's COP2 handler *inline* identical to single-stepping it — but it
		// keeps the EE block intact instead of forcing a block-break + dispatcher
		// round-trip per op. VU0-macro geometry (e.g. FFX) interleaves many COP2 ops
		// with EE code, so this is the win: no fragmentation. The BC2 branches
		// (rs==0x08) write cpuRegs.pc, so they stay on the interpreter single-step path
		// (handled by recRecompile ending the block before them).
		case 0x12:
			if (rs == 0x08)
				return false; // BC2F/BC2T/BC2FL/BC2TL — single-step (writes PC)
			recEmitInterpInline(op);
			return true;

		// COP2 quadword load/store (VF[rt] ↔ memory). Straight-line, no PC write —
		// inline the interpreter handler like the COP2 macro ops above.
		case OP_LQC2: recEmitInterpInline(op); return true;
		case OP_SQC2: recEmitInterpInline(op); return true;

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
		default:
			return false;
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
	armEmitCall(reinterpret_cast<const void*>(R5900::GetInstruction(op).interpret));
}

// Compile one straight-line or delay-slot instruction: const-folded/native generator
// if we have one, otherwise an inline interpreter call.
static void recEmitOp(u32 op, RecGprConstState& const_state, RecGprCacheState& cache_state)
{
	if (!recTranslateOpOptimized(op, const_state, cache_state))
		recEmitInterpInline(op);
}

// cpuRegs.pc = imm (block fallthrough / early-exit target).
static void recEmitWritePc(u32 pc)
{
	armAsm->Mov(RSCRATCHADDR.W(), pc);
	armAsm->Str(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_PC_OFFSET));
}

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
// Logs once and bails out of the rec via the exit fastjmp, mirroring x86 recError(0).
static void recExitUnmapped()
{
	Console.Error("ARM64 EE rec: jump to unmapped recLUT page (PC=0x%08x)", cpuRegs.pc);
	eeRecExitRequested = true;
	fastjmp_jmp(&s_jmp_buf, 1);
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
	// then dispatch. We never return through here (exit is a fastjmp out of
	// recEventTest), so callee-saved registers need no preserving — fastjmp restores
	// recExecute's full context. Blocks therefore need no per-block prologue/epilogue.
	EnterRecompiledCode = armGetCurrentCodePointer();
	armMoveAddressToReg(RESTATEPTR, &cpuRegs);
	armLoadPtr(REVTLBPTR, &vtlb_private::vtlbdata.vmap);
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

#if ARMSX2_ENABLE_EE_HOTPATH_DIAGNOSTICS
	static int s_dispatcher_log_count = 0;
	if (s_dispatcher_log_count++ < 2)
	{
		std::fprintf(stderr,
			"@@EE_REC_DISPATCHERS@@ base=%p recPtr=%p dispatch=%p event=%p compile=%p enter=%p unmapped=%p discard=%p page_reset=%p\n",
			SysMemory::GetEERec(), recPtr, DispatcherReg, DispatcherEvent, JITCompile,
			EnterRecompiledCode, UnmappedRecLUTPage, DispatchBlockDiscard, DispatchPageReset);
		std::fflush(stderr);
	}
#endif
}

// Emit a block's tail: charge the block's scaled guest cycles, then the inline event
// test. Mirrors iR5900.cpp iBranchTest (dynamic-target form): if (s64)(cycle -
// nextEventCycle) < 0 there is no event due, so jump straight back into the dispatcher
// (DispatcherReg re-reads cpuRegs.pc and chains into the next block); otherwise fall
// to DispatcherEvent to service events first. `add_cycles` is false for interpreter
// single-step blocks, which charge their own cycles inside intExecuteOneInst.
static void recEmitEventTestAndDispatch(u32 scaled_cycles, bool add_cycles, bool known_dispatch_pc, u32 dispatch_pc)
{
	armAsm->Ldr(RXARG1, a64::MemOperand(RESTATEPTR, EE_CYCLE_OFFSET)); // x0 = cpuRegs.cycle (u64)
	if (add_cycles)
	{
		armAsm->Add(RXARG1, RXARG1, scaled_cycles);
		armAsm->Str(RXARG1, a64::MemOperand(RESTATEPTR, EE_CYCLE_OFFSET));
	}
	armAsm->Ldr(RXARG2, a64::MemOperand(RESTATEPTR, EE_NEXTEVENTCYCLE_OFFSET));
	armAsm->Cmp(RXARG1, RXARG2);

	if (known_dispatch_pc)
	{
		armEmitCondBranch(a64::pl, DispatcherEvent); // event due => service before continuing
		recEmitDispatchToKnownPc(dispatch_pc);
		return;
	}

	armEmitCondBranch(a64::mi, DispatcherReg); // N set => (cycle - nextEvent) < 0 => continue
	armEmitJmp(DispatcherEvent);
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
static void recRecompile(u32 startpc)
{
#if ARMSX2_ENABLE_EE_HOTPATH_DIAGNOSTICS
	static int s_recompile_log_count = 0;
	int recompile_log_index = -1;
#endif
	const u32 hw_startpc = recHWAddr(startpc);
#if ARMSX2_ENABLE_EE_HOTPATH_DIAGNOSTICS
	if (s_recompile_log_count < 16)
	{
		recompile_log_index = s_recompile_log_count++;
		const u32 op = memRead32(startpc);
		uptr* const slot = recPtrToBlock(startpc);
		std::fprintf(stderr,
			"@@EE_REC_RECOMPILE_BEGIN@@ idx=%d pc=0x%08x op=0x%08x recPtr=%p recEnd=%p slot=%p slot_before=%p cycle=%lld next=%lld\n",
			recompile_log_index, startpc, op, recPtr, recPtrEnd, slot,
			reinterpret_cast<void*>(*slot), static_cast<long long>(cpuRegs.cycle),
			static_cast<long long>(cpuRegs.nextEventCycle));
		std::fflush(stderr);
	}
#endif

	// Reset the whole cache if the emit cursor has run within one block's worth of the
	// constant-pool tail. Doing it here (before emitting) is safe: the dispatcher stubs
	// regenerate byte-identically at the same addresses, so the JITCompile stub this
	// call returns into is unchanged. Mirrors x86 recRecompile.
	if (recPtr >= recPtrEnd - RECOMPILE_HEADROOM)
		eeRecNeedsReset = true;

	if (hw_startpc == VMManager::Internal::GetCurrentELFEntryPoint())
	{
#if defined(__APPLE__) && TARGET_OS_IPHONE
		std::fprintf(stderr,
			"@@IOS_ELF_ENTRY_COMPILE@@ pc=0x%08x hw=0x%08x entry=0x%08x fastboot_in_progress=%d booted=%d\n",
			startpc, hw_startpc, VMManager::Internal::GetCurrentELFEntryPoint(),
			VMManager::Internal::IsFastBootInProgress() ? 1 : 0,
			VMManager::Internal::HasBootedELF() ? 1 : 0);
		std::fflush(stderr);
#endif
		VMManager::Internal::EntryPointCompilingOnCPUThread();
	}

	if (eeRecNeedsReset)
		recResetRaw();

	armSetAsmPtr(recPtr, recPtrEnd - recPtr, &s_const_pool);
	u8* const entry = armStartBlock();

	if (hw_startpc == EELOAD_START)
	{
		const u32 mainjump = memRead32(EELOAD_START + 0x9c);
		if (mainjump >> 26 == 3) // JAL
			g_eeloadMain = ((EELOAD_START + 0xa0) & 0xf0000000U) | ((mainjump << 2) & 0x0fffffffu);
#if defined(__APPLE__) && TARGET_OS_IPHONE
		std::fprintf(stderr,
			"@@IOS_FASTBOOT_HOOK@@ stage=eeload_start pc=0x%08x hw=0x%08x main=0x%08x fastboot_in_progress=%d\n",
			startpc, hw_startpc, g_eeloadMain, VMManager::Internal::IsFastBootInProgress() ? 1 : 0);
		std::fflush(stderr);
#endif
	}

	if (g_eeloadMain && hw_startpc == recHWAddr(g_eeloadMain))
	{
		armEmitCall(reinterpret_cast<const void*>(eeloadHook));
#if defined(__APPLE__) && TARGET_OS_IPHONE
		std::fprintf(stderr,
			"@@IOS_FASTBOOT_HOOK@@ stage=eeload_main pc=0x%08x hw=0x%08x main=0x%08x fastboot_in_progress=%d\n",
			startpc, hw_startpc, g_eeloadMain, VMManager::Internal::IsFastBootInProgress() ? 1 : 0);
		std::fflush(stderr);
#endif
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
#if defined(__APPLE__) && TARGET_OS_IPHONE
			std::fprintf(stderr,
				"@@IOS_FASTBOOT_HOOK@@ stage=eeload_exec_detect pc=0x%08x exec=0x%08x typeA=0x%08x typeB=0x%08x typeC=0x%08x typeD=0x%08x\n",
				startpc, g_eeloadExec, typeAexecjump, typeBexecjump, typeCexecjump, typeDexecjump);
			std::fflush(stderr);
#endif
		}
	}

	if (g_eeloadExec && hw_startpc == recHWAddr(g_eeloadExec))
	{
		armEmitCall(reinterpret_cast<const void*>(eeloadHook2));
#if defined(__APPLE__) && TARGET_OS_IPHONE
		std::fprintf(stderr,
			"@@IOS_FASTBOOT_HOOK@@ stage=eeload_exec pc=0x%08x hw=0x%08x exec=0x%08x fastboot_in_progress=%d\n",
			startpc, hw_startpc, g_eeloadExec, VMManager::Internal::IsFastBootInProgress() ? 1 : 0);
		std::fflush(stderr);
#endif
	}

	u32 pc = startpc;
	u32 endpc = startpc;
	u32 raw_cycles = 0;
	u32 compiled = 0;
	bool interp_step = false;
	bool known_dispatch_pc = false;
	u32 dispatch_pc = 0;
	RecGprConstState const_state;
	RecGprCacheState cache_state;

	for (;;)
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
		const R5900::OPCODE& info = R5900::GetInstruction(op);

		if (recIsHandledBranch(op))
		{
			// Terminate the block: branch generator + delay slot + dispatch tail.
			raw_cycles += info.cycles;
			known_dispatch_pc = recGetKnownBranchTarget(op, pc, const_state, &dispatch_pc);
			recCacheFlushAll(cache_state);
			recCacheKillAll(cache_state);
			recEmitBranch(op, pc); // writes cpuRegs.pc (taken/fallthrough/link)
			recConstApplyBranchLink(op, pc, const_state);

			const u32 delay_op = memRead32(pc + 4);
			raw_cycles += R5900::GetInstruction(delay_op).cycles;
			recEmitOp(delay_op, const_state, cache_state); // delay slot — must not write cpuRegs.pc
			endpc = pc + 8;
			break;
		}

		// Straight-line op we can codegen? (Generators decode from `op` directly;
		// they never read cpuRegs.code, so nothing to set here at compile time.)
		if (recTranslateOpOptimized(op, const_state, cache_state))
		{
			raw_cycles += info.cycles;
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
		!interp_step && known_dispatch_pc, dispatch_pc);

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
#if ARMSX2_ENABLE_EE_HOTPATH_DIAGNOSTICS
	if (recompile_log_index >= 0)
	{
		std::fprintf(stderr,
			"@@EE_REC_RECOMPILE_END@@ idx=%d pc=0x%08x endpc=0x%08x entry=%p block_entry=%p recPtr=%p compiled=%u interp=%d raw_cycles=%u slot_after=%p\n",
			recompile_log_index, startpc, endpc, entry, block_entry, recPtr, compiled,
			interp_step ? 1 : 0, raw_cycles, reinterpret_cast<void*>(*recPtrToBlock(startpc)));
		std::fflush(stderr);
		recDumpCodeBytes("block", block_entry, 32);
	}
#endif
}

static void recEventTest()
{
#if ARMSX2_ENABLE_EE_HOTPATH_DIAGNOSTICS
	static int s_event_log_count = 0;
	if (s_event_log_count < 16)
	{
		std::fprintf(stderr,
			"@@EE_REC_EVENT@@ idx=%d pc=0x%08x cycle=%lld next=%lld state=%d exit_requested=%d\n",
			s_event_log_count++, cpuRegs.pc, static_cast<long long>(cpuRegs.cycle),
			static_cast<long long>(cpuRegs.nextEventCycle), static_cast<int>(VMManager::GetState()),
			eeRecExitRequested ? 1 : 0);
			std::fflush(stderr);
	}
#endif

	_cpuEventTest_Shared();

	if (eeRecExitRequested)
	{
		eeRecExitRequested = false;
		fastjmp_jmp(&s_jmp_buf, 1);
	}
}

// C entry point. Pins the exit fastjmp target, then jumps into the generated
// EnterRecompiledCode stub, which establishes RESTATEPTR and runs blocks chained
// entirely in host code (block -> DispatcherReg -> block ...). Control only returns
// here via the fastjmp in recEventTest (state-check / exit request).
static void recExecute()
{
	if (eeRecNeedsReset || !EnterRecompiledCode)
		recResetRaw();

#if defined(__APPLE__) && TARGET_OS_IPHONE
	DarwinMisc::LegacyEnsureExecutable();
#endif

	if (fastjmp_set(&s_jmp_buf) != 0)
	{
		eeRecExecuting = false;
#if ARMSX2_ENABLE_EE_HOTPATH_DIAGNOSTICS
		std::fprintf(stderr, "@@EE_REC_EXEC_EXIT@@ pc=0x%08x cycle=%lld next=%lld state=%d\n",
			cpuRegs.pc, static_cast<long long>(cpuRegs.cycle),
			static_cast<long long>(cpuRegs.nextEventCycle), static_cast<int>(VMManager::GetState()));
		std::fflush(stderr);
#endif
		return;
	}

#if ARMSX2_ENABLE_EE_HOTPATH_DIAGNOSTICS
	static int s_exec_log_count = 0;
	if (s_exec_log_count++ < 4)
	{
		std::fprintf(stderr,
			"@@EE_REC_EXEC_ENTER@@ pc=0x%08x enter=%p dispatch=%p compile=%p recPtr=%p recEnd=%p jitBase=0x%llx jitEnd=0x%llx rw_offset=%td state=%d cycle=%lld next=%lld\n",
			cpuRegs.pc, EnterRecompiledCode, DispatcherReg, JITCompile, recPtr, recPtrEnd,
#if defined(__APPLE__) && TARGET_OS_IPHONE
			static_cast<unsigned long long>(DarwinMisc::GetJitBase()),
			static_cast<unsigned long long>(DarwinMisc::GetJitEnd()),
			DarwinMisc::g_code_rw_offset,
#else
			0ull, 0ull, static_cast<ptrdiff_t>(0),
#endif
			static_cast<int>(VMManager::GetState()), static_cast<long long>(cpuRegs.cycle),
			static_cast<long long>(cpuRegs.nextEventCycle));
		std::fflush(stderr);
		recDumpCodeBytes("exec_enter", EnterRecompiledCode, 32);
	}
#endif

	eeRecExecuting = true;
	reinterpret_cast<void (*)()>(reinterpret_cast<uintptr_t>(EnterRecompiledCode))();
	// EnterRecompiledCode never returns; the only way out is the fastjmp above.
#if ARMSX2_ENABLE_EE_HOTPATH_DIAGNOSTICS
	std::fprintf(stderr, "@@EE_REC_EXEC_RETURN_UNEXPECTED@@ pc=0x%08x\n", cpuRegs.pc);
	std::fflush(stderr);
#endif
}

static void recSafeExitExecution()
{
	// Ask the dispatcher loop to fastjmp out at the next event test. Forcing the
	// event cycle to 0 guarantees the test fires after the current block.
	eeRecExitRequested = true;
	cpuRegs.nextEventCycle = 0;
}

static void recCancelInstruction()
{
	pxFailRel("recCancelInstruction() called, this should never happen!");
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
