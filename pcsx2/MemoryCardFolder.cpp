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
#include "common/SafeArray.inl"

#include "MemoryCardFile.h"
#include "MemoryCardFolder.h"

#include "System.h"
#include "Config.h"

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/StringUtil.h"
#include "common/Timer.h"

#include "fmt/core.h"
#include "ryml_std.hpp"
#include "ryml.hpp"

#include "svnrev.h"

#include <sstream>
#include <mutex>
#include <optional>

static ryml::Tree parseYamlStr(const std::string& str)
{
	ryml::Callbacks rymlCallbacks = ryml::get_callbacks();
	rymlCallbacks.m_error = [](const char* msg, size_t msg_len, ryml::Location loc, void*) {
		throw std::runtime_error(fmt::format("[YAML] Parsing error at {}:{} (bufpos={}): {}",
			loc.line, loc.col, loc.offset, msg));
	};
	ryml::set_callbacks(rymlCallbacks);
	c4::set_error_callback([](const char* msg, size_t msg_size) {
		throw std::runtime_error(fmt::format("[YAML] Internal Parsing error: {}",
			msg));
	});
	ryml::Tree tree = ryml::parse_in_arena(c4::to_csubstr(str));

	ryml::reset_callbacks();
	return tree;
}

// A helper function to parse the YAML file
static std::optional<ryml::Tree> loadYamlFile(const char* filePath)
{
	try
	{
		std::optional<std::string> buffer = FileSystem::ReadFileToString(filePath);
		if (!buffer.has_value())
		{
			return std::nullopt;
		}
		ryml::Tree tree = parseYamlStr(buffer.value());
		return std::make_optional(tree);
	}
	catch (const std::exception& e)
	{
		Console.Error(fmt::format("[MemoryCard] Error occured when parsing folder memory card at path '{}': {}", filePath, e.what()));
		ryml::reset_callbacks();
		return std::nullopt;
	}
}

/// A helper function to write a YAML file
static void SaveYAMLToFile(const char* filename, const ryml::NodeRef& node)
{
	auto file = FileSystem::OpenCFile(filename, "w");
	ryml::emit(node, file);
	std::fflush(file);
	std::fclose(file);
}

static constexpr time_t MEMORY_CARD_FILE_ENTRY_DATE_TIME_OFFSET = 60 * 60 * 9; // 9 hours from UTC

MemoryCardFileEntryDateTime MemoryCardFileEntryDateTime::FromTime(time_t time)
{
	// TODO: Is this safe with regard to DST?
	time += MEMORY_CARD_FILE_ENTRY_DATE_TIME_OFFSET;

	struct tm converted = {};
#ifdef _MSC_VER
	gmtime_s(&converted, &time);
#else
	gmtime_r(&time, &converted);
#endif

	MemoryCardFileEntryDateTime ret;
	ret.unused = 0;
	ret.second = converted.tm_sec;
	ret.minute = converted.tm_min;
	ret.hour = converted.tm_hour;
	ret.day = converted.tm_mday;
	ret.month = converted.tm_mon + 1;
	ret.year = converted.tm_year + 1900;
	return ret;
}

time_t MemoryCardFileEntryDateTime::ToTime() const
{
	struct tm converted = {};
	converted.tm_sec = second;
	converted.tm_min = minute;
	converted.tm_hour = hour;
	converted.tm_mday = day;
	converted.tm_mon = std::max(static_cast<int>(month) - 1, 0);
	converted.tm_year = std::max(static_cast<int>(year) - 1900, 0);
	return mktime(&converted);
}

FolderMemoryCard::FolderMemoryCard()
{
	m_slot = 0;
	m_isEnabled = false;
	m_performFileWrites = false;
	m_framesUntilFlush = 0;
	m_timeLastWritten = 0;
	m_filteringEnabled = false;
}

void FolderMemoryCard::InitializeInternalData()
{
	memset(&m_superBlock, 0xFF, sizeof(m_superBlock));
	memset(&m_indirectFat, 0xFF, sizeof(m_indirectFat));
	memset(&m_fat, 0xFF, sizeof(m_fat));
	memset(&m_backupBlock1, 0xFF, sizeof(m_backupBlock1));
	memset(&m_backupBlock2, 0xFF, sizeof(m_backupBlock2));
	m_cache.clear();
	m_oldDataCache.clear();
	m_lastAccessedFile.CloseAll();
	m_fileMetadataQuickAccess.clear();
	m_timeLastWritten = 0;
	m_isEnabled = false;
	m_framesUntilFlush = 0;
	m_performFileWrites = true;
	m_filteringEnabled = false;
	m_filteringString = {};
}

bool FolderMemoryCard::IsFormatted() const
{
	// this should be a good enough arbitrary check, if someone can think of a case where this doesn't work feel free to change
	return m_superBlock.raw[0x16] == 0x6F;
}

void FolderMemoryCard::Open(const bool enableFiltering, std::string filter)
{
	Open(EmuConfig.FullpathToMcd(m_slot), EmuConfig.Mcd[m_slot], 0, enableFiltering, std::move(filter), false);
}

void FolderMemoryCard::Open(std::string fullPath, const Pcsx2Config::McdOptions& mcdOptions, const u32 sizeInClusters, const bool enableFiltering, std::string filter, bool simulateFileWrites)
{
	InitializeInternalData();
	m_performFileWrites = !simulateFileWrites;

	m_folderName = Path::Canonicalize(fullPath);
	std::string_view str(fullPath);
	bool disabled = false;

	if (mcdOptions.Enabled && mcdOptions.Type == MemoryCardType::Folder)
	{
		if (fullPath.empty())
		{
			str = "[empty filename]";
			disabled = true;
		}
		if (!disabled && FileSystem::FileExists(fullPath.c_str()))
		{
			str = "[is file, should be folder]";
			disabled = true;
		}

		// if nothing exists at a valid location, create a directory for the memory card
		if (!disabled && m_performFileWrites && !FileSystem::DirectoryExists(fullPath.c_str()))
		{
			if (!FileSystem::CreateDirectoryPath(fullPath.c_str(), false))
			{
				str = "[couldn't create folder]";
				disabled = true;
			}
		}
	}
	else
	{
		// if the user has disabled this slot or is using a different memory card type, just return without a console log
		return;
	}

	Console.WriteLn(disabled ? Color_Gray : Color_Green, "McdSlot %u: [Folder] %.*s",
		m_slot, static_cast<int>(str.size()), str.data());
	if (disabled)
		return;

	m_isEnabled = true;
	m_filteringEnabled = enableFiltering;
	m_filteringString = std::move(filter);
	LoadMemoryCardData(sizeInClusters, enableFiltering, m_filteringString);

	SetTimeLastWrittenToNow();
	m_framesUntilFlush = 0;
}

void FolderMemoryCard::Close(bool flush)
{
	if (!m_isEnabled)
	{
		return;
	}

	if (flush)
	{
		Flush();
	}

	m_cache.clear();
	m_oldDataCache.clear();
	m_lastAccessedFile.CloseAll();
	m_fileMetadataQuickAccess.clear();
}

bool FolderMemoryCard::ReIndex(bool enableFiltering, const std::string& filter)
{
	if (!m_isEnabled)
	{
		return false;
	}

	if (m_filteringEnabled != enableFiltering || m_filteringString != filter)
	{
		Close();
		Open(enableFiltering, filter);
		return true;
	}

	return false;
}

void FolderMemoryCard::LoadMemoryCardData(const u32 sizeInClusters, const bool enableFiltering, const std::string& filter)
{
	bool formatted = false;

	// read superblock if it exists
	const std::string superBlockFileName(Path::Combine(m_folderName, "_pcsx2_superblock"));
	if (FileSystem::FileExists(superBlockFileName.c_str()))
	{
		auto superBlockFile = FileSystem::OpenManagedCFile(superBlockFileName.c_str(), "rb");
		if (superBlockFile && std::fread(&m_superBlock.raw, sizeof(m_superBlock.raw), 1, superBlockFile.get()) == 1)
		{
			formatted = IsFormatted();
		}
	}

	if (sizeInClusters > 0 && sizeInClusters != GetSizeInClusters())
	{
		SetSizeInClusters(sizeInClusters);
		FlushBlock(0);
	}

	// if superblock was valid, load folders and files
	if (formatted)
	{
		if (enableFiltering)
		{
			Console.WriteLn(Color_Green, "(FolderMcd) Indexing slot %u with filter \"%s\".", m_slot, filter.c_str());
		}
		else
		{
			Console.WriteLn(Color_Green, "(FolderMcd) Indexing slot %u without filter.", m_slot);
		}

		CreateFat();
		CreateRootDir();
		MemoryCardFileEntry* const rootDirEntry = &m_fileEntryDict[m_superBlock.data.rootdir_cluster].entries[0];
		AddFolder(rootDirEntry, m_folderName, nullptr, enableFiltering, filter);

#ifdef DEBUG_WRITE_FOLDER_CARD_IN_MEMORY_TO_FILE_ON_CHANGE
		WriteToFile(m_folderName.GetFullPath().RemoveLast() + L"-debug_" + wxDateTime::Now().Format(L"%Y-%m-%d-%H-%M-%S") + L"_load.ps2");
#endif
	}
}

void FolderMemoryCard::CreateFat()
{
	const u32 totalClusters = m_superBlock.data.clusters_per_card;
	const u32 clusterSize = m_superBlock.data.page_len * m_superBlock.data.pages_per_cluster;
	const u32 fatEntriesPerCluster = clusterSize / 4;
	const u32 countFatClusters = (totalClusters % fatEntriesPerCluster) != 0 ? (totalClusters / fatEntriesPerCluster + 1) : (totalClusters / fatEntriesPerCluster);
	const u32 countDataClusters = m_superBlock.data.alloc_end;

	// create indirect FAT
	for (unsigned int i = 0; i < countFatClusters; ++i)
	{
		m_indirectFat.data[0][i] = GetFreeSystemCluster();
	}

	// fill FAT with default values
	for (unsigned int i = 0; i < countDataClusters; ++i)
	{
		m_fat.data[0][0][i] = 0x7FFFFFFFu;
	}
}

void FolderMemoryCard::CreateRootDir()
{
	MemoryCardFileEntryCluster* const rootCluster = &m_fileEntryDict[m_superBlock.data.rootdir_cluster];
	memset(rootCluster->entries[0].entry.raw, 0x00, sizeof(rootCluster->entries[0].entry.raw));
	rootCluster->entries[0].entry.data.mode = MemoryCardFileEntry::Mode_Read | MemoryCardFileEntry::Mode_Write | MemoryCardFileEntry::Mode_Execute | MemoryCardFileEntry::Mode_Directory | MemoryCardFileEntry::Mode_Unknown0x0400 | MemoryCardFileEntry::Mode_Used;
	rootCluster->entries[0].entry.data.length = 2;
	rootCluster->entries[0].entry.data.name[0] = '.';

	memset(rootCluster->entries[1].entry.raw, 0x00, sizeof(rootCluster->entries[1].entry.raw));
	rootCluster->entries[1].entry.data.mode = MemoryCardFileEntry::Mode_Write | MemoryCardFileEntry::Mode_Execute | MemoryCardFileEntry::Mode_Directory | MemoryCardFileEntry::Mode_Unknown0x0400 | MemoryCardFileEntry::Mode_Unknown0x2000 | MemoryCardFileEntry::Mode_Used;
	rootCluster->entries[1].entry.data.name[0] = '.';
	rootCluster->entries[1].entry.data.name[1] = '.';

	// mark root dir cluster as used
	m_fat.data[0][0][m_superBlock.data.rootdir_cluster] = LastDataCluster | DataClusterInUseMask;
}

