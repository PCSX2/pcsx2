// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "Common.h"

#include "common/SingleRegisterTypes.h"

void resetCache();
void writeCache8(u32 mem, u8 value);
void writeCache16(u32 mem, u16 value);
void writeCache32(u32 mem, u32 value);
void writeCache64(u32 mem, const u64 value);
void writeCache128(u32 mem, const mem128_t* value);
u8 readCache8(u32 mem);
u16 readCache16(u32 mem);
u32 readCache32(u32 mem);
u64 readCache64(u32 mem);
RETURNS_R128 readCache128(u32 mem);
