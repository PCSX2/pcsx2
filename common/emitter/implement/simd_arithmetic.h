// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

namespace x86Emitter
{

	// --------------------------------------------------------------------------------------
	//  _SimdShiftHelper
	// --------------------------------------------------------------------------------------
	struct _SimdShiftHelper
	{
		SIMDInstructionInfo info;
		SIMDInstructionInfo infoImm;

		void operator()(const xRegisterSSE& dst, const xRegisterSSE& src)  const { (*this)(dst, dst, src); }
		void operator()(const xRegisterSSE& dst, const xIndirectVoid& src) const { (*this)(dst, dst, src); }
		void operator()(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE& src2)  const;
		void operator()(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2) const;

		void operator()(const xRegisterSSE& dst, u8 imm8) const { (*this)(dst, dst, imm8); }
		void operator()(const xRegisterSSE& dst, const xRegisterSSE& src, u8 imm8) const;
	};

	// --------------------------------------------------------------------------------------
	//  xImplSimd_Shift / xImplSimd_ShiftWithoutQ
	// --------------------------------------------------------------------------------------

	// Used for PSRA, which lacks the Q form.
	//
	struct xImplSimd_ShiftWithoutQ
	{
		const _SimdShiftHelper W;
		const _SimdShiftHelper D;
	};

	// Implements PSRL and PSLL
	//
	struct xImplSimd_Shift
	{
		const _SimdShiftHelper W;
		const _SimdShiftHelper D;
		const _SimdShiftHelper Q;

		void DQ(const xRegisterSSE& dst, u8 imm8) const { DQ(dst, dst, imm8); }
		void DQ(const xRegisterSSE& dst, const xRegisterSSE& src, u8 imm8) const;
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	//
	struct xImplSimd_AddSub
	{
		const xImplSimd_3Arg B;
		const xImplSimd_3Arg W;
		const xImplSimd_3Arg D;
		const xImplSimd_3Arg Q;

		// Add/Sub packed signed byte [8bit] integers from src into dest, and saturate the results.
		const xImplSimd_3Arg SB;

		// Add/Sub packed signed word [16bit] integers from src into dest, and saturate the results.
		const xImplSimd_3Arg SW;

		// Add/Sub packed unsigned byte [8bit] integers from src into dest, and saturate the results.
		const xImplSimd_3Arg USB;

		// Add/Sub packed unsigned word [16bit] integers from src into dest, and saturate the results.
		const xImplSimd_3Arg USW;
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	//
	struct xImplSimd_PMul
	{
		const xImplSimd_3Arg LW;
		const xImplSimd_3Arg HW;
		const xImplSimd_3Arg HUW;
		const xImplSimd_3Arg UDQ;

		// [SSE-3] PMULHRSW multiplies vertically each signed 16-bit integer from dest with the
		// corresponding signed 16-bit integer of source, producing intermediate signed 32-bit
		// integers. Each intermediate 32-bit integer is truncated to the 18 most significant
		// bits. Rounding is always performed by adding 1 to the least significant bit of the
		// 18-bit intermediate result. The final result is obtained by selecting the 16 bits
		// immediately to the right of the most significant bit of each 18-bit intermediate
		// result and packed to the destination operand.
		//
		// Both operands can be MMX or XMM registers.  Source can be register or memory.
		//
		const xImplSimd_3Arg HRSW;

		// [SSE-4.1] Multiply the packed dword signed integers in dest with src, and store
		// the low 32 bits of each product in xmm1.
		const xImplSimd_3Arg LD;

		// [SSE-4.1] Multiply the packed signed dword integers in dest with src.
		const xImplSimd_3Arg DQ;
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	// For instructions that have PS/SS form only (most commonly reciprocal Sqrt functions)
	//
	struct xImplSimd_rSqrt
	{
		const xImplSimd_2Arg PS;
		const xImplSimd_3Arg SS;
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	// SQRT has PS/SS/PD/SD forms
	//
	struct xImplSimd_Sqrt
	{
		const xImplSimd_2Arg PS;
		const xImplSimd_3Arg SS;
		const xImplSimd_2Arg PD;
		const xImplSimd_3Arg SD;
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	//
	struct xImplSimd_AndNot
	{
		const xImplSimd_3Arg PS;
		const xImplSimd_3Arg PD;
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	// Packed absolute value. [sSSE3 only]
	//
	struct xImplSimd_PAbsolute
	{
		// [sSSE-3] Computes the absolute value of bytes in the src, and stores the result
		// in dest, as UNSIGNED.
		const xImplSimd_2Arg B;

