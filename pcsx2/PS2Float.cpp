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
#include "PS2Float.h"
#include "Common.h"

//****************************************************************
// Radix Divisor
// Algorithm reference: DOI 10.1109/ARITH.1995.465363
//****************************************************************

struct CSAResult
{
	u32 sum;
	u32 carry;
};

static struct CSAResult CSA(u32 a, u32 b, u32 c)
{
	u32 u = a ^ b;
	u32 h = (a & b) | (u & c);
	u32 l = u ^ c;
	return {l, h << 1};
}

static s32 quotientSelect(struct CSAResult current)
{
	// Note: Decimal point is between bits 24 and 25
	u32 mask = (1 << 24) - 1; // Bit 23 needs to be or'd in instead of added
	s32 test = ((current.sum & ~mask) + current.carry) | (current.sum & mask);
	if (test >= 1 << 23)
	{ // test >= 0.25
		return 1;
	}
	else if (test < (s32)(~0u << 24))
	{ // test < -0.5
		return -1;
	}
	else
	{
		return 0;
	}
}

static u32 mantissa(u32 x)
{
	return (x & 0x7fffff) | 0x800000;
}

static u32 exponent(u32 x)
{
	return (x >> 23) & 0xff;
}

//****************************************************************
// Booth Multiplier
//****************************************************************

struct BoothRecode
{
	u32 data;
	u32 negate;
};

struct AddResult
{
	u32 lo;
	u32 hi;
};

static BoothRecode Booth(u32 a, u32 b, u32 bit)
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

static AddResult Add3(u32 a, u32 b, u32 c)
{
	u32 u = a ^ b;
	return {u ^ c, ((u & c) | (a & b)) << 1};
}

static u64 MulMantissa(u32 a, u32 b)
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

PS2Float::PS2Float(s32 value)
	: raw((u32)value)
{}

PS2Float::PS2Float(u32 value)
	: raw(value)
{}

PS2Float::PS2Float(float value)
	: raw(std::bit_cast<u32>(value))
{}

PS2Float::PS2Float(bool sign, u8 exponent, u32 mantissa)
	: raw((sign ? 1u : 0u) << 31 |
		(u32)(exponent << MANTISSA_BITS) |
		(mantissa & 0x7FFFFF))
{}

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
	{
		bool sign = DetermineAdditionOperationSign(*this, addend);

		if (IsDenormalized() && !addend.IsDenormalized())
			return PS2Float(sign, addend.Exponent(), addend.Mantissa());
		else if (!IsDenormalized() && addend.IsDenormalized())
			return PS2Float(sign, Exponent(), Mantissa());
		else if (IsDenormalized() && addend.IsDenormalized())
		{
			if (!Sign() || !addend.Sign())
				return PS2Float(false, 0, 0);
			else if (Sign() && addend.Sign())
				return PS2Float(true, 0, 0);
			else
				Console.Error("Unhandled addition operation flags");
		}
		else
			Console.Error("Both numbers are not denormalized");

		return PS2Float(0);
	}

	u32 a = raw;
	u32 b = addend.raw;

	//exponent difference
	s32 exp_diff = Exponent() - addend.Exponent();

	//diff = 1 .. 24, expt < expd
	if (exp_diff > 0 && exp_diff < 25)
	{
		exp_diff = exp_diff - 1;
		b = (MIN_FLOATING_POINT_VALUE << exp_diff) & b;
	}

	//diff = -24 .. -1 , expd < expt
	else if (exp_diff < 0 && exp_diff > -25)
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
	{
		bool sign = DetermineSubtractionOperationSign(*this, subtrahend);

		if (IsDenormalized() && !subtrahend.IsDenormalized())
			return PS2Float(sign, subtrahend.Exponent(), subtrahend.Mantissa());
		else if (!IsDenormalized() && subtrahend.IsDenormalized())
			return PS2Float(sign, Exponent(), Mantissa());
		else if (IsDenormalized() && subtrahend.IsDenormalized())
		{
			if (!Sign() || subtrahend.Sign())
				return PS2Float(false, 0, 0);
			else if (Sign() && !subtrahend.Sign())
				return PS2Float(true, 0, 0);
			else
				Console.Error("Unhandled subtraction operation flags");
		}
		else
			Console.Error("Both numbers are not denormalized");

		return PS2Float(0);
	}

	u32 a = raw;
	u32 b = subtrahend.raw;

	//exponent difference
	s32 exp_diff = Exponent() - subtrahend.Exponent();

	//diff = 1 .. 24, expt < expd
	if (exp_diff > 0 && exp_diff < 25)
	{
		exp_diff = exp_diff - 1;
		b = (MIN_FLOATING_POINT_VALUE << exp_diff) & b;
	}

	//diff = -24 .. -1 , expd < expt
	else if (exp_diff < 0 && exp_diff > -25)
	{
		exp_diff = -exp_diff;
		exp_diff = exp_diff - 1;
		a = a & (MIN_FLOATING_POINT_VALUE << exp_diff);
	}

	return PS2Float(a).DoAdd(PS2Float(b).Negate());
}

