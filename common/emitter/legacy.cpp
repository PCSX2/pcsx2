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
 * ix86 core v0.6.2
 * Authors: linuzappz <linuzappz@pcsx.net>
 *			alexey silinov
 *			goldfinger
 *			zerofrog(@gmail.com)
 *			cottonvibes(@gmail.com)
 */

//------------------------------------------------------------------
// ix86 legacy emitter functions
//------------------------------------------------------------------

#include "common/emitter/legacy_internal.h"
#include "common/Console.h"
#include <cassert>

emitterT void ModRM(uint mod, uint reg, uint rm)
{
	// Note: Following assertions are for legacy support only.
	// The new emitter performs these sanity checks during operand construction, so these
	// assertions can probably be removed once all legacy emitter code has been removed.
	pxAssert(mod < 4);
	pxAssert(reg < 8);
	pxAssert(rm < 8);
	xWrite8((mod << 6) | (reg << 3) | rm);
}

emitterT void SibSB(uint ss, uint index, uint base)
{
	// Note: Following asserts are for legacy support only.
	// The new emitter performs these sanity checks during operand construction, so these
	// assertions can probably be removed once all legacy emitter code has been removed.
	pxAssert(ss < 4);
	pxAssert(index < 8);
	pxAssert(base < 8);
	xWrite8((ss << 6) | (index << 3) | base);
}

using namespace x86Emitter;

//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////
// From here on are instructions that have NOT been implemented in the new emitter.
//////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////

emitterT u8* J8Rel(int cc, int to)
{
	xWrite8(cc);
	xWrite8(to);
	return (u8*)(x86Ptr - 1);
}

emitterT u16* J16Rel(int cc, u32 to)
{
	xWrite16(0x0F66);
	xWrite8(cc);
	xWrite16(to);
	return (u16*)(x86Ptr - 2);
}

emitterT u32* J32Rel(int cc, u32 to)
{
	xWrite8(0x0F);
	xWrite8(cc);
	xWrite32(to);
	return (u32*)(x86Ptr - 4);
}

