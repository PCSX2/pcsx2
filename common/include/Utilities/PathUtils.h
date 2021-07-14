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
#include "Path.h"

namespace fs = ghc::filesystem;


namespace Path
{
	/**
	 * @brief Indicates whether dir2 is contained within dir1
	 * @param dir1 Directory that may or may not contain the second directory
	 * @param dir2 The second directory
	 * @return true/false
	*/
	extern bool IsDirectoryWithinDirectory(fs::path base, fs::path dir);

	// Returns -1 if the file does not exist.
	extern s64 GetFileSize(const fs::path& file);

	extern wxString Normalize(const wxString& srcpath);
	extern wxString Normalize(const wxDirName& srcpath);

	extern fs::path Combine(const fs::path& srcPath, const fs::path& srcFile);
	extern std::string ReplaceExtension(const wxString& src, const wxString& ext);
	extern std::string ReplaceFilename(const wxString& src, const wxString& newfilename);
	extern wxString GetFilenameWithoutExt(const wxString& src);
	extern fs::path GetRootDirectory(const wxString& src);
	extern fs::path GetExecutableDirectory();
	extern wxString ToWxString(const fs::path&);
	extern fs::path FromWxString(const wxString&);
} // namespace Path