PS2Float PS2Float::Mul(PS2Float mulend)
{
	if (IsDenormalized() || mulend.IsDenormalized() || IsZero() || mulend.IsZero())
		return PS2Float(DetermineMultiplicationDivisionOperationSign(*this, mulend), 0, 0);

	return DoMul(mulend);
}

PS2Float PS2Float::MulAdd(PS2Float opsend, PS2Float optend)
{
	PS2Float mulres = opsend.Mul(optend);
	PS2Float addres = Add(mulres);
	u32 rawres = addres.raw;
	bool oflw = addres.of;
	bool uflw = addres.uf;
	DetermineMacException(3, raw, of, mulres.of, mulres.Sign() ? 1 : 0, rawres, oflw, uflw);
	PS2Float result = PS2Float(rawres);
	result.of = oflw;
	result.uf = uflw;
	return result;
}

PS2Float PS2Float::MulAddAcc(PS2Float opsend, PS2Float optend)
{
	PS2Float mulres = opsend.Mul(optend);
	PS2Float addres = Add(mulres);
	u32 rawres = addres.raw;
	bool oflw = addres.of;
	bool uflw = addres.uf;
	DetermineMacException(8, raw, of, mulres.of, mulres.Sign() ? 1 : 0, rawres, oflw, uflw);
	raw = rawres;
	of = oflw;
	PS2Float result = PS2Float(rawres);
	result.of = oflw;
	result.uf = uflw;
	return result;
}

PS2Float PS2Float::MulSub(PS2Float opsend, PS2Float optend)
{
	PS2Float mulres = opsend.Mul(optend);
	PS2Float subres = Sub(mulres);
	u32 rawres = subres.raw;
	bool oflw = subres.of;
	bool uflw = subres.uf;
	DetermineMacException(4, raw, of, mulres.of, mulres.Sign() ? 1 : 0, rawres, oflw, uflw);
	PS2Float result = PS2Float(rawres);
	result.of = oflw;
	result.uf = uflw;
	return result;
}

PS2Float PS2Float::MulSubAcc(PS2Float opsend, PS2Float optend)
{
	PS2Float mulres = opsend.Mul(optend);
	PS2Float subres = Sub(mulres);
	u32 rawres = subres.raw;
	bool oflw = subres.of;
	bool uflw = subres.uf;
	DetermineMacException(9, raw, of, mulres.of, mulres.Sign() ? 1 : 0, rawres, oflw, uflw);
	raw = rawres;
	of = oflw;
	PS2Float result = PS2Float(rawres);
	result.of = oflw;
	result.uf = uflw;
	return result;
}

