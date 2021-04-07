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

#include "RecordingViewerColumn.h"

const std::vector<std::string> column_labels = {
	"Left\nAnalog X",
	"Left\nAnalog Y",
	"Right\nAnalog X",
	"Right\nAnalog Y",
	"Square",
	"Cross",
	"Circle",
	"Triangle",
	"R1",
	"R2",
	"L1",
	"L2",
	"Left",
	"Down",
	"Right",
	"Up",
	"R3",
	"L3",
	"Select",
	"Start"};

RecordingViewerColumn::RecordingViewerColumn()
{
}

RecordingViewerColumn::RecordingViewerColumn(int internal_index, bool shown)
	: m_internal_index(internal_index)
	, m_label(column_labels.at(internal_index))
	, m_shown(shown)
{
}

void appendColumn(int internal_index, std::map<int, RecordingViewerColumn>& grid_columns, AppConfig::InputRecordingOptions& options)
{
	// TODO - AppConfig and .ini system doesn't have an easy way to just use string keys or collections so...another switch it is!
	// Refactor ALL of this once we have a better config system
	switch (internal_index)
	{
		case 0:
			grid_columns[options.ViewerLeftAnalogXIndex] = RecordingViewerColumn(internal_index, options.ViewerLeftAnalogXShown);
			break;
		case 1:
			grid_columns[options.ViewerLeftAnalogYIndex] = RecordingViewerColumn(internal_index, options.ViewerLeftAnalogYShown);
			break;
		case 2:
			grid_columns[options.ViewerRightAnalogXIndex] = RecordingViewerColumn(internal_index, options.ViewerRightAnalogXShown);
			break;
		case 3:
			grid_columns[options.ViewerRightAnalogYIndex] = RecordingViewerColumn(internal_index, options.ViewerRightAnalogYShown);
			break;
		case 4:
			grid_columns[options.ViewerSquareIndex] = RecordingViewerColumn(internal_index, options.ViewerSquareShown);
			break;
		case 5:
			grid_columns[options.ViewerCrossIndex] = RecordingViewerColumn(internal_index, options.ViewerCrossShown);
			break;
		case 6:
			grid_columns[options.ViewerCircleIndex] = RecordingViewerColumn(internal_index, options.ViewerCircleShown);
			break;
		case 7:
			grid_columns[options.ViewerTriangleIndex] = RecordingViewerColumn(internal_index, options.ViewerTriangleShown);
			break;
		case 8:
			grid_columns[options.ViewerR1Index] = RecordingViewerColumn(internal_index, options.ViewerR1Shown);
			break;
		case 9:
			grid_columns[options.ViewerR2Index] = RecordingViewerColumn(internal_index, options.ViewerR2Shown);
			break;
		case 10:
			grid_columns[options.ViewerL1Index] = RecordingViewerColumn(internal_index, options.ViewerL1Shown);
			break;
		case 11:
			grid_columns[options.ViewerL2Index] = RecordingViewerColumn(internal_index, options.ViewerL2Shown);
			break;
		case 12:
			grid_columns[options.ViewerLeftIndex] = RecordingViewerColumn(internal_index, options.ViewerLeftShown);
			break;
		case 13:
			grid_columns[options.ViewerDownIndex] = RecordingViewerColumn(internal_index, options.ViewerDownShown);
			break;
		case 14:
			grid_columns[options.ViewerRightIndex] = RecordingViewerColumn(internal_index, options.ViewerRightShown);
			break;
		case 15:
			grid_columns[options.ViewerUpIndex] = RecordingViewerColumn(internal_index, options.ViewerUpShown);
			break;
		case 16:
			grid_columns[options.ViewerR3Index] = RecordingViewerColumn(internal_index, options.ViewerR3Shown);
			break;
		case 17:
			grid_columns[options.ViewerL3Index] = RecordingViewerColumn(internal_index, options.ViewerL3Shown);
			break;
		case 18:
			grid_columns[options.ViewerSelectIndex] = RecordingViewerColumn(internal_index, options.ViewerSelectShown);
			break;
		case 19:
			grid_columns[options.ViewerStartIndex] = RecordingViewerColumn(internal_index, options.ViewerStartShown);
			break;
		default:
			return;
	}
}

void saveColumnsToConfig(const std::map<int, RecordingViewerColumn>& grid_columns, AppConfig::InputRecordingOptions& options)
{
	// TODO - AppConfig and .ini system doesn't have an easy way to just use string keys or collections so...another switch it is!
	// Refactor ALL of this once we have a better config system
	for (auto& column_entry : grid_columns)
	{
		int column_index = column_entry.first;
		bool shown = column_entry.second.m_shown;
		switch (column_entry.second.m_internal_index)
		{
			case 0:
				options.ViewerLeftAnalogXIndex = column_index;
				options.ViewerLeftAnalogXShown = shown;
				break;
			case 1:
				options.ViewerLeftAnalogYIndex = column_index;
				options.ViewerLeftAnalogYShown = shown;
				break;
			case 2:
				options.ViewerRightAnalogXIndex = column_index;
				options.ViewerRightAnalogXShown = shown;
				break;
			case 3:
				options.ViewerRightAnalogYIndex = column_index;
				options.ViewerRightAnalogYShown = shown;
				break;
			case 4:
				options.ViewerSquareIndex = column_index;
				options.ViewerSquareShown = shown;
				break;
			case 5:
				options.ViewerCrossIndex = column_index;
				options.ViewerCrossShown = shown;
				break;
			case 6:
				options.ViewerCircleIndex = column_index;
				options.ViewerCircleShown = shown;
				break;
			case 7:
				options.ViewerTriangleIndex = column_index;
				options.ViewerTriangleShown = shown;
				break;
			case 8:
				options.ViewerR1Index = column_index;
				options.ViewerR1Shown = shown;
				break;
			case 9:
				options.ViewerR2Index = column_index;
				options.ViewerR2Shown = shown;
				break;
			case 10:
				options.ViewerL1Index = column_index;
				options.ViewerL1Shown = shown;
				break;
			case 11:
				options.ViewerL2Index = column_index;
				options.ViewerL2Shown = shown;
				break;
			case 12:
				options.ViewerLeftIndex = column_index;
				options.ViewerLeftShown = shown;
				break;
			case 13:
				options.ViewerDownIndex = column_index;
				options.ViewerDownShown = shown;
				break;
			case 14:
				options.ViewerRightIndex = column_index;
				options.ViewerRightShown = shown;
				break;
			case 15:
				options.ViewerUpIndex = column_index;
				options.ViewerUpShown = shown;
				break;
			case 16:
				options.ViewerR3Index = column_index;
				options.ViewerR3Shown = shown;
				break;
			case 17:
				options.ViewerL3Index = column_index;
				options.ViewerL3Shown = shown;
				break;
			case 18:
				options.ViewerSelectIndex = column_index;
				options.ViewerSelectShown = shown;
				break;
			case 19:
				options.ViewerStartIndex = column_index;
				options.ViewerStartShown = shown;
				break;
			default:
				return;
		}
	}
}
