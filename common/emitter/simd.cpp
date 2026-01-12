// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/emitter/internal.h"
#include "common/VectorIntrin.h"

namespace x86Emitter
{
	__emitinline static u8 getSSE(SIMDInstructionInfo::Prefix prefix)
	{
		switch (prefix) {
			case SIMDInstructionInfo::Prefix::P66: return 0x66;
			case SIMDInstructionInfo::Prefix::PF3: return 0xf3;
			case SIMDInstructionInfo::Prefix::PF2: return 0xf2;
			case SIMDInstructionInfo::Prefix::None:
			default:
				pxAssert(0);
				return 0;
		}
	}

	__emitinline static u16 getSSE(SIMDInstructionInfo::Map map)
	{
		switch (map) {
			case SIMDInstructionInfo::Map::M0F38: return 0x380f;
			case SIMDInstructionInfo::Map::M0F3A: return 0x3a0f;
			case SIMDInstructionInfo::Map::M0F:
			default:
				pxAssert(0);
				return 0;
		}
	}

	__emitinline static SIMDInstructionInfo getMov(SIMDInstructionInfo::Type type)
	{
		switch (type) {
			case SIMDInstructionInfo::Type::Integer:
				return SIMDInstructionInfo(0x6f).p66().mov();
			case SIMDInstructionInfo::Type::Double:
				return SIMDInstructionInfo(0x28).p66().mov();
			default:
			case SIMDInstructionInfo::Type::Float:
				return SIMDInstructionInfo(0x28).mov();
		}
	}

	template <typename T1, typename T2>
	__emitinline static void xOpWrite0F(SIMDInstructionInfo info, T1 dst, const T2& src, int extraRIPOffset)
	{
		if (info.prefix != SIMDInstructionInfo::Prefix::None)
			xWrite8(getSSE(info.prefix));
		pxAssert(!info.w_bit); // Only used by AVX
		EmitRex(info, dst, src);
		if (info.map == SIMDInstructionInfo::Map::M0F)
		{
			xWrite16(0x0F | (info.opcode << 8));
		}
		else
		{
			xWrite16(getSSE(info.map));
			xWrite8(info.opcode);
		}
		EmitSibMagic(dst, src, extraRIPOffset);
	}

	template <typename S2>
	__emitinline static void EmitSimdOp(SIMDInstructionInfo info, const xRegisterBase& dst, const xRegisterBase& src1, const S2& src2, int extraRIPOffset)
	{
		if (x86Emitter::use_avx)
		{
			EmitVEX(info, dst, info.is_mov ? 0 : src1.GetId(), src2, extraRIPOffset);
		}
		else
		{
			if (dst.GetId() != src1.GetId())
			{
				pxAssert(!info.is_mov);
				// Generate a mov to copy from src1 to dst
				xOpWrite0F(getMov(info.type), dst, src1, 0);
			}
			xOpWrite0F(info, dst, src2, extraRIPOffset);
		}
	}

	void EmitSIMDImpl(SIMDInstructionInfo info, const xRegisterBase& dst, const xRegisterBase& src1, int extraRipOffset)
	{
		pxAssert(!info.is_mov);
		if (x86Emitter::use_avx)
		{
			EmitVEX(info, info.ext, dst.GetId(), src1, extraRipOffset);
		}
		else
		{
			if (dst.GetId() != src1.GetId())
			{
				// Generate a mov to copy from src1 to dst
				xOpWrite0F(getMov(info.type), dst, src1, 0);
			}
			xOpWrite0F(info, info.ext, dst, extraRipOffset);
		}
	}
	void EmitSIMDImpl(SIMDInstructionInfo info, const xRegisterBase& dst, const xRegisterBase& src1, const xRegisterBase& src2, int extraRipOffset)
	{
		pxAssert(!info.is_mov || dst.GetId() == src1.GetId());
		const xRegisterBase* ps1 = &src1;
		const xRegisterBase* ps2 = &src2;
		if (x86Emitter::use_avx)
		{
			if (info.is_commutative && info.map == SIMDInstructionInfo::Map::M0F && src2.IsExtended() && !src1.IsExtended())
			{
				// We can use a C5 op instead of a C4 op if we swap the inputs
				std::swap(ps1, ps2);
			}
		}
		else if (dst.GetId() != src1.GetId() && dst.GetId() == src2.GetId())
		{
			if (info.is_commutative)
				std::swap(ps1, ps2);
			else
				pxAssertRel(0, "SSE4 auto mov would destroy the second source!");
		}
		EmitSimdOp(info, dst, *ps1, *ps2, extraRipOffset);
	}
	void EmitSIMDImpl(SIMDInstructionInfo info, const xRegisterBase& dst, const xRegisterBase& src1, const xIndirectVoid& src2, int extraRipOffset)
	{
		pxAssert(!info.is_mov || dst.GetId() == src1.GetId());
		if (!x86Emitter::use_avx && info.is_commutative && dst.GetId() != src1.GetId())
		{
			// Do load, op instead of mov, op+load
			// No processors differentiate between loads, so always use movaps
			EmitSimdOp(getMov(SIMDInstructionInfo::Type::Float), dst, dst, src2, 0);
			EmitSimdOp(info, dst, dst, src1, extraRipOffset);
		}
		else
		{
			EmitSimdOp(info, dst, src1, src2, extraRipOffset);
		}
	}
	void EmitSIMD(SIMDInstructionInfo info, const xRegisterBase& dst, const xRegisterBase& src1, const xRegisterBase& src2, const xRegisterBase& src3)
	{
		pxAssert(!info.is_mov);
		pxAssertMsg(!info.is_commutative, "I don't think any blend instructions are commutative...");
		if (x86Emitter::use_avx)
		{
			EmitSimdOp(info, dst, src1, src2, 1);
			xWrite8(src3.GetId());
		}
		else
		{
			pxAssertRel(src3.GetId() == 0, "SSE4 requires the third source to be xmm0!");
			if (dst.GetId() != src1.GetId() && dst.GetId() == src2.GetId())
				pxAssertRel(0, "SSE4 auto mov would destroy the second source!");
			EmitSimdOp(info, dst, src1, src2, 0);
		}

	}
	void EmitSIMD(SIMDInstructionInfo info, const xRegisterBase& dst, const xRegisterBase& src1, const xIndirectVoid& src2, const xRegisterBase& src3)
	{
		pxAssert(!info.is_mov);
		pxAssertMsg(!info.is_commutative, "I don't think any blend instructions are commutative...");
		if (x86Emitter::use_avx)
		{
			EmitSimdOp(info, dst, src1, src2, 1);
			xWrite8(src3.GetId());
		}
		else
		{
			pxAssertRel(src3.GetId() == 0, "SSE4 requires the third source to be xmm0!");
			EmitSimdOp(info, dst, src1, src2, 0);
		}
	}

