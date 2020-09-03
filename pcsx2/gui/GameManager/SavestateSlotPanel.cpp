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

#include "SavestateSlotPanel.h"

#include "App.h"
#include "Saveslots.h"
#include "Util.h"

// TODO - Known Issues
// - Loading all bitmaps is a little slow, and it probably shouldn't be, need to track down where the time-waste is
//   - I think the reason is because it seems as if the images are rendered/loaded in serial, not sure if there is a way to do this async / threaded
//     but you can't really preload the save-state previews until a game is selected.  Another option might be to use the OnIdle() event, once
//     all slots have been loaded, refresh.  But this probably just opens up a larger can of worms.
//     - https://forums.wxwidgets.org/viewtopic.php?t=20681
// - I'd like to add savestate version and BIOS information, but havn't figured out how to extract that info _yet_
// - Likely some unneeded flickering, probably refresh'ing/layout'ing too much
SavestateSlotPanel::SavestateSlotPanel(wxWindow* parent, AppConfig::GameManagerOptions& options, int slot, bool isBackup, wxDateTime updatedAt, bool isEmpty, bool isShown)
	: wxPanel(parent, wxID_ANY)
	, options(options)
	, slot(slot)
	, backup(isBackup)
{
	// Determine if the user's system color theme uses light or dark text.
	// Since the GameManager highlights rows via background color changing
	// the color must be adjusted according the user's text color.
	wxColour clr = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
	GuiUtilities::HSLColor hsl = GuiUtilities::rgbToHsl(clr);
	useDarkColours = hsl.lightness > 200;

	expanded = false;
	this->updatedAt = updatedAt;
	this->isEmpty = isEmpty;
	initComponents();
	Show(isShown);
}

void SavestateSlotPanel::initComponents()
{
	// Sizers
	collapsedSizer = new wxBoxSizer(wxHORIZONTAL);
	expandedSizer = new wxBoxSizer(wxHORIZONTAL);
	expandedMetadataSizer = new wxFlexGridSizer(2, 1, 0, 5);
	expandedMetadataSizer->AddGrowableCol(0);

	// Widget Creation
	initPreview();
	collapsedLabel = new wxStaticText(this, wxID_ANY, getLabel(false));
	expandedLabel = new wxStaticText(this, wxID_ANY, getLabel(true));
	
	collapsedBackupLeftPad = new wxStaticText(this, wxID_ANY, "");
	expandedBackupLeftPad = new wxStaticText(this, wxID_ANY, "");

	// Add to Sizers
	if (backup)
	{
		collapsedSizer->Add(collapsedBackupLeftPad, -1, wxLEFT, 15);
		expandedSizer->Add(expandedBackupLeftPad, -1, wxLEFT, 15);
	}
	collapsedSizer->Add(collapsedLabel, 2, wxALIGN_CENTER_VERTICAL | wxALL, 10);
	collapsedSizer->Add(collapsedPreview, 1, wxALL | wxEXPAND, 5);
	expandedMetadataSizer->Add(expandedLabel, 0, 0, 0);
	if (!backup)
	{
		expandedTimestamp = new wxStaticText(this, wxID_ANY, getTimestamp());
		expandedMetadataSizer->Add(expandedTimestamp, 0, 0, 0);
	}	
	expandedSizer->Add(expandedMetadataSizer, 1, wxALIGN_CENTER_VERTICAL | wxALL, 10);
	expandedSizer->Add(expandedPreview, 1, wxALL | wxEXPAND, 5);

	// Background Coloring
	if (States_GetCurrentSlot() == slot)
	{
		slotSelected = true;
		SetBackgroundColour(useDarkColours ? slotSelectedColourDark : slotSelectedColour);
	}

	// Bind Events
	if (backup)
		bindClickEvents({this, collapsedLabel, collapsedBackupLeftPad, collapsedPreview, expandedLabel, expandedBackupLeftPad, expandedPreview});
	else
		bindClickEvents({this, collapsedLabel, collapsedPreview, expandedLabel, expandedTimestamp, expandedPreview});

	// TODO - since i now require this...need to make resize events from the parent window update it
	SetMaxSize(wxSize(options.DisplaySize.GetWidth(), -1));
	// Display Correct Sizer
	SetSizer(expanded ? expandedSizer : collapsedSizer, false);
	collapsedSizer->Show(!expanded);
	expandedSizer->Show(expanded);
}

// TODO - confirm if wxGenericStaticBitmap causes issues on linux
void SavestateSlotPanel::initPreview(bool update)
{
	if (!update)
	{
		// Linux SegFault Especially - Must be initialized before performing operations
		collapsedPreview = new wxGenericStaticBitmap(this, wxID_ANY, wxBitmap());
		expandedPreview = new wxGenericStaticBitmap(this, wxID_ANY, wxBitmap());
	}
	if (wxFileExists(States_SlotImagePreviewPath(slot, backup)))
	{
		wxImage img;
		if (img.LoadFile(States_SlotImagePreviewPath(slot, backup)) && img.IsOk())
		{
			img.Rescale(baseImageX, baseImageY, wxIMAGE_QUALITY_HIGH);
			collapsedPreview->SetMinSize(img.GetSize());
			collapsedPreview->SetBitmap(wxBitmap(img, wxBITMAP_TYPE_PNG));
		}
		// Get the original file again so we are scaling from the original size
		if (img.LoadFile(States_SlotImagePreviewPath(slot, backup)) && img.IsOk())
		{
			img.Rescale(baseImageX * expandedPreviewScaleFactor, baseImageY * expandedPreviewScaleFactor, wxIMAGE_QUALITY_HIGH);
			expandedPreview->SetMinSize(img.GetSize());
			expandedPreview->SetBitmap(wxBitmap(img, wxBITMAP_TYPE_PNG));
		}
	}
}

