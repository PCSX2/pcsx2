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
#include "IopCommon.h"
#include "SaveState.h"

#include "ps2/BiosTools.h"
#include "COP0.h"
#include "VUmicro.h"
#include "MTVU.h"
#include "Cache.h"
#include "Config.h"

#include "Elfheader.h"
#include "Counters.h"
#include "Patch.h"
#include "System/SysThreads.h"
#include "DebugTools/Breakpoints.h"

#include "common/pxStreams.h"
#include "common/SafeArray.inl"
#include "common/StringUtil.h"
#include "SPU2/spu2.h"
#include "USB/USB.h"
#include "PAD/Gamepad.h"

#include <wx/wfstream.h>

#include "gui/ConsoleLogger.h"

#ifndef PCSX2_CORE
#include "gui/App.h"
#endif

#include "common/pxStreams.h"
#include <wx/wfstream.h>
#include <wx/zipstrm.h>

using namespace R5900;


static void PreLoadPrep()
{
	SysClearExecutionCache();
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
wxString SaveStateBase::GetSavestateFolder( int slot, bool isSavingOrLoading )
{
	wxString CRCvalue = wxString::Format(wxT("%08X"), ElfCRC);
	wxString serialName(DiscSerial);

	if (g_GameStarted || g_GameLoading)
	{
		if (DiscSerial.IsEmpty())
		{
			std::string ElfString = LastELF.ToStdString();
			std::string ElfString_delimiter = "/";

#ifndef _UNIX_
			std::replace(ElfString.begin(), ElfString.end(), '\\', '/');
#endif
			size_t pos = 0;
			while ((pos = ElfString.find(ElfString_delimiter)) != std::string::npos)
			{
				// Running homebrew/standalone ELF, return only the ELF name.
				ElfString.erase(0, pos + ElfString_delimiter.length());
			}
			wxString ElfString_toWxString(ElfString.c_str(), wxConvUTF8);
			serialName = ElfString_toWxString;
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
		wxString biosString = (pxsFmt(L"BIOS (%s v%u.%u)", WX_STR(biosZone), (BiosVersion >> 8), BiosVersion & 0xff));
		serialName = biosString;
		CRCvalue = L"None";
	}

	wxFileName dirname = wxFileName::DirName(g_Conf->FullpathToSaveState(serialName, CRCvalue));

	if (isSavingOrLoading)
	{
		if (!wxDirExists(g_Conf->FullpathToSaveState(serialName, CRCvalue)))
		{
			wxMkdir(g_Conf->FullpathToSaveState(serialName, CRCvalue));
		}
	}
	return (dirname.GetPath() + "/" +
			pxsFmt( L"%s (%s).%02d.p2s", WX_STR(serialName), WX_STR(CRCvalue), slot ));
}

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
	pxAssertDev( strlen(src) < allowedlen, pxsFmt( L"Tag name exceeds the allowed length of %d chars.", allowedlen) );

	memzero( m_tagspace );
	strcpy( m_tagspace, src );
	Freeze( m_tagspace );

	if( strcmp( m_tagspace, src ) != 0 )
	{
		wxString msg( L"Savestate data corruption detected while reading tag: " + fromUTF8(src) );
		pxFail( msg );
		throw Exception::SaveStateLoadError().SetDiagMsg(msg);
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

	pxToUTF8 utf8(BiosDescription);

	memzero( biosdesc );
	memcpy( biosdesc, utf8, std::min( sizeof(biosdesc), utf8.Length() ) );
	
	Freeze( bioscheck );
	Freeze( biosdesc );

	if (bioscheck != BiosChecksum)
	{
		Console.Newline();
		Console.Indent(1).Error( "Warning: BIOS Version Mismatch, savestate may be unstable!" );
		Console.Indent(2).Error(
			"Current BIOS:   %ls (crc=0x%08x)\n"
			"Savestate BIOS: %s (crc=0x%08x)\n",
			BiosDescription.wx_str(), BiosChecksum,
			biosdesc, bioscheck
		);
	}
	
	return *this;
}

static const uint MainMemorySizeInBytes =
	Ps2MemSize::MainRam	+ Ps2MemSize::Scratch		+ Ps2MemSize::Hardware +
	Ps2MemSize::IopRam	+ Ps2MemSize::IopHardware;

SaveStateBase& SaveStateBase::FreezeMainMemory()
{
	vu1Thread.WaitVU(); // Finish VU1 just in-case...
	if (IsLoading()) PreLoadPrep();
	else m_memory->MakeRoomFor( m_idx + MainMemorySizeInBytes );

	// First Block - Memory Dumps
	// ---------------------------
	FreezeMem(eeMem->Main,		Ps2MemSize::MainRam);		// 32 MB main memory
	FreezeMem(eeMem->Scratch,	Ps2MemSize::Scratch);		// scratch pad
	FreezeMem(eeHw,				Ps2MemSize::Hardware);		// hardware memory

	FreezeMem(iopMem->Main, 	Ps2MemSize::IopRam);		// 2 MB main memory
	FreezeMem(iopHw,			Ps2MemSize::IopHardware);	// hardware memory
	
	FreezeMem(vuRegs[0].Micro,	VU0_PROGSIZE);
	FreezeMem(vuRegs[0].Mem,	VU0_MEMSIZE);

	FreezeMem(vuRegs[1].Micro,	VU1_PROGSIZE);
	FreezeMem(vuRegs[1].Mem,	VU1_MEMSIZE);
	
	return *this;
}

SaveStateBase& SaveStateBase::FreezeInternals()
{
	vu1Thread.WaitVU(); // Finish VU1 just in-case...
	// Print this until the MTVU problem in gifPathFreeze is taken care of (rama)
	if (THREAD_VU1) Console.Warning("MTVU speedhack is enabled, saved states may not be stable");
	
	if (IsLoading()) PreLoadPrep();

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
	StringUtil::Strlcpy(localDiscSerial, DiscSerial.ToUTF8(), sizeof(localDiscSerial));
	Freeze(localDiscSerial);
	if (IsLoading())
		DiscSerial = wxString::FromUTF8(localDiscSerial);

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

	if( IsLoading() )
		PostLoadPrep();

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

wxString Exception::SaveStateLoadError::FormatDiagnosticMessage() const
{
	FastFormatUnicode retval;
	retval.Write("Savestate is corrupt or incomplete!\n");
	OSDlog(Color_Red, false, "Error: Savestate is corrupt or incomplete!");
	_formatDiagMsg(retval);
	return retval;
}

wxString Exception::SaveStateLoadError::FormatDisplayMessage() const
{
	FastFormatUnicode retval;
	retval.Write(_("The savestate cannot be loaded, as it appears to be corrupt or incomplete."));
	retval.Write("\n");
	OSDlog(Color_Red, false, "Error: The savestate cannot be loaded, as it appears to be corrupt or incomplete.");
	_formatUserMsg(retval);
	return retval;
}

// Used to hold the current state backup (fullcopy of PS2 memory and subcomponents states).
//static VmStateBuffer state_buffer( L"Public Savestate Buffer" );

static const wxChar* EntryFilename_StateVersion = L"PCSX2 Savestate Version.id";
//static const wxChar* EntryFilename_Screenshot = L"Screenshot.jpg";
static const wxChar* EntryFilename_InternalStructures = L"PCSX2 Internal Structures.dat";

struct SysState_Component
{
	const char* name;
	int (*freeze)(FreezeAction, freezeData*);
};

int SysState_MTGSFreeze(FreezeAction mode, freezeData* fP)
{
	ScopedCoreThreadPause paused_core;
	MTGS_FreezeData sstate = { fP, 0 };
	GetMTGS().Freeze(mode, sstate);
	paused_core.AllowResume();
	return sstate.retval;
}

static constexpr SysState_Component SPU2{ "SPU2", SPU2freeze };
static constexpr SysState_Component PAD{ "PAD", PADfreeze };
static constexpr SysState_Component USB{ "USB", USBfreeze };
static constexpr SysState_Component GS{ "GS", SysState_MTGSFreeze };


void SysState_ComponentFreezeOutRoot(void* dest, SysState_Component comp)
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

void SysState_ComponentFreezeIn(pxInputStream& infp, SysState_Component comp)
{
	freezeData fP = { 0, nullptr };
	if (comp.freeze(FreezeAction::Size, &fP) != 0)
		fP.size = 0;

	Console.Indent().WriteLn("Loading %s", comp.name);

	if (!infp.IsOk() || !infp.Length())
	{
		// no state data to read, but component expects some state data?
		// Issue a warning to console...
		if (fP.size != 0)
			Console.Indent().Warning("Warning: No data for %s found. Status may be unpredictable.", comp.name);

		return;
	}

	auto data = std::make_unique<u8[]>(fP.size);
	fP.data = data.get();

	infp.Read(fP.data, fP.size);
	if (comp.freeze(FreezeAction::Load, &fP) != 0)
		throw std::runtime_error(std::string(" * ") + comp.name + std::string(": Error loading state!\n"));
}

void SysState_ComponentFreezeOut(SaveStateBase& writer, SysState_Component comp)
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

	virtual wxString GetFilename() const = 0;
	virtual void FreezeIn(pxInputStream& reader) const = 0;
	virtual void FreezeOut(SaveStateBase& writer) const = 0;
	virtual bool IsRequired() const = 0;
};

class MemorySavestateEntry : public BaseSavestateEntry
{
protected:
	MemorySavestateEntry() {}
	virtual ~MemorySavestateEntry() = default;

public:
	virtual void FreezeIn(pxInputStream& reader) const;
	virtual void FreezeOut(SaveStateBase& writer) const;
	virtual bool IsRequired() const { return true; }

protected:
	virtual u8* GetDataPtr() const = 0;
	virtual uint GetDataSize() const = 0;
};

void MemorySavestateEntry::FreezeIn(pxInputStream& reader) const
{
	const uint entrySize = reader.Length();
	const uint expectedSize = GetDataSize();

	if (entrySize < expectedSize)
	{
		Console.WriteLn(Color_Yellow, " '%s' is incomplete (expected 0x%x bytes, loading only 0x%x bytes)",
			WX_STR(GetFilename()), expectedSize, entrySize);
	}

	uint copylen = std::min(entrySize, expectedSize);
	reader.Read(GetDataPtr(), copylen);
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

	wxString GetFilename() const { return L"eeMemory.bin"; }
	u8* GetDataPtr() const { return eeMem->Main; }
	uint GetDataSize() const { return sizeof(eeMem->Main); }

	virtual void FreezeIn(pxInputStream& reader) const
	{
		SysClearExecutionCache();
		MemorySavestateEntry::FreezeIn(reader);
	}
};

class SavestateEntry_IopMemory : public MemorySavestateEntry
{
public:
	virtual ~SavestateEntry_IopMemory() = default;

	wxString GetFilename() const { return L"iopMemory.bin"; }
	u8* GetDataPtr() const { return iopMem->Main; }
	uint GetDataSize() const { return sizeof(iopMem->Main); }
};

class SavestateEntry_HwRegs : public MemorySavestateEntry
{
public:
	virtual ~SavestateEntry_HwRegs() = default;

	wxString GetFilename() const { return L"eeHwRegs.bin"; }
	u8* GetDataPtr() const { return eeHw; }
	uint GetDataSize() const { return sizeof(eeHw); }
};

class SavestateEntry_IopHwRegs : public MemorySavestateEntry
{
public:
	virtual ~SavestateEntry_IopHwRegs() = default;

	wxString GetFilename() const { return L"iopHwRegs.bin"; }
	u8* GetDataPtr() const { return iopHw; }
	uint GetDataSize() const { return sizeof(iopHw); }
};

class SavestateEntry_Scratchpad : public MemorySavestateEntry
{
public:
	virtual ~SavestateEntry_Scratchpad() = default;

	wxString GetFilename() const { return L"Scratchpad.bin"; }
	u8* GetDataPtr() const { return eeMem->Scratch; }
	uint GetDataSize() const { return sizeof(eeMem->Scratch); }
};

class SavestateEntry_VU0mem : public MemorySavestateEntry
{
public:
	virtual ~SavestateEntry_VU0mem() = default;

	wxString GetFilename() const { return L"vu0Memory.bin"; }
	u8* GetDataPtr() const { return vuRegs[0].Mem; }
	uint GetDataSize() const { return VU0_MEMSIZE; }
};

class SavestateEntry_VU1mem : public MemorySavestateEntry
{
public:
	virtual ~SavestateEntry_VU1mem() = default;

	wxString GetFilename() const { return L"vu1Memory.bin"; }
	u8* GetDataPtr() const { return vuRegs[1].Mem; }
	uint GetDataSize() const { return VU1_MEMSIZE; }
};

class SavestateEntry_VU0prog : public MemorySavestateEntry
{
public:
	virtual ~SavestateEntry_VU0prog() = default;

	wxString GetFilename() const { return L"vu0MicroMem.bin"; }
	u8* GetDataPtr() const { return vuRegs[0].Micro; }
	uint GetDataSize() const { return VU0_PROGSIZE; }
};

class SavestateEntry_VU1prog : public MemorySavestateEntry
{
public:
	virtual ~SavestateEntry_VU1prog() = default;

	wxString GetFilename() const { return L"vu1MicroMem.bin"; }
	u8* GetDataPtr() const { return vuRegs[1].Micro; }
	uint GetDataSize() const { return VU1_PROGSIZE; }
};

class SavestateEntry_SPU2 : public BaseSavestateEntry
{
public:
	virtual ~SavestateEntry_SPU2() = default;

	wxString GetFilename() const { return L"SPU2.bin"; }
	void FreezeIn(pxInputStream& reader) const { return SysState_ComponentFreezeIn(reader, SPU2); }
	void FreezeOut(SaveStateBase& writer) const { return SysState_ComponentFreezeOut(writer, SPU2); }
	bool IsRequired() const { return true; }
};

class SavestateEntry_USB : public BaseSavestateEntry
{
public:
	virtual ~SavestateEntry_USB() = default;

	wxString GetFilename() const { return L"USB.bin"; }
	void FreezeIn(pxInputStream& reader) const { return SysState_ComponentFreezeIn(reader, USB); }
	void FreezeOut(SaveStateBase& writer) const { return SysState_ComponentFreezeOut(writer, USB); }
	bool IsRequired() const { return false; }
};

class SavestateEntry_PAD : public BaseSavestateEntry
{
public:
	virtual ~SavestateEntry_PAD() = default;

	wxString GetFilename() const { return L"PAD.bin"; }
	void FreezeIn(pxInputStream& reader) const { return SysState_ComponentFreezeIn(reader, PAD); }
	void FreezeOut(SaveStateBase& writer) const { return SysState_ComponentFreezeOut(writer, PAD); }
	bool IsRequired() const { return true; }
};

class SavestateEntry_GS : public BaseSavestateEntry
{
public:
	virtual ~SavestateEntry_GS() = default;

	wxString GetFilename() const { return L"GS.bin"; }
	void FreezeIn(pxInputStream& reader) const { return SysState_ComponentFreezeIn(reader, GS); }
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
	std::unique_ptr<BaseSavestateEntry>(new SavestateEntry_USB),
	std::unique_ptr<BaseSavestateEntry>(new SavestateEntry_PAD),
	std::unique_ptr<BaseSavestateEntry>(new SavestateEntry_GS),
};

// It's bad mojo to have savestates trying to read and write from the same file at the
// same time.  To prevent that we use this mutex lock, which is used by both the
// CompressThread and the UnzipFromDisk events.  (note that CompressThread locks the
// mutex during OnStartInThread, which ensures that the ZipToDisk event blocks; preventing
// the SysExecutor's Idle Event from re-enabing savestates and slots.)
//
static Mutex mtx_CompressToDisk;

static void CheckVersion(pxInputStream& thr)
{
	u32 savever;
	thr.Read(savever);

	// Major version mismatch.  Means we can't load this savestate at all.  Support for it
	// was removed entirely.
	if (savever > g_SaveVersion)
		throw Exception::SaveStateLoadError(thr.GetStreamName())
		.SetDiagMsg(pxsFmt(L"Savestate uses an unsupported or unknown savestate version.\n(PCSX2 ver=%x, state ver=%x)", g_SaveVersion, savever))
		.SetUserMsg(_("Cannot load this savestate. The state is an unsupported version."));

	// check for a "minor" version incompatibility; which happens if the savestate being loaded is a newer version
	// than the emulator recognizes.  99% chance that trying to load it will just corrupt emulation or crash.
	if ((savever >> 16) != (g_SaveVersion >> 16))
		throw Exception::SaveStateLoadError(thr.GetStreamName())
		.SetDiagMsg(pxsFmt(L"Savestate uses an unknown savestate version.\n(PCSX2 ver=%x, state ver=%x)", g_SaveVersion, savever))
		.SetUserMsg(_("Cannot load this savestate. The state is an unsupported version."));
};

void SaveState_DownloadState(ArchiveEntryList* destlist)
{
	if (!GetCoreThread().HasActiveMachine())
		throw Exception::RuntimeError()
			.SetDiagMsg(L"SysExecEvent_DownloadState: Cannot freeze/download an invalid VM state!")
			.SetUserMsg(_("There is no active virtual machine state to download or save."));

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
}

// --------------------------------------------------------------------------------------
//  CompressThread_VmState
// --------------------------------------------------------------------------------------
static void ZipStateToDiskOnThread(std::unique_ptr<ArchiveEntryList> srclist, std::unique_ptr<wxFFileOutputStream> outbase, wxString filename, wxString tempfile)
{
#ifndef PCSX2_CORE
	wxGetApp().StartPendingSave();
#endif

	std::unique_ptr<pxOutputStream> out(new pxOutputStream(tempfile, new wxZipOutputStream(outbase.release())));
	wxZipOutputStream* gzfp = (wxZipOutputStream*)out->GetWxStreamBase();

	{
		wxZipEntry* vent = new wxZipEntry(EntryFilename_StateVersion);
		vent->SetMethod(wxZIP_METHOD_STORE);
		gzfp->PutNextEntry(vent);
		out->Write(g_SaveVersion);
		gzfp->CloseEntry();
	}

#if 0
	// This was never actually initialized...
	std::unique_ptr<wxImage> m_screenshot;
	if (m_screenshot)
	{
		wxZipEntry* vent = new wxZipEntry(EntryFilename_Screenshot);
		vent->SetMethod(wxZIP_METHOD_STORE);
		gzfp->PutNextEntry(vent);
		m_screenshot->SaveFile(*gzfp, wxBITMAP_TYPE_JPEG);
		gzfp->CloseEntry();
	}
#endif

	uint listlen = srclist->GetLength();
	for (uint i = 0; i < listlen; ++i)
	{
		const ArchiveEntry& entry = (*srclist)[i];
		if (!entry.GetDataSize())
			continue;

		gzfp->PutNextEntry(entry.GetFilename());

		static const uint BlockSize = 0x64000;
		uint curidx = 0;

		do
		{
			uint thisBlockSize = std::min(BlockSize, entry.GetDataSize() - curidx);
			gzfp->Write(srclist->GetPtr(entry.GetDataIndex() + curidx), thisBlockSize);
			curidx += thisBlockSize;
		} while (curidx < entry.GetDataSize());

		gzfp->CloseEntry();
	}

	gzfp->Close();

	if (!wxRenameFile(out->GetStreamName(), filename, true))
	{
		Console.Error("Failed to rename save state '%s' to '%s'", static_cast<const char*>(out->GetStreamName().c_str()), static_cast<const char*>(filename.c_str()));
#ifndef PCSX2_CORE
		Msgbox::Alert(_("The savestate was not properly saved. The temporary file was created successfully but could not be moved to its final resting place."));
#endif
	}
	else
	{
		Console.WriteLn("(gzipThread) Data saved to disk without error.");
	}

#ifndef PCSX2_CORE
	wxGetApp().ClearPendingSave();
#endif
}

void SaveState_ZipToDisk(ArchiveEntryList* srclist, const wxString& filename)
{
	// Provisionals for scoped cleanup, in case of exception:
	std::unique_ptr<ArchiveEntryList> elist(srclist);

	wxString tempfile(filename + L".tmp");
	std::unique_ptr<wxFFileOutputStream> out = std::make_unique<wxFFileOutputStream>(tempfile);
	if (!out->IsOk())
		throw Exception::CannotCreateStream(tempfile);

	std::thread threaded_save(ZipStateToDiskOnThread, std::move(elist), std::move(out), filename, tempfile);
	threaded_save.detach();
}

void SaveState_UnzipFromDisk(const wxString& filename)
{
	ScopedLock lock(mtx_CompressToDisk);

	// Ugh.  Exception handling made crappy because wxWidgets classes don't support scoped pointers yet.

	std::unique_ptr<wxFFileInputStream> woot(new wxFFileInputStream(filename));
	if (!woot->IsOk())
		throw Exception::CannotCreateStream(filename).SetDiagMsg(L"Cannot open file for reading.");

	std::unique_ptr<pxInputStream> reader(new pxInputStream(filename, new wxZipInputStream(woot.get())));
	woot.release();

	if (!reader->IsOk())
	{
		throw Exception::SaveStateLoadError(filename)
			.SetDiagMsg(L"Savestate file is not a valid gzip archive.")
			.SetUserMsg(_("This savestate cannot be loaded because it is not a valid gzip archive.  It may have been created by an older unsupported version of PCSX2, or it may be corrupted."));
	}

	wxZipInputStream* gzreader = (wxZipInputStream*)reader->GetWxStreamBase();

	// look for version and screenshot information in the zip stream:

	bool foundVersion = false;
	//bool foundScreenshot = false;
	//bool foundEntry[ArraySize(SavestateEntries)] = false;

	std::unique_ptr<wxZipEntry> foundInternal;
	std::unique_ptr<wxZipEntry> foundEntry[std::size(SavestateEntries)];

	while (true)
	{
		Threading::pxTestCancel();

		std::unique_ptr<wxZipEntry> entry(gzreader->GetNextEntry());
		if (!entry)
			break;

		if (entry->GetName().CmpNoCase(EntryFilename_StateVersion) == 0)
		{
			DevCon.WriteLn(Color_Green, L" ... found '%s'", EntryFilename_StateVersion);
			foundVersion = true;
			CheckVersion(*reader);
			continue;
		}

		if (entry->GetName().CmpNoCase(EntryFilename_InternalStructures) == 0)
		{
			DevCon.WriteLn(Color_Green, L" ... found '%s'", EntryFilename_InternalStructures);
			foundInternal = std::move(entry);
			continue;
		}

		// No point in finding screenshots when loading states -- the screenshots are
		// only useful for the UI savestate browser.
		/*if (entry->GetName().CmpNoCase(EntryFilename_Screenshot) == 0)
		{
			foundScreenshot = true;
		}*/

		for (uint i = 0; i < std::size(SavestateEntries); ++i)
		{
			if (entry->GetName().CmpNoCase(SavestateEntries[i]->GetFilename()) == 0)
			{
				DevCon.WriteLn(Color_Green, L" ... found '%s'", WX_STR(SavestateEntries[i]->GetFilename()));
				foundEntry[i] = std::move(entry);
				break;
			}
		}
	}

	if (!foundVersion || !foundInternal)
	{
		throw Exception::SaveStateLoadError(filename)
			.SetDiagMsg(pxsFmt(L"Savestate file does not contain '%s'",
								  !foundVersion ? EntryFilename_StateVersion : EntryFilename_InternalStructures))
			.SetUserMsg(_("This file is not a valid PCSX2 savestate.  See the logfile for details."));
	}

	// Log any parts and pieces that are missing, and then generate an exception.
	bool throwIt = false;
	for (uint i = 0; i < std::size(SavestateEntries); ++i)
	{
		if (foundEntry[i])
			continue;

		if (SavestateEntries[i]->IsRequired())
		{
			throwIt = true;
			Console.WriteLn(Color_Red, " ... not found '%s'!", WX_STR(SavestateEntries[i]->GetFilename()));
		}
	}

	if (throwIt)
		throw Exception::SaveStateLoadError(filename)
			.SetDiagMsg(L"Savestate cannot be loaded: some required components were not found or are incomplete.")
			.SetUserMsg(_("This savestate cannot be loaded due to missing critical components.  See the log file for details."));

	PatchesVerboseReset();
	SysClearExecutionCache();

	for (uint i = 0; i < std::size(SavestateEntries); ++i)
	{
		if (!foundEntry[i])
			continue;

		Threading::pxTestCancel();

		gzreader->OpenEntry(*foundEntry[i]);
		SavestateEntries[i]->FreezeIn(*reader);
	}

	// Load all the internal data

	gzreader->OpenEntry(*foundInternal);

	VmStateBuffer buffer(foundInternal->GetSize(), L"StateBuffer_UnzipFromDisk"); // start with an 8 meg buffer to avoid frequent reallocation.
	reader->Read(buffer.GetPtr(), foundInternal->GetSize());

	memLoadingState(buffer).FreezeBios().FreezeInternals();
}
