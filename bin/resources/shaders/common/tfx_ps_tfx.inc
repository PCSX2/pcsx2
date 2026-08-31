STATIC FLOAT4 tfx(FLOAT4 T, FLOAT4 C)
{
	FLOAT4 C_out;
	FLOAT4 FxT = trunc((C * T) / 128.f);
	if (PS_TFX == 0)
		C_out = FxT;
	else if (PS_TFX == 1)
		C_out = T;
	else if (PS_TFX == 2)
		C_out = FLOAT4(FxT.rgb, T.a) + C.a;
	else if (PS_TFX == 3)
		C_out = FLOAT4(FxT.rgb + C.a, T.a);
	else
		C_out = C;

	if (PS_TCC == FALSE)
		C_out.a = C.a;

	// Clamp only when it is useful
	if (PS_TFX == 0 || PS_TFX == 2 || PS_TFX == 3)
		C_out = min(C_out, 255.f);

	return C_out;
}