PS2Float PS2Float::Div(PS2Float divend)
{
	u32 a = raw;
	u32 b = divend.raw;
	if (((a & 0x7F800000) == 0) && ((b & 0x7F800000) != 0))
	{
		u32 floatResult = 0;
		floatResult &= PS2Float::MAX_FLOATING_POINT_VALUE;
		floatResult |= (u32)(((s32)(b >> 31) != (s32)(a >> 31)) ? 1 : 0 & 1) << 31;
		return PS2Float(floatResult);
	}
	if (((a & 0x7F800000) != 0) && ((b & 0x7F800000) == 0))
	{
		u32 floatResult = PS2Float::MAX_FLOATING_POINT_VALUE;
		floatResult &= PS2Float::MAX_FLOATING_POINT_VALUE;
		floatResult |= (u32)(((s32)(b >> 31) != (s32)(a >> 31)) ? 1 : 0 & 1) << 31;
		PS2Float result = PS2Float(floatResult);
		result.dz = true;
		return result;
	}
	if (((a & 0x7F800000) == 0) && ((b & 0x7F800000) == 0))
	{
		u32 floatResult = PS2Float::MAX_FLOATING_POINT_VALUE;
		floatResult &= PS2Float::MAX_FLOATING_POINT_VALUE;
		floatResult |= (u32)(((s32)(b >> 31) != (s32)(a >> 31)) ? 1 : 0 & 1) << 31;
		PS2Float result = PS2Float(floatResult);
		result.iv = true;
		return result;
	}
	u32 am = mantissa(a) << 2;
	u32 bm = mantissa(b) << 2;
	struct CSAResult current = {am, 0};
	u32 quotient = 0;
	int quotientBit = 1;
	for (int i = 0; i < 25; i++)
	{
		quotient = (quotient << 1) + quotientBit;
		u32 add = quotientBit > 0 ? ~bm : quotientBit < 0 ? bm : 0;
		current.carry += quotientBit > 0;
		struct CSAResult csa = CSA(current.sum, current.carry, add);
		quotientBit = quotientSelect(quotientBit ? csa : current);
		current.sum = csa.sum << 1;
		current.carry = csa.carry << 1;
	}
	u32 sign = ((a ^ b) & 0x80000000);
	u32 Dvdtexp = exponent(a);
	u32 Dvsrexp = exponent(b);
	s32 cexp = Dvdtexp - Dvsrexp + 126;
	if (quotient >= (1 << 24))
	{
		cexp += 1;
		quotient >>= 1;
	}
	if (Dvdtexp == 0 && Dvsrexp == 0)
	{
		PS2Float result = PS2Float(sign | PS2Float::MAX_FLOATING_POINT_VALUE);
		result.iv = true;
		return result;
	}
	else if (Dvdtexp == 0 || Dvsrexp != 0)
	{
		if (Dvdtexp == 0 && Dvsrexp != 0) { return PS2Float(sign); }
	}
	else
	{
		PS2Float result = PS2Float(sign | PS2Float::MAX_FLOATING_POINT_VALUE);
		result.dz = true;
		return result;
	}
	if (cexp > 255)
	{
		PS2Float result = PS2Float(sign | PS2Float::MAX_FLOATING_POINT_VALUE);
		result.of = true;
		return result;
	}
	else if (cexp < 1)
	{
		PS2Float result = PS2Float(sign);
		result.uf = true;
		return result;
	}
	return (quotient & 0x7fffff) | (cexp << 23) | sign;
}

PS2Float PS2Float::Sqrt()
{
	u32 a = raw;
	if ((a & 0x7F800000) == 0)
	{
		PS2Float result = PS2Float(0);
		result.iv = ((a >> 31) & 1) != 0;
		return result;
	}
	u32 m = mantissa(a) << 1;
	if (!(a & 0x800000)) // If exponent is odd after subtracting bias of 127
		m <<= 1;
	struct CSAResult current = {m, 0};
	u32 quotient = 0;
	s32 quotientBit = 1;
	for (s32 i = 0; i < 25; i++)
	{
		// Adding n to quotient adds n * (2*quotient + n) to quotient^2
		// (which is what we need to subtract from the remainder)
		u32 adjust = quotient + (quotientBit << (24 - i));
		quotient += quotientBit << (25 - i);
		u32 add = quotientBit > 0 ? ~adjust : quotientBit < 0 ? adjust : 0;
		current.carry += quotientBit > 0;
		struct CSAResult csa = CSA(current.sum, current.carry, add);
		quotientBit = quotientSelect(quotientBit ? csa : current);
		current.sum = csa.sum << 1;
		current.carry = csa.carry << 1;
	}
	s32 Dvdtexp = exponent(a);
	if (Dvdtexp == 0)
		return PS2Float(0);
	Dvdtexp = (Dvdtexp + 127) >> 1;
	PS2Float result = PS2Float(((quotient >> 2) & 0x7fffff) | (Dvdtexp << 23));
	if (Sign())
	{
		if (result.Sign())
			result = result.Negate();
		result.iv = true;
	}
	return result;
}

PS2Float PS2Float::Rsqrt(PS2Float other)
{
	PS2Float sqrt = PS2Float(false, other.Exponent(), other.Mantissa()).Sqrt();
	PS2Float div = Div(sqrt);
	PS2Float result = PS2Float(div.raw);
	result.dz = sqrt.dz || div.dz;
	result.iv = sqrt.iv || div.iv;
	result.of = div.of;
	result.uf = div.uf;
	return result;
}