		// [sSSE-3] Computes the absolute value of word in the src, and stores the result
		// in dest, as UNSIGNED.
		const xImplSimd_2Arg W;

		// [sSSE-3] Computes the absolute value of doublewords in the src, and stores the
		// result in dest, as UNSIGNED.
		const xImplSimd_2Arg D;
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	// Packed Sign [sSSE3 only] - Negate/zero/preserve packed integers in dest depending on the
	// corresponding sign in src.
	//
	struct xImplSimd_PSign
	{
		// [sSSE-3] negates each byte element of dest if the signed integer value of the
		// corresponding data element in src is less than zero. If the signed integer value
		// of a data element in src is positive, the corresponding data element in dest is
		// unchanged. If a data element in src is zero, the corresponding data element in
		// dest is set to zero.
		const xImplSimd_3Arg B;

		// [sSSE-3] negates each word element of dest if the signed integer value of the
		// corresponding data element in src is less than zero. If the signed integer value
		// of a data element in src is positive, the corresponding data element in dest is
		// unchanged. If a data element in src is zero, the corresponding data element in
		// dest is set to zero.
		const xImplSimd_3Arg W;

		// [sSSE-3] negates each doubleword element of dest if the signed integer value
		// of the corresponding data element in src is less than zero. If the signed integer
		// value of a data element in src is positive, the corresponding data element in dest
		// is unchanged. If a data element in src is zero, the corresponding data element in
		// dest is set to zero.
		const xImplSimd_3Arg D;
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	// Packed Multiply and Add!!
	//
	struct xImplSimd_PMultAdd
	{
		// Multiplies the individual signed words of dest by the corresponding signed words
		// of src, producing temporary signed, doubleword results. The adjacent doubleword
		// results are then summed and stored in the destination operand.
		//
		//   DEST[31:0]  = ( DEST[15:0]  * SRC[15:0])  + (DEST[31:16] * SRC[31:16] );
		//   DEST[63:32] = ( DEST[47:32] * SRC[47:32]) + (DEST[63:48] * SRC[63:48] );
		//   [.. repeat in the case of XMM src/dest operands ..]
		//
		const xImplSimd_3Arg WD;

		// [sSSE-3] multiplies vertically each unsigned byte of dest with the corresponding
		// signed byte of src, producing intermediate signed 16-bit integers. Each adjacent
		// pair of signed words is added and the saturated result is packed to dest.
		// For example, the lowest-order bytes (bits 7-0) in src and dest are multiplied
		// and the intermediate signed word result is added with the corresponding
		// intermediate result from the 2nd lowest-order bytes (bits 15-8) of the operands;
		// the sign-saturated result is stored in the lowest word of dest (bits 15-0).
		// The same operation is performed on the other pairs of adjacent bytes.
		//
		// In Coder Speak:
		//   DEST[15-0]  = SaturateToSignedWord( SRC[15-8]  * DEST[15-8]  + SRC[7-0]   * DEST[7-0]   );
		//   DEST[31-16] = SaturateToSignedWord( SRC[31-24] * DEST[31-24] + SRC[23-16] * DEST[23-16] );
		//   [.. repeat for each 16 bits up to 64 (mmx) or 128 (xmm) ..]
		//
		const xImplSimd_3Arg UBSW;
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	// Packed Horizontal Add [SSE3 only]
	//
	struct xImplSimd_HorizAdd
	{
		// [SSE-3] Horizontal Add of Packed Data.  A three step process:
		// * Adds the single-precision floating-point values in the first and second dwords of
		//   dest and stores the result in the first dword of dest.
		// * Adds single-precision floating-point values in the third and fourth dword of dest
		//   stores the result in the second dword of dest.
		// * Adds single-precision floating-point values in the first and second dword of *src*
		//   and stores the result in the third dword of dest.
		const xImplSimd_3Arg PS;

