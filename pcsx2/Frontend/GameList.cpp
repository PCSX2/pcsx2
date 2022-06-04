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

#include "PrecompiledHeader.h"

#include "Frontend/GameList.h"
#include "HostSettings.h"
#include "common/Assertions.h"
#include "common/Console.h"
#include "common/FileSystem.h"
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

#include "CDVD/CDVD.h"
#include "Elfheader.h"
#include "VMManager.h"

enum : u32
{
	GAME_LIST_CACHE_SIGNATURE = 0x45434C47,
	GAME_LIST_CACHE_VERSION = 32
};

namespace GameList
{
	using CacheMap = std::unordered_map<std::string, GameList::Entry>;

	static Entry* GetMutableEntryForPath(const char* path);

	static bool GetElfListEntry(const std::string& path, GameList::Entry* entry);
	static bool GetIsoListEntry(const std::string& path, GameList::Entry* entry);

	static bool GetGameListEntryFromCache(const std::string& path, GameList::Entry* entry);
	static void ScanDirectory(const char* path, bool recursive, const std::vector<std::string>& excluded_paths,
		ProgressCallback* progress);
	static bool AddFileFromCache(const std::string& path, std::time_t timestamp);
	static bool ScanFile(std::string path, std::time_t timestamp);

	static void LoadCache();
	static bool LoadEntriesFromCache(std::FILE* stream);
	static bool OpenCacheForWriting();
	static bool WriteEntryToCache(const GameList::Entry* entry);
	static void CloseCacheFileStream();
	static void DeleteCacheFile();

	static void LoadDatabase();
} // namespace GameList

static std::vector<GameList::Entry> m_entries;
static std::recursive_mutex s_mutex;
static GameList::CacheMap m_cache_map;
static std::FILE* m_cache_write_stream = nullptr;

static bool m_game_list_loaded = false;

bool GameList::IsGameListLoaded()
{
	return m_game_list_loaded;
}

const char* GameList::EntryTypeToString(EntryType type)
{
	static std::array<const char*, static_cast<int>(EntryType::Count)> names = {
		{"PS2Disc", "PS1Disc", "ELF", "Playlist"}};
	return names[static_cast<int>(type)];
}

