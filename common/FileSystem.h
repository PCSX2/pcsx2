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
#include "Pcsx2Defs.h"
#include <cstdio>
#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <sys/stat.h>

#ifdef _WIN32
#define FS_OSPATH_SEPARATOR_CHARACTER '\\'
#define FS_OSPATH_SEPARATOR_STR "\\"
#else
#define FS_OSPATH_SEPARATOR_CHARACTER '/'
#define FS_OSPATH_SEPARATOR_STR "/"
#endif

enum FILESYSTEM_FILE_ATTRIBUTES
{
	FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY = 1,
	FILESYSTEM_FILE_ATTRIBUTE_READ_ONLY = 2,
	FILESYSTEM_FILE_ATTRIBUTE_COMPRESSED = 4,
};

enum FILESYSTEM_FIND_FLAGS
{
	FILESYSTEM_FIND_RECURSIVE = (1 << 0),
	FILESYSTEM_FIND_RELATIVE_PATHS = (1 << 1),
	FILESYSTEM_FIND_HIDDEN_FILES = (1 << 2),
	FILESYSTEM_FIND_FOLDERS = (1 << 3),
	FILESYSTEM_FIND_FILES = (1 << 4),
	FILESYSTEM_FIND_KEEP_ARRAY = (1 << 5),
};

struct FILESYSTEM_STAT_DATA
{
	std::time_t CreationTime; // actually inode change time on linux
	std::time_t ModificationTime;
	s64 Size;
	u32 Attributes;
};

struct FILESYSTEM_FIND_DATA
{
	std::time_t CreationTime; // actually inode change time on linux
	std::time_t ModificationTime;
	std::string FileName;
	s64 Size;
	u32 Attributes;
};

namespace FileSystem
{
	using FindResultsArray = std::vector<FILESYSTEM_FIND_DATA>;

	/// Returns the display name of a filename. Usually this is the same as the path.
	std::string GetDisplayNameFromPath(const std::string_view& path);

	/// Returns a list of "root directories" (i.e. root/home directories on Linux, drive letters on Windows).
	std::vector<std::string> GetRootDirectoryList();

	/// Search for files
	bool FindFiles(const char* path, const char* pattern, u32 flags, FindResultsArray* results);

	/// Stat file
	bool StatFile(const char* path, struct stat* st);
	bool StatFile(std::FILE* fp, struct stat* st);
	bool StatFile(const char* path, FILESYSTEM_STAT_DATA* pStatData);
	bool StatFile(std::FILE* fp, FILESYSTEM_STAT_DATA* pStatData);
	s64 GetPathFileSize(const char* path);

	/// File exists?
	bool FileExists(const char* path);

	/// Directory exists?
	bool DirectoryExists(const char* path);

	/// Directory does not contain any files?
	bool DirectoryIsEmpty(const char* path);

	/// Delete file
	bool DeleteFilePath(const char* path);

	/// Rename file
	bool RenamePath(const char* OldPath, const char* NewPath);

	/// open files
	using ManagedCFilePtr = std::unique_ptr<std::FILE, void (*)(std::FILE*)>;
	ManagedCFilePtr OpenManagedCFile(const char* filename, const char* mode);
	std::FILE* OpenCFile(const char* filename, const char* mode);
	int FSeek64(std::FILE* fp, s64 offset, int whence);
	s64 FTell64(std::FILE* fp);
	s64 FSize64(std::FILE* fp);

	int OpenFDFile(const char* filename, int flags, int mode);

	std::optional<std::vector<u8>> ReadBinaryFile(const char* filename);
	std::optional<std::vector<u8>> ReadBinaryFile(std::FILE* fp);
	std::optional<std::string> ReadFileToString(const char* filename);
	std::optional<std::string> ReadFileToString(std::FILE* fp);
	bool WriteBinaryFile(const char* filename, const void* data, size_t data_length);
	bool WriteStringToFile(const char* filename, const std::string_view& sv);

	/// creates a directory in the local filesystem
	/// if the directory already exists, the return value will be true.
	/// if Recursive is specified, all parent directories will be created
	/// if they do not exist.
	bool CreateDirectoryPath(const char* path, bool recursive);

	/// Creates a directory if it doesn't already exist.
	/// Returns false if it does not exist and creation failed.
	bool EnsureDirectoryExists(const char* path, bool recursive);

	/// Removes a directory.
	bool DeleteDirectory(const char* path);

	/// Recursively removes a directory and all subdirectories/files.
	bool RecursiveDeleteDirectory(const char* path);

	/// Copies one file to another, optionally replacing it if it already exists.
	bool CopyFilePath(const char* source, const char* destination, bool replace);

	/// Returns the path to the current executable.
	std::string GetProgramPath();

	/// Retrieves the current working directory.
	std::string GetWorkingDirectory();

	/// Sets the current working directory. Returns true if successful.
	bool SetWorkingDirectory(const char* path);

	/// Enables/disables NTFS compression on a file or directory.
	/// Does not apply the compression flag recursively if called for a directory.
	/// Does nothing and returns false on non-Windows platforms.
	bool SetPathCompression(const char* path, bool enable);

}; // namespace FileSystem
