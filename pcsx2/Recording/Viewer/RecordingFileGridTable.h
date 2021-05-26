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

#include <wx/wx.h>
#include <wx/grid.h>

#include "RecordingViewerColumn.h"
#include "Recording/InputRecordingFile.h"

class RecordingFileGridTable : public wxGridTableBase
{
public:
	RecordingFileGridTable(const std::map<int, RecordingViewerColumn>& grid_columns);

	wxString GetValue(int row, int col) override;
	wxString GetColLabelValue(int col) override;
	int GetNumberRows() override;
	int GetNumberCols() override;
	// NOTE - in typical wxWidgets fashion, the performance is terrible when you format many cells
	// Even their own example complains about the problem!
	// - https://github.com/wxWidgets/wxWidgets/blob/2a536c359cf9475ebda7c37647f50a49f4b0c2ae/samples/grid/griddemo.cpp#L1962-L1964
	// I'm not going down another performance rabbit-hole
	// wxGridCellAttr* GetAttr(int row, int col, wxGridCellAttr::wxAttrKind kind) override;
	void SetValue(int row, int col, const wxString& value) override;
	bool IsEmptyCell(int, int) override { return false; }

	void openRecordingFile(wxString filePath);
	void closeRecordingFile();
	void updateGridColumns(const std::map<int, RecordingViewerColumn>& grid_columns);

	InputRecordingFileHeader getRecordingFileHeader();
	void updateRecordingFileHeader(const std::string& author, const std::string& game_name);
	long getUndoCount();

	bool areChangesUnsaved();
	void clearUnsavedChanges();

	int getControllerPort();
	/**
	 * @brief Set the active port to be viewed
	 * @param port Controller port (not zero-based)
	*/
	void setControllerPort(int port);

	bool isFromSavestate();

private:
	std::map<int, RecordingViewerColumn> m_grid_columns;
	int m_controller_port = 0;
	bool m_changes = false;

	InputRecordingFile m_active_file;

	// Cache 1000 rows around current position to enable smooth scrolling / limit file IO
	int m_buffer_size = 1000;
	int m_buffer_pos = 0;
	// When we get 500 rows beyond the last bufferPos, evict the cache and update!
	int m_buffer_threshold = 500;
	std::map<uint, PadData> m_data_buffer;
};