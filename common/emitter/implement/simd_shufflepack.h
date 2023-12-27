// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

namespace x86Emitter
{

	// --------------------------------------------------------------------------------------
	//  xImplSimd_Shuffle
	// --------------------------------------------------------------------------------------
	struct xImplSimd_Shuffle
	{
		inline void _selector_assertion_check(u8 selector) const;

		void PS(const xRegisterSSE& to, const xRegisterSSE& from, u8 selector) const;
		void PS(const xRegisterSSE& to, const xIndirectVoid& from, u8 selector) const;

		void PD(const xRegisterSSE& to, const xRegisterSSE& from, u8 selector) const;
		void PD(const xRegisterSSE& to, const xIndirectVoid& from, u8 selector) const;
	};

	// --------------------------------------------------------------------------------------
	//  xImplSimd_PShuffle
	// --------------------------------------------------------------------------------------
	struct xImplSimd_PShuffle
	{
		// Copies doublewords from src and inserts them into dest at dword locations selected
		// with the order operand (8 bit immediate).
		const xImplSimd_DestRegImmSSE D;

		// Copies words from the low quadword of src and inserts them into the low quadword
		// of dest at word locations selected with the order operand (8 bit immediate).
		// The high quadword of src is copied to the high quadword of dest.
		const xImplSimd_DestRegImmSSE LW;

		// Copies words from the high quadword of src and inserts them into the high quadword
		// of dest at word locations selected with the order operand (8 bit immediate).
		// The low quadword of src is copied to the low quadword of dest.
		const xImplSimd_DestRegImmSSE HW;

		// [sSSE-3] Performs in-place shuffles of bytes in dest according to the shuffle
		// control mask in src.  If the most significant bit (bit[7]) of each byte of the
		// shuffle control mask is set, then constant zero is written in the result byte.
		// Each byte in the shuffle control mask forms an index to permute the corresponding
		// byte in dest. The value of each index is the least significant 4 bits (128-bit
		// operation) or 3 bits (64-bit operation) of the shuffle control byte.
		//
		const xImplSimd_DestRegEither B;

		// below is my test bed for a new system, free of subclasses.  Was supposed to improve intellisense
		// but it doesn't (makes it worse).  Will try again in MSVC 2010. --air

#if 0
	// Copies words from src and inserts them into dest at word locations selected with
	// the order operand (8 bit immediate).

	// Copies doublewords from src and inserts them into dest at dword locations selected
	// with the order operand (8 bit immediate).
	void D( const xRegisterSSE& to, const xRegisterSSE& from, u8 imm ) const	{ xOpWrite0F( 0x66, 0x70, to, from, imm ); }
	void D( const xRegisterSSE& to, const xIndirectVoid& from, u8 imm ) const		{ xOpWrite0F( 0x66, 0x70, to, from, imm ); }

	// Copies words from the low quadword of src and inserts them into the low quadword
	// of dest at word locations selected with the order operand (8 bit immediate).
	// The high quadword of src is copied to the high quadword of dest.
	void LW( const xRegisterSSE& to, const xRegisterSSE& from, u8 imm ) const	{ xOpWrite0F( 0xf2, 0x70, to, from, imm ); }
	void LW( const xRegisterSSE& to, const xIndirectVoid& from, u8 imm ) const		{ xOpWrite0F( 0xf2, 0x70, to, from, imm ); }

	// Copies words from the high quadword of src and inserts them into the high quadword
	// of dest at word locations selected with the order operand (8 bit immediate).
	// The low quadword of src is copied to the low quadword of dest.
	void HW( const xRegisterSSE& to, const xRegisterSSE& from, u8 imm ) const	{ xOpWrite0F( 0xf3, 0x70, to, from, imm ); }
	void HW( const xRegisterSSE& to, const xIndirectVoid& from, u8 imm ) const		{ xOpWrite0F( 0xf3, 0x70, to, from, imm ); }

	// [sSSE-3] Performs in-place shuffles of bytes in dest according to the shuffle
	// control mask in src.  If the most significant bit (bit[7]) of each byte of the
	// shuffle control mask is set, then constant zero is written in the result byte.
	// Each byte in the shuffle control mask forms an index to permute the corresponding
	// byte in dest. The value of each index is the least significant 4 bits (128-bit
	// operation) or 3 bits (64-bit operation) of the shuffle control byte.
	//
	void B( const xRegisterSSE& to, const xRegisterSSE& from ) const	{ OpWriteSSE( 0x66, 0x0038 ); }
	void B( const xRegisterSSE& to, const xIndirectVoid& from ) const		{ OpWriteSSE( 0x66, 0x0038 ); }
#endif
	};

