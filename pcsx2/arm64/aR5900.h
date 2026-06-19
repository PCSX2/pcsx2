// SPDX-FileCopyrightText: 2026 isztld <https://isztld.com/>
// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// ARM64 EE (R5900) recompiler — public interface + register-allocation contract.
//
// This is the ARM64 counterpart to pcsx2/x86/iR5900.h. It is intentionally small
// for now (Phase 1 skeleton): it declares the recompiler entry points and pins
// down the persistent host-register assignments so that every later phase
// (load/store, arithmetic, branches, ...) uses the same map. See
// arm64-port/CONVENTIONS.md §2 for the rationale.

#include "R5900.h"

#include "arm64/AsmHelpers.h"

#include <cstddef>

// --------------------------------------------------------------------------------------
//  Persistent (callee-saved) host registers for hot EE state
// --------------------------------------------------------------------------------------
// Chosen from the AAPCS64 callee-saved GPR set (x19-x28) so they survive C ABI
// calls into the interpreter/helpers without extra save/restore. Reserve them
// here once; do not reuse them as scratch inside generators.
//
//   x19 = &cpuRegs              (base for all guest GPR / PC / HI/LO accesses)
//   x20 = EE GPR cache register (was reserved for a 4GB fastmem base that was never
//                                wired up — the vtlb vmap path via x21 is the fast path;
//                                see REC_GPR_CACHE_REGS in aR5900.cpp)
//   x21 = vtlb table base       (assigned for real in Phase 2)
//
#define RESTATEPTR vixl::aarch64::x19
#define REVTLBPTR vixl::aarch64::x21

// --------------------------------------------------------------------------------------
//  Slow-path EE memory access codegen (Phase 2)
// --------------------------------------------------------------------------------------
// Emit a call to the C++ vtlb_memRead / vtlb_memWrite helpers — semantically
// identical to the interpreter's load/store ops (same virtual-memory path, so
// these are correct by construction). The fastmem fast path (direct access via
// REFASTMEMBASE + SIGSEGV backpatch through vtlb_DynBackpatchLoadStore) is Phase 2.2.
//
//   bits  : 8 / 16 / 32 / 64 (use the *Quad helpers for 128-bit).
//   sign  : sign-extend (true) vs zero-extend (false) the loaded value into the GPR.
//   addr  : 32-bit guest address (the W view of the register is used).
//   dst   : destination guest GPR; result is extended to the full 64-bit X view.
//   data  : value to store (X view for 64-bit, W view otherwise; Q view for quad).
//
// These clobber the AAPCS64 caller-saved set (x0-x17, v0-v7, v16-v31) and LR. The
// persistent state regs (x19-x21) are callee-saved and survive. Once the EE rec
// has a register allocator (Phase 3) it must flush live caller-saved guest state
// before invoking these.
void armEmitVtlbRead(u32 bits, bool sign, const vixl::aarch64::Register& dst, const vixl::aarch64::Register& addr);
void armEmitVtlbWrite(u32 bits, const vixl::aarch64::Register& addr, const vixl::aarch64::Register& data);
void armEmitVtlbReadQuad(const vixl::aarch64::VRegister& dst, const vixl::aarch64::Register& addr);
void armEmitVtlbWriteQuad(const vixl::aarch64::Register& addr, const vixl::aarch64::VRegister& data);

// --------------------------------------------------------------------------------------
//  EE GPR load/store opcode generators (Phase 2.3)
// --------------------------------------------------------------------------------------
// The first vertical slice that turns decoded MIPS load/store ops into ARM64 that
// reads/writes guest memory. These take the simple, non-allocating path: every
// guest GPR is read from / written back to cpuRegs in memory (via RESTATEPTR =
// &cpuRegs) around the access — there is no register allocator yet (Phase 3). The
// access itself goes through the slow-path armEmitVtlbRead/Write helpers above.
//
// Fields are passed explicitly (decoded from cpuRegs.code by the caller) so the
// generators are independent of any opcode-table wiring and are unit-testable.
//
//   armEmitEffectiveAddr: dst.W() = GPR[rs].UL[0] + imm  (the EE address mode).
//   armEmitLoadGpr:  GPR[rt] = sign/zero-extend(mem[addr]); skips write-back for rt==0.
//   armEmitStoreGpr: mem[addr] = GPR[rt] (low `bits` bits).
//
// bits = 8/16/32/64; imm is the sign-extended 16-bit MIPS immediate.

// Byte offset of guest GPR `n`'s low word within cpuRegs (GPR is the first member;
// each GPR_reg is 128 bits wide).
static constexpr u32 EE_GPR_OFFSET(u32 n) { return n * 16u; }

// Byte offset of cpuRegs.pc (the next-PC field the branch/jump generators write).
static constexpr u32 EE_PC_OFFSET = static_cast<u32>(offsetof(cpuRegisters, pc));

