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

TEST(CodegenTests, MOVTest)
{
	runCodegenTests({
		CODEGEN_TEST_BOTH(xMOV(rax, 0), "31 c0"),
		CODEGEN_TEST_64(xMOV(rax, rcx), "48 89 c8"),
		CODEGEN_TEST_32(xMOV(eax, ecx), "89 c8"),
		CODEGEN_TEST_64(xMOV(r8, 0), "45 31 c0"),
		CODEGEN_TEST_64(xMOV(rax, r8), "4c 89 c0"),
		CODEGEN_TEST_64(xMOV(r8, rax), "49 89 c0"),
		CODEGEN_TEST_64(xMOV(r8, r9), "4d 89 c8"),
		CODEGEN_TEST_64(xMOV(rax, ptrNative[rcx]), "48 8b 01"),
		CODEGEN_TEST_32(xMOV(eax, ptrNative[rcx]), "8b 01"),
		CODEGEN_TEST_64(xMOV(ptrNative[rax], rcx), "48 89 08"),
		CODEGEN_TEST_32(xMOV(ptrNative[rax], ecx), "89 08"),
		CODEGEN_TEST_64(xMOV(rax, ptrNative[r8]), "49 8b 00"),
		CODEGEN_TEST_64(xMOV(ptrNative[r8], rax), "4c 8b 00"),
		CODEGEN_TEST_64(xMOV(r8, ptrNative[r9]), "4d 8b 01"),
		CODEGEN_TEST_64(xMOV(ptrNative[r8], r9), "4d 89 08"),
		CODEGEN_TEST_64(xMOV(rax, ptrNative[rbx*4+3+rcx]), "48 8b 44 99 03"),
		CODEGEN_TEST_64(xMOV(ptrNative[rbx*4+3+rax], rcx), "48 89 4c 98 03"),
		CODEGEN_TEST_32(xMOV(eax, ptrNative[rbx*4+3+rcx]), "8b 44 99 03"),
		CODEGEN_TEST_32(xMOV(ptrNative[rbx*4+3+rax], ecx), "89 4c 98 03"),
		CODEGEN_TEST_64(xMOV(r8, ptrNative[r10*4+3+r9]), "4f 8b 44 91 03"),
		CODEGEN_TEST_64(xMOV(ptrNative[r9*4+3+r8], r10), "4f 89 54 88 03"),
		CODEGEN_TEST_64(xMOV64(rax, 0x1234567890), "48 b8 90 78 56 34 12 00 00 00"),
		CODEGEN_TEST_64(xMOV64(r8, 0x1234567890), "49 b8 90 78 56 34 12 00 00 00"),
	});
}

TEST(CodegenTests, PUSHTest)
{
	runCodegenTests({
		CODEGEN_TEST_BOTH(xPUSH(rax), "50"),
		CODEGEN_TEST_64(xPUSH(r8), "41 50"),
		CODEGEN_TEST_BOTH(xPUSH(0x1234), "68 34 12 00 00"),
		CODEGEN_TEST_BOTH(xPUSH(0x12), "68 12 00 00 00"), // TODO: Maybe we should generate "6a 12"
		CODEGEN_TEST_BOTH(xPUSH(ptrNative[rax]), "ff 30"),
		CODEGEN_TEST_64(xPUSH(ptrNative[r8]), "41 ff 30"),
		CODEGEN_TEST_BOTH(xPUSH(ptrNative[rax*2+3+rbx]), "ff 74 43 03"),
		CODEGEN_TEST_64(xPUSH(ptrNative[rax*2+3+r8]), "41 ff 74 40 03"),
		CODEGEN_TEST_64(xPUSH(ptrNative[r9*4+3+r8]), "43 ff 74 88 03"),
		CODEGEN_TEST_64(xPUSH(ptrNative[r8*4+3+rax]), "42 ff 74 80 03"),
		CODEGEN_TEST_BOTH(xPUSH(ptrNative[rax*8+0x1234+rbx]), "ff b4 c3 34 12 00 00"),
	});
}

TEST(CodegenTests, POPTest)
{
	runCodegenTests({
		CODEGEN_TEST_BOTH(xPOP(rax), "58"),
		CODEGEN_TEST_64(xPOP(r8), "41 58"),
		CODEGEN_TEST_BOTH(xPOP(ptrNative[rax]), "8f 00"),
		CODEGEN_TEST_64(xPOP(ptrNative[r8]), "41 8f 00"),
		CODEGEN_TEST_BOTH(xPOP(ptrNative[rax*2+3+rbx]), "8f 44 43 03"),
		CODEGEN_TEST_64(xPOP(ptrNative[rax*2+3+r8]), "41 8f 44 40 03"),
		CODEGEN_TEST_64(xPOP(ptrNative[r9*4+3+r8]), "43 8f 44 88 03"),
		CODEGEN_TEST_64(xPOP(ptrNative[r8*4+3+rax]), "42 8f 44 80 03"),
		CODEGEN_TEST_BOTH(xPOP(ptrNative[rax*8+0x1234+rbx]), "8f 84 c3 34 12 00 00"),
	});
}
