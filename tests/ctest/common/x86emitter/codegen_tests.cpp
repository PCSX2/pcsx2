// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "codegen_tests.h"
#include <gtest/gtest.h>
#include <common/emitter/x86emitter.h>
#include <common/Assertions.h>

using namespace x86Emitter;

thread_local const char *currentTest;

void runCodegenTest(void (*exec)(void *base), const char* description, const char* expected) {
	u8 code[4096];
	memset(code, 0xcc, sizeof(code));
	char str[4096] = {0};

	if (!expected) return;
	currentTest = description;
	xSetPtr(code);
	exec(code);
	char *strPtr = str;
	for (u8* ptr = code; ptr < xGetPtr(); ptr++) {
		sprintf(strPtr, "%02x ", *ptr);
		strPtr += 3;
	}
	if (strPtr != str) {
		// Remove final space
		*--strPtr = '\0';
	}
	EXPECT_STRCASEEQ(expected, str) << "Unexpected codegen from " << description;
}