// Byte offset of cpuRegs.code (the current-instruction field the interpreter reads).
static constexpr u32 EE_CODE_OFFSET = static_cast<u32>(offsetof(cpuRegisters, code));

void armEmitEffectiveAddr(const vixl::aarch64::Register& dst, u32 rs, s32 imm);
void armEmitLoadGpr(u32 bits, bool sign, u32 rt, u32 rs, s32 imm);
void armEmitStoreGpr(u32 bits, u32 rt, u32 rs, s32 imm);

// --------------------------------------------------------------------------------------
//  FPU (COP1) register-file offsets
// --------------------------------------------------------------------------------------
// fpuRegs lives in the same cpuRegistersPack as cpuRegs, with cpuRegs as the first
// (offset-0) member. RESTATEPTR is set to &cpuRegs, which is therefore also the base
// of the pack — so the whole FPU register file is reachable at a fixed offset from
// RESTATEPTR, exactly like the GPRs. fpr[]/ACC are 32-bit FPRreg slots; fprc[] is the
// 32-entry control-register array (fprc[31] = FCR31, the flags/status word).
static_assert(offsetof(cpuRegistersPack, cpuRegs) == 0, "cpuRegs must be first in the pack");
static constexpr u32 EE_FPU_BASE = static_cast<u32>(offsetof(cpuRegistersPack, fpuRegs));
static constexpr u32 EE_FPR_OFFSET(u32 n)
{
	return EE_FPU_BASE + static_cast<u32>(offsetof(fpuRegisters, fpr)) + n * static_cast<u32>(sizeof(FPRreg));
}
static constexpr u32 EE_FPRC_OFFSET(u32 n)
{
	return EE_FPU_BASE + static_cast<u32>(offsetof(fpuRegisters, fprc)) + n * static_cast<u32>(sizeof(u32));
}
// Byte offset of the FPU accumulator (ACC) — the destination of the ADDA/SUBA/MULA/
// MADDA/MSUBA ACC ops.
static constexpr u32 EE_ACC_OFFSET = EE_FPU_BASE + static_cast<u32>(offsetof(fpuRegisters, ACC));
// Byte offset of fpuRegs.ACCflag — the recompiler-internal "ACC overflowed" bit that
// full clamp mode's MADD/MSUB overflow propagation tests (x86 iFPUd recMaddsub).
static constexpr u32 EE_ACCFLAG_OFFSET = EE_FPU_BASE + static_cast<u32>(offsetof(fpuRegisters, ACCflag));

// --------------------------------------------------------------------------------------
//  FPU (COP1) exact-semantics opcode generators (Phase 5.2a)
// --------------------------------------------------------------------------------------
// The subset of COP1 that is pure bit/integer movement (no EE-specific float
// arithmetic quirks), so it is bit-exact against the interpreter on ARM64. The
// arithmetic ops (ADD_S/SUB_S/MUL_S/DIV_S/SQRT_S/compares/ACC ops/BC1) need the
// EE's non-IEEE rounding+clamp behaviour and stay on the interpreter for now.
//
//   MFC1 : GPR[rt].SD[0] = (s32)fpr[fs].UL    (sign-extend into 64-bit GPR; rt==0 skip)
//   MTC1 : fpr[fs].UL    = GPR[rt].UL[0]
//   CFC1 : GPR[rt].SD[0] = (fs==31) ? (s32)fprc[31] : (fs==0) ? 0x2E00 : 0   (rt==0 skip)
//   CTC1 : if (fs==31) fprc[31] = GPR[rt].UL[0]    (other fs are ignored)
//   MOV_S: fpr[fd].UL = fpr[fs].UL
//   ABS_S: fpr[fd].UL = fpr[fs].UL & 0x7fffffff;  clear FCR31 O|U flags
//   NEG_S: fpr[fd].UL = fpr[fs].UL ^ 0x80000000;  clear FCR31 O|U flags
//   LWC1 : fpr[ft].UL = mem32[GPR[rs].UL[0] + imm]
//   SWC1 : mem32[GPR[rs].UL[0] + imm] = fpr[ft].UL
void armEmitMFC1(u32 rt, u32 fs);
void armEmitMTC1(u32 fs, u32 rt);
void armEmitCFC1(u32 rt, u32 fs);
void armEmitCTC1(u32 fs, u32 rt);
void armEmitMOV_S(u32 fd, u32 fs);
void armEmitABS_S(u32 fd, u32 fs);
void armEmitNEG_S(u32 fd, u32 fs);
void armEmitLWC1(u32 ft, u32 rs, s32 imm);
void armEmitSWC1(u32 ft, u32 rs, s32 imm);

