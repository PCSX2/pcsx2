// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "codegen_tests.h"
#include <gtest/gtest.h>
#include <common/emitter/x86emitter.h>
#include <cstdio>

using namespace x86Emitter;

TEST(CodegenTests, MOVTest)
{
	CODEGEN_TEST(xMOV(rax, 0), "31 c0");
	CODEGEN_TEST(xMOV(rax, rcx), "48 89 c8");
	CODEGEN_TEST(xMOV(eax, ecx), "89 c8");
	CODEGEN_TEST(xMOV(r8, 0), "45 31 c0");
	CODEGEN_TEST(xMOV(rax, r8), "4c 89 c0");
	CODEGEN_TEST(xMOV(r8, rax), "49 89 c0");
	CODEGEN_TEST(xMOV(r8, r9), "4d 89 c8");
	CODEGEN_TEST(xMOV(rax, ptr64[rcx]), "48 8b 01");
	CODEGEN_TEST(xMOV(eax, ptr32[rcx]), "8b 01");
	CODEGEN_TEST(xMOV(ptr64[rax], rcx), "48 89 08");
	CODEGEN_TEST(xMOV(ptr32[rax], ecx), "89 08");
	CODEGEN_TEST(xMOV(rax, ptr64[r8]), "49 8b 00");
	CODEGEN_TEST(xMOV(ptr64[r8], rax), "49 89 00");
	CODEGEN_TEST(xMOV(r8, ptr64[r9]), "4d 8b 01");
	CODEGEN_TEST(xMOV(ptr64[r8], r9), "4d 89 08");
	CODEGEN_TEST(xMOV(rax, ptr64[rbx*4+3+rcx]), "48 8b 44 99 03");
	CODEGEN_TEST(xMOV(ptr64[rbx*4+3+rax], rcx), "48 89 4c 98 03");
	CODEGEN_TEST(xMOV(eax, ptr32[rbx*4+3+rcx]), "8b 44 99 03");
	CODEGEN_TEST(xMOV(ptr32[rbx*4+3+rax], ecx), "89 4c 98 03");
	CODEGEN_TEST(xMOV(r8, ptr64[r10*4+3+r9]), "4f 8b 44 91 03");
	CODEGEN_TEST(xMOV(ptr64[r9*4+3+r8], r10), "4f 89 54 88 03");
	CODEGEN_TEST(xMOV(ptr64[r8], 0), "49 c7 00 00 00 00 00");
	CODEGEN_TEST(xMOV(ptr32[rax], 0), "c7 00 00 00 00 00");
	CODEGEN_TEST(xMOV(ptr32[rbx*4+3+rax], -1), "c7 44 98 03 ff ff ff ff");
	CODEGEN_TEST(xMOV(rax, 0xffffffff), "b8 ff ff ff ff");
	CODEGEN_TEST(xMOV(r8, -1), "49 c7 c0 ff ff ff ff");
	CODEGEN_TEST(xMOV64(rax, 0x1234567890), "48 b8 90 78 56 34 12 00 00 00");
	CODEGEN_TEST(xMOV64(r8, 0x1234567890), "49 b8 90 78 56 34 12 00 00 00");
	CODEGEN_TEST(xMOV(ptr32[base], 0x12), "c7 05 f6 ff ff ff 12 00 00 00");
	CODEGEN_TEST(xMOVSX(eax, dx), "0f bf c2");
	CODEGEN_TEST(xMOVSX(rax, r8d), "49 63 c0");
	CODEGEN_TEST(xMOVSX(rax, ebx), "48 63 c3");
}

TEST(CodegenTests, LEATest)
{
	CODEGEN_TEST(xLEA(rax, ptr[rcx]), "48 89 c8"); // Converted to mov rax, rcx
	CODEGEN_TEST(xLEA(eax, ptr[rcx]), "89 c8"); // Converted to mov eax, ecx
	CODEGEN_TEST(xLEA(rax, ptr[r8]), "4c 89 c0"); // Converted to mov rax, r8
	CODEGEN_TEST(xLEA(r8, ptr[r9]), "4d 89 c8"); // Converted to mov r8, r9
	CODEGEN_TEST(xLEA(rax, ptr[rbx*4+3+rcx]), "48 8d 44 99 03");
	CODEGEN_TEST(xLEA(eax, ptr32[rbx*4+3+rcx]), "8d 44 99 03");
	CODEGEN_TEST(xLEA(r8, ptr[r10*4+3+r9]), "4f 8d 44 91 03");
	CODEGEN_TEST(xLEA(r8, ptr[base]), "4c 8d 05 f9 ff ff ff");
	CODEGEN_TEST(xLoadFarAddr(r8, base), "4c 8d 05 f9 ff ff ff");
	CODEGEN_TEST(xLoadFarAddr(r8, (void*)0xff00001234567890), "49 b8 90 78 56 34 12 00 00 ff");
	CODEGEN_TEST(xLEA(rax, ptr[(void*)0x1234]), "b8 34 12 00 00"); // Converted to mov rax, 0x1234
	CODEGEN_TEST(xLoadFarAddr(rax, (void*)0x1234), "b8 34 12 00 00");
	CODEGEN_TEST(xLEA_Writeback(rbx), "48 8d 1d cd cd cd 0d");
}

TEST(CodegenTests, PUSHTest)
{
	CODEGEN_TEST(xPUSH(rax), "50");
	CODEGEN_TEST(xPUSH(r8), "41 50");
	CODEGEN_TEST(xPUSH(0x1234), "68 34 12 00 00");
	CODEGEN_TEST(xPUSH(0x12), "6a 12");
	CODEGEN_TEST(xPUSH(ptr64[rax]), "ff 30");
	CODEGEN_TEST(xPUSH(ptr64[r8]), "41 ff 30");
	CODEGEN_TEST(xPUSH(ptr64[rax*2+3+rbx]), "ff 74 43 03");
	CODEGEN_TEST(xPUSH(ptr64[rax*2+3+r8]), "41 ff 74 40 03");
	CODEGEN_TEST(xPUSH(ptr64[r9*4+3+r8]), "43 ff 74 88 03");
	CODEGEN_TEST(xPUSH(ptr64[r8*4+3+rax]), "42 ff 74 80 03");
	CODEGEN_TEST(xPUSH(ptr64[rax*8+0x1234+rbx]), "ff b4 c3 34 12 00 00");
	CODEGEN_TEST(xPUSH(ptr64[base]), "ff 35 fa ff ff ff");
	CODEGEN_TEST(xPUSH(ptr64[(void*)0x1234]), "ff 34 25 34 12 00 00");
}

TEST(CodegenTests, POPTest)
{
	CODEGEN_TEST(xPOP(rax), "58");
	CODEGEN_TEST(xPOP(r8), "41 58");
	CODEGEN_TEST(xPOP(ptr64[rax]), "8f 00");
	CODEGEN_TEST(xPOP(ptr64[r8]), "41 8f 00");
	CODEGEN_TEST(xPOP(ptr64[rax*2+3+rbx]), "8f 44 43 03");
	CODEGEN_TEST(xPOP(ptr64[rax*2+3+r8]), "41 8f 44 40 03");
	CODEGEN_TEST(xPOP(ptr64[r9*4+3+r8]), "43 8f 44 88 03");
	CODEGEN_TEST(xPOP(ptr64[r8*4+3+rax]), "42 8f 44 80 03");
	CODEGEN_TEST(xPOP(ptr64[rax*8+0x1234+rbx]), "8f 84 c3 34 12 00 00");
	CODEGEN_TEST(xPOP(ptr64[base]), "8f 05 fa ff ff ff");
	CODEGEN_TEST(xPOP(ptr64[(void*)0x1234]), "8f 04 25 34 12 00 00");
}

TEST(CodegenTests, MathTest)
{
	CODEGEN_TEST(xINC(eax), "ff c0");
	CODEGEN_TEST(xDEC(rax), "48 ff c8");
	CODEGEN_TEST(xINC(r8), "49 ff c0");
	CODEGEN_TEST(xADD(r8, r9), "4d 01 c8");
	CODEGEN_TEST(xADD(r8, 0x12), "49 83 c0 12");
	CODEGEN_TEST(xADD(rax, 0x1234), "48 05 34 12 00 00");
	CODEGEN_TEST(xADD(ptr8[base], 1), "80 05 f9 ff ff ff 01");
	CODEGEN_TEST(xADD(ptr32[base], -0x60), "83 05 f9 ff ff ff a0");
	CODEGEN_TEST(xADD(ptr32[base], 0x1234), "81 05 f6 ff ff ff 34 12 00 00");
	CODEGEN_TEST(xADD(eax, ebx), "01 d8");
	CODEGEN_TEST(xADD(eax, 0x1234), "05 34 12 00 00");
	CODEGEN_TEST(xADD(r8, ptr64[r10*4+3+r9]), "4f 03 44 91 03");
	CODEGEN_TEST(xADD(ptr64[r9*4+3+r8], r10), "4f 01 54 88 03");
	CODEGEN_TEST(xADD(eax, ptr32[rbx*4+3+rcx]), "03 44 99 03");
	CODEGEN_TEST(xADD(ptr32[rax*4+3+rbx], ecx), "01 4c 83 03");
	CODEGEN_TEST(xSUB(r8, 0x12), "49 83 e8 12");
	CODEGEN_TEST(xSUB(rax, 0x1234), "48 2d 34 12 00 00");
	CODEGEN_TEST(xSUB(eax, ptr32[rcx*4+rax]), "2b 04 88");
	CODEGEN_TEST(xMUL(ptr32[base]), "f7 2d fa ff ff ff");
	CODEGEN_TEST(xMUL(ptr32[(void*)0x1234]), "f7 2c 25 34 12 00 00");
	CODEGEN_TEST(xDIV(ecx), "f7 f9");
}