u32 FolderMemoryCard::GetFreeSystemCluster() const
{
	// first block is reserved for superblock
	u32 highestUsedCluster = (m_superBlock.data.pages_per_block / m_superBlock.data.pages_per_cluster) - 1;

	// can't use any of the indirect fat clusters
	for (int i = 0; i < IndirectFatClusterCount; ++i)
	{
		highestUsedCluster = std::max(highestUsedCluster, m_superBlock.data.ifc_list[i]);
	}

	// or fat clusters
	for (int i = 0; i < IndirectFatClusterCount; ++i)
	{
		for (int j = 0; j < ClusterSize / 4; ++j)
		{
			if (m_indirectFat.data[i][j] != IndirectFatUnused)
			{
				highestUsedCluster = std::max(highestUsedCluster, m_indirectFat.data[i][j]);
			}
		}
	}

	return highestUsedCluster + 1;
}

u32 FolderMemoryCard::GetAmountDataClusters() const
{
	// BIOS reports different cluster values than what the memory card actually has, match that when adding files
	//  8mb card -> BIOS:  7999 clusters / Superblock:  8135 clusters
	// 16mb card -> BIOS: 15999 clusters / Superblock: 16295 clusters
	// 32mb card -> BIOS: 31999 clusters / Superblock: 32615 clusters
	// 64mb card -> BIOS: 64999 clusters / Superblock: 65255 clusters
	return (m_superBlock.data.alloc_end / 1000) * 1000 - 1;
}

u32 FolderMemoryCard::GetFreeDataCluster() const
{
	const u32 countDataClusters = GetAmountDataClusters();

	for (unsigned int i = 0; i < countDataClusters; ++i)
	{
		const u32 cluster = m_fat.data[0][0][i];

		if ((cluster & DataClusterInUseMask) == 0)
		{
			return i;
		}
	}

	return 0xFFFFFFFFu;
}

u32 FolderMemoryCard::GetAmountFreeDataClusters() const
{
	const u32 countDataClusters = GetAmountDataClusters();
	u32 countFreeDataClusters = 0;

	for (unsigned int i = 0; i < countDataClusters; ++i)
	{
		const u32 cluster = m_fat.data[0][0][i];

		if ((cluster & DataClusterInUseMask) == 0)
		{
			++countFreeDataClusters;
		}
	}

	return countFreeDataClusters;
}

u32 FolderMemoryCard::GetLastClusterOfData(const u32 cluster) const
{
	u32 entryCluster;
	u32 nextCluster = cluster;
	do
	{
		entryCluster = nextCluster;
		nextCluster = m_fat.data[0][0][entryCluster] & NextDataClusterMask;
	} while (nextCluster != LastDataCluster);
	return entryCluster;
}

MemoryCardFileEntry* FolderMemoryCard::AppendFileEntryToDir(const MemoryCardFileEntry* const dirEntry)
{
	u32 entryCluster = GetLastClusterOfData(dirEntry->entry.data.cluster);

	MemoryCardFileEntry* newFileEntry;
	if (dirEntry->entry.data.length % 2 == 0)
	{
		// need new cluster
		u32 newCluster = GetFreeDataCluster();
		if (newCluster == 0xFFFFFFFFu)
		{
			return nullptr;
		}
		m_fat.data[0][0][entryCluster] = newCluster | DataClusterInUseMask;
		m_fat.data[0][0][newCluster] = LastDataCluster | DataClusterInUseMask;
		newFileEntry = &m_fileEntryDict[newCluster].entries[0];
	}
	else
	{
		// can use last page of existing clusters
		newFileEntry = &m_fileEntryDict[entryCluster].entries[1];
	}

	return newFileEntry;
}

static bool FilterMatches(const std::string_view& fileName, const std::string_view& filter)
{
	std::string_view::size_type start = 0;
	std::string_view::size_type len = filter.length();
	while (start < len)
	{
		std::string_view::size_type end = filter.find('/', start);
		if (end == std::string_view::npos)
		{
			end = len;
		}

		std::string_view singleFilter(filter.substr(start, end - start));
		if (fileName.find(singleFilter) != std::string_view::npos)
		{
			return true;
		}

		start = end + 1;
	}

	return false;
}

bool FolderMemoryCard::AddFolder(MemoryCardFileEntry* const dirEntry, const std::string& dirPath, MemoryCardFileMetadataReference* parent /* = nullptr */, const bool enableFiltering /* = false */, const std::string_view& filter /* = "" */)
{
	if (FileSystem::DirectoryExists(dirPath.c_str()))
	{
		std::string localFilter;
		if (enableFiltering)
		{
			bool hasFilter = !filter.empty();
			if (hasFilter)
			{
				localFilter = fmt::format("DATA-SYSTEM/BWNETCNF/{}", filter);
			}
			else
			{
				localFilter = "DATA-SYSTEM/BWNETCNF";
			}
		}

		int entryNumber = 2; // include . and ..
		for (const auto& file : GetOrderedFiles(dirPath))
		{
			if (file.m_isFile)
			{
				// don't load files in the root dir if we're filtering; no official software stores files there
				if (enableFiltering && parent == nullptr)
				{
					continue;
				}
				if (AddFile(dirEntry, dirPath, file, parent))
				{
					++entryNumber;
				}
			}
			else
			{
				// if possible filter added directories by game serial
				// this has the effective result of only files relevant to the current game being loaded into the memory card
				// which means every game essentially sees the memory card as if no other files exist
				if (enableFiltering && !FilterMatches(file.m_fileName, localFilter))
				{
					continue;
				}

				// is a subdirectory
				const std::string filePath(Path::Combine(dirPath, file.m_fileName));

				// make sure we have enough space on the memcard for the directory
				const u32 newNeededClusters = CalculateRequiredClustersOfDirectory(filePath) + ((dirEntry->entry.data.length % 2) == 0 ? 1 : 0);
				if (newNeededClusters > GetAmountFreeDataClusters())
				{
					Console.Warning(GetCardFullMessage(file.m_fileName));
					continue;
				}

				// add entry for subdir in parent dir
				MemoryCardFileEntry* newDirEntry = AppendFileEntryToDir(dirEntry);
				dirEntry->entry.data.length++;

				// set metadata
				const std::string metaFileName(Path::Combine(Path::Combine(dirPath, "_pcsx2_meta_directory"), file.m_fileName));
				if (auto metaFile = FileSystem::OpenManagedCFile(metaFileName.c_str(), "rb"); metaFile)
				{
					if (std::fread(&newDirEntry->entry.raw, 1, sizeof(newDirEntry->entry.raw), metaFile.get()) < 0x60)
					{
						StringUtil::Strlcpy(reinterpret_cast<char*>(newDirEntry->entry.data.name), file.m_fileName.c_str(), sizeof(newDirEntry->entry.data.name));
					}
				}
				else
				{
					newDirEntry->entry.data.mode = MemoryCardFileEntry::DefaultDirMode;
					newDirEntry->entry.data.timeCreated = MemoryCardFileEntryDateTime::FromTime(file.m_timeCreated);
					newDirEntry->entry.data.timeModified = MemoryCardFileEntryDateTime::FromTime(file.m_timeModified);
					StringUtil::Strlcpy(reinterpret_cast<char*>(newDirEntry->entry.data.name), file.m_fileName.c_str(), sizeof(newDirEntry->entry.data.name));
				}

				// create new cluster for . and .. entries
				newDirEntry->entry.data.length = 2;
				u32 newCluster = GetFreeDataCluster();
				m_fat.data[0][0][newCluster] = LastDataCluster | DataClusterInUseMask;
				newDirEntry->entry.data.cluster = newCluster;

				MemoryCardFileEntryCluster* const subDirCluster = &m_fileEntryDict[newCluster];
				memset(subDirCluster->entries[0].entry.raw, 0x00, sizeof(subDirCluster->entries[0].entry.raw));
				subDirCluster->entries[0].entry.data.mode = MemoryCardFileEntry::DefaultDirMode;
				subDirCluster->entries[0].entry.data.dirEntry = entryNumber;
				subDirCluster->entries[0].entry.data.name[0] = '.';

				memset(subDirCluster->entries[1].entry.raw, 0x00, sizeof(subDirCluster->entries[1].entry.raw));
				subDirCluster->entries[1].entry.data.mode = MemoryCardFileEntry::DefaultDirMode;
				subDirCluster->entries[1].entry.data.name[0] = '.';
				subDirCluster->entries[1].entry.data.name[1] = '.';

				MemoryCardFileMetadataReference* dirRef = AddDirEntryToMetadataQuickAccess(newDirEntry, parent);

				++entryNumber;

				// and add all files in subdir
				AddFolder(newDirEntry, filePath, dirRef);
			}
		}

		return true;
	}

	return false;
}

