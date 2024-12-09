// SPDX-FileCopyrightText: 2002-2024 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include <vector>

class PS2Float
{
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

    static u64 MulMantissa(u32 a, u32 b);

    static BoothRecode Booth(u32 a, u32 b, u32 bit);

    static AddResult Add3(u32 a, u32 b, u32 c);

public:
    
    static constexpr u8 BIAS = 127;
    static constexpr u32 SIGNMASK = 0x80000000;
    static constexpr u32 MAX_FLOATING_POINT_VALUE = 0x7FFFFFFF;
    static constexpr u32 MIN_FLOATING_POINT_VALUE = 0xFFFFFFFF;
    static constexpr u32 POSITIVE_INFINITY_VALUE = 0x7F800000;
    static constexpr u32 NEGATIVE_INFINITY_VALUE = 0xFF800000;
    static constexpr u32 ONE = 0x3F800000;
    static constexpr u32 MIN_ONE = 0xBF800000;
    static constexpr int IMPLICIT_LEADING_BIT_POS = 23;

    static constexpr s8 msb[256] = {
		-1, 0, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
		5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,
		7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7
    };

    static constexpr s32 debruijn32[64] = {
		32, 8, 17, -1, -1, 14, -1, -1, -1, 20, -1, -1, -1, 28, -1, 18,
		-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 26, 25, 24,
		4, 11, 23, 31, 3, 7, 10, 16, 22, 30, -1, -1, 2, 6, 13, 9,
		-1, 15, -1, 21, -1, 29, 19, -1, -1, -1, -1, -1, 1, 27, 5, 12
    };

    static constexpr s32 normalizeAmounts[] = {
		0, 0, 0, 0, 0, 0, 0, 0, 0, 8, 8, 8, 8, 8, 8, 8, 8, 16, 16, 16, 16, 16, 16, 16, 16, 24, 24, 24, 24, 24, 24, 24
    };

    u32 raw;

    constexpr u32 Mantissa() const { return raw & 0x7FFFFF; }
    constexpr u8 Exponent() const { return (raw >> 23) & 0xFF; }
    constexpr bool Sign() const { return ((raw >> 31) & 1) != 0; }

    PS2Float(u32 value);

    PS2Float(bool sign, u8 exponent, u32 mantissa);

    static PS2Float Max();

    static PS2Float Min();

    static PS2Float One();

    static PS2Float MinOne();

    PS2Float Add(PS2Float addend);

    PS2Float Sub(PS2Float subtrahend);

    PS2Float Mul(PS2Float mulend);

    PS2Float Div(PS2Float divend);

    PS2Float Sqrt();

    PS2Float Rsqrt(PS2Float other);

    bool IsDenormalized();

    bool IsAbnormal();

    bool IsZero();

    u32 Abs();

    PS2Float Negate();

    PS2Float RoundTowardsZero();

    s32 CompareTo(PS2Float other);

    s32 CompareOperand(PS2Float other);

    double ToDouble();

    std::string ToString();

protected:

private:

    PS2Float DoAdd(PS2Float other);

    PS2Float DoMul(PS2Float other);

    PS2Float DoDiv(PS2Float other);

    static PS2Float SolveAbnormalAdditionOrSubtractionOperation(PS2Float a, PS2Float b, bool add);

    static PS2Float SolveAbnormalMultiplicationOrDivisionOperation(PS2Float a, PS2Float b, bool mul);

    static PS2Float SolveAddSubDenormalizedOperation(PS2Float a, PS2Float b, bool add);

    static PS2Float SolveMultiplicationDenormalizedOperation(PS2Float a, PS2Float b);

    static PS2Float SolveDivisionDenormalizedOperation(PS2Float a, PS2Float b);

    static bool DetermineMultiplicationDivisionOperationSign(PS2Float a, PS2Float b);

    static bool DetermineAdditionOperationSign(PS2Float a, PS2Float b);

    static bool DetermineSubtractionOperationSign(PS2Float a, PS2Float b);

    static s32 GetMostSignificantBitPosition(u32 value);

    static s32 BitScanReverse8(s32 b);

    static s32 clz(s32 x);
};
