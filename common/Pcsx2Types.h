// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <cstdint>

// --------------------------------------------------------------------------------------
//  Basic Atomic Types
// --------------------------------------------------------------------------------------

using s8 = int8_t;
using s16 = int16_t;
using s32 = int32_t;
using s64 = int64_t;

using u8 = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;

using uptr = uintptr_t;
using sptr = intptr_t;

using uint = unsigned int;

// --------------------------------------------------------------------------------------
//  u128 / s128 - A rough-and-ready cross platform 128-bit datatype, Non-SSE style.
// --------------------------------------------------------------------------------------
// Note: These structs don't provide any additional constructors because C++ doesn't allow
// the use of datatypes with constructors in unions (and since unions aren't the primary
// uses of these types, that means we can't have constructors). Embedded functions for
// performing explicit conversion from 64 and 32 bit values are provided instead.
//
union u128
{
	struct
	{
		u64 lo;
		u64 hi;
	};

	u64 _u64[2];
	u32 _u32[4];
	u16 _u16[8];
	u8 _u8[16];

	// Explicit conversion from u64. Zero-extends the source through 128 bits.
	static u128 From64(u64 src)
	{
		u128 retval;
		retval.lo = src;
		retval.hi = 0;
		return retval;
	}

	// Explicit conversion from u32. Zero-extends the source through 128 bits.
	static u128 From32(u32 src)
	{
		u128 retval;
		retval._u32[0] = src;
		retval._u32[1] = 0;
		retval.hi = 0;
		return retval;
	}

	operator u32() const { return _u32[0]; }
	operator u16() const { return _u16[0]; }
	operator u8() const { return _u8[0]; }

	bool operator==(const u128& right) const
	{
		return (lo == right.lo) && (hi == right.hi);
	}

	bool operator!=(const u128& right) const
	{
		return (lo != right.lo) || (hi != right.hi);
	}
};

struct s128
{
	s64 lo;
	s64 hi;

	// explicit conversion from s64, with sign extension.
	static s128 From64(s64 src)
	{
		s128 retval = {src, (src < 0) ? -1 : 0};
		return retval;
	}

	// explicit conversion from s32, with sign extension.
	static s128 From64(s32 src)
	{
		s128 retval = {src, (src < 0) ? -1 : 0};
		return retval;
	}

	operator u32() const { return (s32)lo; }
	operator u16() const { return (s16)lo; }
	operator u8() const { return (s8)lo; }

	bool operator==(const s128& right) const
	{
		return (lo == right.lo) && (hi == right.hi);
	}

	bool operator!=(const s128& right) const
	{
		return (lo != right.lo) || (hi != right.hi);
	}
};
