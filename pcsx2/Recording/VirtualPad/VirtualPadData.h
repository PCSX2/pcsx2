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

#pragma once

#ifndef PCSX2_CORE

#include "common/Pcsx2Types.h"

#include "Recording/PadData.h"
#include "Recording/VirtualPad/VirtualPadResources.h"

class VirtualPadData
{
public:
	/// Controller Background
	ImageFile background;

	/// Pressure Buttons
	ControllerPressureButton circle;
	ControllerPressureButton cross;
	ControllerPressureButton down;
	ControllerPressureButton l1;
	ControllerPressureButton l2;
	ControllerPressureButton left;
	ControllerPressureButton r1;
	ControllerPressureButton r2;
	ControllerPressureButton right;
	ControllerPressureButton square;
	ControllerPressureButton triangle;
	ControllerPressureButton up;

	/// Normal (un)pressed buttons
	ControllerNormalButton l3;
	ControllerNormalButton r3;
	ControllerNormalButton select;
	ControllerNormalButton start;

	/// Analog Sticks
	AnalogStick leftAnalog;
	AnalogStick rightAnalog;

	// Given the input buffer and the current index, updates the respective field(s) within this object
	// Additionally overwrites the PadData object under the following criteria:
	// - If ignoreRealController is true (and readOnly is false) PadData will always be updated
	// - else if the VirtualPad has overwritten the value, and the real controller has not changed since that moment in time
	// returns a boolean to indicate if it has updated the PadData
	bool UpdateVirtualPadData(u16 bufIndex, PadData* padData, bool ignoreRealController = false, bool readOnly = false);
};

#endif
