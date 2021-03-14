/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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

#include <wx/dc.h>
#include <wx/grid.h>
#include <wx/numdlg.h>
#include <wx/renderer.h>

#include "App.h"
#include "InputRecordingViewer.h"
#include "RecordingMetadataDialog.h"
#include "Utilities/EmbeddedImage.h"

wxMenuItem* enableMenuItem(wxMenuItem* item, bool enabled)
{
	item->Enable(enabled);
	return item;
}

wxMenuItem* checkMenuItem(wxMenuItem* item, bool checked)
{
	item->Check(checked);
	return item;
}

InputRecordingViewer::InputRecordingViewer(wxWindow* parent, AppConfig::InputRecordingOptions& options)
	: wxFrame(parent, wxID_ANY, _("Recording Viewer"), wxDefaultPosition, wxDefaultSize)
	, options(options)
{
	// Init Menus
	menuBar = new wxMenuBar();
	fileMenu = new wxMenu();
	openMenuItem = enableMenuItem(fileMenu->Append(wxID_ANY, _("Open"), _("Open an Input Recording file to view/edit")), true);
	closeMenuItem = enableMenuItem(fileMenu->Append(wxID_ANY, _("Close"), _("Close the current file")), false);
	fileMenu->AppendSeparator();
	saveMenuItem = enableMenuItem(fileMenu->Append(wxID_ANY, _("Save"), _("Save the current file, overwriting the original")), false);
	saveAsMenuItem = enableMenuItem(fileMenu->Append(wxID_ANY, _("Save As"), _("Save the current file to a specified location")), false);
	// TODO - implement Exporting!
	// fileMenu->AppendSeparator();
	// importMenuItem = enableMenuItem(fileMenu->Append(wxID_ANY, _("Import From YAML"), _("Import recording data from a YAML file")), false);
	// exportMenuItem = enableMenuItem(fileMenu->Append(wxID_ANY, _("Export As YAML"), _("Export recording data to a YAML file")), false);
	menuBar->Append(fileMenu, _("File"));

	editMenu = new wxMenu();
	changeMetadataMenuItem = enableMenuItem(editMenu->Append(wxID_ANY, _("Change Metadata"), _("Change the recordings relevant metadata")), false);
	// TODO - implement these at a later date
	// changeRecordingTypeMenuItem = enableMenuItem(editMenu->Append(wxID_ANY, _("Change Recording Type"), _("Change the recording's type (ie. power-on / save-state)")), false);
	// changeBaseSavestateMenuItem = enableMenuItem(editMenu->Append(wxID_ANY, _("Change Base Savestate"), _("Change the base savestate for the recording")), false);
	menuBar->Append(editMenu, _("Edit"));

	viewMenu = new wxMenu();
	wxMenu* columnMenu = new wxMenu();
	showAnalogSticksMenuItem = checkMenuItem(columnMenu->Append(wxID_ANY, _("Analog Sticks"), _("Show/hide the analog stick columns"), wxITEM_CHECK), options.ViewerShowAnalogSticks);
	showFaceButtonsMenuItem = checkMenuItem(columnMenu->Append(wxID_ANY, _("Face Buttons"), _("Show/hide the face button columns"), wxITEM_CHECK), options.ViewerShowFaceButtons);
	showDirectionalPadMenuItem = checkMenuItem(columnMenu->Append(wxID_ANY, _("Directional Pad"), _("Show/hide the D-Pad columns"), wxITEM_CHECK), options.ViewerShowDirectionalPad);
	showShoulderButtonsMenuItem = checkMenuItem(columnMenu->Append(wxID_ANY, _("Shoulder Buttons"), _("Show/hide the shoulder button columns"), wxITEM_CHECK), options.ViewerShowShoulderButtons);
	showMiscButtonsMenuItem = checkMenuItem(columnMenu->Append(wxID_ANY, _("Miscellaneous Buttons"), _("Show/hide the remaining miscellaneous columns"), wxITEM_CHECK), options.ViewerShowMiscButtons);
	showColumnsSubmenu = enableMenuItem(viewMenu->AppendSubMenu(columnMenu, _("Show Columns"), _("Show/hide sections of controller data")), true);
	viewMenu->AppendSeparator();
	jumpToFrameMenuItem = enableMenuItem(viewMenu->Append(wxID_ANY, _("Jump to Frame"), _("Jump to a specific frame")), false);
	viewMenu->AppendSeparator();
	wxMenu* controllerMenu = new wxMenu();
	portOneMenuItem = checkMenuItem(controllerMenu->Append(wxID_ANY, _("Port 1"), _("Select port 1"), wxITEM_CHECK), true);
	portTwoMenuItem = checkMenuItem(controllerMenu->Append(wxID_ANY, _("Port 2"), _("Select port 2"), wxITEM_CHECK), false);
	controllerPortSubmenu = enableMenuItem(viewMenu->AppendSubMenu(controllerMenu, _("Change Controller"), _("Switch which controller port is active")), false);
	menuBar->Append(viewMenu, _("View"));

	dataMenu = new wxMenu();
	clearFrameMenuItem = enableMenuItem(dataMenu->Append(wxID_ANY, _("Clear Frame"), _("Clear the entire frame of data")), false);
	defaultFrameMenuItem = enableMenuItem(dataMenu->Append(wxID_ANY, _("Default Frame"), _("Set the frame to the controllers default neutral values")), false);
	duplicateFrameMenuItem = enableMenuItem(dataMenu->Append(wxID_ANY, _("Duplicate Frame"), _("Duplicate and insert the frame")), false);
	insertFrameMenuItem = enableMenuItem(dataMenu->Append(wxID_ANY, _("Insert Frame"), _("Insert a new default frame")), false);
	insertFramesMenuItem = enableMenuItem(dataMenu->Append(wxID_ANY, _("Insert Frame(s)"), _("Insert multiple default frames")), false);
	removeFrameMenuItem = enableMenuItem(dataMenu->Append(wxID_ANY, _("Remove Frame"), _("Remove a frame")), false);
	removeFramesMenuItem = enableMenuItem(dataMenu->Append(wxID_ANY, _("Remove Frame(s)"), _("Remove multiple frames")), false);
	// TODO - implement Data menu and event handlers
	// menuBar->Append(dataMenu, _("Data"));

	// Init Widgets
	recordingDataSource = new RecordingFileGridTable(NUM_COLUMNS);
	recordingGrid = new wxGrid(this, wxID_ANY);
	recordingGrid->SetTable(recordingDataSource, true);
	recordingGrid->EnableDragColSize(false);
	recordingGrid->EnableDragRowSize(false);
	recordingGrid->EnableDragGridSize(false);
	recordingGrid->SetColSize(0, 100);
	recordingGrid->SetColSize(1, 100);
	recordingGrid->SetColSize(2, 100);
	recordingGrid->SetColSize(3, 100);
	recordingGrid->SetColLabelSize(75);

	// Bind Events
	Bind(wxEVT_CLOSE_WINDOW, &InputRecordingViewer::OnCloseWindow, this);
	Bind(wxEVT_MOVE, &InputRecordingViewer::OnMoveAround, this);

	Bind(wxEVT_MENU, &InputRecordingViewer::OnOpenFile, this, openMenuItem->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::OnCloseFile, this, closeMenuItem->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::OnSaveFile, this, saveMenuItem->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::OnSaveAsFile, this, saveAsMenuItem->GetId());
	// TODO - implement Exporting!
	// Bind(wxEVT_MENU, &InputRecordingViewer::OnImport, this, importMenuItem->GetId());
	// Bind(wxEVT_MENU, &InputRecordingViewer::OnExport, this, exportMenuItem->GetId());

	Bind(wxEVT_MENU, &InputRecordingViewer::OnChangeMetadata, this, changeMetadataMenuItem->GetId());
	// TODO - implement!
	// Bind(wxEVT_MENU, &InputRecordingViewer::OnChangeRecordingType, this, changeRecordingTypeMenuItem->GetId());
	// Bind(wxEVT_MENU, &InputRecordingViewer::OnChangeBaseSavestate, this, changeBaseSavestateMenuItem->GetId());

	Bind(wxEVT_MENU, &InputRecordingViewer::OnShowAnalogSticks, this, showAnalogSticksMenuItem->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::OnShowFaceButtons, this, showFaceButtonsMenuItem->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::OnShowDirectionalPad, this, showDirectionalPadMenuItem->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::OnShowShoulderButtons, this, showShoulderButtonsMenuItem->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::OnShowMiscButtons, this, showMiscButtonsMenuItem->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::OnJumpToFrame, this, jumpToFrameMenuItem->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::OnSelectPortOne, this, portOneMenuItem->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::OnSelectPortTwo, this, portTwoMenuItem->GetId());

	Bind(wxEVT_MENU, &InputRecordingViewer::OnClearFrame, this, clearFrameMenuItem->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::OnDefaultFrame, this, defaultFrameMenuItem->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::OnDuplicateFrame, this, duplicateFrameMenuItem->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::OnInsertFrame, this, insertFrameMenuItem->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::OnInsertFrames, this, insertFramesMenuItem->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::OnRemoveFrame, this, removeFrameMenuItem->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::OnRemoveFrames, this, removeFramesMenuItem->GetId());

	// Sizers
	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(recordingGrid, 1, wxEXPAND, 0);
	sizer->SetSizeHints(this);

	SetMenuBar(menuBar);
	SetIcons(wxGetApp().GetIconBundle());

	CreateStatusBar(1);

	SetPosition(options.ViewerPosition);
	SetSizer(sizer);
	SetMinSize(wxSize(500, 500));
	Layout();
	DisplayColumns();
}