// --------------------------------------------------------------------------------------
//  FPU (COP1) float arithmetic opcode generators (Phase 5.2b)
// --------------------------------------------------------------------------------------
// These reproduce the EE FPU's *non-IEEE* float behaviour, mirroring the interpreter
// (pcsx2/FPU.cpp — the project's ground truth and the current fallback):
//   - inputs run through fpuDouble(): denormals/zeros flush to signed zero, inf/NaN
//     clamp to signed fmax (0x7f7fffff);
//   - the single-precision result runs through checkOverflow (inf -> signed fmax) and
//     checkUnderflow (denormal -> signed zero), updating the FCR31 (fprc[31]) flags.
// The arithmetic itself is host single-precision NEON, which is bit-identical to the
// interpreter's `float OP float` (both IEEE round-to-nearest-even).
//
//   ADD_S/SUB_S/MUL_S : fpr[fd] = clamp(fpuDouble(fpr[fs]) OP fpuDouble(fpr[ft]))
//   DIV_S             : fpr[fd] = clamp(fpuDouble(fpr[fs]) / fpuDouble(fpr[ft]))
//   SQRT_S            : fpr[fd] = sqrt(abs(fpuDouble(fpr[ft]))) with signed-zero/invalid handling
//   RSQRT_S           : fpr[fd] = clamp(fpuDouble(fpr[fs]) / sqrt(abs(fpuDouble(fpr[ft]))))
//   ADDA_S/SUBA_S/MULA_S : ACC   = clamp(fpuDouble(fpr[fs]) OP fpuDouble(fpr[ft]))
void armEmitADD_S(u32 fd, u32 fs, u32 ft);
void armEmitSUB_S(u32 fd, u32 fs, u32 ft);
void armEmitMUL_S(u32 fd, u32 fs, u32 ft);
void armEmitDIV_S(u32 fd, u32 fs, u32 ft);
void armEmitSQRT_S(u32 fd, u32 ft);
void armEmitRSQRT_S(u32 fd, u32 fs, u32 ft);
void armEmitADDA_S(u32 fs, u32 ft);
void armEmitSUBA_S(u32 fs, u32 ft);
void armEmitMULA_S(u32 fs, u32 ft);
//   MADD_S/MSUB_S  : fpr[fd] = clamp(fpuDouble(ACC) OP fpuDouble(clamp(fs)*clamp(ft)))
//   MADDA_S/MSUBA_S: ACC     = clamp(ACC.f OP (clamp(fs)*clamp(ft)))  (raw ACC)
//   MAX_S/MIN_S    : fpr[fd] = integer fp_max/fp_min(fpr[fs], fpr[ft]); clears O|U
void armEmitMADD_S(u32 fd, u32 fs, u32 ft);
void armEmitMSUB_S(u32 fd, u32 fs, u32 ft);
void armEmitMADDA_S(u32 fs, u32 ft);
void armEmitMSUBA_S(u32 fs, u32 ft);
void armEmitMAX_S(u32 fd, u32 fs, u32 ft);
void armEmitMIN_S(u32 fd, u32 fs, u32 ft);
//   C.F/C.EQ/C.LT/C.LE : FCR31 C-bit = (fpuDouble(fs) cond fpuDouble(ft)); C.F always clears
//   CVT_W : fpr[fd] = (s32)float(fpr[fs]) with EE saturation; CVT_S : fpr[fd] = (float)(s32)fpr[fs]
void armEmitC_F(u32 fs, u32 ft);
void armEmitC_EQ(u32 fs, u32 ft);
void armEmitC_LT(u32 fs, u32 ft);
void armEmitC_LE(u32 fs, u32 ft);
void armEmitCVT_W(u32 fd, u32 fs);
void armEmitCVT_S(u32 fd, u32 fs);

// 128-bit LQ/SQ: the effective address is forced to 16-byte alignment and the
// whole 128-bit GPR is loaded/stored via the Quad vtlb helpers (NEON q access).
void armEmitLoadQuad(u32 rt, u32 rs, s32 imm);
void armEmitStoreQuad(u32 rt, u32 rs, s32 imm);

// --------------------------------------------------------------------------------------
//  Unaligned load/store opcode generators (LWL/LWR/SWL/SWR, LDL/LDR/SDL/SDR)
// --------------------------------------------------------------------------------------
// Bit-exact ports of the interpreter's byte-merge semantics (R5900OpcodeImpl.cpp):
// the aligned word/doubleword is read through the vtlb, merged with GPR[rt]
// according to the runtime low address bits, and (for the store forms) written
// back. These are heavily used in memcpy-style EE loops (GOW, NFS) and previously
// forced an interpreter single-step block per instruction.
void armEmitLWL(u32 rt, u32 rs, s32 imm);
void armEmitLWR(u32 rt, u32 rs, s32 imm);
void armEmitSWL(u32 rt, u32 rs, s32 imm);
void armEmitSWR(u32 rt, u32 rs, s32 imm);
void armEmitLDL(u32 rt, u32 rs, s32 imm);
void armEmitLDR(u32 rt, u32 rs, s32 imm);
void armEmitSDL(u32 rt, u32 rs, s32 imm);
void armEmitSDR(u32 rt, u32 rs, s32 imm);

