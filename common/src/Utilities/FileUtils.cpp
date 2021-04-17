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

#include "PrecompiledHeader.h"
#include "FileUtils.h"

#include "fmt/core.h"
#include "Utilities/StringUtils.h"

std::fstream FileUtils::fileStream(const fs::path& file_path, std::ios_base::openmode mode, bool create_if_nonexistant)
{
	if (create_if_nonexistant && !fs::exists(file_path))
	{
		std::ofstream temp_stream = fileOutputStream(file_path);
		temp_stream.close();
	}

#ifdef _WIN32
	return std::fstream(file_path.wstring(), mode);
#else
	return std::fstream(file_path.string(), mode);
#endif
}

std::fstream FileUtils::binaryFileStream(const fs::path& file_path, bool create_if_nonexistant)
{
	return fileStream(file_path, std::ios::in | std::ios::out | std::ios::binary, create_if_nonexistant);
}

std::ifstream FileUtils::fileInputStream(const fs::path& file_path, std::ios_base::openmode mode)
{
#ifdef _WIN32
	return std::ifstream(file_path.wstring(), mode);
#else
	return std::ifstream(file_path.string(), mode);
#endif
}

std::ifstream FileUtils::binaryFileInputStream(const fs::path& file_path)
{
	return fileInputStream(file_path, std::ios::binary);
}

std::ofstream FileUtils::fileOutputStream(const fs::path& file_path, std::ios_base::openmode mode)
{
#ifdef _WIN32
	return std::ofstream(file_path.wstring(), mode);
#else
	return std::ofstream(file_path.string(), mode);
#endif
}

std::ofstream FileUtils::binaryFileOutputStream(const fs::path& file_path)
{
	return fileOutputStream(file_path, std::ios::binary);
}

fs::path FileUtils::appendToFilename(fs::path file_path, const std::string& utf8_str)
{
#ifdef _WIN32
	std::wstring curr_file_extension = file_path.extension().wstring();
	std::wstring new_name = file_path.filename().replace_extension().wstring() + StringUtils::UTF8::widen(utf8_str) + curr_file_extension;
	return file_path.replace_filename(fs::path(new_name));
#else
	std::string curr_file_extension = file_path.extension().wstring();
	std::string new_name = file_path.filename().replace_extension().string() +utf8_str + curr_file_extension;
	return file_path.replace_filename(fs::path(new_name));
#endif
}

void FileUtils::backupFileIfExists(const fs::path& file_path)
{
	if (fs::exists(file_path))
	{
		fs::path backup_path = file_path;
		// TODO - test with unicode extensions
		backup_path.replace_extension(fmt::format("{}.{}", file_path.extension().string(), "bak"));
		fs::copy_file(file_path, backup_path, fs::copy_options::overwrite_existing);
	}
}

fs::path FileUtils::wxStringToPath(const wxString& file_path)
{
#ifdef _WIN32
	return fs::path(file_path.ToStdWstring());
#else
	return fs::path(file_path.ToStdString());
#endif
}

wxString FileUtils::wxStringFromPath(const fs::path& file_path)
{
#ifdef _WIN32
	return wxString(file_path.wstring());
#else
	return wxString(file_path.string());
#endif
}