bool FolderMemoryCard::AddFile(MemoryCardFileEntry* const dirEntry, const std::string& dirPath, const EnumeratedFileEntry& fileEntry, MemoryCardFileMetadataReference* parent)
{
	const std::string filePath(Path::Combine(dirPath, fileEntry.m_fileName));
	pxAssertMsg(StringUtil::StartsWith(filePath, m_folderName.c_str()), "Full file path starts with MC folder path");
	const std::string relativeFilePath(filePath.substr(m_folderName.length() + 1));

	if (auto file = FileSystem::OpenManagedCFile(filePath.c_str(), "rb"); file)
	{
		// make sure we have enough space on the memcard to hold the data
		const u32 clusterSize = m_superBlock.data.pages_per_cluster * m_superBlock.data.page_len;
		const u32 filesize = static_cast<u32>(std::clamp<s64>(FileSystem::FSize64(file.get()), 0, std::numeric_limits<u32>::max()));
		const u32 countClusters = (filesize % clusterSize) != 0 ? (filesize / clusterSize + 1) : (filesize / clusterSize);
		const u32 newNeededClusters = (dirEntry->entry.data.length % 2) == 0 ? countClusters + 1 : countClusters;
		if (newNeededClusters > GetAmountFreeDataClusters())
		{
			Console.Warning(GetCardFullMessage(relativeFilePath));
			return false;
		}

		MemoryCardFileEntry* newFileEntry = AppendFileEntryToDir(dirEntry);

		// set file entry metadata
		memset(newFileEntry->entry.raw, 0x00, sizeof(newFileEntry->entry.raw));

		std::string metaFileName(Path::Combine(Path::Combine(dirPath, "_pcsx2_meta"), fileEntry.m_fileName));
		if (auto metaFile = FileSystem::OpenManagedCFile(metaFileName.c_str(), "rb"); metaFile)
		{
			size_t bytesRead = std::fread(&newFileEntry->entry.raw, 1, sizeof(newFileEntry->entry.raw), metaFile.get());
			if (bytesRead < 0x60)
			{
				StringUtil::Strlcpy(reinterpret_cast<char*>(newFileEntry->entry.data.name), fileEntry.m_fileName.c_str(), sizeof(newFileEntry->entry.data.name));
			}
		}
		else
		{
			newFileEntry->entry.data.mode = MemoryCardFileEntry::DefaultFileMode;
			newFileEntry->entry.data.timeCreated = MemoryCardFileEntryDateTime::FromTime(fileEntry.m_timeCreated);
			newFileEntry->entry.data.timeModified = MemoryCardFileEntryDateTime::FromTime(fileEntry.m_timeModified);
			StringUtil::Strlcpy(reinterpret_cast<char*>(newFileEntry->entry.data.name), fileEntry.m_fileName.c_str(), sizeof(newFileEntry->entry.data.name));
		}

		newFileEntry->entry.data.length = filesize;
		if (filesize != 0)
		{
			u32 fileDataStartingCluster = GetFreeDataCluster();
			newFileEntry->entry.data.cluster = fileDataStartingCluster;

			// mark the appropriate amount of clusters as used
			u32 dataCluster = fileDataStartingCluster;
			m_fat.data[0][0][dataCluster] = LastDataCluster | DataClusterInUseMask;
			for (unsigned int i = 0; i < countClusters - 1; ++i)
			{
				u32 newCluster = GetFreeDataCluster();
				m_fat.data[0][0][dataCluster] = newCluster | DataClusterInUseMask;
				m_fat.data[0][0][newCluster] = LastDataCluster | DataClusterInUseMask;
				dataCluster = newCluster;
			}
		}
		else
		{
			newFileEntry->entry.data.cluster = MemoryCardFileEntry::EmptyFileCluster;
		}

		file.reset();

		MemoryCardFileMetadataReference* fileRef = AddFileEntryToMetadataQuickAccess(newFileEntry, parent);
		if (fileRef != nullptr)
		{
			// acquire a handle on the file so nothing else can change the file contents while the memory card is open
			m_lastAccessedFile.ReOpen(m_folderName, fileRef);
		}

		// and finally, increase file count in the directory entry
		dirEntry->entry.data.length++;

		return true;
	}
	else
	{
		Console.WriteLn("(FolderMcd) Could not open file: %s", relativeFilePath.c_str());
		return false;
	}
}

u32 FolderMemoryCard::CalculateRequiredClustersOfDirectory(const std::string& dirPath) const
{
	const u32 clusterSize = m_superBlock.data.pages_per_cluster * m_superBlock.data.page_len;
	u32 requiredFileEntryPages = 2;
	u32 requiredClusters = 0;

	// No need to read the index file as we are only counting space required; order of files is irrelevant.
	FileSystem::FindResultsArray files;
	FileSystem::FindFiles(dirPath.c_str(), "*", FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_FOLDERS | FILESYSTEM_FIND_HIDDEN_FILES | FILESYSTEM_FIND_RELATIVE_PATHS, &files);
	for (const FILESYSTEM_FIND_DATA& fd : files)
	{
		if (StringUtil::StartsWith(fd.FileName, "_pcsx2_"))
			continue;

		++requiredFileEntryPages;

		if (!(fd.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY))
		{
			const u32 filesize = static_cast<u32>(std::min<s64>(fd.Size, std::numeric_limits<u32>::max()));
			const u32 countClusters = (filesize % clusterSize) != 0 ? (filesize / clusterSize + 1) : (filesize / clusterSize);
			requiredClusters += countClusters;
		}
		else
		{
			requiredClusters += CalculateRequiredClustersOfDirectory(Path::Combine(dirPath, fd.FileName));
		}
	}

	return requiredClusters + requiredFileEntryPages / 2 + (requiredFileEntryPages % 2 == 0 ? 0 : 1);
}

MemoryCardFileMetadataReference* FolderMemoryCard::AddDirEntryToMetadataQuickAccess(MemoryCardFileEntry* const entry, MemoryCardFileMetadataReference* const parent)
{
	MemoryCardFileMetadataReference* ref = &m_fileMetadataQuickAccess[entry->entry.data.cluster];
	ref->parent = parent;
	ref->entry = entry;
	ref->consecutiveCluster = 0xFFFFFFFFu;
	return ref;
}

MemoryCardFileMetadataReference* FolderMemoryCard::AddFileEntryToMetadataQuickAccess(MemoryCardFileEntry* const entry, MemoryCardFileMetadataReference* const parent)
{
	const u32 firstFileCluster = entry->entry.data.cluster;
	u32 fileCluster = firstFileCluster;

	// zero-length files have no file clusters
	if (fileCluster == 0xFFFFFFFFu)
	{
		return nullptr;
	}

	u32 clusterNumber = 0;
	do
	{
		MemoryCardFileMetadataReference* ref = &m_fileMetadataQuickAccess[fileCluster & NextDataClusterMask];
		ref->parent = parent;
		ref->entry = entry;
		ref->consecutiveCluster = clusterNumber;
		++clusterNumber;
	} while ((fileCluster = m_fat.data[0][0][fileCluster & NextDataClusterMask]) != (LastDataCluster | DataClusterInUseMask));

	return &m_fileMetadataQuickAccess[firstFileCluster & NextDataClusterMask];
}

s32 FolderMemoryCard::IsPresent() const
{
	return m_isEnabled;
}

void FolderMemoryCard::GetSizeInfo(McdSizeInfo& outways) const
{
	outways.SectorSize = PageSize;
	outways.EraseBlockSizeInSectors = BlockSize / PageSize;
	outways.McdSizeInSectors = GetSizeInClusters() * 2;

	u8* pdata = (u8*)&outways.McdSizeInSectors;
	outways.Xor = 18;
	outways.Xor ^= pdata[0] ^ pdata[1] ^ pdata[2] ^ pdata[3];
}

bool FolderMemoryCard::IsPSX() const
{
	return false;
}

u8* FolderMemoryCard::GetSystemBlockPointer(const u32 adr)
{
	const u32 block = adr / BlockSizeRaw;
	const u32 page = adr / PageSizeRaw;
	const u32 offset = adr % PageSizeRaw;
	const u32 cluster = adr / ClusterSizeRaw;

	const u32 startDataCluster = m_superBlock.data.alloc_offset;
	const u32 endDataCluster = startDataCluster + m_superBlock.data.alloc_end;
	if (cluster >= startDataCluster && cluster < endDataCluster)
	{
		// trying to access a file entry?
		const u32 fatCluster = cluster - m_superBlock.data.alloc_offset;
		// if this cluster is unused according to FAT, we can assume we won't find anything
		if ((m_fat.data[0][0][fatCluster] & DataClusterInUseMask) == 0)
		{
			return nullptr;
		}
		return GetFileEntryPointer(fatCluster, page % 2, offset);
	}

	if (block == 0)
	{
		return &m_superBlock.raw[page * PageSize + offset];
	}
	else if (block == m_superBlock.data.backup_block1)
	{
		return &m_backupBlock1[(page % 16) * PageSize + offset];
	}
	else if (block == m_superBlock.data.backup_block2)
	{
		return &m_backupBlock2.raw[(page % 16) * PageSize + offset];
	}
	else
	{
		// trying to access indirect FAT?
		for (int i = 0; i < IndirectFatClusterCount; ++i)
		{
			if (cluster == m_superBlock.data.ifc_list[i])
			{
				return &m_indirectFat.raw[i][(page % 2) * PageSize + offset];
			}
		}
		// trying to access FAT?
		for (int i = 0; i < IndirectFatClusterCount; ++i)
		{
			for (int j = 0; j < ClusterSize / 4; ++j)
			{
				const u32 fatCluster = m_indirectFat.data[i][j];
				if (fatCluster != IndirectFatUnused && fatCluster == cluster)
				{
					return &m_fat.raw[i][j][(page % 2) * PageSize + offset];
				}
			}
		}
	}

	return nullptr;
}

u8* FolderMemoryCard::GetFileEntryPointer(const u32 searchCluster, const u32 entryNumber, const u32 offset)
{
	const u32 fileCount = m_fileEntryDict[m_superBlock.data.rootdir_cluster].entries[0].entry.data.length;
	MemoryCardFileEntryCluster* ptr = GetFileEntryCluster(m_superBlock.data.rootdir_cluster, searchCluster, fileCount);
	if (ptr != nullptr)
	{
		return &ptr->entries[entryNumber].entry.raw[offset];
	}

	return nullptr;
}

MemoryCardFileEntryCluster* FolderMemoryCard::GetFileEntryCluster(const u32 currentCluster, const u32 searchCluster, const u32 fileCount)
{
	// we found the correct cluster, return pointer to it
	if (currentCluster == searchCluster)
	{
		return &m_fileEntryDict[currentCluster];
	}

	// check other clusters of this directory
	const u32 nextCluster = m_fat.data[0][0][currentCluster] & NextDataClusterMask;
	if (nextCluster != LastDataCluster)
	{
		MemoryCardFileEntryCluster* ptr = GetFileEntryCluster(nextCluster, searchCluster, fileCount - 2);
		if (ptr != nullptr)
		{
			return ptr;
		}
	}

	// check subdirectories
	auto it = m_fileEntryDict.find(currentCluster);
	if (it != m_fileEntryDict.end())
	{
		const u32 filesInThisCluster = std::min(fileCount, 2u);
		for (unsigned int i = 0; i < filesInThisCluster; ++i)
		{
			const MemoryCardFileEntry* const entry = &it->second.entries[i];
			if (entry->IsValid() && entry->IsUsed() && entry->IsDir() && !entry->IsDotDir())
			{
				const u32 newFileCount = entry->entry.data.length;
				MemoryCardFileEntryCluster* ptr = GetFileEntryCluster(entry->entry.data.cluster, searchCluster, newFileCount);
				if (ptr != nullptr)
				{
					return ptr;
				}
			}
		}
	}

	return nullptr;
}

