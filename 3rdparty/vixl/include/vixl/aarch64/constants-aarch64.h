// Copyright 2015, VIXL authors
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//   * Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//   * Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//   * Neither the name of ARM Limited nor the names of its contributors may be
//     used to endorse or promote products derived from this software without
//     specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS CONTRIBUTORS "AS IS" AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef VIXL_AARCH64_CONSTANTS_AARCH64_H_
#define VIXL_AARCH64_CONSTANTS_AARCH64_H_

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#endif

#include "../globals-vixl.h"

namespace vixl {
namespace aarch64 {

const unsigned kNumberOfRegisters = 32;
const unsigned kNumberOfVRegisters = 32;
const unsigned kNumberOfZRegisters = kNumberOfVRegisters;
const unsigned kNumberOfPRegisters = 16;
// Callee saved registers are x21-x30(lr).
const int kNumberOfCalleeSavedRegisters = 10;
const int kFirstCalleeSavedRegisterIndex = 21;
// Callee saved FP registers are d8-d15. Note that the high parts of v8-v15 are
// still caller-saved.
const int kNumberOfCalleeSavedFPRegisters = 8;
const int kFirstCalleeSavedFPRegisterIndex = 8;
// All predicated instructions accept at least p0-p7 as the governing predicate.
const unsigned kNumberOfGoverningPRegisters = 8;

// clang-format off
#define AARCH64_P_REGISTER_CODE_LIST(R)                                        \
  R(0)  R(1)  R(2)  R(3)  R(4)  R(5)  R(6)  R(7)                               \
  R(8)  R(9)  R(10) R(11) R(12) R(13) R(14) R(15)

#define AARCH64_REGISTER_CODE_LIST(R)                                          \
  R(0)  R(1)  R(2)  R(3)  R(4)  R(5)  R(6)  R(7)                               \
  R(8)  R(9)  R(10) R(11) R(12) R(13) R(14) R(15)                              \
  R(16) R(17) R(18) R(19) R(20) R(21) R(22) R(23)                              \
  R(24) R(25) R(26) R(27) R(28) R(29) R(30) R(31)

// SVE loads and stores use "w" instead of "s" for word-sized accesses, so the
// mapping from the load/store variant to constants like k*RegSize is irregular.
#define VIXL_SVE_LOAD_STORE_VARIANT_LIST(V) \
  V(b, B)                            \
  V(h, H)                            \
  V(w, S)                            \
  V(d, D)

// Sign-extending loads don't have double-word variants.
#define VIXL_SVE_LOAD_STORE_SIGNED_VARIANT_LIST(V) \
  V(b, B)                            \
  V(h, H)                            \
  V(w, S)

#define INSTRUCTION_FIELDS_LIST(V_)                                          \
/* Register fields */                                                        \
V_(Rd, 4, 0, ExtractBits)         /* Destination register.                */ \
V_(Rn, 9, 5, ExtractBits)         /* First source register.               */ \
V_(Rm, 20, 16, ExtractBits)       /* Second source register.              */ \
V_(RmLow16, 19, 16, ExtractBits)  /* Second source register (code 0-15).  */ \
V_(Ra, 14, 10, ExtractBits)       /* Third source register.               */ \
V_(Rt, 4, 0, ExtractBits)         /* Load/store register.                 */ \
V_(Rt2, 14, 10, ExtractBits)      /* Load/store second register.          */ \
V_(Rs, 20, 16, ExtractBits)       /* Exclusive access status.             */ \
V_(Pt, 3, 0, ExtractBits)         /* Load/store register (p0-p7).         */ \
V_(Pd, 3, 0, ExtractBits)         /* SVE destination predicate register.  */ \
V_(Pn, 8, 5, ExtractBits)         /* SVE first source predicate register. */ \
V_(Pm, 19, 16, ExtractBits)       /* SVE second source predicate register.*/ \
V_(PgLow8, 12, 10, ExtractBits)   /* Governing predicate (p0-p7).         */ \
                                                                             \
/* Common bits */                                                            \
V_(SixtyFourBits, 31, 31, ExtractBits)                                       \
V_(FlagsUpdate, 29, 29, ExtractBits)                                         \
                                                                             \
/* PC relative addressing */                                                 \
V_(ImmPCRelHi, 23, 5, ExtractSignedBits)                                     \
V_(ImmPCRelLo, 30, 29, ExtractBits)                                          \
                                                                             \
/* Add/subtract/logical shift register */                                    \
V_(ShiftDP, 23, 22, ExtractBits)                                             \
V_(ImmDPShift, 15, 10, ExtractBits)                                          \
                                                                             \
/* Add/subtract immediate */                                                 \
V_(ImmAddSub, 21, 10, ExtractBits)                                           \
V_(ImmAddSubShift, 22, 22, ExtractBits)                                      \
                                                                             \
/* Add/substract extend */                                                   \
V_(ImmExtendShift, 12, 10, ExtractBits)                                      \
V_(ExtendMode, 15, 13, ExtractBits)                                          \
                                                                             \
/* Move wide */                                                              \
V_(ImmMoveWide, 20, 5, ExtractBits)                                          \
V_(ShiftMoveWide, 22, 21, ExtractBits)                                       \
                                                                             \
/* Logical immediate, bitfield and extract */                                \
V_(BitN, 22, 22, ExtractBits)                                                \
V_(ImmRotate, 21, 16, ExtractBits)                                           \
V_(ImmSetBits, 15, 10, ExtractBits)                                          \
V_(ImmR, 21, 16, ExtractBits)                                                \
V_(ImmS, 15, 10, ExtractBits)                                                \
                                                                             \
/* Test and branch immediate */                                              \
V_(ImmTestBranch, 18, 5, ExtractSignedBits)                                  \
V_(ImmTestBranchBit40, 23, 19, ExtractBits)                                  \
V_(ImmTestBranchBit5, 31, 31, ExtractBits)                                   \
                                                                             \
/* Conditionals */                                                           \
V_(Condition, 15, 12, ExtractBits)                                           \
V_(ConditionBranch, 3, 0, ExtractBits)                                       \
V_(Nzcv, 3, 0, ExtractBits)                                                  \
V_(ImmCondCmp, 20, 16, ExtractBits)                                          \
V_(ImmCondBranch, 23, 5, ExtractSignedBits)                                  \
                                                                             \
/* Floating point */                                                         \
V_(FPType, 23, 22, ExtractBits)                                              \
V_(ImmFP, 20, 13, ExtractBits)                                               \
V_(FPScale, 15, 10, ExtractBits)                                             \
                                                                             \
/* Load Store */                                                             \
V_(ImmLS, 20, 12, ExtractSignedBits)                                         \
V_(ImmLSUnsigned, 21, 10, ExtractBits)                                       \
V_(ImmLSPair, 21, 15, ExtractSignedBits)                                     \
V_(ImmShiftLS, 12, 12, ExtractBits)                                          \
V_(LSOpc, 23, 22, ExtractBits)                                               \
V_(LSVector, 26, 26, ExtractBits)                                            \
V_(LSSize, 31, 30, ExtractBits)                                              \
V_(ImmPrefetchOperation, 4, 0, ExtractBits)                                  \
V_(PrefetchHint, 4, 3, ExtractBits)                                          \
V_(PrefetchTarget, 2, 1, ExtractBits)                                        \
V_(PrefetchStream, 0, 0, ExtractBits)                                        \
V_(ImmLSPACHi, 22, 22, ExtractSignedBits)                                    \
V_(ImmLSPACLo, 20, 12, ExtractBits)                                          \
                                                                             \
/* Other immediates */                                                       \
V_(ImmUncondBranch, 25, 0, ExtractSignedBits)                                \
V_(ImmCmpBranch, 23, 5, ExtractSignedBits)                                   \
V_(ImmLLiteral, 23, 5, ExtractSignedBits)                                    \
V_(ImmException, 20, 5, ExtractBits)                                         \
V_(ImmHint, 11, 5, ExtractBits)                                              \
V_(ImmBarrierDomain, 11, 10, ExtractBits)                                    \
V_(ImmBarrierType, 9, 8, ExtractBits)                                        \
V_(ImmUdf, 15, 0, ExtractBits)                                               \
                                                                             \
/* System (MRS, MSR, SYS) */                                                 \
V_(ImmSystemRegister, 20, 5, ExtractBits)                                    \
V_(SysO0, 19, 19, ExtractBits)                                               \
V_(SysOp, 18, 5, ExtractBits)                                                \
V_(SysOp0, 20, 19, ExtractBits)                                              \
V_(SysOp1, 18, 16, ExtractBits)                                              \
V_(SysOp2, 7, 5, ExtractBits)                                                \
V_(CRn, 15, 12, ExtractBits)                                                 \
V_(CRm, 11, 8, ExtractBits)                                                  \
V_(ImmRMIFRotation, 20, 15, ExtractBits)                                     \
                                                                             \
/* Load-/store-exclusive */                                                  \
V_(LdStXLoad, 22, 22, ExtractBits)                                           \
V_(LdStXNotExclusive, 23, 23, ExtractBits)                                   \
V_(LdStXAcquireRelease, 15, 15, ExtractBits)                                 \
V_(LdStXSizeLog2, 31, 30, ExtractBits)                                       \
V_(LdStXPair, 21, 21, ExtractBits)                                           \
                                                                             \
/* NEON generic fields */                                                    \
V_(NEONQ, 30, 30, ExtractBits)                                               \
V_(NEONSize, 23, 22, ExtractBits)                                            \
V_(NEONLSSize, 11, 10, ExtractBits)                                          \
V_(NEONS, 12, 12, ExtractBits)                                               \
V_(NEONL, 21, 21, ExtractBits)                                               \
V_(NEONM, 20, 20, ExtractBits)                                               \
V_(NEONH, 11, 11, ExtractBits)                                               \
V_(ImmNEONExt, 14, 11, ExtractBits)                                          \
V_(ImmNEON5, 20, 16, ExtractBits)                                            \
V_(ImmNEON4, 14, 11, ExtractBits)                                            \
                                                                             \
/* NEON extra fields */                                                      \
V_(ImmRotFcadd, 12, 12, ExtractBits)                                         \
V_(ImmRotFcmlaVec, 12, 11, ExtractBits)                                      \
V_(ImmRotFcmlaSca, 14, 13, ExtractBits)                                      \
                                                                             \
/* NEON Modified Immediate fields */                                         \
V_(ImmNEONabc, 18, 16, ExtractBits)                                          \
V_(ImmNEONdefgh, 9, 5, ExtractBits)                                          \
V_(NEONModImmOp, 29, 29, ExtractBits)                                        \
V_(NEONCmode, 15, 12, ExtractBits)                                           \
                                                                             \
/* NEON Shift Immediate fields */                                            \
V_(ImmNEONImmhImmb, 22, 16, ExtractBits)                                     \
V_(ImmNEONImmh, 22, 19, ExtractBits)                                         \
V_(ImmNEONImmb, 18, 16, ExtractBits)                                         \
                                                                             \
/* SVE generic fields */                                                     \
V_(SVESize, 23, 22, ExtractBits)                                             \
V_(ImmSVEVLScale, 10, 5, ExtractSignedBits)                                  \
V_(ImmSVEIntWideSigned, 12, 5, ExtractSignedBits)                            \
V_(ImmSVEIntWideUnsigned, 12, 5, ExtractBits)                                \
V_(ImmSVEPredicateConstraint, 9, 5, ExtractBits)                             \
                                                                             \
/* SVE Bitwise Immediate bitfield */                                         \
V_(SVEBitN, 17, 17, ExtractBits)                                             \
V_(SVEImmRotate, 16, 11, ExtractBits)                                        \
V_(SVEImmSetBits, 10, 5, ExtractBits)                                        \
                                                                             \
V_(SVEImmPrefetchOperation, 3, 0, ExtractBits)                               \
V_(SVEPrefetchHint, 3, 3, ExtractBits)

// clang-format on

#define SYSTEM_REGISTER_FIELDS_LIST(V_, M_) \
  /* NZCV */                                \
  V_(Flags, 31, 28, ExtractBits)            \
  V_(N, 31, 31, ExtractBits)                \
  V_(Z, 30, 30, ExtractBits)                \
  V_(C, 29, 29, ExtractBits)                \
  V_(V, 28, 28, ExtractBits)                \
  M_(NZCV, Flags_mask)                      \
  /* FPCR */                                \
  V_(AHP, 26, 26, ExtractBits)              \
  V_(DN, 25, 25, ExtractBits)               \
  V_(FZ, 24, 24, ExtractBits)               \
  V_(RMode, 23, 22, ExtractBits)            \
  M_(FPCR, AHP_mask | DN_mask | FZ_mask | RMode_mask)

// Fields offsets.
#define DECLARE_FIELDS_OFFSETS(Name, HighBit, LowBit, X) \
  const int Name##_offset = LowBit;                      \
  const int Name##_width = HighBit - LowBit + 1;         \
  const uint32_t Name##_mask = ((1 << Name##_width) - 1) << LowBit;
#define NOTHING(A, B)
INSTRUCTION_FIELDS_LIST(DECLARE_FIELDS_OFFSETS)
SYSTEM_REGISTER_FIELDS_LIST(DECLARE_FIELDS_OFFSETS, NOTHING)
#undef NOTHING
#undef DECLARE_FIELDS_BITS

// ImmPCRel is a compound field (not present in INSTRUCTION_FIELDS_LIST), formed
// from ImmPCRelLo and ImmPCRelHi.
const int ImmPCRel_mask = ImmPCRelLo_mask | ImmPCRelHi_mask;

// Disable `clang-format` for the `enum`s below. We care about the manual
// formatting that `clang-format` would destroy.
// clang-format off

// Condition codes.
enum Condition {
  eq = 0,   // Z set            Equal.
  ne = 1,   // Z clear          Not equal.
  cs = 2,   // C set            Carry set.
  cc = 3,   // C clear          Carry clear.
  mi = 4,   // N set            Negative.
  pl = 5,   // N clear          Positive or zero.
  vs = 6,   // V set            Overflow.
  vc = 7,   // V clear          No overflow.
  hi = 8,   // C set, Z clear   Unsigned higher.
  ls = 9,   // C clear or Z set Unsigned lower or same.
  ge = 10,  // N == V           Greater or equal.
  lt = 11,  // N != V           Less than.
  gt = 12,  // Z clear, N == V  Greater than.
  le = 13,  // Z set or N != V  Less then or equal
  al = 14,  //                  Always.
  nv = 15,  // Behaves as always/al.

  // Aliases.
  hs = cs,  // C set            Unsigned higher or same.
  lo = cc,  // C clear          Unsigned lower.

  // Floating-point additional condition code.
  uo,       // Unordered comparison.

  // SVE predicate condition aliases.
  sve_none  = eq,  // No active elements were true.
  sve_any   = ne,  // An active element was true.
  sve_nlast = cs,  // The last element was not true.
  sve_last  = cc,  // The last element was true.
  sve_first = mi,  // The first element was true.
  sve_nfrst = pl,  // The first element was not true.
  sve_pmore = hi,  // An active element was true but not the last element.
  sve_plast = ls,  // The last active element was true or no active elements were true.
  sve_tcont = ge,  // CTERM termination condition not deleted.
  sve_tstop = lt   // CTERM termination condition deleted.
};

inline Condition InvertCondition(Condition cond) {
  // Conditions al and nv behave identically, as "always true". They can't be
  // inverted, because there is no "always false" condition.
  VIXL_ASSERT((cond != al) && (cond != nv));
  return static_cast<Condition>(cond ^ 1);
}

enum FPTrapFlags {
  EnableTrap   = 1,
  DisableTrap = 0
};

enum FlagsUpdate {
  SetFlags   = 1,
  LeaveFlags = 0
};

enum StatusFlags {
  NoFlag    = 0,

  // Derive the flag combinations from the system register bit descriptions.
  NFlag     = N_mask,
  ZFlag     = Z_mask,
  CFlag     = C_mask,
  VFlag     = V_mask,
  NZFlag    = NFlag | ZFlag,
  NCFlag    = NFlag | CFlag,
  NVFlag    = NFlag | VFlag,
  ZCFlag    = ZFlag | CFlag,
  ZVFlag    = ZFlag | VFlag,
  CVFlag    = CFlag | VFlag,
  NZCFlag   = NFlag | ZFlag | CFlag,
  NZVFlag   = NFlag | ZFlag | VFlag,
  NCVFlag   = NFlag | CFlag | VFlag,
  ZCVFlag   = ZFlag | CFlag | VFlag,
  NZCVFlag  = NFlag | ZFlag | CFlag | VFlag,

  // Floating-point comparison results.
  FPEqualFlag       = ZCFlag,
  FPLessThanFlag    = NFlag,
  FPGreaterThanFlag = CFlag,
  FPUnorderedFlag   = CVFlag,

  // SVE condition flags.
  SVEFirstFlag   = NFlag,
  SVENoneFlag    = ZFlag,
  SVENotLastFlag = CFlag
};

enum Shift {
  NO_SHIFT = -1,
  LSL = 0x0,
  LSR = 0x1,
  ASR = 0x2,
  ROR = 0x3,
  MSL = 0x4
};

enum Extend {
  NO_EXTEND = -1,
  UXTB      = 0,
  UXTH      = 1,
  UXTW      = 2,
  UXTX      = 3,
  SXTB      = 4,
  SXTH      = 5,
  SXTW      = 6,
  SXTX      = 7
};

enum SVEOffsetModifier {
  NO_SVE_OFFSET_MODIFIER,
  // Multiply (each element of) the offset by either the vector or predicate
  // length, according to the context.
  SVE_MUL_VL,
  // Shift or extend modifiers (as in `Shift` or `Extend`).
  SVE_LSL,
  SVE_UXTW,
  SVE_SXTW
};

enum SystemHint {
  NOP    = 0,
  YIELD  = 1,
  WFE    = 2,
  WFI    = 3,
  SEV    = 4,
  SEVL   = 5,
  ESB    = 16,
  CSDB   = 20,
  BTI    = 32,
  BTI_c  = 34,
  BTI_j  = 36,
  BTI_jc = 38
};

enum BranchTargetIdentifier {
  EmitBTI_none = NOP,
  EmitBTI = BTI,
  EmitBTI_c = BTI_c,
  EmitBTI_j = BTI_j,
  EmitBTI_jc = BTI_jc,

  // These correspond to the values of the CRm:op2 fields in the equivalent HINT
  // instruction.
  EmitPACIASP = 25,
  EmitPACIBSP = 27
};

enum BarrierDomain {
  OuterShareable = 0,
  NonShareable   = 1,
  InnerShareable = 2,
  FullSystem     = 3
};

enum BarrierType {
  BarrierOther  = 0,
  BarrierReads  = 1,
  BarrierWrites = 2,
  BarrierAll    = 3
};

enum PrefetchOperation {
  PLDL1KEEP = 0x00,
  PLDL1STRM = 0x01,
  PLDL2KEEP = 0x02,
  PLDL2STRM = 0x03,
  PLDL3KEEP = 0x04,
  PLDL3STRM = 0x05,

  PrfUnallocated06 = 0x06,
  PrfUnallocated07 = 0x07,

  PLIL1KEEP = 0x08,
  PLIL1STRM = 0x09,
  PLIL2KEEP = 0x0a,
  PLIL2STRM = 0x0b,
  PLIL3KEEP = 0x0c,
  PLIL3STRM = 0x0d,

  PrfUnallocated0e = 0x0e,
  PrfUnallocated0f = 0x0f,

  PSTL1KEEP = 0x10,
  PSTL1STRM = 0x11,
  PSTL2KEEP = 0x12,
  PSTL2STRM = 0x13,
  PSTL3KEEP = 0x14,
  PSTL3STRM = 0x15,

  PrfUnallocated16 = 0x16,
  PrfUnallocated17 = 0x17,
  PrfUnallocated18 = 0x18,
  PrfUnallocated19 = 0x19,
  PrfUnallocated1a = 0x1a,
  PrfUnallocated1b = 0x1b,
  PrfUnallocated1c = 0x1c,
  PrfUnallocated1d = 0x1d,
  PrfUnallocated1e = 0x1e,
  PrfUnallocated1f = 0x1f,
};

constexpr bool IsNamedPrefetchOperation(int op) {
  return ((op >= PLDL1KEEP) && (op <= PLDL3STRM)) ||
      ((op >= PLIL1KEEP) && (op <= PLIL3STRM)) ||
      ((op >= PSTL1KEEP) && (op <= PSTL3STRM));
}

enum BType {
  // Set when executing any instruction on a guarded page, except those cases
  // listed below.
  DefaultBType = 0,

  // Set when an indirect branch is taken from an unguarded page to a guarded
  // page, or from a guarded page to ip0 or ip1 (x16 or x17), eg "br ip0".
  BranchFromUnguardedOrToIP = 1,

  // Set when an indirect branch and link (call) is taken, eg. "blr x0".
  BranchAndLink = 2,

  // Set when an indirect branch is taken from a guarded page to a register
  // that is not ip0 or ip1 (x16 or x17), eg, "br x0".
  BranchFromGuardedNotToIP = 3
};

template<int op0, int op1, int crn, int crm, int op2>
class SystemRegisterEncoder {
 public:
  static const uint32_t value =
      ((op0 << SysO0_offset) |
       (op1 << SysOp1_offset) |
       (crn << CRn_offset) |
       (crm << CRm_offset) |
       (op2 << SysOp2_offset)) >> ImmSystemRegister_offset;
};

// System/special register names.
// This information is not encoded as one field but as the concatenation of
// multiple fields (Op0, Op1, Crn, Crm, Op2).
enum SystemRegister {
  NZCV = SystemRegisterEncoder<3, 3, 4, 2, 0>::value,
  FPCR = SystemRegisterEncoder<3, 3, 4, 4, 0>::value,
  RNDR = SystemRegisterEncoder<3, 3, 2, 4, 0>::value,    // Random number.
  RNDRRS = SystemRegisterEncoder<3, 3, 2, 4, 1>::value   // Reseeded random number.
};

template<int op1, int crn, int crm, int op2>
class CacheOpEncoder {
 public:
  static const uint32_t value =
      ((op1 << SysOp1_offset) |
       (crn << CRn_offset) |
       (crm << CRm_offset) |
       (op2 << SysOp2_offset)) >> SysOp_offset;
};

enum InstructionCacheOp {
  IVAU = CacheOpEncoder<3, 7, 5, 1>::value
};

enum DataCacheOp {
  CVAC = CacheOpEncoder<3, 7, 10, 1>::value,
  CVAU = CacheOpEncoder<3, 7, 11, 1>::value,
  CVAP = CacheOpEncoder<3, 7, 12, 1>::value,
  CVADP = CacheOpEncoder<3, 7, 13, 1>::value,
  CIVAC = CacheOpEncoder<3, 7, 14, 1>::value,
  ZVA = CacheOpEncoder<3, 7, 4, 1>::value,
  GVA = CacheOpEncoder<3, 7, 4, 3>::value,
  GZVA = CacheOpEncoder<3, 7, 4, 4>::value,
  CGVAC = CacheOpEncoder<3, 7, 10, 3>::value,
  CGDVAC = CacheOpEncoder<3, 7, 10, 5>::value,
  CGVAP = CacheOpEncoder<3, 7, 12, 3>::value,
  CGDVAP = CacheOpEncoder<3, 7, 12, 5>::value,
  CIGVAC = CacheOpEncoder<3, 7, 14, 3>::value,
  CIGDVAC = CacheOpEncoder<3, 7, 14, 5>::value
};

// Some SVE instructions support a predicate constraint pattern. This is
// interpreted as a VL-dependent value, and is typically used to initialise
// predicates, or to otherwise limit the number of processed elements.
enum SVEPredicateConstraint {
  // Select 2^N elements, for the largest possible N.
  SVE_POW2 = 0x0,
  // Each VL<N> selects exactly N elements if possible, or zero if N is greater
  // than the number of elements. Note that the encoding values for VL<N> are
  // not linearly related to N.
  SVE_VL1 = 0x1,
  SVE_VL2 = 0x2,
  SVE_VL3 = 0x3,
  SVE_VL4 = 0x4,
  SVE_VL5 = 0x5,
  SVE_VL6 = 0x6,
  SVE_VL7 = 0x7,
  SVE_VL8 = 0x8,
  SVE_VL16 = 0x9,
  SVE_VL32 = 0xa,
  SVE_VL64 = 0xb,
  SVE_VL128 = 0xc,
  SVE_VL256 = 0xd,
  // Each MUL<N> selects the largest multiple of N elements that the vector
  // length supports. Note that for D-sized lanes, this can be zero.
  SVE_MUL4 = 0x1d,
  SVE_MUL3 = 0x1e,
  // Select all elements.
  SVE_ALL = 0x1f
};

// Instruction enumerations.
//
// These are the masks that define a class of instructions, and the list of
// instructions within each class. Each enumeration has a Fixed, FMask and
// Mask value.
//
// Fixed: The fixed bits in this instruction class.
// FMask: The mask used to extract the fixed bits in the class.
// Mask:  The mask used to identify the instructions within a class.
//
// The enumerations can be used like this:
//
// VIXL_ASSERT(instr->Mask(PCRelAddressingFMask) == PCRelAddressingFixed);
// switch(instr->Mask(PCRelAddressingMask)) {
//   case ADR:  Format("adr 'Xd, 'AddrPCRelByte"); break;
//   case ADRP: Format("adrp 'Xd, 'AddrPCRelPage"); break;
//   default:   printf("Unknown instruction\n");
// }


// Generic fields.
enum GenericInstrField : uint32_t {
  SixtyFourBits        = 0x80000000u,
  ThirtyTwoBits        = 0x00000000u,

  FPTypeMask           = 0x00C00000u,
  FP16                 = 0x00C00000u,
  FP32                 = 0x00000000u,
  FP64                 = 0x00400000u
};

enum NEONFormatField : uint32_t {
  NEONFormatFieldMask   = 0x40C00000u,
  NEON_Q                = 0x40000000u,
  NEON_8B               = 0x00000000u,
  NEON_16B              = NEON_8B | NEON_Q,
  NEON_4H               = 0x00400000u,
  NEON_8H               = NEON_4H | NEON_Q,
  NEON_2S               = 0x00800000u,
  NEON_4S               = NEON_2S | NEON_Q,
  NEON_1D               = 0x00C00000u,
  NEON_2D               = 0x00C00000u | NEON_Q
};

enum NEONFPFormatField : uint32_t {
  NEONFPFormatFieldMask = 0x40400000u,
  NEON_FP_4H            = FP16,
  NEON_FP_2S            = FP32,
  NEON_FP_8H            = FP16 | NEON_Q,
  NEON_FP_4S            = FP32 | NEON_Q,
  NEON_FP_2D            = FP64 | NEON_Q
};

enum NEONLSFormatField : uint32_t {
  NEONLSFormatFieldMask = 0x40000C00u,
  LS_NEON_8B            = 0x00000000u,
  LS_NEON_16B           = LS_NEON_8B | NEON_Q,
  LS_NEON_4H            = 0x00000400u,
  LS_NEON_8H            = LS_NEON_4H | NEON_Q,
  LS_NEON_2S            = 0x00000800u,
  LS_NEON_4S            = LS_NEON_2S | NEON_Q,
  LS_NEON_1D            = 0x00000C00u,
  LS_NEON_2D            = LS_NEON_1D | NEON_Q
};

enum NEONScalarFormatField : uint32_t {
  NEONScalarFormatFieldMask = 0x00C00000u,
  NEONScalar                = 0x10000000u,
  NEON_B                    = 0x00000000u,
  NEON_H                    = 0x00400000u,
  NEON_S                    = 0x00800000u,
  NEON_D                    = 0x00C00000u
};

enum SVESizeField {
  SVESizeFieldMask = 0x00C00000,
  SVE_B            = 0x00000000,
  SVE_H            = 0x00400000,
  SVE_S            = 0x00800000,
  SVE_D            = 0x00C00000
};

// PC relative addressing.
enum PCRelAddressingOp : uint32_t {
  PCRelAddressingFixed = 0x10000000u,
  PCRelAddressingFMask = 0x1F000000u,
  PCRelAddressingMask  = 0x9F000000u,
  ADR                  = PCRelAddressingFixed | 0x00000000u,
  ADRP                 = PCRelAddressingFixed | 0x80000000u
};

// Add/sub (immediate, shifted and extended.)
const int kSFOffset = 31;
enum AddSubOp : uint32_t {
  AddSubOpMask      = 0x60000000u,
  AddSubSetFlagsBit = 0x20000000u,
  ADD               = 0x00000000u,
  ADDS              = ADD | AddSubSetFlagsBit,
  SUB               = 0x40000000u,
  SUBS              = SUB | AddSubSetFlagsBit
};

#define ADD_SUB_OP_LIST(V)  \
  V(ADD),                   \
  V(ADDS),                  \
  V(SUB),                   \
  V(SUBS)

enum AddSubImmediateOp : uint32_t {
  AddSubImmediateFixed = 0x11000000u,
  AddSubImmediateFMask = 0x1F800000u,
  AddSubImmediateMask  = 0xFF800000u,
  #define ADD_SUB_IMMEDIATE(A)           \
  A##_w_imm = AddSubImmediateFixed | A,  \
  A##_x_imm = AddSubImmediateFixed | A | SixtyFourBits
  ADD_SUB_OP_LIST(ADD_SUB_IMMEDIATE)
  #undef ADD_SUB_IMMEDIATE
};

enum AddSubShiftedOp : uint32_t {
  AddSubShiftedFixed   = 0x0B000000u,
  AddSubShiftedFMask   = 0x1F200000u,
  AddSubShiftedMask    = 0xFF200000u,
  #define ADD_SUB_SHIFTED(A)             \
  A##_w_shift = AddSubShiftedFixed | A,  \
  A##_x_shift = AddSubShiftedFixed | A | SixtyFourBits
  ADD_SUB_OP_LIST(ADD_SUB_SHIFTED)
  #undef ADD_SUB_SHIFTED
};

enum AddSubExtendedOp : uint32_t {
  AddSubExtendedFixed  = 0x0B200000u,
  AddSubExtendedFMask  = 0x1F200000u,
  AddSubExtendedMask   = 0xFFE00000u,
  #define ADD_SUB_EXTENDED(A)           \
  A##_w_ext = AddSubExtendedFixed | A,  \
  A##_x_ext = AddSubExtendedFixed | A | SixtyFourBits
  ADD_SUB_OP_LIST(ADD_SUB_EXTENDED)
  #undef ADD_SUB_EXTENDED
};

// Add/sub with carry.
enum AddSubWithCarryOp : uint32_t {
  AddSubWithCarryFixed = 0x1A000000u,
  AddSubWithCarryFMask = 0x1FE00000u,
  AddSubWithCarryMask  = 0xFFE0FC00u,
  ADC_w                = AddSubWithCarryFixed | ADD,
  ADC_x                = AddSubWithCarryFixed | ADD | SixtyFourBits,
  ADC                  = ADC_w,
  ADCS_w               = AddSubWithCarryFixed | ADDS,
  ADCS_x               = AddSubWithCarryFixed | ADDS | SixtyFourBits,
  SBC_w                = AddSubWithCarryFixed | SUB,
  SBC_x                = AddSubWithCarryFixed | SUB | SixtyFourBits,
  SBC                  = SBC_w,
  SBCS_w               = AddSubWithCarryFixed | SUBS,
  SBCS_x               = AddSubWithCarryFixed | SUBS | SixtyFourBits
};

// Rotate right into flags.
enum RotateRightIntoFlagsOp : uint32_t {
  RotateRightIntoFlagsFixed = 0x1A000400u,
  RotateRightIntoFlagsFMask = 0x1FE07C00u,
  RotateRightIntoFlagsMask  = 0xFFE07C10u,
  RMIF                      = RotateRightIntoFlagsFixed | 0xA0000000u
};

// Evaluate into flags.
enum EvaluateIntoFlagsOp : uint32_t {
  EvaluateIntoFlagsFixed = 0x1A000800u,
  EvaluateIntoFlagsFMask = 0x1FE03C00u,
  EvaluateIntoFlagsMask  = 0xFFE07C1Fu,
  SETF8                  = EvaluateIntoFlagsFixed | 0x2000000Du,
  SETF16                 = EvaluateIntoFlagsFixed | 0x2000400Du
};


// Logical (immediate and shifted register).
enum LogicalOp : uint32_t {
  LogicalOpMask = 0x60200000u,
  NOT   = 0x00200000u,
  AND   = 0x00000000u,
  BIC   = AND | NOT,
  ORR   = 0x20000000u,
  ORN   = ORR | NOT,
  EOR   = 0x40000000u,
  EON   = EOR | NOT,
  ANDS  = 0x60000000u,
  BICS  = ANDS | NOT
};

// Logical immediate.
enum LogicalImmediateOp : uint32_t {
  LogicalImmediateFixed = 0x12000000u,
  LogicalImmediateFMask = 0x1F800000u,
  LogicalImmediateMask  = 0xFF800000u,
  AND_w_imm   = LogicalImmediateFixed | AND,
  AND_x_imm   = LogicalImmediateFixed | AND | SixtyFourBits,
  ORR_w_imm   = LogicalImmediateFixed | ORR,
  ORR_x_imm   = LogicalImmediateFixed | ORR | SixtyFourBits,
  EOR_w_imm   = LogicalImmediateFixed | EOR,
  EOR_x_imm   = LogicalImmediateFixed | EOR | SixtyFourBits,
  ANDS_w_imm  = LogicalImmediateFixed | ANDS,
  ANDS_x_imm  = LogicalImmediateFixed | ANDS | SixtyFourBits
};

// Logical shifted register.
enum LogicalShiftedOp : uint32_t {
  LogicalShiftedFixed = 0x0A000000u,
  LogicalShiftedFMask = 0x1F000000u,
  LogicalShiftedMask  = 0xFF200000u,
  AND_w               = LogicalShiftedFixed | AND,
  AND_x               = LogicalShiftedFixed | AND | SixtyFourBits,
  AND_shift           = AND_w,
  BIC_w               = LogicalShiftedFixed | BIC,
  BIC_x               = LogicalShiftedFixed | BIC | SixtyFourBits,
  BIC_shift           = BIC_w,
  ORR_w               = LogicalShiftedFixed | ORR,
  ORR_x               = LogicalShiftedFixed | ORR | SixtyFourBits,
  ORR_shift           = ORR_w,
  ORN_w               = LogicalShiftedFixed | ORN,
  ORN_x               = LogicalShiftedFixed | ORN | SixtyFourBits,
  ORN_shift           = ORN_w,
  EOR_w               = LogicalShiftedFixed | EOR,
  EOR_x               = LogicalShiftedFixed | EOR | SixtyFourBits,
  EOR_shift           = EOR_w,
  EON_w               = LogicalShiftedFixed | EON,
  EON_x               = LogicalShiftedFixed | EON | SixtyFourBits,
  EON_shift           = EON_w,
  ANDS_w              = LogicalShiftedFixed | ANDS,
  ANDS_x              = LogicalShiftedFixed | ANDS | SixtyFourBits,
  ANDS_shift          = ANDS_w,
  BICS_w              = LogicalShiftedFixed | BICS,
  BICS_x              = LogicalShiftedFixed | BICS | SixtyFourBits,
  BICS_shift          = BICS_w
};

// Move wide immediate.
enum MoveWideImmediateOp : uint32_t {
  MoveWideImmediateFixed = 0x12800000u,
  MoveWideImmediateFMask = 0x1F800000u,
  MoveWideImmediateMask  = 0xFF800000u,
  MOVN                   = 0x00000000u,
  MOVZ                   = 0x40000000u,
  MOVK                   = 0x60000000u,
  MOVN_w                 = MoveWideImmediateFixed | MOVN,
  MOVN_x                 = MoveWideImmediateFixed | MOVN | SixtyFourBits,
  MOVZ_w                 = MoveWideImmediateFixed | MOVZ,
  MOVZ_x                 = MoveWideImmediateFixed | MOVZ | SixtyFourBits,
  MOVK_w                 = MoveWideImmediateFixed | MOVK,
  MOVK_x                 = MoveWideImmediateFixed | MOVK | SixtyFourBits
};

// Bitfield.
const int kBitfieldNOffset = 22;
enum BitfieldOp : uint32_t {
  BitfieldFixed = 0x13000000u,
  BitfieldFMask = 0x1F800000u,
  BitfieldMask  = 0xFF800000u,
  SBFM_w        = BitfieldFixed | 0x00000000u,
  SBFM_x        = BitfieldFixed | 0x80000000u,
  SBFM          = SBFM_w,
  BFM_w         = BitfieldFixed | 0x20000000u,
  BFM_x         = BitfieldFixed | 0xA0000000u,
  BFM           = BFM_w,
  UBFM_w        = BitfieldFixed | 0x40000000u,
  UBFM_x        = BitfieldFixed | 0xC0000000u,
  UBFM          = UBFM_w
  // Bitfield N field.
};

// Extract.
enum ExtractOp : uint32_t {
  ExtractFixed = 0x13800000u,
  ExtractFMask = 0x1F800000u,
  ExtractMask  = 0xFFA00000u,
  EXTR_w       = ExtractFixed | 0x00000000u,
  EXTR_x       = ExtractFixed | 0x80000000u,
  EXTR         = EXTR_w
};

// Unconditional branch.
enum UnconditionalBranchOp : uint32_t {
  UnconditionalBranchFixed = 0x14000000u,
  UnconditionalBranchFMask = 0x7C000000u,
  UnconditionalBranchMask  = 0xFC000000u,
  B                        = UnconditionalBranchFixed | 0x00000000u,
  BL                       = UnconditionalBranchFixed | 0x80000000u
};

// Unconditional branch to register.
enum UnconditionalBranchToRegisterOp : uint32_t {
  UnconditionalBranchToRegisterFixed = 0xD6000000u,
  UnconditionalBranchToRegisterFMask = 0xFE000000u,
  UnconditionalBranchToRegisterMask  = 0xFFFFFC00u,
  BR      = UnconditionalBranchToRegisterFixed | 0x001F0000u,
  BLR     = UnconditionalBranchToRegisterFixed | 0x003F0000u,
  RET     = UnconditionalBranchToRegisterFixed | 0x005F0000u,

