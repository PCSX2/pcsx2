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

#include "Achievements.h"
#include "CDVD/CDVD.h"
#include "COP0.h"
#include "Cache.h"
#include "Config.h"
#include "Counters.h"
#include "DebugTools/Breakpoints.h"
#include "Elfheader.h"
#include "GS.h"
#include "GS/GS.h"
#include "Host.h"
#include "MTGS.h"
#include "MTVU.h"
#include "SIO/Pad/Pad.h"
#include "Patch.h"
#include "R3000A.h"
#include "SIO/Sio0.h"
#include "SIO/Sio2.h"
#include "SPU2/spu2.h"
#include "SaveState.h"
#include "StateWrapper.h"
#include "USB/USB.h"
#include "VMManager.h"
#include "VUmicro.h"
#include "ps2/BiosTools.h"
#include "svnrev.h"

#include "common/Error.h"
#include "common/FileSystem.h"
#include "common/Path.h"
#include "common/ScopedGuard.h"
#include "common/StringUtil.h"
#include "common/ZipHelpers.h"

#include "fmt/core.h"

#include <csetjmp>
#include <png.h>

using namespace R5900;

static tlbs s_tlb_backup[std::size(tlb)];

static void PreLoadPrep()
{
	// ensure everything is in sync before we start overwriting stuff.
	if (THREAD_VU1)
		vu1Thread.WaitVU();
	MTGS::WaitGS(false);

	// backup current TLBs, since we're going to overwrite them all
	std::memcpy(s_tlb_backup, tlb, sizeof(s_tlb_backup));

	// clear protected pages, since we don't want to fault loading EE memory
	mmap_ResetBlockTracking();

	SysClearExecutionCache();
}

static void PostLoadPrep()
{
	resetCache();
//	WriteCP0Status(cpuRegs.CP0.n.Status.val);
	for (int i = 0; i < 48; i++)
	{
		if (std::memcmp(&s_tlb_backup[i], &tlb[i], sizeof(tlbs)) != 0)
		{
			UnmapTLB(s_tlb_backup[i], i);
			MapTLB(tlb[i], i);
		}
	}

	if (EmuConfig.Gamefixes.GoemonTlbHack) GoemonPreloadTlb();
	CBreakPoints::SetSkipFirst(BREAKPOINT_EE, 0);
	CBreakPoints::SetSkipFirst(BREAKPOINT_IOP, 0);

	UpdateVSyncRate(true);
}

// --------------------------------------------------------------------------------------
//  SaveStateBase  (implementations)
// --------------------------------------------------------------------------------------
SaveStateBase::SaveStateBase(VmStateBuffer& memblock)
	: m_memory(memblock)
	, m_version(g_SaveVersion)
{
}

void SaveStateBase::PrepBlock(int size)
{
	if (m_error)
		return;

	const int end = m_idx + size;
	if (IsSaving())
	{
		if (static_cast<u32>(end) >= m_memory.size())
			m_memory.resize(static_cast<u32>(end));
	}
	else
	{
		if (m_memory.size() < static_cast<u32>(end))
		{
			Console.Error("(SaveStateBase) Buffer overflow in PrepBlock(), expected %d got %zu", end, m_memory.size());
			m_error = true;
		}
	}
}

bool SaveStateBase::FreezeTag(const char* src)
{
	if (m_error)
		return false;

	char tagspace[32];
	pxAssertDev(std::strlen(src) < (sizeof(tagspace) - 1), "Tag name exceeds the allowed length");

	std::memset(tagspace, 0, sizeof(tagspace));
	StringUtil::Strlcpy(tagspace, src, sizeof(tagspace));
	Freeze(tagspace);

	if (std::strcmp(tagspace, src) != 0)
	{
		Console.Error(fmt::format("Savestate data corruption detected while reading tag: {}", src));
		m_error = true;
		return false;
	}

	return true;
}

