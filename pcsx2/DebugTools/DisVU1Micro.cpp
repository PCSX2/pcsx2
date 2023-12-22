// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "Debug.h"
#include "VUmicro.h"

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
		if( !!CpuVU1->IsInterpreter ) std::snprintf(ostr, std::size(ostr), "%8.8x %8.8x:", pc, code); \
		else ostr[0] = 0; \
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

#define dName(i) std::snprintf(ostrA, ostrAL, " %-12s", i); \

#define dNameU(i) { \
	char op[256]; std::snprintf(op, std::size(op), "%s.%s%s%s%s", i, _X ? "x" : "", _Y ? "y" : "", _Z ? "z" : "", _W ? "w" : ""); \
	std::snprintf(ostrA, ostrAL, " %-12s", op); \
}

#define dCP2128f(i) { \
	if( !CpuVU1->IsInterpreter ) std::snprintf(ostrA, ostrAL, " %s,", R5900::COP2_REG_FP[i]); \
	else std::snprintf(ostrA, ostrAL, " w=%f (%8.8x) z=%f (%8.8x) y=%f (%8.8x) x=%f (%8.8x) (%s),", VU1.VF[i].f.w, VU1.VF[i].UL[3], VU1.VF[i].f.z, VU1.VF[i].UL[2], VU1.VF[i].f.y, VU1.VF[i].UL[1], VU1.VF[i].f.x, VU1.VF[i].UL[0], R5900::COP2_REG_FP[i]); \
} \

#define dCP232x(i) { \
	if( !CpuVU1->IsInterpreter ) std::snprintf(ostrA, ostrAL, " %s,", R5900::COP2_REG_FP[i]); \
	else std::snprintf(ostrA, ostrAL, " x=%f (%s),", VU1.VF[i].f.x, R5900::COP2_REG_FP[i]); \
} \

#define dCP232y(i) { \
	if( !CpuVU1->IsInterpreter ) std::snprintf(ostrA, ostrAL, " %s,", R5900::COP2_REG_FP[i]); \
	else std::snprintf(ostrA, ostrAL, " y=%f (%s),", VU1.VF[i].f.y, R5900::COP2_REG_FP[i]); \
} \

#define dCP232z(i) { \
	if( !CpuVU1->IsInterpreter ) std::snprintf(ostrA, ostrAL, " %s,", R5900::COP2_REG_FP[i]); \
	else std::snprintf(ostrA, ostrAL, " z=%f (%s),", VU1.VF[i].f.z, R5900::COP2_REG_FP[i]); \
}

#define dCP232w(i) { \
	if( !CpuVU1->IsInterpreter ) std::snprintf(ostrA, ostrAL, " %s,", R5900::COP2_REG_FP[i]); \
	else std::snprintf(ostrA, ostrAL, " w=%f (%s),", VU1.VF[i].f.w, R5900::COP2_REG_FP[i]); \
}

#define dCP2ACCf() { \
	if( !CpuVU1->IsInterpreter ) std::snprintf(ostrA, ostrAL, " ACC,"); \
	else std::snprintf(ostrA, ostrAL, " w=%f z=%f y=%f x=%f (ACC),", VU1.ACC.f.w, VU1.ACC.f.z, VU1.ACC.f.y, VU1.ACC.f.x); \
} \

#define dCP232i(i) { \
	if( !CpuVU1->IsInterpreter ) std::snprintf(ostrA, ostrAL, " %s,", R5900::COP2_REG_CTL[i]); \
	else std::snprintf(ostrA, ostrAL, " %8.8x (%s),", VU1.VI[i].UL, R5900::COP2_REG_CTL[i]); \
}

#define dCP232iF(i) { \
	if( !CpuVU1->IsInterpreter ) std::snprintf(ostrA, ostrAL, " %s,", R5900::COP2_REG_CTL[i]); \
	else std::snprintf(ostrA, ostrAL, " %f (%s),", VU1.VI[i].F, R5900::COP2_REG_CTL[i]); \
}

#define dCP232f(i, j) { \
	if( !CpuVU1->IsInterpreter ) std::snprintf(ostrA, ostrAL, " %s%s,", R5900::COP2_REG_FP[i], R5900::COP2_VFnames[j]); \
	else std::snprintf(ostrA, ostrAL, " %s=%f (%s),", R5900::COP2_VFnames[j], VU1.VF[i].F[j], R5900::COP2_REG_FP[i]); \
}

#define dImm5()			std::snprintf(ostrA, ostrAL, " %d,", (s16)((code >> 6) & 0x10 ? 0xfff0 | ((code >> 6) & 0xf) : (code >> 6) & 0xf))
#define dImm11()		std::snprintf(ostrA, ostrAL, " %d,", (s16)(code & 0x400 ? 0xfc00 | (code & 0x3ff) : code & 0x3ff))
#define dImm15()		std::snprintf(ostrA, ostrAL, " %d,", ( ( code >> 10 ) & 0x7800 ) | ( code & 0x7ff ))

#define _X ((code>>24) & 0x1)
#define _Y ((code>>23) & 0x1)
#define _Z ((code>>22) & 0x1)
#define _W ((code>>21) & 0x1)

#define _Fsf_ ((code >> 21) & 0x03)
#define _Ftf_ ((code >> 23) & 0x03)

/*********************************************************
* Unknown instruction (would generate an exception)       *
* Format:  ?                                             *
*********************************************************/
//extern char* disNULL DisFInterface;
static MakeDisF(disNULL,		dName("*** Bad OP ***");)

#include "DisVUmicro.h"
#include "DisVUops.h"

_disVUOpcodes(VU1);
_disVUTables(VU1);

