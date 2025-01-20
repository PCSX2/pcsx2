// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include <stdexcept>
#include <cmath>
#include <string>
#include <sstream>
#include <iomanip>
#include <iostream>
#include <bit>
#include "common/Pcsx2Defs.h"
#include "common/BitUtils.h"
#include "FpgaDiv.h"
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

PS2Float::PS2Float(s32 value) { raw = (u32)value; }

PS2Float::PS2Float(u32 value) { raw = value; }

PS2Float::PS2Float(float value) { raw = std::bit_cast<u32>(value); }

PS2Float::PS2Float(bool sign, u8 exponent, u32 mantissa)
{
	raw = 0;
	raw |= (sign ? 1u : 0u) << 31;
	raw |= (u32)(exponent << MANTISSA_BITS);
	raw |= mantissa & 0x7FFFFF;
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

	u32 a = raw;
	u32 b = addend.raw;

	//exponent difference
	s32 exp_diff = Exponent() - addend.Exponent();

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

	u32 a = raw;
	u32 b = subtrahend.raw;

	//exponent difference
	s32 exp_diff = Exponent() - subtrahend.Exponent();

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

	if (IsZero() || mulend.IsZero())
		return PS2Float(DetermineMultiplicationDivisionOperationSign(*this, mulend), 0, 0);

	return DoMul(mulend);
}

PS2Float PS2Float::Div(PS2Float divend)
{
	FpgaDiv fpga = FpgaDiv(true, raw, divend.raw);
	PS2Float result = PS2Float(fpga.floatResult);
	result.dz = fpga.dz;
	result.iv = fpga.iv;
	result.of = fpga.of;
	result.uf = fpga.uf;
	return result;
}

PS2Float PS2Float::Sqrt()
{
	FpgaDiv fpga = FpgaDiv(false, 0, PS2Float(false, Exponent(), Mantissa()).raw);
	PS2Float result = PS2Float(fpga.floatResult);
	result.dz = fpga.dz;
	result.iv = fpga.iv;
	return result;
}

PS2Float PS2Float::Rsqrt(PS2Float other)
{
	FpgaDiv fpgaSqrt = FpgaDiv(false, 0, PS2Float(false, other.Exponent(), other.Mantissa()).raw);
	FpgaDiv fpgaDiv = FpgaDiv(true, raw, fpgaSqrt.floatResult);
	PS2Float result = PS2Float(fpgaDiv.floatResult);
	result.dz = fpgaSqrt.dz || fpgaDiv.dz;
	result.iv = fpgaSqrt.iv || fpgaDiv.iv;
	result.of = fpgaDiv.of;
	result.uf = fpgaDiv.uf;
	return result;
}

PS2Float PS2Float::Pow(s32 exponent)
{
	PS2Float result = PS2Float::One(); // Start with 1, since any number raised to the power of 0 is 1

	if (exponent != 0)
	{
		s32 exp = abs(exponent);

		for (s32 i = 0; i < exp; i++)
		{
			result = result.Mul(*this);
		}
	}

	if (exponent < 0)
		return PS2Float::One().Div(result);
	else
		return result;
}

bool PS2Float::IsDenormalized()
{
	return Exponent() == 0;
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

s32 PS2Float::CompareToSign(PS2Float other)
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

s32 PS2Float::CompareTo(PS2Float other)
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
	else
	{
		oss << "PS2Float(" << res << ")";
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

	s32 amount = Common::normalizeAmounts[Common::CountLeadingSignBits(absMan)];
	rawExp -= amount;
	absMan <<= amount;

	s32 msbIndex = Common::BitScanReverse8(absMan >> MANTISSA_BITS);
	rawExp += msbIndex;
	absMan >>= msbIndex;

	if (rawExp > 255)
	{
		PS2Float result = man < 0 ? Min() : Max();
		result.of = true;
		return result;
	}
	else if (rawExp < 1)
	{
		PS2Float result = PS2Float(man < 0, 0, 0);
		result.uf = true;
		return result;
	}

	return PS2Float(((u32)man & SIGNMASK) | (u32)rawExp << MANTISSA_BITS | ((u32)absMan & 0x7FFFFF));
}

PS2Float PS2Float::DoMul(PS2Float other)
{
	u8 selfExponent = Exponent();
	u8 otherExponent = other.Exponent();
	u32 selfMantissa = Mantissa() | 0x800000;
	u32 otherMantissa = other.Mantissa() | 0x800000;
	u32 sign = (raw ^ other.raw) & SIGNMASK;

	s32 resExponent = selfExponent + otherExponent - 127;
	u32 resMantissa = (u32)(MulMantissa(selfMantissa, otherMantissa) >> MANTISSA_BITS);

	if (resMantissa > 0xFFFFFF)
	{
		resMantissa >>= 1;
		resExponent++;
	}

	if (resExponent > 255)
	{
		PS2Float result = PS2Float(sign | MAX_FLOATING_POINT_VALUE);
		result.of = true;
		return result;
	}
	else if (resExponent < 1)
	{
		PS2Float result = PS2Float(sign);
		result.uf = true;
		return result;
	}

	return PS2Float(sign | (u32)(resExponent << MANTISSA_BITS) | (resMantissa & 0x7FFFFF));
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

PS2Float PS2Float::Itof(s32 complement, s32 f1)
{
	if (f1 == 0)
		return PS2Float(0);

	s32 resExponent;

	bool negative = f1 < 0;

	if (f1 == -2147483648)
	{
		if (complement <= 0)
			// special case
			return PS2Float(0xcf000000);
		else
			f1 = 2147483647;
	}

	s32 u = std::abs(f1);

	s32 shifts;

	s32 lzcnt = Common::CountLeadingSignBits(u);
	if (lzcnt < 8)
	{
		s32 count = 8 - lzcnt;
		u >>= count;
		shifts = -count;
	}
	else
	{
		s32 count = lzcnt - 8;
		u <<= count;
		shifts = count;
	}

	resExponent = BIAS + MANTISSA_BITS - shifts - complement;

	if (resExponent >= 158)
		return negative ? PS2Float(0xcf000000) : PS2Float(0x4f000000);
	else if (resExponent >= 0)
		return PS2Float(negative, (u8)resExponent, (u32)u);

	return PS2Float(0);
}

s32 PS2Float::Ftoi(s32 complement, u32 f1)
{
	u32 a, result;

	a = f1;
	if ((f1 & 0x7F800000) == 0)
		result = 0;
	else
	{
		complement = (s32)(f1 >> MANTISSA_BITS & 0xFF) + complement;
		f1 &= 0x7FFFFF;
		f1 |= 0x800000;
		if (complement < 158)
		{
			if (complement > 126)
			{
				f1 = (f1 << 7) >> (31 - ((u8)complement - 126));
				if ((s32)a < 0)
					f1 = ~f1 + 1;
				result = f1;
			}
			else
				result = 0;
		}
		else if ((s32)a < 0)
			result = SIGNMASK;
		else
			result = MAX_FLOATING_POINT_VALUE;
	}

	return (s32)result;
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
	
	return a.CompareTo(b) >= 0 ? a.Sign() : b.Sign();
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

	return a.CompareTo(b) >= 0 ? a.Sign() : !b.Sign();
}
