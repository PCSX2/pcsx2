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

#include "x86intrin.h"

#else

#include "Intrin.h"

#endif

// Not correctly defined in GCC4.8 and below ! (dunno for VS)
#ifndef _MM_MK_INSERTPS_NDX
#define _MM_MK_INSERTPS_NDX(srcField, dstField, zeroMask) (((srcField)<<6) | ((dstField)<<4) | (zeroMask))
#endif
