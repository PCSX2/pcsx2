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

#include <gtest/gtest.h>
#include <x86emitter.h>
#include <cstdio>

struct CodegenTest {
	void (*exec)(void *base);
	const char* description;
	const char* expected;
};

extern thread_local const char *currentTest;

using namespace x86Emitter;

// Use null to skip, empty string to expect no output
#ifdef __M_X86_64
#define CODEGEN_TEST(command, expected32, expected64) (CodegenTest){ [](void *base){ command; }, #command, expected64 }
#define CODEGEN_TEST_64(command, expected) CODEGEN_TEST(command, nullptr, expected)
#define CODEGEN_TEST_32(command, expected) CODEGEN_TEST(return, expected, nullptr)
#else
#define CODEGEN_TEST(command, expected32, expected64) (CodegenTest){ [](void *base){ command; }, #command, expected32 }
#define CODEGEN_TEST_64(command, expected) CODEGEN_TEST(return, nullptr, expected)
#define CODEGEN_TEST_32(command, expected) CODEGEN_TEST(command, expected, nullptr)
#endif
#define CODEGEN_TEST_BOTH(command, expected) CODEGEN_TEST(command, expected, expected)

template<std::size_t Count>
void runCodegenTests(const CodegenTest(&tests)[Count]) {
	u8 code[4096];
	memset(code, 0xcc, sizeof(code));
	char str[4096] = {0};

	for (const auto& test : tests) {
		if (!test.expected) continue;
		currentTest = test.description;
		xSetPtr(code);
		test.exec(code);
		char *strPtr = str;
		for (u8* ptr = code; ptr < xGetPtr(); ptr++) {
			sprintf(strPtr, "%02x ", *ptr);
			strPtr += 3;
		}
		if (strPtr != str) {
			// Remove final space
			*--strPtr = '\0';
		}
		EXPECT_STRCASEEQ(test.expected, str) << "Unexpected codegen from " << test.description;
	}
}
