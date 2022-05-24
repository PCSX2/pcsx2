/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2020 PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

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