const char* GameList::RegionToString(Region region)
{
	static std::array<const char*, static_cast<int>(Region::Count)> names = {
		{"NTSC-B", "NTSC-C", "NTSC-HK", "NTSC-J", "NTSC-K", "NTSC-T", "NTSC-U",
		 "Other",
		 "PAL-A", "PAL-AF", "PAL-AU", "PAL-BE", "PAL-E", "PAL-F", "PAL-FI", "PAL-G", "PAL-GR", "PAL-I", "PAL-IN", "PAL-M", "PAL-NL", "PAL-NO", "PAL-P", "PAL-R", "PAL-S", "PAL-SC", "PAL-SW", "PAL-SWI", "PAL-UK"}};
		
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
	static const char* extensions[] = {".iso", ".mdf", ".nrg", ".bin", ".img", ".gz", ".cso", ".chd", ".elf", ".irx"};

	for (const char* test_extension : extensions)
	{
		if (StringUtil::EndsWithNoCase(path, test_extension))
			return true;
	}

	return false;
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
		params->filename.clear();
		params->source_type = CDVD_SourceType::NoDisc;
		params->elf_override = entry->path;
	}
	else
	{
		params->filename.clear();
		params->source_type = CDVD_SourceType::NoDisc;
		params->elf_override.clear();
	}
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

	const std::string display_name(FileSystem::GetDisplayNameFromPath(path));
	entry->path = path;
	entry->serial.clear();
	entry->title = Path::StripExtension(display_name);
	entry->region = Region::Other;
	entry->total_size = static_cast<u64>(file_size);
	entry->type = EntryType::ELF;
	entry->compatibility_rating = CompatibilityRating::Unknown;
	return true;
}
// clang-format off
bool GameList::GetIsoListEntry(const std::string& path, GameList::Entry* entry)
{
	FILESYSTEM_STAT_DATA sd;
	if (!FileSystem::StatFile(path.c_str(), &sd))
		return false;

	// This isn't great, we really want to make it all thread-local...
	CDVD = &CDVDapi_Iso;
	if (CDVD->open(path.c_str()) != 0)
		return false;

	const s32 type = DoCDVDdetectDiskType();
	switch (type)
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
			DoCDVDclose();
			return false;
	}

	cdvdReloadElfInfo();

	entry->path = path;
	entry->serial = DiscSerial;
	entry->crc = ElfCRC;
	entry->total_size = sd.Size;
	entry->compatibility_rating = CompatibilityRating::Unknown;

	DoCDVDclose();

	// TODO(Stenzek): These globals are **awful**. Clean it up.
	DiscSerial.clear();
	ElfCRC = 0;
	ElfEntry = -1;
	LastELF.clear();

	if (const GameDatabaseSchema::GameEntry* db_entry = GameDatabase::findGame(entry->serial))
	{
		entry->title = std::move(db_entry->name);
		entry->compatibility_rating = db_entry->compat;
							////// NTSC //////
							//////////////////
		if (StringUtil::StartsWith(db_entry->region, "NTSC-B"))
			entry->region = Region::NTSC_B;
		else if (StringUtil::StartsWith(db_entry->region, "NTSC-C"))
			entry->region = Region::NTSC_C;
		else if (StringUtil::StartsWith(db_entry->region, "NTSC-HK"))
			entry->region = Region::NTSC_HK;
		else if (StringUtil::StartsWith(db_entry->region, "NTSC-J"))
			entry->region = Region::NTSC_J;
		else if (StringUtil::StartsWith(db_entry->region, "NTSC-K"))
			entry->region = Region::NTSC_K;
		else if (StringUtil::StartsWith(db_entry->region, "NTSC-T"))
			entry->region = Region::NTSC_T;
		else if (StringUtil::StartsWith(db_entry->region, "NTSC-U"))
			entry->region = Region::NTSC_U;
							////// PAL //////
							//////////////////
		else if (StringUtil::StartsWith(db_entry->region, "PAL-AF"))
			entry->region = Region::PAL_AF;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-AU"))
			entry->region = Region::PAL_AU;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-A"))
			entry->region = Region::PAL_A;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-BE"))
			entry->region = Region::PAL_BE;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-E"))
			entry->region = Region::PAL_E;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-FI"))
			entry->region = Region::PAL_FI;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-F"))
			entry->region = Region::PAL_F;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-GR"))
			entry->region = Region::PAL_GR;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-G"))
			entry->region = Region::PAL_G;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-IN"))
			entry->region = Region::PAL_IN;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-I"))
			entry->region = Region::PAL_I;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-M"))
			entry->region = Region::PAL_M;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-NL"))
			entry->region = Region::PAL_NL;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-NO"))
			entry->region = Region::PAL_NO;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-P"))
			entry->region = Region::PAL_P;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-R"))
			entry->region = Region::PAL_R;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-SC"))
			entry->region = Region::PAL_SC;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-SWI"))
			entry->region = Region::PAL_SWI;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-SW"))
			entry->region = Region::PAL_SW;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-S"))
			entry->region = Region::PAL_S;
		else if (StringUtil::StartsWith(db_entry->region, "PAL-UK"))
			entry->region = Region::PAL_UK;
		else
			entry->region = Region::Other;
	}
	else
	{
		entry->title = Path::GetFileTitle(path);
		entry->region = Region::Other;
	}

	return true;
}
// clang-format off
bool GameList::PopulateEntryFromPath(const std::string& path, GameList::Entry* entry)
{
	if (VMManager::IsElfFileName(path.c_str()))
		return GetElfListEntry(path, entry);
	else
		return GetIsoListEntry(path, entry);
}

bool GameList::GetGameListEntryFromCache(const std::string& path, GameList::Entry* entry)
{
	auto iter = m_cache_map.find(path);
	if (iter == m_cache_map.end())
		return false;

	*entry = std::move(iter->second);
	m_cache_map.erase(iter);
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
	return (std::fwrite(&size, sizeof(size), 1, stream) > 0 &&
			(size == 0 || std::fwrite(str.data(), size, 1, stream) > 0));
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
	if (!ReadU32(stream, &file_signature) || !ReadU32(stream, &file_version) ||
		file_signature != GAME_LIST_CACHE_SIGNATURE || file_version != GAME_LIST_CACHE_VERSION ||
		(start_pos = FileSystem::FTell64(stream)) < 0 || FileSystem::FSeek64(stream, 0, SEEK_END) != 0 ||
		(file_size = FileSystem::FTell64(stream)) < 0 || FileSystem::FSeek64(stream, start_pos, SEEK_SET) != 0)
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

		if (!ReadString(stream, &path) || !ReadString(stream, &ge.serial) || !ReadString(stream, &ge.title) ||
			!ReadU8(stream, &type) || !ReadU8(stream, &region) || !ReadU64(stream, &ge.total_size) ||
			!ReadU64(stream, &last_modified_time) || !ReadU32(stream, &ge.crc) || !ReadU8(stream, &compatibility_rating) ||
			region >= static_cast<u8>(Region::Count) || type >= static_cast<u8>(EntryType::Count) ||
			compatibility_rating > static_cast<u8>(CompatibilityRating::Perfect))
		{
			Console.Warning("Game list cache entry is corrupted");
			return false;
		}

		ge.path = path;
		ge.region = static_cast<Region>(region);
		ge.type = static_cast<EntryType>(type);
		ge.compatibility_rating = static_cast<CompatibilityRating>(compatibility_rating);
		ge.last_modified_time = static_cast<std::time_t>(last_modified_time);

		auto iter = m_cache_map.find(ge.path);
		if (iter != m_cache_map.end())
			iter->second = std::move(ge);
		else
			m_cache_map.emplace(std::move(path), std::move(ge));
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
		m_cache_map.clear();
		DeleteCacheFile();
		return;
	}
}

