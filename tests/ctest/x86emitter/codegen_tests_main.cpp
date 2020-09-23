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
#include <cstdio>

using namespace x86Emitter;

TEST(CodegenTests, MOVTest)
{
	CODEGEN_TEST_BOTH(xMOV(rax, 0), "31 c0");
	CODEGEN_TEST_64(xMOV(rax, rcx), "48 89 c8");
	CODEGEN_TEST_BOTH(xMOV(eax, ecx), "89 c8");
	CODEGEN_TEST_64(xMOV(r8, 0), "45 31 c0");
	CODEGEN_TEST_64(xMOV(rax, r8), "4c 89 c0");
	CODEGEN_TEST_64(xMOV(r8, rax), "49 89 c0");
	CODEGEN_TEST_64(xMOV(r8, r9), "4d 89 c8");
	CODEGEN_TEST_64(xMOV(rax, ptrNative[rcx]), "48 8b 01");
	CODEGEN_TEST_BOTH(xMOV(eax, ptrNative[rcx]), "8b 01");
	CODEGEN_TEST_64(xMOV(ptrNative[rax], rcx), "48 89 08");
	CODEGEN_TEST_BOTH(xMOV(ptr32[rax], ecx), "89 08");
	CODEGEN_TEST_64(xMOV(rax, ptrNative[r8]), "49 8b 00");
	CODEGEN_TEST_64(xMOV(ptrNative[r8], rax), "49 89 00");
	CODEGEN_TEST_64(xMOV(r8, ptrNative[r9]), "4d 8b 01");
	CODEGEN_TEST_64(xMOV(ptrNative[r8], r9), "4d 89 08");
	CODEGEN_TEST_64(xMOV(rax, ptrNative[rbx*4+3+rcx]), "48 8b 44 99 03");
	CODEGEN_TEST_64(xMOV(ptrNative[rbx*4+3+rax], rcx), "48 89 4c 98 03");
	CODEGEN_TEST_BOTH(xMOV(eax, ptr32[rbx*4+3+rcx]), "8b 44 99 03");
	CODEGEN_TEST_BOTH(xMOV(ptr32[rbx*4+3+rax], ecx), "89 4c 98 03");
	CODEGEN_TEST_64(xMOV(r8, ptrNative[r10*4+3+r9]), "4f 8b 44 91 03");
	CODEGEN_TEST_64(xMOV(ptrNative[r9*4+3+r8], r10), "4f 89 54 88 03");
	CODEGEN_TEST_64(xMOV(ptrNative[r8], 0), "49 c7 00 00 00 00 00");
	CODEGEN_TEST_BOTH(xMOV(ptr32[rax], 0), "c7 00 00 00 00 00");
	CODEGEN_TEST_BOTH(xMOV(ptr32[rbx*4+3+rax], -1), "c7 44 98 03 ff ff ff ff");
	CODEGEN_TEST_64(xMOV(rax, 0xffffffff), "b8 ff ff ff ff");
	CODEGEN_TEST_64(xMOV(r8, -1), "49 c7 c0 ff ff ff ff");
	CODEGEN_TEST_64(xMOV64(rax, 0x1234567890), "48 b8 90 78 56 34 12 00 00 00");
	CODEGEN_TEST_64(xMOV64(r8, 0x1234567890), "49 b8 90 78 56 34 12 00 00 00");
	CODEGEN_TEST_64(xMOV(ptr32[base], 0x12), "c7 05 f6 ff ff ff 12 00 00 00");
	CODEGEN_TEST_BOTH(xMOVSX(eax, dx), "0f bf c2");
	CODEGEN_TEST_64(xMOVSX(rax, r8d), "49 63 c0");
	CODEGEN_TEST_64(xMOVSX(rax, ebx), "48 63 c3");
}