  BRAAZ  = UnconditionalBranchToRegisterFixed | 0x001F0800u,
  BRABZ  = UnconditionalBranchToRegisterFixed | 0x001F0C00u,
  BLRAAZ = UnconditionalBranchToRegisterFixed | 0x003F0800u,
  BLRABZ = UnconditionalBranchToRegisterFixed | 0x003F0C00u,
  RETAA  = UnconditionalBranchToRegisterFixed | 0x005F0800u,
  RETAB  = UnconditionalBranchToRegisterFixed | 0x005F0C00u,
  BRAA   = UnconditionalBranchToRegisterFixed | 0x011F0800u,
  BRAB   = UnconditionalBranchToRegisterFixed | 0x011F0C00u,
  BLRAA  = UnconditionalBranchToRegisterFixed | 0x013F0800u,
  BLRAB  = UnconditionalBranchToRegisterFixed | 0x013F0C00u
};

// Compare and branch.
enum CompareBranchOp : uint32_t {
  CompareBranchFixed = 0x34000000u,
  CompareBranchFMask = 0x7E000000u,
  CompareBranchMask  = 0xFF000000u,
  CBZ_w              = CompareBranchFixed | 0x00000000u,
  CBZ_x              = CompareBranchFixed | 0x80000000u,
  CBZ                = CBZ_w,
  CBNZ_w             = CompareBranchFixed | 0x01000000u,
  CBNZ_x             = CompareBranchFixed | 0x81000000u,
  CBNZ               = CBNZ_w
};

// Test and branch.
enum TestBranchOp : uint32_t {
  TestBranchFixed = 0x36000000u,
  TestBranchFMask = 0x7E000000u,
  TestBranchMask  = 0x7F000000u,
  TBZ             = TestBranchFixed | 0x00000000u,
  TBNZ            = TestBranchFixed | 0x01000000u
};

// Conditional branch.
enum ConditionalBranchOp : uint32_t {
  ConditionalBranchFixed = 0x54000000u,
  ConditionalBranchFMask = 0xFE000000u,
  ConditionalBranchMask  = 0xFF000010u,
  B_cond                 = ConditionalBranchFixed | 0x00000000u
};

// System.
// System instruction encoding is complicated because some instructions use op
// and CR fields to encode parameters. To handle this cleanly, the system
// instructions are split into more than one enum.

enum SystemOp : uint32_t {
  SystemFixed = 0xD5000000u,
  SystemFMask = 0xFFC00000u
};

enum SystemSysRegOp : uint32_t {
  SystemSysRegFixed = 0xD5100000u,
  SystemSysRegFMask = 0xFFD00000u,
  SystemSysRegMask  = 0xFFF00000u,
  MRS               = SystemSysRegFixed | 0x00200000u,
  MSR               = SystemSysRegFixed | 0x00000000u
};

enum SystemPStateOp : uint32_t {
  SystemPStateFixed = 0xD5004000u,
  SystemPStateFMask = 0xFFF8F000u,
  SystemPStateMask  = 0xFFFFF0FFu,
  CFINV             = SystemPStateFixed | 0x0000001Fu,
  XAFLAG            = SystemPStateFixed | 0x0000003Fu,
  AXFLAG            = SystemPStateFixed | 0x0000005Fu
};

enum SystemHintOp : uint32_t {
  SystemHintFixed = 0xD503201Fu,
  SystemHintFMask = 0xFFFFF01Fu,
  SystemHintMask  = 0xFFFFF01Fu,
  HINT            = SystemHintFixed | 0x00000000u
};

enum SystemSysOp : uint32_t {
  SystemSysFixed  = 0xD5080000u,
  SystemSysFMask  = 0xFFF80000u,
  SystemSysMask   = 0xFFF80000u,
  SYS             = SystemSysFixed | 0x00000000u
};

// Exception.
enum ExceptionOp : uint32_t {
  ExceptionFixed = 0xD4000000u,
  ExceptionFMask = 0xFF000000u,
  ExceptionMask  = 0xFFE0001Fu,
  HLT            = ExceptionFixed | 0x00400000u,
  BRK            = ExceptionFixed | 0x00200000u,
  SVC            = ExceptionFixed | 0x00000001u,
  HVC            = ExceptionFixed | 0x00000002u,
  SMC            = ExceptionFixed | 0x00000003u,
  DCPS1          = ExceptionFixed | 0x00A00001u,
  DCPS2          = ExceptionFixed | 0x00A00002u,
  DCPS3          = ExceptionFixed | 0x00A00003u
};

enum MemBarrierOp : uint32_t {
  MemBarrierFixed = 0xD503309Fu,
  MemBarrierFMask = 0xFFFFF09Fu,
  MemBarrierMask  = 0xFFFFF0FFu,
  DSB             = MemBarrierFixed | 0x00000000u,
  DMB             = MemBarrierFixed | 0x00000020u,
  ISB             = MemBarrierFixed | 0x00000040u
};

enum SystemExclusiveMonitorOp : uint32_t {
  SystemExclusiveMonitorFixed = 0xD503305Fu,
  SystemExclusiveMonitorFMask = 0xFFFFF0FFu,
  SystemExclusiveMonitorMask  = 0xFFFFF0FFu,
  CLREX                       = SystemExclusiveMonitorFixed
};

enum SystemPAuthOp : uint32_t {
  SystemPAuthFixed = 0xD503211Fu,
  SystemPAuthFMask = 0xFFFFFD1Fu,
  SystemPAuthMask  = 0xFFFFFFFFu,
  PACIA1716 = SystemPAuthFixed | 0x00000100u,
  PACIB1716 = SystemPAuthFixed | 0x00000140u,
  AUTIA1716 = SystemPAuthFixed | 0x00000180u,
  AUTIB1716 = SystemPAuthFixed | 0x000001C0u,
  PACIAZ    = SystemPAuthFixed | 0x00000300u,
  PACIASP   = SystemPAuthFixed | 0x00000320u,
  PACIBZ    = SystemPAuthFixed | 0x00000340u,
  PACIBSP   = SystemPAuthFixed | 0x00000360u,
  AUTIAZ    = SystemPAuthFixed | 0x00000380u,
  AUTIASP   = SystemPAuthFixed | 0x000003A0u,
  AUTIBZ    = SystemPAuthFixed | 0x000003C0u,
  AUTIBSP   = SystemPAuthFixed | 0x000003E0u,

  // XPACLRI has the same fixed mask as System Hints and needs to be handled
  // differently.
  XPACLRI   = 0xD50320FFu
};

// Any load or store.
enum LoadStoreAnyOp : uint32_t {
  LoadStoreAnyFMask = 0x0a000000u,
  LoadStoreAnyFixed = 0x08000000u
};

// Any load pair or store pair.
enum LoadStorePairAnyOp : uint32_t {
  LoadStorePairAnyFMask = 0x3a000000u,
  LoadStorePairAnyFixed = 0x28000000u
};

#define LOAD_STORE_PAIR_OP_LIST(V)  \
  V(STP, w,   0x00000000u),          \
  V(LDP, w,   0x00400000u),          \
  V(LDPSW, x, 0x40400000u),          \
  V(STP, x,   0x80000000u),          \
  V(LDP, x,   0x80400000u),          \
  V(STP, s,   0x04000000u),          \
  V(LDP, s,   0x04400000u),          \
  V(STP, d,   0x44000000u),          \
  V(LDP, d,   0x44400000u),          \
  V(STP, q,   0x84000000u),          \
  V(LDP, q,   0x84400000u)

// Load/store pair (post, pre and offset.)
enum LoadStorePairOp : uint32_t {
  LoadStorePairMask = 0xC4400000u,
  LoadStorePairLBit = 1 << 22,
  #define LOAD_STORE_PAIR(A, B, C) \
  A##_##B = C
  LOAD_STORE_PAIR_OP_LIST(LOAD_STORE_PAIR)
  #undef LOAD_STORE_PAIR
};

enum LoadStorePairPostIndexOp : uint32_t {
  LoadStorePairPostIndexFixed = 0x28800000u,
  LoadStorePairPostIndexFMask = 0x3B800000u,
  LoadStorePairPostIndexMask  = 0xFFC00000u,
  #define LOAD_STORE_PAIR_POST_INDEX(A, B, C)  \
  A##_##B##_post = LoadStorePairPostIndexFixed | A##_##B
  LOAD_STORE_PAIR_OP_LIST(LOAD_STORE_PAIR_POST_INDEX)
  #undef LOAD_STORE_PAIR_POST_INDEX
};

enum LoadStorePairPreIndexOp : uint32_t {
  LoadStorePairPreIndexFixed = 0x29800000u,
  LoadStorePairPreIndexFMask = 0x3B800000u,
  LoadStorePairPreIndexMask  = 0xFFC00000u,
  #define LOAD_STORE_PAIR_PRE_INDEX(A, B, C)  \
  A##_##B##_pre = LoadStorePairPreIndexFixed | A##_##B
  LOAD_STORE_PAIR_OP_LIST(LOAD_STORE_PAIR_PRE_INDEX)
  #undef LOAD_STORE_PAIR_PRE_INDEX
};

enum LoadStorePairOffsetOp : uint32_t {
  LoadStorePairOffsetFixed = 0x29000000u,
  LoadStorePairOffsetFMask = 0x3B800000u,
  LoadStorePairOffsetMask  = 0xFFC00000u,
  #define LOAD_STORE_PAIR_OFFSET(A, B, C)  \
  A##_##B##_off = LoadStorePairOffsetFixed | A##_##B
  LOAD_STORE_PAIR_OP_LIST(LOAD_STORE_PAIR_OFFSET)
  #undef LOAD_STORE_PAIR_OFFSET
};

enum LoadStorePairNonTemporalOp : uint32_t {
  LoadStorePairNonTemporalFixed = 0x28000000u,
  LoadStorePairNonTemporalFMask = 0x3B800000u,
  LoadStorePairNonTemporalMask  = 0xFFC00000u,
  LoadStorePairNonTemporalLBit = 1 << 22,
  STNP_w = LoadStorePairNonTemporalFixed | STP_w,
  LDNP_w = LoadStorePairNonTemporalFixed | LDP_w,
  STNP_x = LoadStorePairNonTemporalFixed | STP_x,
  LDNP_x = LoadStorePairNonTemporalFixed | LDP_x,
  STNP_s = LoadStorePairNonTemporalFixed | STP_s,
  LDNP_s = LoadStorePairNonTemporalFixed | LDP_s,
  STNP_d = LoadStorePairNonTemporalFixed | STP_d,
  LDNP_d = LoadStorePairNonTemporalFixed | LDP_d,
  STNP_q = LoadStorePairNonTemporalFixed | STP_q,
  LDNP_q = LoadStorePairNonTemporalFixed | LDP_q
};

// Load with pointer authentication.
enum LoadStorePACOp {
  LoadStorePACFixed  = 0xF8200400u,
  LoadStorePACFMask  = 0xFF200400u,
  LoadStorePACMask   = 0xFFA00C00u,
  LoadStorePACPreBit = 0x00000800u,
  LDRAA     = LoadStorePACFixed | 0x00000000u,
  LDRAA_pre = LoadStorePACPreBit | LDRAA,
  LDRAB     = LoadStorePACFixed | 0x00800000u,
  LDRAB_pre = LoadStorePACPreBit | LDRAB
};

// Load literal.
enum LoadLiteralOp : uint32_t {
  LoadLiteralFixed = 0x18000000u,
  LoadLiteralFMask = 0x3B000000u,
  LoadLiteralMask  = 0xFF000000u,
  LDR_w_lit        = LoadLiteralFixed | 0x00000000u,
  LDR_x_lit        = LoadLiteralFixed | 0x40000000u,
  LDRSW_x_lit      = LoadLiteralFixed | 0x80000000u,
  PRFM_lit         = LoadLiteralFixed | 0xC0000000u,
  LDR_s_lit        = LoadLiteralFixed | 0x04000000u,
  LDR_d_lit        = LoadLiteralFixed | 0x44000000u,
  LDR_q_lit        = LoadLiteralFixed | 0x84000000u
};

#define LOAD_STORE_OP_LIST(V)     \
  V(ST, RB, w,  0x00000000u),  \
  V(ST, RH, w,  0x40000000u),  \
  V(ST, R, w,   0x80000000u),  \
  V(ST, R, x,   0xC0000000u),  \
  V(LD, RB, w,  0x00400000u),  \
  V(LD, RH, w,  0x40400000u),  \
  V(LD, R, w,   0x80400000u),  \
  V(LD, R, x,   0xC0400000u),  \
  V(LD, RSB, x, 0x00800000u),  \
  V(LD, RSH, x, 0x40800000u),  \
  V(LD, RSW, x, 0x80800000u),  \
  V(LD, RSB, w, 0x00C00000u),  \
  V(LD, RSH, w, 0x40C00000u),  \
  V(ST, R, b,   0x04000000u),  \
  V(ST, R, h,   0x44000000u),  \
  V(ST, R, s,   0x84000000u),  \
  V(ST, R, d,   0xC4000000u),  \
  V(ST, R, q,   0x04800000u),  \
  V(LD, R, b,   0x04400000u),  \
  V(LD, R, h,   0x44400000u),  \
  V(LD, R, s,   0x84400000u),  \
  V(LD, R, d,   0xC4400000u),  \
  V(LD, R, q,   0x04C00000u)

// Load/store (post, pre, offset and unsigned.)
enum LoadStoreOp : uint32_t {
  LoadStoreMask = 0xC4C00000u,
  LoadStoreVMask = 0x04000000u,
  #define LOAD_STORE(A, B, C, D)  \
  A##B##_##C = D
  LOAD_STORE_OP_LIST(LOAD_STORE),
  #undef LOAD_STORE
  PRFM = 0xC0800000u
};

// Load/store unscaled offset.
enum LoadStoreUnscaledOffsetOp : uint32_t {
  LoadStoreUnscaledOffsetFixed = 0x38000000u,
  LoadStoreUnscaledOffsetFMask = 0x3B200C00u,
  LoadStoreUnscaledOffsetMask  = 0xFFE00C00u,
  PRFUM                        = LoadStoreUnscaledOffsetFixed | PRFM,
  #define LOAD_STORE_UNSCALED(A, B, C, D)  \
  A##U##B##_##C = LoadStoreUnscaledOffsetFixed | D
  LOAD_STORE_OP_LIST(LOAD_STORE_UNSCALED)
  #undef LOAD_STORE_UNSCALED
};

// Load/store post index.
enum LoadStorePostIndex : uint32_t {
  LoadStorePostIndexFixed = 0x38000400u,
  LoadStorePostIndexFMask = 0x3B200C00u,
  LoadStorePostIndexMask  = 0xFFE00C00u,
  #define LOAD_STORE_POST_INDEX(A, B, C, D)  \
  A##B##_##C##_post = LoadStorePostIndexFixed | D
  LOAD_STORE_OP_LIST(LOAD_STORE_POST_INDEX)
  #undef LOAD_STORE_POST_INDEX
};

// Load/store pre index.
enum LoadStorePreIndex : uint32_t {
  LoadStorePreIndexFixed = 0x38000C00u,
  LoadStorePreIndexFMask = 0x3B200C00u,
  LoadStorePreIndexMask  = 0xFFE00C00u,
  #define LOAD_STORE_PRE_INDEX(A, B, C, D)  \
  A##B##_##C##_pre = LoadStorePreIndexFixed | D
  LOAD_STORE_OP_LIST(LOAD_STORE_PRE_INDEX)
  #undef LOAD_STORE_PRE_INDEX
};

// Load/store unsigned offset.
enum LoadStoreUnsignedOffset : uint32_t {
  LoadStoreUnsignedOffsetFixed = 0x39000000u,
  LoadStoreUnsignedOffsetFMask = 0x3B000000u,
  LoadStoreUnsignedOffsetMask  = 0xFFC00000u,
  PRFM_unsigned                = LoadStoreUnsignedOffsetFixed | PRFM,
  #define LOAD_STORE_UNSIGNED_OFFSET(A, B, C, D) \
  A##B##_##C##_unsigned = LoadStoreUnsignedOffsetFixed | D
  LOAD_STORE_OP_LIST(LOAD_STORE_UNSIGNED_OFFSET)
  #undef LOAD_STORE_UNSIGNED_OFFSET
};

// Load/store register offset.
enum LoadStoreRegisterOffset : uint32_t {
  LoadStoreRegisterOffsetFixed = 0x38200800u,
  LoadStoreRegisterOffsetFMask = 0x3B200C00u,
  LoadStoreRegisterOffsetMask  = 0xFFE00C00u,
  PRFM_reg                     = LoadStoreRegisterOffsetFixed | PRFM,
  #define LOAD_STORE_REGISTER_OFFSET(A, B, C, D) \
  A##B##_##C##_reg = LoadStoreRegisterOffsetFixed | D
  LOAD_STORE_OP_LIST(LOAD_STORE_REGISTER_OFFSET)
  #undef LOAD_STORE_REGISTER_OFFSET
};

enum LoadStoreExclusive : uint32_t {
  LoadStoreExclusiveFixed = 0x08000000u,
  LoadStoreExclusiveFMask = 0x3F000000u,
  LoadStoreExclusiveMask  = 0xFFE08000u,
  STXRB_w  = LoadStoreExclusiveFixed | 0x00000000u,
  STXRH_w  = LoadStoreExclusiveFixed | 0x40000000u,
  STXR_w   = LoadStoreExclusiveFixed | 0x80000000u,
  STXR_x   = LoadStoreExclusiveFixed | 0xC0000000u,
  LDXRB_w  = LoadStoreExclusiveFixed | 0x00400000u,
  LDXRH_w  = LoadStoreExclusiveFixed | 0x40400000u,
  LDXR_w   = LoadStoreExclusiveFixed | 0x80400000u,
  LDXR_x   = LoadStoreExclusiveFixed | 0xC0400000u,
  STXP_w   = LoadStoreExclusiveFixed | 0x80200000u,
  STXP_x   = LoadStoreExclusiveFixed | 0xC0200000u,
  LDXP_w   = LoadStoreExclusiveFixed | 0x80600000u,
  LDXP_x   = LoadStoreExclusiveFixed | 0xC0600000u,
  STLXRB_w = LoadStoreExclusiveFixed | 0x00008000u,
  STLXRH_w = LoadStoreExclusiveFixed | 0x40008000u,
  STLXR_w  = LoadStoreExclusiveFixed | 0x80008000u,
  STLXR_x  = LoadStoreExclusiveFixed | 0xC0008000u,
  LDAXRB_w = LoadStoreExclusiveFixed | 0x00408000u,
  LDAXRH_w = LoadStoreExclusiveFixed | 0x40408000u,
  LDAXR_w  = LoadStoreExclusiveFixed | 0x80408000u,
  LDAXR_x  = LoadStoreExclusiveFixed | 0xC0408000u,
  STLXP_w  = LoadStoreExclusiveFixed | 0x80208000u,
  STLXP_x  = LoadStoreExclusiveFixed | 0xC0208000u,
  LDAXP_w  = LoadStoreExclusiveFixed | 0x80608000u,
  LDAXP_x  = LoadStoreExclusiveFixed | 0xC0608000u,
  STLRB_w  = LoadStoreExclusiveFixed | 0x00808000u,
  STLRH_w  = LoadStoreExclusiveFixed | 0x40808000u,
  STLR_w   = LoadStoreExclusiveFixed | 0x80808000u,
  STLR_x   = LoadStoreExclusiveFixed | 0xC0808000u,
  LDARB_w  = LoadStoreExclusiveFixed | 0x00C08000u,
  LDARH_w  = LoadStoreExclusiveFixed | 0x40C08000u,
  LDAR_w   = LoadStoreExclusiveFixed | 0x80C08000u,
  LDAR_x   = LoadStoreExclusiveFixed | 0xC0C08000u,

  // v8.1 Load/store LORegion ops
  STLLRB   = LoadStoreExclusiveFixed | 0x00800000u,
  LDLARB   = LoadStoreExclusiveFixed | 0x00C00000u,
  STLLRH   = LoadStoreExclusiveFixed | 0x40800000u,
  LDLARH   = LoadStoreExclusiveFixed | 0x40C00000u,
  STLLR_w  = LoadStoreExclusiveFixed | 0x80800000u,
  LDLAR_w  = LoadStoreExclusiveFixed | 0x80C00000u,
  STLLR_x  = LoadStoreExclusiveFixed | 0xC0800000u,
  LDLAR_x  = LoadStoreExclusiveFixed | 0xC0C00000u,

  // v8.1 Load/store exclusive ops
  LSEBit_l  = 0x00400000u,
  LSEBit_o0 = 0x00008000u,
  LSEBit_sz = 0x40000000u,
  CASFixed  = LoadStoreExclusiveFixed | 0x80A00000u,
  CASBFixed = LoadStoreExclusiveFixed | 0x00A00000u,
  CASHFixed = LoadStoreExclusiveFixed | 0x40A00000u,
  CASPFixed = LoadStoreExclusiveFixed | 0x00200000u,
  CAS_w    = CASFixed,
  CAS_x    = CASFixed | LSEBit_sz,
  CASA_w   = CASFixed | LSEBit_l,
  CASA_x   = CASFixed | LSEBit_l | LSEBit_sz,
  CASL_w   = CASFixed | LSEBit_o0,
  CASL_x   = CASFixed | LSEBit_o0 | LSEBit_sz,
  CASAL_w  = CASFixed | LSEBit_l | LSEBit_o0,
  CASAL_x  = CASFixed | LSEBit_l | LSEBit_o0 | LSEBit_sz,
  CASB     = CASBFixed,
  CASAB    = CASBFixed | LSEBit_l,
  CASLB    = CASBFixed | LSEBit_o0,
  CASALB   = CASBFixed | LSEBit_l | LSEBit_o0,
  CASH     = CASHFixed,
  CASAH    = CASHFixed | LSEBit_l,
  CASLH    = CASHFixed | LSEBit_o0,
  CASALH   = CASHFixed | LSEBit_l | LSEBit_o0,
  CASP_w   = CASPFixed,
  CASP_x   = CASPFixed | LSEBit_sz,
  CASPA_w  = CASPFixed | LSEBit_l,
  CASPA_x  = CASPFixed | LSEBit_l | LSEBit_sz,
  CASPL_w  = CASPFixed | LSEBit_o0,
  CASPL_x  = CASPFixed | LSEBit_o0 | LSEBit_sz,
  CASPAL_w = CASPFixed | LSEBit_l | LSEBit_o0,
  CASPAL_x = CASPFixed | LSEBit_l | LSEBit_o0 | LSEBit_sz
};

// Load/store RCpc unscaled offset.
enum LoadStoreRCpcUnscaledOffsetOp : uint32_t {
  LoadStoreRCpcUnscaledOffsetFixed = 0x19000000u,
  LoadStoreRCpcUnscaledOffsetFMask = 0x3F200C00u,
  LoadStoreRCpcUnscaledOffsetMask  = 0xFFE00C00u,
  STLURB     = LoadStoreRCpcUnscaledOffsetFixed | 0x00000000u,
  LDAPURB    = LoadStoreRCpcUnscaledOffsetFixed | 0x00400000u,
  LDAPURSB_x = LoadStoreRCpcUnscaledOffsetFixed | 0x00800000u,
  LDAPURSB_w = LoadStoreRCpcUnscaledOffsetFixed | 0x00C00000u,
  STLURH     = LoadStoreRCpcUnscaledOffsetFixed | 0x40000000u,
  LDAPURH    = LoadStoreRCpcUnscaledOffsetFixed | 0x40400000u,
  LDAPURSH_x = LoadStoreRCpcUnscaledOffsetFixed | 0x40800000u,
  LDAPURSH_w = LoadStoreRCpcUnscaledOffsetFixed | 0x40C00000u,
  STLUR_w    = LoadStoreRCpcUnscaledOffsetFixed | 0x80000000u,
  LDAPUR_w   = LoadStoreRCpcUnscaledOffsetFixed | 0x80400000u,
  LDAPURSW   = LoadStoreRCpcUnscaledOffsetFixed | 0x80800000u,
  STLUR_x    = LoadStoreRCpcUnscaledOffsetFixed | 0xC0000000u,
  LDAPUR_x   = LoadStoreRCpcUnscaledOffsetFixed | 0xC0400000u
};

#define ATOMIC_MEMORY_SIMPLE_OPC_LIST(V) \
  V(LDADD, 0x00000000u),                  \
  V(LDCLR, 0x00001000u),                  \
  V(LDEOR, 0x00002000u),                  \
  V(LDSET, 0x00003000u),                  \
  V(LDSMAX, 0x00004000u),                 \
  V(LDSMIN, 0x00005000u),                 \
  V(LDUMAX, 0x00006000u),                 \
  V(LDUMIN, 0x00007000u)

// Atomic memory.
enum AtomicMemoryOp : uint32_t {
  AtomicMemoryFixed = 0x38200000u,
  AtomicMemoryFMask = 0x3B200C00u,
  AtomicMemoryMask = 0xFFE0FC00u,
  SWPB = AtomicMemoryFixed | 0x00008000u,
  SWPAB = AtomicMemoryFixed | 0x00808000u,
  SWPLB = AtomicMemoryFixed | 0x00408000u,
  SWPALB = AtomicMemoryFixed | 0x00C08000u,
  SWPH = AtomicMemoryFixed | 0x40008000u,
  SWPAH = AtomicMemoryFixed | 0x40808000u,
  SWPLH = AtomicMemoryFixed | 0x40408000u,
  SWPALH = AtomicMemoryFixed | 0x40C08000u,
  SWP_w = AtomicMemoryFixed | 0x80008000u,
  SWPA_w = AtomicMemoryFixed | 0x80808000u,
  SWPL_w = AtomicMemoryFixed | 0x80408000u,
  SWPAL_w = AtomicMemoryFixed | 0x80C08000u,
  SWP_x = AtomicMemoryFixed | 0xC0008000u,
  SWPA_x = AtomicMemoryFixed | 0xC0808000u,
  SWPL_x = AtomicMemoryFixed | 0xC0408000u,
  SWPAL_x = AtomicMemoryFixed | 0xC0C08000u,
  LDAPRB = AtomicMemoryFixed | 0x0080C000u,
  LDAPRH = AtomicMemoryFixed | 0x4080C000u,
  LDAPR_w = AtomicMemoryFixed | 0x8080C000u,
  LDAPR_x = AtomicMemoryFixed | 0xC080C000u,

  AtomicMemorySimpleFMask = 0x3B208C00u,
  AtomicMemorySimpleOpMask = 0x00007000u,
#define ATOMIC_MEMORY_SIMPLE(N, OP)              \
  N##Op = OP,                                    \
  N##B = AtomicMemoryFixed | OP,                 \
  N##AB = AtomicMemoryFixed | OP | 0x00800000u,   \
  N##LB = AtomicMemoryFixed | OP | 0x00400000u,   \
  N##ALB = AtomicMemoryFixed | OP | 0x00C00000u,  \
  N##H = AtomicMemoryFixed | OP | 0x40000000u,    \
  N##AH = AtomicMemoryFixed | OP | 0x40800000u,   \
  N##LH = AtomicMemoryFixed | OP | 0x40400000u,   \
  N##ALH = AtomicMemoryFixed | OP | 0x40C00000u,  \
  N##_w = AtomicMemoryFixed | OP | 0x80000000u,   \
  N##A_w = AtomicMemoryFixed | OP | 0x80800000u,  \
  N##L_w = AtomicMemoryFixed | OP | 0x80400000u,  \
  N##AL_w = AtomicMemoryFixed | OP | 0x80C00000u, \
  N##_x = AtomicMemoryFixed | OP | 0xC0000000u,   \
  N##A_x = AtomicMemoryFixed | OP | 0xC0800000u,  \
  N##L_x = AtomicMemoryFixed | OP | 0xC0400000u,  \
  N##AL_x = AtomicMemoryFixed | OP | 0xC0C00000u

  ATOMIC_MEMORY_SIMPLE_OPC_LIST(ATOMIC_MEMORY_SIMPLE)
#undef ATOMIC_MEMORY_SIMPLE
};

// Conditional compare.
enum ConditionalCompareOp : uint32_t {
  ConditionalCompareMask = 0x60000000u,
  CCMN                   = 0x20000000u,
  CCMP                   = 0x60000000u
};

// Conditional compare register.
enum ConditionalCompareRegisterOp : uint32_t {
  ConditionalCompareRegisterFixed = 0x1A400000u,
  ConditionalCompareRegisterFMask = 0x1FE00800u,
  ConditionalCompareRegisterMask  = 0xFFE00C10u,
  CCMN_w = ConditionalCompareRegisterFixed | CCMN,
  CCMN_x = ConditionalCompareRegisterFixed | SixtyFourBits | CCMN,
  CCMP_w = ConditionalCompareRegisterFixed | CCMP,
  CCMP_x = ConditionalCompareRegisterFixed | SixtyFourBits | CCMP
};

// Conditional compare immediate.
enum ConditionalCompareImmediateOp : uint32_t {
  ConditionalCompareImmediateFixed = 0x1A400800u,
  ConditionalCompareImmediateFMask = 0x1FE00800u,
  ConditionalCompareImmediateMask  = 0xFFE00C10u,
  CCMN_w_imm = ConditionalCompareImmediateFixed | CCMN,
  CCMN_x_imm = ConditionalCompareImmediateFixed | SixtyFourBits | CCMN,
  CCMP_w_imm = ConditionalCompareImmediateFixed | CCMP,
  CCMP_x_imm = ConditionalCompareImmediateFixed | SixtyFourBits | CCMP
};

// Conditional select.
enum ConditionalSelectOp : uint32_t {
  ConditionalSelectFixed = 0x1A800000u,
  ConditionalSelectFMask = 0x1FE00000u,
  ConditionalSelectMask  = 0xFFE00C00u,
  CSEL_w                 = ConditionalSelectFixed | 0x00000000u,
  CSEL_x                 = ConditionalSelectFixed | 0x80000000u,
  CSEL                   = CSEL_w,
  CSINC_w                = ConditionalSelectFixed | 0x00000400u,
  CSINC_x                = ConditionalSelectFixed | 0x80000400u,
  CSINC                  = CSINC_w,
  CSINV_w                = ConditionalSelectFixed | 0x40000000u,
  CSINV_x                = ConditionalSelectFixed | 0xC0000000u,
  CSINV                  = CSINV_w,
  CSNEG_w                = ConditionalSelectFixed | 0x40000400u,
  CSNEG_x                = ConditionalSelectFixed | 0xC0000400u,
  CSNEG                  = CSNEG_w
};

// Data processing 1 source.
enum DataProcessing1SourceOp : uint32_t {
  DataProcessing1SourceFixed = 0x5AC00000u,
  DataProcessing1SourceFMask = 0x5FE00000u,
  DataProcessing1SourceMask  = 0xFFFFFC00u,
  RBIT    = DataProcessing1SourceFixed | 0x00000000u,
  RBIT_w  = RBIT,
  RBIT_x  = RBIT | SixtyFourBits,
  REV16   = DataProcessing1SourceFixed | 0x00000400u,
  REV16_w = REV16,
  REV16_x = REV16 | SixtyFourBits,
  REV     = DataProcessing1SourceFixed | 0x00000800u,
  REV_w   = REV,
  REV32_x = REV | SixtyFourBits,
  REV_x   = DataProcessing1SourceFixed | SixtyFourBits | 0x00000C00u,
  CLZ     = DataProcessing1SourceFixed | 0x00001000u,
  CLZ_w   = CLZ,
  CLZ_x   = CLZ | SixtyFourBits,
  CLS     = DataProcessing1SourceFixed | 0x00001400u,
  CLS_w   = CLS,
  CLS_x   = CLS | SixtyFourBits,

