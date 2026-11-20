// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

PS_STATIC bool atst(float4 C)
{
	float a = C.a;

	__IF__ (PS_ATST == PS_ATST_NONE)
		return true; // Nothing to do
	__ENDIF__

	__IF__ (PS_ATST == PS_ATST_LEQUAL)
		return a <= cb.aref;
	__ENDIF__

	__IF__ (PS_ATST == PS_ATST_GEQUAL)
		return a >= cb.aref;
	__ENDIF__

	__IF__ (PS_ATST == PS_ATST_EQUAL)
		return abs(a - cb.aref) <= 0.5f;
	__ENDIF__

	__IF__ (PS_ATST == PS_ATST_NOTEQUAL)
		return abs(a - cb.aref) >= 0.5f;
	__ENDIF__

	return true;
}
