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


#include "PrecompiledHeader.h"
#include "SaveState.h"

#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/SafeArray.inl"
#include "common/ScopedGuard.h"
#include "common/StringUtil.h"
#include "common/ZipHelpers.h"

#include "ps2/BiosTools.h"
#include "COP0.h"
#include "VUmicro.h"
#include "MTVU.h"
#include "Cache.h"
#include "Config.h"
#include "CDVD/CDVD.h"
#include "R3000A.h"
#include "Elfheader.h"
#include "Counters.h"
#include "Patch.h"
#include "DebugTools/Breakpoints.h"
#include "Host.h"
#include "GS.h"
#include "GS/GS.h"
#include "SPU2/spu2.h"
#include "USB/USB.h"
#include "PAD/Gamepad.h"

#ifndef PCSX2_CORE
#include "gui/App.h"
#include "gui/ConsoleLogger.h"
#include "gui/SysThreads.h"
#else
#include "VMManager.h"
#endif

#include "fmt/core.h"

#include <csetjmp>
#include <png.h>

using namespace R5900;

static void PreLoadPrep()
{
	// ensure everything is in sync before we start overwriting stuff.
	if (THREAD_VU1)
		vu1Thread.WaitVU();
	GetMTGS().WaitGS(false);
	SysClearExecutionCache();
#ifndef PCSX2_CORE
	PatchesVerboseReset();
#endif
}

static void PostLoadPrep()
{
	resetCache();
//	WriteCP0Status(cpuRegs.CP0.n.Status.val);
	for(int i=0; i<48; i++) MapTLB(i);
	if (EmuConfig.Gamefixes.GoemonTlbHack) GoemonPreloadTlb();
	CBreakPoints::SetSkipFirst(BREAKPOINT_EE, 0);
	CBreakPoints::SetSkipFirst(BREAKPOINT_IOP, 0);

	UpdateVSyncRate();
}

// --------------------------------------------------------------------------------------
//  SaveStateBase  (implementations)
// --------------------------------------------------------------------------------------
#ifndef PCSX2_CORE
std::string SaveStateBase::GetSavestateFolder(int slot, bool isSavingOrLoading)
{
	std::string CRCvalue(StringUtil::StdStringFromFormat("%08X", ElfCRC));
	std::string serialName;

	if (g_GameStarted || g_GameLoading)
	{
		if (DiscSerial.empty())
		{
			// Running homebrew/standalone ELF, return only the ELF name.
			// can't use FileSystem here because it's DOS paths
			const std::string::size_type pos = std::min(DiscSerial.rfind('/'), DiscSerial.rfind('\\'));
			if (pos != std::string::npos)
				serialName = DiscSerial.substr(pos + 1);
			else
				serialName = DiscSerial;
		}
		else
		{
			// Running a normal retail game
			// Folder format is "SLXX-XXXX - (00000000)"
			serialName = DiscSerial;
		}
	}
	else
	{
		// Still inside the BIOS/not running a game (why would anyone want to do this?)
		serialName = StringUtil::StdStringFromFormat("BIOS (%s v%u.%u)", BiosZone.c_str(), (BiosVersion >> 8), BiosVersion & 0xff);
		CRCvalue = "None";
	}

	const std::string dir(StringUtil::StdStringFromFormat("%s" FS_OSPATH_SEPARATOR_STR "%s - (%s)",
		g_Conf->Folders.Savestates.ToUTF8().data(), serialName.c_str(), CRCvalue.c_str()));

	if (isSavingOrLoading)
	{
		if (!FileSystem::DirectoryExists(dir.c_str()))
		{
			// sstates should exist, no need to create it
			FileSystem::CreateDirectoryPath(dir.c_str(), false);
		}
	}

	return Path::Combine(dir, StringUtil::StdStringFromFormat("%s (%s).%02d.p2s",
		serialName.c_str(), CRCvalue.c_str(), slot));
}
#endif

SaveStateBase::SaveStateBase( SafeArray<u8>& memblock )
{
	Init( &memblock );
}

SaveStateBase::SaveStateBase( SafeArray<u8>* memblock )
{
	Init( memblock );
}

void SaveStateBase::Init( SafeArray<u8>* memblock )
{
	m_memory	= memblock;
	m_version	= g_SaveVersion;
	m_idx		= 0;
}

void SaveStateBase::PrepBlock( int size )
{
	pxAssertDev( m_memory, "Savestate memory/buffer pointer is null!" );

	const int end = m_idx+size;
	if( IsSaving() )
		m_memory->MakeRoomFor( end );
	else
	{
		if( m_memory->GetSizeInBytes() < end )
			throw Exception::SaveStateLoadError();
	}
}

