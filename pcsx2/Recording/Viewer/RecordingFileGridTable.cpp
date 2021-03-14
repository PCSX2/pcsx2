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

RecordingFileGridTable::RecordingFileGridTable(int numColumns)
{
}

int RecordingFileGridTable::GetNumberRows()
{
	if (!activeFile.IsFileOpen())
	{
		return 0;
	}
	return activeFile.GetTotalFrames();
}

bool RecordingFileGridTable::AreChangesUnsaved()
{
	return changes;
}

void RecordingFileGridTable::ClearUnsavedChanges()
{
	changes = false;
}

int RecordingFileGridTable::GetControllerPort()
{
	return controllerPort;
}


void RecordingFileGridTable::SetControllerPort(int port)
{
	port = port == 0 ? port : port - 1;
	controllerPort = port;
	dataBuffer = activeFile.BulkReadPadData(bufferPos - bufferSize, bufferPos + bufferSize, controllerPort);
}

bool RecordingFileGridTable::IsFromSavestate()
{
	return activeFile.FromSaveState();
}

int RecordingFileGridTable::GetNumberCols()
{
	return 20;
}

std::pair<bool, int> getPressedAndPressure(std::string cellValue)
{
	std::regex regex("(\\d+)\\s*\\((\\d+)\\)");
	std::smatch matches;

	if (std::regex_search(cellValue, matches, regex))
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
	if (dataBuffer.empty() || row > (bufferPos + bufferThreshold) || row < (bufferPos - bufferThreshold))
	{
		dataBuffer = activeFile.BulkReadPadData(row - bufferSize, row + bufferSize, controllerPort);
		bufferPos = row;
	}

	if (dataBuffer.count(row) != 1)
	{
		return;
	}
	// Get that entire frame's structure, we can modify it, then save the appropriate buffer
	PadData padData = dataBuffer.at(row);

	int singleValue;
	std::pair<bool, int> multiValueCell;

	if (col >= 4 && col <= 15)
	{
		multiValueCell = getPressedAndPressure(value.ToStdString());
		if (multiValueCell.second < 0)
		{
			multiValueCell.second = 0;
		}
		else if (multiValueCell.second > 255)
		{
			multiValueCell.second = 255;
		}
	}
	else
	{
		singleValue = wxAtoi(value);
		if (col >= 16 && col <= 19)
		{
			singleValue = singleValue != 0 || singleValue != 1 ? 0 : singleValue;
		}
		else
		{
			if (singleValue < 0)
			{
				singleValue = 0;
			}
			else if (singleValue > 255)
			{
				singleValue = 255;
			}
		}
	}

	switch (col)
	{
		case 0:
			padData.leftAnalogX = singleValue;
			break;
		case 1:
			padData.leftAnalogY = singleValue;
			break;
		case 2:
			padData.rightAnalogX = singleValue;
			break;
		case 3:
			padData.rightAnalogY = singleValue;
			break;
		case 4:
			padData.squarePressed = multiValueCell.first;
			padData.squarePressure = multiValueCell.second;
			break;
		case 5:
			padData.crossPressed = multiValueCell.first;
			padData.crossPressure = multiValueCell.second;
			break;
		case 6:
			padData.circlePressed = multiValueCell.first;
			padData.circlePressure = multiValueCell.second;
			break;
		case 7:
			padData.trianglePressed = multiValueCell.first;
			padData.trianglePressure = multiValueCell.second;
			break;
		case 8:
			padData.leftPressed = multiValueCell.first;
			padData.leftPressure = multiValueCell.second;
			break;
		case 9:
			padData.downPressed = multiValueCell.first;
			padData.downPressure = multiValueCell.second;
			break;
		case 10:
			padData.rightPressed = multiValueCell.first;
			padData.rightPressure = multiValueCell.second;
			break;
		case 11:
			padData.upPressed = multiValueCell.first;
			padData.upPressure = multiValueCell.second;
			break;
		case 12:
			padData.r1Pressed = multiValueCell.first;
			padData.r1Pressure = multiValueCell.second;
			break;
		case 13:
			padData.r2Pressed = multiValueCell.first;
			padData.r2Pressure = multiValueCell.second;
			break;
		case 14:
			padData.l1Pressed = multiValueCell.first;
			padData.l1Pressure = multiValueCell.second;
			break;
		case 15:
			padData.l2Pressed = multiValueCell.first;
			padData.l2Pressure = multiValueCell.second;
			break;
		case 16:
			padData.r3 = singleValue;
			break;
		case 17:
			padData.l3 = singleValue;
			break;
		case 18:
			padData.select = singleValue;
			break;
		case 19:
			padData.start = singleValue;
			break;
		default:
			return;
	}

	// Save the frame
	activeFile.WriteFrame(row, controllerPort, padData);

	// Refresh the cache for subsequent GetValues
	dataBuffer = activeFile.BulkReadPadData(bufferPos - bufferSize, bufferPos + bufferSize, controllerPort);

	changes = true;
	return;
}