	// ------------------------------------------------------------------------
	// SimdPrefix - If the lower byte of the opcode is 0x38 or 0x3a, then the opcode is
	// treated as a 16 bit value (in SSE 0x38 and 0x3a denote prefixes for extended SSE3/4
	// instructions).  Any other lower value assumes the upper value is 0 and ignored.
	// Non-zero upper bytes, when the lower byte is not the 0x38 or 0x3a prefix, will
	// generate an assertion.
	//
	__emitinline void SimdPrefix(u8 prefix, u16 opcode)
	{
		pxAssertMsg(prefix == 0, "REX prefix must be just before the opcode");

		const bool is16BitOpcode = ((opcode & 0xff) == 0x38) || ((opcode & 0xff) == 0x3a);

		// If the lower byte is not a valid prefix and the upper byte is non-zero it
		// means we made a mistake!
		if (!is16BitOpcode)
			pxAssert((opcode >> 8) == 0);

		if (prefix != 0)
		{
			if (is16BitOpcode)
				xWrite32((opcode << 16) | 0x0f00 | prefix);
			else
			{
				xWrite16(0x0f00 | prefix);
				xWrite8(opcode);
			}
		}
		else
		{
			if (is16BitOpcode)
			{
				xWrite8(0x0f);
				xWrite16(opcode);
			}
			else
				xWrite16((opcode << 8) | 0x0f);
		}
	}

	// clang-format off

	const xImplSimd_3Arg xPAND  = {SIMDInstructionInfo(0xdb).i().p66().commutative()};
	const xImplSimd_3Arg xPANDN = {SIMDInstructionInfo(0xdf).i().p66()};
	const xImplSimd_3Arg xPOR   = {SIMDInstructionInfo(0xeb).i().p66().commutative()};
	const xImplSimd_3Arg xPXOR  = {SIMDInstructionInfo(0xef).i().p66().commutative()};

	// [SSE-4.1] Performs a bitwise AND of dest against src, and sets the ZF flag
	// only if all bits in the result are 0.  PTEST also sets the CF flag according
	// to the following condition: (xmm2/m128 AND NOT xmm1) == 0;
	const xImplSimd_2Arg xPTEST = {SIMDInstructionInfo(0x17).p66().m0f38()};

	// =====================================================================================================
	// SSE Conversion Operations, as looney as they are.
	// =====================================================================================================
	// These enforce pointer strictness for Indirect forms, due to the otherwise completely confusing
	// nature of the functions.  (so if a function expects an m32, you must use (u32*) or ptr32[]).
	//