void SavestateSlotPanel::bindClickEvents(std::vector<wxWindow*> args)
{
	for (wxWindow* arg : args)
	{
		arg->Bind(wxEVT_LEFT_DOWN, &SavestateSlotPanel::panelItemClicked, this);
		arg->Bind(wxEVT_LEFT_DCLICK, &SavestateSlotPanel::panelItemDoubleClicked, this);
	}
}

void SavestateSlotPanel::panelItemClicked(wxMouseEvent& evt)
{
	// Deselect all other panels
	wxGetApp().GetGameManagerFramePtr()->getSavestateTab()->unhighlightSlots();
	SetBackgroundColour(useDarkColours ? slotHighlightedColourDark : slotHighlightedColour);
	expanded = true;
	SetSizer(expandedSizer, false);
	collapsedSizer->Show(!expanded);
	expandedSizer->Show(expanded);
	Refresh();
	wxGetApp().GetGameManagerFramePtr()->getSavestateTab()->refreshSlots();
}

// TODO - forbid this if no game is active... 
// though Saveslots.cpp seems to already handle it, maybe its the SetCurrentSlot that's the problem
void SavestateSlotPanel::panelItemDoubleClicked(wxMouseEvent& evt)
{
	wxGetApp().GetGameManagerFramePtr()->getSavestateTab()->unhighlightSlots();
	States_SetCurrentSlot(slot);
	backup ? States_DefrostCurrentSlotBackup() : States_DefrostCurrentSlot();
	Refresh();
}

void SavestateSlotPanel::setTimestamp(wxDateTime updatedAt)
{
	this->updatedAt = updatedAt;
}

void SavestateSlotPanel::setIsEmpty(bool isEmpty)
{
	this->isEmpty = isEmpty;
}

wxString SavestateSlotPanel::getLabel(bool expandedVersion)
{
	if (!backup && isEmpty)
		return wxString::Format("Slot %d - Empty", slot);
	if (backup && isEmpty)
		return wxString::Format("Backup - Empty");
	if (backup)
		return wxString::Format("Backup");
	if (!updatedAt.IsValid() || expandedVersion)
		return wxString::Format("Slot %d", slot);
	return wxString::Format("Slot %d - %s", slot, getTimestamp());
}

wxString SavestateSlotPanel::getTimestamp()
{
	if (updatedAt.IsValid())
		return wxString::Format("%s %s", updatedAt.FormatDate(), updatedAt.FormatTime());
	return wxString("Unknown Time");
}

void SavestateSlotPanel::updateLabel()
{
	bool changeDetected = false;
	wxString newCollapsedLabel = getLabel(false);
	wxString newExpandedLabel = getLabel(true);
	wxString newExpandedTimestamp = getTimestamp();
	changeDetected = !newCollapsedLabel.IsSameAs(collapsedLabel->GetLabel()) ||
					 !newExpandedLabel.IsSameAs(expandedLabel->GetLabel()) ||
					 (!backup && !newExpandedTimestamp.IsSameAs(expandedTimestamp->GetLabel()));

	if (changeDetected)
	{
		collapsedLabel->SetLabel(newCollapsedLabel);
		expandedLabel->SetLabel(newExpandedLabel);
		if (!backup)
			expandedTimestamp->SetLabel(newExpandedTimestamp);
		Refresh();
	}
}

void SavestateSlotPanel::updatePreview()
{
	if (isEmpty)
		return; // Don't render a screenshot for an empty slot, even if it exists
	initPreview(true);
	Refresh();
}

void SavestateSlotPanel::selectSlot(bool selected)
{
	wxGetApp().GetGameManagerFramePtr()->getSavestateTab()->unhighlightSlots();
	slotSelected = selected;
	if (selected)
		SetBackgroundColour(useDarkColours ? slotSelectedColourDark : slotSelectedColour);
	else
		SetBackgroundColour(wxNullColour);
	Refresh();
}

void SavestateSlotPanel::unhighlightAndCollapse()
{
	bool changeDetected = false;
	if (slotSelected)
	{
		changeDetected = GetBackgroundColour() != slotSelectedColour;
		SetBackgroundColour(useDarkColours ? slotSelectedColourDark : slotSelectedColour);
	}
	else
	{
		changeDetected = GetBackgroundColour() != wxNullColour;
		SetBackgroundColour(wxNullColour);
	}

	if (expanded)
	{
		expanded = !expanded;
		SetSizer(expanded ? expandedSizer : collapsedSizer, false);
		collapsedSizer->Show(!expanded);
		expandedSizer->Show(expanded);
		Refresh();
	}
	else if (changeDetected)
	{
		Refresh();
	}
}