PS2Float PS2Float::ELENG(PS2Float y, PS2Float z)
{
	PS2Float ACC = Mul(*this);
	ACC.MulAddAcc(y, y);
	PS2Float p = ACC.MulAdd(z, z);
	return p.Sqrt();
}

PS2Float PS2Float::ERCPR()
{
	return PS2Float(ONE).Div(*this);
}

PS2Float PS2Float::ERLENG(PS2Float y, PS2Float z)
{
	PS2Float ACC = Mul(*this);
	ACC.MulAddAcc(y, y);
	PS2Float p = ACC.MulAdd(z, z);
	p = PS2Float(ONE).Rsqrt(p);
	return p;
}

PS2Float PS2Float::ERSADD(PS2Float y, PS2Float z)
{
	PS2Float ACC = Mul(*this);
	ACC.MulAddAcc(y, y);
	PS2Float p = ACC.MulAdd(z, z);
	p = PS2Float(ONE).Div(p);
	return p;
}

PS2Float PS2Float::ESQRT()
{
	return Sqrt();
}

PS2Float PS2Float::ESQUR()
{
	return Mul(*this);
}

PS2Float PS2Float::ESUM(PS2Float y, PS2Float z, PS2Float w)
{
	PS2Float ACC = Mul(PS2Float(ONE));
	ACC.MulAddAcc(y, PS2Float(ONE));
	ACC.MulAddAcc(z, PS2Float(ONE));
	return ACC.MulAdd(w, PS2Float(ONE));
}

PS2Float PS2Float::ERSQRT()
{
	return PS2Float(ONE).Rsqrt(*this);
}

PS2Float PS2Float::ESADD(PS2Float y, PS2Float z)
{
	PS2Float ACC = Mul(*this);
	ACC.MulAddAcc(y, y);
	return ACC.MulAdd(z, z);
}

PS2Float PS2Float::EEXP()
{
	float consts[6] = {0.249998688697815f, 0.031257584691048f, 0.002591371303424f,
		0.000171562001924f, 0.000005430199963f, 0.000000690600018f};

	PS2Float tmp1 = Mul(*this);
	PS2Float ACC = Mul(PS2Float(consts[0]));
	PS2Float tmp2 = tmp1.Mul(*this);
	ACC.MulAddAcc(tmp1, PS2Float(consts[1]));
	tmp1 = tmp2.Mul(*this);
	ACC.MulAddAcc(tmp2, PS2Float(consts[2]));
	tmp2 = tmp1.Mul(*this);
	ACC.MulAddAcc(tmp1, PS2Float(consts[3]));
	tmp1 = tmp2.Mul(*this);
	ACC.MulAddAcc(tmp2, PS2Float(consts[4]));
	ACC.MulAddAcc(PS2Float(ONE), PS2Float(ONE));
	PS2Float p = ACC.MulAdd(tmp1, PS2Float(consts[5]));
	p = p.Mul(p);
	p = p.Mul(p);
	p = PS2Float(ONE).Div(p);

	return p;
}

PS2Float PS2Float::EATAN()
{
	float eatanconst[9] = {0.999999344348907f, -0.333298563957214f, 0.199465364217758f, -0.13085337519646f,
		0.096420042216778f, -0.055909886956215f, 0.021861229091883f, -0.004054057877511f,
		0.785398185253143f};

	PS2Float tmp1 = Add(PS2Float(ONE));
	PS2Float tmp2 = Sub(PS2Float(ONE));
	*this = tmp2.Div(tmp1);
	PS2Float tmp3 = Mul(*this);
	PS2Float ACC = PS2Float(eatanconst[0]).Mul(*this);
	tmp1 = tmp3.Mul(*this);
	tmp2 = tmp1.Mul(tmp3);
	ACC.MulAddAcc(tmp1, PS2Float(eatanconst[1]));
	tmp1 = tmp2.Mul(tmp3);
	ACC.MulAddAcc(tmp2, PS2Float(eatanconst[2]));
	tmp2 = tmp1.Mul(tmp3);
	ACC.MulAddAcc(tmp1, PS2Float(eatanconst[3]));
	tmp1 = tmp2.Mul(tmp3);
	ACC.MulAddAcc(tmp2, PS2Float(eatanconst[4]));
	tmp2 = tmp1.Mul(tmp3);
	ACC.MulAddAcc(tmp1, PS2Float(eatanconst[5]));
	tmp1 = tmp2.Mul(tmp3);
	ACC.MulAddAcc(tmp2, PS2Float(eatanconst[6]));
	ACC.MulAddAcc(PS2Float(ONE), PS2Float(eatanconst[8]));

	return ACC.MulAdd(tmp1, PS2Float(eatanconst[7]));
}

