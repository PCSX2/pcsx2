// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

// Our main memory storage, and defines for accessing it.
extern s8* fwregs;
#define fwRs32(mem) (*(s32*)&fwregs[(mem)&0xffff])
#define fwRu32(mem) (*(u32*)&fwregs[(mem)&0xffff])

//PHY Access Address for ease of use :P
#define PHYACC fwRu32(0x8414)

s32 FWopen();
void FWclose();
void PHYWrite();
void PHYRead();
u32 FWread32(u32 addr);
void FWwrite32(u32 addr, u32 value);
