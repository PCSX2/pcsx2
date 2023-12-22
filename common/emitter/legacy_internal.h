// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/emitter/internal.h"

//------------------------------------------------------------------
// Legacy Helper Macros and Functions (depreciated)
//------------------------------------------------------------------

#define emitterT __fi

using x86Emitter::xWrite8;
using x86Emitter::xWrite16;
using x86Emitter::xWrite32;
using x86Emitter::xWrite64;

#include "common/emitter/legacy_types.h"
#include "common/emitter/legacy_instructions.h"

#define MEMADDR(addr, oplen) (addr)

extern void ModRM(uint mod, uint reg, uint rm);
extern void SibSB(uint ss, uint index, uint base);
extern void SET8R(int cc, int to);
extern u8* J8Rel(int cc, int to);
extern u32* J32Rel(int cc, u32 to);
