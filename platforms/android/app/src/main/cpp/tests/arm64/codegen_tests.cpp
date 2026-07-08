// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "codegen_tests.h"
#include <gtest/gtest.h>
#include <cstdio>
#include <cstring>

using namespace vixl::aarch64;

thread_local const char* currentArmTest;

// Plain heap-allocated buffer — no executable permissions needed.
// We write ARM64 instructions into it and inspect the bytes; we never execute them.
void runArmCodegenTest(
	void (*exec)(MacroAssembler& armAsm),
	const char* description,
	const char* expected)
{
	if (!expected)
		return;

	currentArmTest = description;

	// 4096 bytes is enough for any single-instruction or short multi-instruction test.
	// Static to avoid blowing the stack; protected by gtest's serial test execution.
	static vixl::byte code[4096];
	memset(code, 0xd4, sizeof(code)); // 0xd4200000 prefix == BRK; helps spot runaway code

	MacroAssembler masm(code, sizeof(code));
	exec(masm);
	masm.FinalizeCode();

	const size_t size = masm.GetSizeOfCodeGenerated();

	// Format emitted bytes as lowercase hex separated by spaces ("e0 03 01 aa")
	char str[4096 * 3] = {};
	char* p = str;
	const auto* bytes = reinterpret_cast<const uint8_t*>(code);
	for (size_t i = 0; i < size; i++)
	{
		sprintf(p, "%02x ", bytes[i]);
		p += 3;
	}
	if (p != str)
		*--p = '\0'; // strip trailing space

	EXPECT_STRCASEEQ(expected, str) << "Unexpected codegen from " << description;
}
