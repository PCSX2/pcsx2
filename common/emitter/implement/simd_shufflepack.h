// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

namespace x86Emitter
{

	// --------------------------------------------------------------------------------------
	//  xImplSimd_Shuffle
	// --------------------------------------------------------------------------------------
	struct xImplSimd_Shuffle
	{
		inline void _selector_assertion_check(u8 selector) const;

		void PS(const xRegisterSSE& dst, const xRegisterSSE&  src, u8 selector) const { PS(dst, dst, src, selector); }
		void PS(const xRegisterSSE& dst, const xIndirectVoid& src, u8 selector) const { PS(dst, dst, src, selector); }
		void PS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE&  src2, u8 selector) const;
		void PS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2, u8 selector) const;

		void PD(const xRegisterSSE& dst, const xRegisterSSE&  src, u8 selector) const { PD(dst, dst, src, selector); }
		void PD(const xRegisterSSE& dst, const xIndirectVoid& src, u8 selector) const { PD(dst, dst, src, selector); }
		void PD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE&  src2, u8 selector) const;
		void PD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2, u8 selector) const;
	};

	// --------------------------------------------------------------------------------------
	//  xImplSimd_PShuffle
	// --------------------------------------------------------------------------------------
	struct xImplSimd_PShuffle
	{
		// Copies doublewords from src and inserts them into dest at dword locations selected
		// with the order operand (8 bit immediate).
		const xImplSimd_2ArgImm D;

		// Copies words from the low quadword of src and inserts them into the low quadword
		// of dest at word locations selected with the order operand (8 bit immediate).
		// The high quadword of src is copied to the high quadword of dest.
		const xImplSimd_2ArgImm LW;

		// Copies words from the high quadword of src and inserts them into the high quadword
		// of dest at word locations selected with the order operand (8 bit immediate).
		// The low quadword of src is copied to the low quadword of dest.
		const xImplSimd_2ArgImm HW;

		// [sSSE-3] Performs in-place shuffles of bytes in dest according to the shuffle
		// control mask in src.  If the most significant bit (bit[7]) of each byte of the
		// shuffle control mask is set, then constant zero is written in the result byte.
		// Each byte in the shuffle control mask forms an index to permute the corresponding
		// byte in dest. The value of each index is the least significant 4 bits (128-bit
		// operation) or 3 bits (64-bit operation) of the shuffle control byte.
		//
		const xImplSimd_3Arg B;
	};

	// --------------------------------------------------------------------------------------
	//  SimdImpl_PUnpack
	// --------------------------------------------------------------------------------------
	struct SimdImpl_PUnpack
	{
		// Unpack and interleave low-order bytes from src and dest into dest.
		const xImplSimd_3Arg LBW;
		// Unpack and interleave low-order words from src and dest into dest.
		const xImplSimd_3Arg LWD;
		// Unpack and interleave low-order doublewords from src and dest into dest.
		const xImplSimd_3Arg LDQ;
		// Unpack and interleave low-order quadwords from src and dest into dest.
		const xImplSimd_3Arg LQDQ;

		// Unpack and interleave high-order bytes from src and dest into dest.
		const xImplSimd_3Arg HBW;
		// Unpack and interleave high-order words from src and dest into dest.
		const xImplSimd_3Arg HWD;
		// Unpack and interleave high-order doublewords from src and dest into dest.
		const xImplSimd_3Arg HDQ;
		// Unpack and interleave high-order quadwords from src and dest into dest.
		const xImplSimd_3Arg HQDQ;
	};

	// --------------------------------------------------------------------------------------
	//  SimdImpl_Pack
	// --------------------------------------------------------------------------------------
	// Pack with Signed or Unsigned Saturation
	//
	struct SimdImpl_Pack
	{
		// Converts packed signed word integers from src and dest into packed signed
		// byte integers in dest, using signed saturation.
		const xImplSimd_3Arg SSWB;

		// Converts packed signed dword integers from src and dest into packed signed
		// word integers in dest, using signed saturation.
		const xImplSimd_3Arg SSDW;

		// Converts packed unsigned word integers from src and dest into packed unsigned
		// byte integers in dest, using unsigned saturation.
		const xImplSimd_3Arg USWB;

		// [SSE-4.1] Converts packed unsigned dword integers from src and dest into packed
		// unsigned word integers in dest, using signed saturation.
		const xImplSimd_3Arg USDW;
	};

	// --------------------------------------------------------------------------------------
	//  SimdImpl_Unpack
	// --------------------------------------------------------------------------------------
	struct xImplSimd_Unpack
	{
		// Unpacks the high doubleword [single-precision] values from src and dest into
		// dest, such that the result of dest looks like this:
		//    dest[0] <- dest[2]
		//    dest[1] <- src[2]
		//    dest[2] <- dest[3]
		//    dest[3] <- src[3]
		//
		const xImplSimd_3Arg HPS;

		// Unpacks the high quadword [double-precision] values from src and dest into
		// dest, such that the result of dest looks like this:
		//    dest.lo <- dest.hi
		//    dest.hi <- src.hi
		//
		const xImplSimd_3Arg HPD;

		// Unpacks the low doubleword [single-precision] values from src and dest into
		// dest, such that the result of dest looks like this:
		//    dest[3] <- src[1]
		//    dest[2] <- dest[1]
		//    dest[1] <- src[0]
		//    dest[0] <- dest[0]
		//
		const xImplSimd_3Arg LPS;

		// Unpacks the low quadword [double-precision] values from src and dest into
		// dest, effectively moving the low portion of src into the upper portion of dest.
		// The result of dest is loaded as such:
		//    dest.hi <- src.lo
		//    dest.lo <- dest.lo  [remains unchanged!]
		//
		const xImplSimd_3Arg LPD;
	};


	// --------------------------------------------------------------------------------------
	//  SimdImpl_PInsert
	// --------------------------------------------------------------------------------------
	// PINSRW/B/D [all but Word form are SSE4.1 only!]
	//
	struct xImplSimd_PInsert
	{
		void B(const xRegisterSSE& dst, const xRegister32& src, u8 imm8) const { B(dst, dst, src, imm8); }
		void B(const xRegisterSSE& dst, const xIndirect8&  src, u8 imm8) const { B(dst, dst, src, imm8); }
		void B(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegister32& src2, u8 imm8) const;
		void B(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirect8&  src2, u8 imm8) const;

		void W(const xRegisterSSE& dst, const xRegister32& src, u8 imm8) const { W(dst, dst, src, imm8); }
		void W(const xRegisterSSE& dst, const xIndirect16& src, u8 imm8) const { W(dst, dst, src, imm8); }
		void W(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegister32& src2, u8 imm8) const;
		void W(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirect16& src2, u8 imm8) const;

		void D(const xRegisterSSE& dst, const xRegister32& src, u8 imm8) const { D(dst, dst, src, imm8); }
		void D(const xRegisterSSE& dst, const xIndirect32& src, u8 imm8) const { D(dst, dst, src, imm8); }
		void D(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegister32& src2, u8 imm8) const;
		void D(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirect32& src2, u8 imm8) const;

		void Q(const xRegisterSSE& dst, const xRegister64& src, u8 imm8) const { Q(dst, dst, src, imm8); }
		void Q(const xRegisterSSE& dst, const xIndirect64& src, u8 imm8) const { Q(dst, dst, src, imm8); }
		void Q(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegister64& src2, u8 imm8) const;
		void Q(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirect64& src2, u8 imm8) const;
	};

	//////////////////////////////////////////////////////////////////////////////////////////
	// PEXTRW/B/D [all but Word form are SSE4.1 only!]
	//
	// Note: Word form's indirect memory form is only available in SSE4.1.
	//
	struct SimdImpl_PExtract
	{
		// [SSE-4.1] Copies the byte element specified by imm8 from src to dest.  The upper bits
		// of dest are zero-extended (cleared).  This can be used to extract any single packed
		// byte value from src into an x86 32 bit register.
		void B(const xRegister32& dst, const xRegisterSSE& src, u8 imm8) const;
		void B(const xIndirect8&  dst, const xRegisterSSE& src, u8 imm8) const;

		// Copies the word element specified by imm8 from src to dest.  The upper bits
		// of dest are zero-extended (cleared).  This can be used to extract any single packed
		// word value from src into an x86 32 bit register.
		//
		// [SSE-4.1] Note: Indirect memory forms of this instruction are an SSE-4.1 extension!
		//
		void W(const xRegister32& dst, const xRegisterSSE& src, u8 imm8) const;
		void W(const xIndirect16& dst, const xRegisterSSE& src, u8 imm8) const;

		// [SSE-4.1] Copies the dword element specified by imm8 from src to dest.  This can be
		// used to extract any single packed dword value from src into an x86 32 bit register.
		void D(const xRegister32& dst, const xRegisterSSE& src, u8 imm8) const;
		void D(const xIndirect32& dst, const xRegisterSSE& src, u8 imm8) const;

		// [SSE-4.1] Copies the dword element specified by imm8 from src to dest.  This can be
		// used to extract any single packed dword value from src into an x86 64 bit register.
		void Q(const xRegister64& dst, const xRegisterSSE& src, u8 imm8) const;
		void Q(const xIndirect64& dst, const xRegisterSSE& src, u8 imm8) const;
	};
} // namespace x86Emitter
