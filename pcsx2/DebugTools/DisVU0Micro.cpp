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


#include "PrecompiledHeader.h"

#include "Debug.h"

static char ostr[1024];
#define ostrA (ostr + strlen(ostr))

// Type deffinition of our functions
#define DisFInterface  (u32 code, u32 pc)
#define DisFInterfaceT (u32, u32)
#define DisFInterfaceN (code, pc)

typedef char* (*TdisR5900F)DisFInterface;

// These macros are used to assemble the disassembler functions
#define MakeDisF(fn, b) \
	char* fn DisFInterface { \
		sprintf (ostr, "%8.8x %8.8x:", pc, code); \
		b; \
		return ostr; \
	}

//Lower/Upper instructions can use that..
#define _Ft_ ((code >> 16) & 0x1F)  // The rt part of the instruction register
#define _Fs_ ((code >> 11) & 0x1F)  // The rd part of the instruction register
#define _Fd_ ((code >>  6) & 0x1F)  // The sa part of the instruction register
#define _It_ (_Ft_ & 15)
#define _Is_ (_Fs_ & 15)
#define _Id_ (_Fd_ & 15)

#define dName(i)	sprintf(ostrA, " %-7s,", i)
#define dNameU(i)	{ char op[256]; sprintf(op, "%s.%s%s%s%s", i, _X ? "x" : "", _Y ? "y" : "", _Z ? "z" : "", _W ? "w" : ""); sprintf(ostrA, " %-7s,", op); }


#define dCP2128f(i)		sprintf(ostrA, " w=%f z=%f y=%f x=%f (%s),", VU0.VF[i].f.w, VU0.VF[i].f.z, VU0.VF[i].f.y, VU0.VF[i].f.x, R5900::COP2_REG_FP[i])
#define dCP232x(i)		sprintf(ostrA, " x=%f (%s),", VU0.VF[i].f.x, R5900::COP2_REG_FP[i])
#define dCP232y(i)		sprintf(ostrA, " y=%f (%s),", VU0.VF[i].f.y, R5900::COP2_REG_FP[i])
#define dCP232z(i)		sprintf(ostrA, " z=%f (%s),", VU0.VF[i].f.z, R5900::COP2_REG_FP[i])
#define dCP232w(i)		sprintf(ostrA, " w=%f (%s),", VU0.VF[i].f.w, R5900::COP2_REG_FP[i])
#define dCP2ACCf()		sprintf(ostrA, " w=%f z=%f y=%f x=%f (ACC),", VU0.ACC.f.w, VU0.ACC.f.z, VU0.ACC.f.y, VU0.ACC.f.x)
#define dCP232i(i)		sprintf(ostrA, " %8.8x (%s),", VU0.VI[i].UL, R5900::COP2_REG_CTL[i])
#define dCP232iF(i)		sprintf(ostrA, " %f (%s),", VU0.VI[i].F, R5900::COP2_REG_CTL[i])
#define dCP232f(i, j)	sprintf(ostrA, " Q %s=%f (%s),", R5900::COP2_VFnames[j], VU0.VF[i].F[j], R5900::COP2_REG_FP[i])
#define dImm5()			sprintf(ostrA, " %d,", (code >> 6) & 0x1f)
#define dImm11()		sprintf(ostrA, " %d,", code & 0x7ff)
#define dImm15()		sprintf(ostrA, " %d,", ( ( code >> 10 ) & 0x7800 ) | ( code & 0x7ff ))

#define _X ((code>>24) & 0x1)
#define _Y ((code>>23) & 0x1)
#define _Z ((code>>22) & 0x1)
#define _W ((code>>21) & 0x1)

#define _Fsf_ ((code >> 21) & 0x03)
#define _Ftf_ ((code >> 23) & 0x03)


/*********************************************************
* Unknown instruction (would generate an exception)      *
* Format:  ?                                             *
*********************************************************/
//extern char* disNULL DisFInterface;
static MakeDisF(disNULL,		dName("*** Bad OP ***");)

#include "DisVUmicro.h"
#include "DisVUops.h"
#include "VU.h"

_disVUOpcodes(VU0);
_disVUTables(VU0);

