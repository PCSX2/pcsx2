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
#include "App.h"
#include "AppSaveStates.h"
#include "ConsoleLogger.h"
#include "MainFrame.h"

#include "Common.h"

#include "GS.h"
#include "Elfheader.h"
#include "Saveslots.h"

// --------------------------------------------------------------------------------------
//  Saveslot Section
// --------------------------------------------------------------------------------------

static int StatesC = 0;
static const int StateSlotsCount = 10;

#ifdef USE_NEW_SAVESLOTS_UI
Saveslot saveslot_cache[10] = {{0}, {1}, {2}, {3}, {4}, {5}, {6}, {7}, {8}, {9}};
#endif

// FIXME : Use of the IsSavingOrLoading flag is mostly a hack until we implement a
// complete thread to manage queuing savestate tasks, and zipping states to disk.  --air
static std::atomic<bool> IsSavingOrLoading(false);

class SysExecEvent_ClearSavingLoadingFlag : public SysExecEvent
{
public:
	wxString GetEventName() const { return L"ClearSavingLoadingFlag"; }

	virtual ~SysExecEvent_ClearSavingLoadingFlag() = default;
	SysExecEvent_ClearSavingLoadingFlag() { }
	SysExecEvent_ClearSavingLoadingFlag *Clone() const { return new SysExecEvent_ClearSavingLoadingFlag(); }

protected:
	void InvokeEvent()
	{
		IsSavingOrLoading = false;
		UI_UpdateSysControls();
	}
};

void Sstates_updateLoadBackupMenuItem(bool isBeforeSave);

void States_FreezeCurrentSlot()
{
	// FIXME : Use of the IsSavingOrLoading flag is mostly a hack until we implement a
	// complete thread to manage queuing savestate tasks, and zipping states to disk.  --air
	if (!SysHasValidState())
	{
		Console.WriteLn("Save state: Aborting (VM is not active).");
		return;
	}

	if (wxGetApp().HasPendingSaves() || IsSavingOrLoading.exchange(true))
	{
		Console.WriteLn("Load or save action is already pending.");
		return;
	}
	Sstates_updateLoadBackupMenuItem(true);

	GSchangeSaveState(StatesC, SaveStateBase::GetFilename(StatesC).ToUTF8());
	StateCopy_SaveToSlot(StatesC);

#ifdef USE_NEW_SAVESLOTS_UI
	// Update the saveslot cache with the new saveslot, and give it the current timestamp, 
	// Because we aren't going to be able to get the real timestamp from disk right now.
	saveslot_cache[StatesC].empty = false;
	saveslot_cache[StatesC].updated = wxDateTime::Now();
	saveslot_cache[StatesC].crc = ElfCRC;

	// Update the slot next time we run through the UI update.
	saveslot_cache[StatesC].menu_update = true;
#endif

	GetSysExecutorThread().PostIdleEvent(SysExecEvent_ClearSavingLoadingFlag());
}

void _States_DefrostCurrentSlot(bool isFromBackup)
{
	if (!SysHasValidState())
	{
		Console.WriteLn("Load state: Aborting (VM is not active).");
		return;
	}

	if (IsSavingOrLoading.exchange(true))
	{
		Console.WriteLn("Load or save action is already pending.");
		return;
	}

	GSchangeSaveState(StatesC, SaveStateBase::GetFilename(StatesC).ToUTF8());
	StateCopy_LoadFromSlot(StatesC, isFromBackup);

	GetSysExecutorThread().PostIdleEvent(SysExecEvent_ClearSavingLoadingFlag());

	Sstates_updateLoadBackupMenuItem(false);
}

void States_DefrostCurrentSlot()
{
	_States_DefrostCurrentSlot(false);
}

void States_DefrostCurrentSlotBackup()
{
	_States_DefrostCurrentSlot(true);
}

void States_UpdateSaveslotMenu()
{
#ifdef USE_NEW_SAVESLOTS_UI
	// Run though all the slots.Update if they need updating or the crc changed.
	for (int i = 0; i < 10; i++)
	{
		int load_menu_item = MenuId_State_Load01 + i + 1;
		int save_menu_item = MenuId_State_Save01 + i + 1;
		
		// We need to reload the file information if the crc changed.
		if (saveslot_cache[i].crc != ElfCRC) saveslot_cache[i].invalid_cache = true;

		// Either the cache needs updating, or the menu items do, or both.
		if (saveslot_cache[i].menu_update || saveslot_cache[i].invalid_cache)
		{
			#ifdef SAVESLOT_LOGS
			Console.WriteLn("Updating slot %i.", i);
			if (saveslot_cache[i].menu_update) Console.WriteLn("Menu update needed.");
			if (saveslot_cache[i].invalid_cache) Console.WriteLn("Invalid cache. (CRC different or just initialized.)");
			#endif

			if (saveslot_cache[i].invalid_cache)
			{
				// Pull everything from disk.
				saveslot_cache[i].UpdateCache();

				#ifdef SAVESLOT_LOGS
				saveslot_cache[i].ConsoleDump();
				#endif
			}

			// Update from the cached information.
			saveslot_cache[i].menu_update = false;
			saveslot_cache[i].crc = ElfCRC;

			sMainFrame.EnableMenuItem(load_menu_item, !saveslot_cache[i].empty);
			sMainFrame.SetMenuItemLabel(load_menu_item, saveslot_cache[i].SlotName());
			sMainFrame.SetMenuItemLabel(save_menu_item, saveslot_cache[i].SlotName());
		}

	}
	Sstates_updateLoadBackupMenuItem(false);
#endif
}

void Sstates_updateLoadBackupMenuItem(bool isBeforeSave)
{
	wxString file = SaveStateBase::GetFilename(StatesC);

	if (!(isBeforeSave && g_Conf->EmuOptions.BackupSavestate))
	{
		file = file + L".backup";
	}

	sMainFrame.EnableMenuItem(MenuId_State_LoadBackup, wxFileExists(file));
	sMainFrame.SetMenuItemLabel(MenuId_State_LoadBackup, wxsFormat(L"%s %d", _("Backup"), StatesC));
}

static void OnSlotChanged()
{
	OSDlog(Color_StrongGreen, true, " > Selected savestate slot %d", StatesC);

	if (GSchangeSaveState != NULL)
		GSchangeSaveState(StatesC, SaveStateBase::GetFilename(StatesC).utf8_str());

	Sstates_updateLoadBackupMenuItem(false);
}

int States_GetCurrentSlot()
{
	return StatesC;
}

void States_SetCurrentSlot(int slot)
{
	StatesC = std::min(std::max(slot, 0), StateSlotsCount);
	OnSlotChanged();
}

void States_CycleSlotForward()
{
	StatesC = (StatesC + 1) % StateSlotsCount;
	OnSlotChanged();
}

void States_CycleSlotBackward()
{
	StatesC = (StatesC + StateSlotsCount - 1) % StateSlotsCount;
	OnSlotChanged();
}
