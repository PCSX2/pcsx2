// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "common/Pcsx2Defs.h"

#include <string>
#include <vector>

class Error;

namespace MemoryCardBackup
{
	enum class BackupType
	{
		FILE_CARD,
		FOLDER_CARD,
	};

	struct BackupMetadata
	{
		std::string backup_path;
		u64 backup_size = 0;
		int format_version = -1;
		std::string name;
		BackupType type = BackupType::FILE_CARD;
		u64 creation_time = 0;
		bool temporary = false;
	};

	/// Start a worker thread to backup the memory card, cancelling the
	/// currently in-progress backup operation if there is one. If the backup
	/// completes successfully, and purge_old_backups is true, delete the oldest
	/// rolling backups until the disk space is within the desired limit again.
	void MakeBackup(const std::string& memory_card_path, bool purge_old_backups);

	/// Cancel the currently in-progress backup operation if there is one.
	void CancelBackup();

	/// Extract a memory card from a backup file.
	bool RestoreBackup(const std::string& backup_path, const std::string& output_path, Error* error);

	/// Return a list of all the backups in the specified directory, sorted by
	/// creation time in descending order.
	std::vector<BackupMetadata> EnumerateBackups(const std::string& backups_directory);
} // namespace MemoryCardBackup
