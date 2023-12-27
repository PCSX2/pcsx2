// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#include "common/emitter/legacy_internal.h"

//------------------------------------------------------------------
// FPU instructions
//------------------------------------------------------------------
/* fld m32 to fpu reg stack */
emitterT void FLD32(u32 from)
{
	xWrite8(0xD9);
	ModRM(0, 0x0, DISP32);
	xWrite32(MEMADDR(from, 4));
}

// fld st(i)
emitterT void FLD(int st) { xWrite16(0xc0d9 + (st << 8)); }
emitterT void FLD1() { xWrite16(0xe8d9); }
emitterT void FLDL2E() { xWrite16(0xead9); }

/* fstp m32 from fpu reg stack */
emitterT void FSTP32(u32 to)
{
	xWrite8(0xD9);
	ModRM(0, 0x3, DISP32);
	xWrite32(MEMADDR(to, 4));
}

// fstp st(i)
emitterT void FSTP(int st) { xWrite16(0xd8dd + (st << 8)); }

emitterT void FRNDINT() { xWrite16(0xfcd9); }
emitterT void FXCH(int st) { xWrite16(0xc8d9 + (st << 8)); }
emitterT void F2XM1() { xWrite16(0xf0d9); }
emitterT void FSCALE() { xWrite16(0xfdd9); }
emitterT void FPATAN(void) { xWrite16(0xf3d9); }
emitterT void FSIN(void) { xWrite16(0xfed9); }

/* fadd ST(0) to fpu reg stack ST(src) */
emitterT void FADD320toR(x86IntRegType src)
{
	xWrite8(0xDC);
	xWrite8(0xC0 + src);
}

/* fsub ST(src) to fpu reg stack ST(0) */
emitterT void FSUB32Rto0(x86IntRegType src)
{
	xWrite8(0xD8);
	xWrite8(0xE0 + src);
}

/* fmul m32 to fpu reg stack */
emitterT void FMUL32(u32 from)
{
	xWrite8(0xD8);
	ModRM(0, 0x1, DISP32);
	xWrite32(MEMADDR(from, 4));
}
