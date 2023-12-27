// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

//////////////////////////////////////////////////////////////////////////////////////////
// MMX / SSE Helper Functions!

// ------------------------------------------------------------------------
// For implementing SSE-only logic operations that have xmmreg,xmmreg/rm forms only,
// like ANDPS/ANDPD
//
template <u8 Prefix, u16 Opcode>
class SimdImpl_DestRegSSE
{
public:
	__forceinline void operator()(const xRegisterSSE& to, const xRegisterSSE& from) const { xOpWrite0F(Prefix, Opcode, to, from); }
	__forceinline void operator()(const xRegisterSSE& to, const ModSibBase& from) const
	{
		bool isReallyAligned = ((from.Displacement & 0x0f) == 0) && from.Index.IsEmpty() && from.Base.IsEmpty();
		pxAssertMsg(isReallyAligned, "Alignment check failed on SSE indirect load.");
		xOpWrite0F(Prefix, Opcode, to, from);
	}

	SimdImpl_DestRegSSE() {} //GCWho?
};
