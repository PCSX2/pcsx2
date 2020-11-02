/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <cstdio>
#include <iostream>
#include <sstream>

#define USB_LOG __Log
void __Log(const char* fmt, ...);

#ifdef _WIN32

#include <vector>
void _OSDebugOut(const TCHAR *psz_fmt, ...);
std::wostream& operator<<(std::wostream& os, const std::string& s);

#ifdef _DEBUG
#define OSDebugOut(psz_fmt, ...) _OSDebugOut(TEXT("[USBqemu] [%" SFMTs "]:%d\t") psz_fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define OSDebugOut_noprfx(psz_fmt, ...) _OSDebugOut(TEXT(psz_fmt), ##__VA_ARGS__)
#define OSDebugOutStream_noprfx(psz_str) do{ TSTDSTRINGSTREAM ss; ss << psz_str; _OSDebugOut(_T("%s\n"), ss.str().c_str()); }while(0)
#else
#define OSDebugOut(psz_fmt, ...) do{}while(0)
#define OSDebugOut_noprfx(psz_fmt, ...) do{}while(0)
#define OSDebugOutStream_noprfx(str) do{}while(0)
#endif

#else //_WIN32

#ifdef _DEBUG
#define OSDebugOut(psz_fmt, ...) do{ fprintf(stderr, "[USBqemu] [%s]:%d\t" psz_fmt, __func__, __LINE__, ##__VA_ARGS__); }while(0)
#define OSDebugOut_noprfx(psz_fmt, ...) do{ fprintf(stderr, psz_fmt, ##__VA_ARGS__); }while(0)
#define OSDebugOutStream_noprfx(str) do{ std::cerr << str << std::endl; }while(0)
#else
#define OSDebugOut(psz_fmt, ...) do{}while(0)
#define OSDebugOut_noprfx(psz_fmt, ...) do{}while(0)
#define OSDebugOutStream_noprfx(str) do{}while(0)
#endif

#endif //_WIN32
