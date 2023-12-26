// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "Vif.h"
#include "VU.h"

#include "common/emitter/x86emitter.h"

using namespace x86Emitter;

// newVif_HashBucket.h uses this typedef, so it has to be declared first.
typedef u32  (*nVifCall)(void*, const void*);
typedef void (*nVifrecCall)(uptr dest, uptr src);

#include "newVif_HashBucket.h"

extern void  mVUmergeRegs(const xRegisterSSE& dest, const xRegisterSSE& src,  int xyzw, bool modXYZW = 0);
extern void  mVUsaveReg(const xRegisterSSE& reg, xAddressVoid ptr, int xyzw, bool modXYZW);
extern void _nVifUnpack  (int idx, const u8* data, uint mode, bool isFill);
extern void  dVifReset   (int idx);
extern void  dVifClose   (int idx);
extern void  dVifRelease (int idx);
extern void  VifUnpackSSE_Init();

_vifT extern void dVifUnpack(const u8* data, bool isFill);

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

struct nVifStruct
{
	// Buffer for partial transfers (should always be first to ensure alignment)
	// Maximum buffer size is 256 (vifRegs.Num max range) * 16 (quadword)
	alignas(16) u8 buffer[256*16];
	u32            bSize; // Size of 'buffer'

	// VIF0 or VIF1 - provided for debugging helpfulness only, and is generally unused.
	// (templates are used for most or all VIF indexing)
	u32                     idx;

	u8*                     recWritePtr; // current write pos into the reserve
	u8*                     recEndPtr;

	HashBucket              vifBlocks;   // Vif Blocks


	nVifStruct() = default;
};

extern void resetNewVif(int idx);

alignas(16) extern nVifStruct nVif[2];
alignas(16) extern nVifCall nVifUpk[(2 * 2 * 16) * 4]; // ([USN][Masking][Unpack Type]) [curCycle]
alignas(16) extern u32      nVifMask[3][4][4];         // [MaskNumber][CycleNumber][Vector]

static constexpr bool newVifDynaRec = 1; // Use code in newVif_Dynarec.inl