void SaveStateBase::FreezeTag( const char* src )
{
	const uint allowedlen = sizeof( m_tagspace )-1;
	pxAssertDev(strlen(src) < allowedlen, "Tag name exceeds the allowed length");

	memzero( m_tagspace );
	strcpy( m_tagspace, src );
	Freeze( m_tagspace );

	if( strcmp( m_tagspace, src ) != 0 )
	{
		std::string msg(fmt::format("Savestate data corruption detected while reading tag: {}", src));
		pxFail( msg.c_str() );
		throw Exception::SaveStateLoadError().SetDiagMsg(std::move(msg));
	}
}

SaveStateBase& SaveStateBase::FreezeBios()
{
	FreezeTag( "BIOS" );

	// Check the BIOS, and issue a warning if the bios for this state
	// doesn't match the bios currently being used (chances are it'll still
	// work fine, but some games are very picky).
	
	u32 bioscheck = BiosChecksum;
	char biosdesc[256];
	memzero( biosdesc );
	memcpy( biosdesc, BiosDescription.c_str(), std::min( sizeof(biosdesc), BiosDescription.length() ) );
	
	Freeze( bioscheck );
	Freeze( biosdesc );

	if (bioscheck != BiosChecksum)
	{
		Console.Newline();
		Console.Indent(1).Error( "Warning: BIOS Version Mismatch, savestate may be unstable!" );
		Console.Indent(2).Error(
			"Current BIOS:   %s (crc=0x%08x)\n"
			"Savestate BIOS: %s (crc=0x%08x)\n",
			BiosDescription.c_str(), BiosChecksum,
			biosdesc, bioscheck
		);
	}
	
	return *this;
}

SaveStateBase& SaveStateBase::FreezeInternals()
{
	// Print this until the MTVU problem in gifPathFreeze is taken care of (rama)
	if (THREAD_VU1) Console.Warning("MTVU speedhack is enabled, saved states may not be stable");
	
	// Second Block - Various CPU Registers and States
	// -----------------------------------------------
	FreezeTag( "cpuRegs" );
	Freeze(cpuRegs);		// cpu regs + COP0
	Freeze(psxRegs);		// iop regs
	Freeze(fpuRegs);
	Freeze(tlb);			// tlbs
	Freeze(AllowParams1);	//OSDConfig written (Fast Boot)
	Freeze(AllowParams2);
	Freeze(g_GameStarted);
	Freeze(g_GameLoading);
	Freeze(ElfCRC);

	char localDiscSerial[256];
	StringUtil::Strlcpy(localDiscSerial, DiscSerial.c_str(), sizeof(localDiscSerial));
	Freeze(localDiscSerial);
	if (IsLoading())
		DiscSerial = localDiscSerial;

	// Third Block - Cycle Timers and Events
	// -------------------------------------
	FreezeTag( "Cycles" );
	Freeze(EEsCycle);
	Freeze(EEoCycle);
	Freeze(iopCycleEE);
	Freeze(iopBreak);
	Freeze(g_nextEventCycle);
	Freeze(g_iopNextEventCycle);
	Freeze(s_iLastCOP0Cycle);
	Freeze(s_iLastPERFCycle);
	Freeze(nextCounter);
	Freeze(nextsCounter);
	Freeze(psxNextsCounter);
	Freeze(psxNextCounter);

	// Fourth Block - EE-related systems
	// ---------------------------------
	FreezeTag( "EE-Subsystems" );
	rcntFreeze();
	gsFreeze();
	vuMicroFreeze();
	vuJITFreeze();
	vif0Freeze();
	vif1Freeze();
	sifFreeze();
	ipuFreeze();
	ipuDmaFreeze();
	gifFreeze();
	gifDmaFreeze();
	sprFreeze();
	mtvuFreeze();

	// Fifth Block - iop-related systems
	// ---------------------------------
	FreezeTag( "IOP-Subsystems" );
	FreezeMem(iopMem->Sif, sizeof(iopMem->Sif));		// iop's sif memory (not really needed, but oh well)

	psxRcntFreeze();
	sioFreeze();
	sio2Freeze();
	cdrFreeze();
	cdvdFreeze();

	// technically this is HLE BIOS territory, but we don't have enough such stuff
	// to merit an HLE Bios sub-section... yet.
	deci2Freeze();

	InputRecordingFreeze();

	return *this;
}


// --------------------------------------------------------------------------------------
//  memSavingState (implementations)
// --------------------------------------------------------------------------------------
// uncompressed to/from memory state saves implementation

memSavingState::memSavingState( SafeArray<u8>& save_to )
	: SaveStateBase( save_to )
{
}

memSavingState::memSavingState( SafeArray<u8>* save_to )
	: SaveStateBase( save_to )
{
}

// Saving of state data
void memSavingState::FreezeMem( void* data, int size )
{
	if (!size) return;

	m_memory->MakeRoomFor( m_idx + size );
	memcpy( m_memory->GetPtr(m_idx), data, size );
	m_idx += size;
}