  // Pointer authentication instructions in Armv8.3.
  PACIA  = DataProcessing1SourceFixed | 0x80010000u,
  PACIB  = DataProcessing1SourceFixed | 0x80010400u,
  PACDA  = DataProcessing1SourceFixed | 0x80010800u,
  PACDB  = DataProcessing1SourceFixed | 0x80010C00u,
  AUTIA  = DataProcessing1SourceFixed | 0x80011000u,
  AUTIB  = DataProcessing1SourceFixed | 0x80011400u,
  AUTDA  = DataProcessing1SourceFixed | 0x80011800u,
  AUTDB  = DataProcessing1SourceFixed | 0x80011C00u,
  PACIZA = DataProcessing1SourceFixed | 0x80012000u,
  PACIZB = DataProcessing1SourceFixed | 0x80012400u,
  PACDZA = DataProcessing1SourceFixed | 0x80012800u,
  PACDZB = DataProcessing1SourceFixed | 0x80012C00u,
  AUTIZA = DataProcessing1SourceFixed | 0x80013000u,
  AUTIZB = DataProcessing1SourceFixed | 0x80013400u,
  AUTDZA = DataProcessing1SourceFixed | 0x80013800u,
  AUTDZB = DataProcessing1SourceFixed | 0x80013C00u,
  XPACI  = DataProcessing1SourceFixed | 0x80014000u,
  XPACD  = DataProcessing1SourceFixed | 0x80014400u
};

// Data processing 2 source.
enum DataProcessing2SourceOp : uint32_t {
  DataProcessing2SourceFixed = 0x1AC00000u,
  DataProcessing2SourceFMask = 0x5FE00000u,
  DataProcessing2SourceMask  = 0xFFE0FC00u,
  UDIV_w  = DataProcessing2SourceFixed | 0x00000800u,
  UDIV_x  = DataProcessing2SourceFixed | 0x80000800u,
  UDIV    = UDIV_w,
  SDIV_w  = DataProcessing2SourceFixed | 0x00000C00u,
  SDIV_x  = DataProcessing2SourceFixed | 0x80000C00u,
  SDIV    = SDIV_w,
  LSLV_w  = DataProcessing2SourceFixed | 0x00002000u,
  LSLV_x  = DataProcessing2SourceFixed | 0x80002000u,
  LSLV    = LSLV_w,
  LSRV_w  = DataProcessing2SourceFixed | 0x00002400u,
  LSRV_x  = DataProcessing2SourceFixed | 0x80002400u,
  LSRV    = LSRV_w,
  ASRV_w  = DataProcessing2SourceFixed | 0x00002800u,
  ASRV_x  = DataProcessing2SourceFixed | 0x80002800u,
  ASRV    = ASRV_w,
  RORV_w  = DataProcessing2SourceFixed | 0x00002C00u,
  RORV_x  = DataProcessing2SourceFixed | 0x80002C00u,
  RORV    = RORV_w,
  PACGA   = DataProcessing2SourceFixed | SixtyFourBits | 0x00003000u,
  CRC32B  = DataProcessing2SourceFixed | 0x00004000u,
  CRC32H  = DataProcessing2SourceFixed | 0x00004400u,
  CRC32W  = DataProcessing2SourceFixed | 0x00004800u,
  CRC32X  = DataProcessing2SourceFixed | SixtyFourBits | 0x00004C00u,
  CRC32CB = DataProcessing2SourceFixed | 0x00005000u,
  CRC32CH = DataProcessing2SourceFixed | 0x00005400u,
  CRC32CW = DataProcessing2SourceFixed | 0x00005800u,
  CRC32CX = DataProcessing2SourceFixed | SixtyFourBits | 0x00005C00u
};

// Data processing 3 source.
enum DataProcessing3SourceOp : uint32_t {
  DataProcessing3SourceFixed = 0x1B000000u,
  DataProcessing3SourceFMask = 0x1F000000u,
  DataProcessing3SourceMask  = 0xFFE08000u,
  MADD_w                     = DataProcessing3SourceFixed | 0x00000000u,
  MADD_x                     = DataProcessing3SourceFixed | 0x80000000u,
  MADD                       = MADD_w,
  MSUB_w                     = DataProcessing3SourceFixed | 0x00008000u,
  MSUB_x                     = DataProcessing3SourceFixed | 0x80008000u,
  MSUB                       = MSUB_w,
  SMADDL_x                   = DataProcessing3SourceFixed | 0x80200000u,
  SMSUBL_x                   = DataProcessing3SourceFixed | 0x80208000u,
  SMULH_x                    = DataProcessing3SourceFixed | 0x80400000u,
  UMADDL_x                   = DataProcessing3SourceFixed | 0x80A00000u,
  UMSUBL_x                   = DataProcessing3SourceFixed | 0x80A08000u,
  UMULH_x                    = DataProcessing3SourceFixed | 0x80C00000u
};

// Floating point compare.
enum FPCompareOp : uint32_t {
  FPCompareFixed = 0x1E202000u,
  FPCompareFMask = 0x5F203C00u,
  FPCompareMask  = 0xFFE0FC1Fu,
  FCMP_h         = FPCompareFixed | FP16 | 0x00000000u,
  FCMP_s         = FPCompareFixed | 0x00000000u,
  FCMP_d         = FPCompareFixed | FP64 | 0x00000000u,
  FCMP           = FCMP_s,
  FCMP_h_zero    = FPCompareFixed | FP16 | 0x00000008u,
  FCMP_s_zero    = FPCompareFixed | 0x00000008u,
  FCMP_d_zero    = FPCompareFixed | FP64 | 0x00000008u,
  FCMP_zero      = FCMP_s_zero,
  FCMPE_h        = FPCompareFixed | FP16 | 0x00000010u,
  FCMPE_s        = FPCompareFixed | 0x00000010u,
  FCMPE_d        = FPCompareFixed | FP64 | 0x00000010u,
  FCMPE          = FCMPE_s,
  FCMPE_h_zero   = FPCompareFixed | FP16 | 0x00000018u,
  FCMPE_s_zero   = FPCompareFixed | 0x00000018u,
  FCMPE_d_zero   = FPCompareFixed | FP64 | 0x00000018u,
  FCMPE_zero     = FCMPE_s_zero
};

// Floating point conditional compare.
enum FPConditionalCompareOp : uint32_t {
  FPConditionalCompareFixed = 0x1E200400u,
  FPConditionalCompareFMask = 0x5F200C00u,
  FPConditionalCompareMask  = 0xFFE00C10u,
  FCCMP_h                   = FPConditionalCompareFixed | FP16 | 0x00000000u,
  FCCMP_s                   = FPConditionalCompareFixed | 0x00000000u,
  FCCMP_d                   = FPConditionalCompareFixed | FP64 | 0x00000000u,
  FCCMP                     = FCCMP_s,
  FCCMPE_h                  = FPConditionalCompareFixed | FP16 | 0x00000010u,
  FCCMPE_s                  = FPConditionalCompareFixed | 0x00000010u,
  FCCMPE_d                  = FPConditionalCompareFixed | FP64 | 0x00000010u,
  FCCMPE                    = FCCMPE_s
};

// Floating point conditional select.
enum FPConditionalSelectOp : uint32_t {
  FPConditionalSelectFixed = 0x1E200C00u,
  FPConditionalSelectFMask = 0x5F200C00u,
  FPConditionalSelectMask  = 0xFFE00C00u,
  FCSEL_h                  = FPConditionalSelectFixed | FP16 | 0x00000000u,
  FCSEL_s                  = FPConditionalSelectFixed | 0x00000000u,
  FCSEL_d                  = FPConditionalSelectFixed | FP64 | 0x00000000u,
  FCSEL                    = FCSEL_s
};

// Floating point immediate.
enum FPImmediateOp : uint32_t {
  FPImmediateFixed = 0x1E201000u,
  FPImmediateFMask = 0x5F201C00u,
  FPImmediateMask  = 0xFFE01C00u,
  FMOV_h_imm       = FPImmediateFixed | FP16 | 0x00000000u,
  FMOV_s_imm       = FPImmediateFixed | 0x00000000u,
  FMOV_d_imm       = FPImmediateFixed | FP64 | 0x00000000u
};

// Floating point data processing 1 source.
enum FPDataProcessing1SourceOp : uint32_t {
  FPDataProcessing1SourceFixed = 0x1E204000u,
  FPDataProcessing1SourceFMask = 0x5F207C00u,
  FPDataProcessing1SourceMask  = 0xFFFFFC00u,
  FMOV_h   = FPDataProcessing1SourceFixed | FP16 | 0x00000000u,
  FMOV_s   = FPDataProcessing1SourceFixed | 0x00000000u,
  FMOV_d   = FPDataProcessing1SourceFixed | FP64 | 0x00000000u,
  FMOV     = FMOV_s,
  FABS_h   = FPDataProcessing1SourceFixed | FP16 | 0x00008000u,
  FABS_s   = FPDataProcessing1SourceFixed | 0x00008000u,
  FABS_d   = FPDataProcessing1SourceFixed | FP64 | 0x00008000u,
  FABS     = FABS_s,
  FNEG_h   = FPDataProcessing1SourceFixed | FP16 | 0x00010000u,
  FNEG_s   = FPDataProcessing1SourceFixed | 0x00010000u,
  FNEG_d   = FPDataProcessing1SourceFixed | FP64 | 0x00010000u,
  FNEG     = FNEG_s,
  FSQRT_h  = FPDataProcessing1SourceFixed | FP16 | 0x00018000u,
  FSQRT_s  = FPDataProcessing1SourceFixed | 0x00018000u,
  FSQRT_d  = FPDataProcessing1SourceFixed | FP64 | 0x00018000u,
  FSQRT    = FSQRT_s,
  FCVT_ds  = FPDataProcessing1SourceFixed | 0x00028000,
  FCVT_sd  = FPDataProcessing1SourceFixed | FP64 | 0x00020000,
  FCVT_hs  = FPDataProcessing1SourceFixed | 0x00038000,
  FCVT_hd  = FPDataProcessing1SourceFixed | FP64 | 0x00038000,
  FCVT_sh  = FPDataProcessing1SourceFixed | 0x00C20000,
  FCVT_dh  = FPDataProcessing1SourceFixed | 0x00C28000,
  FRINT32X_s = FPDataProcessing1SourceFixed | 0x00088000u,
  FRINT32X_d = FPDataProcessing1SourceFixed | FP64 | 0x00088000u,
  FRINT32X = FRINT32X_s,
  FRINT32Z_s = FPDataProcessing1SourceFixed | 0x00080000u,
  FRINT32Z_d = FPDataProcessing1SourceFixed | FP64 | 0x00080000u,
  FRINT32Z = FRINT32Z_s,
  FRINT64X_s = FPDataProcessing1SourceFixed | 0x00098000u,
  FRINT64X_d = FPDataProcessing1SourceFixed | FP64 | 0x00098000u,
  FRINT64X = FRINT64X_s,
  FRINT64Z_s = FPDataProcessing1SourceFixed | 0x00090000u,
  FRINT64Z_d = FPDataProcessing1SourceFixed | FP64 | 0x00090000u,
  FRINT64Z = FRINT64Z_s,
  FRINTN_h = FPDataProcessing1SourceFixed | FP16 | 0x00040000u,
  FRINTN_s = FPDataProcessing1SourceFixed | 0x00040000u,
  FRINTN_d = FPDataProcessing1SourceFixed | FP64 | 0x00040000u,
  FRINTN   = FRINTN_s,
  FRINTP_h = FPDataProcessing1SourceFixed | FP16 | 0x00048000u,
  FRINTP_s = FPDataProcessing1SourceFixed | 0x00048000u,
  FRINTP_d = FPDataProcessing1SourceFixed | FP64 | 0x00048000u,
  FRINTP   = FRINTP_s,
  FRINTM_h = FPDataProcessing1SourceFixed | FP16 | 0x00050000u,
  FRINTM_s = FPDataProcessing1SourceFixed | 0x00050000u,
  FRINTM_d = FPDataProcessing1SourceFixed | FP64 | 0x00050000u,
  FRINTM   = FRINTM_s,
  FRINTZ_h = FPDataProcessing1SourceFixed | FP16 | 0x00058000u,
  FRINTZ_s = FPDataProcessing1SourceFixed | 0x00058000u,
  FRINTZ_d = FPDataProcessing1SourceFixed | FP64 | 0x00058000u,
  FRINTZ   = FRINTZ_s,
  FRINTA_h = FPDataProcessing1SourceFixed | FP16 | 0x00060000u,
  FRINTA_s = FPDataProcessing1SourceFixed | 0x00060000u,
  FRINTA_d = FPDataProcessing1SourceFixed | FP64 | 0x00060000u,
  FRINTA   = FRINTA_s,
  FRINTX_h = FPDataProcessing1SourceFixed | FP16 | 0x00070000u,
  FRINTX_s = FPDataProcessing1SourceFixed | 0x00070000u,
  FRINTX_d = FPDataProcessing1SourceFixed | FP64 | 0x00070000u,
  FRINTX   = FRINTX_s,
  FRINTI_h = FPDataProcessing1SourceFixed | FP16 | 0x00078000u,
  FRINTI_s = FPDataProcessing1SourceFixed | 0x00078000u,
  FRINTI_d = FPDataProcessing1SourceFixed | FP64 | 0x00078000u,
  FRINTI   = FRINTI_s
};

// Floating point data processing 2 source.
enum FPDataProcessing2SourceOp : uint32_t {
  FPDataProcessing2SourceFixed = 0x1E200800u,
  FPDataProcessing2SourceFMask = 0x5F200C00u,
  FPDataProcessing2SourceMask  = 0xFFE0FC00u,
  FMUL     = FPDataProcessing2SourceFixed | 0x00000000u,
  FMUL_h   = FMUL | FP16,
  FMUL_s   = FMUL,
  FMUL_d   = FMUL | FP64,
  FDIV     = FPDataProcessing2SourceFixed | 0x00001000u,
  FDIV_h   = FDIV | FP16,
  FDIV_s   = FDIV,
  FDIV_d   = FDIV | FP64,
  FADD     = FPDataProcessing2SourceFixed | 0x00002000u,
  FADD_h   = FADD | FP16,
  FADD_s   = FADD,
  FADD_d   = FADD | FP64,
  FSUB     = FPDataProcessing2SourceFixed | 0x00003000u,
  FSUB_h   = FSUB | FP16,
  FSUB_s   = FSUB,
  FSUB_d   = FSUB | FP64,
  FMAX     = FPDataProcessing2SourceFixed | 0x00004000u,
  FMAX_h   = FMAX | FP16,
  FMAX_s   = FMAX,
  FMAX_d   = FMAX | FP64,
  FMIN     = FPDataProcessing2SourceFixed | 0x00005000u,
  FMIN_h   = FMIN | FP16,
  FMIN_s   = FMIN,
  FMIN_d   = FMIN | FP64,
  FMAXNM   = FPDataProcessing2SourceFixed | 0x00006000u,
  FMAXNM_h = FMAXNM | FP16,
  FMAXNM_s = FMAXNM,
  FMAXNM_d = FMAXNM | FP64,
  FMINNM   = FPDataProcessing2SourceFixed | 0x00007000u,
  FMINNM_h = FMINNM | FP16,
  FMINNM_s = FMINNM,
  FMINNM_d = FMINNM | FP64,
  FNMUL    = FPDataProcessing2SourceFixed | 0x00008000u,
  FNMUL_h  = FNMUL | FP16,
  FNMUL_s  = FNMUL,
  FNMUL_d  = FNMUL | FP64
};

// Floating point data processing 3 source.
enum FPDataProcessing3SourceOp : uint32_t {
  FPDataProcessing3SourceFixed = 0x1F000000u,
  FPDataProcessing3SourceFMask = 0x5F000000u,
  FPDataProcessing3SourceMask  = 0xFFE08000u,
  FMADD_h                      = FPDataProcessing3SourceFixed | 0x00C00000u,
  FMSUB_h                      = FPDataProcessing3SourceFixed | 0x00C08000u,
  FNMADD_h                     = FPDataProcessing3SourceFixed | 0x00E00000u,
  FNMSUB_h                     = FPDataProcessing3SourceFixed | 0x00E08000u,
  FMADD_s                      = FPDataProcessing3SourceFixed | 0x00000000u,
  FMSUB_s                      = FPDataProcessing3SourceFixed | 0x00008000u,
  FNMADD_s                     = FPDataProcessing3SourceFixed | 0x00200000u,
  FNMSUB_s                     = FPDataProcessing3SourceFixed | 0x00208000u,
  FMADD_d                      = FPDataProcessing3SourceFixed | 0x00400000u,
  FMSUB_d                      = FPDataProcessing3SourceFixed | 0x00408000u,
  FNMADD_d                     = FPDataProcessing3SourceFixed | 0x00600000u,
  FNMSUB_d                     = FPDataProcessing3SourceFixed | 0x00608000u
};

// Conversion between floating point and integer.
enum FPIntegerConvertOp : uint32_t {
  FPIntegerConvertFixed = 0x1E200000u,
  FPIntegerConvertFMask = 0x5F20FC00u,
  FPIntegerConvertMask  = 0xFFFFFC00u,
  FCVTNS    = FPIntegerConvertFixed | 0x00000000u,
  FCVTNS_wh = FCVTNS | FP16,
  FCVTNS_xh = FCVTNS | SixtyFourBits | FP16,
  FCVTNS_ws = FCVTNS,
  FCVTNS_xs = FCVTNS | SixtyFourBits,
  FCVTNS_wd = FCVTNS | FP64,
  FCVTNS_xd = FCVTNS | SixtyFourBits | FP64,
  FCVTNU    = FPIntegerConvertFixed | 0x00010000u,
  FCVTNU_wh = FCVTNU | FP16,
  FCVTNU_xh = FCVTNU | SixtyFourBits | FP16,
  FCVTNU_ws = FCVTNU,
  FCVTNU_xs = FCVTNU | SixtyFourBits,
  FCVTNU_wd = FCVTNU | FP64,
  FCVTNU_xd = FCVTNU | SixtyFourBits | FP64,
  FCVTPS    = FPIntegerConvertFixed | 0x00080000u,
  FCVTPS_wh = FCVTPS | FP16,
  FCVTPS_xh = FCVTPS | SixtyFourBits | FP16,
  FCVTPS_ws = FCVTPS,
  FCVTPS_xs = FCVTPS | SixtyFourBits,
  FCVTPS_wd = FCVTPS | FP64,
  FCVTPS_xd = FCVTPS | SixtyFourBits | FP64,
  FCVTPU    = FPIntegerConvertFixed | 0x00090000u,
  FCVTPU_wh = FCVTPU | FP16,
  FCVTPU_xh = FCVTPU | SixtyFourBits | FP16,
  FCVTPU_ws = FCVTPU,
  FCVTPU_xs = FCVTPU | SixtyFourBits,
  FCVTPU_wd = FCVTPU | FP64,
  FCVTPU_xd = FCVTPU | SixtyFourBits | FP64,
  FCVTMS    = FPIntegerConvertFixed | 0x00100000u,
  FCVTMS_wh = FCVTMS | FP16,
  FCVTMS_xh = FCVTMS | SixtyFourBits | FP16,
  FCVTMS_ws = FCVTMS,
  FCVTMS_xs = FCVTMS | SixtyFourBits,
  FCVTMS_wd = FCVTMS | FP64,
  FCVTMS_xd = FCVTMS | SixtyFourBits | FP64,
  FCVTMU    = FPIntegerConvertFixed | 0x00110000u,
  FCVTMU_wh = FCVTMU | FP16,
  FCVTMU_xh = FCVTMU | SixtyFourBits | FP16,
  FCVTMU_ws = FCVTMU,
  FCVTMU_xs = FCVTMU | SixtyFourBits,
  FCVTMU_wd = FCVTMU | FP64,
  FCVTMU_xd = FCVTMU | SixtyFourBits | FP64,
  FCVTZS    = FPIntegerConvertFixed | 0x00180000u,
  FCVTZS_wh = FCVTZS | FP16,
  FCVTZS_xh = FCVTZS | SixtyFourBits | FP16,
  FCVTZS_ws = FCVTZS,
  FCVTZS_xs = FCVTZS | SixtyFourBits,
  FCVTZS_wd = FCVTZS | FP64,
  FCVTZS_xd = FCVTZS | SixtyFourBits | FP64,
  FCVTZU    = FPIntegerConvertFixed | 0x00190000u,
  FCVTZU_wh = FCVTZU | FP16,
  FCVTZU_xh = FCVTZU | SixtyFourBits | FP16,
  FCVTZU_ws = FCVTZU,
  FCVTZU_xs = FCVTZU | SixtyFourBits,
  FCVTZU_wd = FCVTZU | FP64,
  FCVTZU_xd = FCVTZU | SixtyFourBits | FP64,
  SCVTF     = FPIntegerConvertFixed | 0x00020000u,
  SCVTF_hw  = SCVTF | FP16,
  SCVTF_hx  = SCVTF | SixtyFourBits | FP16,
  SCVTF_sw  = SCVTF,
  SCVTF_sx  = SCVTF | SixtyFourBits,
  SCVTF_dw  = SCVTF | FP64,
  SCVTF_dx  = SCVTF | SixtyFourBits | FP64,
  UCVTF     = FPIntegerConvertFixed | 0x00030000u,
  UCVTF_hw  = UCVTF | FP16,
  UCVTF_hx  = UCVTF | SixtyFourBits | FP16,
  UCVTF_sw  = UCVTF,
  UCVTF_sx  = UCVTF | SixtyFourBits,
  UCVTF_dw  = UCVTF | FP64,
  UCVTF_dx  = UCVTF | SixtyFourBits | FP64,
  FCVTAS    = FPIntegerConvertFixed | 0x00040000u,
  FCVTAS_wh = FCVTAS | FP16,
  FCVTAS_xh = FCVTAS | SixtyFourBits | FP16,
  FCVTAS_ws = FCVTAS,
  FCVTAS_xs = FCVTAS | SixtyFourBits,
  FCVTAS_wd = FCVTAS | FP64,
  FCVTAS_xd = FCVTAS | SixtyFourBits | FP64,
  FCVTAU    = FPIntegerConvertFixed | 0x00050000u,
  FCVTAU_wh = FCVTAU | FP16,
  FCVTAU_xh = FCVTAU | SixtyFourBits | FP16,
  FCVTAU_ws = FCVTAU,
  FCVTAU_xs = FCVTAU | SixtyFourBits,
  FCVTAU_wd = FCVTAU | FP64,
  FCVTAU_xd = FCVTAU | SixtyFourBits | FP64,
  FMOV_wh   = FPIntegerConvertFixed | 0x00060000u | FP16,
  FMOV_hw   = FPIntegerConvertFixed | 0x00070000u | FP16,
  FMOV_xh   = FMOV_wh | SixtyFourBits,
  FMOV_hx   = FMOV_hw | SixtyFourBits,
  FMOV_ws   = FPIntegerConvertFixed | 0x00060000u,
  FMOV_sw   = FPIntegerConvertFixed | 0x00070000u,
  FMOV_xd   = FMOV_ws | SixtyFourBits | FP64,
  FMOV_dx   = FMOV_sw | SixtyFourBits | FP64,
  FMOV_d1_x = FPIntegerConvertFixed | SixtyFourBits | 0x008F0000u,
  FMOV_x_d1 = FPIntegerConvertFixed | SixtyFourBits | 0x008E0000u,
  FJCVTZS   = FPIntegerConvertFixed | FP64 | 0x001E0000
};

// Conversion between fixed point and floating point.
enum FPFixedPointConvertOp : uint32_t {
  FPFixedPointConvertFixed = 0x1E000000u,
  FPFixedPointConvertFMask = 0x5F200000u,
  FPFixedPointConvertMask  = 0xFFFF0000u,
  FCVTZS_fixed    = FPFixedPointConvertFixed | 0x00180000u,
  FCVTZS_wh_fixed = FCVTZS_fixed | FP16,
  FCVTZS_xh_fixed = FCVTZS_fixed | SixtyFourBits | FP16,
  FCVTZS_ws_fixed = FCVTZS_fixed,
  FCVTZS_xs_fixed = FCVTZS_fixed | SixtyFourBits,
  FCVTZS_wd_fixed = FCVTZS_fixed | FP64,
  FCVTZS_xd_fixed = FCVTZS_fixed | SixtyFourBits | FP64,
  FCVTZU_fixed    = FPFixedPointConvertFixed | 0x00190000u,
  FCVTZU_wh_fixed = FCVTZU_fixed | FP16,
  FCVTZU_xh_fixed = FCVTZU_fixed | SixtyFourBits | FP16,
  FCVTZU_ws_fixed = FCVTZU_fixed,
  FCVTZU_xs_fixed = FCVTZU_fixed | SixtyFourBits,
  FCVTZU_wd_fixed = FCVTZU_fixed | FP64,
  FCVTZU_xd_fixed = FCVTZU_fixed | SixtyFourBits | FP64,
  SCVTF_fixed     = FPFixedPointConvertFixed | 0x00020000u,
  SCVTF_hw_fixed  = SCVTF_fixed | FP16,
  SCVTF_hx_fixed  = SCVTF_fixed | SixtyFourBits | FP16,
  SCVTF_sw_fixed  = SCVTF_fixed,
  SCVTF_sx_fixed  = SCVTF_fixed | SixtyFourBits,
  SCVTF_dw_fixed  = SCVTF_fixed | FP64,
  SCVTF_dx_fixed  = SCVTF_fixed | SixtyFourBits | FP64,
  UCVTF_fixed     = FPFixedPointConvertFixed | 0x00030000u,
  UCVTF_hw_fixed  = UCVTF_fixed | FP16,
  UCVTF_hx_fixed  = UCVTF_fixed | SixtyFourBits | FP16,
  UCVTF_sw_fixed  = UCVTF_fixed,
  UCVTF_sx_fixed  = UCVTF_fixed | SixtyFourBits,
  UCVTF_dw_fixed  = UCVTF_fixed | FP64,
  UCVTF_dx_fixed  = UCVTF_fixed | SixtyFourBits | FP64
};

// Crypto - two register SHA.
enum Crypto2RegSHAOp : uint32_t {
  Crypto2RegSHAFixed = 0x5E280800u,
  Crypto2RegSHAFMask = 0xFF3E0C00u
};

// Crypto - three register SHA.
enum Crypto3RegSHAOp : uint32_t {
  Crypto3RegSHAFixed = 0x5E000000u,
  Crypto3RegSHAFMask = 0xFF208C00u
};

// Crypto - AES.
enum CryptoAESOp : uint32_t {
  CryptoAESFixed = 0x4E280800u,
  CryptoAESFMask = 0xFF3E0C00u
};

// NEON instructions with two register operands.
enum NEON2RegMiscOp : uint32_t {
  NEON2RegMiscFixed = 0x0E200800u,
  NEON2RegMiscFMask = 0x9F3E0C00u,
  NEON2RegMiscMask  = 0xBF3FFC00u,
  NEON2RegMiscUBit  = 0x20000000u,
  NEON_REV64     = NEON2RegMiscFixed | 0x00000000u,
  NEON_REV32     = NEON2RegMiscFixed | 0x20000000u,
  NEON_REV16     = NEON2RegMiscFixed | 0x00001000u,
  NEON_SADDLP    = NEON2RegMiscFixed | 0x00002000u,
  NEON_UADDLP    = NEON_SADDLP | NEON2RegMiscUBit,
  NEON_SUQADD    = NEON2RegMiscFixed | 0x00003000u,
  NEON_USQADD    = NEON_SUQADD | NEON2RegMiscUBit,
  NEON_CLS       = NEON2RegMiscFixed | 0x00004000u,
  NEON_CLZ       = NEON2RegMiscFixed | 0x20004000u,
  NEON_CNT       = NEON2RegMiscFixed | 0x00005000u,
  NEON_RBIT_NOT  = NEON2RegMiscFixed | 0x20005000u,
  NEON_SADALP    = NEON2RegMiscFixed | 0x00006000u,
  NEON_UADALP    = NEON_SADALP | NEON2RegMiscUBit,
  NEON_SQABS     = NEON2RegMiscFixed | 0x00007000u,
  NEON_SQNEG     = NEON2RegMiscFixed | 0x20007000u,
  NEON_CMGT_zero = NEON2RegMiscFixed | 0x00008000u,
  NEON_CMGE_zero = NEON2RegMiscFixed | 0x20008000u,
  NEON_CMEQ_zero = NEON2RegMiscFixed | 0x00009000u,
  NEON_CMLE_zero = NEON2RegMiscFixed | 0x20009000u,
  NEON_CMLT_zero = NEON2RegMiscFixed | 0x0000A000u,
  NEON_ABS       = NEON2RegMiscFixed | 0x0000B000u,
  NEON_NEG       = NEON2RegMiscFixed | 0x2000B000u,
  NEON_XTN       = NEON2RegMiscFixed | 0x00012000u,
  NEON_SQXTUN    = NEON2RegMiscFixed | 0x20012000u,
  NEON_SHLL      = NEON2RegMiscFixed | 0x20013000u,
  NEON_SQXTN     = NEON2RegMiscFixed | 0x00014000u,
  NEON_UQXTN     = NEON_SQXTN | NEON2RegMiscUBit,

  NEON2RegMiscOpcode = 0x0001F000u,
  NEON_RBIT_NOT_opcode = NEON_RBIT_NOT & NEON2RegMiscOpcode,
  NEON_NEG_opcode = NEON_NEG & NEON2RegMiscOpcode,
  NEON_XTN_opcode = NEON_XTN & NEON2RegMiscOpcode,
  NEON_UQXTN_opcode = NEON_UQXTN & NEON2RegMiscOpcode,

  // These instructions use only one bit of the size field. The other bit is
  // used to distinguish between instructions.
  NEON2RegMiscFPMask = NEON2RegMiscMask | 0x00800000u,
  NEON_FABS   = NEON2RegMiscFixed | 0x0080F000u,
  NEON_FNEG   = NEON2RegMiscFixed | 0x2080F000u,
  NEON_FCVTN  = NEON2RegMiscFixed | 0x00016000u,
  NEON_FCVTXN = NEON2RegMiscFixed | 0x20016000u,
  NEON_FCVTL  = NEON2RegMiscFixed | 0x00017000u,
  NEON_FRINT32X = NEON2RegMiscFixed | 0x2001E000u,
  NEON_FRINT32Z = NEON2RegMiscFixed | 0x0001E000u,
  NEON_FRINT64X = NEON2RegMiscFixed | 0x2001F000u,
  NEON_FRINT64Z = NEON2RegMiscFixed | 0x0001F000u,
  NEON_FRINTN = NEON2RegMiscFixed | 0x00018000u,
  NEON_FRINTA = NEON2RegMiscFixed | 0x20018000u,
  NEON_FRINTP = NEON2RegMiscFixed | 0x00818000u,
  NEON_FRINTM = NEON2RegMiscFixed | 0x00019000u,
  NEON_FRINTX = NEON2RegMiscFixed | 0x20019000u,
  NEON_FRINTZ = NEON2RegMiscFixed | 0x00819000u,
  NEON_FRINTI = NEON2RegMiscFixed | 0x20819000u,
  NEON_FCVTNS = NEON2RegMiscFixed | 0x0001A000u,
  NEON_FCVTNU = NEON_FCVTNS | NEON2RegMiscUBit,
  NEON_FCVTPS = NEON2RegMiscFixed | 0x0081A000u,
  NEON_FCVTPU = NEON_FCVTPS | NEON2RegMiscUBit,
  NEON_FCVTMS = NEON2RegMiscFixed | 0x0001B000u,
  NEON_FCVTMU = NEON_FCVTMS | NEON2RegMiscUBit,
  NEON_FCVTZS = NEON2RegMiscFixed | 0x0081B000u,
  NEON_FCVTZU = NEON_FCVTZS | NEON2RegMiscUBit,
  NEON_FCVTAS = NEON2RegMiscFixed | 0x0001C000u,
  NEON_FCVTAU = NEON_FCVTAS | NEON2RegMiscUBit,
  NEON_FSQRT  = NEON2RegMiscFixed | 0x2081F000u,
  NEON_SCVTF  = NEON2RegMiscFixed | 0x0001D000u,
  NEON_UCVTF  = NEON_SCVTF | NEON2RegMiscUBit,
  NEON_URSQRTE = NEON2RegMiscFixed | 0x2081C000u,
  NEON_URECPE  = NEON2RegMiscFixed | 0x0081C000u,
  NEON_FRSQRTE = NEON2RegMiscFixed | 0x2081D000u,
  NEON_FRECPE  = NEON2RegMiscFixed | 0x0081D000u,
  NEON_FCMGT_zero = NEON2RegMiscFixed | 0x0080C000u,
  NEON_FCMGE_zero = NEON2RegMiscFixed | 0x2080C000u,
  NEON_FCMEQ_zero = NEON2RegMiscFixed | 0x0080D000u,
  NEON_FCMLE_zero = NEON2RegMiscFixed | 0x2080D000u,
  NEON_FCMLT_zero = NEON2RegMiscFixed | 0x0080E000u,

  NEON_FCVTL_opcode = NEON_FCVTL & NEON2RegMiscOpcode,
  NEON_FCVTN_opcode = NEON_FCVTN & NEON2RegMiscOpcode
};

// NEON instructions with two register operands (FP16).
enum NEON2RegMiscFP16Op : uint32_t {
  NEON2RegMiscFP16Fixed = 0x0E780800u,
  NEON2RegMiscFP16FMask = 0x9F7E0C00u,
  NEON2RegMiscFP16Mask  = 0xBFFFFC00u,
  NEON_FRINTN_H     = NEON2RegMiscFP16Fixed | 0x00018000u,
  NEON_FRINTM_H     = NEON2RegMiscFP16Fixed | 0x00019000u,
  NEON_FCVTNS_H     = NEON2RegMiscFP16Fixed | 0x0001A000u,
  NEON_FCVTMS_H     = NEON2RegMiscFP16Fixed | 0x0001B000u,
  NEON_FCVTAS_H     = NEON2RegMiscFP16Fixed | 0x0001C000u,
  NEON_SCVTF_H      = NEON2RegMiscFP16Fixed | 0x0001D000u,
  NEON_FCMGT_H_zero = NEON2RegMiscFP16Fixed | 0x0080C000u,
  NEON_FCMEQ_H_zero = NEON2RegMiscFP16Fixed | 0x0080D000u,
  NEON_FCMLT_H_zero = NEON2RegMiscFP16Fixed | 0x0080E000u,
  NEON_FABS_H       = NEON2RegMiscFP16Fixed | 0x0080F000u,
  NEON_FRINTP_H     = NEON2RegMiscFP16Fixed | 0x00818000u,
  NEON_FRINTZ_H     = NEON2RegMiscFP16Fixed | 0x00819000u,
  NEON_FCVTPS_H     = NEON2RegMiscFP16Fixed | 0x0081A000u,
  NEON_FCVTZS_H     = NEON2RegMiscFP16Fixed | 0x0081B000u,
  NEON_FRECPE_H     = NEON2RegMiscFP16Fixed | 0x0081D000u,
  NEON_FRINTA_H     = NEON2RegMiscFP16Fixed | 0x20018000u,
  NEON_FRINTX_H     = NEON2RegMiscFP16Fixed | 0x20019000u,
  NEON_FCVTNU_H     = NEON2RegMiscFP16Fixed | 0x2001A000u,
  NEON_FCVTMU_H     = NEON2RegMiscFP16Fixed | 0x2001B000u,
  NEON_FCVTAU_H     = NEON2RegMiscFP16Fixed | 0x2001C000u,
  NEON_UCVTF_H      = NEON2RegMiscFP16Fixed | 0x2001D000u,
  NEON_FCMGE_H_zero = NEON2RegMiscFP16Fixed | 0x2080C000u,
  NEON_FCMLE_H_zero = NEON2RegMiscFP16Fixed | 0x2080D000u,
  NEON_FNEG_H       = NEON2RegMiscFP16Fixed | 0x2080F000u,
  NEON_FRINTI_H     = NEON2RegMiscFP16Fixed | 0x20819000u,
  NEON_FCVTPU_H     = NEON2RegMiscFP16Fixed | 0x2081A000u,
  NEON_FCVTZU_H     = NEON2RegMiscFP16Fixed | 0x2081B000u,
  NEON_FRSQRTE_H    = NEON2RegMiscFP16Fixed | 0x2081D000u,
  NEON_FSQRT_H      = NEON2RegMiscFP16Fixed | 0x2081F000u
};

// NEON instructions with three same-type operands.
enum NEON3SameOp : uint32_t {
  NEON3SameFixed = 0x0E200400u,
  NEON3SameFMask = 0x9F200400u,
  NEON3SameMask =  0xBF20FC00u,
  NEON3SameUBit =  0x20000000u,
  NEON_ADD    = NEON3SameFixed | 0x00008000u,
  NEON_ADDP   = NEON3SameFixed | 0x0000B800u,
  NEON_SHADD  = NEON3SameFixed | 0x00000000u,
  NEON_SHSUB  = NEON3SameFixed | 0x00002000u,
  NEON_SRHADD = NEON3SameFixed | 0x00001000u,
  NEON_CMEQ   = NEON3SameFixed | NEON3SameUBit | 0x00008800u,
  NEON_CMGE   = NEON3SameFixed | 0x00003800u,
  NEON_CMGT   = NEON3SameFixed | 0x00003000u,
  NEON_CMHI   = NEON3SameFixed | NEON3SameUBit | NEON_CMGT,
  NEON_CMHS   = NEON3SameFixed | NEON3SameUBit | NEON_CMGE,
  NEON_CMTST  = NEON3SameFixed | 0x00008800u,
  NEON_MLA    = NEON3SameFixed | 0x00009000u,
  NEON_MLS    = NEON3SameFixed | 0x20009000u,
  NEON_MUL    = NEON3SameFixed | 0x00009800u,
  NEON_PMUL   = NEON3SameFixed | 0x20009800u,
  NEON_SRSHL  = NEON3SameFixed | 0x00005000u,
  NEON_SQSHL  = NEON3SameFixed | 0x00004800u,
  NEON_SQRSHL = NEON3SameFixed | 0x00005800u,
  NEON_SSHL   = NEON3SameFixed | 0x00004000u,
  NEON_SMAX   = NEON3SameFixed | 0x00006000u,
  NEON_SMAXP  = NEON3SameFixed | 0x0000A000u,
  NEON_SMIN   = NEON3SameFixed | 0x00006800u,
  NEON_SMINP  = NEON3SameFixed | 0x0000A800u,
  NEON_SABD   = NEON3SameFixed | 0x00007000u,
  NEON_SABA   = NEON3SameFixed | 0x00007800u,
  NEON_UABD   = NEON3SameFixed | NEON3SameUBit | NEON_SABD,
  NEON_UABA   = NEON3SameFixed | NEON3SameUBit | NEON_SABA,
  NEON_SQADD  = NEON3SameFixed | 0x00000800u,
  NEON_SQSUB  = NEON3SameFixed | 0x00002800u,
  NEON_SUB    = NEON3SameFixed | NEON3SameUBit | 0x00008000u,
  NEON_UHADD  = NEON3SameFixed | NEON3SameUBit | NEON_SHADD,
  NEON_UHSUB  = NEON3SameFixed | NEON3SameUBit | NEON_SHSUB,
  NEON_URHADD = NEON3SameFixed | NEON3SameUBit | NEON_SRHADD,
  NEON_UMAX   = NEON3SameFixed | NEON3SameUBit | NEON_SMAX,
  NEON_UMAXP  = NEON3SameFixed | NEON3SameUBit | NEON_SMAXP,
  NEON_UMIN   = NEON3SameFixed | NEON3SameUBit | NEON_SMIN,
  NEON_UMINP  = NEON3SameFixed | NEON3SameUBit | NEON_SMINP,
  NEON_URSHL  = NEON3SameFixed | NEON3SameUBit | NEON_SRSHL,
  NEON_UQADD  = NEON3SameFixed | NEON3SameUBit | NEON_SQADD,
  NEON_UQRSHL = NEON3SameFixed | NEON3SameUBit | NEON_SQRSHL,
  NEON_UQSHL  = NEON3SameFixed | NEON3SameUBit | NEON_SQSHL,
  NEON_UQSUB  = NEON3SameFixed | NEON3SameUBit | NEON_SQSUB,
  NEON_USHL   = NEON3SameFixed | NEON3SameUBit | NEON_SSHL,
  NEON_SQDMULH  = NEON3SameFixed | 0x0000B000u,
  NEON_SQRDMULH = NEON3SameFixed | 0x2000B000u,

  // NEON floating point instructions with three same-type operands.
  NEON3SameFPFixed = NEON3SameFixed | 0x0000C000u,
  NEON3SameFPFMask = NEON3SameFMask | 0x0000C000u,
  NEON3SameFPMask = NEON3SameMask | 0x00800000u,
  NEON_FADD    = NEON3SameFixed | 0x0000D000u,
  NEON_FSUB    = NEON3SameFixed | 0x0080D000u,
  NEON_FMUL    = NEON3SameFixed | 0x2000D800u,
  NEON_FDIV    = NEON3SameFixed | 0x2000F800u,
  NEON_FMAX    = NEON3SameFixed | 0x0000F000u,
  NEON_FMAXNM  = NEON3SameFixed | 0x0000C000u,
  NEON_FMAXP   = NEON3SameFixed | 0x2000F000u,
  NEON_FMAXNMP = NEON3SameFixed | 0x2000C000u,
  NEON_FMIN    = NEON3SameFixed | 0x0080F000u,
  NEON_FMINNM  = NEON3SameFixed | 0x0080C000u,
  NEON_FMINP   = NEON3SameFixed | 0x2080F000u,
  NEON_FMINNMP = NEON3SameFixed | 0x2080C000u,
  NEON_FMLA    = NEON3SameFixed | 0x0000C800u,
  NEON_FMLS    = NEON3SameFixed | 0x0080C800u,
  NEON_FMULX   = NEON3SameFixed | 0x0000D800u,
  NEON_FRECPS  = NEON3SameFixed | 0x0000F800u,
  NEON_FRSQRTS = NEON3SameFixed | 0x0080F800u,
  NEON_FABD    = NEON3SameFixed | 0x2080D000u,
  NEON_FADDP   = NEON3SameFixed | 0x2000D000u,
  NEON_FCMEQ   = NEON3SameFixed | 0x0000E000u,
  NEON_FCMGE   = NEON3SameFixed | 0x2000E000u,
  NEON_FCMGT   = NEON3SameFixed | 0x2080E000u,
  NEON_FACGE   = NEON3SameFixed | 0x2000E800u,
  NEON_FACGT   = NEON3SameFixed | 0x2080E800u,

