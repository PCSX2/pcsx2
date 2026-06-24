// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#ifndef PS_ATST_NONE
#define PS_ATST_NONE 0
#define PS_ATST_LEQUAL 1
#define PS_ATST_GEQUAL 2
#define PS_ATST_EQUAL 3
#define PS_ATST_NOTEQUAL 4
#endif

#ifndef PS_ATST
#define PS_ATST 1
#endif

#include "tfx_ps_resources.hlsl"

export bool atst(float4 C)
{
	float a = C.a;
#if PS_ATST == PS_ATST_LEQUAL
	return (a <= AREF);
#elif PS_ATST == PS_ATST_GEQUAL
	return (a >= AREF);
#elif PS_ATST == PS_ATST_EQUAL
	return (abs(a - AREF) <= 0.5f);
#elif PS_ATST == PS_ATST_NOTEQUAL
	return (abs(a - AREF) >= 0.5f);
#else
	return true;
#endif
}
