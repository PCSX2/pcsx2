/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2023  PCSX2 Dev Team
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

#include "CDVD/CDVD.h"
#include "Elfheader.h"
#include "GameList.h"
#include "Host.h"
#include "VMManager.h"

#include "common/Assertions.h"
#include "common/Console.h"
#include "common/FileSystem.h"
#include "common/HTTPDownloader.h"
#include "common/HeterogeneousContainers.h"
#include "common/Path.h"
#include "common/ProgressCallback.h"
#include "common/StringUtil.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <fstream>
#include <string_view>
#include <utility>

#ifdef _WIN32
#include "common/RedtapeWindows.h"
#endif

namespace GameList
{
	enum : u32
	{
		GAME_LIST_CACHE_SIGNATURE = 0x45434C47,
		GAME_LIST_CACHE_VERSION = 32,


		PLAYED_TIME_SERIAL_LENGTH = 32,
		PLAYED_TIME_LAST_TIME_LENGTH = 20, // uint64
		PLAYED_TIME_TOTAL_TIME_LENGTH = 20, // uint64
		PLAYED_TIME_LINE_LENGTH = PLAYED_TIME_SERIAL_LENGTH + 1 + PLAYED_TIME_LAST_TIME_LENGTH + 1 + PLAYED_TIME_TOTAL_TIME_LENGTH,
	};

	struct PlayedTimeEntry
	{
		std::time_t last_played_time;
		std::time_t total_played_time;
	};

	using CacheMap = UnorderedStringMap<Entry>;
	using PlayedTimeMap = UnorderedStringMap<PlayedTimeEntry>;

	static bool IsScannableFilename(const std::string_view& path);

	static bool GetIsoSerialAndCRC(const std::string& path, s32* disc_type, std::string* serial, u32* crc);
	static Region ParseDatabaseRegion(const std::string_view& db_region);
	static bool GetElfListEntry(const std::string& path, GameList::Entry* entry);
	static bool GetIsoListEntry(const std::string& path, GameList::Entry* entry);

	static bool GetGameListEntryFromCache(const std::string& path, GameList::Entry* entry);
	static void ScanDirectory(const char* path, bool recursive, bool only_cache, const std::vector<std::string>& excluded_paths,
		const PlayedTimeMap& played_time_map, ProgressCallback* progress);
	static bool AddFileFromCache(const std::string& path, std::time_t timestamp, const PlayedTimeMap& played_time_map);
	static bool ScanFile(
		std::string path, std::time_t timestamp, std::unique_lock<std::recursive_mutex>& lock, const PlayedTimeMap& played_time_map);

	static void LoadCache();
	static bool LoadEntriesFromCache(std::FILE* stream);
	static bool OpenCacheForWriting();
	static bool WriteEntryToCache(const GameList::Entry* entry);
	static void CloseCacheFileStream();
	static void DeleteCacheFile();
	static void RewriteCacheFile();

	static std::string GetPlayedTimeFile();
	static bool ParsePlayedTimeLine(char* line, std::string& serial, PlayedTimeEntry& entry);
	static std::string MakePlayedTimeLine(const std::string& serial, const PlayedTimeEntry& entry);
	static PlayedTimeMap LoadPlayedTimeMap(const std::string& path);
	static PlayedTimeEntry UpdatePlayedTimeFile(
		const std::string& path, const std::string& serial, std::time_t last_time, std::time_t add_time);
} // namespace GameList

static std::vector<GameList::Entry> s_entries;
static std::recursive_mutex s_mutex;
static GameList::CacheMap s_cache_map;
static std::FILE* s_cache_write_stream = nullptr;

const char* GameList::EntryTypeToString(EntryType type)
{
	static std::array<const char*, static_cast<int>(EntryType::Count)> names = {{"PS2Disc", "PS1Disc", "ELF"}};
	return names[static_cast<int>(type)];
}

const char* GameList::EntryTypeToDisplayString(EntryType type)
{
	static std::array<const char*, static_cast<int>(EntryType::Count)> names = {{"PS2 Disc", "PS1 Disc", "ELF"}};
	return names[static_cast<int>(type)];
}

const char* GameList::RegionToString(Region region)
{
	static std::array<const char*, static_cast<int>(Region::Count)> names = {{"NTSC-B", "NTSC-C", "NTSC-HK", "NTSC-J", "NTSC-K", "NTSC-T",
		"NTSC-U", "Other", "PAL-A", "PAL-AF", "PAL-AU", "PAL-BE", "PAL-E", "PAL-F", "PAL-FI", "PAL-G", "PAL-GR", "PAL-I", "PAL-IN", "PAL-M",
		"PAL-NL", "PAL-NO", "PAL-P", "PAL-R", "PAL-S", "PAL-SC", "PAL-SW", "PAL-SWI", "PAL-UK"}};

	return names[static_cast<int>(region)];
}

const char* GameList::EntryCompatibilityRatingToString(CompatibilityRating rating)
{
	// clang-format off
	switch (rating)
	{
	case CompatibilityRating::Unknown: return "Unknown";
	case CompatibilityRating::Nothing: return "Nothing";
	case CompatibilityRating::Intro: return "Intro";
	case CompatibilityRating::Menu: return "Menu";
	case CompatibilityRating::InGame: return "InGame";
	case CompatibilityRating::Playable: return "Playable";
	case CompatibilityRating::Perfect: return "Perfect";
	default: return "";
	}
	// clang-format on
}

bool GameList::IsScannableFilename(const std::string_view& path)
{
	return VMManager::IsDiscFileName(path) || VMManager::IsElfFileName(path);
}

