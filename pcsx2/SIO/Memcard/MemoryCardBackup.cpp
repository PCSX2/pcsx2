// SPDX-FileCopyrightText: 2002-2026 PCSX2 Dev Team
// SPDX-License-Identifier: GPL-3.0+

#include "MemoryCardBackup.h"

#include "Config.h"
#include "Host.h"
#include "SIO/Memcard/MemoryCardFile.h"

#include "common/Console.h"
#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/ZipFile.h"

#include "rapidjson/document.h"
#include "rapidjson/writer.h"

#include <chrono>
#include <thread>

static bool OpenBackupForReading(
	std::string path, ZipArchive& archive, MemoryCardBackup::BackupMetadata& metadata, Error* error);
static bool MakeMemoryCardBackup(
	const std::string& memory_card_path,
	const std::string& backup_path,
	std::chrono::time_point<std::chrono::system_clock> creation_time,
	bool zstd_compression,
	Error* error);
static bool WriteBackupMetadata(
	ZipArchive& archive, const MemoryCardBackup::BackupMetadata& metadata, Error* error);
static size_t PurgeOldBackups(
	const std::string& backup_directory,
	size_t min_backups_to_keep,
	size_t max_backups_to_keep,
	size_t drive_space_usage_limit_mib);
static std::string GetBackupPath(
	const std::string& memory_card_path,
	const std::string& backup_directory,
	std::chrono::time_point<std::chrono::system_clock> creation_time);

static constexpr const char* MEMORY_CARD_BACKUP_FORMAT_NAME = "PCSX2 Memory Card Backup";
static constexpr int MEMORY_CARD_BACKUP_FORMAT_VERSION = 1;

static constexpr const char* MEMORY_CARD_BACKUP_JSON_FILE_NAME = "backup.json";
static constexpr const char* MEMORY_CARD_BACKUP_CARD_FILE_NAME = "card.ps2";

static std::jthread s_backup_thread;

void MemoryCardBackup::MakeBackup(
	const std::string& memory_card_path, bool purge_old_backups)
{
	const std::chrono::time_point creation_time = std::chrono::system_clock::now();

	const bool zstd_compression = EmuConfig.MemoryCard.Backup.EnableZstdCompression;
	const u32 min_backups_to_keep = EmuConfig.MemoryCard.Backup.MinimumBackupsToKeep;
	const u32 max_backups_to_keep = EmuConfig.MemoryCard.Backup.MaximumBackupsToKeep;
	const u32 drive_space_usage_limit_mib = EmuConfig.MemoryCard.Backup.DriveSpaceUsageLimitMiB;

	// We don't want to overwrite another backup made on the same day, since
	// then if the memory card does get corrupted, we'd be more likely to write
	// over the most recent backup with the corrupted version.
	const std::string backup_directory = EmuFolders::MemoryCardBackups;
	const std::string backup_path = GetBackupPath(backup_directory, memory_card_path, creation_time);
	if (FileSystem::FileExists(backup_path.c_str()))
		return;

	s_backup_thread = std::jthread(
		[memory_card_path, backup_directory, backup_path, creation_time, zstd_compression,
			purge_old_backups, min_backups_to_keep, max_backups_to_keep, drive_space_usage_limit_mib](
			std::stop_token token) {
			const std::string_view file_name = Path::GetFileName(memory_card_path);

			Error error;
			if (!MakeMemoryCardBackup(memory_card_path, backup_path, creation_time, zstd_compression, &error))
			{
				Console.ErrorFmt("[MemoryCard] Cannot create backup of '{}': {}",
					file_name, error.GetDescription());
				return;
			}

			size_t backups_deleted = 0;
			if (purge_old_backups)
				backups_deleted = PurgeOldBackups(
					backup_directory, min_backups_to_keep, max_backups_to_keep, drive_space_usage_limit_mib);

			Console.WriteLnFmt("[MemoryCard] Wrote backup of '{}' to '{}', deleted {} old backup{}.",
				file_name, backup_path, backups_deleted, (backups_deleted != 1) ? "s" : "");
		});
}

void MemoryCardBackup::CancelBackup()
{
	if (s_backup_thread.joinable())
	{
		s_backup_thread.request_stop();
		s_backup_thread.join();
	}
}

