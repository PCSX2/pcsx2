// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Config.h"
#include "R5900.h"
#include "R5900OpcodeTables.h"
#include "VU.h"
#include "arm64/iCore-arm64.h"

// Per-category interpreter fallback toggles.
// Comment out a define to enable native ARM64 codegen for that category.
// #define FORCE_INTERP_BRANCH 1
// #define FORCE_INTERP_JUMP 1
// #define FORCE_INTERP_MOVE 1
// #define FORCE_INTERP_SHIFT 1
// #define FORCE_INTERP_ALU 1
// #define FORCE_INTERP_ARITIMM 1
// #define FORCE_INTERP_MULTDIV 1
// #define FORCE_INTERP_MEMORY 1
// #define FORCE_INTERP_COP0 1
// #define FORCE_INTERP_FPU 1
// #define FORCE_INTERP_COP2 1

// Reserved ARM64 registers for the recompiler
// x19: Fastmem base pointer (callee-saved)
#define RFASTMEMBASE vixl::aarch64::x19
// x20: Pointer to cpuRegs struct (callee-saved). Loaded once at JIT entry by
// EnterRecompiledCode and never modified for the duration of JIT execution.
// Use armCpuRegMem() to construct cpuRegs-relative MemOperands cheaply.
#define RSTATE vixl::aarch64::x20
// x24: Pinned &VU0 (callee-saved). Loaded once at JIT entry by
// EnterRecompiledCode; used to address VU0.{VF,VI,ACC,q,statusflag,...}
// fields via single [RVU0, #imm12] load/store in iCOP2-arm64.cpp,
// instead of the 3-mov+ldr abs-addr materialization sequence.
// Survives armEmitCall (callee-saved per AAPCS) and the mVU dispatcher's
// outer Stp/Ldp pair, so cross-EE/mVU dispatches preserve it.
#define RVU0 vixl::aarch64::x24
// x25: Pinned cpuRegs.cycle (callee-saved). Always valid while in JIT;
// flushed to memory only around C calls (DispatcherEvent, JITCompile,
// recBranchCall). Block-to-block control flow keeps it live, so linked
// branches don't reload the cycle counter from memory each iteration.
#define RECCYCLE vixl::aarch64::x25

// Build a MemOperand addressing a cpuRegs field via RSTATE.
// Replaces the 3-instruction `armMoveAddressToReg(RSCRATCHADDR, &cpuRegs.X);
// Ldr/Str ..., [RSCRATCHADDR]` pattern with a single Ldr/Str using a
// signed/unsigned-immediate offset on RSTATE. ARM64 LDR with imm12 covers
// offsets up to 32760 bytes (64-bit) / 16380 bytes (32-bit) — easily larger
// than cpuRegs / fpuRegs combined, so a single instruction suffices for
// every reachable field.
static __fi vixl::aarch64::MemOperand armCpuRegMem(const void* field)
{
	const ptrdiff_t off = reinterpret_cast<const u8*>(field) - reinterpret_cast<const u8*>(&cpuRegs);
	return vixl::aarch64::MemOperand(RSTATE, static_cast<int64_t>(off));
}

// Pinned-base load/store helpers: when the target is anywhere inside
// _cpuRegistersPack (cpuRegs + fpuRegs), reach it via [RSTATE, #off] in one
// instruction; otherwise fall back to the generic 4-inst armLoadPtr/StorePtr.
static __fi bool armIsCpuRegPtr(const void* field)
{
	const u8* base = reinterpret_cast<const u8*>(&_cpuRegistersPack);
	const u8* p    = reinterpret_cast<const u8*>(field);
	return p >= base && p < base + sizeof(cpuRegistersPack);
}
static __fi void armLoadEERegPtr(const vixl::aarch64::CPURegister& reg, const void* field)
{
	if (armIsCpuRegPtr(field))
		armAsm->Ldr(reg, armCpuRegMem(field));
	else
		armLoadPtr(reg, field);
}
static __fi void armStoreEERegPtr(const vixl::aarch64::CPURegister& reg, const void* field)
{
	if (armIsCpuRegPtr(field))
		armAsm->Str(reg, armCpuRegMem(field));
	else
		armStorePtr(reg, field);
}

// Build a MemOperand addressing a VU0 field via RVU0. VURegs is < 2 KB, so
// every reachable field fits in imm12 for byte/halfword/word/doubleword/quad
// ldr/str. Mirrors armCpuRegMem for VU0 — used by iCOP2-arm64.cpp.
static __fi vixl::aarch64::MemOperand armVU0Mem(const void* field)
{
	const ptrdiff_t off = reinterpret_cast<const u8*>(field) - reinterpret_cast<const u8*>(&VU0);
	return vixl::aarch64::MemOperand(RVU0, static_cast<int64_t>(off));
}