  // NEON logical instructions with three same-type operands.
  NEON3SameLogicalFixed = NEON3SameFixed | 0x00001800u,
  NEON3SameLogicalFMask = NEON3SameFMask | 0x0000F800u,
  NEON3SameLogicalMask = 0xBFE0FC00u,
  NEON3SameLogicalFormatMask = NEON_Q,
  NEON_AND = NEON3SameLogicalFixed | 0x00000000u,
  NEON_ORR = NEON3SameLogicalFixed | 0x00A00000u,
  NEON_ORN = NEON3SameLogicalFixed | 0x00C00000u,
  NEON_EOR = NEON3SameLogicalFixed | 0x20000000u,
  NEON_BIC = NEON3SameLogicalFixed | 0x00400000u,
  NEON_BIF = NEON3SameLogicalFixed | 0x20C00000u,
  NEON_BIT = NEON3SameLogicalFixed | 0x20800000u,
  NEON_BSL = NEON3SameLogicalFixed | 0x20400000u,

  // FHM (FMLAL-like) instructions have an oddball encoding scheme under 3Same.
  NEON3SameFHMMask = 0xBFE0FC00u,                // U  size  opcode
  NEON_FMLAL   = NEON3SameFixed | 0x0000E800u,   // 0    00   11101
  NEON_FMLAL2  = NEON3SameFixed | 0x2000C800u,   // 1    00   11001
  NEON_FMLSL   = NEON3SameFixed | 0x0080E800u,   // 0    10   11101
  NEON_FMLSL2  = NEON3SameFixed | 0x2080C800u    // 1    10   11001
};


enum NEON3SameFP16 : uint32_t {
  NEON3SameFP16Fixed = 0x0E400400u,
  NEON3SameFP16FMask = 0x9F60C400u,
  NEON3SameFP16Mask =  0xBFE0FC00u,
  NEON_FMAXNM_H  = NEON3SameFP16Fixed | 0x00000000u,
  NEON_FMLA_H    = NEON3SameFP16Fixed | 0x00000800u,
  NEON_FADD_H    = NEON3SameFP16Fixed | 0x00001000u,
  NEON_FMULX_H   = NEON3SameFP16Fixed | 0x00001800u,
  NEON_FCMEQ_H   = NEON3SameFP16Fixed | 0x00002000u,
  NEON_FMAX_H    = NEON3SameFP16Fixed | 0x00003000u,
  NEON_FRECPS_H  = NEON3SameFP16Fixed | 0x00003800u,
  NEON_FMINNM_H  = NEON3SameFP16Fixed | 0x00800000u,
  NEON_FMLS_H    = NEON3SameFP16Fixed | 0x00800800u,
  NEON_FSUB_H    = NEON3SameFP16Fixed | 0x00801000u,
  NEON_FMIN_H    = NEON3SameFP16Fixed | 0x00803000u,
  NEON_FRSQRTS_H = NEON3SameFP16Fixed | 0x00803800u,
  NEON_FMAXNMP_H = NEON3SameFP16Fixed | 0x20000000u,
  NEON_FADDP_H   = NEON3SameFP16Fixed | 0x20001000u,
  NEON_FMUL_H    = NEON3SameFP16Fixed | 0x20001800u,
  NEON_FCMGE_H   = NEON3SameFP16Fixed | 0x20002000u,
  NEON_FACGE_H   = NEON3SameFP16Fixed | 0x20002800u,
  NEON_FMAXP_H   = NEON3SameFP16Fixed | 0x20003000u,
  NEON_FDIV_H    = NEON3SameFP16Fixed | 0x20003800u,
  NEON_FMINNMP_H = NEON3SameFP16Fixed | 0x20800000u,
  NEON_FABD_H    = NEON3SameFP16Fixed | 0x20801000u,
  NEON_FCMGT_H   = NEON3SameFP16Fixed | 0x20802000u,
  NEON_FACGT_H   = NEON3SameFP16Fixed | 0x20802800u,
  NEON_FMINP_H   = NEON3SameFP16Fixed | 0x20803000u
};


// 'Extra' NEON instructions with three same-type operands.
enum NEON3SameExtraOp : uint32_t {
  NEON3SameExtraFixed = 0x0E008400u,
  NEON3SameExtraUBit = 0x20000000u,
  NEON3SameExtraFMask = 0x9E208400u,
  NEON3SameExtraMask = 0xBE20FC00u,
  NEON_SQRDMLAH = NEON3SameExtraFixed | NEON3SameExtraUBit,
  NEON_SQRDMLSH = NEON3SameExtraFixed | NEON3SameExtraUBit | 0x00000800u,
  NEON_SDOT = NEON3SameExtraFixed | 0x00001000u,
  NEON_UDOT = NEON3SameExtraFixed | NEON3SameExtraUBit | 0x00001000u,

  /* v8.3 Complex Numbers */
  NEON3SameExtraFCFixed = 0x2E00C400u,
  NEON3SameExtraFCFMask = 0xBF20C400u,
  // FCMLA fixes opcode<3:2>, and uses opcode<1:0> to encode <rotate>.
  NEON3SameExtraFCMLAMask = NEON3SameExtraFCFMask | 0x00006000u,
  NEON_FCMLA = NEON3SameExtraFCFixed,
  // FCADD fixes opcode<3:2, 0>, and uses opcode<1> to encode <rotate>.
  NEON3SameExtraFCADDMask = NEON3SameExtraFCFMask | 0x00006800u,
  NEON_FCADD = NEON3SameExtraFCFixed | 0x00002000u
  // Other encodings under NEON3SameExtraFCFMask are UNALLOCATED.
};

// NEON instructions with three different-type operands.
enum NEON3DifferentOp : uint32_t {
  NEON3DifferentFixed = 0x0E200000u,
  NEON3DifferentFMask = 0x9F200C00u,
  NEON3DifferentMask  = 0xFF20FC00u,
  NEON_ADDHN    = NEON3DifferentFixed | 0x00004000u,
  NEON_ADDHN2   = NEON_ADDHN | NEON_Q,
  NEON_PMULL    = NEON3DifferentFixed | 0x0000E000u,
  NEON_PMULL2   = NEON_PMULL | NEON_Q,
  NEON_RADDHN   = NEON3DifferentFixed | 0x20004000u,
  NEON_RADDHN2  = NEON_RADDHN | NEON_Q,
  NEON_RSUBHN   = NEON3DifferentFixed | 0x20006000u,
  NEON_RSUBHN2  = NEON_RSUBHN | NEON_Q,
  NEON_SABAL    = NEON3DifferentFixed | 0x00005000u,
  NEON_SABAL2   = NEON_SABAL | NEON_Q,
  NEON_SABDL    = NEON3DifferentFixed | 0x00007000u,
  NEON_SABDL2   = NEON_SABDL | NEON_Q,
  NEON_SADDL    = NEON3DifferentFixed | 0x00000000u,
  NEON_SADDL2   = NEON_SADDL | NEON_Q,
  NEON_SADDW    = NEON3DifferentFixed | 0x00001000u,
  NEON_SADDW2   = NEON_SADDW | NEON_Q,
  NEON_SMLAL    = NEON3DifferentFixed | 0x00008000u,
  NEON_SMLAL2   = NEON_SMLAL | NEON_Q,
  NEON_SMLSL    = NEON3DifferentFixed | 0x0000A000u,
  NEON_SMLSL2   = NEON_SMLSL | NEON_Q,
  NEON_SMULL    = NEON3DifferentFixed | 0x0000C000u,
  NEON_SMULL2   = NEON_SMULL | NEON_Q,
  NEON_SSUBL    = NEON3DifferentFixed | 0x00002000u,
  NEON_SSUBL2   = NEON_SSUBL | NEON_Q,
  NEON_SSUBW    = NEON3DifferentFixed | 0x00003000u,
  NEON_SSUBW2   = NEON_SSUBW | NEON_Q,
  NEON_SQDMLAL  = NEON3DifferentFixed | 0x00009000u,
  NEON_SQDMLAL2 = NEON_SQDMLAL | NEON_Q,
  NEON_SQDMLSL  = NEON3DifferentFixed | 0x0000B000u,
  NEON_SQDMLSL2 = NEON_SQDMLSL | NEON_Q,
  NEON_SQDMULL  = NEON3DifferentFixed | 0x0000D000u,
  NEON_SQDMULL2 = NEON_SQDMULL | NEON_Q,
  NEON_SUBHN    = NEON3DifferentFixed | 0x00006000u,
  NEON_SUBHN2   = NEON_SUBHN | NEON_Q,
  NEON_UABAL    = NEON_SABAL | NEON3SameUBit,
  NEON_UABAL2   = NEON_UABAL | NEON_Q,
  NEON_UABDL    = NEON_SABDL | NEON3SameUBit,
  NEON_UABDL2   = NEON_UABDL | NEON_Q,
  NEON_UADDL    = NEON_SADDL | NEON3SameUBit,
  NEON_UADDL2   = NEON_UADDL | NEON_Q,
  NEON_UADDW    = NEON_SADDW | NEON3SameUBit,
  NEON_UADDW2   = NEON_UADDW | NEON_Q,
  NEON_UMLAL    = NEON_SMLAL | NEON3SameUBit,
  NEON_UMLAL2   = NEON_UMLAL | NEON_Q,
  NEON_UMLSL    = NEON_SMLSL | NEON3SameUBit,
  NEON_UMLSL2   = NEON_UMLSL | NEON_Q,
  NEON_UMULL    = NEON_SMULL | NEON3SameUBit,
  NEON_UMULL2   = NEON_UMULL | NEON_Q,
  NEON_USUBL    = NEON_SSUBL | NEON3SameUBit,
  NEON_USUBL2   = NEON_USUBL | NEON_Q,
  NEON_USUBW    = NEON_SSUBW | NEON3SameUBit,
  NEON_USUBW2   = NEON_USUBW | NEON_Q
};

// NEON instructions operating across vectors.
enum NEONAcrossLanesOp : uint32_t {
  NEONAcrossLanesFixed = 0x0E300800u,
  NEONAcrossLanesFMask = 0x9F3E0C00u,
  NEONAcrossLanesMask  = 0xBF3FFC00u,
  NEON_ADDV   = NEONAcrossLanesFixed | 0x0001B000u,
  NEON_SADDLV = NEONAcrossLanesFixed | 0x00003000u,
  NEON_UADDLV = NEONAcrossLanesFixed | 0x20003000u,
  NEON_SMAXV  = NEONAcrossLanesFixed | 0x0000A000u,
  NEON_SMINV  = NEONAcrossLanesFixed | 0x0001A000u,
  NEON_UMAXV  = NEONAcrossLanesFixed | 0x2000A000u,
  NEON_UMINV  = NEONAcrossLanesFixed | 0x2001A000u,

  NEONAcrossLanesFP16Fixed = NEONAcrossLanesFixed | 0x0000C000u,
  NEONAcrossLanesFP16FMask = NEONAcrossLanesFMask | 0x2000C000u,
  NEONAcrossLanesFP16Mask  = NEONAcrossLanesMask  | 0x20800000u,
  NEON_FMAXNMV_H = NEONAcrossLanesFP16Fixed | 0x00000000u,
  NEON_FMAXV_H   = NEONAcrossLanesFP16Fixed | 0x00003000u,
  NEON_FMINNMV_H = NEONAcrossLanesFP16Fixed | 0x00800000u,
  NEON_FMINV_H   = NEONAcrossLanesFP16Fixed | 0x00803000u,

  // NEON floating point across instructions.
  NEONAcrossLanesFPFixed = NEONAcrossLanesFixed | 0x2000C000u,
  NEONAcrossLanesFPFMask = NEONAcrossLanesFMask | 0x2000C000u,
  NEONAcrossLanesFPMask  = NEONAcrossLanesMask  | 0x20800000u,

  NEON_FMAXV   = NEONAcrossLanesFPFixed | 0x2000F000u,
  NEON_FMINV   = NEONAcrossLanesFPFixed | 0x2080F000u,
  NEON_FMAXNMV = NEONAcrossLanesFPFixed | 0x2000C000u,
  NEON_FMINNMV = NEONAcrossLanesFPFixed | 0x2080C000u
};

// NEON instructions with indexed element operand.
enum NEONByIndexedElementOp : uint32_t {
  NEONByIndexedElementFixed = 0x0F000000u,
  NEONByIndexedElementFMask = 0x9F000400u,
  NEONByIndexedElementMask  = 0xBF00F400u,
  NEON_MUL_byelement   = NEONByIndexedElementFixed | 0x00008000u,
  NEON_MLA_byelement   = NEONByIndexedElementFixed | 0x20000000u,
  NEON_MLS_byelement   = NEONByIndexedElementFixed | 0x20004000u,
  NEON_SMULL_byelement = NEONByIndexedElementFixed | 0x0000A000u,
  NEON_SMLAL_byelement = NEONByIndexedElementFixed | 0x00002000u,
  NEON_SMLSL_byelement = NEONByIndexedElementFixed | 0x00006000u,
  NEON_UMULL_byelement = NEONByIndexedElementFixed | 0x2000A000u,
  NEON_UMLAL_byelement = NEONByIndexedElementFixed | 0x20002000u,
  NEON_UMLSL_byelement = NEONByIndexedElementFixed | 0x20006000u,
  NEON_SQDMULL_byelement = NEONByIndexedElementFixed | 0x0000B000u,
  NEON_SQDMLAL_byelement = NEONByIndexedElementFixed | 0x00003000u,
  NEON_SQDMLSL_byelement = NEONByIndexedElementFixed | 0x00007000u,
  NEON_SQDMULH_byelement  = NEONByIndexedElementFixed | 0x0000C000u,
  NEON_SQRDMULH_byelement = NEONByIndexedElementFixed | 0x0000D000u,
  NEON_SDOT_byelement = NEONByIndexedElementFixed | 0x0000E000u,
  NEON_SQRDMLAH_byelement = NEONByIndexedElementFixed | 0x2000D000u,
  NEON_UDOT_byelement = NEONByIndexedElementFixed | 0x2000E000u,
  NEON_SQRDMLSH_byelement = NEONByIndexedElementFixed | 0x2000F000u,

  NEON_FMLA_H_byelement   = NEONByIndexedElementFixed | 0x00001000u,
  NEON_FMLS_H_byelement   = NEONByIndexedElementFixed | 0x00005000u,
  NEON_FMUL_H_byelement   = NEONByIndexedElementFixed | 0x00009000u,
  NEON_FMULX_H_byelement  = NEONByIndexedElementFixed | 0x20009000u,

  // Floating point instructions.
  NEONByIndexedElementFPFixed = NEONByIndexedElementFixed | 0x00800000u,
  NEONByIndexedElementFPMask = NEONByIndexedElementMask | 0x00800000u,
  NEON_FMLA_byelement  = NEONByIndexedElementFPFixed | 0x00001000u,
  NEON_FMLS_byelement  = NEONByIndexedElementFPFixed | 0x00005000u,
  NEON_FMUL_byelement  = NEONByIndexedElementFPFixed | 0x00009000u,
  NEON_FMULX_byelement = NEONByIndexedElementFPFixed | 0x20009000u,

  // FMLAL-like instructions.
  // For all cases: U = x, size = 10, opcode = xx00
  NEONByIndexedElementFPLongFixed = NEONByIndexedElementFixed | 0x00800000u,
  NEONByIndexedElementFPLongFMask = NEONByIndexedElementFMask | 0x00C03000u,
  NEONByIndexedElementFPLongMask = 0xBFC0F400u,
  NEON_FMLAL_H_byelement  = NEONByIndexedElementFixed | 0x00800000u,
  NEON_FMLAL2_H_byelement = NEONByIndexedElementFixed | 0x20808000u,
  NEON_FMLSL_H_byelement  = NEONByIndexedElementFixed | 0x00804000u,
  NEON_FMLSL2_H_byelement = NEONByIndexedElementFixed | 0x2080C000u,

  // Complex instruction(s).
  // This is necessary because the 'rot' encoding moves into the
  // NEONByIndex..Mask space.
  NEONByIndexedElementFPComplexMask = 0xBF009400u,
  NEON_FCMLA_byelement = NEONByIndexedElementFixed | 0x20001000u
};

// NEON register copy.
enum NEONCopyOp : uint32_t {
  NEONCopyFixed = 0x0E000400u,
  NEONCopyFMask = 0x9FE08400u,
  NEONCopyMask  = 0x3FE08400u,
  NEONCopyInsElementMask = NEONCopyMask | 0x40000000u,
  NEONCopyInsGeneralMask = NEONCopyMask | 0x40007800u,
  NEONCopyDupElementMask = NEONCopyMask | 0x20007800u,
  NEONCopyDupGeneralMask = NEONCopyDupElementMask,
  NEONCopyUmovMask       = NEONCopyMask | 0x20007800u,
  NEONCopySmovMask       = NEONCopyMask | 0x20007800u,
  NEON_INS_ELEMENT       = NEONCopyFixed | 0x60000000u,
  NEON_INS_GENERAL       = NEONCopyFixed | 0x40001800u,
  NEON_DUP_ELEMENT       = NEONCopyFixed | 0x00000000u,
  NEON_DUP_GENERAL       = NEONCopyFixed | 0x00000800u,
  NEON_SMOV              = NEONCopyFixed | 0x00002800u,
  NEON_UMOV              = NEONCopyFixed | 0x00003800u
};

// NEON extract.
enum NEONExtractOp : uint32_t {
  NEONExtractFixed = 0x2E000000u,
  NEONExtractFMask = 0xBF208400u,
  NEONExtractMask =  0xBFE08400u,
  NEON_EXT = NEONExtractFixed | 0x00000000u
};

enum NEONLoadStoreMultiOp : uint32_t {
  NEONLoadStoreMultiL    = 0x00400000u,
  NEONLoadStoreMulti1_1v = 0x00007000u,
  NEONLoadStoreMulti1_2v = 0x0000A000u,
  NEONLoadStoreMulti1_3v = 0x00006000u,
  NEONLoadStoreMulti1_4v = 0x00002000u,
  NEONLoadStoreMulti2    = 0x00008000u,
  NEONLoadStoreMulti3    = 0x00004000u,
  NEONLoadStoreMulti4    = 0x00000000u
};

// NEON load/store multiple structures.
enum NEONLoadStoreMultiStructOp : uint32_t {
  NEONLoadStoreMultiStructFixed = 0x0C000000u,
  NEONLoadStoreMultiStructFMask = 0xBFBF0000u,
  NEONLoadStoreMultiStructMask  = 0xBFFFF000u,
  NEONLoadStoreMultiStructStore = NEONLoadStoreMultiStructFixed,
  NEONLoadStoreMultiStructLoad  = NEONLoadStoreMultiStructFixed |
                                  NEONLoadStoreMultiL,
  NEON_LD1_1v = NEONLoadStoreMultiStructLoad | NEONLoadStoreMulti1_1v,
  NEON_LD1_2v = NEONLoadStoreMultiStructLoad | NEONLoadStoreMulti1_2v,
  NEON_LD1_3v = NEONLoadStoreMultiStructLoad | NEONLoadStoreMulti1_3v,
  NEON_LD1_4v = NEONLoadStoreMultiStructLoad | NEONLoadStoreMulti1_4v,
  NEON_LD2    = NEONLoadStoreMultiStructLoad | NEONLoadStoreMulti2,
  NEON_LD3    = NEONLoadStoreMultiStructLoad | NEONLoadStoreMulti3,
  NEON_LD4    = NEONLoadStoreMultiStructLoad | NEONLoadStoreMulti4,
  NEON_ST1_1v = NEONLoadStoreMultiStructStore | NEONLoadStoreMulti1_1v,
  NEON_ST1_2v = NEONLoadStoreMultiStructStore | NEONLoadStoreMulti1_2v,
  NEON_ST1_3v = NEONLoadStoreMultiStructStore | NEONLoadStoreMulti1_3v,
  NEON_ST1_4v = NEONLoadStoreMultiStructStore | NEONLoadStoreMulti1_4v,
  NEON_ST2    = NEONLoadStoreMultiStructStore | NEONLoadStoreMulti2,
  NEON_ST3    = NEONLoadStoreMultiStructStore | NEONLoadStoreMulti3,
  NEON_ST4    = NEONLoadStoreMultiStructStore | NEONLoadStoreMulti4
};

// NEON load/store multiple structures with post-index addressing.
enum NEONLoadStoreMultiStructPostIndexOp : uint32_t {
  NEONLoadStoreMultiStructPostIndexFixed = 0x0C800000u,
  NEONLoadStoreMultiStructPostIndexFMask = 0xBFA00000u,
  NEONLoadStoreMultiStructPostIndexMask  = 0xBFE0F000u,
  NEONLoadStoreMultiStructPostIndex = 0x00800000u,
  NEON_LD1_1v_post = NEON_LD1_1v | NEONLoadStoreMultiStructPostIndex,
  NEON_LD1_2v_post = NEON_LD1_2v | NEONLoadStoreMultiStructPostIndex,
  NEON_LD1_3v_post = NEON_LD1_3v | NEONLoadStoreMultiStructPostIndex,
  NEON_LD1_4v_post = NEON_LD1_4v | NEONLoadStoreMultiStructPostIndex,
  NEON_LD2_post = NEON_LD2 | NEONLoadStoreMultiStructPostIndex,
  NEON_LD3_post = NEON_LD3 | NEONLoadStoreMultiStructPostIndex,
  NEON_LD4_post = NEON_LD4 | NEONLoadStoreMultiStructPostIndex,
  NEON_ST1_1v_post = NEON_ST1_1v | NEONLoadStoreMultiStructPostIndex,
  NEON_ST1_2v_post = NEON_ST1_2v | NEONLoadStoreMultiStructPostIndex,
  NEON_ST1_3v_post = NEON_ST1_3v | NEONLoadStoreMultiStructPostIndex,
  NEON_ST1_4v_post = NEON_ST1_4v | NEONLoadStoreMultiStructPostIndex,
  NEON_ST2_post = NEON_ST2 | NEONLoadStoreMultiStructPostIndex,
  NEON_ST3_post = NEON_ST3 | NEONLoadStoreMultiStructPostIndex,
  NEON_ST4_post = NEON_ST4 | NEONLoadStoreMultiStructPostIndex
};

enum NEONLoadStoreSingleOp : uint32_t {
  NEONLoadStoreSingle1        = 0x00000000u,
  NEONLoadStoreSingle2        = 0x00200000u,
  NEONLoadStoreSingle3        = 0x00002000u,
  NEONLoadStoreSingle4        = 0x00202000u,
  NEONLoadStoreSingleL        = 0x00400000u,
  NEONLoadStoreSingle_b       = 0x00000000u,
  NEONLoadStoreSingle_h       = 0x00004000u,
  NEONLoadStoreSingle_s       = 0x00008000u,
  NEONLoadStoreSingle_d       = 0x00008400u,
  NEONLoadStoreSingleAllLanes = 0x0000C000u,
  NEONLoadStoreSingleLenMask  = 0x00202000u
};

// NEON load/store single structure.
enum NEONLoadStoreSingleStructOp : uint32_t {
  NEONLoadStoreSingleStructFixed = 0x0D000000u,
  NEONLoadStoreSingleStructFMask = 0xBF9F0000u,
  NEONLoadStoreSingleStructMask  = 0xBFFFE000u,
  NEONLoadStoreSingleStructStore = NEONLoadStoreSingleStructFixed,
  NEONLoadStoreSingleStructLoad  = NEONLoadStoreSingleStructFixed |
                                   NEONLoadStoreSingleL,
  NEONLoadStoreSingleStructLoad1 = NEONLoadStoreSingle1 |
                                   NEONLoadStoreSingleStructLoad,
  NEONLoadStoreSingleStructLoad2 = NEONLoadStoreSingle2 |
                                   NEONLoadStoreSingleStructLoad,
  NEONLoadStoreSingleStructLoad3 = NEONLoadStoreSingle3 |
                                   NEONLoadStoreSingleStructLoad,
  NEONLoadStoreSingleStructLoad4 = NEONLoadStoreSingle4 |
                                   NEONLoadStoreSingleStructLoad,
  NEONLoadStoreSingleStructStore1 = NEONLoadStoreSingle1 |
                                    NEONLoadStoreSingleStructFixed,
  NEONLoadStoreSingleStructStore2 = NEONLoadStoreSingle2 |
                                    NEONLoadStoreSingleStructFixed,
  NEONLoadStoreSingleStructStore3 = NEONLoadStoreSingle3 |
                                    NEONLoadStoreSingleStructFixed,
  NEONLoadStoreSingleStructStore4 = NEONLoadStoreSingle4 |
                                    NEONLoadStoreSingleStructFixed,
  NEON_LD1_b = NEONLoadStoreSingleStructLoad1 | NEONLoadStoreSingle_b,
  NEON_LD1_h = NEONLoadStoreSingleStructLoad1 | NEONLoadStoreSingle_h,
  NEON_LD1_s = NEONLoadStoreSingleStructLoad1 | NEONLoadStoreSingle_s,
  NEON_LD1_d = NEONLoadStoreSingleStructLoad1 | NEONLoadStoreSingle_d,
  NEON_LD1R  = NEONLoadStoreSingleStructLoad1 | NEONLoadStoreSingleAllLanes,
  NEON_ST1_b = NEONLoadStoreSingleStructStore1 | NEONLoadStoreSingle_b,
  NEON_ST1_h = NEONLoadStoreSingleStructStore1 | NEONLoadStoreSingle_h,
  NEON_ST1_s = NEONLoadStoreSingleStructStore1 | NEONLoadStoreSingle_s,
  NEON_ST1_d = NEONLoadStoreSingleStructStore1 | NEONLoadStoreSingle_d,

  NEON_LD2_b = NEONLoadStoreSingleStructLoad2 | NEONLoadStoreSingle_b,
  NEON_LD2_h = NEONLoadStoreSingleStructLoad2 | NEONLoadStoreSingle_h,
  NEON_LD2_s = NEONLoadStoreSingleStructLoad2 | NEONLoadStoreSingle_s,
  NEON_LD2_d = NEONLoadStoreSingleStructLoad2 | NEONLoadStoreSingle_d,
  NEON_LD2R  = NEONLoadStoreSingleStructLoad2 | NEONLoadStoreSingleAllLanes,
  NEON_ST2_b = NEONLoadStoreSingleStructStore2 | NEONLoadStoreSingle_b,
  NEON_ST2_h = NEONLoadStoreSingleStructStore2 | NEONLoadStoreSingle_h,
  NEON_ST2_s = NEONLoadStoreSingleStructStore2 | NEONLoadStoreSingle_s,
  NEON_ST2_d = NEONLoadStoreSingleStructStore2 | NEONLoadStoreSingle_d,

  NEON_LD3_b = NEONLoadStoreSingleStructLoad3 | NEONLoadStoreSingle_b,
  NEON_LD3_h = NEONLoadStoreSingleStructLoad3 | NEONLoadStoreSingle_h,
  NEON_LD3_s = NEONLoadStoreSingleStructLoad3 | NEONLoadStoreSingle_s,
  NEON_LD3_d = NEONLoadStoreSingleStructLoad3 | NEONLoadStoreSingle_d,
  NEON_LD3R  = NEONLoadStoreSingleStructLoad3 | NEONLoadStoreSingleAllLanes,
  NEON_ST3_b = NEONLoadStoreSingleStructStore3 | NEONLoadStoreSingle_b,
  NEON_ST3_h = NEONLoadStoreSingleStructStore3 | NEONLoadStoreSingle_h,
  NEON_ST3_s = NEONLoadStoreSingleStructStore3 | NEONLoadStoreSingle_s,
  NEON_ST3_d = NEONLoadStoreSingleStructStore3 | NEONLoadStoreSingle_d,

  NEON_LD4_b = NEONLoadStoreSingleStructLoad4 | NEONLoadStoreSingle_b,
  NEON_LD4_h = NEONLoadStoreSingleStructLoad4 | NEONLoadStoreSingle_h,
  NEON_LD4_s = NEONLoadStoreSingleStructLoad4 | NEONLoadStoreSingle_s,
  NEON_LD4_d = NEONLoadStoreSingleStructLoad4 | NEONLoadStoreSingle_d,
  NEON_LD4R  = NEONLoadStoreSingleStructLoad4 | NEONLoadStoreSingleAllLanes,
  NEON_ST4_b = NEONLoadStoreSingleStructStore4 | NEONLoadStoreSingle_b,
  NEON_ST4_h = NEONLoadStoreSingleStructStore4 | NEONLoadStoreSingle_h,
  NEON_ST4_s = NEONLoadStoreSingleStructStore4 | NEONLoadStoreSingle_s,
  NEON_ST4_d = NEONLoadStoreSingleStructStore4 | NEONLoadStoreSingle_d
};

// NEON load/store single structure with post-index addressing.
enum NEONLoadStoreSingleStructPostIndexOp : uint32_t {
  NEONLoadStoreSingleStructPostIndexFixed = 0x0D800000u,
  NEONLoadStoreSingleStructPostIndexFMask = 0xBF800000u,
  NEONLoadStoreSingleStructPostIndexMask  = 0xBFE0E000u,
  NEONLoadStoreSingleStructPostIndex =      0x00800000u,
  NEON_LD1_b_post = NEON_LD1_b | NEONLoadStoreSingleStructPostIndex,
  NEON_LD1_h_post = NEON_LD1_h | NEONLoadStoreSingleStructPostIndex,
  NEON_LD1_s_post = NEON_LD1_s | NEONLoadStoreSingleStructPostIndex,
  NEON_LD1_d_post = NEON_LD1_d | NEONLoadStoreSingleStructPostIndex,
  NEON_LD1R_post  = NEON_LD1R | NEONLoadStoreSingleStructPostIndex,
  NEON_ST1_b_post = NEON_ST1_b | NEONLoadStoreSingleStructPostIndex,
  NEON_ST1_h_post = NEON_ST1_h | NEONLoadStoreSingleStructPostIndex,
  NEON_ST1_s_post = NEON_ST1_s | NEONLoadStoreSingleStructPostIndex,
  NEON_ST1_d_post = NEON_ST1_d | NEONLoadStoreSingleStructPostIndex,

  NEON_LD2_b_post = NEON_LD2_b | NEONLoadStoreSingleStructPostIndex,
  NEON_LD2_h_post = NEON_LD2_h | NEONLoadStoreSingleStructPostIndex,
  NEON_LD2_s_post = NEON_LD2_s | NEONLoadStoreSingleStructPostIndex,
  NEON_LD2_d_post = NEON_LD2_d | NEONLoadStoreSingleStructPostIndex,
  NEON_LD2R_post  = NEON_LD2R | NEONLoadStoreSingleStructPostIndex,
  NEON_ST2_b_post = NEON_ST2_b | NEONLoadStoreSingleStructPostIndex,
  NEON_ST2_h_post = NEON_ST2_h | NEONLoadStoreSingleStructPostIndex,
  NEON_ST2_s_post = NEON_ST2_s | NEONLoadStoreSingleStructPostIndex,
  NEON_ST2_d_post = NEON_ST2_d | NEONLoadStoreSingleStructPostIndex,

  NEON_LD3_b_post = NEON_LD3_b | NEONLoadStoreSingleStructPostIndex,
  NEON_LD3_h_post = NEON_LD3_h | NEONLoadStoreSingleStructPostIndex,
  NEON_LD3_s_post = NEON_LD3_s | NEONLoadStoreSingleStructPostIndex,
  NEON_LD3_d_post = NEON_LD3_d | NEONLoadStoreSingleStructPostIndex,
  NEON_LD3R_post  = NEON_LD3R | NEONLoadStoreSingleStructPostIndex,
  NEON_ST3_b_post = NEON_ST3_b | NEONLoadStoreSingleStructPostIndex,
  NEON_ST3_h_post = NEON_ST3_h | NEONLoadStoreSingleStructPostIndex,
  NEON_ST3_s_post = NEON_ST3_s | NEONLoadStoreSingleStructPostIndex,
  NEON_ST3_d_post = NEON_ST3_d | NEONLoadStoreSingleStructPostIndex,

  NEON_LD4_b_post = NEON_LD4_b | NEONLoadStoreSingleStructPostIndex,
  NEON_LD4_h_post = NEON_LD4_h | NEONLoadStoreSingleStructPostIndex,
  NEON_LD4_s_post = NEON_LD4_s | NEONLoadStoreSingleStructPostIndex,
  NEON_LD4_d_post = NEON_LD4_d | NEONLoadStoreSingleStructPostIndex,
  NEON_LD4R_post  = NEON_LD4R | NEONLoadStoreSingleStructPostIndex,
  NEON_ST4_b_post = NEON_ST4_b | NEONLoadStoreSingleStructPostIndex,
  NEON_ST4_h_post = NEON_ST4_h | NEONLoadStoreSingleStructPostIndex,
  NEON_ST4_s_post = NEON_ST4_s | NEONLoadStoreSingleStructPostIndex,
  NEON_ST4_d_post = NEON_ST4_d | NEONLoadStoreSingleStructPostIndex
};

// NEON modified immediate.
enum NEONModifiedImmediateOp : uint32_t {
  NEONModifiedImmediateFixed = 0x0F000400u,
  NEONModifiedImmediateFMask = 0x9FF80400u,
  NEONModifiedImmediateOpBit = 0x20000000u,
  NEONModifiedImmediate_FMOV = NEONModifiedImmediateFixed | 0x00000800u,
  NEONModifiedImmediate_MOVI = NEONModifiedImmediateFixed | 0x00000000u,
  NEONModifiedImmediate_MVNI = NEONModifiedImmediateFixed | 0x20000000u,
  NEONModifiedImmediate_ORR  = NEONModifiedImmediateFixed | 0x00001000u,
  NEONModifiedImmediate_BIC  = NEONModifiedImmediateFixed | 0x20001000u
};

// NEON shift immediate.
enum NEONShiftImmediateOp : uint32_t {
  NEONShiftImmediateFixed = 0x0F000400u,
  NEONShiftImmediateFMask = 0x9F800400u,
  NEONShiftImmediateMask  = 0xBF80FC00u,
  NEONShiftImmediateUBit  = 0x20000000u,
  NEON_SHL      = NEONShiftImmediateFixed | 0x00005000u,
  NEON_SSHLL    = NEONShiftImmediateFixed | 0x0000A000u,
  NEON_USHLL    = NEONShiftImmediateFixed | 0x2000A000u,
  NEON_SLI      = NEONShiftImmediateFixed | 0x20005000u,
  NEON_SRI      = NEONShiftImmediateFixed | 0x20004000u,
  NEON_SHRN     = NEONShiftImmediateFixed | 0x00008000u,
  NEON_RSHRN    = NEONShiftImmediateFixed | 0x00008800u,
  NEON_UQSHRN   = NEONShiftImmediateFixed | 0x20009000u,
  NEON_UQRSHRN  = NEONShiftImmediateFixed | 0x20009800u,
  NEON_SQSHRN   = NEONShiftImmediateFixed | 0x00009000u,
  NEON_SQRSHRN  = NEONShiftImmediateFixed | 0x00009800u,
  NEON_SQSHRUN  = NEONShiftImmediateFixed | 0x20008000u,
  NEON_SQRSHRUN = NEONShiftImmediateFixed | 0x20008800u,
  NEON_SSHR     = NEONShiftImmediateFixed | 0x00000000u,
  NEON_SRSHR    = NEONShiftImmediateFixed | 0x00002000u,
  NEON_USHR     = NEONShiftImmediateFixed | 0x20000000u,
  NEON_URSHR    = NEONShiftImmediateFixed | 0x20002000u,
  NEON_SSRA     = NEONShiftImmediateFixed | 0x00001000u,
  NEON_SRSRA    = NEONShiftImmediateFixed | 0x00003000u,
  NEON_USRA     = NEONShiftImmediateFixed | 0x20001000u,
  NEON_URSRA    = NEONShiftImmediateFixed | 0x20003000u,
  NEON_SQSHLU   = NEONShiftImmediateFixed | 0x20006000u,
  NEON_SCVTF_imm = NEONShiftImmediateFixed | 0x0000E000u,
  NEON_UCVTF_imm = NEONShiftImmediateFixed | 0x2000E000u,
  NEON_FCVTZS_imm = NEONShiftImmediateFixed | 0x0000F800u,
  NEON_FCVTZU_imm = NEONShiftImmediateFixed | 0x2000F800u,
  NEON_SQSHL_imm = NEONShiftImmediateFixed | 0x00007000u,
  NEON_UQSHL_imm = NEONShiftImmediateFixed | 0x20007000u
};

// NEON table.
enum NEONTableOp : uint32_t {
  NEONTableFixed = 0x0E000000u,
  NEONTableFMask = 0xBF208C00u,
  NEONTableExt   = 0x00001000u,
  NEONTableMask  = 0xBF20FC00u,
  NEON_TBL_1v    = NEONTableFixed | 0x00000000u,
  NEON_TBL_2v    = NEONTableFixed | 0x00002000u,
  NEON_TBL_3v    = NEONTableFixed | 0x00004000u,
  NEON_TBL_4v    = NEONTableFixed | 0x00006000u,
  NEON_TBX_1v    = NEON_TBL_1v | NEONTableExt,
  NEON_TBX_2v    = NEON_TBL_2v | NEONTableExt,
  NEON_TBX_3v    = NEON_TBL_3v | NEONTableExt,
  NEON_TBX_4v    = NEON_TBL_4v | NEONTableExt
};

// NEON perm.
enum NEONPermOp : uint32_t {
  NEONPermFixed = 0x0E000800u,
  NEONPermFMask = 0xBF208C00u,
  NEONPermMask  = 0x3F20FC00u,
  NEON_UZP1 = NEONPermFixed | 0x00001000u,
  NEON_TRN1 = NEONPermFixed | 0x00002000u,
  NEON_ZIP1 = NEONPermFixed | 0x00003000u,
  NEON_UZP2 = NEONPermFixed | 0x00005000u,
  NEON_TRN2 = NEONPermFixed | 0x00006000u,
  NEON_ZIP2 = NEONPermFixed | 0x00007000u
};

// NEON scalar instructions with two register operands.
enum NEONScalar2RegMiscOp : uint32_t {
  NEONScalar2RegMiscFixed = 0x5E200800u,
  NEONScalar2RegMiscFMask = 0xDF3E0C00u,
  NEONScalar2RegMiscMask = NEON_Q | NEONScalar | NEON2RegMiscMask,
  NEON_CMGT_zero_scalar = NEON_Q | NEONScalar | NEON_CMGT_zero,
  NEON_CMEQ_zero_scalar = NEON_Q | NEONScalar | NEON_CMEQ_zero,
  NEON_CMLT_zero_scalar = NEON_Q | NEONScalar | NEON_CMLT_zero,
  NEON_CMGE_zero_scalar = NEON_Q | NEONScalar | NEON_CMGE_zero,
  NEON_CMLE_zero_scalar = NEON_Q | NEONScalar | NEON_CMLE_zero,
  NEON_ABS_scalar       = NEON_Q | NEONScalar | NEON_ABS,
  NEON_SQABS_scalar     = NEON_Q | NEONScalar | NEON_SQABS,
  NEON_NEG_scalar       = NEON_Q | NEONScalar | NEON_NEG,
  NEON_SQNEG_scalar     = NEON_Q | NEONScalar | NEON_SQNEG,
  NEON_SQXTN_scalar     = NEON_Q | NEONScalar | NEON_SQXTN,
  NEON_UQXTN_scalar     = NEON_Q | NEONScalar | NEON_UQXTN,
  NEON_SQXTUN_scalar    = NEON_Q | NEONScalar | NEON_SQXTUN,
  NEON_SUQADD_scalar    = NEON_Q | NEONScalar | NEON_SUQADD,
  NEON_USQADD_scalar    = NEON_Q | NEONScalar | NEON_USQADD,

