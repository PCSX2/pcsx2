// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <stdexcept>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <bit>
#include "PS2Float.h"
#include "Common.h"

//****************************************************************
// Booth Multiplier
//****************************************************************

PS2Float::BoothRecode PS2Float::Booth(u32 a, u32 b, u32 bit)
{
	u32 test = (bit ? b >> (bit * 2 - 1) : b << 1) & 7;
	a <<= (bit * 2);
	a += (test == 3 || test == 4) ? a : 0;
	u32 neg = (test >= 4 && test <= 6) ? ~0u : 0;
	u32 pos = 1 << (bit * 2);
	a ^= (neg & -pos);
	a &= (test >= 1 && test <= 6) ? ~0u : 0;
	return {a, neg & pos};
}

PS2Float::AddResult PS2Float::Add3(u32 a, u32 b, u32 c)
{
	u32 u = a ^ b;
	return {u ^ c, ((u & c) | (a & b)) << 1};
}

u64 PS2Float::MulMantissa(u32 a, u32 b)
{
	u64 full = static_cast<u64>(a) * static_cast<u64>(b);
	BoothRecode b0 = Booth(a, b, 0);
	BoothRecode b1 = Booth(a, b, 1);
	BoothRecode b2 = Booth(a, b, 2);
	BoothRecode b3 = Booth(a, b, 3);
	BoothRecode b4 = Booth(a, b, 4);
	BoothRecode b5 = Booth(a, b, 5);
	BoothRecode b6 = Booth(a, b, 6);
	BoothRecode b7 = Booth(a, b, 7);

	// First cycle
	AddResult t0 = Add3(b1.data, b2.data, b3.data);
	AddResult t1 = Add3(b4.data & ~0x7ffu, b5.data & ~0xfffu, b6.data);
	// A few adds get skipped, squeeze them back in
	t1.hi |= b6.negate | (b5.data & 0x800);
	b7.data |= (b5.data & 0x400) + b5.negate;

	// Second cycle
	AddResult t2 = Add3(b0.data, t0.lo, t0.hi);
	AddResult t3 = Add3(b7.data, t1.lo, t1.hi);

	// Third cycle
	AddResult t4 = Add3(t2.hi, t3.lo, t3.hi);

	// Fourth cycle
	AddResult t5 = Add3(t2.lo, t4.lo, t4.hi);

	// Discard bits and sum
	t5.hi += b7.negate;
	t5.lo &= ~0x7fffu;
	t5.hi &= ~0x7fffu;
	u32 ps2lo = t5.lo + t5.hi;
	return full - ((ps2lo ^ full) & 0x8000);
}

//****************************************************************
// Float Processor
//****************************************************************

PS2Float::PS2Float(u32 value) { raw = value; }

PS2Float::PS2Float(bool sign, u8 exponent, u32 mantissa)
{
	raw = 0;
	raw |= (sign ? 1u : 0u) << 31;
	raw |= (u32)(exponent << 23);
	raw |= mantissa;
}

PS2Float PS2Float::Max()
{
	return PS2Float(MAX_FLOATING_POINT_VALUE);
}

PS2Float PS2Float::Min()
{
	return PS2Float(MIN_FLOATING_POINT_VALUE);
}

PS2Float PS2Float::One()
{
	return PS2Float(ONE);
}

PS2Float PS2Float::MinOne()
{
	return PS2Float(MIN_ONE);
}

