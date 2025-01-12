// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <vector>
#include <array>

class FpgaDiv
{
public:

	bool dz = false;
	bool iv = false;
	bool of = false;
	bool uf = false;

	u32 floatResult;

	FpgaDiv(bool divMode, u32 f1, u32 f2);

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