// --------------------------------------------------------------------------------------
//  EE immediate arithmetic opcode generators (Phase 3.1)
// --------------------------------------------------------------------------------------
// Format: OP rt, rs, immediate (I-type). imm is sign-extended (_Imm_) for ADDI/… and
// zero-extended (_ImmU_) for ANDI/ORI/XORI. $zero writes are discarded.
//
// ADDI/ADDIU  : Rt = s32(GPR[rs].UL[0] + imm)   (add as 32-bit, sign-extend)
// DADDI/DADDIU: Rt = GPR[rs].UD[0] + (s64)imm   (64-bit add, no overflow check)
// SLTI        : Rt = (GPR[rs].SD[0] <  imm) ? 1 : 0
// SLTIU       : Rt = (GPR[rs].UD[0] < (u64)(s64)imm) ? 1 : 0
// ANDI        : Rt = GPR[rs].UD[0] & (u64)imm_u
// ORI         : Rt = GPR[rs].UD[0] | (u64)imm_u
// XORI        : Rt = GPR[rs].UD[0] ^ (u64)imm_u
// LUI         : Rt = s32(imm << 16)

void armEmitADDI(u32 rt, u32 rs, s32 imm);
void armEmitADDIU(u32 rt, u32 rs, s32 imm);
void armEmitDADDI(u32 rt, u32 rs, s32 imm);
void armEmitDADDIU(u32 rt, u32 rs, s32 imm);
void armEmitSLTI(u32 rt, u32 rs, s32 imm);
void armEmitSLTIU(u32 rt, u32 rs, s32 imm);
void armEmitANDI(u32 rt, u32 rs, u32 imm_u);
void armEmitORI(u32 rt, u32 rs, u32 imm_u);
void armEmitXORI(u32 rt, u32 rs, u32 imm_u);
void armEmitLUI(u32 rt, u32 imm);

// --------------------------------------------------------------------------------------
//  EE register-register arithmetic opcode generators (Phase 3.2)
// --------------------------------------------------------------------------------------
// Format: OP rd, rs, rt (R-type). All operations are on the low 64-bit GPR word;
// 32-bit results are sign-extended to 64. $zero writes are discarded.
void armEmitADD(u32 rd, u32 rs, u32 rt);
void armEmitADDU(u32 rd, u32 rs, u32 rt);
void armEmitDADD(u32 rd, u32 rs, u32 rt);
void armEmitDADDU(u32 rd, u32 rs, u32 rt);
void armEmitSUB(u32 rd, u32 rs, u32 rt);
void armEmitSUBU(u32 rd, u32 rs, u32 rt);
void armEmitDSUB(u32 rd, u32 rs, u32 rt);
void armEmitDSUBU(u32 rd, u32 rs, u32 rt);
void armEmitAND(u32 rd, u32 rs, u32 rt);
void armEmitOR(u32 rd, u32 rs, u32 rt);
void armEmitXOR(u32 rd, u32 rs, u32 rt);
void armEmitNOR(u32 rd, u32 rs, u32 rt);
void armEmitSLT(u32 rd, u32 rs, u32 rt);
void armEmitSLTU(u32 rd, u32 rs, u32 rt);

// --------------------------------------------------------------------------------------
//  EE shift opcode generators (Phase 3.3)
// --------------------------------------------------------------------------------------
// Format: OP rd, rt, sa (immediate shifts) or OP rd, rt, rs (variable shifts).
// 32-bit results are sign-extended to 64. $zero writes are discarded.
//
//   sa: 5-bit shift amount from the MIPS `sa` field (bits 10:6). DSLL32/DSRL32/DSRA32
//       add 32 to the effective amount.
//
//   rs: variable shift amount is read from GPR[rs].UL[0] (low word). The value is
//       masked by the shift width by ARM64 hardware (5 bits for W-reg, 6 for X-reg),
//       matching MIPS semantics.
void armEmitSLL(u32 rd, u32 rt, u32 sa);
void armEmitSRL(u32 rd, u32 rt, u32 sa);
void armEmitSRA(u32 rd, u32 rt, u32 sa);
void armEmitSLLV(u32 rd, u32 rt, u32 rs);
void armEmitSRLV(u32 rd, u32 rt, u32 rs);
void armEmitSRAV(u32 rd, u32 rt, u32 rs);
void armEmitDSLLV(u32 rd, u32 rt, u32 rs);
void armEmitDSRLV(u32 rd, u32 rt, u32 rs);
void armEmitDSRAV(u32 rd, u32 rt, u32 rs);
void armEmitDSLL(u32 rd, u32 rt, u32 sa);
void armEmitDSRL(u32 rd, u32 rt, u32 sa);
void armEmitDSRA(u32 rd, u32 rt, u32 sa);
void armEmitDSLL32(u32 rd, u32 rt, u32 sa);
void armEmitDSRL32(u32 rd, u32 rt, u32 sa);
void armEmitDSRA32(u32 rd, u32 rt, u32 sa);