bool MemoryCardBackup::RestoreBackup(
	const std::string& backup_path, const std::string& output_path, Error* error)
{
	ZipArchive archive;
	BackupMetadata metadata;
	if (!OpenBackupForReading(backup_path, archive, metadata, error))
		return false;

	switch (metadata.type)
	{
		case BackupType::FILE_CARD:
		{
			std::optional<ZipEntryIndex> index = archive.LocateFile(
				MEMORY_CARD_BACKUP_CARD_FILE_NAME, ZIP_FL_ENC_UTF_8, error);
			if (!index.has_value())
				return false;

			if (!archive.ExtractFile(*index, output_path.c_str(), error))
				return false;

			break;
		}
		case BackupType::FOLDER_CARD:
		{
			if (!FileSystem::CreateDirectoryPath(output_path.c_str(), false, error))
				return false;

			if (!archive.ExtractDirectory(MEMORY_CARD_BACKUP_CARD_FILE_NAME, output_path.c_str(), error))
				return false;

			break;
		}
	}

	return true;
}

std::vector<MemoryCardBackup::BackupMetadata> MemoryCardBackup::EnumerateBackups(
	const std::string& backups_directory)
{
	std::vector<FILESYSTEM_FIND_DATA> files;
	if (!FileSystem::FindFiles(
			backups_directory.c_str(), "*.zip", FILESYSTEM_FIND_HIDDEN_FILES | FILESYSTEM_FIND_FILES, &files))
		return {};

	std::vector<MemoryCardBackup::BackupMetadata> backups;

	for (const FILESYSTEM_FIND_DATA& file : files)
	{
		ZipArchive archive;
		BackupMetadata metadata;
		if (!OpenBackupForReading(file.FileName, archive, metadata, nullptr))
		{
			// Ignore any errors, so that if the user puts random .zip files in
			// the backups folder it doesn't spam log messages.
			continue;
		}

		backups.push_back(std::move(metadata));
	}

	// Sort by creation time in descending order.
	std::sort(backups.begin(), backups.end(),
		[](const BackupMetadata& lhs, const BackupMetadata& rhs) {
			return lhs.creation_time > rhs.creation_time;
		});

	return backups;
}

static bool OpenBackupForReading(
	std::string path, ZipArchive& archive, MemoryCardBackup::BackupMetadata& metadata, Error* error)
{
	const fmt::runtime_format_string<> missing_or_invalid_error_message =
		TRANSLATE_FS("MemoryCardBackup", "The '{}' file has missing or invalid '{}' property.");

	metadata.backup_path = path;

	FILESYSTEM_STAT_DATA stat;
	if (!FileSystem::StatFile(path.c_str(), &stat))
	{
		Error::SetString(error, TRANSLATE("MemoryCardBackup", "Failed to stat archive file."));
		return false;
	}

	metadata.backup_size = static_cast<u64>(stat.Size);

	if (!archive.Open(path.c_str(), ZIP_RDONLY, nullptr, error))
		return false;

	std::optional<ZipEntryIndex> index = archive.LocateFile(MEMORY_CARD_BACKUP_JSON_FILE_NAME, 0, nullptr);
	if (!index.has_value())
	{
		Error::SetString(error, TRANSLATE("MemoryCardBackup", "Archive does not contain a 'backup.json' file."));
		return false;
	}

	std::optional<std::string> json_string = archive.ReadTextFile(*index, 0, error);
	if (!json_string.has_value())
		return false;

	rapidjson::Document json;
	if (json.Parse(json_string->c_str()).HasParseError() || !json.IsObject())
	{
		Error::SetString(error, TRANSLATE("MemoryCardBackup", "Failed to parse 'backup.json' file."));
		return false;
	}

	auto format = json.FindMember("format");
	if (format == json.MemberEnd() ||
		!format->value.IsString() ||
		std::strcmp(format->value.GetString(), MEMORY_CARD_BACKUP_FORMAT_NAME) != 0)
	{
		Error::SetStringFmt(error, missing_or_invalid_error_message, MEMORY_CARD_BACKUP_JSON_FILE_NAME, "format");
		return false;
	}

	auto version = json.FindMember("version");
	if (version == json.MemberEnd() ||
		!version->value.IsInt())
	{
		Error::SetStringFmt(error, missing_or_invalid_error_message, MEMORY_CARD_BACKUP_JSON_FILE_NAME, "version");
		return false;
	}

	metadata.format_version = version->value.GetInt();
	if (metadata.format_version > MEMORY_CARD_BACKUP_FORMAT_VERSION)
	{
		Error::SetString(error,
			TRANSLATE(
				"MemoryCardBackup",
				"The 'backup.json' file was created by a newer version of PCSX2 and is not recognized."));
		return false;
	}

	auto name = json.FindMember("name");
	if (name == json.MemberEnd() ||
		!name->value.IsString())
	{
		Error::SetStringFmt(error, missing_or_invalid_error_message, MEMORY_CARD_BACKUP_JSON_FILE_NAME, "name");
		return false;
	}

	metadata.name = name->value.GetString();

	auto type = json.FindMember("type");
	if (type == json.MemberEnd() ||
		!type->value.IsString() ||
		(std::strcmp(type->value.GetString(), "file") != 0 &&
			std::strcmp(type->value.GetString(), "folder") != 0))
	{
		Error::SetStringFmt(error, missing_or_invalid_error_message, MEMORY_CARD_BACKUP_JSON_FILE_NAME, "type");
		return false;
	}

	if (std::strcmp(type->value.GetString(), "file") == 0)
		metadata.type = MemoryCardBackup::BackupType::FILE_CARD;
	else
		metadata.type = MemoryCardBackup::BackupType::FOLDER_CARD;

	auto creation_time = json.FindMember("creation_time");
	if (creation_time == json.MemberEnd() ||
		!creation_time->value.IsUint64())
	{
		Error::SetStringFmt(
			error, missing_or_invalid_error_message, MEMORY_CARD_BACKUP_JSON_FILE_NAME, "creation_time");
		return false;
	}

	metadata.creation_time = creation_time->value.GetUint64();

	auto temporary = json.FindMember("temporary");
	if (temporary == json.MemberEnd() ||
		!temporary->value.IsBool())
	{
		Error::SetStringFmt(
			error, missing_or_invalid_error_message, MEMORY_CARD_BACKUP_JSON_FILE_NAME, "temporary");
		return false;
	}

	metadata.temporary = temporary->value.GetBool();

	return true;
}