void memSavingState::MakeRoomForData()
{
	pxAssertDev( m_memory, "Savestate memory/buffer pointer is null!" );

	m_memory->ChunkSize = ReallocThreshold;
	m_memory->MakeRoomFor( m_idx + MemoryBaseAllocSize );
}

// --------------------------------------------------------------------------------------
//  memLoadingState  (implementations)
// --------------------------------------------------------------------------------------
memLoadingState::memLoadingState( const SafeArray<u8>& load_from )
	: SaveStateBase( const_cast<SafeArray<u8>&>(load_from) )
{
}

memLoadingState::memLoadingState( const SafeArray<u8>* load_from )
	: SaveStateBase( const_cast<SafeArray<u8>*>(load_from) )
{
}

// Loading of state data from a memory buffer...
void memLoadingState::FreezeMem( void* data, int size )
{
	const u8* const src = m_memory->GetPtr(m_idx);
	m_idx += size;
	memcpy( data, src, size );
}

std::string Exception::SaveStateLoadError::FormatDiagnosticMessage() const
{
	std::string retval = "Savestate is corrupt or incomplete!\n";
	Host::AddOSDMessage("Error: Savestate is corrupt or incomplete!", 15.0f);
	_formatDiagMsg(retval);
	return retval;
}

std::string Exception::SaveStateLoadError::FormatDisplayMessage() const
{
	std::string retval = "The savestate cannot be loaded, as it appears to be corrupt or incomplete.\n";
	Host::AddOSDMessage("Error: The savestate cannot be loaded, as it appears to be corrupt or incomplete.", 15.0f);
	_formatUserMsg(retval);
	return retval;
}

// Used to hold the current state backup (fullcopy of PS2 memory and subcomponents states).
//static VmStateBuffer state_buffer( L"Public Savestate Buffer" );

static const char* EntryFilename_StateVersion = "PCSX2 Savestate Version.id";
static const char* EntryFilename_Screenshot = "Screenshot.png";
static const char* EntryFilename_InternalStructures = "PCSX2 Internal Structures.dat";

struct SysState_Component
{
	const char* name;
	int (*freeze)(FreezeAction, freezeData*);
};

static int SysState_MTGSFreeze(FreezeAction mode, freezeData* fP)
{
#ifndef PCSX2_CORE
	ScopedCoreThreadPause paused_core;
#endif
	MTGS_FreezeData sstate = { fP, 0 };
	GetMTGS().Freeze(mode, sstate);
#ifndef PCSX2_CORE
	paused_core.AllowResume();
#endif
	return sstate.retval;
}

static constexpr SysState_Component SPU2{ "SPU2", SPU2freeze };
static constexpr SysState_Component PAD_{ "PAD", PADfreeze };
static constexpr SysState_Component USB{ "USB", USBfreeze };
static constexpr SysState_Component GS{ "GS", SysState_MTGSFreeze };


static void SysState_ComponentFreezeOutRoot(void* dest, SysState_Component comp)
{
	freezeData fP = { 0, (u8*)dest };
	if (comp.freeze(FreezeAction::Size, &fP) != 0)
		return;
	if (!fP.size)
		return;

	Console.Indent().WriteLn("Saving %s", comp.name);

	if (comp.freeze(FreezeAction::Save, &fP) != 0)
		throw std::runtime_error(std::string(" * ") + comp.name + std::string(": Error saving state!\n"));
}

static void SysState_ComponentFreezeIn(zip_file_t* zf, SysState_Component comp)
{
	freezeData fP = { 0, nullptr };
	if (comp.freeze(FreezeAction::Size, &fP) != 0)
		fP.size = 0;

	Console.Indent().WriteLn("Loading %s", comp.name);

	auto data = std::make_unique<u8[]>(fP.size);
	fP.data = data.get();

	if (zip_fread(zf, data.get(), fP.size) != static_cast<zip_int64_t>(fP.size) || comp.freeze(FreezeAction::Load, &fP) != 0)
		throw std::runtime_error(std::string(" * ") + comp.name + std::string(": Error loading state!\n"));
}

static void SysState_ComponentFreezeOut(SaveStateBase& writer, SysState_Component comp)
{
	freezeData fP = { 0, NULL };
	if (comp.freeze(FreezeAction::Size, &fP) == 0)
	{
		const int size = fP.size;
		writer.PrepBlock(size);
		SysState_ComponentFreezeOutRoot(writer.GetBlockPtr(), comp);
		writer.CommitBlock(size);
	}
	return;
}

// --------------------------------------------------------------------------------------
//  BaseSavestateEntry
// --------------------------------------------------------------------------------------
class BaseSavestateEntry
{
protected:
	BaseSavestateEntry() = default;

public:
	virtual ~BaseSavestateEntry() = default;

