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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "PrecompiledHeader.h"
#include "System.h"

using namespace std;

void _format_vstring( string& dest, const char* format, va_list args )
{
	int writtenCount;
	int newSize = strlen(format) * 2;
	char *buf, *out;

	while( true )
	{
		buf = new char[newSize + 1];

		// note!  vsnprintf doesn't always return consistent results on GCC.
		// We can't assume it returns -1;
		writtenCount = vsnprintf(buf, newSize, format, args);

		if (writtenCount >= newSize)
			writtenCount = -1;
		else if( writtenCount != -1 ) break;

		// Gotta try again -_-
		newSize += newSize / 2;
		delete[] buf;
	}

	buf[writtenCount] = '\0';
	cout << buf;
	delete[] buf;
}

string format_string( const char* format, ... )
{
	string joe;
	va_list args;
	va_start(args, format);
	_format_vstring( joe, format, args );
	va_end(args);
	return joe;
}

void format_string( string& dest, const char* format, ... )
{
	va_list args;
	va_start(args, format);
	_format_vstring( dest, format, args );
	va_end(args);
}
