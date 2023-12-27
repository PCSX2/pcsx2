// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

namespace x86Emitter
{

	// Implementations here cover SHLD and SHRD.

	// --------------------------------------------------------------------------------------
	//  xImpl_DowrdShift
	// --------------------------------------------------------------------------------------
	// I use explicit method declarations here instead of templates, in order to provide
	// *only* 32 and 16 bit register operand forms (8 bit registers are not valid in SHLD/SHRD).
	//
	// Optimization Note: Imm shifts by 0 are ignore (no code generated).  This is a safe optimization
	// because shifts by 0 do *not* affect flags status (intel docs cited).
	//
	struct xImpl_DwordShift
	{
		u16 OpcodeBase;

		void operator()(const xRegister16or32or64& to, const xRegister16or32or64& from, const xRegisterCL& clreg) const;

		void operator()(const xRegister16or32or64& to, const xRegister16or32or64& from, u8 shiftcnt) const;

		void operator()(const xIndirectVoid& dest, const xRegister16or32or64& from, const xRegisterCL& clreg) const;
		void operator()(const xIndirectVoid& dest, const xRegister16or32or64& from, u8 shiftcnt) const;
	};

} // End namespace x86Emitter