void GameList::FillBootParametersForEntry(VMBootParameters* params, const Entry* entry)
{
	if (entry->type == GameList::EntryType::PS1Disc || entry->type == GameList::EntryType::PS2Disc)
	{
		params->filename = entry->path;
		params->source_type = CDVD_SourceType::Iso;
		params->elf_override.clear();
	}
	else if (entry->type == GameList::EntryType::ELF)
	{
		params->filename = VMManager::GetDiscOverrideFromGameSettings(entry->path);
		params->source_type = params->filename.empty() ? CDVD_SourceType::NoDisc : CDVD_SourceType::Iso;
		params->elf_override = entry->path;
	}
	else
	{
		params->filename.clear();
		params->source_type = CDVD_SourceType::NoDisc;
		params->elf_override.clear();
	}
}

bool GameList::GetIsoSerialAndCRC(const std::string& path, s32* disc_type, std::string* serial, u32* crc)
{
	// This isn't great, we really want to make it all thread-local...
	CDVD = &CDVDapi_Iso;
	if (CDVD->open(path.c_str()) != 0)
		return false;

	// TODO: we could include the version in the game list?
	*disc_type = DoCDVDdetectDiskType();
	cdvdGetDiscInfo(serial, nullptr, nullptr, crc, nullptr);
	DoCDVDclose();
	return true;
}

bool GameList::GetElfListEntry(const std::string& path, GameList::Entry* entry)
{
	const s64 file_size = FileSystem::GetPathFileSize(path.c_str());
	if (file_size <= 0)
		return false;

	try
	{
		ElfObject eo(path, static_cast<uint>(file_size), false);
		entry->crc = eo.getCRC();
	}
	catch (...)
	{
		Console.Error("Failed to parse ELF '%s'", path.c_str());
		return false;
	}

	entry->path = path;
	entry->serial.clear();
	entry->title = Path::GetFileTitle(FileSystem::GetDisplayNameFromPath(path));
	entry->region = Region::Other;
	entry->total_size = static_cast<u64>(file_size);
	entry->type = EntryType::ELF;
	entry->compatibility_rating = CompatibilityRating::Unknown;

	std::string disc_path(VMManager::GetDiscOverrideFromGameSettings(path));
	if (!disc_path.empty())
	{
		s32 disc_type;
		u32 disc_crc;
		if (GetIsoSerialAndCRC(disc_path, &disc_type, &entry->serial, &disc_crc))
		{
			// use serial/region/compat info from the db
			if (const GameDatabaseSchema::GameEntry* db_entry = GameDatabase::findGame(entry->serial))
			{
				entry->compatibility_rating = db_entry->compat;
				entry->region = ParseDatabaseRegion(db_entry->region);
			}
		}
	}

	return true;
}

GameList::Region GameList::ParseDatabaseRegion(const std::string_view& db_region)
{
	// clang-format off
						////// NTSC //////
						//////////////////
	if (StringUtil::StartsWith(db_region, "NTSC-B"))
		return Region::NTSC_B;
	else if (StringUtil::StartsWith(db_region, "NTSC-C"))
		return Region::NTSC_C;
	else if (StringUtil::StartsWith(db_region, "NTSC-HK"))
		return Region::NTSC_HK;
	else if (StringUtil::StartsWith(db_region, "NTSC-J"))
		return Region::NTSC_J;
	else if (StringUtil::StartsWith(db_region, "NTSC-K"))
		return Region::NTSC_K;
	else if (StringUtil::StartsWith(db_region, "NTSC-T"))
		return Region::NTSC_T;
	else if (StringUtil::StartsWith(db_region, "NTSC-U"))
		return Region::NTSC_U;
						////// PAL //////
						//////////////////
	else if (StringUtil::StartsWith(db_region, "PAL-AF"))
		return Region::PAL_AF;
	else if (StringUtil::StartsWith(db_region, "PAL-AU"))
		return Region::PAL_AU;
	else if (StringUtil::StartsWith(db_region, "PAL-A"))
		return Region::PAL_A;
	else if (StringUtil::StartsWith(db_region, "PAL-BE"))
		return Region::PAL_BE;
	else if (StringUtil::StartsWith(db_region, "PAL-E"))
		return Region::PAL_E;
	else if (StringUtil::StartsWith(db_region, "PAL-FI"))
		return Region::PAL_FI;
	else if (StringUtil::StartsWith(db_region, "PAL-F"))
		return Region::PAL_F;
	else if (StringUtil::StartsWith(db_region, "PAL-GR"))
		return Region::PAL_GR;
	else if (StringUtil::StartsWith(db_region, "PAL-G"))
		return Region::PAL_G;
	else if (StringUtil::StartsWith(db_region, "PAL-IN"))
		return Region::PAL_IN;
	else if (StringUtil::StartsWith(db_region, "PAL-I"))
		return Region::PAL_I;
	else if (StringUtil::StartsWith(db_region, "PAL-M"))
		return Region::PAL_M;
	else if (StringUtil::StartsWith(db_region, "PAL-NL"))
		return Region::PAL_NL;
	else if (StringUtil::StartsWith(db_region, "PAL-NO"))
		return Region::PAL_NO;
	else if (StringUtil::StartsWith(db_region, "PAL-P"))
		return Region::PAL_P;
	else if (StringUtil::StartsWith(db_region, "PAL-R"))
		return Region::PAL_R;
	else if (StringUtil::StartsWith(db_region, "PAL-SC"))
		return Region::PAL_SC;
	else if (StringUtil::StartsWith(db_region, "PAL-SWI"))
		return Region::PAL_SWI;
	else if (StringUtil::StartsWith(db_region, "PAL-SW"))
		return Region::PAL_SW;
	else if (StringUtil::StartsWith(db_region, "PAL-S"))
		return Region::PAL_S;
	else if (StringUtil::StartsWith(db_region, "PAL-UK"))
		return Region::PAL_UK;
	else
		return Region::Other;
	// clang-format on
}

