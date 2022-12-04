/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2022  PCSX2 Dev Team
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

#include "common/ProgressCallback.h"

#ifdef _WIN32
#include "7z.h"
#include "7zFile.h"
#endif

#include <string>
#include <vector>

class Updater
{
public:
	Updater(ProgressCallback* progress);
	~Updater();

	static void SetupLogging(ProgressCallback* progress, const std::string& destination_directory);

	bool Initialize(std::string destination_directory);

	bool OpenUpdateZip(const char* path);
	bool PrepareStagingDirectory();
	bool StageUpdate();
	bool CommitUpdate();
	void CleanupStagingDirectory();
	void RemoveUpdateZip();

	std::string FindPCSX2Exe() const;

private:
	static bool RecursiveDeleteDirectory(const char* path);

	void CloseUpdateZip();

	struct FileToUpdate
	{
		u32 file_index;
		std::string destination_filename;
	};

	bool ParseZip();

	std::string m_zip_path;
	std::string m_destination_directory;
	std::string m_staging_directory;

	std::vector<FileToUpdate> m_update_paths;
	std::vector<std::string> m_update_directories;

	ProgressCallback* m_progress;

#ifdef _WIN32
	CFileInStream m_archive_stream = {};
	CLookToRead2 m_look_stream = {};
	CSzArEx m_archive = {};

	bool m_file_opened = false;
	bool m_archive_opened = false;
#endif
};
