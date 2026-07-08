/// @ref gtx_color_space_YCoCg
/// @file glm/gtx/color_space_YCoCg.inl

namespace glm
{
	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec3<T, P> rgb2YCoCg
	(
		tvec3<T, P> const & rgbColor
	)
	{
		tvec3<T, P> result;
		result.x/*Y */ =   rgbColor.r / T(4) + rgbColor.g / T(2) + rgbColor.b / T(4);
		result.y/*Co*/ =   rgbColor.r / T(2) + rgbColor.g * T(0) - rgbColor.b / T(2);
		result.z/*Cg*/ = - rgbColor.r / T(4) + rgbColor.g / T(2) - rgbColor.b / T(4);
		return result;
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec3<T, P> YCoCg2rgb
	(
		tvec3<T, P> const & YCoCgColor
	)
	{
		tvec3<T, P> result;
		result.r = YCoCgColor.x + YCoCgColor.y - YCoCgColor.z;
		result.g = YCoCgColor.x				   + YCoCgColor.z;
		result.b = YCoCgColor.x - YCoCgColor.y - YCoCgColor.z;
		return result;
	}

	template <typename T, precision P, bool isInteger>
	class compute_YCoCgR {
	public:
		static GLM_FUNC_QUALIFIER tvec3<T, P> rgb2YCoCgR
		(
			tvec3<T, P> const & rgbColor
		)
		{
			tvec3<T, P> result;
			result.x/*Y */ = rgbColor.g / T(2) + (rgbColor.r + rgbColor.b) / T(4);
			result.y/*Co*/ = rgbColor.r - rgbColor.b;
			result.z/*Cg*/ = rgbColor.g - (rgbColor.r + rgbColor.b) / T(2);
			return result;
		}

		static GLM_FUNC_QUALIFIER tvec3<T, P> YCoCgR2rgb
		(
			tvec3<T, P> const & YCoCgRColor
		)
		{
			tvec3<T, P> result;
			T tmp = YCoCgRColor.x - (YCoCgRColor.z / T(2));
			result.g = YCoCgRColor.z + tmp;
			result.b = tmp - (YCoCgRColor.y / T(2));
			result.r = result.b + YCoCgRColor.y;
			return result;
		}
	};

	template <typename T, precision P>
	class compute_YCoCgR<T, P, true> {
	public:
		static GLM_FUNC_QUALIFIER tvec3<T, P> rgb2YCoCgR
		(
			tvec3<T, P> const & rgbColor
		)
		{
			tvec3<T, P> result;
			result.y/*Co*/ = rgbColor.r - rgbColor.b;
			T tmp = rgbColor.b + (result.y >> 1);
			result.z/*Cg*/ = rgbColor.g - tmp;
			result.x/*Y */ = tmp + (result.z >> 1);
			return result;
		}

		static GLM_FUNC_QUALIFIER tvec3<T, P> YCoCgR2rgb
		(
			tvec3<T, P> const & YCoCgRColor
		)
		{
			tvec3<T, P> result;
			T tmp = YCoCgRColor.x - (YCoCgRColor.z >> 1);
			result.g = YCoCgRColor.z + tmp;
			result.b = tmp - (YCoCgRColor.y >> 1);
			result.r = result.b + YCoCgRColor.y;
			return result;
		}
	};

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec3<T, P> rgb2YCoCgR
	(
		tvec3<T, P> const & rgbColor
	)
	{
		return compute_YCoCgR<T, P, std::numeric_limits<T>::is_integer>::rgb2YCoCgR(rgbColor);
	}

	template <typename T, precision P>
	GLM_FUNC_QUALIFIER tvec3<T, P> YCoCgR2rgb
	(
		tvec3<T, P> const & YCoCgRColor
	)
	{
		return compute_YCoCgR<T, P, std::numeric_limits<T>::is_integer>::YCoCgR2rgb(YCoCgRColor);
	}
}//namespace glm