	virtual const char* GetFilename() const = 0;
	virtual void FreezeIn(zip_file_t* zf) const = 0;
	virtual void FreezeOut(SaveStateBase& writer) const = 0;
	virtual bool IsRequired() const = 0;
};

class MemorySavestateEntry : public BaseSavestateEntry
{
protected:
	MemorySavestateEntry() {}
	virtual ~MemorySavestateEntry() = default;

public:
	virtual void FreezeIn(zip_file_t* zf) const;
	virtual void FreezeOut(SaveStateBase& writer) const;
	virtual bool IsRequired() const { return true; }

protected:
	virtual u8* GetDataPtr() const = 0;
	virtual u32 GetDataSize() const = 0;
};

void MemorySavestateEntry::FreezeIn(zip_file_t* zf) const
{
	const u32 expectedSize = GetDataSize();
	const s64 bytesRead = zip_fread(zf, GetDataPtr(), expectedSize);
	if (bytesRead != static_cast<s64>(expectedSize))
	{
		Console.WriteLn(Color_Yellow, " '%s' is incomplete (expected 0x%x bytes, loading only 0x%x bytes)",
			GetFilename(), expectedSize, static_cast<u32>(bytesRead));
	}
}

void MemorySavestateEntry::FreezeOut(SaveStateBase& writer) const
{
	writer.FreezeMem(GetDataPtr(), GetDataSize());
}

// --------------------------------------------------------------------------------------
//  SavestateEntry_* (EmotionMemory, IopMemory, etc)
// --------------------------------------------------------------------------------------
// Implementation Rationale:
//  The address locations of PS2 virtual memory components is fully dynamic, so we need to
//  resolve the pointers at the time they are requested (eeMem, iopMem, etc).  Thusly, we
//  cannot use static struct member initializers -- we need virtual functions that compute
//  and resolve the addresses on-demand instead... --air

class SavestateEntry_EmotionMemory : public MemorySavestateEntry
{
public:
	virtual ~SavestateEntry_EmotionMemory() = default;

	const char* GetFilename() const { return "eeMemory.bin"; }
	u8* GetDataPtr() const { return eeMem->Main; }
	uint GetDataSize() const { return sizeof(eeMem->Main); }

	virtual void FreezeIn(zip_file_t* zf) const
	{
		SysClearExecutionCache();
		MemorySavestateEntry::FreezeIn(zf);
	}
};

class SavestateEntry_IopMemory : public MemorySavestateEntry
{
public:
	virtual ~SavestateEntry_IopMemory() = default;

	const char* GetFilename() const { return "iopMemory.bin"; }
	u8* GetDataPtr() const { return iopMem->Main; }
	uint GetDataSize() const { return sizeof(iopMem->Main); }
};

class SavestateEntry_HwRegs : public MemorySavestateEntry
{
public:
	virtual ~SavestateEntry_HwRegs() = default;

	const char* GetFilename() const { return "eeHwRegs.bin"; }
	u8* GetDataPtr() const { return eeHw; }
	uint GetDataSize() const { return sizeof(eeHw); }
};

class SavestateEntry_IopHwRegs : public MemorySavestateEntry
{
public:
	virtual ~SavestateEntry_IopHwRegs() = default;

	const char* GetFilename() const { return "iopHwRegs.bin"; }
	u8* GetDataPtr() const { return iopHw; }
	uint GetDataSize() const { return sizeof(iopHw); }
};

class SavestateEntry_Scratchpad : public MemorySavestateEntry
{
public:
	virtual ~SavestateEntry_Scratchpad() = default;

	const char* GetFilename() const { return "Scratchpad.bin"; }
	u8* GetDataPtr() const { return eeMem->Scratch; }
	uint GetDataSize() const { return sizeof(eeMem->Scratch); }
};

class SavestateEntry_VU0mem : public MemorySavestateEntry
{
public:
	virtual ~SavestateEntry_VU0mem() = default;

	const char* GetFilename() const { return "vu0Memory.bin"; }
	u8* GetDataPtr() const { return vuRegs[0].Mem; }
	uint GetDataSize() const { return VU0_MEMSIZE; }
};

class SavestateEntry_VU1mem : public MemorySavestateEntry
{
public:
	virtual ~SavestateEntry_VU1mem() = default;

	const char* GetFilename() const { return "vu1Memory.bin"; }
	u8* GetDataPtr() const { return vuRegs[1].Mem; }
	uint GetDataSize() const { return VU1_MEMSIZE; }
};

class SavestateEntry_VU0prog : public MemorySavestateEntry
{
public:
	virtual ~SavestateEntry_VU0prog() = default;

	const char* GetFilename() const { return "vu0MicroMem.bin"; }
	u8* GetDataPtr() const { return vuRegs[0].Micro; }
	uint GetDataSize() const { return VU0_PROGSIZE; }
};