// --------------------------------------------------------------------------------------
//  EE move opcode generators (Phase 3.4)
// --------------------------------------------------------------------------------------
// Format: OP rd, rs, rt (R-type conditional moves) or OP rd, fs (HI/LO moves).
// $zero writes are discarded.
//
//   MOVZ: Rd = (Rt == 0) ? Rs : Rd   (conditional move if Rt equals zero)
//   MOVN: Rd = (Rt != 0) ? Rs : Rd   (conditional move if Rt non-zero)
//   MFHI: Rd = HI
//   MTHI: HI = Rs
//   MFLO: Rd = LO
//   MTLO: LO = Rs
void armEmitMOVZ(u32 rd, u32 rs, u32 rt);
void armEmitMOVN(u32 rd, u32 rs, u32 rt);
void armEmitMFHI(u32 rd);
void armEmitMTHI(u32 rs);
void armEmitMFLO(u32 rd);
void armEmitMTLO(u32 rs);

// --------------------------------------------------------------------------------------
//  EE multiply/divide opcode generators (Phase 3.5)
// --------------------------------------------------------------------------------------
// Format: OP rd, rs, rt (R-type). Results written to the HI/LO special registers
// (the R5900 has no DMULT/DDIV — those are not EE instructions and are omitted).
//
//   MULT    : HI/LO = (s64)(GPR[rs].SL[0] * GPR[rt].SL[0]); if rd!=0 GPR[rd]=LO
//   MULTU   : HI/LO = (u64)(GPR[rs].UL[0] * GPR[rt].UL[0]); if rd!=0 GPR[rd]=LO
//   DIV     : LO = quotient, HI = remainder (signed 32-bit, with overflow/div0 handling)
//   DIVU    : LO = quotient, HI = remainder (unsigned 32-bit, with div0 handling)
//
// The "1" variants (MMI group) run on the second multiplier pipeline: results go
// to the upper doubleword HI1/LO1 (HI.SD[1]/LO.SD[1]) and Rd reads LO.UD[1].

void armEmitMULT(u32 rd, u32 rs, u32 rt);
void armEmitMULTU(u32 rd, u32 rs, u32 rt);
void armEmitDIV(u32 rs, u32 rt);
void armEmitDIVU(u32 rs, u32 rt);

// MMI second-pipeline variants (HI1/LO1).
void armEmitMULT1(u32 rd, u32 rs, u32 rt);
void armEmitMULTU1(u32 rd, u32 rs, u32 rt);
void armEmitDIV1(u32 rs, u32 rt);
void armEmitDIVU1(u32 rs, u32 rt);

// --------------------------------------------------------------------------------------
//  EE jump opcode generators (Phase 4.1)
// --------------------------------------------------------------------------------------
// These emit ONLY the control-flow effect — the next-PC write (cpuRegs.pc) and, for
// the linking forms, the GPR[31]/GPR[rd] return-address write. The block compiler is
// responsible for compiling the delay-slot instruction and for terminating the block
// (RET back to the dispatcher loop, which re-reads cpuRegs.pc). Register-target jumps
// (JR/JALR) read GPR[rs] *before* the delay slot is compiled, so the generator must be
// invoked before the delay slot; the value lands in cpuRegs.pc immediately (the delay
// slot never touches pc, so the early write is safe).
//
// All address arguments are absolute and precomputed by the caller relative to the
// delay slot (= branchpc + 4):
//   target : J/JAL  -> (instr_index << 2) | ((branchpc + 4) & 0xF0000000)
//   linkpc : JAL/JALR return address = branchpc + 8 (zero-extended into GPR.UD[0]).
//
//   J    : cpuRegs.pc = target
//   JAL  : GPR[31].UD[0] = linkpc; cpuRegs.pc = target
//   JR   : cpuRegs.pc = GPR[rs].UL[0]
//   JALR : cpuRegs.pc = GPR[rs].UL[0]; if (rd) GPR[rd].UD[0] = linkpc  (rs read first)
void armEmitJ(u32 target);
void armEmitJAL(u32 target, u32 linkpc);
void armEmitJR(u32 rs);
void armEmitJALR(u32 rd, u32 rs, u32 linkpc);

