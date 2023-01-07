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

#pragma once

//#define SPAM_DEPRECATION_WARNINGS
#if defined(__linux__) && defined(__clang__) && defined(SPAM_DEPRECATION_WARNINGS)
#define ATTR_DEP [[deprecated]]
#else
#define ATTR_DEP
#endif

#ifdef FSCALE
# undef FSCALE // Defined in a macOS header
#endif

//------------------------------------------------------------------
// legacy jump/align functions
//------------------------------------------------------------------
ATTR_DEP extern void x86SetPtr(u8* ptr);
ATTR_DEP extern void x86SetJ8(u8* j8);
ATTR_DEP extern void x86SetJ8A(u8* j8);
ATTR_DEP extern void x86SetJ32(u32* j32);
ATTR_DEP extern void x86SetJ32A(u32* j32);
ATTR_DEP extern void x86Align(int bytes);
//------------------------------------------------------------------

////////////////////////////////////
// jump instructions              //
////////////////////////////////////

// jmp rel8
ATTR_DEP extern u8* JMP8(u8 to);
// jmp rel32
ATTR_DEP extern u32* JMP32(uptr to);
// je rel8
ATTR_DEP extern u8* JE8(u8 to);
// jz rel8
ATTR_DEP extern u8* JZ8(u8 to);
// jg rel8
ATTR_DEP extern u8* JG8(u8 to);
// jge rel8
ATTR_DEP extern u8* JGE8(u8 to);
// jns rel8
ATTR_DEP extern u8* JNS8(u8 to);
// jl rel8
ATTR_DEP extern u8* JL8(u8 to);
// jae rel8
ATTR_DEP extern u8* JAE8(u8 to);
// jb rel8
ATTR_DEP extern u8* JB8(u8 to);
// jbe rel8
ATTR_DEP extern u8* JBE8(u8 to);
// jle rel8
ATTR_DEP extern u8* JLE8(u8 to);
// jne rel8
ATTR_DEP extern u8* JNE8(u8 to);
// jnz rel8
ATTR_DEP extern u8* JNZ8(u8 to);
// jng rel8
ATTR_DEP extern u8* JNG8(u8 to);
// jnge rel8
ATTR_DEP extern u8* JNGE8(u8 to);
// jnl rel8
ATTR_DEP extern u8* JNL8(u8 to);
// jnle rel8
ATTR_DEP extern u8* JNLE8(u8 to);
// jb rel32
ATTR_DEP extern u32* JB32(u32 to);
// je rel32
ATTR_DEP extern u32* JE32(u32 to);
// jz rel32
ATTR_DEP extern u32* JZ32(u32 to);
// jg rel32
ATTR_DEP extern u32* JG32(u32 to);
// jge rel32
ATTR_DEP extern u32* JGE32(u32 to);
// jl rel32
ATTR_DEP extern u32* JL32(u32 to);
// jle rel32
ATTR_DEP extern u32* JLE32(u32 to);
// jne rel32
ATTR_DEP extern u32* JNE32(u32 to);
// jnz rel32
ATTR_DEP extern u32* JNZ32(u32 to);


//******************
// FPU instructions
//******************

//*********************
// SSE   instructions *
//*********************
ATTR_DEP extern void SSE_MAXSS_XMM_to_XMM(x86SSERegType to, x86SSERegType from);
ATTR_DEP extern void SSE_MINSS_XMM_to_XMM(x86SSERegType to, x86SSERegType from);
ATTR_DEP extern void SSE_ADDSS_XMM_to_XMM(x86SSERegType to, x86SSERegType from);
ATTR_DEP extern void SSE_SUBSS_XMM_to_XMM(x86SSERegType to, x86SSERegType from);

//*********************
//  SSE 2 Instructions*
//*********************

ATTR_DEP extern void SSE2_MAXSD_XMM_to_XMM(x86SSERegType to, x86SSERegType from);
ATTR_DEP extern void SSE2_MINSD_XMM_to_XMM(x86SSERegType to, x86SSERegType from);
ATTR_DEP extern void SSE2_ADDSD_XMM_to_XMM(x86SSERegType to, x86SSERegType from);
ATTR_DEP extern void SSE2_SUBSD_XMM_to_XMM(x86SSERegType to, x86SSERegType from);
