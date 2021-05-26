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
#include <wx/rearrangectrl.h>
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
	: wxFrame(parent, wxID_ANY, _("Input Recording Viewer"), wxDefaultPosition, wxDefaultSize)
	, options(options)
{
	// Init Menus
	m_menu_bar = new wxMenuBar();
	m_file_menu = new wxMenu();
	m_open_menu_item = enableMenuItem(m_file_menu->Append(wxID_ANY, _("Open"), _("Open an Input Recording file to view/edit")), true);
	m_close_menu_item = enableMenuItem(m_file_menu->Append(wxID_ANY, _("Close"), _("Close the current file")), false);
	m_file_menu->AppendSeparator();
	m_save_menu_item = enableMenuItem(m_file_menu->Append(wxID_ANY, _("Save"), _("Save the current file, overwriting the original")), false);
	m_save_as_menu_item = enableMenuItem(m_file_menu->Append(wxID_ANY, _("Save As"), _("Save the current file to a specified location")), false);
	// TODO - implement Exporting!
	// fileMenu->AppendSeparator();
	// importMenuItem = enableMenuItem(fileMenu->Append(wxID_ANY, _("Import From YAML"), _("Import recording data from a YAML file")), false);
	// exportMenuItem = enableMenuItem(fileMenu->Append(wxID_ANY, _("Export As YAML"), _("Export recording data to a YAML file")), false);
	m_menu_bar->Append(m_file_menu, _("File"));

	m_edit_menu = new wxMenu();
	m_change_metadata_menu_item = enableMenuItem(m_edit_menu->Append(wxID_ANY, _("Change Metadata"), _("Change the recordings relevant metadata")), false);
	// TODO - implement these at a later date
	// changeRecordingTypeMenuItem = enableMenuItem(editMenu->Append(wxID_ANY, _("Change Recording Type"), _("Change the recording's type (ie. power-on / save-state)")), false);
	// changeBaseSavestateMenuItem = enableMenuItem(editMenu->Append(wxID_ANY, _("Change Base Savestate"), _("Change the base savestate for the recording")), false);
	m_menu_bar->Append(m_edit_menu, _("Edit"));

	m_view_menu = new wxMenu();
	m_config_columns_menu_item = enableMenuItem(m_view_menu->Append(wxID_ANY, _("Config Columns"), _("Change the order and displaying of columns")), true);
	m_view_menu->AppendSeparator();
	m_jump_to_frame_menu_item = enableMenuItem(m_view_menu->Append(wxID_ANY, _("Jump to Frame"), _("Jump to a specific frame")), false);
	m_view_menu->AppendSeparator();
	wxMenu* controller_menu = new wxMenu();
	m_port_one_menu_item = checkMenuItem(controller_menu->Append(wxID_ANY, _("Port 1"), _("Select port 1"), wxITEM_CHECK), true);
	m_port_two_menu_item = checkMenuItem(controller_menu->Append(wxID_ANY, _("Port 2"), _("Select port 2"), wxITEM_CHECK), false);
	m_controller_port_submenu = enableMenuItem(m_view_menu->AppendSubMenu(controller_menu, _("Change Controller"), _("Switch which controller port is active")), false);
	m_menu_bar->Append(m_view_menu, _("View"));

	m_data_menu = new wxMenu();
	m_clear_frame_menu_item = enableMenuItem(m_data_menu->Append(wxID_ANY, _("Clear Frame"), _("Clear the entire frame of data")), false);
	m_default_frame_menu_item = enableMenuItem(m_data_menu->Append(wxID_ANY, _("Default Frame"), _("Set the frame to the controllers default neutral values")), false);
	m_duplicate_frame_menu_item = enableMenuItem(m_data_menu->Append(wxID_ANY, _("Duplicate Frame"), _("Duplicate and insert the frame")), false);
	m_insert_frame_menu_item = enableMenuItem(m_data_menu->Append(wxID_ANY, _("Insert Frame"), _("Insert a new default frame")), false);
	m_insert_frames_menu_item = enableMenuItem(m_data_menu->Append(wxID_ANY, _("Insert Frame(s)"), _("Insert multiple default frames")), false);
	m_remove_frame_menu_item = enableMenuItem(m_data_menu->Append(wxID_ANY, _("Remove Frame"), _("Remove a frame")), false);
	m_remove_frames_menu_item = enableMenuItem(m_data_menu->Append(wxID_ANY, _("Remove Frame(s)"), _("Remove multiple frames")), false);
	// TODO - implement Data menu and event handlers
	// menuBar->Append(dataMenu, _("Data"));

	// Initialize Grid Column Order
	initColumns();

	// Init Widgets
	m_recording_data_source = new RecordingFileGridTable(m_grid_columns);
	m_recording_grid = new wxGrid(this, wxID_ANY);
	m_recording_grid->SetTable(m_recording_data_source, true);
	m_recording_grid->EnableDragColSize(false);
	m_recording_grid->EnableDragRowSize(false);
	m_recording_grid->EnableDragGridSize(false);
	m_recording_grid->SetColSize(0, 100);
	m_recording_grid->SetColSize(1, 100);
	m_recording_grid->SetColSize(2, 100);
	m_recording_grid->SetColSize(3, 100);
	m_recording_grid->SetColLabelSize(75);

	// Bind Events
	Bind(wxEVT_CLOSE_WINDOW, &InputRecordingViewer::onClose, this);
	Bind(wxEVT_MOVE, &InputRecordingViewer::onMoveAround, this);

	Bind(wxEVT_MENU, &InputRecordingViewer::onOpenFile, this, m_open_menu_item->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::onCloseFile, this, m_close_menu_item->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::onSaveFile, this, m_save_menu_item->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::onSaveAsFile, this, m_save_as_menu_item->GetId());
	// TODO - implement Exporting!
	// Bind(wxEVT_MENU, &InputRecordingViewer::OnImport, this, importMenuItem->GetId());
	// Bind(wxEVT_MENU, &InputRecordingViewer::OnExport, this, exportMenuItem->GetId());

	Bind(wxEVT_MENU, &InputRecordingViewer::onChangeMetadata, this, m_change_metadata_menu_item->GetId());
	// TODO - implement!
	// Bind(wxEVT_MENU, &InputRecordingViewer::OnChangeRecordingType, this, changeRecordingTypeMenuItem->GetId());
	// Bind(wxEVT_MENU, &InputRecordingViewer::OnChangeBaseSavestate, this, changeBaseSavestateMenuItem->GetId());

	Bind(wxEVT_MENU, &InputRecordingViewer::onConfigColumns, this, m_config_columns_menu_item->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::onJumpToFrame, this, m_jump_to_frame_menu_item->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::onSelectPortOne, this, m_port_one_menu_item->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::onSelectPortTwo, this, m_port_two_menu_item->GetId());

	Bind(wxEVT_MENU, &InputRecordingViewer::onClearFrame, this, m_clear_frame_menu_item->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::onDefaultFrame, this, m_default_frame_menu_item->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::onDuplicateFrame, this, m_duplicate_frame_menu_item->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::onInsertFrame, this, m_insert_frame_menu_item->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::onInsertFrames, this, m_insert_frames_menu_item->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::onRemoveFrame, this, m_remove_frame_menu_item->GetId());
	Bind(wxEVT_MENU, &InputRecordingViewer::onRemoveFrames, this, m_remove_frames_menu_item->GetId());

	// Sizers
	wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
	sizer->Add(m_recording_grid, 1, wxEXPAND, 0);
	sizer->SetSizeHints(this);

	SetMenuBar(m_menu_bar);
	SetIcons(wxGetApp().GetIconBundle());

	CreateStatusBar(1);

	SetPosition(options.ViewerPosition);
	SetSizer(sizer);
	SetMinSize(wxSize(500, 500));
	Layout();
	refreshColumns();
}