void InputRecordingViewer::OnCloseWindow(wxCloseEvent& event)
{
	recordingDataSource->CloseRecordingFile();
	recordingGrid->ForceRefresh();
	wxRemoveFile(tempFilePath);
	ToggleMenuItems(false);
	Hide();
}

void InputRecordingViewer::OnMoveAround(wxMoveEvent& event)
{
	if (IsBeingDeleted() || !IsVisible() || IsIconized())
		return;

	if (!IsMaximized())
		options.ViewerPosition = GetPosition();
	event.Skip();
}

void InputRecordingViewer::ToggleMenuItems(bool fileOpen)
{
	openMenuItem->Enable(!fileOpen);
	closeMenuItem->Enable(fileOpen);
	saveMenuItem->Enable(fileOpen);
	saveAsMenuItem->Enable(fileOpen);
	// importMenuItem->Enable(fileOpen);
	// exportMenuItem->Enable(fileOpen);

	changeMetadataMenuItem->Enable(fileOpen);
	// changeRecordingTypeMenuItem->Enable(fileOpen);
	// changeBaseSavestateMenuItem->Enable(fileOpen);

	jumpToFrameMenuItem->Enable(fileOpen);
	controllerPortSubmenu->Enable(fileOpen);
	portOneMenuItem->Check(fileOpen && recordingDataSource->GetControllerPort() == 1);
	portTwoMenuItem->Check(fileOpen && recordingDataSource->GetControllerPort() == 2);

	clearFrameMenuItem->Enable(fileOpen);
	defaultFrameMenuItem->Enable(fileOpen);
	duplicateFrameMenuItem->Enable(fileOpen);
	insertFrameMenuItem->Enable(fileOpen);
	insertFramesMenuItem->Enable(fileOpen);
	removeFrameMenuItem->Enable(fileOpen);
	removeFramesMenuItem->Enable(fileOpen);
}

