/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include "System.h"
#include "Utilities/Exceptions.h"

enum class FreezeAction
{
	Load,
	Save,
	Size,
};

// Savestate Versioning!
//  If you make changes to the savestate version, please increment the value below.
//  If the change is minor and compatibility with old states is retained, increment
//  the lower 16 bit value.  IF the change is breaking of all compatibility with old
//  states, increment the upper 16 bit value, and clear the lower 16 bits to 0.

static const u32 g_SaveVersion = (0x9A22 << 16) | 0x0000;

// the freezing data between submodules and core
// an interesting thing to note is that this dates back from before plugin
// merges and was used to pass data between plugins and cores, although the
// struct was system dependant as the size of int differs between systems, thus
// subsystems making use of freezeData, like GSDump and save states aren't
// necessarily portable; we might want to investigate this in the future -- govanify
typedef struct
{
    int size;
    s8 *data;
} freezeData;

// this function is meant to be used in the place of GSfreeze, and provides a safe layer
// between the GS saving function and the MTGS's needs. :)
extern s32 CALLBACK gsSafeFreeze( int mode, freezeData *data );

// --------------------------------------------------------------------------------------
//  SaveStateBase class
// --------------------------------------------------------------------------------------
// Provides the base API for both loading and saving savestates.  Normally you'll want to
// use one of the four "functional" derived classes rather than this class directly: gzLoadingState, gzSavingState (gzipped disk-saved
// states), and memLoadingState, memSavingState (uncompressed memory states).
class SaveStateBase
{
protected:
	VmStateBuffer* m_memory;
	char m_tagspace[32];

	u32 m_version;		// version of the savestate being loaded.

	int m_idx;			// current read/write index of the allocation

public:
	SaveStateBase( VmStateBuffer& memblock );
	SaveStateBase( VmStateBuffer* memblock );
	virtual ~SaveStateBase() { }

	static wxString GetFilename( int slot );

	// Gets the version of savestate that this object is acting on.
	// The version refers to the low 16 bits only (high 16 bits classifies Pcsx2 build types)
	u32 GetVersion() const
	{
		return (m_version & 0xffff);
	}

	virtual SaveStateBase& FreezeMainMemory();
	virtual SaveStateBase& FreezeBios();
	virtual SaveStateBase& FreezeInternals();

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

	uint GetCurrentPos() const
	{
		return m_idx;
	}

	u8* GetBlockPtr()
	{
		return m_memory->GetPtr(m_idx);
	}
	
	u8* GetPtrEnd() const
	{
		return m_memory->GetPtrEnd();
	}

	void CommitBlock( int size )
	{
		m_idx += size;
	}

	// Freezes an identifier value into the savestate for troubleshooting purposes.
	// Identifiers can be used to determine where in a savestate that data has become
	// skewed (if the value does not match then the error occurs somewhere prior to that
	// position).
	void FreezeTag( const char* src );

	// Returns true if this object is a StateLoading type object.
	bool IsLoading() const { return !IsSaving(); }

	// Loads or saves a memory block.
	virtual void FreezeMem( void* data, int size )=0;

	// Returns true if this object is a StateSaving type object.
	virtual bool IsSaving() const=0;

public:
	// note: gsFreeze() needs to be public because of the GSState recorder.
	void gsFreeze();

protected:
	void Init( VmStateBuffer* memblock );

	// Load/Save functions for the various components of our glorious emulator!

	void mtvuFreeze();
	void rcntFreeze();
	void vuMicroFreeze();
	void vif0Freeze();
	void vif1Freeze();
	void sifFreeze();
	void ipuFreeze();
	void ipuDmaFreeze();
	void gifFreeze();
	void gifDmaFreeze();
	void gifPathFreeze(u32 path); // called by gifFreeze()

	void sprFreeze();

	void sioFreeze();
	void cdrFreeze();
	void cdvdFreeze();
	void psxRcntFreeze();
	void sio2Freeze();

	void deci2Freeze();

	// Save or load PCSX2's global frame counter (g_FrameCount) along with each savestate
	//
	// This is to prevent any inaccuracy issues caused by having a different
	// internal emulation frame count than what it was at the beginning of the
	// original recording
	void InputRecordingFreeze();
};

// --------------------------------------------------------------------------------------
//  Saving and Loading Specialized Implementations...
// --------------------------------------------------------------------------------------

class memSavingState : public SaveStateBase
{
	typedef SaveStateBase _parent;

protected:
	static const int ReallocThreshold		= _1mb / 4;		// 256k reallocation block size.
	static const int MemoryBaseAllocSize	= _8mb;			// 8 meg base alloc when PS2 main memory is excluded

public:
	virtual ~memSavingState() = default;
	memSavingState( VmStateBuffer& save_to );
	memSavingState( VmStateBuffer* save_to );

	void MakeRoomForData();

	void FreezeMem( void* data, int size );

	bool IsSaving() const { return true; }
};

class memLoadingState : public SaveStateBase
{
public:
	virtual ~memLoadingState() = default;

	memLoadingState( const VmStateBuffer& load_from );
	memLoadingState( const VmStateBuffer* load_from );

	void FreezeMem( void* data, int size );

	bool IsSaving() const { return false; }
	bool IsFinished() const { return m_idx >= m_memory->GetSizeInBytes(); }
};


namespace Exception
{
	// Exception thrown when a corrupted or truncated savestate is encountered.
	class SaveStateLoadError : public BadStream
	{
		DEFINE_STREAM_EXCEPTION(SaveStateLoadError, BadStream)

		virtual wxString FormatDiagnosticMessage() const;
		virtual wxString FormatDisplayMessage() const;
	};
}; // namespace Exception

