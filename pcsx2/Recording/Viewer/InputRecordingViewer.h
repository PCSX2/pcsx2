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
	std::map<int, RecordingViewerColumn> gridColumns;
	
	bool fileOpened = false;

	wxString filePath;
	wxString tempFilePath;

	RecordingFileGridTable* recordingDataSource;
	wxGrid* recordingGrid;

	void ToggleMenuItems(bool fileOpen);
	void InitColumns();
	void RefreshColumns();
	void CloseActiveFile();

	void OnClose(wxCloseEvent& event);
	void OnMoveAround(wxMoveEvent& event);

	// Menu items and handlers
	wxMenuBar* menuBar;

	wxMenu* fileMenu;
	wxMenuItem* openMenuItem;
	wxMenuItem* closeMenuItem;
	wxMenuItem* saveMenuItem;
	wxMenuItem* saveAsMenuItem;
	wxMenuItem* importMenuItem;
	wxMenuItem* exportMenuItem;
	void OnOpenFile(wxCommandEvent& event);
	void OnCloseFile(wxCommandEvent& event);
	void OnSaveFile(wxCommandEvent& event);
	void OnSaveAsFile(wxCommandEvent& event);
	void OnImport(wxCommandEvent& event);
	void OnExport(wxCommandEvent& event);

	wxMenu* editMenu;
	wxMenuItem* changeMetadataMenuItem;
	wxMenuItem* changeRecordingTypeMenuItem;
	wxMenuItem* changeBaseSavestateMenuItem;
	void OnChangeMetadata(wxCommandEvent& event);
	void OnChangeRecordingType(wxCommandEvent& event);
	void OnChangeBaseSavestate(wxCommandEvent& event);

	wxMenu* viewMenu;
	wxMenuItem* configColumnsMenuItem;
	wxMenuItem* jumpToFrameMenuItem;
	wxMenuItem* controllerPortSubmenu;
	wxMenuItem* portOneMenuItem;
	wxMenuItem* portTwoMenuItem;
	void OnConfigColumns(wxCommandEvent& event);
	void OnJumpToFrame(wxCommandEvent& event);
	void OnSelectPortOne(wxCommandEvent& event);
	void OnSelectPortTwo(wxCommandEvent& event);

	wxMenu* dataMenu;
	wxMenuItem* clearFrameMenuItem;
	wxMenuItem* defaultFrameMenuItem;
	wxMenuItem* duplicateFrameMenuItem;
	wxMenuItem* insertFrameMenuItem;
	wxMenuItem* insertFramesMenuItem;
	wxMenuItem* removeFrameMenuItem;
	wxMenuItem* removeFramesMenuItem;
	void OnClearFrame(wxCommandEvent& event);
	void OnDefaultFrame(wxCommandEvent& event);
	void OnDuplicateFrame(wxCommandEvent& event);
	void OnInsertFrame(wxCommandEvent& event);
	void OnInsertFrames(wxCommandEvent& event);
	void OnRemoveFrame(wxCommandEvent& event);
	void OnRemoveFrames(wxCommandEvent& event);
};
