/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2020  PCSX2 Dev Team
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

#include "SavestateSlotPanel.h"

#include <vector>

#include "AppConfig.h"

#include "wx/wx.h"
#include "wx/fswatcher.h"

class SavestateTab : public wxScrolledWindow
{
public:
	SavestateTab(wxWindow* parent, AppConfig::GameManagerOptions& options);
	virtual ~SavestateTab() = default;

	void changeSelectedSlot(int slotNum);
	void unhighlightSlots();
	void refreshSlots();
	void updateSlot(int slotNum, wxDateTime updatedAt, bool isEmpty, bool fullReload);
	void updateSlotLabel(int slotNum, wxDateTime updatedAt, bool isEmpty);
	void updateSlotImagePreview(int slotNum);
	void updateBackupSlotVisibility();

private:
	struct SavestateSlotRow
	{
		SavestateSlotPanel* current;
		SavestateSlotPanel* backup;
	};

	AppConfig::GameManagerOptions& options;

	wxFileSystemWatcher* fsWatcher;
	int selectedSlot = 0;
	void SavestateFolderModified(wxFileSystemWatcherEvent& event);
	int GetSaveslotFromFilename(wxString fileName);

	wxFlexGridSizer* savestateContainer;
	std::vector<SavestateSlotRow> savestates;
};
