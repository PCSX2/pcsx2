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

#ifdef USE_NEW_SAVESLOTS_UI
std::array<Saveslot,StateSlotsCount> saveslot_cache = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
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

	StateCopy_SaveToSlot(StatesC);

#ifdef USE_NEW_SAVESLOTS_UI
	saveslot_cache[StatesC].Used();
#endif

	GetSysExecutorThread().PostIdleEvent(SysExecEvent_ClearSavingLoadingFlag());

	States_updateLoadBackupMenuItem();
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

	StateCopy_LoadFromSlot(StatesC, isFromBackup);

	GetSysExecutorThread().PostIdleEvent(SysExecEvent_ClearSavingLoadingFlag());

	States_updateLoadBackupMenuItem();
}

void States_DefrostCurrentSlot()
{
	_States_DefrostCurrentSlot(false);
}

void States_DefrostCurrentSlotBackup()
{
	_States_DefrostCurrentSlot(true);
}

void States_updateLoadBackupMenuItem()
{
	wxString file(StringUtil::UTF8StringToWxString(SaveStateBase::GetSavestateFolder(StatesC) + ".backup"));

	sMainFrame.EnableMenuItem(MenuId_State_LoadBackup, wxFileExists(file));
	sMainFrame.SetMenuItemLabel(MenuId_State_LoadBackup, wxsFormat(L"%s %d", _("Backup"), StatesC));
}

static void OnSlotChanged()
{
	OSDlog(Color_StrongGreen, true, " > Selected savestate slot %d", StatesC);
	States_updateLoadBackupMenuItem();
}

int States_GetCurrentSlot()
{
	return StatesC;
}

void States_SetCurrentSlot(int slot_num)
{
	StatesC = std::min(std::max(slot_num, 0), StateSlotsCount);
	for (Saveslot& slot : saveslot_cache)
	{
		sMainFrame.CheckMenuItem(slot.load_item_id, slot.slot_num == slot_num);
		sMainFrame.CheckMenuItem(slot.save_item_id, slot.slot_num == slot_num);
	}
	OnSlotChanged();
}

void States_CycleSlotForward()
{
	States_SetCurrentSlot((StatesC + 1) % StateSlotsCount);
}

void States_CycleSlotBackward()
{
	States_SetCurrentSlot((StatesC + StateSlotsCount - 1) % StateSlotsCount);
}
