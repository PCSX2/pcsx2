/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021 PCSX2 Dev Team
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

using namespace Xbyak;

#ifdef _M_AMD64
// Yeah let use mips naming ;)
	#ifdef _WIN64
		#define a0 rcx
		#define a1 rdx
		#define a2 r8
		#define a3 r9
		#define t0 rdi
		#define t1 rsi
	#else
		#define a0 rdi
		#define a1 rsi
		#define a2 rdx
		#define a3 rcx
		#define t0 r8
		#define t1 r9
	#endif
#endif

