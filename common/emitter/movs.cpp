/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

/*
 * ix86 core v0.9.1
 *
 * Original Authors (v0.6.2 and prior):
 *		linuzappz <linuzappz@pcsx.net>
 *		alexey silinov
 *		goldfinger
 *		zerofrog(@gmail.com)
 *
 * Authors of v0.9.1:
 *		Jake.Stine(@gmail.com)
 *		cottonvibes(@gmail.com)
 *		sudonim(1@gmail.com)
 */

#include "common/emitter/internal.h"
#include "common/emitter/implement/helpers.h"

namespace x86Emitter
{

	void _xMovRtoR(const xRegisterInt& to, const xRegisterInt& from)
	{
		pxAssert(to.GetOperandSize() == from.GetOperandSize());

		if (to == from)
			return; // ignore redundant MOVs.

		xOpWrite(from.GetPrefix16(), from.Is8BitOp() ? 0x88 : 0x89, from, to);
	}

	void xImpl_Mov::operator()(const xRegisterInt& to, const xRegisterInt& from) const
	{
		// FIXME WTF?
		_xMovRtoR(to, from);
	}

	void xImpl_Mov::operator()(const xIndirectVoid& dest, const xRegisterInt& from) const
	{
		// mov eax has a special from when writing directly to a DISP32 address
		// (sans any register index/base registers).

		xOpWrite(from.GetPrefix16(), from.Is8BitOp() ? 0x88 : 0x89, from, dest);
	}

	void xImpl_Mov::operator()(const xRegisterInt& to, const xIndirectVoid& src) const
	{
		// mov eax has a special from when reading directly from a DISP32 address
		// (sans any register index/base registers).

		xOpWrite(to.GetPrefix16(), to.Is8BitOp() ? 0x8a : 0x8b, to, src);
	}

	void xImpl_Mov::operator()(const xIndirect64orLess& dest, sptr imm) const
	{
		switch (dest.GetOperandSize())
		{
			case 1:
				pxAssertMsg(imm == (s8)imm || imm == (u8)imm, "Immediate won't fit!");
				break;
			case 2:
				pxAssertMsg(imm == (s16)imm || imm == (u16)imm, "Immediate won't fit!");
				break;
			case 4:
				pxAssertMsg(imm == (s32)imm || imm == (u32)imm, "Immediate won't fit!");
				break;
			case 8:
				pxAssertMsg(imm == (s32)imm, "Immediate won't fit in immediate slot, go through a register!");
				break;
			default:
				pxAssertMsg(0, "Bad indirect size!");
		}
		xOpWrite(dest.GetPrefix16(), dest.Is8BitOp() ? 0xc6 : 0xc7, 0, dest, dest.GetImmSize());
		dest.xWriteImm(imm);
	}

	// preserve_flags  - set to true to disable optimizations which could alter the state of
	//   the flags (namely replacing mov reg,0 with xor).
	void xImpl_Mov::operator()(const xRegisterInt& to, sptr imm, bool preserve_flags) const
	{
		switch (to.GetOperandSize())
		{
			case 1:
				pxAssertMsg(imm == (s8)imm || imm == (u8)imm, "Immediate won't fit!");
				break;
			case 2:
				pxAssertMsg(imm == (s16)imm || imm == (u16)imm, "Immediate won't fit!");
				break;
			case 4:
				pxAssertMsg(imm == (s32)imm || imm == (u32)imm, "Immediate won't fit!");
				break;
			case 8:
				pxAssertMsg(imm == (s32)imm || imm == (u32)imm, "Immediate won't fit in immediate slot, use mov64 or lea!");
				break;
			default:
				pxAssertMsg(0, "Bad indirect size!");
		}
		const xRegisterInt& to_ = to.GetNonWide();
		if (!preserve_flags && (imm == 0))
		{
			_g1_EmitOp(G1Type_XOR, to_, to_);
		}
		else if (imm == (sptr)(u32)imm || !to.IsWide())
		{
			// Note: MOV does not have (reg16/32,imm8) forms.
			u8 opcode = (to_.Is8BitOp() ? 0xb0 : 0xb8) | to_.Id;
			xOpAccWrite(to_.GetPrefix16(), opcode, 0, to_);
			to_.xWriteImm(imm);
		}
		else
		{
			xOpWrite(to.GetPrefix16(), 0xc7, 0, to);
			to.xWriteImm(imm);
		}
	}

	const xImpl_Mov xMOV;

	void xImpl_MovImm64::operator()(const xRegister64& to, s64 imm, bool preserve_flags) const
	{
		if (imm == (u32)imm || imm == (s32)imm)
		{
			xMOV(to, imm, preserve_flags);
		}
		else
		{
			u8 opcode = 0xb8 | to.Id;
			xOpAccWrite(to.GetPrefix16(), opcode, 0, to);
			xWrite64(imm);
		}
	}

	const xImpl_MovImm64 xMOV64;

	// --------------------------------------------------------------------------------------
	//  CMOVcc
	// --------------------------------------------------------------------------------------

#define ccSane() pxAssertDev(ccType >= 0 && ccType <= 0x0f, "Invalid comparison type specifier.")

// Macro useful for trapping unwanted use of EBP.
//#define EbpAssert() pxAssert( to != ebp )
#define EbpAssert()



	void xImpl_CMov::operator()(const xRegister16or32or64& to, const xRegister16or32or64& from) const
	{
		pxAssert(to->GetOperandSize() == from->GetOperandSize());
		ccSane();
		xOpWrite0F(to->GetPrefix16(), 0x40 | ccType, to, from);
	}

