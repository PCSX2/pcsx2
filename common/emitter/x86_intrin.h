// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

// Because nobody can't agree on a single name !
#if defined(__GNUC__)

// Yes there are several files for the same features!
// x86intrin.h which is the general include provided by the compiler
// x86_intrin.h, this file, which is compatibility layer for severals intrinsics
#include "x86intrin.h"

#else

#include "Intrin.h"

#endif

// Rotate instruction
#if defined(__clang__) && __clang_major__ < 9
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"

// Seriously what is so complicated to provided this bunch of intrinsics in clangs.
static unsigned int _rotr(unsigned int x, int s)
{
	return (x >> s) | (x << (32 - s));
}

static unsigned int _rotl(unsigned int x, int s)
{
	return (x << s) | (x >> (32 - s));
}

#pragma clang diagnostic pop
#endif

// Not correctly defined in GCC4.8 and below ! (dunno for VS)
#ifndef _MM_MK_INSERTPS_NDX
#define _MM_MK_INSERTPS_NDX(srcField, dstField, zeroMask) (((srcField) << 6) | ((dstField) << 4) | (zeroMask))
#endif
