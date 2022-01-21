/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#define PCSX2_VersionHi     1
#define PCSX2_VersionMid    7
#define PCSX2_VersionLo     0

#define STRINGIZE2(s) #s
#define STRINGIZE(s) STRINGIZE2(s)

#define VER_FILE_DESCRIPTION_STR    "PCSX2 PS2 Emulator"
#define VER_FILE_VERSION            PCSX2_VersionHi, PCSX2_VersionMid, PCSX2_VersionLo, 0
#define VER_FILE_VERSION_STR        STRINGIZE(PCSX2_VersionHi)        \
                                    "." STRINGIZE(PCSX2_VersionMid)    \
                                    "." STRINGIZE(PCSX2_VersionLo) \
                                    "." STRINGIZE(0)    \

#define VER_PRODUCTNAME_STR         "PCSX2"
#define VER_PRODUCT_VERSION         VER_FILE_VERSION
#define VER_PRODUCT_VERSION_STR     VER_FILE_VERSION_STR
#define VER_ORIGINAL_FILENAME_STR   VER_PRODUCTNAME_STR ".exe"
#define VER_INTERNAL_NAME_STR       VER_ORIGINAL_FILENAME_STR
#define VER_COPYRIGHT_STR           "Copyright (C) 2022"

static const bool PCSX2_isReleaseVersion = 0;

class SysCoreThread;

class CpuInitializerSet;
