/// @ref gtx_wrap
/// @file glm/gtx/wrap.inl

namespace glm
{
	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> clamp(vecType<T, P> const& Texcoord)
	{
		return glm::clamp(Texcoord, vecType<T, P>(0), vecType<T, P>(1));
	}

	template <typename genType>
	GLM_FUNC_QUALIFIER genType clamp(genType const & Texcoord)
	{
		return clamp(tvec1<genType, defaultp>(Texcoord)).x;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> repeat(vecType<T, P> const& Texcoord)
	{
		return glm::fract(Texcoord);
	}

	template <typename genType>
	GLM_FUNC_QUALIFIER genType repeat(genType const & Texcoord)
	{
		return repeat(tvec1<genType, defaultp>(Texcoord)).x;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> mirrorClamp(vecType<T, P> const& Texcoord)
	{
		return glm::fract(glm::abs(Texcoord));
	}

	template <typename genType>
	GLM_FUNC_QUALIFIER genType mirrorClamp(genType const & Texcoord)
	{
		return mirrorClamp(tvec1<genType, defaultp>(Texcoord)).x;
	}

	template <typename T, precision P, template <typename, precision> class vecType>
	GLM_FUNC_QUALIFIER vecType<T, P> mirrorRepeat(vecType<T, P> const& Texcoord)
	{
		vecType<T, P> const Abs = glm::abs(Texcoord);
		vecType<T, P> const Clamp = glm::mod(glm::floor(Abs), vecType<T, P>(2));
		vecType<T, P> const Floor = glm::floor(Abs);
		vecType<T, P> const Rest = Abs - Floor;
		vecType<T, P> const Mirror = Clamp + Rest;
		return mix(Rest, vecType<T, P>(1) - Rest, glm::greaterThanEqual(Mirror, vecType<T, P>(1)));
	}

	template <typename genType>
	GLM_FUNC_QUALIFIER genType mirrorRepeat(genType const& Texcoord)
	{
		return mirrorRepeat(tvec1<genType, defaultp>(Texcoord)).x;
	}
}//namespace glm