PS2Float PS2Float::Add(PS2Float addend)
{
	if (IsDenormalized() || addend.IsDenormalized())
		return SolveAddSubDenormalizedOperation(*this, addend, true);

	if (IsAbnormal() && addend.IsAbnormal())
		return SolveAbnormalAdditionOrSubtractionOperation(*this, addend, true);

	u32 a = raw;
	u32 b = addend.raw;

	//exponent difference
	s32 exp_diff = ((a >> 23) & 0xFF) - ((b >> 23) & 0xFF);

	//diff = 25 .. 255 , expt < expd
	if (exp_diff >= 25)
	{
		b = b & SIGNMASK;
	}

	//diff = 1 .. 24, expt < expd
	else if (exp_diff > 0)
	{
		exp_diff = exp_diff - 1;
		b = (MIN_FLOATING_POINT_VALUE << exp_diff) & b;
	}

	//diff = -255 .. -25, expd < expt
	else if (exp_diff <= -25)
	{
		a = a & SIGNMASK;
	}

	//diff = -24 .. -1 , expd < expt
	else if (exp_diff < 0)
	{
		exp_diff = -exp_diff;
		exp_diff = exp_diff - 1;
		a = a & (MIN_FLOATING_POINT_VALUE << exp_diff);
	}

	return PS2Float(a).DoAdd(PS2Float(b));
}

PS2Float PS2Float::Sub(PS2Float subtrahend)
{
	if (IsDenormalized() || subtrahend.IsDenormalized())
		return SolveAddSubDenormalizedOperation(*this, subtrahend, false);

	if (IsAbnormal() && subtrahend.IsAbnormal())
		return SolveAbnormalAdditionOrSubtractionOperation(*this, subtrahend, false);

	u32 a = raw;
	u32 b = subtrahend.raw;

	//exponent difference
	s32 exp_diff = ((a >> 23) & 0xFF) - ((b >> 23) & 0xFF);

	//diff = 25 .. 255 , expt < expd
	if (exp_diff >= 25)
	{
		b = b & SIGNMASK;
	}

	//diff = 1 .. 24, expt < expd
	else if (exp_diff > 0)
	{
		exp_diff = exp_diff - 1;
		b = (MIN_FLOATING_POINT_VALUE << exp_diff) & b;
	}

	//diff = -255 .. -25, expd < expt
	else if (exp_diff <= -25)
	{
		a = a & SIGNMASK;
	}

	//diff = -24 .. -1 , expd < expt
	else if (exp_diff < 0)
	{
		exp_diff = -exp_diff;
		exp_diff = exp_diff - 1;
		a = a & (MIN_FLOATING_POINT_VALUE << exp_diff);
	}


	return PS2Float(a).DoAdd(PS2Float(b).Negate());
}

PS2Float PS2Float::Mul(PS2Float mulend)
{
	if (IsDenormalized() || mulend.IsDenormalized())
		return SolveMultiplicationDenormalizedOperation(*this, mulend);

	if (IsAbnormal() && mulend.IsAbnormal())
		return SolveAbnormalMultiplicationOrDivisionOperation(*this, mulend, true);

	if (IsZero() || mulend.IsZero())
		return PS2Float(DetermineMultiplicationDivisionOperationSign(*this, mulend), 0, 0);

	return DoMul(mulend);
}

PS2Float PS2Float::Div(PS2Float divend)
{
	if (IsDenormalized() || divend.IsDenormalized())
		return SolveDivisionDenormalizedOperation(*this, divend);

	if (IsAbnormal() && divend.IsAbnormal())
		return SolveAbnormalMultiplicationOrDivisionOperation(*this, divend, false);

	if (IsZero())
		return PS2Float(DetermineMultiplicationDivisionOperationSign(*this, divend), 0, 0);
	else if (divend.IsZero())
		return DetermineMultiplicationDivisionOperationSign(*this, divend) ? Min() : Max();

	return DoDiv(divend);
}