	__fi void xCVTDQ2PD(const xRegisterSSE& to, const xRegisterSSE& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0xe6).pf3()); }
	__fi void xCVTDQ2PD(const xRegisterSSE& to, const xIndirect64& from)  { OpWriteSIMDMovOp(SIMDInstructionInfo(0xe6).pf3()); }
	__fi void xCVTDQ2PS(const xRegisterSSE& to, const xRegisterSSE& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0x5b)); }
	__fi void xCVTDQ2PS(const xRegisterSSE& to, const xIndirect128& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0x5b)); }

	__fi void xCVTPD2DQ(const xRegisterSSE& to, const xRegisterSSE& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0xe6).pf2()); }
	__fi void xCVTPD2DQ(const xRegisterSSE& to, const xIndirect128& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0xe6).pf2()); }
	__fi void xCVTPD2PS(const xRegisterSSE& to, const xRegisterSSE& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0x5a).p66()); }
	__fi void xCVTPD2PS(const xRegisterSSE& to, const xIndirect128& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0x5a).p66()); }

	__fi void xCVTPI2PD(const xRegisterSSE& to, const xIndirect64& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0x2a).p66()); }
	__fi void xCVTPI2PS(const xRegisterSSE& to, const xIndirect64& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0x2a)); }

	__fi void xCVTPS2DQ(const xRegisterSSE& to, const xRegisterSSE& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0x5b).p66()); }
	__fi void xCVTPS2DQ(const xRegisterSSE& to, const xIndirect128& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0x5b).p66()); }
	__fi void xCVTPS2PD(const xRegisterSSE& to, const xRegisterSSE& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0x5a)); }
	__fi void xCVTPS2PD(const xRegisterSSE& to, const xIndirect64& from)  { OpWriteSIMDMovOp(SIMDInstructionInfo(0x5a)); }

	__fi void xCVTSD2SI(const xRegister32or64& to, const xRegisterSSE& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0x2d).dstw().pf2()); }
	__fi void xCVTSD2SI(const xRegister32or64& to, const xIndirect64& from)  { OpWriteSIMDMovOp(SIMDInstructionInfo(0x2d).dstw().pf2()); }
	__fi void xCVTSD2SS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE& src2)    { EmitSIMD(SIMDInstructionInfo(0x5a).pf2(), dst, src1, src2); }
	__fi void xCVTSD2SS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirect64& src2)     { EmitSIMD(SIMDInstructionInfo(0x5a).pf2(), dst, src1, src2); }
	__fi void xCVTSI2SS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegister32or64& src2) { EmitSIMD(SIMDInstructionInfo(0x2a).srcw().pf3(), dst, src1, src2); }
	__fi void xCVTSI2SS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirect32& src2)     { EmitSIMD(SIMDInstructionInfo(0x2a).srcw().pf3(), dst, src1, src2); }
	__fi void xCVTSI2SS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirect64& src2)     { EmitSIMD(SIMDInstructionInfo(0x2a).srcw().pf3(), dst, src1, src2); }

	__fi void xCVTSS2SI(const xRegister32or64& to, const xRegisterSSE& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0x2d).dstw().pf3()); }
	__fi void xCVTSS2SI(const xRegister32or64& to, const xIndirect32& from)  { OpWriteSIMDMovOp(SIMDInstructionInfo(0x2d).dstw().pf3()); }
	__fi void xCVTSS2SD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE& src2) { EmitSIMD(SIMDInstructionInfo(0x5a).pf3(), dst, src1, src2); }
	__fi void xCVTSS2SD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirect32& src2)  { EmitSIMD(SIMDInstructionInfo(0x5a).pf3(), dst, src1, src2); }

	__fi void xCVTTPD2DQ(const xRegisterSSE& to, const xRegisterSSE& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0xe6).p66()); }
	__fi void xCVTTPD2DQ(const xRegisterSSE& to, const xIndirect128& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0xe6).p66()); }
	__fi void xCVTTPS2DQ(const xRegisterSSE& to, const xRegisterSSE& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0x5b).pf3()); }
	__fi void xCVTTPS2DQ(const xRegisterSSE& to, const xIndirect128& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0x5b).pf3()); }

	__fi void xCVTTSD2SI(const xRegister32or64& to, const xRegisterSSE& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0x2c).dstw().pf2()); }
	__fi void xCVTTSD2SI(const xRegister32or64& to, const xIndirect64& from)  { OpWriteSIMDMovOp(SIMDInstructionInfo(0x2c).dstw().pf2()); }
	__fi void xCVTTSS2SI(const xRegister32or64& to, const xRegisterSSE& from) { OpWriteSIMDMovOp(SIMDInstructionInfo(0x2c).dstw().pf3()); }
	__fi void xCVTTSS2SI(const xRegister32or64& to, const xIndirect32& from)  { OpWriteSIMDMovOp(SIMDInstructionInfo(0x2c).dstw().pf3()); }


	// ------------------------------------------------------------------------

	void xImplSimd_2Arg::operator()(const xRegisterSSE& dst, const xRegisterSSE& src)  const { EmitSIMD(info, dst, dst, src); }
	void xImplSimd_2Arg::operator()(const xRegisterSSE& dst, const xIndirectVoid& src) const { EmitSIMD(info, dst, dst, src); }
	void xImplSimd_2ArgImm::operator()(const xRegisterSSE& dst, const xRegisterSSE& src,  u8 imm) const { EmitSIMD(info, dst, dst, src, imm); }
	void xImplSimd_2ArgImm::operator()(const xRegisterSSE& dst, const xIndirectVoid& src, u8 imm) const { EmitSIMD(info, dst, dst, src, imm); }
	void xImplSimd_3Arg::operator()(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE& src2)  const { EmitSIMD(info, dst, src1, src2); }
	void xImplSimd_3Arg::operator()(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2) const { EmitSIMD(info, dst, src1, src2); }
	void xImplSimd_3ArgImm::operator()(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE& src2,  u8 imm) const { EmitSIMD(info, dst, src1, src2, imm); }
	void xImplSimd_3ArgImm::operator()(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2, u8 imm) const { EmitSIMD(info, dst, src1, src2, imm); }
	void xImplSimd_3ArgCmp::operator()(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE& src2,  SSE2_ComparisonType imm) const { EmitSIMD(info, dst, src1, src2, imm); }
	void xImplSimd_3ArgCmp::operator()(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2, SSE2_ComparisonType imm) const { EmitSIMD(info, dst, src1, src2, imm); }
	void xImplSimd_4ArgBlend::operator()(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE& src2,  const xRegisterSSE& src3) const { EmitSIMD(x86Emitter::use_avx ? avx : sse, dst, src1, src2, src3); }
	void xImplSimd_4ArgBlend::operator()(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2, const xRegisterSSE& src3) const { EmitSIMD(x86Emitter::use_avx ? avx : sse, dst, src1, src2, src3); }

	// =====================================================================================================
	//  SIMD Arithmetic Instructions
	// =====================================================================================================

	void _SimdShiftHelper::operator()(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE& src2)  const { EmitSIMD(info, dst, src1, src2); }
	void _SimdShiftHelper::operator()(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2) const { EmitSIMD(info, dst, src1, src2); }


	void _SimdShiftHelper::operator()(const xRegisterSSE& dst, const xRegisterSSE& src, u8 imm8) const { EmitSIMD(infoImm, dst, src, imm8); }

	void xImplSimd_Shift::DQ(const xRegisterSSE& dst, const xRegisterSSE& src, u8 imm8) const
	{
		SIMDInstructionInfo info = Q.infoImm;
		info.ext += 1;
		EmitSIMD(info, dst, src, imm8);
	}


	const xImplSimd_ShiftWithoutQ xPSRA =
	{
		{SIMDInstructionInfo(0xe1).p66().i(), SIMDInstructionInfo(0x71, 4).p66().i()}, // W
		{SIMDInstructionInfo(0xe2).p66().i(), SIMDInstructionInfo(0x72, 4).p66().i()}, // D
	};

	const xImplSimd_Shift xPSRL =
	{
		{SIMDInstructionInfo(0xd1).p66().i(), SIMDInstructionInfo(0x71, 2).p66().i()}, // W
		{SIMDInstructionInfo(0xd2).p66().i(), SIMDInstructionInfo(0x72, 2).p66().i()}, // D
		{SIMDInstructionInfo(0xd3).p66().i(), SIMDInstructionInfo(0x73, 2).p66().i()}, // Q
	};

	const xImplSimd_Shift xPSLL =
	{
		{SIMDInstructionInfo(0xf1).p66().i(), SIMDInstructionInfo(0x71, 6).p66().i()}, // W
		{SIMDInstructionInfo(0xf2).p66().i(), SIMDInstructionInfo(0x72, 6).p66().i()}, // D
		{SIMDInstructionInfo(0xf3).p66().i(), SIMDInstructionInfo(0x73, 6).p66().i()}, // Q
	};

	const xImplSimd_AddSub xPADD =
	{
		{SIMDInstructionInfo(0xfc).p66().i().commutative()}, // B
		{SIMDInstructionInfo(0xfd).p66().i().commutative()}, // W
		{SIMDInstructionInfo(0xfe).p66().i().commutative()}, // D
		{SIMDInstructionInfo(0xd4).p66().i().commutative()}, // Q

		{SIMDInstructionInfo(0xec).p66().i().commutative()}, // SB
		{SIMDInstructionInfo(0xed).p66().i().commutative()}, // SW
		{SIMDInstructionInfo(0xdc).p66().i().commutative()}, // USB
		{SIMDInstructionInfo(0xdd).p66().i().commutative()}, // USW
	};

	const xImplSimd_AddSub xPSUB =
	{
		{SIMDInstructionInfo(0xf8).p66().i()}, // B
		{SIMDInstructionInfo(0xf9).p66().i()}, // W
		{SIMDInstructionInfo(0xfa).p66().i()}, // D
		{SIMDInstructionInfo(0xfb).p66().i()}, // Q

		{SIMDInstructionInfo(0xe8).p66().i()}, // SB
		{SIMDInstructionInfo(0xe9).p66().i()}, // SW
		{SIMDInstructionInfo(0xd8).p66().i()}, // USB
		{SIMDInstructionInfo(0xd9).p66().i()}, // USW
	};

	const xImplSimd_PMul xPMUL =
	{
		{SIMDInstructionInfo(0xd5).p66().i().commutative()}, // LW
		{SIMDInstructionInfo(0xe5).p66().i().commutative()}, // HW
		{SIMDInstructionInfo(0xe4).p66().i().commutative()}, // HUW
		{SIMDInstructionInfo(0xf4).p66().i().commutative()}, // UDQ

		{SIMDInstructionInfo(0x0b).p66().m0f38().i().commutative()}, // HRSW
		{SIMDInstructionInfo(0x40).p66().m0f38().i().commutative()}, // LD
		{SIMDInstructionInfo(0x28).p66().m0f38().i().commutative()}, // DQ
	};

	const xImplSimd_rSqrt xRSQRT =
	{
		{SIMDInstructionInfo(0x52)},       // PS
		{SIMDInstructionInfo(0x52).pf3()}, // SS
	};

	const xImplSimd_rSqrt xRCP =
	{
		{SIMDInstructionInfo(0x53)},       // PS
		{SIMDInstructionInfo(0x53).pf3()}, // SS
	};

	const xImplSimd_Sqrt xSQRT =
	{
		{SIMDInstructionInfo(0x51)},       // PS
		{SIMDInstructionInfo(0x51).pf3()}, // SS
		{SIMDInstructionInfo(0x51).p66()}, // PD
		{SIMDInstructionInfo(0x51).pf2()}, // SS
	};

	const xImplSimd_AndNot xANDN =
	{
		{SIMDInstructionInfo(0x55)},       // PS
		{SIMDInstructionInfo(0x55).p66()}, // PD
	};

	const xImplSimd_PAbsolute xPABS =
	{
		{SIMDInstructionInfo(0x1c).p66().m0f38().i()}, // B
		{SIMDInstructionInfo(0x1d).p66().m0f38().i()}, // W
		{SIMDInstructionInfo(0x1e).p66().m0f38().i()}, // D
	};

	const xImplSimd_PSign xPSIGN =
	{
		{SIMDInstructionInfo(0x08).p66().m0f38().i()}, // B
		{SIMDInstructionInfo(0x09).p66().m0f38().i()}, // W
		{SIMDInstructionInfo(0x0a).p66().m0f38().i()}, // D
	};

	const xImplSimd_PMultAdd xPMADD =
	{
		{SIMDInstructionInfo(0xf5).p66().i().commutative()},         // WD
		{SIMDInstructionInfo(0x04).p66().m0f38().i().commutative()}, // UBSW
	};

	const xImplSimd_HorizAdd xHADD =
	{
		{SIMDInstructionInfo(0x7c).pf2()}, // PS
		{SIMDInstructionInfo(0x7c).p66()}, // PD
	};

	const xImplSimd_DotProduct xDP =
	{
		{SIMDInstructionInfo(0x40).p66().m0f3a().commutative()}, // PS
		{SIMDInstructionInfo(0x41).p66().m0f3a().commutative()}, // PD
	};

	const xImplSimd_Round xROUND =
	{
		{SIMDInstructionInfo(0x08).p66().m0f3a()}, // PS
		{SIMDInstructionInfo(0x09).p66().m0f3a()}, // PD
		{SIMDInstructionInfo(0x0a).p66().m0f3a()}, // SS
		{SIMDInstructionInfo(0x0b).p66().m0f3a()}, // SD
	};

	// =====================================================================================================
	//  SIMD Comparison Instructions
	// =====================================================================================================

	void xImplSimd_Compare::PS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE&  src2) const { EmitSIMD(SIMDInstructionInfo(0xc2).f(), dst, src1, src2, CType); }
	void xImplSimd_Compare::PS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2) const { EmitSIMD(SIMDInstructionInfo(0xc2).f(), dst, src1, src2, CType); }

	void xImplSimd_Compare::PD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE&  src2) const { EmitSIMD(SIMDInstructionInfo(0xc2).d().p66(), dst, src1, src2, CType); }
	void xImplSimd_Compare::PD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2) const { EmitSIMD(SIMDInstructionInfo(0xc2).d().p66(), dst, src1, src2, CType); }

	void xImplSimd_Compare::SS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE&  src2) const { EmitSIMD(SIMDInstructionInfo(0xc2).f().pf3(), dst, src1, src2, CType); }
	void xImplSimd_Compare::SS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2) const { EmitSIMD(SIMDInstructionInfo(0xc2).f().pf3(), dst, src1, src2, CType); }

	void xImplSimd_Compare::SD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE&  src2) const { EmitSIMD(SIMDInstructionInfo(0xc2).d().pf2(), dst, src1, src2, CType); }
	void xImplSimd_Compare::SD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2) const { EmitSIMD(SIMDInstructionInfo(0xc2).d().pf2(), dst, src1, src2, CType); }

	const xImplSimd_MinMax xMIN =
	{
		{SIMDInstructionInfo(0x5d).f()},       // PS
		{SIMDInstructionInfo(0x5d).d().p66()}, // PD
		{SIMDInstructionInfo(0x5d).f().pf3()}, // SS
		{SIMDInstructionInfo(0x5d).d().pf2()}, // SD
	};

	const xImplSimd_MinMax xMAX =
	{
		{SIMDInstructionInfo(0x5f).f()},       // PS
		{SIMDInstructionInfo(0x5f).d().p66()}, // PD
		{SIMDInstructionInfo(0x5f).f().pf3()}, // SS
		{SIMDInstructionInfo(0x5f).d().pf2()}, // SD
	};

	// [TODO] : Merge this into the xCMP class, so that they are notation as: xCMP.EQ

	const xImplSimd_Compare xCMPEQ = {SSE2_Equal};
	const xImplSimd_Compare xCMPLT = {SSE2_Less};
	const xImplSimd_Compare xCMPLE = {SSE2_LessOrEqual};
	const xImplSimd_Compare xCMPUNORD = {SSE2_Unordered};
	const xImplSimd_Compare xCMPNE = {SSE2_NotEqual};
	const xImplSimd_Compare xCMPNLT = {SSE2_NotLess};
	const xImplSimd_Compare xCMPNLE = {SSE2_NotLessOrEqual};
	const xImplSimd_Compare xCMPORD = {SSE2_Ordered};

	const xImplSimd_COMI xCOMI =
	{
		{SIMDInstructionInfo(0x2f)},       // SS
		{SIMDInstructionInfo(0x2f).p66()}, // SD
	};

	const xImplSimd_COMI xUCOMI =
	{
		{SIMDInstructionInfo(0x2e)},       // SS
		{SIMDInstructionInfo(0x2e).p66()}, // SD
	};

	const xImplSimd_PCompare xPCMP =
	{
		{SIMDInstructionInfo(0x74).i().p66().commutative()}, // EQB
		{SIMDInstructionInfo(0x75).i().p66().commutative()}, // EQW
		{SIMDInstructionInfo(0x76).i().p66().commutative()}, // EQD

		{SIMDInstructionInfo(0x64).i().p66()}, // GTB
		{SIMDInstructionInfo(0x65).i().p66()}, // GTW
		{SIMDInstructionInfo(0x66).i().p66()}, // GTD
	};

	const xImplSimd_PMinMax xPMIN =
	{
		{SIMDInstructionInfo(0xda).i().p66().commutative()},         // UB
		{SIMDInstructionInfo(0xea).i().p66().commutative()},         // SW
		{SIMDInstructionInfo(0x38).i().p66().m0f38().commutative()}, // SB
		{SIMDInstructionInfo(0x39).i().p66().m0f38().commutative()}, // SD
		{SIMDInstructionInfo(0x3a).i().p66().m0f38().commutative()}, // UW
		{SIMDInstructionInfo(0x3b).i().p66().m0f38().commutative()}, // UD
	};

	const xImplSimd_PMinMax xPMAX =
	{
		{SIMDInstructionInfo(0xde).i().p66().commutative()},         // UB
		{SIMDInstructionInfo(0xee).i().p66().commutative()},         // SW
		{SIMDInstructionInfo(0x3c).i().p66().m0f38().commutative()}, // SB
		{SIMDInstructionInfo(0x3d).i().p66().m0f38().commutative()}, // SD
		{SIMDInstructionInfo(0x3e).i().p66().m0f38().commutative()}, // UW
		{SIMDInstructionInfo(0x3f).i().p66().m0f38().commutative()}, // UD
	};

	// =====================================================================================================
	//  SIMD Shuffle/Pack  (Shuffle puck?)
	// =====================================================================================================

	__fi void xImplSimd_Shuffle::_selector_assertion_check(u8 selector) const
	{
		pxAssertMsg((selector & ~3) == 0,
			"Invalid immediate operand on SSE Shuffle: Upper 6 bits of the SSE Shuffle-PD Selector are reserved and must be zero.");
	}

	void xImplSimd_Shuffle::PS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE& src2, u8 selector) const
	{
		EmitSIMD(SIMDInstructionInfo(0xc6), dst, src1, src2, selector);
	}

	void xImplSimd_Shuffle::PS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2, u8 selector) const
	{
		EmitSIMD(SIMDInstructionInfo(0xc6), dst, src1, src2, selector);
	}

	void xImplSimd_Shuffle::PD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE& src2, u8 selector) const
	{
		_selector_assertion_check(selector);
		EmitSIMD(SIMDInstructionInfo(0xc6).d().p66(), dst, src1, src2, selector);
	}

	void xImplSimd_Shuffle::PD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2, u8 selector) const
	{
		_selector_assertion_check(selector);
		EmitSIMD(SIMDInstructionInfo(0xc6).d().p66(), dst, src1, src2, selector);
	}

	void xImplSimd_PInsert::B(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegister32& src2, u8 imm8) const { EmitSIMD(SIMDInstructionInfo(0x20).i().p66().m0f3a(), dst, src1, src2, imm8); }
	void xImplSimd_PInsert::B(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirect8&  src2, u8 imm8) const { EmitSIMD(SIMDInstructionInfo(0x20).i().p66().m0f3a(), dst, src1, src2, imm8); }

	void xImplSimd_PInsert::W(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegister32& src2, u8 imm8) const { EmitSIMD(SIMDInstructionInfo(0xc4).i().p66(), dst, src1, src2, imm8); }
	void xImplSimd_PInsert::W(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirect16& src2, u8 imm8) const { EmitSIMD(SIMDInstructionInfo(0xc4).i().p66(), dst, src1, src2, imm8); }

	void xImplSimd_PInsert::D(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegister32& src2, u8 imm8) const { EmitSIMD(SIMDInstructionInfo(0x22).i().p66().m0f3a().srcw(), dst, src1, src2, imm8); }
	void xImplSimd_PInsert::D(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirect32& src2, u8 imm8) const { EmitSIMD(SIMDInstructionInfo(0x22).i().p66().m0f3a().srcw(), dst, src1, src2, imm8); }

	void xImplSimd_PInsert::Q(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegister64& src2, u8 imm8) const { EmitSIMD(SIMDInstructionInfo(0x22).i().p66().m0f3a().srcw(), dst, src1, src2, imm8); }
	void xImplSimd_PInsert::Q(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirect64& src2, u8 imm8) const { EmitSIMD(SIMDInstructionInfo(0x22).i().p66().m0f3a().srcw(), dst, src1, src2, imm8); }

	void SimdImpl_PExtract::B(const xRegister32& dst, const xRegisterSSE& src, u8 imm8) const { EmitSIMD(SIMDInstructionInfo(0x14).mov().p66().m0f3a(), src, src, dst, imm8); }
	void SimdImpl_PExtract::B(const xIndirect8&  dst, const xRegisterSSE& src, u8 imm8) const { EmitSIMD(SIMDInstructionInfo(0x14).mov().p66().m0f3a(), src, src, dst, imm8); }

	void SimdImpl_PExtract::W(const xRegister32& dst, const xRegisterSSE& src, u8 imm8) const { EmitSIMD(SIMDInstructionInfo(0xc5).mov().p66(), dst, dst, src, imm8); }
	void SimdImpl_PExtract::W(const xIndirect16& dst, const xRegisterSSE& src, u8 imm8) const { EmitSIMD(SIMDInstructionInfo(0x15).mov().p66().m0f3a(), src, src, dst, imm8); }

	void SimdImpl_PExtract::D(const xRegister32& dst, const xRegisterSSE& src, u8 imm8) const { EmitSIMD(SIMDInstructionInfo(0x16).mov().p66().m0f3a().srcw(), src, src, dst, imm8); }
	void SimdImpl_PExtract::D(const xIndirect32& dst, const xRegisterSSE& src, u8 imm8) const { EmitSIMD(SIMDInstructionInfo(0x16).mov().p66().m0f3a().srcw(), src, src, dst, imm8); }

	void SimdImpl_PExtract::Q(const xRegister64& dst, const xRegisterSSE& src, u8 imm8) const { EmitSIMD(SIMDInstructionInfo(0x16).mov().p66().m0f3a().srcw(), src, src, dst, imm8); }
	void SimdImpl_PExtract::Q(const xIndirect64& dst, const xRegisterSSE& src, u8 imm8) const { EmitSIMD(SIMDInstructionInfo(0x16).mov().p66().m0f3a().srcw(), src, src, dst, imm8); }

	const xImplSimd_Shuffle xSHUF = {};

	const xImplSimd_PShuffle xPSHUF =
	{
		{SIMDInstructionInfo(0x70).i().p66()},         // D
		{SIMDInstructionInfo(0x70).i().pf2()},         // LW
		{SIMDInstructionInfo(0x70).i().pf3()},         // HW
		{SIMDInstructionInfo(0x00).i().p66().m0f38()}, // B
	};

	const SimdImpl_PUnpack xPUNPCK =
	{
		{SIMDInstructionInfo(0x60).i().p66()}, // LBW
		{SIMDInstructionInfo(0x61).i().p66()}, // LWD
		{SIMDInstructionInfo(0x62).i().p66()}, // LDQ
		{SIMDInstructionInfo(0x6c).i().p66()}, // LQDQ

		{SIMDInstructionInfo(0x68).i().p66()}, // HBW
		{SIMDInstructionInfo(0x69).i().p66()}, // HWD
		{SIMDInstructionInfo(0x6a).i().p66()}, // HDQ
		{SIMDInstructionInfo(0x6d).i().p66()}, // HQDQ
	};

	const SimdImpl_Pack xPACK =
	{
		{SIMDInstructionInfo(0x63).i().p66()},         // SSWB
		{SIMDInstructionInfo(0x6b).i().p66()},         // SSDW
		{SIMDInstructionInfo(0x67).i().p66()},         // USWB
		{SIMDInstructionInfo(0x2b).i().p66().m0f38()}, // USDW
	};

	const xImplSimd_Unpack xUNPCK =
	{
		{SIMDInstructionInfo(0x15).f()},       // HPS
		{SIMDInstructionInfo(0x15).d().p66()}, // HPD
		{SIMDInstructionInfo(0x14).f()},       // LPS
		{SIMDInstructionInfo(0x14).d().p66()}, // LPD
	};

	const xImplSimd_PInsert xPINSR;
	const SimdImpl_PExtract xPEXTR;

	static SIMDInstructionInfo nextop(SIMDInstructionInfo op, u32 offset = 1)
	{
		op.opcode += offset;
		return op;
	}

	// =====================================================================================================
	//  SIMD Move And Blend Instructions
	// =====================================================================================================

	void xImplSimd_MovHL::PS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2) const { EmitSIMD(info, dst, src1, src2); }
	void xImplSimd_MovHL::PS(const xIndirectVoid& dst, const xRegisterSSE& src) const { EmitSIMD(nextop(info).mov(), src, src, dst); }

	void xImplSimd_MovHL::PD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirectVoid& src2) const { EmitSIMD(info.p66(), dst, src1, src2); }
	void xImplSimd_MovHL::PD(const xIndirectVoid& dst, const xRegisterSSE& src) const { EmitSIMD(nextop(info).p66().mov(), src, src, dst); }

	void xImplSimd_MovHL_RtoR::PS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE& src2) const { EmitSIMD(info, dst, src1, src2); }
	void xImplSimd_MovHL_RtoR::PD(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE& src2) const { EmitSIMD(info.p66(), dst, src1, src2); }

	static bool IsAligned(const xRegisterSSE& reg, const xIndirectVoid& mem)
	{
		u32 mask = reg.GetOperandSize() - 1;
		// Aligned if it's displacement-only and the displacement is aligned
		if (mem.Displacement & mask)
			return false;
		return mem.Index.IsEmpty() && mem.Base.IsEmpty();
	}

	static const xImplSimd_MoveSSE& GetLoadStoreOp(const xImplSimd_MoveSSE* op)
	{
		if (!x86Emitter::use_avx)
		{
			// movaps is shorter, and no processor differentiates between the various movs for load/store
			const bool aligned = std::bit_cast<u32>(op->aligned_load) == std::bit_cast<u32>(op->unaligned_load);
			return aligned ? xMOVAPS : xMOVUPS;
		}
		return *op;
	}

	void xImplSimd_MoveSSE::operator()(const xRegisterSSE& dst, const xRegisterSSE& src) const
	{
		if (dst.GetId() == src.GetId() && dst.GetOperandSize() == src.GetOperandSize())
			return;
		SIMDInstructionInfo info = aligned_load;
		const xRegisterSSE* arg0 = &dst;
		const xRegisterSSE* arg1 = &src;
		if (x86Emitter::use_avx)
		{
			if (arg1->IsExtended() && !arg0->IsExtended())
			{
				// Can save a byte by using the store opcode
				info = aligned_store;
				std::swap(arg0, arg1);
			}
		}
		EmitSIMD(info, *arg0, *arg0, *arg1);
	}

	void xImplSimd_MoveSSE::operator()(const xRegisterSSE& dst, const xIndirectVoid& src) const
	{
		const xImplSimd_MoveSSE& op = GetLoadStoreOp(this);
		EmitSIMD(IsAligned(dst, src) ? op.aligned_load : op.unaligned_load, dst, dst, src);
	}

	void xImplSimd_MoveSSE::operator()(const xIndirectVoid& dst, const xRegisterSSE& src) const
	{
		const xImplSimd_MoveSSE& op = GetLoadStoreOp(this);
		EmitSIMD(IsAligned(src, dst) ? aligned_store : op.unaligned_store, src, src, dst);
	}

	void xImplSimd_PMove::BW(const xRegisterSSE& dst, const xRegisterSSE&  src) const { EmitSIMD(info, dst, dst, src); }
	void xImplSimd_PMove::BW(const xRegisterSSE& dst, const xIndirectVoid& src) const { EmitSIMD(info, dst, dst, src); }

	void xImplSimd_PMove::BD(const xRegisterSSE& dst, const xRegisterSSE&  src) const { EmitSIMD(nextop(info), dst, dst, src); }
	void xImplSimd_PMove::BD(const xRegisterSSE& dst, const xIndirectVoid& src) const { EmitSIMD(nextop(info), dst, dst, src); }

	void xImplSimd_PMove::BQ(const xRegisterSSE& dst, const xRegisterSSE&  src) const { EmitSIMD(nextop(info, 2), dst, dst, src); }
	void xImplSimd_PMove::BQ(const xRegisterSSE& dst, const xIndirectVoid& src) const { EmitSIMD(nextop(info, 2), dst, dst, src); }

	void xImplSimd_PMove::WD(const xRegisterSSE& dst, const xRegisterSSE&  src) const { EmitSIMD(nextop(info, 3), dst, dst, src); }
	void xImplSimd_PMove::WD(const xRegisterSSE& dst, const xIndirectVoid& src) const { EmitSIMD(nextop(info, 3), dst, dst, src); }

	void xImplSimd_PMove::WQ(const xRegisterSSE& dst, const xRegisterSSE&  src) const { EmitSIMD(nextop(info, 4), dst, dst, src); }
	void xImplSimd_PMove::WQ(const xRegisterSSE& dst, const xIndirectVoid& src) const { EmitSIMD(nextop(info, 4), dst, dst, src); }

	void xImplSimd_PMove::DQ(const xRegisterSSE& dst, const xRegisterSSE&  src) const { EmitSIMD(nextop(info, 5), dst, dst, src); }
	void xImplSimd_PMove::DQ(const xRegisterSSE& dst, const xIndirectVoid& src) const { EmitSIMD(nextop(info, 5), dst, dst, src); }


	const xImplSimd_MoveSSE xMOVAPS = {
		SIMDInstructionInfo(0x28).mov(), SIMDInstructionInfo(0x29).mov(),
		SIMDInstructionInfo(0x28).mov(), SIMDInstructionInfo(0x29).mov(),
	};
	const xImplSimd_MoveSSE xMOVUPS = {
		SIMDInstructionInfo(0x28).mov(), SIMDInstructionInfo(0x29).mov(),
		SIMDInstructionInfo(0x10).mov(), SIMDInstructionInfo(0x11).mov(),
	};

	const xImplSimd_MoveSSE xMOVDQA = {
		SIMDInstructionInfo(0x6f).p66().mov(), SIMDInstructionInfo(0x7f).p66().mov(),
		SIMDInstructionInfo(0x6f).p66().mov(), SIMDInstructionInfo(0x7f).p66().mov(),
	};
	const xImplSimd_MoveSSE xMOVDQU = {
		SIMDInstructionInfo(0x6f).p66().mov(), SIMDInstructionInfo(0x7f).p66().mov(),
		SIMDInstructionInfo(0x6f).pf3().mov(), SIMDInstructionInfo(0x7f).pf3().mov(),
	};

	const xImplSimd_MoveSSE xMOVAPD = {
		SIMDInstructionInfo(0x28).p66().mov(), SIMDInstructionInfo(0x29).p66().mov(),
		SIMDInstructionInfo(0x28).p66().mov(), SIMDInstructionInfo(0x29).p66().mov(),
	};
	const xImplSimd_MoveSSE xMOVUPD = {
		SIMDInstructionInfo(0x28).p66().mov(), SIMDInstructionInfo(0x29).p66().mov(),
		SIMDInstructionInfo(0x10).p66().mov(), SIMDInstructionInfo(0x11).p66().mov(),
	};


	const xImplSimd_MovHL xMOVH = {SIMDInstructionInfo(0x16)};
	const xImplSimd_MovHL xMOVL = {SIMDInstructionInfo(0x12)};

	const xImplSimd_MovHL_RtoR xMOVLH = {SIMDInstructionInfo(0x16)};
	const xImplSimd_MovHL_RtoR xMOVHL = {SIMDInstructionInfo(0x12)};

	const xImplSimd_PBlend xPBLEND =
	{
		{SIMDInstructionInfo(0x0e).i().p66().m0f3a()}, // W
		{SIMDInstructionInfo(0x10).i().p66().m0f38(), SIMDInstructionInfo(0x4c).i().p66().m0f3a()}, // VB
	};

	const xImplSimd_Blend xBLEND =
	{
		{SIMDInstructionInfo(0x0c).p66().f().m0f3a()}, // PS
		{SIMDInstructionInfo(0x0d).p66().d().m0f3a()}, // PD
		{SIMDInstructionInfo(0x14).p66().f().m0f38(), SIMDInstructionInfo(0x4a).f().p66().m0f3a()}, // VPS
		{SIMDInstructionInfo(0x15).p66().d().m0f38(), SIMDInstructionInfo(0x4b).d().p66().m0f3a()}, // VPD
	};

	const xImplSimd_PMove xPMOVSX = {SIMDInstructionInfo(0x20).p66().m0f38().mov()};
	const xImplSimd_PMove xPMOVZX = {SIMDInstructionInfo(0x30).p66().m0f38().mov()};

	// [SSE-3]
	const xImplSimd_2Arg xMOVSLDUP = {SIMDInstructionInfo(0x12).pf3()};

	// [SSE-3]
	const xImplSimd_2Arg xMOVSHDUP = {SIMDInstructionInfo(0x16).pf3()};

	//////////////////////////////////////////////////////////////////////////////////////////
	// MMX Mov Instructions (MOVD, MOVQ, MOVSS).
	//
	// Notes:
	//  * Some of the functions have been renamed to more clearly reflect what they actually
	//    do.  Namely we've affixed "ZX" to several MOVs that take a register as a destination
	//    since that's what they do (MOVD clears upper 32/96 bits, etc).
	//
	//  * MOVD has valid forms for MMX and XMM registers.
	//

	__fi void xMOVDZX(const xRegisterSSE& dst, const xRegister32or64& src) { EmitSIMD(SIMDInstructionInfo(0x6e).p66().srcw().mov(), dst, dst, src); }
	__fi void xMOVDZX(const xRegisterSSE& dst, const xIndirectVoid&   src) { EmitSIMD(SIMDInstructionInfo(0x6e).p66().mov(),        dst, dst, src); }

	__fi void xMOVD(const xRegister32or64& dst, const xRegisterSSE& src) { EmitSIMD(SIMDInstructionInfo(0x7e).p66().srcw().mov(), src, src, dst); }
	__fi void xMOVD(const xIndirectVoid&   dst, const xRegisterSSE& src) { EmitSIMD(SIMDInstructionInfo(0x7e).p66().mov(),        src, src, dst); }

	__fi void xMOVQZX(const xRegisterSSE& dst, const xRegisterSSE&  src) { EmitSIMD(SIMDInstructionInfo(0x7e).pf3().mov(), dst, dst, src); }
	__fi void xMOVQZX(const xRegisterSSE& dst, const xIndirectVoid& src) { EmitSIMD(SIMDInstructionInfo(0x7e).pf3().mov(), dst, dst, src); }

	// Moves lower quad of XMM to ptr64 (no bits are cleared)
	__fi void xMOVQ(const xIndirectVoid& dst, const xRegisterSSE& src) { EmitSIMD(SIMDInstructionInfo(0xd6).p66().mov(), src, src, dst); }

	//////////////////////////////////////////////////////////////////////////////////////////
	//

