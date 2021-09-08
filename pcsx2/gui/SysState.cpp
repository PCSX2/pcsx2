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
#include "MemoryTypes.h"
#include "App.h"

#include "System/SysThreads.h"
#include "SaveState.h"
#include "VUmicro.h"

#include "common/pxStreams.h"
#include "SPU2/spu2.h"
#include "USB/USB.h"
#include "PAD/Gamepad.h"

#include "ConsoleLogger.h"

#include <wx/wfstream.h>
#include <memory>

#include "Patch.h"

// Required for savestate folder creation
#include "CDVD/CDVD.h"
#include "ps2/BiosTools.h"
#include "Elfheader.h"

// --------------------------------------------------------------------------------------
//  SysExecEvent_DownloadState
// --------------------------------------------------------------------------------------
// Pauses core emulation and downloads the savestate into a memory buffer.  The memory buffer
// is then mailed to another thread for zip archiving, while the main emulation process is
// allowed to continue execution.
//
class SysExecEvent_DownloadState : public SysExecEvent
{
protected:
	ArchiveEntryList* m_dest_list;

public:
	wxString GetEventName() const { return L"VM_Download"; }

	virtual ~SysExecEvent_DownloadState() = default;
	SysExecEvent_DownloadState* Clone() const { return new SysExecEvent_DownloadState(*this); }
	SysExecEvent_DownloadState(ArchiveEntryList* dest_list = NULL)
	{
		m_dest_list = dest_list;
	}

	bool IsCriticalEvent() const { return true; }
	bool AllowCancelOnExit() const { return false; }

protected:
	void InvokeEvent()
	{
		ScopedCoreThreadPause paused_core;
		SaveState_DownloadState(m_dest_list);
		UI_EnableStateActions();
		paused_core.AllowResume();
	}
};




// --------------------------------------------------------------------------------------
//  SysExecEvent_ZipToDisk
// --------------------------------------------------------------------------------------
class SysExecEvent_ZipToDisk : public SysExecEvent
{
protected:
	ArchiveEntryList* m_src_list;
	wxString m_filename;

public:
	wxString GetEventName() const { return L"VM_ZipToDisk"; }

	virtual ~SysExecEvent_ZipToDisk() = default;

	SysExecEvent_ZipToDisk* Clone() const { return new SysExecEvent_ZipToDisk(*this); }

	SysExecEvent_ZipToDisk(ArchiveEntryList& srclist, const wxString& filename)
		: m_filename(filename)
	{
		m_src_list = &srclist;
	}

	SysExecEvent_ZipToDisk(ArchiveEntryList* srclist, const wxString& filename)
		: m_filename(filename)
	{
		m_src_list = srclist;
	}

	bool IsCriticalEvent() const { return true; }
	bool AllowCancelOnExit() const { return false; }

protected:
	void InvokeEvent()
	{
		SaveState_ZipToDisk(m_src_list, m_filename);
	}

	void CleanupEvent()
	{
	}
};

// --------------------------------------------------------------------------------------
//  SysExecEvent_UnzipFromDisk
// --------------------------------------------------------------------------------------
// Note: Unzipping always goes directly into the SysCoreThread's static VM state, and is
// always a blocking action on the SysExecutor thread (the system cannot execute other
// commands while states are unzipping or uploading into the system).
//
class SysExecEvent_UnzipFromDisk : public SysExecEvent
{
protected:
	wxString m_filename;

public:
	wxString GetEventName() const { return L"VM_UnzipFromDisk"; }

	virtual ~SysExecEvent_UnzipFromDisk() = default;
	SysExecEvent_UnzipFromDisk* Clone() const { return new SysExecEvent_UnzipFromDisk(*this); }
	SysExecEvent_UnzipFromDisk(const wxString& filename)
		: m_filename(filename)
	{
	}

	wxString GetStreamName() const { return m_filename; }

protected:
	void InvokeEvent()
	{
		// We use direct Suspend/Resume control here, since it's desirable that emulation
		// *ALWAYS* start execution after the new savestate is loaded.
		GetCoreThread().Pause({});
		SaveState_UnzipFromDisk(m_filename);
		GetCoreThread().Resume(); // force resume regardless of emulation state earlier.
	}
};

// =====================================================================================================
//  StateCopy Public Interface
// =====================================================================================================

void StateCopy_SaveToFile(const wxString& file)
{
	UI_DisableStateActions();

	std::unique_ptr<ArchiveEntryList> ziplist(new ArchiveEntryList(new VmStateBuffer(L"Zippable Savestate")));

	GetSysExecutorThread().PostEvent(new SysExecEvent_DownloadState(ziplist.get()));
	GetSysExecutorThread().PostEvent(new SysExecEvent_ZipToDisk(ziplist.get(), file));

	ziplist.release();
}

void StateCopy_LoadFromFile(const wxString& file)
{
	UI_DisableSysActions();
	GetSysExecutorThread().PostEvent(new SysExecEvent_UnzipFromDisk(file));
}

// Saves recovery state info to the given saveslot, or saves the active emulation state
// (if one exists) and no recovery data was found.  This is needed because when a recovery
// state is made, the emulation state is usually reset so the only persisting state is
// the one in the memory save. :)
void StateCopy_SaveToSlot(uint num)
{
	const wxString file(SaveStateBase::GetSavestateFolder(num, true));

	// Backup old Savestate if one exists.
	if (wxFileExists(file) && EmuConfig.BackupSavestate)
	{
		const wxString copy(SaveStateBase::GetSavestateFolder(num, true) + pxsFmt(L".backup"));

		Console.Indent().WriteLn(Color_StrongGreen, L"Backing up existing state in slot %d.", num);
		wxRenameFile(file, copy);
	}

	OSDlog(Color_StrongGreen, true, "Saving savestate to slot %d...", num);
	Console.Indent().WriteLn(Color_StrongGreen, L"filename: %s", WX_STR(file));

	StateCopy_SaveToFile(file);
#ifdef USE_NEW_SAVESLOTS_UI
	UI_UpdateSysControls();
#endif
}

void StateCopy_LoadFromSlot(uint slot, bool isFromBackup)
{
	wxString file(SaveStateBase::GetSavestateFolder(slot, true) + wxString(isFromBackup ? L".backup" : L""));

	if (!wxFileExists(file))
	{
		OSDlog(Color_StrongGreen, true, "Savestate slot %d%s is empty.", slot, isFromBackup ? " (backup)" : "");
		return;
	}

	OSDlog(Color_StrongGreen, true, "Loading savestate from slot %d...%s", slot, isFromBackup ? " (backup)" : "");
	Console.Indent().WriteLn(Color_StrongGreen, L"filename: %s", WX_STR(file));

	StateCopy_LoadFromFile(file);
#ifdef USE_NEW_SAVESLOTS_UI
	UI_UpdateSysControls();
#endif
}

