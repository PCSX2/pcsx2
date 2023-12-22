// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "Hw.h"

#include "common/SingleRegisterTypes.h"

// hw read functions
template< uint page > extern mem8_t  hwRead8  (u32 mem);
template< uint page > extern mem16_t hwRead16 (u32 mem);
template< uint page > extern mem32_t hwRead32 (u32 mem);
template< uint page > extern mem64_t hwRead64 (u32 mem);
template< uint page > extern RETURNS_R128       hwRead128(u32 mem);

// Internal hwRead32 which does not log reads, used by hwWrite8/16 to perform
// read-modify-write operations.
template< uint page, bool intcstathack >
extern mem32_t _hwRead32(u32 mem);

extern mem16_t hwRead16_page_0F_INTC_HACK(u32 mem);
extern mem32_t hwRead32_page_0F_INTC_HACK(u32 mem);


// hw write functions
template<uint page> extern void hwWrite8  (u32 mem, u8  value);
template<uint page> extern void hwWrite16 (u32 mem, u16 value);

template<uint page> extern void hwWrite32 (u32 mem, mem32_t value);
template<uint page> extern void hwWrite64 (u32 mem, mem64_t srcval);
template<uint page> extern void TAKES_R128 hwWrite128(u32 mem, r128 srcval);

// --------------------------------------------------------------------------------------
//  Hardware FIFOs (128 bit access only!)
// --------------------------------------------------------------------------------------
// VIF0   -- 0x10004000 -- eeHw[0x4000]
// VIF1   -- 0x10005000 -- eeHw[0x5000]
// GIF    -- 0x10006000 -- eeHw[0x6000]
// IPUout -- 0x10007000 -- eeHw[0x7000]
// IPUin  -- 0x10007010 -- eeHw[0x7010]

extern void ReadFIFO_VIF1(mem128_t* out);
extern void ReadFIFO_IPUout(mem128_t* out);

extern void WriteFIFO_VIF0(const mem128_t* value);
extern void WriteFIFO_VIF1(const mem128_t* value);
extern void WriteFIFO_GIF(const mem128_t* value);
extern void WriteFIFO_IPUin(const mem128_t* value);
