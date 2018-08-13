/*
 * context.h
 *
 * Description:
 * POSIX thread macros related to thread cancellation.
 *
 * --------------------------------------------------------------------------
 *
 *      Pthreads4w - POSIX Threads Library for Win32
 *      Copyright(C) 1998 John E. Bossom
 *      Copyright(C) 1999-2018, Pthreads4w contributors
 *
 *      Homepage: https://sourceforge.net/projects/pthreads4w/
 *
 *      The current list of contributors is contained
 *      in the file CONTRIBUTORS included with the source
 *      code distribution. The list can also be seen at the
 *      following World Wide Web location:
 *      https://sourceforge.net/p/pthreads4w/wiki/Contributors/
 *
 * This file is part of Pthreads4w.
 *
 *    Pthreads4w is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    Pthreads4w is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with Pthreads4w.  If not, see <http://www.gnu.org/licenses/>. *
 */

#ifndef PTW32_CONTEXT_H
#define PTW32_CONTEXT_H

#undef PTW32_PROGCTR

#if defined(_M_IX86) || (defined(_X86_) && !defined(__amd64__))
#define PTW32_PROGCTR(Context)  ((Context).Eip)
#endif

#if defined (_M_IA64) || defined(_IA64)
#define PTW32_PROGCTR(Context)  ((Context).StIIP)
#endif

#if defined(_MIPS_) || defined(MIPS)
#define PTW32_PROGCTR(Context)  ((Context).Fir)
#endif

#if defined(_ALPHA_)
#define PTW32_PROGCTR(Context)  ((Context).Fir)
#endif

#if defined(_PPC_)
#define PTW32_PROGCTR(Context)  ((Context).Iar)
#endif

#if defined(_AMD64_) || defined(__amd64__)
#define PTW32_PROGCTR(Context)  ((Context).Rip)
#endif

#if defined(_ARM_) || defined(ARM) || defined(_M_ARM) || defined(_M_ARM64)
#define PTW32_PROGCTR(Context)  ((Context).Pc)
#endif

#if !defined(PTW32_PROGCTR)
#error Module contains CPU-specific code; modify and recompile.
#endif

#endif
