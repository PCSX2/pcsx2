/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

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
