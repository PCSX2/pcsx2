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

#include "fmt/format.h"

#include <iostream>
#include <fstream>

namespace fs = ghc::filesystem;

template <>
struct fmt::formatter<fs::path> : formatter<string_view>
{
	// parse is inherited from formatter<string_view>.
	template <typename FormatContext>
	auto format(fs::path path, FormatContext& ctx)
	{
#ifdef _WIN32
		return formatter<string_view>::format(StringUtils::UTF8::narrow(path.wstring()), ctx);
#else
		return formatter<string_view>::format(path.string(), ctx);
#endif
	}
};

namespace FileUtils
{
	/**
	 * @brief Creates an input/output file stream
	 * @param file_path Relevant file-path
	 * @param mode Defaults to std::ios::in | std::ios::out
	 * @param create_if_nonexistant Will create the file if it does not already exist at the provided path
	 * @return std::filesystem fstream with the provided mode.
	*/
	std::fstream fileStream(const fs::path& file_path, std::ios_base::openmode mode = std::ios::in | std::ios::out, bool create_if_nonexistant = false);

	/**
	 * @brief Creates an input/output binary file stream
	 * @param file_path Relevant file-path
	 * @param create_if_nonexistant Will create the file if it does not already exist at the provided path
	 * @return std::filesystem fstream in binary mode
	*/
	std::fstream binaryFileStream(const fs::path& file_path, bool create_if_nonexistant = false);

	/**
	 * @brief Creates an input binary file stream
	 * @param file_path Relevant file-path
	 * @return std::filesystem ifstream in binary mode
	*/
	std::ifstream binaryFileInputStream(const fs::path& file_path);

	/**
	 * @brief Creates an output binary file stream
	 * @param file_path Relevant file-path
	 * @return std::filesystem ofstream in binary mode
	*/
	std::ofstream binaryFileOutputStream(const fs::path& file_path);

	/**
	 * @brief Appends a string to the end of the current file name (preceeding the extension)
	 * @param file_path The current file/file-path
	 * @param utf8_str The string to append to the current file name
	 * @return New file-path with the appropriate changes
	*/
	fs::path appendToFilename(fs::path file_path, const std::string& utf8_str);

	/**
	 * @brief Helper function to backup a file in it's current directory (appends .bak)
	 * @param file_path File path
	*/
	void backupFileIfExists(const fs::path& file_path);

	// --- wxWidgets Conversions

	/**
	 * @brief Convert wxString to a std::filesystem path, handles unicode appropriately.
	 * @param file_path File-path as a wxString
	 * @return std::filesystem path
	*/
	fs::path wxStringToPath(const wxString& file_path);

	/**
	 * @brief Convert a std::filesystem path to an wxString, handles unicode appropriately.
	 * @param file_path std::filesystem path
	 * @return wxString representation of the path
	*/
	wxString wxStringFromPath(const fs::path& file_path);
} // namespace FileUtils
