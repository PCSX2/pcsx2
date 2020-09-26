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

#ifndef DISABLE_RECORDING

#include "Pcsx2Types.h"

#include "Recording/PadData.h"
#include "Recording/VirtualPad/VirtualPadResources.h"

class VirtualPadData
{
public:
	/// Controller Background
	ImageFile m_background;

	/// Pressure Buttons
	ControllerPressureButton m_circle;
	ControllerPressureButton m_cross;
	ControllerPressureButton m_down;
	ControllerPressureButton m_l1;
	ControllerPressureButton m_l2;
	ControllerPressureButton m_left;
	ControllerPressureButton m_r1;
	ControllerPressureButton m_r2;
	ControllerPressureButton m_right;
	ControllerPressureButton m_square;
	ControllerPressureButton m_triangle;
	ControllerPressureButton m_up;

	/// Normal (un)pressed buttons
	ControllerNormalButton m_l3;
	ControllerNormalButton m_r3;
	ControllerNormalButton m_select;
	ControllerNormalButton m_start;

	/// Analog Sticks
	AnalogStick m_leftAnalog;
	AnalogStick m_rightAnalog;

	// Given the input buffer and the current index, updates the respective field(s) within this object
	// Additionally overwrites the PadData object under the following criteria:
	// - If ignoreRealController is true (and readOnly is false) PadData will always be updated
	// - else if the VirtualPad has overwritten the value, and the real controller has not changed since that moment in time
	// returns a boolean to indicate if it has updated the PadData
	bool UpdateVirtualPadData(u16 bufIndex, PadData* padData, bool ignoreRealController = false, bool readOnly = false);
};

#endif