class SavestateEntry_VU1prog : public MemorySavestateEntry
{
public:
	virtual ~SavestateEntry_VU1prog() = default;

	const char* GetFilename() const { return "vu1MicroMem.bin"; }
	u8* GetDataPtr() const { return vuRegs[1].Micro; }
	uint GetDataSize() const { return VU1_PROGSIZE; }
};

class SavestateEntry_SPU2 : public BaseSavestateEntry
{
public:
	virtual ~SavestateEntry_SPU2() = default;

	const char* GetFilename() const { return "SPU2.bin"; }
	void FreezeIn(zip_file_t* zf) const { return SysState_ComponentFreezeIn(zf, SPU2); }
	void FreezeOut(SaveStateBase& writer) const { return SysState_ComponentFreezeOut(writer, SPU2); }
	bool IsRequired() const { return true; }
};

class SavestateEntry_USB : public BaseSavestateEntry
{
public:
	virtual ~SavestateEntry_USB() = default;

	const char* GetFilename() const { return "USB.bin"; }
	void FreezeIn(zip_file_t* zf) const { return SysState_ComponentFreezeIn(zf, USB); }
	void FreezeOut(SaveStateBase& writer) const { return SysState_ComponentFreezeOut(writer, USB); }
	bool IsRequired() const { return false; }
};

class SavestateEntry_PAD : public BaseSavestateEntry
{
public:
	virtual ~SavestateEntry_PAD() = default;

	const char* GetFilename() const { return "PAD.bin"; }
	void FreezeIn(zip_file_t* zf) const { return SysState_ComponentFreezeIn(zf, PAD_); }
	void FreezeOut(SaveStateBase& writer) const { return SysState_ComponentFreezeOut(writer, PAD_); }
	bool IsRequired() const { return true; }
};

class SavestateEntry_GS : public BaseSavestateEntry
{
public:
	virtual ~SavestateEntry_GS() = default;

	const char* GetFilename() const { return "GS.bin"; }
	void FreezeIn(zip_file_t* zf) const { return SysState_ComponentFreezeIn(zf, GS); }
	void FreezeOut(SaveStateBase& writer) const { return SysState_ComponentFreezeOut(writer, GS); }
	bool IsRequired() const { return true; }
};



// (cpuRegs, iopRegs, VPU/GIF/DMAC structures should all remain as part of a larger unified
//  block, since they're all PCSX2-dependent and having separate files in the archie for them
//  would not be useful).
//

static const std::unique_ptr<BaseSavestateEntry> SavestateEntries[] = {
	std::unique_ptr<BaseSavestateEntry>(new SavestateEntry_EmotionMemory),
	std::unique_ptr<BaseSavestateEntry>(new SavestateEntry_IopMemory),
	std::unique_ptr<BaseSavestateEntry>(new SavestateEntry_HwRegs),
	std::unique_ptr<BaseSavestateEntry>(new SavestateEntry_IopHwRegs),
	std::unique_ptr<BaseSavestateEntry>(new SavestateEntry_Scratchpad),
	std::unique_ptr<BaseSavestateEntry>(new SavestateEntry_VU0mem),
	std::unique_ptr<BaseSavestateEntry>(new SavestateEntry_VU1mem),
	std::unique_ptr<BaseSavestateEntry>(new SavestateEntry_VU0prog),
	std::unique_ptr<BaseSavestateEntry>(new SavestateEntry_VU1prog),
	std::unique_ptr<BaseSavestateEntry>(new SavestateEntry_SPU2),
#ifndef PCSX2_CORE
	std::unique_ptr<BaseSavestateEntry>(new SavestateEntry_USB),
#endif
	std::unique_ptr<BaseSavestateEntry>(new SavestateEntry_PAD),
	std::unique_ptr<BaseSavestateEntry>(new SavestateEntry_GS),
};

std::unique_ptr<ArchiveEntryList> SaveState_DownloadState()
{
#ifndef PCSX2_CORE
	if (!GetCoreThread().HasActiveMachine())
		throw Exception::RuntimeError()
			.SetDiagMsg("SysExecEvent_DownloadState: Cannot freeze/download an invalid VM state!")
			.SetUserMsg("There is no active virtual machine state to download or save.");
#endif

	std::unique_ptr<ArchiveEntryList> destlist = std::make_unique<ArchiveEntryList>(new VmStateBuffer("Zippable Savestate"));

	memSavingState saveme(destlist->GetBuffer());
	ArchiveEntry internals(EntryFilename_InternalStructures);
	internals.SetDataIndex(saveme.GetCurrentPos());

	saveme.FreezeBios();
	saveme.FreezeInternals();

	internals.SetDataSize(saveme.GetCurrentPos() - internals.GetDataIndex());
	destlist->Add(internals);

	for (const std::unique_ptr<BaseSavestateEntry>& entry : SavestateEntries)
	{
		uint startpos = saveme.GetCurrentPos();
		entry->FreezeOut(saveme);
		destlist->Add(
			ArchiveEntry(entry->GetFilename())
				.SetDataIndex(startpos)
				.SetDataSize(saveme.GetCurrentPos() - startpos));
	}

	return destlist;
}

