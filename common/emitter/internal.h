// SPDX-FileCopyrightText: 2002-2025 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#pragma once

#include "common/emitter/x86types.h"
#include "common/emitter/instructions.h"

namespace x86Emitter
{

#define OpWriteSSE(pre, op) xOpWrite0F(pre, op, to, from)
#define OpWriteSIMDMovOp(op) EmitSIMD(op.mov(), to, to, from)

	extern void SimdPrefix(u8 prefix, u16 opcode);
	extern void EmitSibMagic(uint regfield, const void* address, int extraRIPOffset = 0);
	extern void EmitSibMagic(uint regfield, const xIndirectVoid& info, int extraRIPOffset = 0);
	extern void EmitSibMagic(uint reg1, const xRegisterBase& reg2, int = 0);
	extern void EmitSibMagic(const xRegisterBase& reg1, const xRegisterBase& reg2, int = 0);
	extern void EmitSibMagic(const xRegisterBase& reg1, const void* src, int extraRIPOffset = 0);
	extern void EmitSibMagic(const xRegisterBase& reg1, const xIndirectVoid& sib, int extraRIPOffset = 0);

	extern void EmitRex(uint regfield, const void* address);
	extern void EmitRex(uint regfield, const xIndirectVoid& info);
	extern void EmitRex(uint reg1, const xRegisterBase& reg2);
	extern void EmitRex(const xRegisterBase& reg1, const xRegisterBase& reg2);
	extern void EmitRex(const xRegisterBase& reg1, const void* src);
	extern void EmitRex(const xRegisterBase& reg1, const xIndirectVoid& sib);
	extern void EmitRex(SIMDInstructionInfo info, u32 reg1, const xRegisterBase& reg2);
	extern void EmitRex(SIMDInstructionInfo info, const xRegisterBase& reg1, const xRegisterBase& reg2);
	extern void EmitRex(SIMDInstructionInfo info, const xRegisterBase& reg1, const xIndirectVoid& sib);

	extern void _xMovRtoR(const xRegisterInt& to, const xRegisterInt& from);

	template <typename T>
	inline void xWrite(T val)
	{
		*(T*)x86Ptr = val;
		x86Ptr += sizeof(T);
	}

	template <typename T1, typename T2>
	__emitinline void xOpWrite(u8 prefix, u8 opcode, const T1& param1, const T2& param2, int extraRIPOffset = 0)
	{
		if (prefix != 0)
			xWrite8(prefix);
		EmitRex(param1, param2);

		xWrite8(opcode);

		EmitSibMagic(param1, param2, extraRIPOffset);
	}

	template <typename T1, typename T2>
	__emitinline void xOpAccWrite(u8 prefix, u8 opcode, const T1& param1, const T2& param2)
	{
		if (prefix != 0)
			xWrite8(prefix);
		EmitRex(param1, param2);

		xWrite8(opcode);
	}


	//////////////////////////////////////////////////////////////////////////////////////////
	// emitter helpers for xmm instruction with prefixes, most of which are using
	// the basic opcode format (items inside braces denote optional or conditional
	// emission):
	//
	//   [Prefix] / 0x0f / [OpcodePrefix] / Opcode / ModRM+[SibSB]
	//
	// Prefixes are typically 0x66, 0xf2, or 0xf3.  OpcodePrefixes are either 0x38 or
	// 0x3a [and other value will result in assertion failue].
	//
	template <typename T1, typename T2>
	__emitinline void xOpWrite0F(u8 prefix, u16 opcode, const T1& param1, const T2& param2)
	{
		if (prefix != 0)
			xWrite8(prefix);
		EmitRex(param1, param2);

		SimdPrefix(0, opcode);

		EmitSibMagic(param1, param2);
	}

	template <typename T1, typename T2>
	__emitinline void xOpWrite0F(u8 prefix, u16 opcode, const T1& param1, const T2& param2, u8 imm8)
	{
		if (prefix != 0)
			xWrite8(prefix);
		EmitRex(param1, param2);

		SimdPrefix(0, opcode);

		EmitSibMagic(param1, param2, 1);
		xWrite8(imm8);
	}

	template <typename T1, typename T2>
	__emitinline void xOpWrite0F(u16 opcode, const T1& param1, const T2& param2)
	{
		xOpWrite0F(0, opcode, param1, param2);
	}

	template <typename T1, typename T2>
	__emitinline void xOpWrite0F(u16 opcode, const T1& param1, const T2& param2, u8 imm8)
	{
		xOpWrite0F(0, opcode, param1, param2, imm8);
	}