bool GameList::GetIsoListEntry(const std::string& path, GameList::Entry* entry)
{
	FILESYSTEM_STAT_DATA sd;
	if (!FileSystem::StatFile(path.c_str(), &sd))
		return false;

	s32 disc_type;
	if (!GetIsoSerialAndCRC(path, &disc_type, &entry->serial, &entry->crc))
		return false;

	switch (disc_type)
	{
		case CDVD_TYPE_PSCD:
		case CDVD_TYPE_PSCDDA:
			entry->type = EntryType::PS1Disc;
			break;

		case CDVD_TYPE_PS2CD:
		case CDVD_TYPE_PS2CDDA:
		case CDVD_TYPE_PS2DVD:
			entry->type = EntryType::PS2Disc;
			break;

		case CDVD_TYPE_ILLEGAL:
		default:
			return false;
	}

	entry->path = path;
	entry->total_size = sd.Size;
	entry->compatibility_rating = CompatibilityRating::Unknown;

	if (const GameDatabaseSchema::GameEntry* db_entry = GameDatabase::findGame(entry->serial))
	{
		entry->title = std::move(db_entry->name);
		entry->compatibility_rating = db_entry->compat;
		entry->region = ParseDatabaseRegion(db_entry->region);
	}
	else
	{
		entry->title = Path::GetFileTitle(path);
		entry->region = Region::Other;
	}

	return true;
}

bool GameList::PopulateEntryFromPath(const std::string& path, GameList::Entry* entry)
{
	if (VMManager::IsElfFileName(path.c_str()))
		return GetElfListEntry(path, entry);
	else
		return GetIsoListEntry(path, entry);
}

bool GameList::GetGameListEntryFromCache(const std::string& path, GameList::Entry* entry)
{
	auto iter = UnorderedStringMapFind(s_cache_map, path);
	if (iter == s_cache_map.end())
		return false;

	*entry = std::move(iter->second);
	s_cache_map.erase(iter);
	return true;
}

static bool ReadString(std::FILE* stream, std::string* dest)
{
	u32 size;
	if (std::fread(&size, sizeof(size), 1, stream) != 1)
		return false;

	dest->resize(size);
	if (size > 0 && std::fread(dest->data(), size, 1, stream) != 1)
		return false;

	return true;
}

static bool ReadU8(std::FILE* stream, u8* dest)
{
	return std::fread(dest, sizeof(u8), 1, stream) > 0;
}

static bool ReadU32(std::FILE* stream, u32* dest)
{
	return std::fread(dest, sizeof(u32), 1, stream) > 0;
}

static bool ReadU64(std::FILE* stream, u64* dest)
{
	return std::fread(dest, sizeof(u64), 1, stream) > 0;
}

static bool WriteString(std::FILE* stream, const std::string& str)
{
	const u32 size = static_cast<u32>(str.size());
	return (std::fwrite(&size, sizeof(size), 1, stream) > 0 && (size == 0 || std::fwrite(str.data(), size, 1, stream) > 0));
}

static bool WriteU8(std::FILE* stream, u8 dest)
{
	return std::fwrite(&dest, sizeof(u8), 1, stream) > 0;
}

static bool WriteU32(std::FILE* stream, u32 dest)
{
	return std::fwrite(&dest, sizeof(u32), 1, stream) > 0;
}

static bool WriteU64(std::FILE* stream, u64 dest)
{
	return std::fwrite(&dest, sizeof(u64), 1, stream) > 0;
}

bool GameList::LoadEntriesFromCache(std::FILE* stream)
{
	u32 file_signature, file_version;
	s64 start_pos, file_size;
	if (!ReadU32(stream, &file_signature) || !ReadU32(stream, &file_version) || file_signature != GAME_LIST_CACHE_SIGNATURE ||
		file_version != GAME_LIST_CACHE_VERSION || (start_pos = FileSystem::FTell64(stream)) < 0 ||
		FileSystem::FSeek64(stream, 0, SEEK_END) != 0 || (file_size = FileSystem::FTell64(stream)) < 0 ||
		FileSystem::FSeek64(stream, start_pos, SEEK_SET) != 0)
	{
		Console.Warning("Game list cache is corrupted");
		return false;
	}

	while (FileSystem::FTell64(stream) != file_size)
	{
		std::string path;
		GameList::Entry ge;

		u8 type;
		u8 region;
		u8 compatibility_rating;
		u64 last_modified_time;

		if (!ReadString(stream, &path) || !ReadString(stream, &ge.serial) || !ReadString(stream, &ge.title) || !ReadU8(stream, &type) ||
			!ReadU8(stream, &region) || !ReadU64(stream, &ge.total_size) || !ReadU64(stream, &last_modified_time) ||
			!ReadU32(stream, &ge.crc) || !ReadU8(stream, &compatibility_rating) || region >= static_cast<u8>(Region::Count) ||
			type >= static_cast<u8>(EntryType::Count) || compatibility_rating > static_cast<u8>(CompatibilityRating::Perfect))
		{
			Console.Warning("Game list cache entry is corrupted");
			return false;
		}

		ge.path = path;
		ge.region = static_cast<Region>(region);
		ge.type = static_cast<EntryType>(type);
		ge.compatibility_rating = static_cast<CompatibilityRating>(compatibility_rating);
		ge.last_modified_time = static_cast<std::time_t>(last_modified_time);

		auto iter = UnorderedStringMapFind(s_cache_map, ge.path);
		if (iter != s_cache_map.end())
			iter->second = std::move(ge);
		else
			s_cache_map.emplace(std::move(path), std::move(ge));
	}

	return true;
}

static std::string GetCacheFilename()
{
	return Path::Combine(EmuFolders::Cache, "gamelist.cache");
}

