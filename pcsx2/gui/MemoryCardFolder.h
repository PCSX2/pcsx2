/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
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

#include <wx/file.h>
#include <wx/dir.h>
#include <wx/stopwatch.h>
#include <wx/ffile.h>
#include <map>

#include "PluginCallbacks.h"

// --------------------------------------------------------------------------------------
//  Superblock Header Struct
// --------------------------------------------------------------------------------------
#pragma pack(push, 1)
struct superblock {
	char magic[28]; 			// 0x00
	char version[12]; 			// 0x1c
	u16 page_len; 				// 0x28
	u16 pages_per_cluster;	 	// 0x2a
	u16 pages_per_block;		// 0x2c
	u16 unused; 				// 0x2e
	u32 clusters_per_card;	 	// 0x30
	u32 alloc_offset; 			// 0x34
	u32 alloc_end; 				// 0x38
	u32 rootdir_cluster;		// 0x3c
	u32 backup_block1;			// 0x40
	u32 backup_block2;			// 0x44
	u64 padding0x48;			// 0x48
	u32 ifc_list[32]; 			// 0x50
	u32 bad_block_list[32]; 	// 0xd0
	u8 card_type; 				// 0x150
	u8 card_flags; 				// 0x151
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MemoryCardFileEntryDateTime {
	u8 unused;
	u8 second;
	u8 minute;
	u8 hour;
	u8 day;
	u8 month;
	u16 year;
};
#pragma pack(pop)

// --------------------------------------------------------------------------------------
//  MemoryCardFileEntry
// --------------------------------------------------------------------------------------
// Structure for directory and file relationships as stored on memory cards
#pragma pack(push, 1)
struct MemoryCardFileEntry {
	union {
		struct MemoryCardFileEntryData {
			u32 mode;
			u32 length; // number of bytes for file, number of files for dir
			union {
				MemoryCardFileEntryDateTime data;
				u64 value;
				u8 raw[8];
			} timeCreated;
			u32 cluster; // cluster the start of referred file or folder can be found in
			u32 dirEntry; // parent directory entry number, only used if "." entry of subdir
			union {
				MemoryCardFileEntryDateTime data;
				u64 value;
				u8 raw[8];
			} timeModified;
			u32 attr;
			u8 padding[0x1C];
			u8 name[0x20];
			u8 unused[0x1A0];
		} data;
		u8 raw[0x200];
	} entry;

	bool IsFile() { return !!( entry.data.mode & 0x0010 ); }
	bool IsDir()  { return !!( entry.data.mode & 0x0020 ); }
	bool IsUsed() { return !!( entry.data.mode & 0x8000 ); }
	bool IsValid() { return entry.data.mode != 0xFFFFFFFF; }

	static const u32 DefaultDirMode = 0x8427;
	static const u32 DefaultFileMode = 0x8497;
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MemoryCardFileEntryCluster {
	MemoryCardFileEntry entries[2];
};
#pragma pack(pop)

#pragma pack(push, 1)
struct MemoryCardPage {
	static const int PageSize = 0x200;
	u8 raw[PageSize];
};
#pragma pack(pop)

// --------------------------------------------------------------------------------------
//  FolderMemoryCard
// --------------------------------------------------------------------------------------
// Fakes a memory card using a regular folder/file structure in the host file system
class FolderMemoryCard {
protected:
	wxFileName folderName;

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

	static const int FramesAfterWriteUntilFlush = 60;

	union superBlockUnion {
		superblock data;
		u8 raw[BlockSize];
	} m_superBlock;
	union indirectFatUnion {
		u32 data[IndirectFatClusterCount][ClusterSize / 4];
		u8 raw[IndirectFatClusterCount][ClusterSize];
	} m_indirectFat;
	union fatUnion {
		u32 data[IndirectFatClusterCount][ClusterSize / 4][ClusterSize / 4];
		u8 raw[IndirectFatClusterCount][ClusterSize / 4][ClusterSize];
	} m_fat;
	u8 m_backupBlock1[BlockSize];
	u8 m_backupBlock2[BlockSize];

	std::map<u32, MemoryCardFileEntryCluster> m_fileEntryDict;

	// holds a copy of modified areas of the memory card, in page-sized chunks
	std::map<u32, MemoryCardPage> m_cache;

	uint m_slot;
	bool m_isEnabled;
	u64 m_timeLastWritten;
	int m_framesUntilFlush;

public:
	FolderMemoryCard();
	virtual ~FolderMemoryCard() throw() {}

	void Lock();
	void Unlock();

	void Open( const bool enableFiltering, const wxString& filter );
	void Close();

	s32  IsPresent();
	void GetSizeInfo( PS2E_McdSizeInfo& outways );
	bool IsPSX();
	s32  Read( u8 *dest, u32 adr, int size );
	s32  Save( const u8 *src, u32 adr, int size );
	s32  EraseBlock( u32 adr );
	u64  GetCRC();

	void SetSlot( uint slot );

	// called once per frame, used for flushing data after FramesAfterWriteUntilFlush frames of no writes
	void NextFrame();

	static void CalculateECC( u8* ecc, const u8* data );

protected:
	// initializes memory card data, as if it was fresh from the factory
	void InitializeInternalData();

	bool IsFormatted();

