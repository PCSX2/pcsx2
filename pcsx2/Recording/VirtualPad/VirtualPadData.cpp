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

#ifndef PCSX2_CORE

#include "Recording/VirtualPad/VirtualPadData.h"

bool VirtualPadData::UpdateVirtualPadData(u16 bufIndex, PadData* padData, bool ignoreRealController, bool readOnly)
{
	bool changeDetected = false;
	PadData::BufferIndex index = static_cast<PadData::BufferIndex>(bufIndex);
	switch (index)
	{
		case PadData::BufferIndex::PressedFlagsGroupOne:
			changeDetected |= left.UpdateData(padData->leftPressed, ignoreRealController, readOnly);
			changeDetected |= down.UpdateData(padData->downPressed, ignoreRealController, readOnly);
			changeDetected |= right.UpdateData(padData->rightPressed, ignoreRealController, readOnly);
			changeDetected |= up.UpdateData(padData->upPressed, ignoreRealController, readOnly);
			changeDetected |= start.UpdateData(padData->start, ignoreRealController, readOnly);
			changeDetected |= r3.UpdateData(padData->r3, ignoreRealController, readOnly);
			changeDetected |= l3.UpdateData(padData->l3, ignoreRealController, readOnly);
			changeDetected |= select.UpdateData(padData->select, ignoreRealController, readOnly);
			return changeDetected;
		case PadData::BufferIndex::PressedFlagsGroupTwo:
			changeDetected |= square.UpdateData(padData->squarePressed, ignoreRealController, readOnly);
			changeDetected |= cross.UpdateData(padData->crossPressed, ignoreRealController, readOnly);
			changeDetected |= circle.UpdateData(padData->circlePressed, ignoreRealController, readOnly);
			changeDetected |= triangle.UpdateData(padData->trianglePressed, ignoreRealController, readOnly);
			changeDetected |= r1.UpdateData(padData->r1Pressed, ignoreRealController, readOnly);
			changeDetected |= l1.UpdateData(padData->l1Pressed, ignoreRealController, readOnly);
			changeDetected |= r2.UpdateData(padData->r2Pressed, ignoreRealController, readOnly);
			changeDetected |= l2.UpdateData(padData->l2Pressed, ignoreRealController, readOnly);
			return changeDetected;
		case PadData::BufferIndex::RightAnalogXVector:
			return rightAnalog.xVector.UpdateData(padData->rightAnalogX, ignoreRealController, readOnly);
		case PadData::BufferIndex::RightAnalogYVector:
			return rightAnalog.yVector.UpdateData(padData->rightAnalogY, ignoreRealController, readOnly);
		case PadData::BufferIndex::LeftAnalogXVector:
			return leftAnalog.xVector.UpdateData(padData->leftAnalogX, ignoreRealController, readOnly);
		case PadData::BufferIndex::LeftAnalogYVector:
			return leftAnalog.yVector.UpdateData(padData->leftAnalogY, ignoreRealController, readOnly);
		case PadData::BufferIndex::RightPressure:
			return right.UpdateData(padData->rightPressure, ignoreRealController, readOnly);
		case PadData::BufferIndex::LeftPressure:
			return left.UpdateData(padData->leftPressure, ignoreRealController, readOnly);
		case PadData::BufferIndex::UpPressure:
			return up.UpdateData(padData->upPressure, ignoreRealController, readOnly);
		case PadData::BufferIndex::DownPressure:
			return down.UpdateData(padData->downPressure, ignoreRealController, readOnly);
		case PadData::BufferIndex::TrianglePressure:
			return triangle.UpdateData(padData->trianglePressure, ignoreRealController, readOnly);
		case PadData::BufferIndex::CirclePressure:
			return circle.UpdateData(padData->circlePressure, ignoreRealController, readOnly);
		case PadData::BufferIndex::CrossPressure:
			return cross.UpdateData(padData->crossPressure, ignoreRealController, readOnly);
		case PadData::BufferIndex::SquarePressure:
			return square.UpdateData(padData->squarePressure, ignoreRealController, readOnly);
		case PadData::BufferIndex::L1Pressure:
			return l1.UpdateData(padData->l1Pressure, ignoreRealController, readOnly);
		case PadData::BufferIndex::R1Pressure:
			return r1.UpdateData(padData->r1Pressure, ignoreRealController, readOnly);
		case PadData::BufferIndex::L2Pressure:
			return l2.UpdateData(padData->l2Pressure, ignoreRealController, readOnly);
		case PadData::BufferIndex::R2Pressure:
			return r2.UpdateData(padData->r2Pressure, ignoreRealController, readOnly);
	}
	return changeDetected;
}

#endif
