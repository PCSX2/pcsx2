/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2015  PCSX2 Dev Team
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
#include "Utilities/SafeArray.inl"

#include "MemoryCardFile.h"
#include "MemoryCardFolder.h"

#include "System.h"
#include "AppConfig.h"

#include "yaml-cpp/yaml.h"

#include "svnrev.h"

bool RemoveDirectory(const wxString& dirname);

// A helper function to parse the YAML file
static YAML::Node LoadYAMLFromFile(const wxString& fileName)
{
	YAML::Node index;

	wxFFile indexFile;
	bool result;
	{
		// Suppress "file does not exist" errors
		wxLogNull noLog;
		result = indexFile.Open(fileName, L"r");
	}

	if (result)
	{
		wxString fileContents;
		if (indexFile.ReadAll(&fileContents))
		{
			index = YAML::Load(fileContents.mbc_str());
		}
	}

	return index;
}

FolderMemoryCard::FolderMemoryCard()
{
	m_slot = 0;
	m_isEnabled = false;
	m_performFileWrites = false;
	m_framesUntilFlush = 0;
	m_timeLastWritten = 0;
	m_filteringEnabled = false;
	m_filteringString = L"";
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
	m_filteringString = L"";
}

bool FolderMemoryCard::IsFormatted() const
{
	// this should be a good enough arbitrary check, if someone can think of a case where this doesn't work feel free to change
	return m_superBlock.raw[0x16] == 0x6F;
}

void FolderMemoryCard::Open(const bool enableFiltering, const wxString& filter)
{
	Open(g_Conf->FullpathToMcd(m_slot), g_Conf->Mcd[m_slot], 0, enableFiltering, filter, false);
}

