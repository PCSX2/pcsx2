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

#include "ghc/filesystem.h"
#include "wx/string.h"

#include <iostream>
#include <fstream>

namespace fs = ghc::filesystem;

namespace FileUtils
{
	/**
	 * @brief TODO doxygen comments!
	 * @param file_path 
	 * @param mode 
	 * @return 
	*/
	std::fstream fileStream(const fs::path& file_path, std::ios_base::openmode mode = std::ios::in | std::ios::out, bool create_if_nonexistant = false);
	std::fstream binaryFileStream(const fs::path& file_path, bool create_if_nonexistant = false);

	std::ifstream fileInputStream(const fs::path& file_path, std::ios_base::openmode mode = std::ios::in);
	std::ifstream binaryFileInputStream(const fs::path& file_path);

	std::ofstream fileOutputStream(const fs::path& file_path, std::ios_base::openmode mode = std::ios::out);
	std::ofstream binaryFileOutputStream(const fs::path& file_path);

	fs::path appendToFilename(fs::path file_path, const std::string& utf8_str);
	void backupFileIfExists(const fs::path& file_path);

	// --- wxWidgets Conversions

	fs::path wxStringToPath(const wxString& file_path);
	wxString wxStringFromPath(const fs::path& file_path);
} // namespace FileUtils