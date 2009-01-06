/*  Pcsx2 - Pc Ps2 Emulator
 *  Copyright (C) 2002-2008  Pcsx2 Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#ifndef _PCSX2_STRINGUTILS_H_
#define _PCSX2_STRINGUTILS_H_

#include <string>
#include <cstdarg>

// to_string: A utility template for quick and easy inline string type conversion.
// Use to_string(intval), or to_string(float), etc.  Anything that the STL itself
// would support should be supported here. :)
template< typename T >
std::string to_string(const T& value)
{
	std::ostringstream oss;
	oss << value;
	return oss.str();
}

// when the dummy assert fails, it usually means you forgot to use the params macro.

#ifdef PCSX2_DEVBUILD
#define params va_arg_dummy,
#define dummy_assert()  // jASSUME( dummy == &va_arg_dummy );
#else
#define params va_arg_dummy,			// use null in release -- faster!
#define dummy_assert() // jASSUME( dummy == 0 );
#endif

// dummy structure used to type-guard the dummy parameter that's been inserted to
// allow us to use the va_list feature on references.
struct VARG_PARAM
{
};

extern VARG_PARAM va_arg_dummy;

extern void ssprintf(std::string& dest, const std::string& fmt, VARG_PARAM dummy, ...);
extern void vssprintf(std::string& dest, const std::string& format, va_list args);
extern std::string fmt_string( const std::string& fmt, VARG_PARAM dummy, ... );
#endif