void GameList::LoadCache()
{
	const std::string cache_filename(GetCacheFilename());
	auto stream = FileSystem::OpenManagedCFile(cache_filename.c_str(), "rb");
	if (!stream)
		return;

	if (!LoadEntriesFromCache(stream.get()))
	{
		Console.Warning("Deleting corrupted cache file '%s'", cache_filename.c_str());
		stream.reset();
		s_cache_map.clear();
		DeleteCacheFile();
		return;
	}
}

bool GameList::OpenCacheForWriting()
{
	const std::string cache_filename(GetCacheFilename());
	if (cache_filename.empty())
		return false;

	pxAssert(!s_cache_write_stream);
	s_cache_write_stream = FileSystem::OpenCFile(cache_filename.c_str(), "r+b");
	if (s_cache_write_stream)
	{
		// check the header
		u32 signature, version;
		if (ReadU32(s_cache_write_stream, &signature) && signature == GAME_LIST_CACHE_SIGNATURE &&
			ReadU32(s_cache_write_stream, &version) && version == GAME_LIST_CACHE_VERSION &&
			FileSystem::FSeek64(s_cache_write_stream, 0, SEEK_END) == 0)
		{
			return true;
		}

		std::fclose(s_cache_write_stream);
	}

	Console.WriteLn("Creating new game list cache file: '%s'", cache_filename.c_str());

	s_cache_write_stream = FileSystem::OpenCFile(cache_filename.c_str(), "w+b");
	if (!s_cache_write_stream)
		return false;


	// new cache file, write header
	if (!WriteU32(s_cache_write_stream, GAME_LIST_CACHE_SIGNATURE) || !WriteU32(s_cache_write_stream, GAME_LIST_CACHE_VERSION))
	{
		Console.Error("Failed to write game list cache header");
		std::fclose(s_cache_write_stream);
		s_cache_write_stream = nullptr;
		FileSystem::DeleteFilePath(cache_filename.c_str());
		return false;
	}

	return true;
}

bool GameList::WriteEntryToCache(const Entry* entry)
{
	bool result = true;
	result &= WriteString(s_cache_write_stream, entry->path);
	result &= WriteString(s_cache_write_stream, entry->serial);
	result &= WriteString(s_cache_write_stream, entry->title);
	result &= WriteU8(s_cache_write_stream, static_cast<u8>(entry->type));
	result &= WriteU8(s_cache_write_stream, static_cast<u8>(entry->region));
	result &= WriteU64(s_cache_write_stream, entry->total_size);
	result &= WriteU64(s_cache_write_stream, static_cast<u64>(entry->last_modified_time));
	result &= WriteU32(s_cache_write_stream, entry->crc);
	result &= WriteU8(s_cache_write_stream, static_cast<u8>(entry->compatibility_rating));

	// flush after each entry, that way we don't end up with a corrupted file if we crash scanning.
	if (result)
		result = (std::fflush(s_cache_write_stream) == 0);

	return result;
}

void GameList::CloseCacheFileStream()
{
	if (!s_cache_write_stream)
		return;

	std::fclose(s_cache_write_stream);
	s_cache_write_stream = nullptr;
}

void GameList::DeleteCacheFile()
{
	pxAssert(!s_cache_write_stream);

	const std::string cache_filename(GetCacheFilename());
	if (cache_filename.empty() || !FileSystem::FileExists(cache_filename.c_str()))
		return;

	if (FileSystem::DeleteFilePath(cache_filename.c_str()))
		Console.WriteLn("Deleted game list cache '%s'", cache_filename.c_str());
	else
		Console.Warning("Failed to delete game list cache '%s'", cache_filename.c_str());
}

void GameList::RewriteCacheFile()
{
	CloseCacheFileStream();
	DeleteCacheFile();

	if (OpenCacheForWriting())
	{
		for (const GameList::Entry& entry : s_entries)
			WriteEntryToCache(&entry);

		CloseCacheFileStream();
	}
}

static bool IsPathExcluded(const std::vector<std::string>& excluded_paths, const std::string& path)
{
	return (std::find(excluded_paths.begin(), excluded_paths.end(), path) != excluded_paths.end());
}

void GameList::ScanDirectory(const char* path, bool recursive, bool only_cache, const std::vector<std::string>& excluded_paths,
	const PlayedTimeMap& played_time_map, ProgressCallback* progress)
{
	Console.WriteLn("Scanning %s%s", path, recursive ? " (recursively)" : "");

	progress->PushState();
	progress->SetFormattedStatusText("Scanning directory '%s'%s...", path, recursive ? " (recursively)" : "");

	FileSystem::FindResultsArray files;
	FileSystem::FindFiles(path, "*",
		recursive ? (FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES | FILESYSTEM_FIND_RECURSIVE) :
                    (FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES),
		&files);

	u32 files_scanned = 0;
	progress->SetProgressRange(static_cast<u32>(files.size()));
	progress->SetProgressValue(0);

	for (FILESYSTEM_FIND_DATA& ffd : files)
	{
		files_scanned++;

		if (progress->IsCancelled() || !GameList::IsScannableFilename(ffd.FileName) || IsPathExcluded(excluded_paths, ffd.FileName))
		{
			continue;
		}

		std::unique_lock lock(s_mutex);
		if (GetEntryForPath(ffd.FileName.c_str()) || AddFileFromCache(ffd.FileName, ffd.ModificationTime, played_time_map) || only_cache)
		{
			continue;
		}

		progress->SetFormattedStatusText("Scanning '%s'...", FileSystem::GetDisplayNameFromPath(ffd.FileName).c_str());
		ScanFile(std::move(ffd.FileName), ffd.ModificationTime, lock, played_time_map);
		progress->SetProgressValue(files_scanned);
	}

	progress->SetProgressValue(files_scanned);
	progress->PopState();
}

