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

#pragma once

#include <wx/filepicker.h>
#include <wx/grid.h>
#include <wx/image.h>
#include <wx/wx.h>

#include "Recording/InputRecordingFile.h"
#include "RecordingFileGridTable.h"
#include "RecordingViewerColumn.h"

class InputRecordingViewer : public wxFrame
{
public:
	InputRecordingViewer(wxWindow* parent, AppConfig::InputRecordingOptions& options);

private:
	AppConfig::InputRecordingOptions& options;

	const int NUM_COLUMNS = 20;
	// displayIndex : RecordingViewerColumn
	std::map<int, RecordingViewerColumn> m_grid_columns;

	bool m_file_opened = false;

	wxString m_filepath;
	wxString m_temp_filepath;

	RecordingFileGridTable* m_recording_data_source;
	wxGrid* m_recording_grid;

	void toggleMenuItems(bool file_open);
	void initColumns();
	void refreshColumns();
	void closeActiveFile();

	void onClose(wxCloseEvent& event);
	void onMoveAround(wxMoveEvent& event);

	// Menu items and handlers
	wxMenuBar* m_menu_bar;

	wxMenu* m_file_menu;
	wxMenuItem* m_open_menu_item;
	wxMenuItem* m_close_menu_item;
	wxMenuItem* m_save_menu_item;
	wxMenuItem* m_save_as_menu_item;
	wxMenuItem* m_import_menu_item;
	wxMenuItem* m_export_menu_item;
	void onOpenFile(wxCommandEvent& event);
	void onCloseFile(wxCommandEvent& event);
	void onSaveFile(wxCommandEvent& event);
	void onSaveAsFile(wxCommandEvent& event);
	void onImport(wxCommandEvent& event);
	void onExport(wxCommandEvent& event);

	wxMenu* m_edit_menu;
	wxMenuItem* m_change_metadata_menu_item;
	wxMenuItem* m_change_recording_type_menu_item;
	wxMenuItem* m_change_base_savestate_menu_item;
	void onChangeMetadata(wxCommandEvent& event);
	void onChangeRecordingType(wxCommandEvent& event);
	void onChangeBaseSavestate(wxCommandEvent& event);

	wxMenu* m_view_menu;
	wxMenuItem* m_config_columns_menu_item;
	wxMenuItem* m_jump_to_frame_menu_item;
	wxMenuItem* m_controller_port_submenu;
	wxMenuItem* m_port_one_menu_item;
	wxMenuItem* m_port_two_menu_item;
	void onConfigColumns(wxCommandEvent& event);
	void onJumpToFrame(wxCommandEvent& event);
	void onSelectPortOne(wxCommandEvent& event);
	void onSelectPortTwo(wxCommandEvent& event);

	wxMenu* m_data_menu;
	wxMenuItem* m_clear_frame_menu_item;
	wxMenuItem* m_default_frame_menu_item;
	wxMenuItem* m_duplicate_frame_menu_item;
	wxMenuItem* m_insert_frame_menu_item;
	wxMenuItem* m_insert_frames_menu_item;
	wxMenuItem* m_remove_frame_menu_item;
	wxMenuItem* m_remove_frames_menu_item;
	void onClearFrame(wxCommandEvent& event);
	void onDefaultFrame(wxCommandEvent& event);
	void onDuplicateFrame(wxCommandEvent& event);
	void onInsertFrame(wxCommandEvent& event);
	void onInsertFrames(wxCommandEvent& event);
	void onRemoveFrame(wxCommandEvent& event);
	void onRemoveFrames(wxCommandEvent& event);
};
