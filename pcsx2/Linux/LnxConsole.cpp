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
#include "Debug.h"
#include "System.h"

// Linux Note : The Linux Console is pretty simple.  It just dumps to the stdio!
// (no console open/clode/title stuff tho, so those functions are dummies)
namespace Console
{
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
		vsprintf(msg,2047,fmt,list);
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
		_vsnprintf(msg,2046,fmt,list);	// 2046 to leave room for the newline
		va_end(list);

		strcat( msg, "\n" );		// yeah, that newline!
		Write( msg );
		if( emuLog != NULL )
			fflush( emuLog );		// manual flush to accomany manual newline
		return false;
	}
}