std::unique_ptr<SaveStateScreenshotData> SaveState_SaveScreenshot()
{
	static constexpr u32 SCREENSHOT_WIDTH = 640;
	static constexpr u32 SCREENSHOT_HEIGHT = 480;

	std::vector<u32> pixels(SCREENSHOT_WIDTH * SCREENSHOT_HEIGHT);
	if (!GetMTGS().SaveMemorySnapshot(SCREENSHOT_WIDTH, SCREENSHOT_HEIGHT, &pixels))
	{
		// saving failed for some reason, device lost?
		return nullptr;
	}

	std::unique_ptr<SaveStateScreenshotData> data = std::make_unique<SaveStateScreenshotData>();
	data->width = SCREENSHOT_WIDTH;
	data->height = SCREENSHOT_HEIGHT;
	data->pixels = std::move(pixels);
	return data;
}

static bool SaveState_CompressScreenshot(SaveStateScreenshotData* data, zip_t* zf)
{
	zip_error_t ze = {};
	zip_source_t* const zs = zip_source_buffer_create(nullptr, 0, 0, &ze);
	if (!zs)
		return false;

	if (zip_source_begin_write(zs) != 0)
	{
		zip_source_free(zs);
		return false;
	}

	ScopedGuard zs_free([zs]() { zip_source_free(zs); });

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	png_infop info_ptr = nullptr;
	if (!png_ptr)
		return false;

	ScopedGuard cleanup([&png_ptr, &info_ptr]() {
		if (png_ptr)
			png_destroy_write_struct(&png_ptr, info_ptr ? &info_ptr : nullptr);
	});

	info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
		return false;

	if (setjmp(png_jmpbuf(png_ptr)))
		return false;

	png_set_write_fn(png_ptr, zs, [](png_structp png_ptr, png_bytep data_ptr, png_size_t size) {
		zip_source_write(static_cast<zip_source_t*>(png_get_io_ptr(png_ptr)), data_ptr, size);
	}, [](png_structp png_ptr) {});
	png_set_compression_level(png_ptr, 5);
	png_set_IHDR(png_ptr, info_ptr, data->width, data->height, 8, PNG_COLOR_TYPE_RGBA,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
	png_write_info(png_ptr, info_ptr);

	for (u32 y = 0; y < data->height; ++y)
	{
		// ensure the alpha channel is set to opaque
		u32* row = &data->pixels[y * data->width];
		for (u32 x = 0; x < data->width; x++)
			row[x] |= 0xFF000000u;

		png_write_row(png_ptr, reinterpret_cast<png_bytep>(row));
	}

	png_write_end(png_ptr, nullptr);

	if (zip_source_commit_write(zs) != 0)
		return false;

	const s64 file_index = zip_file_add(zf, EntryFilename_Screenshot, zs, 0);
	if (file_index < 0)
		return false;

	// png is already compressed, no point doing it twice
	zip_set_file_compression(zf, file_index, ZIP_CM_STORE, 0);

	// source is now owned by the zip file for later compression
	zs_free.Cancel();
	return true;
}

static bool SaveState_ReadScreenshot(zip_t* zf, u32* out_width, u32* out_height, std::vector<u32>* out_pixels)
{
	auto zff = zip_fopen_managed(zf, EntryFilename_Screenshot, 0);
	if (!zff)
		return false;

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
	if (!png_ptr)
		return false;

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, nullptr, nullptr);
		return false;
	}

	ScopedGuard cleanup([&png_ptr, &info_ptr]() {
		png_destroy_read_struct(&png_ptr, &info_ptr, nullptr);
	});

	if (setjmp(png_jmpbuf(png_ptr)))
		return false;

	png_set_read_fn(png_ptr, zff.get(), [](png_structp png_ptr, png_bytep data_ptr, png_size_t size) {
		zip_fread(static_cast<zip_file_t*>(png_get_io_ptr(png_ptr)), data_ptr, size);
	});

	png_read_info(png_ptr, info_ptr);

	png_uint_32 width = 0;
	png_uint_32 height = 0;
	int bitDepth = 0;
	int colorType = -1;
	if (png_get_IHDR(png_ptr, info_ptr, &width, &height, &bitDepth, &colorType, nullptr, nullptr, nullptr) != 1 ||
		width == 0 || height == 0)
	{
		return false;
	}

	const png_uint_32 bytesPerRow = png_get_rowbytes(png_ptr, info_ptr);
	std::vector<u8> rowData(bytesPerRow);

	*out_width = width;
	*out_height = height;
	out_pixels->resize(width * height);

	for (u32 y = 0; y < height; y++)
	{
		png_read_row(png_ptr, static_cast<png_bytep>(rowData.data()), nullptr);

		const u8* row_ptr = rowData.data();
		u32* out_ptr = &out_pixels->at(y * width);
		if (colorType == PNG_COLOR_TYPE_RGB)
		{
			for (u32 x = 0; x < width; x++)
			{
				u32 pixel = static_cast<u32>(*(row_ptr)++);
				pixel |= static_cast<u32>(*(row_ptr)++) << 8;
				pixel |= static_cast<u32>(*(row_ptr)++) << 16;
				pixel |= static_cast<u32>(*(row_ptr)++) << 24;
				*(out_ptr++) = pixel | 0xFF000000u; // make opaque
			}
		}
		else if (colorType == PNG_COLOR_TYPE_RGBA)
		{
			for (u32 x = 0; x < width; x++)
			{
				u32 pixel;
				std::memcpy(&pixel, row_ptr, sizeof(u32));
				row_ptr += sizeof(u32);
				*(out_ptr++) = pixel | 0xFF000000u; // make opaque
			}
		}
	}

	return true;
}

