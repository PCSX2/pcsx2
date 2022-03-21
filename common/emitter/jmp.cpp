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

namespace x86Emitter
{

	void xImpl_JmpCall::operator()(const xAddressReg& absreg) const
	{
		// Jumps are always wide and don't need the rex.W
		xOpWrite(0, 0xff, isJmp ? 4 : 2, absreg.GetNonWide());
	}
	void xImpl_JmpCall::operator()(const xIndirectNative& src) const
	{
		// Jumps are always wide and don't need the rex.W
		EmitRex(0, xIndirect32(src.Base, src.Index, 1, 0));
		xWrite8(0xff);
		EmitSibMagic(isJmp ? 4 : 2, src);
	}

	const xImpl_JmpCall xJMP = {true};
	const xImpl_JmpCall xCALL = {false};


	template <typename Reg1, typename Reg2>
	void prepareRegsForFastcall(const Reg1& a1, const Reg2& a2)
	{
		if (a1.IsEmpty())
			return;

		// Make sure we don't mess up if someone tries to fastcall with a1 in arg2reg and a2 in arg1reg
		if (a2.Id != arg1reg.Id)
		{
			xMOV(Reg1(arg1reg), a1);
			if (!a2.IsEmpty())
			{
				xMOV(Reg2(arg2reg), a2);
			}
		}
		else if (a1.Id != arg2reg.Id)
		{
			xMOV(Reg2(arg2reg), a2);
			xMOV(Reg1(arg1reg), a1);
		}
		else
		{
			xPUSH(a1);
			xMOV(Reg2(arg2reg), a2);
			xPOP(Reg1(arg1reg));
		}
	}

	void xImpl_FastCall::operator()(void* f, const xRegister32& a1, const xRegister32& a2) const
	{
		prepareRegsForFastcall(a1, a2);
		uptr disp = ((uptr)xGetPtr() + 5) - (uptr)f;
		if ((sptr)disp == (s32)disp)
		{
			xCALL(f);
		}
		else
		{
			xMOV(rax, ptrNative[f]);
			xCALL(rax);
		}
	}

	void xImpl_FastCall::operator()(void* f, const xRegisterLong& a1, const xRegisterLong& a2) const
	{
		prepareRegsForFastcall(a1, a2);
		uptr disp = ((uptr)xGetPtr() + 5) - (uptr)f;
		if ((sptr)disp == (s32)disp)
		{
			xCALL(f);
		}
		else
		{
			xMOV(rax, ptrNative[f]);
			xCALL(rax);
		}
	}

	void xImpl_FastCall::operator()(void* f, u32 a1, const xRegisterLong& a2) const
	{
		if (!a2.IsEmpty())
		{
			xMOV(arg2reg, a2);
		}
		xMOV(arg1reg, a1);
		(*this)(f, arg1reg, arg2reg);
	}

	void xImpl_FastCall::operator()(void* f, void* a1) const
	{
		xLEA(arg1reg, ptr[a1]);
		(*this)(f, arg1reg, arg2reg);
	}

	void xImpl_FastCall::operator()(void* f, u32 a1, const xRegister32& a2) const
	{
		if (!a2.IsEmpty())
		{
			xMOV(arg2regd, a2);
		}
		xMOV(arg1regd, a1);
		(*this)(f, arg1regd, arg2regd);
	}

	void xImpl_FastCall::operator()(void* f, const xIndirect32& a1) const
	{
		xMOV(arg1regd, a1);
		(*this)(f, arg1regd);
	}

	void xImpl_FastCall::operator()(void* f, u32 a1, u32 a2) const
	{
		xMOV(arg1regd, a1);
		xMOV(arg2regd, a2);
		(*this)(f, arg1regd, arg2regd);
	}

	void xImpl_FastCall::operator()(const xIndirectNative& f, const xRegisterLong& a1, const xRegisterLong& a2) const
	{
		prepareRegsForFastcall(a1, a2);
		xCALL(f);
	}

	const xImpl_FastCall xFastCall = {};

	// ------------------------------------------------------------------------
	// Emits a 32 bit jump, and returns a pointer to the 32 bit displacement.
	// (displacements should be assigned relative to the end of the jump instruction,
	// or in other words *(retval+1) )
	__emitinline s32* xJcc32(JccComparisonType comparison, s32 displacement)
	{
		if (comparison == Jcc_Unconditional)
			xWrite8(0xe9);
		else
		{
			xWrite8(0x0f);
			xWrite8(0x80 | comparison);
		}
		xWrite<s32>(displacement);

		return ((s32*)xGetPtr()) - 1;
	}