static bool MakeMemoryCardBackup(
	const std::string& memory_card_path,
	const std::string& backup_path,
	std::chrono::time_point<std::chrono::system_clock> creation_time,
	bool zstd_compression,
	Error* error)
{
	FILESYSTEM_STAT_DATA stat_data;
	if (!FileSystem::StatFile(memory_card_path.c_str(), &stat_data))
	{
		Error::SetString(error, "Failed to stat file.");
		return false;
	}

	ZipArchive archive;

	Error open_error;
	int error_code;
	if (!archive.Open(backup_path.c_str(), ZIP_CREATE | ZIP_EXCL, &error_code, &open_error))
	{
		Error::SetString(error, open_error.GetDescription() + " (while opening the archive)");
		return false;
	}

	MemoryCardBackup::BackupMetadata metadata;
	metadata.format_version = MEMORY_CARD_BACKUP_FORMAT_VERSION;
	metadata.name = Path::GetFileName(memory_card_path);

	std::chrono::duration creation_time_duration = creation_time.time_since_epoch();
	metadata.creation_time = std::chrono::duration_cast<std::chrono::seconds>(creation_time_duration).count();

	metadata.temporary = true;

	ZipCompressionOptions compression;
	if (zstd_compression)
	{
		compression.method = ZIP_CM_ZSTD;
		compression.level = 3;
	}
	else
	{
		compression.method = ZIP_CM_DEFLATE;
		compression.level = 6;
	}

	if (!(stat_data.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY))
	{
		metadata.type = MemoryCardBackup::BackupType::FILE_CARD;

		Error metadata_error;
		if (!WriteBackupMetadata(archive, metadata, &metadata_error))
		{
			Error::SetStringFmt(error,
				"{} (while writing '{}')", metadata_error.GetDescription(), MEMORY_CARD_BACKUP_JSON_FILE_NAME);
			return false;
		}

		Error add_error;
		if (!archive.AddFile(memory_card_path.c_str(), MEMORY_CARD_BACKUP_CARD_FILE_NAME, compression, &add_error))
		{
			Error::SetString(error, add_error.GetDescription() + " (while adding the memory card file to the archive)");
			return false;
		}
	}
	else
	{
		if (!FileMcd_IsFolder(memory_card_path))
		{
			Error::SetString(error, "Folder does not contain a superblock.");
			return false;
		}

		metadata.type = MemoryCardBackup::BackupType::FOLDER_CARD;

		Error metadata_error;
		if (!WriteBackupMetadata(archive, metadata, &metadata_error))
		{
			Error::SetStringFmt(error,
				"{} (while writing '{}')", metadata_error.GetDescription(), MEMORY_CARD_BACKUP_JSON_FILE_NAME);
			return false;
		}

		Error add_error;
		if (!archive.AddDirectory(
				memory_card_path.c_str(), MEMORY_CARD_BACKUP_CARD_FILE_NAME, compression, false, &add_error))
		{
			Error::SetString(error, add_error.GetDescription() + " (while adding memory card files to the archive)");
			return false;
		}
	}

	Error save_error;
	if (!archive.SaveChangesAndClose(&save_error))
	{
		Error::SetString(error, save_error.GetDescription() + " (while saving changes to the archive)");
		return false;
	}

	return true;
}

