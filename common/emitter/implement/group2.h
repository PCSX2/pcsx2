// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

namespace x86Emitter
{

	enum G2Type
	{
		G2Type_ROL = 0,
		G2Type_ROR,
		G2Type_RCL,
		G2Type_RCR,
		G2Type_SHL,
		G2Type_SHR,
		G2Type_Unused,
		G2Type_SAR
	};

	// --------------------------------------------------------------------------------------
	//  xImpl_Group2
	// --------------------------------------------------------------------------------------
	// Group 2 (shift) instructions have no Sib/ModRM forms.
	// Optimization Note: For Imm forms, we ignore the instruction if the shift count is zero.
	// This is a safe optimization since any zero-value shift does not affect any flags.
	//
	struct xImpl_Group2
	{
		G2Type InstType;

		void operator()(const xRegisterInt& to, const xRegisterCL& from) const;
		void operator()(const xIndirect64orLess& to, const xRegisterCL& from) const;
		void operator()(const xRegisterInt& to, u8 imm) const;
		void operator()(const xIndirect64orLess& to, u8 imm) const;

#if 0
	// ------------------------------------------------------------------------
	template< typename T > __noinline void operator()( const xDirectOrIndirect<T>& to, u8 imm ) const
	{
		_DoI_helpermess( *this, to, imm );
	}

	template< typename T > __noinline void operator()( const xDirectOrIndirect<T>& to, const xRegisterCL& from ) const
	{
		_DoI_helpermess( *this, to, from );
	}
#endif
	};

} // End namespace x86Emitter
