// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include "common/Pcsx2Defs.h"

enum SSE_RoundMode
{
	SSE_RoundMode_FIRST = 0,
	SSEround_Nearest = 0,
	SSEround_NegInf,
	SSEround_PosInf,
	SSEround_Chop,
	SSE_RoundMode_COUNT
};

ImplementEnumOperators(SSE_RoundMode);

// Predeclaration for xIndirect32
namespace x86Emitter
{
	template <typename T>
	class xIndirect;
	typedef xIndirect<u32> xIndirect32;
} // namespace x86Emitter

// --------------------------------------------------------------------------------------
//  SSE_MXCSR  -  Control/Status Register (bitfield)
// --------------------------------------------------------------------------------------
// Bits 0-5 are exception flags; used only if SSE exceptions have been enabled.
//   Bits in this field are "sticky" and, once an exception has occured, must be manually
//   cleared using LDMXCSR or FXRSTOR.
//
// Bits 7-12 are the masks for disabling the exceptions in bits 0-5.  Cleared bits allow
//   exceptions, set bits mask exceptions from being raised.
//
union SSE_MXCSR
{
	u32 bitmask;
	struct
	{
		u32
			InvalidOpFlag : 1,
			DenormalFlag : 1,
			DivideByZeroFlag : 1,
			OverflowFlag : 1,
			UnderflowFlag : 1,
			PrecisionFlag : 1,

			DenormalsAreZero : 1,

			InvalidOpMask : 1,
			DenormalMask : 1,
			DivideByZeroMask : 1,
			OverflowMask : 1,
			UnderflowMask : 1,
			PrecisionMask : 1,

			RoundingControl : 2,
			FlushToZero : 1;
	};

	static SSE_MXCSR GetCurrent();
	static void SetCurrent(const SSE_MXCSR& value);

	SSE_RoundMode GetRoundMode() const;
	SSE_MXCSR& SetRoundMode(SSE_RoundMode mode);
	SSE_MXCSR& ClearExceptionFlags();
	SSE_MXCSR& EnableExceptions();
	SSE_MXCSR& DisableExceptions();

	bool operator==(const SSE_MXCSR& right) const
	{
		return bitmask == right.bitmask;
	}

	bool operator!=(const SSE_MXCSR& right) const
	{
		return bitmask != right.bitmask;
	}

	operator x86Emitter::xIndirect32() const;
};