bool GameList::AddFileFromCache(const std::string& path, std::time_t timestamp, const PlayedTimeMap& played_time_map)
{
	Entry entry;
	if (!GetGameListEntryFromCache(path, &entry) || entry.last_modified_time != timestamp)
		return false;

	auto iter = UnorderedStringMapFind(played_time_map, entry.serial);
	if (iter != played_time_map.end())
	{
		entry.last_played_time = iter->second.last_played_time;
		entry.total_played_time = iter->second.total_played_time;
	}

	s_entries.push_back(std::move(entry));
	return true;
}

bool GameList::ScanFile(
	std::string path, std::time_t timestamp, std::unique_lock<std::recursive_mutex>& lock, const PlayedTimeMap& played_time_map)
{
	// don't block UI while scanning
	lock.unlock();

	DevCon.WriteLn("Scanning '%s'...", path.c_str());

	Entry entry;
	if (!PopulateEntryFromPath(path, &entry))
		return false;

	entry.path = std::move(path);
	entry.last_modified_time = timestamp;

	if (s_cache_write_stream || OpenCacheForWriting())
	{
		if (!WriteEntryToCache(&entry))
			Console.Warning("Failed to write entry '%s' to cache", entry.path.c_str());
	}

	auto iter = UnorderedStringMapFind(played_time_map, entry.serial);
	if (iter != played_time_map.end())
	{
		entry.last_played_time = iter->second.last_played_time;
		entry.total_played_time = iter->second.total_played_time;
	}

	lock.lock();

	// remove if present
	auto it = std::find_if(
		s_entries.begin(), s_entries.end(), [&entry](const Entry& existing_entry) { return (existing_entry.path == entry.path); });
	if (it != s_entries.end())
		s_entries.erase(it);

	s_entries.push_back(std::move(entry));
	return true;
}

std::unique_lock<std::recursive_mutex> GameList::GetLock()
{
	return std::unique_lock<std::recursive_mutex>(s_mutex);
}

const GameList::Entry* GameList::GetEntryByIndex(u32 index)
{
	return (index < s_entries.size()) ? &s_entries[index] : nullptr;
}

const GameList::Entry* GameList::GetEntryForPath(const char* path)
{
	const size_t path_length = std::strlen(path);
	for (const Entry& entry : s_entries)
	{
		if (entry.path.size() == path_length && StringUtil::Strcasecmp(entry.path.c_str(), path) == 0)
			return &entry;
	}

	return nullptr;
}

const GameList::Entry* GameList::GetEntryByCRC(u32 crc)
{
	for (const Entry& entry : s_entries)
	{
		if (entry.crc == crc)
			return &entry;
	}

	return nullptr;
}

const GameList::Entry* GameList::GetEntryBySerialAndCRC(const std::string_view& serial, u32 crc)
{
	for (const Entry& entry : s_entries)
	{
		if (entry.crc == crc && StringUtil::compareNoCase(entry.serial, serial))
			return &entry;
	}

	return nullptr;
}

u32 GameList::GetEntryCount()
{
	return static_cast<u32>(s_entries.size());
}

void GameList::Refresh(bool invalidate_cache, bool only_cache, ProgressCallback* progress /* = nullptr */)
{
	if (!progress)
		progress = ProgressCallback::NullProgressCallback;

	if (invalidate_cache)
		DeleteCacheFile();
	else
		LoadCache();

	// don't delete the old entries, since the frontend might still access them
	std::vector<Entry> old_entries;
	{
		std::unique_lock lock(s_mutex);
		old_entries.swap(s_entries);
	}

	const std::vector<std::string> excluded_paths(Host::GetBaseStringListSetting("GameList", "ExcludedPaths"));
	const std::vector<std::string> dirs(Host::GetBaseStringListSetting("GameList", "Paths"));
	const std::vector<std::string> recursive_dirs(Host::GetBaseStringListSetting("GameList", "RecursivePaths"));
	const PlayedTimeMap played_time(LoadPlayedTimeMap(GetPlayedTimeFile()));

	if (!dirs.empty() || !recursive_dirs.empty())
	{
		progress->SetProgressRange(static_cast<u32>(dirs.size() + recursive_dirs.size()));
		progress->SetProgressValue(0);

		// we manually count it here, because otherwise pop state updates it itself
		int directory_counter = 0;
		for (const std::string& dir : dirs)
		{
			if (progress->IsCancelled())
				break;

			ScanDirectory(dir.c_str(), false, only_cache, excluded_paths, played_time, progress);
			progress->SetProgressValue(++directory_counter);
		}
		for (const std::string& dir : recursive_dirs)
		{
			if (progress->IsCancelled())
				break;

			ScanDirectory(dir.c_str(), true, only_cache, excluded_paths, played_time, progress);
			progress->SetProgressValue(++directory_counter);
		}
	}

	// don't need unused cache entries
	CloseCacheFileStream();
	s_cache_map.clear();
}

bool GameList::RescanPath(const std::string& path)
{
	FILESYSTEM_STAT_DATA sd;
	if (!FileSystem::StatFile(path.c_str(), &sd))
		return false;

	std::unique_lock lock(s_mutex);

	const PlayedTimeMap played_time(LoadPlayedTimeMap(GetPlayedTimeFile()));

	{
		// cancel if excluded
		const std::vector<std::string> excluded_paths(Host::GetBaseStringListSetting("GameList", "ExcludedPaths"));
		if (std::find(excluded_paths.begin(), excluded_paths.end(), path) != excluded_paths.end())
			return false;
	}

	// re-scan!
	if (!ScanFile(path, sd.ModificationTime, lock, played_time))
		return true;

	// update cache.. this is far from ideal, but since everything's variable length, all we can do.
	RewriteCacheFile();
	return true;
}

