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
#include "Debug.h"

namespace Console
{
	static HANDLE hConsole = NULL;

	static const int tbl_color_codes[] = 
	{
		0					// black
	,	FOREGROUND_RED | FOREGROUND_INTENSITY
	,	FOREGROUND_GREEN | FOREGROUND_INTENSITY
	,	FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY
	,	FOREGROUND_BLUE | FOREGROUND_INTENSITY
	,	FOREGROUND_RED | FOREGROUND_BLUE
	,	FOREGROUND_GREEN | FOREGROUND_BLUE
	,	FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY
	};

	void SetTitle( const char* title )
	{
		if( !hConsole || title==NULL ) return;
		SetConsoleTitle( title );
	}

	void Open()
	{
		COORD csize;
		CONSOLE_SCREEN_BUFFER_INFO csbiInfo; 
		SMALL_RECT srect;

		if( hConsole ) return;
		AllocConsole();
		SetConsoleTitle(_("Ps2 Output"));

		csize.X = 100;
		csize.Y = 2048;
		SetConsoleScreenBufferSize(GetStdHandle(STD_OUTPUT_HANDLE), csize);
		GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbiInfo);

		srect = csbiInfo.srWindow;
		srect.Right = srect.Left + 99;
		srect.Bottom = srect.Top + 64;
		SetConsoleWindowInfo(GetStdHandle(STD_OUTPUT_HANDLE), TRUE, &srect);
		hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	}

	void Close()
	{
		if( hConsole == NULL ) return;
		FreeConsole();
		hConsole = NULL;
	}

	__forceinline void __fastcall SetColor( Colors color )
	{
		SetConsoleTextAttribute( hConsole, tbl_color_codes[color] );
	}

	__forceinline void ClearColor()
	{
		SetConsoleTextAttribute( hConsole,
			FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE );
	}


	// Writes a newline to the console.
	__forceinline bool __fastcall WriteLn()
	{
		if (hConsole != NULL)
		{
			DWORD tmp;
			WriteConsole(hConsole, "\r\n", 2, &tmp, 0);
		}

		if (emuLog != NULL)
		{
			fputs("\r\n", emuLog);
			fflush( emuLog );
		}

		return false;
	}

	// Writes an unformatted string of text to the console (fast!)
	// No newline is appended.
	__forceinline bool __fastcall Write( const char* fmt )
	{
		if (hConsole != NULL)
		{
			DWORD tmp;
			WriteConsole(hConsole, fmt, (DWORD)strlen(fmt), &tmp, 0);
		}

		// No flushing here -- only flush after newlines.
		if (emuLog != NULL)
			fputs(fmt, emuLog);

		return false;
	}
	
	// Writes an unformatted string of text to the console (fast!)
	// A newline is automatically appended.
	__forceinline bool __fastcall WriteLn( const char* fmt )
	{
		Write( fmt );
		WriteLn();
		return false;
	}

	// Writes a formatted message to the console, with appended newline.
	static __forceinline void __fastcall _MsgLn( Colors color, const char* fmt, va_list args )
	{
		char msg[2048];

		vsprintf_s(msg,2045,fmt,args);
		strcat( msg, "\r\n" );
		SetColor( color );
		Write( msg );
		ClearColor();

		if( emuLog != NULL )
			fflush( emuLog );		// manual flush to accompany manual newline
	}

	// Writes a line of colored text to the console, with automatic newline appendage.
	bool MsgLn( Colors color, const char* fmt, ... )
	{
		va_list list;
		va_start(list,fmt);
		_MsgLn( Color_White, fmt, list );
		va_end(list);
		return false;
	}

	// writes a formatted message to the console (no newline and no color)
	bool Msg( const char* fmt, ... )
	{
		va_list list;
		char msg[2048];

		va_start(list,fmt);
		vsprintf_s(msg,fmt,list);
		msg[2047] = '\0';
		va_end(list);

		Write( msg );
		return false;
	}

	// writes a formatted message to the console (no newline and no color)
	bool Msg( Colors color, const char* fmt, ... )
	{
		va_list list;
		char msg[2048];

		va_start(list,fmt);
		vsprintf_s(msg,fmt,list);
		msg[2047] = '\0';
		va_end(list);

		SetColor( color );
		Write( msg );
		ClearColor();
		return false;
	}

	// Writes a formatted message to the console, with appended newline.
	// (no coloring)
	bool MsgLn( const char* fmt, ... )
	{
		va_list list;
		char msg[2048];

		va_start(list,fmt);
		vsprintf_s(msg,2045,fmt,list);
		va_end(list);

		strcat( msg, "\r\n" );
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