// Rounding can be slightly off: (PS2: rsqrt(0x7FFFFFF0) -> 0x5FB504ED | SoftFloat/IEEE754 rsqrt(0x7FFFFFF0) -> 0x5FB504EE).
PS2Float PS2Float::Sqrt()
{
	s32 t;
	s32 s = 0;
	s32 q = 0;
	u32 r = 0x01000000; /* r = moving bit from right to left */

	if (IsDenormalized())
		return PS2Float(0);

	// PS2 only takes positive numbers for SQRT, and convert if necessary.
	s32 ix = (s32)PS2Float(false, Exponent(), Mantissa()).raw;

	/* extract mantissa and unbias exponent */
	s32 m = (ix >> 23) - BIAS;

	ix = (ix & 0x007FFFFF) | 0x00800000;
	if ((m & 1) == 1)
	{
		/* odd m, double x to make it even */
		ix += ix;
	}

	m >>= 1; /* m = [m/2] */

	/* generate sqrt(x) bit by bit */
	ix += ix;

	while (r != 0)
	{
		t = s + (s32)(r);
		if (t <= ix)
		{
			s = t + (s32)(r);
			ix -= t;
			q += (s32)(r);
		}

		ix += ix;
		r >>= 1;
	}

	/* use floating add to find out rounding direction */
	if (ix != 0)
	{
		q += q & 1;
	}

	ix = (q >> 1) + 0x3F000000;
	ix += m << 23;

	return PS2Float((u32)(ix));
}

PS2Float PS2Float::Rsqrt(PS2Float other)
{
	return Div(other.Sqrt());
}

bool PS2Float::IsDenormalized()
{
	return Exponent() == 0;
}

bool PS2Float::IsAbnormal()
{
	u32 val = raw;
	return val == MAX_FLOATING_POINT_VALUE || val == MIN_FLOATING_POINT_VALUE ||
		   val == POSITIVE_INFINITY_VALUE || val == NEGATIVE_INFINITY_VALUE;
}

bool PS2Float::IsZero()
{
	return Abs() == 0;
}

u32 PS2Float::Abs()
{
	return (raw & MAX_FLOATING_POINT_VALUE);
}

PS2Float PS2Float::Negate()
{
	return PS2Float(raw ^ 0x80000000);
}

PS2Float PS2Float::RoundTowardsZero()
{
	return PS2Float((u32)std::trunc((double)raw));
}

s32 PS2Float::CompareTo(PS2Float other)
{
	s32 selfTwoComplementVal = (s32)Abs();
	if (Sign())
		selfTwoComplementVal = -selfTwoComplementVal;

	s32 otherTwoComplementVal = (s32)other.Abs();
	if (other.Sign())
		otherTwoComplementVal = -otherTwoComplementVal;

	if (selfTwoComplementVal < otherTwoComplementVal)
		return -1;
	else if (selfTwoComplementVal == otherTwoComplementVal)
		return 0;
	else
		return 1;
}

s32 PS2Float::CompareOperand(PS2Float other)
{
	s32 selfTwoComplementVal = (s32)Abs();
	s32 otherTwoComplementVal = (s32)other.Abs();

	if (selfTwoComplementVal < otherTwoComplementVal)
		return -1;
	else if (selfTwoComplementVal == otherTwoComplementVal)
		return 0;
	else
		return 1;
}

double PS2Float::ToDouble()
{
	return std::bit_cast<double>(((u64)Sign() << 63) | ((((u64)Exponent() - BIAS) + 1023ULL) << 52) | ((u64)Mantissa() << 29));
}

std::string PS2Float::ToString()
{
	double res = ToDouble();

	u32 value = raw;
	std::ostringstream oss;
	oss << std::fixed << std::setprecision(6);

	if (IsDenormalized())
	{
		oss << "Denormalized(" << res << ")";
	}
	else if (value == MAX_FLOATING_POINT_VALUE)
	{
		oss << "Fmax(" << res << ")";
	}
	else if (value == MIN_FLOATING_POINT_VALUE)
	{
		oss << "-Fmax(" << res << ")";
	}
	else if (value == POSITIVE_INFINITY_VALUE)
	{
		oss << "Inf(" << res << ")";
	}
	else if (value == NEGATIVE_INFINITY_VALUE)
	{
		oss << "-Inf(" << res << ")";
	}
	else
	{
		oss << "Ps2Float(" << res << ")";
	}

	return oss.str();
}