TEST(CodegenTests, BitwiseTest)
{
	CODEGEN_TEST(xSHR(r8, cl), "49 d3 e8");
	CODEGEN_TEST(xSHR(rax, cl), "48 d3 e8");
	CODEGEN_TEST(xSHR(ecx, cl), "d3 e9");
	CODEGEN_TEST(xSAR(r8, 1), "49 d1 f8");
	CODEGEN_TEST(xSAR(rax, 60), "48 c1 f8 3c");
	CODEGEN_TEST(xSAR(eax, 30), "c1 f8 1e");
	CODEGEN_TEST(xSHL(ebx, 30), "c1 e3 1e");
	CODEGEN_TEST(xSHL(ptr32[base], 4), "c1 25 f9 ff ff ff 04");
	CODEGEN_TEST(xAND(r8, r9), "4d 21 c8");
	CODEGEN_TEST(xXOR(rax, ptr64[r10]), "49 33 02");
	CODEGEN_TEST(xOR(esi, ptr32[rax+rbx]), "0b 34 18");
	CODEGEN_TEST(xNOT(r8), "49 f7 d0");
	CODEGEN_TEST(xNOT(ptr64[rax]), "48 f7 10");
	CODEGEN_TEST(xNOT(ptr32[rbx]), "f7 13");
}

TEST(CodegenTests, JmpTest)
{
	CODEGEN_TEST(xJMP(r8), "41 ff e0");
	CODEGEN_TEST(xJMP(rdi), "ff e7");
	CODEGEN_TEST(xJMP(ptr64[rax]), "ff 20");
	CODEGEN_TEST(xJA(base), "77 fe");
	CODEGEN_TEST(xJB((char*)base - 0xFFFF), "0f 82 fb ff fe ff");
}

