// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/FileSystem.h"
#include "common/Path.h"
#include <gtest/gtest.h>

#ifdef __linux__

static std::optional<std::string> create_test_directory()
{
	for (u16 i = 0; i < UINT16_MAX; i++)
	{
		std::string path = std::string("/tmp/pcsx2_filesystem_test_") + std::to_string(i);
		if (!FileSystem::DirectoryExists(path.c_str()))
		{
			if (!FileSystem::CreateDirectoryPath(path.c_str(), false))
				break;

			return path;
		}
	}

	return std::nullopt;
}

TEST(FileSystem, RecursiveDeleteDirectoryDontFollowSymbolicLinks)
{
	// Find a suitable location to write some test files.
	std::optional<std::string> test_dir = create_test_directory();
	ASSERT_TRUE(test_dir.has_value());

	// Create a target directory containing a file that shouldn't be deleted.
	std::string target_dir = Path::Combine(*test_dir, "target_dir");
	ASSERT_TRUE(FileSystem::CreateDirectoryPath(target_dir.c_str(), false));
	std::string file_path = Path::Combine(target_dir, "file.txt");
	ASSERT_TRUE(FileSystem::WriteStringToFile(file_path.c_str(), "Lorem ipsum!"));

	// Create a directory containing a symlink to the target directory.
	std::string dir_to_delete = Path::Combine(*test_dir, "dir_to_delete");
	ASSERT_TRUE(FileSystem::CreateDirectoryPath(dir_to_delete.c_str(), false));
	std::string symlink_path = Path::Combine(dir_to_delete, "link");
	ASSERT_TRUE(FileSystem::CreateSymLink(symlink_path.c_str(), target_dir.c_str()));

	// Delete the directory containing the symlink.
	ASSERT_TRUE(dir_to_delete.starts_with("/tmp/pcsx2_filesystem_test_"));
	ASSERT_TRUE(FileSystem::RecursiveDeleteDirectory(dir_to_delete.c_str()));

	// Make sure the file in the target directory didn't get deleted.
	ASSERT_TRUE(FileSystem::FileExists(file_path.c_str()));

	// Clean up.
	ASSERT_TRUE(FileSystem::DeleteFilePath(file_path.c_str()));
	ASSERT_TRUE(FileSystem::DeleteDirectory(target_dir.c_str()));
	ASSERT_TRUE(FileSystem::DeleteDirectory(test_dir->c_str()));
}

#endif