void FolderMemoryCard::Open(const wxString& fullPath, const AppConfig::McdOptions& mcdOptions, const u32 sizeInClusters, const bool enableFiltering, const wxString& filter, bool simulateFileWrites)
{
	InitializeInternalData();
	m_performFileWrites = !simulateFileWrites;

	wxFileName configuredFileName(fullPath);
	m_folderName = wxFileName(configuredFileName.GetFullPath() + L"/");
	wxString str(configuredFileName.GetFullPath());
	bool disabled = false;

	if (mcdOptions.Enabled && mcdOptions.Type == MemoryCardType::MemoryCard_Folder)
	{
		if (configuredFileName.GetFullName().IsEmpty())
		{
			str = L"[empty filename]";
			disabled = true;
		}
		if (!disabled && configuredFileName.FileExists())
		{
			str = L"[is file, should be folder]";
			disabled = true;
		}

		// if nothing exists at a valid location, create a directory for the memory card
		if (!disabled && m_performFileWrites && !m_folderName.DirExists())
		{
			if (!m_folderName.Mkdir())
			{
				str = L"[couldn't create folder]";
				disabled = true;
			}
		}
	}
	else
	{
		// if the user has disabled this slot or is using a different memory card type, just return without a console log
		return;
	}

	Console.WriteLn(disabled ? Color_Gray : Color_Green, L"McdSlot %u: [Folder] " + str, m_slot);
	if (disabled)
		return;

	m_isEnabled = true;
	m_filteringEnabled = enableFiltering;
	m_filteringString = filter;
	LoadMemoryCardData(sizeInClusters, enableFiltering, filter);

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

bool FolderMemoryCard::ReIndex(bool enableFiltering, const wxString& filter)
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

void FolderMemoryCard::LoadMemoryCardData(const u32 sizeInClusters, const bool enableFiltering, const wxString& filter)
{
	bool formatted = false;

	// read superblock if it exists
	wxFileName superBlockFileName(m_folderName.GetPath(), L"_pcsx2_superblock");
	if (superBlockFileName.FileExists())
	{
		wxFFile superBlockFile(superBlockFileName.GetFullPath().c_str(), L"rb");
		if (superBlockFile.IsOpened() && superBlockFile.Read(&m_superBlock.raw, sizeof(m_superBlock.raw)) >= sizeof(m_superBlock.data))
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
			Console.WriteLn(Color_Green, L"(FolderMcd) Indexing slot %u with filter \"%s\".", m_slot, WX_STR(filter));
		}
		else
		{
			Console.WriteLn(Color_Green, L"(FolderMcd) Indexing slot %u without filter.", m_slot);
		}

		CreateFat();
		CreateRootDir();
		MemoryCardFileEntry* const rootDirEntry = &m_fileEntryDict[m_superBlock.data.rootdir_cluster].entries[0];
		AddFolder(rootDirEntry, m_folderName.GetPath(), nullptr, enableFiltering, filter);

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

bool FilterMatches(const wxString& fileName, const wxString& filter)
{
	size_t start = 0;
	size_t len = filter.Len();
	while (start < len)
	{
		size_t end = filter.find('/', start);
		if (end == wxString::npos)
		{
			end = len;
		}

		wxString singleFilter = filter.Mid(start, end - start);
		if (fileName.Contains(singleFilter))
		{
			return true;
		}

		start = end + 1;
	}

	return false;
}

bool FolderMemoryCard::AddFolder(MemoryCardFileEntry* const dirEntry, const wxString& dirPath, MemoryCardFileMetadataReference* parent, const bool enableFiltering, const wxString& filter)
{
	if (wxDir::Exists(dirPath))
	{

		wxString localFilter;
		if (enableFiltering)
		{
			bool hasFilter = !filter.IsEmpty();
			if (hasFilter)
			{
				localFilter = L"DATA-SYSTEM/BWNETCNF/" + filter;
			}
			else
			{
				localFilter = L"DATA-SYSTEM/BWNETCNF";
			}
		}

		int entryNumber = 2; // include . and ..
		for (const auto& file : GetOrderedFiles(dirPath))
		{

			wxFileName fileInfo(dirPath, file.m_fileName);

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

				// make sure we have enough space on the memcard for the directory
				const u32 newNeededClusters = CalculateRequiredClustersOfDirectory(dirPath + L"/" + file.m_fileName) + ((dirEntry->entry.data.length % 2) == 0 ? 1 : 0);
				if (newNeededClusters > GetAmountFreeDataClusters())
				{
					Console.Warning(GetCardFullMessage(file.m_fileName));
					continue;
				}

				// is a subdirectory
				fileInfo.AppendDir(fileInfo.GetFullName());
				fileInfo.SetName(L"");
				fileInfo.ClearExt();

				// add entry for subdir in parent dir
				MemoryCardFileEntry* newDirEntry = AppendFileEntryToDir(dirEntry);
				dirEntry->entry.data.length++;

				// set metadata
				wxFileName metaFileName(dirPath, L"_pcsx2_meta_directory");
				metaFileName.AppendDir(file.m_fileName);
				wxFFile metaFile;
				if (metaFileName.FileExists() && metaFile.Open(metaFileName.GetFullPath(), L"rb"))
				{
					size_t bytesRead = metaFile.Read(&newDirEntry->entry.raw, sizeof(newDirEntry->entry.raw));
					metaFile.Close();
					if (bytesRead < 0x60)
					{
						strcpy(reinterpret_cast<char*>(newDirEntry->entry.data.name), file.m_fileName.mbc_str());
					}
				}
				else
				{
					newDirEntry->entry.data.mode = MemoryCardFileEntry::DefaultDirMode;
					newDirEntry->entry.data.timeCreated = MemoryCardFileEntryDateTime::FromTime(file.m_timeCreated);
					newDirEntry->entry.data.timeModified = MemoryCardFileEntryDateTime::FromTime(file.m_timeModified);
					strcpy(reinterpret_cast<char*>(newDirEntry->entry.data.name), file.m_fileName.mbc_str());
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
				AddFolder(newDirEntry, fileInfo.GetFullPath(), dirRef);
			}
		}

		return true;
	}

	return false;
}

bool FolderMemoryCard::AddFile(MemoryCardFileEntry* const dirEntry, const wxString& dirPath, const EnumeratedFileEntry& fileEntry, MemoryCardFileMetadataReference* parent)
{
	wxFileName relativeFilePath(dirPath, fileEntry.m_fileName);
	relativeFilePath.MakeRelativeTo(m_folderName.GetPath());

	wxFileName fileInfo(dirPath, fileEntry.m_fileName);
	wxFFile file(fileInfo.GetFullPath(), L"rb");
	if (file.IsOpened())
	{
		// make sure we have enough space on the memcard to hold the data
		const u32 clusterSize = m_superBlock.data.pages_per_cluster * m_superBlock.data.page_len;
		const u32 filesize = file.Length();
		const u32 countClusters = (filesize % clusterSize) != 0 ? (filesize / clusterSize + 1) : (filesize / clusterSize);
		const u32 newNeededClusters = (dirEntry->entry.data.length % 2) == 0 ? countClusters + 1 : countClusters;
		if (newNeededClusters > GetAmountFreeDataClusters())
		{
			Console.Warning(GetCardFullMessage(relativeFilePath.GetFullPath()));
			return false;
		}

		MemoryCardFileEntry* newFileEntry = AppendFileEntryToDir(dirEntry);

		// set file entry metadata
		memset(newFileEntry->entry.raw, 0x00, sizeof(newFileEntry->entry.raw));

		wxFileName metaFileName(dirPath, fileEntry.m_fileName);
		metaFileName.AppendDir(L"_pcsx2_meta");
		wxFFile metaFile;
		if (metaFileName.FileExists() && metaFile.Open(metaFileName.GetFullPath(), L"rb"))
		{
			size_t bytesRead = metaFile.Read(&newFileEntry->entry.raw, sizeof(newFileEntry->entry.raw));
			metaFile.Close();
			if (bytesRead < 0x60)
			{
				strcpy(reinterpret_cast<char*>(newFileEntry->entry.data.name), fileEntry.m_fileName.mbc_str());
			}
		}
		else
		{
			newFileEntry->entry.data.mode = MemoryCardFileEntry::DefaultFileMode;
			newFileEntry->entry.data.timeCreated = MemoryCardFileEntryDateTime::FromTime(fileEntry.m_timeCreated);
			newFileEntry->entry.data.timeModified = MemoryCardFileEntryDateTime::FromTime(fileEntry.m_timeModified);
			strcpy(reinterpret_cast<char*>(newFileEntry->entry.data.name), fileEntry.m_fileName.mbc_str());
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

		file.Close();

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
		Console.WriteLn(L"(FolderMcd) Could not open file: %s", WX_STR(relativeFilePath.GetFullPath()));
		return false;
	}
}

u32 FolderMemoryCard::CalculateRequiredClustersOfDirectory(const wxString& dirPath) const
{
	const u32 clusterSize = m_superBlock.data.pages_per_cluster * m_superBlock.data.page_len;
	u32 requiredFileEntryPages = 2;
	u32 requiredClusters = 0;

	// No need to read the index file as we are only counting space required; order of files is irrelevant.
	wxDir dir(dirPath);
	wxString fileName;
	bool hasNext = dir.GetFirst(&fileName);
	while (hasNext)
	{
		if (fileName.StartsWith(L"_pcsx2_"))
		{
			hasNext = dir.GetNext(&fileName);
			continue;
		}

		++requiredFileEntryPages;
		wxFileName file(dirPath, fileName);
		wxString path = file.GetFullPath();
		bool isFile = wxFile::Exists(path);

		if (isFile)
		{
			const u32 filesize = file.GetSize().GetValue();
			const u32 countClusters = (filesize % clusterSize) != 0 ? (filesize / clusterSize + 1) : (filesize / clusterSize);
			requiredClusters += countClusters;
		}
		else
		{
			requiredClusters += CalculateRequiredClustersOfDirectory(path);
		}

		hasNext = dir.GetNext(&fileName);
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
MemoryCardFileEntry* FolderMemoryCard::GetFileEntryFromFileDataCluster(const u32 currentCluster, const u32 searchCluster, wxFileName* fileName, const size_t originalDirCount, u32* outClusterNumber)
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
					fileName->SetName(wxString::FromAscii((const char*)entry->entry.data.name));
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
				fileName->InsertDir(originalDirCount, wxString::FromAscii((const char*)entry->entry.data.name));
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
		wxFFile* file = m_lastAccessedFile.ReOpen(m_folderName, &it->second);
		if (file->IsOpened())
		{
			const u32 clusterOffset = (page % 2) * PageSize + offset;
			const u32 fileOffset = clusterNumber * ClusterSize + clusterOffset;

			if (fileOffset != file->Tell())
			{
				file->Seek(fileOffset);
			}
			size_t bytesRead = file->Read(dest, dataLength);

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

	Console.WriteLn(L"(FolderMcd) Writing data for slot %u to file system...", m_slot);
	const u64 timeFlushStart = wxGetLocalTimeMillis().GetValue();

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
		Console.Warning(L"(FolderMcd) Aborting flush of slot %u, emulation was interrupted during save process!", m_slot);
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

	const u64 timeFlushEnd = wxGetLocalTimeMillis().GetValue();
	Console.WriteLn(L"(FolderMcd) Done! Took %u ms.", timeFlushEnd - timeFlushStart);

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
		wxFileName superBlockFileName(m_folderName.GetPath(), L"_pcsx2_superblock");
		wxFFile superBlockFile(superBlockFileName.GetFullPath().c_str(), L"wb");
		if (superBlockFile.IsOpened())
		{
			superBlockFile.Write(&m_superBlock.raw, sizeof(m_superBlock.raw));
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

void FolderMemoryCard::FlushFileEntries(const u32 dirCluster, const u32 remainingFiles, const wxString& dirPath, MemoryCardFileMetadataReference* parent)
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
					const wxString subDirName = wxString::FromAscii((const char*)cleanName);
					const wxString subDirPath = dirPath + L"/" + subDirName;

					if (m_performFileWrites)
					{
						// if this directory has nonstandard metadata, write that to the file system
						wxFileName metaFileName(m_folderName.GetFullPath() + subDirPath, L"_pcsx2_meta_directory");
						if (!metaFileName.DirExists())
						{
							metaFileName.Mkdir();
						}

						if (filenameCleaned || entry->entry.data.mode != MemoryCardFileEntry::DefaultDirMode || entry->entry.data.attr != 0)
						{
							wxFFile metaFile(metaFileName.GetFullPath(), L"wb");
							if (metaFile.IsOpened())
							{
								metaFile.Write(entry->entry.raw, sizeof(entry->entry.raw));
							}
						}
						else
						{
							// if metadata is standard make sure to remove a possibly existing metadata file
							if (metaFileName.FileExists())
							{
								wxRemoveFile(metaFileName.GetFullPath());
							}
						}

						// write the directory index
						metaFileName.SetName(L"_pcsx2_index");
						YAML::Node index = LoadYAMLFromFile(metaFileName.GetFullPath());
						YAML::Node entryNode = index["%ROOT"];

						entryNode["timeCreated"] = entry->entry.data.timeCreated.ToTime();
						entryNode["timeModified"] = entry->entry.data.timeModified.ToTime();

						// Write out the changes
						wxFFile indexFile;
						if (indexFile.Open(metaFileName.GetFullPath(), L"w"))
						{
							indexFile.Write(YAML::Dump(index));
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
						const wxString filePath = dirPath + L"/" + wxString::FromAscii((const char*)cleanName);
						wxFileName fn(m_folderName.GetFullPath() + filePath);

						if (!fn.FileExists())
						{
							if (!fn.DirExists())
							{
								fn.Mkdir(0777, wxPATH_MKDIR_FULL);
							}
							wxFFile createEmptyFile(fn.GetFullPath(), L"wb");
							createEmptyFile.Close();
						}
					}
				}

				if (m_performFileWrites)
				{
					FileAccessHelper::WriteIndex(m_folderName.GetFullPath() + dirPath, entry, parent);
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

void FolderMemoryCard::FlushDeletedFilesAndRemoveUnchangedDataFromCache(const std::vector<MemoryCardFileEntryTreeNode>& oldFileEntries, const u32 newCluster, const u32 newFileCount, const wxString& dirPath)
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
				const wxString fileName = wxString::FromAscii(cleanName);
				const wxString filePath = m_folderName.GetFullPath() + dirPath + L"/" + fileName;
				m_lastAccessedFile.CloseMatching(filePath);
				const wxString newFilePath = m_folderName.GetFullPath() + dirPath + L"/_pcsx2_deleted_" + fileName;
				if (wxFileName::DirExists(newFilePath))
				{
					// wxRenameFile doesn't overwrite directories, so we have to remove the old one first
					RemoveDirectory(newFilePath);
				}
				wxRenameFile(filePath, newFilePath);
				DeleteFromIndex(m_folderName.GetFullPath() + dirPath, fileName);
			}
			else if (entry->IsDir())
			{
				// still exists and is a directory, recursive call for subdir
				char cleanName[sizeof(entry->entry.data.name)];
				memcpy(cleanName, (const char*)entry->entry.data.name, sizeof(cleanName));
				FileAccessHelper::CleanMemcardFilename(cleanName);
				const wxString subDirName = wxString::FromAscii(cleanName);
				const wxString subDirPath = dirPath + L"/" + subDirName;
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
			wxFFile* file = m_lastAccessedFile.ReOpen(m_folderName, &it->second, true);
			if (file->IsOpened())
			{
				const u32 clusterOffset = (page % 2) * PageSize + offset;
				const u32 fileSize = entry->entry.data.length;
				const u32 fileOffsetStart = std::min(clusterNumber * ClusterSize + clusterOffset, fileSize);
				const u32 fileOffsetEnd = std::min(fileOffsetStart + dataLength, fileSize);
				const u32 bytesToWrite = fileOffsetEnd - fileOffsetStart;

				wxFileOffset actualFileSize = file->Length();
				if (actualFileSize < fileOffsetStart)
				{
					file->Seek(actualFileSize);
					const u32 diff = fileOffsetStart - actualFileSize;
					u8 temp = 0xFF;
					for (u32 i = 0; i < diff; ++i)
					{
						file->Write(&temp, 1);
					}
				}

				const wxFileOffset fileOffset = file->Tell();
				if (fileOffset != fileOffsetStart)
				{
					file->Seek(fileOffsetStart);
				}
				if (bytesToWrite > 0)
				{
					file->Write(src, bytesToWrite);
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
	m_timeLastWritten = wxGetLocalTimeMillis().GetValue();
	m_framesUntilFlush = FramesAfterWriteUntilFlush;
}

std::vector<FolderMemoryCard::EnumeratedFileEntry> FolderMemoryCard::GetOrderedFiles(const wxString& dirPath) const
{
	std::vector<EnumeratedFileEntry> result;

	wxDir dir(dirPath);
	if (dir.IsOpened())
	{

		const YAML::Node index = LoadYAMLFromFile(wxFileName(dirPath, "_pcsx2_index").GetFullPath());

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

		const auto getOptionalNodeAttribute = [](const YAML::Node& node, const char* attribName, auto def) {
			auto result = std::move(def);
			if (node.IsDefined())
			{
				result = node[attribName].as<decltype(def)>(def);
			}
			return result;
		};

		wxString fileName;
		bool hasNext = dir.GetFirst(&fileName);
		while (hasNext)
		{
			if (fileName.StartsWith(L"_pcsx2_"))
			{
				hasNext = dir.GetNext(&fileName);
				continue;
			}

			wxFileName fileInfo(dirPath, fileName);
			if (wxFile::Exists(fileInfo.GetFullPath()))
			{
				wxDateTime creationTime, modificationTime;
				fileInfo.GetTimes(nullptr, &modificationTime, &creationTime);

				const wxCharTypeBuffer fileNameUTF8(fileName.ToUTF8());
				const YAML::Node& node = index[fileNameUTF8.data()];

				// orderForLegacyFiles will decrement even if it ends up being unused, but that's fine
				auto key = std::make_pair(true, getOptionalNodeAttribute(node, "order", orderForLegacyFiles--));
				EnumeratedFileEntry entry{fileName, getOptionalNodeAttribute(node, "timeCreated", creationTime.GetTicks()),
					getOptionalNodeAttribute(node, "timeModified", modificationTime.GetTicks()), true};
				sortContainer.try_emplace(std::move(key), std::move(entry));
			}
			else
			{
				fileInfo.AppendDir(fileInfo.GetFullName());
				fileInfo.SetName(L"");

				wxDateTime creationTime, modificationTime;
				fileInfo.GetTimes(nullptr, &modificationTime, &creationTime);

				const YAML::Node indexForDirectory = LoadYAMLFromFile(wxFileName(fileInfo.GetFullPath(), "_pcsx2_index").GetFullPath());
				const YAML::Node& node = indexForDirectory["%ROOT"];

				// orderForDirectories will increment even if it ends up being unused, but that's fine
				auto key = std::make_pair(false, orderForDirectories++);
				EnumeratedFileEntry entry{fileName, getOptionalNodeAttribute(node, "timeCreated", creationTime.GetTicks()),
					getOptionalNodeAttribute(node, "timeModified", modificationTime.GetTicks()), false};
				sortContainer.try_emplace(std::move(key), std::move(entry));
			}

			hasNext = dir.GetNext(&fileName);
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

void FolderMemoryCard::DeleteFromIndex(const wxString& filePath, const wxString& entry) const
{
	const wxString indexName = wxFileName(filePath, "_pcsx2_index").GetFullPath();

	YAML::Node index = LoadYAMLFromFile(indexName);

	const wxCharTypeBuffer entryUTF8(entry.ToUTF8());
	index.remove(entryUTF8.data());

	// Write out the changes
	wxFFile indexFile;
	if (indexFile.Open(indexName, L"w"))
	{
		indexFile.Write(YAML::Dump(index));
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

void FolderMemoryCard::WriteToFile(const wxString& filename)
{
	wxFFile targetFile(filename, L"wb");

	u8 buffer[FolderMemoryCard::PageSizeRaw];
	u32 adr = 0;
	while (adr < GetSizeInClusters() * FolderMemoryCard::ClusterSizeRaw)
	{
		Read(buffer, adr, FolderMemoryCard::PageSizeRaw);
		targetFile.Write(buffer, FolderMemoryCard::PageSizeRaw);
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

wxFFile* FileAccessHelper::Open(const wxFileName& folderName, MemoryCardFileMetadataReference* fileRef, bool writeMetadata)
{
	wxFileName fn(folderName);
	fileRef->GetPath(&fn);
	wxString filename(fn.GetFullPath());

	if (!fn.FileExists())
	{
		if (!fn.DirExists())
		{
			fn.Mkdir(0777, wxPATH_MKDIR_FULL);
		}
		wxFFile createEmptyFile(filename, L"wb");
		createEmptyFile.Close();
	}

	wxFFile* file = new wxFFile(filename, L"r+b");

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

void FileAccessHelper::WriteMetadata(wxFileName folderName, const MemoryCardFileMetadataReference* fileRef)
{
	const bool cleanedFilename = fileRef->GetPath(&folderName);
	folderName.AppendDir(L"_pcsx2_meta");

	const auto* entry = &fileRef->entry->entry;
	const bool metadataIsNonstandard = cleanedFilename || entry->data.mode != MemoryCardFileEntry::DefaultFileMode || entry->data.attr != 0;

	if (metadataIsNonstandard)
	{
		// write metadata of file if it's nonstandard
		if (!folderName.DirExists())
		{
			folderName.Mkdir();
		}
		wxFFile metaFile(folderName.GetFullPath(), L"wb");
		if (metaFile.IsOpened())
		{
			metaFile.Write(entry->raw, sizeof(entry->raw));
		}
	}
	else
	{
		// if metadata is standard remove metadata file if it exists
		if (folderName.FileExists())
		{
			wxRemoveFile(folderName.GetFullPath());

			// and remove the metadata dir if it's now empty
			wxDir metaDir(folderName.GetPath());
			if (metaDir.IsOpened() && !metaDir.HasFiles())
			{
				wxRmdir(folderName.GetPath());
			}
		}
	}
}

void FileAccessHelper::WriteIndex(wxFileName folderName, MemoryCardFileEntry* const entry, MemoryCardFileMetadataReference* const parent)
{
	parent->GetPath(&folderName);
	char cleanName[sizeof(entry->entry.data.name)];
	memcpy(cleanName, (const char*)entry->entry.data.name, sizeof(cleanName));
	FileAccessHelper::CleanMemcardFilename(cleanName);

	if (entry->IsDir())
	{
		folderName.AppendDir(wxString::FromAscii(cleanName));
	}
	else if (entry->IsFile())
	{
		folderName.SetName(wxString::FromAscii(cleanName));
	}

	const wxCharTypeBuffer fileName(folderName.GetName().ToUTF8());
	folderName.SetName(L"_pcsx2_index");

	YAML::Node index = LoadYAMLFromFile(folderName.GetFullPath());
	YAML::Node entryNode = index[fileName.data()];

	if (!entryNode.IsDefined())
	{
		// Newly added file - figure out the sort order as the entry should be added to the end of the list
		unsigned int order = 0;
		for (const auto& node : index)
		{
			order = std::max(order, node.second["order"].as<unsigned int>(0));
		}

		entryNode["order"] = order + 1;
	}

	// Update timestamps basing on internal data
	const auto* e = &entry->entry.data;
	entryNode["timeCreated"] = e->timeCreated.ToTime();
	entryNode["timeModified"] = e->timeModified.ToTime();

	// Write out the changes
	wxFFile indexFile;
	if (indexFile.Open(folderName.GetFullPath(), L"w"))
	{
		indexFile.Write(YAML::Dump(index));
	}
}

wxFFile* FileAccessHelper::ReOpen(const wxFileName& folderName, MemoryCardFileMetadataReference* fileRef, bool writeMetadata)
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

void FileAccessHelper::CloseFileHandle(wxFFile* file, const MemoryCardFileEntry* entry)
{
	file->Close();

	delete file;
}

void FileAccessHelper::CloseMatching(const wxString& path)
{
	wxFileName fn(path);
	fn.Normalize();
	wxString pathNormalized = fn.GetFullPath();
	for (auto it = m_files.begin(); it != m_files.end();)
	{
		wxString openPath = it->second.fileHandle->GetName();
		if (openPath.StartsWith(pathNormalized))
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
		it->second.fileHandle->Flush();
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

bool MemoryCardFileMetadataReference::GetPath(wxFileName* fileName) const
{
	bool parentCleaned = false;
	if (parent)
	{
		parentCleaned = parent->GetPath(fileName);
	}

	char cleanName[sizeof(entry->entry.data.name)];
	memcpy(cleanName, (const char*)entry->entry.data.name, sizeof(cleanName));
	bool localCleaned = FileAccessHelper::CleanMemcardFilename(cleanName);

	if (entry->IsDir())
	{
		fileName->AppendDir(wxString::FromAscii(cleanName));
	}
	else if (entry->IsFile())
	{
		fileName->SetName(wxString::FromAscii(cleanName));
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

bool FolderMemoryCardAggregator::ReIndex(uint slot, const bool enableFiltering, const wxString& filter)
{
	if (m_cards[slot].ReIndex(enableFiltering, filter))
	{
		SetFiltering(enableFiltering);
		m_lastKnownFilter = filter;
		return true;
	}

	return false;
}
