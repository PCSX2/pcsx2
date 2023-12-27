// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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

#if 0
	template< typename T >
	void operator()( const xDirectOrIndirect<T>& from ) const
	{
		_DoI_helpermess( *this, from );
	}
#endif
	};

	// --------------------------------------------------------------------------------------
	//  xImpl_MulDivBase
	// --------------------------------------------------------------------------------------
	// This class combines x86 and SSE/SSE2 instructions for iMUL and iDIV.
	//
	struct xImpl_MulDivBase
	{
		G3Type InstType;
		u16 OpcodeSSE;

		void operator()(const xRegisterInt& from) const;
		void operator()(const xIndirect64orLess& from) const;

		const xImplSimd_DestRegSSE PS;
		const xImplSimd_DestRegSSE PD;
		const xImplSimd_DestRegSSE SS;
		const xImplSimd_DestRegSSE SD;
	};

	// --------------------------------------------------------------------------------------
	//  xImpl_iDiv
	// --------------------------------------------------------------------------------------
	struct xImpl_iDiv
	{
		void operator()(const xRegisterInt& from) const;
		void operator()(const xIndirect64orLess& from) const;

		const xImplSimd_DestRegSSE PS;
		const xImplSimd_DestRegSSE PD;
		const xImplSimd_DestRegSSE SS;
		const xImplSimd_DestRegSSE SD;
	};

	// --------------------------------------------------------------------------------------
	//  xImpl_iMul
	// --------------------------------------------------------------------------------------
	//
	struct xImpl_iMul
	{
		void operator()(const xRegisterInt& from) const;
		void operator()(const xIndirect64orLess& from) const;

		// The following iMul-specific forms are valid for 16 and 32 bit register operands only!

		void operator()(const xRegister32& to, const xRegister32& from) const;
		void operator()(const xRegister32& to, const xIndirectVoid& src) const;
		void operator()(const xRegister16& to, const xRegister16& from) const;
		void operator()(const xRegister16& to, const xIndirectVoid& src) const;

		void operator()(const xRegister32& to, const xRegister32& from, s32 imm) const;
		void operator()(const xRegister32& to, const xIndirectVoid& from, s32 imm) const;
		void operator()(const xRegister16& to, const xRegister16& from, s16 imm) const;
		void operator()(const xRegister16& to, const xIndirectVoid& from, s16 imm) const;

		const xImplSimd_DestRegSSE PS;
		const xImplSimd_DestRegSSE PD;
		const xImplSimd_DestRegSSE SS;
		const xImplSimd_DestRegSSE SD;
	};
} // namespace x86Emitter