void InputRecordingViewer::DisplayColumns()
{
	for (int i = 0; i < 4; i++)
	{
		options.ViewerShowAnalogSticks ? recordingGrid->ShowCol(i) : recordingGrid->HideCol(i);
	}
	for (int i = 4; i < 8; i++)
	{
		options.ViewerShowFaceButtons ? recordingGrid->ShowCol(i) : recordingGrid->HideCol(i);
	}
	for (int i = 8; i < 12; i++)
	{
		options.ViewerShowDirectionalPad ? recordingGrid->ShowCol(i) : recordingGrid->HideCol(i);
	}
	for (int i = 12; i < 16; i++)
	{
		options.ViewerShowShoulderButtons ? recordingGrid->ShowCol(i) : recordingGrid->HideCol(i);
	}
	for (int i = 16; i < 20; i++)
	{
		options.ViewerShowMiscButtons ? recordingGrid->ShowCol(i) : recordingGrid->HideCol(i);
	}
}

void InputRecordingViewer::OnOpenFile(wxCommandEvent& event)
{
	wxFileDialog* openFileDialog =
		new wxFileDialog(this, _("Open Input Recording File"), wxEmptyString, wxEmptyString, "p2m2 file(*.p2m2)|*.p2m2",
						 wxFD_OPEN, wxDefaultPosition);

	if (openFileDialog->ShowModal() == wxID_OK)
	{
		// TODO - utility function
		filePath = openFileDialog->GetPath();
		// wxWidget's removes the extension if it contains wildcards
		// on wxGTK https://trac.wxwidgets.org/ticket/15285
		if (!filePath.EndsWith(".p2m2"))
			filePath = wxString::Format("%s.p2m2", filePath);

		if (!wxFileExists(filePath))
		{
			wxMessageBox(_("Unable to Open Input Recording File"), "Input Recording Viewer Error", wxOK | wxICON_ERROR, this);
			return;
		}

		// Copy the file, as this is out working copy if edits are made
		// The user can then choose to overwrite the existing file, or save elsewhere.
		// At which time, we remove the temp file
		tempFilePath = filePath + ".tmp";
		if (!wxCopyFile(filePath, tempFilePath, true))
		{
			wxMessageBox(_("Unable to Open Input Recording File"), "Input Recording Viewer Error", wxOK | wxICON_ERROR, this);
			return;
		}

		recordingDataSource->OpenRecordingFile(tempFilePath);
		recordingGrid->ForceRefresh();

		// Enable various menu options
		ToggleMenuItems(true);
	}
}

