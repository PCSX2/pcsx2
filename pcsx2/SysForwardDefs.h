// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

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

#define PCSX2_WEBSITE_URL "https://pcsx2.net/"
#define PCSX2_FORUMS_URL "https://forums.pcsx2.net/"
#define PCSX2_GITHUB_URL "https://github.com/PCSX2/pcsx2"
#define PCSX2_LICENSE_URL "https://github.com/PCSX2/pcsx2/blob/master/pcsx2/Docs/License.txt"
#define PCSX2_DISCORD_URL "https://discord.com/invite/TCz3t9k"

static const bool PCSX2_isReleaseVersion = 0;
