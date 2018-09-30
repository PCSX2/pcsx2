/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2018  PCSX2 Dev Team
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

#include "PS2Edefs.h"
#include "System.h"
#include "Elfheader.h"

//#define USE_NEW_SAVESLOTS_UI

class Saveslot
{
public:
	u32 slot_num;
	bool empty;
	wxDateTime updated;
	u32 crc;

	Saveslot()
	{
		slot_num = 0;
		empty = true;
		updated = wxInvalidDateTime;
		crc = ElfCRC;
	}

	Saveslot(int i)
	{
		slot_num = i;
		empty = true;
		updated = wxInvalidDateTime;
		crc = ElfCRC;
	}

	bool isUsed()
	{
		return wxFileExists(SaveStateBase::GetFilename(slot_num));
	}

	wxDateTime GetTimestamp()
	{
		if (!isUsed()) return wxInvalidDateTime;

		return wxDateTime(wxFileModificationTime(SaveStateBase::GetFilename(slot_num)));
	}

	void UpdateCache()
	{
		empty = !isUsed();
		updated = GetTimestamp();
		crc = ElfCRC;
	}

	wxString SlotName()
	{
		if (empty) return wxsFormat(_("Slot %d - Empty"), slot_num);

		if (updated.IsValid()) return wxsFormat(_("Slot %d - %s %s"), slot_num, updated.FormatDate(), updated.FormatTime());

		return wxsFormat(_("Slot %d - Unknown Time"), slot_num);
	}

	void ConsoleDump()
	{
		Console.WriteLn("Slot %i information:", slot_num);
		Console.WriteLn("Internal CRC = %i; Current CRC = %i.", crc, ElfCRC);
		if (empty)
			Console.WriteLn("Slot cache says it is empty.");
		else
			Console.WriteLn("Slot cache says it is used.");

		if (updated != wxInvalidDateTime)
			Console.WriteLn(wxsFormat(_("Write time is %s %s."), updated.FormatDate(), updated.FormatTime()));

		if (isUsed())
			Console.WriteLn(wxsFormat(_("The disk has a file on it dated %s %s."), GetTimestamp().FormatDate(), GetTimestamp().FormatTime()));
	}
};

extern Saveslot saveslot_cache[10];
extern void States_DefrostCurrentSlotBackup();
extern void States_DefrostCurrentSlot();
extern void States_FreezeCurrentSlot();
extern void States_CycleSlotForward();
extern void States_CycleSlotBackward();
extern void States_SetCurrentSlot(int slot);
extern int States_GetCurrentSlot();
extern void Sstates_updateLoadBackupMenuItem(bool isBeforeSave);