  NEONScalar2RegMiscOpcode = NEON2RegMiscOpcode,
  NEON_NEG_scalar_opcode = NEON_NEG_scalar & NEONScalar2RegMiscOpcode,

  NEONScalar2RegMiscFPMask  = NEONScalar2RegMiscMask | 0x00800000u,
  NEON_FRSQRTE_scalar    = NEON_Q | NEONScalar | NEON_FRSQRTE,
  NEON_FRECPE_scalar     = NEON_Q | NEONScalar | NEON_FRECPE,
  NEON_SCVTF_scalar      = NEON_Q | NEONScalar | NEON_SCVTF,
  NEON_UCVTF_scalar      = NEON_Q | NEONScalar | NEON_UCVTF,
  NEON_FCMGT_zero_scalar = NEON_Q | NEONScalar | NEON_FCMGT_zero,
  NEON_FCMEQ_zero_scalar = NEON_Q | NEONScalar | NEON_FCMEQ_zero,
  NEON_FCMLT_zero_scalar = NEON_Q | NEONScalar | NEON_FCMLT_zero,
  NEON_FCMGE_zero_scalar = NEON_Q | NEONScalar | NEON_FCMGE_zero,
  NEON_FCMLE_zero_scalar = NEON_Q | NEONScalar | NEON_FCMLE_zero,
  NEON_FRECPX_scalar     = NEONScalar2RegMiscFixed | 0x0081F000u,
  NEON_FCVTNS_scalar     = NEON_Q | NEONScalar | NEON_FCVTNS,
  NEON_FCVTNU_scalar     = NEON_Q | NEONScalar | NEON_FCVTNU,
  NEON_FCVTPS_scalar     = NEON_Q | NEONScalar | NEON_FCVTPS,
  NEON_FCVTPU_scalar     = NEON_Q | NEONScalar | NEON_FCVTPU,
  NEON_FCVTMS_scalar     = NEON_Q | NEONScalar | NEON_FCVTMS,
  NEON_FCVTMU_scalar     = NEON_Q | NEONScalar | NEON_FCVTMU,
  NEON_FCVTZS_scalar     = NEON_Q | NEONScalar | NEON_FCVTZS,
  NEON_FCVTZU_scalar     = NEON_Q | NEONScalar | NEON_FCVTZU,
  NEON_FCVTAS_scalar     = NEON_Q | NEONScalar | NEON_FCVTAS,
  NEON_FCVTAU_scalar     = NEON_Q | NEONScalar | NEON_FCVTAU,
  NEON_FCVTXN_scalar     = NEON_Q | NEONScalar | NEON_FCVTXN
};

// NEON instructions with two register operands (FP16).
enum NEONScalar2RegMiscFP16Op : uint32_t {
  NEONScalar2RegMiscFP16Fixed = 0x5E780800u,
  NEONScalar2RegMiscFP16FMask = 0xDF7E0C00u,
  NEONScalar2RegMiscFP16Mask  = 0xFFFFFC00u,
  NEON_FCVTNS_H_scalar     = NEON_Q | NEONScalar | NEON_FCVTNS_H,
  NEON_FCVTMS_H_scalar     = NEON_Q | NEONScalar | NEON_FCVTMS_H,
  NEON_FCVTAS_H_scalar     = NEON_Q | NEONScalar | NEON_FCVTAS_H,
  NEON_SCVTF_H_scalar      = NEON_Q | NEONScalar | NEON_SCVTF_H,
  NEON_FCMGT_H_zero_scalar = NEON_Q | NEONScalar | NEON_FCMGT_H_zero,
  NEON_FCMEQ_H_zero_scalar = NEON_Q | NEONScalar | NEON_FCMEQ_H_zero,
  NEON_FCMLT_H_zero_scalar = NEON_Q | NEONScalar | NEON_FCMLT_H_zero,
  NEON_FCVTPS_H_scalar     = NEON_Q | NEONScalar | NEON_FCVTPS_H,
  NEON_FCVTZS_H_scalar     = NEON_Q | NEONScalar | NEON_FCVTZS_H,
  NEON_FRECPE_H_scalar     = NEON_Q | NEONScalar | NEON_FRECPE_H,
  NEON_FRECPX_H_scalar     = NEONScalar2RegMiscFP16Fixed | 0x0081F000u,
  NEON_FCVTNU_H_scalar     = NEON_Q | NEONScalar | NEON_FCVTNU_H,
  NEON_FCVTMU_H_scalar     = NEON_Q | NEONScalar | NEON_FCVTMU_H,
  NEON_FCVTAU_H_scalar     = NEON_Q | NEONScalar | NEON_FCVTAU_H,
  NEON_UCVTF_H_scalar      = NEON_Q | NEONScalar | NEON_UCVTF_H,
  NEON_FCMGE_H_zero_scalar = NEON_Q | NEONScalar | NEON_FCMGE_H_zero,
  NEON_FCMLE_H_zero_scalar = NEON_Q | NEONScalar | NEON_FCMLE_H_zero,
  NEON_FCVTPU_H_scalar     = NEON_Q | NEONScalar | NEON_FCVTPU_H,
  NEON_FCVTZU_H_scalar     = NEON_Q | NEONScalar | NEON_FCVTZU_H,
  NEON_FRSQRTE_H_scalar    = NEON_Q | NEONScalar | NEON_FRSQRTE_H
};

// NEON scalar instructions with three same-type operands.
enum NEONScalar3SameOp : uint32_t {
  NEONScalar3SameFixed = 0x5E200400u,
  NEONScalar3SameFMask = 0xDF200400u,
  NEONScalar3SameMask  = 0xFF20FC00u,
  NEON_ADD_scalar    = NEON_Q | NEONScalar | NEON_ADD,
  NEON_CMEQ_scalar   = NEON_Q | NEONScalar | NEON_CMEQ,
  NEON_CMGE_scalar   = NEON_Q | NEONScalar | NEON_CMGE,
  NEON_CMGT_scalar   = NEON_Q | NEONScalar | NEON_CMGT,
  NEON_CMHI_scalar   = NEON_Q | NEONScalar | NEON_CMHI,
  NEON_CMHS_scalar   = NEON_Q | NEONScalar | NEON_CMHS,
  NEON_CMTST_scalar  = NEON_Q | NEONScalar | NEON_CMTST,
  NEON_SUB_scalar    = NEON_Q | NEONScalar | NEON_SUB,
  NEON_UQADD_scalar  = NEON_Q | NEONScalar | NEON_UQADD,
  NEON_SQADD_scalar  = NEON_Q | NEONScalar | NEON_SQADD,
  NEON_UQSUB_scalar  = NEON_Q | NEONScalar | NEON_UQSUB,
  NEON_SQSUB_scalar  = NEON_Q | NEONScalar | NEON_SQSUB,
  NEON_USHL_scalar   = NEON_Q | NEONScalar | NEON_USHL,
  NEON_SSHL_scalar   = NEON_Q | NEONScalar | NEON_SSHL,
  NEON_UQSHL_scalar  = NEON_Q | NEONScalar | NEON_UQSHL,
  NEON_SQSHL_scalar  = NEON_Q | NEONScalar | NEON_SQSHL,
  NEON_URSHL_scalar  = NEON_Q | NEONScalar | NEON_URSHL,
  NEON_SRSHL_scalar  = NEON_Q | NEONScalar | NEON_SRSHL,
  NEON_UQRSHL_scalar = NEON_Q | NEONScalar | NEON_UQRSHL,
  NEON_SQRSHL_scalar = NEON_Q | NEONScalar | NEON_SQRSHL,
  NEON_SQDMULH_scalar = NEON_Q | NEONScalar | NEON_SQDMULH,
  NEON_SQRDMULH_scalar = NEON_Q | NEONScalar | NEON_SQRDMULH,

  // NEON floating point scalar instructions with three same-type operands.
  NEONScalar3SameFPFixed = NEONScalar3SameFixed | 0x0000C000u,
  NEONScalar3SameFPFMask = NEONScalar3SameFMask | 0x0000C000u,
  NEONScalar3SameFPMask  = NEONScalar3SameMask | 0x00800000u,
  NEON_FACGE_scalar   = NEON_Q | NEONScalar | NEON_FACGE,
  NEON_FACGT_scalar   = NEON_Q | NEONScalar | NEON_FACGT,
  NEON_FCMEQ_scalar   = NEON_Q | NEONScalar | NEON_FCMEQ,
  NEON_FCMGE_scalar   = NEON_Q | NEONScalar | NEON_FCMGE,
  NEON_FCMGT_scalar   = NEON_Q | NEONScalar | NEON_FCMGT,
  NEON_FMULX_scalar   = NEON_Q | NEONScalar | NEON_FMULX,
  NEON_FRECPS_scalar  = NEON_Q | NEONScalar | NEON_FRECPS,
  NEON_FRSQRTS_scalar = NEON_Q | NEONScalar | NEON_FRSQRTS,
  NEON_FABD_scalar    = NEON_Q | NEONScalar | NEON_FABD
};

// NEON scalar FP16 instructions with three same-type operands.
enum NEONScalar3SameFP16Op : uint32_t {
  NEONScalar3SameFP16Fixed = 0x5E400400u,
  NEONScalar3SameFP16FMask = 0xDF60C400u,
  NEONScalar3SameFP16Mask  = 0xFFE0FC00u,
  NEON_FABD_H_scalar    = NEON_Q | NEONScalar | NEON_FABD_H,
  NEON_FMULX_H_scalar   = NEON_Q | NEONScalar | NEON_FMULX_H,
  NEON_FCMEQ_H_scalar   = NEON_Q | NEONScalar | NEON_FCMEQ_H,
  NEON_FCMGE_H_scalar   = NEON_Q | NEONScalar | NEON_FCMGE_H,
  NEON_FCMGT_H_scalar   = NEON_Q | NEONScalar | NEON_FCMGT_H,
  NEON_FACGE_H_scalar   = NEON_Q | NEONScalar | NEON_FACGE_H,
  NEON_FACGT_H_scalar   = NEON_Q | NEONScalar | NEON_FACGT_H,
  NEON_FRECPS_H_scalar  = NEON_Q | NEONScalar | NEON_FRECPS_H,
  NEON_FRSQRTS_H_scalar = NEON_Q | NEONScalar | NEON_FRSQRTS_H
};

// 'Extra' NEON scalar instructions with three same-type operands.
enum NEONScalar3SameExtraOp : uint32_t {
  NEONScalar3SameExtraFixed = 0x5E008400u,
  NEONScalar3SameExtraFMask = 0xDF208400u,
  NEONScalar3SameExtraMask = 0xFF20FC00u,
  NEON_SQRDMLAH_scalar = NEON_Q | NEONScalar | NEON_SQRDMLAH,
  NEON_SQRDMLSH_scalar = NEON_Q | NEONScalar | NEON_SQRDMLSH
};

// NEON scalar instructions with three different-type operands.
enum NEONScalar3DiffOp : uint32_t {
  NEONScalar3DiffFixed = 0x5E200000u,
  NEONScalar3DiffFMask = 0xDF200C00u,
  NEONScalar3DiffMask  = NEON_Q | NEONScalar | NEON3DifferentMask,
  NEON_SQDMLAL_scalar  = NEON_Q | NEONScalar | NEON_SQDMLAL,
  NEON_SQDMLSL_scalar  = NEON_Q | NEONScalar | NEON_SQDMLSL,
  NEON_SQDMULL_scalar  = NEON_Q | NEONScalar | NEON_SQDMULL
};

// NEON scalar instructions with indexed element operand.
enum NEONScalarByIndexedElementOp : uint32_t {
  NEONScalarByIndexedElementFixed = 0x5F000000u,
  NEONScalarByIndexedElementFMask = 0xDF000400u,
  NEONScalarByIndexedElementMask  = 0xFF00F400u,
  NEON_SQDMLAL_byelement_scalar  = NEON_Q | NEONScalar | NEON_SQDMLAL_byelement,
  NEON_SQDMLSL_byelement_scalar  = NEON_Q | NEONScalar | NEON_SQDMLSL_byelement,
  NEON_SQDMULL_byelement_scalar  = NEON_Q | NEONScalar | NEON_SQDMULL_byelement,
  NEON_SQDMULH_byelement_scalar  = NEON_Q | NEONScalar | NEON_SQDMULH_byelement,
  NEON_SQRDMULH_byelement_scalar
    = NEON_Q | NEONScalar | NEON_SQRDMULH_byelement,
  NEON_SQRDMLAH_byelement_scalar
    = NEON_Q | NEONScalar | NEON_SQRDMLAH_byelement,
  NEON_SQRDMLSH_byelement_scalar
    = NEON_Q | NEONScalar | NEON_SQRDMLSH_byelement,
  NEON_FMLA_H_byelement_scalar  = NEON_Q | NEONScalar | NEON_FMLA_H_byelement,
  NEON_FMLS_H_byelement_scalar  = NEON_Q | NEONScalar | NEON_FMLS_H_byelement,
  NEON_FMUL_H_byelement_scalar  = NEON_Q | NEONScalar | NEON_FMUL_H_byelement,
  NEON_FMULX_H_byelement_scalar = NEON_Q | NEONScalar | NEON_FMULX_H_byelement,

