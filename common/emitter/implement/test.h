// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

// Implementations found here: TEST + BTS/BT/BTC/BTR + BSF/BSR! (for lack of better location)

namespace x86Emitter
{

	// --------------------------------------------------------------------------------------
	//  xImpl_Test
	// --------------------------------------------------------------------------------------
	//
	struct xImpl_Test
	{
		void operator()(const xRegisterInt& to, const xRegisterInt& from) const;
		void operator()(const xIndirect64orLess& dest, int imm) const;
		void operator()(const xRegisterInt& to, int imm) const;
	};

	enum G8Type
	{
		G8Type_BT = 4,
		G8Type_BTS,
		G8Type_BTR,
		G8Type_BTC,
	};

	// --------------------------------------------------------------------------------------
	//  BSF / BSR
	// --------------------------------------------------------------------------------------
	// 16/32 operands are available.  No 8 bit ones, not that any of you cared, I bet.
	//
	struct xImpl_BitScan
	{
		// 0xbc [fwd] / 0xbd [rev]
		u16 Opcode;

		void operator()(const xRegister16or32or64& to, const xRegister16or32or64& from) const;
		void operator()(const xRegister16or32or64& to, const xIndirectVoid& sibsrc) const;
	};

	// --------------------------------------------------------------------------------------
	//  xImpl_Group8
	// --------------------------------------------------------------------------------------
	// Bit Test Instructions - Valid on 16/32 bit instructions only.
	//
	struct xImpl_Group8
	{
		G8Type InstType;

		void operator()(const xRegister16or32or64& bitbase, const xRegister16or32or64& bitoffset) const;
		void operator()(const xRegister16or32or64& bitbase, u8 bitoffset) const;

		void operator()(const xIndirectVoid& bitbase, const xRegister16or32or64& bitoffset) const;

		void operator()(const xIndirect64& bitbase, u8 bitoffset) const;
		void operator()(const xIndirect32& bitbase, u8 bitoffset) const;
		void operator()(const xIndirect16& bitbase, u8 bitoffset) const;
	};

} // End namespace x86Emitter
