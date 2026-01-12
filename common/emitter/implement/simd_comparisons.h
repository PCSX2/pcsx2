// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
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
		// Compare packed bytes for equality.
		// If a data element in dest is equal to the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplSimd_3Arg EQB;

		// Compare packed words for equality.
		// If a data element in dest is equal to the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplSimd_3Arg EQW;

		// Compare packed doublewords [32-bits] for equality.
		// If a data element in dest is equal to the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplSimd_3Arg EQD;

		// Compare packed signed bytes for greater than.
		// If a data element in dest is greater than the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplSimd_3Arg GTB;

		// Compare packed signed words for greater than.
		// If a data element in dest is greater than the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplSimd_3Arg GTW;

		// Compare packed signed doublewords [32-bits] for greater than.
		// If a data element in dest is greater than the corresponding date element src, the
		// corresponding data element in dest is set to all 1s; otherwise, it is set to all 0s.
		const xImplSimd_3Arg GTD;
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	//
	struct xImplSimd_PMinMax
	{
		// Compare packed unsigned byte integers in dest to src and store packed min/max
		// values in dest.
		const xImplSimd_3Arg UB;

		// Compare packed signed word integers in dest to src and store packed min/max
		// values in dest.
		const xImplSimd_3Arg SW;

		// [SSE-4.1] Compare packed signed byte integers in dest to src and store
		// packed min/max values in dest. (SSE operands only)
		const xImplSimd_3Arg SB;

		// [SSE-4.1] Compare packed signed doubleword integers in dest to src and store
		// packed min/max values in dest. (SSE operands only)
		const xImplSimd_3Arg SD;

		// [SSE-4.1] Compare packed unsigned word integers in dest to src and store
		// packed min/max values in dest. (SSE operands only)
		const xImplSimd_3Arg UW;

		// [SSE-4.1] Compare packed unsigned doubleword integers in dest to src and store
		// packed min/max values in dest. (SSE operands only)
		const xImplSimd_3Arg UD;
	};

} // end namespace x86Emitter
