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

#include <windows.h>

#include "Misc.h"
#include "Debug.h"
#include "System.h"

namespace Console
{
	static HANDLE hConsole = NULL;

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
	
	__forceinline bool __fastcall WriteLn( const char* fmt )
	{
		Write( fmt );
		WriteLn();
		return false;
	}

	bool Format( const char* fmt, ... )
	{
		va_list list;
		char msg[2048];

		va_start(list,fmt);
		_vsnprintf(msg,2047,fmt,list);
		msg[2047] = '\0';
		va_end(list);

		Write( msg );
		return false;
	}

	bool FormatLn( const char* fmt, ... )
	{
		va_list list;
		char msg[2048];

		va_start(list,fmt);
		_vsnprintf(msg,2045,fmt,list);
		va_end(list);

		strcat( msg, "\r\n" );
		Write( msg );
		if( emuLog != NULL )
			fflush( emuLog );		// manual flush to accomany manual newline
		return false;
	}
}