TEST(CodegenTests, SSETest)
{
	x86Emitter::use_avx = false;

	CODEGEN_TEST(xPAND(xmm3, xmm8),  "66 41 0f db d8");
	CODEGEN_TEST(xPANDN(xmm4, xmm9), "66 41 0f df e1");
	CODEGEN_TEST(xPOR(xmm5, xmm8),   "66 41 0f eb e8");
	CODEGEN_TEST(xPXOR(xmm9, xmm4),  "66 44 0f ef cc");
	CODEGEN_TEST(xPTEST(xmm2, xmm9), "66 41 0f 38 17 d1");

	CODEGEN_TEST(xAND.PS(xmm3, xmm8),  "41 0f 54 d8");
	CODEGEN_TEST(xOR.PS(xmm5, xmm8),   "41 0f 56 e8");
	CODEGEN_TEST(xXOR.PS(xmm9, xmm4),  "44 0f 57 cc");

	CODEGEN_TEST(xCVTDQ2PD(xmm0, ptr64[rax]), "f3 0f e6 00");
	CODEGEN_TEST(xCVTDQ2PS(xmm0, xmm8),       "41 0f 5b c0");
	CODEGEN_TEST(xCVTPD2DQ(xmm8, ptr128[r8]), "f2 45 0f e6 00");
	CODEGEN_TEST(xCVTPD2PS(xmm1, xmm7),       "66 0f 5a cf");
	CODEGEN_TEST(xCVTSD2SI(rax,  xmm1),       "f2 48 0f 2d c1");
	CODEGEN_TEST(xCVTSD2SI(esi,  ptr64[rax]), "f2 0f 2d 30");
	CODEGEN_TEST(xCVTSD2SS(xmm3, xmm4),       "f2 0f 5a dc");
	CODEGEN_TEST(xCVTSI2SS(xmm8, ecx),        "f3 44 0f 2a c1");
	CODEGEN_TEST(xCVTSI2SS(xmm3, ptr32[r8]),  "f3 41 0f 2a 18");
	CODEGEN_TEST(xCVTSI2SS(xmm3, ptr64[r8]),  "f3 49 0f 2a 18");
	CODEGEN_TEST(xCVTSS2SD(xmm8, xmm7),       "f3 44 0f 5a c7");
	CODEGEN_TEST(xCVTSS2SD(xmm4, ptr32[rcx]), "f3 0f 5a 21");
	CODEGEN_TEST(xCVTSS2SI(eax,  xmm4),       "f3 0f 2d c4");
	CODEGEN_TEST(xCVTSS2SI(rcx,  ptr32[rax]), "f3 48 0f 2d 08");
	CODEGEN_TEST(xCVTTPD2DQ(xmm4, xmm7),      "66 0f e6 e7");
	CODEGEN_TEST(xCVTTPS2DQ(xmm5, xmm3),      "f3 0f 5b eb");
	CODEGEN_TEST(xCVTTSD2SI(rdx,  xmm4),      "f2 48 0f 2c d4");
	CODEGEN_TEST(xCVTTSS2SI(ecx,  xmm3),      "f3 0f 2c cb");

	CODEGEN_TEST(xPSLL.W(xmm8, ptr[r8]),  "66 45 0f f1 00");
	CODEGEN_TEST(xPSLL.D(xmm0, xmm1),     "66 0f f2 c1");
	CODEGEN_TEST(xPSLL.Q(xmm4, ptr[rcx]), "66 0f f3 21");
	CODEGEN_TEST(xPSLL.W(xmm5, 2),        "66 0f 71 f5 02");
	CODEGEN_TEST(xPSLL.D(xmm6, 3),        "66 0f 72 f6 03");
	CODEGEN_TEST(xPSLL.Q(xmm7, 4),        "66 0f 73 f7 04");
	CODEGEN_TEST(xPSLL.DQ(xmm8, 5),       "66 41 0f 73 f8 05");
	CODEGEN_TEST(xPSRA.W(xmm4, xmm2),     "66 0f e1 e2");
	CODEGEN_TEST(xPSRA.D(xmm5, ptr[rdi]), "66 0f e2 2f");
	CODEGEN_TEST(xPSRA.W(xmm4, 3),        "66 0f 71 e4 03");
	CODEGEN_TEST(xPSRA.D(xmm5, 7),        "66 0f 72 e5 07");
	CODEGEN_TEST(xPSRL.W(xmm8, ptr[r8]),  "66 45 0f d1 00");
	CODEGEN_TEST(xPSRL.D(xmm0, xmm1),     "66 0f d2 c1");
	CODEGEN_TEST(xPSRL.Q(xmm4, ptr[rcx]), "66 0f d3 21");
	CODEGEN_TEST(xPSRL.W(xmm5, 2),        "66 0f 71 d5 02");
	CODEGEN_TEST(xPSRL.D(xmm6, 3),        "66 0f 72 d6 03");
	CODEGEN_TEST(xPSRL.Q(xmm7, 4),        "66 0f 73 d7 04");
	CODEGEN_TEST(xPSRL.DQ(xmm8, 5),       "66 41 0f 73 d8 05");

	CODEGEN_TEST(xPADD.B(xmm1, xmm8),     "66 41 0f fc c8");
	CODEGEN_TEST(xPADD.W(xmm4, xmm7),     "66 0f fd e7");
	CODEGEN_TEST(xPADD.D(xmm2, ptr[rcx]), "66 0f fe 11");
	CODEGEN_TEST(xPADD.Q(xmm8, xmm2),     "66 44 0f d4 c2");
	CODEGEN_TEST(xPADD.SB(xmm9, xmm8),    "66 45 0f ec c8");
	CODEGEN_TEST(xPADD.SW(xmm2, ptr[r8]), "66 41 0f ed 10");
	CODEGEN_TEST(xPADD.USB(xmm3, xmm3),   "66 0f dc db");
	CODEGEN_TEST(xPADD.USW(xmm2, xmm9),   "66 41 0f dd d1");
	CODEGEN_TEST(xPSUB.B(xmm1, xmm8),     "66 41 0f f8 c8");
	CODEGEN_TEST(xPSUB.W(xmm4, xmm7),     "66 0f f9 e7");
	CODEGEN_TEST(xPSUB.D(xmm2, ptr[rcx]), "66 0f fa 11");
	CODEGEN_TEST(xPSUB.Q(xmm8, xmm2),     "66 44 0f fb c2");
	CODEGEN_TEST(xPSUB.SB(xmm9, xmm8),    "66 45 0f e8 c8");
	CODEGEN_TEST(xPSUB.SW(xmm2, ptr[r8]), "66 41 0f e9 10");
	CODEGEN_TEST(xPSUB.USB(xmm3, xmm3),   "66 0f d8 db");
	CODEGEN_TEST(xPSUB.USW(xmm2, xmm9),   "66 41 0f d9 d1");
	CODEGEN_TEST(xPMUL.LW(xmm2, xmm8),    "66 41 0f d5 d0");
	CODEGEN_TEST(xPMUL.HW(xmm9, ptr[r9]), "66 45 0f e5 09");
	CODEGEN_TEST(xPMUL.HUW(xmm4, xmm3),   "66 0f e4 e3");
	CODEGEN_TEST(xPMUL.UDQ(xmm1, xmm7),   "66 0f f4 cf");
	CODEGEN_TEST(xPMUL.HRSW(xmm2, xmm4),  "66 0f 38 0b d4");
	CODEGEN_TEST(xPMUL.LD(xmm1, xmm8),    "66 41 0f 38 40 c8");
	CODEGEN_TEST(xPMUL.DQ(xmm4, xmm9),    "66 41 0f 38 28 e1");

	CODEGEN_TEST(xADD.SS(xmm1, xmm8),     "f3 41 0f 58 c8");
	CODEGEN_TEST(xADD.SD(xmm4, xmm7),     "f2 0f 58 e7");
	CODEGEN_TEST(xADD.PS(xmm2, ptr[rcx]), "0f 58 11");
	CODEGEN_TEST(xADD.PD(xmm8, xmm2),     "66 44 0f 58 c2");
	CODEGEN_TEST(xSUB.SS(xmm1, xmm8),     "f3 41 0f 5c c8");
	CODEGEN_TEST(xSUB.SD(xmm4, xmm7),     "f2 0f 5c e7");
	CODEGEN_TEST(xSUB.PS(xmm2, ptr[rcx]), "0f 5c 11");
	CODEGEN_TEST(xSUB.PD(xmm8, xmm2),     "66 44 0f 5c c2");
	CODEGEN_TEST(xMUL.SS(xmm2, xmm8),     "f3 41 0f 59 d0");
	CODEGEN_TEST(xMUL.SD(xmm9, ptr[r9]),  "f2 45 0f 59 09");
	CODEGEN_TEST(xMUL.PS(xmm4, xmm3),     "0f 59 e3");
	CODEGEN_TEST(xMUL.PD(xmm1, xmm8),     "66 41 0f 59 c8");
	CODEGEN_TEST(xDIV.SS(xmm2, xmm4),     "f3 0f 5e d4");
	CODEGEN_TEST(xDIV.SD(xmm1, xmm8),     "f2 41 0f 5e c8");
	CODEGEN_TEST(xDIV.PS(xmm4, xmm9),     "41 0f 5e e1");
	CODEGEN_TEST(xDIV.PD(xmm9, xmm2),     "66 44 0f 5e ca");

	CODEGEN_TEST(xRSQRT.PS(xmm0, xmm8),    "41 0f 52 c0");
	CODEGEN_TEST(xRSQRT.SS(xmm4, ptr[r9]), "f3 41 0f 52 21");
	CODEGEN_TEST(xRCP.PS(xmm4, ptr[rcx]),  "0f 53 21");
	CODEGEN_TEST(xRCP.SS(xmm5, xmm8),      "f3 41 0f 53 e8");
	CODEGEN_TEST(xSQRT.PS(xmm4, xmm2),     "0f 51 e2");
	CODEGEN_TEST(xSQRT.SS(xmm5, xmm1),     "f3 0f 51 e9");
	CODEGEN_TEST(xSQRT.PD(xmm7, ptr[rdi]), "66 0f 51 3f");
	CODEGEN_TEST(xSQRT.SD(xmm5, xmm2),     "f2 0f 51 ea");
	CODEGEN_TEST(xANDN.PS(xmm6, ptr[rdi]), "0f 55 37");
	CODEGEN_TEST(xANDN.PD(xmm3, xmm8),     "66 41 0f 55 d8");

	CODEGEN_TEST(xPABS.B(xmm0, xmm2),     "66 0f 38 1c c2");
	CODEGEN_TEST(xPABS.W(xmm4, xmm8),     "66 41 0f 38 1d e0");
	CODEGEN_TEST(xPABS.D(xmm6, ptr[rax]), "66 0f 38 1e 30");
	CODEGEN_TEST(xPSIGN.B(xmm0, xmm2),    "66 0f 38 08 c2");
	CODEGEN_TEST(xPSIGN.W(xmm4, xmm8),    "66 41 0f 38 09 e0");
	CODEGEN_TEST(xPSIGN.D(xmm2, ptr[r8]), "66 41 0f 38 0a 10");
	CODEGEN_TEST(xPMADD.WD(xmm0, xmm8),   "66 41 0f f5 c0");
	CODEGEN_TEST(xPMADD.UBSW(xmm0, xmm8), "66 41 0f 38 04 c0");

	CODEGEN_TEST(xHADD.PS(xmm1, xmm8),     "f2 41 0f 7c c8");
	CODEGEN_TEST(xHADD.PD(xmm4, ptr[r8]),  "66 41 0f 7c 20");
	CODEGEN_TEST(xDP.PS(xmm3, xmm9, 0xf7), "66 41 0f 3a 40 d9 f7");
	CODEGEN_TEST(xDP.PD(xmm8, xmm4, 0x33), "66 44 0f 3a 41 c4 33");
	CODEGEN_TEST(xROUND.PS(xmm1, xmm3, 0), "66 0f 3a 08 cb 00");
	CODEGEN_TEST(xROUND.PD(xmm3, xmm9, 1), "66 41 0f 3a 09 d9 01");
	CODEGEN_TEST(xROUND.SS(xmm5, xmm2, 2), "66 0f 3a 0a ea 02");
	CODEGEN_TEST(xROUND.SD(xmm8, xmm2, 3), "66 44 0f 3a 0b c2 03");

	CODEGEN_TEST(xCMPEQ.PS(xmm4, xmm8),   "41 0f c2 e0 00");
	CODEGEN_TEST(xCMPLT.PD(xmm6, xmm9),   "66 41 0f c2 f1 01");
	CODEGEN_TEST(xCMPLE.SS(xmm2, xmm5),   "f3 0f c2 d5 02");
	CODEGEN_TEST(xCMPNE.SD(xmm1, xmm9),   "f2 41 0f c2 c9 04");
	CODEGEN_TEST(xMIN.PS(xmm2, xmm8),     "41 0f 5d d0");
	CODEGEN_TEST(xMIN.PD(xmm3, ptr[rax]), "66 0f 5d 18");
	CODEGEN_TEST(xMIN.SS(xmm8, xmm2),     "f3 44 0f 5d c2");
	CODEGEN_TEST(xMIN.SD(xmm1, ptr[r8]),  "f2 41 0f 5d 08");
	CODEGEN_TEST(xMAX.PS(xmm2, xmm8),     "41 0f 5f d0");
	CODEGEN_TEST(xMAX.PD(xmm3, ptr[rax]), "66 0f 5f 18");
	CODEGEN_TEST(xMAX.SS(xmm8, xmm2),     "f3 44 0f 5f c2");
	CODEGEN_TEST(xMAX.SD(xmm1, ptr[r8]),  "f2 41 0f 5f 08");
	CODEGEN_TEST(xCOMI.SS(xmm2, xmm8),    "41 0f 2f d0");
	CODEGEN_TEST(xCOMI.SD(xmm3, ptr[r8]), "66 41 0f 2f 18");
	CODEGEN_TEST(xUCOMI.SS(xmm8, xmm2),   "44 0f 2e c2");
	CODEGEN_TEST(xUCOMI.SD(xmm2, xmm3),   "66 0f 2e d3");

	CODEGEN_TEST(xPCMP.EQB(xmm0, xmm8),    "66 41 0f 74 c0");
	CODEGEN_TEST(xPCMP.EQW(xmm4, ptr[r8]), "66 41 0f 75 20");
	CODEGEN_TEST(xPCMP.EQD(xmm3, xmm4),    "66 0f 76 dc");
	CODEGEN_TEST(xPCMP.GTB(xmm0, xmm8),    "66 41 0f 64 c0");
	CODEGEN_TEST(xPCMP.GTW(xmm4, ptr[r8]), "66 41 0f 65 20");
	CODEGEN_TEST(xPCMP.GTD(xmm3, xmm4),    "66 0f 66 dc");
	CODEGEN_TEST(xPMIN.UB(xmm0, xmm8),     "66 41 0f da c0");
	CODEGEN_TEST(xPMIN.SW(xmm4, ptr[rcx]), "66 0f ea 21");
	CODEGEN_TEST(xPMIN.SB(xmm3, xmm4),     "66 0f 38 38 dc");
	CODEGEN_TEST(xPMIN.SD(xmm8, xmm3),     "66 44 0f 38 39 c3");
	CODEGEN_TEST(xPMIN.UW(xmm4, xmm9),     "66 41 0f 38 3a e1");
	CODEGEN_TEST(xPMIN.UD(xmm2, ptr[r10]), "66 41 0f 38 3b 12");
	CODEGEN_TEST(xPMAX.UB(xmm0, xmm8),     "66 41 0f de c0");
	CODEGEN_TEST(xPMAX.SW(xmm4, ptr[rcx]), "66 0f ee 21");
	CODEGEN_TEST(xPMAX.SB(xmm3, xmm4),     "66 0f 38 3c dc");
	CODEGEN_TEST(xPMAX.SD(xmm8, xmm3),     "66 44 0f 38 3d c3");
	CODEGEN_TEST(xPMAX.UW(xmm4, xmm9),     "66 41 0f 38 3e e1");
	CODEGEN_TEST(xPMAX.UD(xmm2, ptr[r10]), "66 41 0f 38 3f 12");

	CODEGEN_TEST(xSHUF.PS(xmm0, xmm8, 0x33),       "41 0f c6 c0 33");
	CODEGEN_TEST(xSHUF.PS(xmm0, ptr[r8], 0),       "41 0f c6 00 00");
	CODEGEN_TEST(xSHUF.PD(xmm3, ptr[rcx], 0),      "66 0f c6 19 00");
	CODEGEN_TEST(xSHUF.PD(xmm3, xmm2, 2),          "66 0f c6 da 02");
	CODEGEN_TEST(xINSERTPS(xmm1, xmm2, 0x87),      "66 0f 3a 21 ca 87");
	CODEGEN_TEST(xINSERTPS(xmm1, ptr32[r8], 0x87), "66 41 0f 3a 21 08 87");
	CODEGEN_TEST(xEXTRACTPS(eax, xmm2, 2),         "66 0f 3a 17 d0 02");
	CODEGEN_TEST(xEXTRACTPS(ptr32[r9], xmm3, 3),   "66 41 0f 3a 17 19 03");
	CODEGEN_TEST(xEXTRACTPS(ptr32[base], xmm1, 2), "66 0f 3a 17 0d f6 ff ff ff 02");

	CODEGEN_TEST(xPSHUF.D(xmm2, ptr[r8], 0),    "66 41 0f 70 10 00");
	CODEGEN_TEST(xPSHUF.LW(xmm3, xmm8, 1),      "f2 41 0f 70 d8 01");
	CODEGEN_TEST(xPSHUF.HW(xmm4, xmm2, 8),      "f3 0f 70 e2 08");
	CODEGEN_TEST(xPSHUF.B(xmm2, ptr[r8]),       "66 41 0f 38 00 10");
	CODEGEN_TEST(xPINSR.B(xmm1, ebx, 1),        "66 0f 3a 20 cb 01");
	CODEGEN_TEST(xPINSR.W(xmm1, ebx, 1),        "66 0f c4 cb 01");
	CODEGEN_TEST(xPINSR.D(xmm1, ebx, 1),        "66 0f 3a 22 cb 01");
	CODEGEN_TEST(xPINSR.Q(xmm1, rbx, 1),        "66 48 0f 3a 22 cb 01");
	CODEGEN_TEST(xPINSR.B(xmm9, ptr8[rax], 1),  "66 44 0f 3a 20 08 01");
	CODEGEN_TEST(xPINSR.W(xmm9, ptr16[rax], 1), "66 44 0f c4 08 01");
	CODEGEN_TEST(xPINSR.D(xmm9, ptr32[rax], 1), "66 44 0f 3a 22 08 01");
	CODEGEN_TEST(xPINSR.Q(xmm9, ptr64[rax], 1), "66 4c 0f 3a 22 08 01");
	CODEGEN_TEST(xPEXTR.B(ebx, xmm1, 1),        "66 0f 3a 14 cb 01");
	CODEGEN_TEST(xPEXTR.W(ebx, xmm1, 1),        "66 0f c5 d9 01");
	CODEGEN_TEST(xPEXTR.D(ebx, xmm1, 1),        "66 0f 3a 16 cb 01");
	CODEGEN_TEST(xPEXTR.Q(rbx, xmm1, 1),        "66 48 0f 3a 16 cb 01");
	CODEGEN_TEST(xPEXTR.B(ptr8[rax],  xmm9, 1), "66 44 0f 3a 14 08 01");
	CODEGEN_TEST(xPEXTR.W(ptr16[rax], xmm9, 1), "66 44 0f 3a 15 08 01");
	CODEGEN_TEST(xPEXTR.D(ptr32[rax], xmm9, 1), "66 44 0f 3a 16 08 01");
	CODEGEN_TEST(xPEXTR.Q(ptr64[rax], xmm9, 1), "66 4c 0f 3a 16 08 01");

	CODEGEN_TEST(xPUNPCK.LBW(xmm1, xmm2),    "66 0f 60 ca");
	CODEGEN_TEST(xPUNPCK.LWD(xmm1, ptr[r8]), "66 41 0f 61 08");
	CODEGEN_TEST(xPUNPCK.LDQ(xmm1, xmm8),    "66 41 0f 62 c8");
	CODEGEN_TEST(xPUNPCK.LQDQ(xmm8, xmm2),   "66 44 0f 6c c2");
	CODEGEN_TEST(xPUNPCK.HBW(xmm1, xmm2),    "66 0f 68 ca");
	CODEGEN_TEST(xPUNPCK.HWD(xmm1, ptr[r8]), "66 41 0f 69 08");
	CODEGEN_TEST(xPUNPCK.HDQ(xmm1, xmm8),    "66 41 0f 6a c8");
	CODEGEN_TEST(xPUNPCK.HQDQ(xmm8, xmm2),   "66 44 0f 6d c2");
	CODEGEN_TEST(xPACK.SSWB(xmm1, xmm2),     "66 0f 63 ca");
	CODEGEN_TEST(xPACK.SSDW(xmm1, ptr[rax]), "66 0f 6b 08");
	CODEGEN_TEST(xPACK.USWB(xmm1, xmm8),     "66 41 0f 67 c8");
	CODEGEN_TEST(xPACK.USDW(xmm8, xmm2),     "66 44 0f 38 2b c2");
	CODEGEN_TEST(xUNPCK.LPS(xmm1, xmm2),     "0f 14 ca");
	CODEGEN_TEST(xUNPCK.LPD(xmm1, ptr[r8]),  "66 41 0f 14 08");
	CODEGEN_TEST(xUNPCK.HPS(xmm1, xmm8),     "41 0f 15 c8");
	CODEGEN_TEST(xUNPCK.HPD(xmm8, xmm2),     "66 44 0f 15 c2");

	CODEGEN_TEST(xMOVH.PS(ptr[r8], xmm2),      "41 0f 17 10");
	CODEGEN_TEST(xMOVH.PD(xmm2, ptr[rcx]),     "66 0f 16 11");
	CODEGEN_TEST(xMOVL.PS(xmm8, ptr[rax]),     "44 0f 12 00");
	CODEGEN_TEST(xMOVL.PD(ptr[r8 + r9], xmm9), "66 47 0f 13 0c 08");
	CODEGEN_TEST(xMOVHL.PS(xmm4, xmm9),        "41 0f 12 e1");
	CODEGEN_TEST(xMOVLH.PS(xmm2, xmm1),        "0f 16 d1");

	CODEGEN_TEST(xMOVAPS(xmm0, xmm8),     "41 0f 28 c0");
	CODEGEN_TEST(xMOVUPS(xmm8, xmm3),     "44 0f 28 c3");
	CODEGEN_TEST(xMOVAPS(ptr[r8], xmm4),  "41 0f 29 20");
	CODEGEN_TEST(xMOVUPS(ptr[rax], xmm5), "0f 11 28");
	CODEGEN_TEST(xMOVAPS(xmm8, ptr[r8]),  "45 0f 28 00");
	CODEGEN_TEST(xMOVUPS(xmm5, ptr[r9]),  "41 0f 10 29");
	CODEGEN_TEST(xMOVAPD(ptr[rcx], xmm8), "44 0f 29 01");
	CODEGEN_TEST(xMOVUPD(ptr[r8], xmm11), "45 0f 11 18");
	CODEGEN_TEST(xMOVAPD(xmm15, ptr[r9]), "45 0f 28 39");
	CODEGEN_TEST(xMOVUPD(xmm1, ptr[rax]), "0f 10 08");
	CODEGEN_TEST(xMOVDQA(ptr[r9], xmm0),  "41 0f 29 01");
	CODEGEN_TEST(xMOVDQU(ptr[r8], xmm3),  "41 0f 11 18");
	CODEGEN_TEST(xMOVDQA(xmm8, ptr[rsi]), "44 0f 28 06");
	CODEGEN_TEST(xMOVDQU(xmm7, ptr[rcx]), "0f 10 39");
	CODEGEN_TEST(xMOVAPD(xmm4, xmm8),     "66 41 0f 28 e0");
	CODEGEN_TEST(xMOVUPD(xmm1, xmm4),     "66 0f 28 cc");
	CODEGEN_TEST(xMOVDQA(xmm9, xmm11),    "66 45 0f 6f cb");
	CODEGEN_TEST(xMOVDQU(xmm7, xmm10),    "66 41 0f 6f fa");

	CODEGEN_TEST(xBLEND.PS(xmm0, xmm1, 0x55), "66 0f 3a 0c c1 55");
	CODEGEN_TEST(xBLEND.PD(xmm8, xmm9, 0xaa), "66 45 0f 3a 0d c1 aa");
	CODEGEN_TEST(xBLEND.VPS(xmm8, ptr[r8]),   "66 45 0f 38 14 00");
	CODEGEN_TEST(xBLEND.VPD(xmm1, ptr[base]), "66 0f 38 15 0d f7 ff ff ff");
	CODEGEN_TEST(xPBLEND.W(xmm0, xmm1, 0x55), "66 0f 3a 0e c1 55");
	CODEGEN_TEST(xPBLEND.VB(xmm1, xmm2),      "66 0f 38 10 ca");

	CODEGEN_TEST(xMOVSLDUP(xmm1, xmm2),    "f3 0f 12 ca");
	CODEGEN_TEST(xMOVSLDUP(xmm1, ptr[r8]), "f3 41 0f 12 08");
	CODEGEN_TEST(xMOVSHDUP(xmm9, xmm1),    "f3 44 0f 16 c9");
	CODEGEN_TEST(xMOVSHDUP(xmm9, xmm8),    "f3 45 0f 16 c8");

	CODEGEN_TEST(xPMOVSX.BW(xmm0, ptr[rax]), "66 0f 38 20 00");
	CODEGEN_TEST(xPMOVZX.BW(xmm4, ptr[r8]),  "66 41 0f 38 30 20");
	CODEGEN_TEST(xPMOVSX.BD(xmm3, xmm4),     "66 0f 38 21 dc");
	CODEGEN_TEST(xPMOVZX.BD(xmm8, xmm3),     "66 44 0f 38 31 c3");
	CODEGEN_TEST(xPMOVSX.BQ(xmm2, xmm8),     "66 41 0f 38 22 d0");
	CODEGEN_TEST(xPMOVZX.BQ(xmm8, ptr[rax]), "66 44 0f 38 32 00");
	CODEGEN_TEST(xPMOVSX.WD(xmm4, xmm6),     "66 0f 38 23 e6");
	CODEGEN_TEST(xPMOVZX.WD(xmm6, xmm9),     "66 41 0f 38 33 f1");
	CODEGEN_TEST(xPMOVSX.WQ(xmm2, ptr[rcx]), "66 0f 38 24 11");
	CODEGEN_TEST(xPMOVZX.WQ(xmm5, xmm7),     "66 0f 38 34 ef");
	CODEGEN_TEST(xPMOVSX.DQ(xmm2, xmm3),     "66 0f 38 25 d3");
	CODEGEN_TEST(xPMOVZX.DQ(xmm4, xmm9),     "66 41 0f 38 35 e1");

	CODEGEN_TEST(xMOVD(eax, xmm1),       "66 0f 7e c8");
	CODEGEN_TEST(xMOVD(eax, xmm10),      "66 44 0f 7e d0");
	CODEGEN_TEST(xMOVD(rax, xmm1),       "66 48 0f 7e c8");
	CODEGEN_TEST(xMOVD(r10, xmm1),       "66 49 0f 7e ca");
	CODEGEN_TEST(xMOVD(rax, xmm10),      "66 4c 0f 7e d0");
	CODEGEN_TEST(xMOVD(r10, xmm10),      "66 4d 0f 7e d2");
	CODEGEN_TEST(xMOVD(ptr[r8], xmm9),   "66 45 0f 7e 08");
	CODEGEN_TEST(xMOVQ(ptr[r8], xmm9),   "66 45 0f d6 08");
	CODEGEN_TEST(xMOVDZX(xmm9, ecx),     "66 44 0f 6e c9");
	CODEGEN_TEST(xMOVDZX(xmm9, rcx),     "66 4c 0f 6e c9");
	CODEGEN_TEST(xMOVDZX(xmm9, ptr[r9]), "66 45 0f 6e 09");
	CODEGEN_TEST(xMOVQZX(xmm9, xmm4),    "f3 44 0f 7e cc");
	CODEGEN_TEST(xMOVQZX(xmm9, ptr[r8]), "f3 45 0f 7e 08");

	CODEGEN_TEST(xMOVSS(xmm1, xmm1),      "");
	CODEGEN_TEST(xMOVSS(xmm1, xmm4),      "f3 0f 10 cc");
	CODEGEN_TEST(xMOVSS(ptr[rax], xmm8),  "f3 44 0f 11 00");
	CODEGEN_TEST(xMOVSSZX(xmm8, ptr[r8]), "f3 45 0f 10 00");
	CODEGEN_TEST(xMOVSD(xmm4, xmm8),      "f2 41 0f 10 e0");
	CODEGEN_TEST(xMOVSD(ptr[rcx], xmm3),  "f2 0f 11 19");
	CODEGEN_TEST(xMOVSDZX(xmm2, ptr[r9]), "f2 41 0f 10 11");

	CODEGEN_TEST(xMOVNTDQA(xmm2, ptr[r9]), "66 41 0f 38 2a 11");
	CODEGEN_TEST(xMOVNTDQA(ptr[r9], xmm3), "66 41 0f e7 19");
	CODEGEN_TEST(xMOVNTPD(ptr[rax], xmm4), "66 0f 2b 20");
	CODEGEN_TEST(xMOVNTPS(ptr[rcx], xmm8), "44 0f 2b 01");

	CODEGEN_TEST(xMOVMSKPS(ecx, xmm8),    "41 0f 50 c8");
	CODEGEN_TEST(xMOVMSKPD(r8d, xmm2),    "66 44 0f 50 c2");
	CODEGEN_TEST(xPMOVMSKB(eax, xmm2),    "66 0f d7 c2");
	CODEGEN_TEST(xPALIGNR(xmm4, xmm8, 1), "66 41 0f 3a 0f e0 01");
	CODEGEN_TEST(xMASKMOV(xmm2, xmm9),    "66 41 0f f7 d1");

	CODEGEN_TEST(xPADD.B(xmm0, xmm1, ptr[r8]), "41 0f 28 00 66 0f fc c1");    // movaps xmm0, [r8]; paddb xmm0, xmm1
	CODEGEN_TEST(xPSUB.B(xmm0, xmm1, ptr[r8]), "66 0f 6f c1 66 41 0f f8 00"); // movdqa xmm0, xmm1; psubb xmm0, [r8]
	CODEGEN_TEST(xADD.PS(xmm0, xmm1, ptr[r8]), "41 0f 28 00 0f 58 c1");       // movaps xmm0, [r8]; addps xmm0, xmm1
	CODEGEN_TEST(xSUB.PS(xmm0, xmm1, ptr[r8]), "0f 28 c1 41 0f 5c 00");       // movaps xmm0, xmm1; subps xmm0, [r8]

	CODEGEN_TEST(xPMAX.SD(xmm2, xmm1, xmm0), "66 0f 6f d1 66 0f 38 3d d0"); // movdqa xmm2, xmm1; pmaxsd xmm2, xmm0
	CODEGEN_TEST(xPMAX.SD(xmm0, xmm1, xmm0), "66 0f 38 3d c1");             // pmaxsd xmm0, xmm1
}