  // Floating point instructions.
  NEONScalarByIndexedElementFPFixed
    = NEONScalarByIndexedElementFixed | 0x00800000u,
  NEONScalarByIndexedElementFPMask
    = NEONScalarByIndexedElementMask | 0x00800000u,
  NEON_FMLA_byelement_scalar  = NEON_Q | NEONScalar | NEON_FMLA_byelement,
  NEON_FMLS_byelement_scalar  = NEON_Q | NEONScalar | NEON_FMLS_byelement,
  NEON_FMUL_byelement_scalar  = NEON_Q | NEONScalar | NEON_FMUL_byelement,
  NEON_FMULX_byelement_scalar = NEON_Q | NEONScalar | NEON_FMULX_byelement
};

// NEON scalar register copy.
enum NEONScalarCopyOp : uint32_t {
  NEONScalarCopyFixed = 0x5E000400u,
  NEONScalarCopyFMask = 0xDFE08400u,
  NEONScalarCopyMask  = 0xFFE0FC00u,
  NEON_DUP_ELEMENT_scalar = NEON_Q | NEONScalar | NEON_DUP_ELEMENT
};

// NEON scalar pairwise instructions.
enum NEONScalarPairwiseOp : uint32_t {
  NEONScalarPairwiseFixed = 0x5E300800u,
  NEONScalarPairwiseFMask = 0xDF3E0C00u,
  NEONScalarPairwiseMask  = 0xFFB1F800u,
  NEON_ADDP_scalar      = NEONScalarPairwiseFixed | 0x0081B000u,
  NEON_FMAXNMP_h_scalar = NEONScalarPairwiseFixed | 0x0000C000u,
  NEON_FADDP_h_scalar   = NEONScalarPairwiseFixed | 0x0000D000u,
  NEON_FMAXP_h_scalar   = NEONScalarPairwiseFixed | 0x0000F000u,
  NEON_FMINNMP_h_scalar = NEONScalarPairwiseFixed | 0x0080C000u,
  NEON_FMINP_h_scalar   = NEONScalarPairwiseFixed | 0x0080F000u,
  NEON_FMAXNMP_scalar   = NEONScalarPairwiseFixed | 0x2000C000u,
  NEON_FMINNMP_scalar   = NEONScalarPairwiseFixed | 0x2080C000u,
  NEON_FADDP_scalar     = NEONScalarPairwiseFixed | 0x2000D000u,
  NEON_FMAXP_scalar     = NEONScalarPairwiseFixed | 0x2000F000u,
  NEON_FMINP_scalar     = NEONScalarPairwiseFixed | 0x2080F000u
};

// NEON scalar shift immediate.
enum NEONScalarShiftImmediateOp : uint32_t {
  NEONScalarShiftImmediateFixed = 0x5F000400u,
  NEONScalarShiftImmediateFMask = 0xDF800400u,
  NEONScalarShiftImmediateMask  = 0xFF80FC00u,
  NEON_SHL_scalar  =       NEON_Q | NEONScalar | NEON_SHL,
  NEON_SLI_scalar  =       NEON_Q | NEONScalar | NEON_SLI,
  NEON_SRI_scalar  =       NEON_Q | NEONScalar | NEON_SRI,
  NEON_SSHR_scalar =       NEON_Q | NEONScalar | NEON_SSHR,
  NEON_USHR_scalar =       NEON_Q | NEONScalar | NEON_USHR,
  NEON_SRSHR_scalar =      NEON_Q | NEONScalar | NEON_SRSHR,
  NEON_URSHR_scalar =      NEON_Q | NEONScalar | NEON_URSHR,
  NEON_SSRA_scalar =       NEON_Q | NEONScalar | NEON_SSRA,
  NEON_USRA_scalar =       NEON_Q | NEONScalar | NEON_USRA,
  NEON_SRSRA_scalar =      NEON_Q | NEONScalar | NEON_SRSRA,
  NEON_URSRA_scalar =      NEON_Q | NEONScalar | NEON_URSRA,
  NEON_UQSHRN_scalar =     NEON_Q | NEONScalar | NEON_UQSHRN,
  NEON_UQRSHRN_scalar =    NEON_Q | NEONScalar | NEON_UQRSHRN,
  NEON_SQSHRN_scalar =     NEON_Q | NEONScalar | NEON_SQSHRN,
  NEON_SQRSHRN_scalar =    NEON_Q | NEONScalar | NEON_SQRSHRN,
  NEON_SQSHRUN_scalar =    NEON_Q | NEONScalar | NEON_SQSHRUN,
  NEON_SQRSHRUN_scalar =   NEON_Q | NEONScalar | NEON_SQRSHRUN,
  NEON_SQSHLU_scalar =     NEON_Q | NEONScalar | NEON_SQSHLU,
  NEON_SQSHL_imm_scalar  = NEON_Q | NEONScalar | NEON_SQSHL_imm,
  NEON_UQSHL_imm_scalar  = NEON_Q | NEONScalar | NEON_UQSHL_imm,
  NEON_SCVTF_imm_scalar =  NEON_Q | NEONScalar | NEON_SCVTF_imm,
  NEON_UCVTF_imm_scalar =  NEON_Q | NEONScalar | NEON_UCVTF_imm,
  NEON_FCVTZS_imm_scalar = NEON_Q | NEONScalar | NEON_FCVTZS_imm,
  NEON_FCVTZU_imm_scalar = NEON_Q | NEONScalar | NEON_FCVTZU_imm
};

enum SVE32BitGatherLoadHalfwords_ScalarPlus32BitScaledOffsetsOp : uint32_t {
  SVE32BitGatherLoadHalfwords_ScalarPlus32BitScaledOffsetsFixed = 0x84A00000u,
  SVE32BitGatherLoadHalfwords_ScalarPlus32BitScaledOffsetsFMask = 0xFFA08000u,
  SVE32BitGatherLoadHalfwords_ScalarPlus32BitScaledOffsetsMask = 0xFFA0E000u,
  LD1SH_z_p_bz_s_x32_scaled = SVE32BitGatherLoadHalfwords_ScalarPlus32BitScaledOffsetsFixed,
  LDFF1SH_z_p_bz_s_x32_scaled = SVE32BitGatherLoadHalfwords_ScalarPlus32BitScaledOffsetsFixed | 0x00002000u,
  LD1H_z_p_bz_s_x32_scaled = SVE32BitGatherLoadHalfwords_ScalarPlus32BitScaledOffsetsFixed | 0x00004000u,
  LDFF1H_z_p_bz_s_x32_scaled = SVE32BitGatherLoadHalfwords_ScalarPlus32BitScaledOffsetsFixed | 0x00006000u
};

enum SVE32BitGatherLoadWords_ScalarPlus32BitScaledOffsetsOp : uint32_t {
  SVE32BitGatherLoadWords_ScalarPlus32BitScaledOffsetsFixed = 0x85200000u,
  SVE32BitGatherLoadWords_ScalarPlus32BitScaledOffsetsFMask = 0xFFA08000u,
  SVE32BitGatherLoadWords_ScalarPlus32BitScaledOffsetsMask = 0xFFA0E000u,
  LD1W_z_p_bz_s_x32_scaled = SVE32BitGatherLoadWords_ScalarPlus32BitScaledOffsetsFixed | 0x00004000u,
  LDFF1W_z_p_bz_s_x32_scaled = SVE32BitGatherLoadWords_ScalarPlus32BitScaledOffsetsFixed | 0x00006000u
};

enum SVE32BitGatherLoad_ScalarPlus32BitUnscaledOffsetsOp : uint32_t {
  SVE32BitGatherLoad_ScalarPlus32BitUnscaledOffsetsFixed = 0x84000000u,
  SVE32BitGatherLoad_ScalarPlus32BitUnscaledOffsetsFMask = 0xFE208000u,
  SVE32BitGatherLoad_ScalarPlus32BitUnscaledOffsetsMask = 0xFFA0E000u,
  LD1SB_z_p_bz_s_x32_unscaled = SVE32BitGatherLoad_ScalarPlus32BitUnscaledOffsetsFixed,
  LDFF1SB_z_p_bz_s_x32_unscaled = SVE32BitGatherLoad_ScalarPlus32BitUnscaledOffsetsFixed | 0x00002000u,
  LD1B_z_p_bz_s_x32_unscaled = SVE32BitGatherLoad_ScalarPlus32BitUnscaledOffsetsFixed | 0x00004000u,
  LDFF1B_z_p_bz_s_x32_unscaled = SVE32BitGatherLoad_ScalarPlus32BitUnscaledOffsetsFixed | 0x00006000u,
  LD1SH_z_p_bz_s_x32_unscaled = SVE32BitGatherLoad_ScalarPlus32BitUnscaledOffsetsFixed | 0x00800000u,
  LDFF1SH_z_p_bz_s_x32_unscaled = SVE32BitGatherLoad_ScalarPlus32BitUnscaledOffsetsFixed | 0x00802000u,
  LD1H_z_p_bz_s_x32_unscaled = SVE32BitGatherLoad_ScalarPlus32BitUnscaledOffsetsFixed | 0x00804000u,
  LDFF1H_z_p_bz_s_x32_unscaled = SVE32BitGatherLoad_ScalarPlus32BitUnscaledOffsetsFixed | 0x00806000u,
  LD1W_z_p_bz_s_x32_unscaled = SVE32BitGatherLoad_ScalarPlus32BitUnscaledOffsetsFixed | 0x01004000u,
  LDFF1W_z_p_bz_s_x32_unscaled = SVE32BitGatherLoad_ScalarPlus32BitUnscaledOffsetsFixed | 0x01006000u
};

enum SVE32BitGatherLoad_VectorPlusImmOp : uint32_t {
  SVE32BitGatherLoad_VectorPlusImmFixed = 0x84208000u,
  SVE32BitGatherLoad_VectorPlusImmFMask = 0xFE608000u,
  SVE32BitGatherLoad_VectorPlusImmMask = 0xFFE0E000u,
  LD1SB_z_p_ai_s = SVE32BitGatherLoad_VectorPlusImmFixed,
  LDFF1SB_z_p_ai_s = SVE32BitGatherLoad_VectorPlusImmFixed | 0x00002000u,
  LD1B_z_p_ai_s = SVE32BitGatherLoad_VectorPlusImmFixed | 0x00004000u,
  LDFF1B_z_p_ai_s = SVE32BitGatherLoad_VectorPlusImmFixed | 0x00006000u,
  LD1SH_z_p_ai_s = SVE32BitGatherLoad_VectorPlusImmFixed | 0x00800000u,
  LDFF1SH_z_p_ai_s = SVE32BitGatherLoad_VectorPlusImmFixed | 0x00802000u,
  LD1H_z_p_ai_s = SVE32BitGatherLoad_VectorPlusImmFixed | 0x00804000u,
  LDFF1H_z_p_ai_s = SVE32BitGatherLoad_VectorPlusImmFixed | 0x00806000u,
  LD1W_z_p_ai_s = SVE32BitGatherLoad_VectorPlusImmFixed | 0x01004000u,
  LDFF1W_z_p_ai_s = SVE32BitGatherLoad_VectorPlusImmFixed | 0x01006000u
};

enum SVE32BitGatherPrefetch_ScalarPlus32BitScaledOffsetsOp : uint32_t {
  SVE32BitGatherPrefetch_ScalarPlus32BitScaledOffsetsFixed = 0x84200000u,
  SVE32BitGatherPrefetch_ScalarPlus32BitScaledOffsetsFMask = 0xFFA08010u,
  SVE32BitGatherPrefetch_ScalarPlus32BitScaledOffsetsMask = 0xFFA0E010u,
  PRFB_i_p_bz_s_x32_scaled = SVE32BitGatherPrefetch_ScalarPlus32BitScaledOffsetsFixed,
  PRFH_i_p_bz_s_x32_scaled = SVE32BitGatherPrefetch_ScalarPlus32BitScaledOffsetsFixed | 0x00002000u,
  PRFW_i_p_bz_s_x32_scaled = SVE32BitGatherPrefetch_ScalarPlus32BitScaledOffsetsFixed | 0x00004000u,
  PRFD_i_p_bz_s_x32_scaled = SVE32BitGatherPrefetch_ScalarPlus32BitScaledOffsetsFixed | 0x00006000u
};

enum SVE32BitGatherPrefetch_VectorPlusImmOp : uint32_t {
  SVE32BitGatherPrefetch_VectorPlusImmFixed = 0x8400E000u,
  SVE32BitGatherPrefetch_VectorPlusImmFMask = 0xFE60E010u,
  SVE32BitGatherPrefetch_VectorPlusImmMask = 0xFFE0E010u,
  PRFB_i_p_ai_s = SVE32BitGatherPrefetch_VectorPlusImmFixed,
  PRFH_i_p_ai_s = SVE32BitGatherPrefetch_VectorPlusImmFixed | 0x00800000u,
  PRFW_i_p_ai_s = SVE32BitGatherPrefetch_VectorPlusImmFixed | 0x01000000u,
  PRFD_i_p_ai_s = SVE32BitGatherPrefetch_VectorPlusImmFixed | 0x01800000u
};

enum SVE32BitScatterStore_ScalarPlus32BitScaledOffsetsOp : uint32_t {
  SVE32BitScatterStore_ScalarPlus32BitScaledOffsetsFixed = 0xE4608000u,
  SVE32BitScatterStore_ScalarPlus32BitScaledOffsetsFMask = 0xFE60A000u,
  SVE32BitScatterStore_ScalarPlus32BitScaledOffsetsMask = 0xFFE0A000u,
  ST1H_z_p_bz_s_x32_scaled = SVE32BitScatterStore_ScalarPlus32BitScaledOffsetsFixed | 0x00800000u,
  ST1W_z_p_bz_s_x32_scaled = SVE32BitScatterStore_ScalarPlus32BitScaledOffsetsFixed | 0x01000000u
};

enum SVE32BitScatterStore_ScalarPlus32BitUnscaledOffsetsOp : uint32_t {
  SVE32BitScatterStore_ScalarPlus32BitUnscaledOffsetsFixed = 0xE4408000u,
  SVE32BitScatterStore_ScalarPlus32BitUnscaledOffsetsFMask = 0xFE60A000u,
  SVE32BitScatterStore_ScalarPlus32BitUnscaledOffsetsMask = 0xFFE0A000u,
  ST1B_z_p_bz_s_x32_unscaled = SVE32BitScatterStore_ScalarPlus32BitUnscaledOffsetsFixed,
  ST1H_z_p_bz_s_x32_unscaled = SVE32BitScatterStore_ScalarPlus32BitUnscaledOffsetsFixed | 0x00800000u,
  ST1W_z_p_bz_s_x32_unscaled = SVE32BitScatterStore_ScalarPlus32BitUnscaledOffsetsFixed | 0x01000000u
};

enum SVE32BitScatterStore_VectorPlusImmOp : uint32_t {
  SVE32BitScatterStore_VectorPlusImmFixed = 0xE460A000u,
  SVE32BitScatterStore_VectorPlusImmFMask = 0xFE60E000u,
  SVE32BitScatterStore_VectorPlusImmMask = 0xFFE0E000u,
  ST1B_z_p_ai_s = SVE32BitScatterStore_VectorPlusImmFixed,
  ST1H_z_p_ai_s = SVE32BitScatterStore_VectorPlusImmFixed | 0x00800000u,
  ST1W_z_p_ai_s = SVE32BitScatterStore_VectorPlusImmFixed | 0x01000000u
};

enum SVE64BitGatherLoad_ScalarPlus32BitUnpackedScaledOffsetsOp : uint32_t {
  SVE64BitGatherLoad_ScalarPlus32BitUnpackedScaledOffsetsFixed = 0xC4200000u,
  SVE64BitGatherLoad_ScalarPlus32BitUnpackedScaledOffsetsFMask = 0xFE208000u,
  SVE64BitGatherLoad_ScalarPlus32BitUnpackedScaledOffsetsMask = 0xFFA0E000u,
  LD1SH_z_p_bz_d_x32_scaled = SVE64BitGatherLoad_ScalarPlus32BitUnpackedScaledOffsetsFixed | 0x00800000u,
  LDFF1SH_z_p_bz_d_x32_scaled = SVE64BitGatherLoad_ScalarPlus32BitUnpackedScaledOffsetsFixed | 0x00802000u,
  LD1H_z_p_bz_d_x32_scaled = SVE64BitGatherLoad_ScalarPlus32BitUnpackedScaledOffsetsFixed | 0x00804000u,
  LDFF1H_z_p_bz_d_x32_scaled = SVE64BitGatherLoad_ScalarPlus32BitUnpackedScaledOffsetsFixed | 0x00806000u,
  LD1SW_z_p_bz_d_x32_scaled = SVE64BitGatherLoad_ScalarPlus32BitUnpackedScaledOffsetsFixed | 0x01000000u,
  LDFF1SW_z_p_bz_d_x32_scaled = SVE64BitGatherLoad_ScalarPlus32BitUnpackedScaledOffsetsFixed | 0x01002000u,
  LD1W_z_p_bz_d_x32_scaled = SVE64BitGatherLoad_ScalarPlus32BitUnpackedScaledOffsetsFixed | 0x01004000u,
  LDFF1W_z_p_bz_d_x32_scaled = SVE64BitGatherLoad_ScalarPlus32BitUnpackedScaledOffsetsFixed | 0x01006000u,
  LD1D_z_p_bz_d_x32_scaled = SVE64BitGatherLoad_ScalarPlus32BitUnpackedScaledOffsetsFixed | 0x01804000u,
  LDFF1D_z_p_bz_d_x32_scaled = SVE64BitGatherLoad_ScalarPlus32BitUnpackedScaledOffsetsFixed | 0x01806000u
};

enum SVE64BitGatherLoad_ScalarPlus64BitScaledOffsetsOp : uint32_t {
  SVE64BitGatherLoad_ScalarPlus64BitScaledOffsetsFixed = 0xC4608000u,
  SVE64BitGatherLoad_ScalarPlus64BitScaledOffsetsFMask = 0xFE608000u,
  SVE64BitGatherLoad_ScalarPlus64BitScaledOffsetsMask = 0xFFE0E000u,
  LD1SH_z_p_bz_d_64_scaled = SVE64BitGatherLoad_ScalarPlus64BitScaledOffsetsFixed | 0x00800000u,
  LDFF1SH_z_p_bz_d_64_scaled = SVE64BitGatherLoad_ScalarPlus64BitScaledOffsetsFixed | 0x00802000u,
  LD1H_z_p_bz_d_64_scaled = SVE64BitGatherLoad_ScalarPlus64BitScaledOffsetsFixed | 0x00804000u,
  LDFF1H_z_p_bz_d_64_scaled = SVE64BitGatherLoad_ScalarPlus64BitScaledOffsetsFixed | 0x00806000u,
  LD1SW_z_p_bz_d_64_scaled = SVE64BitGatherLoad_ScalarPlus64BitScaledOffsetsFixed | 0x01000000u,
  LDFF1SW_z_p_bz_d_64_scaled = SVE64BitGatherLoad_ScalarPlus64BitScaledOffsetsFixed | 0x01002000u,
  LD1W_z_p_bz_d_64_scaled = SVE64BitGatherLoad_ScalarPlus64BitScaledOffsetsFixed | 0x01004000u,
  LDFF1W_z_p_bz_d_64_scaled = SVE64BitGatherLoad_ScalarPlus64BitScaledOffsetsFixed | 0x01006000u,
  LD1D_z_p_bz_d_64_scaled = SVE64BitGatherLoad_ScalarPlus64BitScaledOffsetsFixed | 0x01804000u,
  LDFF1D_z_p_bz_d_64_scaled = SVE64BitGatherLoad_ScalarPlus64BitScaledOffsetsFixed | 0x01806000u
};

enum SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsOp : uint32_t {
  SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsFixed = 0xC4408000u,
  SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsFMask = 0xFE608000u,
  SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsMask = 0xFFE0E000u,
  LD1SB_z_p_bz_d_64_unscaled = SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsFixed,
  LDFF1SB_z_p_bz_d_64_unscaled = SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsFixed | 0x00002000u,
  LD1B_z_p_bz_d_64_unscaled = SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsFixed | 0x00004000u,
  LDFF1B_z_p_bz_d_64_unscaled = SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsFixed | 0x00006000u,
  LD1SH_z_p_bz_d_64_unscaled = SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsFixed | 0x00800000u,
  LDFF1SH_z_p_bz_d_64_unscaled = SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsFixed | 0x00802000u,
  LD1H_z_p_bz_d_64_unscaled = SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsFixed | 0x00804000u,
  LDFF1H_z_p_bz_d_64_unscaled = SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsFixed | 0x00806000u,
  LD1SW_z_p_bz_d_64_unscaled = SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsFixed | 0x01000000u,
  LDFF1SW_z_p_bz_d_64_unscaled = SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsFixed | 0x01002000u,
  LD1W_z_p_bz_d_64_unscaled = SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsFixed | 0x01004000u,
  LDFF1W_z_p_bz_d_64_unscaled = SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsFixed | 0x01006000u,
  LD1D_z_p_bz_d_64_unscaled = SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsFixed | 0x01804000u,
  LDFF1D_z_p_bz_d_64_unscaled = SVE64BitGatherLoad_ScalarPlus64BitUnscaledOffsetsFixed | 0x01806000u
};

enum SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsOp : uint32_t {
  SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsFixed = 0xC4000000u,
  SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsFMask = 0xFE208000u,
  SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsMask = 0xFFA0E000u,
  LD1SB_z_p_bz_d_x32_unscaled = SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsFixed,
  LDFF1SB_z_p_bz_d_x32_unscaled = SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsFixed | 0x00002000u,
  LD1B_z_p_bz_d_x32_unscaled = SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsFixed | 0x00004000u,
  LDFF1B_z_p_bz_d_x32_unscaled = SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsFixed | 0x00006000u,
  LD1SH_z_p_bz_d_x32_unscaled = SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsFixed | 0x00800000u,
  LDFF1SH_z_p_bz_d_x32_unscaled = SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsFixed | 0x00802000u,
  LD1H_z_p_bz_d_x32_unscaled = SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsFixed | 0x00804000u,
  LDFF1H_z_p_bz_d_x32_unscaled = SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsFixed | 0x00806000u,
  LD1SW_z_p_bz_d_x32_unscaled = SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsFixed | 0x01000000u,
  LDFF1SW_z_p_bz_d_x32_unscaled = SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsFixed | 0x01002000u,
  LD1W_z_p_bz_d_x32_unscaled = SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsFixed | 0x01004000u,
  LDFF1W_z_p_bz_d_x32_unscaled = SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsFixed | 0x01006000u,
  LD1D_z_p_bz_d_x32_unscaled = SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsFixed | 0x01804000u,
  LDFF1D_z_p_bz_d_x32_unscaled = SVE64BitGatherLoad_ScalarPlusUnpacked32BitUnscaledOffsetsFixed | 0x01806000u
};

enum SVE64BitGatherLoad_VectorPlusImmOp : uint32_t {
  SVE64BitGatherLoad_VectorPlusImmFixed = 0xC4208000u,
  SVE64BitGatherLoad_VectorPlusImmFMask = 0xFE608000u,
  SVE64BitGatherLoad_VectorPlusImmMask = 0xFFE0E000u,
  LD1SB_z_p_ai_d = SVE64BitGatherLoad_VectorPlusImmFixed,
  LDFF1SB_z_p_ai_d = SVE64BitGatherLoad_VectorPlusImmFixed | 0x00002000u,
  LD1B_z_p_ai_d = SVE64BitGatherLoad_VectorPlusImmFixed | 0x00004000u,
  LDFF1B_z_p_ai_d = SVE64BitGatherLoad_VectorPlusImmFixed | 0x00006000u,
  LD1SH_z_p_ai_d = SVE64BitGatherLoad_VectorPlusImmFixed | 0x00800000u,
  LDFF1SH_z_p_ai_d = SVE64BitGatherLoad_VectorPlusImmFixed | 0x00802000u,
  LD1H_z_p_ai_d = SVE64BitGatherLoad_VectorPlusImmFixed | 0x00804000u,
  LDFF1H_z_p_ai_d = SVE64BitGatherLoad_VectorPlusImmFixed | 0x00806000u,
  LD1SW_z_p_ai_d = SVE64BitGatherLoad_VectorPlusImmFixed | 0x01000000u,
  LDFF1SW_z_p_ai_d = SVE64BitGatherLoad_VectorPlusImmFixed | 0x01002000u,
  LD1W_z_p_ai_d = SVE64BitGatherLoad_VectorPlusImmFixed | 0x01004000u,
  LDFF1W_z_p_ai_d = SVE64BitGatherLoad_VectorPlusImmFixed | 0x01006000u,
  LD1D_z_p_ai_d = SVE64BitGatherLoad_VectorPlusImmFixed | 0x01804000u,
  LDFF1D_z_p_ai_d = SVE64BitGatherLoad_VectorPlusImmFixed | 0x01806000u
};

enum SVE64BitGatherPrefetch_ScalarPlus64BitScaledOffsetsOp : uint32_t {
  SVE64BitGatherPrefetch_ScalarPlus64BitScaledOffsetsFixed = 0xC4608000u,
  SVE64BitGatherPrefetch_ScalarPlus64BitScaledOffsetsFMask = 0xFFE08010u,
  SVE64BitGatherPrefetch_ScalarPlus64BitScaledOffsetsMask = 0xFFE0E010u,
  PRFB_i_p_bz_d_64_scaled = SVE64BitGatherPrefetch_ScalarPlus64BitScaledOffsetsFixed,
  PRFH_i_p_bz_d_64_scaled = SVE64BitGatherPrefetch_ScalarPlus64BitScaledOffsetsFixed | 0x00002000u,
  PRFW_i_p_bz_d_64_scaled = SVE64BitGatherPrefetch_ScalarPlus64BitScaledOffsetsFixed | 0x00004000u,
  PRFD_i_p_bz_d_64_scaled = SVE64BitGatherPrefetch_ScalarPlus64BitScaledOffsetsFixed | 0x00006000u
};

enum SVE64BitGatherPrefetch_ScalarPlusUnpacked32BitScaledOffsetsOp : uint32_t {
  SVE64BitGatherPrefetch_ScalarPlusUnpacked32BitScaledOffsetsFixed = 0xC4200000u,
  SVE64BitGatherPrefetch_ScalarPlusUnpacked32BitScaledOffsetsFMask = 0xFFA08010u,
  SVE64BitGatherPrefetch_ScalarPlusUnpacked32BitScaledOffsetsMask = 0xFFA0E010u,
  PRFB_i_p_bz_d_x32_scaled = SVE64BitGatherPrefetch_ScalarPlusUnpacked32BitScaledOffsetsFixed,
  PRFH_i_p_bz_d_x32_scaled = SVE64BitGatherPrefetch_ScalarPlusUnpacked32BitScaledOffsetsFixed | 0x00002000u,
  PRFW_i_p_bz_d_x32_scaled = SVE64BitGatherPrefetch_ScalarPlusUnpacked32BitScaledOffsetsFixed | 0x00004000u,
  PRFD_i_p_bz_d_x32_scaled = SVE64BitGatherPrefetch_ScalarPlusUnpacked32BitScaledOffsetsFixed | 0x00006000u
};

enum SVE64BitGatherPrefetch_VectorPlusImmOp : uint32_t {
  SVE64BitGatherPrefetch_VectorPlusImmFixed = 0xC400E000u,
  SVE64BitGatherPrefetch_VectorPlusImmFMask = 0xFE60E010u,
  SVE64BitGatherPrefetch_VectorPlusImmMask = 0xFFE0E010u,
  PRFB_i_p_ai_d = SVE64BitGatherPrefetch_VectorPlusImmFixed,
  PRFH_i_p_ai_d = SVE64BitGatherPrefetch_VectorPlusImmFixed | 0x00800000u,
  PRFW_i_p_ai_d = SVE64BitGatherPrefetch_VectorPlusImmFixed | 0x01000000u,
  PRFD_i_p_ai_d = SVE64BitGatherPrefetch_VectorPlusImmFixed | 0x01800000u
};

enum SVE64BitScatterStore_ScalarPlus64BitScaledOffsetsOp : uint32_t {
  SVE64BitScatterStore_ScalarPlus64BitScaledOffsetsFixed = 0xE420A000u,
  SVE64BitScatterStore_ScalarPlus64BitScaledOffsetsFMask = 0xFE60E000u,
  SVE64BitScatterStore_ScalarPlus64BitScaledOffsetsMask = 0xFFE0E000u,
  ST1H_z_p_bz_d_64_scaled = SVE64BitScatterStore_ScalarPlus64BitScaledOffsetsFixed | 0x00800000u,
  ST1W_z_p_bz_d_64_scaled = SVE64BitScatterStore_ScalarPlus64BitScaledOffsetsFixed | 0x01000000u,
  ST1D_z_p_bz_d_64_scaled = SVE64BitScatterStore_ScalarPlus64BitScaledOffsetsFixed | 0x01800000u
};

enum SVE64BitScatterStore_ScalarPlus64BitUnscaledOffsetsOp : uint32_t {
  SVE64BitScatterStore_ScalarPlus64BitUnscaledOffsetsFixed = 0xE400A000u,
  SVE64BitScatterStore_ScalarPlus64BitUnscaledOffsetsFMask = 0xFE60E000u,
  SVE64BitScatterStore_ScalarPlus64BitUnscaledOffsetsMask = 0xFFE0E000u,
  ST1B_z_p_bz_d_64_unscaled = SVE64BitScatterStore_ScalarPlus64BitUnscaledOffsetsFixed,
  ST1H_z_p_bz_d_64_unscaled = SVE64BitScatterStore_ScalarPlus64BitUnscaledOffsetsFixed | 0x00800000u,
  ST1W_z_p_bz_d_64_unscaled = SVE64BitScatterStore_ScalarPlus64BitUnscaledOffsetsFixed | 0x01000000u,
  ST1D_z_p_bz_d_64_unscaled = SVE64BitScatterStore_ScalarPlus64BitUnscaledOffsetsFixed | 0x01800000u
};

enum SVE64BitScatterStore_ScalarPlusUnpacked32BitScaledOffsetsOp : uint32_t {
  SVE64BitScatterStore_ScalarPlusUnpacked32BitScaledOffsetsFixed = 0xE4208000u,
  SVE64BitScatterStore_ScalarPlusUnpacked32BitScaledOffsetsFMask = 0xFE60A000u,
  SVE64BitScatterStore_ScalarPlusUnpacked32BitScaledOffsetsMask = 0xFFE0A000u,
  ST1H_z_p_bz_d_x32_scaled = SVE64BitScatterStore_ScalarPlusUnpacked32BitScaledOffsetsFixed | 0x00800000u,
  ST1W_z_p_bz_d_x32_scaled = SVE64BitScatterStore_ScalarPlusUnpacked32BitScaledOffsetsFixed | 0x01000000u,
  ST1D_z_p_bz_d_x32_scaled = SVE64BitScatterStore_ScalarPlusUnpacked32BitScaledOffsetsFixed | 0x01800000u
};

enum SVE64BitScatterStore_ScalarPlusUnpacked32BitUnscaledOffsetsOp : uint32_t {
  SVE64BitScatterStore_ScalarPlusUnpacked32BitUnscaledOffsetsFixed = 0xE4008000u,
  SVE64BitScatterStore_ScalarPlusUnpacked32BitUnscaledOffsetsFMask = 0xFE60A000u,
  SVE64BitScatterStore_ScalarPlusUnpacked32BitUnscaledOffsetsMask = 0xFFE0A000u,
  ST1B_z_p_bz_d_x32_unscaled = SVE64BitScatterStore_ScalarPlusUnpacked32BitUnscaledOffsetsFixed,
  ST1H_z_p_bz_d_x32_unscaled = SVE64BitScatterStore_ScalarPlusUnpacked32BitUnscaledOffsetsFixed | 0x00800000u,
  ST1W_z_p_bz_d_x32_unscaled = SVE64BitScatterStore_ScalarPlusUnpacked32BitUnscaledOffsetsFixed | 0x01000000u,
  ST1D_z_p_bz_d_x32_unscaled = SVE64BitScatterStore_ScalarPlusUnpacked32BitUnscaledOffsetsFixed | 0x01800000u
};

enum SVE64BitScatterStore_VectorPlusImmOp : uint32_t {
  SVE64BitScatterStore_VectorPlusImmFixed = 0xE440A000u,
  SVE64BitScatterStore_VectorPlusImmFMask = 0xFE60E000u,
  SVE64BitScatterStore_VectorPlusImmMask = 0xFFE0E000u,
  ST1B_z_p_ai_d = SVE64BitScatterStore_VectorPlusImmFixed,
  ST1H_z_p_ai_d = SVE64BitScatterStore_VectorPlusImmFixed | 0x00800000u,
  ST1W_z_p_ai_d = SVE64BitScatterStore_VectorPlusImmFixed | 0x01000000u,
  ST1D_z_p_ai_d = SVE64BitScatterStore_VectorPlusImmFixed | 0x01800000u
};

enum SVEAddressGenerationOp : uint32_t {
  SVEAddressGenerationFixed = 0x0420A000u,
  SVEAddressGenerationFMask = 0xFF20F000u,
  SVEAddressGenerationMask = 0xFFE0F000u,
  ADR_z_az_d_s32_scaled = SVEAddressGenerationFixed,
  ADR_z_az_d_u32_scaled = SVEAddressGenerationFixed | 0x00400000u,
  ADR_z_az_s_same_scaled = SVEAddressGenerationFixed | 0x00800000u,
  ADR_z_az_d_same_scaled = SVEAddressGenerationFixed | 0x00C00000u
};

enum SVEBitwiseLogicalUnpredicatedOp : uint32_t {
  SVEBitwiseLogicalUnpredicatedFixed = 0x04202000u,
  SVEBitwiseLogicalUnpredicatedFMask = 0xFF20E000u,
  SVEBitwiseLogicalUnpredicatedMask = 0xFFE0FC00u,
  AND_z_zz = SVEBitwiseLogicalUnpredicatedFixed | 0x00001000u,
  ORR_z_zz = SVEBitwiseLogicalUnpredicatedFixed | 0x00401000u,
  EOR_z_zz = SVEBitwiseLogicalUnpredicatedFixed | 0x00801000u,
  BIC_z_zz = SVEBitwiseLogicalUnpredicatedFixed | 0x00C01000u
};

enum SVEBitwiseLogicalWithImm_UnpredicatedOp : uint32_t {
  SVEBitwiseLogicalWithImm_UnpredicatedFixed = 0x05000000u,
  SVEBitwiseLogicalWithImm_UnpredicatedFMask = 0xFF3C0000u,
  SVEBitwiseLogicalWithImm_UnpredicatedMask = 0xFFFC0000u,
  ORR_z_zi = SVEBitwiseLogicalWithImm_UnpredicatedFixed,
  EOR_z_zi = SVEBitwiseLogicalWithImm_UnpredicatedFixed | 0x00400000u,
  AND_z_zi = SVEBitwiseLogicalWithImm_UnpredicatedFixed | 0x00800000u
};

enum SVEBitwiseLogical_PredicatedOp : uint32_t {
  SVEBitwiseLogical_PredicatedFixed = 0x04180000u,
  SVEBitwiseLogical_PredicatedFMask = 0xFF38E000u,
  SVEBitwiseLogical_PredicatedMask = 0xFF3FE000u,
  ORR_z_p_zz = SVEBitwiseLogical_PredicatedFixed,
  EOR_z_p_zz = SVEBitwiseLogical_PredicatedFixed | 0x00010000u,
  AND_z_p_zz = SVEBitwiseLogical_PredicatedFixed | 0x00020000u,
  BIC_z_p_zz = SVEBitwiseLogical_PredicatedFixed | 0x00030000u
};

enum SVEBitwiseShiftByImm_PredicatedOp : uint32_t {
  SVEBitwiseShiftByImm_PredicatedFixed = 0x04008000u,
  SVEBitwiseShiftByImm_PredicatedFMask = 0xFF30E000u,
  SVEBitwiseShiftByImm_PredicatedMask = 0xFF3FE000u,
  ASR_z_p_zi = SVEBitwiseShiftByImm_PredicatedFixed,
  LSR_z_p_zi = SVEBitwiseShiftByImm_PredicatedFixed | 0x00010000u,
  LSL_z_p_zi = SVEBitwiseShiftByImm_PredicatedFixed | 0x00030000u,
  ASRD_z_p_zi = SVEBitwiseShiftByImm_PredicatedFixed | 0x00040000u
};

enum SVEBitwiseShiftByVector_PredicatedOp : uint32_t {
  SVEBitwiseShiftByVector_PredicatedFixed = 0x04108000u,
  SVEBitwiseShiftByVector_PredicatedFMask = 0xFF38E000u,
  SVEBitwiseShiftByVector_PredicatedMask = 0xFF3FE000u,
  ASR_z_p_zz = SVEBitwiseShiftByVector_PredicatedFixed,
  LSR_z_p_zz = SVEBitwiseShiftByVector_PredicatedFixed | 0x00010000u,
  LSL_z_p_zz = SVEBitwiseShiftByVector_PredicatedFixed | 0x00030000u,
  ASRR_z_p_zz = SVEBitwiseShiftByVector_PredicatedFixed | 0x00040000u,
  LSRR_z_p_zz = SVEBitwiseShiftByVector_PredicatedFixed | 0x00050000u,
  LSLR_z_p_zz = SVEBitwiseShiftByVector_PredicatedFixed | 0x00070000u
};

enum SVEBitwiseShiftByWideElements_PredicatedOp : uint32_t {
  SVEBitwiseShiftByWideElements_PredicatedFixed = 0x04188000u,
  SVEBitwiseShiftByWideElements_PredicatedFMask = 0xFF38E000u,
  SVEBitwiseShiftByWideElements_PredicatedMask = 0xFF3FE000u,
  ASR_z_p_zw = SVEBitwiseShiftByWideElements_PredicatedFixed,
  LSR_z_p_zw = SVEBitwiseShiftByWideElements_PredicatedFixed | 0x00010000u,
  LSL_z_p_zw = SVEBitwiseShiftByWideElements_PredicatedFixed | 0x00030000u
};

enum SVEBitwiseShiftUnpredicatedOp : uint32_t {
  SVEBitwiseShiftUnpredicatedFixed = 0x04208000u,
  SVEBitwiseShiftUnpredicatedFMask = 0xFF20E000u,
  SVEBitwiseShiftUnpredicatedMask = 0xFF20FC00u,
  ASR_z_zw = SVEBitwiseShiftUnpredicatedFixed,
  LSR_z_zw = SVEBitwiseShiftUnpredicatedFixed | 0x00000400u,
  LSL_z_zw = SVEBitwiseShiftUnpredicatedFixed | 0x00000C00u,
  ASR_z_zi = SVEBitwiseShiftUnpredicatedFixed | 0x00001000u,
  LSR_z_zi = SVEBitwiseShiftUnpredicatedFixed | 0x00001400u,
  LSL_z_zi = SVEBitwiseShiftUnpredicatedFixed | 0x00001C00u
};

enum SVEBroadcastBitmaskImmOp : uint32_t {
  SVEBroadcastBitmaskImmFixed = 0x05C00000u,
  SVEBroadcastBitmaskImmFMask = 0xFFFC0000u,
  SVEBroadcastBitmaskImmMask = 0xFFFC0000u,
  DUPM_z_i = SVEBroadcastBitmaskImmFixed
};

enum SVEBroadcastFPImm_UnpredicatedOp : uint32_t {
  SVEBroadcastFPImm_UnpredicatedFixed = 0x2539C000u,
  SVEBroadcastFPImm_UnpredicatedFMask = 0xFF39C000u,
  SVEBroadcastFPImm_UnpredicatedMask = 0xFF3FE000u,
  FDUP_z_i = SVEBroadcastFPImm_UnpredicatedFixed
};

enum SVEBroadcastGeneralRegisterOp : uint32_t {
  SVEBroadcastGeneralRegisterFixed = 0x05203800u,
  SVEBroadcastGeneralRegisterFMask = 0xFF3FFC00u,
  SVEBroadcastGeneralRegisterMask = 0xFF3FFC00u,
  DUP_z_r = SVEBroadcastGeneralRegisterFixed
};

enum SVEBroadcastIndexElementOp : uint32_t {
  SVEBroadcastIndexElementFixed = 0x05202000u,
  SVEBroadcastIndexElementFMask = 0xFF20FC00u,
  SVEBroadcastIndexElementMask = 0xFF20FC00u,
  DUP_z_zi = SVEBroadcastIndexElementFixed
};

enum SVEBroadcastIntImm_UnpredicatedOp : uint32_t {
  SVEBroadcastIntImm_UnpredicatedFixed = 0x2538C000u,
  SVEBroadcastIntImm_UnpredicatedFMask = 0xFF39C000u,
  SVEBroadcastIntImm_UnpredicatedMask = 0xFF3FC000u,
  DUP_z_i = SVEBroadcastIntImm_UnpredicatedFixed
};

enum SVECompressActiveElementsOp : uint32_t {
  SVECompressActiveElementsFixed = 0x05A18000u,
  SVECompressActiveElementsFMask = 0xFFBFE000u,
  SVECompressActiveElementsMask = 0xFFBFE000u,
  COMPACT_z_p_z = SVECompressActiveElementsFixed
};

enum SVEConditionallyBroadcastElementToVectorOp : uint32_t {
  SVEConditionallyBroadcastElementToVectorFixed = 0x05288000u,
  SVEConditionallyBroadcastElementToVectorFMask = 0xFF3EE000u,
  SVEConditionallyBroadcastElementToVectorMask = 0xFF3FE000u,
  CLASTA_z_p_zz = SVEConditionallyBroadcastElementToVectorFixed,
  CLASTB_z_p_zz = SVEConditionallyBroadcastElementToVectorFixed | 0x00010000u
};

enum SVEConditionallyExtractElementToGeneralRegisterOp : uint32_t {
  SVEConditionallyExtractElementToGeneralRegisterFixed = 0x0530A000u,
  SVEConditionallyExtractElementToGeneralRegisterFMask = 0xFF3EE000u,
  SVEConditionallyExtractElementToGeneralRegisterMask = 0xFF3FE000u,
  CLASTA_r_p_z = SVEConditionallyExtractElementToGeneralRegisterFixed,
  CLASTB_r_p_z = SVEConditionallyExtractElementToGeneralRegisterFixed | 0x00010000u
};

enum SVEConditionallyExtractElementToSIMDFPScalarOp : uint32_t {
  SVEConditionallyExtractElementToSIMDFPScalarFixed = 0x052A8000u,
  SVEConditionallyExtractElementToSIMDFPScalarFMask = 0xFF3EE000u,
  SVEConditionallyExtractElementToSIMDFPScalarMask = 0xFF3FE000u,
  CLASTA_v_p_z = SVEConditionallyExtractElementToSIMDFPScalarFixed,
  CLASTB_v_p_z = SVEConditionallyExtractElementToSIMDFPScalarFixed | 0x00010000u
};

enum SVEConditionallyTerminateScalarsOp : uint32_t {
  SVEConditionallyTerminateScalarsFixed = 0x25202000u,
  SVEConditionallyTerminateScalarsFMask = 0xFF20FC0Fu,
  SVEConditionallyTerminateScalarsMask = 0xFFA0FC1Fu,
  CTERMEQ_rr = SVEConditionallyTerminateScalarsFixed | 0x00800000u,
  CTERMNE_rr = SVEConditionallyTerminateScalarsFixed | 0x00800010u
};

enum SVEConstructivePrefix_UnpredicatedOp : uint32_t {
  SVEConstructivePrefix_UnpredicatedFixed = 0x0420BC00u,
  SVEConstructivePrefix_UnpredicatedFMask = 0xFF20FC00u,
  SVEConstructivePrefix_UnpredicatedMask = 0xFFFFFC00u,
  MOVPRFX_z_z = SVEConstructivePrefix_UnpredicatedFixed
};

enum SVEContiguousFirstFaultLoad_ScalarPlusScalarOp : uint32_t {
  SVEContiguousFirstFaultLoad_ScalarPlusScalarFixed = 0xA4006000u,
  SVEContiguousFirstFaultLoad_ScalarPlusScalarFMask = 0xFE00E000u,
  SVEContiguousFirstFaultLoad_ScalarPlusScalarMask = 0xFFE0E000u,
  LDFF1B_z_p_br_u8 = SVEContiguousFirstFaultLoad_ScalarPlusScalarFixed,
  LDFF1B_z_p_br_u16 = SVEContiguousFirstFaultLoad_ScalarPlusScalarFixed | 0x00200000u,
  LDFF1B_z_p_br_u32 = SVEContiguousFirstFaultLoad_ScalarPlusScalarFixed | 0x00400000u,
  LDFF1B_z_p_br_u64 = SVEContiguousFirstFaultLoad_ScalarPlusScalarFixed | 0x00600000u,
  LDFF1SW_z_p_br_s64 = SVEContiguousFirstFaultLoad_ScalarPlusScalarFixed | 0x00800000u,
  LDFF1H_z_p_br_u16 = SVEContiguousFirstFaultLoad_ScalarPlusScalarFixed | 0x00A00000u,
  LDFF1H_z_p_br_u32 = SVEContiguousFirstFaultLoad_ScalarPlusScalarFixed | 0x00C00000u,
  LDFF1H_z_p_br_u64 = SVEContiguousFirstFaultLoad_ScalarPlusScalarFixed | 0x00E00000u,
  LDFF1SH_z_p_br_s64 = SVEContiguousFirstFaultLoad_ScalarPlusScalarFixed | 0x01000000u,
  LDFF1SH_z_p_br_s32 = SVEContiguousFirstFaultLoad_ScalarPlusScalarFixed | 0x01200000u,
  LDFF1W_z_p_br_u32 = SVEContiguousFirstFaultLoad_ScalarPlusScalarFixed | 0x01400000u,
  LDFF1W_z_p_br_u64 = SVEContiguousFirstFaultLoad_ScalarPlusScalarFixed | 0x01600000u,
  LDFF1SB_z_p_br_s64 = SVEContiguousFirstFaultLoad_ScalarPlusScalarFixed | 0x01800000u,
  LDFF1SB_z_p_br_s32 = SVEContiguousFirstFaultLoad_ScalarPlusScalarFixed | 0x01A00000u,
  LDFF1SB_z_p_br_s16 = SVEContiguousFirstFaultLoad_ScalarPlusScalarFixed | 0x01C00000u,
  LDFF1D_z_p_br_u64 = SVEContiguousFirstFaultLoad_ScalarPlusScalarFixed | 0x01E00000u
};

enum SVEContiguousLoad_ScalarPlusImmOp : uint32_t {
  SVEContiguousLoad_ScalarPlusImmFixed = 0xA400A000u,
  SVEContiguousLoad_ScalarPlusImmFMask = 0xFE10E000u,
  SVEContiguousLoad_ScalarPlusImmMask = 0xFFF0E000u,
  LD1B_z_p_bi_u8 = SVEContiguousLoad_ScalarPlusImmFixed,
  LD1B_z_p_bi_u16 = SVEContiguousLoad_ScalarPlusImmFixed | 0x00200000u,
  LD1B_z_p_bi_u32 = SVEContiguousLoad_ScalarPlusImmFixed | 0x00400000u,
  LD1B_z_p_bi_u64 = SVEContiguousLoad_ScalarPlusImmFixed | 0x00600000u,
  LD1SW_z_p_bi_s64 = SVEContiguousLoad_ScalarPlusImmFixed | 0x00800000u,
  LD1H_z_p_bi_u16 = SVEContiguousLoad_ScalarPlusImmFixed | 0x00A00000u,
  LD1H_z_p_bi_u32 = SVEContiguousLoad_ScalarPlusImmFixed | 0x00C00000u,
  LD1H_z_p_bi_u64 = SVEContiguousLoad_ScalarPlusImmFixed | 0x00E00000u,
  LD1SH_z_p_bi_s64 = SVEContiguousLoad_ScalarPlusImmFixed | 0x01000000u,
  LD1SH_z_p_bi_s32 = SVEContiguousLoad_ScalarPlusImmFixed | 0x01200000u,
  LD1W_z_p_bi_u32 = SVEContiguousLoad_ScalarPlusImmFixed | 0x01400000u,
  LD1W_z_p_bi_u64 = SVEContiguousLoad_ScalarPlusImmFixed | 0x01600000u,
  LD1SB_z_p_bi_s64 = SVEContiguousLoad_ScalarPlusImmFixed | 0x01800000u,
  LD1SB_z_p_bi_s32 = SVEContiguousLoad_ScalarPlusImmFixed | 0x01A00000u,
  LD1SB_z_p_bi_s16 = SVEContiguousLoad_ScalarPlusImmFixed | 0x01C00000u,
  LD1D_z_p_bi_u64 = SVEContiguousLoad_ScalarPlusImmFixed | 0x01E00000u
};

enum SVEContiguousLoad_ScalarPlusScalarOp : uint32_t {
  SVEContiguousLoad_ScalarPlusScalarFixed = 0xA4004000u,
  SVEContiguousLoad_ScalarPlusScalarFMask = 0xFE00E000u,
  SVEContiguousLoad_ScalarPlusScalarMask = 0xFFE0E000u,
  LD1B_z_p_br_u8 = SVEContiguousLoad_ScalarPlusScalarFixed,
  LD1B_z_p_br_u16 = SVEContiguousLoad_ScalarPlusScalarFixed | 0x00200000u,
  LD1B_z_p_br_u32 = SVEContiguousLoad_ScalarPlusScalarFixed | 0x00400000u,
  LD1B_z_p_br_u64 = SVEContiguousLoad_ScalarPlusScalarFixed | 0x00600000u,
  LD1SW_z_p_br_s64 = SVEContiguousLoad_ScalarPlusScalarFixed | 0x00800000u,
  LD1H_z_p_br_u16 = SVEContiguousLoad_ScalarPlusScalarFixed | 0x00A00000u,
  LD1H_z_p_br_u32 = SVEContiguousLoad_ScalarPlusScalarFixed | 0x00C00000u,
  LD1H_z_p_br_u64 = SVEContiguousLoad_ScalarPlusScalarFixed | 0x00E00000u,
  LD1SH_z_p_br_s64 = SVEContiguousLoad_ScalarPlusScalarFixed | 0x01000000u,
  LD1SH_z_p_br_s32 = SVEContiguousLoad_ScalarPlusScalarFixed | 0x01200000u,
  LD1W_z_p_br_u32 = SVEContiguousLoad_ScalarPlusScalarFixed | 0x01400000u,
  LD1W_z_p_br_u64 = SVEContiguousLoad_ScalarPlusScalarFixed | 0x01600000u,
  LD1SB_z_p_br_s64 = SVEContiguousLoad_ScalarPlusScalarFixed | 0x01800000u,
  LD1SB_z_p_br_s32 = SVEContiguousLoad_ScalarPlusScalarFixed | 0x01A00000u,
  LD1SB_z_p_br_s16 = SVEContiguousLoad_ScalarPlusScalarFixed | 0x01C00000u,
  LD1D_z_p_br_u64 = SVEContiguousLoad_ScalarPlusScalarFixed | 0x01E00000u
};

enum SVEContiguousNonFaultLoad_ScalarPlusImmOp : uint32_t {
  SVEContiguousNonFaultLoad_ScalarPlusImmFixed = 0xA410A000u,
  SVEContiguousNonFaultLoad_ScalarPlusImmFMask = 0xFE10E000u,
  SVEContiguousNonFaultLoad_ScalarPlusImmMask = 0xFFF0E000u,
  LDNF1B_z_p_bi_u8 = SVEContiguousNonFaultLoad_ScalarPlusImmFixed,
  LDNF1B_z_p_bi_u16 = SVEContiguousNonFaultLoad_ScalarPlusImmFixed | 0x00200000u,
  LDNF1B_z_p_bi_u32 = SVEContiguousNonFaultLoad_ScalarPlusImmFixed | 0x00400000u,
  LDNF1B_z_p_bi_u64 = SVEContiguousNonFaultLoad_ScalarPlusImmFixed | 0x00600000u,
  LDNF1SW_z_p_bi_s64 = SVEContiguousNonFaultLoad_ScalarPlusImmFixed | 0x00800000u,
  LDNF1H_z_p_bi_u16 = SVEContiguousNonFaultLoad_ScalarPlusImmFixed | 0x00A00000u,
  LDNF1H_z_p_bi_u32 = SVEContiguousNonFaultLoad_ScalarPlusImmFixed | 0x00C00000u,
  LDNF1H_z_p_bi_u64 = SVEContiguousNonFaultLoad_ScalarPlusImmFixed | 0x00E00000u,
  LDNF1SH_z_p_bi_s64 = SVEContiguousNonFaultLoad_ScalarPlusImmFixed | 0x01000000u,
  LDNF1SH_z_p_bi_s32 = SVEContiguousNonFaultLoad_ScalarPlusImmFixed | 0x01200000u,
  LDNF1W_z_p_bi_u32 = SVEContiguousNonFaultLoad_ScalarPlusImmFixed | 0x01400000u,
  LDNF1W_z_p_bi_u64 = SVEContiguousNonFaultLoad_ScalarPlusImmFixed | 0x01600000u,
  LDNF1SB_z_p_bi_s64 = SVEContiguousNonFaultLoad_ScalarPlusImmFixed | 0x01800000u,
  LDNF1SB_z_p_bi_s32 = SVEContiguousNonFaultLoad_ScalarPlusImmFixed | 0x01A00000u,
  LDNF1SB_z_p_bi_s16 = SVEContiguousNonFaultLoad_ScalarPlusImmFixed | 0x01C00000u,
  LDNF1D_z_p_bi_u64 = SVEContiguousNonFaultLoad_ScalarPlusImmFixed | 0x01E00000u
};

enum SVEContiguousNonTemporalLoad_ScalarPlusImmOp : uint32_t {
  SVEContiguousNonTemporalLoad_ScalarPlusImmFixed = 0xA400E000u,
  SVEContiguousNonTemporalLoad_ScalarPlusImmFMask = 0xFE70E000u,
  SVEContiguousNonTemporalLoad_ScalarPlusImmMask = 0xFFF0E000u,
  LDNT1B_z_p_bi_contiguous = SVEContiguousNonTemporalLoad_ScalarPlusImmFixed,
  LDNT1H_z_p_bi_contiguous = SVEContiguousNonTemporalLoad_ScalarPlusImmFixed | 0x00800000u,
  LDNT1W_z_p_bi_contiguous = SVEContiguousNonTemporalLoad_ScalarPlusImmFixed | 0x01000000u,
  LDNT1D_z_p_bi_contiguous = SVEContiguousNonTemporalLoad_ScalarPlusImmFixed | 0x01800000u
};

enum SVEContiguousNonTemporalLoad_ScalarPlusScalarOp : uint32_t {
  SVEContiguousNonTemporalLoad_ScalarPlusScalarFixed = 0xA400C000u,
  SVEContiguousNonTemporalLoad_ScalarPlusScalarFMask = 0xFE60E000u,
  SVEContiguousNonTemporalLoad_ScalarPlusScalarMask = 0xFFE0E000u,
  LDNT1B_z_p_br_contiguous = SVEContiguousNonTemporalLoad_ScalarPlusScalarFixed,
  LDNT1H_z_p_br_contiguous = SVEContiguousNonTemporalLoad_ScalarPlusScalarFixed | 0x00800000u,
  LDNT1W_z_p_br_contiguous = SVEContiguousNonTemporalLoad_ScalarPlusScalarFixed | 0x01000000u,
  LDNT1D_z_p_br_contiguous = SVEContiguousNonTemporalLoad_ScalarPlusScalarFixed | 0x01800000u
};

enum SVEContiguousNonTemporalStore_ScalarPlusImmOp : uint32_t {
  SVEContiguousNonTemporalStore_ScalarPlusImmFixed = 0xE410E000u,
  SVEContiguousNonTemporalStore_ScalarPlusImmFMask = 0xFE70E000u,
  SVEContiguousNonTemporalStore_ScalarPlusImmMask = 0xFFF0E000u,
  STNT1B_z_p_bi_contiguous = SVEContiguousNonTemporalStore_ScalarPlusImmFixed,
  STNT1H_z_p_bi_contiguous = SVEContiguousNonTemporalStore_ScalarPlusImmFixed | 0x00800000u,
  STNT1W_z_p_bi_contiguous = SVEContiguousNonTemporalStore_ScalarPlusImmFixed | 0x01000000u,
  STNT1D_z_p_bi_contiguous = SVEContiguousNonTemporalStore_ScalarPlusImmFixed | 0x01800000u
};

enum SVEContiguousNonTemporalStore_ScalarPlusScalarOp : uint32_t {
  SVEContiguousNonTemporalStore_ScalarPlusScalarFixed = 0xE4006000u,
  SVEContiguousNonTemporalStore_ScalarPlusScalarFMask = 0xFE60E000u,
  SVEContiguousNonTemporalStore_ScalarPlusScalarMask = 0xFFE0E000u,
  STNT1B_z_p_br_contiguous = SVEContiguousNonTemporalStore_ScalarPlusScalarFixed,
  STNT1H_z_p_br_contiguous = SVEContiguousNonTemporalStore_ScalarPlusScalarFixed | 0x00800000u,
  STNT1W_z_p_br_contiguous = SVEContiguousNonTemporalStore_ScalarPlusScalarFixed | 0x01000000u,
  STNT1D_z_p_br_contiguous = SVEContiguousNonTemporalStore_ScalarPlusScalarFixed | 0x01800000u
};

enum SVEContiguousPrefetch_ScalarPlusImmOp : uint32_t {
  SVEContiguousPrefetch_ScalarPlusImmFixed = 0x85C00000u,
  SVEContiguousPrefetch_ScalarPlusImmFMask = 0xFFC08010u,
  SVEContiguousPrefetch_ScalarPlusImmMask = 0xFFC0E010u,
  PRFB_i_p_bi_s = SVEContiguousPrefetch_ScalarPlusImmFixed,
  PRFH_i_p_bi_s = SVEContiguousPrefetch_ScalarPlusImmFixed | 0x00002000u,
  PRFW_i_p_bi_s = SVEContiguousPrefetch_ScalarPlusImmFixed | 0x00004000u,
  PRFD_i_p_bi_s = SVEContiguousPrefetch_ScalarPlusImmFixed | 0x00006000u
};

enum SVEContiguousPrefetch_ScalarPlusScalarOp : uint32_t {
  SVEContiguousPrefetch_ScalarPlusScalarFixed = 0x8400C000u,
  SVEContiguousPrefetch_ScalarPlusScalarFMask = 0xFE60E010u,
  SVEContiguousPrefetch_ScalarPlusScalarMask = 0xFFE0E010u,
  PRFB_i_p_br_s = SVEContiguousPrefetch_ScalarPlusScalarFixed,
  PRFH_i_p_br_s = SVEContiguousPrefetch_ScalarPlusScalarFixed | 0x00800000u,
  PRFW_i_p_br_s = SVEContiguousPrefetch_ScalarPlusScalarFixed | 0x01000000u,
  PRFD_i_p_br_s = SVEContiguousPrefetch_ScalarPlusScalarFixed | 0x01800000u
};

enum SVEContiguousStore_ScalarPlusImmOp : uint32_t {
  SVEContiguousStore_ScalarPlusImmFixed = 0xE400E000u,
  SVEContiguousStore_ScalarPlusImmFMask = 0xFE10E000u,
  SVEContiguousStore_ScalarPlusImmMask = 0xFF90E000u,
  ST1B_z_p_bi = SVEContiguousStore_ScalarPlusImmFixed,
  ST1H_z_p_bi = SVEContiguousStore_ScalarPlusImmFixed | 0x00800000u,
  ST1W_z_p_bi = SVEContiguousStore_ScalarPlusImmFixed | 0x01000000u,
  ST1D_z_p_bi = SVEContiguousStore_ScalarPlusImmFixed | 0x01800000u
};

enum SVEContiguousStore_ScalarPlusScalarOp : uint32_t {
  SVEContiguousStore_ScalarPlusScalarFixed = 0xE4004000u,
  SVEContiguousStore_ScalarPlusScalarFMask = 0xFE00E000u,
  SVEContiguousStore_ScalarPlusScalarMask = 0xFF80E000u,
  ST1B_z_p_br = SVEContiguousStore_ScalarPlusScalarFixed,
  ST1H_z_p_br = SVEContiguousStore_ScalarPlusScalarFixed | 0x00800000u,
  ST1W_z_p_br = SVEContiguousStore_ScalarPlusScalarFixed | 0x01000000u,
  ST1D_z_p_br = SVEContiguousStore_ScalarPlusScalarFixed | 0x01800000u
};

enum SVECopyFPImm_PredicatedOp : uint32_t {
  SVECopyFPImm_PredicatedFixed = 0x0510C000u,
  SVECopyFPImm_PredicatedFMask = 0xFF30E000u,
  SVECopyFPImm_PredicatedMask = 0xFF30E000u,
  FCPY_z_p_i = SVECopyFPImm_PredicatedFixed
};

enum SVECopyGeneralRegisterToVector_PredicatedOp : uint32_t {
  SVECopyGeneralRegisterToVector_PredicatedFixed = 0x0528A000u,
  SVECopyGeneralRegisterToVector_PredicatedFMask = 0xFF3FE000u,
  SVECopyGeneralRegisterToVector_PredicatedMask = 0xFF3FE000u,
  CPY_z_p_r = SVECopyGeneralRegisterToVector_PredicatedFixed
};

enum SVECopyIntImm_PredicatedOp : uint32_t {
  SVECopyIntImm_PredicatedFixed = 0x05100000u,
  SVECopyIntImm_PredicatedFMask = 0xFF308000u,
  SVECopyIntImm_PredicatedMask = 0xFF308000u,
  CPY_z_p_i = SVECopyIntImm_PredicatedFixed
};

enum SVECopySIMDFPScalarRegisterToVector_PredicatedOp : uint32_t {
  SVECopySIMDFPScalarRegisterToVector_PredicatedFixed = 0x05208000u,
  SVECopySIMDFPScalarRegisterToVector_PredicatedFMask = 0xFF3FE000u,
  SVECopySIMDFPScalarRegisterToVector_PredicatedMask = 0xFF3FE000u,
  CPY_z_p_v = SVECopySIMDFPScalarRegisterToVector_PredicatedFixed
};

enum SVEElementCountOp : uint32_t {
  SVEElementCountFixed = 0x0420E000u,
  SVEElementCountFMask = 0xFF30F800u,
  SVEElementCountMask = 0xFFF0FC00u,
  CNTB_r_s = SVEElementCountFixed,
  CNTH_r_s = SVEElementCountFixed | 0x00400000u,
  CNTW_r_s = SVEElementCountFixed | 0x00800000u,
  CNTD_r_s = SVEElementCountFixed | 0x00C00000u
};

enum SVEExtractElementToGeneralRegisterOp : uint32_t {
  SVEExtractElementToGeneralRegisterFixed = 0x0520A000u,
  SVEExtractElementToGeneralRegisterFMask = 0xFF3EE000u,
  SVEExtractElementToGeneralRegisterMask = 0xFF3FE000u,
  LASTA_r_p_z = SVEExtractElementToGeneralRegisterFixed,
  LASTB_r_p_z = SVEExtractElementToGeneralRegisterFixed | 0x00010000u
};

enum SVEExtractElementToSIMDFPScalarRegisterOp : uint32_t {
  SVEExtractElementToSIMDFPScalarRegisterFixed = 0x05228000u,
  SVEExtractElementToSIMDFPScalarRegisterFMask = 0xFF3EE000u,
  SVEExtractElementToSIMDFPScalarRegisterMask = 0xFF3FE000u,
  LASTA_v_p_z = SVEExtractElementToSIMDFPScalarRegisterFixed,
  LASTB_v_p_z = SVEExtractElementToSIMDFPScalarRegisterFixed | 0x00010000u
};

enum SVEFFRInitialiseOp : uint32_t {
  SVEFFRInitialiseFixed = 0x252C9000u,
  SVEFFRInitialiseFMask = 0xFF3FFFFFu,
  SVEFFRInitialiseMask = 0xFFFFFFFFu,
  SETFFR_f = SVEFFRInitialiseFixed
};

enum SVEFFRWriteFromPredicateOp : uint32_t {
  SVEFFRWriteFromPredicateFixed = 0x25289000u,
  SVEFFRWriteFromPredicateFMask = 0xFF3FFE1Fu,
  SVEFFRWriteFromPredicateMask = 0xFFFFFE1Fu,
  WRFFR_f_p = SVEFFRWriteFromPredicateFixed
};

enum SVEFPAccumulatingReductionOp : uint32_t {
  SVEFPAccumulatingReductionFixed = 0x65182000u,
  SVEFPAccumulatingReductionFMask = 0xFF38E000u,
  SVEFPAccumulatingReductionMask = 0xFF3FE000u,
  FADDA_v_p_z = SVEFPAccumulatingReductionFixed
};

enum SVEFPArithmeticUnpredicatedOp : uint32_t {
  SVEFPArithmeticUnpredicatedFixed = 0x65000000u,
  SVEFPArithmeticUnpredicatedFMask = 0xFF20E000u,
  SVEFPArithmeticUnpredicatedMask = 0xFF20FC00u,
  FADD_z_zz = SVEFPArithmeticUnpredicatedFixed,
  FSUB_z_zz = SVEFPArithmeticUnpredicatedFixed | 0x00000400u,
  FMUL_z_zz = SVEFPArithmeticUnpredicatedFixed | 0x00000800u,
  FTSMUL_z_zz = SVEFPArithmeticUnpredicatedFixed | 0x00000C00u,
  FRECPS_z_zz = SVEFPArithmeticUnpredicatedFixed | 0x00001800u,
  FRSQRTS_z_zz = SVEFPArithmeticUnpredicatedFixed | 0x00001C00u
};

enum SVEFPArithmeticWithImm_PredicatedOp : uint32_t {
  SVEFPArithmeticWithImm_PredicatedFixed = 0x65188000u,
  SVEFPArithmeticWithImm_PredicatedFMask = 0xFF38E3C0u,
  SVEFPArithmeticWithImm_PredicatedMask = 0xFF3FE3C0u,
  FADD_z_p_zs = SVEFPArithmeticWithImm_PredicatedFixed,
  FSUB_z_p_zs = SVEFPArithmeticWithImm_PredicatedFixed | 0x00010000u,
  FMUL_z_p_zs = SVEFPArithmeticWithImm_PredicatedFixed | 0x00020000u,
  FSUBR_z_p_zs = SVEFPArithmeticWithImm_PredicatedFixed | 0x00030000u,
  FMAXNM_z_p_zs = SVEFPArithmeticWithImm_PredicatedFixed | 0x00040000u,
  FMINNM_z_p_zs = SVEFPArithmeticWithImm_PredicatedFixed | 0x00050000u,
  FMAX_z_p_zs = SVEFPArithmeticWithImm_PredicatedFixed | 0x00060000u,
  FMIN_z_p_zs = SVEFPArithmeticWithImm_PredicatedFixed | 0x00070000u
};

enum SVEFPArithmetic_PredicatedOp : uint32_t {
  SVEFPArithmetic_PredicatedFixed = 0x65008000u,
  SVEFPArithmetic_PredicatedFMask = 0xFF30E000u,
  SVEFPArithmetic_PredicatedMask = 0xFF3FE000u,
  FADD_z_p_zz = SVEFPArithmetic_PredicatedFixed,
  FSUB_z_p_zz = SVEFPArithmetic_PredicatedFixed | 0x00010000u,
  FMUL_z_p_zz = SVEFPArithmetic_PredicatedFixed | 0x00020000u,
  FSUBR_z_p_zz = SVEFPArithmetic_PredicatedFixed | 0x00030000u,
  FMAXNM_z_p_zz = SVEFPArithmetic_PredicatedFixed | 0x00040000u,
  FMINNM_z_p_zz = SVEFPArithmetic_PredicatedFixed | 0x00050000u,
  FMAX_z_p_zz = SVEFPArithmetic_PredicatedFixed | 0x00060000u,
  FMIN_z_p_zz = SVEFPArithmetic_PredicatedFixed | 0x00070000u,
  FABD_z_p_zz = SVEFPArithmetic_PredicatedFixed | 0x00080000u,
  FSCALE_z_p_zz = SVEFPArithmetic_PredicatedFixed | 0x00090000u,
  FMULX_z_p_zz = SVEFPArithmetic_PredicatedFixed | 0x000A0000u,
  FDIVR_z_p_zz = SVEFPArithmetic_PredicatedFixed | 0x000C0000u,
  FDIV_z_p_zz = SVEFPArithmetic_PredicatedFixed | 0x000D0000u
};

enum SVEFPCompareVectorsOp : uint32_t {
  SVEFPCompareVectorsFixed = 0x65004000u,
  SVEFPCompareVectorsFMask = 0xFF204000u,
  SVEFPCompareVectorsMask = 0xFF20E010u,
  FCMGE_p_p_zz = SVEFPCompareVectorsFixed,
  FCMGT_p_p_zz = SVEFPCompareVectorsFixed | 0x00000010u,
  FCMEQ_p_p_zz = SVEFPCompareVectorsFixed | 0x00002000u,
  FCMNE_p_p_zz = SVEFPCompareVectorsFixed | 0x00002010u,
  FCMUO_p_p_zz = SVEFPCompareVectorsFixed | 0x00008000u,
  FACGE_p_p_zz = SVEFPCompareVectorsFixed | 0x00008010u,
  FACGT_p_p_zz = SVEFPCompareVectorsFixed | 0x0000A010u
};

enum SVEFPCompareWithZeroOp : uint32_t {
  SVEFPCompareWithZeroFixed = 0x65102000u,
  SVEFPCompareWithZeroFMask = 0xFF38E000u,
  SVEFPCompareWithZeroMask = 0xFF3FE010u,
  FCMGE_p_p_z0 = SVEFPCompareWithZeroFixed,
  FCMGT_p_p_z0 = SVEFPCompareWithZeroFixed | 0x00000010u,
  FCMLT_p_p_z0 = SVEFPCompareWithZeroFixed | 0x00010000u,
  FCMLE_p_p_z0 = SVEFPCompareWithZeroFixed | 0x00010010u,
  FCMEQ_p_p_z0 = SVEFPCompareWithZeroFixed | 0x00020000u,
  FCMNE_p_p_z0 = SVEFPCompareWithZeroFixed | 0x00030000u
};

enum SVEFPComplexAdditionOp : uint32_t {
  SVEFPComplexAdditionFixed = 0x64008000u,
  SVEFPComplexAdditionFMask = 0xFF3EE000u,
  SVEFPComplexAdditionMask = 0xFF3EE000u,
  FCADD_z_p_zz = SVEFPComplexAdditionFixed
};

enum SVEFPComplexMulAddOp : uint32_t {
  SVEFPComplexMulAddFixed = 0x64000000u,
  SVEFPComplexMulAddFMask = 0xFF208000u,
  SVEFPComplexMulAddMask = 0xFF208000u,
  FCMLA_z_p_zzz = SVEFPComplexMulAddFixed
};

enum SVEFPComplexMulAddIndexOp : uint32_t {
  SVEFPComplexMulAddIndexFixed = 0x64201000u,
  SVEFPComplexMulAddIndexFMask = 0xFF20F000u,
  SVEFPComplexMulAddIndexMask = 0xFFE0F000u,
  FCMLA_z_zzzi_h = SVEFPComplexMulAddIndexFixed | 0x00800000u,
  FCMLA_z_zzzi_s = SVEFPComplexMulAddIndexFixed | 0x00C00000u
};

enum SVEFPConvertPrecisionOp : uint32_t {
  SVEFPConvertPrecisionFixed = 0x6508A000u,
  SVEFPConvertPrecisionFMask = 0xFF3CE000u,
  SVEFPConvertPrecisionMask = 0xFFFFE000u,
  FCVT_z_p_z_s2h = SVEFPConvertPrecisionFixed | 0x00800000u,
  FCVT_z_p_z_h2s = SVEFPConvertPrecisionFixed | 0x00810000u,
  FCVT_z_p_z_d2h = SVEFPConvertPrecisionFixed | 0x00C00000u,
  FCVT_z_p_z_h2d = SVEFPConvertPrecisionFixed | 0x00C10000u,
  FCVT_z_p_z_d2s = SVEFPConvertPrecisionFixed | 0x00C20000u,
  FCVT_z_p_z_s2d = SVEFPConvertPrecisionFixed | 0x00C30000u
};

enum SVEFPConvertToIntOp : uint32_t {
  SVEFPConvertToIntFixed = 0x6518A000u,
  SVEFPConvertToIntFMask = 0xFF38E000u,
  SVEFPConvertToIntMask = 0xFFFFE000u,
  FCVTZS_z_p_z_fp162h = SVEFPConvertToIntFixed | 0x00420000u,
  FCVTZU_z_p_z_fp162h = SVEFPConvertToIntFixed | 0x00430000u,
  FCVTZS_z_p_z_fp162w = SVEFPConvertToIntFixed | 0x00440000u,
  FCVTZU_z_p_z_fp162w = SVEFPConvertToIntFixed | 0x00450000u,
  FCVTZS_z_p_z_fp162x = SVEFPConvertToIntFixed | 0x00460000u,
  FCVTZU_z_p_z_fp162x = SVEFPConvertToIntFixed | 0x00470000u,
  FCVTZS_z_p_z_s2w = SVEFPConvertToIntFixed | 0x00840000u,
  FCVTZU_z_p_z_s2w = SVEFPConvertToIntFixed | 0x00850000u,
  FCVTZS_z_p_z_d2w = SVEFPConvertToIntFixed | 0x00C00000u,
  FCVTZU_z_p_z_d2w = SVEFPConvertToIntFixed | 0x00C10000u,
  FCVTZS_z_p_z_s2x = SVEFPConvertToIntFixed | 0x00C40000u,
  FCVTZU_z_p_z_s2x = SVEFPConvertToIntFixed | 0x00C50000u,
  FCVTZS_z_p_z_d2x = SVEFPConvertToIntFixed | 0x00C60000u,
  FCVTZU_z_p_z_d2x = SVEFPConvertToIntFixed | 0x00C70000u
};

enum SVEFPExponentialAcceleratorOp : uint32_t {
  SVEFPExponentialAcceleratorFixed = 0x0420B800u,
  SVEFPExponentialAcceleratorFMask = 0xFF20FC00u,
  SVEFPExponentialAcceleratorMask = 0xFF3FFC00u,
  FEXPA_z_z = SVEFPExponentialAcceleratorFixed
};

enum SVEFPFastReductionOp : uint32_t {
  SVEFPFastReductionFixed = 0x65002000u,
  SVEFPFastReductionFMask = 0xFF38E000u,
  SVEFPFastReductionMask = 0xFF3FE000u,
  FADDV_v_p_z = SVEFPFastReductionFixed,
  FMAXNMV_v_p_z = SVEFPFastReductionFixed | 0x00040000u,
  FMINNMV_v_p_z = SVEFPFastReductionFixed | 0x00050000u,
  FMAXV_v_p_z = SVEFPFastReductionFixed | 0x00060000u,
  FMINV_v_p_z = SVEFPFastReductionFixed | 0x00070000u
};

enum SVEFPMulAddOp : uint32_t {
  SVEFPMulAddFixed = 0x65200000u,
  SVEFPMulAddFMask = 0xFF200000u,
  SVEFPMulAddMask = 0xFF20E000u,
  FMLA_z_p_zzz = SVEFPMulAddFixed,
  FMLS_z_p_zzz = SVEFPMulAddFixed | 0x00002000u,
  FNMLA_z_p_zzz = SVEFPMulAddFixed | 0x00004000u,
  FNMLS_z_p_zzz = SVEFPMulAddFixed | 0x00006000u,
  FMAD_z_p_zzz = SVEFPMulAddFixed | 0x00008000u,
  FMSB_z_p_zzz = SVEFPMulAddFixed | 0x0000A000u,
  FNMAD_z_p_zzz = SVEFPMulAddFixed | 0x0000C000u,
  FNMSB_z_p_zzz = SVEFPMulAddFixed | 0x0000E000u
};

enum SVEFPMulAddIndexOp : uint32_t {
  SVEFPMulAddIndexFixed = 0x64200000u,
  SVEFPMulAddIndexFMask = 0xFF20F800u,
  SVEFPMulAddIndexMask = 0xFFE0FC00u,
  FMLA_z_zzzi_h = SVEFPMulAddIndexFixed,
  FMLA_z_zzzi_h_i3h = FMLA_z_zzzi_h | 0x00400000u,
  FMLS_z_zzzi_h = SVEFPMulAddIndexFixed | 0x00000400u,
  FMLS_z_zzzi_h_i3h = FMLS_z_zzzi_h | 0x00400000u,
  FMLA_z_zzzi_s = SVEFPMulAddIndexFixed | 0x00800000u,
  FMLS_z_zzzi_s = SVEFPMulAddIndexFixed | 0x00800400u,
  FMLA_z_zzzi_d = SVEFPMulAddIndexFixed | 0x00C00000u,
  FMLS_z_zzzi_d = SVEFPMulAddIndexFixed | 0x00C00400u
};

enum SVEFPMulIndexOp : uint32_t {
  SVEFPMulIndexFixed = 0x64202000u,
  SVEFPMulIndexFMask = 0xFF20FC00u,
  SVEFPMulIndexMask = 0xFFE0FC00u,
  FMUL_z_zzi_h = SVEFPMulIndexFixed,
  FMUL_z_zzi_h_i3h = FMUL_z_zzi_h | 0x00400000u,
  FMUL_z_zzi_s = SVEFPMulIndexFixed | 0x00800000u,
  FMUL_z_zzi_d = SVEFPMulIndexFixed | 0x00C00000u
};

enum SVEFPRoundToIntegralValueOp : uint32_t {
  SVEFPRoundToIntegralValueFixed = 0x6500A000u,
  SVEFPRoundToIntegralValueFMask = 0xFF38E000u,
  SVEFPRoundToIntegralValueMask = 0xFF3FE000u,
  FRINTN_z_p_z = SVEFPRoundToIntegralValueFixed,
  FRINTP_z_p_z = SVEFPRoundToIntegralValueFixed | 0x00010000u,
  FRINTM_z_p_z = SVEFPRoundToIntegralValueFixed | 0x00020000u,
  FRINTZ_z_p_z = SVEFPRoundToIntegralValueFixed | 0x00030000u,
  FRINTA_z_p_z = SVEFPRoundToIntegralValueFixed | 0x00040000u,
  FRINTX_z_p_z = SVEFPRoundToIntegralValueFixed | 0x00060000u,
  FRINTI_z_p_z = SVEFPRoundToIntegralValueFixed | 0x00070000u
};

enum SVEFPTrigMulAddCoefficientOp : uint32_t {
  SVEFPTrigMulAddCoefficientFixed = 0x65108000u,
  SVEFPTrigMulAddCoefficientFMask = 0xFF38FC00u,
  SVEFPTrigMulAddCoefficientMask = 0xFF38FC00u,
  FTMAD_z_zzi = SVEFPTrigMulAddCoefficientFixed
};

enum SVEFPTrigSelectCoefficientOp : uint32_t {
  SVEFPTrigSelectCoefficientFixed = 0x0420B000u,
  SVEFPTrigSelectCoefficientFMask = 0xFF20F800u,
  SVEFPTrigSelectCoefficientMask = 0xFF20FC00u,
  FTSSEL_z_zz = SVEFPTrigSelectCoefficientFixed
};

enum SVEFPUnaryOpOp : uint32_t {
  SVEFPUnaryOpFixed = 0x650CA000u,
  SVEFPUnaryOpFMask = 0xFF3CE000u,
  SVEFPUnaryOpMask = 0xFF3FE000u,
  FRECPX_z_p_z = SVEFPUnaryOpFixed,
  FSQRT_z_p_z = SVEFPUnaryOpFixed | 0x00010000u
};

enum SVEFPUnaryOpUnpredicatedOp : uint32_t {
  SVEFPUnaryOpUnpredicatedFixed = 0x65083000u,
  SVEFPUnaryOpUnpredicatedFMask = 0xFF38F000u,
  SVEFPUnaryOpUnpredicatedMask = 0xFF3FFC00u,
  FRECPE_z_z = SVEFPUnaryOpUnpredicatedFixed | 0x00060000u,
  FRSQRTE_z_z = SVEFPUnaryOpUnpredicatedFixed | 0x00070000u
};

enum SVEIncDecByPredicateCountOp : uint32_t {
  SVEIncDecByPredicateCountFixed = 0x25288000u,
  SVEIncDecByPredicateCountFMask = 0xFF38F000u,
  SVEIncDecByPredicateCountMask = 0xFF3FFE00u,
  SQINCP_z_p_z = SVEIncDecByPredicateCountFixed,
  SQINCP_r_p_r_sx = SVEIncDecByPredicateCountFixed | 0x00000800u,
  SQINCP_r_p_r_x = SVEIncDecByPredicateCountFixed | 0x00000C00u,
  UQINCP_z_p_z = SVEIncDecByPredicateCountFixed | 0x00010000u,
  UQINCP_r_p_r_uw = SVEIncDecByPredicateCountFixed | 0x00010800u,
  UQINCP_r_p_r_x = SVEIncDecByPredicateCountFixed | 0x00010C00u,
  SQDECP_z_p_z = SVEIncDecByPredicateCountFixed | 0x00020000u,
  SQDECP_r_p_r_sx = SVEIncDecByPredicateCountFixed | 0x00020800u,
  SQDECP_r_p_r_x = SVEIncDecByPredicateCountFixed | 0x00020C00u,
  UQDECP_z_p_z = SVEIncDecByPredicateCountFixed | 0x00030000u,
  UQDECP_r_p_r_uw = SVEIncDecByPredicateCountFixed | 0x00030800u,
  UQDECP_r_p_r_x = SVEIncDecByPredicateCountFixed | 0x00030C00u,
  INCP_z_p_z = SVEIncDecByPredicateCountFixed | 0x00040000u,
  INCP_r_p_r = SVEIncDecByPredicateCountFixed | 0x00040800u,
  DECP_z_p_z = SVEIncDecByPredicateCountFixed | 0x00050000u,
  DECP_r_p_r = SVEIncDecByPredicateCountFixed | 0x00050800u
};

enum SVEIncDecRegisterByElementCountOp : uint32_t {
  SVEIncDecRegisterByElementCountFixed = 0x0430E000u,
  SVEIncDecRegisterByElementCountFMask = 0xFF30F800u,
  SVEIncDecRegisterByElementCountMask = 0xFFF0FC00u,
  INCB_r_rs = SVEIncDecRegisterByElementCountFixed,
  DECB_r_rs = SVEIncDecRegisterByElementCountFixed | 0x00000400u,
  INCH_r_rs = SVEIncDecRegisterByElementCountFixed | 0x00400000u,
  DECH_r_rs = SVEIncDecRegisterByElementCountFixed | 0x00400400u,
  INCW_r_rs = SVEIncDecRegisterByElementCountFixed | 0x00800000u,
  DECW_r_rs = SVEIncDecRegisterByElementCountFixed | 0x00800400u,
  INCD_r_rs = SVEIncDecRegisterByElementCountFixed | 0x00C00000u,
  DECD_r_rs = SVEIncDecRegisterByElementCountFixed | 0x00C00400u
};

enum SVEIncDecVectorByElementCountOp : uint32_t {
  SVEIncDecVectorByElementCountFixed = 0x0430C000u,
  SVEIncDecVectorByElementCountFMask = 0xFF30F800u,
  SVEIncDecVectorByElementCountMask = 0xFFF0FC00u,
  INCH_z_zs = SVEIncDecVectorByElementCountFixed | 0x00400000u,
  DECH_z_zs = SVEIncDecVectorByElementCountFixed | 0x00400400u,
  INCW_z_zs = SVEIncDecVectorByElementCountFixed | 0x00800000u,
  DECW_z_zs = SVEIncDecVectorByElementCountFixed | 0x00800400u,
  INCD_z_zs = SVEIncDecVectorByElementCountFixed | 0x00C00000u,
  DECD_z_zs = SVEIncDecVectorByElementCountFixed | 0x00C00400u
};

enum SVEIndexGenerationOp : uint32_t {
  SVEIndexGenerationFixed = 0x04204000u,
  SVEIndexGenerationFMask = 0xFF20F000u,
  SVEIndexGenerationMask = 0xFF20FC00u,
  INDEX_z_ii = SVEIndexGenerationFixed,
  INDEX_z_ri = SVEIndexGenerationFixed | 0x00000400u,
  INDEX_z_ir = SVEIndexGenerationFixed | 0x00000800u,
  INDEX_z_rr = SVEIndexGenerationFixed | 0x00000C00u
};

enum SVEInsertGeneralRegisterOp : uint32_t {
  SVEInsertGeneralRegisterFixed = 0x05243800u,
  SVEInsertGeneralRegisterFMask = 0xFF3FFC00u,
  SVEInsertGeneralRegisterMask = 0xFF3FFC00u,
  INSR_z_r = SVEInsertGeneralRegisterFixed
};

enum SVEInsertSIMDFPScalarRegisterOp : uint32_t {
  SVEInsertSIMDFPScalarRegisterFixed = 0x05343800u,
  SVEInsertSIMDFPScalarRegisterFMask = 0xFF3FFC00u,
  SVEInsertSIMDFPScalarRegisterMask = 0xFF3FFC00u,
  INSR_z_v = SVEInsertSIMDFPScalarRegisterFixed
};

enum SVEIntAddSubtractImm_UnpredicatedOp : uint32_t {
  SVEIntAddSubtractImm_UnpredicatedFixed = 0x2520C000u,
  SVEIntAddSubtractImm_UnpredicatedFMask = 0xFF38C000u,
  SVEIntAddSubtractImm_UnpredicatedMask = 0xFF3FC000u,
  ADD_z_zi = SVEIntAddSubtractImm_UnpredicatedFixed,
  SUB_z_zi = SVEIntAddSubtractImm_UnpredicatedFixed | 0x00010000u,
  SUBR_z_zi = SVEIntAddSubtractImm_UnpredicatedFixed | 0x00030000u,
  SQADD_z_zi = SVEIntAddSubtractImm_UnpredicatedFixed | 0x00040000u,
  UQADD_z_zi = SVEIntAddSubtractImm_UnpredicatedFixed | 0x00050000u,
  SQSUB_z_zi = SVEIntAddSubtractImm_UnpredicatedFixed | 0x00060000u,
  UQSUB_z_zi = SVEIntAddSubtractImm_UnpredicatedFixed | 0x00070000u
};

enum SVEIntAddSubtractVectors_PredicatedOp : uint32_t {
  SVEIntAddSubtractVectors_PredicatedFixed = 0x04000000u,
  SVEIntAddSubtractVectors_PredicatedFMask = 0xFF38E000u,
  SVEIntAddSubtractVectors_PredicatedMask = 0xFF3FE000u,
  ADD_z_p_zz = SVEIntAddSubtractVectors_PredicatedFixed,
  SUB_z_p_zz = SVEIntAddSubtractVectors_PredicatedFixed | 0x00010000u,
  SUBR_z_p_zz = SVEIntAddSubtractVectors_PredicatedFixed | 0x00030000u
};

enum SVEIntArithmeticUnpredicatedOp : uint32_t {
  SVEIntArithmeticUnpredicatedFixed = 0x04200000u,
  SVEIntArithmeticUnpredicatedFMask = 0xFF20E000u,
  SVEIntArithmeticUnpredicatedMask = 0xFF20FC00u,
  ADD_z_zz = SVEIntArithmeticUnpredicatedFixed,
  SUB_z_zz = SVEIntArithmeticUnpredicatedFixed | 0x00000400u,
  SQADD_z_zz = SVEIntArithmeticUnpredicatedFixed | 0x00001000u,
  UQADD_z_zz = SVEIntArithmeticUnpredicatedFixed | 0x00001400u,
  SQSUB_z_zz = SVEIntArithmeticUnpredicatedFixed | 0x00001800u,
  UQSUB_z_zz = SVEIntArithmeticUnpredicatedFixed | 0x00001C00u
};

enum SVEIntCompareScalarCountAndLimitOp : uint32_t {
  SVEIntCompareScalarCountAndLimitFixed = 0x25200000u,
  SVEIntCompareScalarCountAndLimitFMask = 0xFF20E000u,
  SVEIntCompareScalarCountAndLimitMask = 0xFF20EC10u,
  WHILELT_p_p_rr = SVEIntCompareScalarCountAndLimitFixed | 0x00000400u,
  WHILELE_p_p_rr = SVEIntCompareScalarCountAndLimitFixed | 0x00000410u,
  WHILELO_p_p_rr = SVEIntCompareScalarCountAndLimitFixed | 0x00000C00u,
  WHILELS_p_p_rr = SVEIntCompareScalarCountAndLimitFixed | 0x00000C10u
};

enum SVEIntCompareSignedImmOp : uint32_t {
  SVEIntCompareSignedImmFixed = 0x25000000u,
  SVEIntCompareSignedImmFMask = 0xFF204000u,
  SVEIntCompareSignedImmMask = 0xFF20E010u,
  CMPGE_p_p_zi = SVEIntCompareSignedImmFixed,
  CMPGT_p_p_zi = SVEIntCompareSignedImmFixed | 0x00000010u,
  CMPLT_p_p_zi = SVEIntCompareSignedImmFixed | 0x00002000u,
  CMPLE_p_p_zi = SVEIntCompareSignedImmFixed | 0x00002010u,
  CMPEQ_p_p_zi = SVEIntCompareSignedImmFixed | 0x00008000u,
  CMPNE_p_p_zi = SVEIntCompareSignedImmFixed | 0x00008010u
};

enum SVEIntCompareUnsignedImmOp : uint32_t {
  SVEIntCompareUnsignedImmFixed = 0x24200000u,
  SVEIntCompareUnsignedImmFMask = 0xFF200000u,
  SVEIntCompareUnsignedImmMask = 0xFF202010u,
  CMPHS_p_p_zi = SVEIntCompareUnsignedImmFixed,
  CMPHI_p_p_zi = SVEIntCompareUnsignedImmFixed | 0x00000010u,
  CMPLO_p_p_zi = SVEIntCompareUnsignedImmFixed | 0x00002000u,
  CMPLS_p_p_zi = SVEIntCompareUnsignedImmFixed | 0x00002010u
};

enum SVEIntCompareVectorsOp : uint32_t {
  SVEIntCompareVectorsFixed = 0x24000000u,
  SVEIntCompareVectorsFMask = 0xFF200000u,
  SVEIntCompareVectorsMask = 0xFF20E010u,
  CMPHS_p_p_zz = SVEIntCompareVectorsFixed,
  CMPHI_p_p_zz = SVEIntCompareVectorsFixed | 0x00000010u,
  CMPEQ_p_p_zw = SVEIntCompareVectorsFixed | 0x00002000u,
  CMPNE_p_p_zw = SVEIntCompareVectorsFixed | 0x00002010u,
  CMPGE_p_p_zw = SVEIntCompareVectorsFixed | 0x00004000u,
  CMPGT_p_p_zw = SVEIntCompareVectorsFixed | 0x00004010u,
  CMPLT_p_p_zw = SVEIntCompareVectorsFixed | 0x00006000u,
  CMPLE_p_p_zw = SVEIntCompareVectorsFixed | 0x00006010u,
  CMPGE_p_p_zz = SVEIntCompareVectorsFixed | 0x00008000u,
  CMPGT_p_p_zz = SVEIntCompareVectorsFixed | 0x00008010u,
  CMPEQ_p_p_zz = SVEIntCompareVectorsFixed | 0x0000A000u,
  CMPNE_p_p_zz = SVEIntCompareVectorsFixed | 0x0000A010u,
  CMPHS_p_p_zw = SVEIntCompareVectorsFixed | 0x0000C000u,
  CMPHI_p_p_zw = SVEIntCompareVectorsFixed | 0x0000C010u,
  CMPLO_p_p_zw = SVEIntCompareVectorsFixed | 0x0000E000u,
  CMPLS_p_p_zw = SVEIntCompareVectorsFixed | 0x0000E010u
};

enum SVEIntConvertToFPOp : uint32_t {
  SVEIntConvertToFPFixed = 0x6510A000u,
  SVEIntConvertToFPFMask = 0xFF38E000u,
  SVEIntConvertToFPMask = 0xFFFFE000u,
  SCVTF_z_p_z_h2fp16 = SVEIntConvertToFPFixed | 0x00420000u,
  UCVTF_z_p_z_h2fp16 = SVEIntConvertToFPFixed | 0x00430000u,
  SCVTF_z_p_z_w2fp16 = SVEIntConvertToFPFixed | 0x00440000u,
  UCVTF_z_p_z_w2fp16 = SVEIntConvertToFPFixed | 0x00450000u,
  SCVTF_z_p_z_x2fp16 = SVEIntConvertToFPFixed | 0x00460000u,
  UCVTF_z_p_z_x2fp16 = SVEIntConvertToFPFixed | 0x00470000u,
  SCVTF_z_p_z_w2s = SVEIntConvertToFPFixed | 0x00840000u,
  UCVTF_z_p_z_w2s = SVEIntConvertToFPFixed | 0x00850000u,
  SCVTF_z_p_z_w2d = SVEIntConvertToFPFixed | 0x00C00000u,
  UCVTF_z_p_z_w2d = SVEIntConvertToFPFixed | 0x00C10000u,
  SCVTF_z_p_z_x2s = SVEIntConvertToFPFixed | 0x00C40000u,
  UCVTF_z_p_z_x2s = SVEIntConvertToFPFixed | 0x00C50000u,
  SCVTF_z_p_z_x2d = SVEIntConvertToFPFixed | 0x00C60000u,
  UCVTF_z_p_z_x2d = SVEIntConvertToFPFixed | 0x00C70000u
};

enum SVEIntDivideVectors_PredicatedOp : uint32_t {
  SVEIntDivideVectors_PredicatedFixed = 0x04140000u,
  SVEIntDivideVectors_PredicatedFMask = 0xFF3CE000u,
  SVEIntDivideVectors_PredicatedMask = 0xFF3FE000u,
  SDIV_z_p_zz = SVEIntDivideVectors_PredicatedFixed,
  UDIV_z_p_zz = SVEIntDivideVectors_PredicatedFixed | 0x00010000u,
  SDIVR_z_p_zz = SVEIntDivideVectors_PredicatedFixed | 0x00020000u,
  UDIVR_z_p_zz = SVEIntDivideVectors_PredicatedFixed | 0x00030000u
};

enum SVEIntMinMaxDifference_PredicatedOp : uint32_t {
  SVEIntMinMaxDifference_PredicatedFixed = 0x04080000u,
  SVEIntMinMaxDifference_PredicatedFMask = 0xFF38E000u,
  SVEIntMinMaxDifference_PredicatedMask = 0xFF3FE000u,
  SMAX_z_p_zz = SVEIntMinMaxDifference_PredicatedFixed,
  UMAX_z_p_zz = SVEIntMinMaxDifference_PredicatedFixed | 0x00010000u,
  SMIN_z_p_zz = SVEIntMinMaxDifference_PredicatedFixed | 0x00020000u,
  UMIN_z_p_zz = SVEIntMinMaxDifference_PredicatedFixed | 0x00030000u,
  SABD_z_p_zz = SVEIntMinMaxDifference_PredicatedFixed | 0x00040000u,
  UABD_z_p_zz = SVEIntMinMaxDifference_PredicatedFixed | 0x00050000u
};

enum SVEIntMinMaxImm_UnpredicatedOp : uint32_t {
  SVEIntMinMaxImm_UnpredicatedFixed = 0x2528C000u,
  SVEIntMinMaxImm_UnpredicatedFMask = 0xFF38C000u,
  SVEIntMinMaxImm_UnpredicatedMask = 0xFF3FE000u,
  SMAX_z_zi = SVEIntMinMaxImm_UnpredicatedFixed,
  UMAX_z_zi = SVEIntMinMaxImm_UnpredicatedFixed | 0x00010000u,
  SMIN_z_zi = SVEIntMinMaxImm_UnpredicatedFixed | 0x00020000u,
  UMIN_z_zi = SVEIntMinMaxImm_UnpredicatedFixed | 0x00030000u
};

enum SVEIntMulAddPredicatedOp : uint32_t {
  SVEIntMulAddPredicatedFixed = 0x04004000u,
  SVEIntMulAddPredicatedFMask = 0xFF204000u,
  SVEIntMulAddPredicatedMask = 0xFF20E000u,
  MLA_z_p_zzz = SVEIntMulAddPredicatedFixed,
  MLS_z_p_zzz = SVEIntMulAddPredicatedFixed | 0x00002000u,
  MAD_z_p_zzz = SVEIntMulAddPredicatedFixed | 0x00008000u,
  MSB_z_p_zzz = SVEIntMulAddPredicatedFixed | 0x0000A000u
};

enum SVEIntMulAddUnpredicatedOp : uint32_t {
  SVEIntMulAddUnpredicatedFixed = 0x44000000u,
  SVEIntMulAddUnpredicatedFMask = 0xFF208000u,
  SVEIntMulAddUnpredicatedMask = 0xFF20FC00u,
  SDOT_z_zzz = SVEIntMulAddUnpredicatedFixed,
  UDOT_z_zzz = SVEIntMulAddUnpredicatedFixed | 0x00000400u
};

enum SVEIntMulImm_UnpredicatedOp : uint32_t {
  SVEIntMulImm_UnpredicatedFixed = 0x2530C000u,
  SVEIntMulImm_UnpredicatedFMask = 0xFF38C000u,
  SVEIntMulImm_UnpredicatedMask = 0xFF3FE000u,
  MUL_z_zi = SVEIntMulImm_UnpredicatedFixed
};

enum SVEIntMulVectors_PredicatedOp : uint32_t {
  SVEIntMulVectors_PredicatedFixed = 0x04100000u,
  SVEIntMulVectors_PredicatedFMask = 0xFF3CE000u,
  SVEIntMulVectors_PredicatedMask = 0xFF3FE000u,
  MUL_z_p_zz = SVEIntMulVectors_PredicatedFixed,
  SMULH_z_p_zz = SVEIntMulVectors_PredicatedFixed | 0x00020000u,
  UMULH_z_p_zz = SVEIntMulVectors_PredicatedFixed | 0x00030000u
};

enum SVEMovprfxOp : uint32_t {
  SVEMovprfxFixed = 0x04002000u,
  SVEMovprfxFMask = 0xFF20E000u,
  SVEMovprfxMask = 0xFF3EE000u,
  MOVPRFX_z_p_z = SVEMovprfxFixed | 0x00100000u
};

enum SVEIntReductionOp : uint32_t {
  SVEIntReductionFixed = 0x04002000u,
  SVEIntReductionFMask = 0xFF20E000u,
  SVEIntReductionMask = 0xFF3FE000u,
  SADDV_r_p_z = SVEIntReductionFixed,
  UADDV_r_p_z = SVEIntReductionFixed | 0x00010000u,
  SMAXV_r_p_z = SVEIntReductionFixed | 0x00080000u,
  UMAXV_r_p_z = SVEIntReductionFixed | 0x00090000u,
  SMINV_r_p_z = SVEIntReductionFixed | 0x000A0000u,
  UMINV_r_p_z = SVEIntReductionFixed | 0x000B0000u
};

enum SVEIntReductionLogicalOp : uint32_t {
  SVEIntReductionLogicalFixed = 0x04182000u,
  SVEIntReductionLogicalFMask = 0xFF38E000u,
  SVEIntReductionLogicalMask = 0xFF3FE000u,
  ORV_r_p_z = SVEIntReductionLogicalFixed | 0x00180000u,
  EORV_r_p_z = SVEIntReductionLogicalFixed | 0x00190000u,
  ANDV_r_p_z = SVEIntReductionLogicalFixed | 0x001A0000u
};

enum SVEIntUnaryArithmeticPredicatedOp : uint32_t {
  SVEIntUnaryArithmeticPredicatedFixed = 0x0400A000u,
  SVEIntUnaryArithmeticPredicatedFMask = 0xFF20E000u,
  SVEIntUnaryArithmeticPredicatedMask = 0xFF3FE000u,
  SXTB_z_p_z = SVEIntUnaryArithmeticPredicatedFixed | 0x00100000u,
  UXTB_z_p_z = SVEIntUnaryArithmeticPredicatedFixed | 0x00110000u,
  SXTH_z_p_z = SVEIntUnaryArithmeticPredicatedFixed | 0x00120000u,
  UXTH_z_p_z = SVEIntUnaryArithmeticPredicatedFixed | 0x00130000u,
  SXTW_z_p_z = SVEIntUnaryArithmeticPredicatedFixed | 0x00140000u,
  UXTW_z_p_z = SVEIntUnaryArithmeticPredicatedFixed | 0x00150000u,
  ABS_z_p_z = SVEIntUnaryArithmeticPredicatedFixed | 0x00160000u,
  NEG_z_p_z = SVEIntUnaryArithmeticPredicatedFixed | 0x00170000u,
  CLS_z_p_z = SVEIntUnaryArithmeticPredicatedFixed | 0x00180000u,
  CLZ_z_p_z = SVEIntUnaryArithmeticPredicatedFixed | 0x00190000u,
  CNT_z_p_z = SVEIntUnaryArithmeticPredicatedFixed | 0x001A0000u,
  CNOT_z_p_z = SVEIntUnaryArithmeticPredicatedFixed | 0x001B0000u,
  FABS_z_p_z = SVEIntUnaryArithmeticPredicatedFixed | 0x001C0000u,
  FNEG_z_p_z = SVEIntUnaryArithmeticPredicatedFixed | 0x001D0000u,
  NOT_z_p_z = SVEIntUnaryArithmeticPredicatedFixed | 0x001E0000u
};

enum SVELoadAndBroadcastElementOp : uint32_t {
  SVELoadAndBroadcastElementFixed = 0x84408000u,
  SVELoadAndBroadcastElementFMask = 0xFE408000u,
  SVELoadAndBroadcastElementMask = 0xFFC0E000u,
  LD1RB_z_p_bi_u8 = SVELoadAndBroadcastElementFixed,
  LD1RB_z_p_bi_u16 = SVELoadAndBroadcastElementFixed | 0x00002000u,
  LD1RB_z_p_bi_u32 = SVELoadAndBroadcastElementFixed | 0x00004000u,
  LD1RB_z_p_bi_u64 = SVELoadAndBroadcastElementFixed | 0x00006000u,
  LD1RSW_z_p_bi_s64 = SVELoadAndBroadcastElementFixed | 0x00800000u,
  LD1RH_z_p_bi_u16 = SVELoadAndBroadcastElementFixed | 0x00802000u,
  LD1RH_z_p_bi_u32 = SVELoadAndBroadcastElementFixed | 0x00804000u,
  LD1RH_z_p_bi_u64 = SVELoadAndBroadcastElementFixed | 0x00806000u,
  LD1RSH_z_p_bi_s64 = SVELoadAndBroadcastElementFixed | 0x01000000u,
  LD1RSH_z_p_bi_s32 = SVELoadAndBroadcastElementFixed | 0x01002000u,
  LD1RW_z_p_bi_u32 = SVELoadAndBroadcastElementFixed | 0x01004000u,
  LD1RW_z_p_bi_u64 = SVELoadAndBroadcastElementFixed | 0x01006000u,
  LD1RSB_z_p_bi_s64 = SVELoadAndBroadcastElementFixed | 0x01800000u,
  LD1RSB_z_p_bi_s32 = SVELoadAndBroadcastElementFixed | 0x01802000u,
  LD1RSB_z_p_bi_s16 = SVELoadAndBroadcastElementFixed | 0x01804000u,
  LD1RD_z_p_bi_u64 = SVELoadAndBroadcastElementFixed | 0x01806000u
};

enum SVELoadAndBroadcastQuadword_ScalarPlusImmOp : uint32_t {
  SVELoadAndBroadcastQuadword_ScalarPlusImmFixed = 0xA4002000u,
  SVELoadAndBroadcastQuadword_ScalarPlusImmFMask = 0xFE10E000u,
  SVELoadAndBroadcastQuadword_ScalarPlusImmMask = 0xFFF0E000u,
  LD1RQB_z_p_bi_u8 = SVELoadAndBroadcastQuadword_ScalarPlusImmFixed,
  LD1RQH_z_p_bi_u16 = SVELoadAndBroadcastQuadword_ScalarPlusImmFixed | 0x00800000u,
  LD1RQW_z_p_bi_u32 = SVELoadAndBroadcastQuadword_ScalarPlusImmFixed | 0x01000000u,
  LD1RQD_z_p_bi_u64 = SVELoadAndBroadcastQuadword_ScalarPlusImmFixed | 0x01800000u
};

enum SVELoadAndBroadcastQuadword_ScalarPlusScalarOp : uint32_t {
  SVELoadAndBroadcastQuadword_ScalarPlusScalarFixed = 0xA4000000u,
  SVELoadAndBroadcastQuadword_ScalarPlusScalarFMask = 0xFE00E000u,
  SVELoadAndBroadcastQuadword_ScalarPlusScalarMask = 0xFFE0E000u,
  LD1RQB_z_p_br_contiguous = SVELoadAndBroadcastQuadword_ScalarPlusScalarFixed,
  LD1RQH_z_p_br_contiguous = SVELoadAndBroadcastQuadword_ScalarPlusScalarFixed | 0x00800000u,
  LD1RQW_z_p_br_contiguous = SVELoadAndBroadcastQuadword_ScalarPlusScalarFixed | 0x01000000u,
  LD1RQD_z_p_br_contiguous = SVELoadAndBroadcastQuadword_ScalarPlusScalarFixed | 0x01800000u
};

enum SVELoadMultipleStructures_ScalarPlusImmOp : uint32_t {
  SVELoadMultipleStructures_ScalarPlusImmFixed = 0xA400E000u,
  SVELoadMultipleStructures_ScalarPlusImmFMask = 0xFE10E000u,
  SVELoadMultipleStructures_ScalarPlusImmMask = 0xFFF0E000u,
  LD2B_z_p_bi_contiguous = SVELoadMultipleStructures_ScalarPlusImmFixed | 0x00200000u,
  LD3B_z_p_bi_contiguous = SVELoadMultipleStructures_ScalarPlusImmFixed | 0x00400000u,
  LD4B_z_p_bi_contiguous = SVELoadMultipleStructures_ScalarPlusImmFixed | 0x00600000u,
  LD2H_z_p_bi_contiguous = SVELoadMultipleStructures_ScalarPlusImmFixed | 0x00A00000u,
  LD3H_z_p_bi_contiguous = SVELoadMultipleStructures_ScalarPlusImmFixed | 0x00C00000u,
  LD4H_z_p_bi_contiguous = SVELoadMultipleStructures_ScalarPlusImmFixed | 0x00E00000u,
  LD2W_z_p_bi_contiguous = SVELoadMultipleStructures_ScalarPlusImmFixed | 0x01200000u,
  LD3W_z_p_bi_contiguous = SVELoadMultipleStructures_ScalarPlusImmFixed | 0x01400000u,
  LD4W_z_p_bi_contiguous = SVELoadMultipleStructures_ScalarPlusImmFixed | 0x01600000u,
  LD2D_z_p_bi_contiguous = SVELoadMultipleStructures_ScalarPlusImmFixed | 0x01A00000u,
  LD3D_z_p_bi_contiguous = SVELoadMultipleStructures_ScalarPlusImmFixed | 0x01C00000u,
  LD4D_z_p_bi_contiguous = SVELoadMultipleStructures_ScalarPlusImmFixed | 0x01E00000u
};

enum SVELoadMultipleStructures_ScalarPlusScalarOp : uint32_t {
  SVELoadMultipleStructures_ScalarPlusScalarFixed = 0xA400C000u,
  SVELoadMultipleStructures_ScalarPlusScalarFMask = 0xFE00E000u,
  SVELoadMultipleStructures_ScalarPlusScalarMask = 0xFFE0E000u,
  LD2B_z_p_br_contiguous = SVELoadMultipleStructures_ScalarPlusScalarFixed | 0x00200000u,
  LD3B_z_p_br_contiguous = SVELoadMultipleStructures_ScalarPlusScalarFixed | 0x00400000u,
  LD4B_z_p_br_contiguous = SVELoadMultipleStructures_ScalarPlusScalarFixed | 0x00600000u,
  LD2H_z_p_br_contiguous = SVELoadMultipleStructures_ScalarPlusScalarFixed | 0x00A00000u,
  LD3H_z_p_br_contiguous = SVELoadMultipleStructures_ScalarPlusScalarFixed | 0x00C00000u,
  LD4H_z_p_br_contiguous = SVELoadMultipleStructures_ScalarPlusScalarFixed | 0x00E00000u,
  LD2W_z_p_br_contiguous = SVELoadMultipleStructures_ScalarPlusScalarFixed | 0x01200000u,
  LD3W_z_p_br_contiguous = SVELoadMultipleStructures_ScalarPlusScalarFixed | 0x01400000u,
  LD4W_z_p_br_contiguous = SVELoadMultipleStructures_ScalarPlusScalarFixed | 0x01600000u,
  LD2D_z_p_br_contiguous = SVELoadMultipleStructures_ScalarPlusScalarFixed | 0x01A00000u,
  LD3D_z_p_br_contiguous = SVELoadMultipleStructures_ScalarPlusScalarFixed | 0x01C00000u,
  LD4D_z_p_br_contiguous = SVELoadMultipleStructures_ScalarPlusScalarFixed | 0x01E00000u
};

enum SVELoadPredicateRegisterOp : uint32_t {
  SVELoadPredicateRegisterFixed = 0x85800000u,
  SVELoadPredicateRegisterFMask = 0xFFC0E010u,
  SVELoadPredicateRegisterMask = 0xFFC0E010u,
  LDR_p_bi = SVELoadPredicateRegisterFixed
};

enum SVELoadVectorRegisterOp : uint32_t {
  SVELoadVectorRegisterFixed = 0x85804000u,
  SVELoadVectorRegisterFMask = 0xFFC0E000u,
  SVELoadVectorRegisterMask = 0xFFC0E000u,
  LDR_z_bi = SVELoadVectorRegisterFixed
};

enum SVEMulIndexOp : uint32_t {
  SVEMulIndexFixed = 0x44200000u,
  SVEMulIndexFMask = 0xFF200000u,
  SVEMulIndexMask = 0xFFE0FC00u,
  SDOT_z_zzzi_s = SVEMulIndexFixed | 0x00800000u,
  UDOT_z_zzzi_s = SVEMulIndexFixed | 0x00800400u,
  SDOT_z_zzzi_d = SVEMulIndexFixed | 0x00C00000u,
  UDOT_z_zzzi_d = SVEMulIndexFixed | 0x00C00400u
};

enum SVEPartitionBreakConditionOp : uint32_t {
  SVEPartitionBreakConditionFixed = 0x25104000u,
  SVEPartitionBreakConditionFMask = 0xFF3FC200u,
  SVEPartitionBreakConditionMask = 0xFFFFC200u,
  BRKA_p_p_p = SVEPartitionBreakConditionFixed,
  BRKAS_p_p_p_z = SVEPartitionBreakConditionFixed | 0x00400000u,
  BRKB_p_p_p = SVEPartitionBreakConditionFixed | 0x00800000u,
  BRKBS_p_p_p_z = SVEPartitionBreakConditionFixed | 0x00C00000u
};

enum SVEPermutePredicateElementsOp : uint32_t {
  SVEPermutePredicateElementsFixed = 0x05204000u,
  SVEPermutePredicateElementsFMask = 0xFF30E210u,
  SVEPermutePredicateElementsMask = 0xFF30FE10u,
  ZIP1_p_pp = SVEPermutePredicateElementsFixed,
  ZIP2_p_pp = SVEPermutePredicateElementsFixed | 0x00000400u,
  UZP1_p_pp = SVEPermutePredicateElementsFixed | 0x00000800u,
  UZP2_p_pp = SVEPermutePredicateElementsFixed | 0x00000C00u,
  TRN1_p_pp = SVEPermutePredicateElementsFixed | 0x00001000u,
  TRN2_p_pp = SVEPermutePredicateElementsFixed | 0x00001400u
};

enum SVEPermuteVectorExtractOp : uint32_t {
  SVEPermuteVectorExtractFixed = 0x05200000u,
  SVEPermuteVectorExtractFMask = 0xFF20E000u,
  SVEPermuteVectorExtractMask = 0xFFE0E000u,
  EXT_z_zi_des = SVEPermuteVectorExtractFixed
};

enum SVEPermuteVectorInterleavingOp : uint32_t {
  SVEPermuteVectorInterleavingFixed = 0x05206000u,
  SVEPermuteVectorInterleavingFMask = 0xFF20E000u,
  SVEPermuteVectorInterleavingMask = 0xFF20FC00u,
  ZIP1_z_zz = SVEPermuteVectorInterleavingFixed,
  ZIP2_z_zz = SVEPermuteVectorInterleavingFixed | 0x00000400u,
  UZP1_z_zz = SVEPermuteVectorInterleavingFixed | 0x00000800u,
  UZP2_z_zz = SVEPermuteVectorInterleavingFixed | 0x00000C00u,
  TRN1_z_zz = SVEPermuteVectorInterleavingFixed | 0x00001000u,
  TRN2_z_zz = SVEPermuteVectorInterleavingFixed | 0x00001400u
};

enum SVEPredicateCountOp : uint32_t {
  SVEPredicateCountFixed = 0x25208000u,
  SVEPredicateCountFMask = 0xFF38C000u,
  SVEPredicateCountMask = 0xFF3FC200u,
  CNTP_r_p_p = SVEPredicateCountFixed
};

enum SVEPredicateFirstActiveOp : uint32_t {
  SVEPredicateFirstActiveFixed = 0x2518C000u,
  SVEPredicateFirstActiveFMask = 0xFF3FFE10u,
  SVEPredicateFirstActiveMask = 0xFFFFFE10u,
  PFIRST_p_p_p = SVEPredicateFirstActiveFixed | 0x00400000u
};

enum SVEPredicateInitializeOp : uint32_t {
  SVEPredicateInitializeFixed = 0x2518E000u,
  SVEPredicateInitializeFMask = 0xFF3EFC10u,
  SVEPredicateInitializeMask = 0xFF3FFC10u,
  SVEPredicateInitializeSetFlagsBit = 0x00010000u,
  PTRUE_p_s = SVEPredicateInitializeFixed | 0x00000000u,
  PTRUES_p_s = SVEPredicateInitializeFixed | SVEPredicateInitializeSetFlagsBit
};

enum SVEPredicateLogicalOp : uint32_t {
  SVEPredicateLogicalFixed = 0x25004000u,
  SVEPredicateLogicalFMask = 0xFF30C000u,
  SVEPredicateLogicalMask = 0xFFF0C210u,
  SVEPredicateLogicalSetFlagsBit = 0x00400000u,
  AND_p_p_pp_z = SVEPredicateLogicalFixed,
  ANDS_p_p_pp_z = AND_p_p_pp_z | SVEPredicateLogicalSetFlagsBit,
  BIC_p_p_pp_z = SVEPredicateLogicalFixed | 0x00000010u,
  BICS_p_p_pp_z = BIC_p_p_pp_z | SVEPredicateLogicalSetFlagsBit,
  EOR_p_p_pp_z = SVEPredicateLogicalFixed | 0x00000200u,
  EORS_p_p_pp_z = EOR_p_p_pp_z | SVEPredicateLogicalSetFlagsBit,
  ORR_p_p_pp_z = SVEPredicateLogicalFixed | 0x00800000u,
  ORRS_p_p_pp_z = ORR_p_p_pp_z | SVEPredicateLogicalSetFlagsBit,
  ORN_p_p_pp_z = SVEPredicateLogicalFixed | 0x00800010u,
  ORNS_p_p_pp_z = ORN_p_p_pp_z | SVEPredicateLogicalSetFlagsBit,
  NAND_p_p_pp_z = SVEPredicateLogicalFixed | 0x00800210u,
  NANDS_p_p_pp_z = NAND_p_p_pp_z | SVEPredicateLogicalSetFlagsBit,
  NOR_p_p_pp_z = SVEPredicateLogicalFixed | 0x00800200u,
  NORS_p_p_pp_z = NOR_p_p_pp_z | SVEPredicateLogicalSetFlagsBit,
  SEL_p_p_pp = SVEPredicateLogicalFixed | 0x00000210u
};

enum SVEPredicateNextActiveOp : uint32_t {
  SVEPredicateNextActiveFixed = 0x2519C400u,
  SVEPredicateNextActiveFMask = 0xFF3FFE10u,
  SVEPredicateNextActiveMask = 0xFF3FFE10u,
  PNEXT_p_p_p = SVEPredicateNextActiveFixed
};

enum SVEPredicateReadFromFFR_PredicatedOp : uint32_t {
  SVEPredicateReadFromFFR_PredicatedFixed = 0x2518F000u,
  SVEPredicateReadFromFFR_PredicatedFMask = 0xFF3FFE10u,
  SVEPredicateReadFromFFR_PredicatedMask = 0xFFFFFE10u,
  RDFFR_p_p_f = SVEPredicateReadFromFFR_PredicatedFixed,
  RDFFRS_p_p_f = SVEPredicateReadFromFFR_PredicatedFixed | 0x00400000u
};

enum SVEPredicateReadFromFFR_UnpredicatedOp : uint32_t {
  SVEPredicateReadFromFFR_UnpredicatedFixed = 0x2519F000u,
  SVEPredicateReadFromFFR_UnpredicatedFMask = 0xFF3FFFF0u,
  SVEPredicateReadFromFFR_UnpredicatedMask = 0xFFFFFFF0u,
  RDFFR_p_f = SVEPredicateReadFromFFR_UnpredicatedFixed
};

enum SVEPredicateTestOp : uint32_t {
  SVEPredicateTestFixed = 0x2510C000u,
  SVEPredicateTestFMask = 0xFF3FC210u,
  SVEPredicateTestMask = 0xFFFFC21Fu,
  PTEST_p_p = SVEPredicateTestFixed | 0x00400000u
};

enum SVEPredicateZeroOp : uint32_t {
  SVEPredicateZeroFixed = 0x2518E400u,
  SVEPredicateZeroFMask = 0xFF3FFFF0u,
  SVEPredicateZeroMask = 0xFFFFFFF0u,
  PFALSE_p = SVEPredicateZeroFixed
};

enum SVEPropagateBreakOp : uint32_t {
  SVEPropagateBreakFixed = 0x2500C000u,
  SVEPropagateBreakFMask = 0xFF30C000u,
  SVEPropagateBreakMask = 0xFFF0C210u,
  BRKPA_p_p_pp = SVEPropagateBreakFixed,
  BRKPB_p_p_pp = SVEPropagateBreakFixed | 0x00000010u,
  BRKPAS_p_p_pp = SVEPropagateBreakFixed | 0x00400000u,
  BRKPBS_p_p_pp = SVEPropagateBreakFixed | 0x00400010u
};

enum SVEPropagateBreakToNextPartitionOp : uint32_t {
  SVEPropagateBreakToNextPartitionFixed = 0x25184000u,
  SVEPropagateBreakToNextPartitionFMask = 0xFFBFC210u,
  SVEPropagateBreakToNextPartitionMask = 0xFFFFC210u,
  BRKN_p_p_pp = SVEPropagateBreakToNextPartitionFixed,
  BRKNS_p_p_pp = SVEPropagateBreakToNextPartitionFixed | 0x00400000u
};

enum SVEReversePredicateElementsOp : uint32_t {
  SVEReversePredicateElementsFixed = 0x05344000u,
  SVEReversePredicateElementsFMask = 0xFF3FFE10u,
  SVEReversePredicateElementsMask = 0xFF3FFE10u,
  REV_p_p = SVEReversePredicateElementsFixed
};

enum SVEReverseVectorElementsOp : uint32_t {
  SVEReverseVectorElementsFixed = 0x05383800u,
  SVEReverseVectorElementsFMask = 0xFF3FFC00u,
  SVEReverseVectorElementsMask = 0xFF3FFC00u,
  REV_z_z = SVEReverseVectorElementsFixed
};

enum SVEReverseWithinElementsOp : uint32_t {
  SVEReverseWithinElementsFixed = 0x05248000u,
  SVEReverseWithinElementsFMask = 0xFF3CE000u,
  SVEReverseWithinElementsMask = 0xFF3FE000u,
  REVB_z_z = SVEReverseWithinElementsFixed,
  REVH_z_z = SVEReverseWithinElementsFixed | 0x00010000u,
  REVW_z_z = SVEReverseWithinElementsFixed | 0x00020000u,
  RBIT_z_p_z = SVEReverseWithinElementsFixed | 0x00030000u
};

enum SVESaturatingIncDecRegisterByElementCountOp : uint32_t {
  SVESaturatingIncDecRegisterByElementCountFixed = 0x0420F000u,
  SVESaturatingIncDecRegisterByElementCountFMask = 0xFF20F000u,
  SVESaturatingIncDecRegisterByElementCountMask = 0xFFF0FC00u,
  SQINCB_r_rs_sx = SVESaturatingIncDecRegisterByElementCountFixed,
  UQINCB_r_rs_uw = SVESaturatingIncDecRegisterByElementCountFixed | 0x00000400u,
  SQDECB_r_rs_sx = SVESaturatingIncDecRegisterByElementCountFixed | 0x00000800u,
  UQDECB_r_rs_uw = SVESaturatingIncDecRegisterByElementCountFixed | 0x00000C00u,
  SQINCB_r_rs_x = SVESaturatingIncDecRegisterByElementCountFixed | 0x00100000u,
  UQINCB_r_rs_x = SVESaturatingIncDecRegisterByElementCountFixed | 0x00100400u,
  SQDECB_r_rs_x = SVESaturatingIncDecRegisterByElementCountFixed | 0x00100800u,
  UQDECB_r_rs_x = SVESaturatingIncDecRegisterByElementCountFixed | 0x00100C00u,
  SQINCH_r_rs_sx = SVESaturatingIncDecRegisterByElementCountFixed | 0x00400000u,
  UQINCH_r_rs_uw = SVESaturatingIncDecRegisterByElementCountFixed | 0x00400400u,
  SQDECH_r_rs_sx = SVESaturatingIncDecRegisterByElementCountFixed | 0x00400800u,
  UQDECH_r_rs_uw = SVESaturatingIncDecRegisterByElementCountFixed | 0x00400C00u,
  SQINCH_r_rs_x = SVESaturatingIncDecRegisterByElementCountFixed | 0x00500000u,
  UQINCH_r_rs_x = SVESaturatingIncDecRegisterByElementCountFixed | 0x00500400u,
  SQDECH_r_rs_x = SVESaturatingIncDecRegisterByElementCountFixed | 0x00500800u,
  UQDECH_r_rs_x = SVESaturatingIncDecRegisterByElementCountFixed | 0x00500C00u,
  SQINCW_r_rs_sx = SVESaturatingIncDecRegisterByElementCountFixed | 0x00800000u,
  UQINCW_r_rs_uw = SVESaturatingIncDecRegisterByElementCountFixed | 0x00800400u,
  SQDECW_r_rs_sx = SVESaturatingIncDecRegisterByElementCountFixed | 0x00800800u,
  UQDECW_r_rs_uw = SVESaturatingIncDecRegisterByElementCountFixed | 0x00800C00u,
  SQINCW_r_rs_x = SVESaturatingIncDecRegisterByElementCountFixed | 0x00900000u,
  UQINCW_r_rs_x = SVESaturatingIncDecRegisterByElementCountFixed | 0x00900400u,
  SQDECW_r_rs_x = SVESaturatingIncDecRegisterByElementCountFixed | 0x00900800u,
  UQDECW_r_rs_x = SVESaturatingIncDecRegisterByElementCountFixed | 0x00900C00u,
  SQINCD_r_rs_sx = SVESaturatingIncDecRegisterByElementCountFixed | 0x00C00000u,
  UQINCD_r_rs_uw = SVESaturatingIncDecRegisterByElementCountFixed | 0x00C00400u,
  SQDECD_r_rs_sx = SVESaturatingIncDecRegisterByElementCountFixed | 0x00C00800u,
  UQDECD_r_rs_uw = SVESaturatingIncDecRegisterByElementCountFixed | 0x00C00C00u,
  SQINCD_r_rs_x = SVESaturatingIncDecRegisterByElementCountFixed | 0x00D00000u,
  UQINCD_r_rs_x = SVESaturatingIncDecRegisterByElementCountFixed | 0x00D00400u,
  SQDECD_r_rs_x = SVESaturatingIncDecRegisterByElementCountFixed | 0x00D00800u,
  UQDECD_r_rs_x = SVESaturatingIncDecRegisterByElementCountFixed | 0x00D00C00u
};

enum SVESaturatingIncDecVectorByElementCountOp : uint32_t {
  SVESaturatingIncDecVectorByElementCountFixed = 0x0420C000u,
  SVESaturatingIncDecVectorByElementCountFMask = 0xFF30F000u,
  SVESaturatingIncDecVectorByElementCountMask = 0xFFF0FC00u,
  SQINCH_z_zs = SVESaturatingIncDecVectorByElementCountFixed | 0x00400000u,
  UQINCH_z_zs = SVESaturatingIncDecVectorByElementCountFixed | 0x00400400u,
  SQDECH_z_zs = SVESaturatingIncDecVectorByElementCountFixed | 0x00400800u,
  UQDECH_z_zs = SVESaturatingIncDecVectorByElementCountFixed | 0x00400C00u,
  SQINCW_z_zs = SVESaturatingIncDecVectorByElementCountFixed | 0x00800000u,
  UQINCW_z_zs = SVESaturatingIncDecVectorByElementCountFixed | 0x00800400u,
  SQDECW_z_zs = SVESaturatingIncDecVectorByElementCountFixed | 0x00800800u,
  UQDECW_z_zs = SVESaturatingIncDecVectorByElementCountFixed | 0x00800C00u,
  SQINCD_z_zs = SVESaturatingIncDecVectorByElementCountFixed | 0x00C00000u,
  UQINCD_z_zs = SVESaturatingIncDecVectorByElementCountFixed | 0x00C00400u,
  SQDECD_z_zs = SVESaturatingIncDecVectorByElementCountFixed | 0x00C00800u,
  UQDECD_z_zs = SVESaturatingIncDecVectorByElementCountFixed | 0x00C00C00u
};

enum SVEStackFrameAdjustmentOp {
  SVEStackFrameAdjustmentFixed = 0x04205000u,
  SVEStackFrameAdjustmentFMask = 0xFFA0F800u,
  SVEStackFrameAdjustmentMask = 0xFFE0F800u,
  ADDVL_r_ri = SVEStackFrameAdjustmentFixed,
  ADDPL_r_ri = SVEStackFrameAdjustmentFixed | 0x00400000u
};

enum SVEStackFrameSizeOp : uint32_t {
  SVEStackFrameSizeFixed = 0x04BF5000u,
  SVEStackFrameSizeFMask = 0xFFFFF800u,
  SVEStackFrameSizeMask = 0xFFFFF800u,
  RDVL_r_i = SVEStackFrameSizeFixed
};

enum SVEStoreMultipleStructures_ScalarPlusImmOp : uint32_t {
  SVEStoreMultipleStructures_ScalarPlusImmFixed = 0xE410E000u,
  SVEStoreMultipleStructures_ScalarPlusImmFMask = 0xFE10E000u,
  SVEStoreMultipleStructures_ScalarPlusImmMask = 0xFFF0E000u,
  ST2B_z_p_bi_contiguous = SVEStoreMultipleStructures_ScalarPlusImmFixed | 0x00200000u,
  ST3B_z_p_bi_contiguous = SVEStoreMultipleStructures_ScalarPlusImmFixed | 0x00400000u,
  ST4B_z_p_bi_contiguous = SVEStoreMultipleStructures_ScalarPlusImmFixed | 0x00600000u,
  ST2H_z_p_bi_contiguous = SVEStoreMultipleStructures_ScalarPlusImmFixed | 0x00A00000u,
  ST3H_z_p_bi_contiguous = SVEStoreMultipleStructures_ScalarPlusImmFixed | 0x00C00000u,
  ST4H_z_p_bi_contiguous = SVEStoreMultipleStructures_ScalarPlusImmFixed | 0x00E00000u,
  ST2W_z_p_bi_contiguous = SVEStoreMultipleStructures_ScalarPlusImmFixed | 0x01200000u,
  ST3W_z_p_bi_contiguous = SVEStoreMultipleStructures_ScalarPlusImmFixed | 0x01400000u,
  ST4W_z_p_bi_contiguous = SVEStoreMultipleStructures_ScalarPlusImmFixed | 0x01600000u,
  ST2D_z_p_bi_contiguous = SVEStoreMultipleStructures_ScalarPlusImmFixed | 0x01A00000u,
  ST3D_z_p_bi_contiguous = SVEStoreMultipleStructures_ScalarPlusImmFixed | 0x01C00000u,
  ST4D_z_p_bi_contiguous = SVEStoreMultipleStructures_ScalarPlusImmFixed | 0x01E00000u
};

enum SVEStoreMultipleStructures_ScalarPlusScalarOp : uint32_t {
  SVEStoreMultipleStructures_ScalarPlusScalarFixed = 0xE4006000u,
  SVEStoreMultipleStructures_ScalarPlusScalarFMask = 0xFE00E000u,
  SVEStoreMultipleStructures_ScalarPlusScalarMask = 0xFFE0E000u,
  ST2B_z_p_br_contiguous = SVEStoreMultipleStructures_ScalarPlusScalarFixed | 0x00200000u,
  ST3B_z_p_br_contiguous = SVEStoreMultipleStructures_ScalarPlusScalarFixed | 0x00400000u,
  ST4B_z_p_br_contiguous = SVEStoreMultipleStructures_ScalarPlusScalarFixed | 0x00600000u,
  ST2H_z_p_br_contiguous = SVEStoreMultipleStructures_ScalarPlusScalarFixed | 0x00A00000u,
  ST3H_z_p_br_contiguous = SVEStoreMultipleStructures_ScalarPlusScalarFixed | 0x00C00000u,
  ST4H_z_p_br_contiguous = SVEStoreMultipleStructures_ScalarPlusScalarFixed | 0x00E00000u,
  ST2W_z_p_br_contiguous = SVEStoreMultipleStructures_ScalarPlusScalarFixed | 0x01200000u,
  ST3W_z_p_br_contiguous = SVEStoreMultipleStructures_ScalarPlusScalarFixed | 0x01400000u,
  ST4W_z_p_br_contiguous = SVEStoreMultipleStructures_ScalarPlusScalarFixed | 0x01600000u,
  ST2D_z_p_br_contiguous = SVEStoreMultipleStructures_ScalarPlusScalarFixed | 0x01A00000u,
  ST3D_z_p_br_contiguous = SVEStoreMultipleStructures_ScalarPlusScalarFixed | 0x01C00000u,
  ST4D_z_p_br_contiguous = SVEStoreMultipleStructures_ScalarPlusScalarFixed | 0x01E00000u
};

enum SVEStorePredicateRegisterOp : uint32_t {
  SVEStorePredicateRegisterFixed = 0xE5800000u,
  SVEStorePredicateRegisterFMask = 0xFFC0E010u,
  SVEStorePredicateRegisterMask = 0xFFC0E010u,
  STR_p_bi = SVEStorePredicateRegisterFixed
};

enum SVEStoreVectorRegisterOp : uint32_t {
  SVEStoreVectorRegisterFixed = 0xE5804000u,
  SVEStoreVectorRegisterFMask = 0xFFC0E000u,
  SVEStoreVectorRegisterMask = 0xFFC0E000u,
  STR_z_bi = SVEStoreVectorRegisterFixed
};

enum SVETableLookupOp : uint32_t {
  SVETableLookupFixed = 0x05203000u,
  SVETableLookupFMask = 0xFF20FC00u,
  SVETableLookupMask = 0xFF20FC00u,
  TBL_z_zz_1 = SVETableLookupFixed
};

enum SVEUnpackPredicateElementsOp : uint32_t {
  SVEUnpackPredicateElementsFixed = 0x05304000u,
  SVEUnpackPredicateElementsFMask = 0xFFFEFE10u,
  SVEUnpackPredicateElementsMask = 0xFFFFFE10u,
  PUNPKLO_p_p = SVEUnpackPredicateElementsFixed,
  PUNPKHI_p_p = SVEUnpackPredicateElementsFixed | 0x00010000u
};

enum SVEUnpackVectorElementsOp : uint32_t {
  SVEUnpackVectorElementsFixed = 0x05303800u,
  SVEUnpackVectorElementsFMask = 0xFF3CFC00u,
  SVEUnpackVectorElementsMask = 0xFF3FFC00u,
  SUNPKLO_z_z = SVEUnpackVectorElementsFixed,
  SUNPKHI_z_z = SVEUnpackVectorElementsFixed | 0x00010000u,
  UUNPKLO_z_z = SVEUnpackVectorElementsFixed | 0x00020000u,
  UUNPKHI_z_z = SVEUnpackVectorElementsFixed | 0x00030000u
};

enum SVEVectorSelectOp : uint32_t {
  SVEVectorSelectFixed = 0x0520C000u,
  SVEVectorSelectFMask = 0xFF20C000u,
  SVEVectorSelectMask = 0xFF20C000u,
  SEL_z_p_zz = SVEVectorSelectFixed
};

enum SVEVectorSpliceOp : uint32_t {
  SVEVectorSpliceFixed = 0x052C8000u,
  SVEVectorSpliceFMask = 0xFF3FE000u,
  SVEVectorSpliceMask = 0xFF3FE000u,
  SPLICE_z_p_zz_des = SVEVectorSpliceFixed
};

enum ReservedOp : uint32_t {
  ReservedFixed = 0x00000000u,
  ReservedFMask = 0x1E000000u,
  ReservedMask = 0xFFFF0000u,
  UDF = ReservedFixed | 0x00000000u
};

// Unimplemented and unallocated instructions. These are defined to make fixed
// bit assertion easier.
enum UnimplementedOp : uint32_t {
  UnimplementedFixed = 0x00000000u,
  UnimplementedFMask = 0x00000000u
};

enum UnallocatedOp : uint32_t {
  UnallocatedFixed = 0x00000000u,
  UnallocatedFMask = 0x00000000u
};

// Re-enable `clang-format` after the `enum`s.
// clang-format on

}  // namespace aarch64
}  // namespace vixl

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#endif  // VIXL_AARCH64_CONSTANTS_AARCH64_H_