PS2Float PS2Float::DoAdd(PS2Float other)
{
	const u8 roundingMultiplier = 6;

	u8 selfExponent = Exponent();
	s32 resExponent = selfExponent - other.Exponent();

	if (resExponent < 0)
		return other.DoAdd(*this);
	else if (resExponent >= 25)
		return *this;

	// http://graphics.stanford.edu/~seander/bithacks.html#ConditionalNegate
	u32 sign1 = (u32)((s32)raw >> 31);
	s32 selfMantissa = (s32)(((Mantissa() | 0x800000) ^ sign1) - sign1);
	u32 sign2 = (u32)((s32)other.raw >> 31);
	s32 otherMantissa = (s32)(((other.Mantissa() | 0x800000) ^ sign2) - sign2);

	// PS2 multiply by 2 before doing the Math here.
	s32 man = (selfMantissa << roundingMultiplier) + ((otherMantissa << roundingMultiplier) >> resExponent);
	s32 absMan = abs(man);
	if (absMan == 0)
		return PS2Float(0);

	// Remove from exponent the PS2 Multiplier value.
	s32 rawExp = selfExponent - roundingMultiplier;

	s32 amount = normalizeAmounts[clz(absMan)];
	rawExp -= amount;
	absMan <<= amount;

	s32 msbIndex = BitScanReverse8(absMan >> 23);
	rawExp += msbIndex;
	absMan >>= msbIndex;

	if (rawExp > 255)
		return man < 0 ? Min() : Max();
	else if (rawExp <= 0)
		return PS2Float(man < 0, 0, 0);

	return PS2Float(((u32)man & SIGNMASK) | (u32)rawExp << 23 | ((u32)absMan & 0x7FFFFF));
}

PS2Float PS2Float::DoMul(PS2Float other)
{
	u8 selfExponent = Exponent();
	u8 otherExponent = other.Exponent();
	u32 selfMantissa = Mantissa() | 0x800000;
	u32 otherMantissa = other.Mantissa() | 0x800000;
	u32 sign = (raw ^ other.raw) & SIGNMASK;

	s32 resExponent = selfExponent + otherExponent - 127;
	u32 resMantissa = (u32)(MulMantissa(selfMantissa, otherMantissa) >> 23);

	if (resMantissa > 0xFFFFFF)
	{
		resMantissa >>= 1;
		resExponent++;
	}

	if (resExponent > 255)
		return PS2Float(sign | MAX_FLOATING_POINT_VALUE);
	else if (resExponent <= 0)
		return PS2Float(sign);

	return PS2Float(sign | (u32)(resExponent << 23) | (resMantissa & 0x7FFFFF));
}

// Rounding can be slightly off: (PS2: 0x3F800000 / 0x3F800001 = 0x3F7FFFFF | SoftFloat/IEEE754: 0x3F800000 / 0x3F800001 = 0x3F7FFFFE).
PS2Float PS2Float::DoDiv(PS2Float other)
{
	bool sign = DetermineMultiplicationDivisionOperationSign(*this, other);
	u32 selfMantissa = Mantissa() | 0x800000;
	u32 otherMantissa = other.Mantissa() | 0x800000;
	s32 resExponent = Exponent() - other.Exponent() + BIAS;
	u64 selfMantissa64;

	if (resExponent > 255)
		return sign ? Min() : Max();
	else if (resExponent <= 0)
		return PS2Float(sign, 0, 0);

	if (selfMantissa < otherMantissa)
	{
		--resExponent;
		if (resExponent == 0)
			return PS2Float(sign, 0, 0);
		selfMantissa64 = (u64)(selfMantissa) << 31;
	}
	else
	{
		selfMantissa64 = (u64)(selfMantissa) << 30;
	}

	u32 resMantissa = (u32)(selfMantissa64 / otherMantissa);

	if ((resMantissa & 0x3F) == 0)
		resMantissa |= ((u64)(otherMantissa)*resMantissa != selfMantissa64) ? 1U : 0;

	resMantissa = (resMantissa + 0x40U) >> 7;

	if (resMantissa > 0)
	{
		s32 leadingBitPosition = PS2Float::GetMostSignificantBitPosition(resMantissa);

		while (leadingBitPosition != IMPLICIT_LEADING_BIT_POS)
		{
			if (leadingBitPosition > IMPLICIT_LEADING_BIT_POS)
			{
				resMantissa >>= 1;

				s32 exp = resExponent + 1;

				if (exp > 255)
					return sign ? Min() : Max();

				resExponent = exp;

				leadingBitPosition--;
			}
			else if (leadingBitPosition < IMPLICIT_LEADING_BIT_POS)
			{
				resMantissa <<= 1;

				s32 exp = resExponent - 1;

				if (exp <= 0)
					return PS2Float(sign, 0, 0);

				resExponent = exp;

				leadingBitPosition++;
			}
		}
	}

	resMantissa &= 0x7FFFFF;
	return PS2Float(sign, (u8)resExponent, resMantissa).RoundTowardsZero();
}

