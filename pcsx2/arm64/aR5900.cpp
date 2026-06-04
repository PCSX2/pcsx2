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

#include <unordered_map>

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
//  Block cache + dispatcher state (Phase 4.3)
// --------------------------------------------------------------------------------------
// A compiled EE block: its host entry point and the (scaled) guest-cycle cost the
// dispatcher charges to cpuRegs.cycle after running it. This is the bring-up cache —
// a flat guest-PC -> block map, cleared wholesale on cache reset. The two-level
// recLUT + hardlinking optimisation is Phase 4.4.
struct EEBlock
{
	u8* entry;
	u32 cycles;
};
static std::unordered_map<u32, EEBlock> s_blocks;

// Hard cap on instructions per block, so straight-line code can't run the emit
// cursor away before we get a chance to reset. (x86 uses page/branch boundaries;
// this is a simpler bring-up bound.)
static constexpr u32 MAX_BLOCK_INSTS = 256;

// Execution / reset / exit plumbing, mirroring the x86 rec (iR5900.cpp).
static bool eeRecExecuting = false;
static bool eeRecNeedsReset = false;
static bool eeRecExitRequested = false;
static fastjmp_buf s_jmp_buf;

static void recResetRaw();

static void recReserve()
{
	recPtr = SysMemory::GetEERec();
	recPtrEnd = SysMemory::GetEERecEnd() - EE_CONSTPOOL_SIZE;

	s_const_pool.Init(recPtrEnd, EE_CONSTPOOL_SIZE);
}

static void recShutdown()
{
	s_const_pool.Destroy();
	s_blocks.clear();

	recPtr = nullptr;
	recPtrEnd = nullptr;
}

