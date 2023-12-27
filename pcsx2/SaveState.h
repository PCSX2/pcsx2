// SPDX-FileCopyrightText: 2002-2023 PCSX2 Dev Team
// SPDX-License-Identifier: LGPL-3.0+

#pragma once

#include <deque>
#include <memory>
#include <string>
#include <vector>

#include "common/Assertions.h"

class Error;

enum class FreezeAction
{
	Load,
	Save,
	Size,
};

// Savestate Versioning!

// NOTICE: When updating g_SaveVersion, please make sure you add the following line to your commit message somewhere:
// [SAVEVERSION+]
// This informs the auto updater that the users savestates will be invalidated.

static const u32 g_SaveVersion = (0x9A4B << 16) | 0x0000;


// the freezing data between submodules and core
// an interesting thing to note is that this dates back from before plugin
// merges and was used to pass data between plugins and cores, although the
// struct was system dependant as the size of int differs between systems, thus
// subsystems making use of freezeData, like GSDump and save states aren't
// necessarily portable; we might want to investigate this in the future -- govanify
struct freezeData
{
	int size;
	u8* data;
};

struct SaveStateScreenshotData
{
	u32 width;
	u32 height;
	std::vector<u32> pixels;
};

class ArchiveEntryList;

// Wrappers to generate a save state compatible across all frontends.
// These functions assume that the caller has paused the core thread.
extern std::unique_ptr<ArchiveEntryList> SaveState_DownloadState(Error* error);
extern std::unique_ptr<SaveStateScreenshotData> SaveState_SaveScreenshot();
extern bool SaveState_ZipToDisk(std::unique_ptr<ArchiveEntryList> srclist, std::unique_ptr<SaveStateScreenshotData> screenshot, const char* filename);
extern bool SaveState_ReadScreenshot(const std::string& filename, u32* out_width, u32* out_height, std::vector<u32>* out_pixels);
extern bool SaveState_UnzipFromDisk(const std::string& filename, Error* error);

// --------------------------------------------------------------------------------------
//  SaveStateBase class
// --------------------------------------------------------------------------------------
// Provides the base API for both loading and saving savestates.  Normally you'll want to
// use one of the four "functional" derived classes rather than this class directly: gzLoadingState, gzSavingState (gzipped disk-saved
// states), and memLoadingState, memSavingState (uncompressed memory states).
class SaveStateBase
{
public:
	using VmStateBuffer = std::vector<u8>;

protected:
	VmStateBuffer& m_memory;

	u32 m_version = 0;		// version of the savestate being loaded.

	int m_idx = 0;			// current read/write index of the allocation

	bool m_error = false; // error occurred while reading/writing

public:
	SaveStateBase(VmStateBuffer& memblock);
	virtual ~SaveStateBase() = default;

	__fi bool HasError() const { return m_error; }
	__fi bool IsOkay() const { return !m_error; }

	// Gets the version of savestate that this object is acting on.
	// The version refers to the low 16 bits only (high 16 bits classifies Pcsx2 build types)
	u32 GetVersion() const
	{
		return (m_version & 0xffff);
	}

	bool FreezeBios();
	bool FreezeInternals();

	// Loads or saves an arbitrary data type.  Usable on atomic types, structs, and arrays.
	// For dynamically allocated pointers use FreezeMem instead.
	template<typename T>
	void Freeze( T& data )
	{
		FreezeMem( const_cast<void*>((void*)&data), sizeof( T ) );
	}

	// FreezeLegacy can be used to load structures short of their full size, which is
	// useful for loading structures that have had new stuff added since a previous version.
	template<typename T>
	void FreezeLegacy( T& data, int sizeOfNewStuff )
	{
		FreezeMem( &data, sizeof( T ) - sizeOfNewStuff );
	}

	void PrepBlock( int size );

	template <typename T>
	void FreezeDeque(std::deque<T>& q)
	{
		// overwritten when loading
		u32 count = static_cast<u32>(q.size());
		Freeze(count);

		// have to use a temp array, because deque doesn't have a contiguous block of memory
		std::unique_ptr<T[]> temp;
		if (count > 0)
		{
			temp = std::make_unique<T[]>(count);
			if (IsSaving())
			{
				u32 pos = 0;
				for (const T& it : q)
					temp[pos++] = it;
			}

			FreezeMem(temp.get(), static_cast<int>(sizeof(T) * count));
		}

		if (IsLoading())
		{
			q.clear();
			for (u32 i = 0; i < count; i++)
				q.push_back(temp[i]);
		}
	}

	void FreezeString(std::string& s)
	{
		// overwritten when loading
		u32 length = static_cast<u32>(s.length());
		Freeze(length);

		if (IsLoading())
			s.resize(length);

		FreezeMem(s.data(), length);
	}

