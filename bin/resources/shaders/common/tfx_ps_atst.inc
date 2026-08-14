STATIC bool atst(IN_PARAM(PSMainState, state), FLOAT4 C)
{
	float a = C.a;
	switch (PS_ATST)
	{
		case PS_ATST_NONE:
			return true; // Nothing to do
		case PS_ATST_LEQUAL:
			return a <= state.cb.aref;
		case PS_ATST_GEQUAL:
			return a >= state.cb.aref;
		case PS_ATST_EQUAL:
			return abs(a - state.cb.aref) <= 0.5f;
		case PS_ATST_NOTEQUAL:
			return abs(a - state.cb.aref) >= 0.5f;
	}
	return true;
}
