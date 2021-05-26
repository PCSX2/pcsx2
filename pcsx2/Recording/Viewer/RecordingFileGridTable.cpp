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

#include "RecordingFileGridTable.h"
#include <regex>

RecordingFileGridTable::RecordingFileGridTable(const std::map<int, RecordingViewerColumn>& grid_columns)
	: m_grid_columns(grid_columns)
{
}

int RecordingFileGridTable::GetNumberRows()
{
	if (!m_active_file.IsFileOpen())
	{
		return 0;
	}
	return m_active_file.GetTotalFrames();
}

bool RecordingFileGridTable::areChangesUnsaved()
{
	return m_changes;
}

void RecordingFileGridTable::clearUnsavedChanges()
{
	m_changes = false;
}

int RecordingFileGridTable::getControllerPort()
{
	return m_controller_port;
}


void RecordingFileGridTable::setControllerPort(int port)
{
	port = port == 0 ? port : port - 1;
	m_controller_port = port;
	m_data_buffer = m_active_file.BulkReadPadData(m_buffer_pos - m_buffer_size, m_buffer_pos + m_buffer_size, m_controller_port);
}

bool RecordingFileGridTable::isFromSavestate()
{
	return m_active_file.FromSaveState();
}

int RecordingFileGridTable::GetNumberCols()
{
	return m_grid_columns.size();
}

std::pair<bool, int> getPressedAndPressure(std::string cell_value)
{
	std::regex regex("(\\d+)\\s*\\((\\d+)\\)");
	std::smatch matches;

	if (std::regex_search(cell_value, matches, regex))
	{
		if (matches.size() != 3)
		{
			return {0, 0}; // TODO constants in PadData would be better
		}
		return {std::stoi(matches[1]), std::stoi(matches[2])};
	}
	return {0, 0}; // TODO constants in PadData would be better
}

void RecordingFileGridTable::SetValue(int row, int col, const wxString& value)
{
	// Check if we have to refresh the cache
	if (m_data_buffer.empty() || row > (m_buffer_pos + m_buffer_threshold) || row < (m_buffer_pos - m_buffer_threshold))
	{
		m_data_buffer = m_active_file.BulkReadPadData(row - m_buffer_size, row + m_buffer_size, m_controller_port);
		m_buffer_pos = row;
	}

	if (m_data_buffer.count(row) != 1)
	{
		return;
	}
	// Get that entire frame's structure, we can modify it, then save the appropriate buffer
	PadData pad_data = m_data_buffer.at(row);

	int single_value;
	std::pair<bool, int> multi_value_cell;

	if (col >= 4 && col <= 15)
	{
		multi_value_cell = getPressedAndPressure(value.ToStdString());
		if (multi_value_cell.second < 0)
		{
			multi_value_cell.second = 0;
		}
		else if (multi_value_cell.second > 255)
		{
			multi_value_cell.second = 255;
		}
	}
	else
	{
		single_value = wxAtoi(value);
		if (col >= 16 && col <= 19)
		{
			single_value = single_value == 0 || single_value == 1 ? single_value : 0;
		}
		else
		{
			if (single_value < 0)
			{
				single_value = 0;
			}
			else if (single_value > 255)
			{
				single_value = 255;
			}
		}
	}

	switch (col)
	{
		case 0:
			pad_data.leftAnalogX = single_value;
			break;
		case 1:
			pad_data.leftAnalogY = single_value;
			break;
		case 2:
			pad_data.rightAnalogX = single_value;
			break;
		case 3:
			pad_data.rightAnalogY = single_value;
			break;
		case 4:
			pad_data.squarePressed = multi_value_cell.first;
			pad_data.squarePressure = multi_value_cell.second;
			break;
		case 5:
			pad_data.crossPressed = multi_value_cell.first;
			pad_data.crossPressure = multi_value_cell.second;
			break;
		case 6:
			pad_data.circlePressed = multi_value_cell.first;
			pad_data.circlePressure = multi_value_cell.second;
			break;
		case 7:
			pad_data.trianglePressed = multi_value_cell.first;
			pad_data.trianglePressure = multi_value_cell.second;
			break;
		case 8:
			pad_data.r1Pressed = multi_value_cell.first;
			pad_data.r1Pressure = multi_value_cell.second;
			break;
		case 9:
			pad_data.r2Pressed = multi_value_cell.first;
			pad_data.r2Pressure = multi_value_cell.second;
			break;
		case 10:
			pad_data.l1Pressed = multi_value_cell.first;
			pad_data.l1Pressure = multi_value_cell.second;
			break;
		case 11:
			pad_data.l2Pressed = multi_value_cell.first;
			pad_data.l2Pressure = multi_value_cell.second;
			break;
		case 12:
			pad_data.leftPressed = multi_value_cell.first;
			pad_data.leftPressure = multi_value_cell.second;
			break;
		case 13:
			pad_data.downPressed = multi_value_cell.first;
			pad_data.downPressure = multi_value_cell.second;
			break;
		case 14:
			pad_data.rightPressed = multi_value_cell.first;
			pad_data.rightPressure = multi_value_cell.second;
			break;
		case 15:
			pad_data.upPressed = multi_value_cell.first;
			pad_data.upPressure = multi_value_cell.second;
			break;
		case 16:
			pad_data.r3 = single_value;
			break;
		case 17:
			pad_data.l3 = single_value;
			break;
		case 18:
			pad_data.select = single_value;
			break;
		case 19:
			pad_data.start = single_value;
			break;
		default:
			return;
	}

	// Save the frame
	m_active_file.WriteFrame(row, m_controller_port, pad_data);

	// Refresh the cache for subsequent GetValues
	m_data_buffer = m_active_file.BulkReadPadData(m_buffer_pos - m_buffer_size, m_buffer_pos + m_buffer_size, m_controller_port);

	m_changes = true;
	return;
}