wxString RecordingFileGridTable::GetColLabelValue(int col)
{
	if (col > GetNumberCols() - 1)
	{
		return "?";
	}
	return columnLabels.at(col);
}

wxString RecordingFileGridTable::GetValue(int row, int col)
{
	if (!activeFile.IsFileOpen())
		return "";

	// Check if we have to refresh the cache
	if (dataBuffer.empty() || row > (bufferPos + bufferThreshold) || row < (bufferPos - bufferThreshold))
	{
		dataBuffer = activeFile.BulkReadPadData(row - bufferSize, row + bufferSize, controllerPort);
		bufferPos = row;
	}

	if (dataBuffer.count(row) != 1)
	{
		return "";
	}
	const PadData padData = dataBuffer.at(row);

	switch (col)
	{
		case 0:
			return wxString::Format("%d", padData.leftAnalogX);
		case 1:
			return wxString::Format("%d", padData.leftAnalogY);
		case 2:
			return wxString::Format("%d", padData.rightAnalogX);
		case 3:
			return wxString::Format("%d", padData.rightAnalogY);
		case 4:
			return wxString::Format("%d (%d)", padData.squarePressed, padData.squarePressure);
		case 5:
			return wxString::Format("%d (%d)", padData.crossPressed, padData.crossPressure);
		case 6:
			return wxString::Format("%d (%d)", padData.circlePressed, padData.circlePressure);
		case 7:
			return wxString::Format("%d (%d)", padData.trianglePressed, padData.trianglePressure);
		case 8:
			return wxString::Format("%d (%d)", padData.leftPressed, padData.leftPressure);
		case 9:
			return wxString::Format("%d (%d)", padData.downPressed, padData.downPressure);
		case 10:
			return wxString::Format("%d (%d)", padData.rightPressed, padData.rightPressure);
		case 11:
			return wxString::Format("%d (%d)", padData.upPressed, padData.upPressure);
		case 12:
			return wxString::Format("%d (%d)", padData.r1Pressed, padData.r1Pressure);
		case 13:
			return wxString::Format("%d (%d)", padData.r2Pressed, padData.r2Pressure);
		case 14:
			return wxString::Format("%d (%d)", padData.l1Pressed, padData.l1Pressure);
		case 15:
			return wxString::Format("%d (%d)", padData.l2Pressed, padData.l2Pressure);
		case 16:
			return wxString::Format("%d", padData.r3);
		case 17:
			return wxString::Format("%d", padData.l3);
		case 18:
			return wxString::Format("%d", padData.select);
		case 19:
			return wxString::Format("%d", padData.start);
		default:
			return "?";
	}
}

void RecordingFileGridTable::OpenRecordingFile(wxString filePath)
{
	activeFile.OpenExisting(filePath);
	dataBuffer.clear();
	wxGridTableMessage msg(this, wxGRIDTABLE_NOTIFY_ROWS_APPENDED, activeFile.GetTotalFrames(), 0);
	GetView()->ProcessTableMessage(msg);
}

void RecordingFileGridTable::CloseRecordingFile()
{
	wxGridTableMessage msg(this, wxGRIDTABLE_NOTIFY_ROWS_DELETED, 0, activeFile.GetTotalFrames());
	GetView()->ProcessTableMessage(msg);
	activeFile.Close();
	dataBuffer.clear();
}

InputRecordingFileHeader RecordingFileGridTable::GetRecordingFileHeader()
{
	return activeFile.GetHeader();
}

void RecordingFileGridTable::UpdateRecordingFileHeader(const std::string author, const std::string gameName)
{
	// Check to see if any changes were made
	// TODO - comparison func in InputRecordingFileHeader, not going to bother right now though since v2 is on the horizon
	InputRecordingFileHeader currHeader = activeFile.GetHeader();
	if (author == currHeader.author && gameName == currHeader.gameName)
	{
		return;
	}
	changes = true;
	activeFile.GetHeader().SetAuthor(author);
	activeFile.GetHeader().SetGameName(gameName);
	activeFile.WriteHeader();
}

long RecordingFileGridTable::GetUndoCount()
{
	return activeFile.GetUndoCount();
}

void RecordingFileGridTable::SetUndoCount(long undoCount)
{
	if (activeFile.GetUndoCount() == undoCount)
	{
		return;
	}
	changes = true;
	activeFile.SetUndoCount(undoCount);
	activeFile.WriteHeader();
}