// --------------------------------------------------------------------------------------
//  EE conditional branch opcode generators (Phase 4.2)
// --------------------------------------------------------------------------------------
// Same contract as the jumps: emit only the next-PC selection (and, for the *AL forms,
// the unconditional GPR[31] link). The condition is evaluated on the source GPR(s) read
// here, then cpuRegs.pc is set to `target` (branch taken) or `fallthrough` (not taken):
//   target      = (branchpc + 4) + (s16(imm) << 2)
//   fallthrough = branchpc + 8   (the instruction after the delay slot)
//
// Comparisons match the interpreter: BEQ/BNE compare the full 64-bit GPR[rs]/GPR[rt];
// the single-operand forms compare signed 64-bit GPR[rs] against zero. The *AL forms
// write the link *before* reading rs (matching the interpreter's _SetLink ordering).
void armEmitBEQ(u32 rs, u32 rt, u32 target, u32 fallthrough);
void armEmitBNE(u32 rs, u32 rt, u32 target, u32 fallthrough);
void armEmitBLTZ(u32 rs, u32 target, u32 fallthrough);
void armEmitBGEZ(u32 rs, u32 target, u32 fallthrough);
void armEmitBLEZ(u32 rs, u32 target, u32 fallthrough);
void armEmitBGTZ(u32 rs, u32 target, u32 fallthrough);
void armEmitBLTZAL(u32 rs, u32 target, u32 fallthrough, u32 linkpc);
void armEmitBGEZAL(u32 rs, u32 target, u32 fallthrough, u32 linkpc);
// COP1 FP-condition branches (test the FCR31 C bit). BC1F branches when C==0,
// BC1T when C!=0.
void armEmitBC1F(u32 target, u32 fallthrough);
void armEmitBC1T(u32 target, u32 fallthrough);

// COP2 VU0-macro condition branches (test VU0.VI[REG_VPU_STAT].UL & 0x100, the VBS0
// bit). BC2F branches when the bit is CLEAR, BC2T when SET. No VU sync/cycle commit
// (faithful to x86 microVU_Macro.inl recBC2F/T — a plain bit-test branch).
void armEmitBC2F(u32 target, u32 fallthrough);
void armEmitBC2T(u32 target, u32 fallthrough);

// --------------------------------------------------------------------------------------
//  Branch-likely forms (BEQL/BNEL/BLEZL/BGTZL, BLTZL/BGEZL, BC1FL/BC1TL)
// --------------------------------------------------------------------------------------
// Same next-PC selection as the normal forms, but the delay slot is NULLIFIED when
// the branch is not taken. The generator evaluates the condition, writes
// cpuRegs.pc = taken ? target : fallthrough, and returns the ARM64 condition that
// is true when the branch is TAKEN — with the flags still live, so the block
// compiler can emit `b.<inverted> skip` around the delay-slot code it compiles
// next. Returns the condition; `op` is the raw instruction word (the generator
// decodes the form itself). Only call for ops recIsLikelyBranch accepts.
vixl::aarch64::Condition armEmitBranchLikelyTest(u32 op, u32 target, u32 fallthrough);

