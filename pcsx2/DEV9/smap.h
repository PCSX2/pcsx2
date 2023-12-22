// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "DEV9.h"

u8 smap_read8(u32 addr);
u16 smap_read16(u32 addr);
u32 smap_read32(u32 addr);

void smap_write8(u32 addr, u8 value);
void smap_write16(u32 addr, u16 value);
void smap_write32(u32 addr, u32 value);

void smap_readDMA8Mem(u32* pMem, int size);
void smap_writeDMA8Mem(u32* pMem, int size);
void smap_async(u32 cycles);
