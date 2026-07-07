// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

// ARM64 EE MMI (Multimedia Instructions) Codegen — NEON-based
//
// All MMI instructions are 128-bit SIMD operations on the EE's 128-bit GPRs.
// NEON Q registers are used throughout: load from cpuRegs.GPR, operate, store back.

#include "arm64/iR5900-arm64.h"
#include "arm64/AsmHelpers.h"
#include "common/Assertions.h"

namespace a64 = vixl::aarch64;

namespace R5900 {
namespace Dynarec {
namespace OpcodeImpl {
namespace MMI {

namespace Interp = R5900::Interpreter::OpcodeImpl::MMI;

// ============================================================================
//  Helpers for 128-bit GPR load/store
// ============================================================================

// Flush any const propagation state for a register and invalidate allocations.
// Must be called before reading a register's 128-bit value from memory,
// since const prop only tracks the lower 64 bits.
static void mmiFlushReg(int reg)
{
	if (reg == 0) return;
	if (GPR_IS_CONST1(reg))
	{
		// Const prop only has lower 64 bits — flush to memory so upper 64 bits
		// are preserved alongside the correct lower 64 bits.
		_flushEEreg(reg);
	}
	_deleteEEreg(reg, 1);
}

// Prepare destination: invalidate const/alloc state (the full 128 bits will be overwritten)
static void mmiInvalidateDest(int reg)
{
	if (reg == 0) return;
	_deleteEEreg(reg, 0);
	GPR_DEL_CONST(reg);
}

// Load 128-bit GPR into a NEON Q register
static void mmiLoadReg(const a64::VRegister& qreg, int gpr)
{
	if (gpr == 0)
	{
		// r0 is always zero
		armAsm->Movi(qreg.V16B(), 0);
	}
	else
	{
		armAsm->Ldr(qreg, armCpuRegMem(&cpuRegs.GPR.r[gpr].UQ));
		// Lazy-dirty: merge the pin over the possibly-stale lower half.
		armMergeEEPinIntoQuad(qreg, gpr);
	}
}

// Store 128-bit NEON Q register to GPR
static void mmiStoreReg(int gpr, const a64::VRegister& qreg)
{
	pxAssert(gpr != 0);
	armStoreEEGPRQuad(qreg, gpr);
}

// Standard 3-operand MMI: rd = rs OP rt (128-bit).
//
// Routes through eeRecompileCodeXMM so consecutive MMI ops on the same guest
// register stay register-resident in the allocator-managed NEON pool instead
// of bouncing through memory. Allocator handles const tracking, GPR-side
// eviction, NaN-zero of r0, and "already in NEON" reuse.
//
// Each user gets three locals:
//   qs — VRegister view of EEREC_S (Rs input, MODE_READ)
//   qt — VRegister view of EEREC_T (Rt input, MODE_READ)
//   qd — VRegister view of EEREC_D (Rd output, MODE_WRITE)
//
// Functions that need a fourth temp can use RQSCRATCH / RQSCRATCH2 — both are
// outside the allocator pool. Do NOT clobber qs or qt
// before the final write to qd, otherwise the allocator's MODE_READ state
// for them is invalidated.
#define MMI_3OP_SETUP() \
	if (!_Rd_) return; \
	int info = eeRecompileCodeXMM(XMMINFO_READS | XMMINFO_READT | XMMINFO_WRITED); \
	const a64::VRegister qs = armQRegister(EEREC_S); \
	const a64::VRegister qt = armQRegister(EEREC_T); \
	const a64::VRegister qd = armQRegister(EEREC_D); \
	(void)info

// 2-operand: rd = OP(rt).
//   qt — VRegister view of EEREC_T (Rt input, MODE_READ)
//   qd — VRegister view of EEREC_D (Rd output, MODE_WRITE)
#define MMI_2OP_SETUP() \
	if (!_Rd_) return; \
	int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED); \
	const a64::VRegister qt = armQRegister(EEREC_T); \
	const a64::VRegister qd = armQRegister(EEREC_D); \
	(void)info

// ============================================================================
//  Logical Operations (128-bit)
// ============================================================================

void recPAND()
{
	MMI_3OP_SETUP();
	armAsm->And(qd.V16B(), qs.V16B(), qt.V16B());
}

void recPOR()
{
	if (!_Rd_)
		return;

	// `por rd, r0, rt` is the canonical PS2 128-bit register-move idiom and is
	// common. Special-case an r0 operand to avoid allocating r0 into a NEON reg
	// and materialize a zero just to OR it in (conditional XMMINFO,
	// Movi when both r0, register-copy when one is r0).
	const bool s_zero = (_Rs_ == 0);
	const bool t_zero = (_Rt_ == 0);
	int info = eeRecompileCodeXMM((s_zero ? 0 : XMMINFO_READS) | (t_zero ? 0 : XMMINFO_READT) | XMMINFO_WRITED);
	const a64::VRegister qd = armQRegister(EEREC_D);

	if (s_zero && t_zero)
		armAsm->Movi(qd.V2D(), 0);
	else if (s_zero)
		armAsm->Mov(qd.V16B(), armQRegister(EEREC_T).V16B());
	else if (t_zero)
		armAsm->Mov(qd.V16B(), armQRegister(EEREC_S).V16B());
	else
		armAsm->Orr(qd.V16B(), armQRegister(EEREC_S).V16B(), armQRegister(EEREC_T).V16B());
}

void recPXOR()
{
	MMI_3OP_SETUP();
	armAsm->Eor(qd.V16B(), qs.V16B(), qt.V16B());
}

void recPNOR()
{
	MMI_3OP_SETUP();
	armAsm->Orr(qd.V16B(), qs.V16B(), qt.V16B());
	armAsm->Not(qd.V16B(), qd.V16B());
}

// ============================================================================
//  Packed Arithmetic — Signed
// ============================================================================

void recPADDW()
{
	MMI_3OP_SETUP();
	armAsm->Add(qd.V4S(), qs.V4S(), qt.V4S());
}

void recPSUBW()
{
	MMI_3OP_SETUP();
	armAsm->Sub(qd.V4S(), qs.V4S(), qt.V4S());
}

void recPADDH()
{
	MMI_3OP_SETUP();
	armAsm->Add(qd.V8H(), qs.V8H(), qt.V8H());
}

void recPSUBH()
{
	MMI_3OP_SETUP();
	armAsm->Sub(qd.V8H(), qs.V8H(), qt.V8H());
}

void recPADDB()
{
	MMI_3OP_SETUP();
	armAsm->Add(qd.V16B(), qs.V16B(), qt.V16B());
}

void recPSUBB()
{
	MMI_3OP_SETUP();
	armAsm->Sub(qd.V16B(), qs.V16B(), qt.V16B());
}

// ============================================================================
//  Packed Arithmetic — Unsigned
// ============================================================================

void recPADDUW()
{
	MMI_3OP_SETUP();
	armAsm->Uqadd(qd.V4S(), qs.V4S(), qt.V4S());
}

void recPSUBUW()
{
	MMI_3OP_SETUP();
	armAsm->Uqsub(qd.V4S(), qs.V4S(), qt.V4S());
}

void recPADDUH()
{
	MMI_3OP_SETUP();
	armAsm->Uqadd(qd.V8H(), qs.V8H(), qt.V8H());
}

void recPSUBUH()
{
	MMI_3OP_SETUP();
	armAsm->Uqsub(qd.V8H(), qs.V8H(), qt.V8H());
}

void recPADDUB()
{
	MMI_3OP_SETUP();
	armAsm->Uqadd(qd.V16B(), qs.V16B(), qt.V16B());
}

void recPSUBUB()
{
	MMI_3OP_SETUP();
	armAsm->Uqsub(qd.V16B(), qs.V16B(), qt.V16B());
}

// ============================================================================
//  Packed Arithmetic — Saturating Signed
// ============================================================================

void recPADDSW()
{
	MMI_3OP_SETUP();
	armAsm->Sqadd(qd.V4S(), qs.V4S(), qt.V4S());
}

void recPSUBSW()
{
	MMI_3OP_SETUP();
	armAsm->Sqsub(qd.V4S(), qs.V4S(), qt.V4S());
}

void recPADDSH()
{
	MMI_3OP_SETUP();
	armAsm->Sqadd(qd.V8H(), qs.V8H(), qt.V8H());
}

void recPSUBSH()
{
	MMI_3OP_SETUP();
	armAsm->Sqsub(qd.V8H(), qs.V8H(), qt.V8H());
}

void recPADDSB()
{
	MMI_3OP_SETUP();
	armAsm->Sqadd(qd.V16B(), qs.V16B(), qt.V16B());
}

void recPSUBSB()
{
	MMI_3OP_SETUP();
	armAsm->Sqsub(qd.V16B(), qs.V16B(), qt.V16B());
}

// ============================================================================
//  Packed Compare — Greater Than (signed)
// ============================================================================

void recPCGTW()
{
	MMI_3OP_SETUP();
	armAsm->Cmgt(qd.V4S(), qs.V4S(), qt.V4S());
}

void recPCGTH()
{
	MMI_3OP_SETUP();
	armAsm->Cmgt(qd.V8H(), qs.V8H(), qt.V8H());
}

void recPCGTB()
{
	MMI_3OP_SETUP();
	armAsm->Cmgt(qd.V16B(), qs.V16B(), qt.V16B());
}

// ============================================================================
//  Packed Compare — Equal
// ============================================================================

void recPCEQW()
{
	MMI_3OP_SETUP();
	armAsm->Cmeq(qd.V4S(), qs.V4S(), qt.V4S());
}

void recPCEQH()
{
	MMI_3OP_SETUP();
	armAsm->Cmeq(qd.V8H(), qs.V8H(), qt.V8H());
}

void recPCEQB()
{
	MMI_3OP_SETUP();
	armAsm->Cmeq(qd.V16B(), qs.V16B(), qt.V16B());
}

// ============================================================================
//  Packed Min/Max (signed)
// ============================================================================

void recPMAXW()
{
	MMI_3OP_SETUP();
	armAsm->Smax(qd.V4S(), qs.V4S(), qt.V4S());
}

void recPMINW()
{
	MMI_3OP_SETUP();
	armAsm->Smin(qd.V4S(), qs.V4S(), qt.V4S());
}

void recPMAXH()
{
	MMI_3OP_SETUP();
	armAsm->Smax(qd.V8H(), qs.V8H(), qt.V8H());
}

void recPMINH()
{
	MMI_3OP_SETUP();
	armAsm->Smin(qd.V8H(), qs.V8H(), qt.V8H());
}

// ============================================================================
//  Packed Absolute Value (signed)
// ============================================================================

void recPABSW()
{
	MMI_2OP_SETUP();
	// PS2 PABSW saturates INT_MIN → INT_MAX (per MMI.cpp _PABSW). NEON Abs
	// preserves INT_MIN; Sqabs is the saturating form that matches.
	armAsm->Sqabs(qd.V4S(), qt.V4S());
}

void recPABSH()
{
	MMI_2OP_SETUP();
	// Mirror of PABSW for halfword lanes.
	armAsm->Sqabs(qd.V8H(), qt.V8H());
}

// ============================================================================
//  Register Copy / Move
// ============================================================================

// PCPYLD: rd = { rs.UD[0], rt.UD[0] } — copy (concatenate) lower doubleword of each source.
void recPCPYLD()
{
	MMI_3OP_SETUP();
	armAsm->Zip1(qd.V2D(), qt.V2D(), qs.V2D());
}

// PCPYUD: rd = {rt[127:64], rs[127:64]} — upper doublewords interleaved
void recPCPYUD()
{
	MMI_3OP_SETUP();
	armAsm->Zip2(qd.V2D(), qs.V2D(), qt.V2D());
}

// PCPYH: rd = {rt.UH[4] x4, rt.UH[0] x4} — replicate halfwords.
// Register-resident via the allocator (MMI_2OP_SETUP) instead of a
// memory-bounce (Ldr q from Rt + Str q to Rd + const flush), matching
// sibling single-source MMI ops (PABSW/PCPYLD). Saves a full-width load
// + store per execution.
// Broadcast rt.H[4] into scratch FIRST so the qd==qt aliased case stays correct
// (a qd write would otherwise clobber rt before H[4] is read).
void recPCPYH()
{
	MMI_2OP_SETUP();
	armAsm->Dup(RQSCRATCH.V8H(), qt.V8H(), 4); // rt.H[4] x8 (read qt before qd write)
	armAsm->Dup(qd.V8H(), qt.V8H(), 0);        // qd = rt.H[0] x8
	armAsm->Mov(qd.V2D(), 1, RQSCRATCH.V2D(), 0); // upper 64 <- rt.H[4] x4
}

// PMFHI: rd = HI (128-bit)
void recPMFHI()
{
	if (!_Rd_) return;
	mmiInvalidateDest(_Rd_);

		armAsm->Ldr(RQSCRATCH, armCpuRegMem(&cpuRegs.HI.UQ));
	mmiStoreReg(_Rd_, RQSCRATCH);
}

// PMFLO: rd = LO (128-bit)
void recPMFLO()
{
	if (!_Rd_) return;
	mmiInvalidateDest(_Rd_);

		armAsm->Ldr(RQSCRATCH, armCpuRegMem(&cpuRegs.LO.UQ));
	mmiStoreReg(_Rd_, RQSCRATCH);
}

// PMTHI: HI = rs (128-bit). Take Rs from its NEON slot when allocated and store
// straight to HI memory. HI is never NEON-resident in the EE rec (no opcode
// passes XMMINFO_*HI — see iR5900Templates-arm64.cpp), so no allocator
// invalidation is needed.
void recPMTHI()
{
	int info = eeRecompileCodeXMM(XMMINFO_READS);
	(void)info;
	armAsm->Str(armQRegister(EEREC_S), armCpuRegMem(&cpuRegs.HI.UQ));
}

// PMTLO: LO = rs (128-bit). LO is never NEON-resident (see recPMTHI).
void recPMTLO()
{
	int info = eeRecompileCodeXMM(XMMINFO_READS);
	(void)info;
	armAsm->Str(armQRegister(EEREC_S), armCpuRegMem(&cpuRegs.LO.UQ));
}

// ============================================================================
//  Packed Shifts (by immediate sa field)
// ============================================================================

void recPSLLW()
{
	if (!_Rd_) return;
	mmiFlushReg(_Rt_);
	mmiInvalidateDest(_Rd_);
	mmiLoadReg(RQSCRATCH, _Rt_);
	if (_Sa_ == 0)
	{
		mmiStoreReg(_Rd_, RQSCRATCH);
		return;
	}
	armAsm->Shl(RQSCRATCH.V4S(), RQSCRATCH.V4S(), _Sa_);
	mmiStoreReg(_Rd_, RQSCRATCH);
}

void recPSRLW()
{
	if (!_Rd_) return;
	mmiFlushReg(_Rt_);
	mmiInvalidateDest(_Rd_);
	mmiLoadReg(RQSCRATCH, _Rt_);
	if (_Sa_ == 0)
	{
		mmiStoreReg(_Rd_, RQSCRATCH);
		return;
	}
	armAsm->Ushr(RQSCRATCH.V4S(), RQSCRATCH.V4S(), _Sa_);
	mmiStoreReg(_Rd_, RQSCRATCH);
}

void recPSRAW()
{
	if (!_Rd_) return;
	mmiFlushReg(_Rt_);
	mmiInvalidateDest(_Rd_);
	mmiLoadReg(RQSCRATCH, _Rt_);
	if (_Sa_ == 0)
	{
		mmiStoreReg(_Rd_, RQSCRATCH);
		return;
	}
	armAsm->Sshr(RQSCRATCH.V4S(), RQSCRATCH.V4S(), _Sa_);
	mmiStoreReg(_Rd_, RQSCRATCH);
}

// Halfword shifts: interp uses (_Sa_ & 0xf) per MMI.cpp:228/240/252 —
// only 4 of the 5 sa bits are live since the lane is 16-bit. vixl
// Shl/Ushr/Sshr V8H require shift ∈ [0,15]; mask up front to match
// interp and stay inside the encoder's range.
void recPSLLH()
{
	if (!_Rd_) return;
	mmiFlushReg(_Rt_);
	mmiInvalidateDest(_Rd_);
	mmiLoadReg(RQSCRATCH, _Rt_);
	const u32 sa = _Sa_ & 0xf;
	if (sa == 0)
	{
		mmiStoreReg(_Rd_, RQSCRATCH);
		return;
	}
	armAsm->Shl(RQSCRATCH.V8H(), RQSCRATCH.V8H(), sa);
	mmiStoreReg(_Rd_, RQSCRATCH);
}

void recPSRLH()
{
	if (!_Rd_) return;
	mmiFlushReg(_Rt_);
	mmiInvalidateDest(_Rd_);
	mmiLoadReg(RQSCRATCH, _Rt_);
	const u32 sa = _Sa_ & 0xf;
	if (sa == 0)
	{
		mmiStoreReg(_Rd_, RQSCRATCH);
		return;
	}
	armAsm->Ushr(RQSCRATCH.V8H(), RQSCRATCH.V8H(), sa);
	mmiStoreReg(_Rd_, RQSCRATCH);
}

void recPSRAH()
{
	if (!_Rd_) return;
	mmiFlushReg(_Rt_);
	mmiInvalidateDest(_Rd_);
	mmiLoadReg(RQSCRATCH, _Rt_);
	const u32 sa = _Sa_ & 0xf;
	if (sa == 0)
	{
		mmiStoreReg(_Rd_, RQSCRATCH);
		return;
	}
	armAsm->Sshr(RQSCRATCH.V8H(), RQSCRATCH.V8H(), sa);
	mmiStoreReg(_Rd_, RQSCRATCH);
}

// ============================================================================
//  Pack / Unpack (Extend / Compress)
// ============================================================================

// PEXTLW: interleave lower 32-bit words of rs and rt
// rd = {rs.UL[1], rt.UL[1], rs.UL[0], rt.UL[0]}
void recPEXTLW()
{
	MMI_3OP_SETUP();
	armAsm->Zip1(qd.V4S(), qt.V4S(), qs.V4S());
}

// PEXTUW: interleave upper 32-bit words of rs and rt
// rd = {rs.UL[3], rt.UL[3], rs.UL[2], rt.UL[2]}
void recPEXTUW()
{
	MMI_3OP_SETUP();
	armAsm->Zip2(qd.V4S(), qt.V4S(), qs.V4S());
}

// PEXTLH: interleave lower 16-bit halfwords
void recPEXTLH()
{
	if (!_Rd_)
		return;

	// rs==0 fast path: the odd output halfwords are all zero, so this
	// is just a zero-extend of rt's lower 4 halfwords to words. Skip requesting
	// XMMINFO_READS — otherwise the allocator pins a callee-saved NEON reg and
	// materializes a zero vector for r0 just to Zip it in. Zip1(qt,qt) duplicates
	// each halfword, then Ushr clears the high half of every word lane.
	if (_Rs_ == 0)
	{
		int info = eeRecompileCodeXMM(XMMINFO_READT | XMMINFO_WRITED);
		const a64::VRegister qt = armQRegister(EEREC_T);
		const a64::VRegister qd = armQRegister(EEREC_D);
		(void)info;
		armAsm->Zip1(qd.V8H(), qt.V8H(), qt.V8H()); // {H0,H0,H1,H1,H2,H2,H3,H3}
		armAsm->Ushr(qd.V4S(), qd.V4S(), 16);        // {0:H0, 0:H1, 0:H2, 0:H3}
		return;
	}

	MMI_3OP_SETUP();
	armAsm->Zip1(qd.V8H(), qt.V8H(), qs.V8H());
}

// PEXTUH: interleave upper 16-bit halfwords
void recPEXTUH()
{
	MMI_3OP_SETUP();
	armAsm->Zip2(qd.V8H(), qt.V8H(), qs.V8H());
}

// PEXTLB: interleave lower bytes
void recPEXTLB()
{
	MMI_3OP_SETUP();
	armAsm->Zip1(qd.V16B(), qt.V16B(), qs.V16B());
}

// PEXTUB: interleave upper bytes
void recPEXTUB()
{
	MMI_3OP_SETUP();
	armAsm->Zip2(qd.V16B(), qt.V16B(), qs.V16B());
}

// PPACW: pack words — {rs.UL[2], rs.UL[0], rt.UL[2], rt.UL[0]}
void recPPACW()
{
	MMI_3OP_SETUP();
	armAsm->Uzp1(qd.V4S(), qt.V4S(), qs.V4S());
}

// PPACH: pack halfwords — even halfwords from rs and rt
void recPPACH()
{
	MMI_3OP_SETUP();
	armAsm->Uzp1(qd.V8H(), qt.V8H(), qs.V8H());
}

// PPACB: pack bytes — even bytes from rs and rt
void recPPACB()
{
	MMI_3OP_SETUP();
	armAsm->Uzp1(qd.V16B(), qt.V16B(), qs.V16B());
}

// PADSBH: rd.UH[0..3] = rs.UH[0..3] - rt.UH[0..3], rd.UH[4..7] = rs.UH[4..7] + rt.UH[4..7]
// Lower 4 halfwords: subtract. Upper 4 halfwords: add.
void recPADSBH()
{
	MMI_3OP_SETUP();
	// Compute the add into a scratch FIRST. If Rd aliases Rs or Rt, the
	// allocator hands qd back as the same Q-reg as qs/qt — writing qd in
	// the sub step would clobber the source still needed for the add.
	armAsm->Add(RQSCRATCH.V8H(), qs.V8H(), qt.V8H());
	// qd = sub result (all 8 halfwords); safe to clobber qs/qt now.
	armAsm->Sub(qd.V8H(), qs.V8H(), qt.V8H());
	// Blend: keep lower 64 bits of sub in qd, upper 64 bits from add.
	armAsm->Mov(qd.V2D(), 1, RQSCRATCH.V2D(), 1);
}

// ============================================================================
//  Interleave halfwords
// ============================================================================

// PINTH: rd.US[2k]=Rt.US[k], rd.US[2k+1]=Rs.US[k+4], k=0..3 — interleave low 4
// halfwords of Rt with high 4 of Rs.
void recPINTH()
{
	MMI_3OP_SETUP();
	// Move rs upper 64 → low position of scratch (don't clobber qs).
	armAsm->Dup(RQSCRATCH.V2D(), qs.V2D(), 1); // tmp = {rs.UD[1], rs.UD[1]}
	// zip1.8h of rt(lower) and rs_upper(lower) gives interleaved result.
	armAsm->Zip1(qd.V8H(), qt.V8H(), RQSCRATCH.V8H());
}

// PINTEH: rd = {rs.UH[6],rt.UH[6], rs.UH[4],rt.UH[4], rs.UH[2],rt.UH[2], rs.UH[0],rt.UH[0]}
// Interleave even halfwords
void recPINTEH()
{
	MMI_3OP_SETUP();
	// Extract even halfwords from each into scratch — never touch qs/qt.
	armAsm->Uzp1(RQSCRATCH.V8H(), qs.V8H(), qs.V8H());    // rs evens in lower 64
	armAsm->Uzp1(RQSCRATCH2.V8H(), qt.V8H(), qt.V8H());   // rt evens in lower 64
	// Zip the lower 64 bits of each into qd.
	armAsm->Zip1(qd.V8H(), RQSCRATCH2.V8H(), RQSCRATCH.V8H());
}

// ============================================================================
//  Shuffles / Permutations
// ============================================================================

// PEXEW: rd = {rt[2], rt[1], rt[0], rt[3]} (lane order) — swap words 0 and 2.
// 2-op idiom (Rev64 + Ext) instead of a scratch snapshot + full copy + 2 lane
// inserts. Both ops read qt fully before writing, so it is alias-safe when the
// allocator hands back qd == qt.
void recPEXEW()
{
	MMI_2OP_SETUP();
	armAsm->Rev64(qd.V4S(), qt.V4S());            // {rt[1],rt[0],rt[3],rt[2]}
	armAsm->Ext(qd.V16B(), qd.V16B(), qd.V16B(), 12); // {rt[2],rt[1],rt[0],rt[3]}
}

// PEXEH: swap halfwords 0↔2 in each 64-bit lane
// rd = {H[2],H[1],H[0],H[3], H[6],H[5],H[4],H[7]}
void recPEXEH()
{
	MMI_2OP_SETUP();
	armAsm->Mov(RQSCRATCH.V16B(), qt.V16B());
	armAsm->Mov(qd.V8H(), RQSCRATCH.V8H());
	armAsm->Mov(qd.V8H(), 0, RQSCRATCH.V8H(), 2);
	armAsm->Mov(qd.V8H(), 2, RQSCRATCH.V8H(), 0);
	armAsm->Mov(qd.V8H(), 4, RQSCRATCH.V8H(), 6);
	armAsm->Mov(qd.V8H(), 6, RQSCRATCH.V8H(), 4);
}

// PREVH: reverse halfwords within each 64-bit lane
// rd = {H[3],H[2],H[1],H[0], H[7],H[6],H[5],H[4]}
void recPREVH()
{
	MMI_2OP_SETUP();
	armAsm->Rev64(qd.V8H(), qt.V8H());
}

// PROT3W: rotate lower 3 words: rd = {rt[1], rt[2], rt[0], rt[3]} (lane order).
// 3-op shuffle (Rev64 + Ext + Zip1) instead of a scratch snapshot + full copy
// + 3 lane inserts. Rev64 and Ext read qt into scratches
// first, so Zip1 → qd is alias-safe when qd == qt.
//   rev  = {rt[1],rt[0],rt[3],rt[2]}
//   ext8 = {rt[2],rt[3],rt[0],rt[1]}
//   Zip1(rev,ext8) = {rev[0],ext8[0],rev[1],ext8[1]} = {rt[1],rt[2],rt[0],rt[3]}
void recPROT3W()
{
	MMI_2OP_SETUP();
	armAsm->Rev64(RQSCRATCH.V4S(), qt.V4S());
	armAsm->Ext(RQSCRATCH2.V16B(), qt.V16B(), qt.V16B(), 8);
	armAsm->Zip1(qd.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S());
}

// PEXCW: swap words 1 and 2: rd = {rt[0], rt[2], rt[1], rt[3]} (lane order).
// 2-op idiom (Rev64 + Uzp1) instead of a scratch snapshot + full copy + 2 lane
// inserts. Both read their sources fully before
// writing qd, so it is alias-safe when qd == qt.
//   rev = {rt[1],rt[0],rt[3],rt[2]}
//   Uzp1(qt,rev) = {qt[0],qt[2],rev[0],rev[2]} = {rt[0],rt[2],rt[1],rt[3]}
void recPEXCW()
{
	MMI_2OP_SETUP();
	armAsm->Rev64(RQSCRATCH.V4S(), qt.V4S());
	armAsm->Uzp1(qd.V4S(), qt.V4S(), RQSCRATCH.V4S());
}

// PEXCH: swap halfwords 1↔2 within each 64-bit lane
// {H[0],H[2],H[1],H[3], H[4],H[6],H[5],H[7]}
void recPEXCH()
{
	MMI_2OP_SETUP();
	armAsm->Mov(RQSCRATCH.V16B(), qt.V16B());
	armAsm->Mov(qd.V8H(), RQSCRATCH.V8H());
	armAsm->Mov(qd.V8H(), 1, RQSCRATCH.V8H(), 2);
	armAsm->Mov(qd.V8H(), 2, RQSCRATCH.V8H(), 1);
	armAsm->Mov(qd.V8H(), 5, RQSCRATCH.V8H(), 6);
	armAsm->Mov(qd.V8H(), 6, RQSCRATCH.V8H(), 5);
}

// PEXT5: expand each 32-bit lane's PS2 RGB1555 field into BGRA8 layout.
//   Per-lane:
//     rd = ((rt & 0x001F) << 3)     // R bits [4:0]   -> [7:3]
//        | ((rt & 0x03E0) << 6)     // G bits [9:5]   -> [15:11]
//        | ((rt & 0x7C00) << 9)     // B bits [14:10] -> [23:19]
//        | ((rt & 0x8000) << 16);   // A bit [15]     -> [31]
void recPEXT5()
{
	MMI_2OP_SETUP();
	// Preserve qt in case allocator assigned qd == qt — rt is needed for all
	// four shift+mask passes below, but the first write to qd would clobber
	// it if they share a slot.
	armAsm->Mov(RQSCRATCH3.V16B(), qt.V16B());

	// Field 0: (rt << 3) & 0x000000F8 -> qd
	armAsm->Shl(qd.V4S(), RQSCRATCH3.V4S(), 3);
	armAsm->Movi(RQSCRATCH.V4S(), 0xF8);
	armAsm->And(qd.V16B(), qd.V16B(), RQSCRATCH.V16B());

	// Field 1: (rt << 6) & 0x0000F800 -> qd
	armAsm->Shl(RQSCRATCH2.V4S(), RQSCRATCH3.V4S(), 6);
	armAsm->Movi(RQSCRATCH.V4S(), 0xF8, vixl::aarch64::LSL, 8);
	armAsm->And(RQSCRATCH2.V16B(), RQSCRATCH2.V16B(), RQSCRATCH.V16B());
	armAsm->Orr(qd.V16B(), qd.V16B(), RQSCRATCH2.V16B());

	// Field 2: (rt << 9) & 0x00F80000 -> qd
	armAsm->Shl(RQSCRATCH2.V4S(), RQSCRATCH3.V4S(), 9);
	armAsm->Movi(RQSCRATCH.V4S(), 0xF8, vixl::aarch64::LSL, 16);
	armAsm->And(RQSCRATCH2.V16B(), RQSCRATCH2.V16B(), RQSCRATCH.V16B());
	armAsm->Orr(qd.V16B(), qd.V16B(), RQSCRATCH2.V16B());

	// Field 3: (rt << 16) & 0x80000000 -> qd
	armAsm->Shl(RQSCRATCH2.V4S(), RQSCRATCH3.V4S(), 16);
	armAsm->Movi(RQSCRATCH.V4S(), 0x80, vixl::aarch64::LSL, 24);
	armAsm->And(RQSCRATCH2.V16B(), RQSCRATCH2.V16B(), RQSCRATCH.V16B());
	armAsm->Orr(qd.V16B(), qd.V16B(), RQSCRATCH2.V16B());
}

// PPAC5: pack BGRA8-style 32-bit lanes back into PS2 RGB1555 16-bit layout
// (the inverse of PEXT5). Upper 16 bits of each lane left as garbage from
// the shifted-source — interp does not mask them either.
//   Per-lane:
//     rd = ((rt >>  3) & 0x001F)
//        | ((rt >>  6) & 0x03E0)
//        | ((rt >>  9) & 0x7C00)
//        | ((rt >> 16) & 0x8000);
void recPPAC5()
{
	MMI_2OP_SETUP();
	armAsm->Mov(RQSCRATCH3.V16B(), qt.V16B());

	// Field 0: (rt >> 3) & 0x0000001F -> qd
	armAsm->Ushr(qd.V4S(), RQSCRATCH3.V4S(), 3);
	armAsm->Movi(RQSCRATCH.V4S(), 0x1F);
	armAsm->And(qd.V16B(), qd.V16B(), RQSCRATCH.V16B());

	// Field 1: (rt >> 6) & 0x000003E0 -> qd
	//   0x3E0 has two non-zero bytes; vixl's Movi macro materializes it via
	//   Mov scratch_w + Dup (2 host insns) rather than the single LSL form.
	armAsm->Ushr(RQSCRATCH2.V4S(), RQSCRATCH3.V4S(), 6);
	armAsm->Movi(RQSCRATCH.V4S(), 0x3E0);
	armAsm->And(RQSCRATCH2.V16B(), RQSCRATCH2.V16B(), RQSCRATCH.V16B());
	armAsm->Orr(qd.V16B(), qd.V16B(), RQSCRATCH2.V16B());

	// Field 2: (rt >> 9) & 0x00007C00 -> qd
	armAsm->Ushr(RQSCRATCH2.V4S(), RQSCRATCH3.V4S(), 9);
	armAsm->Movi(RQSCRATCH.V4S(), 0x7C, vixl::aarch64::LSL, 8);
	armAsm->And(RQSCRATCH2.V16B(), RQSCRATCH2.V16B(), RQSCRATCH.V16B());
	armAsm->Orr(qd.V16B(), qd.V16B(), RQSCRATCH2.V16B());

	// Field 3: (rt >> 16) & 0x00008000 -> qd
	armAsm->Ushr(RQSCRATCH2.V4S(), RQSCRATCH3.V4S(), 16);
	armAsm->Movi(RQSCRATCH.V4S(), 0x80, vixl::aarch64::LSL, 8);
	armAsm->And(RQSCRATCH2.V16B(), RQSCRATCH2.V16B(), RQSCRATCH.V16B());
	armAsm->Orr(qd.V16B(), qd.V16B(), RQSCRATCH2.V16B());
}

// ============================================================================
//  Variable shifts — operate on words 0 and 2 only, sign-extend to 64
// ============================================================================

// PSLLVW: rd.SD[0] = sign_ext(rt.UL[0] << (rs.UL[0] & 0x1F))
//         rd.SD[1] = sign_ext(rt.UL[2] << (rs.UL[2] & 0x1F))
void recPSLLVW()
{
	if (!_Rd_) return;
	mmiFlushReg(_Rs_);
	mmiFlushReg(_Rt_);
	mmiInvalidateDest(_Rd_);

	// Word 0
	armLoadEERegPtr(a64::w0, &cpuRegs.GPR.r[_Rt_].UL[0]);
	armLoadEERegPtr(a64::w1, &cpuRegs.GPR.r[_Rs_].UL[0]);
	armAsm->And(a64::w1, a64::w1, 0x1F);
	armAsm->Lsl(a64::w0, a64::w0, a64::w1);
	armAsm->Sxtw(a64::x0, a64::w0);
	armStoreEERegPtr(a64::x0, &cpuRegs.GPR.r[_Rd_].UD[0]);

	// Word 2
	armLoadEERegPtr(a64::w0, &cpuRegs.GPR.r[_Rt_].UL[2]);
	armLoadEERegPtr(a64::w1, &cpuRegs.GPR.r[_Rs_].UL[2]);
	armAsm->And(a64::w1, a64::w1, 0x1F);
	armAsm->Lsl(a64::w0, a64::w0, a64::w1);
	armAsm->Sxtw(a64::x0, a64::w0);
	armStoreEERegPtr(a64::x0, &cpuRegs.GPR.r[_Rd_].UD[1]);
}

void recPSRLVW()
{
	if (!_Rd_) return;
	mmiFlushReg(_Rs_);
	mmiFlushReg(_Rt_);
	mmiInvalidateDest(_Rd_);

	armLoadEERegPtr(a64::w0, &cpuRegs.GPR.r[_Rt_].UL[0]);
	armLoadEERegPtr(a64::w1, &cpuRegs.GPR.r[_Rs_].UL[0]);
	armAsm->And(a64::w1, a64::w1, 0x1F);
	armAsm->Lsr(a64::w0, a64::w0, a64::w1);
	armAsm->Sxtw(a64::x0, a64::w0);
	armStoreEERegPtr(a64::x0, &cpuRegs.GPR.r[_Rd_].UD[0]);

	armLoadEERegPtr(a64::w0, &cpuRegs.GPR.r[_Rt_].UL[2]);
	armLoadEERegPtr(a64::w1, &cpuRegs.GPR.r[_Rs_].UL[2]);
	armAsm->And(a64::w1, a64::w1, 0x1F);
	armAsm->Lsr(a64::w0, a64::w0, a64::w1);
	armAsm->Sxtw(a64::x0, a64::w0);
	armStoreEERegPtr(a64::x0, &cpuRegs.GPR.r[_Rd_].UD[1]);
}

void recPSRAVW()
{
	if (!_Rd_) return;
	mmiFlushReg(_Rs_);
	mmiFlushReg(_Rt_);
	mmiInvalidateDest(_Rd_);

	armLoadEERegPtr(a64::w0, &cpuRegs.GPR.r[_Rt_].UL[0]);
	armLoadEERegPtr(a64::w1, &cpuRegs.GPR.r[_Rs_].UL[0]);
	armAsm->And(a64::w1, a64::w1, 0x1F);
	armAsm->Asr(a64::w0, a64::w0, a64::w1);
	armAsm->Sxtw(a64::x0, a64::w0);
	armStoreEERegPtr(a64::x0, &cpuRegs.GPR.r[_Rd_].UD[0]);

	armLoadEERegPtr(a64::w0, &cpuRegs.GPR.r[_Rt_].UL[2]);
	armLoadEERegPtr(a64::w1, &cpuRegs.GPR.r[_Rs_].UL[2]);
	armAsm->And(a64::w1, a64::w1, 0x1F);
	armAsm->Asr(a64::w0, a64::w0, a64::w1);
	armAsm->Sxtw(a64::x0, a64::w0);
	armStoreEERegPtr(a64::x0, &cpuRegs.GPR.r[_Rd_].UD[1]);
}

// ============================================================================
//  Multiply / Divide / MAC — at x86 parity via interpreter fallback
// ============================================================================

// These stay as REC_FUNC because production x86 stays REC_FUNC too (the
// MMI2_RECOMPILE native code paths in pcsx2/x86/iMMI.cpp live behind a never-
// defined macro):
//
//   PDIVW / PDIVBW / PDIVUW — AArch64 NEON has no integer divide, and x86's
//     commented-out "native" path is itself `recCall(Interp::PDIV*)` after a
//     targeted `_deleteEEreg(_Rd_, 0)`.  There is no codegen to port.
//
// PMADDUW gets a native impl below — its interp is plain u64 arithmetic (no
// errata), so a NEON port matches interp bit-for-bit.
//
// PMADDW / PMSUBW get a native SCALAR impl (AX-16): the old "any fast path
// diverges" claim was wrong — the errata division is by the CONSTANT
// 0xFFFFFFFF, so a 64-bit SDIV against positive 0x00000000FFFFFFFF is
// exactly the interp's C `temp2 / 4294967295` (s64 truncation toward zero;
// the divisor is positive so the INT64_MIN/-1 overflow case can't arise),
// and the lane-0 "division voodoo" is two logical-immediate tests. Proven
// emittable by ARMSX2's mac backend emitPMADDWLane (Tyler Bochard, GPLv3).
REC_FUNC(PDIVW);
REC_FUNC(PDIVBW);
REC_FUNC(PDIVUW);

// One PMADDW/PMSUBW lane pair (dd = dest half, ss = source SL index), matching
// MMI.cpp _PMADDW/_PMSUBW field-for-field:
//   temp  = (s64)Rs.SL[ss] * Rt.SL[ss]
//   acc   = temp + (HI.SL[ss] << 32)        (PMADDW; +0x70000000 lane-0 voodoo)
//         = (HI.SL[ss] << 32) - temp        (PMSUBW; never voodoo)
//   HI.SD[dd] = (s32)(acc / 0xFFFFFFFF)     (trunc toward zero — NOT >>32)
//   LO.SD[dd] = (s64)LO.SL[ss] ± (s64)(s32)low32(temp)
//       (the interp's `(s32)+(s32)` overflows are compiled as a 64-bit add of
//        the sign-extended halves; x86 shares the exact interp path via
//        REC_FUNC, so that UB-resolution is the cross-arch reference — pinned
//        by EeRecMmi.PmaddwLoWrapAddAndRsAliasesRt)
//   Rd.UD[dd] = LO.UL[dd*2] | HI.UL[dd*2] << 32
//
// Scratches w8/w9/w10/x17 are all outside the allocatable GPR pool, and
// armCpuRegMem is a pure [RSTATE, #imm] MemOperand (no address scratch), so
// x17 is safe as a value register here. Lane 1 reads SL[2]/UL[2] fields that
// lane 0's SD[0]/UD[0] stores never touch, so sequential per-lane commit
// matches the interp's ordering under every Rd/Rs/Rt aliasing.
static void recPMADDWLane(int dd, int ss, bool isSub)
{
	// armLoadEERegPtr: lane 0 (SL[0]) substitutes a pinned reg's mirror —
	// required under lazy-dirty (memory lower half may be stale) and a free
	// Ldr→Mov under write-through; lane 2 (SL[2]) is upper-half → memory is
	// always canonical there and the helper falls through to the plain Ldr.
	armLoadEERegPtr(a64::w8, &cpuRegs.GPR.r[_Rs_].SL[ss]);
	armLoadEERegPtr(a64::w9, &cpuRegs.GPR.r[_Rt_].SL[ss]);

	if (!isSub && ss == 0)
	{
		// x17 = voodoo addend: 0x70000000 iff ((rt & 0x7FFFFFFF) is 0 or
		// 0x7FFFFFFF) && rs != rt, else 0.
		armAsm->And(a64::w10, a64::w9, 0x7FFFFFFF);
		armAsm->Eor(a64::w17, a64::w10, 0x7FFFFFFF);
		// 64-bit product of two nonzero u32 can't be zero, so x10 == 0 iff
		// either factor was 0 iff (rt & 0x7FFFFFFF) hit a boundary value.
		armAsm->Umull(a64::x10, a64::w10, a64::w17);
		armAsm->Cmp(a64::w8, a64::w9);
		// If rs != rt: flags = (x10 == 0). Else: nzcv = 0 so eq fails.
		armAsm->Ccmp(a64::x10, 0, vixl::aarch64::NoFlag, a64::ne);
		armAsm->Mov(a64::w17, 0x70000000);
		armAsm->Csel(a64::x17, a64::x17, a64::xzr, a64::eq);
	}

	armAsm->Smull(a64::x10, a64::w8, a64::w9); // temp
	armAsm->Ldr(a64::w8, armCpuRegMem(&cpuRegs.HI.SL[ss]));
	if (!isSub)
	{
		armAsm->Add(a64::x8, a64::x10, a64::Operand(a64::x8, a64::LSL, 32));
		if (ss == 0)
			armAsm->Add(a64::x8, a64::x8, a64::x17);
	}
	else
	{
		armAsm->Lsl(a64::x8, a64::x8, 32);
		armAsm->Sub(a64::x8, a64::x8, a64::x10);
	}

	armAsm->Mov(a64::x17, 0xFFFFFFFFull);
	armAsm->Sdiv(a64::x17, a64::x8, a64::x17);
	armAsm->Sxtw(a64::x17, a64::w17); // HI.SD[dd] = (s32)quotient
	armAsm->Str(a64::x17, armCpuRegMem(&cpuRegs.HI.SD[dd]));

	armAsm->Ldrsw(a64::x9, armCpuRegMem(&cpuRegs.LO.SL[ss]));
	if (!isSub)
		armAsm->Add(a64::x9, a64::x9, a64::Operand(a64::w10, a64::SXTW));
	else
		armAsm->Sub(a64::x9, a64::x9, a64::Operand(a64::w10, a64::SXTW));
	armAsm->Str(a64::x9, armCpuRegMem(&cpuRegs.LO.SD[dd]));

	if (_Rd_)
	{
		// Rd.UD[dd] = the two low words just stored: LO in x9[31:0], HI
		// inserted from x17[31:0].
		armAsm->Bfi(a64::x9, a64::x17, 32, 32);
		armAsm->Str(a64::x9, armCpuRegMem(&cpuRegs.GPR.r[_Rd_].UD[dd]));
	}
}

void recPMADDW()
{
	mmiFlushReg(_Rs_);
	mmiFlushReg(_Rt_);
	if (_Rd_)
		mmiInvalidateDest(_Rd_);
	recPMADDWLane(0, 0, false);
	recPMADDWLane(1, 2, false);
}

void recPMSUBW()
{
	mmiFlushReg(_Rs_);
	mmiFlushReg(_Rt_);
	if (_Rd_)
		mmiInvalidateDest(_Rd_);
	recPMADDWLane(0, 0, true);
	recPMADDWLane(1, 2, true);
}

// PMULTW: 2-lane signed 32x32->64 multiply on even-indexed source words.
//   prod[0] = (s64)Rs.SL[0] * (s64)Rt.SL[0]
//   prod[1] = (s64)Rs.SL[2] * (s64)Rt.SL[2]
//   LO.UD[0..1] = sign-extended low32 of each product
//   HI.UD[0..1] = sign-extended high32 of each product
//   Rd.SD[0..1] = full 64-bit products
void recPMULTW()
{
	mmiFlushReg(_Rs_);
	mmiFlushReg(_Rt_);
	if (_Rd_)
		mmiInvalidateDest(_Rd_);

	mmiLoadReg(RQSCRATCH, _Rs_);
	mmiLoadReg(RQSCRATCH2, _Rt_);

	// Pack even-indexed 32-bit lanes into the low half (SL[0],SL[2] -> S[0],S[1])
	armAsm->Uzp1(RQSCRATCH.V4S(),  RQSCRATCH.V4S(),  RQSCRATCH.V4S());
	armAsm->Uzp1(RQSCRATCH2.V4S(), RQSCRATCH2.V4S(), RQSCRATCH2.V4S());

	// 2-lane signed 32x32->64 -> { prod0, prod1 } as 2x64
	armAsm->Smull(RQSCRATCH3.V2D(), RQSCRATCH.V2S(), RQSCRATCH2.V2S());

	// LO = sign-extended low32 of each product
	armAsm->Xtn(RQSCRATCH.V2S(), RQSCRATCH3.V2D());
	armAsm->Sxtl(RQSCRATCH.V2D(), RQSCRATCH.V2S());
	armAsm->Str(RQSCRATCH, armCpuRegMem(&cpuRegs.LO.UQ));

	// HI = sign-extended high32 of each product (shift right narrow + sxtl)
	armAsm->Shrn(RQSCRATCH.V2S(), RQSCRATCH3.V2D(), 32);
	armAsm->Sxtl(RQSCRATCH.V2D(), RQSCRATCH.V2S());
	armAsm->Str(RQSCRATCH, armCpuRegMem(&cpuRegs.HI.UQ));

	if (_Rd_)
		mmiStoreReg(_Rd_, RQSCRATCH3);
}

// PMULTUW: 2-lane unsigned 32x32->64 multiply on even-indexed source words.
//   prod[0] = (u64)Rs.UL[0] * (u64)Rt.UL[0]
//   prod[1] = (u64)Rs.UL[2] * (u64)Rt.UL[2]
//   LO.UD[0..1] = sign-extended low32 of each product (interp casts (s32))
//   HI.UD[0..1] = sign-extended high32 of each product
//   Rd.UD[0..1] = full 64-bit products
void recPMULTUW()
{
	mmiFlushReg(_Rs_);
	mmiFlushReg(_Rt_);
	if (_Rd_)
		mmiInvalidateDest(_Rd_);

	mmiLoadReg(RQSCRATCH, _Rs_);
	mmiLoadReg(RQSCRATCH2, _Rt_);

	armAsm->Uzp1(RQSCRATCH.V4S(),  RQSCRATCH.V4S(),  RQSCRATCH.V4S());
	armAsm->Uzp1(RQSCRATCH2.V4S(), RQSCRATCH2.V4S(), RQSCRATCH2.V4S());

	armAsm->Umull(RQSCRATCH3.V2D(), RQSCRATCH.V2S(), RQSCRATCH2.V2S());

	armAsm->Xtn(RQSCRATCH.V2S(), RQSCRATCH3.V2D());
	armAsm->Sxtl(RQSCRATCH.V2D(), RQSCRATCH.V2S());
	armAsm->Str(RQSCRATCH, armCpuRegMem(&cpuRegs.LO.UQ));

	armAsm->Shrn(RQSCRATCH.V2S(), RQSCRATCH3.V2D(), 32);
	armAsm->Sxtl(RQSCRATCH.V2D(), RQSCRATCH.V2S());
	armAsm->Str(RQSCRATCH, armCpuRegMem(&cpuRegs.HI.UQ));

	if (_Rd_)
		mmiStoreReg(_Rd_, RQSCRATCH3);
}

// PMADDUW: 2-lane unsigned 32x32+64->64 multiply-accumulate on even-indexed
// source words.
//   tempu[k] = (u64)(LO.UL[2k] | (HI.UL[2k] << 32)) + (u64)Rs.UL[2k] * Rt.UL[2k]
//   LO.UD[k] = sign-extended low32 of tempu[k]
//   HI.UD[k] = sign-extended high32 of tempu[k]
//   Rd.UD[k] = tempu[k]   (full u64)
//
// Interp has no PS2 multiplication errata for the unsigned variant — plain u64
// arithmetic — so this matches interp bit-for-bit (unlike PMADDW/PMSUBW which
// stay REC_FUNC above).
//
// Bypasses the LO/HI allocator path: EE rec's info-word layout packs EEREC_LO
// and EEREC_HI into the same 5-bit field (the EEREC_LO/EEREC_HI info-word
// macros decode the same bits), so it can't produce two distinct register
// indices. An op that needs both LO and HI live must load them from memory.
void recPMADDUW()
{
	mmiFlushReg(_Rs_);
	mmiFlushReg(_Rt_);
	if (_Rd_)
		mmiInvalidateDest(_Rd_);

	// LO/HI are never NEON-resident in the EE rec (no opcode passes XMMINFO_*LO/HI),
	// so the Ldrs below already see fresh memory — no allocator flush needed.
	mmiLoadReg(RQSCRATCH, _Rs_);
	mmiLoadReg(RQSCRATCH2, _Rt_);

	// Pack even-indexed 32-bit lanes into the low half (UL[0],UL[2] -> S[0],S[1])
	armAsm->Uzp1(RQSCRATCH.V4S(),  RQSCRATCH.V4S(),  RQSCRATCH.V4S());
	armAsm->Uzp1(RQSCRATCH2.V4S(), RQSCRATCH2.V4S(), RQSCRATCH2.V4S());

	// 2-lane unsigned 32x32->64 product
	armAsm->Umull(RQSCRATCH3.V2D(), RQSCRATCH.V2S(), RQSCRATCH2.V2S());

	// Compose accumulator: { LO.UL[0] | HI.UL[0]<<32, LO.UL[2] | HI.UL[2]<<32 }
	// Trn1.V4S(d, a, b) = { a[0], b[0], a[2], b[2] } -> as V2D, gives LO|HI<<32 per lane.
	armAsm->Ldr(RQSCRATCH,  armCpuRegMem(&cpuRegs.LO.UQ));
	armAsm->Ldr(RQSCRATCH2, armCpuRegMem(&cpuRegs.HI.UQ));
	armAsm->Trn1(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH2.V4S());

	// sum = composed + product (2x64 unsigned add)
	armAsm->Add(RQSCRATCH3.V2D(), RQSCRATCH.V2D(), RQSCRATCH3.V2D());

	// LO = sign-extended low32 of each 64-bit lane
	armAsm->Xtn(RQSCRATCH.V2S(), RQSCRATCH3.V2D());
	armAsm->Sxtl(RQSCRATCH.V2D(), RQSCRATCH.V2S());
	armAsm->Str(RQSCRATCH, armCpuRegMem(&cpuRegs.LO.UQ));

	// HI = sign-extended high32 of each 64-bit lane
	armAsm->Shrn(RQSCRATCH.V2S(), RQSCRATCH3.V2D(), 32);
	armAsm->Sxtl(RQSCRATCH.V2D(), RQSCRATCH.V2S());
	armAsm->Str(RQSCRATCH, armCpuRegMem(&cpuRegs.HI.UQ));

	// Rd = full 2x64 unsigned sum
	if (_Rd_)
		mmiStoreReg(_Rd_, RQSCRATCH3);
}

// PHMADH: 8-lane signed 16x16->32 multiply, pair-sum (n + n+1):
//   sum[k] = Rs.SH[2k]*Rt.SH[2k] + Rs.SH[2k+1]*Rt.SH[2k+1]  for k = 0..3
//   firsttemp[k] = Rs.SH[2k+1]*Rt.SH[2k+1]   (the second product of each pair)
//   LO = { sum[0], firsttemp[0], sum[2], firsttemp[2] }
//   HI = { sum[1], firsttemp[1], sum[3], firsttemp[3] }
//   Rd = { sum[0], sum[1], sum[2], sum[3] } (post-update LO/HI even lanes)
void recPHMADH()
{
	mmiFlushReg(_Rs_);
	mmiFlushReg(_Rt_);
	if (_Rd_)
		mmiInvalidateDest(_Rd_);

	mmiLoadReg(RQSCRATCH, _Rs_);
	mmiLoadReg(RQSCRATCH2, _Rt_);

	// p_lo = { Rs.SH[i] * Rt.SH[i] } for i = 0..3 (as 4x32)
	// p_hi = { Rs.SH[i] * Rt.SH[i] } for i = 4..7 (as 4x32)
	armAsm->Smull(RQSCRATCH3.V4S(), RQSCRATCH.V4H(), RQSCRATCH2.V4H());
	armAsm->Smull2(RQSCRATCH.V4S(), RQSCRATCH.V8H(), RQSCRATCH2.V8H());

	// sums = ADDP(p_lo, p_hi).4S = { p0+p1, p2+p3, p4+p5, p6+p7 }
	armAsm->Addp(RQSCRATCH2.V4S(), RQSCRATCH3.V4S(), RQSCRATCH.V4S());
	// firsts = UZP2(p_lo, p_hi).4S = { p1, p3, p5, p7 }
	armAsm->Uzp2(RQSCRATCH3.V4S(), RQSCRATCH3.V4S(), RQSCRATCH.V4S());

	// LO = TRN1.4S(sums, firsts) = { sum0, p1, sum2, p5 }
	armAsm->Trn1(RQSCRATCH.V4S(), RQSCRATCH2.V4S(), RQSCRATCH3.V4S());
	armAsm->Str(RQSCRATCH, armCpuRegMem(&cpuRegs.LO.UQ));

	// HI = TRN2.4S(sums, firsts) = { sum1, p3, sum3, p7 }
	armAsm->Trn2(RQSCRATCH.V4S(), RQSCRATCH2.V4S(), RQSCRATCH3.V4S());
	armAsm->Str(RQSCRATCH, armCpuRegMem(&cpuRegs.HI.UQ));

	if (_Rd_)
		mmiStoreReg(_Rd_, RQSCRATCH2);
}

// PHMSBH: 8-lane signed 16x16->32 multiply, pair-diff (n+1 - n):
//   sum[k] = Rs.SH[2k+1]*Rt.SH[2k+1] - Rs.SH[2k]*Rt.SH[2k]   (k = 0..3)
//   firsttemp[k] = Rs.SH[2k+1]*Rt.SH[2k+1]    (the second product per pair)
//   LO = { sum[0], ~firsttemp[0], sum[2], ~firsttemp[2] }   (note: bitwise NOT)
//   HI = { sum[1], ~firsttemp[1], sum[3], ~firsttemp[3] }
//   Rd = { sum[0], sum[1], sum[2], sum[3] }
void recPHMSBH()
{
	mmiFlushReg(_Rs_);
	mmiFlushReg(_Rt_);
	if (_Rd_)
		mmiInvalidateDest(_Rd_);

	mmiLoadReg(RQSCRATCH, _Rs_);
	mmiLoadReg(RQSCRATCH2, _Rt_);

	armAsm->Smull(RQSCRATCH3.V4S(), RQSCRATCH.V4H(), RQSCRATCH2.V4H());
	armAsm->Smull2(RQSCRATCH.V4S(), RQSCRATCH.V8H(), RQSCRATCH2.V8H());

	// odds  = UZP2(p_lo, p_hi).4S = { p1, p3, p5, p7 }  (firsttemps)
	// evens = UZP1(p_lo, p_hi).4S = { p0, p2, p4, p6 }
	armAsm->Uzp2(RQSCRATCH2.V4S(), RQSCRATCH3.V4S(), RQSCRATCH.V4S());
	armAsm->Uzp1(RQSCRATCH3.V4S(), RQSCRATCH3.V4S(), RQSCRATCH.V4S());

	// sums = odds - evens = { p1-p0, p3-p2, p5-p4, p7-p6 }
	armAsm->Sub(RQSCRATCH.V4S(), RQSCRATCH2.V4S(), RQSCRATCH3.V4S());

	// nfirsts = ~odds (reuse RQSCRATCH3 — evens are dead after Sub)
	armAsm->Mvn(RQSCRATCH3.V16B(), RQSCRATCH2.V16B());

	if (_Rd_)
		mmiStoreReg(_Rd_, RQSCRATCH);

	// LO = TRN1.4S(sums, nfirsts) = { sum0, ~p1, sum2, ~p5 }
	armAsm->Trn1(RQSCRATCH2.V4S(), RQSCRATCH.V4S(), RQSCRATCH3.V4S());
	armAsm->Str(RQSCRATCH2, armCpuRegMem(&cpuRegs.LO.UQ));

	// HI = TRN2.4S(sums, nfirsts) = { sum1, ~p3, sum3, ~p7 }
	armAsm->Trn2(RQSCRATCH2.V4S(), RQSCRATCH.V4S(), RQSCRATCH3.V4S());
	armAsm->Str(RQSCRATCH2, armCpuRegMem(&cpuRegs.HI.UQ));
}

// PMULTH: 8-lane signed 16x16->32 multiply.
//   r[i] = Rs.SH[i] * Rt.SH[i] for i in 0..7
//   LO = { r0, r1, r4, r5 }
//   HI = { r2, r3, r6, r7 }
//   Rd = { r0, r2, r4, r6 }   (even-indexed products)
void recPMULTH()
{
	mmiFlushReg(_Rs_);
	mmiFlushReg(_Rt_);
	if (_Rd_)
		mmiInvalidateDest(_Rd_);

	mmiLoadReg(RQSCRATCH, _Rs_);
	mmiLoadReg(RQSCRATCH2, _Rt_);

	// q29 = SMULL  Rs.4H, Rt.4H -> { r0,r1,r2,r3 } as 4x32
	// q31 = SMULL2 Rs.8H, Rt.8H -> { r4,r5,r6,r7 } as 4x32 (in-place over Rt)
	armAsm->Smull(RQSCRATCH3.V4S(), RQSCRATCH.V4H(), RQSCRATCH2.V4H());
	armAsm->Smull2(RQSCRATCH2.V4S(), RQSCRATCH.V8H(), RQSCRATCH2.V8H());

	// LO = TRN1.2D(prod_lo, prod_hi) = { prod_lo.D[0], prod_hi.D[0] } = { r0,r1,r4,r5 }
	armAsm->Trn1(RQSCRATCH.V2D(), RQSCRATCH3.V2D(), RQSCRATCH2.V2D());
	armAsm->Str(RQSCRATCH, armCpuRegMem(&cpuRegs.LO.UQ));

	// HI = TRN2.2D(prod_lo, prod_hi) = { prod_lo.D[1], prod_hi.D[1] } = { r2,r3,r6,r7 }
	armAsm->Trn2(RQSCRATCH.V2D(), RQSCRATCH3.V2D(), RQSCRATCH2.V2D());
	armAsm->Str(RQSCRATCH, armCpuRegMem(&cpuRegs.HI.UQ));

	if (_Rd_)
	{
		// Rd = UZP1.4S(prod_lo, prod_hi) = { r0, r2, r4, r6 } (even-indexed)
		armAsm->Uzp1(RQSCRATCH.V4S(), RQSCRATCH3.V4S(), RQSCRATCH2.V4S());
		mmiStoreReg(_Rd_, RQSCRATCH);
	}
}

// PMADDH: 8-lane signed 16x16->32 multiply, accumulate into existing LO/HI.
//   r[i] = Rs.SH[i] * Rt.SH[i] for i in 0..7
//   LO.UL[0..3] += { r0, r1, r4, r5 }
//   HI.UL[0..3] += { r2, r3, r6, r7 }
//   Rd = { new_LO[0], new_HI[0], new_LO[2], new_HI[2] }
void recPMADDH()
{
	mmiFlushReg(_Rs_);
	mmiFlushReg(_Rt_);
	if (_Rd_)
		mmiInvalidateDest(_Rd_);

	mmiLoadReg(RQSCRATCH, _Rs_);
	mmiLoadReg(RQSCRATCH2, _Rt_);

	armAsm->Smull(RQSCRATCH3.V4S(), RQSCRATCH.V4H(), RQSCRATCH2.V4H());
	armAsm->Smull2(RQSCRATCH2.V4S(), RQSCRATCH.V8H(), RQSCRATCH2.V8H());

	// q30 = LO_increment = { r0,r1,r4,r5 }
	armAsm->Trn1(RQSCRATCH.V2D(), RQSCRATCH3.V2D(), RQSCRATCH2.V2D());
	// q29 = HI_increment = { r2,r3,r6,r7 }
	armAsm->Trn2(RQSCRATCH3.V2D(), RQSCRATCH3.V2D(), RQSCRATCH2.V2D());

	// q31 = old LO; add and store
	armAsm->Ldr(RQSCRATCH2, armCpuRegMem(&cpuRegs.LO.UQ));
	armAsm->Add(RQSCRATCH.V4S(), RQSCRATCH2.V4S(), RQSCRATCH.V4S());
	armAsm->Str(RQSCRATCH, armCpuRegMem(&cpuRegs.LO.UQ));

	// q31 = old HI; add and store
	armAsm->Ldr(RQSCRATCH2, armCpuRegMem(&cpuRegs.HI.UQ));
	armAsm->Add(RQSCRATCH3.V4S(), RQSCRATCH2.V4S(), RQSCRATCH3.V4S());
	armAsm->Str(RQSCRATCH3, armCpuRegMem(&cpuRegs.HI.UQ));

	if (_Rd_)
	{
		// Rd = TRN1.4S(new_LO, new_HI) = { LO[0], HI[0], LO[2], HI[2] }
		armAsm->Trn1(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH3.V4S());
		mmiStoreReg(_Rd_, RQSCRATCH);
	}
}

// PMSUBH: 8-lane signed 16x16->32 multiply, subtract from existing LO/HI.
//   r[i] = Rs.SH[i] * Rt.SH[i] for i in 0..7
//   LO.UL[0..3] -= { r0, r1, r4, r5 }
//   HI.UL[0..3] -= { r2, r3, r6, r7 }
//   Rd = { new_LO[0], new_HI[0], new_LO[2], new_HI[2] }
void recPMSUBH()
{
	mmiFlushReg(_Rs_);
	mmiFlushReg(_Rt_);
	if (_Rd_)
		mmiInvalidateDest(_Rd_);

	mmiLoadReg(RQSCRATCH, _Rs_);
	mmiLoadReg(RQSCRATCH2, _Rt_);

	armAsm->Smull(RQSCRATCH3.V4S(), RQSCRATCH.V4H(), RQSCRATCH2.V4H());
	armAsm->Smull2(RQSCRATCH2.V4S(), RQSCRATCH.V8H(), RQSCRATCH2.V8H());

	armAsm->Trn1(RQSCRATCH.V2D(), RQSCRATCH3.V2D(), RQSCRATCH2.V2D());
	armAsm->Trn2(RQSCRATCH3.V2D(), RQSCRATCH3.V2D(), RQSCRATCH2.V2D());

	armAsm->Ldr(RQSCRATCH2, armCpuRegMem(&cpuRegs.LO.UQ));
	armAsm->Sub(RQSCRATCH.V4S(), RQSCRATCH2.V4S(), RQSCRATCH.V4S());
	armAsm->Str(RQSCRATCH, armCpuRegMem(&cpuRegs.LO.UQ));

	armAsm->Ldr(RQSCRATCH2, armCpuRegMem(&cpuRegs.HI.UQ));
	armAsm->Sub(RQSCRATCH3.V4S(), RQSCRATCH2.V4S(), RQSCRATCH3.V4S());
	armAsm->Str(RQSCRATCH3, armCpuRegMem(&cpuRegs.HI.UQ));

	if (_Rd_)
	{
		armAsm->Trn1(RQSCRATCH.V4S(), RQSCRATCH.V4S(), RQSCRATCH3.V4S());
		mmiStoreReg(_Rd_, RQSCRATCH);
	}
}

// ============================================================================
//  QFSRV: Quad Funnel Shift Right Variable
// ============================================================================

// QFSRV: Rd = {Rs, Rt} >> (sa * 8), truncated to 128 bits.
// cpuRegs.sa is in bytes (0-15). Concatenate Rt (low) and Rs (high)
// into a 256-bit value, shift right by sa bytes, take lower 128 bits.
// Implementation: store {Rt, Rs} to adjacent memory, unaligned load at offset sa.
// Matches x86 approach using tempqw buffer.
alignas(16) static u8 s_qfsrvTemp[32];

void recQFSRV()
{
	if (!_Rd_) return;

	mmiFlushReg(_Rs_);
	mmiFlushReg(_Rt_);
	mmiInvalidateDest(_Rd_);

	// Adjacent-source fast path: when Rs == Rt+1 the 256-bit
	// {Rt:Rs} window already exists contiguously in the GPR array
	// (GPR.r[Rt] immediately precedes GPR.r[Rt+1]==GPR.r[Rs], 32 bytes), now
	// memory-coherent after the flushes above. Read the unaligned 128 bits
	// directly at &GPR.r[Rt] + sa and skip the two temp stores. sa is 0..15 so
	// the load stays within the two registers' 32 bytes. Gate on Rt != 0 to avoid
	// depending on GPR.r[0] holding zero in memory (the slow path Movi's it).
	if (_Rt_ != 0 && _Rs_ == _Rt_ + 1)
	{
		armLoadEERegPtr(RWSCRATCH, &cpuRegs.sa);
		// Clamp sa to 0..15 before indexing host memory: MTSA is a full
		// 32-bit copy (matching the interp), so cpuRegs.sa can hold >= 16 —
		// unmasked that walks this 128-bit load out of the two registers'
		// 32 bytes (guest-controlled host OOB read). x86 gets the same
		// effective clamp by masking in recMTSA; we mask at consumption so
		// the MFSA round-trip stays interp-faithful. (AX-03)
		armAsm->And(RWSCRATCH, RWSCRATCH, 0xf);
		armMoveAddressToReg(RSCRATCHADDR, &cpuRegs.GPR.r[_Rt_]);
		armAsm->Add(RSCRATCHADDR, RSCRATCHADDR, RXSCRATCH);
		armAsm->Ldr(RQSCRATCH, a64::MemOperand(RSCRATCHADDR));
		armStoreEEGPRQuad(RQSCRATCH, _Rd_);
		return;
	}

	// Store Rt at temp[0:15], Rs at temp[16:31]
	mmiLoadReg(RQSCRATCH, _Rt_);
	armMoveAddressToReg(RSCRATCHADDR, &s_qfsrvTemp[0]);
	armAsm->Str(RQSCRATCH, a64::MemOperand(RSCRATCHADDR));

	mmiLoadReg(RQSCRATCH, _Rs_);
	armMoveAddressToReg(RSCRATCHADDR, &s_qfsrvTemp[16]);
	armAsm->Str(RQSCRATCH, a64::MemOperand(RSCRATCHADDR));

	// Load sa (byte offset), clamped to 0..15 — see the fast path above; an
	// unmasked sa >= 16 reads past the 32-byte temp buffer. (AX-03)
	armLoadEERegPtr(RWSCRATCH, &cpuRegs.sa);
	armAsm->And(RWSCRATCH, RWSCRATCH, 0xf);

	// Unaligned 128-bit load from temp + sa
	armMoveAddressToReg(RSCRATCHADDR, &s_qfsrvTemp[0]);
	armAsm->Add(RSCRATCHADDR, RSCRATCHADDR, RXSCRATCH); // addr = temp + sa
	armAsm->Ldr(RQSCRATCH, a64::MemOperand(RSCRATCHADDR));

	// Store result to Rd
	armStoreEEGPRQuad(RQSCRATCH, _Rd_);
}

// ============================================================================
//  Other MMI
// ============================================================================

// PLZCW: count leading sign bits (excluding the sign bit itself) for words 0 and 1
void recPLZCW()
{
	if (!_Rd_) return;
	mmiFlushReg(_Rs_);
	mmiInvalidateDest(_Rd_);

	// Word 0: ARM64 CLS counts leading sign bits excluding the MSB sign bit itself,
	// which matches the PS2 PLZCW definition (CountLeadingSignBits - 1).
	armLoadEERegPtr(a64::w0, &cpuRegs.GPR.r[_Rs_].UL[0]);
	armAsm->Cls(a64::w0, a64::w0);
	armStoreEERegPtr(a64::w0, &cpuRegs.GPR.r[_Rd_].UL[0]);

	// Word 1
	armLoadEERegPtr(a64::w0, &cpuRegs.GPR.r[_Rs_].UL[1]);
	armAsm->Cls(a64::w0, a64::w0);
	armStoreEERegPtr(a64::w0, &cpuRegs.GPR.r[_Rd_].UL[1]);
}

// PMFHL — read LO/HI 128-bit register pair, dispatch on sa:
//   0x00 LW : Rd = { LO.UL[0], HI.UL[0], LO.UL[2], HI.UL[2] }
//   0x01 UW : Rd = { LO.UL[1], HI.UL[1], LO.UL[3], HI.UL[3] }
//   0x02 SLW: composed s64 (HI.UL[2k]:LO.UL[2k]) signed-saturated to s32
//             then sign-extended to s64; written to Rd.UD[k] for k=0,1.
//   0x03 LH : Rd.US lanes from even-indexed LO/HI halfwords, interleaved
//             at 32-bit-pair granularity.
//   0x04 SH : per-lane s32→s16 signed saturation of LO/HI words, interleaved
//             at 32-bit-pair granularity.
// sa >= 5 — interpreter is a no-op (no default-case in MMI.cpp PMFHL); mirror
// that by early-returning without touching Rd. (x86 rec asserts on this path,
// but the interp doesn't, so the recompiled-vs-interp diff would fail an assert
// rather than catch real divergence — matching the interp is the safer choice.)
void recPMFHL()
{
	if (!_Rd_)
		return;
	if (_Sa_ > 0x04)
		return;

	// LO/HI loaded directly from memory rather than via the allocator
	// (XMMINFO_READLO | XMMINFO_READHI). Reason: the arm64 EE rec's
	// info-word layout packs PROCESS_EE_SET_LO and PROCESS_EE_SET_HI
	// into the SAME 5-bit field at bits 23..27 (see iCore-arm64.h —
	// EEREC_LO == EEREC_HI == EEREC_ACC). PMFHL needs both LO and HI as
	// simultaneous live inputs, which that field collision cannot represent.
	// Bypass with explicit Ldrs. LO/HI are never NEON-resident in the EE rec
	// (no opcode passes XMMINFO_*LO/HI), so the memory image is already current.

	int info = eeRecompileCodeXMM(XMMINFO_WRITED);
	const a64::VRegister qd = armQRegister(EEREC_D);
	(void)info;

	// Pre-loaded into reserved scratch quads (outside the allocator pool).
	const a64::VRegister qlo = RQSCRATCH;
	const a64::VRegister qhi = RQSCRATCH2;
	armAsm->Ldr(qlo, armCpuRegMem(&cpuRegs.LO.UQ));
	armAsm->Ldr(qhi, armCpuRegMem(&cpuRegs.HI.UQ));

	switch (_Sa_)
	{
		case 0x00: // LW: pick even-indexed words from LO/HI and interleave
			//   TRN1.V4S → { LO.S[0], HI.S[0], LO.S[2], HI.S[2] }
			armAsm->Trn1(qd.V4S(), qlo.V4S(), qhi.V4S());
			break;

		case 0x01: // UW: pick odd-indexed words from LO/HI and interleave
			//   TRN2.V4S → { LO.S[1], HI.S[1], LO.S[3], HI.S[3] }
			armAsm->Trn2(qd.V4S(), qlo.V4S(), qhi.V4S());
			break;

		case 0x02: // SLW: compose s64 (HI:LO) per even-word lane, saturate to s32, sign-extend back
			//   TRN1.V4S → V2D { (HI[0]:LO[0]), (HI[2]:LO[2]) } (LO in low 32 of each 64)
			//   SQXTN.V2S — signed-saturating narrow 2x64 → 2x32 (matches interp's
			//   "in-range -> (s64)(s32)LO.UL[2k]; saturate to INT32_MIN/MAX" bounds).
			//   SXTL.V2D — sign-extend 2x32 → 2x64 (= the recorded Rd.UD shape).
			armAsm->Trn1(qd.V4S(), qlo.V4S(), qhi.V4S());
			armAsm->Sqxtn(qd.V2S(), qd.V2D());
			armAsm->Sxtl(qd.V2D(), qd.V2S());
			break;

		case 0x03: // LH: even halfwords from LO/HI, interleaved at S-pair granularity
			//   UZP1.V8H(x, x) gathers x's even halfwords into the low 64 bits of x.
			//   ZIP1.V4S picks S[0]/S[1] of each input → output S[0..3] =
			//     { (LO[0]:LO[2]), (HI[0]:HI[2]), (LO[4]:LO[6]), (HI[4]:HI[6]) }
			//   which as V8H = { LO[0], LO[2], HI[0], HI[2], LO[4], LO[6], HI[4], HI[6] }.
			armAsm->Uzp1(qlo.V8H(), qlo.V8H(), qlo.V8H());
			armAsm->Uzp1(qhi.V8H(), qhi.V8H(), qhi.V8H());
			armAsm->Zip1(qd.V4S(), qlo.V4S(), qhi.V4S());
			break;

		case 0x04: // SH: signed-saturating narrow 32→16 per word, interleaved at S-pair granularity
			//   SQXTN.V4H — 4x32 signed-sat narrowed to 4x16 in low 64 of each scratch.
			//   ZIP1.V4S → output S[0..3] = { sat(LO[0..1]), sat(HI[0..1]), sat(LO[2..3]), sat(HI[2..3]) }
			//   which as V8H is exactly the interp's PMFHL_CLAMP-per-lane pattern.
			armAsm->Sqxtn(qlo.V4H(), qlo.V4S());
			armAsm->Sqxtn(qhi.V4H(), qhi.V4S());
			armAsm->Zip1(qd.V4S(), qlo.V4S(), qhi.V4S());
			break;
	}
}

// PMTHL.LW: even-indexed words of LO/HI receive Rs's four words; the
// odd-indexed words (UL[1] and UL[3] of each) are preserved. Matches
// interp at MMI.cpp:217-224 and x86 BLENDPS/SHUFPS sequence at
// iMMI.cpp:234-248. Strategy: load LO/HI as Q regs, INS lanes 1+3 from
// the prior values to preserve them; lane 0 and lane 2 come from Rs's
// word 0/2 for LO, word 1/3 for HI.
void recPMTHL()
{
	if (_Sa_ != 0)
		return;

	mmiFlushReg(_Rs_);
	mmiLoadReg(RQSCRATCH, _Rs_);

	// LO_new = [Rs.UL[0], LO.UL[1], Rs.UL[2], LO.UL[3]]
	armAsm->Ldr(RQSCRATCH2, armCpuRegMem(&cpuRegs.LO.UQ));
	armAsm->Mov(RQSCRATCH3.V16B(), RQSCRATCH.V16B());
	armAsm->Ins(RQSCRATCH3.V4S(), 1, RQSCRATCH2.V4S(), 1);
	armAsm->Ins(RQSCRATCH3.V4S(), 3, RQSCRATCH2.V4S(), 3);
	armAsm->Str(RQSCRATCH3, armCpuRegMem(&cpuRegs.LO.UQ));

	// HI_new = [Rs.UL[1], HI.UL[1], Rs.UL[3], HI.UL[3]]
	armAsm->Ldr(RQSCRATCH3, armCpuRegMem(&cpuRegs.HI.UQ));
	armAsm->Ins(RQSCRATCH3.V4S(), 0, RQSCRATCH.V4S(), 1);
	armAsm->Ins(RQSCRATCH3.V4S(), 2, RQSCRATCH.V4S(), 3);
	armAsm->Str(RQSCRATCH3, armCpuRegMem(&cpuRegs.HI.UQ));
}

} // namespace MMI
} // namespace OpcodeImpl
} // namespace Dynarec
} // namespace R5900