	// --------------------------------------------------------------------------------------
	//  SimdImpl_PUnpack
	// --------------------------------------------------------------------------------------
	struct SimdImpl_PUnpack
	{
		// Unpack and interleave low-order bytes from src and dest into dest.
		const xImplSimd_DestRegEither LBW;
		// Unpack and interleave low-order words from src and dest into dest.
		const xImplSimd_DestRegEither LWD;
		// Unpack and interleave low-order doublewords from src and dest into dest.
		const xImplSimd_DestRegEither LDQ;
		// Unpack and interleave low-order quadwords from src and dest into dest.
		const xImplSimd_DestRegSSE LQDQ;

		// Unpack and interleave high-order bytes from src and dest into dest.
		const xImplSimd_DestRegEither HBW;
		// Unpack and interleave high-order words from src and dest into dest.
		const xImplSimd_DestRegEither HWD;
		// Unpack and interleave high-order doublewords from src and dest into dest.
		const xImplSimd_DestRegEither HDQ;
		// Unpack and interleave high-order quadwords from src and dest into dest.
		const xImplSimd_DestRegSSE HQDQ;
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
		const xImplSimd_DestRegEither SSWB;

		// Converts packed signed dword integers from src and dest into packed signed
		// word integers in dest, using signed saturation.
		const xImplSimd_DestRegEither SSDW;

		// Converts packed unsigned word integers from src and dest into packed unsigned
		// byte integers in dest, using unsigned saturation.
		const xImplSimd_DestRegEither USWB;

		// [SSE-4.1] Converts packed unsigned dword integers from src and dest into packed
		// unsigned word integers in dest, using signed saturation.
		const xImplSimd_DestRegSSE USDW;
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
		const xImplSimd_DestRegSSE HPS;

		// Unpacks the high quadword [double-precision] values from src and dest into
		// dest, such that the result of dest looks like this:
		//    dest.lo <- dest.hi
		//    dest.hi <- src.hi
		//
		const xImplSimd_DestRegSSE HPD;

		// Unpacks the low doubleword [single-precision] values from src and dest into
		// dest, such that the result of dest looks like this:
		//    dest[3] <- src[1]
		//    dest[2] <- dest[1]
		//    dest[1] <- src[0]
		//    dest[0] <- dest[0]
		//
		const xImplSimd_DestRegSSE LPS;

		// Unpacks the low quadword [double-precision] values from src and dest into
		// dest, effectively moving the low portion of src into the upper portion of dest.
		// The result of dest is loaded as such:
		//    dest.hi <- src.lo
		//    dest.lo <- dest.lo  [remains unchanged!]
		//
		const xImplSimd_DestRegSSE LPD;
	};


	// --------------------------------------------------------------------------------------
	//  SimdImpl_PInsert
	// --------------------------------------------------------------------------------------
	// PINSRW/B/D [all but Word form are SSE4.1 only!]
	//
	struct xImplSimd_PInsert
	{
		void B(const xRegisterSSE& to, const xRegister32& from, u8 imm8) const;
		void B(const xRegisterSSE& to, const xIndirect32& from, u8 imm8) const;

		void W(const xRegisterSSE& to, const xRegister32& from, u8 imm8) const;
		void W(const xRegisterSSE& to, const xIndirect32& from, u8 imm8) const;

		void D(const xRegisterSSE& to, const xRegister32& from, u8 imm8) const;
		void D(const xRegisterSSE& to, const xIndirect32& from, u8 imm8) const;

		void Q(const xRegisterSSE& to, const xRegister64& from, u8 imm8) const;
		void Q(const xRegisterSSE& to, const xIndirect64& from, u8 imm8) const;
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
		void B(const xRegister32& to, const xRegisterSSE& from, u8 imm8) const;
		void B(const xIndirect32& dest, const xRegisterSSE& from, u8 imm8) const;

		// Copies the word element specified by imm8 from src to dest.  The upper bits
		// of dest are zero-extended (cleared).  This can be used to extract any single packed
		// word value from src into an x86 32 bit register.
		//
		// [SSE-4.1] Note: Indirect memory forms of this instruction are an SSE-4.1 extension!
		//
		void W(const xRegister32& to, const xRegisterSSE& from, u8 imm8) const;
		void W(const xIndirect32& dest, const xRegisterSSE& from, u8 imm8) const;

		// [SSE-4.1] Copies the dword element specified by imm8 from src to dest.  This can be
		// used to extract any single packed dword value from src into an x86 32 bit register.
		void D(const xRegister32& to, const xRegisterSSE& from, u8 imm8) const;
		void D(const xIndirect32& dest, const xRegisterSSE& from, u8 imm8) const;

		// Insert a qword integer value from r/m64 into the xmm1 at the destination element specified by imm8.
		void Q(const xRegister64& to, const xRegisterSSE& from, u8 imm8) const;
		void Q(const xIndirect64& dest, const xRegisterSSE& from, u8 imm8) const;
	};
} // namespace x86Emitter