bool SaveStateBase::FreezeBios()
{
	if (!FreezeTag("BIOS"))
		return false;

	// Check the BIOS, and issue a warning if the bios for this state
	// doesn't match the bios currently being used (chances are it'll still
	// work fine, but some games are very picky).

	u32 bioscheck = BiosChecksum;
	char biosdesc[256];
	std::memset(biosdesc, 0, sizeof(biosdesc));
	StringUtil::Strlcpy(biosdesc, BiosDescription, sizeof(biosdesc));

	Freeze(bioscheck);
	Freeze(biosdesc);

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

	return IsOkay();
}

bool SaveStateBase::FreezeInternals()
{
	// Print this until the MTVU problem in gifPathFreeze is taken care of (rama)
	if (THREAD_VU1)
		Console.Warning("MTVU speedhack is enabled, saved states may not be stable");

	if (!vmFreeze())
		return false;

	// Second Block - Various CPU Registers and States
	// -----------------------------------------------
	if (!FreezeTag("cpuRegs"))
		return false;

	Freeze(cpuRegs);		// cpu regs + COP0
	Freeze(psxRegs);		// iop regs
	Freeze(fpuRegs);
	Freeze(tlb);			// tlbs
	Freeze(AllowParams1);	//OSDConfig written (Fast Boot)
	Freeze(AllowParams2);

	// Third Block - Cycle Timers and Events
	// -------------------------------------
	if (!FreezeTag("Cycles"))
		return false;

	Freeze(EEsCycle);
	Freeze(EEoCycle);
	Freeze(nextCounter);
	Freeze(nextsCounter);
	Freeze(psxNextsCounter);
	Freeze(psxNextCounter);

	// Fourth Block - EE-related systems
	// ---------------------------------
	if (!FreezeTag("EE-Subsystems"))
		return false;

	bool okay = rcntFreeze();
	okay = okay && gsFreeze();
	okay = okay && vuMicroFreeze();
	okay = okay && vuJITFreeze();
	okay = okay && vif0Freeze();
	okay = okay && vif1Freeze();
	okay = okay && sifFreeze();
	okay = okay && ipuFreeze();
	okay = okay && ipuDmaFreeze();
	okay = okay && gifFreeze();
	okay = okay && gifDmaFreeze();
	okay = okay && sprFreeze();
	okay = okay && mtvuFreeze();
	if (!okay)
		return false;

	// Fifth Block - iop-related systems
	// ---------------------------------
	if (!FreezeTag("IOP-Subsystems"))
		return false;

	FreezeMem(iopMem->Sif, sizeof(iopMem->Sif));		// iop's sif memory (not really needed, but oh well)

	okay = okay && psxRcntFreeze();

	// TODO: move all the others over to StateWrapper too...
	if (!okay)
		return false;
	{
		// This is horrible. We need to move the rest over...
		std::optional<StateWrapper::VectorMemoryStream> save_stream;
		std::optional<StateWrapper::ReadOnlyMemoryStream> load_stream;
		if (IsSaving())
			save_stream.emplace();
		else
			load_stream.emplace(&m_memory[m_idx], static_cast<int>(m_memory.size()) - m_idx);

		StateWrapper sw(IsSaving() ? static_cast<StateWrapper::IStream*>(&save_stream.value()) :
									 static_cast<StateWrapper::IStream*>(&load_stream.value()),
			IsSaving() ? StateWrapper::Mode::Write : StateWrapper::Mode::Read, g_SaveVersion);

		okay = okay && g_Sio0.DoState(sw);
		okay = okay && g_Sio2.DoState(sw);
		if (!okay || !sw.IsGood())
			return false;

		if (IsSaving())
		{
			FreezeMem(const_cast<u8*>(save_stream->GetBuffer().data()), save_stream->GetPosition());
		}
		else
		{
			const int new_idx = m_idx + static_cast<int>(load_stream->GetPosition());
			if (static_cast<size_t>(new_idx) >= m_memory.size())
				return false;

			m_idx = new_idx;
		}
	}

	okay = okay && cdrFreeze();
	okay = okay && cdvdFreeze();

	// technically this is HLE BIOS territory, but we don't have enough such stuff
	// to merit an HLE Bios sub-section... yet.
	okay = okay && deci2Freeze();

	okay = okay && InputRecordingFreeze();

	return okay;
}


