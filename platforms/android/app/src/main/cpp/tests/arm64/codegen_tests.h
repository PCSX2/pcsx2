// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"
#include "vixl/aarch64/macro-assembler-aarch64.h"

void runArmCodegenTest(
	void (*exec)(vixl::aarch64::MacroAssembler& armAsm),
	const char* description,
	const char* expected);

// Lambda receives a MacroAssembler& named armAsm — matches production call sites
// (production uses armAsm->Xxx, test uses armAsm.Xxx — identical operands).
#define ARM_CODEGEN_TEST(command, expected) \
	runArmCodegenTest( \
		[](vixl::aarch64::MacroAssembler& armAsm) { \
			using namespace vixl::aarch64; \
			command; \
		}, \
		#command, \
		expected)
