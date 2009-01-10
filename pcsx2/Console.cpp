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

#include "PrecompiledHeader.h"

#include "System.h"
#include "DebugTools/Debug.h"

using namespace std;

const _VARG_PARAM va_arg_dummy = { 0 };

// Methods of the Console namespace not defined here are to be found in the platform
// dependent implementations in WinConsole.cpp and LnxConsole.cpp.

namespace Console
{
	static __forceinline void __fastcall _WriteLn( Colors color, const string& fmt, va_list args )
	{
		string dest;
		vssprintf( dest, fmt, args);
		SetColor( color );
		WriteLn( dest );
		ClearColor();
	}
		
	bool Write( const string& fmt, VARG_PARAM dummy, ... )
	{
		varg_assert();

		string dest;

		va_list list;
		va_start(list,dummy);
		vssprintf( dest, fmt, list);
		va_end(list);

		WriteLn( dest );
		return false;
	}

	bool Write( Colors color, const string& fmt )
	{
		SetColor( color );
		Write( fmt );
		ClearColor();
		return false;
	}

	bool Write( Colors color, const string& fmt, VARG_PARAM dummy, ... )
	{
		varg_assert();

		string dest;

		va_list list;
		va_start(list,dummy);
		vssprintf( dest, fmt, list);
		va_end(list);

		Write( dest );
		return false;
	}

	bool WriteLn( const string& fmt, VARG_PARAM dummy, ... )
	{
		varg_assert();

		string dest;

		va_list list;
		va_start(list,dummy);
		vssprintf( dest, fmt, list);
		va_end(list);

		WriteLn( dest );
		return false;
	}

	// Writes an unformatted string of text to the console (fast!)
	// A newline is automatically appended.
	__forceinline bool __fastcall WriteLn( const string& fmt )
	{
		Write( fmt );
		Newline();
		return false;
	}

	// Writes an unformatted string of text to the console (fast!)
	// A newline is automatically appended.
	__forceinline bool __fastcall WriteLn( Colors color, const string& fmt )
	{
		Write( color, fmt );
		Newline();
		return false;
	}

	// Writes a line of colored text to the console, with automatic newline appendage.
	bool WriteLn( Colors color, const string& fmt, VARG_PARAM dummy, ... )
	{
		varg_assert();

		va_list list;
		va_start(list,dummy);
		_WriteLn( Color_White, fmt, list );
		va_end(list);
		return false;
	}

	// Displays a message in the console with red emphasis.
	// Newline is automatically appended.
	bool Error( const string& fmt, VARG_PARAM dummy, ... )
	{
		varg_assert();

		va_list list;
		va_start(list,dummy);
		_WriteLn( Color_Red, fmt, list );
		va_end(list);
		return false;
	}

	// Displays a message in the console with yellow emphasis.
	// Newline is automatically appended.
	bool Notice( const string& fmt, VARG_PARAM dummy, ... )
	{
		varg_assert();

		va_list list;

		va_start(list,dummy);
		_WriteLn( Color_Yellow, fmt, list );
		va_end(list);
		return false;
	}

	// Displays a message in the console with green emphasis.
	// Newline is automatically appended.
	bool Status( const string& fmt, VARG_PARAM dummy, ... )
	{
		varg_assert();

		va_list list;
		va_start(list,dummy);
		_WriteLn( Color_Yellow, fmt, list );
		va_end(list);
		return false;
	}

	// Displays a message in the console with red emphasis.
	// Newline is automatically appended.
	bool Error( const string& fmt )
	{
		WriteLn( Color_Red, fmt );
		return false;
	}

	// Displays a message in the console with yellow emphasis.
	// Newline is automatically appended.
	bool Notice( const string& fmt )
	{
		WriteLn( Color_Yellow, fmt );
		return false;
	}

	// Displays a message in the console with green emphasis.
	// Newline is automatically appended.
	bool Status( const string& fmt )
	{
		WriteLn( Color_Yellow, fmt );
		return false;
	}

}

