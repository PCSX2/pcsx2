// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

namespace x86Emitter
{

	// =====================================================================================================
	//  xImpl_SIMD Types (template free!)
	// =====================================================================================================

	// ------------------------------------------------------------------------
	// For implementing SSE-only logic operations that have xmmreg,xmmreg/rm forms only,
	// like ANDPS/ANDPD
	//
	struct xImplSimd_DestRegSSE
	{
		u8 Prefix;
		u16 Opcode;

		void operator()(const xRegisterSSE& to, const xRegisterSSE& from) const;
		void operator()(const xRegisterSSE& to, const xIndirectVoid& from) const;
	};

	// ------------------------------------------------------------------------
	// For implementing SSE-only logic operations that have xmmreg,reg/rm,imm forms only
	// (PSHUFD / PSHUFHW / etc).
	//
	struct xImplSimd_DestRegImmSSE
	{
		u8 Prefix;
		u16 Opcode;

		void operator()(const xRegisterSSE& to, const xRegisterSSE& from, u8 imm) const;
		void operator()(const xRegisterSSE& to, const xIndirectVoid& from, u8 imm) const;
	};

	struct xImplSimd_DestSSE_CmpImm
	{
		u8 Prefix;
		u16 Opcode;

		void operator()(const xRegisterSSE& to, const xRegisterSSE& from, SSE2_ComparisonType imm) const;
		void operator()(const xRegisterSSE& to, const xIndirectVoid& from, SSE2_ComparisonType imm) const;
	};

	// ------------------------------------------------------------------------
	// For implementing SSE operations that have reg,reg/rm forms only,
	// but accept either MM or XMM destinations (most PADD/PSUB and other P arithmetic ops).
	//
	struct xImplSimd_DestRegEither
	{
		u8 Prefix;
		u16 Opcode;

		void operator()(const xRegisterSSE& to, const xRegisterSSE& from) const;
		void operator()(const xRegisterSSE& to, const xIndirectVoid& from) const;
	};

} // end namespace x86Emitter
