/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

namespace x86Emitter
{
	struct xImplAVX_Move
	{
		u8 Prefix;
		u8 LoadOpcode;
		u8 StoreOpcode;

		void operator()(const xRegisterSSE& to, const xRegisterSSE& from) const;
		void operator()(const xRegisterSSE& to, const xIndirectVoid& from) const;
		void operator()(const xIndirectVoid& to, const xRegisterSSE& from) const;
	};

	struct xImplAVX_ThreeArg
	{
		u8 Prefix;
		u8 Opcode;

		void operator()(const xRegisterSSE& to, const xRegisterSSE& from1, const xRegisterSSE& from2) const;
		void operator()(const xRegisterSSE& to, const xRegisterSSE& from1, const xIndirectVoid& from2) const;
	};

	struct xImplAVX_ThreeArgYMM : xImplAVX_ThreeArg
	{
		void operator()(const xRegisterSSE& to, const xRegisterSSE& from1, const xRegisterSSE& from2) const;
		void operator()(const xRegisterSSE& to, const xRegisterSSE& from1, const xIndirectVoid& from2) const;
	};

	struct xImplAVX_ArithFloat
	{
		xImplAVX_ThreeArgYMM PS;
		xImplAVX_ThreeArgYMM PD;
		xImplAVX_ThreeArg SS;
		xImplAVX_ThreeArg SD;
	};

	struct xImplAVX_CmpFloatHelper
	{
		SSE2_ComparisonType CType;

		void PS(const xRegisterSSE& to, const xRegisterSSE& from1, const xRegisterSSE& from2) const;
		void PS(const xRegisterSSE& to, const xRegisterSSE& from1, const xIndirectVoid& from2) const;
		void PD(const xRegisterSSE& to, const xRegisterSSE& from1, const xRegisterSSE& from2) const;
		void PD(const xRegisterSSE& to, const xRegisterSSE& from1, const xIndirectVoid& from2) const;

		void SS(const xRegisterSSE& to, const xRegisterSSE& from1, const xRegisterSSE& from2) const;
		void SS(const xRegisterSSE& to, const xRegisterSSE& from1, const xIndirectVoid& from2) const;
		void SD(const xRegisterSSE& to, const xRegisterSSE& from1, const xRegisterSSE& from2) const;
		void SD(const xRegisterSSE& to, const xRegisterSSE& from1, const xIndirectVoid& from2) const;
	};

	struct xImplAVX_CmpFloat
	{
		xImplAVX_CmpFloatHelper EQ;
		xImplAVX_CmpFloatHelper LT;
		xImplAVX_CmpFloatHelper LE;
		xImplAVX_CmpFloatHelper UO;
		xImplAVX_CmpFloatHelper NE;
		xImplAVX_CmpFloatHelper GE;
		xImplAVX_CmpFloatHelper GT;
		xImplAVX_CmpFloatHelper OR;
	};

	struct xImplAVX_CmpInt
	{
		// Compare packed bytes for equality.
		// If a data element in dest is equal to the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplAVX_ThreeArgYMM EQB;

		// Compare packed words for equality.
		// If a data element in dest is equal to the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplAVX_ThreeArgYMM EQW;

		// Compare packed doublewords [32-bits] for equality.
		// If a data element in dest is equal to the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplAVX_ThreeArgYMM EQD;

		// Compare packed signed bytes for greater than.
		// If a data element in dest is greater than the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplAVX_ThreeArgYMM GTB;

		// Compare packed signed words for greater than.
		// If a data element in dest is greater than the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplAVX_ThreeArgYMM GTW;

		// Compare packed signed doublewords [32-bits] for greater than.
		// If a data element in dest is greater than the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplAVX_ThreeArgYMM GTD;
	};
} // namespace x86Emitter
