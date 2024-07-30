// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "Vif.h"
#include "Vif_HashBucket.h"
#include "VU.h"

typedef u32 (*nVifCall)(void*, const void*);
typedef void (*nVifrecCall)(uptr dest, uptr src);

extern void _nVifUnpack(int idx, const u8* data, uint mode, bool isFill);
extern void dVifReset(int idx);
extern void dVifRelease(int idx);
extern void VifUnpackSSE_Init();

_vifT extern void dVifUnpack(const u8* data, bool isFill);

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
