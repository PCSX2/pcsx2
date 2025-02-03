// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "FpgaDiv.h"
#include "PS2Float.h"
#include "Common.h"

FpgaDiv::FpgaDiv(bool divMode, u32 f1, u32 f2)
{
	FpgaDiv::divMode = divMode;

	if (divMode)
	{
		if (((f1 & 0x7F800000) == 0) && ((f2 & 0x7F800000) != 0))
		{
			floatResult = 0;
			floatResult &= PS2Float::MAX_FLOATING_POINT_VALUE;
			floatResult |= (u32)(((s32)(f2 >> 31) != (s32)(f1 >> 31)) ? 1 : 0 & 1) << 31;
			return;
		}
		if (((f1 & 0x7F800000) != 0) && ((f2 & 0x7F800000) == 0))
		{
			dz = true;
			floatResult = PS2Float::MAX_FLOATING_POINT_VALUE;
			floatResult &= PS2Float::MAX_FLOATING_POINT_VALUE;
			floatResult |= (u32)(((s32)(f2 >> 31) != (s32)(f1 >> 31)) ? 1 : 0 & 1) << 31;
			return;
		}
		if (((f1 & 0x7F800000) == 0) && ((f2 & 0x7F800000) == 0))
		{
			iv = true;
			floatResult = PS2Float::MAX_FLOATING_POINT_VALUE;
			floatResult &= PS2Float::MAX_FLOATING_POINT_VALUE;
			floatResult |= (u32)(((s32)(f2 >> 31) != (s32)(f1 >> 31)) ? 1 : 0 & 1) << 31;
			return;
		}
	}
	else if ((f2 & 0x7F800000) == 0)
	{
		floatResult = 0;
		iv = ((f2 >> 31) & 1) != 0;
		return;
	}

	u32 floatDivisor, floatDividend;
	s32 i, j, csaRes;
	s32 man = 0;
	s32 QuotientValueDomain = 1;

	Product[0] = 1;
	Carry[25] = 1;

	if (divMode)
	{
		floatDividend = f1;
		floatDivisor = f2;
	}
	else
	{
		floatDividend = f2;
		floatDivisor = f1;
	}

	u8 Dvdtexp = (u8)((floatDividend >> 23) & 0xFF);
	u8 Dvsrexp = (u8)((floatDivisor >> 23) & 0xFF);
	s32 Dvdtsign = (s32)(floatDividend >> 31);
	s32 Dvsrsign = (s32)(floatDivisor >> 31);

	Sum[0] = 1;
	Sum[1] = ((floatDividend & 0x400000) != 0);
	Sum[2] = ((floatDividend & 0x200000) != 0);
	Sum[3] = ((floatDividend & 0x100000) != 0);
	Sum[4] = ((floatDividend & 0x80000) != 0);
	Sum[5] = ((floatDividend & 0x40000) != 0);
	Sum[6] = ((floatDividend & 0x20000) != 0);
	Sum[7] = (s32)((floatDividend >> 16) & 1);
	Sum[8] = (s32)((floatDividend >> 15) & 1);
	Sum[9] = ((floatDividend & 0x4000) != 0);
	Sum[10] = ((floatDividend & 0x2000) != 0);
	Sum[11] = ((floatDividend & 0x1000) != 0);
	Sum[12] = ((floatDividend & 0x800) != 0);
	Sum[13] = ((floatDividend & 0x400) != 0);
	Sum[14] = ((floatDividend & 0x200) != 0);
	Sum[15] = (s32)((floatDividend >> 8) & 1);
	Sum[16] = (s32)((floatDividend >> 7) & 1);
	Sum[17] = ((floatDividend & 0x40) != 0);
	Sum[18] = ((floatDividend & 0x20) != 0);
	Sum[19] = ((floatDividend & 0x10) != 0);
	Sum[20] = ((floatDividend & 8) != 0);
	Sum[21] = ((floatDividend & 4) != 0);
	Sum[22] = ((floatDividend & 2) != 0);
	Sum[23] = (s32)(floatDividend & 1);
	Sum[24] = 0;
	Sum[25] = 0;

	Divisor[0] = 1;
	Divisor[1] = ((floatDivisor & 0x400000) != 0);
	Divisor[2] = ((floatDivisor & 0x200000) != 0);
	Divisor[3] = ((floatDivisor & 0x100000) != 0);
	Divisor[4] = ((floatDivisor & 0x80000) != 0);
	Divisor[5] = ((floatDivisor & 0x40000) != 0);
	Divisor[6] = ((floatDivisor & 0x20000) != 0);
	Divisor[7] = (s32)((floatDivisor >> 16) & 1);
	Divisor[8] = (s32)((floatDivisor >> 15) & 1);
	Divisor[9] = ((floatDivisor & 0x4000) != 0);
	Divisor[10] = ((floatDivisor & 0x2000) != 0);
	Divisor[11] = ((floatDivisor & 0x1000) != 0);
	Divisor[12] = ((floatDivisor & 0x800) != 0);
	Divisor[13] = ((floatDivisor & 0x400) != 0);
	Divisor[14] = ((floatDivisor & 0x200) != 0);
	Divisor[15] = (s32)((floatDivisor >> 8) & 1);
	Divisor[16] = (s32)((floatDivisor >> 7) & 1);
	Divisor[17] = ((floatDivisor & 0x40) != 0);
	Divisor[18] = ((floatDivisor & 0x20) != 0);
	Divisor[19] = ((floatDivisor & 0x10) != 0);
	Divisor[20] = ((floatDivisor & 8) != 0);
	Divisor[21] = ((floatDivisor & 4) != 0);
	Divisor[22] = ((floatDivisor & 2) != 0);
	Divisor[23] = (s32)(floatDivisor & 1);
	Divisor[24] = 0;
	Divisor[25] = 0;

	if (!divMode && Dvdtexp % 2 == 1)
	{
		for (i = 0; i <= 24; i++)
		{
			Sum[25 - i] = Sum[24 - i];
		}
		Sum[0] = 0;
	}

	for (i = 0; i <= 24; ++i)
	{
		MultipleFormation(QuotientValueDomain);
		csaRes = CSAQSLAdder(QuotientValueDomain);
		ProductQuotientRestTransformation(i, QuotientValueDomain);
		Carry[25] = csaRes > 0 ? 1 : 0;
		QuotientValueDomain = csaRes;
	}

	s32 sign = SignCalc(Dvdtsign, Dvsrsign) ? 1 : 0;
	s32 exp = ExpCalc(Dvdtexp, Dvsrexp);

	if (divMode && (Quotient[0] == 0))
		exp--;

	if (divMode)
	{
		if ((Dvdtexp == 0) && (Dvsrexp == 0))
		{
			iv = true;
			exp = 255;
			for (i = 0; i < 25; i++)
			{
				Quotient[i] = 1;
			}
		}
		else if ((Dvdtexp == 0) || (Dvsrexp != 0))
		{
			if ((Dvdtexp == 0) && (Dvsrexp != 0))
			{
				exp = 0;
				for (i = 0; i < 25; i++)
				{
					Quotient[i] = 0;
				}
			}
		}
		else
		{
			dz = true;
			exp = 255;
			for (i = 0; i < 25; i++)
			{
				Quotient[i] = 1;
			}
		}
	}
	else
	{
		if (Dvdtexp == 0)
		{
			sign = 0;
			exp = 0;
			for (i = 0; i < 25; i++)
			{
				Quotient[i] = 0;
			}
		}
		if (Dvdtsign == 1)
		{
			iv = true;
			sign = 0;
		}
	}

	if (divMode)
	{
		if (exp < 256)
		{
			if (exp < 1)
			{
				uf = true;
				exp = 0;
				for (i = 0; i < 25; i++)
				{
					Quotient[i] = 0;
				}
			}
		}
		else
		{
			of = true;
			exp = 255;
			for (i = 0; i < 25; i++)
			{
				Quotient[i] = 1;
			}
		}
	}

	if (divMode)
		j = 2 - Quotient[0];
	else
		j = 1;

	for (i = j; i < j + 23; i++)
	{
		man = man * 2 + Quotient[i];
	}

	floatResult = 0;
	floatResult &= PS2Float::MAX_FLOATING_POINT_VALUE;
	floatResult |= (u32)(sign & 1) << 31;
	floatResult &= 0x807FFFFF;
	floatResult |= (u32)(exp & 0xFF) << 23;
	floatResult &= 0xFF800000;
	floatResult |= (u32)man & 0x7FFFFF;
}

