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

#include "Common.h"

// This global set true by path methods when they encounter long paths.
// If it flags true it means the app isn't stable and should be terminated.
// (someday this should be replaced with proper C++ exception handling)
int g_Error_PathTooLong = FALSE;

#ifdef WIN32
static const char PathSeparator = '\\';
#else
static const char PathSeparator = '/';
#endif


int isPathRooted( const char* path )
{
	// if the first character is a backslash or period, or the second character
	// a colon, it's a safe bet we're rooted.

	if( path[0] == 0 ) return FALSE;
#ifdef WIN32
	return (path[0] == '/') || (path[0] == '\\') || (path[1] == ':');
#else
	return (path[0] == PathSeparator);
#endif
}

// Concatenates two pathnames together, inserting delimiters (backslash on win32)
// as needed! Assumes the 'dest' is allocated to at least g_MaxPath length.
void CombinePaths( char* dest, const char* srcPath, const char* srcFile )
{
	int pathlen, guesslen;
	char tmp[g_MaxPath];

	if( g_Error_PathTooLong )
	{
		// This means a previous call has already failed and given the user
		// a message.  Pcsx2 will exit as soon as it finds out, so skip
		// this operation (avoids the redundant message below)
		// [TODO] : Proper exception handling would resolve this hack!

		return;
	}

	if( srcFile == NULL || srcFile[0] == 0 )
	{
		// No source filename?  Return the path unmodified.
		if( srcPath != NULL ) strcpy( dest, srcPath );
		return;
	}

	if( isPathRooted( srcFile ) || srcPath == NULL || srcPath[0] == 0 )
	{
		// No source path?  Or source filename is rooted?
		// Return the filename unmodified.
		strcpy( dest, srcFile );
		return;
	}

	
	// strip off the srcPath's trialing backslashes (if any)
	// Note: The win32 build works better if I check for both forward and backslashes.
	// This might be a problem on Linux builds or maybe it doesn't matter?
	strcpy( tmp, srcPath );
	pathlen = strlen( tmp );
	while( pathlen > 0 && ((tmp[pathlen-1] == '\\') || (tmp[pathlen-1] == '/')) )
	{
		--pathlen;
		tmp[pathlen] = 0;
	}

	// Concatenate strings:
	guesslen = pathlen + strlen(srcFile) + 2;

	if( guesslen >= g_MaxPath )
	{
		SysMessage(
			"Pcsx2 path names are too long.  Please move or reinstall Pcsx2 to\n"
			"a location on your hard drive that has a shorter total path."
		);
		
		g_Error_PathTooLong = TRUE;
		// [TODO]: Here would be a nice place for future C++ exception handling!
	}

#ifdef WIN32
	sprintf_s( dest, g_MaxPath, "%s%c%s", tmp, PathSeparator, srcFile );
#else
	sprintf( dest, "%s%c%s", srcPath, PathSeparator, srcFile );
#endif
}