static void recResetRaw()
{
	// Rewind the emit cursor and drop the block cache + all cached trampolines/literals.
	recPtr = SysMemory::GetEERec();
	s_const_pool.Reset();
	s_blocks.clear();

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

		// COP1 (FPU). Only the bit-exact transfer/move ops are compiled (Phase
		// 5.2a); the sub-opcode is the rs field, S-format ops sub-decode on funct.
		// Float arithmetic / compares / BC1 branches return false and fall to the
		// interpreter (they need the EE's non-IEEE rounding behaviour). Operand
		// mapping per R5900OpcodeTables: ft=rt, fs=rd, fd=sa.
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
						case 0x05: armEmitABS_S(sa, rd); return true; // ABS_S (fd=sa, fs=rd)
						case 0x06: armEmitMOV_S(sa, rd); return true; // MOV_S
						case 0x07: armEmitNEG_S(sa, rd); return true; // NEG_S
						default:   return false;
					}
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
	const u32 rt = (op >> 16) & 0x1f;
	switch (opcode)
	{
		case 0x02: case 0x03: case 0x04: case 0x05: case 0x06: case 0x07:
			return true;
		case 0x00:
			return funct == 0x08 || funct == 0x09;
		case 0x01:
			return rt == 0x00 || rt == 0x01 || rt == 0x10 || rt == 0x11;
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

// --------------------------------------------------------------------------------------
//  Block compiler (Phase 4.3)
// --------------------------------------------------------------------------------------
// Compile a straight-line run starting at startpc into one host block:
//   - straight-line ops we can codegen are emitted inline;
//   - the run stops at the first control-flow op we have a generator for — that
//     branch + its delay slot are compiled and the block ends (the branch generator
//     wrote cpuRegs.pc);
//   - if an op we cannot start a block with is hit first, returns nullptr so the
//     dispatcher interprets it instead;
//   - otherwise the block ends at the next un-compilable op (or the length cap),
//     writing cpuRegs.pc to that address so the dispatcher resumes there.
// On success fills *out_cycles with the scaled guest-cycle cost. The host block is
// self-contained: prologue establishes RESTATEPTR(x19) via the enter trampoline (it
// is set by the caller of the block), and the epilogue is just RET.
static u8* recCompileBlock(u32 startpc, u32* out_cycles)
{
	// Whole-cache reset once we run past the code region into the constant-pool tail.
	if (recPtr >= recPtrEnd)
		recResetRaw();

	armSetAsmPtr(recPtr, recPtrEnd - recPtr, &s_const_pool);
	u8* const entry = armStartBlock();

	// Block prologue: preserve the caller's RESTATEPTR(x19) + LR (the body's vtlb /
	// interpreter calls clobber LR), and point RESTATEPTR at the guest cpuRegs file.
	// Matches the unit-test harness (RunEEGen). sp stays 16-byte aligned.
	armAsm->Stp(RESTATEPTR, a64::x30, a64::MemOperand(a64::sp, -16, a64::PreIndex));
	armMoveAddressToReg(RESTATEPTR, &cpuRegs);

	u32 pc = startpc;
	u32 raw_cycles = 0;
	u32 compiled = 0;
	bool ok = true;

	for (;;)
	{
		const u32 op = memRead32(pc);
		const R5900::OPCODE& info = R5900::GetInstruction(op);

		if (recIsHandledBranch(op))
		{
			// Terminate the block: branch generator + delay slot + exit.
			raw_cycles += info.cycles;
			recEmitBranch(op, pc); // writes cpuRegs.pc (taken/fallthrough/link)

			const u32 delay_op = memRead32(pc + 4);
			raw_cycles += R5900::GetInstruction(delay_op).cycles;
			recEmitOp(delay_op); // delay slot — must not write cpuRegs.pc
			break;
		}

		// Straight-line op we can codegen? (Generators decode from `op` directly;
		// they never read cpuRegs.code, so nothing to set here at compile time.)
		if (recTranslateOp(op))
		{
			raw_cycles += info.cycles;
			pc += 4;
			if (++compiled >= MAX_BLOCK_INSTS)
			{
				recEmitWritePc(pc); // resume at the next instruction
				break;
			}
			continue;
		}

		// Un-compilable, non-branch op (FPU/COP/MMI/syscall/likely-branch/...).
		if (compiled == 0)
		{
			// Nothing emitted yet — let the dispatcher interpret this single op.
			ok = false;
			break;
		}

		// End the block here; the dispatcher will interpret this op next.
		recEmitWritePc(pc);
		break;
	}

	if (!ok)
	{
		// First op isn't compilable — discard this (never-executed) block without
		// advancing recPtr, so the space is reused. Close the assembler cleanly; the
		// dispatcher will interpret the op instead.
		armAsm->Ldp(RESTATEPTR, a64::x30, a64::MemOperand(a64::sp, 16, a64::PostIndex));
		armAsm->Ret();
		armEndBlock();
		return nullptr;
	}

	// Block epilogue: restore the caller's RESTATEPTR(x19) + LR, then return.
	armAsm->Ldp(RESTATEPTR, a64::x30, a64::MemOperand(a64::sp, 16, a64::PostIndex));
	armAsm->Ret();
	recPtr = armEndBlock();

	*out_cycles = recScaleBlockCycles(raw_cycles);
	return entry;
}

// Look up (or compile) the block at `pc`. Returns nullptr if `pc` starts on an op
// the rec can't compile (the caller should interpret one instruction instead).
static const EEBlock* recGetBlock(u32 pc)
{
	const auto it = s_blocks.find(pc);
	if (it != s_blocks.end())
		return &it->second;

	u32 cycles = 0;
	u8* const entry = recCompileBlock(pc, &cycles);
	if (!entry)
		return nullptr;

	const auto ins = s_blocks.emplace(pc, EEBlock{entry, cycles});
	return &ins.first->second;
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

// The dispatcher loop. Reads cpuRegs.pc, runs (compiling if needed) the block there
// — or interprets one instruction when the block starts on an un-compilable op —
// then charges cycles and runs the EE event test. Exits via fastjmp on request.
static void recExecute()
{
	if (eeRecNeedsReset)
		recResetRaw();

	if (fastjmp_set(&s_jmp_buf) != 0)
	{
		eeRecExecuting = false;
		return;
	}

	eeRecExecuting = true;

	for (;;)
	{
		const EEBlock* const block = recGetBlock(cpuRegs.pc);
		if (block)
		{
			// The block establishes RESTATEPTR(x19) itself and returns via its saved
			// LR, so it can be entered with a plain indirect call.
			reinterpret_cast<void (*)()>(block->entry)();
			cpuRegs.cycle += block->cycles;
		}
		else
		{
			// Block starts on an op we can't compile yet — interpret exactly one
			// instruction (it handles its own delay slot / PC / cycles).
			intExecuteOneInst();
		}

		if (static_cast<s32>(cpuRegs.cycle - cpuRegs.nextEventCycle) >= 0)
			recEventTest();
	}
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
	// Bring-up: any code-cache clear (TLB remap, manual invalidation) drops the
	// whole block cache. Targeted invalidation + the recLUT land in Phase 4.4/4.5.
	if (eeRecExecuting)
	{
		eeRecNeedsReset = true;
		recSafeExitExecution();
		return;
	}
	recResetRaw();
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
