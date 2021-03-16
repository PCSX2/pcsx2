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

const std::vector<std::string> columnLabels = {
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

RecordingViewerColumn::RecordingViewerColumn(int internalIndex, bool shown)
	: internalIndex(internalIndex)
	, label(columnLabels.at(internalIndex))
	, shown(shown)
{
}

void appendColumn(int internalIndex, std::map<int, RecordingViewerColumn>& gridColumns, AppConfig::InputRecordingOptions& options)
{
	// TODO - AppConfig and .ini system doesn't have an easy way to just use string keys or collections so...another switch it is!
	// Refactor ALL of this once we have a better config system
	switch (internalIndex)
	{
		case 0:
			gridColumns[options.ViewerLeftAnalogXIndex] = RecordingViewerColumn(internalIndex, options.ViewerLeftAnalogXShown);
			break;
		case 1:
			gridColumns[options.ViewerLeftAnalogYIndex] = RecordingViewerColumn(internalIndex, options.ViewerLeftAnalogYShown);
			break;
		case 2:
			gridColumns[options.ViewerRightAnalogXIndex] = RecordingViewerColumn(internalIndex, options.ViewerRightAnalogXShown);
			break;
		case 3:
			gridColumns[options.ViewerRightAnalogYIndex] = RecordingViewerColumn(internalIndex, options.ViewerRightAnalogYShown);
			break;
		case 4:
			gridColumns[options.ViewerSquareIndex] = RecordingViewerColumn(internalIndex, options.ViewerSquareShown);
			break;
		case 5:
			gridColumns[options.ViewerCrossIndex] = RecordingViewerColumn(internalIndex, options.ViewerCrossShown);
			break;
		case 6:
			gridColumns[options.ViewerCircleIndex] = RecordingViewerColumn(internalIndex, options.ViewerCircleShown);
			break;
		case 7:
			gridColumns[options.ViewerTriangleIndex] = RecordingViewerColumn(internalIndex, options.ViewerTriangleShown);
			break;
		case 8:
			gridColumns[options.ViewerR1Index] = RecordingViewerColumn(internalIndex, options.ViewerR1Shown);
			break;
		case 9:
			gridColumns[options.ViewerR2Index] = RecordingViewerColumn(internalIndex, options.ViewerR2Shown);
			break;
		case 10:
			gridColumns[options.ViewerL1Index] = RecordingViewerColumn(internalIndex, options.ViewerL1Shown);
			break;
		case 11:
			gridColumns[options.ViewerL2Index] = RecordingViewerColumn(internalIndex, options.ViewerL2Shown);
			break;
		case 12:
			gridColumns[options.ViewerLeftIndex] = RecordingViewerColumn(internalIndex, options.ViewerLeftShown);
			break;
		case 13:
			gridColumns[options.ViewerDownIndex] = RecordingViewerColumn(internalIndex, options.ViewerDownShown);
			break;
		case 14:
			gridColumns[options.ViewerRightIndex] = RecordingViewerColumn(internalIndex, options.ViewerRightShown);
			break;
		case 15:
			gridColumns[options.ViewerUpIndex] = RecordingViewerColumn(internalIndex, options.ViewerUpShown);
			break;
		case 16:
			gridColumns[options.ViewerR3Index] = RecordingViewerColumn(internalIndex, options.ViewerR3Shown);
			break;
		case 17:
			gridColumns[options.ViewerL3Index] = RecordingViewerColumn(internalIndex, options.ViewerL3Shown);
			break;
		case 18:
			gridColumns[options.ViewerSelectIndex] = RecordingViewerColumn(internalIndex, options.ViewerSelectShown);
			break;
		case 19:
			gridColumns[options.ViewerStartIndex] = RecordingViewerColumn(internalIndex, options.ViewerStartShown);
			break;
		default:
			return;
	}
}

void saveColumnsToConfig(const std::map<int, RecordingViewerColumn>& gridColumns, AppConfig::InputRecordingOptions& options)
{
	// TODO - AppConfig and .ini system doesn't have an easy way to just use string keys or collections so...another switch it is!
	// Refactor ALL of this once we have a better config system
	for (auto& columnEntry : gridColumns)
	{
		int columnIndex = columnEntry.first;
		bool shown = columnEntry.second.shown;
		switch (columnEntry.second.internalIndex)
		{
			case 0:
				options.ViewerLeftAnalogXIndex = columnIndex;
				options.ViewerLeftAnalogXShown = shown;
				break;
			case 1:
				options.ViewerLeftAnalogYIndex = columnIndex;
				options.ViewerLeftAnalogYShown = shown;
				break;
			case 2:
				options.ViewerRightAnalogXIndex = columnIndex;
				options.ViewerRightAnalogXShown = shown;
				break;
			case 3:
				options.ViewerRightAnalogYIndex = columnIndex;
				options.ViewerRightAnalogYShown = shown;
				break;
			case 4:
				options.ViewerSquareIndex = columnIndex;
				options.ViewerSquareShown = shown;
				break;
			case 5:
				options.ViewerCrossIndex = columnIndex;
				options.ViewerCrossShown = shown;
				break;
			case 6:
				options.ViewerCircleIndex = columnIndex;
				options.ViewerCircleShown = shown;
				break;
			case 7:
				options.ViewerTriangleIndex = columnIndex;
				options.ViewerTriangleShown = shown;
				break;
			case 8:
				options.ViewerR1Index = columnIndex;
				options.ViewerR1Shown = shown;
				break;
			case 9:
				options.ViewerR2Index = columnIndex;
				options.ViewerR2Shown = shown;
				break;
			case 10:
				options.ViewerL1Index = columnIndex;
				options.ViewerL1Shown = shown;
				break;
			case 11:
				options.ViewerL2Index = columnIndex;
				options.ViewerL2Shown = shown;
				break;
			case 12:
				options.ViewerLeftIndex = columnIndex;
				options.ViewerLeftShown = shown;
				break;
			case 13:
				options.ViewerDownIndex = columnIndex;
				options.ViewerDownShown = shown;
				break;
			case 14:
				options.ViewerRightIndex = columnIndex;
				options.ViewerRightShown = shown;
				break;
			case 15:
				options.ViewerUpIndex = columnIndex;
				options.ViewerUpShown = shown;
				break;
			case 16:
				options.ViewerR3Index = columnIndex;
				options.ViewerR3Shown = shown;
				break;
			case 17:
				options.ViewerL3Index = columnIndex;
				options.ViewerL3Shown = shown;
				break;
			case 18:
				options.ViewerSelectIndex = columnIndex;
				options.ViewerSelectShown = shown;
				break;
			case 19:
				options.ViewerStartIndex = columnIndex;
				options.ViewerStartShown = shown;
				break;
			default:
				return;
		}
	}
}