	// returns the in-memory address of data the given memory card adr corresponds to
	// returns nullptr if adr corresponds to a folder or file entry
	u8* GetSystemBlockPointer( const u32 adr );

	// returns in-memory address of file or directory metadata searchCluster corresponds to
	// returns nullptr if searchCluster contains something else
	// originally call by passing:
	// - currentCluster: the root directory cluster as indicated in the superblock
	// - searchCluster: the cluster that is being accessed, relative to alloc_offset in the superblock
	// - entryNumber: page of cluster
	// - offset: offset of page
	u8* GetFileEntryPointer( const u32 currentCluster, const u32 searchCluster, const u32 entryNumber, const u32 offset );

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
	MemoryCardFileEntry* GetFileEntryFromFileDataCluster( const u32 currentCluster, const u32 searchCluster, wxFileName* fileName, const size_t originalDirCount, u32* outClusterNumber );


	// loads files and folders from the host file system if a superblock exists in the root directory
	// if enableFiltering is set to true, only folders whose name contain the filter string are loaded
	// filter string can include multiple filters by separating them with "/"
	void LoadMemoryCardData( const bool enableFiltering, const wxString& filter );

	// creates the FAT and indirect FAT
	void CreateFat();

	// creates file entries for the root directory
	void CreateRootDir();


	// returns the system cluster past the highest used one (will be the lowest free one under normal use)
	// this is used for creating the FAT, don't call otherwise unless you know exactly what you're doing
	u32 GetFreeSystemCluster();

	// returns the lowest unused data cluster, relative to alloc_offset in the superblock
	// returns 0xFFFFFFFFu when the memory card is full
	u32 GetFreeDataCluster();

	// returns the amount of unused data clusters
	u32 GetAmountFreeDataClusters();

	// returns the final cluster of the file or directory which is (partially) stored in the given cluster
	u32 GetLastClusterOfData( const u32 cluster );

	u64 ConvertToMemoryCardTimestamp( const wxDateTime& time );


	// creates and returns a new file entry in the given directory entry, ready to be filled
	// returns nullptr when the memory card is full
	MemoryCardFileEntry* AppendFileEntryToDir( MemoryCardFileEntry* const dirEntry );

	// adds a folder in the host file system to the memory card, including all files and subdirectories
	// - dirEntry: the entry of the directory in the parent directory, or the root "." entry
	// - dirPath: the full path to the directory in the host file system
	// - enableFiltering and filter: filter loaded contents, see LoadMemoryCardData()
	bool AddFolder( MemoryCardFileEntry* const dirEntry, const wxString& dirPath, const bool enableFiltering = false, const wxString& filter = L"" );

	// adds a file in the host file sytem to the memory card
	// - dirEntry: the entry of the directory in the parent directory, or the root "." entry
	// - dirPath: the full path to the directory containing the file in the host file system
	// - fileName: the name of the file, without path
	bool AddFile( MemoryCardFileEntry* const dirEntry, const wxString& dirPath, const wxString& fileName );


	bool ReadFromFile( u8 *dest, u32 adr, u32 dataLength );
	bool WriteToFile( const u8* src, u32 adr, u32 dataLength );


	// flush the whole cache to the internal data and/or host file system
	void Flush();

	// flush a single page of the cache to the internal data and/or host file system
	void Flush( const u32 page );

	// flush a directory's file entries and all its subdirectories to the internal data
	void FlushFileEntries( const u32 dirCluster, const u32 remainingFiles, const wxString& dirPath = L"" );

	// write data as Save() normally would, but ignore the cache; used for flushing
	s32 WriteWithoutCache( const u8 *src, u32 adr, int size );

	void SetTimeLastWrittenToNow();

	wxString GetDisabledMessage( uint slot ) const {
		return wxsFormat( pxE( L"The PS2-slot %d has been automatically disabled.  You can correct the problem\nand re-enable it at any time using Config:Memory cards from the main menu."
			), slot//TODO: translate internal slot index to human-readable slot description
			);
	}
	wxString GetCardFullMessage( const wxString& filePath ) const {
		return wxsFormat( pxE( L"(FolderMcd) Memory Card is full, could not add: %s" ), WX_STR( filePath ) );
	}
};

// --------------------------------------------------------------------------------------
//  FolderMemoryCardAggregator
// --------------------------------------------------------------------------------------
// Forwards the API's requests for specific memory card slots to the correct FolderMemoryCard.
class FolderMemoryCardAggregator {
protected:
	static const int totalCardSlots = 8;
	FolderMemoryCard m_cards[totalCardSlots];
	bool m_enableFiltering = true;
	wxString m_lastKnownFilter = L"";

public:
	FolderMemoryCardAggregator();
	virtual ~FolderMemoryCardAggregator() throw( ) {}

	void Open();
	void Close();

	void SetFiltering( const bool enableFiltering );

	s32  IsPresent( uint slot );
	void GetSizeInfo( uint slot, PS2E_McdSizeInfo& outways );
	bool IsPSX( uint slot );
	s32  Read( uint slot, u8 *dest, u32 adr, int size );
	s32  Save( uint slot, const u8 *src, u32 adr, int size );
	s32  EraseBlock( uint slot, u32 adr );
	u64  GetCRC( uint slot );
	void NextFrame( uint slot );
	void ReIndex( uint slot, const bool enableFiltering, const wxString& filter );
};