PS2Float PS2Float::SolveAbnormalAdditionOrSubtractionOperation(PS2Float a, PS2Float b, bool add)
{
	u32 aval = a.raw;
	u32 bval = b.raw;

	if (aval == MAX_FLOATING_POINT_VALUE && bval == MAX_FLOATING_POINT_VALUE)
		return add ? Max() : PS2Float(0);

	if (aval == MIN_FLOATING_POINT_VALUE && bval == MIN_FLOATING_POINT_VALUE)
		return add ? Min() : PS2Float(0);

	if (aval == MIN_FLOATING_POINT_VALUE && bval == MAX_FLOATING_POINT_VALUE)
		return add ? PS2Float(0) : Min();

	if (aval == MAX_FLOATING_POINT_VALUE && bval == MIN_FLOATING_POINT_VALUE)
		return add ? PS2Float(0) : Max();

	if (aval == POSITIVE_INFINITY_VALUE && bval == POSITIVE_INFINITY_VALUE)
		return add ? Max() : PS2Float(0);

	if (aval == NEGATIVE_INFINITY_VALUE && bval == POSITIVE_INFINITY_VALUE)
		return add ? PS2Float(0) : Min();

	if (aval == POSITIVE_INFINITY_VALUE && bval == NEGATIVE_INFINITY_VALUE)
		return add ? PS2Float(0) : Max();

	if (aval == NEGATIVE_INFINITY_VALUE && bval == NEGATIVE_INFINITY_VALUE)
		return add ? Min() : PS2Float(0);

	if (aval == MAX_FLOATING_POINT_VALUE && bval == POSITIVE_INFINITY_VALUE)
		return add ? Max() : PS2Float(0x7F7FFFFE);

	if (aval == MAX_FLOATING_POINT_VALUE && bval == NEGATIVE_INFINITY_VALUE)
		return add ? PS2Float(0x7F7FFFFE) : Max();

	if (aval == MIN_FLOATING_POINT_VALUE && bval == POSITIVE_INFINITY_VALUE)
		return add ? PS2Float(0xFF7FFFFE) : Min();

	if (aval == MIN_FLOATING_POINT_VALUE && bval == NEGATIVE_INFINITY_VALUE)
		return add ? Min() : PS2Float(0xFF7FFFFE);

	if (aval == POSITIVE_INFINITY_VALUE && bval == MAX_FLOATING_POINT_VALUE)
		return add ? Max() : PS2Float(0xFF7FFFFE);

	if (aval == POSITIVE_INFINITY_VALUE && bval == MIN_FLOATING_POINT_VALUE)
		return add ? PS2Float(0xFF7FFFFE) : Max();

	if (aval == NEGATIVE_INFINITY_VALUE && bval == MAX_FLOATING_POINT_VALUE)
		return add ? PS2Float(0x7F7FFFFE) : Min();

	if (aval == NEGATIVE_INFINITY_VALUE && bval == MIN_FLOATING_POINT_VALUE)
		return add ? Min() : PS2Float(0x7F7FFFFE);

	Console.Error("Unhandled abnormal add/sub floating point operation");
	
	return PS2Float(0);
}

