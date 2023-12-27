// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

// Implement BMI1/BMI2 instruction set

namespace x86Emitter
{

	struct xImplBMI_RVM
	{
		u8 Prefix;
		u8 MbPrefix;
		u8 Opcode;

		// RVM
		// MULX 	Unsigned multiply without affecting flags, and arbitrary destination registers
		// PDEP 	Parallel bits deposit
		// PEXT 	Parallel bits extract
		// ANDN 	Logical and not 	~x & y
		void operator()(const xRegisterInt& to, const xRegisterInt& from1, const xRegisterInt& from2) const;
		void operator()(const xRegisterInt& to, const xRegisterInt& from1, const xIndirectVoid& from2) const;

#if 0
		// RMV
		// BEXTR 	Bit field extract (with register) 	(src >> start) & ((1 << len)-1)[9]
		// BZHI 	Zero high bits starting with specified bit position
		// SARX 	Shift arithmetic right without affecting flags
		// SHRX 	Shift logical right without affecting flags
		// SHLX 	Shift logical left without affecting flags
		// FIXME: WARNING same as above but V and M are inverted
		//void operator()( const xRegisterInt& to, const xRegisterInt& from1, const xRegisterInt& from2) const;
		//void operator()( const xRegisterInt& to, const xIndirectVoid& from1, const xRegisterInt& from2) const;

		// VM
		// BLSI 	Extract lowest set isolated bit 	x & -x
		// BLSMSK 	Get mask up to lowest set bit 	x ^ (x - 1)
		// BLSR 	Reset lowest set bit 	x & (x - 1)
		void operator()( const xRegisterInt& to, const xRegisterInt& from) const;
		void operator()( const xRegisterInt& to, const xIndirectVoid& from) const;

		// RMI
		//RORX 	Rotate right logical without affecting flags
		void operator()( const xRegisterInt& to, const xRegisterInt& from, u8 imm) const;
		void operator()( const xRegisterInt& to, const xIndirectVoid& from, u8 imm) const;
#endif
	};
} // namespace x86Emitter