// --------------------------------------------------------------------------------------
//  CompressThread_VmState
// --------------------------------------------------------------------------------------
static bool SaveState_AddToZip(zip_t* zf, ArchiveEntryList* srclist, SaveStateScreenshotData* screenshot)
{
	// use zstd compression, it can be 10x+ faster for saving.
	const u32 compression = EmuConfig.SavestateZstdCompression ? ZIP_CM_ZSTD : ZIP_CM_DEFLATE;
	const u32 compression_level = 0;

	// version indicator
	{
		zip_source_t* const zs = zip_source_buffer(zf, &g_SaveVersion, sizeof(g_SaveVersion), 0);
		if (!zs)
			return false;

		// NOTE: Source should not be freed if successful.
		const s64 fi = zip_file_add(zf, EntryFilename_StateVersion, zs, ZIP_FL_ENC_UTF_8);
		if (fi < 0)
		{
			zip_source_free(zs);
			return false;
		}

		zip_set_file_compression(zf, fi, ZIP_CM_STORE, 0);
	}

	const uint listlen = srclist->GetLength();
	for (uint i = 0; i < listlen; ++i)
	{
		const ArchiveEntry& entry = (*srclist)[i];
		if (!entry.GetDataSize())
			continue;

		zip_source_t* const zs = zip_source_buffer(zf, srclist->GetPtr(entry.GetDataIndex()), entry.GetDataSize(), 0);
		if (!zs)
			return false;

		const s64 fi = zip_file_add(zf, entry.GetFilename().c_str(), zs, ZIP_FL_ENC_UTF_8);
		if (fi < 0)
		{
			zip_source_free(zs);
			return false;
		}

		zip_set_file_compression(zf, fi, compression, compression_level);
	}

	if (screenshot)
	{
		if (!SaveState_CompressScreenshot(screenshot, zf))
			return false;
	}

	return true;
}

bool SaveState_ZipToDisk(std::unique_ptr<ArchiveEntryList> srclist, std::unique_ptr<SaveStateScreenshotData> screenshot, const char* filename)
{
	zip_error_t ze = {};
	zip_source_t* zs = zip_source_file_create(filename, 0, 0, &ze);
	zip_t* zf = nullptr;
	if (zs && !(zf = zip_open_from_source(zs, ZIP_CREATE | ZIP_TRUNCATE, &ze)))
	{
		Console.Error("Failed to open zip file '%s' for save state: %s", filename, zip_error_strerror(&ze));

		// have to clean up source
		zip_source_free(zs);
		return false;
	}

	// discard zip file if we fail saving something
	if (!SaveState_AddToZip(zf, srclist.get(), screenshot.get()))
	{
		Console.Error("Failed to save state to zip file '%s'", filename);
		zip_discard(zf);
		return false;
	}

	// force the zip to close, this is the expensive part with libzip.
	zip_close(zf);
	return true;
}

bool SaveState_ReadScreenshot(const std::string& filename, u32* out_width, u32* out_height, std::vector<u32>* out_pixels)
{
	zip_error_t ze = {};
	auto zf = zip_open_managed(filename.c_str(), ZIP_RDONLY, &ze);
	if (!zf)
	{
		Console.Error("Failed to open zip file '%s' for save state screenshot: %s", filename.c_str(), zip_error_strerror(&ze));
		return false;
	}

	return SaveState_ReadScreenshot(zf.get(), out_width, out_height, out_pixels);
}