////////////////////////////////////////////////////
emitterT void x86SetPtr(u8* ptr)
{
	x86Ptr = ptr;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Jump Label API (as rough as it might be)
//
// I don't auto-inline these because of the console logging in case of error, which tends
// to cause quite a bit of code bloat.
//
void x86SetJ8(u8* j8)
{
	u32 jump = (x86Ptr - j8) - 1;

	if (jump > 0x7f)
	{
		Console.Error("j8 greater than 0x7f!!");
		assert(0);
	}
	*j8 = (u8)jump;
}

void x86SetJ8A(u8* j8)
{
	u32 jump = (x86Ptr - j8) - 1;

	if (jump > 0x7f)
	{
		Console.Error("j8 greater than 0x7f!!");
		assert(0);
	}

	if (((uptr)x86Ptr & 0xf) > 4)
	{

		uptr newjump = jump + 16 - ((uptr)x86Ptr & 0xf);

		if (newjump <= 0x7f)
		{
			jump = newjump;
			while ((uptr)x86Ptr & 0xf)
				*x86Ptr++ = 0x90;
		}
	}
	*j8 = (u8)jump;
}

////////////////////////////////////////////////////
emitterT void x86SetJ32(u32* j32)
{
	*j32 = (x86Ptr - (u8*)j32) - 4;
}

emitterT void x86SetJ32A(u32* j32)
{
	while ((uptr)x86Ptr & 0xf)
		*x86Ptr++ = 0x90;
	x86SetJ32(j32);
}

////////////////////////////////////////////////////
emitterT void x86Align(int bytes)
{
	// forward align
	x86Ptr = (u8*)(((uptr)x86Ptr + bytes - 1) & ~(bytes - 1));
}

/********************/
/* IX86 instructions */
/********************/

////////////////////////////////////
// jump instructions				/
////////////////////////////////////

/* jmp rel8 */
emitterT u8* JMP8(u8 to)
{
	xWrite8(0xEB);
	xWrite8(to);
	return x86Ptr - 1;
}

/* jmp rel32 */
emitterT u32* JMP32(uptr to)
{
	assert((sptr)to <= 0x7fffffff && (sptr)to >= -0x7fffffff);
	xWrite8(0xE9);
	xWrite32(to);
	return (u32*)(x86Ptr - 4);
}

/* jp rel8 */
emitterT u8* JP8(u8 to)
{
	return J8Rel(0x7A, to);
}

/* jnp rel8 */
emitterT u8* JNP8(u8 to)
{
	return J8Rel(0x7B, to);
}

/* je rel8 */
emitterT u8* JE8(u8 to)
{
	return J8Rel(0x74, to);
}

/* jz rel8 */
emitterT u8* JZ8(u8 to)
{
	return J8Rel(0x74, to);
}

/* js rel8 */
emitterT u8* JS8(u8 to)
{
	return J8Rel(0x78, to);
}

/* jns rel8 */
emitterT u8* JNS8(u8 to)
{
	return J8Rel(0x79, to);
}

/* jg rel8 */
emitterT u8* JG8(u8 to)
{
	return J8Rel(0x7F, to);
}

/* jge rel8 */
emitterT u8* JGE8(u8 to)
{
	return J8Rel(0x7D, to);
}

/* jl rel8 */
emitterT u8* JL8(u8 to)
{
	return J8Rel(0x7C, to);
}

/* ja rel8 */
emitterT u8* JA8(u8 to)
{
	return J8Rel(0x77, to);
}

emitterT u8* JAE8(u8 to)
{
	return J8Rel(0x73, to);
}

/* jb rel8 */
emitterT u8* JB8(u8 to)
{
	return J8Rel(0x72, to);
}

/* jbe rel8 */
emitterT u8* JBE8(u8 to)
{
	return J8Rel(0x76, to);
}

/* jle rel8 */
emitterT u8* JLE8(u8 to)
{
	return J8Rel(0x7E, to);
}

/* jne rel8 */
emitterT u8* JNE8(u8 to)
{
	return J8Rel(0x75, to);
}

/* jnz rel8 */
emitterT u8* JNZ8(u8 to)
{
	return J8Rel(0x75, to);
}

/* jng rel8 */
emitterT u8* JNG8(u8 to)
{
	return J8Rel(0x7E, to);
}

/* jnge rel8 */
emitterT u8* JNGE8(u8 to)
{
	return J8Rel(0x7C, to);
}

/* jnl rel8 */
emitterT u8* JNL8(u8 to)
{
	return J8Rel(0x7D, to);
}

/* jnle rel8 */
emitterT u8* JNLE8(u8 to)
{
	return J8Rel(0x7F, to);
}

/* jo rel8 */
emitterT u8* JO8(u8 to)
{
	return J8Rel(0x70, to);
}

/* jno rel8 */
emitterT u8* JNO8(u8 to)
{
	return J8Rel(0x71, to);
}
// jb rel32
emitterT u32* JB32(u32 to)
{
	return J32Rel(0x82, to);
}

/* je rel32 */
emitterT u32* JE32(u32 to)
{
	return J32Rel(0x84, to);
}

/* jz rel32 */
emitterT u32* JZ32(u32 to)
{
	return J32Rel(0x84, to);
}

/* js rel32 */
emitterT u32* JS32(u32 to)
{
	return J32Rel(0x88, to);
}

/* jns rel32 */
emitterT u32* JNS32(u32 to)
{
	return J32Rel(0x89, to);
}

/* jg rel32 */
emitterT u32* JG32(u32 to)
{
	return J32Rel(0x8F, to);
}

/* jge rel32 */
emitterT u32* JGE32(u32 to)
{
	return J32Rel(0x8D, to);
}

/* jl rel32 */
emitterT u32* JL32(u32 to)
{
	return J32Rel(0x8C, to);
}

/* jle rel32 */
emitterT u32* JLE32(u32 to)
{
	return J32Rel(0x8E, to);
}

/* ja rel32 */
emitterT u32* JA32(u32 to)
{
	return J32Rel(0x87, to);
}

/* jae rel32 */
emitterT u32* JAE32(u32 to)
{
	return J32Rel(0x83, to);
}

/* jne rel32 */
emitterT u32* JNE32(u32 to)
{
	return J32Rel(0x85, to);
}

/* jnz rel32 */
emitterT u32* JNZ32(u32 to)
{
	return J32Rel(0x85, to);
}

/* jng rel32 */
emitterT u32* JNG32(u32 to)
{
	return J32Rel(0x8E, to);
}

/* jnge rel32 */
emitterT u32* JNGE32(u32 to)
{
	return J32Rel(0x8C, to);
}

/* jnl rel32 */
emitterT u32* JNL32(u32 to)
{
	return J32Rel(0x8D, to);
}

/* jnle rel32 */
emitterT u32* JNLE32(u32 to)
{
	return J32Rel(0x8F, to);
}

/* jo rel32 */
emitterT u32* JO32(u32 to)
{
	return J32Rel(0x80, to);
}

/* jno rel32 */
emitterT u32* JNO32(u32 to)
{
	return J32Rel(0x81, to);
}