PS2Float PS2Float::SolveAbnormalMultiplicationOrDivisionOperation(PS2Float a, PS2Float b, bool mul)
{
	u32 aval = a.raw;
	u32 bval = b.raw;

	if (mul)
	{
		if ((aval == MAX_FLOATING_POINT_VALUE && bval == MAX_FLOATING_POINT_VALUE) ||
			(aval == MIN_FLOATING_POINT_VALUE && bval == MIN_FLOATING_POINT_VALUE))
			return Max();

		if ((aval == MAX_FLOATING_POINT_VALUE && bval == MIN_FLOATING_POINT_VALUE) ||
			(aval == MIN_FLOATING_POINT_VALUE && bval == MAX_FLOATING_POINT_VALUE))
			return Min();

		if (aval == POSITIVE_INFINITY_VALUE && bval == POSITIVE_INFINITY_VALUE)
			return Max();

		if (aval == NEGATIVE_INFINITY_VALUE && bval == POSITIVE_INFINITY_VALUE)
			return Min();

		if (aval == POSITIVE_INFINITY_VALUE && bval == NEGATIVE_INFINITY_VALUE)
			return Min();

		if (aval == NEGATIVE_INFINITY_VALUE && bval == NEGATIVE_INFINITY_VALUE)
			return Max();

		if (aval == MAX_FLOATING_POINT_VALUE && bval == POSITIVE_INFINITY_VALUE)
			return Max();

		if (aval == MAX_FLOATING_POINT_VALUE && bval == NEGATIVE_INFINITY_VALUE)
			return Min();

		if (aval == MIN_FLOATING_POINT_VALUE && bval == POSITIVE_INFINITY_VALUE)
			return Min();

		if (aval == MIN_FLOATING_POINT_VALUE && bval == NEGATIVE_INFINITY_VALUE)
			return Max();

		if (aval == POSITIVE_INFINITY_VALUE && bval == MAX_FLOATING_POINT_VALUE)
			return Max();

		if (aval == POSITIVE_INFINITY_VALUE && bval == MIN_FLOATING_POINT_VALUE)
			return Min();

		if (aval == NEGATIVE_INFINITY_VALUE && bval == MAX_FLOATING_POINT_VALUE)
			return Min();

		if (aval == NEGATIVE_INFINITY_VALUE && bval == MIN_FLOATING_POINT_VALUE)
			return Max();
	}
	else
	{
		if ((aval == MAX_FLOATING_POINT_VALUE && bval == MAX_FLOATING_POINT_VALUE) ||
			(aval == MIN_FLOATING_POINT_VALUE && bval == MIN_FLOATING_POINT_VALUE))
			return One();

		if ((aval == MAX_FLOATING_POINT_VALUE && bval == MIN_FLOATING_POINT_VALUE) ||
			(aval == MIN_FLOATING_POINT_VALUE && bval == MAX_FLOATING_POINT_VALUE))
			return MinOne();

		if (aval == POSITIVE_INFINITY_VALUE && bval == POSITIVE_INFINITY_VALUE)
			return One();

		if (aval == NEGATIVE_INFINITY_VALUE && bval == POSITIVE_INFINITY_VALUE)
			return MinOne();

		if (aval == POSITIVE_INFINITY_VALUE && bval == NEGATIVE_INFINITY_VALUE)
			return MinOne();

		if (aval == NEGATIVE_INFINITY_VALUE && bval == NEGATIVE_INFINITY_VALUE)
			return One();

		if (aval == MAX_FLOATING_POINT_VALUE && bval == POSITIVE_INFINITY_VALUE)
			return PS2Float(0x3FFFFFFF);

		if (aval == MAX_FLOATING_POINT_VALUE && bval == NEGATIVE_INFINITY_VALUE)
			return PS2Float(0xBFFFFFFF);

		if (aval == MIN_FLOATING_POINT_VALUE && bval == POSITIVE_INFINITY_VALUE)
			return PS2Float(0xBFFFFFFF);

		if (aval == MIN_FLOATING_POINT_VALUE && bval == NEGATIVE_INFINITY_VALUE)
			return PS2Float(0x3FFFFFFF);

		if (aval == POSITIVE_INFINITY_VALUE && bval == MAX_FLOATING_POINT_VALUE)
			return PS2Float(0x3F000001);

		if (aval == POSITIVE_INFINITY_VALUE && bval == MIN_FLOATING_POINT_VALUE)
			return PS2Float(0xBF000001);

		if (aval == NEGATIVE_INFINITY_VALUE && bval == MAX_FLOATING_POINT_VALUE)
			return PS2Float(0xBF000001);

		if (aval == NEGATIVE_INFINITY_VALUE && bval == MIN_FLOATING_POINT_VALUE)
			return PS2Float(0x3F000001);
	}

	Console.Error("Unhandled abnormal mul/div floating point operation");
	
	return PS2Float(0);
}

