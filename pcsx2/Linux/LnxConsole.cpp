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

#include "Linux.h"

#define COLOR_RESET		"\033[0m"


// Linux Note : The Linux Console is pretty simple.  It just dumps to the stdio!
// (no console open/close/title stuff tho, so those functions are dummies)
namespace Console
{
	static const char* tbl_color_codes[] = 
	{
		"\033[30m"		// black
	,	"\033[31m"		// red
	,	"\033[32m"		// green
	,	"\033[33m"		// yellow
	,	"\033[34m"		// blue
	,	"\033[35m"		// magenta
	,	"\033[36m"		// cyan
	,	"\033[37m"		// white!
	};

	void SetTitle( const char* title )
	{
	}

	void Open()
	{
	}

	void Close()
	{
	}

	__forceinline bool __fastcall Newline()
	{
		if (Config.PsxOut != 0)
			puts( "\n" );

		if (emuLog != NULL)
		{
			fputs("\n", emuLog);
			fflush( emuLog );
		}

		return false;
	}

	__forceinline bool __fastcall Msg( const char* fmt )
	{
		if (Config.PsxOut != 0)
			puts( fmt );

		// No flushing here -- only flush after newlines.
		if (emuLog != NULL)
			fputs(fmt, emuLog);

		return false;
	}
	
	void __fastcall SetColor( Colors color )
	{
		Msg( tbl_color_codes[color] );
	}

	void ClearColor()
	{
		Msg( COLOR_RESET );
	}

	__forceinline bool __fastcall MsgLn( const char* fmt )
	{
		Msg( fmt );
		Newline();
		return false;
	}

	static __forceinline void __fastcall _WriteLn( Colors color, const char* fmt, va_list args )
	{
		char msg[2048];

		vsnprintf(msg,2045,fmt,args);
		msg[2044] = '\0';
		strcat( msg, "\n" );
		SetColor( color );
		Msg( msg );
		ClearColor();

		if( emuLog != NULL )
			fflush( emuLog );		// manual flush to accompany manual newline
	}
	
	// Writes a line of colored text to the console, with automatic newline appendage.
	bool WriteLn( Colors color, const char* fmt, ... )
	{
		va_list list;
		va_start(list,fmt);
		_WriteLn( Color_White, fmt, list );
		va_end(list);
		return false;
	}
	
	bool Write( Colors color, const char* fmt, ... )
	{
		va_list list;
		char msg[2048];

		va_start(list,fmt);
		vsnprintf(msg,2047,fmt,list);
		msg[2047] = '\0';
		va_end(list);

		SetColor( color );
		Msg( msg );
		ClearColor();
		return false;
	}

	bool Write( const char* fmt, ... )
	{
		va_list list;
		char msg[2048];

		va_start(list,fmt);
		vsnprintf(msg,2047,fmt,list);
		msg[2047] = '\0';
		va_end(list);

		Msg( msg );
		return false;
	}

	bool WriteLn( const char* fmt, ... )
	{
		va_list list;
		char msg[2048];

		va_start(list,fmt);
		vsnprintf(msg,2046,fmt,list);	// 2046 to leave room for the newline
		va_end(list);

		strcat( msg, "\n" );		// yeah, that newline!
		Msg( msg );
		if( emuLog != NULL )
			fflush( emuLog );		// manual flush to accomany manual newline
		return false;
	}

	// Displays a message in the console with red emphasis.
	// Newline is automatically appended.
	bool Error( const char* fmt, ... )
	{
		va_list list;
		va_start(list,fmt);
		_WriteLn( Color_Red, fmt, list );
		va_end(list);
		return false;
	}

	// Displays a message in the console with yellow emphasis.
	// Newline is automatically appended.
	bool Notice( const char* fmt, ... )
	{
		va_list list;
		va_start(list,fmt);
		_WriteLn( Color_Yellow, fmt, list );
		va_end(list);
		return false;
	}

	// Displays a message in the console with green emphasis.
	// Newline is automatically appended.
	bool Status( const char* fmt, ... )
	{
		va_list list;
		va_start(list,fmt);
		_WriteLn( Color_Yellow, fmt, list );
		va_end(list);
		return false;
	}
}