bool GameList::GetSerialAndCRCForFilename(const char* filename, std::string* serial, u32* crc)
{
	if (const GameList::Entry* entry = GetEntryForPath(filename); entry)
	{
		*serial = entry->serial;
		*crc = entry->crc;
		return true;
	}

	// Just scan it.. hopefully it'll come back okay.
	GameList::Entry temp_entry;
	if (GameList::PopulateEntryFromPath(filename, &temp_entry))
	{
		*serial = std::move(temp_entry.serial);
		*crc = temp_entry.crc;
		return true;
	}

	return false;
}

std::string GameList::GetPlayedTimeFile()
{
	return Path::Combine(EmuFolders::Settings, "playtime.dat");
}

bool GameList::ParsePlayedTimeLine(char* line, std::string& serial, PlayedTimeEntry& entry)
{
	size_t len = std::strlen(line);
	if (len != (PLAYED_TIME_LINE_LENGTH + 1)) // \n
	{
		Console.Warning("(ParsePlayedTimeLine) Malformed line: '%s'", line);
		return false;
	}

	const std::string_view serial_tok(StringUtil::StripWhitespace(std::string_view(line, PLAYED_TIME_SERIAL_LENGTH)));
	const std::string_view total_played_time_tok(
		StringUtil::StripWhitespace(std::string_view(line + PLAYED_TIME_SERIAL_LENGTH + 1, PLAYED_TIME_LAST_TIME_LENGTH)));
	const std::string_view last_played_time_tok(StringUtil::StripWhitespace(
		std::string_view(line + PLAYED_TIME_SERIAL_LENGTH + 1 + PLAYED_TIME_LAST_TIME_LENGTH + 1, PLAYED_TIME_TOTAL_TIME_LENGTH)));

	const std::optional<u64> total_played_time(StringUtil::FromChars<u64>(total_played_time_tok));
	const std::optional<u64> last_played_time(StringUtil::FromChars<u64>(last_played_time_tok));
	if (serial_tok.empty() || !last_played_time.has_value() || !total_played_time.has_value())
	{
		Console.Warning("(ParsePlayedTimeLine) Malformed line: '%s'", line);
		return false;
	}

	serial = serial_tok;
	entry.last_played_time = static_cast<std::time_t>(last_played_time.value());
	entry.total_played_time = static_cast<std::time_t>(total_played_time.value());
	return true;
}

std::string GameList::MakePlayedTimeLine(const std::string& serial, const PlayedTimeEntry& entry)
{
	return fmt::format("{:<{}} {:<{}} {:<{}}\n", serial, static_cast<unsigned>(PLAYED_TIME_SERIAL_LENGTH), entry.total_played_time,
		static_cast<unsigned>(PLAYED_TIME_TOTAL_TIME_LENGTH), entry.last_played_time, static_cast<unsigned>(PLAYED_TIME_LAST_TIME_LENGTH));
}

GameList::PlayedTimeMap GameList::LoadPlayedTimeMap(const std::string& path)
{
	PlayedTimeMap ret;

	// Use write mode here, even though we're not writing, so we can lock the file from other updates.
	auto fp = FileSystem::OpenManagedCFile(path.c_str(), "r+b");

#ifdef _WIN32
	// On Windows, the file is implicitly locked.
	while (!fp && GetLastError() == ERROR_SHARING_VIOLATION)
	{
		Sleep(10);
		fp = FileSystem::OpenManagedCFile(path.c_str(), "r+b");
	}
#endif

	if (fp)
	{
#ifndef _WIN32
		FileSystem::POSIXLock flock(fp.get());
#endif

		char line[256];
		while (std::fgets(line, sizeof(line), fp.get()))
		{
			std::string serial;
			PlayedTimeEntry entry;
			if (!ParsePlayedTimeLine(line, serial, entry))
				continue;

			if (UnorderedStringMapFind(ret, serial) != ret.end())
			{
				Console.Warning("(LoadPlayedTimeMap) Duplicate entry: '%s'", serial.c_str());
				continue;
			}

			ret.emplace(std::move(serial), entry);
		}
	}

	return ret;
}

GameList::PlayedTimeEntry GameList::UpdatePlayedTimeFile(
	const std::string& path, const std::string& serial, std::time_t last_time, std::time_t add_time)
{
	const PlayedTimeEntry new_entry{last_time, add_time};

	auto fp = FileSystem::OpenManagedCFile(path.c_str(), "r+b");

#ifdef _WIN32
	// On Windows, the file is implicitly locked.
	while (!fp && GetLastError() == ERROR_SHARING_VIOLATION)
	{
		Sleep(10);
		fp = FileSystem::OpenManagedCFile(path.c_str(), "r+b");
	}
#endif

	// Doesn't exist? Create it.
	if (!fp && errno == ENOENT)
		fp = FileSystem::OpenManagedCFile(path.c_str(), "w+b");

	if (!fp)
	{
		Console.Error("Failed to open '%s' for update.", path.c_str());
		return new_entry;
	}

#ifndef _WIN32
	FileSystem::POSIXLock flock(fp.get());
#endif

	for (;;)
	{
		char line[256];
		const s64 line_pos = FileSystem::FTell64(fp.get());
		if (!std::fgets(line, sizeof(line), fp.get()))
			break;

		std::string line_serial;
		PlayedTimeEntry line_entry;
		if (!ParsePlayedTimeLine(line, line_serial, line_entry))
			continue;

		if (line_serial != serial)
			continue;

		// found it!
		line_entry.last_played_time = (last_time != 0) ? last_time : 0;
		line_entry.total_played_time = (last_time != 0) ? (line_entry.total_played_time + add_time) : 0;

		std::string new_line(MakePlayedTimeLine(serial, line_entry));
		if (FileSystem::FSeek64(fp.get(), line_pos, SEEK_SET) != 0 || std::fwrite(new_line.data(), new_line.length(), 1, fp.get()) != 1 ||
			std::fflush(fp.get()) != 0)
		{
			Console.Error("Failed to update '%s'.", path.c_str());
		}

		return line_entry;
	}

	if (last_time != 0)
	{
		// new entry.
		std::string new_line(MakePlayedTimeLine(serial, new_entry));
		if (FileSystem::FSeek64(fp.get(), 0, SEEK_END) != 0 || std::fwrite(new_line.data(), new_line.length(), 1, fp.get()) != 1)
		{
			Console.Error("Failed to write '%s'.", path.c_str());
		}
	}

	return new_entry;
}