PS2Float PS2Float::ESIN()
{
	float sinconsts[5] = {1.0f, -0.166666567325592f, 0.008333025500178f, -0.000198074136279f, 0.000002601886990f};

	PS2Float tmp3 = Mul(*this);
	PS2Float ACC = Mul(PS2Float(sinconsts[0]));
	PS2Float tmp1 = tmp3.Mul(*this);
	PS2Float tmp2 = tmp1.Mul(tmp3);
	ACC.MulAddAcc(tmp1, PS2Float(sinconsts[1]));
	tmp1 = tmp2.Mul(tmp3);
	ACC.MulAddAcc(tmp2, PS2Float(sinconsts[2]));
	tmp2 = tmp1.Mul(tmp3);
	ACC.MulAddAcc(tmp1, PS2Float(sinconsts[3]));

	return ACC.MulAdd(tmp2, PS2Float(sinconsts[4]));
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
	return PS2Float(raw ^ SIGNMASK);
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

s32 PS2Float::CompareOperands(PS2Float other)
{
	u32 selfTwoComplementVal = Abs();
	u32 otherTwoComplementVal = other.Abs();

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
	u8 selfExponent = Exponent();
	s32 resExponent = selfExponent - other.Exponent();

	if (resExponent < 0)
		return other.DoAdd(*this);
	else if (resExponent >= 25)
		return *this;

	const u8 roundingMultiplier = 6;

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

u8 PS2Float::Clip(u32 f1, u32 f2, bool& cplus, bool& cminus)
{
	bool resultPlus = false;
	bool resultMinus = false;
	u32 a;

	if ((f1 & 0x7F800000) == 0)
	{
		f1 &= 0xFF800000;
	}

	a = f1;

	if ((f2 & 0x7F800000) == 0)
	{
		f2 &= 0xFF800000;
	}

	f1 = f1 & MAX_FLOATING_POINT_VALUE;
	f2 = f2 & MAX_FLOATING_POINT_VALUE;

	if ((-1 < (int)a) && (f2 < f1))
		resultPlus = true;

	cplus = resultPlus;

	if (((int)a < 0) && (f2 < f1))
		resultMinus = true;

	cminus = resultMinus;

	return 0;
}

bool PS2Float::DetermineMultiplicationDivisionOperationSign(PS2Float a, PS2Float b)
{
	return a.Sign() ^ b.Sign();
}

bool PS2Float::DetermineAdditionOperationSign(PS2Float a, PS2Float b)
{
	return a.CompareOperands(b) >= 0 ? a.Sign() : b.Sign();
}

bool PS2Float::DetermineSubtractionOperationSign(PS2Float a, PS2Float b)
{
	return a.CompareOperands(b) >= 0 ? a.Sign() : !b.Sign();
}

u8 PS2Float::DetermineMacException(u8 mode, u32 acc, bool acc_oflw, bool moflw, s32 msign, u32& addsubres, bool& oflw, bool& uflw)
{
	bool roundToMax;

	if ((mode == 3) || (mode == 8))
		roundToMax = msign == 0;
	else
	{
		if ((mode != 4) && (mode != 9))
		{
			Console.Error("Unhandled MacFlag operation flags");
			return 1;
		}

		roundToMax = msign != 0;
	}

	if (!acc_oflw)
	{
		if (moflw)
		{
			if (roundToMax)
			{
				addsubres = MAX_FLOATING_POINT_VALUE;
				uflw = false;
				oflw = true;
			}
			else
			{
				addsubres = MIN_FLOATING_POINT_VALUE;
				uflw = false;
				oflw = true;
			}
		}
	}
	else if (!moflw)
	{
		addsubres = acc;
		uflw = false;
		oflw = true;
	}
	else if (roundToMax)
	{
		addsubres = MAX_FLOATING_POINT_VALUE;
		uflw = false;
		oflw = true;
	}
	else
	{
		addsubres = MIN_FLOATING_POINT_VALUE;
		uflw = false;
		oflw = true;
	}

	return 0;
}