// --------------------------------------------------------------------------------------
//  memSavingState (implementations)
// --------------------------------------------------------------------------------------
// uncompressed to/from memory state saves implementation

memSavingState::memSavingState(VmStateBuffer& save_to)
	: SaveStateBase(save_to)
{
}

// Saving of state data
void memSavingState::FreezeMem(void* data, int size)
{
	if (!size) return;

	const int new_size = m_idx + size;
	if (static_cast<u32>(new_size) > m_memory.size())
		m_memory.resize(static_cast<u32>(new_size));

	std::memcpy(&m_memory[m_idx], data, size);
	m_idx += size;
}

// --------------------------------------------------------------------------------------
//  memLoadingState  (implementations)
// --------------------------------------------------------------------------------------
memLoadingState::memLoadingState(const VmStateBuffer& load_from)
	: SaveStateBase(const_cast<VmStateBuffer&>(load_from))
{
}

// Loading of state data from a memory buffer...
void memLoadingState::FreezeMem( void* data, int size )
{
	if (m_error)
	{
		std::memset(data, 0, size);
		return;
	}

	const u8* const src = &m_memory[m_idx];
	m_idx += size;
	std::memcpy(data, src, size);
}

static const char* EntryFilename_StateVersion = "PCSX2 Savestate Version.id";
static const char* EntryFilename_Screenshot = "Screenshot.png";
static const char* EntryFilename_InternalStructures = "PCSX2 Internal Structures.dat";
static constexpr u32 STATE_PCSX2_VERSION_SIZE = 32;

struct SysState_Component
{
	const char* name;
	int (*freeze)(FreezeAction, freezeData*);
};

static int SysState_MTGSFreeze(FreezeAction mode, freezeData* fP)
{
	MTGS::FreezeData sstate = { fP, 0 };
	MTGS::Freeze(mode, sstate);
	return sstate.retval;
}

static constexpr SysState_Component SPU2_{ "SPU2", SPU2freeze };
static constexpr SysState_Component GS{ "GS", SysState_MTGSFreeze };

static bool SysState_ComponentFreezeIn(zip_file_t* zf, SysState_Component comp)
{
	if (!zf)
		return true;

	freezeData fP = { 0, nullptr };
	if (comp.freeze(FreezeAction::Size, &fP) != 0)
		fP.size = 0;

	Console.Indent().WriteLn("Loading %s", comp.name);

	std::unique_ptr<u8[]> data;
	if (fP.size > 0)
	{
		data = std::make_unique<u8[]>(fP.size);
		fP.data = data.get();

		if (zip_fread(zf, data.get(), fP.size) != static_cast<zip_int64_t>(fP.size))
		{
			Console.Error(fmt::format("* {}: Failed to decompress save data", comp.name));
			return false;
		}
	}

	if (comp.freeze(FreezeAction::Load, &fP) != 0)
	{
		Console.Error(fmt::format("* {}: Failed to load freeze data", comp.name));
		return false;
	}

	return true;
}

static bool SysState_ComponentFreezeOut(SaveStateBase& writer, SysState_Component comp)
{
	freezeData fP = {};
	if (comp.freeze(FreezeAction::Size, &fP) != 0)
	{
		Console.Error(fmt::format("* {}: Failed to get freeze size", comp.name));
		return false;
	}

	if (fP.size == 0)
		return true;

	const int size = fP.size;
	writer.PrepBlock(size);

	Console.Indent().WriteLn("Saving %s", comp.name);

	fP.data = writer.GetBlockPtr();
	if (comp.freeze(FreezeAction::Save, &fP) != 0)
	{
		Console.Error(fmt::format("* {}: Failed to save freeze data", comp.name));
		return false;
	}

	writer.CommitBlock(size);
	return true;
}

static bool SysState_ComponentFreezeInNew(zip_file_t* zf, const char* name, bool(*do_state_func)(StateWrapper&))
{
	// TODO: We could decompress on the fly here for a little bit more speed.
	std::vector<u8> data;
	if (zf)
	{
		std::optional<std::vector<u8>> optdata(ReadBinaryFileInZip(zf));
		if (optdata.has_value())
			data = std::move(optdata.value());
	}

	StateWrapper::ReadOnlyMemoryStream stream(data.empty() ? nullptr : data.data(), data.size());
	StateWrapper sw(&stream, StateWrapper::Mode::Read, g_SaveVersion);

	return do_state_func(sw);
}