TEST(CodegenTests, AVXTest)
{
	x86Emitter::use_avx = true;

	CODEGEN_TEST(xPAND(xmm3, xmm8),  "c5 b9 db db"); // => vpand xmm3, xmm8, xmm3
	CODEGEN_TEST(xPANDN(xmm4, xmm9), "c4 c1 59 df e1");
	CODEGEN_TEST(xPOR(xmm5, xmm8),   "c5 b9 eb ed"); // => vpor xmm5, xmm8, xmm5
	CODEGEN_TEST(xPXOR(xmm9, xmm4),  "c5 31 ef cc");
	CODEGEN_TEST(xPTEST(xmm2, xmm9), "c4 c2 79 17 d1");

	CODEGEN_TEST(xAND.PS(xmm3, xmm8),  "c5 b8 54 db"); // => andps xmm3, xmm8, xmm3
	CODEGEN_TEST(xOR.PS(xmm5, xmm8),   "c5 b8 56 ed"); // => orps xmm5, xmm8, xmm5
	CODEGEN_TEST(xXOR.PS(xmm9, xmm4),  "c5 30 57 cc");

	CODEGEN_TEST(xCVTDQ2PD(xmm0, ptr64[rax]), "c5 fa e6 00");
	CODEGEN_TEST(xCVTDQ2PS(xmm0, xmm8),       "c4 c1 78 5b c0");
	CODEGEN_TEST(xCVTPD2DQ(xmm8, ptr128[r8]), "c4 41 7b e6 00");
	CODEGEN_TEST(xCVTPD2PS(xmm1, xmm7),       "c5 f9 5a cf");
	CODEGEN_TEST(xCVTSD2SI(rax,  xmm1),       "c4 e1 fb 2d c1");
	CODEGEN_TEST(xCVTSD2SI(esi,  ptr64[rax]), "c5 fb 2d 30");
	CODEGEN_TEST(xCVTSD2SS(xmm3, xmm4),       "c5 e3 5a dc");
	CODEGEN_TEST(xCVTSI2SS(xmm8, ecx),        "c5 3a 2a c1");
	CODEGEN_TEST(xCVTSI2SS(xmm3, ptr32[r8]),  "c4 c1 62 2a 18");
	CODEGEN_TEST(xCVTSI2SS(xmm3, ptr64[r8]),  "c4 c1 e2 2a 18");
	CODEGEN_TEST(xCVTSS2SD(xmm8, xmm7),       "c5 3a 5a c7");
	CODEGEN_TEST(xCVTSS2SD(xmm4, ptr32[rcx]), "c5 da 5a 21");
	CODEGEN_TEST(xCVTSS2SI(eax,  xmm4),       "c5 fa 2d c4");
	CODEGEN_TEST(xCVTSS2SI(rcx,  ptr32[rax]), "c4 e1 fa 2d 08");
	CODEGEN_TEST(xCVTTPD2DQ(xmm4, xmm7),      "c5 f9 e6 e7");
	CODEGEN_TEST(xCVTTPS2DQ(xmm5, xmm3),      "c5 fa 5b eb");
	CODEGEN_TEST(xCVTTSD2SI(rdx,  xmm4),      "c4 e1 fb 2c d4");
	CODEGEN_TEST(xCVTTSS2SI(ecx,  xmm3),      "c5 fa 2c cb");

	CODEGEN_TEST(xPSLL.W(xmm8, ptr[r8]),  "c4 41 39 f1 00");
	CODEGEN_TEST(xPSLL.D(xmm0, xmm1),     "c5 f9 f2 c1");
	CODEGEN_TEST(xPSLL.Q(xmm4, ptr[rcx]), "c5 d9 f3 21");
	CODEGEN_TEST(xPSLL.W(xmm5, 2),        "c5 d1 71 f5 02");
	CODEGEN_TEST(xPSLL.D(xmm6, 3),        "c5 c9 72 f6 03");
	CODEGEN_TEST(xPSLL.Q(xmm7, 4),        "c5 c1 73 f7 04");
	CODEGEN_TEST(xPSLL.DQ(xmm8, 5),       "c4 c1 39 73 f8 05");
	CODEGEN_TEST(xPSRA.W(xmm4, xmm2),     "c5 d9 e1 e2");
	CODEGEN_TEST(xPSRA.D(xmm5, ptr[rdi]), "c5 d1 e2 2f");
	CODEGEN_TEST(xPSRA.W(xmm4, 3),        "c5 d9 71 e4 03");
	CODEGEN_TEST(xPSRA.D(xmm5, 7),        "c5 d1 72 e5 07");
	CODEGEN_TEST(xPSRL.W(xmm8, ptr[r8]),  "c4 41 39 d1 00");
	CODEGEN_TEST(xPSRL.D(xmm0, xmm1),     "c5 f9 d2 c1");
	CODEGEN_TEST(xPSRL.Q(xmm4, ptr[rcx]), "c5 d9 d3 21");
	CODEGEN_TEST(xPSRL.W(xmm5, 2),        "c5 d1 71 d5 02");
	CODEGEN_TEST(xPSRL.D(xmm6, 3),        "c5 c9 72 d6 03");
	CODEGEN_TEST(xPSRL.Q(xmm7, 4),        "c5 c1 73 d7 04");
	CODEGEN_TEST(xPSRL.DQ(xmm8, 5),       "c4 c1 39 73 d8 05");

	CODEGEN_TEST(xPADD.B(xmm1, xmm8),     "c5 b9 fc c9"); // => vpaddb xmm1, xmm8, xmm1
	CODEGEN_TEST(xPADD.W(xmm4, xmm7),     "c5 d9 fd e7");
	CODEGEN_TEST(xPADD.D(xmm2, ptr[rcx]), "c5 e9 fe 11");
	CODEGEN_TEST(xPADD.Q(xmm8, xmm2),     "c5 39 d4 c2");
	CODEGEN_TEST(xPADD.SB(xmm9, xmm8),    "c4 41 31 ec c8");
	CODEGEN_TEST(xPADD.SW(xmm2, ptr[r8]), "c4 c1 69 ed 10");
	CODEGEN_TEST(xPADD.USB(xmm3, xmm3),   "c5 e1 dc db");
	CODEGEN_TEST(xPADD.USW(xmm2, xmm9),   "c5 b1 dd d2"); // => vpaddd xmm2, xmm9, xmm2
	CODEGEN_TEST(xPSUB.B(xmm1, xmm8),     "c4 c1 71 f8 c8");
	CODEGEN_TEST(xPSUB.W(xmm4, xmm7),     "c5 d9 f9 e7");
	CODEGEN_TEST(xPSUB.D(xmm2, ptr[rcx]), "c5 e9 fa 11");
	CODEGEN_TEST(xPSUB.Q(xmm8, xmm2),     "c5 39 fb c2");
	CODEGEN_TEST(xPSUB.SB(xmm9, xmm8),    "c4 41 31 e8 c8");
	CODEGEN_TEST(xPSUB.SW(xmm2, ptr[r8]), "c4 c1 69 e9 10");
	CODEGEN_TEST(xPSUB.USB(xmm3, xmm3),   "c5 e1 d8 db");
	CODEGEN_TEST(xPSUB.USW(xmm2, xmm9),   "c4 c1 69 d9 d1");
	CODEGEN_TEST(xPMUL.LW(xmm2, xmm8),    "c5 b9 d5 d2"); // => vpmullw xmm2, xmm8, xmm2
	CODEGEN_TEST(xPMUL.HW(xmm9, ptr[r9]), "c4 41 31 e5 09");
	CODEGEN_TEST(xPMUL.HUW(xmm4, xmm3),   "c5 d9 e4 e3");
	CODEGEN_TEST(xPMUL.UDQ(xmm1, xmm7),   "c5 f1 f4 cf");
	CODEGEN_TEST(xPMUL.HRSW(xmm2, xmm4),  "c4 e2 69 0b d4");
	CODEGEN_TEST(xPMUL.LD(xmm1, xmm8),    "c4 c2 71 40 c8");
	CODEGEN_TEST(xPMUL.DQ(xmm4, xmm9),    "c4 c2 59 28 e1");

	CODEGEN_TEST(xADD.SS(xmm1, xmm8),     "c4 c1 72 58 c8");
	CODEGEN_TEST(xADD.SD(xmm4, xmm7),     "c5 db 58 e7");
	CODEGEN_TEST(xADD.PS(xmm2, ptr[rcx]), "c5 e8 58 11");
	CODEGEN_TEST(xADD.PD(xmm8, xmm2),     "c5 39 58 c2");
	CODEGEN_TEST(xSUB.SS(xmm1, xmm8),     "c4 c1 72 5c c8");
	CODEGEN_TEST(xSUB.SD(xmm4, xmm7),     "c5 db 5c e7");
	CODEGEN_TEST(xSUB.PS(xmm2, ptr[rcx]), "c5 e8 5c 11");
	CODEGEN_TEST(xSUB.PD(xmm8, xmm2),     "c5 39 5c c2");
	CODEGEN_TEST(xMUL.SS(xmm2, xmm8),     "c4 c1 6a 59 d0");
	CODEGEN_TEST(xMUL.SD(xmm9, ptr[r9]),  "c4 41 33 59 09");
	CODEGEN_TEST(xMUL.PS(xmm4, xmm3),     "c5 d8 59 e3");
	CODEGEN_TEST(xMUL.PD(xmm1, xmm8),     "c5 b9 59 c9"); // => vmulpd xmm1, xmm8, xmm1
	CODEGEN_TEST(xDIV.SS(xmm2, xmm4),     "c5 ea 5e d4");
	CODEGEN_TEST(xDIV.SD(xmm1, xmm8),     "c4 c1 73 5e c8");
	CODEGEN_TEST(xDIV.PS(xmm4, xmm9),     "c4 c1 58 5e e1");
	CODEGEN_TEST(xDIV.PD(xmm9, xmm2),     "c5 31 5e ca");

	CODEGEN_TEST(xRSQRT.PS(xmm0, xmm8),    "c4 c1 78 52 c0");
	CODEGEN_TEST(xRSQRT.SS(xmm4, ptr[r9]), "c4 c1 5a 52 21");
	CODEGEN_TEST(xRCP.PS(xmm4, ptr[rcx]),  "c5 f8 53 21");
	CODEGEN_TEST(xRCP.SS(xmm5, xmm8),      "c4 c1 52 53 e8");
	CODEGEN_TEST(xSQRT.PS(xmm4, xmm2),     "c5 f8 51 e2");
	CODEGEN_TEST(xSQRT.SS(xmm5, xmm1),     "c5 d2 51 e9");
	CODEGEN_TEST(xSQRT.PD(xmm7, ptr[rdi]), "c5 f9 51 3f");
	CODEGEN_TEST(xSQRT.SD(xmm5, xmm2),     "c5 d3 51 ea");
	CODEGEN_TEST(xANDN.PS(xmm6, ptr[rdi]), "c5 c8 55 37");
	CODEGEN_TEST(xANDN.PD(xmm3, xmm8),     "c4 c1 61 55 d8");

	CODEGEN_TEST(xPABS.B(xmm0, xmm2),     "c4 e2 79 1c c2");
	CODEGEN_TEST(xPABS.W(xmm4, xmm8),     "c4 c2 79 1d e0");
	CODEGEN_TEST(xPABS.D(xmm6, ptr[rax]), "c4 e2 79 1e 30");
	CODEGEN_TEST(xPSIGN.B(xmm0, xmm2),    "c4 e2 79 08 c2");
	CODEGEN_TEST(xPSIGN.W(xmm4, xmm8),    "c4 c2 59 09 e0");
	CODEGEN_TEST(xPSIGN.D(xmm2, ptr[r8]), "c4 c2 69 0a 10");
	CODEGEN_TEST(xPMADD.WD(xmm0, xmm8),   "c5 b9 f5 c0"); // => vpmaddwd xmm0, xmm8, xmm0
	CODEGEN_TEST(xPMADD.UBSW(xmm0, xmm8), "c4 c2 79 04 c0");

	CODEGEN_TEST(xHADD.PS(xmm1, xmm8),     "c4 c1 73 7c c8");
	CODEGEN_TEST(xHADD.PD(xmm4, ptr[r8]),  "c4 c1 59 7c 20");
	CODEGEN_TEST(xDP.PS(xmm3, xmm9, 0xf7), "c4 c3 61 40 d9 f7");
	CODEGEN_TEST(xDP.PD(xmm8, xmm4, 0x33), "c4 63 39 41 c4 33");
	CODEGEN_TEST(xROUND.PS(xmm1, xmm3, 0), "c4 e3 79 08 cb 00");
	CODEGEN_TEST(xROUND.PD(xmm3, xmm9, 1), "c4 c3 79 09 d9 01");
	CODEGEN_TEST(xROUND.SS(xmm5, xmm2, 2), "c4 e3 51 0a ea 02");
	CODEGEN_TEST(xROUND.SD(xmm8, xmm2, 3), "c4 63 39 0b c2 03");

	CODEGEN_TEST(xCMPEQ.PS(xmm4, xmm8),   "c4 c1 58 c2 e0 00");
	CODEGEN_TEST(xCMPLT.PD(xmm6, xmm9),   "c4 c1 49 c2 f1 01");
	CODEGEN_TEST(xCMPLE.SS(xmm2, xmm5),   "c5 ea c2 d5 02");
	CODEGEN_TEST(xCMPNE.SD(xmm1, xmm9),   "c4 c1 73 c2 c9 04");
	CODEGEN_TEST(xMIN.PS(xmm2, xmm8),     "c4 c1 68 5d d0");
	CODEGEN_TEST(xMIN.PD(xmm3, ptr[rax]), "c5 e1 5d 18");
	CODEGEN_TEST(xMIN.SS(xmm8, xmm2),     "c5 3a 5d c2");
	CODEGEN_TEST(xMIN.SD(xmm1, ptr[r8]),  "c4 c1 73 5d 08");
	CODEGEN_TEST(xMAX.PS(xmm2, xmm8),     "c4 c1 68 5f d0");
	CODEGEN_TEST(xMAX.PD(xmm3, ptr[rax]), "c5 e1 5f 18");
	CODEGEN_TEST(xMAX.SS(xmm8, xmm2),     "c5 3a 5f c2");
	CODEGEN_TEST(xMAX.SD(xmm1, ptr[r8]),  "c4 c1 73 5f 08");
	CODEGEN_TEST(xCOMI.SS(xmm2, xmm8),    "c4 c1 78 2f d0");
	CODEGEN_TEST(xCOMI.SD(xmm3, ptr[r8]), "c4 c1 79 2f 18");
	CODEGEN_TEST(xUCOMI.SS(xmm8, xmm2),   "c5 78 2e c2");
	CODEGEN_TEST(xUCOMI.SD(xmm2, xmm3),   "c5 f9 2e d3");

	CODEGEN_TEST(xPCMP.EQB(xmm0, xmm8),    "c5 b9 74 c0"); // => vpcmpeqb xmm0, xmm8, xmm0
	CODEGEN_TEST(xPCMP.EQW(xmm4, ptr[r8]), "c4 c1 59 75 20");
	CODEGEN_TEST(xPCMP.EQD(xmm3, xmm4),    "c5 e1 76 dc");
	CODEGEN_TEST(xPCMP.GTB(xmm0, xmm8),    "c4 c1 79 64 c0");
	CODEGEN_TEST(xPCMP.GTW(xmm4, ptr[r8]), "c4 c1 59 65 20");
	CODEGEN_TEST(xPCMP.GTD(xmm3, xmm4),    "c5 e1 66 dc");
	CODEGEN_TEST(xPMIN.UB(xmm0, xmm8),     "c5 b9 da c0"); // => vpminub xmm0, xmm8, xmm0
	CODEGEN_TEST(xPMIN.SW(xmm4, ptr[rcx]), "c5 d9 ea 21");
	CODEGEN_TEST(xPMIN.SB(xmm3, xmm4),     "c4 e2 61 38 dc");
	CODEGEN_TEST(xPMIN.SD(xmm8, xmm3),     "c4 62 39 39 c3");
	CODEGEN_TEST(xPMIN.UW(xmm4, xmm9),     "c4 c2 59 3a e1");
	CODEGEN_TEST(xPMIN.UD(xmm2, ptr[r10]), "c4 c2 69 3b 12");
	CODEGEN_TEST(xPMAX.UB(xmm0, xmm8),     "c5 b9 de c0"); // => vpmaxub xmm0, xmm8, xmm0
	CODEGEN_TEST(xPMAX.SW(xmm4, ptr[rcx]), "c5 d9 ee 21");
	CODEGEN_TEST(xPMAX.SB(xmm3, xmm4),     "c4 e2 61 3c dc");
	CODEGEN_TEST(xPMAX.SD(xmm8, xmm3),     "c4 62 39 3d c3");
	CODEGEN_TEST(xPMAX.UW(xmm4, xmm9),     "c4 c2 59 3e e1");
	CODEGEN_TEST(xPMAX.UD(xmm2, ptr[r10]), "c4 c2 69 3f 12");

	CODEGEN_TEST(xSHUF.PS(xmm0, xmm8, 0x33),       "c4 c1 78 c6 c0 33");
	CODEGEN_TEST(xSHUF.PS(xmm0, ptr[r8], 0),       "c4 c1 78 c6 00 00");
	CODEGEN_TEST(xSHUF.PD(xmm3, ptr[rcx], 0),      "c5 e1 c6 19 00");
	CODEGEN_TEST(xSHUF.PD(xmm3, xmm2, 2),          "c5 e1 c6 da 02");
	CODEGEN_TEST(xINSERTPS(xmm1, xmm2, 0x87),      "c4 e3 71 21 ca 87");
	CODEGEN_TEST(xINSERTPS(xmm1, ptr32[r8], 0x87), "c4 c3 71 21 08 87");
	CODEGEN_TEST(xEXTRACTPS(eax, xmm2, 2),         "c4 e3 79 17 d0 02");
	CODEGEN_TEST(xEXTRACTPS(ptr32[r9], xmm3, 3),   "c4 c3 79 17 19 03");
	CODEGEN_TEST(xEXTRACTPS(ptr32[base], xmm1, 2), "c4 e3 79 17 0d f6 ff ff ff 02");

	CODEGEN_TEST(xPSHUF.D(xmm2, ptr[r8], 0),    "c4 c1 79 70 10 00");
	CODEGEN_TEST(xPSHUF.LW(xmm3, xmm8, 1),      "c4 c1 7b 70 d8 01");
	CODEGEN_TEST(xPSHUF.HW(xmm4, xmm2, 8),      "c5 fa 70 e2 08");
	CODEGEN_TEST(xPSHUF.B(xmm2, ptr[r8]),       "c4 c2 69 00 10");
	CODEGEN_TEST(xPINSR.B(xmm1, ebx, 1),        "c4 e3 71 20 cb 01");
	CODEGEN_TEST(xPINSR.W(xmm1, ebx, 1),        "c5 f1 c4 cb 01");
	CODEGEN_TEST(xPINSR.D(xmm1, ebx, 1),        "c4 e3 71 22 cb 01");
	CODEGEN_TEST(xPINSR.Q(xmm1, rbx, 1),        "c4 e3 f1 22 cb 01");
	CODEGEN_TEST(xPINSR.B(xmm9, ptr8[rax], 1),  "c4 63 31 20 08 01");
	CODEGEN_TEST(xPINSR.W(xmm9, ptr16[rax], 1), "c5 31 c4 08 01");
	CODEGEN_TEST(xPINSR.D(xmm9, ptr32[rax], 1), "c4 63 31 22 08 01");
	CODEGEN_TEST(xPINSR.Q(xmm9, ptr64[rax], 1), "c4 63 b1 22 08 01");
	CODEGEN_TEST(xPEXTR.B(ebx, xmm1, 1),        "c4 e3 79 14 cb 01");
	CODEGEN_TEST(xPEXTR.W(ebx, xmm1, 1),        "c5 f9 c5 d9 01");
	CODEGEN_TEST(xPEXTR.D(ebx, xmm1, 1),        "c4 e3 79 16 cb 01");
	CODEGEN_TEST(xPEXTR.Q(rbx, xmm1, 1),        "c4 e3 f9 16 cb 01");
	CODEGEN_TEST(xPEXTR.B(ptr8[rax],  xmm9, 1), "c4 63 79 14 08 01");
	CODEGEN_TEST(xPEXTR.W(ptr16[rax], xmm9, 1), "c4 63 79 15 08 01");
	CODEGEN_TEST(xPEXTR.D(ptr32[rax], xmm9, 1), "c4 63 79 16 08 01");
	CODEGEN_TEST(xPEXTR.Q(ptr64[rax], xmm9, 1), "c4 63 f9 16 08 01");

	CODEGEN_TEST(xPUNPCK.LBW(xmm1, xmm2),    "c5 f1 60 ca");
	CODEGEN_TEST(xPUNPCK.LWD(xmm1, ptr[r8]), "c4 c1 71 61 08");
	CODEGEN_TEST(xPUNPCK.LDQ(xmm1, xmm8),    "c4 c1 71 62 c8");
	CODEGEN_TEST(xPUNPCK.LQDQ(xmm8, xmm2),   "c5 39 6c c2");
	CODEGEN_TEST(xPUNPCK.HBW(xmm1, xmm2),    "c5 f1 68 ca");
	CODEGEN_TEST(xPUNPCK.HWD(xmm1, ptr[r8]), "c4 c1 71 69 08");
	CODEGEN_TEST(xPUNPCK.HDQ(xmm1, xmm8),    "c4 c1 71 6a c8");
	CODEGEN_TEST(xPUNPCK.HQDQ(xmm8, xmm2),   "c5 39 6d c2");
	CODEGEN_TEST(xPACK.SSWB(xmm1, xmm2),     "c5 f1 63 ca");
	CODEGEN_TEST(xPACK.SSDW(xmm1, ptr[rax]), "c5 f1 6b 08");
	CODEGEN_TEST(xPACK.USWB(xmm1, xmm8),     "c4 c1 71 67 c8");
	CODEGEN_TEST(xPACK.USDW(xmm8, xmm2),     "c4 62 39 2b c2");
	CODEGEN_TEST(xUNPCK.LPS(xmm1, xmm2),     "c5 f0 14 ca");
	CODEGEN_TEST(xUNPCK.LPD(xmm1, ptr[r8]),  "c4 c1 71 14 08");
	CODEGEN_TEST(xUNPCK.HPS(xmm1, xmm8),     "c4 c1 70 15 c8");
	CODEGEN_TEST(xUNPCK.HPD(xmm8, xmm2),     "c5 39 15 c2");

	CODEGEN_TEST(xMOVH.PS(ptr[r8], xmm2),      "c4 c1 78 17 10");
	CODEGEN_TEST(xMOVH.PD(xmm2, ptr[rcx]),     "c5 e9 16 11");
	CODEGEN_TEST(xMOVL.PS(xmm8, ptr[rax]),     "c5 38 12 00");
	CODEGEN_TEST(xMOVL.PD(ptr[r8 + r9], xmm9), "c4 01 79 13 0c 08");
	CODEGEN_TEST(xMOVHL.PS(xmm4, xmm9),        "c4 c1 58 12 e1");
	CODEGEN_TEST(xMOVLH.PS(xmm2, xmm1),        "c5 e8 16 d1");

	CODEGEN_TEST(xMOVAPS(xmm0, xmm8),     "c5 78 29 c0");
	CODEGEN_TEST(xMOVUPS(xmm8, xmm3),     "c5 78 28 c3");
	CODEGEN_TEST(xMOVAPS(ptr[r8], xmm4),  "c4 c1 78 29 20");
	CODEGEN_TEST(xMOVUPS(ptr[rax], xmm5), "c5 f8 11 28");
	CODEGEN_TEST(xMOVAPS(xmm8, ptr[r8]),  "c4 41 78 28 00");
	CODEGEN_TEST(xMOVUPS(xmm5, ptr[r9]),  "c4 c1 78 10 29");
	CODEGEN_TEST(xMOVAPD(xmm4, xmm8),     "c5 79 29 c4");
	CODEGEN_TEST(xMOVUPD(xmm1, xmm4),     "c5 f9 28 cc");
	CODEGEN_TEST(xMOVAPD(ptr[rcx], xmm8), "c5 79 29 01");
	CODEGEN_TEST(xMOVUPD(ptr[r8], xmm11), "c4 41 79 11 18");
	CODEGEN_TEST(xMOVAPD(xmm15, ptr[r9]), "c4 41 79 28 39");
	CODEGEN_TEST(xMOVUPD(xmm1, ptr[rax]), "c5 f9 10 08");
	CODEGEN_TEST(xMOVDQA(xmm9, xmm11),    "c4 41 79 6f cb");
	CODEGEN_TEST(xMOVDQU(xmm7, xmm10),    "c5 79 7f d7");
	CODEGEN_TEST(xMOVDQA(ptr[r9], xmm0),  "c4 c1 79 7f 01");
	CODEGEN_TEST(xMOVDQU(ptr[r8], xmm3),  "c4 c1 7a 7f 18");
	CODEGEN_TEST(xMOVDQA(xmm8, ptr[rsi]), "c5 79 6f 06");
	CODEGEN_TEST(xMOVDQU(xmm7, ptr[rcx]), "c5 fa 6f 39");

	CODEGEN_TEST(xBLEND.PS(xmm0, xmm1, 0x55), "c4 e3 79 0c c1 55");
	CODEGEN_TEST(xBLEND.PD(xmm8, xmm9, 0xaa), "c4 43 39 0d c1 aa");
	CODEGEN_TEST(xBLEND.VPS(xmm8, ptr[r8]),   "c4 43 39 4a 00 00");
	CODEGEN_TEST(xBLEND.VPD(xmm1, ptr[base]), "c4 e3 71 4b 0d f6 ff ff ff 00");
	CODEGEN_TEST(xPBLEND.W(xmm0, xmm1, 0x55), "c4 e3 79 0e c1 55");
	CODEGEN_TEST(xPBLEND.VB(xmm1, xmm2),      "c4 e3 71 4c ca 00");

	CODEGEN_TEST(xPMOVSX.BW(xmm0, ptr[rax]), "c4 e2 79 20 00");
	CODEGEN_TEST(xPMOVZX.BW(xmm4, ptr[r8]),  "c4 c2 79 30 20");
	CODEGEN_TEST(xPMOVSX.BD(xmm3, xmm4),     "c4 e2 79 21 dc");
	CODEGEN_TEST(xPMOVZX.BD(xmm8, xmm3),     "c4 62 79 31 c3");
	CODEGEN_TEST(xPMOVSX.BQ(xmm2, xmm8),     "c4 c2 79 22 d0");
	CODEGEN_TEST(xPMOVZX.BQ(xmm8, ptr[rax]), "c4 62 79 32 00");
	CODEGEN_TEST(xPMOVSX.WD(xmm4, xmm6),     "c4 e2 79 23 e6");
	CODEGEN_TEST(xPMOVZX.WD(xmm6, xmm9),     "c4 c2 79 33 f1");
	CODEGEN_TEST(xPMOVSX.WQ(xmm2, ptr[rcx]), "c4 e2 79 24 11");
	CODEGEN_TEST(xPMOVZX.WQ(xmm5, xmm7),     "c4 e2 79 34 ef");
	CODEGEN_TEST(xPMOVSX.DQ(xmm2, xmm3),     "c4 e2 79 25 d3");
	CODEGEN_TEST(xPMOVZX.DQ(xmm4, xmm9),     "c4 c2 79 35 e1");

	CODEGEN_TEST(xMOVSLDUP(xmm1, xmm2),    "c5 fa 12 ca");
	CODEGEN_TEST(xMOVSLDUP(xmm1, ptr[r8]), "c4 c1 7a 12 08");
	CODEGEN_TEST(xMOVSHDUP(xmm9, xmm1),    "c5 7a 16 c9");
	CODEGEN_TEST(xMOVSHDUP(xmm9, xmm8),    "c4 41 7a 16 c8");

	CODEGEN_TEST(xMOVD(eax, xmm1),       "c5 f9 7e c8");
	CODEGEN_TEST(xMOVD(eax, xmm10),      "c5 79 7e d0");
	CODEGEN_TEST(xMOVD(rax, xmm1),       "c4 e1 f9 7e c8");
	CODEGEN_TEST(xMOVD(r10, xmm1),       "c4 c1 f9 7e ca");
	CODEGEN_TEST(xMOVD(rax, xmm10),      "c4 61 f9 7e d0");
	CODEGEN_TEST(xMOVD(r10, xmm10),      "c4 41 f9 7e d2");
	CODEGEN_TEST(xMOVD(ptr[r8], xmm9),   "c4 41 79 7e 08");
	CODEGEN_TEST(xMOVQ(ptr[r8], xmm9),   "c4 41 79 d6 08");
	CODEGEN_TEST(xMOVDZX(xmm9, ecx),     "c5 79 6e c9");
	CODEGEN_TEST(xMOVDZX(xmm9, rcx),     "c4 61 f9 6e c9");
	CODEGEN_TEST(xMOVDZX(xmm9, ptr[r9]), "c4 41 79 6e 09");
	CODEGEN_TEST(xMOVQZX(xmm9, xmm4),    "c5 7a 7e cc");
	CODEGEN_TEST(xMOVQZX(xmm9, ptr[r8]), "c4 41 7a 7e 08");

	CODEGEN_TEST(xMOVSS(xmm1, xmm1),      "");
	CODEGEN_TEST(xMOVSS(xmm1, xmm4),      "c5 f2 10 cc");
	CODEGEN_TEST(xMOVSS(ptr[rax], xmm8),  "c5 7a 11 00");
	CODEGEN_TEST(xMOVSSZX(xmm8, ptr[r8]), "c4 41 7a 10 00");
	CODEGEN_TEST(xMOVSD(xmm4, xmm8),      "c5 5b 11 c4");
	CODEGEN_TEST(xMOVSD(ptr[rcx], xmm3),  "c5 fb 11 19");
	CODEGEN_TEST(xMOVSDZX(xmm2, ptr[r9]), "c4 c1 7b 10 11");

	CODEGEN_TEST(xMOVNTDQA(xmm2, ptr[r9]), "c4 c2 79 2a 11");
	CODEGEN_TEST(xMOVNTDQA(ptr[r9], xmm3), "c4 c1 79 e7 19");
	CODEGEN_TEST(xMOVNTPD(ptr[rax], xmm4), "c5 f9 2b 20");
	CODEGEN_TEST(xMOVNTPS(ptr[rcx], xmm8), "c5 78 2b 01");

	CODEGEN_TEST(xMOVMSKPS(ecx, xmm8),    "c4 c1 78 50 c8");
	CODEGEN_TEST(xMOVMSKPD(r8d, xmm2),    "c5 79 50 c2");
	CODEGEN_TEST(xPMOVMSKB(eax, xmm2),    "c5 f9 d7 c2");
	CODEGEN_TEST(xPALIGNR(xmm4, xmm8, 1), "c4 c3 59 0f e0 01");
	CODEGEN_TEST(xMASKMOV(xmm2, xmm9),    "c4 c1 79 f7 d1");

	CODEGEN_TEST(xPADD.B(xmm0, xmm1, ptr[r8]), "c4 c1 71 fc 00");
	CODEGEN_TEST(xPSUB.B(xmm0, xmm1, ptr[r8]), "c4 c1 71 f8 00");
	CODEGEN_TEST(xADD.PS(xmm0, xmm1, ptr[r8]), "c4 c1 70 58 00");
	CODEGEN_TEST(xSUB.PS(xmm0, xmm1, ptr[r8]), "c4 c1 70 5c 00");

	CODEGEN_TEST(xPMAX.SD(xmm2, xmm1, xmm0), "c4 e2 71 3d d0");
	CODEGEN_TEST(xPMAX.SD(xmm0, xmm1, xmm0), "c4 e2 71 3d c0");
}

