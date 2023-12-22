// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "vtlb.h"

#define PSM(mem)	(vtlb_GetPhyPtr((mem)&0x1fffffff)) //pcsx2 is a competition.The one with most hacks wins :D

#define psHs8(mem)	(*(s8 *)&eeHw[(mem) & 0xffff])
#define psHs16(mem)	(*(s16*)&eeHw[(mem) & 0xffff])
#define psHs32(mem)	(*(s32*)&eeHw[(mem) & 0xffff])
#define psHs64(mem)	(*(s64*)&eeHw[(mem) & 0xffff])
#define psHu8(mem)	(*(u8 *)&eeHw[(mem) & 0xffff])
#define psHu16(mem)	(*(u16*)&eeHw[(mem) & 0xffff])
#define psHu32(mem)	(*(u32*)&eeHw[(mem) & 0xffff])
#define psHu64(mem)	(*(u64*)&eeHw[(mem) & 0xffff])
#define psHu128(mem)(*(u128*)&eeHw[(mem) & 0xffff])

#define psMs8(mem)	(*(s8 *)&eeMem->Main[(mem) & 0x1ffffff])
#define psMs16(mem)	(*(s16*)&eeMem->Main[(mem) & 0x1ffffff])
#define psMs32(mem)	(*(s32*)&eeMem->Main[(mem) & 0x1ffffff])
#define psMs64(mem)	(*(s64*)&eeMem->Main[(mem) & 0x1ffffff])
#define psMu8(mem)	(*(u8 *)&eeMem->Main[(mem) & 0x1ffffff])
#define psMu16(mem)	(*(u16*)&eeMem->Main[(mem) & 0x1ffffff])
#define psMu32(mem)	(*(u32*)&eeMem->Main[(mem) & 0x1ffffff])
#define psMu64(mem)	(*(u64*)&eeMem->Main[(mem) & 0x1ffffff])

#define psRs8(mem)	(*(s8 *)&eeMem->ROM[(mem) & 0x3fffff])
#define psRs16(mem)	(*(s16*)&eeMem->ROM[(mem) & 0x3fffff])
#define psRs32(mem)	(*(s32*)&eeMem->ROM[(mem) & 0x3fffff])
#define psRs64(mem)	(*(s64*)&eeMem->ROM[(mem) & 0x3fffff])
#define psRu8(mem)	(*(u8 *)&eeMem->ROM[(mem) & 0x3fffff])
#define psRu16(mem)	(*(u16*)&eeMem->ROM[(mem) & 0x3fffff])
#define psRu32(mem)	(*(u32*)&eeMem->ROM[(mem) & 0x3fffff])
#define psRu64(mem)	(*(u64*)&eeMem->ROM[(mem) & 0x3fffff])

#define psR1s8(mem)		(*(s8 *)&eeMem->ROM1[(mem) & 0x3fffff])
#define psR1s16(mem)	(*(s16*)&eeMem->ROM1[(mem) & 0x3fffff])
#define psR1s32(mem)	(*(s32*)&eeMem->ROM1[(mem) & 0x3fffff])
#define psR1s64(mem)	(*(s64*)&eeMem->ROM1[(mem) & 0x3fffff])
#define psR1u8(mem)		(*(u8 *)&eeMem->ROM1[(mem) & 0x3fffff])
#define psR1u16(mem)	(*(u16*)&eeMem->ROM1[(mem) & 0x3fffff])
#define psR1u32(mem)	(*(u32*)&eeMem->ROM1[(mem) & 0x3fffff])
#define psR1u64(mem)	(*(u64*)&eeMem->ROM1[(mem) & 0x3fffff])

#define psR2s8(mem)		(*(s8 *)&eeMem->ROM2[(mem) & 0x7ffff])
#define psR2s16(mem)	(*(s16*)&eeMem->ROM2[(mem) & 0x7ffff])
#define psR2s32(mem)	(*(s32*)&eeMem->ROM2[(mem) & 0x7ffff])
#define psR2s64(mem)	(*(s64*)&eeMem->ROM2[(mem) & 0x7ffff])
#define psR2u8(mem)		(*(u8 *)&eeMem->ROM2[(mem) & 0x7ffff])
#define psR2u16(mem)	(*(u16*)&eeMem->ROM2[(mem) & 0x7ffff])
#define psR2u32(mem)	(*(u32*)&eeMem->ROM2[(mem) & 0x7ffff])
#define psR2u64(mem)	(*(u64*)&eeMem->ROM2[(mem) & 0x7ffff])

#define psSs32(mem)		(*(s32 *)&eeMem->Scratch[(mem) & 0x3fff])
#define psSs64(mem)		(*(s64 *)&eeMem->Scratch[(mem) & 0x3fff])
#define psSs128(mem)	(*(s128*)&eeMem->Scratch[(mem) & 0x3fff])
#define psSu32(mem)		(*(u32 *)&eeMem->Scratch[(mem) & 0x3fff])
#define psSu64(mem)		(*(u64 *)&eeMem->Scratch[(mem) & 0x3fff])
#define psSu128(mem)	(*(u128*)&eeMem->Scratch[(mem) & 0x3fff])

extern void memAllocate();
extern void memReset();
extern void memRelease();

extern void memSetKernelMode();
//extern void memSetSupervisorMode();
extern void memSetUserMode();
extern void memSetPageAddr(u32 vaddr, u32 paddr);
extern void memClearPageAddr(u32 vaddr);
extern void memBindConditionalHandlers();

extern void memMapVUmicro();

#define memRead8 vtlb_memRead<mem8_t>
#define memRead16 vtlb_memRead<mem16_t>
#define memRead32 vtlb_memRead<mem32_t>
#define memRead64 vtlb_memRead<mem64_t>

#define memWrite8 vtlb_memWrite<mem8_t>
#define memWrite16 vtlb_memWrite<mem16_t>
#define memWrite32 vtlb_memWrite<mem32_t>
#define memWrite64 vtlb_memWrite<mem64_t>

static __fi void memRead128(u32 mem, mem128_t* out) { r128_store(out, vtlb_memRead128(mem)); }
static __fi void memRead128(u32 mem, mem128_t& out) { memRead128(mem, &out); }

static __fi void memWrite128(u32 mem, const mem128_t* val)	{ vtlb_memWrite128(mem, r128_load(val)); }
static __fi void memWrite128(u32 mem, const mem128_t& val)	{ vtlb_memWrite128(mem, r128_load(&val)); }

extern void ba0W16(u32 mem, u16 value);
extern u16 ba0R16(u32 mem);