	uint GetCurrentPos() const
	{
		return m_idx;
	}

	u8* GetBlockPtr()
	{
		return &m_memory[m_idx];
	}

	void CommitBlock( int size )
	{
		m_idx += size;
	}

	// Freezes an identifier value into the savestate for troubleshooting purposes.
	// Identifiers can be used to determine where in a savestate that data has become
	// skewed (if the value does not match then the error occurs somewhere prior to that
	// position).
	bool FreezeTag( const char* src );

	// Returns true if this object is a StateLoading type object.
	bool IsLoading() const { return !IsSaving(); }

	// Loads or saves a memory block.
	virtual void FreezeMem( void* data, int size )=0;

	// Returns true if this object is a StateSaving type object.
	virtual bool IsSaving() const=0;

public:
	// note: gsFreeze() needs to be public because of the GSState recorder.
	bool gsFreeze();

protected:
	bool vmFreeze();
	bool mtvuFreeze();
	bool rcntFreeze();
	bool memFreeze();
	bool vuMicroFreeze();
	bool vuJITFreeze();
	bool vif0Freeze();
	bool vif1Freeze();
	bool sifFreeze();
	bool ipuFreeze();
	bool ipuDmaFreeze();
	bool gifFreeze();
	bool gifDmaFreeze();
	bool gifPathFreeze(u32 path); // called by gifFreeze()

	bool sprFreeze();

	bool sioFreeze();
	bool cdrFreeze();
	bool cdvdFreeze();
	bool psxRcntFreeze();
	bool deci2Freeze();

	// Save or load PCSX2's global frame counter (g_FrameCount) along with each savestate
	//
	// This is to prevent any inaccuracy issues caused by having a different
	// internal emulation frame count than what it was at the beginning of the
	// original recording
	bool InputRecordingFreeze();
};

// --------------------------------------------------------------------------------------
//  ArchiveEntry
// --------------------------------------------------------------------------------------
class ArchiveEntry final
{
protected:
	std::string	m_filename;
	uptr		m_dataidx;
	size_t		m_datasize;

public:
	ArchiveEntry(std::string filename)
		: m_filename(std::move(filename))
	{
		m_dataidx = 0;
		m_datasize = 0;
	}

	~ArchiveEntry() = default;

	ArchiveEntry& SetDataIndex(uptr idx)
	{
		m_dataidx = idx;
		return *this;
	}

	ArchiveEntry& SetDataSize(size_t size)
	{
		m_datasize = size;
		return *this;
	}

	const std::string& GetFilename() const
	{
		return m_filename;
	}

	uptr GetDataIndex() const
	{
		return m_dataidx;
	}

	uint GetDataSize() const
	{
		return m_datasize;
	}
};

// --------------------------------------------------------------------------------------
//  ArchiveEntryList
// --------------------------------------------------------------------------------------
class ArchiveEntryList final
{
public:
	using VmStateBuffer = std::vector<u8>;
	DeclareNoncopyableObject(ArchiveEntryList);

protected:
	std::vector<ArchiveEntry> m_list;
	VmStateBuffer m_data;

public:
	ArchiveEntryList() = default;
	~ArchiveEntryList() = default;

	const VmStateBuffer& GetBuffer() const
	{
		return m_data;
	}

	VmStateBuffer& GetBuffer()
	{
		return m_data;
	}

	u8* GetPtr(uint idx)
	{
		return &m_data[idx];
	}

	const u8* GetPtr(uint idx) const
	{
		return &m_data[idx];
	}

	ArchiveEntryList& Add(const ArchiveEntry& src)
	{
		m_list.push_back(src);
		return *this;
	}

	size_t GetLength() const
	{
		return m_list.size();
	}

	ArchiveEntry& operator[](uint idx)
	{
		return m_list[idx];
	}

	const ArchiveEntry& operator[](uint idx) const
	{
		return m_list[idx];
	}
};

// --------------------------------------------------------------------------------------
//  Saving and Loading Specialized Implementations...
// --------------------------------------------------------------------------------------

class memSavingState final : public SaveStateBase
{
	typedef SaveStateBase _parent;

public:
	memSavingState(VmStateBuffer& save_to);
	~memSavingState() override = default;

	void FreezeMem(void* data, int size) override;
	bool IsSaving() const override { return true; }
};

class memLoadingState final : public SaveStateBase
{
public:
	memLoadingState(const VmStateBuffer& load_from);
	~memLoadingState() override = default;

	void FreezeMem(void* data, int size) override;
	bool IsSaving() const override { return false; }
};