bool FpgaDiv::SignCalc(s32 Dvdtsign, s32 Dvsrsign)
{
	return divMode && Dvsrsign != Dvdtsign;
}

bool FpgaDiv::BitInvert(s32 val)
{
	return val < 1;
}

s32 FpgaDiv::ExpCalc(s32 Dvdtexp, s32 Dvsrexp)
{
	s32 result;

	if (divMode)
		return Dvdtexp - Dvsrexp + 127;
	if ((Dvdtexp & 1) != 0)
		result = (Dvdtexp - 127) / 2;
	else
		result = (Dvdtexp - 128) / 2;
	return result + 127;
}

s32 FpgaDiv::CSAQSLAdder(s32 QuotientValueDomain)
{
	s32 CarryArray[4];
	s32 SumArray[4];
	s32 i;
	s32 tmpSum;
	s32 tmpCarry;

	if (QuotientValueDomain == 0)
	{
		SumArray[0] = SubSum;
		CarryArray[0] = SubCarry;
		for (i = 1; i <= 3; i++)
		{
			SumArray[i] = Sum[i - 1];
			CarryArray[i] = Carry[i - 1];
		}
	}
	CSAAdder(SubSum, SubCarry, SubMult, tmpSum, tmpCarry);
	SubSum0 = tmpSum;
	CSAAdder(Sum[0], Carry[0], Mult[0], tmpSum, tmpCarry);
	SubSum = tmpSum;
	SubCarry0 = tmpCarry;
	CSAAdder(Sum[1], Carry[1], Mult[1], tmpSum, tmpCarry);
	Sum[0] = tmpSum;
	SubCarry = tmpCarry;
	for (i = 2; i <= 25; i++)
	{
		CSAAdder(Sum[i], Carry[i], Mult[i], tmpSum, tmpCarry);
		Sum[i - 1] = tmpSum;
		Carry[i - 2] = tmpCarry;
	}
	Sum[i - 1] = 0;
	Carry[i - 2] = 0;
	Carry[i - 1] = ~QuotientValueDomain;
	Carry[i - 1] = (s32)((u32)Carry[i - 1] >> 31);
	if (QuotientValueDomain != 0)
	{
		SumArray[0] = SubSum0;
		CarryArray[0] = SubCarry0;
		SumArray[1] = SubSum;
		CarryArray[1] = SubCarry;
		for (i = 2; i <= 3; i++)
		{
			SumArray[i] = Sum[i - 2];
			CarryArray[i] = Carry[i - 2];
		}
	}
	return QSLAdder(SumArray, CarryArray);
}