static bool WriteBackupMetadata(
	ZipArchive& archive, const MemoryCardBackup::BackupMetadata& metadata, Error* error)
{
	rapidjson::Document json(rapidjson::kObjectType);

	rapidjson::Value format;
	format.SetString(MEMORY_CARD_BACKUP_FORMAT_NAME, std::strlen(MEMORY_CARD_BACKUP_FORMAT_NAME));
	json.AddMember("format", format, json.GetAllocator());

	json.AddMember("version", MEMORY_CARD_BACKUP_FORMAT_VERSION, json.GetAllocator());

	rapidjson::Value name;
	name.SetString(metadata.name.c_str(), metadata.name.size());
	json.AddMember("name", name, json.GetAllocator());

	rapidjson::Value type;
	switch (metadata.type)
	{
		case MemoryCardBackup::BackupType::FILE_CARD:
			type.SetString("file", std::strlen("file"));
			break;
		case MemoryCardBackup::BackupType::FOLDER_CARD:
			type.SetString("folder", std::strlen("folder"));
			break;
	}
	json.AddMember("type", type, json.GetAllocator());

	rapidjson::Value creation_time;
	creation_time.SetUint64(metadata.creation_time);
	json.AddMember("creation_time", creation_time, json.GetAllocator());

	json.AddMember("temporary", metadata.temporary, json.GetAllocator());

	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	json.Accept(writer);

	// We need to make a copy here, since the pointer will be stored and read
	// later when the zip file is actually written to disk.
	void* string = std::malloc(buffer.GetSize());
	std::memcpy(string, buffer.GetString(), buffer.GetSize());

	if (!archive.AddFileFromBuffer(
			MEMORY_CARD_BACKUP_JSON_FILE_NAME, string, buffer.GetSize(), ZIP_FL_ENC_UTF_8, 1, error))
	{
		return false;
	}

	return true;
}

static size_t PurgeOldBackups(
	const std::string& backup_directory,
	const size_t min_backups_to_keep,
	const size_t max_backups_to_keep,
	const size_t drive_space_usage_limit_mib)
{
	std::vector<MemoryCardBackup::BackupMetadata> backups = MemoryCardBackup::EnumerateBackups(backup_directory);

	size_t drive_space_usage_bytes = 0;
	for (const MemoryCardBackup::BackupMetadata& backup : backups)
		drive_space_usage_bytes += backup.backup_size;

	size_t backups_deleted = 0;

	const size_t drive_space_usage_limit_bytes = drive_space_usage_limit_mib * _1mb;
	for (size_t i = backups.size(); i > 0; i--)
	{
		const size_t backups_remaining = backups.size() - backups_deleted;
		if (backups_remaining <= max_backups_to_keep)
		{
			if (backups_remaining <= min_backups_to_keep)
				break;

			if (drive_space_usage_bytes <= drive_space_usage_limit_bytes)
				break;
		}

		const MemoryCardBackup::BackupMetadata& backup = backups[i - 1];

		Error error;
		if (!FileSystem::DeleteFilePath(backup.backup_path.c_str(), &error))
		{
			Console.ErrorFmt("[MemoryCard] Cannot delete old backup file '{}': {}",
				backup.backup_path, error.GetDescription());
			break;
		}

		drive_space_usage_bytes -= backup.backup_size;
		backups_deleted++;
	}

	return backups_deleted;
}

static std::string GetBackupPath(
	const std::string& memory_card_path,
	const std::string& backup_directory,
	std::chrono::time_point<std::chrono::system_clock> creation_time)
{
	std::chrono::year_month_day year_month_day(std::chrono::floor<std::chrono::days>(creation_time));
	s32 year = static_cast<s32>(year_month_day.year());
	u32 month = static_cast<u32>(year_month_day.month());
	u32 day = static_cast<u32>(year_month_day.day());

	std::string_view memory_card_name = Path::GetFileName(memory_card_path);

	std::string file_name = fmt::format("temporary_{:04}-{:02}-{:02}_{}.zip", year, month, day, memory_card_name);
	return Path::Combine(backup_directory, file_name);
}