// This method is actually unused since the introduction of m_fileMetadataQuickAccess.
// I'll leave it here anyway though to show how you traverse the file system.
MemoryCardFileEntry* FolderMemoryCard::GetFileEntryFromFileDataCluster(const u32 currentCluster, const u32 searchCluster, std::string* fileName, const size_t originalDirCount, u32* outClusterNumber)
{
	// check both entries of the current cluster if they're the file we're searching for, and if yes return it
	for (int i = 0; i < 2; ++i)
	{
		MemoryCardFileEntry* const entry = &m_fileEntryDict[currentCluster].entries[i];
		if (entry->IsValid() && entry->IsUsed() && entry->IsFile())
		{
			u32 fileCluster = entry->entry.data.cluster;
			u32 clusterNumber = 0;
			do
			{
				if (fileCluster == searchCluster)
				{
					Path::ChangeFileName(fileName, (const char*)entry->entry.data.name);
					*outClusterNumber = clusterNumber;
					return entry;
				}
				++clusterNumber;
			} while ((fileCluster = m_fat.data[0][0][fileCluster] & NextDataClusterMask) != LastDataCluster);
		}
	}

	// check other clusters of this directory
	// this can probably be solved more efficiently by looping through nextClusters instead of recursively calling
	const u32 nextCluster = m_fat.data[0][0][currentCluster] & NextDataClusterMask;
	if (nextCluster != LastDataCluster)
	{
		MemoryCardFileEntry* ptr = GetFileEntryFromFileDataCluster(nextCluster, searchCluster, fileName, originalDirCount, outClusterNumber);
		if (ptr != nullptr)
		{
			return ptr;
		}
	}

	// check subdirectories
	for (int i = 0; i < 2; ++i)
	{
		MemoryCardFileEntry* const entry = &m_fileEntryDict[currentCluster].entries[i];
		if (entry->IsValid() && entry->IsUsed() && entry->IsDir() && !entry->IsDotDir())
		{
			MemoryCardFileEntry* ptr = GetFileEntryFromFileDataCluster(entry->entry.data.cluster, searchCluster, fileName, originalDirCount, outClusterNumber);
			if (ptr != nullptr)
			{
				std::vector<std::string_view> components(Path::SplitNativePath(*fileName));
				components.insert(components.begin() + originalDirCount, (const char*)entry->entry.data.name);
				*fileName = Path::JoinNativePath(components);
				return ptr;
			}
		}
	}

	return nullptr;
}

bool FolderMemoryCard::ReadFromFile(u8* dest, u32 adr, u32 dataLength)
{
	const u32 page = adr / PageSizeRaw;
	const u32 offset = adr % PageSizeRaw;
	const u32 cluster = adr / ClusterSizeRaw;
	const u32 fatCluster = cluster - m_superBlock.data.alloc_offset;

	// if the cluster is unused according to FAT, just return
	if ((m_fat.data[0][0][fatCluster] & DataClusterInUseMask) == 0)
	{
		return false;
	}

	// figure out which file to read from
	auto it = m_fileMetadataQuickAccess.find(fatCluster);
	if (it != m_fileMetadataQuickAccess.end())
	{
		const u32 clusterNumber = it->second.consecutiveCluster;
		std::FILE* file = m_lastAccessedFile.ReOpen(m_folderName, &it->second);
		if (file)
		{
			const u32 clusterOffset = (page % 2) * PageSize + offset;
			const u32 fileOffset = clusterNumber * ClusterSize + clusterOffset;

			size_t bytesRead = 0;
			if (fileOffset == FileSystem::FTell64(file) || FileSystem::FSeek64(file, fileOffset, SEEK_SET) == 0)
				bytesRead = std::fread(dest, 1, dataLength, file);

			// if more bytes were requested than actually exist, fill the rest with 0xFF
			if (bytesRead < dataLength)
			{
				memset(&dest[bytesRead], 0xFF, dataLength - bytesRead);
			}

			return bytesRead > 0;
		}
	}

	return false;
}

s32 FolderMemoryCard::Read(u8* dest, u32 adr, int size)
{
	//const u32 block = adr / BlockSizeRaw;
	const u32 page = adr / PageSizeRaw;
	const u32 offset = adr % PageSizeRaw;
	//const u32 cluster = adr / ClusterSizeRaw;
	const u32 end = offset + size;

	if (end > PageSizeRaw)
	{
		// is trying to read more than one page at a time
		// do this recursively so that each function call only has to care about one page
		const u32 toNextPage = PageSizeRaw - offset;
		Read(dest + toNextPage, adr + toNextPage, size - toNextPage);
		size = toNextPage;
	}

	if (offset < PageSize)
	{
		// is trying to read (part of) an actual data block
		const u32 dataLength = std::min((u32)size, (u32)(PageSize - offset));

		// if we have a cache for this page, just load from that
		auto it = m_cache.find(page);
		if (it != m_cache.end())
		{
			memcpy(dest, &it->second.raw[offset], dataLength);
		}
		else
		{
			ReadDataWithoutCache(dest, adr, dataLength);
		}
	}

	if (end > PageSize)
	{
		// is trying to (partially) read the ECC
		const u32 eccOffset = PageSize - offset;
		const u32 eccLength = std::min((u32)(size - offset), (u32)EccSize);
		const u32 adrStart = page * PageSizeRaw;

		u8 data[PageSize];
		Read(data, adrStart, PageSize);

		u8 ecc[EccSize];
		memset(ecc, 0xFF, EccSize);

		for (int i = 0; i < PageSize / 0x80; ++i)
		{
			FolderMemoryCard::CalculateECC(ecc + (i * 3), &data[i * 0x80]);
		}

		memcpy(dest + eccOffset, ecc, eccLength);
	}

	SetTimeLastReadToNow();

	// return 0 on fail, 1 on success?
	return 1;
}

void FolderMemoryCard::ReadDataWithoutCache(u8* const dest, const u32 adr, const u32 dataLength)
{
	u8* src = GetSystemBlockPointer(adr);
	if (src != nullptr)
	{
		memcpy(dest, src, dataLength);
	}
	else
	{
		if (!ReadFromFile(dest, adr, dataLength))
		{
			memset(dest, 0xFF, dataLength);
		}
	}
}

s32 FolderMemoryCard::Save(const u8* src, u32 adr, int size)
{
	//const u32 block = adr / BlockSizeRaw;
	//const u32 cluster = adr / ClusterSizeRaw;
	const u32 page = adr / PageSizeRaw;
	const u32 offset = adr % PageSizeRaw;
	const u32 end = offset + size;

	if (end > PageSizeRaw)
	{
		// is trying to store more than one page at a time
		// do this recursively so that each function call only has to care about one page
		const u32 toNextPage = PageSizeRaw - offset;
		Save(src + toNextPage, adr + toNextPage, size - toNextPage);
		size = toNextPage;
	}

	if (offset < PageSize)
	{
		// is trying to store (part of) an actual data block
		const u32 dataLength = std::min((u32)size, PageSize - offset);

		// if cache page has not yet been touched, fill it with the data from our memory card
		auto it = m_cache.find(page);
		MemoryCardPage* cachePage;
		if (it == m_cache.end())
		{
			cachePage = &m_cache[page];
			const u32 adrLoad = page * PageSizeRaw;
			ReadDataWithoutCache(&cachePage->raw[0], adrLoad, PageSize);
			memcpy(&m_oldDataCache[page].raw[0], &cachePage->raw[0], PageSize);
		}
		else
		{
			cachePage = &it->second;
		}

		// then just write to the cache
		memcpy(&cachePage->raw[offset], src, dataLength);

		SetTimeLastWrittenToNow();
	}

	return 1;
}

void FolderMemoryCard::NextFrame()
{
	if (m_framesUntilFlush > 0 && --m_framesUntilFlush == 0)
	{
		Flush();
	}
}

void FolderMemoryCard::Flush()
{
	if (m_cache.empty())
	{
		return;
	}

#ifdef DEBUG_WRITE_FOLDER_CARD_IN_MEMORY_TO_FILE_ON_CHANGE
	WriteToFile(m_folderName.GetFullPath().RemoveLast() + L"-debug_" + wxDateTime::Now().Format(L"%Y-%m-%d-%H-%M-%S") + L"_pre-flush.ps2");
#endif

	Console.WriteLn("(FolderMcd) Writing data for slot %u to file system...", m_slot);
	Common::Timer timeFlushStart;

	// Keep a copy of the old file entries so we can figure out which files and directories, if any, have been deleted from the memory card.
	std::vector<MemoryCardFileEntryTreeNode> oldFileEntryTree;
	if (IsFormatted())
	{
		CopyEntryDictIntoTree(&oldFileEntryTree, m_superBlock.data.rootdir_cluster, m_fileEntryDict[m_superBlock.data.rootdir_cluster].entries[0].entry.data.length);
	}

	// first write the superblock if necessary
	FlushSuperBlock();
	if (!IsFormatted())
	{
		return;
	}

	// check if we were interrupted in the middle of a save operation, if yes abort
	FlushBlock(m_superBlock.data.backup_block1);
	FlushBlock(m_superBlock.data.backup_block2);
	if (m_backupBlock2.programmedBlock != 0xFFFFFFFFu)
	{
		Console.Warning("(FolderMcd) Aborting flush of slot %u, emulation was interrupted during save process!", m_slot);
		return;
	}

	const u32 clusterCount = GetSizeInClusters();
	const u32 pageCount = clusterCount * 2;

	// then write the indirect FAT
	for (int i = 0; i < IndirectFatClusterCount; ++i)
	{
		const u32 cluster = m_superBlock.data.ifc_list[i];
		if (cluster > 0 && cluster < clusterCount)
		{
			FlushCluster(cluster);
		}
	}

	// and the FAT
	for (int i = 0; i < IndirectFatClusterCount; ++i)
	{
		for (int j = 0; j < ClusterSize / 4; ++j)
		{
			const u32 cluster = m_indirectFat.data[i][j];
			if (cluster > 0 && cluster < clusterCount)
			{
				FlushCluster(cluster);
			}
		}
	}

	// then all directory and file entries
	FlushFileEntries();

	// Now we have the new file system, compare it to the old one and "delete" any files that were in it before but aren't anymore.
	FlushDeletedFilesAndRemoveUnchangedDataFromCache(oldFileEntryTree);

	// and finally, flush everything that hasn't been flushed yet
	for (uint i = 0; i < pageCount; ++i)
	{
		FlushPage(i);
	}

	m_lastAccessedFile.FlushAll();
	m_lastAccessedFile.ClearMetadataWriteState();
	m_oldDataCache.clear();

	Console.WriteLn("(FolderMcd) Done! Took %.2f ms.", timeFlushStart.GetTimeMilliseconds());

#ifdef DEBUG_WRITE_FOLDER_CARD_IN_MEMORY_TO_FILE_ON_CHANGE
	WriteToFile(m_folderName.GetFullPath().RemoveLast() + L"-debug_" + wxDateTime::Now().Format(L"%Y-%m-%d-%H-%M-%S") + L"_post-flush.ps2");
#endif
}

bool FolderMemoryCard::FlushPage(const u32 page)
{
	auto it = m_cache.find(page);
	if (it != m_cache.end())
	{
		WriteWithoutCache(&it->second.raw[0], page * PageSizeRaw, PageSize);
		m_cache.erase(it);
		return true;
	}
	return false;
}

bool FolderMemoryCard::FlushCluster(const u32 cluster)
{
	const u32 page = cluster * 2;
	bool flushed = false;
	if (FlushPage(page))
	{
		flushed = true;
	}
	if (FlushPage(page + 1))
	{
		flushed = true;
	}
	return flushed;
}

bool FolderMemoryCard::FlushBlock(const u32 block)
{
	const u32 page = block * 16;
	bool flushed = false;
	for (int i = 0; i < 16; ++i)
	{
		if (FlushPage(page + i))
		{
			flushed = true;
		}
	}
	return flushed;
}