bool GameList::OpenCacheForWriting()
{
	const std::string cache_filename(GetCacheFilename());
	if (cache_filename.empty())
		return false;

	pxAssert(!m_cache_write_stream);
	m_cache_write_stream = FileSystem::OpenCFile(cache_filename.c_str(), "r+b");
	if (m_cache_write_stream)
	{
		// check the header
		u32 signature, version;
		if (ReadU32(m_cache_write_stream, &signature) && signature == GAME_LIST_CACHE_SIGNATURE &&
			ReadU32(m_cache_write_stream, &version) && version == GAME_LIST_CACHE_VERSION &&
			FileSystem::FSeek64(m_cache_write_stream, 0, SEEK_END) == 0)
		{
			return true;
		}

		std::fclose(m_cache_write_stream);
	}

	Console.WriteLn("Creating new game list cache file: '%s'", cache_filename.c_str());

	m_cache_write_stream = FileSystem::OpenCFile(cache_filename.c_str(), "w+b");
	if (!m_cache_write_stream)
		return false;


	// new cache file, write header
	if (!WriteU32(m_cache_write_stream, GAME_LIST_CACHE_SIGNATURE) ||
		!WriteU32(m_cache_write_stream, GAME_LIST_CACHE_VERSION))
	{
		Console.Error("Failed to write game list cache header");
		std::fclose(m_cache_write_stream);
		m_cache_write_stream = nullptr;
		FileSystem::DeleteFilePath(cache_filename.c_str());
		return false;
	}

	return true;
}

bool GameList::WriteEntryToCache(const Entry* entry)
{
	bool result = true;
	result &= WriteString(m_cache_write_stream, entry->path);
	result &= WriteString(m_cache_write_stream, entry->serial);
	result &= WriteString(m_cache_write_stream, entry->title);
	result &= WriteU8(m_cache_write_stream, static_cast<u8>(entry->type));
	result &= WriteU8(m_cache_write_stream, static_cast<u8>(entry->region));
	result &= WriteU64(m_cache_write_stream, entry->total_size);
	result &= WriteU64(m_cache_write_stream, static_cast<u64>(entry->last_modified_time));
	result &= WriteU32(m_cache_write_stream, entry->crc);
	result &= WriteU8(m_cache_write_stream, static_cast<u8>(entry->compatibility_rating));

	// flush after each entry, that way we don't end up with a corrupted file if we crash scanning.
	if (result)
		result = (std::fflush(m_cache_write_stream) == 0);

	return result;
}

void GameList::CloseCacheFileStream()
{
	if (!m_cache_write_stream)
		return;

	std::fclose(m_cache_write_stream);
	m_cache_write_stream = nullptr;
}

void GameList::DeleteCacheFile()
{
	pxAssert(!m_cache_write_stream);

	const std::string cache_filename(GetCacheFilename());
	if (cache_filename.empty() || !FileSystem::FileExists(cache_filename.c_str()))
		return;

	if (FileSystem::DeleteFilePath(cache_filename.c_str()))
		Console.WriteLn("Deleted game list cache '%s'", cache_filename.c_str());
	else
		Console.Warning("Failed to delete game list cache '%s'", cache_filename.c_str());
}

static bool IsPathExcluded(const std::vector<std::string>& excluded_paths, const std::string& path)
{
	return (std::find(excluded_paths.begin(), excluded_paths.end(), path) != excluded_paths.end());
}

void GameList::ScanDirectory(const char* path, bool recursive, const std::vector<std::string>& excluded_paths,
	ProgressCallback* progress)
{
	Console.WriteLn("Scanning %s%s", path, recursive ? " (recursively)" : "");

	progress->PushState();
	progress->SetFormattedStatusText("Scanning directory '%s'%s...", path, recursive ? " (recursively)" : "");

	FileSystem::FindResultsArray files;
	FileSystem::FindFiles(path, "*",
		recursive ? (FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES | FILESYSTEM_FIND_RECURSIVE) :
                    (FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_HIDDEN_FILES),
		&files);

	progress->SetProgressRange(static_cast<u32>(files.size()));
	progress->SetProgressValue(0);

	for (FILESYSTEM_FIND_DATA& ffd : files)
	{
		if (progress->IsCancelled() || !GameList::IsScannableFilename(ffd.FileName) ||
			IsPathExcluded(excluded_paths, ffd.FileName))
		{
			continue;
		}

		{
			std::unique_lock lock(s_mutex);
			if (GetEntryForPath(ffd.FileName.c_str()) || AddFileFromCache(ffd.FileName, ffd.ModificationTime))
			{
				progress->IncrementProgressValue();
				continue;
			}
		}

		// ownership of fp is transferred
		progress->SetFormattedStatusText("Scanning '%s'...", FileSystem::GetDisplayNameFromPath(ffd.FileName).c_str());
		ScanFile(std::move(ffd.FileName), ffd.ModificationTime);
		progress->IncrementProgressValue();
	}

	progress->SetProgressValue(static_cast<u32>(files.size()));
	progress->PopState();
}

