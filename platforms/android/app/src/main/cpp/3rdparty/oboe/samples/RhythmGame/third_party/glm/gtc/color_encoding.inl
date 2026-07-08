/// @ref gtc_color_encoding
/// @file glm/gtc/color_encoding.inl

namespace glm
{
	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec3<T, P> convertLinearSRGBToD65XYZ(tvec3<T, P> const& ColorLinearSRGB)
	{
		tvec3<T, P> const M(0.490f, 0.17697f, 0.2f);
		tvec3<T, P> const N(0.31f,  0.8124f, 0.01063f);
		tvec3<T, P> const O(0.490f, 0.01f, 0.99f);

		return (M * ColorLinearSRGB + N * ColorLinearSRGB + O * ColorLinearSRGB) * static_cast<T>(5.650675255693055f);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec3<T, P> convertD65XYZToLinearSRGB(tvec3<T, P> const& ColorD65XYZ)
	{
		tvec3<T, P> const M(0.41847f, -0.091169f, 0.0009209f);
		tvec3<T, P> const N(-0.15866f, 0.25243f, 0.015708f);
		tvec3<T, P> const O(0.0009209f, -0.0025498f, 0.1786f);

		return M * ColorD65XYZ + N * ColorD65XYZ + O * ColorD65XYZ;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec3<T, P> convertLinearSRGBToD50XYZ(tvec3<T, P> const& ColorLinearSRGB)
	{
		tvec3<T, P> const M(0.436030342570117f, 0.222438466210245f, 0.013897440074263f);
		tvec3<T, P> const N(0.385101860087134f, 0.716942745571917f, 0.097076381494207f);
		tvec3<T, P> const O(0.143067806654203f, 0.060618777416563f, 0.713926257896652f);

		return M * ColorLinearSRGB + N * ColorLinearSRGB + O * ColorLinearSRGB;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec3<T, P> convertD50XYZToLinearSRGB(tvec3<T, P> const& ColorD50XYZ)
	{
		tvec3<T, P> const M();
		tvec3<T, P> const N();
		tvec3<T, P> const O();

		return M * ColorD65XYZ + N * ColorD65XYZ + O * ColorD65XYZ;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec3<T, P> convertD65XYZToD50XYZ(tvec3<T, P> const& ColorD65XYZ)
	{
		tvec3<T, P> const M(+1.047844353856414f, +0.029549007606644f, -0.009250984365223f);
		tvec3<T, P> const N(+0.022898981050086f, +0.990508028941971f, +0.015072338237051f);
		tvec3<T, P> const O(-0.050206647741605f, -0.017074711360960f, +0.751717835079977f);

		return M * ColorD65XYZ + N * ColorD65XYZ + O * ColorD65XYZ;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec3<T, P> convertD50XYZToD65XYZ(tvec3<T, P> const& ColorD50XYZ)
	{
		tvec3<T, P> const M();
		tvec3<T, P> const N();
		tvec3<T, P> const O();

		return M * ColorD50XYZ + N * ColorD50XYZ + O * ColorD50XYZ;
	}
}//namespace glm