static bool SysState_ComponentFreezeOutNew(SaveStateBase& writer, const char* name, u32 reserve, bool (*do_state_func)(StateWrapper&))
{
	StateWrapper::VectorMemoryStream stream(reserve);
	StateWrapper sw(&stream, StateWrapper::Mode::Write, g_SaveVersion);

	if (!do_state_func(sw))
		return false;

	const int size = static_cast<int>(stream.GetBuffer().size());
	if (size > 0)
	{
		writer.PrepBlock(size);
		std::memcpy(writer.GetBlockPtr(), stream.GetBuffer().data(), size);
		writer.CommitBlock(size);
	}

	return true;
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
	virtual bool FreezeIn(zip_file_t* zf) const = 0;
	virtual bool FreezeOut(SaveStateBase& writer) const = 0;
	virtual bool IsRequired() const = 0;
};

class MemorySavestateEntry : public BaseSavestateEntry
{
protected:
	MemorySavestateEntry() {}
	virtual ~MemorySavestateEntry() = default;

public:
	virtual bool FreezeIn(zip_file_t* zf) const;
	virtual bool FreezeOut(SaveStateBase& writer) const;
	virtual bool IsRequired() const { return true; }

protected:
	virtual u8* GetDataPtr() const = 0;
	virtual u32 GetDataSize() const = 0;
};

bool MemorySavestateEntry::FreezeIn(zip_file_t* zf) const
{
	const u32 expectedSize = GetDataSize();
	const s64 bytesRead = zip_fread(zf, GetDataPtr(), expectedSize);
	if (bytesRead != static_cast<s64>(expectedSize))
	{
		Console.WriteLn(Color_Yellow, " '%s' is incomplete (expected 0x%x bytes, loading only 0x%x bytes)",
			GetFilename(), expectedSize, static_cast<u32>(bytesRead));
	}

	return true;
}

bool MemorySavestateEntry::FreezeOut(SaveStateBase& writer) const
{
	writer.FreezeMem(GetDataPtr(), GetDataSize());
	return writer.IsOkay();
}

// --------------------------------------------------------------------------------------
//  SavestateEntry_* (EmotionMemory, IopMemory, etc)
// --------------------------------------------------------------------------------------
// Implementation Rationale:
//  The address locations of PS2 virtual memory components is fully dynamic, so we need to
//  resolve the pointers at the time they are requested (eeMem, iopMem, etc).  Thusly, we
//  cannot use static struct member initializers -- we need virtual functions that compute
//  and resolve the addresses on-demand instead... --air

class SavestateEntry_EmotionMemory final : public MemorySavestateEntry
{
public:
	~SavestateEntry_EmotionMemory() override = default;

	const char* GetFilename() const override { return "eeMemory.bin"; }
	u8* GetDataPtr() const override { return eeMem->Main; }
	uint GetDataSize() const override { return sizeof(eeMem->Main); }

	virtual bool FreezeIn(zip_file_t* zf) const override
	{
		SysClearExecutionCache();
		return MemorySavestateEntry::FreezeIn(zf);
	}
};

class SavestateEntry_IopMemory final : public MemorySavestateEntry
{
public:
	~SavestateEntry_IopMemory() override = default;

	const char* GetFilename() const override { return "iopMemory.bin"; }
	u8* GetDataPtr() const override { return iopMem->Main; }
	uint GetDataSize() const override { return sizeof(iopMem->Main); }
};

class SavestateEntry_HwRegs final : public MemorySavestateEntry
{
public:
	~SavestateEntry_HwRegs() override = default;

	const char* GetFilename() const override { return "eeHwRegs.bin"; }
	u8* GetDataPtr() const override { return eeHw; }
	uint GetDataSize() const override { return sizeof(eeHw); }
};

class SavestateEntry_IopHwRegs final : public MemorySavestateEntry
{
public:
	~SavestateEntry_IopHwRegs() = default;

