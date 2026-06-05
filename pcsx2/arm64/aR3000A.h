// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

// ARM64 IOP (R3000A) recompiler — public interface + register-allocation contract.
//
// ARM64 counterpart to pcsx2/x86/iR3000A.cpp. The structure mirrors the ARM64 EE
// recompiler (pcsx2/arm64/aR5900.cpp): a recLUT two-level page table maps guest PC
// -> host block, emitted dispatcher stubs chain blocks in host code, and any opcode
// the rec cannot (yet) compile falls back to the interpreter one instruction at a
// time via iopExecuteOneInst(). The R3000A is plain 32-bit MIPS-I, so it is a strict
// subset of the EE work: GPRs are 32-bit, there is no 128-bit/FPU state, and memory
// access goes through the iopMemRead/Write helpers (no vtlb fastmem).

#include "R3000A.h"

#include "arm64/AsmHelpers.h"

#include <cstddef>

// --------------------------------------------------------------------------------------
//  Persistent (callee-saved) host register for hot IOP state
// --------------------------------------------------------------------------------------
// x19 holds &psxRegs for the duration of a dispatch (re-pinned at the dispatcher
// funnel, like the EE rec, because the C++ callees we re-enter through don't preserve
// it). The IOP needs no fastmem/vtlb base registers — its loads/stores call the
// iopMem* helpers directly — so unlike the EE we only reserve this one.
#ifndef RESTATEPTR
#define RESTATEPTR vixl::aarch64::x19
#endif

// --------------------------------------------------------------------------------------
//  psxRegs field offsets used by the generators / dispatch tail
// --------------------------------------------------------------------------------------
// GPR is the first member of psxRegisters and GPRRegs is a u32[34] union, so guest
// GPR `n`'s offset is just n*4 (hi == r[32], lo == r[33]).
static constexpr u32 IOP_GPR_OFFSET(u32 n) { return n * 4u; }
static constexpr u32 IOP_HI_OFFSET = IOP_GPR_OFFSET(32);
static constexpr u32 IOP_LO_OFFSET = IOP_GPR_OFFSET(33);

static constexpr u32 IOP_PC_OFFSET = static_cast<u32>(offsetof(psxRegisters, pc));
static constexpr u32 IOP_CODE_OFFSET = static_cast<u32>(offsetof(psxRegisters, code));
static constexpr u32 IOP_CYCLE_OFFSET = static_cast<u32>(offsetof(psxRegisters, cycle));
static constexpr u32 IOP_NEXTEVENTCYCLE_OFFSET = static_cast<u32>(offsetof(psxRegisters, iopNextEventCycle));
static constexpr u32 IOP_CYCLEEE_OFFSET = static_cast<u32>(offsetof(psxRegisters, iopCycleEE));

// --------------------------------------------------------------------------------------
//  Interpreter single-step fallback (defined in R3000AInterpreter.cpp)
// --------------------------------------------------------------------------------------
// Interpret exactly one IOP instruction at psxRegs.pc (handling its own PC, delay slot
// and cycle++), then return — the rec's per-opcode fallback. Must NOT exit the
// timeslice itself; the rec block tail drives iopCycleEE / event tests.
extern void iopExecuteOneInst();