// Emit LD1R from a VU0 field, broadcasting to all lanes. ARM64 LD1R does
// NOT support [base, #imm] addressing — only [base] or post-index. vixl's
// LoadStoreStructAddrModeField silently drops the offset (and the assert
// is gated on VIXL_DEBUG, so Devel builds ship the wrong encoding instead
// of trapping). Materialize the address with a single ADD imm12 instead of
// 3-mov: VURegs fields fit within 4 KB of &VU0, so one ADD suffices.
// Total cost: ADD + LD1R = 2 insns, vs the original 4-insn 3-mov + LD1R.
static __fi void armLd1rVU0(const vixl::aarch64::VRegister& vt, const void* field)
{
	const ptrdiff_t off = reinterpret_cast<const u8*>(field) - reinterpret_cast<const u8*>(&VU0);
	armAsm->Add(RSCRATCHADDR, RVU0, static_cast<int64_t>(off));
	armAsm->Ld1r(vt, vixl::aarch64::MemOperand(RSCRATCHADDR));
}

extern u32 maxrecmem;
extern u32 pc;             // recompiler pc
extern int g_branch;       // set for branch
extern u32 target;         // branch target
extern u32 s_nBlockCycles; // cycles of current block recompiling
extern bool s_nBlockInterlocked; // Current block has VU0 interlocking

//////////////////////////////////////////////////////////////////////////////////////////
// Interpreter fallback macros

#define REC_FUNC(f) \
	void rec##f() \
	{ \
		/* Delete destination register's const/alloc state before interpreter call. \
		 * The interpreter writes directly to cpuRegs, making any cached const or \
		 * allocated register stale. SPECIAL ops (Rd), loads/COP (Rt). */ \
		const u32 _op = cpuRegs.code >> 26; \
		const int _dest = (_op == 0 || _op == 0x1C) ? _Rd_ : _Rt_; \
		if (_dest > 0) \
			_deleteEEreg(_dest, 1); \
		recCall(Interp::f); \
	}

#define REC_FUNC_DEL(f, delreg) \
	void rec##f() \
	{ \
		if ((delreg) > 0) \
			_deleteEEreg(delreg, 1); \
		recCall(Interp::f); \
	}

#define REC_SYS(f) \
	void rec##f() \
	{ \
		recBranchCall(Interp::f); \
	}

#define REC_SYS_DEL(f, delreg) \
	void rec##f() \
	{ \
		if ((delreg) > 0) \
			_deleteEEreg(delreg, 1); \
		recBranchCall(Interp::f); \
	}

extern bool g_recompilingDelaySlot;

// Used for generating backpatch thunks for fastmem
u8* recBeginThunk();
u8* recEndThunk();

// Branch processing
bool TrySwapDelaySlot(u32 rs, u32 rt, u32 rd, bool allow_loadstore);
void SaveBranchState();
void LoadBranchState();

void recompileNextInstruction(bool delayslot, bool swapped_delay_slot);
void SetBranchReg();
void SetBranchImm(u32 imm);

void iFlushCall(int flushtype);
void recBranchCall(void (*func)());
void recCall(void (*func)());
// Emit the post-interpreter-call TLB-miss exception dispatch (defined in
// iR5900-arm64.cpp). DispatcherReg/s_recTlbMissOccurred are file-local there,
// so cross-TU interpreter-call sites (recVTLB-arm64.cpp) route through this.
void recEmitInterpTlbMissCheck();
u32 scaleblockcycles_clear();

// COP2 / VU0 sync emit helper (defined in iCOP2-arm64.cpp).
// interlock=true mirrors x86 COP2_Interlock (CFC2/CTC2/QMFC2/QMTC2 path);
// interlock=false mirrors mVUSyncVU0 / mVUFinishVU0 gating used by LQC2/SQC2
// and the COP2 macro-arithmetic setup. finishFunc is the secondary helper
// to invoke after vu0Sync (typically _vu0FinishMicro or _vu0WaitMicro);
// pass nullptr for "sync only". Emits zero instructions when EEINST analysis
// flags say no sync is needed.
void cop2EmitConditionalSync(bool interlock, void (*finishFunc)());

// COP2 macro-mode microVU0 state setup/teardown (defined in microVU-arm64.cpp).
// Mirrors x86 setupMacroOp/endMacroOp's regAlloc reset, microVU0.cop2 = 1,
// prog.IRinfo.curPC/info[0] init, code = cpuRegs.code, and flag scaffolding.
// Required before invoking any mVU emitter (mVU_LQI/SQI/MFIR/...) from a
// COP2 macro-mode dispatch wrapper. eeinstInfo is g_pCurInstInfo->info (or 0
// when EEINST analysis isn't live for this site).
void mVUmacroSetupCOP2State(int mode, u32 eeinstInfo);
void mVUmacroEndCOP2State();

// COP2 macro-mode setup/teardown wrapper (defined in iCOP2-arm64.cpp).
// Calls cop2EmitConditionalSync, emits status-flag denormalize/normalize when
// mode & 0x10, then runs mVUmacroSetup/EndCOP2State to ready microVU0 for the
// mVU emitter pass. REC_COP2_mVU0_ARM64-style wrappers in iR5900Misc-arm64.cpp
// bracket calls to mVUmacroEmit_<op> with these.
void setupMacroOp_arm64(int mode);
void endMacroOp_arm64(int mode);