	const char* GetFilename() const override { return "iopHwRegs.bin"; }
	u8* GetDataPtr() const override { return iopHw; }
	uint GetDataSize() const override { return sizeof(iopHw); }
};

class SavestateEntry_Scratchpad final : public MemorySavestateEntry
{
public:
	~SavestateEntry_Scratchpad() = default;

	const char* GetFilename() const override { return "Scratchpad.bin"; }
	u8* GetDataPtr() const override { return eeMem->Scratch; }
	uint GetDataSize() const override { return sizeof(eeMem->Scratch); }
};

class SavestateEntry_VU0mem final : public MemorySavestateEntry
{
public:
	~SavestateEntry_VU0mem() = default;

	const char* GetFilename() const override { return "vu0Memory.bin"; }
	u8* GetDataPtr() const override { return vuRegs[0].Mem; }
	uint GetDataSize() const override { return VU0_MEMSIZE; }
};

class SavestateEntry_VU1mem final : public MemorySavestateEntry
{
public:
	~SavestateEntry_VU1mem() = default;

	const char* GetFilename() const override { return "vu1Memory.bin"; }
	u8* GetDataPtr() const override { return vuRegs[1].Mem; }
	uint GetDataSize() const override { return VU1_MEMSIZE; }
};

class SavestateEntry_VU0prog final : public MemorySavestateEntry
{
public:
	~SavestateEntry_VU0prog() = default;

	const char* GetFilename() const override { return "vu0MicroMem.bin"; }
	u8* GetDataPtr() const override { return vuRegs[0].Micro; }
	uint GetDataSize() const override { return VU0_PROGSIZE; }
};

class SavestateEntry_VU1prog final : public MemorySavestateEntry
{
public:
	~SavestateEntry_VU1prog() = default;

	const char* GetFilename() const override { return "vu1MicroMem.bin"; }
	u8* GetDataPtr() const override { return vuRegs[1].Micro; }
	uint GetDataSize() const override { return VU1_PROGSIZE; }
};

class SavestateEntry_SPU2 final : public BaseSavestateEntry
{
public:
	~SavestateEntry_SPU2() override = default;

	const char* GetFilename() const override { return "SPU2.bin"; }
	bool FreezeIn(zip_file_t* zf) const override { return SysState_ComponentFreezeIn(zf, SPU2_); }
	bool FreezeOut(SaveStateBase& writer) const override { return SysState_ComponentFreezeOut(writer, SPU2_); }
	bool IsRequired() const override { return true; }
};

class SavestateEntry_USB final : public BaseSavestateEntry
{
public:
	~SavestateEntry_USB() override = default;

	const char* GetFilename() const override { return "USB.bin"; }
	bool FreezeIn(zip_file_t* zf) const override { return SysState_ComponentFreezeInNew(zf, "USB", &USB::DoState); }
	bool FreezeOut(SaveStateBase& writer) const override { return SysState_ComponentFreezeOutNew(writer, "USB", 16 * 1024, &USB::DoState); }
	bool IsRequired() const override { return false; }
};

class SavestateEntry_PAD final : public BaseSavestateEntry
{
public:
	~SavestateEntry_PAD() override = default;

	const char* GetFilename() const override { return "PAD.bin"; }
	bool FreezeIn(zip_file_t* zf) const override { return SysState_ComponentFreezeInNew(zf, "PAD", &Pad::Freeze); }
	bool FreezeOut(SaveStateBase& writer) const override { return SysState_ComponentFreezeOutNew(writer, "PAD", 16 * 1024, &Pad::Freeze); }
	bool IsRequired() const override { return true; }
};

class SavestateEntry_GS final : public BaseSavestateEntry
{
public:
	~SavestateEntry_GS() = default;

	const char* GetFilename() const { return "GS.bin"; }
	bool FreezeIn(zip_file_t* zf) const { return SysState_ComponentFreezeIn(zf, GS); }
	bool FreezeOut(SaveStateBase& writer) const { return SysState_ComponentFreezeOut(writer, GS); }
	bool IsRequired() const { return true; }
};

