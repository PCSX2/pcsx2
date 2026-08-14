PS_STATIC bool atst(float4 C)
{
	float a = C.a;
	switch (PS_ATST)
	{
		case PS_ATST_NONE:
			return true; // Nothing to do
		case PS_ATST_LEQUAL:
			return a <= cb.aref;
		case PS_ATST_GEQUAL:
			return a >= cb.aref;
		case PS_ATST_EQUAL:
			return abs(a - cb.aref) <= 0.5f;
		case PS_ATST_NOTEQUAL:
			return abs(a - cb.aref) >= 0.5f;
	}
	return true;
}