TEST(CodegenTests, LEATest)
{
	CODEGEN_TEST_64(xLEA(rax, ptr[rcx]), "48 89 c8"); // Converted to mov rax, rcx
	CODEGEN_TEST_BOTH(xLEA(eax, ptr[rcx]), "89 c8"); // Converted to mov eax, ecx
	CODEGEN_TEST_64(xLEA(rax, ptr[r8]), "4c 89 c0"); // Converted to mov rax, r8
	CODEGEN_TEST_64(xLEA(r8, ptr[r9]), "4d 89 c8"); // Converted to mov r8, r9
	CODEGEN_TEST_64(xLEA(rax, ptr[rbx*4+3+rcx]), "48 8d 44 99 03");
	CODEGEN_TEST_BOTH(xLEA(eax, ptr32[rbx*4+3+rcx]), "8d 44 99 03");
	CODEGEN_TEST_64(xLEA(r8, ptr[r10*4+3+r9]), "4f 8d 44 91 03");
	CODEGEN_TEST_64(xLEA(r8, ptr[base]), "4c 8d 05 f9 ff ff ff");
	CODEGEN_TEST_64(xLoadFarAddr(r8, base), "4c 8d 05 f9 ff ff ff");
	CODEGEN_TEST_64(xLoadFarAddr(r8, (void*)0x1234567890), "49 b8 90 78 56 34 12 00 00 00");
	CODEGEN_TEST_BOTH(xLEA(rax, ptr[(void*)0x1234]), "b8 34 12 00 00"); // Converted to mov rax, 0x1234
	CODEGEN_TEST_BOTH(xLoadFarAddr(rax, (void*)0x1234), "b8 34 12 00 00");
	CODEGEN_TEST(xLEA_Writeback(rbx), "bb cd cd cd cd", "48 8d 1d cd cd cd 0d");
}

TEST(CodegenTests, PUSHTest)
{
	CODEGEN_TEST_BOTH(xPUSH(rax), "50");
	CODEGEN_TEST_64(xPUSH(r8), "41 50");
	CODEGEN_TEST_BOTH(xPUSH(0x1234), "68 34 12 00 00");
	CODEGEN_TEST_BOTH(xPUSH(0x12), "6a 12");
	CODEGEN_TEST_BOTH(xPUSH(ptrNative[rax]), "ff 30");
	CODEGEN_TEST_64(xPUSH(ptrNative[r8]), "41 ff 30");
	CODEGEN_TEST_BOTH(xPUSH(ptrNative[rax*2+3+rbx]), "ff 74 43 03");
	CODEGEN_TEST_64(xPUSH(ptrNative[rax*2+3+r8]), "41 ff 74 40 03");
	CODEGEN_TEST_64(xPUSH(ptrNative[r9*4+3+r8]), "43 ff 74 88 03");
	CODEGEN_TEST_64(xPUSH(ptrNative[r8*4+3+rax]), "42 ff 74 80 03");
	CODEGEN_TEST_BOTH(xPUSH(ptrNative[rax*8+0x1234+rbx]), "ff b4 c3 34 12 00 00");
	CODEGEN_TEST_64(xPUSH(ptrNative[base]), "ff 35 fa ff ff ff");
	CODEGEN_TEST(xPUSH(ptrNative[(void*)0x1234]), "ff 35 34 12 00 00", "ff 34 25 34 12 00 00");
}

TEST(CodegenTests, POPTest)
{
	CODEGEN_TEST_BOTH(xPOP(rax), "58");
	CODEGEN_TEST_64(xPOP(r8), "41 58");
	CODEGEN_TEST_BOTH(xPOP(ptrNative[rax]), "8f 00");
	CODEGEN_TEST_64(xPOP(ptrNative[r8]), "41 8f 00");
	CODEGEN_TEST_BOTH(xPOP(ptrNative[rax*2+3+rbx]), "8f 44 43 03");
	CODEGEN_TEST_64(xPOP(ptrNative[rax*2+3+r8]), "41 8f 44 40 03");
	CODEGEN_TEST_64(xPOP(ptrNative[r9*4+3+r8]), "43 8f 44 88 03");
	CODEGEN_TEST_64(xPOP(ptrNative[r8*4+3+rax]), "42 8f 44 80 03");
	CODEGEN_TEST_BOTH(xPOP(ptrNative[rax*8+0x1234+rbx]), "8f 84 c3 34 12 00 00");
	CODEGEN_TEST_64(xPOP(ptrNative[base]), "8f 05 fa ff ff ff");
	CODEGEN_TEST(xPOP(ptrNative[(void*)0x1234]), "8f 05 34 12 00 00", "8f 04 25 34 12 00 00");
}