class SaveStateEntry_Achievements final : public BaseSavestateEntry
{
	~SaveStateEntry_Achievements() override = default;

	const char* GetFilename() const override { return "Achievements.bin"; }
	bool FreezeIn(zip_file_t* zf) const override
	{
		if (!Achievements::IsActive())
			return true;

		std::optional<std::vector<u8>> data;
		if (zf)
			data = ReadBinaryFileInZip(zf);

		if (data.has_value() && !data->empty())
			Achievements::LoadState(data->data(), data->size());
		else
			Achievements::LoadState(nullptr, 0);

		return true;
	}

	bool FreezeOut(SaveStateBase& writer) const override
	{
		if (!Achievements::IsActive())
			return true;

		std::vector<u8> data(Achievements::SaveState());
		if (!data.empty())
		{
			writer.PrepBlock(static_cast<int>(data.size()));
			std::memcpy(writer.GetBlockPtr(), data.data(), data.size());
			writer.CommitBlock(static_cast<int>(data.size()));
		}

		return writer.IsOkay();
	}

	bool IsRequired() const override { return false; }
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
	std::unique_ptr<BaseSavestateEntry>(new SaveStateEntry_Achievements),
};

std::unique_ptr<ArchiveEntryList> SaveState_DownloadState(Error* error)
{
	std::unique_ptr<ArchiveEntryList> destlist = std::make_unique<ArchiveEntryList>();
	destlist->GetBuffer().resize(1024 * 1024 * 64);

	memSavingState saveme(destlist->GetBuffer());
	ArchiveEntry internals(EntryFilename_InternalStructures);
	internals.SetDataIndex(saveme.GetCurrentPos());

	if (!saveme.FreezeBios())
	{
		Error::SetString(error, "FreezeBios() failed");
		return nullptr;
	}

	if (!saveme.FreezeInternals())
	{
		Error::SetString(error, "FreezeInternals() failed");
		return nullptr;
	}

	internals.SetDataSize(saveme.GetCurrentPos() - internals.GetDataIndex());
	destlist->Add(internals);

	for (const std::unique_ptr<BaseSavestateEntry>& entry : SavestateEntries)
	{
		uint startpos = saveme.GetCurrentPos();
		if (!entry->FreezeOut(saveme))
		{
			Error::SetString(error, fmt::format("FreezeOut() failed for {}.", entry->GetFilename()));
			destlist.reset();
			break;
		}

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

	u32 width, height;
	std::vector<u32> pixels;
	if (!MTGS::SaveMemorySnapshot(SCREENSHOT_WIDTH, SCREENSHOT_HEIGHT, true, false, &width, &height, &pixels))
	{
		// saving failed for some reason, device lost?
		return nullptr;
	}

	std::unique_ptr<SaveStateScreenshotData> data = std::make_unique<SaveStateScreenshotData>();
	data->width = width;
	data->height = height;
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
		struct VersionIndicator
		{
			u32 save_version;
			char version[STATE_PCSX2_VERSION_SIZE];
		};

		VersionIndicator* vi = static_cast<VersionIndicator*>(std::malloc(sizeof(VersionIndicator)));
		vi->save_version = g_SaveVersion;
#if GIT_TAGGED_COMMIT
		StringUtil::Strlcpy(vi->version, GIT_TAG, std::size(vi->version));
#else
		StringUtil::Strlcpy(vi->version, "Unknown", std::size(vi->version));
#endif

		zip_source_t* const zs = zip_source_buffer(zf, vi, sizeof(*vi), 1);
		if (!zs)
		{
			std::free(vi);
			return false;
		}

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

static bool CheckVersion(const std::string& filename, zip_t* zf, Error* error)
{
	u32 savever;

	auto zff = zip_fopen_managed(zf, EntryFilename_StateVersion, 0);
	if (!zff || zip_fread(zff.get(), &savever, sizeof(savever)) != sizeof(savever))
	{
		Error::SetString(error, "Savestate file does not contain version indicator.");
		return false;
	}

	char version_string[STATE_PCSX2_VERSION_SIZE];
	if (zip_fread(zff.get(), version_string, STATE_PCSX2_VERSION_SIZE) == STATE_PCSX2_VERSION_SIZE)
		version_string[STATE_PCSX2_VERSION_SIZE - 1] = 0;
	else
		StringUtil::Strlcpy(version_string, "Unknown", std::size(version_string));

	// Major version mismatch.  Means we can't load this savestate at all.  Support for it
	// was removed entirely.
	// check for a "minor" version incompatibility; which happens if the savestate being loaded is a newer version
	// than the emulator recognizes.  99% chance that trying to load it will just corrupt emulation or crash.
	if (savever > g_SaveVersion || (savever >> 16) != (g_SaveVersion >> 16))
	{
		Error::SetString(error, fmt::format(TRANSLATE_FS("SaveState","This savestate is an unsupported version and cannot be used.\n\n"
											"You can download PCSX2 {} from pcsx2.net and make a normal memory card save.\n"
											"Otherwise delete the savestate and do a fresh boot."),
											version_string));
		return false;
	}

	return true;
}

static zip_int64_t CheckFileExistsInState(zip_t* zf, const char* name, bool required)
{
	zip_int64_t index = zip_name_locate(zf, name, /*ZIP_FL_NOCASE*/ 0);
	if (index >= 0)
	{
		DevCon.WriteLn(Color_Green, " ... found '%s'", name);
		return index;
	}

	if (required)
		Console.WriteLn(Color_Red, " ... not found '%s'!", name);
	else
		DevCon.WriteLn(Color_Red, " ... not found '%s'!", name);

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

	std::vector<u8> buffer(zst.size);
	if (zip_fread(zff.get(), buffer.data(), buffer.size()) != static_cast<zip_int64_t>(buffer.size()))
		return false;

	memLoadingState state(buffer);
	if (!state.FreezeBios())
		return false;
	
	if (!state.FreezeInternals())
		return false;

	return true;
}

bool SaveState_UnzipFromDisk(const std::string& filename, Error* error)
{
	zip_error_t ze = {};
	auto zf = zip_open_managed(filename.c_str(), ZIP_RDONLY, &ze);
	if (!zf)
	{
		Console.Error("Failed to open zip file '%s' for save state load: %s", filename.c_str(), zip_error_strerror(&ze));
		if (zip_error_code_zip(&ze) == ZIP_ER_NOENT)
			Error::SetString(error, "Savestate file does not exist.");
		else
			Error::SetString(error, fmt::format("Savestate zip error: {}", zip_error_strerror(&ze)));

		return false;
	}

	// look for version and screenshot information in the zip stream:
	if (!CheckVersion(filename, zf.get(), error))
		return false;

	// check that all parts are included
	const s64 internal_index = CheckFileExistsInState(zf.get(), EntryFilename_InternalStructures, true);
	s64 entryIndices[std::size(SavestateEntries)];

	// Log any parts and pieces that are missing, and then generate an exception.
	bool allPresent = (internal_index >= 0);
	for (u32 i = 0; i < std::size(SavestateEntries); i++)
	{
		const bool required = SavestateEntries[i]->IsRequired();
		entryIndices[i] = CheckFileExistsInState(zf.get(), SavestateEntries[i]->GetFilename(), required);
		if (entryIndices[i] < 0 && required)
		{
			allPresent = false;
			break;
		}
	}
	if (!allPresent)
	{
		Error::SetString(error, "Some required components were not found or are incomplete.");
		return false;
	}

	PreLoadPrep();

	if (!LoadInternalStructuresState(zf.get(), internal_index))
	{
		Error::SetString(error, "Save state corruption in internal structures.");
		return false;
	}

	for (u32 i = 0; i < std::size(SavestateEntries); ++i)
	{
		if (entryIndices[i] < 0)
		{
			SavestateEntries[i]->FreezeIn(nullptr);
			continue;
		}

		auto zff = zip_fopen_index_managed(zf.get(), entryIndices[i], 0);
		if (!zff || !SavestateEntries[i]->FreezeIn(zff.get()))
		{
			Error::SetString(error, fmt::format("Save state corruption in {}.", SavestateEntries[i]->GetFilename()));
			return false;
		}
	}

	PostLoadPrep();
	return true;
}