void InputRecordingViewer::OnCloseFile(wxCommandEvent& event)
{
	if (recordingDataSource->AreChangesUnsaved())
	{
		int answer = wxMessageBox(_("Close without saving changes?"), _("Confirm"),
								  wxYES_NO | wxCANCEL, this);
		if (answer != wxYES)
		{
			return;
		}
	}
	recordingDataSource->CloseRecordingFile();
	recordingGrid->ForceRefresh();
	wxRemoveFile(tempFilePath);
	ToggleMenuItems(false);
}

// TODO - a not on saving, currently we don't allow modifying the original save-state, so there are no concerns about it
// When we do though, these save functions will need to expand!

void InputRecordingViewer::OnSaveFile(wxCommandEvent& event)
{
	if (!wxCopyFile(tempFilePath, filePath + ".bak", true))
	{
		wxMessageBox(_("Unable to backup original recording before saving, aborting!"), "Input Recording Viewer Error", wxOK | wxICON_ERROR, this);
		return;
	}

	// Overwrite existing file, this is just a simple rename
	if (!wxCopyFile(tempFilePath, filePath, true))
	{
		wxMessageBox(_("Unable to Save Input Recording File"), "Input Recording Viewer Error", wxOK | wxICON_ERROR, this);
		return;
	}

	recordingDataSource->ClearUnsavedChanges();
}

