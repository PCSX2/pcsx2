// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "arm64/iR5900-arm64.h"
#include "common/Assertions.h"
#include "common/Console.h"

namespace a64 = vixl::aarch64;

namespace R5900 {
namespace Dynarec {

// Forward declarations for native COP2 codegen (defined in iCOP2-arm64.cpp)
namespace OpcodeImpl {
	// Transfer ops
	void recCOP2_QMFC2();
	void recCOP2_QMTC2();
	void recCOP2_CFC2();
	// SIMPLE
	void recCOP2_VMOVE();
	void recCOP2_VMR32();
	void recCOP2_VNOP();
	void recCOP2_VWAITQ();
	void recCOP2_VABS();
	// VEC_ARITH
	void recCOP2_VADD();
	void recCOP2_VSUB();
	void recCOP2_VMUL();
	void recCOP2_VMAX();
	void recCOP2_VMINI();
	// BC variants
	void recCOP2_VADDx();  void recCOP2_VADDy();  void recCOP2_VADDz();  void recCOP2_VADDw();
	void recCOP2_VSUBx();  void recCOP2_VSUBy();  void recCOP2_VSUBz();  void recCOP2_VSUBw();
	void recCOP2_VMULx();  void recCOP2_VMULy();  void recCOP2_VMULz();  void recCOP2_VMULw();
	void recCOP2_VMAXx();  void recCOP2_VMAXy();  void recCOP2_VMAXz();  void recCOP2_VMAXw();
	void recCOP2_VMINIx(); void recCOP2_VMINIy(); void recCOP2_VMINIz(); void recCOP2_VMINIw();
	void recCOP2_VMAXi();  void recCOP2_VMINIi();
	// Q/I variants
	void recCOP2_VADDq();  void recCOP2_VSUBq();  void recCOP2_VMULq();
	void recCOP2_VADDi();  void recCOP2_VSUBi();  void recCOP2_VMULi();
	// MADD/MSUB
	void recCOP2_VMADD();  void recCOP2_VMSUB();
	void recCOP2_VMADDx(); void recCOP2_VMADDy(); void recCOP2_VMADDz(); void recCOP2_VMADDw();
	void recCOP2_VMSUBx(); void recCOP2_VMSUBy(); void recCOP2_VMSUBz(); void recCOP2_VMSUBw();
	void recCOP2_VMADDq(); void recCOP2_VMSUBq();
	void recCOP2_VMADDi(); void recCOP2_VMSUBi();
	void recCOP2_VOPMSUB();
	// Accumulator
	void recCOP2_VADDA();  void recCOP2_VSUBA();  void recCOP2_VMULA();
	void recCOP2_VADDAx(); void recCOP2_VADDAy(); void recCOP2_VADDAz(); void recCOP2_VADDAw();
	void recCOP2_VSUBAx(); void recCOP2_VSUBAy(); void recCOP2_VSUBAz(); void recCOP2_VSUBAw();
	void recCOP2_VMULAx(); void recCOP2_VMULAy(); void recCOP2_VMULAz(); void recCOP2_VMULAw();
	void recCOP2_VMULAq(); void recCOP2_VMULAi();
	void recCOP2_VADDAq(); void recCOP2_VSUBAq();
	void recCOP2_VADDAi(); void recCOP2_VSUBAi();
	void recCOP2_VMADDA();  void recCOP2_VMSUBA();
	void recCOP2_VMADDAx(); void recCOP2_VMADDAy(); void recCOP2_VMADDAz(); void recCOP2_VMADDAw();
	void recCOP2_VMSUBAx(); void recCOP2_VMSUBAy(); void recCOP2_VMSUBAz(); void recCOP2_VMSUBAw();
	void recCOP2_VMADDAq(); void recCOP2_VMSUBAq();
	void recCOP2_VMADDAi(); void recCOP2_VMSUBAi();
	void recCOP2_VOPMULA();
	// Conversion
	void recCOP2_VITOF0();  void recCOP2_VITOF4();  void recCOP2_VITOF12(); void recCOP2_VITOF15();
	void recCOP2_VFTOI0();  void recCOP2_VFTOI4();  void recCOP2_VFTOI12(); void recCOP2_VFTOI15();
	// Integer ops
	void recCOP2_VIADD();  void recCOP2_VISUB();  void recCOP2_VIADDI();
	void recCOP2_VIAND();  void recCOP2_VIOR();
	// CTC2
	void recCOP2_CTC2();
	// Division ops
	void recCOP2_VDIV();
	void recCOP2_VSQRT();
	void recCOP2_VRSQRT();
	// Clip
	void recCOP2_VCLIP();
} // namespace OpcodeImpl

// Branch helper — not implemented on ARM64. Callers (iCOP0/iFPU/COP2 macro
// paths) drive SaveBranchState/SetBranchImm directly instead. Fail loudly
// rather than silently no-op if a future port wires this in by mistake.
void recDoBranchImm(u32 branchTo, u32* jmpSkip, bool isLikely, bool swappedDelaySlot)
{
	pxFailRel("recDoBranchImm is not implemented on ARM64");
}

namespace OpcodeImpl {

namespace Interp = R5900::Interpreter::OpcodeImpl;

void recPREF() {}

// SYSCALL and BREAK — flush state and call interpreter
void recSYSCALL()
{
	if (GPR_IS_CONST1(3))
	{
		// FlushCache (0x64) / iFlushCache (0x68): the EE cache is not modelled,
		// so account for the kernel handler cycles inline and skip the call.
		// Cycle count from github.com/F0bes/flushcache-cycles. Mirrors x86 recSYSCALL.
		//
		// This skip leaves v0/v1/at/t0/t1 and EPC at their pre-syscall values,
		// whereas the interpreter actually raises cpuException(0x20) and runs the
		// BIOS 0x80000180 trampoline, which clobbers them. That JIT-vs-interp
		// divergence is REAL but ABI-benign: FlushCache is a syscall, so under
		// the MIPS calling convention those are all caller-saved/temporary regs
		// (plus EPC, which user code never reads) — correct code never depends on
		// them surviving. Upstream PCSX2-x86 ships this skip as a correct, faster
		// optimization.
		const u8 syscallNum = g_cpuConstRegs[3].UC[0];
		if (syscallNum == 0x64 || syscallNum == 0x68)
		{
			s_nBlockCycles += 5650;
			return;
		}
	}
	recBranchCall(Interp::SYSCALL);
}

void recBREAK()
{
	recBranchCall(Interp::BREAK);
}

// =====================================================================================================
//  COP2 (VU0 macro mode) — dispatch table with per-sub-opcode interpreter fallback
//  Mirrors the x86 dispatch structure: recCOP2 → recCOP2t[_Rs_] → SPEC1/SPEC2
// =====================================================================================================

// COP2 macro-mode mVU-reuse wrapper. Drives the existing microVU emitter
// (mVU_<op> in microVU_Lower-arm64.inl) via the mVUmacroEmit_<op> adapter
// declared in iR5900-arm64.h. Mirrors x86 REC_COP2_mVU0 (microVU_Macro.inl:122).
// Mode bits per x86 microVU_Macro.inl:158-165:
//   0x01 reads Q reg / 0x02 writes Q reg / 0x04 requires analysis pass
//   0x08 writes CLIP / 0x10 writes status/mac / 0x100 requires x86 regs.
#define REC_COP2_mVU0_ARM64(name, mode) \
	static void recV##name() \
	{ \
		setupMacroOp_arm64(mode); \
		mVUmacroEmit_##name(mode); \
		endMacroOp_arm64(mode); \
	}

// Transfer ops — native codegen for QMFC2/QMTC2/CFC2, CTC2 stays interpreter
static void recVQMFC2() { recCOP2_QMFC2(); }
static void recVQMTC2() { recCOP2_QMTC2(); }
static void recVCFC2()  { recCOP2_CFC2(); }
static void recVCTC2()  { recCOP2_CTC2(); }

// Branch ops — native COP2 condition branch. CP2COND = bit 8 of
// VU0.VI[REG_VPU_STAT] (COP2.cpp:11). Mirrors x86 _setupBranchTest
// (microVU_Macro.inl) and the recBC1F FPU-branch shape: a lightweight
// _eeFlushAllDirty + a single Tbz/Tbnz on the flag bit, then the standard EE
// branch-imm machinery — avoiding FLUSH_INTERPRETER + C-call + dispatcher
// round-trip overhead.
static a64::Label* s_pBC2Label = nullptr;

static void recSetBranchCOP2(bool branchOnTrue)
{
	_eeFlushAllDirty();
	armAsm->Ldr(RWSCRATCH, armVU0Mem(&VU0.VI[REG_VPU_STAT]));

	// The forward branch skips the taken path: BC2T (branchOnTrue) is taken when
	// CP2COND is set → skip when clear → Tbz; BC2F is taken when clear → skip
	// when set → Tbnz. Matches x86 JZ32/JNZ32 in recBC2T/recBC2F.
	s_pBC2Label = new a64::Label();
	if (branchOnTrue)
		armAsm->Tbz(RWSCRATCH, 8, s_pBC2Label);
	else
		armAsm->Tbnz(RWSCRATCH, 8, s_pBC2Label);
}

static void recBindBC2Label()
{
	armAsm->Bind(s_pBC2Label);
	delete s_pBC2Label;
	s_pBC2Label = nullptr;
}

// Non-likely (BC2F/BC2T): attempt a delay-slot swap (allow_loadstore=false,
// matching x86 _setupBranchTest's TrySwapDelaySlot(0,0,0,false)).
static void recVBC2F()
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	const bool swap = TrySwapDelaySlot(0, 0, 0, false);
	recSetBranchCOP2(false);