wxString RecordingFileGridTable::GetColLabelValue(int col)
{
	if (col > GetNumberCols() - 1)
	{
		return "?";
	}
	return wxString(m_grid_columns.at(col).m_label);
}

wxString RecordingFileGridTable::GetValue(int row, int col)
{
	if (!m_active_file.IsFileOpen())
		return "";

	// Check if we have to refresh the cache
	if (m_data_buffer.empty() || row > (m_buffer_pos + m_buffer_threshold) || row < (m_buffer_pos - m_buffer_threshold))
	{
		m_data_buffer = m_active_file.BulkReadPadData(row - m_buffer_size, row + m_buffer_size, m_controller_port);
		m_buffer_pos = row;
	}

	if (m_data_buffer.count(row) != 1)
	{
		return "";
	}
	const PadData pad_data = m_data_buffer.at(row);

	switch (col)
	{
		case 0:
			return wxString::Format("%d", pad_data.leftAnalogX);
		case 1:
			return wxString::Format("%d", pad_data.leftAnalogY);
		case 2:
			return wxString::Format("%d", pad_data.rightAnalogX);
		case 3:
			return wxString::Format("%d", pad_data.rightAnalogY);
		case 4:
			return wxString::Format("%d (%d)", pad_data.squarePressed, pad_data.squarePressure);
		case 5:
			return wxString::Format("%d (%d)", pad_data.crossPressed, pad_data.crossPressure);
		case 6:
			return wxString::Format("%d (%d)", pad_data.circlePressed, pad_data.circlePressure);
		case 7:
			return wxString::Format("%d (%d)", pad_data.trianglePressed, pad_data.trianglePressure);
		case 8:
			return wxString::Format("%d (%d)", pad_data.r1Pressed, pad_data.r1Pressure);
		case 9:
			return wxString::Format("%d (%d)", pad_data.r2Pressed, pad_data.r2Pressure);
		case 10:
			return wxString::Format("%d (%d)", pad_data.l1Pressed, pad_data.l1Pressure);
		case 11:
			return wxString::Format("%d (%d)", pad_data.l2Pressed, pad_data.l2Pressure);
		case 12:
			return wxString::Format("%d (%d)", pad_data.leftPressed, pad_data.leftPressure);
		case 13:
			return wxString::Format("%d (%d)", pad_data.downPressed, pad_data.downPressure);
		case 14:
			return wxString::Format("%d (%d)", pad_data.rightPressed, pad_data.rightPressure);
		case 15:
			return wxString::Format("%d (%d)", pad_data.upPressed, pad_data.upPressure);
		case 16:
			return wxString::Format("%d", pad_data.r3);
		case 17:
			return wxString::Format("%d", pad_data.l3);
		case 18:
			return wxString::Format("%d", pad_data.select);
		case 19:
			return wxString::Format("%d", pad_data.start);
		default:
			return "?";
	}
}

void RecordingFileGridTable::openRecordingFile(wxString filePath)
{
	m_active_file.OpenExisting(filePath);
	m_data_buffer.clear();
	wxGridTableMessage msg(this, wxGRIDTABLE_NOTIFY_ROWS_APPENDED, m_active_file.GetTotalFrames(), 0);
	GetView()->ProcessTableMessage(msg);
	m_changes = false;
}

void RecordingFileGridTable::closeRecordingFile()
{
	wxGridTableMessage msg(this, wxGRIDTABLE_NOTIFY_ROWS_DELETED, 0, m_active_file.GetTotalFrames());
	GetView()->ProcessTableMessage(msg);
	m_active_file.Close();
	m_data_buffer.clear();
	m_changes = false;
}

void RecordingFileGridTable::updateGridColumns(const std::map<int, RecordingViewerColumn>& grid_columns)
{
	this->m_grid_columns = grid_columns;
}

InputRecordingFileHeader RecordingFileGridTable::getRecordingFileHeader()
{
	return m_active_file.GetHeader();
}

void RecordingFileGridTable::updateRecordingFileHeader(const std::string& author, const std::string& game_name)
{
	// Check to see if any changes were made
	// TODO - comparison func in InputRecordingFileHeader, not going to bother right now though since v2 is on the horizon
	InputRecordingFileHeader current_header = m_active_file.GetHeader();
	if (author == current_header.author && game_name == current_header.gameName)
	{
		return;
	}
	m_changes = true;
	m_active_file.GetHeader().SetAuthor(author);
	m_active_file.GetHeader().SetGameName(game_name);
	m_active_file.WriteHeader();
}

long RecordingFileGridTable::getUndoCount()
{
	return m_active_file.GetUndoCount();
}
