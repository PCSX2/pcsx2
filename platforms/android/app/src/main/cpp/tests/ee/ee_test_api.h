// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+
#pragma once

#include "common/Pcsx2Defs.h"

// Test helper API for the R5900 (EE) interpreter.
// Implementations live in Interpreter.cpp so they can access internal state
// without exposing static symbols.

/// Virtual address in EE main RAM where test programs are loaded.
static constexpr u32 EE_TEST_PC   = 0x00100000u;
/// Virtual address used as a scratch data region by load/store tests.
static constexpr u32 EE_TEST_DATA = 0x00200000u;

/// Initialise EE for testing.  Allocates SysMemory and sets up VTLB if not
/// already done.  Safe to call once before a batch of tests.
void EE_TestInit();

/// Called after all tests complete; clears test-mode flag.
void EE_TestShutdown();

/// Write `count` u32 words into EE main RAM at EE_TEST_PC.
void EE_TestWriteProg(const u32* words, u32 count);

/// Execute up to maxInstrs instructions starting at EE_TEST_PC.
/// Stops early when a halt instruction (BEQ $0,$0,-1 = 0x1000FFFF) is reached.
void EE_TestExec(u32 maxInstrs = 100000u);
