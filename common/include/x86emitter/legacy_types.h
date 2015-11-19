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

#if defined(__linux__) && defined(__clang__)

[[deprecated]] const int DISP32 = 5;	// maps to EBP

[[deprecated]] const int EAX = 0;
[[deprecated]] const int EBX = 3;
[[deprecated]] const int ECX = 1;
[[deprecated]] const int EDX = 2;
[[deprecated]] const int ESI = 6;
[[deprecated]] const int EDI = 7;
[[deprecated]] const int EBP = 5;
[[deprecated]] const int ESP = 4;

#else

//#define SIB 4		// maps to ESP
//#define SIBDISP 5	// maps to EBP
#define DISP32 5	// maps to EBP

#define EAX 0
#define EBX 3
#define ECX 1
#define EDX 2
#define ESI 6
#define EDI 7
#define EBP 5
#define ESP 4

#endif

// general types
typedef int x86IntRegType;
typedef int x86MMXRegType;
typedef int x86SSERegType;
