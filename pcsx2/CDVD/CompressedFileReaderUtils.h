/*  PCSX2 - PS2 Emulator for PCs
*  Copyright (C) 2002-2014  PCSX2 Dev Team
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

#include "common/Pcsx2Types.h"

/////////// Some complementary utilities for zlib_indexed.c //////////

// This is ugly, but it's hard to find something which will work/compile for both
// windows and *nix and work with non-english file names.
// Maybe some day we'll convert all file related ops to wxWidgets, which means also the
// instances at zlib_indexed.h (which use plain stdio FILE*)
#ifdef _WIN32
#define PX_wfilename(name_wxstr) (name_wxstr.wc_str())
#define PX_fopen_rb(name_wxstr) (_wfopen(PX_wfilename(name_wxstr), L"rb"))
#else
#define PX_wfilename(name_wxstr) (name_wxstr.mbc_str())
#define PX_fopen_rb(name_wxstr) (fopen(PX_wfilename(name_wxstr), "rb"))
#endif

#ifdef _WIN32
#define PX_fseeko _fseeki64
#define PX_ftello _ftelli64
#define PX_off_t s64 /* __int64 */
#else
#define PX_fseeko fseeko
#define PX_ftello ftello
#define PX_off_t off_t
#endif

/////////// End of complementary utilities for zlib_indexed.c //////////