		// [SSE-3] Horizontal Add of Packed Data.  A two step process:
		// * Adds the double-precision floating-point values in the high and low quadwords of
		//   dest and stores the result in the low quadword of dest.
		// * Adds the double-precision floating-point values in the high and low quadwords of
		//   *src* stores the result in the high quadword of dest.
		const xImplSimd_3Arg PD;
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	// DotProduct calculation (SSE4.1 only!)
	//
	struct xImplSimd_DotProduct
	{
		// [SSE-4.1] Conditionally multiplies the packed single precision floating-point
		// values in dest with the packed single-precision floats in src depending on a
		// mask extracted from the high 4 bits of the immediate byte. If a condition mask
		// bit in Imm8[7:4] is zero, the corresponding multiplication is replaced by a value
		// of 0.0.	The four resulting single-precision values are summed into an inter-
		// mediate result.
		//
		// The intermediate result is conditionally broadcasted to the destination using a
		// broadcast mask specified by bits [3:0] of the immediate byte. If a broadcast
		// mask bit is 1, the intermediate result is copied to the corresponding dword
		// element in dest.  If a broadcast mask bit is zero, the corresponding element in
		// the destination is set to zero.
		//
		xImplSimd_3ArgImm PS;

		// [SSE-4.1]
		xImplSimd_3ArgImm PD;
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	// Rounds floating point values (packed or single scalar) by an arbitrary rounding mode.
	// (SSE4.1 only!)
	struct xImplSimd_Round
	{
		// [SSE-4.1] Rounds the 4 packed single-precision src values and stores them in dest.
		//
		// Imm8 specifies control fields for the rounding operation:
		//   Bit  3 - processor behavior for a precision exception (0: normal, 1: inexact)
		//   Bit  2 - If enabled, use MXCSR.RC, else use RC specified in bits 1:0 of this Imm8.
		//   Bits 1:0 - Specifies a rounding mode for this instruction only.
		//
		// Rounding Mode Reference:
		//   0 - Nearest, 1 - Negative Infinity, 2 - Positive infinity, 3 - Truncate.
		//
		const xImplSimd_2ArgImm PS;

		// [SSE-4.1] Rounds the 2 packed double-precision src values and stores them in dest.
		//
		// Imm8 specifies control fields for the rounding operation:
		//   Bit  3 - processor behavior for a precision exception (0: normal, 1: inexact)
		//   Bit  2 - If enabled, use MXCSR.RC, else use RC specified in bits 1:0 of this Imm8.
		//   Bits 1:0 - Specifies a rounding mode for this instruction only.
		//
		// Rounding Mode Reference:
		//   0 - Nearest, 1 - Negative Infinity, 2 - Positive infinity, 3 - Truncate.
		//
		const xImplSimd_2ArgImm PD;

		// [SSE-4.1] Rounds the single-precision src value and stores in dest.
		//
		// Imm8 specifies control fields for the rounding operation:
		//   Bit  3 - processor behavior for a precision exception (0: normal, 1: inexact)
		//   Bit  2 - If enabled, use MXCSR.RC, else use RC specified in bits 1:0 of this Imm8.
		//   Bits 1:0 - Specifies a rounding mode for this instruction only.
		//
		// Rounding Mode Reference:
		//   0 - Nearest, 1 - Negative Infinity, 2 - Positive infinity, 3 - Truncate.
		//
		const xImplSimd_3ArgImm SS;

		// [SSE-4.1] Rounds the double-precision src value and stores in dest.
		//
		// Imm8 specifies control fields for the rounding operation:
		//   Bit  3 - processor behavior for a precision exception (0: normal, 1: inexact)
		//   Bit  2 - If enabled, use MXCSR.RC, else use RC specified in bits 1:0 of this Imm8.
		//   Bits 1:0 - Specifies a rounding mode for this instruction only.
		//
		// Rounding Mode Reference:
		//   0 - Nearest, 1 - Negative Infinity, 2 - Positive infinity, 3 - Truncate.
		//
		const xImplSimd_3ArgImm SD;
	};

} // End namespace x86Emitter
