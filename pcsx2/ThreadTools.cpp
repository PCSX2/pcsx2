/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2008  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *	Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include "Misc.h"

////////////////////////////////////////////////////////////
// Cross-platform atomic operations for GCC
#ifndef _WIN32

typedef void* PVOID;

/*inline unsigned long _Atomic_swap(unsigned long * __p, unsigned long __q) {
 #       if __mips < 3 || !(defined (_ABIN32) || defined(_ABI64))
             return test_and_set(__p, __q);
 #       else
             return __test_and_set(__p, (unsigned long)__q);
 #       endif
 }*/
__forceinline void InterlockedExchangePointer(PVOID volatile* Target, void* Value)
{
#ifdef __x86_64__
	__asm__ __volatile__(".intel_syntax\n"
						 "lock xchg [%0], %%rax\n"
						 ".att_syntax\n" : : "r"(Target), "a"(Value) : "memory" );
#else
	__asm__ __volatile__(".intel_syntax\n"
						 "lock xchg [%0], %%eax\n"
						 ".att_syntax\n" : : "r"(Target), "a"(Value) : "memory" );
#endif
}

__forceinline long InterlockedExchange(long volatile* Target, long Value)
{
	__asm__ __volatile__(".intel_syntax\n"
						 "lock xchg [%0], %%eax\n"
						 ".att_syntax\n" : : "r"(Target), "a"(Value) : "memory" );
	return 0; // The only function that even looks at this is a debugging function
}

__forceinline long InterlockedExchangeAdd(long volatile* Addend, long Value)
{
	__asm__ __volatile__(".intel_syntax\n"
						 "lock xadd [%0], %%eax\n"
						 ".att_syntax\n" : : "r"(Addend), "a"(Value) : "memory" );
	return 0; // The return value is never looked at.
}

__forceinline long InterlockedIncrement( volatile long* Addend )
{
	return InterlockedExchangeAdd( Addend, 1 );
}

__forceinline long InterlockedDecrement( volatile long* Addend )
{
	return InterlockedExchangeAdd( Addend, -1 );
}

__forceinline long InterlockedCompareExchange(volatile long *dest, long exch, long comp)
{
	long old;

#ifdef __x86_64__
	  __asm__ __volatile__ 
	(
		"lock; cmpxchgq %q2, %1"
		: "=a" (old), "=m" (*dest)
		: "r" (exch), "m" (*dest), "0" (comp)); 
#else
	__asm__ __volatile__
	(
		"lock; cmpxchgl %2, %0"
		: "=m" (*dest), "=a" (old)
		: "r" (exch), "m" (*dest), "a" (comp)
	);
#endif
	
	return(old);
}

__forceinline long InterlockedCompareExchangePointer(PVOID volatile *dest, PVOID exch, long comp)
{
	long old;

#ifdef __x86_64__
	__asm__ __volatile__
	( 
		"lock; cmpxchgq %q2, %1"
		: "=a" (old), "=m" (*dest)
		: "r" (exch), "m" (*dest), "0" (comp)
	);
#else
	__asm__ __volatile__
	(
		"lock; cmpxchgl %2, %0"
		: "=m" (*dest), "=a" (old)
		: "r" (exch), "m" (*dest), "a" (comp)
	);
#endif
	return(old);
}
#endif

//////////////////////////////////////////////////////////////////
// define some overloads for InterlockedExchanges
// for commonly used types, like u32 and s32.

__forceinline void AtomicExchange( u32& Target, u32 value )
{
	InterlockedExchange( (volatile LONG*)&Target, value );
}

__forceinline void AtomicExchangeAdd( u32& Target, u32 value )
{
	InterlockedExchangeAdd( (volatile LONG*)&Target, value );
}

__forceinline void AtomicIncrement( u32& Target )
{
	InterlockedIncrement( (volatile LONG*)&Target );
}

__forceinline void AtomicDecrement( u32& Target )
{
	InterlockedDecrement( (volatile LONG*)&Target );
}

__forceinline void AtomicExchange( s32& Target, s32 value )
{
	InterlockedExchange( (volatile LONG*)&Target, value );
}

__forceinline void AtomicExchangeAdd( s32& Target, u32 value )
{
	InterlockedExchangeAdd( (volatile LONG*)&Target, value );
}

__forceinline void AtomicIncrement( s32& Target )
{
	InterlockedIncrement( (volatile LONG*)&Target );
}

__forceinline void AtomicDecrement( s32& Target )
{
	InterlockedDecrement( (volatile LONG*)&Target );
}


__forceinline void _TIMESLICE()
{
#ifdef _WIN32
	    Sleep(0);
#else
	    usleep(500);
#endif
}
