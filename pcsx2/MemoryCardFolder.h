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

#pragma once

#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "Config.h"

#include "fmt/core.h"

//#define DEBUG_WRITE_FOLDER_CARD_IN_MEMORY_TO_FILE_ON_CHANGE

// --------------------------------------------------------------------------------------
//  Superblock Header Struct
// --------------------------------------------------------------------------------------
#pragma pack(push, 1)
struct superblock
{
	char magic[28]; // 0x00
	char version[12]; // 0x1c
	u16 page_len; // 0x28
	u16 pages_per_cluster; // 0x2a
	u16 pages_per_block; // 0x2c
	u16 unused; // 0x2e
	u32 clusters_per_card; // 0x30
	u32 alloc_offset; // 0x34
	u32 alloc_end; // 0x38
	u32 rootdir_cluster; // 0x3c
	u32 backup_block1; // 0x40
	u32 backup_block2; // 0x44
	u64 padding0x48; // 0x48
	u32 ifc_list[32]; // 0x50
	u32 bad_block_list[32]; // 0xd0
	u8 card_type; // 0x150
	u8 card_flags; // 0x151
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MemoryCardFileEntryDateTime
{
	u8 unused;
	u8 second;
	u8 minute;
	u8 hour;
	u8 day;
	u8 month;
	u16 year;

	static MemoryCardFileEntryDateTime FromTime(time_t time);

	time_t ToTime() const;

	bool operator==(const MemoryCardFileEntryDateTime& other) const
	{
		return unused == other.unused && second == other.second && minute == other.minute && hour == other.hour && day == other.day && month == other.month && year == other.year;
	}
	bool operator!=(const MemoryCardFileEntryDateTime& other) const
	{
		return !(*this == other);
	}
};
#pragma pack(pop)

// --------------------------------------------------------------------------------------
//  MemoryCardFileEntry
// --------------------------------------------------------------------------------------
// Structure for directory and file relationships as stored on memory cards
#pragma pack(push, 1)
struct MemoryCardFileEntry
{
	enum MemoryCardFileModeFlags
	{
		Mode_Read = 0x0001,
		Mode_Write = 0x0002,
		Mode_Execute = 0x0004,
		Mode_CopyProtected = 0x0008,
		Mode_File = 0x0010,
		Mode_Directory = 0x0020,
		Mode_Unknown0x0040 = 0x0040,
		Mode_Unknown0x0080 = 0x0080,
		Mode_Unknown0x0100 = 0x0100,
		Mode_Unknown0x0200 = 0x0200,
		Mode_Unknown0x0400 = 0x0400, // Maybe Mode_PS2_Save or something along those lines?
		Mode_PocketStation = 0x0800,
		Mode_PSX = 0x1000,
		Mode_Unknown0x2000 = 0x2000, // Supposedly Mode_Hidden but files still show up in the PS2 browser with this set
		Mode_Unknown0x4000 = 0x4000,
		Mode_Used = 0x8000
	};

	union
	{
		struct MemoryCardFileEntryData
		{
			u32 mode;
			u32 length; // number of bytes for file, number of files for dir
			MemoryCardFileEntryDateTime timeCreated;
			u32 cluster; // cluster the start of referred file or folder can be found in
			u32 dirEntry; // parent directory entry number, only used if "." entry of subdir
			MemoryCardFileEntryDateTime timeModified;
			u32 attr;
			u8 padding[0x1C];
			u8 name[0x20];
			u8 unused[0x1A0];
		} data;
		u8 raw[0x200];
	} entry;

	bool IsFile() const { return !!(entry.data.mode & Mode_File); }
	bool IsDir() const { return !!(entry.data.mode & Mode_Directory); }
	bool IsUsed() const { return !!(entry.data.mode & Mode_Used); }
	bool IsValid() const { return entry.data.mode != 0xFFFFFFFF; }
	// checks if we're either "." or ".."
	bool IsDotDir() const { return entry.data.name[0] == '.' && (entry.data.name[1] == '\0' || (entry.data.name[1] == '.' && entry.data.name[2] == '\0')); }

