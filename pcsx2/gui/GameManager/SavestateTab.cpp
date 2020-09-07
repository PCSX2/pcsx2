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

#include "PrecompiledHeader.h"

#include "SavestateTab.h"

#include "App.h"
#include "Saveslots.h"

SavestateTab::SavestateTab(wxWindow* parent, AppConfig::GameManagerOptions& options)
	: wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL)
	, options(options)
{
	SetScrollRate(10, 10);
	// There will always be a minimum of 'StateSlotsCount' slots and each slot can have a backup
	savestateContainer = new wxFlexGridSizer(StateSlotsCount * 2, 1, 0, 0);

	for (Saveslot& slot : saveslot_cache)
	{
		SavestateSlotRow newSlot = SavestateSlotRow();
		newSlot.current = new SavestateSlotPanel(this, options, slot.slot_num, false, slot.updated, slot.empty);
		newSlot.backup = new SavestateSlotPanel(this, options, slot.slot_num, true, slot.updated, slot.empty, !options.DisplaySingleBackup || States_GetCurrentSlot() == slot.slot_num);
		savestates.push_back(newSlot);
		savestateContainer->Add(newSlot.current, 1, wxEXPAND, 0);
		savestateContainer->Add(newSlot.backup, 1, wxEXPAND, 0);
	}

	// TODO - I don't think the current UI handles the case where
	// if save-states are manually deleted from the file-system
	// with this, we could handle that edge-case.
	fsWatcher = new wxFileSystemWatcher(g_Conf->Folders.Savestates.GetFilename());
	fsWatcher->Bind(wxEVT_FSWATCHER, &SavestateTab::SavestateFolderModified, this);

	savestateContainer->AddGrowableCol(0);
	SetSizer(savestateContainer);

	refreshSlots();
}

void SavestateTab::resizeSlotWidth(int newWidth)
{
	for (SavestateSlotRow& slotRow : savestates)
	{
		slotRow.current->SetMaxSize(wxSize(newWidth, -1));
		slotRow.backup->SetMaxSize(wxSize(newWidth, -1));
	}
}


void SavestateTab::changeSelectedSlot(int slotNum)
{
	if (slotNum >= (int)savestates.size())
		return;

	if (options.DisplaySingleBackup)
		savestates.at(selectedSlot).backup->Show(false);
	savestates.at(selectedSlot).current->selectSlot(false);
	savestates.at(selectedSlot).backup->selectSlot(false);
	selectedSlot = slotNum;
	savestates.at(selectedSlot).current->selectSlot(true);
	savestates.at(selectedSlot).backup->selectSlot(true);
	if (options.DisplaySingleBackup)
		savestates.at(selectedSlot).backup->Show(true);
	savestateContainer->Layout(); // resize wxFlexGridSizer row heights to accomodate potential new images
	SendSizeEvent();              // force scrollbars to render
	// NOTE - Can be a generic function for scrolling an element into position
	int scroll_rate_y = 0;
	GetScrollPixelsPerUnit(NULL, &scroll_rate_y);
	wxPoint window_pos = CalcUnscrolledPosition(savestates.at(selectedSlot).current->GetPosition());
	Scroll(0, window_pos.y / scroll_rate_y);
}

void SavestateTab::unhighlightSlots()
{
	for (SavestateSlotRow& slotRow : savestates)
	{
		slotRow.current->unhighlightAndCollapse();
		slotRow.backup->unhighlightAndCollapse();
	}
}

void SavestateTab::refreshSlots()
{
	savestateContainer->Layout(); // resize wxFlexGridSizer row heights to accomodate potential new images
	SendSizeEvent();              // force scrollbars to render
}

void SavestateTab::updateSlot(int slotNum, wxDateTime updatedAt, bool isEmpty, bool fullReload)
{
	if (slotNum >= (int)savestates.size())
		return;
	savestates.at(slotNum).current->setTimestamp(updatedAt);
	savestates.at(slotNum).current->setIsEmpty(isEmpty);
	savestates.at(slotNum).backup->setIsEmpty(!States_SlotHasBackup(slotNum));
	updateSlotLabel(slotNum, updatedAt, isEmpty);
	updateSlotImagePreview(slotNum);
	// If we are updating all slots (ie. loading an ISO fresh)
	// we should be updating all widgets and only refreshing the window _once_ at the end to maximize performance.
	if (!fullReload || slotNum >= StateSlotsCount - 1)
	{
		savestateContainer->Layout(); // resize wxFlexGridSizer row heights to accomodate potential new images
		SendSizeEvent();              // force scrollbars to render
	}
}

void SavestateTab::updateSlotLabel(int slotNum, wxDateTime updatedAt, bool isEmpty)
{
	if (slotNum >= (int)savestates.size())
		return;
	savestates.at(slotNum).current->updateLabel();
	savestates.at(slotNum).backup->updateLabel();
}

void SavestateTab::updateSlotImagePreview(int slotNum)
{
	if (slotNum >= (int)savestates.size())
		return;
	savestates.at(slotNum).current->updatePreview();
	savestates.at(slotNum).backup->updatePreview();

	savestateContainer->Layout(); // resize wxFlexGridSizer row heights to accomodate potential new images
	SendSizeEvent();              // force scrollbars to render
}

void SavestateTab::updateBackupSlotVisibility()
{
	for (int i = 0; i < (int)savestates.size(); i++)
		savestates.at(i).backup->Show(!options.DisplaySingleBackup || i == selectedSlot);
}

int SavestateTab::GetSaveslotFromFilename(wxString fileName)
{
	// Filename Expectation - `SERIAL-NUM (CRC).SLOTNUM[.backup].EXTENSION`
	wxStringTokenizer tokenizer(fileName, ".");
	if (tokenizer.CountTokens() < 3)
		return -1;
	std::vector<wxString> tokens;
	while (tokenizer.HasMoreTokens())
		tokens.push_back(tokenizer.GetNextToken());
	long slotNum;
	if (!tokens.at(1).ToLong(&slotNum))
		return -1;
	return slotNum;
}

void SavestateTab::SavestateFolderModified(wxFileSystemWatcherEvent& event)
{
	wxString fileName = event.GetPath().GetFullName();
	// Only interested in new images
	if (event.GetChangeType() == wxFSW_EVENT_CREATE && fileName.EndsWith(".png"))
	{
		int slotNum = GetSaveslotFromFilename(fileName);
		if (slotNum != -1)
			updateSlotImagePreview(slotNum);
	}
}