static void CheckVersion(const std::string& filename, zip_t* zf)
{
	u32 savever;

	auto zff = zip_fopen_managed(zf, EntryFilename_StateVersion, 0);
	if (!zff || zip_fread(zff.get(), &savever, sizeof(savever)) != sizeof(savever))
	{
		throw Exception::SaveStateLoadError(filename)
			.SetDiagMsg("Savestate file does not contain version indicator.")
			.SetUserMsg("This file is not a valid PCSX2 savestate.  See the logfile for details.");
	}

	// Major version mismatch.  Means we can't load this savestate at all.  Support for it
	// was removed entirely.
	if (savever > g_SaveVersion)
		throw Exception::SaveStateLoadError(filename)
			.SetDiagMsg(fmt::format("Savestate uses an unsupported or unknown savestate version.\n(PCSX2 ver={:x}, state ver={:x})", g_SaveVersion, savever))
			.SetUserMsg("Cannot load this savestate. The state is an unsupported version.");

	// check for a "minor" version incompatibility; which happens if the savestate being loaded is a newer version
	// than the emulator recognizes.  99% chance that trying to load it will just corrupt emulation or crash.
	if ((savever >> 16) != (g_SaveVersion >> 16))
		throw Exception::SaveStateLoadError(filename)
			.SetDiagMsg(fmt::format("Savestate uses an unknown savestate version.\n(PCSX2 ver={:x}, state ver={:x})", g_SaveVersion, savever))
			.SetUserMsg("Cannot load this savestate. The state is an unsupported version.");
}

static zip_int64_t CheckFileExistsInState(zip_t* zf, const char* name)
{
	zip_int64_t index = zip_name_locate(zf, name, /*ZIP_FL_NOCASE*/ 0);
	if (index >= 0)
	{
		DevCon.WriteLn(Color_Green, " ... found '%s'", name);
		return index;
	}

	Console.WriteLn(Color_Red, " ... not found '%s'!", name);
	return index;
}

static bool LoadInternalStructuresState(zip_t* zf, s64 index)
{
	zip_stat_t zst;
	if (zip_stat_index(zf, index, 0, &zst) != 0 || zst.size > std::numeric_limits<int>::max())
		return false;

	// Load all the internal data
	auto zff = zip_fopen_index_managed(zf, index, 0);
	if (!zff)
		return false;

	VmStateBuffer buffer(static_cast<int>(zst.size), "StateBuffer_UnzipFromDisk"); // start with an 8 meg buffer to avoid frequent reallocation.
	if (zip_fread(zff.get(), buffer.GetPtr(), buffer.GetSizeInBytes()) != buffer.GetSizeInBytes())
		return false;

	memLoadingState(buffer).FreezeBios().FreezeInternals();
	return true;
}

void SaveState_UnzipFromDisk(const std::string& filename)
{
	zip_error_t ze = {};
	auto zf = zip_open_managed(filename.c_str(), ZIP_RDONLY, &ze);
	if (!zf)
	{
		Console.Error("Failed to open zip file '%s' for save state load: %s", filename.c_str(), zip_error_strerror(&ze));
		throw Exception::SaveStateLoadError(filename)
			.SetDiagMsg("Savestate file is not a valid gzip archive.")
			.SetUserMsg("This savestate cannot be loaded because it is not a valid gzip archive.  It may have been created by an older unsupported version of PCSX2, or it may be corrupted.");
	}

	// look for version and screenshot information in the zip stream:
	CheckVersion(filename, zf.get());

	// check that all parts are included
	const s64 internal_index = CheckFileExistsInState(zf.get(), EntryFilename_InternalStructures);
	s64 entryIndices[std::size(SavestateEntries)];

	// Log any parts and pieces that are missing, and then generate an exception.
	bool throwIt = (internal_index < 0);
	for (u32 i = 0; i < std::size(SavestateEntries); i++)
	{
		entryIndices[i] = CheckFileExistsInState(zf.get(), SavestateEntries[i]->GetFilename());
		if (entryIndices[i] < 0 && SavestateEntries[i]->IsRequired())
			throwIt = true;
	}

	if (!throwIt)
	{
		PreLoadPrep();
		throwIt = !LoadInternalStructuresState(zf.get(), internal_index);
	}

	if (!throwIt)
	{
		for (u32 i = 0; i < std::size(SavestateEntries); ++i)
		{
			if (entryIndices[i] < 0)
				continue;

			auto zff = zip_fopen_index_managed(zf.get(), entryIndices[i], 0);
			if (!zff)
			{
				throwIt = true;
				break;
			}

			SavestateEntries[i]->FreezeIn(zff.get());
		}
	}

	if (throwIt)
	{
		throw Exception::SaveStateLoadError(filename)
			.SetDiagMsg("Savestate cannot be loaded: some required components were not found or are incomplete.")
			.SetUserMsg("This savestate cannot be loaded due to missing critical components.  See the log file for details.");
	}

	PostLoadPrep();
}