	void xImpl_CMov::operator()(const xRegister16or32or64& to, const xIndirectVoid& sibsrc) const
	{
		ccSane();
		xOpWrite0F(to->GetPrefix16(), 0x40 | ccType, to, sibsrc);
	}

	//void xImpl_CMov::operator()( const xDirectOrIndirect32& to, const xDirectOrIndirect32& from ) const { ccSane(); _DoI_helpermess( *this, to, from ); }
	//void xImpl_CMov::operator()( const xDirectOrIndirect16& to, const xDirectOrIndirect16& from ) const { ccSane(); _DoI_helpermess( *this, to, from ); }

	void xImpl_Set::operator()(const xRegister8& to) const
	{
		ccSane();
		xOpWrite0F(0x90 | ccType, 0, to);
	}
	void xImpl_Set::operator()(const xIndirect8& dest) const
	{
		ccSane();
		xOpWrite0F(0x90 | ccType, 0, dest);
	}
	//void xImpl_Set::operator()( const xDirectOrIndirect8& dest ) const		{ ccSane(); _DoI_helpermess( *this, dest ); }

	void xImpl_MovExtend::operator()(const xRegister16or32or64& to, const xRegister8& from) const
	{
		EbpAssert();
		xOpWrite0F(
			(to->GetOperandSize() == 2) ? 0x66 : 0,
			SignExtend ? 0xbe : 0xb6,
			to, from);
	}

	void xImpl_MovExtend::operator()(const xRegister16or32or64& to, const xIndirect8& sibsrc) const
	{
		EbpAssert();
		xOpWrite0F(
			(to->GetOperandSize() == 2) ? 0x66 : 0,
			SignExtend ? 0xbe : 0xb6,
			to, sibsrc);
	}

	void xImpl_MovExtend::operator()(const xRegister32or64& to, const xRegister16& from) const
	{
		EbpAssert();
		xOpWrite0F(SignExtend ? 0xbf : 0xb7, to, from);
	}

	void xImpl_MovExtend::operator()(const xRegister32or64& to, const xIndirect16& sibsrc) const
	{
		EbpAssert();
		xOpWrite0F(SignExtend ? 0xbf : 0xb7, to, sibsrc);
	}

	void xImpl_MovExtend::operator()(const xRegister64& to, const xRegister32& from) const
	{
		EbpAssert();
		pxAssertMsg(SignExtend, "Use mov for 64-bit movzx");
		xOpWrite(0, 0x63, to, from);
	}

	void xImpl_MovExtend::operator()(const xRegister64& to, const xIndirect32& sibsrc) const
	{
		EbpAssert();
		pxAssertMsg(SignExtend, "Use mov for 64-bit movzx");
		xOpWrite(0, 0x63, to, sibsrc);
	}

	const xImpl_MovExtend xMOVSX = {true};
	const xImpl_MovExtend xMOVZX = {false};

	const xImpl_CMov xCMOVA = {Jcc_Above};
	const xImpl_CMov xCMOVAE = {Jcc_AboveOrEqual};
	const xImpl_CMov xCMOVB = {Jcc_Below};
	const xImpl_CMov xCMOVBE = {Jcc_BelowOrEqual};

	const xImpl_CMov xCMOVG = {Jcc_Greater};
	const xImpl_CMov xCMOVGE = {Jcc_GreaterOrEqual};
	const xImpl_CMov xCMOVL = {Jcc_Less};
	const xImpl_CMov xCMOVLE = {Jcc_LessOrEqual};

	const xImpl_CMov xCMOVZ = {Jcc_Zero};
	const xImpl_CMov xCMOVE = {Jcc_Equal};
	const xImpl_CMov xCMOVNZ = {Jcc_NotZero};
	const xImpl_CMov xCMOVNE = {Jcc_NotEqual};

	const xImpl_CMov xCMOVO = {Jcc_Overflow};
	const xImpl_CMov xCMOVNO = {Jcc_NotOverflow};
	const xImpl_CMov xCMOVC = {Jcc_Carry};
	const xImpl_CMov xCMOVNC = {Jcc_NotCarry};

	const xImpl_CMov xCMOVS = {Jcc_Signed};
	const xImpl_CMov xCMOVNS = {Jcc_Unsigned};
	const xImpl_CMov xCMOVPE = {Jcc_ParityEven};
	const xImpl_CMov xCMOVPO = {Jcc_ParityOdd};


	const xImpl_Set xSETA = {Jcc_Above};
	const xImpl_Set xSETAE = {Jcc_AboveOrEqual};
	const xImpl_Set xSETB = {Jcc_Below};
	const xImpl_Set xSETBE = {Jcc_BelowOrEqual};

	const xImpl_Set xSETG = {Jcc_Greater};
	const xImpl_Set xSETGE = {Jcc_GreaterOrEqual};
	const xImpl_Set xSETL = {Jcc_Less};
	const xImpl_Set xSETLE = {Jcc_LessOrEqual};

	const xImpl_Set xSETZ = {Jcc_Zero};
	const xImpl_Set xSETE = {Jcc_Equal};
	const xImpl_Set xSETNZ = {Jcc_NotZero};
	const xImpl_Set xSETNE = {Jcc_NotEqual};

	const xImpl_Set xSETO = {Jcc_Overflow};
	const xImpl_Set xSETNO = {Jcc_NotOverflow};
	const xImpl_Set xSETC = {Jcc_Carry};
	const xImpl_Set xSETNC = {Jcc_NotCarry};

	const xImpl_Set xSETS = {Jcc_Signed};
	const xImpl_Set xSETNS = {Jcc_Unsigned};
	const xImpl_Set xSETPE = {Jcc_ParityEven};
	const xImpl_Set xSETPO = {Jcc_ParityOdd};

} // end namespace x86Emitter