void FolderMemoryCard::FlushSuperBlock()
{
	if (FlushBlock(0) && m_performFileWrites)
	{
		const std::string superBlockFileName(Path::Combine(m_folderName, "_pcsx2_superblock"));
		if (auto superBlockFile = FileSystem::OpenManagedCFile(superBlockFileName.c_str(), "wb"); superBlockFile)
		{
			std::fwrite(&m_superBlock.raw, sizeof(m_superBlock.raw), 1, superBlockFile.get());
		}
	}
}

void FolderMemoryCard::FlushFileEntries()
{
	// Flush all file entry data from the cache into m_fileEntryDict.
	const u32 rootDirCluster = m_superBlock.data.rootdir_cluster;
	FlushCluster(rootDirCluster + m_superBlock.data.alloc_offset);
	MemoryCardFileEntryCluster* rootEntries = &m_fileEntryDict[rootDirCluster];
	if (rootEntries->entries[0].IsValid() && rootEntries->entries[0].IsUsed())
	{
		FlushFileEntries(rootDirCluster, rootEntries->entries[0].entry.data.length);
	}
}

void FolderMemoryCard::FlushFileEntries(const u32 dirCluster, const u32 remainingFiles, const std::string& dirPath, MemoryCardFileMetadataReference* parent)
{
	// flush the current cluster
	FlushCluster(dirCluster + m_superBlock.data.alloc_offset);

	// if either of the current entries is a subdir, flush that too
	MemoryCardFileEntryCluster* entries = &m_fileEntryDict[dirCluster];
	const u32 filesInThisCluster = std::min(remainingFiles, 2u);
	for (unsigned int i = 0; i < filesInThisCluster; ++i)
	{
		MemoryCardFileEntry* entry = &entries->entries[i];
		if (entry->IsValid() && entry->IsUsed())
		{
			if (entry->IsDir())
			{
				if (!entry->IsDotDir())
				{
					char cleanName[sizeof(entry->entry.data.name)];
					memcpy(cleanName, (const char*)entry->entry.data.name, sizeof(cleanName));
					bool filenameCleaned = FileAccessHelper::CleanMemcardFilename(cleanName);
					const std::string subDirPath(Path::Combine(dirPath, cleanName));

					if (m_performFileWrites)
					{
						// if this directory has nonstandard metadata, write that to the file system
						const std::string fullSubDirPath(Path::Combine(m_folderName, subDirPath));
						std::string metaFileName(Path::Combine(fullSubDirPath, "_pcsx2_meta_directory"));
						if (!FileSystem::DirectoryExists(fullSubDirPath.c_str()))
						{
							FileSystem::CreateDirectoryPath(fullSubDirPath.c_str(), false);
						}

						// TODO: This logic doesn't make sense. If it's not a directory, create it, then open it as a file?!
						if (filenameCleaned || entry->entry.data.mode != MemoryCardFileEntry::DefaultDirMode || entry->entry.data.attr != 0)
						{
							if (auto metaFile = FileSystem::OpenManagedCFile(metaFileName.c_str(), "wb"); metaFile)
							{
								std::fwrite(entry->entry.raw, sizeof(entry->entry.raw), 1, metaFile.get());
							}
						}
						else
						{
							// if metadata is standard make sure to remove a possibly existing metadata file
							if (FileSystem::FileExists(metaFileName.c_str()))
							{
								FileSystem::DeleteFilePath(metaFileName.c_str());
							}
						}

						// write the directory index
						metaFileName = Path::Combine(fullSubDirPath, "_pcsx2_index");
						std::optional<ryml::Tree> yaml = loadYamlFile(metaFileName.c_str());

						// if _pcsx2_index hasn't been made yet, start a new file
						if (!yaml.has_value())
						{
							char initialData[] = "{$ROOT: {timeCreated: 0, timeModified: 0}}";
							ryml::Tree newYaml = ryml::parse_in_arena(c4::to_csubstr(initialData));
							ryml::NodeRef newNode = newYaml.rootref()["$ROOT"];
							newNode["timeCreated"] << entry->entry.data.timeCreated.ToTime();
							newNode["timeModified"] << entry->entry.data.timeModified.ToTime();
							SaveYAMLToFile(metaFileName.c_str(), newYaml);
						}
						else if (!yaml.value().empty())
						{
							ryml::NodeRef index = yaml.value().rootref();

							// Detect broken index files, every index file should have atleast ONE child ('[$%]ROOT')
							if (!index.has_children())
							{
								AttemptToRecreateIndexFile(fullSubDirPath);
								yaml = loadYamlFile(metaFileName.c_str());
								index = yaml.value().rootref();
							}

							ryml::NodeRef entryNode;
							if (index.has_child("%ROOT"))
							{
								// NOTE - working around a rapidyaml issue that needs to get resolved upstream
								// '%' is a directive in YAML and it's not being quoted, this makes the memcards backwards compatible
								// switched from '%' to '$'
								// NOTE - this issue has now been resolved, but should be preserved for backwards compatibility
								entryNode = index["%ROOT"];
								entryNode.set_key("$ROOT");
							}
							if (index.has_child("$ROOT"))
							{
								entryNode = index["$ROOT"];
								entryNode["timeCreated"] << entry->entry.data.timeCreated.ToTime();
								entryNode["timeModified"] << entry->entry.data.timeModified.ToTime();

								// Write out the changes
								SaveYAMLToFile(metaFileName.c_str(), index);
							}
						}
					}

					MemoryCardFileMetadataReference* dirRef = AddDirEntryToMetadataQuickAccess(entry, parent);

					FlushFileEntries(entry->entry.data.cluster, entry->entry.data.length, subDirPath, dirRef);
				}
			}
			else if (entry->IsFile())
			{
				AddFileEntryToMetadataQuickAccess(entry, parent);

				if (entry->entry.data.length == 0)
				{
					// empty files need to be explicitly created, as there will be no data cluster referencing it later
					if (m_performFileWrites)
					{
						char cleanName[sizeof(entry->entry.data.name)];
						memcpy(cleanName, (const char*)entry->entry.data.name, sizeof(cleanName));
						FileAccessHelper::CleanMemcardFilename(cleanName);
						const std::string fullDirPath(Path::Combine(m_folderName, dirPath));
						const std::string fn(Path::Combine(fullDirPath, cleanName));

						if (!FileSystem::FileExists(fn.c_str()))
						{
							if (!FileSystem::DirectoryExists(fullDirPath.c_str()))
							{
								FileSystem::CreateDirectoryPath(fullDirPath.c_str(), false);
							}

							auto createEmptyFile = FileSystem::OpenManagedCFile(fn.c_str(), "wb");
						}
					}
				}

				if (m_performFileWrites)
				{
					FileAccessHelper::WriteIndex(m_folderName, entry, parent);
				}
			}
		}
	}

	// continue to the next cluster of this directory
	const u32 nextCluster = m_fat.data[0][0][dirCluster];
	if (nextCluster != (LastDataCluster | DataClusterInUseMask))
	{
		FlushFileEntries(nextCluster & NextDataClusterMask, remainingFiles - 2, dirPath, parent);
	}
}

void FolderMemoryCard::FlushDeletedFilesAndRemoveUnchangedDataFromCache(const std::vector<MemoryCardFileEntryTreeNode>& oldFileEntries)
{
	const u32 newRootDirCluster = m_superBlock.data.rootdir_cluster;
	const u32 newFileCount = m_fileEntryDict[newRootDirCluster].entries[0].entry.data.length;
	FlushDeletedFilesAndRemoveUnchangedDataFromCache(oldFileEntries, newRootDirCluster, newFileCount, "");
}

void FolderMemoryCard::FlushDeletedFilesAndRemoveUnchangedDataFromCache(const std::vector<MemoryCardFileEntryTreeNode>& oldFileEntries, const u32 newCluster, const u32 newFileCount, const std::string& dirPath)
{
	// go through all file entires of the current directory of the old data
	for (auto it = oldFileEntries.cbegin(); it != oldFileEntries.cend(); ++it)
	{
		const MemoryCardFileEntry* entry = &it->entry;
		if (entry->IsValid() && entry->IsUsed() && !entry->IsDotDir())
		{
			// check if an equivalent entry exists in m_fileEntryDict
			const MemoryCardFileEntry* newEntry = FindEquivalent(entry, newCluster, newFileCount);
			if (newEntry == nullptr)
			{
				// file/dir doesn't exist anymore, remove!
				char cleanName[sizeof(entry->entry.data.name)];
				memcpy(cleanName, (const char*)entry->entry.data.name, sizeof(cleanName));
				FileAccessHelper::CleanMemcardFilename(cleanName);
				const std::string fullDirPath(Path::Combine(m_folderName, dirPath));
				const std::string filePath(Path::Combine(fullDirPath, cleanName));
				m_lastAccessedFile.CloseMatching(filePath);
				const std::string newFilePath(Path::Combine(Path::Combine(m_folderName, dirPath), fmt::format("_pcsx2_deleted_{}", cleanName)));
				if (FileSystem::DirectoryExists(newFilePath.c_str()))
				{
					// wxRenameFile doesn't overwrite directories, so we have to remove the old one first
					FileSystem::RecursiveDeleteDirectory(newFilePath.c_str());
				}
				FileSystem::RenamePath(filePath.c_str(), newFilePath.c_str());
				DeleteFromIndex(fullDirPath, cleanName);
			}
			else if (entry->IsDir())
			{
				// still exists and is a directory, recursive call for subdir
				char cleanName[sizeof(entry->entry.data.name)];
				memcpy(cleanName, (const char*)entry->entry.data.name, sizeof(cleanName));
				FileAccessHelper::CleanMemcardFilename(cleanName);
				const std::string subDirPath(Path::Combine(dirPath, cleanName));
				FlushDeletedFilesAndRemoveUnchangedDataFromCache(it->subdir, newEntry->entry.data.cluster, newEntry->entry.data.length, subDirPath);
			}
			else if (entry->IsFile())
			{
				// still exists and is a file, see if we can remove unchanged data from m_cache
				RemoveUnchangedDataFromCache(entry, newEntry);
			}
		}
	}
}

void FolderMemoryCard::RemoveUnchangedDataFromCache(const MemoryCardFileEntry* const oldEntry, const MemoryCardFileEntry* const newEntry)
{
	// Disclaimer: Technically, to actually prove that file data has not changed and still belongs to the same file, we'd need to keep a copy
	// of the old FAT cluster chain and compare that as well, and only acknowledge the file as unchanged if none of those have changed. However,
	// the chain of events that leads to a file having the exact same file contents as a deleted old file while also being placed in the same
	// data clusters as the deleted file AND matching this condition here, in a quick enough succession that no flush has occurred yet since the
	// deletion of that old file is incredibly unlikely, so I'm not sure if it's actually worth coding for.
	if (oldEntry->entry.data.timeModified != newEntry->entry.data.timeModified || oldEntry->entry.data.timeCreated != newEntry->entry.data.timeCreated || oldEntry->entry.data.length != newEntry->entry.data.length || oldEntry->entry.data.cluster != newEntry->entry.data.cluster)
	{
		return;
	}

	u32 cluster = newEntry->entry.data.cluster & NextDataClusterMask;
	const u32 alloc_offset = m_superBlock.data.alloc_offset;
	while (cluster != LastDataCluster)
	{
		for (int i = 0; i < 2; ++i)
		{
			const u32 page = (cluster + alloc_offset) * 2 + i;
			auto newIt = m_cache.find(page);
			if (newIt == m_cache.end())
			{
				continue;
			}
			auto oldIt = m_oldDataCache.find(page);
			if (oldIt == m_oldDataCache.end())
			{
				continue;
			}

			if (memcmp(&oldIt->second.raw[0], &newIt->second.raw[0], PageSize) == 0)
			{
				m_cache.erase(newIt);
			}
		}

		cluster = m_fat.data[0][0][cluster] & NextDataClusterMask;
	}
}