PS2Float PS2Float::SolveAddSubDenormalizedOperation(PS2Float a, PS2Float b, bool add)
{
	bool sign = add ? DetermineAdditionOperationSign(a, b) : DetermineSubtractionOperationSign(a, b);

	if (a.IsDenormalized() && !b.IsDenormalized())
		return PS2Float(sign, b.Exponent(), b.Mantissa());
	else if (!a.IsDenormalized() && b.IsDenormalized())
		return PS2Float(sign, a.Exponent(), a.Mantissa());
	else if (a.IsDenormalized() && b.IsDenormalized())
		return PS2Float(sign, 0, 0);
	else
		Console.Error("Both numbers are not denormalized");

	return PS2Float(0);
}

PS2Float PS2Float::SolveMultiplicationDenormalizedOperation(PS2Float a, PS2Float b)
{
	return PS2Float(DetermineMultiplicationDivisionOperationSign(a, b), 0, 0);
}

PS2Float PS2Float::SolveDivisionDenormalizedOperation(PS2Float a, PS2Float b)
{
	bool sign = DetermineMultiplicationDivisionOperationSign(a, b);

	if (a.IsDenormalized() && !b.IsDenormalized())
		return PS2Float(sign, 0, 0);
	else if (!a.IsDenormalized() && b.IsDenormalized())
		return sign ? Min() : Max();
	else if (a.IsDenormalized() && b.IsDenormalized())
		return sign ? Min() : Max();
	else
		Console.Error("Both numbers are not denormalized");

	return PS2Float(0);
}

bool PS2Float::DetermineMultiplicationDivisionOperationSign(PS2Float a, PS2Float b)
{
	return a.Sign() ^ b.Sign();
}

bool PS2Float::DetermineAdditionOperationSign(PS2Float a, PS2Float b)
{
	if (a.IsZero() && b.IsZero())
	{
		if (!a.Sign() || !b.Sign())
			return false;
		else if (a.Sign() && b.Sign())
			return true;
		else
			Console.Error("Unhandled addition operation flags");
	}
	
	return a.CompareOperand(b) >= 0 ? a.Sign() : b.Sign();
}

bool PS2Float::DetermineSubtractionOperationSign(PS2Float a, PS2Float b)
{
	if (a.IsZero() && b.IsZero())
	{
		if (!a.Sign() || b.Sign())
			return false;
		else if (a.Sign() && !b.Sign())
			return true;
		else
			Console.Error("Unhandled subtraction operation flags");
	}

	return a.CompareOperand(b) >= 0 ? a.Sign() : !b.Sign();
}

s32 PS2Float::clz(s32 x)
{
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;

	return debruijn32[(u32)x * 0x8c0b2891u >> 26];
}

s32 PS2Float::BitScanReverse8(s32 b)
{
	return msb[b];
}

s32 PS2Float::GetMostSignificantBitPosition(u32 value)
{
	for (s32 i = 31; i >= 0; i--)
	{
		if (((value >> i) & 1) != 0)
			return i;
	}
	return -1;
}
