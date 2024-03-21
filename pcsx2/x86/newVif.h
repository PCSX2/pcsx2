// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "Vif_Dynarec.h"

#include "common/emitter/x86emitter.h"

using namespace x86Emitter;

extern void  mVUmergeRegs(const xRegisterSSE& dest, const xRegisterSSE& src,  int xyzw, bool modXYZW = 0);
extern void  mVUsaveReg(const xRegisterSSE& reg, xAddressVoid ptr, int xyzw, bool modXYZW);

#define VUFT VIFUnpackFuncTable
#define _v0 0
#define _v1 0x55
#define _v2 0xaa
#define _v3 0xff
#define xmmCol0 xmm2
#define xmmCol1 xmm3
#define xmmCol2 xmm4
#define xmmCol3 xmm5
#define xmmRow  xmm6
#define xmmTemp xmm7
