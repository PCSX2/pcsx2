// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

namespace x86Emitter
{

	enum G1Type
	{
		G1Type_ADD = 0,
		G1Type_OR,
		G1Type_ADC,
		G1Type_SBB,
		G1Type_AND,
		G1Type_SUB,
		G1Type_XOR,
		G1Type_CMP
	};

	// --------------------------------------------------------------------------------------
	//  xImpl_Group1
	// --------------------------------------------------------------------------------------
	struct xImpl_Group1
	{
		G1Type InstType;

		void operator()(const xRegisterInt& to, const xRegisterInt& from) const;

		void operator()(const xIndirectVoid& to, const xRegisterInt& from) const;
		void operator()(const xRegisterInt& to, const xIndirectVoid& from) const;
		void operator()(const xRegisterInt& to, int imm) const;
		void operator()(const xIndirect64orLess& to, int imm) const;
	};

	// ------------------------------------------------------------------------
	// This class combines x86 with SSE/SSE2 logic operations (ADD, OR, and NOT).
	// Note: ANDN [AndNot] is handled below separately.
	//
	struct xImpl_G1Logic : public xImpl_Group1
	{
		xImplSimd_3Arg PS; // packed single precision
		xImplSimd_3Arg PD; // packed double precision
	};

	// ------------------------------------------------------------------------
	// This class combines x86 with SSE/SSE2 arithmetic operations (ADD/SUB).
	//
	struct xImpl_G1Arith : public xImpl_Group1
	{
		xImplSimd_3Arg PS; // packed single precision
		xImplSimd_3Arg PD; // packed double precision
		xImplSimd_3Arg SS; // scalar single precision
		xImplSimd_3Arg SD; // scalar double precision
	};

} // End namespace x86Emitter
