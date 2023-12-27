// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Debug.h"

static char ostr[1024];
#define ostrA (ostr + std::strlen(ostr))
#define ostrAL (std::size(ostr) - std::strlen(ostr))

// Type deffinition of our functions
#define DisFInterface  (u32 code, u32 pc)
#define DisFInterfaceT (u32, u32)
#define DisFInterfaceN (code, pc)

typedef char* (*TdisR5900F)DisFInterface;

// These macros are used to assemble the disassembler functions
#define MakeDisF(fn, b) \
	char* fn DisFInterface { \
		std::snprintf (ostr, std::size(ostr), "%8.8x %8.8x:", pc, code); \
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

#define dName(i)	std::snprintf(ostrA, ostrAL, " %-7s,", i)
#define dNameU(i)	{ char op[256]; std::snprintf(op, std::size(op), "%s.%s%s%s%s", i, _X ? "x" : "", _Y ? "y" : "", _Z ? "z" : "", _W ? "w" : ""); std::snprintf(ostrA, ostrAL, " %-7s,", op); }


#define dCP2128f(i)		std::snprintf(ostrA, ostrAL, " w=%f z=%f y=%f x=%f (%s),", VU0.VF[i].f.w, VU0.VF[i].f.z, VU0.VF[i].f.y, VU0.VF[i].f.x, R5900::COP2_REG_FP[i])
#define dCP232x(i)		std::snprintf(ostrA, ostrAL, " x=%f (%s),", VU0.VF[i].f.x, R5900::COP2_REG_FP[i])
#define dCP232y(i)		std::snprintf(ostrA, ostrAL, " y=%f (%s),", VU0.VF[i].f.y, R5900::COP2_REG_FP[i])
#define dCP232z(i)		std::snprintf(ostrA, ostrAL, " z=%f (%s),", VU0.VF[i].f.z, R5900::COP2_REG_FP[i])
#define dCP232w(i)		std::snprintf(ostrA, ostrAL, " w=%f (%s),", VU0.VF[i].f.w, R5900::COP2_REG_FP[i])
#define dCP2ACCf()		std::snprintf(ostrA, ostrAL, " w=%f z=%f y=%f x=%f (ACC),", VU0.ACC.f.w, VU0.ACC.f.z, VU0.ACC.f.y, VU0.ACC.f.x)
#define dCP232i(i)		std::snprintf(ostrA, ostrAL, " %8.8x (%s),", VU0.VI[i].UL, R5900::COP2_REG_CTL[i])
#define dCP232iF(i)		std::snprintf(ostrA, ostrAL, " %f (%s),", VU0.VI[i].F, R5900::COP2_REG_CTL[i])
#define dCP232f(i, j)	std::snprintf(ostrA, ostrAL, " Q %s=%f (%s),", R5900::COP2_VFnames[j], VU0.VF[i].F[j], R5900::COP2_REG_FP[i])
#define dImm5()			std::snprintf(ostrA, ostrAL, " %d,", (code >> 6) & 0x1f)
#define dImm11()		std::snprintf(ostrA, ostrAL, " %d,", code & 0x7ff)
#define dImm15()		std::snprintf(ostrA, ostrAL, " %d,", ( ( code >> 10 ) & 0x7800 ) | ( code & 0x7ff ))

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