s32 FolderMemoryCard::WriteWithoutCache(const u8* src, u32 adr, int size)
{
	//const u32 block = adr / BlockSizeRaw;
	//const u32 cluster = adr / ClusterSizeRaw;
	//const u32 page = adr / PageSizeRaw;
	const u32 offset = adr % PageSizeRaw;
	const u32 end = offset + size;

	if (end > PageSizeRaw)
	{
		// is trying to store more than one page at a time
		// do this recursively so that each function call only has to care about one page
		const u32 toNextPage = PageSizeRaw - offset;
		Save(src + toNextPage, adr + toNextPage, size - toNextPage);
		size = toNextPage;
	}

	if (offset < PageSize)
	{
		// is trying to store (part of) an actual data block
		const u32 dataLength = std::min((u32)size, PageSize - offset);

		u8* dest = GetSystemBlockPointer(adr);
		if (dest != nullptr)
		{
			memcpy(dest, src, dataLength);
		}
		else
		{
			WriteToFile(src, adr, dataLength);
		}
	}

	if (end > PageSize)
	{
		// is trying to store ECC
		// simply ignore this, is automatically generated when reading
	}

	// return 0 on fail, 1 on success?
	return 1;
}

bool FolderMemoryCard::WriteToFile(const u8* src, u32 adr, u32 dataLength)
{
	const u32 cluster = adr / ClusterSizeRaw;
	const u32 page = adr / PageSizeRaw;
	const u32 offset = adr % PageSizeRaw;
	const u32 fatCluster = cluster - m_superBlock.data.alloc_offset;

	// if the cluster is unused according to FAT, just skip all this, we're not gonna find anything anyway
	if ((m_fat.data[0][0][fatCluster] & DataClusterInUseMask) == 0)
	{
		return false;
	}

	// figure out which file to write to
	auto it = m_fileMetadataQuickAccess.find(fatCluster);
	if (it != m_fileMetadataQuickAccess.end())
	{
		const MemoryCardFileEntry* const entry = it->second.entry;
		const u32 clusterNumber = it->second.consecutiveCluster;

		if (m_performFileWrites)
		{
			std::FILE* file = m_lastAccessedFile.ReOpen(m_folderName, &it->second, true);
			if (file)
			{
				const u32 clusterOffset = (page % 2) * PageSize + offset;
				const u32 fileSize = entry->entry.data.length;
				const u32 fileOffsetStart = std::min(clusterNumber * ClusterSize + clusterOffset, fileSize);
				const u32 fileOffsetEnd = std::min(fileOffsetStart + dataLength, fileSize);
				const u32 bytesToWrite = fileOffsetEnd - fileOffsetStart;

				u32 actualFileSize = static_cast<u32>(std::clamp<s64>(FileSystem::FSize64(file), 0, std::numeric_limits<u32>::max()));
				if (actualFileSize < fileOffsetStart)
				{
					FileSystem::FSeek64(file, actualFileSize, SEEK_SET);
					const u32 diff = fileOffsetStart - actualFileSize;
					u8 temp = 0xFF;
					for (u32 i = 0; i < diff; ++i)
					{
						std::fwrite(&temp, 1, 1, file);
					}
				}

				if (FileSystem::FTell64(file) == fileOffsetStart || FileSystem::FSeek64(file, fileOffsetStart, SEEK_SET) == 0)
				{
					if (bytesToWrite > 0)
					{
						std::fwrite(src, bytesToWrite, 1, file);
					}
				}
			}
			else
			{
				return false;
			}
		}

		return true;
	}

	return false;
}

void FolderMemoryCard::CopyEntryDictIntoTree(std::vector<MemoryCardFileEntryTreeNode>* fileEntryTree, const u32 cluster, const u32 fileCount)
{
	const MemoryCardFileEntryCluster* entryCluster = &m_fileEntryDict[cluster];
	u32 fileCluster = cluster;

	for (size_t i = 0; i < fileCount; ++i)
	{
		const MemoryCardFileEntry* entry = &entryCluster->entries[i % 2];

		if (entry->IsValid() && entry->IsUsed())
		{
			fileEntryTree->emplace_back(*entry);

			if (entry->IsDir() && !entry->IsDotDir())
			{
				MemoryCardFileEntryTreeNode* treeEntry = &fileEntryTree->back();
				CopyEntryDictIntoTree(&treeEntry->subdir, entry->entry.data.cluster, entry->entry.data.length);
			}
		}

		if (i % 2 == 1)
		{
			fileCluster = m_fat.data[0][0][fileCluster] & 0x7FFFFFFFu;
			if (fileCluster == 0x7FFFFFFFu)
			{
				return;
			}
			entryCluster = &m_fileEntryDict[fileCluster];
		}
	}
}

const MemoryCardFileEntry* FolderMemoryCard::FindEquivalent(const MemoryCardFileEntry* searchEntry, const u32 cluster, const u32 fileCount)
{
	const MemoryCardFileEntryCluster* entryCluster = &m_fileEntryDict[cluster];
	u32 fileCluster = cluster;

	for (size_t i = 0; i < fileCount; ++i)
	{
		const MemoryCardFileEntry* entry = &entryCluster->entries[i % 2];

		if (entry->IsValid() && entry->IsUsed())
		{
			if (entry->IsFile() == searchEntry->IsFile() && entry->IsDir() == searchEntry->IsDir() && strncmp((const char*)searchEntry->entry.data.name, (const char*)entry->entry.data.name, sizeof(entry->entry.data.name)) == 0)
			{
				return entry;
			}
		}

		if (i % 2 == 1)
		{
			fileCluster = m_fat.data[0][0][fileCluster] & 0x7FFFFFFFu;
			if (fileCluster == 0x7FFFFFFFu)
			{
				return nullptr;
			}
			entryCluster = &m_fileEntryDict[fileCluster];
		}
	}

	return nullptr;
}

s32 FolderMemoryCard::EraseBlock(u32 adr)
{
	const u32 block = adr / BlockSizeRaw;

	u8 eraseData[PageSize];
	memset(eraseData, 0xFF, PageSize);
	for (int page = 0; page < 16; ++page)
	{
		const u32 adr = block * BlockSizeRaw + page * PageSizeRaw;
		Save(eraseData, adr, PageSize);
	}

	// return 0 on fail, 1 on success?
	return 1;
}

u64 FolderMemoryCard::GetCRC() const
{
	// Since this is just used as integrity check for savestate loading,
	// give a timestamp of the last time the memory card was written to
	return m_timeLastWritten;
}

void FolderMemoryCard::SetSlot(uint slot)
{
	pxAssert(slot < 8);
	m_slot = slot;
}

u32 FolderMemoryCard::GetSizeInClusters() const
{
	const u32 clusters = m_superBlock.data.clusters_per_card;
	if (clusters > 0 && clusters < 0xFFFFFFFFu)
	{
		return clusters;
	}
	else
	{
		return TotalClusters;
	}
}

void FolderMemoryCard::SetSizeInClusters(u32 clusters)
{
	superBlockUnion newSuperBlock;
	memcpy(&newSuperBlock.raw[0], &m_superBlock.raw[0], sizeof(newSuperBlock.raw));

	newSuperBlock.data.clusters_per_card = clusters;

	const u32 alloc_offset = clusters / 0x100 + 9;
	newSuperBlock.data.alloc_offset = alloc_offset;
	newSuperBlock.data.alloc_end = clusters - 0x10 - alloc_offset;

	const u32 blocks = clusters / 8;
	newSuperBlock.data.backup_block1 = blocks - 1;
	newSuperBlock.data.backup_block2 = blocks - 2;

	for (size_t i = 0; i < sizeof(newSuperBlock.raw) / PageSize; ++i)
	{
		Save(&newSuperBlock.raw[i * PageSize], i * PageSizeRaw, PageSize);
	}
}

void FolderMemoryCard::SetSizeInMB(u32 megaBytes)
{
	SetSizeInClusters((megaBytes * 1024 * 1024) / ClusterSize);
}

void FolderMemoryCard::SetTimeLastReadToNow()
{
	m_framesUntilFlush = FramesAfterWriteUntilFlush;
}

void FolderMemoryCard::SetTimeLastWrittenToNow()
{
	// CHANGE: this was local time milliseconds, which might be problematic...
	m_timeLastWritten = std::time(nullptr);// wxGetLocalTimeMillis().GetValue();
	m_framesUntilFlush = FramesAfterWriteUntilFlush;
}

void FolderMemoryCard::AttemptToRecreateIndexFile(const std::string& directory) const
{
	// Attempt to fix broken index files (potentially broken in v1.7.2115, fixed in 1.7.2307
	Console.Error(fmt::format("[Memcard] Folder memory card index file is malformed, backing up and attempting to re-create.  This may not work for all games (ie. GTA), so backing up the current index file!. '{}'",
		directory));

	// This isn't full-proof, so we backup the broken index file
	FileSystem::CopyFilePath(Path::Combine(directory, "_pcsx2_index").c_str(),
		Path::Combine(directory, "_pcsx2_index.invalid.bak").c_str(), true);

	// Create everything relative to a point in time, with an artifical delay to minimize edge-cases
	auto currTime = std::time(nullptr) - 1000;
	auto currOrder = 1;
	ryml::Tree tree;
	ryml::NodeRef root = tree.rootref();
	root |= ryml::MAP;
	root.append_child() << ryml::key("$ROOT") |= ryml::MAP;
	root["$ROOT"]["timeCreated"] << currTime++;

	FileSystem::FindResultsArray results;
	FileSystem::FindFiles(directory.c_str(), "*", FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_RELATIVE_PATHS | FILESYSTEM_FIND_HIDDEN_FILES, &results);
	for (const FILESYSTEM_FIND_DATA& fd : results)
	{
		if (fd.FileName.rfind("_pcsx2_", 0) == 0)
		{
			continue;
		}

		root.append_child() << ryml::key(fd.FileName) |= ryml::MAP;
		ryml::NodeRef newNode = root[c4::to_csubstr(fd.FileName)];
		newNode["order"] << currOrder++;
		newNode["timeCreated"] << currTime++;
		newNode["timeModified"] << currTime++;
	}

	root["$ROOT"]["timeModified"] << currTime;

	auto file = FileSystem::OpenManagedCFile(Path::Combine(directory, "_pcsx2_index").c_str(), "w");
	if (file)
		ryml::emit(tree, file.get());
}