// COP2 macro-mode emit adapters (defined in microVU-arm64.cpp). Each runs the
// pass1+pass2 dispatch x86 uses in REC_COP2_mVU0 (microVU_Macro.inl:127-133).
// mode is the same mode bits passed to setupMacroOp_arm64; only bit 0x04
// (requires analysis pass) is observed by the adapter.
void mVUmacroEmit_LQI(int mode);
void mVUmacroEmit_SQI(int mode);
void mVUmacroEmit_LQD(int mode);
void mVUmacroEmit_SQD(int mode);
void mVUmacroEmit_MTIR(int mode);
void mVUmacroEmit_MFIR(int mode);
void mVUmacroEmit_ILWR(int mode);
void mVUmacroEmit_ISWR(int mode);
void mVUmacroEmit_RNEXT(int mode);
void mVUmacroEmit_RGET(int mode);
void mVUmacroEmit_RINIT(int mode);
void mVUmacroEmit_RXOR(int mode);

namespace R5900
{
	namespace Dynarec
	{
		extern void recDoBranchImm(u32 branchTo, u32* jmpSkip, bool isLikely = false, bool swappedDelaySlot = false);
	}
}

////////////////////////////////////////////////////////////////////
// Constant Propagation

#define GPR_IS_CONST1(reg) (EE_CONST_PROP && (reg) < 32 && (g_cpuHasConstReg & (1 << (reg))))
#define GPR_IS_CONST2(reg1, reg2) (EE_CONST_PROP && (g_cpuHasConstReg & (1 << (reg1))) && (g_cpuHasConstReg & (1 << (reg2))))
#define GPR_IS_DIRTY_CONST(reg) (EE_CONST_PROP && (reg) < 32 && (g_cpuHasConstReg & (1 << (reg))) && (!(g_cpuFlushedConstReg & (1 << (reg)))))
#define GPR_SET_CONST(reg) \
	{ \
		if ((reg) < 32) \
		{ \
			g_cpuHasConstReg |= (1 << (reg)); \
			g_cpuFlushedConstReg &= ~(1 << (reg)); \
		} \
	}

#define GPR_DEL_CONST(reg) \
	{ \
		if ((reg) < 32) \
			g_cpuHasConstReg &= ~(1 << (reg)); \
	}

alignas(16) extern GPR_reg64 g_cpuConstRegs[32];
extern u32 g_cpuHasConstReg, g_cpuFlushedConstReg;

// Move guest GPR value to an ARM64 register
void _eeMoveGPRtoR(const vixl::aarch64::Register& to, int fromgpr, bool allow_preload = true);

void _eeFlushAllDirty();
void _eeOnWriteReg(int reg, int signext);

// Totally deletes from const, NEON, and GPR entries
// if flush is 1, also flushes to memory
void _deleteEEreg(int reg, int flush);
void _deleteEEreg128(int reg);

void _flushEEreg(int reg, bool clear = false);

//////////////////////////////////////
// Templates for code recompilation //
//////////////////////////////////////

typedef void (*R5900FNPTR)();
typedef void (*R5900FNPTR_INFO)(int info);

// Memory-based templates — no register allocation, all operands via cpuRegs memory.
void eeRecompileCodeRC0_MEM(R5900FNPTR constcode, R5900FNPTR_INFO constscode, R5900FNPTR_INFO consttcode, R5900FNPTR_INFO noconstcode, int xmminfo);
void eeRecompileCodeRC1_MEM(R5900FNPTR constcode, R5900FNPTR_INFO noconstcode, int xmminfo);
void eeRecompileCodeRC2_MEM(R5900FNPTR constcode, R5900FNPTR_INFO noconstcode, int xmminfo);

#define EERECOMPILE_CODERC0_MEM(fn, xmminfo) \
	void rec##fn(void) \
	{ \
		eeRecompileCodeRC0_MEM(rec##fn##_const, rec##fn##_consts, rec##fn##_constt, rec##fn##_, (xmminfo)); \
	}

#define EERECOMPILE_CODEX_MEM(codename, fn, xmminfo) \
	void rec##fn(void) \
	{ \
		codename(rec##fn##_const, rec##fn##_, (xmminfo)); \
	}

#define FPURECOMPILE_CONSTCODE(fn, xmminfo) \
	void rec##fn(void) \
	{ \
		if (CHECK_FPU_FULL) \
			eeFPURecompileCode(DOUBLE::rec##fn##_xmm, R5900::Interpreter::OpcodeImpl::COP1::fn, xmminfo); \
		else \
			eeFPURecompileCode(rec##fn##_xmm, R5900::Interpreter::OpcodeImpl::COP1::fn, xmminfo); \
	}

int eeRecompileCodeXMM(int xmminfo);
void eeFPURecompileCode(R5900FNPTR_INFO xmmcode, R5900FNPTR fpucode, int xmminfo);