void GameList::AddPlayedTimeForSerial(const std::string& serial, std::time_t last_time, std::time_t add_time)
{
	if (serial.empty())
		return;

	const PlayedTimeEntry pt(UpdatePlayedTimeFile(GetPlayedTimeFile(), serial, last_time, add_time));
	Console.WriteLn("Add %u seconds play time to %s -> now %u", static_cast<unsigned>(add_time), serial.c_str(),
		static_cast<unsigned>(pt.total_played_time));

	std::unique_lock<std::recursive_mutex> lock(s_mutex);
	for (GameList::Entry& entry : s_entries)
	{
		if (entry.serial != serial)
			continue;

		entry.last_played_time = pt.last_played_time;
		entry.total_played_time = pt.total_played_time;
	}
}

void GameList::ClearPlayedTimeForSerial(const std::string& serial)
{
	if (serial.empty())
		return;

	UpdatePlayedTimeFile(GetPlayedTimeFile(), serial, 0, 0);

	std::unique_lock<std::recursive_mutex> lock(s_mutex);
	for (GameList::Entry& entry : s_entries)
	{
		if (entry.serial != serial)
			continue;

		entry.last_played_time = 0;
		entry.total_played_time = 0;
	}
}


std::time_t GameList::GetCachedPlayedTimeForSerial(const std::string& serial)
{
	if (serial.empty())
		return 0;

	std::unique_lock<std::recursive_mutex> lock(s_mutex);
	for (GameList::Entry& entry : s_entries)
	{
		if (entry.serial == serial)
			return entry.total_played_time;
	}

	return 0;
}

std::string GameList::FormatTimestamp(std::time_t timestamp)
{
	// TODO: All these strings should be translateable.
	std::string ret;

	if (timestamp == 0)
	{
		ret = "Never";
	}
	else
	{
		struct tm ctime = {};
		struct tm ttime = {};
		const std::time_t ctimestamp = std::time(nullptr);
#ifdef _MSC_VER
		localtime_s(&ctime, &ctimestamp);
		localtime_s(&ttime, &timestamp);
#else
		localtime_r(&ctimestamp, &ctime);
		localtime_r(&timestamp, &ttime);
#endif

		if (ctime.tm_year == ttime.tm_year && ctime.tm_yday == ttime.tm_yday)
		{
			ret = "Today";
		}
		else if ((ctime.tm_year == ttime.tm_year && ctime.tm_yday == (ttime.tm_yday + 1)) ||
				 (ctime.tm_yday == 0 && (ctime.tm_year - 1) == ttime.tm_year))
		{
			ret = "Yesterday";
		}
		else
		{
			char buf[128];
			std::strftime(buf, std::size(buf), "%x", &ttime);
			ret.assign(buf);
		}
	}

	return ret;
}

std::string GameList::FormatTimespan(std::time_t timespan, bool long_format)
{
	const u32 hours = static_cast<u32>(timespan / 3600);
	const u32 minutes = static_cast<u32>((timespan % 3600) / 60);
	const u32 seconds = static_cast<u32>((timespan % 3600) % 60);

	std::string ret;
	if (!long_format)
	{
		if (hours >= 100)
			ret = fmt::format("{}h {}m", hours, minutes);
		else if (hours > 0)
			ret = fmt::format("{}h {}m {}s", hours, minutes, seconds);
		else if (minutes > 0)
			ret = fmt::format("{}m {}s", minutes, seconds);
		else if (seconds > 0)
			ret = fmt::format("{}s", seconds);
		else
			ret = "None";
	}
	else
	{
		if (hours > 0)
			ret = fmt::format("{} hours", hours);
		else
			ret = fmt::format("{} minutes", minutes);
	}

	return ret;
}

std::string GameList::GetCoverImagePathForEntry(const Entry* entry)
{
	return GetCoverImagePath(entry->path, entry->serial, entry->title);
}

std::string GameList::GetCoverImagePath(const std::string& path, const std::string& serial, const std::string& title)
{
	static const char* extensions[] = {".jpg", ".jpeg", ".png", ".webp"};

	// TODO(Stenzek): Port to filesystem...

	std::string cover_path;
	for (const char* extension : extensions)
	{

		// Prioritize lookup by serial (Most specific)
		if (!serial.empty())
		{
			const std::string cover_filename(serial + extension);
			cover_path = Path::Combine(EmuFolders::Covers, cover_filename);
			if (FileSystem::FileExists(cover_path.c_str()))
				return cover_path;
		}

		// Try file title (for modded games or specific like above)
		const std::string_view file_title(Path::GetFileTitle(path));
		if (!file_title.empty() && title != file_title)
		{
			std::string cover_filename(file_title);
			cover_filename += extension;

			cover_path = Path::Combine(EmuFolders::Covers, cover_filename);
			if (FileSystem::FileExists(cover_path.c_str()))
				return cover_path;
		}

		// Last resort, check the game title
		if (!title.empty())
		{
			std::string cover_filename(title + extension);
			Path::SanitizeFileName(&cover_filename);
			cover_path = Path::Combine(EmuFolders::Covers, cover_filename);
			if (FileSystem::FileExists(cover_path.c_str()))
				return cover_path;
		}
	}

	cover_path.clear();
	return cover_path;
}