void InputRecordingViewer::onClose(wxCloseEvent& event)
{
	if (m_file_opened)
	{
		closeActiveFile();
	}
	Hide();
}

void InputRecordingViewer::onMoveAround(wxMoveEvent& event)
{
	if (IsBeingDeleted() || !IsVisible() || IsIconized())
		return;

	if (!IsMaximized())
		options.ViewerPosition = GetPosition();
	event.Skip();
}

void InputRecordingViewer::toggleMenuItems(bool file_open)
{
	m_close_menu_item->Enable(file_open);
	m_save_menu_item->Enable(file_open);
	m_save_as_menu_item->Enable(file_open);
	// importMenuItem->Enable(fileOpen);
	// exportMenuItem->Enable(fileOpen);

	m_change_metadata_menu_item->Enable(file_open);
	// changeRecordingTypeMenuItem->Enable(fileOpen);
	// changeBaseSavestateMenuItem->Enable(fileOpen);

	m_jump_to_frame_menu_item->Enable(file_open);
	m_controller_port_submenu->Enable(file_open);
	m_port_one_menu_item->Check(file_open && m_recording_data_source->getControllerPort() == 1);
	m_port_two_menu_item->Check(file_open && m_recording_data_source->getControllerPort() == 2);

	m_clear_frame_menu_item->Enable(file_open);
	m_default_frame_menu_item->Enable(file_open);
	m_duplicate_frame_menu_item->Enable(file_open);
	m_insert_frame_menu_item->Enable(file_open);
	m_insert_frames_menu_item->Enable(file_open);
	m_remove_frame_menu_item->Enable(file_open);
	m_remove_frames_menu_item->Enable(file_open);
}