s32 FpgaDiv::QSLAdder(s32 SumArray[], s32 CarryArray[])
{
	s32 specialCondition = 0;
	s32 result;
	s32 claResult = CLAAdder(SumArray, CarryArray);

	if (SumArray[3] == 1 || CarryArray[3] == 1 || (claResult % 2 != 0))
		specialCondition = 1;

	switch (claResult)
	{
		case 0:
			result = specialCondition;
			break;
		case 1:
			result = specialCondition;
			break;
		case 2:
		case 3:
			result = 1;
			break;
		case 4:
		case 5:
		case 6:
			result = -1;
			break;
		case 7:
			result = 0;
			break;
		default:
			result = 0;
			break;
	}

	return result;
}

s32 FpgaDiv::ProductQuotientRestTransformation(s32 increment, s32 QuotientValueDomain)
{
	s32 i;

	Product[increment] = 0;
	Product[increment + 1] = 1;
	if (QuotientValueDomain == 0)
		Rest[increment] = 1;
	else
	{
		if (QuotientValueDomain == -1)
		{
			for (i = 0; i <= 25; i++)
				Quotient[i] = Rest[i];
			Quotient[increment] = 1;
			return 0;
		}
		else if (QuotientValueDomain == 1)
		{
			for (i = 0; i <= 25; ++i)
				Rest[i] = Quotient[i];
			Quotient[increment] = 1;
			return 0;
		}
		Console.Error("PQRTF: Quotient value domain error!");
		return -1;
	}

	return 0;
}

s32 FpgaDiv::CSAAdder(s32 sum, s32 carry, s32 mult, s32& resSum, s32& resCarry)
{
	s32 addResult = carry + sum + mult;
	resCarry = 0;
	resSum = 0;
	if (addResult == 1)
		resSum = 1;
	else if (addResult == 2)
		resCarry = 1;
	else if (addResult == 3)
	{
		resSum = 1;
		resCarry = 1;
	}

	return 0;
}

s32 FpgaDiv::CLAAdder(s32 SumArray[], s32 CarryArray[])
{
	return (2 * CarryArray[1] + 4 * CarryArray[0] + CarryArray[2] + 2 * SumArray[1] + 4 * SumArray[0] + SumArray[2]) % 8;
}

s32 FpgaDiv::MultipleFormation(s32 QuotientValueDomain)
{
	s32 i;

	if (QuotientValueDomain == 0)
	{
		SubMult = 0;
		for (i = 0; i <= 25; i++)
			Mult[i] = 0;
	}
	else if (divMode)
		DivideModeFormation(QuotientValueDomain);
	else
		RootModeFormation(QuotientValueDomain);

	return 0;
}

s32 FpgaDiv::DivideModeFormation(s32 QuotientValueDomain)
{
	s32 i;

	if (QuotientValueDomain <= 0)
	{
		SubMult = 0;
		for (i = 0; i <= 25; i++)
			Mult[i] = Divisor[i];
	}
	else
	{
		SubMult = 1;
		for (i = 0; i <= 25; i++)
			Mult[i] = BitInvert(Divisor[i]) ? 1 : 0;
	}

	return 0;
}

s32 FpgaDiv::RootModeFormation(s32 QuotientValueDomain)
{
	s32 i;

	if (QuotientValueDomain <= 0)
	{
		SubMult = 0;
		if (Product[0] == 1)
			Mult[0] = 1;
		else
			Mult[0] = Rest[0];
		for (i = 1; i <= 25; i++)
		{
			if (Product[i - 1] == 1 || Product[i] == 1)
				Mult[i] = 1;
			else
				Mult[i] = Rest[i];
		}
	}
	else
	{
		SubMult = 1;
		Mult[0] = BitInvert(Quotient[0]) ? 1 : 0;
		for (i = 1; i <= 25; i++)
		{
			if (Product[i - 1] == 1)
				Mult[i] = 0;
			else
				Mult[i] = BitInvert(Quotient[i]) ? 1 : 0;
		}
	}

	return 0;
}