	// ------------------------------------------------------------------------
	// Emits a 32 bit jump, and returns a pointer to the 8 bit displacement.
	// (displacements should be assigned relative to the end of the jump instruction,
	// or in other words *(retval+1) )
	__emitinline s8* xJcc8(JccComparisonType comparison, s8 displacement)
	{
		xWrite8((comparison == Jcc_Unconditional) ? 0xeb : (0x70 | comparison));
		xWrite<s8>(displacement);
		return (s8*)xGetPtr() - 1;
	}

	// ------------------------------------------------------------------------
	// Writes a jump at the current x86Ptr, which targets a pre-established target address.
	// (usually a backwards jump)
	//
	// slideForward - used internally by xSmartJump to indicate that the jump target is going
	// to slide forward in the event of an 8 bit displacement.
	//
	__emitinline void xJccKnownTarget(JccComparisonType comparison, const void* target, bool slideForward)
	{
		// Calculate the potential j8 displacement first, assuming an instruction length of 2:
		sptr displacement8 = (sptr)target - (sptr)(xGetPtr() + 2);

		const int slideVal = slideForward ? ((comparison == Jcc_Unconditional) ? 3 : 4) : 0;
		displacement8 -= slideVal;

		if (slideForward)
		{
			pxAssertDev(displacement8 >= 0, "Used slideForward on a backward jump; nothing to slide!");
		}

		if (is_s8(displacement8))
			xJcc8(comparison, displacement8);
		else
		{
			// Perform a 32 bit jump instead. :(
			s32* bah = xJcc32(comparison);
			sptr distance = (sptr)target - (sptr)xGetPtr();

			// This assert won't physically happen on x86 targets
			pxAssertDev(distance >= -0x80000000LL && distance < 0x80000000LL, "Jump target is too far away, needs an indirect register");

			*bah = (s32)distance;
		}
	}

	// Low-level jump instruction!  Specify a comparison type and a target in void* form, and
	// a jump (either 8 or 32 bit) is generated.
	__emitinline void xJcc(JccComparisonType comparison, const void* target)
	{
		xJccKnownTarget(comparison, target, false);
	}

	xForwardJumpBase::xForwardJumpBase(uint opsize, JccComparisonType cctype)
	{
		pxAssert(opsize == 1 || opsize == 4);
		pxAssertDev(cctype != Jcc_Unknown, "Invalid ForwardJump conditional type.");

		BasePtr = (s8*)xGetPtr() +
				  ((opsize == 1) ? 2 : // j8's are always 2 bytes.
                                   ((cctype == Jcc_Unconditional) ? 5 : 6)); // j32's are either 5 or 6 bytes

		if (opsize == 1)
			xWrite8((cctype == Jcc_Unconditional) ? 0xeb : (0x70 | cctype));
		else
		{
			if (cctype == Jcc_Unconditional)
				xWrite8(0xe9);
			else
			{
				xWrite8(0x0f);
				xWrite8(0x80 | cctype);
			}
		}

		xAdvancePtr(opsize);
	}

	void xForwardJumpBase::_setTarget(uint opsize) const
	{
		pxAssertDev(BasePtr != NULL, "");

		sptr displacement = (sptr)xGetPtr() - (sptr)BasePtr;
		if (opsize == 1)
		{
			pxAssertDev(is_s8(displacement), "Emitter Error: Invalid short jump displacement.");
			BasePtr[-1] = (s8)displacement;
		}
		else
		{
			// full displacement, no sanity checks needed :D
			((s32*)BasePtr)[-1] = displacement;
		}
	}

	// returns the inverted conditional type for this Jcc condition.  Ie, JNS will become JS.
	__fi JccComparisonType xInvertCond(JccComparisonType src)
	{
		pxAssert(src != Jcc_Unknown);
		if (Jcc_Unconditional == src)
			return Jcc_Unconditional;

		// x86 conditionals are clever!  To invert conditional types, just invert the lower bit:
		return (JccComparisonType)((int)src ^ 1);
	}
} // namespace x86Emitter