void InputRecordingViewer::initColumns()
{
	for (int i = 0; i < NUM_COLUMNS; i++)
	{
		appendColumn(i, m_grid_columns, options);
	}
}

void InputRecordingViewer::refreshColumns()
{
	m_recording_data_source->updateGridColumns(m_grid_columns);
	for (auto& columnEntry : m_grid_columns)
	{
		columnEntry.second.m_shown ? m_recording_grid->ShowCol(columnEntry.first) : m_recording_grid->HideCol(columnEntry.first);
	}
	saveColumnsToConfig(m_grid_columns, options);
	m_recording_grid->ForceRefresh();
}

void InputRecordingViewer::closeActiveFile()
{
	m_recording_data_source->closeRecordingFile();
	m_recording_grid->ForceRefresh();
	if (wxFileExists(m_temp_filepath))
	{
		wxRemoveFile(m_temp_filepath);
	}
	toggleMenuItems(false);
	m_file_opened = false;
}

void InputRecordingViewer::onOpenFile(wxCommandEvent& event)
{
	if (m_file_opened)
	{
		int answer;
		if (m_recording_data_source->areChangesUnsaved())
		{
			answer = wxMessageBox(_("Close active file without saving changes?"), _("Confirm"),
								  wxYES_NO | wxCANCEL, this);
		}
		else
		{
			answer = wxMessageBox(_("Close active file?"), _("Confirm"),
								  wxYES_NO | wxCANCEL, this);
		}
		if (answer != wxYES)
		{
			return;
		}
		closeActiveFile();
	}

	wxFileDialog* open_file_dialog =
		new wxFileDialog(this, _("Open Input Recording File"), wxEmptyString, wxEmptyString, "p2m2 file(*.p2m2)|*.p2m2",
						 wxFD_OPEN, wxDefaultPosition);

	if (open_file_dialog->ShowModal() == wxID_OK)
	{
		// TODO - utility function
		m_filepath = open_file_dialog->GetPath();
		// wxWidget's removes the extension if it contains wildcards
		// on wxGTK https://trac.wxwidgets.org/ticket/15285
		if (!m_filepath.EndsWith(".p2m2"))
			m_filepath = wxString::Format("%s.p2m2", m_filepath);

		if (!wxFileExists(m_filepath))
		{
			wxMessageBox(_("Unable to Open Input Recording File"), "Input Recording Viewer Error", wxOK | wxICON_ERROR, this);
			return;
		}

		// Copy the file, as this is out working copy if edits are made
		// The user can then choose to overwrite the existing file, or save elsewhere.
		// At which time, we remove the temp file
		m_temp_filepath = m_filepath + ".tmp";
		if (!wxCopyFile(m_filepath, m_temp_filepath, true))
		{
			wxMessageBox(_("Unable to Open Input Recording File"), "Input Recording Viewer Error", wxOK | wxICON_ERROR, this);
			return;
		}

		m_recording_data_source->openRecordingFile(m_temp_filepath);
		m_recording_grid->ForceRefresh();

		// Enable various menu options
		toggleMenuItems(true);
		m_file_opened = true;
	}
}

void InputRecordingViewer::onCloseFile(wxCommandEvent& event)
{
	if (m_recording_data_source->areChangesUnsaved())
	{
		int answer = wxMessageBox(_("Close without saving changes?"), _("Confirm"),
								  wxYES_NO | wxCANCEL, this);
		if (answer != wxYES)
		{
			return;
		}
	}
	closeActiveFile();
}

// TODO - a note on saving, currently we don't allow modifying the original save-state, so there are no concerns about it
// When we do though, these save functions will need to expand!

void InputRecordingViewer::onSaveFile(wxCommandEvent& event)
{
	if (!wxCopyFile(m_temp_filepath, m_filepath + ".bak", true))
	{
		wxMessageBox(_("Unable to backup original recording before saving, aborting!"), "Input Recording Viewer Error", wxOK | wxICON_ERROR, this);
		return;
	}

	// Overwrite existing file, this is just a simple rename
	if (!wxCopyFile(m_temp_filepath, m_filepath, true))
	{
		wxMessageBox(_("Unable to Save Input Recording File"), "Input Recording Viewer Error", wxOK | wxICON_ERROR, this);
		return;
	}

	m_recording_data_source->clearUnsavedChanges();
}