	if (!swap)
	{
		SaveBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(branchTo);

	recBindBC2Label();

	if (!swap)
	{
		pc -= 4;
		LoadBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(pc);
}

static void recVBC2T()
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	const bool swap = TrySwapDelaySlot(0, 0, 0, false);
	recSetBranchCOP2(true);

	if (!swap)
	{
		SaveBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(branchTo);

	recBindBC2Label();

	if (!swap)
	{
		pc -= 4;
		LoadBranchState();
		recompileNextInstruction(true, false);
	}
	SetBranchImm(pc);
}

// Likely (BC2FL/BC2TL): delay slot squashed when not taken; no swap, matching
// the x86 isLikely path (and the interp's `else { cpuRegs.pc += 4; }`).
static void recVBC2FL()
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	recSetBranchCOP2(false);

	SaveBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);

	recBindBC2Label();
	LoadBranchState();
	SetBranchImm(pc);
}

static void recVBC2TL()
{
	u32 branchTo = ((s32)_Imm_ * 4) + pc;

	recSetBranchCOP2(true);

	SaveBranchState();
	recompileNextInstruction(true, false);
	SetBranchImm(branchTo);

	recBindBC2Label();
	LoadBranchState();
	SetBranchImm(pc);
}

// Upper instructions (SPEC1) — native NEON codegen
// BC variants: VF[fd] = VF[fs] OP VF[ft].bc
static void recVADDx()  { recCOP2_VADDx(); }  static void recVADDy()  { recCOP2_VADDy(); }
static void recVADDz()  { recCOP2_VADDz(); }  static void recVADDw()  { recCOP2_VADDw(); }
static void recVSUBx()  { recCOP2_VSUBx(); }  static void recVSUBy()  { recCOP2_VSUBy(); }
static void recVSUBz()  { recCOP2_VSUBz(); }  static void recVSUBw()  { recCOP2_VSUBw(); }
static void recVMADDx() { recCOP2_VMADDx(); }  static void recVMADDy() { recCOP2_VMADDy(); }
static void recVMADDz() { recCOP2_VMADDz(); }  static void recVMADDw() { recCOP2_VMADDw(); }
static void recVMSUBx() { recCOP2_VMSUBx(); }  static void recVMSUBy() { recCOP2_VMSUBy(); }
static void recVMSUBz() { recCOP2_VMSUBz(); }  static void recVMSUBw() { recCOP2_VMSUBw(); }
static void recVMAXx()  { recCOP2_VMAXx(); }   static void recVMAXy()  { recCOP2_VMAXy(); }
static void recVMAXz()  { recCOP2_VMAXz(); }   static void recVMAXw()  { recCOP2_VMAXw(); }
static void recVMINIx() { recCOP2_VMINIx(); }  static void recVMINIy() { recCOP2_VMINIy(); }
static void recVMINIz() { recCOP2_VMINIz(); }  static void recVMINIw() { recCOP2_VMINIw(); }
static void recVMULx()  { recCOP2_VMULx(); }   static void recVMULy()  { recCOP2_VMULy(); }
static void recVMULz()  { recCOP2_VMULz(); }   static void recVMULw()  { recCOP2_VMULw(); }
static void recVMULq()  { recCOP2_VMULq(); }   static void recVMAXi()  { recCOP2_VMAXi(); }
static void recVMULi()  { recCOP2_VMULi(); }   static void recVMINIi() { recCOP2_VMINIi(); }
static void recVADDq()  { recCOP2_VADDq(); }   static void recVMADDq() { recCOP2_VMADDq(); }
static void recVADDi()  { recCOP2_VADDi(); }   static void recVMADDi() { recCOP2_VMADDi(); }
static void recVSUBq()  { recCOP2_VSUBq(); }   static void recVMSUBq() { recCOP2_VMSUBq(); }
static void recVSUBi()  { recCOP2_VSUBi(); }   static void recVMSUBi() { recCOP2_VMSUBi(); }
static void recVADD()   { recCOP2_VADD(); }    static void recVMADD()  { recCOP2_VMADD(); }
static void recVMUL()   { recCOP2_VMUL(); }    static void recVMAX()   { recCOP2_VMAX(); }
static void recVSUB()   { recCOP2_VSUB(); }    static void recVMSUB()  { recCOP2_VMSUB(); }
static void recVOPMSUB(){ recCOP2_VOPMSUB(); } static void recVMINI()  { recCOP2_VMINI(); }
// Integer ops — native
static void recVIADD()  { recCOP2_VIADD(); }  static void recVISUB()  { recCOP2_VISUB(); }
static void recVIADDI() { recCOP2_VIADDI(); }
static void recVIAND()  { recCOP2_VIAND(); }  static void recVIOR()   { recCOP2_VIOR(); }
// CALLMS/CALLMSR kick off a VU0 microprogram via the interpreter — they are
// NOT EE branches (x86 iR5900Analysis case 56/57 just `break;`) so they
// must NOT exit the recompiled block the way recBranchCall does. Mirror
// x86's INTERPRETATE_COP2_FUNC(CALLMS) (microVU_Macro.inl:142): full
// FLUSH_INTERPRETER flush (so cpuRegs.code is current — VCALLMS reads
// the start PC from `(cpuRegs.code >> 6) & 0x7FFF`), apply pending block
// cycles, call the interpreter (which itself runs _vu0FinishMicro +
// vu0ExecMicro), then reload RECCYCLE in case the interp advanced
// cpuRegs.cycle. Block execution continues at the next opcode.
// Using iFlushCall(FLUSH_INTERPRETER) inline avoids the g_branch=2 block
// exit that recBranchCall would trigger on every CALLMS; the flush cost
// is the same, with no dispatcher round-trip.
static void recVCallmsImpl(void (*func)())
{
	iFlushCall(FLUSH_INTERPRETER);

	u32 cycles = scaleblockcycles_clear();
	if (cycles != 0)
		armAsm->Add(RECCYCLE, RECCYCLE, cycles);

	armAsm->Str(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));
	armEmitCall((void*)func);
	armAsm->Ldr(RECCYCLE, armCpuRegMem(&cpuRegs.cycle));
}

static void recVCALLMS()  { recVCallmsImpl(VCALLMS); }
static void recVCALLMSR() { recVCallmsImpl(VCALLMSR); }

// Lower instructions (SPEC2) — native NEON codegen for accumulator/conversion/simple ops
// Accumulator BC variants
static void recVADDAx() { recCOP2_VADDAx(); }  static void recVADDAy() { recCOP2_VADDAy(); }
static void recVADDAz() { recCOP2_VADDAz(); }  static void recVADDAw() { recCOP2_VADDAw(); }
static void recVSUBAx() { recCOP2_VSUBAx(); }  static void recVSUBAy() { recCOP2_VSUBAy(); }
static void recVSUBAz() { recCOP2_VSUBAz(); }  static void recVSUBAw() { recCOP2_VSUBAw(); }
static void recVMADDAx(){ recCOP2_VMADDAx(); }  static void recVMADDAy(){ recCOP2_VMADDAy(); }
static void recVMADDAz(){ recCOP2_VMADDAz(); }  static void recVMADDAw(){ recCOP2_VMADDAw(); }
static void recVMSUBAx(){ recCOP2_VMSUBAx(); }  static void recVMSUBAy(){ recCOP2_VMSUBAy(); }
static void recVMSUBAz(){ recCOP2_VMSUBAz(); }  static void recVMSUBAw(){ recCOP2_VMSUBAw(); }
// Conversions — native NEON
static void recVITOF0()  { recCOP2_VITOF0(); }  static void recVITOF4()  { recCOP2_VITOF4(); }
static void recVITOF12() { recCOP2_VITOF12(); } static void recVITOF15() { recCOP2_VITOF15(); }
static void recVFTOI0()  { recCOP2_VFTOI0(); }  static void recVFTOI4()  { recCOP2_VFTOI4(); }
static void recVFTOI12() { recCOP2_VFTOI12(); } static void recVFTOI15() { recCOP2_VFTOI15(); }
// Accumulator MULAx/y/z/w
static void recVMULAx() { recCOP2_VMULAx(); }  static void recVMULAy() { recCOP2_VMULAy(); }
static void recVMULAz() { recCOP2_VMULAz(); }  static void recVMULAw() { recCOP2_VMULAw(); }
static void recVMULAq() { recCOP2_VMULAq(); }  static void recVABS()   { recCOP2_VABS(); }
static void recVMULAi() { recCOP2_VMULAi(); }
// CLIP — still interpreter fallback (complex flag logic)
static void recVCLIP() { recCOP2_VCLIP(); }
// Accumulator Q/I variants
static void recVADDAq() { recCOP2_VADDAq(); }  static void recVMADDAq(){ recCOP2_VMADDAq(); }
static void recVADDAi() { recCOP2_VADDAi(); }  static void recVMADDAi(){ recCOP2_VMADDAi(); }
static void recVSUBAq() { recCOP2_VSUBAq(); }  static void recVMSUBAq(){ recCOP2_VMSUBAq(); }
static void recVSUBAi() { recCOP2_VSUBAi(); }  static void recVMSUBAi(){ recCOP2_VMSUBAi(); }
// Accumulator full-vector variants
static void recVADDA()  { recCOP2_VADDA(); }   static void recVMADDA() { recCOP2_VMADDA(); }
static void recVMULA()  { recCOP2_VMULA(); }
static void recVSUBA()  { recCOP2_VSUBA(); }   static void recVMSUBA() { recCOP2_VMSUBA(); }
static void recVOPMULA(){ recCOP2_VOPMULA(); }  static void recVNOP()   { recCOP2_VNOP(); }
// Simple data movement — native
static void recVMOVE() { recCOP2_VMOVE(); }    static void recVMR32() { recCOP2_VMR32(); }
// Load/store — full group native via mVU emit (mode bits from x86 microVU_Macro.inl:276-279).
REC_COP2_mVU0_ARM64(LQI, 0x104);   REC_COP2_mVU0_ARM64(SQI, 0x100);
REC_COP2_mVU0_ARM64(LQD, 0x104);   REC_COP2_mVU0_ARM64(SQD, 0x100);
// Division ops — native
static void recVDIV()  { recCOP2_VDIV(); }
static void recVSQRT() { recCOP2_VSQRT(); }
static void recVRSQRT(){ recCOP2_VRSQRT(); }
static void recVWAITQ() { recCOP2_VWAITQ(); }
REC_COP2_mVU0_ARM64(MTIR, 0x104);  REC_COP2_mVU0_ARM64(MFIR, 0x104);
REC_COP2_mVU0_ARM64(ILWR, 0x104);  REC_COP2_mVU0_ARM64(ISWR, 0x100);
REC_COP2_mVU0_ARM64(RNEXT, 0x104); REC_COP2_mVU0_ARM64(RGET,  0x104);
REC_COP2_mVU0_ARM64(RINIT, 0x100); REC_COP2_mVU0_ARM64(RXOR,  0x100);

static void rec_C2UNK() { Console.Error("EE: Unrecognized COP2 opcode %08X", cpuRegs.code); }

// Dispatch tables — mirror x86 structure
static void recCOP2_BC2();
static void recCOP2_SPEC1();
static void recCOP2_SPEC2();

static void (*recCOP2t[32])() = {
	rec_C2UNK,      recVQMFC2,       recVCFC2,        rec_C2UNK,       rec_C2UNK,       recVQMTC2,       recVCTC2,        rec_C2UNK,
	recCOP2_BC2,    rec_C2UNK,       rec_C2UNK,       rec_C2UNK,       rec_C2UNK,       rec_C2UNK,       rec_C2UNK,       rec_C2UNK,
	recCOP2_SPEC1,  recCOP2_SPEC1,   recCOP2_SPEC1,   recCOP2_SPEC1,   recCOP2_SPEC1,   recCOP2_SPEC1,   recCOP2_SPEC1,   recCOP2_SPEC1,
	recCOP2_SPEC1,  recCOP2_SPEC1,   recCOP2_SPEC1,   recCOP2_SPEC1,   recCOP2_SPEC1,   recCOP2_SPEC1,   recCOP2_SPEC1,   recCOP2_SPEC1,
};

static void (*recCOP2_BC2t[32])() = {
	recVBC2F,  recVBC2T,  recVBC2FL, recVBC2TL, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK,
	rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK,
	rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK,
	rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK, rec_C2UNK,
};

static void (*recCOP2SPECIAL1t[64])() = {
	recVADDx,   recVADDy,   recVADDz,  recVADDw,  recVSUBx,      recVSUBy,      recVSUBz,      recVSUBw,
	recVMADDx,  recVMADDy,  recVMADDz, recVMADDw, recVMSUBx,     recVMSUBy,     recVMSUBz,     recVMSUBw,
	recVMAXx,   recVMAXy,   recVMAXz,  recVMAXw,  recVMINIx,     recVMINIy,     recVMINIz,     recVMINIw,
	recVMULx,   recVMULy,   recVMULz,  recVMULw,  recVMULq,      recVMAXi,      recVMULi,      recVMINIi,
	recVADDq,   recVMADDq,  recVADDi,  recVMADDi, recVSUBq,      recVMSUBq,     recVSUBi,      recVMSUBi,
	recVADD,    recVMADD,   recVMUL,   recVMAX,   recVSUB,       recVMSUB,      recVOPMSUB,    recVMINI,
	recVIADD,   recVISUB,   recVIADDI, rec_C2UNK, recVIAND,      recVIOR,       rec_C2UNK,     rec_C2UNK,
	recVCALLMS, recVCALLMSR,rec_C2UNK, rec_C2UNK, recCOP2_SPEC2, recCOP2_SPEC2, recCOP2_SPEC2, recCOP2_SPEC2,
};

static void (*recCOP2SPECIAL2t[128])() = {
	recVADDAx,  recVADDAy, recVADDAz,  recVADDAw,  recVSUBAx,  recVSUBAy,  recVSUBAz,  recVSUBAw,
	recVMADDAx,recVMADDAy, recVMADDAz, recVMADDAw, recVMSUBAx, recVMSUBAy, recVMSUBAz, recVMSUBAw,
	recVITOF0,  recVITOF4, recVITOF12, recVITOF15, recVFTOI0,  recVFTOI4,  recVFTOI12, recVFTOI15,
	recVMULAx,  recVMULAy, recVMULAz,  recVMULAw,  recVMULAq,  recVABS,    recVMULAi,  recVCLIP,
	recVADDAq,  recVMADDAq,recVADDAi,  recVMADDAi, recVSUBAq,  recVMSUBAq, recVSUBAi,  recVMSUBAi,
	recVADDA,   recVMADDA, recVMULA,   rec_C2UNK,  recVSUBA,   recVMSUBA,  recVOPMULA, recVNOP,
	recVMOVE,   recVMR32,  rec_C2UNK,  rec_C2UNK,  recVLQI,    recVSQI,    recVLQD,    recVSQD,
	recVDIV,    recVSQRT,  recVRSQRT,  recVWAITQ,  recVMTIR,   recVMFIR,   recVILWR,   recVISWR,
	recVRNEXT,  recVRGET,  recVRINIT,  recVRXOR,   rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,
	rec_C2UNK,  rec_C2UNK, rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,
	rec_C2UNK,  rec_C2UNK, rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,
	rec_C2UNK,  rec_C2UNK, rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,
	rec_C2UNK,  rec_C2UNK, rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,
	rec_C2UNK,  rec_C2UNK, rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,
	rec_C2UNK,  rec_C2UNK, rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,
	rec_C2UNK,  rec_C2UNK, rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,  rec_C2UNK,
};

static void recCOP2_BC2()  { recCOP2_BC2t[_Rt_](); }
static void recCOP2_SPEC1() { recCOP2SPECIAL1t[cpuRegs.code & 0x3f](); }
static void recCOP2_SPEC2() { recCOP2SPECIAL2t[(cpuRegs.code & 0x3) | ((cpuRegs.code >> 4) & 0x7c)](); }

void recCOP2()
{
#ifdef FORCE_INTERP_COP2
	// Use interpreter for all COP2 — but branches need recBranchCall
	if (_Rs_ == 8) // BC2 branch instructions
		recBranchCall(Interp::COP2);
	else
		recCall(Interp::COP2);
#else
	recCOP2t[_Rs_]();
#endif
}

void recSYNC() {}

// MFSA — rd = sa (shift amount register)
void recMFSA()
{
	if (!_Rd_) return;
	_deleteEEreg(_Rd_, 0);
	GPR_DEL_CONST(_Rd_);
	armLoadEERegPtr(RWSCRATCH, &cpuRegs.sa);
	armAsm->Mov(RXSCRATCH, RWSCRATCH); // zero-extend to 64
	armStoreEERegPtr(RXSCRATCH, &cpuRegs.GPR.r[_Rd_].UD[0]);
}

// MTSA — sa = rs
void recMTSA()
{
	if (GPR_IS_CONST1(_Rs_))
	{
		armAsm->Mov(RWSCRATCH, g_cpuConstRegs[_Rs_].UL[0]);
		armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.sa));
	}
	else
	{
		_deleteEEreg(_Rs_, 1);
		armLoadEERegPtr(RWSCRATCH, &cpuRegs.GPR.r[_Rs_].UL[0]);
		armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.sa));
	}
}