TEST(CodegenTests, MathTest)
{
	CODEGEN_TEST(xINC(eax), "40", "ff c0");
	CODEGEN_TEST(xDEC(rax), "48", "48 ff c8");
	CODEGEN_TEST_64(xINC(r8), "49 ff c0");
	CODEGEN_TEST_64(xADD(r8, r9), "4d 01 c8");
	CODEGEN_TEST_64(xADD(r8, 0x12), "49 83 c0 12");
	CODEGEN_TEST_64(xADD(rax, 0x1234), "48 05 34 12 00 00");
	CODEGEN_TEST_64(xADD(ptr32[base], -0x60), "83 05 f9 ff ff ff a0");
	CODEGEN_TEST_64(xADD(ptr32[base], 0x1234), "81 05 f6 ff ff ff 34 12 00 00");
	CODEGEN_TEST_BOTH(xADD(eax, ebx), "01 d8");
	CODEGEN_TEST_BOTH(xADD(eax, 0x1234), "05 34 12 00 00");
	CODEGEN_TEST_64(xADD(r8, ptrNative[r10*4+3+r9]), "4f 03 44 91 03");
	CODEGEN_TEST_64(xADD(ptrNative[r9*4+3+r8], r10), "4f 01 54 88 03");
	CODEGEN_TEST_BOTH(xADD(eax, ptr32[rbx*4+3+rcx]), "03 44 99 03");
	CODEGEN_TEST_BOTH(xADD(ptr32[rax*4+3+rbx], ecx), "01 4c 83 03");
	CODEGEN_TEST_64(xSUB(r8, 0x12), "49 83 e8 12");
	CODEGEN_TEST_64(xSUB(rax, 0x1234), "48 2d 34 12 00 00");
	CODEGEN_TEST_BOTH(xSUB(eax, ptr32[rcx*4+rax]), "2b 04 88");
	CODEGEN_TEST_64(xMUL(ptr32[base]), "f7 2d fa ff ff ff");
	CODEGEN_TEST(xMUL(ptr32[(void*)0x1234]), "f7 2d 34 12 00 00", "f7 2c 25 34 12 00 00");
	CODEGEN_TEST_BOTH(xDIV(ecx), "f7 f9");
}

TEST(CodegenTests, BitwiseTest)
{
	CODEGEN_TEST_64(xSHR(r8, cl), "49 d3 e8");
	CODEGEN_TEST_64(xSHR(rax, cl), "48 d3 e8");
	CODEGEN_TEST_BOTH(xSHR(ecx, cl), "d3 e9");
	CODEGEN_TEST_64(xSAR(r8, 1), "49 d1 f8");
	CODEGEN_TEST_64(xSAR(rax, 60), "48 c1 f8 3c");
	CODEGEN_TEST_BOTH(xSAR(eax, 30), "c1 f8 1e");
	CODEGEN_TEST_BOTH(xSHL(ebx, 30), "c1 e3 1e");
	CODEGEN_TEST_64(xSHL(ptr32[base], 4), "c1 25 f9 ff ff ff 04");
	CODEGEN_TEST_64(xAND(r8, r9), "4d 21 c8");
	CODEGEN_TEST_64(xXOR(rax, ptrNative[r10]), "49 33 02");
	CODEGEN_TEST_BOTH(xOR(esi, ptr32[rax+rbx]), "0b 34 18");
	CODEGEN_TEST_64(xNOT(r8), "49 f7 d0");
	CODEGEN_TEST_64(xNOT(ptrNative[rax]), "48 f7 10");
	CODEGEN_TEST_BOTH(xNOT(ptr32[rbx]), "f7 13");
}

TEST(CodegenTests, JmpTest)
{
	CODEGEN_TEST_64(xJMP(r8), "41 ff e0");
	CODEGEN_TEST_BOTH(xJMP(rdi), "ff e7");
	CODEGEN_TEST_BOTH(xJMP(ptrNative[rax]), "ff 20");
	CODEGEN_TEST_BOTH(xJA(base), "77 fe");
	CODEGEN_TEST_BOTH(xJB((char*)base - 0xFFFF), "0f 82 fb ff fe ff");
}

TEST(CodegenTests, SSETest)
{
	CODEGEN_TEST_BOTH(xMOVAPS(xmm0, xmm1), "0f 28 c1");
	CODEGEN_TEST_64(xMOVAPS(xmm8, xmm9), "45 0f 28 c1");
	CODEGEN_TEST_64(xMOVUPS(xmm8, ptr128[r8+r9]), "47 0f 10 04 08");
	CODEGEN_TEST_64(xMOVAPS(ptr128[rax+r9], xmm8), "46 0f 29 04 08");
	CODEGEN_TEST_BOTH(xBLEND.PS(xmm0, xmm1, 0x55), "66 0f 3a 0c c1 55");
	CODEGEN_TEST_64(xBLEND.PD(xmm8, xmm9, 0xaa), "66 45 0f 3a 0d c1 aa");
	CODEGEN_TEST_64(xEXTRACTPS(ptr32[base], xmm1, 2), "66 0f 3a 17 0d f6 ff ff ff 02");
}