#define IMPLEMENT_xMOVS(ssd, prefix) \
	__fi void xMOV##ssd(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE& src2) \
	{ \
		if (src1 == src2) \
			return xMOVAPS(dst, src1); \
		SIMDInstructionInfo op = SIMDInstructionInfo(0x10).prefix(); \
		const xRegisterSSE* psrc = &src2; \
		const xRegisterSSE* pdst = &dst; \
		if (x86Emitter::use_avx && src2.IsExtended() && !dst.IsExtended()) \
		{ \
			op.opcode = 0x11; \
			std::swap(psrc, pdst); \
		} \
		EmitSIMD(op, *pdst, src1, *psrc); \
	} \
	__fi void xMOV##ssd##ZX(const xRegisterSSE& dst, const xIndirectVoid& src) { EmitSIMD(SIMDInstructionInfo(0x10).prefix().mov(), dst, dst, src); } \
	__fi void xMOV##ssd    (const xIndirectVoid& dst, const xRegisterSSE& src) { EmitSIMD(SIMDInstructionInfo(0x11).prefix().mov(), src, src, dst); }

	IMPLEMENT_xMOVS(SS, pf3)
	IMPLEMENT_xMOVS(SD, pf2)

	__fi void xMOVNTDQA(const xRegisterSSE& dst, const xIndirectVoid& src) { EmitSIMD(SIMDInstructionInfo(0x2a).p66().m0f38().mov(), dst, dst, src); }
	__fi void xMOVNTDQA(const xIndirectVoid& dst, const xRegisterSSE& src) { EmitSIMD(SIMDInstructionInfo(0xe7).p66().mov(),         src, src, dst); }

	__fi void xMOVNTPD(const xIndirectVoid& dst, const xRegisterSSE& src) { EmitSIMD(SIMDInstructionInfo(0x2b).p66().mov(), src, src, dst); }
	__fi void xMOVNTPS(const xIndirectVoid& dst, const xRegisterSSE& src) { EmitSIMD(SIMDInstructionInfo(0x2b).mov(),       src, src, dst); }

	// ------------------------------------------------------------------------

	__fi void xMOVMSKPS(const xRegister32& dst, const xRegisterSSE& src) { EmitSIMD(SIMDInstructionInfo(0x50).mov(),       dst, dst, src); }
	__fi void xMOVMSKPD(const xRegister32& dst, const xRegisterSSE& src) { EmitSIMD(SIMDInstructionInfo(0x50).mov().p66(), dst, dst, src); }

	// xMASKMOV:
	// Selectively write bytes from mm1/xmm1 to memory location using the byte mask in mm2/xmm2.
	// The default memory location is specified by DS:EDI.  The most significant bit in each byte
	// of the mask operand determines whether the corresponding byte in the source operand is
	// written to the corresponding byte location in memory.
	__fi void xMASKMOV(const xRegisterSSE& dst, const xRegisterSSE& src) { EmitSIMD(SIMDInstructionInfo(0xf7).mov().p66(), dst, dst, src); }

	// xPMOVMSKB:
	// Creates a mask made up of the most significant bit of each byte of the source
	// operand and stores the result in the low byte or word of the destination operand.
	// Upper bits of the destination are cleared to zero.
	//
	// When operating on a 64-bit (MMX) source, the byte mask is 8 bits; when operating on
	// 128-bit (SSE) source, the byte mask is 16-bits.
	//
	__fi void xPMOVMSKB(const xRegister32or64& dst, const xRegisterSSE& src) { EmitSIMD(SIMDInstructionInfo(0xd7).mov().p66(), dst, dst, src); }

	// [sSSE-3] Concatenates dest and source operands into an intermediate composite,
	// shifts the composite at byte granularity to the right by a constant immediate,
	// and extracts the right-aligned result into the destination.
	//
	__fi void xPALIGNR(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE& src2, u8 imm8) { EmitSIMD(SIMDInstructionInfo(0x0f).i().p66().m0f3a(), dst, src1, src2, imm8); }


	// --------------------------------------------------------------------------------------
	//  INSERTPS / EXTRACTPS   [SSE4.1 only!]
	// --------------------------------------------------------------------------------------
	// [TODO] these might be served better as classes, especially if other instructions use
	// the M32,sse,imm form (I forget offhand if any do).


	// [SSE-4.1] Insert a single-precision floating-point value from src into a specified
	// location in dest, and selectively zero out the data elements in dest according to
	// the mask  field in the immediate byte. The source operand can be a memory location
	// (32 bits) or an XMM register (lower 32 bits used).
	//
	// Imm8 provides three fields:
	//  * COUNT_S: The value of Imm8[7:6] selects the dword element from src.  It is 0 if
	//    the source is a memory operand.
	//  * COUNT_D: The value of Imm8[5:4] selects the target dword element in dest.
	//  * ZMASK: Each bit of Imm8[3:0] selects a dword element in dest to  be written
	//    with 0.0 if set to 1.
	//
	__emitinline void xINSERTPS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xRegisterSSE& src2, u8 imm8) { EmitSIMD(SIMDInstructionInfo(0x21).p66().m0f3a(), dst, src1, src2, imm8); }
	__emitinline void xINSERTPS(const xRegisterSSE& dst, const xRegisterSSE& src1, const xIndirect32&  src2, u8 imm8) { EmitSIMD(SIMDInstructionInfo(0x21).p66().m0f3a(), dst, src1, src2, imm8); }

	// [SSE-4.1] Extract a single-precision floating-point value from src at an offset
	// determined by imm8[1-0]*32. The extracted single precision floating-point value
	// is stored into the low 32-bits of dest (or at a 32-bit memory pointer).
	//
	__emitinline void xEXTRACTPS(const xRegister32& dst, const xRegisterSSE& src, u8 imm8) { EmitSIMD(SIMDInstructionInfo(0x17).mov().p66().m0f3a(), src, src, dst, imm8); }
	__emitinline void xEXTRACTPS(const xIndirect32& dst, const xRegisterSSE& src, u8 imm8) { EmitSIMD(SIMDInstructionInfo(0x17).mov().p66().m0f3a(), src, src, dst, imm8); }


	// =====================================================================================================
	//  Ungrouped Instructions!
	// =====================================================================================================


	// Store Streaming SIMD Extension Control/Status to Mem32.
	__emitinline void xSTMXCSR(const xIndirect32& dest)
	{
		xOpWrite0F(0, 0xae, 3, dest);
	}

	// Load Streaming SIMD Extension Control/Status from Mem32.
	__emitinline void xLDMXCSR(const xIndirect32& src)
	{
		xOpWrite0F(0, 0xae, 2, src);
	}

	// Save x87 FPU, MMX Technology, and SSE State to buffer
	// Target buffer must be at least 512 bytes in length to hold the result.
	__emitinline void xFXSAVE(const xIndirectVoid& dest)
	{
		xOpWrite0F(0, 0xae, 0, dest);
	}

	// Restore x87 FPU, MMX , XMM, and MXCSR State.
	// Source buffer should be 512 bytes in length.
	__emitinline void xFXRSTOR(const xIndirectVoid& src)
	{
		xOpWrite0F(0, 0xae, 1, src);
	}

	void xVZEROUPPER()
	{
		// rather than dealing with nonexistant operands..
		xWrite8(0xc5);
		xWrite8(0xf8);
		xWrite8(0x77);
	}
} // namespace x86Emitter
