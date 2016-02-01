/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2016  PCSX2 Dev Team
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

// Because nobody can't agree on a single name !
#if defined(__GNUG__)

// Yes there are several files for the same features!
// x86intrin.h which is the general include provided by the compiler
// intrin_x86.h which is a compatibility layer (as this file) but with a BSD license
// x86_intrin.h, this file, which is another compatibility layer
#include "x86intrin.h"
#include "intrin_x86.h"

#else

#include "Intrin.h"

#endif

#if defined(__clang__)
// Seriously what is so complicated to provided this bunch of intrinsics in clangs.
static unsigned int _rotr(unsigned int x, int s)
{
	return (x >> s) | (x << (32 - s));
}

static unsigned int _rotl(unsigned int x, int s)
{
	return (x << s) | (x >> (32 - s));
}
#endif

// Not correctly defined in GCC4.8 and below ! (dunno for VS)
#ifndef _MM_MK_INSERTPS_NDX
#define _MM_MK_INSERTPS_NDX(srcField, dstField, zeroMask) (((srcField)<<6) | ((dstField)<<4) | (zeroMask))
#endif
