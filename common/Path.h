/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#include "common/Pcsx2Defs.h"

#include <string>
#include <string_view>
#include <vector>

namespace Path
{
	/// Converts any forward slashes to backslashes on Win32.
	std::string ToNativePath(const std::string_view& path);
	void ToNativePath(std::string* path);

	/// Builds a path relative to the specified file
	std::string BuildRelativePath(const std::string_view& filename, const std::string_view& new_filename);

	/// Joins path components together, producing a new path.
	std::string Combine(const std::string_view& base, const std::string_view& next);

	/// Removes all .. and . components from a path.
	std::string Canonicalize(const std::string_view& path);
	void Canonicalize(std::string* path);

	/// Sanitizes a filename for use in a filesystem.
	void SanitizeFileName(char* Destination, u32 cbDestination, const char* FileName, bool StripSlashes /* = true */);
	void SanitizeFileName(std::string& Destination, bool StripSlashes = true);

	/// Returns true if the specified path is an absolute path (C:\Path on Windows or /path on Unix).
	bool IsAbsolute(const std::string_view& path);

	/// Makes the specified path relative to another (e.g. /a/b/c, /a/b -> ../c).
	/// Both paths must be relative, otherwise this function will just return the input path.
	std::string MakeRelative(const std::string_view& path, const std::string_view& relative_to);

	/// Returns a view of the extension of a filename.
	std::string_view GetExtension(const std::string_view& path);

	/// Removes the extension of a filename.
	std::string_view StripExtension(const std::string_view& path);

	/// Replaces the extension of a filename with another.
	std::string ReplaceExtension(const std::string_view& path, const std::string_view& new_extension);

	/// Returns the directory component of a filename.
	std::string_view GetDirectory(const std::string_view& path);

	/// Returns the filename component of a filename.
	std::string_view GetFileName(const std::string_view& path);

	/// Returns the file title (less the extension and path) from a filename.
	std::string_view GetFileTitle(const std::string_view& path);

	/// Changes the filename in a path.
	std::string ChangeFileName(const std::string_view& path, const std::string_view& new_filename);
	void ChangeFileName(std::string* path, const std::string_view& new_filename);

	/// Appends a directory to a path.
	std::string AppendDirectory(const std::string_view& path, const std::string_view& new_dir);
	void AppendDirectory(std::string* path, const std::string_view& new_dir);

	/// Splits a path into its components, handling both Windows and Unix separators.
	std::vector<std::string_view> SplitWindowsPath(const std::string_view& path);
	std::string JoinWindowsPath(const std::vector<std::string_view>& components);

	/// Splits a path into its components, only handling native separators.
	std::vector<std::string_view> SplitNativePath(const std::string_view& path);
	std::string JoinNativePath(const std::vector<std::string_view>& components);
} // namespace Path