	// VEX 2 Bytes Prefix
	template <typename T1, typename T2, typename T3>
	__emitinline void xOpWriteC5(u8 prefix, u8 opcode, const T1& param1, const T2& param2, const T3& param3)
	{
		pxAssert(prefix == 0 || prefix == 0x66 || prefix == 0xF3 || prefix == 0xF2);

		const xRegisterBase& reg = param1.IsReg() ? param1 : param2;

		u8 nR = reg.IsExtended() ? 0x00 : 0x80;
		u8 L;

		// Needed for 256-bit movemask.
		if constexpr (std::is_same_v<T3, xRegisterSSE>)
			L = param3.IsWideSIMD() ? 4 : 0;
		else
			L = reg.IsWideSIMD() ? 4 : 0;

		u8 nv = (param2.IsEmpty() ? 0xF : ((~param2.GetId() & 0xF))) << 3;

		u8 p =
			prefix == 0xF2 ? 3 :
			prefix == 0xF3 ? 2 :
			prefix == 0x66 ? 1 :
                             0;

		xWrite8(0xC5);
		xWrite8(nR | nv | L | p);
		xWrite8(opcode);
		EmitSibMagic(param1, param3);
	}

	// VEX 3 Bytes Prefix
	template <typename T1, typename T2, typename T3>
	__emitinline void xOpWriteC4(u8 prefix, u8 mb_prefix, u8 opcode, const T1& param1, const T2& param2, const T3& param3, int w = -1)
	{
		pxAssert(prefix == 0 || prefix == 0x66 || prefix == 0xF3 || prefix == 0xF2);
		pxAssert(mb_prefix == 0x0F || mb_prefix == 0x38 || mb_prefix == 0x3A);

		const xRegisterInt& reg = param1.IsReg() ? param1 : param2;

		u8 nR = reg.IsExtended() ? 0x00 : 0x80;
		u8 nB = param3.IsExtended() ? 0x00 : 0x20;
		u8 nX = 0x40; // likely unused so hardwired to disabled
		u8 L = reg.IsWideSIMD() ? 4 : 0;
		u8 W = (w == -1) ? (reg.GetOperandSize() == 8 ? 0x80 : 0) : // autodetect the size
                           0x80 * w; // take directly the W value

		u8 nv = (~param2.GetId() & 0xF) << 3;

		u8 p =
			prefix == 0xF2 ? 3 :
			prefix == 0xF3 ? 2 :
			prefix == 0x66 ? 1 :
                             0;

		u8 m =
			mb_prefix == 0x3A ? 3 :
			mb_prefix == 0x38 ? 2 :
                                1;

		xWrite8(0xC4);
		xWrite8(nR | nX | nB | m);
		xWrite8(W | nv | L | p);
		xWrite8(opcode);
		EmitSibMagic(param1, param3);
	}

	void EmitVEX(SIMDInstructionInfo info, const xRegisterBase& dst, u8 src1, const xRegisterBase& src2, int extraRipOffset = 0);
	void EmitVEX(SIMDInstructionInfo info, const xRegisterBase& dst, u8 src1, const xIndirectVoid& src2, int extraRipOffset = 0);
	void EmitVEX(SIMDInstructionInfo info, u32 ext, u8 dst, const xRegisterBase& src2, int extraRipOffset = 0);

	template <typename S2>
	__emitinline static void EmitVEX(SIMDInstructionInfo info, const xRegisterBase& dst, const xRegisterBase& src1, const S2& src2, int extraRipOffset = 0)
	{
		EmitVEX(info, dst, src1.GetId(), src2, extraRipOffset);
	}

	// Emitter helpers for SIMD operations
	// These will dispatch to either SSE or AVX implementations

	void EmitSIMDImpl(SIMDInstructionInfo info, const xRegisterBase& dst, const xRegisterBase& src1, int extraRipOffset);
	void EmitSIMDImpl(SIMDInstructionInfo info, const xRegisterBase& dst, const xRegisterBase& src1, const xRegisterBase& src2, int extraRipOffset);
	void EmitSIMDImpl(SIMDInstructionInfo info, const xRegisterBase& dst, const xRegisterBase& src1, const xIndirectVoid& src2, int extraRipOffset);
	void EmitSIMD(SIMDInstructionInfo info, const xRegisterBase& dst, const xRegisterBase& src1, const xRegisterBase& src2, const xRegisterBase& src3);
	void EmitSIMD(SIMDInstructionInfo info, const xRegisterBase& dst, const xRegisterBase& src1, const xIndirectVoid& src2, const xRegisterBase& src3);

	__emitinline static void EmitSIMD(SIMDInstructionInfo info, const xRegisterBase& dst, const xRegisterBase& src1)
	{
		EmitSIMDImpl(info, dst, src1, 0);
	}
	__emitinline static void EmitSIMD(SIMDInstructionInfo info, const xRegisterBase& dst, const xRegisterBase& src1, u8 imm)
	{
		EmitSIMDImpl(info, dst, src1, 1);
		xWrite8(imm);
	}
	template <typename S2>
	__emitinline static void EmitSIMD(SIMDInstructionInfo info, const xRegisterBase& dst, const xRegisterBase& src1, const S2& src2)
	{
		EmitSIMDImpl(info, dst, src1, src2, 0);
	}
	template <typename S2>
	__emitinline static void EmitSIMD(SIMDInstructionInfo info, const xRegisterBase& dst, const xRegisterBase& src1, const S2& src2, u8 imm)
	{
		EmitSIMDImpl(info, dst, src1, src2, 1);
		xWrite8(imm);
	}
} // namespace x86Emitter
