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

#include "Memory.h"
#include "R5900.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/Pcsx2Defs.h"

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

static void recReserve()
{
	recPtr = SysMemory::GetEERec();
	recPtrEnd = SysMemory::GetEERecEnd() - EE_CONSTPOOL_SIZE;

	s_const_pool.Init(recPtrEnd, EE_CONSTPOOL_SIZE);
}

static void recShutdown()
{
	s_const_pool.Destroy();

	recPtr = nullptr;
	recPtrEnd = nullptr;
}

static void recResetEE()
{
	// Rewind the emit cursor and drop all cached trampolines/literals. Block map
	// and dispatcher regeneration land in Phase 1.4; const-prop state in Phase 3.6.
	recPtr = SysMemory::GetEERec();
	s_const_pool.Reset();
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
};

// Translate a single guest instruction (cpuRegs.code) into the open block. Returns
// true if a real generator handled it, false if it fell through to a placeholder.
// Decodes the MIPS fields explicitly and hands them to the Phase 2.3 load/store
// generators (which read/write guest GPRs through RESTATEPTR and route memory
// access via the slow-path vtlb helpers).
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
				default:   return false;
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

		default: return false;
	}
}

// --------------------------------------------------------------------------------------
//  Minimal block compile loop (Phase 1.4, extended in 2.3)
// --------------------------------------------------------------------------------------
// Compile a single guest instruction at cpuRegs.pc into a block and return its
// entry point. This exercises the production emission lifecycle (armSetAsmPtr ->
// armStartBlock -> emit -> armEndBlock) against the real EE code cache, now with a
// real MIPS decode + dispatch for LW/SW instead of a fixed NOP body.
//
// This path is still INERT in normal operation: the interpreter stays the active
// Cpu provider (recExecute is never entered — Phase 4 adds the enter-trampoline
// that pins RESTATEPTR=&cpuRegs, the block LUT, PC/cycle management, and event
// tests). It is groundwork validated by compile-clean + the Arm64EmitEE unit tests
// that exercise the generators directly; full guest-memory round-trip validation
// is Phase 2.4.
static u8* recCompileBlock()
{
	// Whole-cache reset once we run past the code region into the constant-pool tail.
	if (recPtr >= recPtrEnd)
		recResetEE();

	armSetAsmPtr(recPtr, recPtrEnd - recPtr, &s_const_pool);

	u8* const entry = armStartBlock();

	const u32 op = memRead32(cpuRegs.pc);
	cpuRegs.code = op;
	if (!recTranslateOp(op))
		armAsm->Nop(); // unhandled opcode placeholder (most ops, until later phases)

	armAsm->Ret();

	recPtr = armEndBlock();
	return entry;
}

static void recExecute()
{
	// Phase 1.4 proof-of-life: compile one trivial block through the real emitter,
	// enter it, and return. There is no execution loop / dispatcher yet, so this
	// runs exactly one (empty) block. The interpreter remains the active Cpu
	// provider (recCpu is not yet selected in VMManager — Phase 1.5), so this path
	// is not hit in normal operation; it exists to validate emit + enter + return
	// end-to-end on the EE code cache before real codegen lands.
	u8* const entry = recCompileBlock();
	reinterpret_cast<void (*)()>(entry)();
}

static void recSafeExitExecution()
{
	// TODO(Phase 4): signal the dispatcher loop to exit at a safe point.
}

static void recCancelInstruction()
{
	pxFailRel("recCancelInstruction() called, this should never happen!");
}

static void recClear(u32 addr, u32 size)
{
	// TODO(Phase 4.5): invalidate compiled blocks covering [addr, addr+size).
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