void InputRecordingViewer::OnSaveAsFile(wxCommandEvent& event)
{
	wxFileDialog* openFileDialog =
		new wxFileDialog(this, _("Save Input Recording File"), wxEmptyString, wxEmptyString, "p2m2 file(*.p2m2)|*.p2m2",
						 wxFD_SAVE | wxFD_OVERWRITE_PROMPT, wxDefaultPosition);

	if (openFileDialog->ShowModal() == wxID_OK)
	{
		wxString savePath = openFileDialog->GetPath();
		// wxWidget's removes the extension if it contains wildcards
		// on wxGTK https://trac.wxwidgets.org/ticket/15285
		if (!savePath.EndsWith(".p2m2"))
			savePath = wxString::Format("%s.p2m2", savePath);

		if (savePath == filePath)
		{
			if (!wxCopyFile(tempFilePath, savePath + ".bak", true))
			{
				wxMessageBox(_("Unable to backup original recording before saving, aborting!"), "Input Recording Viewer Error", wxOK | wxICON_ERROR, this);
				return;
			}
		}

		if (!wxCopyFile(tempFilePath, savePath, true))
		{
			wxMessageBox(_("Unable to Save Input Recording File"), "Input Recording Viewer Error", wxOK | wxICON_ERROR, this);
			return;
		}

		// If it's a save-state recording, copy over the save-state as well
		if (recordingDataSource->IsFromSavestate())
		{
			if (wxFileExists(filePath + "_SaveState.p2s"))
			{
				if (savePath != filePath && !wxCopyFile(filePath + "_SaveState.p2s", savePath + "_SaveState.p2s", true))
				{
					wxMessageBox(_("Unable to Save Input Recording's SaveState!"), "Input Recording Viewer Error", wxOK | wxICON_ERROR, this);
				}
			}
			else
			{
				wxMessageBox(_("Unable to Save Input Recording's SaveState, not found!"), "Input Recording Viewer Error", wxOK | wxICON_ERROR, this);
			}
		}

		recordingDataSource->ClearUnsavedChanges();
	}
}

void InputRecordingViewer::OnImport(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::OnExport(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::OnChangeMetadata(wxCommandEvent& event)
{
	InputRecordingFileHeader currHeader = recordingDataSource->GetRecordingFileHeader();
	RecordingMetadataDialog* custom = new RecordingMetadataDialog(this, currHeader.author, currHeader.gameName, recordingDataSource->GetUndoCount());
	if (custom->ShowModal() == wxID_OK)
	{
		recordingDataSource->UpdateRecordingFileHeader(custom->GetAuthor(), custom->GetGameName());
		recordingDataSource->SetUndoCount(custom->GetUndoCount());
	}
}

void InputRecordingViewer::OnChangeRecordingType(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::OnChangeBaseSavestate(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::OnShowAnalogSticks(wxCommandEvent& event)
{
	options.ViewerShowAnalogSticks = showAnalogSticksMenuItem->IsChecked();
	DisplayColumns();
}

void InputRecordingViewer::OnShowFaceButtons(wxCommandEvent& event)
{
	options.ViewerShowFaceButtons = showFaceButtonsMenuItem->IsChecked();
	DisplayColumns();
}

void InputRecordingViewer::OnShowDirectionalPad(wxCommandEvent& event)
{
	options.ViewerShowDirectionalPad = showDirectionalPadMenuItem->IsChecked();
	DisplayColumns();
}

void InputRecordingViewer::OnShowShoulderButtons(wxCommandEvent& event)
{
	options.ViewerShowShoulderButtons = showShoulderButtonsMenuItem->IsChecked();
	DisplayColumns();
}

void InputRecordingViewer::OnShowMiscButtons(wxCommandEvent& event)
{
	options.ViewerShowMiscButtons = showMiscButtonsMenuItem->IsChecked();
	DisplayColumns();
}

void InputRecordingViewer::OnJumpToFrame(wxCommandEvent& event)
{
	long frame = wxGetNumberFromUser(_("Enter Frame Number"), wxEmptyString, _("Jump To Frame"), 0, 0, recordingDataSource->GetNumberRows(), this);
	frame = frame == 0 ? 0 : frame - 1;
	recordingGrid->SelectRow(frame);
	recordingGrid->MakeCellVisible(frame, 0);
}

void InputRecordingViewer::OnSelectPortOne(wxCommandEvent& event)
{
	bool selected = portOneMenuItem->IsChecked();
	portTwoMenuItem->Check(!selected);
	recordingDataSource->SetControllerPort(1);
	recordingGrid->ForceRefresh();
}

void InputRecordingViewer::OnSelectPortTwo(wxCommandEvent& event)
{
	bool selected = portTwoMenuItem->IsChecked();
	portOneMenuItem->Check(!selected);
	recordingDataSource->SetControllerPort(2);
	recordingGrid->ForceRefresh();
}

void InputRecordingViewer::OnClearFrame(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::OnDefaultFrame(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::OnDuplicateFrame(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::OnInsertFrame(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::OnInsertFrames(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::OnRemoveFrame(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::OnRemoveFrames(wxCommandEvent& event)
{
	// TODO
}
