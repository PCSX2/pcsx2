/// @ref gtx_io
/// @file glm/gtx/io.hpp
/// @author Jan P Springer (regnirpsj@gmail.com)
///
/// @see core (dependence)
/// @see gtc_matrix_access (dependence)
/// @see gtc_quaternion (dependence)
///
/// @defgroup gtx_io GLM_GTX_io
/// @ingroup gtx
/// 
/// @brief std::[w]ostream support for glm types
///
/// std::[w]ostream support for glm types + precision/width/etc. manipulators
/// based on howard hinnant's std::chrono io proposal
/// [http://home.roadrunner.com/~hinnant/bloomington/chrono_io.html]
///
/// <glm/gtx/io.hpp> needs to be included to use these functionalities.

#pragma once

// Dependency:
#include "../glm.hpp"
#include "../gtx/quaternion.hpp"

#if GLM_MESSAGES == GLM_MESSAGES_ENABLED && !defined(GLM_EXT_INCLUDED)
# pragma message("GLM: GLM_GTX_io extension included")
#endif

#include <iosfwd>  // std::basic_ostream<> (fwd)
#include <locale>  // std::locale, std::locale::facet, std::locale::id
#include <utility> // std::pair<>

namespace glm
{
	/// @addtogroup gtx_io
	/// @{

	namespace io
	{
		enum order_type { column_major, row_major};

		template <typename CTy>
		class format_punct : public std::locale::facet
		{
			typedef CTy char_type;

		public:

			static std::locale::id id;

			bool       formatted;
			unsigned   precision;
			unsigned   width;
			char_type  separator;
			char_type  delim_left;
			char_type  delim_right;
			char_type  space;
			char_type  newline;
			order_type order;

			GLM_FUNC_DECL explicit format_punct(size_t a = 0);
			GLM_FUNC_DECL explicit format_punct(format_punct const&);
		};

		template <typename CTy, typename CTr = std::char_traits<CTy> >
		class basic_state_saver {

		public:

			GLM_FUNC_DECL explicit basic_state_saver(std::basic_ios<CTy,CTr>&);
			GLM_FUNC_DECL ~basic_state_saver();

		private:

			typedef ::std::basic_ios<CTy,CTr>      state_type;
			typedef typename state_type::char_type char_type;
			typedef ::std::ios_base::fmtflags      flags_type;
			typedef ::std::streamsize              streamsize_type;
			typedef ::std::locale const            locale_type;

			state_type&     state_;
			flags_type      flags_;
			streamsize_type precision_;
			streamsize_type width_;
			char_type       fill_;
			locale_type     locale_;

			GLM_FUNC_DECL basic_state_saver& operator=(basic_state_saver const&);
		};

		typedef basic_state_saver<char>     state_saver;
		typedef basic_state_saver<wchar_t> wstate_saver;

		template <typename CTy, typename CTr = std::char_traits<CTy> >
		class basic_format_saver
		{
		public:

			GLM_FUNC_DECL explicit basic_format_saver(std::basic_ios<CTy,CTr>&);
			GLM_FUNC_DECL ~basic_format_saver();

		private:

			basic_state_saver<CTy> const bss_;

			GLM_FUNC_DECL basic_format_saver& operator=(basic_format_saver const&);
		};

		typedef basic_format_saver<char>     format_saver;
		typedef basic_format_saver<wchar_t> wformat_saver;

		struct precision
		{
			unsigned value;

			GLM_FUNC_DECL explicit precision(unsigned);
		};

		struct width
		{
			unsigned value;

			GLM_FUNC_DECL explicit width(unsigned);
		};

		template <typename CTy>
		struct delimeter
		{
			CTy value[3];

			GLM_FUNC_DECL explicit delimeter(CTy /* left */, CTy /* right */, CTy /* separator */ = ',');
		};

		struct order
		{
			order_type value;

			GLM_FUNC_DECL explicit order(order_type);
		};

		// functions, inlined (inline)

		template <typename FTy, typename CTy, typename CTr>
		FTy const& get_facet(std::basic_ios<CTy,CTr>&);
		template <typename FTy, typename CTy, typename CTr>
		std::basic_ios<CTy,CTr>& formatted(std::basic_ios<CTy,CTr>&);
		template <typename FTy, typename CTy, typename CTr>
		std::basic_ios<CTy,CTr>& unformattet(std::basic_ios<CTy,CTr>&);

		template <typename CTy, typename CTr>
		std::basic_ostream<CTy, CTr>& operator<<(std::basic_ostream<CTy, CTr>&, precision const&);
		template <typename CTy, typename CTr>
		std::basic_ostream<CTy, CTr>& operator<<(std::basic_ostream<CTy, CTr>&, width const&);
		template <typename CTy, typename CTr>
		std::basic_ostream<CTy, CTr>& operator<<(std::basic_ostream<CTy, CTr>&, delimeter<CTy> const&);
		template <typename CTy, typename CTr>
		std::basic_ostream<CTy, CTr>& operator<<(std::basic_ostream<CTy, CTr>&, order const&);
	}//namespace io

	template <typename CTy, typename CTr, typename T, precision P>
	GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tquat<T,P> const&);
	template <typename CTy, typename CTr, typename T, precision P>
	GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tvec1<T,P> const&);
	template <typename CTy, typename CTr, typename T, precision P>
	GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tvec2<T,P> const&);
	template <typename CTy, typename CTr, typename T, precision P>
	GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tvec3<T,P> const&);
	template <typename CTy, typename CTr, typename T, precision P>
	GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tvec4<T,P> const&);
	template <typename CTy, typename CTr, typename T, precision P>
	GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat2x2<T,P> const&);
	template <typename CTy, typename CTr, typename T, precision P>
	GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat2x3<T,P> const&);
	template <typename CTy, typename CTr, typename T, precision P>
	GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat2x4<T,P> const&);
	template <typename CTy, typename CTr, typename T, precision P>
	GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat3x2<T,P> const&);
	template <typename CTy, typename CTr, typename T, precision P>
	GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat3x3<T,P> const&);
	template <typename CTy, typename CTr, typename T, precision P>
	GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat3x4<T,P> const&);
	template <typename CTy, typename CTr, typename T, precision P>
	GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat4x2<T,P> const&);
	template <typename CTy, typename CTr, typename T, precision P>
	GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat4x3<T,P> const&);
	template <typename CTy, typename CTr, typename T, precision P>
	GLM_FUNC_DECL std::basic_ostream<CTy,CTr>& operator<<(std::basic_ostream<CTy,CTr>&, tmat4x4<T,P> const&);

  template <typename CTy, typename CTr, typename T, precision P>
	GLM_FUNC_DECL std::basic_ostream<CTy,CTr> & operator<<(std::basic_ostream<CTy,CTr> &,
                                                         std::pair<tmat4x4<T,P> const, tmat4x4<T,P> const> const &);

	/// @}
}//namespace glm

#include "io.inl"
