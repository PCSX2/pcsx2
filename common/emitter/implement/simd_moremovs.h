// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

namespace x86Emitter
{

	// --------------------------------------------------------------------------------------
	//  xImplSimd_MovHL
	// --------------------------------------------------------------------------------------
	// Moves to/from high/low portions of an xmm register.
	// These instructions cannot be used in reg/reg form.
	//
	struct xImplSimd_MovHL
	{
		SIMDInstructionInfo info;

		void PS(const xRegisterSSE& dst, const xIndirectVoid& src) const { PS(dst, dst, src); }
		void PS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2) const;
		void PS(const xIndirectVoid& dst, const xRegisterSSE& src) const;

		void PD(const xRegisterSSE& dst, const xIndirectVoid& src) const { PD(dst, dst, src); }
		void PD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2) const;
		void PD(const xIndirectVoid& dst, const xRegisterSSE& src) const;
	};

	// --------------------------------------------------------------------------------------
	//  xImplSimd_MovHL_RtoR
	// --------------------------------------------------------------------------------------
	// RegtoReg forms of MOVHL/MOVLH -- these are the same opcodes as MOVH/MOVL but
	// do something kinda different! Fun!
	//
	struct xImplSimd_MovHL_RtoR
	{
		SIMDInstructionInfo info;

		void PS(const xRegisterSSE& dst, const xRegisterSSE& src) const { PS(dst, dst, src); }
		void PD(const xRegisterSSE& dst, const xRegisterSSE& src) const { PD(dst, dst, src); }
		void PS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE& src2) const;
		void PD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE& src2) const;
	};

	// --------------------------------------------------------------------------------------
	//  xImplSimd_MoveSSE
	// --------------------------------------------------------------------------------------
	// Legends in their own right: MOVAPS / MOVAPD / MOVUPS / MOVUPD / MOVDQA / MOVDQU
	//
	// All implementations of Unaligned Movs will, when possible, use aligned movs instead.
	// This happens when using Mem,Reg or Reg,Mem forms where the address is simple displacement
	// which can be checked for alignment at runtime.
	//
	struct xImplSimd_MoveSSE
	{
		SIMDInstructionInfo aligned_load;
		SIMDInstructionInfo aligned_store;
		SIMDInstructionInfo unaligned_load;
		SIMDInstructionInfo unaligned_store;

		void operator()(const xRegisterSSE& to, const xRegisterSSE& from) const;
		void operator()(const xRegisterSSE& to, const xIndirectVoid& from) const;
		void operator()(const xIndirectVoid& to, const xRegisterSSE& from) const;
	};

	// --------------------------------------------------------------------------------------
	//  xImplSimd_Blend
	// --------------------------------------------------------------------------------------
	// Blend - Conditional copying of values in src into dest.
	//
	struct xImplSimd_Blend
	{
		// [SSE-4.1] Conditionally copies dword values from src to dest, depending on the
		// mask bits in the immediate operand (bits [3:0]).  Each mask bit corresponds to a
		// dword element in a 128-bit operand.
		//
		// If a mask bit is 1, then the corresponding dword in the source operand is copied
		// to dest, else the dword element in dest is left unchanged.
		//
		xImplSimd_3ArgImm PS;

		// [SSE-4.1] Conditionally copies quadword values from src to dest, depending on the
		// mask bits in the immediate operand (bits [1:0]).  Each mask bit corresponds to a
		// quadword element in a 128-bit operand.
		//
		// If a mask bit is 1, then the corresponding dword in the source operand is copied
		// to dest, else the dword element in dest is left unchanged.
		//
		xImplSimd_3ArgImm PD;

		// [SSE-4.1] Conditionally copies dword values from src to dest, depending on the
		// mask (bits [3:0]) in XMM0 (yes, the fixed register).  Each mask bit corresponds
		// to a dword element in the 128-bit operand.
		//
		// If a mask bit is 1, then the corresponding dword in the source operand is copied
		// to dest, else the dword element in dest is left unchanged.
		//
		xImplSimd_4ArgBlend VPS;

		// [SSE-4.1] Conditionally copies quadword values from src to dest, depending on the
		// mask (bits [1:0]) in XMM0 (yes, the fixed register).  Each mask bit corresponds
		// to a quadword element in the 128-bit operand.
		//
		// If a mask bit is 1, then the corresponding dword in the source operand is copied
		// to dest, else the dword element in dest is left unchanged.
		//
		xImplSimd_4ArgBlend VPD;
	};

	struct xImplSimd_PBlend
	{
		xImplSimd_3ArgImm W;
		xImplSimd_4ArgBlend VB;
	};

	// --------------------------------------------------------------------------------------
	//  xImplSimd_PMove
	// --------------------------------------------------------------------------------------
	// Packed Move with Sign or Zero extension.
	//
	struct xImplSimd_PMove
	{
		SIMDInstructionInfo info;

		// [SSE-4.1] Zero/Sign-extend the low byte values in src into word integers
		// and store them in dest.
		void BW(const xRegisterSSE& to, const xRegisterSSE& from) const;
		void BW(const xRegisterSSE& to, const xIndirectVoid& from) const;

		// [SSE-4.1] Zero/Sign-extend the low byte values in src into dword integers
		// and store them in dest.
		void BD(const xRegisterSSE& to, const xRegisterSSE& from) const;
		void BD(const xRegisterSSE& to, const xIndirectVoid& from) const;

		// [SSE-4.1] Zero/Sign-extend the low byte values in src into qword integers
		// and store them in dest.
		void BQ(const xRegisterSSE& to, const xRegisterSSE& from) const;
		void BQ(const xRegisterSSE& to, const xIndirectVoid& from) const;

		// [SSE-4.1] Zero/Sign-extend the low word values in src into dword integers
		// and store them in dest.
		void WD(const xRegisterSSE& to, const xRegisterSSE& from) const;
		void WD(const xRegisterSSE& to, const xIndirectVoid& from) const;

		// [SSE-4.1] Zero/Sign-extend the low word values in src into qword integers
		// and store them in dest.
		void WQ(const xRegisterSSE& to, const xRegisterSSE& from) const;
		void WQ(const xRegisterSSE& to, const xIndirectVoid& from) const;

		// [SSE-4.1] Zero/Sign-extend the low dword values in src into qword integers
		// and store them in dest.
		void DQ(const xRegisterSSE& to, const xRegisterSSE& from) const;
		void DQ(const xRegisterSSE& to, const xIndirectVoid& from) const;
	};
} // namespace x86Emitter
