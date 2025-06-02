// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

namespace x86Emitter
{

	struct xImplSimd_MinMax
	{
		const xImplSimd_3Arg PS; // packed single precision
		const xImplSimd_3Arg PD; // packed double precision
		const xImplSimd_3Arg SS; // scalar single precision
		const xImplSimd_3Arg SD; // scalar double precision
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	//
	struct xImplSimd_Compare
	{
		SSE2_ComparisonType CType;

		void PS(const xRegisterSSE& dst, const xRegisterSSE&  src) const { PS(dst, dst, src); }
		void PS(const xRegisterSSE& dst, const xIndirectVoid& src) const { PS(dst, dst, src); }
		void PS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE&  src2) const;
		void PS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2) const;

		void PD(const xRegisterSSE& dst, const xRegisterSSE&  src) const { PD(dst, dst, src); }
		void PD(const xRegisterSSE& dst, const xIndirectVoid& src) const { PD(dst, dst, src); }
		void PD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE&  src2) const;
		void PD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2) const;

		void SS(const xRegisterSSE& dst, const xRegisterSSE&  src) const { SS(dst, dst, src); }
		void SS(const xRegisterSSE& dst, const xIndirectVoid& src) const { SS(dst, dst, src); }
		void SS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE&  src2) const;
		void SS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2) const;

		void SD(const xRegisterSSE& dst, const xRegisterSSE&  src) const { SD(dst, dst, src); }
		void SD(const xRegisterSSE& dst, const xIndirectVoid& src) const { SD(dst, dst, src); }
		void SD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE&  src2) const;
		void SD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2) const;
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	// Compare scalar floating point values and set EFLAGS (Ordered or Unordered)
	//
	struct xImplSimd_COMI
	{
		const xImplSimd_2Arg SS;
		const xImplSimd_2Arg SD;
	};


	//////////////////////////////////////////////////////////////////////////////////////////
	//
	struct xImplSimd_PCompare
	{
	public:
		// Compare packed bytes for equality.
		// If a data element in dest is equal to the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplSimd_DestRegEither EQB;

		// Compare packed words for equality.
		// If a data element in dest is equal to the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplSimd_DestRegEither EQW;

		// Compare packed doublewords [32-bits] for equality.
		// If a data element in dest is equal to the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplSimd_DestRegEither EQD;

		// Compare packed signed bytes for greater than.
		// If a data element in dest is greater than the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplSimd_DestRegEither GTB;

		// Compare packed signed words for greater than.
		// If a data element in dest is greater than the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplSimd_DestRegEither GTW;

		// Compare packed signed doublewords [32-bits] for greater than.
		// If a data element in dest is greater than the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplSimd_DestRegEither GTD;
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	//
	struct xImplSimd_PMinMax
	{
		// Compare packed unsigned byte integers in dest to src and store packed min/max
		// values in dest.
		const xImplSimd_DestRegEither UB;

		// Compare packed signed word integers in dest to src and store packed min/max
		// values in dest.
		const xImplSimd_DestRegEither SW;

		// [SSE-4.1] Compare packed signed byte integers in dest to src and store
		// packed min/max values in dest. (SSE operands only)
		const xImplSimd_DestRegSSE SB;

		// [SSE-4.1] Compare packed signed doubleword integers in dest to src and store
		// packed min/max values in dest. (SSE operands only)
		const xImplSimd_DestRegSSE SD;

		// [SSE-4.1] Compare packed unsigned word integers in dest to src and store
		// packed min/max values in dest. (SSE operands only)
		const xImplSimd_DestRegSSE UW;

		// [SSE-4.1] Compare packed unsigned doubleword integers in dest to src and store
		// packed min/max values in dest. (SSE operands only)
		const xImplSimd_DestRegSSE UD;
	};

} // end namespace x86Emitter
