// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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

	extern void _g1_EmitOp(G1Type InstType, const xRegisterInt& to, const xRegisterInt& from);

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

#if 0
	// ------------------------------------------------------------------------
	template< typename T > __noinline void operator()( const ModSibBase& to, const xImmReg<T>& immOrReg ) const
	{
		_DoI_helpermess( *this, to, immOrReg );
	}

	template< typename T > __noinline void operator()( const xDirectOrIndirect<T>& to, const xImmReg<T>& immOrReg ) const
	{
		_DoI_helpermess( *this, to, immOrReg );
	}

	template< typename T > __noinline void operator()( const xDirectOrIndirect<T>& to, int imm ) const
	{
		_DoI_helpermess( *this, to, imm );
	}

	template< typename T > __noinline void operator()( const xDirectOrIndirect<T>& to, const xDirectOrIndirect<T>& from ) const
	{
		_DoI_helpermess( *this, to, from );
	}

	// FIXME : Make this struct to 8, 16, and 32 bit registers
	template< typename T > __noinline void operator()( const xRegisterBase& to, const xDirectOrIndirect<T>& from ) const
	{
		_DoI_helpermess( *this, xDirectOrIndirect<T>( to ), from );
	}

	// FIXME : Make this struct to 8, 16, and 32 bit registers
	template< typename T > __noinline void operator()( const xDirectOrIndirect<T>& to, const xRegisterBase& from ) const
	{
		_DoI_helpermess( *this, to, xDirectOrIndirect<T>( from ) );
	}
#endif
	};

	// ------------------------------------------------------------------------
	// This class combines x86 with SSE/SSE2 logic operations (ADD, OR, and NOT).
	// Note: ANDN [AndNot] is handled below separately.
	//
	struct xImpl_G1Logic
	{
		G1Type InstType;

		void operator()(const xRegisterInt& to, const xRegisterInt& from) const;

		void operator()(const xIndirectVoid& to, const xRegisterInt& from) const;
		void operator()(const xRegisterInt& to, const xIndirectVoid& from) const;
		void operator()(const xRegisterInt& to, int imm) const;

		void operator()(const xIndirect64orLess& to, int imm) const;

		xImplSimd_DestRegSSE PS; // packed single precision
		xImplSimd_DestRegSSE PD; // packed double precision
	};

	// ------------------------------------------------------------------------
	// This class combines x86 with SSE/SSE2 arithmetic operations (ADD/SUB).
	//
	struct xImpl_G1Arith
	{
		G1Type InstType;

		void operator()(const xRegisterInt& to, const xRegisterInt& from) const;

		void operator()(const xIndirectVoid& to, const xRegisterInt& from) const;
		void operator()(const xRegisterInt& to, const xIndirectVoid& from) const;
		void operator()(const xRegisterInt& to, int imm) const;

		void operator()(const xIndirect64orLess& to, int imm) const;

		xImplSimd_DestRegSSE PS; // packed single precision
		xImplSimd_DestRegSSE PD; // packed double precision
		xImplSimd_DestRegSSE SS; // scalar single precision
		xImplSimd_DestRegSSE SD; // scalar double precision
	};

	// ------------------------------------------------------------------------
	struct xImpl_G1Compare
	{
		void operator()(const xRegisterInt& to, const xRegisterInt& from) const;

		void operator()(const xIndirectVoid& to, const xRegisterInt& from) const;
		void operator()(const xRegisterInt& to, const xIndirectVoid& from) const;
		void operator()(const xRegisterInt& to, int imm) const;

		void operator()(const xIndirect64orLess& to, int imm) const;

		xImplSimd_DestSSE_CmpImm PS;
		xImplSimd_DestSSE_CmpImm PD;
		xImplSimd_DestSSE_CmpImm SS;
		xImplSimd_DestSSE_CmpImm SD;
	};

} // End namespace x86Emitter