	static const u32 DefaultDirMode = Mode_Read | Mode_Write | Mode_Execute | Mode_Directory | Mode_Unknown0x0400 | Mode_Used;
	static const u32 DefaultFileMode = Mode_Read | Mode_Write | Mode_Execute | Mode_File | Mode_Unknown0x0080 | Mode_Unknown0x0400 | Mode_Used;

	// used in the cluster entry of empty files on real memory cards, as far as we know
	static const u32 EmptyFileCluster = 0xFFFFFFFFu;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MemoryCardFileEntryCluster
{
	MemoryCardFileEntry entries[2];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MemoryCardPage
{
	static const int PageSize = 0x200;
	u8 raw[PageSize];
};
#pragma pack(pop)

struct MemoryCardFileEntryTreeNode
{
	MemoryCardFileEntry entry;
	std::vector<MemoryCardFileEntryTreeNode> subdir;

	MemoryCardFileEntryTreeNode(const MemoryCardFileEntry& entry)
		: entry(entry)
	{
	}
};

// --------------------------------------------------------------------------------------
//  MemoryCardFileMetadataReference
// --------------------------------------------------------------------------------------
// Helper structure to quickly access file entries from any file data FAT cluster
struct MemoryCardFileMetadataReference
{
	MemoryCardFileMetadataReference* parent;
	MemoryCardFileEntry* entry;
	u32 consecutiveCluster;

	// returns true if filename was modified and metadata containing the actual filename should be written
	bool GetPath(std::string* fileName) const;

	// gives the internal memory card file system path, not to be used for writes to the host file system
	void GetInternalPath(std::string* fileName) const;
};

struct MemoryCardFileHandleStructure
{
	MemoryCardFileMetadataReference* fileRef;
	std::string hostFilePath;
	std::FILE* fileHandle;
};

// --------------------------------------------------------------------------------------
//  FileAccessHelper
// --------------------------------------------------------------------------------------
// Small helper class to keep memory card files opened between calls to Read()/Save()
class FileAccessHelper
{
private:
	std::map<std::string, MemoryCardFileHandleStructure> m_files;
	MemoryCardFileMetadataReference* m_lastWrittenFileRef = nullptr; // we remember this to reduce redundant metadata checks/writes

public:
	FileAccessHelper();
	~FileAccessHelper();

	// Get an already opened file if possible, or open a new one and remember it
	std::FILE* ReOpen(const std::string_view& folderName, MemoryCardFileMetadataReference* fileRef, bool writeMetadata = false);
	// Close all open files that start with the given path, so either a file if a filename is given or all files in a directory and its subdirectories when a directory is given
	void CloseMatching(const std::string_view& path);
	// Close all open files
	void CloseAll();
	// Flush the written data of all open files to the file system
	void FlushAll();

	// Force metadata to be written on next file access, not sure if this is necessary but it can't hurt.
	void ClearMetadataWriteState();

	// removes characters from a PS2 file name that would be illegal in a Windows file system
	// returns true if any changes were made
	static bool CleanMemcardFilename(char* name);

	static void WriteIndex(const std::string& baseFolderName, MemoryCardFileEntry* const entry, MemoryCardFileMetadataReference* const parent);

private:
	// helper function for CleanMemcardFilename()
	static bool CleanMemcardFilenameEndDotOrSpace(char* name, size_t length);

	// Open a new file and remember it for later
	std::FILE* Open(const std::string_view& folderName, MemoryCardFileMetadataReference* fileRef, bool writeMetadata = false);
	// Close a file and delete its handle
	// If entry is given, it also attempts to set the created and modified timestamps of the file according to the entry
	void CloseFileHandle(std::FILE*& file, const MemoryCardFileEntry* entry = nullptr);

	void WriteMetadata(const std::string_view& folderName, const MemoryCardFileMetadataReference* fileRef);
};

// --------------------------------------------------------------------------------------
//  FolderMemoryCard
// --------------------------------------------------------------------------------------
// Fakes a memory card using a regular folder/file structure in the host file system
class FolderMemoryCard
{
public:
	// a few constants so we could in theory change the memory card size without too much effort
	static const int IndirectFatClusterCount = 1; // should be 32 but only 1 is ever used
	static const int PageSize = MemoryCardPage::PageSize;
	static const int ClusterSize = PageSize * 2;
	static const int BlockSize = ClusterSize * 8;
	static const int EccSize = 0x10;
	static const int PageSizeRaw = PageSize + EccSize;
	static const int ClusterSizeRaw = PageSizeRaw * 2;
	static const int BlockSizeRaw = ClusterSizeRaw * 8;
	static const int TotalPages = 0x4000;
	static const int TotalClusters = TotalPages / 2;
	static const int TotalBlocks = TotalClusters / 8;
	static const int TotalSizeRaw = TotalPages * PageSizeRaw;

	static const u32 IndirectFatUnused = 0xFFFFFFFFu;
	static const u32 LastDataCluster = 0x7FFFFFFFu;
	static const u32 NextDataClusterMask = 0x7FFFFFFFu;
	static const u32 DataClusterInUseMask = 0x80000000u;

	static const int FramesAfterWriteUntilFlush = 2;

protected:
	union superBlockUnion
	{
		superblock data;
		u8 raw[BlockSize];
	} m_superBlock;
	union indirectFatUnion
	{
		u32 data[IndirectFatClusterCount][ClusterSize / 4];
		u8 raw[IndirectFatClusterCount][ClusterSize];
	} m_indirectFat;
	union fatUnion
	{
		u32 data[IndirectFatClusterCount][ClusterSize / 4][ClusterSize / 4];
		u8 raw[IndirectFatClusterCount][ClusterSize / 4][ClusterSize];
	} m_fat;
	u8 m_backupBlock1[BlockSize];
	union backupBlock2Union
	{
		u32 programmedBlock;
		u8 raw[BlockSize];
	} m_backupBlock2;

	// stores directory and file metadata
	std::map<u32, MemoryCardFileEntryCluster> m_fileEntryDict;
	// quick-access map of related file entry metadata for each memory card FAT cluster that contains file data
	std::map<u32, MemoryCardFileMetadataReference> m_fileMetadataQuickAccess;

	// holds a copy of modified pages of the memory card before they're flushed to the file system
	std::map<u32, MemoryCardPage> m_cache;
	// contains the state of how the data looked before the first write to it
	// used to reduce the amount of disk I/O by not re-writing unchanged data that just happened to be
	// touched in memory due to how actual physical memory cards have to erase and rewrite in blocks
	std::map<u32, MemoryCardPage> m_oldDataCache;
	// if > 0, the amount of frames until data is flushed to the file system
	// reset to FramesAfterWriteUntilFlush on each write
	int m_framesUntilFlush;
	// used to figure out if contents were changed for savestate-related purposes, see GetCRC()
	u64 m_timeLastWritten;

	// remembers and keeps the last accessed file open for further access
	FileAccessHelper m_lastAccessedFile;

	// path to the folder that contains the files of this memory card
	std::string m_folderName;

	// PS2 memory card slot this card is inserted into
	uint m_slot;

	bool m_isEnabled;

	// if set to false, nothing is actually written to the file system while flushing, and data is discarded instead
	bool m_performFileWrites;

	// currently active filter settings
	bool m_filteringEnabled;
	std::string m_filteringString;

public:
	FolderMemoryCard();
	virtual ~FolderMemoryCard() = default;

	void Lock();
	void Unlock();

	// Initialize & Load Memory Card with values configured in the Memory Card Manager
	void Open(const bool enableFiltering, std::string filter);
	// Initialize & Load Memory Card with provided custom values
	void Open(std::string fullPath, const Pcsx2Config::McdOptions& mcdOptions, const u32 sizeInClusters, const bool enableFiltering, std::string filter, bool simulateFileWrites = false);
	// Close the memory card and flush changes to the file system. Set flush to false to not store changes.
	void Close(bool flush = true);

	// Closes and reopens card with given filter options if they differ from the current ones (returns true),
	// or does nothing if they match already (returns false).
	// Does nothing and returns false when called on a closed memory card.
	bool ReIndex(bool enableFiltering, const std::string& filter);

	s32 IsPresent() const;
	void GetSizeInfo(McdSizeInfo& outways) const;
	bool IsPSX() const;
	s32 Read(u8* dest, u32 adr, int size);
	s32 Save(const u8* src, u32 adr, int size);
	s32 EraseBlock(u32 adr);
	u64 GetCRC() const;

	void SetSlot(uint slot);

	u32 GetSizeInClusters() const;

	// WARNING: The intended use-case for this is resetting back to 8MB if a differently-sized superblock was loaded
	// setting to a different size is untested and will probably not work correctly
	void SetSizeInClusters(u32 clusters);
	// see SetSizeInClusters()
	void SetSizeInMB(u32 megaBytes);

	// called once per frame, used for flushing data after FramesAfterWriteUntilFlush frames of no writes
	void NextFrame();

	static void CalculateECC(u8* ecc, const u8* data);

	void WriteToFile(const std::string& filename);

protected:
	struct EnumeratedFileEntry
	{
		std::string m_fileName;
		time_t m_timeCreated;
		time_t m_timeModified;
		bool m_isFile;
	};

	// initializes memory card data, as if it was fresh from the factory
	void InitializeInternalData();

	bool IsFormatted() const;

	// returns the in-memory address of data the given memory card adr corresponds to
	// returns nullptr if adr corresponds to a folder or file entry
	u8* GetSystemBlockPointer(const u32 adr);

	// returns in-memory address of file or directory metadata searchCluster corresponds to
	// returns nullptr if searchCluster contains something else
	// - searchCluster: the cluster that is being accessed, relative to alloc_offset in the superblock
	// - entryNumber: page of cluster
	// - offset: offset of page
	u8* GetFileEntryPointer(const u32 searchCluster, const u32 entryNumber, const u32 offset);

	// used by GetFileEntryPointer to find the correct cluster
	// returns nullptr if searchCluster is not a file or directory metadata cluster
	// - currentCluster: the cluster we're currently traversing
	// - searchCluster: the cluster we want
	// - fileCount: the number of files left in the directory currently traversed
	MemoryCardFileEntryCluster* GetFileEntryCluster(const u32 currentCluster, const u32 searchCluster, const u32 fileCount);

	// returns file entry of the file at the given searchCluster
	// the passed fileName will be filled with a path to the file being accessed
	// returns nullptr if searchCluster contains no file
	// call by passing:
	// - currentCluster: the root directory cluster as indicated in the superblock
	// - searchCluster: the cluster that is being accessed, relative to alloc_offset in the superblock
	// - fileName: wxFileName of the root directory of the memory card folder in the host file system (filename part doesn't matter)
	// - originalDirCount: the point in fileName where to insert the found folder path, usually fileName->GetDirCount()
	// - outClusterNumber: the cluster's sequential number of the file will be written to this pointer,
	//                     which can be used to calculate the in-file offset of the address being accessed
	MemoryCardFileEntry* GetFileEntryFromFileDataCluster(const u32 currentCluster, const u32 searchCluster, std::string* fileName, const size_t originalDirCount, u32* outClusterNumber);


	// loads files and folders from the host file system if a superblock exists in the root directory
	// - sizeInClusters: total memory card size in clusters, 0 for default
	// - enableFiltering: if set to true, only folders whose name contain the filter string are loaded
	// - filter: can include multiple filters by separating them with "/"
	void LoadMemoryCardData(const u32 sizeInClusters, const bool enableFiltering, const std::string& filter);

	// creates the FAT and indirect FAT
	void CreateFat();

	// creates file entries for the root directory
	void CreateRootDir();


	// returns the system cluster past the highest used one (will be the lowest free one under normal use)
	// this is used for creating the FAT, don't call otherwise unless you know exactly what you're doing
	u32 GetFreeSystemCluster() const;

	// returns the total amount of data clusters available on the memory card, both used and unused
	u32 GetAmountDataClusters() const;

	// returns the lowest unused data cluster, relative to alloc_offset in the superblock
	// returns 0xFFFFFFFFu when the memory card is full
	u32 GetFreeDataCluster() const;

	// returns the amount of unused data clusters
	u32 GetAmountFreeDataClusters() const;

	// returns the final cluster of the file or directory which is (partially) stored in the given cluster
	u32 GetLastClusterOfData(const u32 cluster) const;


	// creates and returns a new file entry in the given directory entry, ready to be filled
	// returns nullptr when the memory card is full
	MemoryCardFileEntry* AppendFileEntryToDir(const MemoryCardFileEntry* const dirEntry);

	// adds a folder in the host file system to the memory card, including all files and subdirectories
	// - dirEntry: the entry of the directory in the parent directory, or the root "." entry
	// - dirPath: the full path to the directory in the host file system
	// - parent: pointer to the parent dir's quick-access reference element
	// - enableFiltering and filter: filter loaded contents, see LoadMemoryCardData()
	bool AddFolder(MemoryCardFileEntry* const dirEntry, const std::string& dirPath, MemoryCardFileMetadataReference* parent = nullptr, const bool enableFiltering = false, const std::string_view& filter = "");

	// adds a file in the host file sytem to the memory card
	// - dirEntry: the entry of the directory in the parent directory, or the root "." entry
	// - dirPath: the full path to the directory containing the file in the host file system
	// - fileName: the name of the file, without path
	// - parent: pointer to the parent dir's quick-access reference element
	bool AddFile(MemoryCardFileEntry* const dirEntry, const std::string& dirPath, const EnumeratedFileEntry& fileEntry, MemoryCardFileMetadataReference* parent = nullptr);

	// calculates the amount of clusters a directory would use up if put into a memory card
	u32 CalculateRequiredClustersOfDirectory(const std::string& dirPath) const;


	// adds a file to the quick-access dictionary, so it can be accessed more efficiently (ie, without searching through the entire file system) later
	// returns the MemoryCardFileMetadataReference of the first file cluster, or nullptr if the file is zero-length
	MemoryCardFileMetadataReference* AddFileEntryToMetadataQuickAccess(MemoryCardFileEntry* const entry, MemoryCardFileMetadataReference* const parent);

	// creates a reference to a directory entry, so it can be passed as parent to other files/directories
	MemoryCardFileMetadataReference* AddDirEntryToMetadataQuickAccess(MemoryCardFileEntry* const entry, MemoryCardFileMetadataReference* const parent);


	// read data from the memory card, ignoring the cache
	// do NOT attempt to read ECC with this method, it will not work
	void ReadDataWithoutCache(u8* const dest, const u32 adr, const u32 dataLength);


	bool ReadFromFile(u8* dest, u32 adr, u32 dataLength);
	bool WriteToFile(const u8* src, u32 adr, u32 dataLength);


	// flush the whole cache to the internal data and/or host file system
	void Flush();

	// flush a single page of the cache to the internal data and/or host file system
	bool FlushPage(const u32 page);

	// flush a memory card cluster of the cache to the internal data and/or host file system
	bool FlushCluster(const u32 cluster);

	// flush a whole memory card block of the cache to the internal data and/or host file system
	bool FlushBlock(const u32 block);

	// flush the superblock to the internal data and/or host file system
	void FlushSuperBlock();

	// flush all directory and file entries to the internal data
	void FlushFileEntries();

	// flush a directory's file entries and all its subdirectories to the internal data
	void FlushFileEntries(const u32 dirCluster, const u32 remainingFiles, const std::string& dirPath = {}, MemoryCardFileMetadataReference* parent = nullptr);

	// "delete" (prepend '_pcsx2_deleted_' to) any files that exist in oldFileEntries but no longer exist in m_fileEntryDict
	// also calls RemoveUnchangedDataFromCache() since both operate on comparing with the old file entires
	void FlushDeletedFilesAndRemoveUnchangedDataFromCache(const std::vector<MemoryCardFileEntryTreeNode>& oldFileEntries);

	// recursive worker method of the above
	// - newCluster: Current directory dotdir cluster of the new entries.
	// - newFileCount: Number of file entries in the new directory.
	// - dirPath: Path to the current directory relative to the root of the memcard. Must be identical for both entries.
	void FlushDeletedFilesAndRemoveUnchangedDataFromCache(const std::vector<MemoryCardFileEntryTreeNode>& oldFileEntries, const u32 newCluster, const u32 newFileCount, const std::string& dirPath);

	// try and remove unchanged data from m_cache
	// oldEntry and newEntry should be equivalent entries found by FindEquivalent()
	void RemoveUnchangedDataFromCache(const MemoryCardFileEntry* const oldEntry, const MemoryCardFileEntry* const newEntry);

	// write data as Save() normally would, but ignore the cache; used for flushing
	s32 WriteWithoutCache(const u8* src, u32 adr, int size);

	// copies the contents of m_fileEntryDict into the tree structure fileEntryTree
	void CopyEntryDictIntoTree(std::vector<MemoryCardFileEntryTreeNode>* fileEntryTree, const u32 cluster, const u32 fileCount);

	// find equivalent (same name and type) of searchEntry in m_fileEntryDict in the directory indicated by cluster
	const MemoryCardFileEntry* FindEquivalent(const MemoryCardFileEntry* searchEntry, const u32 cluster, const u32 fileCount);

	void SetTimeLastReadToNow();
	void SetTimeLastWrittenToNow();

	void AttemptToRecreateIndexFile(const std::string& directory) const;

	std::string GetDisabledMessage(uint slot) const;
	std::string GetCardFullMessage(const std::string& filePath) const;

	// get the list of files (and their timestamps) in directory ordered as specified by the index file
	// for legacy entries without an entry in the index file, order is unspecified and should not be relied on
	std::vector<EnumeratedFileEntry> GetOrderedFiles(const std::string& dirPath) const;

	void DeleteFromIndex(const std::string& filePath, const std::string_view& entry) const;
};

// --------------------------------------------------------------------------------------
//  FolderMemoryCardAggregator
// --------------------------------------------------------------------------------------
// Forwards the API's requests for specific memory card slots to the correct FolderMemoryCard.
class FolderMemoryCardAggregator
{
protected:
	static const int TotalCardSlots = 8;
	FolderMemoryCard m_cards[TotalCardSlots];

	// stores the specifics of the current filtering settings, so they can be
	// re-applied automatically when memory cards are reloaded
	bool m_enableFiltering = true;
	std::string m_lastKnownFilter;

public:
	FolderMemoryCardAggregator();
	virtual ~FolderMemoryCardAggregator() = default;

	void Open();
	void Close();

	void SetFiltering(const bool enableFiltering);

	s32 IsPresent(uint slot);
	void GetSizeInfo(uint slot, McdSizeInfo& outways);
	bool IsPSX(uint slot);
	s32 Read(uint slot, u8* dest, u32 adr, int size);
	s32 Save(uint slot, const u8* src, u32 adr, int size);
	s32 EraseBlock(uint slot, u32 adr);
	u64 GetCRC(uint slot);
	void NextFrame(uint slot);
	bool ReIndex(uint slot, const bool enableFiltering, const std::string& filter);
};
