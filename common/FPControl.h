// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

// This file abstracts the floating-point control registers, known as MXCSR on x86, and FPCR on AArch64.

#pragma once

#include "common/Pcsx2Defs.h"
#include "common/VectorIntrin.h"

enum class FPRoundMode : u8
{
	Nearest,
	NegativeInfinity,
	PositiveInfinity,
	ChopZero,

	MaxCount
};

struct FPControlRegister
{
#ifdef _M_X86
	u32 bitmask;

	static constexpr u32 EXCEPTION_MASK = (0x3Fu << 7);
	static constexpr u32 ROUNDING_CONTROL_SHIFT = 13;
	static constexpr u32 ROUNDING_CONTROL_MASK = 3u;
	static constexpr u32 ROUNDING_CONTROL_BITS = (ROUNDING_CONTROL_MASK << ROUNDING_CONTROL_SHIFT);
	static constexpr u32 DENORMALS_ARE_ZERO_BIT = (1u << 6);
	static constexpr u32 FLUSH_TO_ZERO_BIT = (1u << 15);

	__fi static FPControlRegister GetCurrent()
	{
		return FPControlRegister{_mm_getcsr()};
	}

	__fi static void SetCurrent(FPControlRegister value)
	{
		_mm_setcsr(value.bitmask);
	}

	__fi static constexpr FPControlRegister GetDefault()
	{
		// 0x1f80 - all exceptions masked, nearest rounding
		return FPControlRegister{0x1f80};
	}

	__fi constexpr FPControlRegister& EnableExceptions()
	{
		bitmask &= ~EXCEPTION_MASK;
		return *this;
	}

	__fi constexpr FPControlRegister DisableExceptions()
	{
		bitmask |= EXCEPTION_MASK;
		return *this;
	}

	__fi constexpr FPRoundMode GetRoundMode() const
	{
		return static_cast<FPRoundMode>((bitmask >> ROUNDING_CONTROL_SHIFT) & ROUNDING_CONTROL_MASK);
	}

	__fi constexpr FPControlRegister& SetRoundMode(FPRoundMode mode)
	{
		// These bits match on x86.
		bitmask = (bitmask & ~ROUNDING_CONTROL_BITS) | ((static_cast<u32>(mode) & ROUNDING_CONTROL_MASK) << ROUNDING_CONTROL_SHIFT);
		return *this;
	}

	__fi constexpr bool GetDenormalsAreZero() const
	{
		return ((bitmask & DENORMALS_ARE_ZERO_BIT) != 0);
	}

	__fi constexpr FPControlRegister SetDenormalsAreZero(bool daz)
	{
		if (daz)
			bitmask |= DENORMALS_ARE_ZERO_BIT;
		else
			bitmask &= ~DENORMALS_ARE_ZERO_BIT;
		return *this;
	}

	__fi constexpr bool GetFlushToZero() const
	{
		return ((bitmask & FLUSH_TO_ZERO_BIT) != 0);
	}

	__fi constexpr FPControlRegister SetFlushToZero(bool ftz)
	{
		if (ftz)
			bitmask |= FLUSH_TO_ZERO_BIT;
		else
			bitmask &= ~FLUSH_TO_ZERO_BIT;
		return *this;
	}

	__fi constexpr bool operator==(const FPControlRegister& rhs) const { return bitmask == rhs.bitmask; }
	__fi constexpr bool operator!=(const FPControlRegister& rhs) const { return bitmask != rhs.bitmask; }

#elif defined(_M_ARM64)
	u64 bitmask;

	static constexpr u64 FZ_BIT = (0x1ULL << 24);
	static constexpr u32 RMODE_SHIFT = 22;
	static constexpr u64 RMODE_MASK = 0x3ULL;
	static constexpr u64 RMODE_BITS = (RMODE_MASK << RMODE_SHIFT);
	static constexpr u32 EXCEPTION_MASK = (0x3Fu << 5);

	__fi static FPControlRegister GetCurrent()
	{
		u64 value;
		asm volatile("\tmrs %0, FPCR\n"
					 : "=r"(value));
		return FPControlRegister{value};
	}

	__fi static void SetCurrent(FPControlRegister value)
	{
		asm volatile("\tmsr FPCR, %0\n" ::"r"(value.bitmask));
	}

	__fi static constexpr FPControlRegister GetDefault()
	{
		// 0x0 - all exceptions masked, nearest rounding
		return FPControlRegister{0x0};
	}

	__fi constexpr FPControlRegister& EnableExceptions()
	{
		bitmask |= EXCEPTION_MASK;
		return *this;
	}

	__fi constexpr FPControlRegister& DisableExceptions()
	{
		bitmask &= ~EXCEPTION_MASK;
		return *this;
	}

	__fi constexpr FPRoundMode GetRoundMode() const
	{
		// Negative/Positive infinity rounding is flipped on A64.
		const u64 RMode = (bitmask >> RMODE_SHIFT) & RMODE_MASK;
		return static_cast<FPRoundMode>((RMode == 0b00 || RMode == 0b11) ? RMode : (RMode ^ 0b11));
	}

	__fi constexpr FPControlRegister& SetRoundMode(FPRoundMode mode)
	{
		const u64 RMode = ((mode == FPRoundMode::Nearest || mode == FPRoundMode::ChopZero) ? static_cast<u64>(mode) : (static_cast<u64>(mode) ^ 0b11));
		bitmask = (bitmask & ~RMODE_BITS) | ((RMode & RMODE_MASK) << RMODE_SHIFT);
		return *this;
	}

	__fi constexpr bool GetDenormalsAreZero() const
	{
		// Without FEAT_AFP, most ARM chips don't have separate DaZ/FtZ. This includes Apple Silicon, which
		// implements x86-like behavior with a vendor-specific extension that we cannot access from usermode.
		// The FZ bit causes both inputs and outputs to be flushed to zero.
		return ((bitmask & FZ_BIT) != 0);
	}

	__fi constexpr FPControlRegister SetDenormalsAreZero(bool daz)
	{
		if (daz)
			bitmask |= FZ_BIT;
		else
			bitmask &= ~FZ_BIT;
		return *this;
	}

	__fi constexpr bool GetFlushToZero() const
	{
		// See note in GetDenormalsAreZero().
		return ((bitmask & FZ_BIT) != 0);
	}

	__fi constexpr FPControlRegister SetFlushToZero(bool ftz)
	{
		if (ftz)
			bitmask |= FZ_BIT;
		else
			bitmask &= ~FZ_BIT;
		return *this;
	}

	__fi constexpr bool operator==(const FPControlRegister& rhs) const { return bitmask == rhs.bitmask; }
	__fi constexpr bool operator!=(const FPControlRegister& rhs) const { return bitmask != rhs.bitmask; }
#else
#error Unknown architecture.
#endif
};

/// Helper to back up/restore FPCR.
class FPControlRegisterBackup
{
public:
	__fi FPControlRegisterBackup(FPControlRegister new_value)
		: m_prev_val(FPControlRegister::GetCurrent())
	{
		FPControlRegister::SetCurrent(new_value);
	}
	__fi ~FPControlRegisterBackup()
	{
		FPControlRegister::SetCurrent(m_prev_val);
	}

	FPControlRegisterBackup(const FPControlRegisterBackup&) = delete;
	FPControlRegisterBackup& operator=(const FPControlRegisterBackup&) = delete;

private:
	FPControlRegister m_prev_val;
};
