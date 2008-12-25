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

#include "Misc.h"
#include "../DebugTools/Debug.h"
#include "System.h"
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

	__forceinline bool __fastcall WriteLn()
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

	__forceinline bool __fastcall Write( const char* fmt )
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
		Write( tbl_color_codes[color] );
	}

	void ClearColor()
	{
		Write( COLOR_RESET );
	}

	__forceinline bool __fastcall WriteLn( const char* fmt )
	{
		Write( fmt );
		WriteLn();
		return false;
	}

	static __forceinline void __fastcall _MsgLn( Colors color, const char* fmt, va_list args )
	{
		char msg[2048];

		vsnprintf(msg,2045,fmt,list);
		msg[2044] = '\0';
		strcat( msg, "\n" );
		SetColor( color );
		Write( msg );
		ClearColor();

		if( emuLog != NULL )
			fflush( emuLog );		// manual flush to accompany manual newline
	}

	bool Msg( Colors color, const char* fmt, ... )
	{
		va_list list;
		char msg[2048];

		va_start(list,fmt);
		vsnprintf(msg,2047,fmt,list);
		msg[2047] = '\0';
		va_end(list);

		SetColor( color );
		Write( msg );
		ClearColor();
		return false;
	}

	bool Msg( const char* fmt, ... )
	{
		va_list list;
		char msg[2048];

		va_start(list,fmt);
		vsnprintf(msg,2047,fmt,list);
		msg[2047] = '\0';
		va_end(list);

		Write( msg );
		return false;
	}

	bool MsgLn( const char* fmt, ... )
	{
		va_list list;
		char msg[2048];

		va_start(list,fmt);
		vsnprintf(msg,2046,fmt,list);	// 2046 to leave room for the newline
		va_end(list);

		strcat( msg, "\n" );		// yeah, that newline!
		Write( msg );
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
		_MsgLn( Color_Red, fmt, list );
		va_end(list);
		return false;
	}

	// Displays a message in the console with yellow emphasis.
	// Newline is automatically appended.
	bool Notice( const char* fmt, ... )
	{
		va_list list;
		va_start(list,fmt);
		_MsgLn( Color_Yellow, fmt, list );
		va_end(list);
		return false;
	}
}

