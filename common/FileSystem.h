// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once
#include "Pcsx2Defs.h"
#include <cstdio>
#include <ctime>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <sys/stat.h>

class Error;

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

	/// Returns the last modified timestamp for a file.
	std::optional<std::time_t> GetFileTimestamp(const char* path);

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

	/// Deleter functor for managed file pointers
	struct FileDeleter
	{
		void operator()(std::FILE* fp)
		{
			std::fclose(fp);
		}
	};

	/// open files
	using ManagedCFilePtr = std::unique_ptr<std::FILE, FileDeleter>;
	ManagedCFilePtr OpenManagedCFile(const char* filename, const char* mode, Error* error = nullptr);
	std::FILE* OpenCFile(const char* filename, const char* mode, Error* error = nullptr);
	int FSeek64(std::FILE* fp, s64 offset, int whence);
	s64 FTell64(std::FILE* fp);
	s64 FSize64(std::FILE* fp);

	int OpenFDFile(const char* filename, int flags, int mode, Error* error = nullptr);

	/// Sharing modes for OpenSharedCFile().
	enum class FileShareMode
	{
		DenyReadWrite, /// Exclusive access.
		DenyWrite, /// Other processes can read from this file.
		DenyRead, /// Other processes can write to this file.
		DenyNone, /// Other processes can read and write to this file.
	};

	/// Opens a file in shareable mode (where other processes can access it concurrently).
	/// Only has an effect on Windows systems.
	ManagedCFilePtr OpenManagedSharedCFile(const char* filename, const char* mode, FileShareMode share_mode, Error* error = nullptr);
	std::FILE* OpenSharedCFile(const char* filename, const char* mode, FileShareMode share_mode, Error* error = nullptr);

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

	/// Abstracts a POSIX file lock.
#ifndef _WIN32
	class POSIXLock
	{
	public:
		POSIXLock(int fd);
		POSIXLock(std::FILE* fp);
		~POSIXLock();

	private:
		int m_fd;
	};
#endif
}; // namespace FileSystem
