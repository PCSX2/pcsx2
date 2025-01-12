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

    PS2Float(s32 value);

    PS2Float(u32 value);

    PS2Float(float value);

    PS2Float(bool sign, u8 exponent, u32 mantissa);

    static PS2Float Max();

    static PS2Float Min();

    static PS2Float One();

    static PS2Float MinOne();

    static PS2Float SolveAddSubDenormalizedOperation(PS2Float a, PS2Float b, bool add);

    static PS2Float SolveMultiplicationDenormalizedOperation(PS2Float a, PS2Float b);

    static PS2Float SolveDivisionDenormalizedOperation(PS2Float a, PS2Float b);

    static PS2Float Itof(s32 complement, s32 f1);

    static s32 Ftoi(s32 complement, u32 f1);

    PS2Float Add(PS2Float addend);

    PS2Float Sub(PS2Float subtrahend);

    PS2Float Mul(PS2Float mulend);

    PS2Float Div(PS2Float divend);

    PS2Float Sqrt();

    PS2Float Rsqrt(PS2Float other);

    PS2Float Pow(s32 exponent);

    bool IsDenormalized();

    bool IsZero();

    u32 Abs();

    PS2Float Negate();

    s32 CompareToSign(PS2Float other);

    s32 CompareTo(PS2Float other);

    double ToDouble();

    std::string ToString();

protected:

private:

    PS2Float DoAdd(PS2Float other);

    PS2Float DoMul(PS2Float other);

    static bool DetermineMultiplicationDivisionOperationSign(PS2Float a, PS2Float b);

    static bool DetermineAdditionOperationSign(PS2Float a, PS2Float b);

    static bool DetermineSubtractionOperationSign(PS2Float a, PS2Float b);
};