void InputRecordingViewer::onSaveAsFile(wxCommandEvent& event)
{
	wxFileDialog* open_file_dialog =
		new wxFileDialog(this, _("Save Input Recording File"), wxEmptyString, wxEmptyString, "p2m2 file(*.p2m2)|*.p2m2",
						 wxFD_SAVE | wxFD_OVERWRITE_PROMPT, wxDefaultPosition);

	if (open_file_dialog->ShowModal() == wxID_OK)
	{
		wxString savePath = open_file_dialog->GetPath();
		// wxWidget's removes the extension if it contains wildcards
		// on wxGTK https://trac.wxwidgets.org/ticket/15285
		if (!savePath.EndsWith(".p2m2"))
			savePath = wxString::Format("%s.p2m2", savePath);

		if (savePath == m_filepath)
		{
			if (!wxCopyFile(m_temp_filepath, savePath + ".bak", true))
			{
				wxMessageBox(_("Unable to backup original recording before saving, aborting!"), "Input Recording Viewer Error", wxOK | wxICON_ERROR, this);
				return;
			}
		}

		if (!wxCopyFile(m_temp_filepath, savePath, true))
		{
			wxMessageBox(_("Unable to Save Input Recording File"), "Input Recording Viewer Error", wxOK | wxICON_ERROR, this);
			return;
		}

		// If it's a save-state recording, copy over the save-state as well
		if (m_recording_data_source->isFromSavestate())
		{
			if (wxFileExists(m_filepath + "_SaveState.p2s"))
			{
				if (savePath != m_filepath && !wxCopyFile(m_filepath + "_SaveState.p2s", savePath + "_SaveState.p2s", true))
				{
					wxMessageBox(_("Unable to Save Input Recording's SaveState!"), "Input Recording Viewer Error", wxOK | wxICON_ERROR, this);
				}
			}
			else
			{
				wxMessageBox(_("Unable to Save Input Recording's SaveState, not found!"), "Input Recording Viewer Error", wxOK | wxICON_ERROR, this);
			}
		}

		m_recording_data_source->clearUnsavedChanges();
	}
}

void InputRecordingViewer::onImport(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::onExport(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::onChangeMetadata(wxCommandEvent& event)
{
	InputRecordingFileHeader current_header = m_recording_data_source->getRecordingFileHeader();
	RecordingMetadataDialog* custom = new RecordingMetadataDialog(this, current_header.author, current_header.gameName);
	if (custom->ShowModal() == wxID_OK)
	{
		m_recording_data_source->updateRecordingFileHeader(custom->getAuthor(), custom->getGameName());
	}
}

void InputRecordingViewer::onChangeRecordingType(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::onChangeBaseSavestate(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::onConfigColumns(wxCommandEvent& event)
{
	wxArrayString items;
	wxArrayInt order;
	for (auto& columnEntry : m_grid_columns)
	{
		items.push_back(columnEntry.second.m_label);
		order.push_back(columnEntry.second.m_shown ? columnEntry.first : ~columnEntry.first);
	}
	wxRearrangeDialog dlg(NULL,
						  "You can also uncheck the items you don't like at all.",
						  "Sort the items in order of preference",
						  order, items);

	if (dlg.ShowModal() == wxID_OK)
	{
		std::map<int, RecordingViewerColumn> newGridColumns;
		order = dlg.GetOrder();
		for (size_t i = 0; i < order.size(); i++)
		{
			// wxWidget's dialog here will return a helpful list where unchecked items are two's complemented
			// However, we don't actually remove them, we hide them, so i have to add them back!
			int oldIndex = order[i];
			if (order[i] < 0)
			{
				oldIndex = abs(oldIndex + 1);
			}
			newGridColumns[i] = m_grid_columns.at(oldIndex);
			newGridColumns.at(i).m_shown = order[i] >= 0;
		}
		m_grid_columns = newGridColumns;
		refreshColumns();
	}
}

void InputRecordingViewer::onJumpToFrame(wxCommandEvent& event)
{
	long frame = wxGetNumberFromUser(_("Enter Frame Number"), wxEmptyString, _("Jump To Frame"), 0, 0, m_recording_data_source->GetNumberRows(), this);
	frame = frame == 0 ? 0 : frame - 1;
	m_recording_grid->SelectRow(frame);
	m_recording_grid->MakeCellVisible(frame, 0);
}

void InputRecordingViewer::onSelectPortOne(wxCommandEvent& event)
{
	bool selected = m_port_one_menu_item->IsChecked();
	m_port_two_menu_item->Check(!selected);
	m_recording_data_source->setControllerPort(1);
	m_recording_grid->ForceRefresh();
}

void InputRecordingViewer::onSelectPortTwo(wxCommandEvent& event)
{
	bool selected = m_port_two_menu_item->IsChecked();
	m_port_one_menu_item->Check(!selected);
	m_recording_data_source->setControllerPort(2);
	m_recording_grid->ForceRefresh();
}

void InputRecordingViewer::onClearFrame(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::onDefaultFrame(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::onDuplicateFrame(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::onInsertFrame(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::onInsertFrames(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::onRemoveFrame(wxCommandEvent& event)
{
	// TODO
}

void InputRecordingViewer::onRemoveFrames(wxCommandEvent& event)
{
	// TODO
}