std::string FolderMemoryCard::GetDisabledMessage(uint slot) const
{
	return fmt::format("The PS2-slot {} has been automatically disabled.  You can correct the problem\nand re-enable it at any time using Config:Memory cards from the main menu.", slot); //TODO: translate internal slot index to human-readable slot description
}

std::string FolderMemoryCard::GetCardFullMessage(const std::string& filePath) const
{
	return fmt::format("(FolderMcd) Memory Card is full, could not add: {}", filePath);
}

std::vector<FolderMemoryCard::EnumeratedFileEntry> FolderMemoryCard::GetOrderedFiles(const std::string& dirPath) const
{
	std::vector<EnumeratedFileEntry> result;

	FileSystem::FindResultsArray results;
	FileSystem::FindFiles(dirPath.c_str(), "*", FILESYSTEM_FIND_FILES | FILESYSTEM_FIND_FOLDERS | FILESYSTEM_FIND_RELATIVE_PATHS | FILESYSTEM_FIND_HIDDEN_FILES, &results);
	if (!results.empty())
	{
		// We must be able to support legacy folder memcards without the index file, so for those
		// track an order variable and make it negative - this way new files get their order preserved
		// and old files are listed first.
		// In the YAML File order is stored as an unsigned int, so use a signed int64_t to accommodate for
		// all possible values without cutting them off
		// Also exploit the fact pairs sort lexicographically to ensure directories are listed first
		// (since they don't carry their own order in the index file)
		std::map<std::pair<bool, int64_t>, EnumeratedFileEntry> sortContainer;
		int64_t orderForDirectories = 1;
		int64_t orderForLegacyFiles = -1;

		for (FILESYSTEM_FIND_DATA& fd : results)
		{
			if (StringUtil::StartsWith(fd.FileName, "_pcsx2_"))
				continue;

			std::string filePath(Path::Combine(dirPath, fd.FileName));
			if (!(fd.Attributes & FILESYSTEM_FILE_ATTRIBUTE_DIRECTORY))
			{
				std::optional<ryml::Tree> yaml = loadYamlFile(Path::Combine(dirPath, "_pcsx2_index").c_str());

				EnumeratedFileEntry entry{fd.FileName, fd.CreationTime, fd.ModificationTime, true};
				int64_t newOrder = orderForLegacyFiles--;
				if (yaml.has_value() && !yaml.value().empty())
				{
					ryml::NodeRef index = yaml.value().rootref();
					for (const ryml::NodeRef& n : index.children())
					{
						auto key = std::string(n.key().str, n.key().len);
					}
					if (index.has_child(c4::to_csubstr(fd.FileName)))
					{
						const ryml::NodeRef& node = index[c4::to_csubstr(fd.FileName)];
						if (node.has_child("timeCreated"))
						{
							node["timeCreated"] >> entry.m_timeCreated;
						}
						if (node.has_child("timeModified"))
						{
							node["timeModified"] >> entry.m_timeModified;
						}
						if (node.has_child("order"))
						{
							node["order"] >> newOrder;
						}
					}
				}

				// orderForLegacyFiles will decrement even if it ends up being unused, but that's fine
				auto key = std::make_pair(true, newOrder);
				sortContainer.try_emplace(std::move(key), std::move(entry));
			}
			else
			{
				std::string subDirPath(Path::Combine(dirPath, fd.FileName));

				std::string subDirIndexPath(Path::Combine(subDirPath, "_pcsx2_index"));
				std::optional<ryml::Tree> yaml = loadYamlFile(subDirIndexPath.c_str());

				EnumeratedFileEntry entry{fd.FileName, fd.CreationTime, fd.ModificationTime, false};
				if (yaml.has_value() && !yaml.value().empty())
				{
					ryml::NodeRef indexForDirectory = yaml.value().rootref();

					// Detect broken index files, every index file should have atleast ONE child ('[$%]ROOT')
					if (!indexForDirectory.has_children())
					{
						AttemptToRecreateIndexFile(subDirPath);
						yaml = loadYamlFile(subDirIndexPath.c_str());
						indexForDirectory = yaml.value().rootref();
					}

					const ryml::NodeRef entryNode;
					if (indexForDirectory.has_child("%ROOT"))
					{
						// NOTE - working around a rapidyaml issue that needs to get resolved upstream
						// '%' is a directive in YAML and it's not being quoted, this makes the memcards backwards compatible
						// switched from '%' to '$'
						const ryml::NodeRef& node = indexForDirectory["%ROOT"];
						if (node.has_child("timeCreated"))
						{
							node["timeCreated"] >> entry.m_timeCreated;
						}
						if (node.has_child("timeModified"))
						{
							node["timeModified"] >> entry.m_timeModified;
						}
					}
					else if (indexForDirectory.has_child("$ROOT"))
					{
						const ryml::NodeRef& node = indexForDirectory["$ROOT"];
						if (node.has_child("timeCreated"))
						{
							node["timeCreated"] >> entry.m_timeCreated;
						}
						if (node.has_child("timeModified"))
						{
							node["timeModified"] >> entry.m_timeModified;
						}
					}
				}

				// orderForDirectories will increment even if it ends up being unused, but that's fine
				auto key = std::make_pair(false, orderForDirectories++);
				sortContainer.try_emplace(std::move(key), std::move(entry));
			}
		}

		// Move items from the intermediate map to a final vector
		result.reserve(sortContainer.size());
		for (auto& e : sortContainer)
		{
			result.push_back(std::move(e.second));
		}
	}

	return result;
}

void FolderMemoryCard::DeleteFromIndex(const std::string& filePath, const std::string_view& entry) const
{
	const std::string indexName(Path::Combine(filePath, "_pcsx2_index"));

	std::optional<ryml::Tree> yaml = loadYamlFile(indexName.c_str());
	if (yaml.has_value() && !yaml.value().empty())
	{
		ryml::NodeRef index = yaml.value().rootref();
		index.remove_child(c4::csubstr(entry.data(), entry.length()));

		// Write out the changes
		SaveYAMLToFile(indexName.c_str(), index);
	}
}

// from http://www.oocities.org/siliconvalley/station/8269/sma02/sma02.html#ECC
void FolderMemoryCard::CalculateECC(u8* ecc, const u8* data)
{
	static const u8 Table[] = {
		0x00, 0x87, 0x96, 0x11, 0xa5, 0x22, 0x33, 0xb4, 0xb4, 0x33, 0x22, 0xa5, 0x11, 0x96, 0x87, 0x00,
		0xc3, 0x44, 0x55, 0xd2, 0x66, 0xe1, 0xf0, 0x77, 0x77, 0xf0, 0xe1, 0x66, 0xd2, 0x55, 0x44, 0xc3,
		0xd2, 0x55, 0x44, 0xc3, 0x77, 0xf0, 0xe1, 0x66, 0x66, 0xe1, 0xf0, 0x77, 0xc3, 0x44, 0x55, 0xd2,
		0x11, 0x96, 0x87, 0x00, 0xb4, 0x33, 0x22, 0xa5, 0xa5, 0x22, 0x33, 0xb4, 0x00, 0x87, 0x96, 0x11,
		0xe1, 0x66, 0x77, 0xf0, 0x44, 0xc3, 0xd2, 0x55, 0x55, 0xd2, 0xc3, 0x44, 0xf0, 0x77, 0x66, 0xe1,
		0x22, 0xa5, 0xb4, 0x33, 0x87, 0x00, 0x11, 0x96, 0x96, 0x11, 0x00, 0x87, 0x33, 0xb4, 0xa5, 0x22,
		0x33, 0xb4, 0xa5, 0x22, 0x96, 0x11, 0x00, 0x87, 0x87, 0x00, 0x11, 0x96, 0x22, 0xa5, 0xb4, 0x33,
		0xf0, 0x77, 0x66, 0xe1, 0x55, 0xd2, 0xc3, 0x44, 0x44, 0xc3, 0xd2, 0x55, 0xe1, 0x66, 0x77, 0xf0,
		0xf0, 0x77, 0x66, 0xe1, 0x55, 0xd2, 0xc3, 0x44, 0x44, 0xc3, 0xd2, 0x55, 0xe1, 0x66, 0x77, 0xf0,
		0x33, 0xb4, 0xa5, 0x22, 0x96, 0x11, 0x00, 0x87, 0x87, 0x00, 0x11, 0x96, 0x22, 0xa5, 0xb4, 0x33,
		0x22, 0xa5, 0xb4, 0x33, 0x87, 0x00, 0x11, 0x96, 0x96, 0x11, 0x00, 0x87, 0x33, 0xb4, 0xa5, 0x22,
		0xe1, 0x66, 0x77, 0xf0, 0x44, 0xc3, 0xd2, 0x55, 0x55, 0xd2, 0xc3, 0x44, 0xf0, 0x77, 0x66, 0xe1,
		0x11, 0x96, 0x87, 0x00, 0xb4, 0x33, 0x22, 0xa5, 0xa5, 0x22, 0x33, 0xb4, 0x00, 0x87, 0x96, 0x11,
		0xd2, 0x55, 0x44, 0xc3, 0x77, 0xf0, 0xe1, 0x66, 0x66, 0xe1, 0xf0, 0x77, 0xc3, 0x44, 0x55, 0xd2,
		0xc3, 0x44, 0x55, 0xd2, 0x66, 0xe1, 0xf0, 0x77, 0x77, 0xf0, 0xe1, 0x66, 0xd2, 0x55, 0x44, 0xc3,
		0x00, 0x87, 0x96, 0x11, 0xa5, 0x22, 0x33, 0xb4, 0xb4, 0x33, 0x22, 0xa5, 0x11, 0x96, 0x87, 0x00};

	int i, c;

	ecc[0] = ecc[1] = ecc[2] = 0;

	for (i = 0; i < 0x80; i++)
	{
		c = Table[data[i]];

		ecc[0] ^= c;
		if (c & 0x80)
		{
			ecc[1] ^= ~i;
			ecc[2] ^= i;
		}
	}
	ecc[0] = ~ecc[0];
	ecc[0] &= 0x77;

	ecc[1] = ~ecc[1];
	ecc[1] &= 0x7f;

	ecc[2] = ~ecc[2];
	ecc[2] &= 0x7f;

	return;
}

void FolderMemoryCard::WriteToFile(const std::string& filename)
{
	auto targetFile = FileSystem::OpenManagedCFile(filename.c_str(), "wb");
	if (!targetFile)
	{
		Console.Error("(FolderMemoryCard::WriteToFile) Failed to open '%s'.", filename.c_str());
		return;
	}

	u8 buffer[FolderMemoryCard::PageSizeRaw];
	u32 adr = 0;
	while (adr < GetSizeInClusters() * FolderMemoryCard::ClusterSizeRaw)
	{
		Read(buffer, adr, FolderMemoryCard::PageSizeRaw);
		std::fwrite(buffer, FolderMemoryCard::PageSizeRaw, 1, targetFile.get());
		adr += FolderMemoryCard::PageSizeRaw;
	}
}