TEST(CodegenTests, AVX256Test)
{
	x86Emitter::use_avx = true;

	CODEGEN_TEST(xMOVAPS(ymm0, ymm1), "c5 fc 28 c1");
	CODEGEN_TEST(xMOVAPS(ymm0, ptr32[rdi]), "c5 fc 28 07");
	CODEGEN_TEST(xMOVAPS(ptr32[rdi], ymm0), "c5 fc 29 07");
	CODEGEN_TEST(xMOVUPS(ymm0, ptr32[rdi]), "c5 fc 10 07");
	CODEGEN_TEST(xMOVUPS(ptr32[rdi], ymm0), "c5 fc 11 07");

	CODEGEN_TEST(xVZEROUPPER(), "c5 f8 77");

	CODEGEN_TEST(xADD.PS(ymm0, ymm1, ymm2), "c5 f4 58 c2");
	CODEGEN_TEST(xADD.PD(ymm0, ymm1, ymm2), "c5 f5 58 c2");
	CODEGEN_TEST(xSUB.PS(ymm0, ymm1, ymm2), "c5 f4 5c c2");
	CODEGEN_TEST(xSUB.PD(ymm0, ymm1, ymm2), "c5 f5 5c c2");
	CODEGEN_TEST(xMUL.PS(ymm0, ymm1, ymm2), "c5 f4 59 c2");
	CODEGEN_TEST(xMUL.PD(ymm0, ymm1, ymm2), "c5 f5 59 c2");
	CODEGEN_TEST(xDIV.PS(ymm0, ymm1, ymm2), "c5 f4 5e c2");
	CODEGEN_TEST(xDIV.PD(ymm0, ymm1, ymm2), "c5 f5 5e c2");

	CODEGEN_TEST(xCMPEQ.PS(ymm0, ymm1, ymm2), "c5 f4 c2 c2 00");
	CODEGEN_TEST(xCMPEQ.PD(ymm0, ymm1, ymm2), "c5 f5 c2 c2 00");
	CODEGEN_TEST(xCMPLE.PS(ymm0, ymm1, ymm2), "c5 f4 c2 c2 02");
	CODEGEN_TEST(xCMPLE.PD(ymm0, ymm1, ymm2), "c5 f5 c2 c2 02");

	CODEGEN_TEST(xPCMP.EQB(ymm0, ymm1, ymm2), "c5 f5 74 c2");
	CODEGEN_TEST(xPCMP.EQW(ymm0, ymm1, ymm2), "c5 f5 75 c2");
	CODEGEN_TEST(xPCMP.EQD(ymm0, ymm1, ymm2), "c5 f5 76 c2");
	CODEGEN_TEST(xPCMP.GTB(ymm0, ymm1, ymm2), "c5 f5 64 c2");
	CODEGEN_TEST(xPCMP.GTW(ymm0, ymm1, ymm2), "c5 f5 65 c2");
	CODEGEN_TEST(xPCMP.GTD(ymm0, ymm1, ymm2), "c5 f5 66 c2");

	CODEGEN_TEST(xPAND(ymm0, ymm1, ymm2), "c5 f5 db c2");
	CODEGEN_TEST(xPANDN(ymm0, ymm1, ymm2), "c5 f5 df c2");
	CODEGEN_TEST(xPOR(ymm0, ymm1, ymm2), "c5 f5 eb c2");
	CODEGEN_TEST(xPXOR(ymm0, ymm1, ymm2), "c5 f5 ef c2");

	CODEGEN_TEST(xMOVMSKPS(eax, ymm1), "c5 fc 50 c1");
	CODEGEN_TEST(xMOVMSKPD(eax, ymm1), "c5 fd 50 c1");
}