bool GameList::AddFileFromCache(const std::string& path, std::time_t timestamp)
{
	if (std::any_of(m_entries.begin(), m_entries.end(), [&path](const Entry& other) { return other.path == path; }))
	{
		// already exists
		return true;
	}

	Entry entry;
	if (!GetGameListEntryFromCache(path, &entry) || entry.last_modified_time != timestamp)
		return false;

	m_entries.push_back(std::move(entry));
	return true;
}

bool GameList::ScanFile(std::string path, std::time_t timestamp)
{
	DevCon.WriteLn("Scanning '%s'...", path.c_str());

	Entry entry;
	if (!PopulateEntryFromPath(path, &entry))
		return false;

	entry.path = std::move(path);
	entry.last_modified_time = timestamp;

	if (m_cache_write_stream || OpenCacheForWriting())
	{
		if (!WriteEntryToCache(&entry))
			Console.Warning("Failed to write entry '%s' to cache", entry.path.c_str());
	}

	std::unique_lock lock(s_mutex);
	m_entries.push_back(std::move(entry));
	return true;
}

std::unique_lock<std::recursive_mutex> GameList::GetLock()
{
	return std::unique_lock<std::recursive_mutex>(s_mutex);
}

const GameList::Entry* GameList::GetEntryByIndex(u32 index)
{
	return (index < m_entries.size()) ? &m_entries[index] : nullptr;
}

const GameList::Entry* GameList::GetEntryForPath(const char* path)
{
	const size_t path_length = std::strlen(path);
	for (const Entry& entry : m_entries)
	{
		if (entry.path.size() == path_length && StringUtil::Strcasecmp(entry.path.c_str(), path) == 0)
			return &entry;
	}

	return nullptr;
}

const GameList::Entry* GameList::GetEntryByCRC(u32 crc)
{
	for (const Entry& entry : m_entries)
	{
		if (entry.crc == crc)
			return &entry;
	}

	return nullptr;
}

const GameList::Entry* GameList::GetEntryBySerialAndCRC(const std::string_view& serial, u32 crc)
{
	for (const Entry& entry : m_entries)
	{
		if (entry.crc == crc && StringUtil::compareNoCase(entry.serial, serial))
			return &entry;
	}

	return nullptr;
}

GameList::Entry* GameList::GetMutableEntryForPath(const char* path)
{
	const size_t path_length = std::strlen(path);
	for (Entry& entry : m_entries)
	{
		if (entry.path.size() == path_length && StringUtil::Strcasecmp(entry.path.c_str(), path) == 0)
			return &entry;
	}

	return nullptr;
}

u32 GameList::GetEntryCount()
{
	return static_cast<u32>(m_entries.size());
}

void GameList::Refresh(bool invalidate_cache, ProgressCallback* progress /* = nullptr */)
{
	m_game_list_loaded = true;

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
		old_entries.swap(m_entries);
	}

	const std::vector<std::string> excluded_paths(Host::GetStringListSetting("GameList", "ExcludedPaths"));
	const std::vector<std::string> dirs(Host::GetStringListSetting("GameList", "Paths"));
	const std::vector<std::string> recursive_dirs(Host::GetStringListSetting("GameList", "RecursivePaths"));

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

			ScanDirectory(dir.c_str(), false, excluded_paths, progress);
			progress->SetProgressValue(++directory_counter);
		}
		for (const std::string& dir : recursive_dirs)
		{
			if (progress->IsCancelled())
				break;

			ScanDirectory(dir.c_str(), true, excluded_paths, progress);
			progress->SetProgressValue(++directory_counter);
		}
	}

	// don't need unused cache entries
	CloseCacheFileStream();
	m_cache_map.clear();
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
			const std::string cover_filename(title + extension);
			cover_path = Path::Combine(EmuFolders::Covers, cover_filename);
			if (FileSystem::FileExists(cover_path.c_str()))
				return cover_path;
		}
	}

	cover_path.clear();
	return cover_path;
}

std::string GameList::GetNewCoverImagePathForEntry(const Entry* entry, const char* new_filename)
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

	const std::string cover_filename(entry->title + extension);
	return Path::Combine(EmuFolders::Covers, cover_filename);
}