FileAccessHelper::FileAccessHelper()
{
}

FileAccessHelper::~FileAccessHelper()
{
	this->CloseAll();
}

std::FILE* FileAccessHelper::Open(const std::string_view& folderName, MemoryCardFileMetadataReference* fileRef, bool writeMetadata /* = false */)
{
	std::string filename(folderName);
	fileRef->GetPath(&filename);

	if (!FileSystem::FileExists(filename.c_str()))
	{
		const std::string directory(Path::GetDirectory(filename));
		if (!FileSystem::DirectoryExists(directory.c_str()))
			FileSystem::CreateDirectoryPath(directory.c_str(), true);

		auto createEmptyFile = FileSystem::OpenManagedCFile(filename.c_str(), "wb");
	}

	std::FILE* file = FileSystem::OpenCFile(filename.c_str(), "r+b");

	std::string internalPath;
	fileRef->GetInternalPath(&internalPath);
	MemoryCardFileHandleStructure handleStruct;
	handleStruct.fileHandle = file;
	handleStruct.fileRef = fileRef;
	m_files.emplace(std::move(internalPath), std::move(handleStruct));

	if (writeMetadata)
	{
		WriteMetadata(folderName, fileRef);
	}

	return file;
}

void FileAccessHelper::WriteMetadata(const std::string_view& folderName, const MemoryCardFileMetadataReference* fileRef)
{
	std::string fileName(folderName);
	const bool cleanedFilename = fileRef->GetPath(&fileName);
	std::string metaFileName(Path::AppendDirectory(fileName, "_pcsx2_meta"));
	std::string metaDirName(Path::GetDirectory(metaFileName));

	const auto* entry = &fileRef->entry->entry;
	const bool metadataIsNonstandard = cleanedFilename || entry->data.mode != MemoryCardFileEntry::DefaultFileMode || entry->data.attr != 0;

	if (metadataIsNonstandard)
	{
		// write metadata of file if it's nonstandard
		if (!FileSystem::DirectoryExists(metaDirName.c_str()))
		{
			FileSystem::CreateDirectoryPath(metaDirName.c_str(), false);
		}

		auto metaFile = FileSystem::OpenManagedCFile(metaFileName.c_str(), "wb");
		if (metaFile)
			std::fwrite(entry->raw, sizeof(entry->raw), 1, metaFile.get());
	}
	else
	{
		// if metadata is standard remove metadata file if it exists
		if (FileSystem::DirectoryExists(metaDirName.c_str()))
		{
			FileSystem::DeleteFilePath(metaFileName.c_str());

			// and remove the metadata dir if it's now empty
			if (FileSystem::DirectoryIsEmpty(metaDirName.c_str()))
				FileSystem::DeleteDirectory(metaDirName.c_str());
		}
	}
}

void FileAccessHelper::WriteIndex(const std::string& baseFolderName, MemoryCardFileEntry* const entry, MemoryCardFileMetadataReference* const parent)
{
	// Not called for directories atm.
	pxAssert(entry->IsFile());

	std::string folderName(baseFolderName);
	parent->GetPath(&folderName);
	char cleanName[sizeof(entry->entry.data.name)];
	memcpy(cleanName, (const char*)entry->entry.data.name, sizeof(cleanName));
	FileAccessHelper::CleanMemcardFilename(cleanName);

	const std::string indexFileName(Path::Combine(folderName, "_pcsx2_index"));
	const c4::csubstr key = c4::to_csubstr(cleanName);
	std::optional<ryml::Tree> yaml = loadYamlFile(indexFileName.c_str());

	if (yaml.has_value() && !yaml.value().empty())
	{
		ryml::NodeRef index = yaml.value().rootref();

		if (!index.has_child(key))
		{
			// Newly added file - figure out the sort order as the entry should be added to the end of the list
			ryml::NodeRef newNode = index[key];
			newNode |= ryml::MAP;
			unsigned int maxOrder = 0;
			for (const ryml::NodeRef& n : index.children())
			{
				unsigned int currOrder = 0; // NOTE - this limits the usefulness of making the order an int64
				if (n.is_map() && n.has_child("order"))
				{
					n["order"] >> currOrder;
				}
				maxOrder = std::max(maxOrder, currOrder);
			}
			newNode["order"] << maxOrder + 1;
		}
		ryml::NodeRef entryNode = index[key];

		// Update timestamps basing on internal data
		const auto* e = &entry->entry.data;
		entryNode["timeCreated"] << e->timeCreated.ToTime();
		entryNode["timeModified"] << e->timeModified.ToTime();

		// Write out the changes
		SaveYAMLToFile(indexFileName.c_str(), index);
	}
}

std::FILE* FileAccessHelper::ReOpen(const std::string_view& folderName, MemoryCardFileMetadataReference* fileRef, bool writeMetadata /* = false */)
{
	std::string internalPath;
	fileRef->GetInternalPath(&internalPath);
	auto it = m_files.find(internalPath);
	if (it != m_files.end())
	{
		// we already have a handle to this file

		// if the caller wants to write metadata and we haven't done this recently, do so and remember that we did
		if (writeMetadata)
		{
			if (m_lastWrittenFileRef != fileRef)
			{
				WriteMetadata(folderName, fileRef);
				m_lastWrittenFileRef = fileRef;
			}
		}
		else
		{
			if (m_lastWrittenFileRef != nullptr)
			{
				m_lastWrittenFileRef = nullptr;
			}
		}

		// update the fileRef in the map since it might have been modified or deleted
		it->second.fileRef = fileRef;

		return it->second.fileHandle;
	}
	else
	{
		return this->Open(folderName, fileRef, writeMetadata);
	}
}

void FileAccessHelper::CloseFileHandle(std::FILE*& file, const MemoryCardFileEntry* entry /* = nullptr */)
{
	if (file)
	{
		std::fclose(file);
		file = nullptr;
	}
}

void FileAccessHelper::CloseMatching(const std::string_view& path)
{
	for (auto it = m_files.begin(); it != m_files.end();)
	{
		if (StringUtil::StartsWith(it->second.hostFilePath, path))
		{
			CloseFileHandle(it->second.fileHandle, it->second.fileRef->entry);
			it = m_files.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void FileAccessHelper::CloseAll()
{
	for (auto it = m_files.begin(); it != m_files.end(); ++it)
	{
		CloseFileHandle(it->second.fileHandle, it->second.fileRef->entry);
	}
	m_files.clear();
}

void FileAccessHelper::FlushAll()
{
	for (auto it = m_files.begin(); it != m_files.end(); ++it)
	{
		std::fflush(it->second.fileHandle);
	}
}

void FileAccessHelper::ClearMetadataWriteState()
{
	m_lastWrittenFileRef = nullptr;
}

bool FileAccessHelper::CleanMemcardFilename(char* name)
{
	// invalid characters for filenames in the PS2 file system: { '/', '?', '*' }
	// the following characters are valid in a PS2 memcard file system but invalid in Windows
	// there's less restrictions on Linux but by cleaning them always we keep the folders cross-compatible
	const char illegalChars[] = {'\\', '%', ':', '|', '"', '<', '>'};
	bool cleaned = false;

	const size_t filenameLength = strlen(name);
	for (size_t i = 0; i < sizeof(illegalChars); ++i)
	{
		for (size_t j = 0; j < filenameLength; ++j)
		{
			if (name[j] == illegalChars[i])
			{
				name[j] = '_';
				cleaned = true;
			}
		}
	}

	cleaned |= CleanMemcardFilenameEndDotOrSpace(name, filenameLength);

	return cleaned;
}

bool FileAccessHelper::CleanMemcardFilenameEndDotOrSpace(char* name, size_t length)
{
	// Windows truncates dots and spaces at the end of filenames, so make sure that doesn't happen
	bool cleaned = false;
	for (size_t j = length; j > 0; --j)
	{
		switch (name[j - 1])
		{
			case ' ':
			case '.':
				name[j - 1] = '_';
				cleaned = true;
				break;
			default:
				return cleaned;
		}
	}

	return cleaned;
}

bool MemoryCardFileMetadataReference::GetPath(std::string* fileName) const
{
	bool parentCleaned = false;
	if (parent)
	{
		parentCleaned = parent->GetPath(fileName);
	}

	char cleanName[sizeof(entry->entry.data.name)];
	memcpy(cleanName, (const char*)entry->entry.data.name, sizeof(cleanName));
	bool localCleaned = FileAccessHelper::CleanMemcardFilename(cleanName);

	if (entry->IsDir() || entry->IsFile())
	{
		*fileName = Path::Combine(*fileName, cleanName);
	}

	return parentCleaned || localCleaned;
}

void MemoryCardFileMetadataReference::GetInternalPath(std::string* fileName) const
{
	if (parent)
	{
		parent->GetInternalPath(fileName);
	}

	fileName->append((const char*)entry->entry.data.name);

	if (entry->IsDir())
	{
		fileName->append("/");
	}
}

FolderMemoryCardAggregator::FolderMemoryCardAggregator()
{
	for (uint i = 0; i < TotalCardSlots; ++i)
	{
		m_cards[i].SetSlot(i);
	}
}

void FolderMemoryCardAggregator::Open()
{
	for (int i = 0; i < TotalCardSlots; ++i)
	{
		m_cards[i].Open(m_enableFiltering, m_lastKnownFilter);
	}
}

void FolderMemoryCardAggregator::Close()
{
	for (int i = 0; i < TotalCardSlots; ++i)
	{
		m_cards[i].Close();
	}
}

void FolderMemoryCardAggregator::SetFiltering(const bool enableFiltering)
{
	m_enableFiltering = enableFiltering;
}

s32 FolderMemoryCardAggregator::IsPresent(uint slot)
{
	return m_cards[slot].IsPresent();
}

void FolderMemoryCardAggregator::GetSizeInfo(uint slot, McdSizeInfo& outways)
{
	m_cards[slot].GetSizeInfo(outways);
}

bool FolderMemoryCardAggregator::IsPSX(uint slot)
{
	return m_cards[slot].IsPSX();
}

s32 FolderMemoryCardAggregator::Read(uint slot, u8* dest, u32 adr, int size)
{
	return m_cards[slot].Read(dest, adr, size);
}

s32 FolderMemoryCardAggregator::Save(uint slot, const u8* src, u32 adr, int size)
{
	return m_cards[slot].Save(src, adr, size);
}

s32 FolderMemoryCardAggregator::EraseBlock(uint slot, u32 adr)
{
	return m_cards[slot].EraseBlock(adr);
}

u64 FolderMemoryCardAggregator::GetCRC(uint slot)
{
	return m_cards[slot].GetCRC();
}

void FolderMemoryCardAggregator::NextFrame(uint slot)
{
	m_cards[slot].NextFrame();
}

bool FolderMemoryCardAggregator::ReIndex(uint slot, const bool enableFiltering, const std::string& filter)
{
	if (m_cards[slot].ReIndex(enableFiltering, filter))
	{
		SetFiltering(enableFiltering);
		m_lastKnownFilter = filter;
		return true;
	}

	return false;
}
