/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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
#include <fstream>
#include <iostream>
#include <iomanip>
#include "FixedPointTypes.inl"
#include "ghc/filesystem.h"

namespace fs = ghc::filesystem;

namespace FileUtils
{
    bool IsRelative(const fs::path& path);
    bool IsDirectoryWithinDirectory(const fs::path& base, const fs::path& dir);
    // Returns -1 if the file does not exist.
    u64 GetFileSize(const fs::path& file);
    fs::path Normalize(const fs::path& srcpath);
    fs::path Combine(const fs::path& srcPath, const fs::path& srcFile);
    fs::path ReplaceFilename(const fs::path& src, const fs::path& newfilename);
    fs::path GetFilename(const fs::path& src);
    fs::path GetDirectory(const fs::path& src);
    fs::path GetFilenameWithoutExt(const fs::path& src);
    fs::path GetRootDirectory(const fs::path& src);
    fs::path GetExecutableDirectory();
    wxString ToWxString(const fs::path&);
    bool Rmdir(const fs::path& path);
    bool Mkdir(const fs::path& path);
    fs::path FromWxString(const wxString&);
} // namespace Path