TEST(CodegenTests, Extended8BitTest)
{
	CODEGEN_TEST(xSETL(al), "0f 9c c0");
	CODEGEN_TEST(xSETL(cl), "0f 9c c1");
	CODEGEN_TEST(xSETL(dl), "0f 9c c2");
	CODEGEN_TEST(xSETL(bl), "0f 9c c3");
	CODEGEN_TEST(xSETL(spl), "40 0f 9c c4");
	CODEGEN_TEST(xSETL(bpl), "40 0f 9c c5");
	CODEGEN_TEST(xSETL(sil), "40 0f 9c c6");
	CODEGEN_TEST(xSETL(dil), "40 0f 9c c7");
	CODEGEN_TEST(xSETL(r8b), "41 0f 9c c0");
	CODEGEN_TEST(xSETL(r9b), "41 0f 9c c1");
	CODEGEN_TEST(xSETL(r10b), "41 0f 9c c2");
	CODEGEN_TEST(xSETL(r11b), "41 0f 9c c3");
	CODEGEN_TEST(xSETL(r12b), "41 0f 9c c4");
	CODEGEN_TEST(xSETL(r13b), "41 0f 9c c5");
	CODEGEN_TEST(xSETL(r14b), "41 0f 9c c6");
	CODEGEN_TEST(xSETL(r15b), "41 0f 9c c7");
}
