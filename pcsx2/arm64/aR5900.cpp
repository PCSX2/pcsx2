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

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/FastJmp.h"
#include "common/Pcsx2Defs.h"

#include <cstring>
#include <vector>

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

// Dynamically-generated dispatcher stubs (emitted into the head of the code cache by
// recGenDispatchers on every reset; addresses are stable across a reset because the
// stubs regenerate byte-identically at the same location — see recRecompile).
static const void* DispatcherReg = nullptr;        // lookup cpuRegs.pc in recLUT, jump
static const void* DispatcherEvent = nullptr;      // run event test, then fall to DispatcherReg
static const void* JITCompile = nullptr;           // compile block at cpuRegs.pc, then dispatch
static const void* EnterRecompiledCode = nullptr;  // C entry: pin RESTATEPTR, then dispatch
static const void* UnmappedRecLUTPage = nullptr;   // jumped to on an unmapped guest PC

// Execution / reset / exit plumbing, mirroring the x86 rec (iR5900.cpp).
static bool eeRecExecuting = false;
static bool eeRecNeedsReset = false;
static bool eeRecExitRequested = false;
static fastjmp_buf s_jmp_buf;

static void recResetRaw();
static void recGenDispatchers();
static void recRecompile(u32 startpc);
static void recEventTest();

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
	recClearLUT();

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

// Compile one straight-line or delay-slot instruction: real generator if we have
// one, otherwise an inline interpreter call.
static void recEmitOp(u32 op)
{
	if (!recTranslateOp(op))
		recEmitInterpInline(op);
}

// cpuRegs.pc = imm (block fallthrough / early-exit target).
static void recEmitWritePc(u32 pc)
{
	armAsm->Mov(RSCRATCHADDR.W(), pc);
	armAsm->Str(RSCRATCHADDR.W(), a64::MemOperand(RESTATEPTR, EE_PC_OFFSET));
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

// Mark every RAM page covered by a compiled block as recompiled code, so writes
// to loaded ELF/game code fault through the existing vtlb page-protection path
// and call Cpu->Clear(). ROM pages return ProtMode_NotRequired and are ignored.
static void recProtectCompiledRange(u32 startpc, u32 endpc)
{
	for (u32 pc = startpc & ~0xfffu; pc < endpc; pc += __pagesize)
	{
		if (mmap_GetRamPageInfo(pc) != ProtMode_NotRequired)
			mmap_MarkCountedRamPage(pc);
	}
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
	armAsm->B(&dispatcher_reg);

	// UnmappedRecLUTPage: target for every word of an unmapped guest page.
	UnmappedRecLUTPage = armGetCurrentCodePointer();
	armEmitCall(reinterpret_cast<const void*>(recExitUnmapped));
	armAsm->B(&dispatcher_reg);

	recPtr = armEndBlock();
}

// Emit a block's tail: charge the block's scaled guest cycles, then the inline event
// test. Mirrors iR5900.cpp iBranchTest (dynamic-target form): if (s64)(cycle -
// nextEventCycle) < 0 there is no event due, so jump straight back into the dispatcher
// (DispatcherReg re-reads cpuRegs.pc and chains into the next block); otherwise fall
// to DispatcherEvent to service events first. `add_cycles` is false for interpreter
// single-step blocks, which charge their own cycles inside intExecuteOneInst.
static void recEmitEventTestAndDispatch(u32 scaled_cycles, bool add_cycles)
{
	armAsm->Ldr(RXARG1, a64::MemOperand(RESTATEPTR, EE_CYCLE_OFFSET)); // x0 = cpuRegs.cycle (u64)
	if (add_cycles)
	{
		armAsm->Add(RXARG1, RXARG1, scaled_cycles);
		armAsm->Str(RXARG1, a64::MemOperand(RESTATEPTR, EE_CYCLE_OFFSET));
	}
	armAsm->Ldr(RXARG2, a64::MemOperand(RESTATEPTR, EE_NEXTEVENTCYCLE_OFFSET));
	armAsm->Cmp(RXARG1, RXARG2);
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
	// Reset the whole cache if the emit cursor has run within one block's worth of the
	// constant-pool tail. Doing it here (before emitting) is safe: the dispatcher stubs
	// regenerate byte-identically at the same addresses, so the JITCompile stub this
	// call returns into is unchanged. Mirrors x86 recRecompile.
	if (recPtr >= recPtrEnd - RECOMPILE_HEADROOM)
		eeRecNeedsReset = true;
	if (eeRecNeedsReset)
		recResetRaw();

	armSetAsmPtr(recPtr, recPtrEnd - recPtr, &s_const_pool);
	u8* const entry = armStartBlock();

	u32 pc = startpc;
	u32 endpc = startpc;
	u32 raw_cycles = 0;
	u32 compiled = 0;
	bool interp_step = false;

	for (;;)
	{
		const u32 op = memRead32(pc);
		const R5900::OPCODE& info = R5900::GetInstruction(op);

		if (recIsHandledBranch(op))
		{
			// Terminate the block: branch generator + delay slot + dispatch tail.
			raw_cycles += info.cycles;
			recEmitBranch(op, pc); // writes cpuRegs.pc (taken/fallthrough/link)

			const u32 delay_op = memRead32(pc + 4);
			raw_cycles += R5900::GetInstruction(delay_op).cycles;
			recEmitOp(delay_op); // delay slot — must not write cpuRegs.pc
			endpc = pc + 8;
			break;
		}

		// Straight-line op we can codegen? (Generators decode from `op` directly;
		// they never read cpuRegs.code, so nothing to set here at compile time.)
		if (recTranslateOp(op))
		{
			raw_cycles += info.cycles;
			pc += 4;
			endpc = pc;
			if (++compiled >= MAX_BLOCK_INSTS)
			{
				recEmitWritePc(pc); // resume at the next instruction
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
		break;
	}

	recEmitEventTestAndDispatch(interp_step ? 0 : recScaleBlockCycles(raw_cycles), !interp_step);

	recPtr = armEndBlock();

	recProtectCompiledRange(startpc, endpc);

	// Install the block so subsequent dispatches to startpc (and its address mirrors)
	// branch straight into it instead of recompiling.
	*recPtrToBlock(startpc) = reinterpret_cast<uptr>(entry);
}

static void recEventTest()
{
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

	if (fastjmp_set(&s_jmp_buf) != 0)
	{
		eeRecExecuting = false;
		return;
	}

	eeRecExecuting = true;
	reinterpret_cast<void (*)()>(reinterpret_cast<uintptr_t>(EnterRecompiledCode))();
	// EnterRecompiledCode never returns; the only way out is the fastjmp above.
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

R5900cpu recCpu = {
	recReserve,
	recShutdown,

	recResetEE,
	recStep,
	recExecute,

	recSafeExitExecution,
	recCancelInstruction,
	recClear};
