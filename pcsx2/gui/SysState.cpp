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

#include "SysThreads.h"
#include "SaveState.h"
#include "VUmicro.h"

#include "common/StringUtil.h"
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

class SysExecEvent_SaveState : public SysExecEvent
{
protected:
	wxString m_filename;

public:
	wxString GetEventName() const { return L"VM_Download"; }

	SysExecEvent_SaveState(const wxString& filename) : m_filename(filename) {}
	virtual ~SysExecEvent_SaveState() = default;

	SysExecEvent_SaveState* Clone() const { return new SysExecEvent_SaveState(*this); }

	bool IsCriticalEvent() const { return true; }
	bool AllowCancelOnExit() const { return false; }

protected:
	void InvokeEvent()
	{
		ScopedCoreThreadPause paused_core;
		std::unique_ptr<ArchiveEntryList> elist = SaveState_DownloadState();
		UI_EnableStateActions();
		paused_core.AllowResume();

		std::thread kickoff(&SysExecEvent_SaveState::ZipThreadProc,
			std::move(elist), StringUtil::wxStringToUTF8String(m_filename));
		kickoff.detach();
	}

	static void ZipThreadProc(std::unique_ptr<ArchiveEntryList> elist, std::string filename)
	{
		wxGetApp().StartPendingSave();
		if (SaveState_ZipToDisk(std::move(elist), nullptr, filename.c_str()))
			Console.WriteLn("(gzipThread) Data saved to disk without error.");
		else
			Console.Error("Failed to zip state to '%s'", filename.c_str());
		wxGetApp().ClearPendingSave();
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
		SaveState_UnzipFromDisk(StringUtil::wxStringToUTF8String(m_filename));
		GetCoreThread().Resume(); // force resume regardless of emulation state earlier.
	}
};

// =====================================================================================================
//  StateCopy Public Interface
// =====================================================================================================

void StateCopy_SaveToFile(const wxString& file)
{
	UI_DisableStateActions();
	GetSysExecutorThread().PostEvent(new SysExecEvent_SaveState(file));
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
	const wxString file(StringUtil::UTF8StringToWxString(SaveStateBase::GetSavestateFolder(num, true)));

	// Backup old Savestate if one exists.
	if (wxFileExists(file) && EmuConfig.BackupSavestate)
	{
		const wxString copy(StringUtil::UTF8StringToWxString(SaveStateBase::GetSavestateFolder(num, true)) + pxsFmt(L".backup"));

		Console.Indent().WriteLn(Color_StrongGreen, "Backing up existing state in slot %d.", num);
		wxRenameFile(file, copy);
	}

	OSDlog(Color_StrongGreen, true, "Saving savestate to slot %d...", num);
	Console.Indent().WriteLn(Color_StrongGreen, "filename: %ls", WX_STR(file));

	StateCopy_SaveToFile(file);
#ifdef USE_NEW_SAVESLOTS_UI
	UI_UpdateSysControls();
#endif
}

void StateCopy_LoadFromSlot(uint slot, bool isFromBackup)
{
	wxString file(StringUtil::UTF8StringToWxString(SaveStateBase::GetSavestateFolder(slot, true)) + wxString(isFromBackup ? L".backup" : L""));

	if (!wxFileExists(file))
	{
		OSDlog(Color_StrongGreen, true, "Savestate slot %d%s is empty.", slot, isFromBackup ? " (backup)" : "");
		return;
	}

	OSDlog(Color_StrongGreen, true, "Loading savestate from slot %d...%s", slot, isFromBackup ? " (backup)" : "");
	Console.Indent().WriteLn(Color_StrongGreen, "filename: %ls", WX_STR(file));

	StateCopy_LoadFromFile(file);
#ifdef USE_NEW_SAVESLOTS_UI
	UI_UpdateSysControls();
#endif
}

