// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

namespace x86Emitter
{

	enum G3Type
	{
		G3Type_NOT = 2,
		G3Type_NEG = 3,
		G3Type_MUL = 4,
		G3Type_iMUL = 5, // partial implementation, iMul has additional forms in ix86.cpp
		G3Type_DIV = 6,
		G3Type_iDIV = 7
	};

	// --------------------------------------------------------------------------------------
	//  xImpl_Group3
	// --------------------------------------------------------------------------------------
	struct xImpl_Group3
	{
		G3Type InstType;

		void operator()(const xRegisterInt& from) const;
		void operator()(const xIndirect64orLess& from) const;
	};

	// --------------------------------------------------------------------------------------
	//  xImpl_iDiv
	// --------------------------------------------------------------------------------------
	struct xImpl_iDiv : public xImpl_Group3
	{
		const xImplSimd_3Arg PS;
		const xImplSimd_3Arg PD;
		const xImplSimd_3Arg SS;
		const xImplSimd_3Arg SD;
	};

	// --------------------------------------------------------------------------------------
	//  xImpl_iMul
	// --------------------------------------------------------------------------------------
	//
	struct xImpl_iMul : public xImpl_Group3
	{
		using xImpl_Group3::operator();

		// The following iMul-specific forms are valid for 16 and 32 bit register operands only!

		void operator()(const xRegister32& to, const xRegister32& from) const;
		void operator()(const xRegister32& to, const xIndirectVoid& src) const;
		void operator()(const xRegister16& to, const xRegister16& from) const;
		void operator()(const xRegister16& to, const xIndirectVoid& src) const;

		void operator()(const xRegister32& to, const xRegister32& from, s32 imm) const;
		void operator()(const xRegister32& to, const xIndirectVoid& from, s32 imm) const;
		void operator()(const xRegister16& to, const xRegister16& from, s16 imm) const;
		void operator()(const xRegister16& to, const xIndirectVoid& from, s16 imm) const;

		const xImplSimd_3Arg PS;
		const xImplSimd_3Arg PD;
		const xImplSimd_3Arg SS;
		const xImplSimd_3Arg SD;
	};
} // namespace x86Emitter