std::string GameList::GetNewCoverImagePathForEntry(const Entry* entry, const char* new_filename, bool use_serial)
{
	const char* extension = std::strrchr(new_filename, '.');
	if (!extension)
		return {};

	std::string existing_filename = GetCoverImagePathForEntry(entry);
	if (!existing_filename.empty())
	{
		std::string::size_type pos = existing_filename.rfind('.');
		if (pos != std::string::npos && existing_filename.compare(pos, std::strlen(extension), extension) == 0)
			return existing_filename;
	}

	std::string cover_filename(use_serial ? (entry->serial + extension) : (entry->title + extension));
	Path::SanitizeFileName(&cover_filename);
	return Path::Combine(EmuFolders::Covers, cover_filename);
}

bool GameList::DownloadCovers(const std::vector<std::string>& url_templates, bool use_serial, ProgressCallback* progress,
	std::function<void(const Entry*, std::string)> save_callback)
{
	if (!progress)
		progress = ProgressCallback::NullProgressCallback;

	bool has_title = false;
	bool has_file_title = false;
	bool has_serial = false;
	for (const std::string& url_template : url_templates)
	{
		if (!has_title && url_template.find("${title}") != std::string::npos)
			has_title = true;
		if (!has_file_title && url_template.find("${filetitle}") != std::string::npos)
			has_file_title = true;
		if (!has_serial && url_template.find("${serial}") != std::string::npos)
			has_serial = true;
	}
	if (!has_title && !has_file_title && !has_serial)
	{
		progress->DisplayError("URL template must contain at least one of ${title}, ${filetitle}, or ${serial}.");
		return false;
	}

	std::vector<std::pair<std::string, std::string>> download_urls;
	{
		std::unique_lock lock(s_mutex);
		for (const GameList::Entry& entry : s_entries)
		{
			const std::string existing_path(GetCoverImagePathForEntry(&entry));
			if (!existing_path.empty())
				continue;

			for (const std::string& url_template : url_templates)
			{
				std::string url(url_template);
				if (has_title)
					StringUtil::ReplaceAll(&url, "${title}", Common::HTTPDownloader::URLEncode(entry.title));
				if (has_file_title)
				{
					std::string display_name(FileSystem::GetDisplayNameFromPath(entry.path));
					StringUtil::ReplaceAll(&url, "${filetitle}", Common::HTTPDownloader::URLEncode(Path::GetFileTitle(display_name)));
				}
				if (has_serial)
					StringUtil::ReplaceAll(&url, "${serial}", Common::HTTPDownloader::URLEncode(entry.serial));

				download_urls.emplace_back(entry.path, std::move(url));
			}
		}
	}
	if (download_urls.empty())
	{
		progress->DisplayError("No URLs to download enumerated.");
		return false;
	}

	std::unique_ptr<Common::HTTPDownloader> downloader(Common::HTTPDownloader::Create());
	if (!downloader)
	{
		progress->DisplayError("Failed to create HTTP downloader.");
		return false;
	}

	progress->SetCancellable(true);
	progress->SetProgressRange(static_cast<u32>(download_urls.size()));

	for (auto& [entry_path, url] : download_urls)
	{
		if (progress->IsCancelled())
			break;

		// make sure it didn't get done already
		{
			std::unique_lock lock(s_mutex);
			const GameList::Entry* entry = GetEntryForPath(entry_path.c_str());
			if (!entry || !GetCoverImagePathForEntry(entry).empty())
			{
				progress->IncrementProgressValue();
				continue;
			}

			progress->SetFormattedStatusText("Downloading cover for %s [%s]...", entry->title.c_str(), entry->serial.c_str());
		}

		// we could actually do a few in parallel here...
		std::string filename(Common::HTTPDownloader::URLDecode(url));
		downloader->CreateRequest(
			std::move(url), [use_serial, &save_callback, entry_path = std::move(entry_path), filename = std::move(filename)](
								s32 status_code, const std::string& content_type, Common::HTTPDownloader::Request::Data data) {
				if (status_code != Common::HTTPDownloader::HTTP_OK || data.empty())
					return;

				std::unique_lock lock(s_mutex);
				const GameList::Entry* entry = GetEntryForPath(entry_path.c_str());
				if (!entry || !GetCoverImagePathForEntry(entry).empty())
					return;

				// prefer the content type from the response for the extension
				// otherwise, if it's missing, and the request didn't have an extension.. fall back to jpegs.
				std::string template_filename;
				std::string content_type_extension(Common::HTTPDownloader::GetExtensionForContentType(content_type));

				// don't treat the domain name as an extension..
				const std::string::size_type last_slash = filename.find('/');
				const std::string::size_type last_dot = filename.find('.');
				if (!content_type_extension.empty())
					template_filename = fmt::format("cover.{}", content_type_extension);
				else if (last_slash != std::string::npos && last_dot != std::string::npos && last_dot > last_slash)
					template_filename = Path::GetFileName(filename);
				else
					template_filename = "cover.jpg";

				std::string write_path(GetNewCoverImagePathForEntry(entry, template_filename.c_str(), use_serial));
				if (write_path.empty())
					return;

				if (FileSystem::WriteBinaryFile(write_path.c_str(), data.data(), data.size()) && save_callback)
					save_callback(entry, std::move(write_path));
			});
		downloader->WaitForAllRequests();
		progress->IncrementProgressValue();
	}

	return true;
}
