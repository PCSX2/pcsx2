// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
#pragma once

#include "common/Pcsx2Defs.h"

// Test helper API for microVU0 / microVU1 JIT tests.
// Implementations live in microVU.cpp so they can access internal types
// without causing duplicate-symbol errors from the .inl includes.

/// (Re-)initialise microVU0 and allocate the JIT cache.  Safe to call
/// repeatedly; performs a close before each init.
void mVU0_TestInit();

/// Release the microVU0 JIT cache.
void mVU0_TestShutdown();

/// Copy `count` u32 words into VU0.Micro and invalidate the JIT program
/// cache so the next Execute call recompiles the new program.
void mVU0_TestWriteProg(const u32* words, u32 count);

/// Execute the microVU0 JIT from byte offset `startPC` (pass 0 for the
/// start of the program) for up to `cycles` cycles.
void mVU0_TestExec(u32 startPC, u32 cycles);

/// (Re-)initialise microVU1 and allocate the JIT cache.  Safe to call
/// repeatedly; performs a close before each init.
void mVU1_TestInit();

/// Release the microVU1 JIT cache.
void mVU1_TestShutdown();

/// Copy `count` u32 words into VU1.Micro and invalidate the JIT program
/// cache so the next Execute call recompiles the new program.
void mVU1_TestWriteProg(const u32* words, u32 count);

/// Execute the microVU1 JIT from byte offset `startPC` (pass 0 for the
/// start of the program) for up to `cycles` cycles.
void mVU1_TestExec(u32 startPC, u32 cycles);