// --------------------------------------------------------------------------------------
//  MMI 128-bit SIMD opcode generators (Phase 5.4)
// --------------------------------------------------------------------------------------
// Packed integer ops over the full 128-bit GPRs, mapped to ARM64 NEON. Format is
// OP rd, rs, rt (binary) or OP rd, rt (PABS*/PCPYH). $zero writes are discarded.
// Semantics mirror pcsx2/MMI.cpp; see aR5900MMI.cpp for the per-op NEON mapping.
//
//   PADD*/PSUB*       : wrapping add/subtract of word/halfword/byte lanes
//   PADDS*/PSUBS*     : signed-saturating add/subtract
//   PADDU*/PSUBU*     : unsigned-saturating add/subtract
//   PCGT*             : signed Rs > Rt -> all-ones lane mask
//   PCEQ*             : Rs == Rt -> all-ones lane mask
//   PMAX*/PMIN*       : signed lane max/min (word + halfword)
//   PABSW/PABSH       : saturating absolute value of Rt
//   PAND/POR/PXOR/PNOR: 128-bit bitwise logic
//   PEXTL*/PEXTU*     : interleave low/high halves of Rt and Rs
//   PPAC*             : pack even-indexed lanes of Rt then Rs
//   PCPYLD/PCPYUD     : combine low/high doublewords of Rt and Rs
//   PCPYH             : broadcast Rt.US[0]/US[4] into the low/high doublewords
void armEmitPADDW(u32 rd, u32 rs, u32 rt);
void armEmitPADDH(u32 rd, u32 rs, u32 rt);
void armEmitPADDB(u32 rd, u32 rs, u32 rt);
void armEmitPSUBW(u32 rd, u32 rs, u32 rt);
void armEmitPSUBH(u32 rd, u32 rs, u32 rt);
void armEmitPSUBB(u32 rd, u32 rs, u32 rt);
void armEmitPADDSW(u32 rd, u32 rs, u32 rt);
void armEmitPADDSH(u32 rd, u32 rs, u32 rt);
void armEmitPADDSB(u32 rd, u32 rs, u32 rt);
void armEmitPSUBSW(u32 rd, u32 rs, u32 rt);
void armEmitPSUBSH(u32 rd, u32 rs, u32 rt);
void armEmitPSUBSB(u32 rd, u32 rs, u32 rt);
void armEmitPADDUW(u32 rd, u32 rs, u32 rt);
void armEmitPADDUH(u32 rd, u32 rs, u32 rt);
void armEmitPADDUB(u32 rd, u32 rs, u32 rt);
void armEmitPSUBUW(u32 rd, u32 rs, u32 rt);
void armEmitPSUBUH(u32 rd, u32 rs, u32 rt);
void armEmitPSUBUB(u32 rd, u32 rs, u32 rt);
void armEmitPCGTW(u32 rd, u32 rs, u32 rt);
void armEmitPCGTH(u32 rd, u32 rs, u32 rt);
void armEmitPCGTB(u32 rd, u32 rs, u32 rt);
void armEmitPCEQW(u32 rd, u32 rs, u32 rt);
void armEmitPCEQH(u32 rd, u32 rs, u32 rt);
void armEmitPCEQB(u32 rd, u32 rs, u32 rt);
void armEmitPMAXW(u32 rd, u32 rs, u32 rt);
void armEmitPMAXH(u32 rd, u32 rs, u32 rt);
void armEmitPMINW(u32 rd, u32 rs, u32 rt);
void armEmitPMINH(u32 rd, u32 rs, u32 rt);
void armEmitPAND(u32 rd, u32 rs, u32 rt);
void armEmitPOR(u32 rd, u32 rs, u32 rt);
void armEmitPXOR(u32 rd, u32 rs, u32 rt);
void armEmitPNOR(u32 rd, u32 rs, u32 rt);
void armEmitPEXTLW(u32 rd, u32 rs, u32 rt);
void armEmitPEXTLH(u32 rd, u32 rs, u32 rt);
void armEmitPEXTLB(u32 rd, u32 rs, u32 rt);
void armEmitPEXTUW(u32 rd, u32 rs, u32 rt);
void armEmitPEXTUH(u32 rd, u32 rs, u32 rt);
void armEmitPEXTUB(u32 rd, u32 rs, u32 rt);
void armEmitPPACW(u32 rd, u32 rs, u32 rt);
void armEmitPPACH(u32 rd, u32 rs, u32 rt);
void armEmitPPACB(u32 rd, u32 rs, u32 rt);
void armEmitPCPYLD(u32 rd, u32 rs, u32 rt);
void armEmitPCPYUD(u32 rd, u32 rs, u32 rt);
void armEmitPABSW(u32 rd, u32 rt);
void armEmitPABSH(u32 rd, u32 rt);
void armEmitPCPYH(u32 rd, u32 rt);

// Parallel shifts by immediate (Phase 5.4 continuation). Each lane is shifted
// independently by the same immediate amount `sa` (masked by the lane width):
//   PSLLH/PSLLW — logical shift left (zero-fill out bits)
//   PSRLH/PSRLW — logical shift right (zero-fill from left)
//   PSRAH/PSRAW — arithmetic shift right (sign-extend from left)
void armEmitPSLLH(u32 rd, u32 rt, u32 sa);
void armEmitPSLLW(u32 rd, u32 rt, u32 sa);
void armEmitPSRLH(u32 rd, u32 rt, u32 sa);
void armEmitPSRLW(u32 rd, u32 rt, u32 sa);
void armEmitPSRAH(u32 rd, u32 rt, u32 sa);
void armEmitPSRAW(u32 rd, u32 rt, u32 sa);

// Parallel lane permutes (Phase 5.4 continuation). These rearrange the halfword/word
// lanes within the 128-bit GPR. Mapped to NEON interleave/reverse instructions.
//   PINTH  : interleave low half of Rt with high half of Rs (halfwords)
//   PINTEH : interleave even-indexed halfwords of Rt and Rs
//   PEXEH  : extract/reverse even halfwords within each 64-bit half
//   PEXEW  : extract/reverse even words (swap word pairs)
//   PREVH  : reverse halfwords within each 64-bit half
void armEmitPINTH(u32 rd, u32 rs, u32 rt);
void armEmitPINTEH(u32 rd, u32 rs, u32 rt);
void armEmitPEXEH(u32 rd, u32 rt);
void armEmitPEXEW(u32 rd, u32 rt);
void armEmitPREVH(u32 rd, u32 rt);