// MTSAB — sa = (rs[3:0] ^ imm[3:0])
void recMTSAB()
{
	if (GPR_IS_CONST1(_Rs_))
	{
		u32 val = (g_cpuConstRegs[_Rs_].UL[0] & 0xF) ^ (_Imm_ & 0xF);
		armAsm->Mov(RWSCRATCH, val);
		armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.sa));
	}
	else
	{
		_deleteEEreg(_Rs_, 1);
		armLoadEERegPtr(RWSCRATCH, &cpuRegs.GPR.r[_Rs_].UL[0]);
		armAsm->And(RWSCRATCH, RWSCRATCH, 0xF);
		armAsm->Eor(RWSCRATCH, RWSCRATCH, _Imm_ & 0xF);
		armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.sa));
	}
}

// MTSAH — sa = ((rs[2:0] ^ imm[2:0]) << 1)
void recMTSAH()
{
	if (GPR_IS_CONST1(_Rs_))
	{
		u32 val = ((g_cpuConstRegs[_Rs_].UL[0] & 0x7) ^ (_Imm_ & 0x7)) << 1;
		armAsm->Mov(RWSCRATCH, val);
		armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.sa));
	}
	else
	{
		_deleteEEreg(_Rs_, 1);
		armLoadEERegPtr(RWSCRATCH, &cpuRegs.GPR.r[_Rs_].UL[0]);
		armAsm->Eor(RWSCRATCH, RWSCRATCH, _Imm_ & 0x7);
		// ubfiz w, w, #1, #3 extracts bits[2:0] and places them at bit 1
		armAsm->Ubfiz(RWSCRATCH, RWSCRATCH, 1, 3);
		armAsm->Str(RWSCRATCH, armCpuRegMem(&cpuRegs.sa));
	}
}

void recNULL()
{
	Console.Error("EE: Unimplemented op %x", cpuRegs.code);
}

void recUnknown()
{
	Console.Error("EE: Unrecognized op %x", cpuRegs.code);
}

void recMMI_Unknown()
{
	Console.Error("EE: Unrecognized MMI op %x", cpuRegs.code);
}

void recCOP0_Unknown()
{
	Console.Error("EE: Unrecognized COP0 op %x", cpuRegs.code);
}

void recCOP1_Unknown()
{
	Console.Error("EE: Unrecognized FPU/COP1 op %x", cpuRegs.code);
}

void recCACHE() {}

REC_SYS(TGE);
REC_SYS(TGEU);
REC_SYS(TLT);
REC_SYS(TLTU);
REC_SYS(TEQ);
REC_SYS(TNE);
REC_SYS(TGEI);
REC_SYS(TGEIU);
REC_SYS(TLTI);
REC_SYS(TLTIU);
REC_SYS(TEQI);
REC_SYS(TNEI);

} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900

// recBackpropBSC is provided by the shared x86/iR5900Analysis.cpp
// (compiled for ARM64 via ARCH_ARM64 conditional include).

