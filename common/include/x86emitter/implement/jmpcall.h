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

// Implementations found here: CALL and JMP!  (unconditional only)

namespace x86Emitter {

#ifdef __GNUG__
	// GCC has a bug that causes the templated function handler for Jmp/Call emitters to generate
	// bad asm code.  (error is something like "7#*_uber_379s_mangled_$&02_name is already defined!")
	// Using GCC's always_inline attribute fixes it.  This differs from __fi in that it
	// inlines *even in debug builds* which is (usually) undesirable.
	//  ... except when it avoids compiler bugs.

	// Note: I try with -fabi-version=6 without success
	// {standard input}: Assembler messages:
	// {standard input}:30773: Error: symbol `_ZNK10x86Emitter13xImpl_JmpCallclIFvvEEEvPT_' is already defined
	// pcsx2/CMakeFiles/PCSX2.dir/build.make:4550: recipe for target 'pcsx2/CMakeFiles/PCSX2.dir/x86/ix86-32/iR5900-32.cpp.o' failed

#	define __always_inline_tmpl_fail	__attribute__((always_inline))
#else
#	define __always_inline_tmpl_fail
#endif

extern void xJccKnownTarget( JccComparisonType comparison, const void* target, bool slideForward );

// ------------------------------------------------------------------------
struct xImpl_JmpCall
{
	bool	isJmp;

	void operator()( const xRegisterInt& absreg ) const;
	void operator()( const xIndirect32orLess& src ) const;

	// Special form for calling functions.  This form automatically resolves the
	// correct displacement based on the size of the instruction being generated.
	template< typename T > __fi __always_inline_tmpl_fail
	void operator()( T* func ) const
	{
		if( isJmp )
			xJccKnownTarget( Jcc_Unconditional, (void*)(uptr)func, false );	// double cast to/from (uptr) needed to appease GCC
		else
		{
			// calls are relative to the instruction after this one, and length is
			// always 5 bytes (16 bit calls are bad mojo, so no bother to do special logic).

			sptr dest = (sptr)func - ((sptr)xGetPtr() + 5);
			xWrite8( 0xe8 );
			xWrite32( dest );
		}
	}
};

// yes it is awful. Due to template code is in a header with a nice circular dep.
extern const xImpl_Mov			xMOV;
extern const xImpl_JmpCall		xCALL;

struct xImpl_FastCall
{
	// FIXME: current 64 bits is mostly a copy/past potentially it would require to push/pop
	// some registers. But I think it is enough to handle the first call.


	// Type unsafety is nice
#ifdef __x86_64__

#define XFASTCALL \
	xCALL(func);

#define XFASTCALL1 \
	xMOV(rdi, a1); \
	xCALL(func);

#define XFASTCALL2 \
	xMOV(rdi, a1); \
	xMOV(rsi, a2); \
	xCALL(func);

#else

#define XFASTCALL \
	xCALL(func);

#define XFASTCALL1 \
	xMOV(ecx, a1); \
	xCALL(func);

#define XFASTCALL2 \
	xMOV(ecx, a1); \
	xMOV(edx, a2); \
	xCALL(func);

#endif

	template< typename T > __fi __always_inline_tmpl_fail
	void operator()( T* func, const xRegisterLong& a1 = xEmptyReg, const xRegisterLong& a2 = xEmptyReg) const
	{
#ifdef __x86_64__
		if (a1.IsEmpty()) {
			XFASTCALL;
		} else if (a2.IsEmpty()) {
			XFASTCALL1;
		} else {
			XFASTCALL2;
		}
#else
		if (a1.IsEmpty()) {
			XFASTCALL;
		} else if (a2.IsEmpty()) {
			XFASTCALL1;
		} else {
			XFASTCALL2;
		}
#endif
	}

	template< typename T > __fi __always_inline_tmpl_fail
	void operator()( T* func, u32 a1, const xRegisterLong& a2) const
	{
#ifdef __x86_64__
		XFASTCALL2;
#else
		XFASTCALL2;
#endif
	}

	template< typename T > __fi __always_inline_tmpl_fail
	void operator()( T* func, const xIndirectVoid& a1) const
	{
#ifdef __x86_64__
		XFASTCALL1;
#else
		XFASTCALL1;
#endif
	}

	template< typename T > __fi __always_inline_tmpl_fail
	void operator()( T* func, u32 a1, u32 a2) const
	{
#ifdef __x86_64__
		XFASTCALL2;
#else
		XFASTCALL2;
#endif
	}

	template< typename T > __fi __always_inline_tmpl_fail
	void operator()( T* func, u32 a1) const
	{
#ifdef __x86_64__
		XFASTCALL1;
#else
		XFASTCALL1;
#endif
	}

	void operator()(const xIndirect32& func, const xRegisterLong& a1 = xEmptyReg, const xRegisterLong& a2 = xEmptyReg) const
	{
#ifdef __x86_64__
		if (a1.IsEmpty()) {
			XFASTCALL;
		} else if (a2.IsEmpty()) {
			XFASTCALL1;
		} else {
			XFASTCALL2;
		}
#else
		if (a1.IsEmpty()) {
			XFASTCALL;
		} else if (a2.IsEmpty()) {
			XFASTCALL1;
		} else {
			XFASTCALL2;
		}
#endif
	}

#undef XFASTCALL
#undef XFASTCALL1
#undef XFASTCALL2
};

}	// End namespace x86Emitter

