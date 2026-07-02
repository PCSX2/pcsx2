// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

class PS2Float
{
public:
	static constexpr u8 BIAS = 127;
	static constexpr u8 MANTISSA_BITS = 23;
	static constexpr u32 SIGNMASK = 0x80000000;
	static constexpr u32 MAX_FLOATING_POINT_VALUE = 0x7FFFFFFF;
	static constexpr u32 MIN_FLOATING_POINT_VALUE = 0xFFFFFFFF;
	static constexpr u32 ONE = 0x3F800000;
	static constexpr u32 MIN_ONE = 0xBF800000;

	bool dz = false;
	bool iv = false;
	bool of = false;
	bool uf = false;

	u32 raw;

	constexpr u32 Mantissa() const { return raw & 0x7FFFFF; }
	constexpr u8 Exponent() const { return (raw >> 23) & 0xFF; }
	constexpr bool Sign() const { return ((raw >> 31) & 1) != 0; }

	__fi PS2Float(s32 value)
		: raw((u32)value)
	{
	}

	__fi PS2Float(u32 value)
		: raw(value)
	{
	}

	__fi PS2Float(float value)
		: raw(std::bit_cast<u32>(value))
	{
	}

	__fi PS2Float(bool sign, u8 exponent, u32 mantissa)
		: raw((sign ? 1u : 0u) << 31 |
			  (u32)(exponent << MANTISSA_BITS) |
			  (mantissa & 0x7FFFFF))
	{
	}

	__fi static PS2Float Max()
	{
		return PS2Float(MAX_FLOATING_POINT_VALUE);
	}

	__fi static PS2Float Min()
	{
		return PS2Float(MIN_FLOATING_POINT_VALUE);
	}

	__fi static PS2Float One()
	{
		return PS2Float(ONE);
	}

	__fi static PS2Float MinOne()
	{
		return PS2Float(MIN_ONE);
	}

	static u8 Clip(u32 f1, u32 f2, bool& cplus, bool& cminus);

	PS2Float Add(PS2Float addend);

	PS2Float Sub(PS2Float subtrahend);

	PS2Float Mul(PS2Float mulend);

	PS2Float MulAdd(PS2Float opsend, PS2Float optend);

	PS2Float MulAddAcc(PS2Float opsend, PS2Float optend);

	PS2Float MulSub(PS2Float opsend, PS2Float optend);

	PS2Float MulSubAcc(PS2Float opsend, PS2Float optend);

	PS2Float Div(PS2Float divend);

	PS2Float Sqrt();

	PS2Float Rsqrt(PS2Float other);

	PS2Float ERCPR();

	PS2Float ESQRT();

	PS2Float ESQUR();

	PS2Float ERSQRT();

	bool IsDenormalized();

	bool IsZero();

	u32 Abs();

	PS2Float Negate();

	s32 CompareTo(PS2Float other);

	s32 CompareOperands(PS2Float other);

	double ToDouble();

	std::string ToString();

protected:
private:
	PS2Float DoAdd(PS2Float other);

	PS2Float DoMul(PS2Float other);

	static bool DetermineMultiplicationDivisionOperationSign(PS2Float a, PS2Float b);

	static bool DetermineAdditionOperationSign(PS2Float a, PS2Float b);

	static bool DetermineSubtractionOperationSign(PS2Float a, PS2Float b);

    static u8 DetermineMacException(u8 mode, u32 acc, bool acc_oflw, bool moflw, s32 msign, u32& addsubres, bool& oflw, bool& uflw);
};
