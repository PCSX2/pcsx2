// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <vector>
#include <array>

class PS2Div
{
	struct CSAResult
	{
		uint32_t sum;
		uint32_t carry;
	};

	static struct CSAResult CSA(uint32_t a, uint32_t b, uint32_t c)
	{
		uint32_t u = a ^ b;
		uint32_t h = (a & b) | (u & c);
		uint32_t l = u ^ c;
		return {l, h << 1};
	}

public:

	bool dz = false;
	bool iv = false;
	bool of = false;
	bool uf = false;

	u32 floatResult;

	PS2Div(bool divMode, u32 f1, u32 f2);

protected:

private:

	s32 Rest[26] = {0};
	s32 Quotient[26] = {0};
	s32 Product[26] = {0};
	s32 Sum[26] = {0};
	s32 Divisor[26] = {0};
	s32 Carry[26] = {0};
	s32 Mult[26] = {0};

	bool divMode;

	s32 SubCarry = 0;
	s32 SubCarry0 = 0;
	s32 SubSum = 0;
	s32 SubSum0 = 0;
	s32 SubMult = 0;

	static s32 quotientSelect(CSAResult current);

	static u32 mantissa(u32 x);

	static u32 exponent(u32 x);

	u32 fastdiv(u32 a, u32 b);

	bool SignCalc(s32 Dvdtsign, s32 Dvsrsign);

	bool BitInvert(s32 val);

	s32 ExpCalc(s32 Dvdtexp, s32 Dvsrexp);

	s32 CSAQSLAdder(s32 QuotientValueDomain);

	s32 QSLAdder(s32 SumArray[], s32 CarryArray[]);

	s32 ProductQuotientRestTransformation(s32 increment, s32 QuotientValueDomain);

	s32 CSAAdder(s32 sum, s32 carry, s32 mult, s32& resSum, s32& resCarry);

	s32 CLAAdder(s32 SumArray[], s32 CarryArray[]);

	s32 MultipleFormation(s32 QuotientValueDomain);

	s32 DivideModeFormation(s32 QuotientValueDomain);

	s32 RootModeFormation(s32 QuotientValueDomain);
};