// Remaining permutes (Phase 5.4 continuation).
//   PROT3W : rotate 3 words (Rt-only, {Rt[1], Rt[2], Rt[0], Rt[3]})
//   PEXCH  : extract even halfwords (swap halfword pairs 1<->2 within each 64-bit half)
//   PEXCW  : extract even words (swap word pairs 1<->2)
void armEmitPROT3W(u32 rd, u32 rt);
void armEmitPEXCH(u32 rd, u32 rt);
void armEmitPEXCW(u32 rd, u32 rt);

// Parallel variable shifts (Phase 5.4 continuation).
//   PSLLVW : parallel logical shift left by GPR[rs] (per-lane amount, 5-bit masked)
//   PSRLVW : parallel logical shift right by GPR[rs] (zero-fill)
//   PSRAVW : parallel arithmetic shift right by GPR[rs] (sign-extend)
void armEmitPSLLVW(u32 rd, u32 rs, u32 rt);
void armEmitPSRLVW(u32 rd, u32 rs, u32 rt);
void armEmitPSRAVW(u32 rd, u32 rs, u32 rt);

// --------------------------------------------------------------------------------------
//  MMI multiply-accumulate opcode generators (Phase 5.4 continuation)
// --------------------------------------------------------------------------------------
// These ops multiply pairs of elements and accumulate into the HI/LO special
// registers. Some also write the result to GPR[rd].
//
// Word multiply-accumulate (lanes 0 and 2):
//   PMULTW   : HI/LO = Rs * Rt (signed 32x32->64 per lane)
//   PMULTUW  : HI/LO = Rs * Rt (unsigned 32x32->64 per lane)
//   PMADDW   : HI/LO = Rs*Rt + (HI<<32) with EE division voodoo (signed)
//   PMSUBW   : HI/LO = (HI<<32) - Rs*Rt (signed)
//
// Halfword multiply-accumulate (8 lanes, alternating LO/HI):
//   PMULTH   : HI/LO = Rs * Rt (signed 16x16->32, 4 lanes x 2)
//   PMADDH   : LO/HI = Rs*Rt + LO/HI (signed 16x16->32, 8 lanes)
//   PMSUBH   : LO/HI = LO/HI - Rs*Rt (signed 16x16->32, 8 lanes)
//   PHMADH   : HI/LO = Rs[n]*Rt[n] + Rs[n+1]*Rt[n+1] + HI/LO (8 lanes)
//   PHMSBH   : HI/LO = Rs[n]*Rt[n] - Rs[n+1]*Rt[n+1] + HI/LO (8 lanes)
void armEmitPMULTW(u32 rd, u32 rs, u32 rt);
void armEmitPMULTUW(u32 rd, u32 rs, u32 rt);
void armEmitPMADDW(u32 rd, u32 rs, u32 rt);
void armEmitPMADDUW(u32 rd, u32 rs, u32 rt);
void armEmitPMSUBW(u32 rd, u32 rs, u32 rt);
void armEmitPMULTH(u32 rd, u32 rs, u32 rt);
void armEmitPMADDH(u32 rd, u32 rs, u32 rt);
void armEmitPMSUBH(u32 rd, u32 rs, u32 rt);
void armEmitPHMADH(u32 rd, u32 rs, u32 rt);
void armEmitPHMSBH(u32 rd, u32 rs, u32 rt);

// MMI HI/LO special register moves (Phase 5.4 continuation).
// These access the HI/LO registers (full 128-bit for MMI variants):
//   PMFHI : Rd = HI       (full 128-bit)
//   PMFLO : Rd = LO       (full 128-bit)
//   PMTHI : HI = Rs       (full 128-bit)
//   PMTLO : LO = Rs       (full 128-bit)
void armEmitPMFHI(u32 rd);
void armEmitPMFLO(u32 rd);
void armEmitPMTHI(u32 rs);
void armEmitPMTLO(u32 rs);

// Remaining MMI misc ops (Phase 5.4 completion).
//   PLZCW  : count leading sign bits (ARM64 CLS) per 32-bit lane
//   PADSBH : subtract low 4 halfwords, add high 4 (wrapping, no saturation)
//   PEXT5  : expand four 5-bit fields to 8-bit fields per lane
//   PPAC5  : compress four 8-bit fields to 5-bit fields per lane
//   PMFHL  : move from HI/LO (5 variants: LW/UW/SLW/LH/SH). Returns false for an
//            unhandled variant so the dispatcher falls back to the interpreter.
//   PMTHL  : move to HI/LO (sa=0 only)
// QFSRV is intentionally absent: its shift amount comes from the runtime SA
// register (cpuRegs.sa), not the instruction, so it stays on the interpreter.
void armEmitPLZCW(u32 rd, u32 rs);
void armEmitPADSBH(u32 rd, u32 rs, u32 rt);
void armEmitPEXT5(u32 rd, u32 rt);
void armEmitPPAC5(u32 rd, u32 rt);
bool armEmitPMFHL(u32 rd, u32 sa);
void armEmitPMTHL(u32 rs, u32 sa);
